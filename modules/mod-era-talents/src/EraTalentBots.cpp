#include "EraTalentBots.h"
#include "EraTalents.h"
#include "EraTalentContent.h"
#include "EraTalentsConfig.h"
#include "EraGlyphGate.h"
#include "Player.h"
#include "Log.h"
#include "Playerbots.h"          // GET_PLAYERBOT_AI — the bot test
#include "PlayerbotFactory.h"    // native re-init on a band crossing (hard dep, like roster->IP)
#include "SpellMgr.h"            // resolver: stock-name lookup + bot spell-map scan
#include "SpellInfo.h"
#include "Util.h"                // Utf8toWStr / wstrToLower / Utf8FitTo (SpellIdValue's pattern)
#include "DatabaseEnv.h"         // trainer ReqLevel index (WorldDatabase, built once at startup)
#include "QueryResult.h"         // DatabaseEnv.h only forward-declares ResultSet
#include "Field.h"
#include <algorithm>
#include <array>
#include <map>
#include <mutex>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace EraTalentBots
{
    // Session flag first: the bot AI attaches only AFTER core login completes, so during
    // OnPlayerLogin GET_PLAYERBOT_AI is still null for a logging-in bot — without the session
    // test, OnBotLogin would gate itself out at the exact moment it must run (era passive
    // reapply), and EraFor would misread a logging-in bot as a player (glyph gate stripped
    // WotLK-band bots' glyphs at login via EraFromIP==Vanilla).
    // Public (declared in the header) since 2026-09-07: EraAdvanceGossip.cpp uses it too.
    bool IsBot(Player* p) { return p && (p->GetSession()->IsBot() || GET_PLAYERBOT_AI(p) != nullptr); }
}

namespace
{
    using EraTalentBots::IsBot;

    // Every entry point bails here FIRST (module-hooks gotcha: guard before any deref).
    bool Gated(Player* p) { return !sEraTalentsConfig->Enabled() || !sEraTalentsConfig->BotTalents() || !IsBot(p); }

    // Spec-tab cache backing the patch-0020 AiFactory::GetPlayerSpecTabs bridge. That function
    // runs on per-second AI paths (raid triggers, chat formatting, gear scoring) while the era
    // rows live in the characters DB — NEVER query per call. One SELECT on first miss, then
    // pure memory; invalidated after every era-row mutation in this unit and on logout.
    std::unordered_map<ObjectGuid, std::array<uint32, 3>> g_specTabsCache;

    // Resolve cache backing the patch-0021 EraTalentBots_ResolveSpellId bridge: (guid, stock
    // id) -> resolved id (0 cached too — "doesn't know it" must not re-scan every AI tick).
    std::unordered_map<ObjectGuid, std::unordered_map<uint32, uint32>> g_resolveCache;

    // Guards BOTH caches. Bot AI ticks inside Map::Update, and maps update in PARALLEL on the
    // MapUpdater pool — so these bridges are called concurrently from different map threads
    // (plus world-thread invalidations). The original "world-thread only, no locking needed"
    // assumption was wrong: a cross-thread operator[] rehash race on g_resolveCache segfaulted
    // prod inside _M_find_before_node (2026-08-26, kernel-log + addr2line confirmed). Lock
    // discipline: hold only around cache lookups/inserts/erases — compute misses OUTSIDE the
    // lock (a benign double-compute beats holding it across EraTalents::Ranks, which takes the
    // rank-cache mutex; never nest the two).
    std::mutex g_botCacheMutex;

    void InvalidateSpecTabs(Player* bot)
    {
        if (!bot)
            return;
        std::lock_guard<std::mutex> guard(g_botCacheMutex);
        g_specTabsCache.erase(bot->GetGUID());
        g_resolveCache.erase(bot->GetGUID());   // spellbook mutations track era-row mutations
    }

    // ---------------------------------------------------------------------------------------
    // Trainer ReqLevel index: SpellId -> MIN(trainer_spell.ReqLevel) over every trainer row.
    //
    // WHY THIS EXISTS (bracket DOWN-move residue, bots only). A bot moved DOWN a level bracket
    // (mod-player-bot-level-brackets, or a `.character level` in testing) keeps every
    // character_spell row it accumulated at the higher level, and nothing owned the stock ones:
    //   * node-granted STOCK spells are covered by EraTalents::Reset (TeardownStale's first arm);
    //   * everything in the era band [920000,950000) is covered by the Phase 9.5 band sweep
    //     (EraBandClassifier) — and this index deliberately EXCLUDES band ids so the two can
    //     never fight over the same spell (the "never removeSpell(<band id>)" invariant);
    //   * a TRAINER-TAUGHT stock spell above the new band's level ceiling had NO owner at all.
    // Measured case: stock Frostfire Bolt 42985 (trainer ReqLevel 77) still known by TBC-band
    // (level 61-70) mage bots after an 80 -> 70 move, i.e. a bot casting a WotLK spell in a TBC
    // band. Gear residue is a KNOWN, ACCEPTED gap (the factory re-gears on its own cadence and
    // an ilvl sweep is a much bigger hammer) — this covers the spellbook only.
    //
    // Built ONCE at worldserver startup (EraTalentBots::BuildTrainerLevelIndex, called from the
    // module's WorldScript::OnStartup) and READ-ONLY thereafter, so the map-thread readers need
    // no lock — unlike the two mutable caches above, nothing ever writes it again.
    std::unordered_map<uint32, uint32> g_trainerMinReqLevel;

    // Companion index, built in the same startup pass: every STOCK spell id that some node in
    // (era, class) grants, keyed (era << 8 | class). ~150 DISTINCT stock ids across era-data/
    // are node grants (the boot canary prints the (era,class)-scoped tuple count instead: 504
    // today), and a node grant is the era system's own property — it must NEVER be strippable by
    // the trainer-level rule, whatever ReqLevel a trainer happens to carry for it. Ice Block 45438
    // is the worked example: a Vanilla node-18046 grant that is ALSO trainer-taught at 30, so the
    // trainer row's level never exceeded a Vanilla bot's and the rule missed it only by accident.
    // This makes the exclusion STRUCTURAL instead of incidental. Band ids never reach here (the
    // trainer index already drops them — the Phase 9.5 sweep owns those).
    std::unordered_map<uint32, std::unordered_set<uint32>> g_eraNodeGrantStock;

    inline uint32 EraClassKey(EraId era, uint8 classId) { return (uint32(era) << 8) | classId; }

    bool IsEraNodeGrantedStock(EraId era, uint8 classId, uint32 spellId)
    {
        auto it = g_eraNodeGrantStock.find(EraClassKey(era, classId));
        return it != g_eraNodeGrantStock.end() && it->second.count(spellId) != 0;
    }

    // Drop trainer-taught STOCK spells the bot's (possibly reduced) level can no longer justify.
    // Deliberately unconditional rather than direction-detecting: a bot AT or ABOVE a spell's
    // trainer level holds nothing that qualifies, so this is a pure no-op on an up-move / a normal
    // level-up, and a down-move is the only case it can fire on.
    //
    // MEASURED 2026-09-06: PlayerbotFactory::InitAvailableSpells does NOT honour trainer ReqLevel,
    // so it RE-TEACHES above-level spells on every walk — including the walk in the AI-tick action
    // AutoMaintenanceOnLevelupAction::LearnTrainerSpells, which runs AFTER the whole factory pass.
    // That is why this also hangs off the patch-0020 post-trainer-walk bridge and not only off
    // TeardownStale: a TeardownStale-only version left 9 of Drimdinn's spells re-taught at 69.
    void StripAboveLevelTrainerSpells(Player* bot)
    {
        if (!bot || g_trainerMinReqLevel.empty())
            return;
        uint8 const level = bot->GetLevel();
        EraId const era = EraTalentBots::BotEra(bot);   // this arm is bots-only (Gated upstream)
        uint8 const classId = bot->getClass();
        std::vector<uint32> doomed;   // collect first — removeSpell mutates the map we are walking
        for (auto const& [spellId, pSpell] : bot->GetSpellMap())
        {
            // Deliberately NOT filtered on `Active`. The criterion is "holds a character_spell
            // row", and `active` is not even persisted (character_spell is guid/spell/specMask) —
            // core recomputes it at load, marking a lower rank INACTIVE while a higher rank of the
            // same chain is known. Draining a two-rank above-level chain in ONE pass needs both.
            //
            // WHAT THIS DOES *NOT* FIX, so nobody re-opens it: after the strip, above-level rows
            // whose chain root is a TALENT spell REMAIN in character_spell with specMask 0 — core's
            // removeSpell retains those rather than marking them REMOVED (the standing specMask-
            // retention gotcha). Measured on Eragon at 70: 42890/42891 (Pyroblast r11/r12),
            // 42944 (Blast Wave r8), 42949 (Dragon's Breath r5), 43038/43039 (Ice Barrier r7/r8),
            // 55360 (Living Bomb r3) all sit at specMask 0. HasSpell is FALSE for every one of
            // them, so the bot does not know or cast them — a raw character_spell row count is the
            // wrong way to audit this; join on `specMask <> 0`.
            if (!pSpell || pSpell->State == PLAYERSPELL_REMOVED)
                continue;
            auto it = g_trainerMinReqLevel.find(spellId);
            if (it == g_trainerMinReqLevel.end() || it->second <= uint32(level))
                continue;
            if (IsEraNodeGrantedStock(era, classId, spellId))
                continue;   // the era system owns it — never strippable by a trainer's ReqLevel
            doomed.push_back(spellId);
        }
        for (uint32 spellId : doomed)
            bot->removeSpell(spellId, SPEC_MASK_ALL, false);
        if (!doomed.empty())
            LOG_DEBUG("module", "[mod-era-talents] bot {} (level {}): stripped {} trainer-taught spell(s) above level",
                bot->GetName(), uint32(level), uint32(doomed.size()));
    }

    // Strip rows + granted clones for every managed era the bot is NOT currently in, then drop
    // trainer-taught STOCK spells the bot's (possibly reduced) level can no longer justify.
    // Covers bracket up-moves and band crossings; a bot with no rows is a cheap no-op.
    void TeardownStale(Player* bot, EraId current)
    {
        for (uint8 e = 0; e <= uint8(ERA_WOTLK); ++e)
        {
            EraId era = EraId(e);
            if (era == current || !EraHasTalentTrees(era))
                continue;
            if (EraTalents::SpentPoints(bot, era) > 0)
            {
                EraTalents::Reset(bot, era);
                InvalidateSpecTabs(bot);
            }
        }

        StripAboveLevelTrainerSpells(bot);
    }

    // TBC faction DPS seals — the shared arm behind OnBotLevelChanged and the patch-0020
    // post-trainer-walk bridge. The factory's trainer walk (InitAvailableSpells) is class-only
    // gated, so a TBC-band paladin bot learns BOTH Seal of Blood (946080, Horde/BE) and Seal of
    // Vengeance (946084, Alliance) every time it runs, AFTER the mid-build reconcile has already
    // stripped the wrong one. Readiness is the same question EraTalents.cpp's file-static
    // EraReplacementSpellReady asks (that helper is not exported): with TBC outside the
    // implemented-era allowlist, or the clone absent from spell_dbc, a TBC paladin is on the
    // native frame and there is nothing of ours to strip.
    bool TbcPalSealReady()
    {
        return EraHasTalentTrees(ERA_TBC) && sSpellMgr->GetSpellInfo(946080) != nullptr;
    }

    void StripTbcSealIfPaladin(Player* bot, EraId era)
    {
        if (!bot || era != ERA_TBC || bot->getClass() != CLASS_PALADIN || !TbcPalSealReady())
            return;
        EraTalents::StripWrongFactionTbcPalSeal(bot);
    }

    // Authored build orders: (era, class, 0-based specTab) -> node ids learned to maxRank in
    // order. FULL COVERAGE of every dataset in era-data/datasets.txt (Vanilla: all 9 classes;
    // TBC: each class as its tree ships — the tool FAILS on a manifest dataset with no order),
    // x 3 tabs + druid's specTab-3 "cat" pseudo-spec. The greedy fallback below tier-fills,
    // which maxes low-tier filler before keystones — a level-48 shadow priest stalled below
    // Vampiric Embrace/Shadowform (live report, 2026-08-26). The key carries the ERA because
    // node ids are per-era (Vanilla 18xxx / TBC 20xxx): a class-only key silently missed for
    // every TBC-band bot and every one of them greedy-filled (workflow lesson 10, fixed
    // 2026-09-03). Every order is validated SKIP-FREE against TryLearn's exact rules (same-era
    // node, in-tab prereqPoints, prereqTalent-at-max) by tools/gen_bot_builds.py: skip-free at
    // unlimited budget means a level cutoff just truncates the walk, so every level gets the
    // build's prefix. Each order covers the era's full AvailablePoints budget (Vanilla 51 /
    // TBC 61 — keystones as early as gates allow, then filler, then an off-tab spill) so the
    // top of a band never reaches the greedy fill; regenerate the table with the tool — do NOT
    // hand-edit entries.
    const std::map<std::tuple<uint8, uint8, int>, std::vector<uint32>> kAuthoredOrders = {
        // Vanilla Warrior specTab 0: improved_rend, improved_heroic_strike, tactical_mastery, deep_wounds, ...
        { { ERA_VANILLA, CLASS_WARRIOR, 0 }, { 18503, 18501, 18505, 18509, 18508, 18511, 18510, 18513, 18515, 18507, 18518, 18502, 18504, 18520, 18522, 18526 } },
        // Vanilla Warrior specTab 1: cruelty, unbridled_wrath, improved_battle_shout, enrage, ...
        { { ERA_VANILLA, CLASS_WARRIOR, 1 }, { 18520, 18522, 18526, 18529, 18531, 18528, 18525, 18534, 18535, 18527, 18524, 18523, 18502, 18505, 18503 } },
        // Vanilla Warrior specTab 2: shield_specialization, anticipation, improved_shield_block, improved_revenge, ...
        { { ERA_VANILLA, CLASS_WARRIOR, 2 }, { 18536, 18537, 18542, 18543, 18544, 18545, 18549, 18538, 18541, 18551, 18552, 18547, 18539, 18502, 18505 } },
        // Vanilla Paladin specTab 0: divine_intellect, spiritual_focus, healing_light, unyielding_faith, ...
        { { ERA_VANILLA, CLASS_PALADIN, 0 }, { 18602, 18603, 18605, 18608, 18609, 18606, 18610, 18611, 18612, 18613, 18614, 18607, 18601, 18615, 18618, 18619 } },
        // Vanilla Paladin specTab 1: redoubt, improved_devotion_aura, shield_specialization, blessing_of_kings, ...
        { { ERA_VANILLA, CLASS_PALADIN, 1 }, { 18616, 18615, 18622, 18620, 18623, 18624, 18626, 18627, 18628, 18629, 18619, 18617, 18601, 18602 } },
        // Vanilla Paladin specTab 2: improved_blessing_of_might, benediction, seal_of_command, conviction, ...
        { { ERA_VANILLA, CLASS_PALADIN, 2 }, { 18630, 18631, 18637, 18636, 18632, 18634, 18641, 18643, 18644, 18642, 18635, 18640, 18633, 18601, 18602 } },
        // Vanilla Hunter specTab 0: improved_aspect_of_the_hawk, endurance_training, unleashed_fury, ferocity, ...
        { { ERA_VANILLA, CLASS_HUNTER, 0 }, { 18301, 18302, 18309, 18311, 18313, 18312, 18310, 18315, 18316, 18305, 18308, 18307, 18318, 18320, 18321, 18322 } },
        // Vanilla Hunter specTab 1: efficiency, lethal_shots, aimed_shot, improved_arcane_shot, ...
        { { ERA_VANILLA, CLASS_HUNTER, 1 }, { 18318, 18320, 18321, 18322, 18325, 18327, 18326, 18329, 18330, 18323, 18324, 18319, 18333, 18334, 18338 } },
        // Vanilla Hunter specTab 2: deflection, savage_strikes, improved_wing_clip, survivalist, ...
        { { ERA_VANILLA, CLASS_HUNTER, 2 }, { 18333, 18335, 18336, 18338, 18337, 18340, 18341, 18343, 18345, 18346, 18339, 18344, 18334, 18318, 18320, 18321 } },
        // Vanilla Rogue specTab 0: malice, ruthlessness, murder, relentless_strikes, ...
        { { ERA_VANILLA, CLASS_ROGUE, 0 }, { 18403, 18404, 18405, 18407, 18409, 18410, 18412, 18411, 18414, 18415, 18401, 18402, 18417, 18418, 18421, 18420 } },
        // Vanilla Rogue specTab 1: improved_sinister_strike, lightning_reflexes, precision, deflection, ...
        { { ERA_VANILLA, CLASS_ROGUE, 1 }, { 18417, 18418, 18421, 18420, 18423, 18427, 18430, 18429, 18433, 18432, 18434, 18416, 18422, 18403, 18404, 18405, 18407 } },
        // Vanilla Rogue specTab 2: master_of_deception, opportunity, initiative, improved_ambush, ...
        { { ERA_VANILLA, CLASS_ROGUE, 2 }, { 18435, 18436, 18440, 18442, 18443, 18445, 18447, 18449, 18448, 18450, 18446, 18451, 18439, 18417, 18418, 18421 } },
        // Vanilla Priest specTab 0: wand_specialization, improved_power_word_shield, improved_power_word_fortitude, meditation, ...
        { { ERA_VANILLA, CLASS_PRIEST, 0 }, { 18102, 18105, 18104, 18108, 18107, 18103, 18110, 18112, 18113, 18114, 18115, 18109, 18118, 18120, 18117 } },
        // Vanilla Priest specTab 1: holy_specialization, divine_fury, improved_renew, blessed_recovery, ...
        { { ERA_VANILLA, CLASS_PRIEST, 1 }, { 18118, 18120, 18117, 18122, 18125, 18123, 18128, 18129, 18130, 18131, 18124, 18121, 18116, 18102, 18105, 18104, 18108 } },
        // Vanilla Priest specTab 2: spirit_tap, shadow_focus, improved_mind_blast, mind_flay, ...
        { { ERA_VANILLA, CLASS_PRIEST, 2 }, { 18132, 18136, 18138, 18139, 18135, 18142, 18144, 18145, 18146, 18147, 18141, 18134, 18133, 18102, 18104, 18105 } },
        // Vanilla Shaman specTab 0: concussion, convection, call_of_thunder, call_of_flame, ...
        { { ERA_VANILLA, CLASS_SHAMAN, 0 }, { 18801, 18800, 18807, 18804, 18805, 18810, 18812, 18811, 18813, 18814, 18806, 18803, 18832, 18831, 18836 } },
        // Vanilla Shaman specTab 1: ancestral_knowledge, thundering_strikes, two_handed_axes_and_maces, anticipation, ...
        { { ERA_VANILLA, CLASS_SHAMAN, 1 }, { 18815, 18818, 18822, 18823, 18824, 18827, 18826, 18829, 18830, 18821, 18825, 18801, 18800, 18804 } },
        // Vanilla Shaman specTab 2: improved_healing_wave, tidal_focus, totemic_focus, totemic_mastery, ...
        { { ERA_VANILLA, CLASS_SHAMAN, 2 }, { 18831, 18832, 18835, 18838, 18840, 18841, 18843, 18844, 18845, 18842, 18837, 18815, 18816 } },
        // Vanilla Mage specTab 0: arcane_focus, arcane_concentration, improved_arcane_explosion, arcane_resilience, ...
        { { ERA_VANILLA, CLASS_MAGE, 0 }, { 18002, 18006, 18008, 18009, 18005, 18012, 18013, 18014, 18015, 18016, 18003, 18001, 18017, 18019, 18024, 18018 } },
        // Vanilla Mage specTab 1: improved_fireball, ignite, pyroblast, impact, ...
        { { ERA_VANILLA, CLASS_MAGE, 1 }, { 18017, 18019, 18024, 18018, 18021, 18026, 18029, 18030, 18031, 18032, 18028, 18025, 18020, 18002, 18006, 18001 } },
        // Vanilla Mage specTab 2: improved_frostbolt, ice_shards, improved_frost_nova, piercing_ice, ...
        { { ERA_VANILLA, CLASS_MAGE, 2 }, { 18034, 18036, 18038, 18040, 18045, 18046, 18044, 18043, 18048, 18049, 18047, 18037, 18035, 18041, 18002, 18006 } },
        // Vanilla Warlock specTab 0: improved_corruption, improved_life_tap, improved_life_drain, improved_curse_of_agony, ...
        { { ERA_VANILLA, CLASS_WARLOCK, 0 }, { 18202, 18205, 18206, 18207, 18209, 18208, 18211, 18213, 18210, 18216, 18217, 18212, 18204, 18203, 18235, 18236, 18237 } },
        // Vanilla Warlock specTab 1: demonic_embrace, improved_imp, fel_intellect, fel_domination, ...
        { { ERA_VANILLA, CLASS_WARLOCK, 1 }, { 18220, 18219, 18223, 18225, 18226, 18228, 18230, 18227, 18232, 18233, 18222, 18224, 18235, 18236, 18237 } },
        // Vanilla Warlock specTab 2: improved_shadow_bolt, cataclysm, bane, devastation, ...
        { { ERA_VANILLA, CLASS_WARLOCK, 2 }, { 18235, 18236, 18237, 18241, 18242, 18248, 18243, 18244, 18246, 18247, 18249, 18250, 18238, 18202, 18205, 18206 } },
        // Vanilla Druid specTab 0: improved_wrath, improved_moonfire, natural_shapeshifter, nature_s_reach, ...
        { { ERA_VANILLA, CLASS_DRUID, 0 }, { 18700, 18704, 18706, 18709, 18710, 18712, 18713, 18707, 18714, 18711, 18715, 18701, 18702, 18732, 18735, 18733 } },
        // Vanilla Druid specTab 1: ferocity, thick_hide, feral_instinct, sharpened_claws, ...
        { { ERA_VANILLA, CLASS_DRUID, 1 }, { 18716, 18720, 18718, 18723, 18722, 18725, 18727, 18728, 18729, 18730, 18731, 18721, 18724, 18717, 18732, 18733 } },
        // Vanilla Druid specTab 2: improved_mark_of_the_wild, nature_s_focus, improved_healing_touch, insect_swarm, ...
        { { ERA_VANILLA, CLASS_DRUID, 2 }, { 18732, 18735, 18734, 18738, 18740, 18742, 18743, 18745, 18746, 18741, 18737, 18700, 18704, 18706 } },
        // Vanilla Druid specTab 3: ferocity, feral_aggression, sharpened_claws, brutal_impact, ...
        { { ERA_VANILLA, CLASS_DRUID, 3 }, { 18716, 18717, 18723, 18719, 18727, 18725, 18728, 18726, 18721, 18730, 18731, 18722, 18724, 18718, 18720, 18733, 18732 } },
        // TBC Warrior specTab 0: improved_heroic_strike, deflection, improved_charge, deep_wounds, ...
        { { ERA_TBC, CLASS_WARRIOR, 0 }, { 20300, 20301, 20303, 20308, 20307, 20302, 20310, 20309, 20312, 20314, 20318, 20319, 20320, 20321, 20322, 20306, 20316, 20324, 20326, 20325 } },
        // TBC Warrior specTab 1: cruelty, unbridled_wrath, commanding_presence, enrage, ...
        { { ERA_TBC, CLASS_WARRIOR, 1 }, { 20324, 20326, 20330, 20333, 20335, 20331, 20338, 20340, 20336, 20341, 20342, 20343, 20339, 20332, 20337, 20300, 20301, 20303, 20308 } },
        // TBC Warrior specTab 2: anticipation, shield_specialization, improved_shield_block, defiance, ...
        { { ERA_TBC, CLASS_WARRIOR, 2 }, { 20346, 20347, 20350, 20352, 20349, 20348, 20357, 20351, 20353, 20360, 20362, 20363, 20364, 20365, 20359, 20361, 20345, 20344, 20300, 20301, 20303 } },
        // TBC Paladin specTab 0: divine_intellect, spiritual_focus, healing_light, unyielding_faith, ...
        { { ERA_TBC, CLASS_PALADIN, 0 }, { 20001, 20002, 20004, 20007, 20009, 20008, 20011, 20005, 20012, 20014, 20016, 20015, 20010, 20018, 20019, 20017, 20020, 20024, 20023, 20025 } },
        // TBC Paladin specTab 1: redoubt, improved_devotion_aura, improved_righteous_fury, toughness, ...
        { { ERA_TBC, CLASS_PALADIN, 1 }, { 20021, 20020, 20026, 20024, 20028, 20033, 20027, 20036, 20038, 20039, 20037, 20040, 20041, 20035, 20023, 20043, 20046, 20044 } },
        // TBC Paladin specTab 2: benediction, improved_judgement, improved_seal_of_the_crusader, seal_of_command, ...
        { { ERA_TBC, CLASS_PALADIN, 2 }, { 20043, 20044, 20045, 20049, 20048, 20053, 20050, 20055, 20056, 20057, 20060, 20054, 20059, 20062, 20063, 20058, 20042, 20000, 20001 } },
        // TBC Hunter specTab 0: improved_aspect_of_the_hawk, endurance_training, focused_fire, thick_hide, ...
        { { ERA_TBC, CLASS_HUNTER, 0 }, { 20600, 20601, 20602, 20604, 20608, 20610, 20609, 20612, 20611, 20613, 20615, 20614, 20617, 20616, 20619, 20620, 20622, 20624, 20623 } },
        // TBC Hunter specTab 1: lethal_shots, efficiency, improved_hunter_s_mark, aimed_shot, ...
        { { ERA_TBC, CLASS_HUNTER, 1 }, { 20622, 20624, 20623, 20627, 20626, 20630, 20629, 20632, 20633, 20631, 20635, 20634, 20637, 20636, 20638, 20639, 20640, 20643 } },
        // TBC Hunter specTab 2: humanoid_slaying, hawk_eye, savage_strikes, deflection, ...
        { { ERA_TBC, CLASS_HUNTER, 2 }, { 20642, 20643, 20644, 20646, 20645, 20649, 20650, 20652, 20651, 20655, 20656, 20654, 20658, 20657, 20660, 20661, 20659, 20662, 20663, 20622, 20600 } },
        // TBC Rogue specTab 0: malice, ruthlessness, murder, relentless_strikes, ...
        { { ERA_TBC, CLASS_ROGUE, 0 }, { 20402, 20403, 20404, 20406, 20408, 20405, 20410, 20412, 20415, 20417, 20409, 20419, 20420, 20416, 20414, 20400, 20411, 20423, 20426, 20424 } },
        // TBC Rogue specTab 1: improved_sinister_strike, lightning_reflexes, precision, improved_slice_and_dice, ...
        { { ERA_TBC, CLASS_ROGUE, 1 }, { 20422, 20423, 20426, 20424, 20432, 20434, 20435, 20438, 20439, 20441, 20440, 20437, 20443, 20444, 20427, 20442, 20421, 20402, 20403, 20404, 20406, 20408 } },
        // TBC Rogue specTab 2: master_of_deception, opportunity, initiative, improved_ambush, ...
        { { ERA_TBC, CLASS_ROGUE, 2 }, { 20445, 20446, 20450, 20452, 20455, 20448, 20457, 20459, 20453, 20460, 20461, 20463, 20465, 20466, 20458, 20454, 20464, 20447, 20402, 20403, 20404, 20406, 20408 } },
        // TBC Priest specTab 0: unbreakable_will, silent_resolve, improved_power_word_fortitude, improved_power_word_shield, ...
        { { ERA_TBC, CLASS_PRIEST, 0 }, { 20500, 20502, 20503, 20504, 20507, 20508, 20510, 20512, 20513, 20514, 20516, 20515, 20518, 20519, 20520, 20521, 20522, 20523, 20524 } },
        // TBC Priest specTab 1: holy_specialization, improved_renew, healing_focus, divine_fury, ...
        { { ERA_TBC, CLASS_PRIEST, 1 }, { 20524, 20523, 20522, 20526, 20527, 20529, 20531, 20530, 20532, 20535, 20534, 20533, 20537, 20536, 20538, 20539, 20541, 20542, 20500, 20503, 20504 } },
        // TBC Priest specTab 2: spirit_tap, shadow_focus, improved_shadow_word_pain, shadow_affinity, ...
        { { ERA_TBC, CLASS_PRIEST, 2 }, { 20543, 20547, 20546, 20545, 20550, 20549, 20548, 20553, 20552, 20555, 20556, 20557, 20554, 20559, 20560, 20561, 20562, 20563, 20500, 20503 } },
        // TBC Shaman specTab 0: concussion, convection, elemental_focus, call_of_thunder, ...
        { { ERA_TBC, CLASS_SHAMAN, 0 }, { 20201, 20200, 20205, 20207, 20204, 20209, 20212, 20211, 20213, 20215, 20216, 20214, 20218, 20219, 20217, 20242, 20245, 20246 } },
        // TBC Shaman specTab 1: ancestral_knowledge, thundering_strikes, shamanistic_focus, enhancing_totems, ...
        { { ERA_TBC, CLASS_SHAMAN, 1 }, { 20220, 20223, 20227, 20226, 20222, 20229, 20232, 20233, 20231, 20235, 20237, 20238, 20236, 20234, 20239, 20240, 20201, 20200, 20205, 20207 } },
        // TBC Shaman specTab 2: tidal_focus, improved_healing_wave, totemic_mastery, totemic_focus, ...
        { { ERA_TBC, CLASS_SHAMAN, 2 }, { 20242, 20241, 20248, 20245, 20250, 20253, 20251, 20255, 20256, 20252, 20258, 20259, 20260, 20246, 20244, 20247, 20200, 20203, 20202 } },
        // TBC Mage specTab 0: arcane_focus, improved_arcane_missiles, arcane_concentration, arcane_impact, ...
        { { ERA_TBC, CLASS_MAGE, 0 }, { 20801, 20802, 20805, 20807, 20808, 20811, 20810, 20813, 20814, 20816, 20817, 20819, 20820, 20818, 20821, 20822, 20823, 20825, 20826, 20827 } },
        // TBC Mage specTab 1: improved_fireball, impact, ignite, pyroblast, ...
        { { ERA_TBC, CLASS_MAGE, 1 }, { 20823, 20824, 20825, 20830, 20828, 20831, 20834, 20832, 20836, 20837, 20835, 20839, 20841, 20842, 20840, 20843, 20844, 20801, 20802, 20805 } },
        // TBC Mage specTab 2: improved_frostbolt, elemental_precision, ice_shards, improved_frost_nova, ...
        { { ERA_TBC, CLASS_MAGE, 2 }, { 20846, 20847, 20848, 20850, 20853, 20852, 20857, 20856, 20859, 20858, 20862, 20863, 20864, 20865, 20866, 20849, 20801, 20802 } },
        // TBC Warlock specTab 0: improved_corruption, suppression, improved_life_tap, soul_siphon, ...
        { { ERA_TBC, CLASS_WARLOCK, 0 }, { 20701, 20700, 20704, 20705, 20702, 20708, 20706, 20710, 20711, 20713, 20714, 20712, 20715, 20717, 20716, 20719, 20720, 20743, 20744, 20745 } },
        // TBC Warlock specTab 1: demonic_embrace, improved_imp, fel_intellect, improved_voidwalker, ...
        { { ERA_TBC, CLASS_WARLOCK, 1 }, { 20723, 20722, 20726, 20725, 20729, 20728, 20730, 20732, 20731, 20734, 20735, 20737, 20736, 20739, 20740, 20738, 20741, 20742, 20701, 20700 } },
        // TBC Warlock specTab 2: improved_shadow_bolt, cataclysm, bane, devastation, ...
        { { ERA_TBC, CLASS_WARLOCK, 2 }, { 20743, 20744, 20745, 20749, 20750, 20752, 20751, 20756, 20755, 20758, 20760, 20761, 20759, 20762, 20763, 20701, 20700, 20704 } },
        // TBC Druid specTab 0: starlight_wrath, focused_starlight, improved_moonfire, control_of_nature, ...
        { { ERA_TBC, CLASS_DRUID, 0 }, { 20100, 20104, 20105, 20103, 20107, 20108, 20109, 20112, 20110, 20111, 20113, 20114, 20117, 20116, 20119, 20120, 20115, 20143, 20144, 20146, 20147 } },
        // TBC Druid specTab 1: ferocity, thick_hide, feral_instinct, feral_charge, ...
        { { ERA_TBC, CLASS_DRUID, 1 }, { 20121, 20125, 20123, 20127, 20128, 20130, 20131, 20133, 20132, 20126, 20136, 20135, 20138, 20137, 20140, 20141, 20139, 20124, 20122, 20143, 20144 } },
        // TBC Druid specTab 2: improved_mark_of_the_wild, nature_s_focus, intensity, omen_of_clarity, ...
        { { ERA_TBC, CLASS_DRUID, 2 }, { 20142, 20145, 20147, 20149, 20144, 20151, 20152, 20153, 20156, 20158, 20157, 20160, 20161, 20155, 20159, 20150, 20100, 20104, 20105 } },
        // TBC Druid specTab 3: ferocity, feral_aggression, sharpened_claws, feral_charge, ...
        { { ERA_TBC, CLASS_DRUID, 3 }, { 20121, 20122, 20128, 20127, 20126, 20129, 20130, 20131, 20132, 20133, 20135, 20136, 20138, 20139, 20140, 20141, 20123, 20124, 20125, 20143, 20144 } },
    };

    // Learn one node to maxRank (TryLearn increments a single rank per call and enforces
    // era/class/prereq/budget — a bad order wastes points but cannot corrupt state).
    void LearnToMax(Player* bot, uint32 talentId)
    {
        std::string err;
        while (EraTalents::TryLearn(bot, talentId, err)) {}
    }

    void SpendBuild(Player* bot, EraId era, int specTab)
    {
        // Defer DB writes + baseline reconciles to ONE flush/reconcile at scope end — the
        // per-learn versions ran ~40x per build on the world thread (prod lag, 2026-08-26).
        // The scope destructor also covers the reconcile FactoryReconcile used to run itself.
        EraTalents::BotBuildScope batch(bot, era);

        auto it = kAuthoredOrders.find(std::make_tuple(uint8(era), bot->getClass(), specTab));
        if (it != kAuthoredOrders.end())
            for (uint32 talentId : it->second)
            {
                LearnToMax(bot, talentId);
                // Skip canary: with points still available, every authored entry must land at
                // max rank (orders are validated skip-free against TryLearn's gates by
                // gen_bot_builds.py). A skip here means the order and the tree data drifted
                // (e.g. a re-authored prereq) — warn loudly instead of silently mis-building.
                if (EraTalents::AvailablePoints(bot, era) > 0)
                    if (const EraTalentNode* n = sEraTalentContent->Node(talentId))
                        if (EraTalents::CurrentRank(bot, era, talentId) < n->maxRank)
                            LOG_WARN("module", "[mod-era-talents] authored build order skipped {} (talent {}) for {} era{} tab{} — order/tree drift, regenerate kAuthoredOrders",
                                     n->maxRank, talentId, bot->GetName(), uint8(era), specTab);
            }

        // Greedy fill: chosen tab first, tier-ascending then column; overflow into the other
        // tabs. Multi-pass until a full pass makes no progress (prereqs unlock across passes).
        // Each node is learned TO MAX before moving on — a single-rank-per-pass walk spreads
        // points near-uniformly across ALL tabs (each pass adds one rank to every learnable
        // node everywhere), so the requested tab frequently was not the max tab and the
        // spec-tab bridge read the bot as the wrong spec (live: a resto-slot druid built
        // 20/12/19 Balance/Feral/Resto — no healer in the raid).
        std::vector<const EraTalentNode*> nodes = sEraTalentContent->NodesFor(uint8(era), bot->getClass());
        std::stable_sort(nodes.begin(), nodes.end(),
            [&](const EraTalentNode* a, const EraTalentNode* b)
            {
                bool aPrim = a->tab == uint8(specTab + 1), bPrim = b->tab == uint8(specTab + 1);
                if (aPrim != bPrim) return aPrim;
                if (a->tierRow != b->tierRow) return a->tierRow < b->tierRow;
                return a->col < b->col;
            });
        bool progress = true;
        while (progress && EraTalents::AvailablePoints(bot, era) > 0)
        {
            progress = false;
            for (const EraTalentNode* n : nodes)
            {
                std::string err;
                while (EraTalents::TryLearn(bot, n->id, err))
                    progress = true;
            }
        }
    }
}

namespace EraTalentBots
{
    EraId BotEra(Player* bot)
    {
        uint8 lvl = bot ? bot->GetLevel() : 80;
        if (lvl <= 60) return ERA_VANILLA;
        if (lvl <= 70) return ERA_TBC;
        return ERA_WOTLK;
    }

    EraId EraFor(Player* p)
    {
        return IsBot(p) ? BotEra(p) : EraFromIP(p);
    }

    bool FactoryReconcile(Player* bot, int specTab)
    {
        if (Gated(bot))
            return false;

        EraId era = BotEra(bot);
        TeardownStale(bot, era);

        // Randomize/bracket moves land here for every managed bot: enforce the glyph era
        // rule alongside the talent teardown (no-op for WotLK-band bots and DKs).
        EraGlyphGate::StripIfDisallowed(bot);

        if (!EraHasTalentTrees(era))
            return false;                                            // unmanaged band -> native
        if (sEraTalentContent->NodesFor(uint8(era), bot->getClass()).empty())
            return false;                                            // DK-in-Vanilla guard

        // Clean slate both sides, then spend. resetTalents(true) = no cost.
        bot->resetTalents(true);
        EraTalents::Reset(bot, era);
        SpendBuild(bot, era, specTab);         // its BotBuildScope runs the post-build reconcile
        InvalidateSpecTabs(bot);               // era rows changed -> spec-tab bridge re-reads
        // NB: no trainer re-walk needed here — Randomize runs InitAvailableSpells a second
        // time AFTER InitTalentsTree (PlayerbotFactory.cpp ~:708), so the era presence
        // markers this reconcile just granted (932749 Vanilla / 946078 TBC seals, 932416 totems, ...) are seen
        // by that walk and the marker-gated era chains get taught in the same pass.
        bot->SetFreeTalentPoints(0);
        bot->SendTalentsInfoData(false);
        LOG_DEBUG("module", "[mod-era-talents] bot build: {} class {} tab{} {}pts",
                 bot->GetName(), bot->getClass(), specTab, EraTalents::SpentPoints(bot, era));
        return true;
    }

    void OnBotLogin(Player* bot)
    {
        if (Gated(bot))
            return;
        EraId era = BotEra(bot);
        TeardownStale(bot, era);   // belt-and-braces; normally a no-op (levels can't change offline)
        if (EraHasTalentTrees(era) && EraTalents::SpentPoints(bot, era) > 0)
            EraTalents::ReapplyOnLogin(bot, era);

        // Run the per-class reconcile arms at login, exactly as the player login path does
        // (EraTalentPin::OnPlayerLogin). ReapplyOnLogin only re-GRANTS clones from the persisted
        // era_character_talent rows — it evaluates none of reconcile's grant-change strips,
        // baseline gates or marker grants, so before this call a managed bot kept its stale stock
        // spells until the next factory reconcile / level change. Measured 2026-09-03 (Task 8b),
        // after a boot with the TBC-rogue Riposte rework: 7 Vanilla-band rogue bots still held
        // stock Riposte 14251 (specMask 1) alongside the new era clone 932989, and 4 held stock
        // 13750 with no clone; `.character level` on one cleared it instantly. That matters beyond
        // cosmetics because EraTalentBots_ResolveSpellId's HasSpell fast path returns the STOCK id
        // whenever the bot still knows it, so the AI casts the wrong-era spell.
        //
        // Ordering: after ReapplyOnLogin, so the era grants exist before the arms evaluate them
        // (several arms are "strip X unless the era clone/rank is present").
        //
        // Cost: ONE direct call — deliberately NOT wrapped in BotBuildScope. That scope exists to
        // collapse the ~40 per-TryLearn reconciles+writes of a factory BUILD into one; there are no
        // TryLearn calls here, and its destructor would ADD a synchronous REPLACE INTO
        // era_character_talent plus a second reconcile per login. Reconcile itself issues no DB
        // query: it reads rank state only through CurrentRank, whose lazy per-(guid,era) SELECT was
        // already spent by TeardownStale + the SpentPoints call above.
        // UNCONDITIONAL, mirroring EraTalentPin.cpp:71 ("unconditional: must also run for WotLK
        // priests") — an UNMANAGED (WotLK) band must still run the cross-era strips: hunter
        // 948280/948281, the priest Prayer-of-Spirit chain 948029/948030, and the warlock stone
        // markers are trainer-/reconcile-managed, so TeardownStale (which only resets node-granted
        // era_character_talent ranks) never touches them. Measured 2026-09-04: a hunter bot moved
        // 70->80 kept the TBC marker 948280 and the TBC Scorpid clone 948281 indefinitely, because
        // both this call and FactoryReconcile bailed on !EraHasTalentTrees(WotLK).
        // Costs no extra DB query in an unmanaged band: reconcile reaches CurrentRank only with
        // ERA_VANILLA/ERA_TBC (whose per-(guid,era) caches TeardownStale's SpentPoints just seeded)
        // and the band sweep (EraBandClassifier) resolves a WotLK character's held band ids against a
        // NodesFor(ERA_WOTLK) owner set that is empty while no WotLK trees are authored.
        //
        // SIDE EFFECT, stated explicitly because it is a real behaviour change for WotLK-band bots:
        // reaching reconcile in an unmanaged band ALSO hands them things they previously never got —
        // the kBaselineSpellGates `era >= ERA_TBC` force-grants (Holy Nova r1, Shield Slam r1, Ice
        // Block, Divine Spirit r1, Consecration r1) and the WotLK-era presence markers (932939
        // warlock stones / 932417 shaman totems / 932305 hunter). That is the CORRECT WotLK state and
        // is exactly what a real WotLK player already gets from EraTalentPin.cpp:71; bots were simply
        // never reaching it. Cost is bounded: one learnSpell per missing spell per bot, ONCE, and it
        // persists to character_spell — steady-state logins re-check with plain HasSpell probes and
        // grant nothing.
        EraTalents::ReconcileBaselineSpells(bot, era);

        // TBC faction DPS seals (Task 5d): the factory trainer-walk (InitAvailableSpells) is
        // class-only-gated, so a TBC-band paladin bot learns BOTH Seal of Blood + Seal of Vengeance
        // during a randomize (after the mid-build reconcile has already run). This login-time strip
        // removes the wrong-faction one in steady state (the saved character_spell is loaded before
        // this hook fires). No-op for non-paladins / when the wrong seal isn't held.
        if (era == ERA_TBC)
            EraTalents::StripWrongFactionTbcPalSeal(bot);
    }

    void OnBotLevelChanged(Player* bot)
    {
        if (Gated(bot))
            return;
        EraId era = BotEra(bot);

        // Band crossing OUT of a managed era (60 -> 61): teardown, then hand the bot a fresh
        // native template. InitTalentsTree re-picks the spec — acceptable for a random bot on
        // a rare boundary event; the factory re-randomizes them periodically anyway.
        bool hadStale = false;
        for (uint8 e = 0; e <= uint8(ERA_WOTLK); ++e)
            if (EraId(e) != era && EraHasTalentTrees(EraId(e)) && EraTalents::SpentPoints(bot, EraId(e)) > 0)
                hadStale = true;
        TeardownStale(bot, era);
        InvalidateSpecTabs(bot);   // level changed (and possibly rows) -> bridge must re-read

        // A move INTO an unmanaged (WotLK) band gets no reconcile from anywhere else: the
        // InitTalentsTree branch below routes through FactoryReconcile, which returns early on
        // !EraHasTalentTrees, and BotBuildScope's end-of-build reconcile only runs for managed
        // bands. Same rationale (and same measured defect) as the unconditional call in
        // OnBotLogin above; placed BEFORE the early return so both exits are covered. Managed
        // bands are deliberately NOT reconciled here — SpendBuild's BotBuildScope already does it,
        // and a second pass would be pure duplicate work.
        if (!EraHasTalentTrees(era))
            EraTalents::ReconcileBaselineSpells(bot, era);

        if (hadStale && !EraHasTalentTrees(era))
        {
            PlayerbotFactory factory(bot, bot->GetLevel());
            factory.InitTalentsTree(false, true, true);
            // EVERY exit of this hook re-runs the TBC seal strip (I-3): a level change can land a
            // paladin bot in the TBC band right after a factory pass has re-taught both faction
            // seals, and the only other strip sites (OnBotLogin, mid-reconcile) both run BEFORE
            // the trainer walk. A no-op for every non-TBC / non-paladin case.
            StripTbcSealIfPaladin(bot, era);
            return;
        }

        // Inside a managed band with an existing build: spend the newly-accrued point(s) in
        // the same shape (deterministic spender + same tab = the same build, extended).
        if (EraHasTalentTrees(era) && EraTalents::SpentPoints(bot, era) > 0
            && EraTalents::AvailablePoints(bot, era) > 0)
        {
            // Recover the build's tab from where the points already are (the spec-tab bridge
            // shares exactly this computation, and its cache was just invalidated above).
            uint32 tabs3[3] = { 0, 0, 0 };
            int bestTab = 0;
            if (EraTalentBots_SpecTabs(bot, tabs3))
                for (int tab = 1; tab < 3; ++tab)
                    if (tabs3[tab] > tabs3[bestTab])
                        bestTab = tab;
            SpendBuild(bot, era, bestTab);
            InvalidateSpecTabs(bot);   // rows grew -> re-read on next AI query
        }

        StripTbcSealIfPaladin(bot, era);   // second (fall-through) exit — see the note above
    }

    void OnLogout(Player* p)
    {
        if (!p)
            return;
        std::lock_guard<std::mutex> guard(g_botCacheMutex);
        g_specTabsCache.erase(p->GetGUID());       // unconditional: must run even if knob off
        g_resolveCache.erase(p->GetGUID());
    }

    void InvalidateSpecCache(Player* p)
    {
        InvalidateSpecTabs(p);   // player era-row mutations (TryLearn/Reset) route here — the
                                 // spec-tab bridge serves era-managed PLAYERS too, so a learn
                                 // via the addon must not leave a stale cached distribution
    }

    void BuildTrainerLevelIndex()
    {
        // ONE query at worldserver startup; read-only for the rest of the process (see the
        // g_trainerMinReqLevel comment for why that means no lock). ReqLevel 0 rows carry no
        // level information, and band ids are the sweep's business, never ours.
        g_trainerMinReqLevel.clear();
        if (QueryResult r = WorldDatabase.Query(
                "SELECT SpellId, MIN(ReqLevel) FROM trainer_spell WHERE ReqLevel > 0 GROUP BY SpellId"))
        {
            do
            {
                Field* f = r->Fetch();
                uint32 spellId = f[0].Get<uint32>();
                if (spellId >= EraTalents::ERA_CUSTOM_BAND_LOW && spellId < EraTalents::ERA_CUSTOM_BAND_HIGH)
                    continue;
                g_trainerMinReqLevel[spellId] = f[1].Get<uint32>();
            } while (r->NextRow());
        }
        // Companion pass, same startup call (content is already loaded — the loader runs
        // sEraTalentContent->Load() first): every STOCK id any (era, class) node grants. Read-only
        // afterwards, same no-lock argument as the trainer index above.
        g_eraNodeGrantStock.clear();
        uint32 stockGrants = 0;
        for (uint8 e = 0; e <= uint8(ERA_WOTLK); ++e)
            for (uint8 c = CLASS_WARRIOR; c <= CLASS_DRUID; ++c)
                for (const EraTalentNode* n : sEraTalentContent->NodesFor(e, c))
                    for (uint32 grant : n->rankSpell)
                    {
                        if (!grant || (grant >= EraTalents::ERA_CUSTOM_BAND_LOW && grant < EraTalents::ERA_CUSTOM_BAND_HIGH))
                            continue;   // display-only rank, or a band id the sweep owns
                        if (g_eraNodeGrantStock[EraClassKey(EraId(e), c)].insert(grant).second)
                            ++stockGrants;
                    }

        LOG_INFO("module", "[mod-era-talents] trainer level map: {} stock spell(s) indexed by min trainer ReqLevel, {} node-granted stock id(s) excluded across {} (era,class) set(s)",
            uint32(g_trainerMinReqLevel.size()), stockGrants, uint32(g_eraNodeGrantStock.size()));
    }
}

// Bridges for fork patch 0020 — signatures must match the extern declarations in the patch.
bool EraTalentBots_FactoryReconcile(Player* bot, int specTab)
{
    return EraTalentBots::FactoryReconcile(bot, specTab);
}

void EraTalentBots_PostTrainerWalk(Player* bot)
{
    // Called from patch 0020 right after each InitAvailableSpells() trainer walk — THREE sites,
    // and the third is the load-bearing one (measured 2026-09-06): PlayerbotFactory::Randomize
    // step 2, PlayerbotFactory::Refresh, and AutoMaintenanceOnLevelupAction::LearnTrainerSpells,
    // an AI-TICK action that runs after the entire factory pass has finished. Fixing only the two
    // factory sites left both faction seals and 9 above-level trainer spells on a re-levelled
    // paladin bot, because the maintenance action re-taught them afterwards.
    //
    // The walk is CLASS-only gated (no faction, no trainer ReqLevel), so it re-teaches (a) BOTH
    // TBC paladin faction seals and (b) trainer spells above the bot's level. Both arms are the
    // same ones TeardownStale runs; this is where they get the last word. Keep this cheap — it is
    // on a per-bot factory/AI path — and keep every arm bail-gated before any deref.
    if (Gated(bot))
        return;
    StripAboveLevelTrainerSpells(bot);                        // any band, incl. WotLK
    StripTbcSealIfPaladin(bot, EraTalentBots::EraFor(bot));   // self-gates on era TBC + readiness
}

bool EraTalentBots_SpecTabs(Player* p, uint32* tabs3)
{
    // Serves BOTS and era-managed real PLAYERS alike. An era-managed player's NATIVE talent
    // map is stripped by the era system, so every native spec consumer (PlayerbotAI::IsTank/
    // IsHeal on the MASTER — .raidroster login's role auto-detect — formations, ...) read
    // them as untalented/tab-0. Live bug: a Shadow-era-built priest master auto-detected as
    // the group HEALER (priest tab 0 = Disc), so login 5 fielded no healer bot. The fork's
    // patched GetPlayerSpecTabs calls this for any Player*; we decide who it serves.
    if (!p || !tabs3 || !sEraTalentsConfig->Enabled())
        return false;
    if (IsBot(p) && !sEraTalentsConfig->BotTalents())
        return false;
    EraId era = EraTalentBots::EraFor(p);
    if (!EraHasTalentTrees(era))
        return false;

    std::array<uint32, 3> t{ 0, 0, 0 };
    bool cached = false;
    {
        std::lock_guard<std::mutex> guard(g_botCacheMutex);
        auto it = g_specTabsCache.find(p->GetGUID());
        if (it != g_specTabsCache.end())
        {
            t = it->second;   // copy OUT under the lock — a reference would escape it
            cached = true;
        }
    }
    if (!cached)
    {
        // Cache miss: sum the character's era ranks per tab from EraTalents' rank cache (era
        // tab keys are 1-based) — no DB round trip of its own. Cached even when empty so an
        // unbuilt character doesn't recompute every AI tick. Invalidated on every era-row
        // mutation (bot paths here; player learns/resets via EraTalents::TryLearn/Reset ->
        // InvalidateSpecCache) and on logout. Computed OUTSIDE g_botCacheMutex: Ranks() takes
        // the rank-cache mutex, and two concurrent misses just compute the same value twice.
        for (auto const& [talentId, rank] : EraTalents::Ranks(p, era))
            if (const EraTalentNode* n = sEraTalentContent->Node(talentId))
                if (n->tab >= 1 && n->tab <= 3)
                    t[n->tab - 1] += rank;
        std::lock_guard<std::mutex> guard(g_botCacheMutex);
        g_specTabsCache[p->GetGUID()] = t;
    }

    if (t[0] + t[1] + t[2] == 0)
        return false;   // no era rows yet (not reconciled) -> caller uses the native path
    tabs3[0] = t[0];
    tabs3[1] = t[1];
    tabs3[2] = t[2];
    return true;
}

uint32 EraTalentBots_ResolveSpellId(Player* bot, uint32 stockSpellId)
{
    if (!bot || !stockSpellId)
        return 0;
    if (bot->HasSpell(stockSpellId))
        return stockSpellId;    // knows the stock id: native bots, WotLK band, knob off,
                                // and era-gated stock grants (Shield Slam) all end here
    if (Gated(bot))
        return 0;               // player / module off -> exactly a false HasSpell
    EraId era = EraTalentBots::BotEra(bot);
    if (!EraHasTalentTrees(era))
        return 0;

    // Lookup under the lock, copy the VALUE out; the scan below runs unlocked (it touches only
    // this bot's spell map, which its own thread owns) and the insert re-takes the lock. This
    // path is the exact site of the 2026-08-26 prod segfault: unlocked operator[] from parallel
    // map-update threads rehash-raced the outer map.
    {
        std::lock_guard<std::mutex> guard(g_botCacheMutex);
        auto botIt = g_resolveCache.find(bot->GetGUID());
        if (botIt != g_resolveCache.end())
        {
            auto cached = botIt->second.find(stockSpellId);
            if (cached != botIt->second.end())
                return cached->second;
        }
    }

    // Scan the bot's spellbook for a KNOWN same-name spell inside the era custom band
    // (clones deliberately keep stock names; the band restriction stops accidental
    // cross-matches to unrelated stock spells). SpellIdValue's match rules, except
    // passives are INCLUDED — Thick Hide-style talent discriminators are passives.
    //
    // TWO naming conventions live in the band and BOTH must resolve (Task 9, 2026-08-31):
    //   * HAND-AUTHORED helper clones (`helpers:` in the dataset) keep the stock name verbatim
    //     — "Omen of Clarity", "Blessing of Sanctuary", the shaman totems, Vampiric Embrace.
    //   * GENERATED auto-passives are named "<node name> (Rank N)" (tools/gen_era_talents.py),
    //     so an exact-name compare NEVER matched them. Every patch-0021 discriminator that
    //     targets a talent PASSIVE was therefore resolving to 0 — i.e. permanently false:
    //     druid Thick Hide 16931 (the bear discriminator behind IsTank / the LFG role / the
    //     bear-vs-cat strategy pick) and paladin Improved Blessing of Might/Wisdom. Verified
    //     on a live TBC-band druid bot: stock 16931 is "Thick Hide", the clone the bot knows
    //     is "Thick Hide (Rank 3)". This is a Vanilla-era bug too, not a TBC-druid regression.
    // So a trailing " (Rank N)" is trimmed before the compare, and the parsed N must be >= the
    // STOCK spell's rank: the by-id checks mean "this rank or better" (16931 IS Thick Hide r3),
    // era nodes can carry a different rank count than the stock chain (Vanilla Thick Hide is a
    // 5-rank node), and only the bot's CURRENT rank is in its spellbook. A stock spell with no
    // rank chain (GetSpellRank 0) accepts any rank, which is the old behavior for those.
    //
    // ERA-RENAME ALIASES. The compare below assumes the era clone carries the LIVE (3.3.5) name,
    // which holds wherever the era dataset reuses the live node name. It does NOT hold where a
    // later expansion RENAMED the talent and the era dataset (correctly) keeps the era-authentic
    // name: WotLK renamed 1.12's "Improved Battle Shout" to "Commanding Presence", so stock
    // 12318/12857/12858/12860/12861 are all named "Commanding Presence" while the Vanilla clones
    // 924208-924212 are named "Improved Battle Shout (Rank N)". A pure name match therefore
    // returned 0 for EVERY Vanilla-band warrior — the same class of silent-false defect as the
    // "(Rank N)" suffix bug, pre-existing since the Vanilla warrior phase and found during the
    // TBC Phase 4 by-id sweep (patch 0021's BattleShoutTrigger Commanding Presence check).
    // The alias is BAND-AGNOSTIC on purpose: it just adds candidate NAMES, and the clone the bot
    // actually knows still decides (a TBC warrior matches on the live name and never reaches the
    // alias; a WotLK-band/native warrior returns from the HasSpell fast path far above and never
    // gets here at all, so an alias can never shadow a stock spell the bot really knows).
    // Aliases are lowercase (the compare is case-insensitive via Utf8FitTo on a lowered name).
    //
    // Second instance, found by the TBC Phase 5 (rogue) by-id sweep 2026-09-03: WotLK renamed
    // the rogue's 1.12/2.4 "Sword Specialization" to "Hack and Slash" (and widened it to axes),
    // so stock 13960-13964 are all "Hack and Slash" while BOTH era chains keep the era-authentic
    // name — Vanilla 923440-444 and TBC 939480-484 are "Sword Specialization (Rank N)". Unlike
    // Commanding Presence (where only the Vanilla dataset diverges), this alias is the ONLY way
    // any era-band rogue resolves 13964, i.e. patch 0021's StatsWeightCalculator sword/axe gear
    // preference was dead for every era rogue in both bands.
    struct EraNameAlias { char const* liveName; char const* eraName; };
    static constexpr EraNameAlias kEraNameAliases[] = {
        { "commanding presence", "improved battle shout" },   // WotLK rename of the 1.12 talent
        { "hack and slash",      "sword specialization" },    // WotLK rename of the 1.12/2.4 talent
    };

    uint32 resolved = 0;
    if (SpellInfo const* stock = sSpellMgr->GetSpellInfo(stockSpellId))
    {
        uint8 const stockRank = sSpellMgr->GetSpellRank(stockSpellId);

        // One spellbook scan for a single candidate name. Run for the stock name first; only if
        // that finds nothing do the aliases get a turn, so the common path costs exactly what it
        // did before.
        auto scanForName = [&](std::wstring const& wname) -> uint32
        {
            // TWO bests (M-72). Ids ascend with rank inside an era chain, so "highest id wins" is
            // the right rank tiebreak — but it is the WRONG tiebreak across a castable/passive
            // PAIR that shares a name. Measured: stock Vampiric Embrace 15286 resolves on a
            // Vanilla shadow priest to BOTH the castable clone 921152 and the hidden reconcile-
            // managed proc-passive 932950, and the plain highest-id rule handed the AI 932950 —
            // an id it can never cast. Prefer a NON-passive candidate; fall back to the highest id
            // when every candidate is passive (the Thick Hide / Improved Battle Shout style
            // discriminators are passive-only by nature and must still resolve).
            // Safe for every patch-0021 consumer, most of which do `HasAura(resolved)` (or cast /
            // compare it) rather than a bare `!= 0`: a name whose candidates are ALL passive still
            // returns `best`, unchanged; and where a castable AND a passive share the name, the
            // castable is the id the consumer wants — VE resolves to the castable 921152 instead
            // of the hidden proc-passive 932950, and Blessing of Sanctuary to the buff the bot
            // actually casts on a target rather than its passive partner.
            uint32 best = 0;
            uint32 bestCastable = 0;
            size_t nameLen = wname.length();
            for (auto const& [spellId, pSpell] : bot->GetSpellMap())
            {
                if (spellId < EraTalents::ERA_CUSTOM_BAND_LOW || spellId >= EraTalents::ERA_CUSTOM_BAND_HIGH)
                    continue;
                if (pSpell->State == PLAYERSPELL_REMOVED || !pSpell->Active)
                    continue;
                // Skip out-of-spec rows (specMask=0 ghosts) — same guard patch 0021 adds to
                // SpellIdValue; band rows are module-managed so a ghost here is unlikely, but
                // resolving one would hand the AI an uncastable id.
                if (!pSpell->IsInSpec(bot->GetActiveSpec()))
                    continue;
                SpellInfo const* info = sSpellMgr->GetSpellInfo(spellId);
                if (!info || info->Effects[0].Effect == SPELL_EFFECT_LEARN_SPELL)
                    continue;
                char const* name = info->SpellName[LOCALE_enUS];
                if (!name)
                    continue;
                std::string cand(name);
                size_t const suffix = cand.rfind(" (Rank ");
                if (suffix != std::string::npos && cand.back() == ')')
                {
                    uint32 cloneRank = 0;
                    bool digits = false;
                    for (size_t i = suffix + 7; i + 1 < cand.size(); ++i)
                    {
                        if (cand[i] < '0' || cand[i] > '9') { digits = false; break; }
                        cloneRank = cloneRank * 10 + uint32(cand[i] - '0');
                        digits = true;
                    }
                    if (!digits)
                        continue;                       // e.g. "Thick Hide (Pet, Rank 1)" — not ours
                    if (stockRank && cloneRank < stockRank)
                        continue;                       // holds a LOWER rank than the check asks for
                    cand.resize(suffix);
                }
                if (cand.length() != nameLen || !Utf8FitTo(cand, wname))
                    continue;
                if (spellId > best)
                    best = spellId;   // ids ascend with rank in the era chains
                if (!info->IsPassive() && spellId > bestCastable)
                    bestCastable = spellId;
            }
            return bestCastable ? bestCastable : best;
        };

        std::wstring wname;
        if (Utf8toWStr(stock->SpellName[LOCALE_enUS], wname))
        {
            wstrToLower(wname);
            resolved = scanForName(wname);

            // Only on a miss: retry under each era-authentic alias of this stock name.
            if (!resolved)
            {
                for (EraNameAlias const& alias : kEraNameAliases)
                {
                    std::wstring wlive;
                    if (!Utf8toWStr(alias.liveName, wlive))
                        continue;
                    wstrToLower(wlive);
                    if (wlive != wname)
                        continue;                       // alias is for a different stock spell
                    std::wstring wera;
                    if (!Utf8toWStr(alias.eraName, wera))
                        continue;
                    wstrToLower(wera);
                    resolved = scanForName(wera);
                    if (resolved)
                        break;
                }
            }
        }
    }
    {
        std::lock_guard<std::mutex> guard(g_botCacheMutex);
        g_resolveCache[bot->GetGUID()][stockSpellId] = resolved;   // cache misses too (0 = known-none)
    }
    return resolved;
}
