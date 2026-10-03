#include "RaidRosterItemTier.h"
#include "RaidRosterTierRules.h"
#include "IndividualProgression.h"   // ProgressionState + sIndividualProgression (see RaidRosterEra.h)
#include "DatabaseEnv.h"
#include "QueryResult.h"
#include "Field.h"
#include "DBCStores.h"
#include "ObjectMgr.h"
#include "ItemTemplate.h"
#include "CreatureData.h"
#include "StringFormat.h"
#include "Log.h"
#include <algorithm>
#include <unordered_map>
#include <unordered_set>

using namespace RaidRosterTierRules;

static_assert(kTierPreAq == PROGRESSION_PRE_AQ && kTierPreTbc == PROGRESSION_PRE_TBC &&
              kTierWotlk == PROGRESSION_TBC_TIER_5,
              "RaidRosterTierRules constants drifted from IP's ProgressionState");

namespace
{
    enum SourceKind : uint8 { SRC_DROP, SRC_REF_DROP, SRC_CHEST, SRC_REF_CHEST, SRC_VENDOR, SRC_QUEST };

    char const* KindName(uint8 kind)
    {
        switch (kind)
        {
            case SRC_DROP:      return "drop";
            case SRC_REF_DROP:  return "drop(ref)";
            case SRC_CHEST:     return "chest";
            case SRC_REF_CHEST: return "chest(ref)";
            case SRC_VENDOR:    return "vendor";
            default:            return "quest";
        }
    }

    // One source of one pool item. Physical kinds: id = loot entry / reference entry / vendor
    // creature entry, with the spawn map. SRC_QUEST: id = quest id (map/mapTier/condTier unused).
    struct SourceRecord
    {
        uint8 kind;
        uint32 id;
        uint32 map;
        uint8 mapTier;
        uint8 scriptTier; // IP CanBeSeen gate of the dropping/selling creature or chest GO
        uint8 condTier;
        uint8 tier;       // source tier; kTierUnset on a quest that is not a source (no starter)
    };

    struct ItemTierInfo
    {
        uint8 tier = 0;
        uint8 keyLevel = 0;           // equipCacheNew key
        bool fallback = false;        // no source -> level band
        std::vector<SourceRecord> sources;
    };

    std::unordered_map<uint32, ItemTierInfo> g_items;      // pool entry -> verdict
    std::unordered_map<uint32, QuestFacts> g_quests;       // every quest
    std::unordered_map<uint32, uint8> g_questTier;         // resolved tiers of reward quests
    std::unordered_map<uint32, uint8> g_physTier;          // item -> MIN drop/chest/vendor tier
    bool g_built = false;

    // COPY of mod-enchanter-npc's EnchanterEra::TierForMap (modules must not depend on each other;
    // keep the named cases in sync), plus map 609: DK-start quest rewards are req 55-58 and would
    // otherwise reach a level-60 Vanilla bot. The DEFAULT branch deliberately differs from the
    // enchanter's `return 0`: an unlisted map takes its expansion (Map.dbc), so every TBC/WotLK
    // 5-man is PRE_TBC/TBC_TIER_5 instead of 0 (Hellfire Ramparts req-60 drops reached Vanilla bots).
    uint8 TierForMap(uint32 mapId)
    {
        switch (mapId)
        {
            case 469: return PROGRESSION_MOLTEN_CORE;                                    // Blackwing Lair
            case 309: return uint8(sIndividualProgression->RequiredZulGurubProgression); // Zul'Gurub
            case 509:                                                                    // Ruins of Ahn'Qiraj
            case 531: return PROGRESSION_PRE_AQ;                                         // Temple of Ahn'Qiraj
            case 533: return PROGRESSION_AQ;          // Naxxramas (the WotLK copy's loot is level-gated anyway)
            case 532:                                                                    // Karazhan
            case 565:                                                                    // Gruul's Lair
            case 544: return PROGRESSION_PRE_TBC;                                        // Magtheridon's Lair
            case 548:                                                                    // Serpentshrine Cavern
            case 550: return PROGRESSION_TBC_TIER_1;                                     // Tempest Keep
            case 534:                                                                    // Hyjal Summit
            case 564: return PROGRESSION_TBC_TIER_2;                                     // Black Temple
            case 568: return uint8(sIndividualProgression->RequiredZulAmanProgression);  // Zul'Aman
            case 580: return PROGRESSION_TBC_TIER_4;                                     // Sunwell Plateau
            case 615:                                                                    // Obsidian Sanctum
            case 616:                                                                    // Eye of Eternity
            case 624: return PROGRESSION_TBC_TIER_5;                                     // Vault of Archavon
            case 603: return PROGRESSION_WOTLK_TIER_1;                                   // Ulduar
            case 649: return PROGRESSION_WOTLK_TIER_2;                                   // Trial of the Crusader
            case 631: return PROGRESSION_WOTLK_TIER_3;                                   // Icecrown Citadel
            case 724: return PROGRESSION_WOTLK_TIER_4;                                   // Ruby Sanctum
            case 530: return PROGRESSION_PRE_TBC;     // Outland
            case 571: return PROGRESSION_TBC_TIER_5;  // Northrend
            case 609: return PROGRESSION_TBC_TIER_5;  // Ebon Hold / DK start (raidroster-only addition)
            default:                                  // Azeroth open world + dungeons, MC, Onyxia: 0
                break;
        }
        if (MapEntry const* map = sMapStore.LookupEntry(mapId))
        {
            if (map->Expansion() == 1)
                return PROGRESSION_PRE_TBC;
            if (map->Expansion() == 2)
                return PROGRESSION_TBC_TIER_5;
        }
        return 0;
    }

    // IP hides creatures/gameobjects per player in C++ (`CanBeSeen` in IP's
    // IndividualProgressionAwareness.cpp). Each script's LOWER bound: the earliest state at which
    // the NPC/GO is visible at all. Windowed scripts (visible only BETWEEN two states) use the
    // window's start — something obtainable once stays legit. "Before state N"-only scripts are 0.
    // Built per Build() because ZG/ZA/early-DS2 come from IP's config.
    std::unordered_map<std::string, uint8> g_scriptTier;

    void LoadScriptTiers()
    {
        auto const* ip = sIndividualProgression;
        g_scriptTier = {
            { "npc_ipp_preaq",        PROGRESSION_BLACKWING_LAIR },
            { "npc_ipp_zg",           uint8(ip->RequiredZulGurubProgression) },
            { "npc_ipp_we",           PROGRESSION_BLACKWING_LAIR },   // window BWL..<PRE_AQ
            { "npc_ipp_aq",           PROGRESSION_AQ_WAR },
            { "npc_ipp_aqwewar",      PROGRESSION_BLACKWING_LAIR },   // window BWL..<AQ_WAR
            { "npc_ipp_aqwar",        PROGRESSION_PRE_AQ },           // window PRE_AQ..<AQ_WAR
            { "npc_ipp_si",           PROGRESSION_AQ },               // window AQ..<NAXX40
            { "npc_ipp_naxx40",       PROGRESSION_AQ },
            { "npc_ipp_tbc",          PROGRESSION_PRE_TBC },
            { "npc_ipp_tbc_t3",       PROGRESSION_TBC_TIER_2 },
            { "npc_ipp_za",           uint8(ip->RequiredZulAmanProgression) },
            { "npc_ipp_wotlk",        PROGRESSION_TBC_TIER_5 },
            { "npc_ipp_wotlk_ulduar", PROGRESSION_WOTLK_TIER_1 },
            { "npc_ipp_wotlk_totc",   PROGRESSION_WOTLK_TIER_2 },
            { "npc_ipp_wotlk_icc",    PROGRESSION_WOTLK_TIER_3 },
            { "npc_ipp_ds2",          uint8(ip->earlyDungeonSet2 ? 0 : PROGRESSION_BLACKWING_LAIR) },
            { "gobject_ipp_preaq",    PROGRESSION_BLACKWING_LAIR },
            { "gobject_ipp_aqwar",    PROGRESSION_PRE_AQ },           // window PRE_AQ..<AQ_WAR
            { "gobject_ipp_si",       PROGRESSION_AQ },               // window AQ..<NAXX40
            { "gobject_ipp_naxx40",   PROGRESSION_AQ },
            { "gobject_ipp_tbc",      PROGRESSION_PRE_TBC },
            { "gobject_ipp_tbc_t4",   PROGRESSION_TBC_TIER_4 },
            { "gobject_ipp_wotlk",    PROGRESSION_TBC_TIER_5 },
            // 0 (before-only): npc_ipp_pre_naxx40, npc_ipp_pre_tbc, npc_ipp_tbc_pre_t3,
            // npc_ipp_pre_wotlk, gobject_ipp_pre_tbc.
        };
    }

    uint8 ScriptTier(uint32 scriptId)
    {
        if (!scriptId)
            return 0;
        auto it = g_scriptTier.find(sObjectMgr->GetScriptName(scriptId));
        return it != g_scriptTier.end() ? it->second : 0;
    }

    uint8 CreatureScriptTier(uint32 entry)
    {
        CreatureTemplate const* ct = sObjectMgr->GetCreatureTemplate(entry);
        return ct ? ScriptTier(ct->ScriptID) : 0;
    }

    uint8 GameObjectScriptTier(uint32 entry)
    {
        GameObjectTemplate const* gt = sObjectMgr->GetGameObjectTemplate(entry);
        return gt ? ScriptTier(gt->ScriptId) : 0;
    }

    // Quest chains IP gates in C++ (gong / phasing) that no table shows. Add an entry ONLY when
    // `.raidroster tiersweep` shows a real leak, with a one-line provenance comment.
    struct QuestOverride { uint32 quest; uint8 tier; };
    QuestOverride const kQuestTierOverride[] = {
        { 8745, PROGRESSION_PRE_AQ },   // Treasure of the Timeless One: Scarab Gong reward (Jonathan the Revelator)
        { 8572, PROGRESSION_AQ_WAR },   // Veteran's Battlegear: Cenarion Hold war-effort QM (IP AQ_WAR comment)
        { 8573, PROGRESSION_AQ_WAR },   // Champion's Battlegear: same quartermaster chain
        // Added from the 2026-10-01 `tiersweep 3 66` review. IP's ProgressionState comment puts the
        // "AQ quest line" (Scepter of the Shifting Sands, Brood of Nozdormu at Neutral) and ZG at
        // BLACKWING_LAIR; the data alone tiered these 0-1.
        { 8620, PROGRESSION_BLACKWING_LAIR },  // The Only Prescription: Scepter chain (Narain Soothfancy)
        { 8729, PROGRESSION_BLACKWING_LAIR },  // The Wrath of Neptulon: Scepter chain (Azuregos)
        { 8730, PROGRESSION_BLACKWING_LAIR },  // Nefarius's Corruption: Scepter chain (Vaelastrasz)
        { 8736, PROGRESSION_BLACKWING_LAIR },  // The Nightmare Manifests: Scepter chain (Eranikus)
        { 8201, PROGRESSION_BLACKWING_LAIR },  // A Collection of Heads: ZG priest heads (RequiredZulGurubProgression default 3)
    };

    // Quests whose starter is gated INDIRECTLY by an IP script NPC, so the starter's own script
    // tier misses it. The tier is that script's g_scriptTier entry (honours IP config).
    struct QuestScriptGate { uint32 quest; char const* script; };
    QuestScriptGate const kQuestScriptGate[] = {
        // Dead Man's Plea (root of the Anthion T0.5 legs chain 8951-8959): Anthion Harmon (16016) has
        // no script but is invisible (aura 27614) without the Extra-Dimensional Ghost Revealer
        // (22115), the start item of In Search of Anthion 8929/8930 from npc_ipp_ds2 Deliana/Mokvar.
        { 8945, "npc_ipp_ds2" },
    };

    constexpr uint32 kFactionBroodOfNozdormu = 910;   // rep exists only inside AQ

    // IP condition column: CAST(MAX(ConditionValue1) AS UNSIGNED), NULL when no gating row.
    uint8 CondTier(Field const& f)
    {
        if (f.IsNull())
            return 0;
        uint64 v = f.Get<uint64>();
        return (v > 66000 && v <= 66018) ? uint8(v - 66000) : 0;
    }

    // Shared tail of every IP-condition LEFT JOIN: positive "quest 66000+N rewarded" rows only.
    // Negative rows ("only before stage N") are ignored: an item obtainable earlier stays legit.
    constexpr char const* kCondFilter =
        "AND cd.ConditionTypeOrReference = 8 AND cd.NegativeCondition = 0 "
        "AND cd.ConditionValue1 BETWEEN 66001 AND 66018";

    template <typename Fn>
    void ForChunks(std::vector<uint32> const& ids, Fn&& fn)
    {
        constexpr size_t kChunk = 2000;
        for (size_t i = 0; i < ids.size(); i += kChunk)
        {
            std::string in;
            for (size_t j = i; j < std::min(ids.size(), i + kChunk); ++j)
            {
                if (!in.empty())
                    in += ',';
                in += std::to_string(ids[j]);
            }
            fn(in);
        }
    }

    // creature entry -> every map it can be found on. A loot row's creature is located through
    // this, not a plain `JOIN creature`, because two big classes of loot-bearing templates have no
    // spawn row of their own: (1) script-summoned bosses (Nefarian, Ragnaros, C'Thun...) — located
    // via instance_encounters' kill credit -> DungeonEncounter.dbc map; (2) heroic / 25-man
    // difficulty templates, whose loot lives on DifficultyEntry[] while the spawn row names the
    // base entry — they inherit the base entry's maps. A creature still unlocated is skipped as a
    // source (no known location), exactly as a spawnless creature was under a plain inner join.
    std::unordered_map<uint32, std::vector<uint32>> g_creatureMaps;

    // Script-summoned bosses with no spawn row and no instance_encounters kill credit.
    struct CreatureMap { uint32 entry; uint32 map; };
    CreatureMap const kCreatureMapOverride[] = {
        { 15082, 309 },   // Gri'lek (ZG Edge of Madness)
        { 15083, 309 },   // Hazza'rah (ZG Edge of Madness)
        { 15084, 309 },   // Renataki (ZG Edge of Madness)
        { 15085, 309 },   // Wushoolay (ZG Edge of Madness)
    };

    void AddCreatureMap(uint32 entry, uint32 map)
    {
        std::vector<uint32>& maps = g_creatureMaps[entry];
        if (std::find(maps.begin(), maps.end(), map) == maps.end())
            maps.push_back(map);
    }

    void LoadCreatureMaps()
    {
        if (QueryResult r = WorldDatabase.Query(
                "SELECT DISTINCT CAST(id AS UNSIGNED), CAST(map AS UNSIGNED) FROM creature"))
            do
            {
                Field* f = r->Fetch();
                AddCreatureMap(uint32(f[0].Get<uint64>()), uint32(f[1].Get<uint64>()));
            } while (r->NextRow());

        if (QueryResult r = WorldDatabase.Query(
                "SELECT CAST(entry AS UNSIGNED), CAST(creditEntry AS UNSIGNED) FROM instance_encounters "
                "WHERE creditType = 0 AND creditEntry > 0"))
            do
            {
                Field* f = r->Fetch();
                if (DungeonEncounterEntry const* enc = sDungeonEncounterStore.LookupEntry(uint32(f[0].Get<uint64>())))
                    AddCreatureMap(uint32(f[1].Get<uint64>()), enc->mapId);
            } while (r->NextRow());

        for (CreatureMap const& c : kCreatureMapOverride)
            AddCreatureMap(c.entry, c.map);

        // Collect first: inserting while iterating an unordered_map can rehash.
        std::vector<std::pair<uint32, uint32>> inherited;   // (difficulty entry, map)
        for (auto const& [entry, maps] : g_creatureMaps)
            if (CreatureTemplate const* ct = sObjectMgr->GetCreatureTemplate(entry))
                for (uint32 diff : ct->DifficultyEntry)
                    if (diff)
                        for (uint32 map : maps)
                            inherited.push_back({ diff, map });
        for (auto const& [diff, map] : inherited)
            AddCreatureMap(diff, map);
    }

    // gameobject template entry -> maps, the chest counterpart of g_creatureMaps. Encounter chests
    // are summoned by boss scripts and have no `gameobject` spawn row, so their location is
    // authored here. List derived 2026-10-01 from the DB: type-3 templates with no spawn whose loot
    // (direct or one reference deep) holds uncommon+ equippable gear of ilvl >= 66. Not placed:
    // none. The Outland treasure chests are placed on 530 as an era stand-in: they also
    // spawn in TBC 5-mans (TierForMap 0), but their TBC req-60 epics (31125-31134) have no other
    // located source and fell back to tier 0.
    std::unordered_map<uint32, std::vector<uint32>> g_goMaps;

    struct ChestMap { uint32 goEntry; uint32 map; };
    ChestMap const kChestMapOverride[] = {
        { 181207, 533 },   // Runed Demonic Blade (Andonisus, Naxx40 Kel'Thuzad)
        { 361000, 533 },   // Four Horsemen Chest (IP Naxx40)
        { 181366, 533 },   // Four Horsemen Chest (Naxx 10)
        { 193426, 533 },   // Four Horsemen Chest (Naxx 25)
        { 184930, 530 },   // Solid Fel Iron Chest (Outland treasure)
        { 184931, 530 },   // Bound Fel Iron Chest
        { 184932, 530 },   // Bound Fel Iron Chest
        { 184933, 530 },   // Solid Fel Iron Chest
        { 184934, 530 },   // Bound Fel Iron Chest
        { 184935, 530 },   // Solid Fel Iron Chest
        { 184936, 530 },   // Bound Adamantite Chest
        { 184937, 530 },   // Solid Adamantite Chest
        { 184938, 530 },   // Bound Adamantite Chest
        { 184939, 530 },   // Solid Adamantite Chest
        { 184940, 530 },   // Bound Adamantite Chest
        { 184941, 530 },   // Solid Adamantite Chest
        { 186667, 568 },   // Kraz's Package (Zul'Aman timed event)
        { 186672, 568 },   // Ashli's Bag (Zul'Aman timed event)
        { 186744, 568 },   // Amani Treasure Box (Zul'Aman timed event)
        { 187021, 568 },   // Harkor's Satchel (Zul'Aman timed event)
        { 187892, 547 },   // Ice Chest (Ahune, Slave Pens)
        { 188192, 547 },   // Ice Chest (Ahune heroic, Slave Pens)
        { 190663, 595 },   // Dark Runed Chest (Culling of Stratholme)
        { 193597, 595 },   // Dark Runed Chest (Culling of Stratholme heroic)
        { 193905, 616 },   // Alexstrasza's Gift (Eye of Eternity 10)
        { 193967, 616 },   // Alexstrasza's Gift (Eye of Eternity 25)
        { 194312, 603 },   // Cache of Storms (Ulduar, Thorim)
        { 194313, 603 },   // Cache of Storms
        { 194314, 603 },   // Cache of Storms
        { 194315, 603 },   // Cache of Storms
        { 194324, 603 },   // Freya's Gift (Ulduar, Freya)
        { 194325, 603 },   // Freya's Gift
        { 194326, 603 },   // Freya's Gift
        { 194327, 603 },   // Freya's Gift
        { 194328, 603 },   // Freya's Gift
        { 194329, 603 },   // Freya's Gift
        { 194330, 603 },   // Freya's Gift
        { 194331, 603 },   // Freya's Gift
        { 194789, 603 },   // Cache of Innovation (Ulduar, Mimiron)
        { 194956, 603 },   // Cache of Innovation
        { 194957, 603 },   // Cache of Innovation
        { 194958, 603 },   // Cache of Innovation
        { 194821, 603 },   // Gift of the Observer (Ulduar, Algalon)
        { 194822, 603 },   // Gift of the Observer
        { 195046, 603 },   // Cache of Living Stone (Ulduar, Kologarn)
        { 195047, 603 },   // Cache of Living Stone
        { 195323, 650 },   // Confessor's Cache (Trial of the Champion)
        { 195324, 650 },   // Confessor's Cache (heroic)
        { 195374, 650 },   // Eadric's Cache (Trial of the Champion)
        { 195375, 650 },   // Eadric's Cache (heroic)
        { 195709, 650 },   // Champion's Cache (Trial of the Champion)
        { 195710, 650 },   // Champion's Cache (heroic)
        { 195631, 649 },   // Champions' Cache (Trial of the Crusader, Faction Champions)
        { 195632, 649 },   // Champions' Cache
        { 195633, 649 },   // Champions' Cache
        { 195635, 649 },   // Champions' Cache
        { 195665, 649 },   // Argent Crusade Tribute Chest (Trial of the Crusader)
        { 195666, 649 },   // Argent Crusade Tribute Chest
        { 195667, 649 },   // Argent Crusade Tribute Chest
        { 195669, 649 },   // Argent Crusade Tribute Chest
        { 195670, 649 },   // Argent Crusade Tribute Chest
        { 195671, 649 },   // Argent Crusade Tribute Chest
        { 201959, 631 },   // Cache of the Dreamwalker (ICC, Valithria)
        { 202338, 631 },   // Cache of the Dreamwalker
        { 202339, 631 },   // Cache of the Dreamwalker
        { 202340, 631 },   // Cache of the Dreamwalker
        { 202212, 631 },   // The Captain's Chest (ICC, Gunship Battle)
        { 202337, 631 },   // The Captain's Chest
    };

    void AddGoMap(uint32 entry, uint32 map)
    {
        std::vector<uint32>& maps = g_goMaps[entry];
        if (std::find(maps.begin(), maps.end(), map) == maps.end())
            maps.push_back(map);
    }

    void LoadGameObjectMaps()
    {
        if (QueryResult r = WorldDatabase.Query(
                "SELECT DISTINCT CAST(id AS UNSIGNED), CAST(map AS UNSIGNED) FROM gameobject"))
            do
            {
                Field* f = r->Fetch();
                AddGoMap(uint32(f[0].Get<uint64>()), uint32(f[1].Get<uint64>()));
            } while (r->NextRow());
        for (ChestMap const& c : kChestMapOverride)
            AddGoMap(c.goEntry, c.map);
    }

    // Every physical source table returns (item, location, sourceId, condTier) rows. The location
    // is a creature entry (resolved through g_creatureMaps) or, for chests, a gameobject template
    // entry (resolved through g_goMaps).
    void LoadPhysical(std::vector<uint32> const& ids)
    {
        struct Q { uint8 kind; std::unordered_map<uint32, std::vector<uint32>> const* locs; uint8 (*scriptTier)(uint32); char const* sql; };
        Q const queries[] = {
            { SRC_DROP, &g_creatureMaps, &CreatureScriptTier,
              "SELECT CAST(l.Item AS UNSIGNED), CAST(ct.entry AS UNSIGNED), CAST(l.Entry AS UNSIGNED), "
              "CAST(MAX(cd.ConditionValue1) AS UNSIGNED) FROM creature_loot_template l "
              "JOIN creature_template ct ON ct.lootid = l.Entry "
              "LEFT JOIN conditions cd ON cd.SourceTypeOrReferenceId = 1 AND cd.SourceGroup = l.Entry "
              "AND cd.SourceEntry = l.Item {1} "
              "WHERE l.Reference = 0 AND l.Item IN ({0}) GROUP BY l.Item, ct.entry, l.Entry" },
            { SRC_REF_DROP, &g_creatureMaps, &CreatureScriptTier,
              "SELECT CAST(rl.Item AS UNSIGNED), CAST(ct.entry AS UNSIGNED), CAST(rl.Entry AS UNSIGNED), "
              "CAST(MAX(cd.ConditionValue1) AS UNSIGNED) FROM reference_loot_template rl "
              "JOIN creature_loot_template l ON l.Reference = rl.Entry "
              "JOIN creature_template ct ON ct.lootid = l.Entry "
              "LEFT JOIN conditions cd ON cd.SourceTypeOrReferenceId = 10 AND cd.SourceGroup = rl.Entry "
              "AND cd.SourceEntry = rl.Item {1} "
              "WHERE rl.Reference = 0 AND rl.Item IN ({0}) GROUP BY rl.Item, ct.entry, rl.Entry" },
            { SRC_CHEST, &g_goMaps, &GameObjectScriptTier,
              "SELECT CAST(l.Item AS UNSIGNED), CAST(gt.entry AS UNSIGNED), CAST(l.Entry AS UNSIGNED), "
              "CAST(MAX(cd.ConditionValue1) AS UNSIGNED) FROM gameobject_loot_template l "
              "JOIN gameobject_template gt ON gt.type = 3 AND gt.Data1 = l.Entry "
              "LEFT JOIN conditions cd ON cd.SourceTypeOrReferenceId = 4 AND cd.SourceGroup = l.Entry "
              "AND cd.SourceEntry = l.Item {1} "
              "WHERE l.Reference = 0 AND l.Item IN ({0}) GROUP BY l.Item, gt.entry, l.Entry" },
            { SRC_REF_CHEST, &g_goMaps, &GameObjectScriptTier,
              "SELECT CAST(rl.Item AS UNSIGNED), CAST(gt.entry AS UNSIGNED), CAST(rl.Entry AS UNSIGNED), "
              "CAST(MAX(cd.ConditionValue1) AS UNSIGNED) FROM reference_loot_template rl "
              "JOIN gameobject_loot_template l ON l.Reference = rl.Entry "
              "JOIN gameobject_template gt ON gt.type = 3 AND gt.Data1 = l.Entry "
              "LEFT JOIN conditions cd ON cd.SourceTypeOrReferenceId = 10 AND cd.SourceGroup = rl.Entry "
              "AND cd.SourceEntry = rl.Item {1} "
              "WHERE rl.Reference = 0 AND rl.Item IN ({0}) GROUP BY rl.Item, gt.entry, rl.Entry" },
            { SRC_VENDOR, &g_creatureMaps, &CreatureScriptTier,
              "SELECT CAST(v.item AS UNSIGNED), CAST(v.entry AS UNSIGNED), CAST(v.entry AS UNSIGNED), "
              "CAST(MAX(cd.ConditionValue1) AS UNSIGNED) FROM npc_vendor v "
              "LEFT JOIN conditions cd ON cd.SourceTypeOrReferenceId = 23 AND cd.SourceGroup = v.entry "
              "AND cd.SourceEntry = v.item {1} "
              "WHERE v.item IN ({0}) GROUP BY v.item, v.entry" },
        };

        auto addSource = [](uint8 kind, uint32 item, uint32 srcId, uint32 map, uint8 scriptTier, uint8 condTier)
        {
            uint8 mapTier = TierForMap(map);
            uint8 tier = 0;
            FoldSource(tier, mapTier);
            FoldSource(tier, scriptTier);
            FoldSource(tier, condTier);

            auto pt = g_physTier.find(item);
            if (pt == g_physTier.end())
                g_physTier[item] = tier;
            else
                FoldItem(pt->second, tier);

            auto it = g_items.find(item);
            if (it == g_items.end())
                return;
            // Several templates can share one loot id on the same map: keep one line per source, the
            // LOWEST-tier one (templates can differ by IP script gate), since items fold over lines.
            for (SourceRecord& s : it->second.sources)
                if (s.kind == kind && s.id == srcId && s.map == map)
                {
                    if (tier < s.tier)
                        s = { kind, srcId, map, mapTier, scriptTier, condTier, tier };
                    return;
                }
            it->second.sources.push_back({ kind, srcId, map, mapTier, scriptTier, condTier, tier });
        };

        for (Q const& q : queries)
            ForChunks(ids, [&](std::string const& in)
            {
                QueryResult r = WorldDatabase.Query(Acore::StringFormat(q.sql, in, kCondFilter));
                if (!r)
                    return;
                do
                {
                    Field* f = r->Fetch();
                    uint32 item = uint32(f[0].Get<uint64>());
                    uint32 where = uint32(f[1].Get<uint64>());
                    uint32 srcId = uint32(f[2].Get<uint64>());
                    uint8 condTier = CondTier(f[3]);
                    auto cm = q.locs->find(where);
                    if (cm == q.locs->end())
                        continue;   // never spawned, not an encounter boss / authored chest: no known location
                    uint8 scriptTier = q.scriptTier(where);
                    for (uint32 map : cm->second)
                        addSource(q.kind, item, srcId, map, scriptTier, condTier);
                } while (r->NextRow());
            });
    }

    uint8 PhysTier(uint32 item, bool& found)
    {
        auto it = g_physTier.find(item);
        found = it != g_physTier.end();
        return found ? it->second : 0;
    }

    uint8 FallbackTier(uint32 entry, uint8 keyLevel)
    {
        uint32 level = keyLevel;
        if (ItemTemplate const* proto = sObjectMgr->GetItemTemplate(entry))
            level = std::max<uint32>(level, proto->RequiredLevel);
        return LevelBandTier(level);
    }
}

void RaidRosterItemTier::Build(std::vector<std::pair<uint32, uint8>> const& entries)
{
    if (g_built)
        return;
    g_built = true;

    for (auto const& [entry, keyLevel] : entries)
        g_items[entry].keyLevel = keyLevel;
    LoadScriptTiers();

    // ---- Quest facts (every quest), plus the extra items whose physical tier quests need. ----
    std::unordered_set<uint32> extraItems;   // RequiredItemId* + quest-starting items
    std::unordered_map<uint32, std::vector<uint32>> questReqItems;
    if (QueryResult r = WorldDatabase.Query(
            "SELECT CAST(q.ID AS SIGNED), CAST(q.QuestSortID AS SIGNED), "
            "CAST(q.RequiredItemId1 AS SIGNED), CAST(q.RequiredItemId2 AS SIGNED), CAST(q.RequiredItemId3 AS SIGNED), "
            "CAST(q.RequiredItemId4 AS SIGNED), CAST(q.RequiredItemId5 AS SIGNED), CAST(q.RequiredItemId6 AS SIGNED), "
            "CAST(IFNULL(a.PrevQuestID, 0) AS SIGNED), CAST(IFNULL(a.RequiredMinRepFaction, 0) AS SIGNED), "
            "CAST(IFNULL(a.RequiredMinRepValue, 0) AS SIGNED) "
            "FROM quest_template q LEFT JOIN quest_template_addon a ON a.ID = q.ID"))
    {
        do
        {
            Field* f = r->Fetch();
            uint32 id = uint32(f[0].Get<int64>());
            QuestFacts& q = g_quests[id];
            int64 sort = f[1].Get<int64>();
            if (sort > 0)
                if (AreaTableEntry const* area = sAreaTableStore.LookupEntry(uint32(sort)))
                    q.sortTier = TierForMap(area->mapid);
            for (uint8 i = 2; i <= 7; ++i)
                if (int64 req = f[i].Get<int64>(); req > 0)
                {
                    questReqItems[id].push_back(uint32(req));
                    extraItems.insert(uint32(req));
                }
            int64 prev = f[8].Get<int64>();
            q.prevQuest = uint32(prev < 0 ? -prev : prev);
            if (uint32(f[9].Get<int64>()) == kFactionBroodOfNozdormu && f[10].Get<int64>() > 0)
                q.repTier = PROGRESSION_PRE_AQ;
        } while (r->NextRow());
    }
    for (QuestOverride const& o : kQuestTierOverride)
        if (auto it = g_quests.find(o.quest); it != g_quests.end())
            it->second.overrideTier = o.tier;
    for (QuestScriptGate const& g : kQuestScriptGate)
        if (auto it = g_quests.find(g.quest); it != g_quests.end())
            if (auto st = g_scriptTier.find(g.script); st != g_scriptTier.end())
                FoldSource(it->second.overrideTier, st->second);

    // Starters. A starter NPC/GO with no spawn row still counts (tier 0) — scripted/summoned
    // givers exist; the sort-zone and chain rules still apply to their quests.
    auto foldStarter = [](uint32 quest, uint8 tier)
    {
        auto it = g_quests.find(quest);
        if (it == g_quests.end())
            return;
        QuestFacts& q = it->second;
        if (!q.hasStarter)
        {
            q.hasStarter = true;
            q.starterTier = tier;
        }
        else
            FoldItem(q.starterTier, tier);
    };
    // A starter's tier also folds its IP CanBeSeen script gate (e.g. npc_ipp_ds2 Anthion/Aurel).
    struct StarterQ { char const* sql; uint8 (*scriptTier)(uint32); };
    for (StarterQ const& sq : {
             StarterQ{ "SELECT CAST(qs.quest AS UNSIGNED), CAST(qs.id AS UNSIGNED), CAST(c.map AS UNSIGNED) "
                       "FROM creature_queststarter qs LEFT JOIN creature c ON c.id = qs.id GROUP BY qs.quest, qs.id, c.map",
                       &CreatureScriptTier },
             StarterQ{ "SELECT CAST(qs.quest AS UNSIGNED), CAST(qs.id AS UNSIGNED), CAST(g.map AS UNSIGNED) "
                       "FROM gameobject_queststarter qs LEFT JOIN gameobject g ON g.id = qs.id GROUP BY qs.quest, qs.id, g.map",
                       &GameObjectScriptTier } })
        if (QueryResult r = WorldDatabase.Query(sq.sql))
            do
            {
                Field* f = r->Fetch();
                uint8 tier = f[2].IsNull() ? 0 : TierForMap(uint32(f[2].Get<uint64>()));
                FoldSource(tier, sq.scriptTier(uint32(f[1].Get<uint64>())));
                foldStarter(uint32(f[0].Get<uint64>()), tier);
            } while (r->NextRow());

    std::vector<std::pair<uint32, uint32>> itemStarters;   // (item, quest)
    if (QueryResult r = WorldDatabase.Query(
            "SELECT CAST(entry AS UNSIGNED), CAST(startquest AS UNSIGNED) FROM item_template WHERE startquest > 0"))
        do
        {
            Field* f = r->Fetch();
            uint32 item = uint32(f[0].Get<uint64>());
            itemStarters.push_back({ item, uint32(f[1].Get<uint64>()) });
            extraItems.insert(item);
        } while (r->NextRow());

    if (QueryResult r = WorldDatabase.Query(Acore::StringFormat(
            "SELECT CAST(cd.SourceEntry AS UNSIGNED), CAST(MAX(cd.ConditionValue1) AS UNSIGNED) FROM conditions cd "
            "WHERE cd.SourceTypeOrReferenceId = 19 {} GROUP BY cd.SourceEntry", kCondFilter)))
        do
        {
            Field* f = r->Fetch();
            if (auto it = g_quests.find(uint32(f[0].Get<uint64>())); it != g_quests.end())
                it->second.condTier = CondTier(f[1]);
        } while (r->NextRow());

    // ---- Physical sources for pool + quest-required + quest-starting items. ----
    std::vector<uint32> ids;
    ids.reserve(g_items.size() + extraItems.size());
    for (auto const& [entry, info] : g_items)
        ids.push_back(entry);
    for (uint32 e : extraItems)
        if (!g_items.count(e))
            ids.push_back(e);
    LoadCreatureMaps();
    LoadGameObjectMaps();
    LoadPhysical(ids);
    g_creatureMaps.clear();   // only needed while gathering
    g_goMaps.clear();

    for (auto const& [item, quest] : itemStarters)
    {
        bool found = false;
        uint8 t = PhysTier(item, found);   // a starter item with no physical source counts at 0
        foldStarter(quest, t);
    }
    for (auto const& [quest, items] : questReqItems)
    {
        QuestFacts& q = g_quests[quest];
        for (uint32 item : items)
        {
            bool found = false;
            FoldSource(q.reqItemTier, PhysTier(item, found));
        }
    }

    // ---- Quest rewards. ----
    QuestTierResolver resolver(g_quests);
    if (QueryResult r = WorldDatabase.Query(
            "SELECT CAST(ID AS UNSIGNED), CAST(RewardItem1 AS UNSIGNED), CAST(RewardItem2 AS UNSIGNED), "
            "CAST(RewardItem3 AS UNSIGNED), CAST(RewardItem4 AS UNSIGNED), CAST(RewardChoiceItemID1 AS UNSIGNED), "
            "CAST(RewardChoiceItemID2 AS UNSIGNED), CAST(RewardChoiceItemID3 AS UNSIGNED), "
            "CAST(RewardChoiceItemID4 AS UNSIGNED), CAST(RewardChoiceItemID5 AS UNSIGNED), "
            "CAST(RewardChoiceItemID6 AS UNSIGNED) FROM quest_template"))
        do
        {
            Field* f = r->Fetch();
            uint32 quest = uint32(f[0].Get<uint64>());
            for (uint8 i = 1; i <= 10; ++i)
            {
                uint32 item = uint32(f[i].Get<uint64>());
                auto it = item ? g_items.find(item) : g_items.end();
                if (it == g_items.end())
                    continue;
                uint8 tier = kTierUnset;
                if (resolver.IsSource(quest))
                {
                    tier = resolver.Tier(quest);
                    g_questTier[quest] = tier;
                }
                it->second.sources.push_back({ SRC_QUEST, quest, 0, 0, 0, 0, tier });
            }
        } while (r->NextRow());

    // ---- Fold each pool item. ----
    uint32 fallbacks = 0;
    for (auto& [entry, info] : g_items)
    {
        uint8 acc = kTierUnset;
        for (SourceRecord const& s : info.sources)
            if (s.tier != kTierUnset)
                FoldItem(acc, s.tier);
        if (acc == kTierUnset)
        {
            acc = FallbackTier(entry, info.keyLevel);
            info.fallback = true;
            ++fallbacks;
        }
        info.tier = acc;
    }

    LOG_INFO("playerbots", "[RaidRoster] Item tier table built: {} pool items ({} level-band fallbacks), {} quests.",
             uint32(g_items.size()), fallbacks, uint32(g_quests.size()));
}

uint8 RaidRosterItemTier::TierOf(uint32 entry)
{
    auto it = g_items.find(entry);
    return it != g_items.end() ? it->second.tier : FallbackTier(entry, 0);
}

std::vector<std::string> RaidRosterItemTier::Explain(uint32 entry)
{
    std::vector<std::string> out;
    ItemTemplate const* proto = sObjectMgr->GetItemTemplate(entry);
    std::string const name = proto ? proto->Name1 : std::string("<unknown item>");
    auto it = g_items.find(entry);
    if (it == g_items.end())
    {
        out.push_back(Acore::StringFormat("Item {} [{}]: not in the sync gear pool; level-band fallback tier {}.",
                                          entry, name, uint32(TierOf(entry))));
        return out;
    }
    ItemTierInfo const& info = it->second;
    out.push_back(Acore::StringFormat("Item {} [{}] ilvl {} req {} (key {}): tier {}{}.", entry, name,
                                      proto ? proto->ItemLevel : 0, proto ? proto->RequiredLevel : 0,
                                      uint32(info.keyLevel), uint32(info.tier), info.fallback ? " (level-band fallback)" : ""));
    for (SourceRecord const& s : info.sources)
    {
        if (s.kind != SRC_QUEST)
        {
            out.push_back(Acore::StringFormat("  {} {} map {}: tier {} (map {}, script {}, IP cond {})", KindName(s.kind),
                                              s.id, s.map, uint32(s.tier), uint32(s.mapTier), uint32(s.scriptTier),
                                              uint32(s.condTier)));
            continue;
        }
        auto q = g_quests.find(s.id);
        if (s.tier == kTierUnset || q == g_quests.end())
        {
            out.push_back(Acore::StringFormat("  quest {}: not a source (no starter)", s.id));
            continue;
        }
        QuestFacts const& f = q->second;
        auto prev = g_questTier.find(f.prevQuest);
        out.push_back(Acore::StringFormat(
            "  quest {}: tier {} (starter {}, sort-zone {}, prev {}{}, req-items {}, IP cond {}, rep {}, override {})",
            s.id, uint32(s.tier), uint32(f.starterTier), uint32(f.sortTier), f.prevQuest,
            prev != g_questTier.end() ? Acore::StringFormat(" -> {}", uint32(prev->second)) : std::string(),
            uint32(f.reqItemTier), uint32(f.condTier), uint32(f.repTier), uint32(f.overrideTier)));
    }
    return out;
}

std::vector<std::string> RaidRosterItemTier::Sweep(uint8 maxTier, uint32 minIlvl)
{
    struct Row { uint32 ilvl; uint32 entry; };
    std::vector<Row> rows;
    for (auto const& [entry, info] : g_items)
    {
        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(entry);
        if (!proto || proto->Quality < ITEM_QUALITY_RARE || proto->ItemLevel < minIlvl || info.tier > maxTier)
            continue;
        if (std::max<uint32>(proto->RequiredLevel, info.keyLevel) > 60)
            continue;
        // Only verdicts the data can get wrong: won by a quest, or no source at all.
        bool questOrFallback = info.fallback;
        for (SourceRecord const& s : info.sources)
            if (s.tier == info.tier && s.kind == SRC_QUEST)
                questOrFallback = true;
        if (questOrFallback)
            rows.push_back({ proto->ItemLevel, entry });
    }
    std::sort(rows.begin(), rows.end(), [](Row const& a, Row const& b)
              { return a.ilvl != b.ilvl ? a.ilvl > b.ilvl : a.entry < b.entry; });

    std::vector<std::string> out;
    out.push_back(Acore::StringFormat("tiersweep: {} rare+ quest/fallback pool items, ilvl >= {}, tier <= {}:",
                                      uint32(rows.size()), minIlvl, uint32(maxTier)));
    for (size_t i = 0; i < rows.size() && i < 200; ++i)
    {
        ItemTierInfo const& info = g_items[rows[i].entry];
        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(rows[i].entry);
        std::string via = info.fallback ? "fallback" : "";
        for (SourceRecord const& s : info.sources)
            if (s.kind == SRC_QUEST && s.tier == info.tier)
            {
                via = Acore::StringFormat("quest {}", s.id);
                break;
            }
        out.push_back(Acore::StringFormat("  {} [{}] ilvl {} tier {} via {}", rows[i].entry, proto->Name1,
                                          rows[i].ilvl, uint32(info.tier), via));
    }
    return out;
}
