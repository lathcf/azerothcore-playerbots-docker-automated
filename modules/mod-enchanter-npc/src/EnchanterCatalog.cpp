#include "EnchanterCatalog.h"
#include "EnchanterEra.h"
#include "EnchanterRules.h"
#include "DatabaseEnv.h"
#include "DBCStores.h"
#include "Field.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "QueryResult.h"
#include "SharedDefines.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Timer.h"
#include <algorithm>
#include <cctype>
#include <unordered_map>
#include <unordered_set>

namespace
{
    std::vector<EnchantOffer> g_offers;
    std::vector<uint32> g_pricedItems;
    std::unordered_set<uint32> g_goldVendorItems;   // priced items some npc_vendor sells for plain gold

    // Profession lines whose recipes can put a permanent enchant straight onto an item.
    constexpr uint32 kRecipeSkills[] =
        { SKILL_ENCHANTING, SKILL_TAILORING, SKILL_LEATHERWORKING, SKILL_ENGINEERING, SKILL_INSCRIPTION };

    // item_template rows that carry an enchant spell but were never obtainable. Filled from the
    // `.enchanter dump` review (Task 10); every entry needs a comment saying why it is excluded.
    // "No source" below = no npc_vendor / creature / reference / gameobject / item / mail loot /
    // quest-reward row in acore_world AND no Spell.dbc CREATE_ITEM spell on any SkillLine (dev box,
    // 2026-09-27). Where the enchant is also sold by an obtainable item, that item is named.
    std::unordered_set<uint32> const kExcludedItems =
    {
        22584,  // "QAEnchant Cloak +3 Agility": QA test row; no source
        35728,  // "Greater Inscription of the Blade": no source; dup of Scryer 28910 (enchant 2997)
        35729,  // "Greater Inscription of the Knight": no source; dup of Scryer 28911 (enchant 2991)
        35730,  // "Greater Inscription of the Oracle": no source; dup of Scryer 28912 (enchant 2993)
        35731,  // "Greater Inscription of the Orb": no source; dup of Scryer 28909 (enchant 2995)
        37311,  // "Skybreaker Whip": unused beta mount-speed enchant item; no source
        37312,  // "Carrot on a Stick" (enchant item, not trinket 11122): creator spell 48534 is on no SkillLine
        37313,  // "Riding Crop" (enchant item, not trinket 25653): unused beta item; no source
        38377,  // "Dragonscale Leg Armor": removed beta LW self-only leg armor; no source
        38378,  // "Wyrmscale Leg Armor": creator spell 50969 is on no SkillLine; no trainer/formula teaches it
        41605,  // "zzDEPRECATED Sanctified Spellthread": deprecated row; no source
        41606,  // "zzDEPRECATED Master's Spellthread": deprecated row; no source
        43302,  // "Inscription of High Discipline": unused beta shoulder inscription; no source
        43303,  // "Inscription of the Frostblade": unused beta shoulder inscription; no source
        43304,  // "Inscription of Kings": unused beta shoulder inscription; no source
        44871,  // "Greater Inscription of the Axe": no source; dup of Hodir 44133 (enchant 3808)
        44872,  // "Greater Inscription of the Crag": no source; dup of Hodir 44134 (enchant 3809)
        44873,  // "Greater Inscription of the Pinnacle": no source; dup of Hodir 44136 (enchant 3811)
        44874,  // "Greater Inscription of the Storm": no source; dup of Hodir 44135 (enchant 3810)
        44875,  // "Arcanum of the Savage Gladiator": no source; dup of 44701/44702 (enchant 3842)
        44876,  // "Arcanum of Blissful Mending": no source; dup of 44152 (enchant 3819)
        44877,  // "Arcanum of Burning Mysteries": no source; dup of 44159 (enchant 3820)
        44878,  // "Arcanum of the Stalwart Protector": no source; dup of 44150 (enchant 3818)
        44879,  // "Arcanum of Torment": no source; dup of 44149 (enchant 3817)
        44880,  // "Arcanum of the Flame's Soul": no source; dup of 44141 (enchant 3816)
        44881,  // "Arcanum of the Eclipsed Moon": no source; dup of 44140 (enchant 3815)
        44882,  // "Arcanum of the Fleeing Shadow": no source; dup of 44139 (enchant 3814)
        44883,  // "Arcanum of Toxic Warding": no source; dup of 44138 (enchant 3813)
        44884,  // "Arcanum of the Frosty Soul": no source; dup of 44137 (enchant 3812)
    };

    // Authored era corrections, applied after every automatic rule. Keyed by spellId for recipes and
    // itemId for upgrade items; one evidence comment per row.
    std::unordered_map<uint32, uint8> const kEraOverride =
    {
        { 23530, EnchanterRules::ERA_TBC },   // "Felsteel Shield Spike": TBC blacksmithing; id < 23728 and RequiredLevel 0 fool the gear proxy
        // AQ40 glove recipes: formulas 20727-20730 drop only in AQ20/AQ40 (reference loot 30411/34045,
        // maps 509/531), but the 3.3.5 Spell.dbc lists Small Prismatic Shard (22448, a TBC mat) as a
        // reagent, so the material rule would push these Vanilla raid recipes into TBC.
        { 25073, EnchanterRules::ERA_VANILLA },   // "Enchant Gloves - Shadow Power"
        { 25074, EnchanterRules::ERA_VANILLA },   // "Enchant Gloves - Frost Power"
        { 25078, EnchanterRules::ERA_VANILLA },   // "Enchant Gloves - Fire Power"
        { 25079, EnchanterRules::ERA_VANILLA },   // "Enchant Gloves - Healing Power"
        { 63746, EnchanterRules::ERA_WOTLK },     // "Enchant Boots - Lesser Accuracy": added in 3.1 (spell id in the Ulduar-patch
                                                  // range beside Blade Ward 64441); trainer rank 225 + Vanilla mats hide it
    };

    bool IsJunkName(std::string n)
    {
        std::transform(n.begin(), n.end(), n.begin(), [](unsigned char c) { return char(std::tolower(c)); });
        return n.rfind("deprecated", 0) == 0 || n.rfind("test", 0) == 0 || n.rfind("monster -", 0) == 0
            || n.rfind("zz", 0) == 0 || n.find("[ph]") != std::string::npos;
    }

    bool PermanentEnchantOf(SpellInfo const* si, uint32& enchantId)
    {
        for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
        {
            if (si->Effects[i].Effect != SPELL_EFFECT_ENCHANT_ITEM)
                continue;
            uint32 id = uint32(si->Effects[i].MiscValue);
            if (sSpellItemEnchantmentStore.LookupEntry(id))
            {
                enchantId = id;
                return true;
            }
        }
        return false;
    }

    // Crafter-only enchants: the enchant itself names a skill (rings, embroideries, fur linings,
    // tinkers); else a non-Enchanting recipe is self-only; else the upgrade item's own skill need.
    void SetPerk(EnchantOffer& o, uint32 itemReqSkill, uint32 itemReqSkillRank)
    {
        SpellItemEnchantmentEntry const* e = sSpellItemEnchantmentStore.LookupEntry(o.enchantId);
        if (e && e->requiredSkill)
        {
            o.perkSkill = e->requiredSkill;
            o.perkSkillValue = e->requiredSkillValue;
        }
        else if (o.kind == EnchantSource::RECIPE && o.skillLine != SKILL_ENCHANTING)
        {
            o.perkSkill = o.skillLine;
            o.perkSkillValue = o.rank;
        }
        else if (itemReqSkill)
        {
            o.perkSkill = itemReqSkill;
            o.perkSkillValue = itemReqSkillRank;
        }
    }

    std::string JoinIds(std::vector<uint32> const& ids)
    {
        std::string s;
        for (uint32 id : ids)
        {
            if (!s.empty())
                s += ',';
            s += std::to_string(id);
        }
        return s;
    }

    // item -> lowest tier over its loot/vendor/quest sources (EnchanterRules::kTierUnset = none seen).
    std::unordered_map<uint32, uint8> SourceTiers(std::vector<uint32> const& items)
    {
        std::unordered_map<uint32, uint8> tier;
        for (uint32 id : items)
            tier[id] = EnchanterRules::kTierUnset;
        if (items.empty())
            return tier;

        std::string const in = JoinIds(items);

        // (item, map) for every creature drop (direct or one reference level deep), chest drop and
        // vendor spawn. The server's creature/gameobject tables use `id`, not `id1`.
        if (QueryResult r = WorldDatabase.Query(
                "SELECT CAST(l.Item AS UNSIGNED), CAST(c.map AS UNSIGNED) FROM creature_loot_template l "
                "JOIN creature_template ct ON ct.lootid = l.Entry JOIN creature c ON c.id = ct.entry "
                "WHERE l.Reference = 0 AND l.Item IN ({0}) "
                "UNION SELECT CAST(rl.Item AS UNSIGNED), CAST(c.map AS UNSIGNED) FROM reference_loot_template rl "
                "JOIN creature_loot_template l ON l.Reference = rl.Entry "
                "JOIN creature_template ct ON ct.lootid = l.Entry JOIN creature c ON c.id = ct.entry "
                "WHERE rl.Item IN ({0}) "
                "UNION SELECT CAST(l.Item AS UNSIGNED), CAST(g.map AS UNSIGNED) FROM gameobject_loot_template l "
                "JOIN gameobject_template gt ON gt.type = 3 AND gt.Data1 = l.Entry JOIN gameobject g ON g.id = gt.entry "
                "WHERE l.Reference = 0 AND l.Item IN ({0}) "
                "UNION SELECT CAST(v.item AS UNSIGNED), CAST(c.map AS UNSIGNED) FROM npc_vendor v "
                "JOIN creature c ON c.id = v.entry WHERE v.item IN ({0})", in))
        {
            do
            {
                Field* f = r->Fetch();
                uint32 item = uint32(f[0].Get<uint64>());
                uint32 map  = uint32(f[1].Get<uint64>());
                EnchanterRules::FoldTier(tier[item], EnchanterEra::TierForMap(map));
            } while (r->NextRow());
        }

        // Quest rewards are always open.
        if (QueryResult r = WorldDatabase.Query(
                "SELECT CAST(x AS UNSIGNED) FROM ("
                "SELECT RewardItem1 x FROM quest_template UNION SELECT RewardItem2 FROM quest_template "
                "UNION SELECT RewardItem3 FROM quest_template UNION SELECT RewardItem4 FROM quest_template "
                "UNION SELECT RewardChoiceItemID1 FROM quest_template UNION SELECT RewardChoiceItemID2 FROM quest_template "
                "UNION SELECT RewardChoiceItemID3 FROM quest_template UNION SELECT RewardChoiceItemID4 FROM quest_template "
                "UNION SELECT RewardChoiceItemID5 FROM quest_template UNION SELECT RewardChoiceItemID6 FROM quest_template"
                ") t WHERE x IN ({})", in))
        {
            do
            {
                EnchanterRules::FoldTier(tier[uint32(r->Fetch()[0].Get<uint64>())], 0);
            } while (r->NextRow());
        }
        return tier;
    }
}

std::vector<EnchantOffer> const& EnchanterCatalog::Offers() { return g_offers; }
std::vector<uint32> const& EnchanterCatalog::PricedItems() { return g_pricedItems; }
bool EnchanterCatalog::SoldByVendorForGold(uint32 entry) { return g_goldVendorItems.count(entry) != 0; }

void EnchanterCatalog::Build()
{
    uint32 const startMs = getMSTime();
    g_offers.clear();
    g_pricedItems.clear();
    g_goldVendorItems.clear();

    ItemTemplateContainer const& itemStore = *sObjectMgr->GetItemTemplateStore();

    // --- Items some profession recipe creates (Source 2 sells only these).
    std::unordered_set<uint32> craftedItems;
    for (SkillLineAbilityEntry const* a : sSkillLineAbilityStore)
    {
        switch (a->SkillLine)
        {
            case SKILL_BLACKSMITHING: case SKILL_LEATHERWORKING: case SKILL_ALCHEMY:
            case SKILL_TAILORING: case SKILL_ENGINEERING: case SKILL_ENCHANTING:
            case SKILL_JEWELCRAFTING: case SKILL_INSCRIPTION:
                break;
            default:
                continue;
        }
        SpellInfo const* si = sSpellMgr->GetSpellInfo(a->Spell);
        if (!si)
            continue;
        for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
            if (si->Effects[i].Effect == SPELL_EFFECT_CREATE_ITEM && si->Effects[i].ItemType)
                craftedItems.insert(si->Effects[i].ItemType);
    }

    // --- Formula items, indexed by the recipe they teach, with the lowest nonzero learn rank.
    std::unordered_map<uint32, std::vector<uint32>> formulasBySpell;
    std::unordered_map<uint32, uint32> formulaRankBySpell;
    for (auto const& [entry, proto] : itemStore)
    {
        if (proto.Class != ITEM_CLASS_RECIPE)
            continue;
        for (auto const& sp : proto.Spells)
        {
            if (sp.SpellId <= 0 || sp.SpellTrigger != ITEM_SPELLTRIGGER_LEARN_SPELL_ID)
                continue;
            uint32 const spell = uint32(sp.SpellId);
            formulasBySpell[spell].push_back(entry);
            if (proto.RequiredSkillRank)
            {
                uint32& best = formulaRankBySpell[spell];
                if (!best || proto.RequiredSkillRank < best)
                    best = proto.RequiredSkillRank;
            }
        }
    }

    // --- Trainer-taught spells with their lowest ReqSkillRank. Present in the map = an open source.
    std::unordered_map<uint32, uint32> trainerRankBySpell;
    if (QueryResult r = WorldDatabase.Query("SELECT SpellId, MIN(ReqSkillRank) FROM trainer_spell GROUP BY SpellId"))
    {
        do
        {
            Field* f = r->Fetch();
            trainerRankBySpell[f[0].Get<uint32>()] = f[1].Get<uint32>();
        } while (r->NextRow());
    }

    // --- Source 1: profession recipes. Keep the lowest-rank recipe per enchant id. The rank is the
    // real learn requirement (trainer, else formula, else SkillLineAbility); MinSkillLineRank alone
    // is 1 for nearly every recipe in this DBC.
    std::unordered_map<uint32, size_t> recipeByEnchant;
    std::unordered_set<uint32> openAcquire;   // recipe spells learned on skill-up (no formula)
    std::unordered_set<uint32> rankUnknown;   // SkillLine rank on a recipe that is not learned on skill-up
    for (uint32 skill : kRecipeSkills)
    {
        for (SkillLineAbilityEntry const* a : GetSkillLineAbilitiesBySkillLine(skill))
        {
            SpellInfo const* si = sSpellMgr->GetSpellInfo(a->Spell);
            uint32 enchantId = 0;
            if (!si || !PermanentEnchantOf(si, enchantId))
                continue;

            auto const tr = trainerRankBySpell.find(si->Id);
            uint32 const trainerRank = tr != trainerRankBySpell.end() ? tr->second : 0;
            auto const fr = formulaRankBySpell.find(si->Id);
            uint32 const formulaRank = fr != formulaRankBySpell.end() ? fr->second : 0;

            EnchantOffer o;
            o.enchantId = enchantId;
            o.spellId = si->Id;
            o.kind = EnchantSource::RECIPE;
            o.skillLine = skill;
            o.rank = EnchanterRules::PickRecipeRank(trainerRank, formulaRank, a->MinSkillLineRank);
            o.rankSource = EnchanterRules::PickRecipeRankSource(trainerRank, formulaRank);
            o.era = EnchanterRules::EraForRecipeRank(o.rank);
            o.name = si->SpellName[0] ? si->SpellName[0] : "";
            for (uint8 r = 0; r < MAX_SPELL_REAGENTS; ++r)
                if (si->Reagent[r] > 0 && si->ReagentCount[r] > 0)
                    o.reagents.emplace_back(uint32(si->Reagent[r]), si->ReagentCount[r]);
            // A recipe is no earlier than the expansion that introduced any of its materials.
            for (auto const& [reagent, count] : o.reagents)
                o.era = std::max<uint8>(o.era, EnchanterRules::MaterialEra(reagent));
            SetPerk(o, 0, 0);
            if (a->AcquireMethod)
                openAcquire.insert(si->Id);
            else if (o.rankSource == EnchanterRules::RANK_SKILLLINE)
                rankUnknown.insert(si->Id);

            auto it = recipeByEnchant.find(enchantId);
            if (it == recipeByEnchant.end())
            {
                recipeByEnchant[enchantId] = g_offers.size();
                g_offers.push_back(std::move(o));
            }
            else if (o.rank < g_offers[it->second].rank)
                g_offers[it->second] = std::move(o);
        }
    }
    size_t const recipeCount = g_offers.size();
    uint32 unknownRank = 0;
    for (size_t i = 0; i < recipeCount; ++i)
        if (rankUnknown.count(g_offers[i].spellId))
            ++unknownRank;

    // --- Source 2: on-use upgrade items, but ONLY ones a profession recipe crafts (leg armors,
    // spellthreads, armor kits, scopes, shield spikes, weapon chains). The NPC acts like a player
    // crafter: quest/reputation/turn-in items (ZG signets and idols, Dire Maul and Kirin Tor
    // arcanums, Naxxramas/Argent Dawn, Aldor/Scryer and Sons of Hodir shoulder inscriptions, ...)
    // stay with their vendors and quest givers.
    uint32 nonCrafted = 0;
    for (auto const& [entry, proto] : itemStore)
    {
        if (proto.Class == ITEM_CLASS_RECIPE || kExcludedItems.count(entry) || IsJunkName(proto.Name1))
            continue;
        for (auto const& sp : proto.Spells)
        {
            if (sp.SpellId <= 0 || sp.SpellTrigger != ITEM_SPELLTRIGGER_ON_USE)
                continue;
            SpellInfo const* si = sSpellMgr->GetSpellInfo(uint32(sp.SpellId));
            uint32 enchantId = 0;
            if (!si || !PermanentEnchantOf(si, enchantId))
                continue;
            if (!craftedItems.count(entry))
            {
                ++nonCrafted;
                break;   // not a crafter's product: next item
            }
            if (recipeByEnchant.count(enchantId))
                break;   // a recipe already sells it (this also drops the Enchanting vellum scrolls)

            EnchantOffer o;
            o.enchantId = enchantId;
            o.spellId = si->Id;
            o.itemId = entry;
            o.kind = EnchantSource::UPGRADE_ITEM;
            o.rank = EnchanterRules::RankForUpgradeItem(proto.RequiredLevel);
            o.era = EnchanterRules::EraForUpgradeItem(proto.RequiredLevel, entry);
            o.name = proto.Name1;
            o.reagents.emplace_back(entry, 1);
            SetPerk(o, proto.RequiredSkill, proto.RequiredSkillRank);
            g_offers.push_back(std::move(o));
            break;
        }
    }

    // --- Authored era corrections, last.
    for (EnchantOffer& o : g_offers)
    {
        auto ov = kEraOverride.find(o.kind == EnchantSource::UPGRADE_ITEM ? o.itemId : o.spellId);
        if (ov != kEraOverride.end())
            o.era = ov->second;
    }

    // --- IP tiers from sources: lowest tier over every source; unknown sources are ignored, and
    // an offer with no known source at all stays open (we only ever err toward open).
    std::vector<uint32> sourceItems;
    for (EnchantOffer const& o : g_offers)
    {
        if (o.kind == EnchantSource::UPGRADE_ITEM)
            sourceItems.push_back(o.itemId);
        else if (auto f = formulasBySpell.find(o.spellId); f != formulasBySpell.end())
            sourceItems.insert(sourceItems.end(), f->second.begin(), f->second.end());
    }
    std::sort(sourceItems.begin(), sourceItems.end());
    sourceItems.erase(std::unique(sourceItems.begin(), sourceItems.end()), sourceItems.end());
    std::unordered_map<uint32, uint8> const itemTier = SourceTiers(sourceItems);

    uint32 tiered = 0;
    for (EnchantOffer& o : g_offers)
    {
        uint8 acc = EnchanterRules::kTierUnset;
        if (o.kind == EnchantSource::UPGRADE_ITEM)
        {
            auto t = itemTier.find(o.itemId);
            if (t != itemTier.end())
                EnchanterRules::FoldTier(acc, t->second);
        }
        else
        {
            if (trainerRankBySpell.count(o.spellId) || openAcquire.count(o.spellId))
                EnchanterRules::FoldTier(acc, 0);
            if (auto f = formulasBySpell.find(o.spellId); f != formulasBySpell.end())
                for (uint32 formula : f->second)
                    if (auto t = itemTier.find(formula); t != itemTier.end())
                        EnchanterRules::FoldTier(acc, t->second);
        }
        o.ipTier = EnchanterRules::ResolveTier(acc);
        if (o.ipTier)
            ++tiered;
    }

    // --- Priced item set.
    std::unordered_set<uint32> priced;
    for (EnchantOffer const& o : g_offers)
        for (auto const& [entry, count] : o.reagents)
            priced.insert(entry);
    g_pricedItems.assign(priced.begin(), priced.end());

    // --- Priced items a vendor sells for plain gold in unlimited supply (ExtendedCost 0, maxcount 0):
    // the price fallback trusts BuyPrice only for these. Limited-stock specials (Darkmoon Faire Black
    // Lotus, enchanting-supplier dust) are not a market price.
    g_goldVendorItems.clear();
    if (!g_pricedItems.empty())
        if (QueryResult r = WorldDatabase.Query(
                "SELECT DISTINCT CAST(item AS UNSIGNED) FROM npc_vendor WHERE ExtendedCost = 0 AND maxcount = 0 AND item IN ({})",
                JoinIds(g_pricedItems)))
        {
            do
            {
                g_goldVendorItems.insert(uint32(r->Fetch()[0].Get<uint64>()));
            } while (r->NextRow());
        }

    uint32 byEra[2][3] = { };
    for (EnchantOffer const& o : g_offers)
        ++byEra[o.kind == EnchantSource::UPGRADE_ITEM ? 1 : 0][std::min<uint8>(o.era, 2)];
    LOG_INFO("server.loading",
        "[Enchanter] Catalog: {} recipes (V/T/W {}/{}/{}), {} upgrade items (V/T/W {}/{}/{}), "
        "{} non-crafted upgrade items skipped, {} rank-unknown, {} tier-gated, {} priced items, built in {} ms",
        recipeCount, byEra[0][0], byEra[0][1], byEra[0][2],
        g_offers.size() - recipeCount, byEra[1][0], byEra[1][1], byEra[1][2], nonCrafted,
        unknownRank, tiered, g_pricedItems.size(), GetMSTimeDiffToNow(startMs));
}

bool EnchanterCatalog::Eligible(Player* player, uint8 playerEra, Item* item, EnchantOffer const& o)
{
    ItemTemplate const* ip = item->GetTemplate();
    SpellInfo const* si = sSpellMgr->GetSpellInfo(o.spellId);
    if (!ip || !si)
        return false;
    if (o.era > playerEra)
        return false;
    if (!EnchanterEra::TierUnlocked(player, o.ipTier))
        return false;
    if (!item->IsFitToSpellRequirements(si))
        return false;
    if (SpellItemEnchantmentEntry const* ench = sSpellItemEnchantmentStore.LookupEntry(o.enchantId))
    {
        // ApplyEnchantment silently skips an enchant above the wearer's level.
        if (ench->requiredLevel > player->GetLevel())
            return false;
        // Spell::CheckCast SPELL_FAILED_ON_USE_ENCHANT: no on-use enchant on an item that already
        // has an on-use effect.
        bool isItemUsable = false;
        for (uint8 e = 0; e < MAX_ITEM_PROTO_SPELLS; ++e)
        {
            if (ip->Spells[e].SpellId && (ip->Spells[e].SpellTrigger == ITEM_SPELLTRIGGER_ON_USE
                    || ip->Spells[e].SpellTrigger == ITEM_SPELLTRIGGER_ON_NO_DELAY_USE))
            {
                isItemUsable = true;
                break;
            }
        }
        if (isItemUsable)
            for (uint8 s = 0; s < MAX_SPELL_ITEM_ENCHANTMENT_EFFECTS; ++s)
                if (ench->type[s] == ITEM_ENCHANTMENT_TYPE_USE_SPELL)
                    return false;
    }
    if (!EnchanterRules::PassesItemLevelRule(ip->RequiredLevel, ip->ItemLevel, si->BaseLevel, si->MaxLevel,
            si->HasAttribute(SPELL_ATTR2_ALLOW_LOW_LEVEL_BUFF), o.kind == EnchantSource::UPGRADE_ITEM))
        return false;
    if (o.perkSkill && player->GetSkillValue(o.perkSkill) < o.perkSkillValue)
        return false;
    if (o.kind == EnchantSource::UPGRADE_ITEM)
    {
        ItemTemplate const* up = sObjectMgr->GetItemTemplate(o.itemId);
        if (!up || player->CanUseItem(up) != EQUIP_ERR_OK)
            return false;
        if (up->RequiredReputationFaction
            && uint32(player->GetReputationRank(up->RequiredReputationFaction)) < up->RequiredReputationRank)
            return false;
    }
    if (item->GetEnchantmentId(PERM_ENCHANTMENT_SLOT) == o.enchantId)
        return false;   // already wearing it
    return true;
}

std::vector<uint32> EnchanterCatalog::EligibleFor(Player* player, Item* item)
{
    std::vector<uint32> out;
    if (!player || !item)
        return out;
    uint8 const era = EnchanterEra::PlayerEra(player);
    for (uint32 i = 0; i < g_offers.size(); ++i)
        if (Eligible(player, era, item, g_offers[i]))
            out.push_back(i);
    std::sort(out.begin(), out.end(), [](uint32 a, uint32 b)
    {
        EnchantOffer const& x = g_offers[a];
        EnchantOffer const& y = g_offers[b];
        if (x.rank != y.rank)
            return x.rank > y.rank;
        if (x.name != y.name)
            return x.name < y.name;
        return a < b;
    });
    std::unordered_set<uint32> seen;
    out.erase(std::remove_if(out.begin(), out.end(),
        [&](uint32 i) { return !seen.insert(g_offers[i].enchantId).second; }), out.end());
    return out;
}
