#include "EraTalents.h"
#include "EraTalentContent.h"
#include "EraBandClassifier.h"
#include "EraTalentIP.h"
#include "EraTalentBots.h"   // EraTalentBots::EraFor — bots derive era from level band, never EraFromIP
#include "EraTalentsConfig.h"
#include "ScriptMgr.h"
#include "Log.h"
#include "Player.h"
#include "DatabaseEnv.h"
#include "QueryResult.h"
#include "Field.h"
#include "SpellMgr.h"
#include "ObjectAccessor.h"

#include <algorithm>
#include <mutex>

// Master Demonologist (EraTalentPetScripts.cpp): re-apply/strip the active demon's branch for the
// CURRENT talent state. Must run on TryLearn/Reset so a respec strips the buff without a resummon.
void EraMasterDemonologist_Refresh(Player* wl);

// Server-authoritative talent engine.
//
// Era talent points are DERIVED, never stored in the core PLAYER_CHARACTER_POINTS1
// field (that field is pinned to 0 for era chars by the era_talent_pin PlayerScript,
// added separately). The
// source of truth for what an era char has spent is era_character_talent.

namespace
{
    // Rank cache: (guid,era) -> talentId -> rank. Presence of the outer key means "loaded"
    // (an empty inner map is a valid loaded state — no re-query for unbuilt characters).
    // Coherence holds because TryLearn/Reset are the only era_character_talent writers.
    uint64 RankKey(Player* p, EraId era)
    {
        return (uint64(p->GetGUID().GetCounter()) << 2) | uint64(era);
    }

    std::unordered_map<uint64, std::unordered_map<uint32, uint8>> g_rankCache;

    // Guards g_rankCache. NOT world-thread-only: the patch-0020 GetPlayerSpecTabs bridge runs
    // on bot-AI paths inside Map::Update, and maps tick in PARALLEL on the MapUpdater pool —
    // an unlocked unordered_map mutated cross-thread rehash-races and segfaults (live prod
    // crash 2026-08-26, g_resolveCache's twin of this bug). Lock discipline: hold only around
    // cache reads/writes, NEVER across learnSpell/removeSpell/Reconcile (their hooks re-enter
    // CurrentRank -> self-deadlock) and never while calling another module unit's locked API.
    std::mutex g_rankCacheMutex;

    // Caller must hold g_rankCacheMutex. (The one-time DB load on miss runs under the lock on
    // purpose: it makes concurrent first-touch loads of the same character impossible.)
    std::unordered_map<uint32, uint8>& LoadRanks(Player* p, EraId era)
    {
        auto [it, inserted] = g_rankCache.try_emplace(RankKey(p, era));
        if (inserted)
        {
            if (QueryResult r = CharacterDatabase.Query(
                    "SELECT talentId, `rank` FROM era_character_talent WHERE guid={} AND eraId={}",
                    p->GetGUID().GetCounter(), uint8(era)))
            {
                do
                {
                    Field* f = r->Fetch();
                    it->second[f[0].Get<uint32>()] = f[1].Get<uint8>();
                } while (r->NextRow());
            }
        }
        return it->second;
    }

    // Active BotBuildScope. THREAD_LOCAL, and the previous comment here was WRONG: it claimed
    // "bot BUILDS run on the world thread only (factory randomize/bracket moves, roster commands,
    // the login/level hooks)". The LEVEL hook does not — `Player::GiveLevel` fires
    // OnPlayerLevelChanged inside Map::Update, so a grouped bot dinging runs EraTalentBots::
    // OnBotLevelChanged -> SpendBuild -> BotBuildScope on a MapUpdater thread, and maps update in
    // parallel. Two concurrent builds on a plain global would race on this struct (a data race
    // outright, and functionally: thread B's guid overwrite makes thread A's InBotBuild() return
    // false, so A silently stops deferring its row writes and B's scope exit flushes against the
    // wrong guid). A scope is a STACK object, so it can never legitimately be observed from another
    // thread — thread_local is both correct and race-free, and "at most one scope is ever live"
    // becomes true per thread, which is all the deferral logic ever needed. (Unlike the rank CACHE
    // above, which map-thread AI readers reach through the spec bridge and which needs its mutex —
    // that one is genuinely SHARED and cannot be thread_local.)
    thread_local struct { uint32 guid = 0; uint8 era = 0; bool active = false; } g_botBuild;

    bool InBotBuild(Player* p)
    {
        return g_botBuild.active && g_botBuild.guid == p->GetGUID().GetCounter();
    }
}

namespace EraTalents
{
    int SpentPoints(Player* p, EraId era)
    {
        std::lock_guard<std::mutex> guard(g_rankCacheMutex);
        int sum = 0;
        for (auto const& [talentId, rank] : LoadRanks(p, era))
            sum += rank;
        return sum;
    }

    uint8 CurrentRank(Player* p, EraId era, uint32 talentId)
    {
        std::lock_guard<std::mutex> guard(g_rankCacheMutex);
        auto const& ranks = LoadRanks(p, era);
        auto it = ranks.find(talentId);
        return it != ranks.end() ? it->second : 0;
    }

    // By VALUE: a reference into the cache would be read by the caller with no lock held,
    // racing writers on other threads. ≤51 entries — the copy is cheap, and the two callers
    // (SpecTabs miss, doctor) are not per-tick paths.
    std::unordered_map<uint32, uint8> Ranks(Player* p, EraId era)
    {
        std::lock_guard<std::mutex> guard(g_rankCacheMutex);
        return LoadRanks(p, era);
    }

    void EvictCache(Player* p)
    {
        if (!p)
            return;
        std::lock_guard<std::mutex> guard(g_rankCacheMutex);
        for (uint8 e = 0; e <= uint8(ERA_WOTLK); ++e)
            g_rankCache.erase(RankKey(p, EraId(e)));
    }

    BotBuildScope::BotBuildScope(Player* p, EraId era)
        : _p(p), _era(era), _active(!g_botBuild.active)
    {
        if (_active)
            g_botBuild = { p->GetGUID().GetCounter(), uint8(era), true };
    }

    BotBuildScope::~BotBuildScope()
    {
        if (!_active)
            return;
        g_botBuild.active = false;

        // Persist the final rank state in one round trip. DirectExecute (not async Execute):
        // the cache is evicted at logout, so the NEXT login's re-load must be guaranteed to
        // see these rows — async runs on a different connection with no ordering guarantee
        // against later sync reads.
        std::unordered_map<uint32, uint8> ranks;
        {
            std::lock_guard<std::mutex> guard(g_rankCacheMutex);
            ranks = LoadRanks(_p, _era);   // copy: don't hold the lock across the DB write below
        }
        if (!ranks.empty())
        {
            std::string values;
            for (auto const& [talentId, rank] : ranks)
            {
                if (!values.empty())
                    values += ',';
                values += '(' + std::to_string(_p->GetGUID().GetCounter()) + ',' + std::to_string(uint32(_era))
                        + ',' + std::to_string(talentId) + ',' + std::to_string(uint32(rank)) + ')';
            }
            CharacterDatabase.DirectExecute(
                "REPLACE INTO era_character_talent (guid,eraId,talentId,`rank`) VALUES {}", values);
        }

        // The single deferred reconcile for the whole build (TryLearn skipped its per-learn
        // ones); MD tracks CurrentRank so the deferred 18232 refresh collapses to this too.
        ReconcileBaselineSpells(_p, _era);
        if (_p->getClass() == CLASS_WARLOCK)
            EraMasterDemonologist_Refresh(_p);
    }

    int AvailablePoints(Player* p, EraId era)
    {
        // Talents open at level 10; 1 point/level thereafter, capped at the era's tree budget
        // (Vanilla 51 / TBC 61): IP normally caps level per era, but `.ip set` + GM levels can
        // put a level-70+ character in a lower era, which must not overspend the smaller trees.
        // Clamped >= 0: content shrinking a maxRank after points were spent could otherwise go negative.
        int points = std::max(0, int(p->GetLevel()) - 9);
        int cap = (era == ERA_VANILLA) ? 51 : (era == ERA_TBC) ? 61 : points;
        points = std::min(points, cap);
        return std::max(0, points - SpentPoints(p, era));
    }

    bool TryLearn(Player* p, uint32 talentId, std::string& err)
    {
        const EraTalentNode* n = sEraTalentContent->Node(talentId);
        if (!n)
        {
            err = "no such talent";
            return false;
        }

        // EraFor, not EraFromIP: TryLearn is on the BOT build path (EraTalentBots::SpendBuild),
        // and EraFromIP reads every bot as Vanilla — a TBC-band bot's learns would all fail
        // "talent not in your era" the moment TBC trees ship (silent 0-point builds).
        EraId era = EraTalentBots::EraFor(p);
        if (n->eraId != uint8(era))
        {
            err = "talent not in your era";
            return false;
        }
        if (n->classId != p->getClass())
        {
            err = "talent not for your class";
            return false;
        }

        uint8 rank = CurrentRank(p, era, talentId);
        if (rank >= n->maxRank)
        {
            err = "already max rank";
            return false;
        }

        if (AvailablePoints(p, era) <= 0)
        {
            err = "no talent points available";
            return false;
        }

        uint8 cls = p->getClass();

        // tab prereq: total ranks spent in the SAME tab across this era's nodes.
        if (n->prereqPoints > 0)
        {
            int tabSpent = 0;
            for (const EraTalentNode* other : sEraTalentContent->NodesFor(uint8(era), cls))
                if (other->tab == n->tab)
                    tabSpent += CurrentRank(p, era, other->id);

            if (tabSpent < int(n->prereqPoints))
            {
                err = "requires " + std::to_string(n->prereqPoints) + " points in tree";
                return false;
            }
        }

        // talent prereq: the named prerequisite must be at its max rank.
        if (n->prereqTalentId != 0)
        {
            const EraTalentNode* pre = sEraTalentContent->Node(n->prereqTalentId);
            if (!pre || CurrentRank(p, era, n->prereqTalentId) != pre->maxRank)
            {
                err = "requires prerequisite talent";
                return false;
            }
        }

        uint8 newRank = rank + 1;
        if (uint32(newRank - 1) >= n->rankSpell.size())
        {
            err = "malformed talent data";
            return false;
        }

        uint32 newSpell = n->rankSpell[newRank - 1];
        if (newSpell == 0 || !sSpellMgr->GetSpellInfo(newSpell))
        {
            // Atomicity: a content-authored id absent from the spell store must not leave a DB row
            // recording rank N while the spell isn't actually known.
            err = "malformed talent data";
            return false;
        }

        // Record the new rank BEFORE granting the spell. learnSpell() below fires OnPlayerLearnSpell,
        // which for a gate-managed stock grant (Divine Spirit 14752, Shield Slam 23922, Ice Block 45438,
        // Consecration/BoK, …) re-runs ReconcileBaselineSpells — and that gate STRIPS the spell when it
        // reads CurrentRank == 0. Ranks now live in the in-memory cache (the read path of CurrentRank/
        // SpentPoints), so the cache write is what makes the hook-reconcile see the new rank; the DB row
        // is pure persistence. Inside a BotBuildScope the row write is deferred to the scope's single
        // multi-row flush — a ~40-point greedy build was ~40 sync REPLACEs + ~1000 reads on the world
        // thread (prod lag spikes, 2026-08-26).
        {
            std::lock_guard<std::mutex> guard(g_rankCacheMutex);
            LoadRanks(p, era)[talentId] = newRank;   // released before learnSpell: its hooks re-read ranks
        }
        if (!InBotBuild(p))
            CharacterDatabase.DirectExecute(
                "REPLACE INTO era_character_talent (guid,eraId,talentId,`rank`) VALUES ({},{},{},{})",
                p->GetGUID().GetCounter(), uint8(era), talentId, newRank);

        if (rank > 0 && uint32(rank - 1) < n->rankSpell.size() && n->rankSpell[rank - 1] != 0)
            p->removeSpell(n->rankSpell[rank - 1], SPEC_MASK_ALL, false);

        p->learnSpell(newSpell);

        if (!InBotBuild(p))
        {
            // Reconcile-managed spells (VE proc-passive 932950 Vanilla / 948032 TBC, the Holy Nova
            // baseline gate) must
            // track talent state IMMEDIATELY — reconcile otherwise only runs at login/level/era
            // hooks, so a mid-session learn left VE heal-less until relog (live-confirmed bug).
            // In a bot build the scope destructor runs the one reconcile against the final state.
            ReconcileBaselineSpells(p, era);

            // Master Demonologist tracks CurrentRank, not the marker: a rank-up while the demon is out
            // must re-apply the branch at the new magnitude immediately (not on the next resummon).
            if (talentId == 18232)
                EraMasterDemonologist_Refresh(p);
        }

        if (sEraTalentsConfig->Debug())
            LOG_INFO("module", "[mod-era-talents] {} learned talent {} rank {} (spell {})",
                     p->GetName(), talentId, newRank, newSpell);

        EraTalentBots::InvalidateSpecCache(p);   // era rows changed — the spec-tab bridge
                                                 // serves era-managed players too (cheap erase)
        return true;
    }

    void Reset(Player* p, EraId era)
    {
        uint8 cls = p->getClass();
        for (const EraTalentNode* n : sEraTalentContent->NodesFor(uint8(era), cls))
        {
            uint8 r = CurrentRank(p, era, n->id);
            if (r > 0 && uint32(r - 1) < n->rankSpell.size() && n->rankSpell[r - 1] != 0)
                p->removeSpell(n->rankSpell[r - 1], SPEC_MASK_ALL, false);
        }

        CharacterDatabase.DirectExecute("DELETE FROM era_character_talent WHERE guid={} AND eraId={}",
                                        p->GetGUID().GetCounter(), uint8(era));
        // operator[] inserts an empty map when absent — "loaded, zero rows" is exactly the
        // post-reset state, so subsequent reads don't re-query.
        {
            std::lock_guard<std::mutex> guard(g_rankCacheMutex);
            g_rankCache[RankKey(p, era)].clear();
        }

        // Symmetric with TryLearn: a respec away from VE must strip 932950 (Vanilla) / 948032 (TBC)
        // immediately.
        ReconcileBaselineSpells(p, era);

        // A respec away from Master Demonologist must strip the active demon's branch immediately
        // (rank is now 0 -> the refresh strips both sides; the exact "stays when I respec" bug).
        if (p->getClass() == CLASS_WARLOCK)
            EraMasterDemonologist_Refresh(p);

        EraTalentBots::InvalidateSpecCache(p);   // rows deleted — see TryLearn
    }

    namespace
    {
        // ---- Staged-content readiness guards (2026-08-30) -------------------------------------
        // An era's reconcile arms are shipped BEFORE the clones/nodes they hand over to: the TBC
        // druid substrate lands across several tasks (arms first, then the 946xxx clones, then the
        // authored tree). A strip of a WORKING stock spell must therefore never fire until its
        // replacement actually exists, or the intermediate state leaves the character with neither
        // — a TBC druid losing Nature's Grasp / Omen of Clarity / Faerie Fire (Feral) outright.
        // These predicates make each such arm SELF-ACTIVATING: dormant (stock behavior preserved)
        // until the content lands, live the moment it does, with no further code change.
        //
        // DO NOT delete them as "redundant" once the content ships. They are also the live-realm
        // safety net for a failed or missing SQL migration: the same DB state that would make a
        // fresh install strip-without-replacement is what these read.
        //
        // NB the two ask different questions. A strip whose replacement is a plain reconcile/trainer
        // grant only needs the SPELL to exist; a strip whose replacement is a TALENT grant needs the
        // NODE to be wired (a node can exist as a display-only seed with an empty rankSpell — 62 such
        // TBC druid nodes shipped with zero era_talent_rank rows — in which case CurrentRank can never
        // be non-zero and the talent-gated arm would strip unconditionally).
        // Where a replacement is a talent-granted CHAIN whose higher ranks hang off a node-granted
        // rank 1 (TBC Nature's Grasp 946270 off node 20101, TBC Omen 946269 off node 20149), BOTH
        // must hold: a clone that exists but has no node granting it is unobtainable, which is the
        // same strip-without-replacement failure. era-regen.sh emits nodes and clones under one
        // generation stamp so they normally land together — the conjunction is the belt-and-braces
        // for a partially-applied migration, not an expected state.
        //
        // THIRD CONJUNCT — the IMPLEMENTED-ERA ALLOWLIST. A strip whose replacement is talent-gated
        // must also require the era to OWN trees (EraHasTalentTrees): the implemented-era policy
        // keeps the allowlist Vanilla-only until all TBC classes ship as a set, and without this
        // conjunct the committed branch strips a TBC player's stock spells (Shield Slam,
        // Bloodthirst, Last Stand, Sweeping Strikes, the druid NG/Omen chains) while the talent
        // window that grants the replacements is disabled — i.e. the DB rows exist and the nodes
        // are wired, so the other two conjuncts both pass, yet the character can never spend a
        // point to get the clone back. ReconcileBaselineSpells runs unconditionally for real
        // players (EraTalentPin.cpp), so nothing upstream of here filters that case out.
        // (Found by the Phase-4 final review, 2026-09-03.)
        //
        // Consequence, stated so it is not "fixed" later: with the era in the allowlist every arm
        // behaves exactly as the phase's verification runs measured; with it out, the TBC arms are
        // inert — the character keeps stock everything, which IS the policy intent for an era whose
        // trees are not yet exposed. ERA_VANILLA passes in both states, so Vanilla is untouched.
        static bool EraReplacementSpellReady(EraId era, uint32 spellId)
        {
            return EraHasTalentTrees(era) && spellId != 0 && sSpellMgr->GetSpellInfo(spellId) != nullptr;
        }

        static bool EraNodeIsWired(EraId era, uint8 classId, uint32 nodeId)
        {
            // Third conjunct (Phase-4 review C-1, extended 2026-09-03 to this predicate): with the era
            // outside the implemented-era allowlist the character is on the NATIVE talent frame and no
            // era node can grant anything back — every arm keyed on this predicate must stay inert, or a
            // committed Vanilla-only allowlist would strip stock spells from a TBC player with nothing
            // to replace them. Matches EraReplacementSpellReady / EraNodeReplacesStock.
            if (!EraHasTalentTrees(era))
                return false;
            for (const EraTalentNode* n : sEraTalentContent->NodesFor(uint8(era), classId))
                if (n->id == nodeId)
                    for (uint32 s : n->rankSpell)
                        if (s != 0)
                            return true;
            return false;
        }

        // Same guard for a version-swap whose replacement CLONE ID IS NOT YET ALLOCATED — the TBC
        // rogue substrate (Phase 5, 2026-09-02) ships its Preparation / Adrenaline Rush /
        // Premeditation stock strips one task BEFORE the authoring task mints their 947xxx clones,
        // so there is no literal id to name here the way the warrior arms name 947512/947547.
        // It asks the same three questions as EraReplacementSpellReady + EraNodeIsWired, but
        // THROUGH the node: the era owns trees, the node grants something that exists, and what it
        // grants is NOT the stock spell the caller is about to strip.
        // That third conjunct is what makes it safe to ship ahead of the authoring verdict: if the
        // tree ends up GRANTING THE STOCK SPELL after all (the disposition several rogue nodes
        // took), the predicate stays false and the strip never fires — whereas a hardcoded clone
        // id would strip the node's own grant. A display-only seed is likewise false, whether it
        // carries no rankSpell at all or the placeholder stock id.
        static bool EraNodeReplacesStock(EraId era, uint8 classId, uint32 nodeId, uint32 stockId)
        {
            if (!EraHasTalentTrees(era))
                return false;
            for (const EraTalentNode* n : sEraTalentContent->NodesFor(uint8(era), classId))
                if (n->id == nodeId)
                    for (uint32 s : n->rankSpell)
                        if (s != 0 && s != stockId && sSpellMgr->GetSpellInfo(s) != nullptr)
                            return true;
            return false;
        }

        // A spell that is baseline-obtainable starting some era but was talent-only before it.
        // Two flavors, distinguished by grantHigherEra:
        //   * AUTO-baseline (grantHigherEra = true, e.g. Holy Nova): from TBC on the core LEARNS
        //     rank 1 automatically at minLevel, so we must force-grant it to a high-era character
        //     that doesn't have it yet (and strip it from a Vanilla-without-talent character).
        //   * TRAINER-baseline (grantHigherEra = false, e.g. Shield Slam / Ice Block / Divine
        //     Spirit): from WotLK the trainer SELLS it ungated, but the character still has to buy
        //     it. So in TBC/WotLK we do NOTHING (no force-grant, no strip — they train it normally);
        //     we only strip it from a Vanilla character who hasn't spent the gating talent. This is
        //     the "baseline-leak" case: without this gate a Vanilla character gets the spell from
        //     the WotLK-era trainer rows without paying the talent it costs in Vanilla.
        struct BaselineSpellGate
        {
            uint8    classId;         // p->getClass()
            uint32   talentId;        // the Vanilla-era talent that grants rankSpells[0]
            uint32   rankSpells[8];   // 0 = unused slot (all ranks that must be stripped in Vanilla)
            uint8    minLevel;        // AUTO-baseline: level the rank1 auto-learns. Ignored when !grantHigherEra.
            bool     grantHigherEra;  // true = force-grant rank1 in TBC/WotLK; false = strip-only (trainer-baseline)
        };

        static const BaselineSpellGate kBaselineSpellGates[] =
        {
            // Holy Nova: talent-only in Vanilla, AUTO-baseline (rank 1 level-learned) from TBC on.
            { CLASS_PRIEST,  18121, { 15237, 15430, 15431 },                          20, true  },
            // Baseline-leak gates (2026-08-18): talent-only in Vanilla, TRAINER-baseline in WotLK.
            // grantHigherEra=TRUE, same as Holy Nova: their ungated rank-1 trainer rows are DELETED
            // (SQL 2026_08_18_21_era_talent_baseline_gate_trainers.sql) so the trainer no longer offers
            // them to a Vanilla character — the ORIGINAL strip-only approach (grantHigherEra=false, no
            // trainer edit) leaked because the trainer still LISTED the spell. With rank 1 removed from
            // the trainer, WotLK-stage chars must get it here at the spell's level (then train the higher
            // ranks, which chain ReqAbility1 on rank 1); a Vanilla char gets rank 1 only from the talent.
            // Shield Slam: WOTLK-ONLY force-grant as of the final review R7 (2026-09-06). Neither
            // managed era grants a stock rank any more — Vanilla node 18552 grants the clone chain
            // 932841-932844 and TBC node 20362 grants 947541-947546, because every rank either era
            // can reach ships the WotLK damage (r1 294-308 vs 225-235, and so on up). Both managed
            // eras therefore strip this whole rank list UNCONDITIONALLY in the CLASS_WARRIOR arm (4);
            // the row survives for its `era >= ERA_TBC` half, which force-grants stock r1 at L40 to a
            // WotLK-stage warrior whose ungated rank-1 trainer row is globally deleted, and for its
            // rank LIST, which arm (4) reads so the two can never drift apart. The TBC skip guard in
            // the walker below is matched by a Vanilla one only in effect: Vanilla falls into the
            // walker's `else` (strip when the talent is unspent), which is a strict subset of arm
            // (4)'s unconditional strip, so no extra guard is needed.
            { CLASS_WARRIOR, 18552, { 23922, 23923, 23924, 23925, 25258, 30356, 47487, 47488 }, 40, true  },
            // Ice Block: node 18046 grants stock 45438 (single rank) in Vanilla; auto-learned at L30 in WotLK.
            { CLASS_MAGE,    18046, { 45438 },                                        30, true  },
            // Divine Spirit (single-target chain only): node 18113 grants stock rank 1 (14752) in Vanilla;
            // auto-learned at L30 in WotLK.
            { CLASS_PRIEST,  18113, { 14752, 14818, 14819, 27841, 25312, 48073 },     30, true  },
            // Prayer of Spirit (the raid version, 27681/32999/48074): its trainer_spell rows chain
            // ReqAbility1 = Divine Spirit rank 1 (14752), so it is only trainable by a priest who has the
            // Divine Spirit TALENT (which grants 14752). Keyed on the SAME talent 18113 with
            // grantHigherEra=FALSE (strip-only): a Vanilla priest WITHOUT the Divine Spirit talent loses
            // any Prayer of Spirit ranks (and can't train them — no 14752), a Vanilla priest WITH the
            // talent keeps them, and a TBC+ priest is untouched (Prayer of Spirit stays freely trainable
            // behind baseline Divine Spirit). This gates the raid buff to the talent per the design intent
            // — it is the group half of Divine Spirit, so it lives and dies with that talent.
            { CLASS_PRIEST,  18113, { 27681, 32999, 48074 },                          60, false },
            // Consecration (2026-08-18, Paladin Holy node 18606): talent-only in Vanilla, TRAINER-baseline
            // in WotLK (rank 1 = 26573, trained at L20, ReqAbility1=0 = ungated). Its rank-1 trainer row is
            // DELETED (2026_08_18_32_era_talent_paladin_baseline_gate_trainers.sql) so the trainer no longer
            // offers it; WotLK-stage paladins auto-learn rank 1 here at L20 (then train the higher ranks
            // 20116/20922/... which chain ReqAbility1 on 26573). A Vanilla paladin gets rank 1 only from the
            // talent grant. Same grantHigherEra=true pattern as Shield Slam / Divine Spirit.
            { CLASS_PALADIN, 18606, { 26573, 20116, 20922, 20923, 20924, 27173, 48818, 48819 }, 20, true  },
            // Blessing of Kings (2026-08-18, Paladin Protection node 18620): talent-only in Vanilla,
            // TRAINER-baseline in WotLK (20217, trained at L20, ReqAbility1=0 = ungated). Single rank
            // (no higher-rank chain). Its ungated trainer row is DELETED in
            // 2026_08_18_32_era_talent_paladin_baseline_gate_trainers.sql so the trainer no longer
            // offers it; WotLK-stage paladins auto-learn it here at L20. A Vanilla paladin gets it only
            // from the talent grant (node 18620). Same grantHigherEra=true pattern as Consecration.
            { CLASS_PALADIN, 18620, { 20217 },                                        20, true  },
            // Greater Blessing of Kings (2026-08-18, same Paladin Protection node 18620): a SEPARATE
            // stock chain from BoK 20217. In WotLK GBoK is a real baseline trainer spell (SpellLevel 60,
            // aura 137 = +10% all stats), but its `trainer_spell` rows (TrainerIds 3/4/5) carry
            // ReqAbility1=0 (ungated) at ReqLevel 60, so a Vanilla-era paladin could TRAIN it without the
            // BoK talent — the same leak as 20217. In Vanilla, Greater Blessing of Kings did NOT exist as
            // a trainable baseline (it was the reagent/talent-gated raid version), so a Vanilla paladin
            // WITHOUT the BoK talent must not have it. Its ungated trainer rows are DELETED in
            // 2026_08_18_32_era_talent_paladin_baseline_gate_trainers.sql. This is its OWN gate row (not
            // merged into the 20217 row) because the force-grant path only grants rankSpells[0] at
            // minLevel: GBoK's real learn level is 60, not 20, so it needs its own minLevel to be granted
            // back to WotLK-stage paladins at L60 (folding it into the L20 row would grant only 20217 and
            // silently drop GBoK for WotLK paladins). Keyed on the same talent 18620 so the Vanilla strip
            // fires exactly when the BoK talent is absent. 25898 is NOT a node grant (not in
            // era_talent_rank), so the generic orphan strip never touches it — this gate solely owns it.
            { CLASS_PALADIN, 18620, { 25898 },                                        60, true  },
            // Faerie Fire (Feral) (2026-08-21, Druid Feral node 18729): talent-only in Vanilla, but the
            // druid trainer (Id 33) taught stock 16857 at L18 with ReqAbility1=0 (ungated) — a Vanilla
            // druid could train the debuff without the talent (the leak). Its trainer row is DELETED
            // (2026_08_21_00_era_talent_druid_ng_trainer.sql), so WotLK-stage druids auto-learn 16857
            // here at L18 (grantHigherEra=true, same as Consecration/Shield Slam); a Vanilla druid gets it
            // ONLY from the talent grant (node 18729 grants 16857). 16857 is also a node grant, so the
            // generic orphan strip covers a respec too. Single rank (no higher-rank chain).
            { CLASS_DRUID,   18729, { 16857 },                                        18, true  },
        };
    }

    void StripWrongFactionTbcPalSeal(Player* p)
    {
        // Blood Elf is the only Horde paladin race => Horde keeps Seal of Blood (946080), Alliance
        // (Human/Dwarf/Draenei) keeps Seal of Vengeance (946084). Strip the other if held. Called
        // both mid-reconcile (human path) and on every bot login (the factory trainer-walk is
        // class-only-gated, so a bot learns both and only this login-time pass removes the wrong one).
        if (!p || p->getClass() != CLASS_PALADIN)
            return;
        uint32 wrongFactionSeal = (p->GetTeamId() == TEAM_HORDE) ? 946084u   // Horde/BE: drop Seal of Vengeance
                                                                 : 946080u;  // Alliance: drop Seal of Blood
        if (p->HasSpell(wrongFactionSeal))
            p->removeSpell(wrongFactionSeal, SPEC_MASK_ALL, false);
    }

    void ReconcileBaselineSpells(Player* p, EraId era)
    {
        // THREAD_LOCAL, not a plain static: this is re-entrancy protection for ONE call stack, and
        // reconcile is reached from MAP threads, not just the world thread — Player::GiveLevel fires
        // OnPlayerLevelChanged inside Map::Update, and maps update in parallel on the MapUpdater pool
        // (the standing "module globals need locking on map threads" lesson). A process-global bool
        // would let one map thread's guard suppress another's entire reconcile pass, silently skipping
        // a character's strips/grants. Made thread_local when e1e7308 routed every WotLK-band bot
        // level-up through here, multiplying the concurrent callers.
        static thread_local bool s_inReconcile = false;   // reconcile calls learnSpell, which can fire
        if (s_inReconcile)                                // OnPlayerLearnSpell -> re-enter here; the
            return;                                       // outer call already does the full pass
        s_inReconcile = true;
        struct ReconcileReentryGuard { ~ReconcileReentryGuard() { s_inReconcile = false; } } _reentryGuard;

        if (!p)
            return;

        // Warlock marker chain (TBC Phase 8, 2026-09-05). FOUR markers, replacing the old two-marker
        // Vanilla/non-Vanilla split. Each is a hidden passive with a CLIENT row (the Blizzard trainer UI
        // formats the "Requires ..." line off the ReqAbility spell's NAME and throws a Lua error on a
        // nameless one — the original stone-trainer crash). Reconcile GRANTS the era's own marker; the
        // wrong-era markers are era-allowlisted band ids the band sweep (EraBandClassifier) removes:
        //   932930 "Era: Pre-Wrath Cast Times" — patch 0019 keys Corruption's 2 s cast off
        //          HasAura(932930). TBC Corruption is ALSO a 2 s cast at every rank (wago 2.5.4
        //          CastingTimeIndex 5 on 172..25311; WotLK 3.0 made it instant), so a Vanilla warlock
        //          — and a TBC warlock ONCE THE TBC TREES ARE LIVE — carries it. It is NO LONGER a
        //          trainer gate: that job split into 932994/948410.
        //   932994 "Era: Vanilla Warlock" — trainer gate for the Vanilla Create-stone clones (932922-928).
        //   948410 "Era: TBC Warlock"     — trainer gate for the TBC Create-stone clones (948420-428).
        //   932939 "Era: WotLK Warlock"   — gate for the stock WotLK-item Create rows. NARROWED from
        //          `era != ERA_VANILLA` to `era == ERA_WOTLK`: a TBC warlock must NOT see the WotLK
        //          enchant stones now that TBC has its own set.
        // EVERY TBC arm is readiness-gated (EraReplacementSpellReady) so that, with TBC outside the
        // EraHasTalentTrees allowlist or the marker not yet in spell_dbc, nothing is granted that
        // gates rows which do not exist — the hunter 948280 precedent.
        {
            struct WarlockMarker { uint32 id; bool want; };
            bool const wl = p->getClass() == CLASS_WARLOCK;
            // THE TBC READINESS FALLBACK IS LOAD-BEARING, AND EVERY ARM TAKES IT (the hunter 948280
            // lesson, EraTalents.cpp:844): with TBC outside the EraHasTalentTrees allowlist — the
            // COMMITTED state of EraTalentIP.cpp — a TBC warlock must keep the EXACT pre-Phase-8
            // state, not fall into a half-armed branch. Pre-Phase-8 that state is: 932939 (so the
            // stock WotLK enchant stones stay trainable — otherwise the character gets NO stone
            // marker and therefore no stones at all) and NO 932930, i.e. Corruption stays INSTANT.
            // The 932930 arm must therefore ride the same readiness flag as the stone arms: the
            // native WotLK Improved Corruption a non-allowlisted TBC warlock is playing with has no
            // cast-time reduction, so arming patch 0019's 2 s cast for them would leave a Corruption
            // that is strictly slower with no talent able to bring it back down.
            // NB: if TBC is ever removed from EraHasTalentTrees, this fallback grant is an allowlist
            // era mismatch for a TBC-band character and the band sweep strips it each reconcile — the
            // allowlist eras, not this arm, decide.
            bool const tbcWarlockReady = EraReplacementSpellReady(ERA_TBC, 948410);
            WarlockMarker const markers[] =
            {
                { 932930, wl && (era == ERA_VANILLA || (era == ERA_TBC && tbcWarlockReady)) },
                { 932994, wl && era == ERA_VANILLA && EraReplacementSpellReady(ERA_VANILLA, 932994) },
                { 948410, wl && era == ERA_TBC     && tbcWarlockReady },
                { 932939, wl && (era == ERA_WOTLK || (era == ERA_TBC && !tbcWarlockReady)) },
            };
            for (WarlockMarker const& m : markers)
            {
                if (m.want && !p->HasSpell(m.id))
                    p->learnSpell(m.id);
            }
        }

        // ---------------- Warlock ----------------
        // Everything below is a STOCK-side arm. The custom band castables (the Vanilla/TBC Create
        // Firestone/Spellstone sets, the Siphon Life / Conflagrate / Dark Pact / Unstable Affliction /
        // Shadowfury clone chains, the four era markers) need no strip here: they are node grants,
        // spell_ranks ranks above one, or era-allowlisted trainer chains, so the band sweep
        // (EraBandClassifier) at the end of this function owns all of them. What is left is the
        // rule the sweep can never apply — a Vanilla/TBC warlock must NOT keep the STOCK spell its
        // era node or trainer gate replaced (the stock WotLK create spells, stock Dark Pact, stock
        // Conflagrate, ...). The trainer still lists some of them (a positive era condition can't be
        // expressed there), so era_talent_pin's OnPlayerLearnSpell re-runs this reconcile to strip a
        // fresh mistake purchase too.
        if (p->getClass() == CLASS_WARLOCK)
        {
            // Demonic Sacrifice: node 18230 originally granted STOCK 18788; it now grants the
            // custom clone 932929 (Vanilla client tooltip). ReapplyOnLogin grants the clone from
            // the recorded rank, but nothing else would strip the stale stock spell off characters
            // that learned the talent before the rework — same pattern as the priest PI/VE strip.
            // 18788 is unreachable for WotLK-stage characters on this core (the WotLK talent tree
            // has no Demonic Sacrifice), so stripping is safe for every era.
            if (p->HasSpell(18788))
                p->removeSpell(18788, SPEC_MASK_ALL, false);

            // Conflagrate: node 18250 originally granted STOCK 17962 (the WotLK 3.0 rework — instant,
            // damage as a % of Immolate, plus a periodic Fire DoT). It now grants the custom Vanilla
            // clone 932780 (1.5s cast, flat Fire damage, no DoT, consumes Immolate). Strip the stale
            // stock 17962 off a Vanilla warlock that learned Conflagrate before the rework. UNLIKE the
            // Demonic Sacrifice strip above, this is era-GATED: 17962 is a legitimate WotLK Destruction
            // talent for WotLK-stage warlocks, so it must only be stripped in the Vanilla era.
            if (era == ERA_VANILLA && p->HasSpell(17962))
                p->removeSpell(17962, SPEC_MASK_ALL, false);

            // Dark Pact: node 18217 originally granted STOCK 18220 (the WotLK 304-mana drain). It now grants
            // the Vanilla clone 932784 (150). Strip the stale STOCK Dark Pact chain off a Vanilla warlock
            // that learned it before the rework — the whole chain (R1 18220 + the trainer-taught higher ranks
            // 18937/18938/27265/59092) so a Vanilla warlock keeps none of the WotLK-valued ranks. Era-gated:
            // stock Dark Pact is legitimate for WotLK-stage warlocks (their tree grants 18220 + trains higher
            // ranks). Same grant-change strip as the Conflagrate 17962 / Demonic Sacrifice 18788 blocks.
            static const uint32 kStockDarkPact[] = { 18220, 18937, 18938, 27265, 59092 };
            if (era == ERA_VANILLA)
                for (uint32 spell : kStockDarkPact)
                    if (p->HasSpell(spell))
                        p->removeSpell(spell, SPEC_MASK_ALL, false);

            // ---------------- TBC warlock (Phase 8) ----------------
            // Grant-change stock swaps — a TBC node hands over a CLONE, so the stock id must go
            // (lesson 17f: the band sweep (EraBandClassifier) only sees managed-band ids, never a
            // stock one). Readiness-gated per row
            // via EraNodeReplacesStock (the hunter/rogue/priest shape): while the node is a seed or
            // still grants stock, the arm is dormant. `ranks` lists the STOCK trained ranks that hang
            // off a swapped r1 — the r1 strip cascades down the stock spell_ranks chain, and the list
            // is the safety-net for a character whose r1 is already gone. Stock trainer rows are NOT
            // deleted (native WotLK warlocks still train them off their own stock r1).
            // NB Shadowburn (20750) and Dark Pact (20717) are GRANTS of the stock ids in TBC
            // (Amendment A.1) — deliberately absent from this table.
            if (era == ERA_TBC)
            {
                struct WarlockStockSwap { uint32 node; uint32 stockR1; uint32 ranks[4]; };
                static const WarlockStockSwap kEraWlTbcStockSwaps[] =
                {
                    { 20713, 18265, { 0, 0, 0, 0 } },                          // Siphon Life (WotLK passive)
                    { 20760, 17962, { 0, 0, 0, 0 } },                          // Conflagrate (WotLK rework)
                    { 20720, 30108, { 30404, 30405, 47841, 47843 } },          // Unstable Affliction + stock trained ranks
                    { 20763, 30283, { 30413, 30414, 47846, 47847 } },          // Shadowfury + stock trained ranks
                    { 20708, 18288, { 0, 0, 0, 0 } },                          // Amplify Curse (WotLK passive)
                    { 20728, 18708, { 0, 0, 0, 0 } },                          // Fel Domination (TBC 15-min clone)
                    { 20739, 19028, { 0, 0, 0, 0 } },                          // Soul Link stock activatable
                    // 20734 Demonic Sacrifice's stock 18788 is already stripped unconditionally for
                    // EVERY era by the block above, so it needs no row here.
                };
                for (const WarlockStockSwap& sw : kEraWlTbcStockSwaps)
                {
                    if (!EraNodeReplacesStock(ERA_TBC, CLASS_WARLOCK, sw.node, sw.stockR1))
                        continue;
                    if (p->HasSpell(sw.stockR1))
                        p->removeSpell(sw.stockR1, SPEC_MASK_ALL, false);
                    for (uint32 spell : sw.ranks)
                        if (spell != 0 && p->HasSpell(spell))
                            p->removeSpell(spell, SPEC_MASK_ALL, false);
                }
            }

            // Create Firestone/Spellstone: TRAINER-TAUGHT (never auto-granted), gated per era exactly
            // like the shaman totems. Each set is gated by its OWN era marker: the custom Vanilla
            // clones (932922-928) on 932994, the TBC clones (948420-428) on 948410
            // (2026_09_07_02_era_talent_tbc_warlock_markers_stones.sql), and the stock WotLK-item
            // chains on 932939 (2026_08_24_11_era_talent_warlock_stone_trainer_gate.sql) — so each era's
            // warlock sees ONLY its own stones on the trainer. (Before TBC Phase 8 the Vanilla gate sat
            // on 932930, which now belongs to Vanilla AND TBC and can no longer separate them.) EVERY
            // marker carries a client: block so the trainer UI can render its "Requires ..." line (an
            // invisible marker crashes it — the original bug).
            // Only the STOCK set needs an arm here: the custom Vanilla/TBC Create sets are era-
            // allowlisted band ids, so the band sweep (EraBandClassifier) removes the wrong era's set.
            static const uint32 kStockStoneCreates[] =
                { 6366, 17951, 17952, 17953, 27250, 60219, 60220,   // Create Firestone ranks (WotLK items)
                  2362, 17727, 17728, 28172, 47886, 47888 };        // Create Spellstone ranks (WotLK items)
            // Stock set: always wrong for Vanilla; wrong for TBC ONLY once the TBC clones are ready
            // (same fallback as the marker table above — a not-ready TBC warlock keeps 932939 and the
            // stock stones, which is exactly pre-Phase-8 behaviour).
            if (era == ERA_VANILLA || (era == ERA_TBC && EraReplacementSpellReady(ERA_TBC, 948410)))
                for (uint32 spell : kStockStoneCreates)
                    if (p->HasSpell(spell))
                        p->removeSpell(spell, SPEC_MASK_ALL, false);
        }

        // ---------------- Hunter ----------------
        if (p->getClass() == CLASS_HUNTER)
        {
            // Baseline-leak sweep (Task 8 Step 1): stock WotLK Deterrence 19263 (Vanilla talent -> WotLK
            // baseline; trainer.Id 7, L60) was UNGATED, so a Vanilla-era hunter could train the WotLK
            // deflect rework every login even without the talent (which grants the era clone 932964, node
            // 18339). Its trainer row is now RE-GATED behind the "WotLK Hunter" marker 932305
            // (2026_08_25_00_era_talent_hunter_deterrence_gate.sql), so a Vanilla hunter (no 932305) never
            // sees it; this strip stays as spellbook cleanup for anyone who trained it before that gate +
            // the era-transition net. (The stock Aimed Shot 20900-20904 and Counterattack 20909/20910
            // trainer ranks are self-gated by their ReqAbility1 = stock rank 1 19434/19306, which a
            // Vanilla hunter never has, so they need no strip.)
            if (era == ERA_VANILLA && p->HasSpell(19263))
                p->removeSpell(19263, SPEC_MASK_ALL, false);

            // Scorpid Sting baseline version-swap (helpers 932300-303, node 18328 Improved Scorpid
            // Sting). Stock 3043 is the WotLK -hit-chance rework; the authentic Vanilla Str/Agi 4-rank
            // chain is a custom clone set, TRAINER-taught (trainer.Id 7) behind the "Vanilla Hunter"
            // marker 932304, while stock 3043 is re-gated behind the "WotLK Hunter" marker 932305
            // (2026_08_24_12_era_talent_hunter_scorpid_sting.sql). BOTH markers carry a client: block so
            // the trainer UI can render the "Requires ..." line (an invisible marker crashes it — the
            // warlock-stone lesson). Grant the era-matching marker; the WRONG-era markers (932304/
            // 932305/948280) and the Scorpid clones (932300-303 / 948281) are era-allowlisted band ids
            // that the band sweep (EraBandClassifier) removes, so only the STOCK strips live here.
            // Never auto-grants the Scorpid Sting ranks (trainer-taught).
            if (era == ERA_VANILLA)
            {
                if (!p->HasSpell(932304))
                    p->learnSpell(932304);                                    // grant "Vanilla Hunter" marker
                if (p->HasSpell(3043))                                        // stock Scorpid Sting: WotLK rework
                    p->removeSpell(3043, SPEC_MASK_ALL, false);
            }
            // ---------------- TBC hunter (Phase 7) ----------------
            // Third marker of the version-swap chain (shaman three-marker precedent, commit 9b69360).
            // A TBC hunter must hold NEITHER 932304 (would unlock the Vanilla Scorpid clones) NOR
            // 932305 (would unlock stock 3043 AND the stock Deterrence 19263 trainer row — Deterrence
            // is a TBC TALENT, node 20650); the band sweep removes both.
            //
            // THE READINESS CONJUNCT IS ON THE `else if`, NOT INSIDE IT, AND THAT IS LOAD-BEARING.
            // It guards exactly one case: TBC outside the EraHasTalentTrees allowlist (the COMMITTED
            // state of EraTalentIP.cpp — the local TBC-on edit is deliberately unmerged). With the
            // conjunct inside the arm, a not-ready TBC hunter fell into an EMPTY branch and got NO
            // marker at all, losing the 932305 the pre-Phase-7 `else` used to grant — i.e. losing
            // access to stock Scorpid Sting and stock Deterrence with no era replacement. Failing
            // through to the WotLK arm instead makes "not ready" byte-identical to pre-Phase-7
            // behaviour, so it is a safe fallback.
            // (The shaman's 947440 marker arm is ungated because its era shipped with the allowlist;
            // this one must survive an allowlist-off build.)
            else if (era == ERA_TBC && EraReplacementSpellReady(ERA_TBC, 948280))
            {
                if (!p->HasSpell(948280))
                    p->learnSpell(948280);                                    // grant "TBC Hunter" marker
                // Stock 3043 is WotLK's -3%: strip it ONLY once the TBC clone is trainable (else a
                // TBC hunter would have no Scorpid Sting at all).
                if (EraReplacementSpellReady(ERA_TBC, 948281) && p->HasSpell(3043))
                    p->removeSpell(3043, SPEC_MASK_ALL, false);
                if (p->HasSpell(19263))                                       // stock Deterrence: WotLK-only row
                    p->removeSpell(19263, SPEC_MASK_ALL, false);
            }
            else
            {
                // WotLK hunter (unchanged behaviour; the band sweep drops the Vanilla/TBC markers and
                // Scorpid clones on a band move up).
                // NB: if TBC is ever removed from EraHasTalentTrees, this fallback grant is an allowlist
                // era mismatch for a TBC-band character and the band sweep strips it each reconcile — the
                // allowlist eras, not this arm, decide.
                if (!p->HasSpell(932305))
                    p->learnSpell(932305);                                    // grant "WotLK Hunter" marker
            }

            // Grant-change stock swaps — a TBC node hands over a CLONE, so the stock id must go (lesson
            // 17f). Readiness-gated per row via EraNodeReplacesStock (rogue/priest shape): dormant while
            // the node is a seed or grants stock. Plus the STOCK trained ranks behind a swapped r1 — the
            // r1 strip cascades through the stock spell_ranks chain; the list catches a char whose r1 is
            // already gone. Stock trainer rows are NOT deleted (native hunters train them; WotLK ranks
            // 49049/49050, 48998/48999, 49011/49012 must stay reachable — _ref capstone_chains).
            if (era == ERA_TBC)
            {
                struct HunterStockSwap { uint32 node; uint32 stockR1; uint32 ranks[8]; };
                static const HunterStockSwap kEraHunTbcStockSwaps[] =
                {
                    { 20627, 19434, { 20900, 20901, 20902, 20903, 20904, 27065, 49049, 49050 } },   // Aimed Shot
                    { 20637, 19506, { 0, 0, 0, 0, 0, 0, 0, 0 } },                                  // Trueshot Aura (r2-4 ABSENT live, A.9)
                    { 20656, 19306, { 20909, 20910, 27067, 48998, 48999, 0, 0, 0 } },              // Counterattack
                    { 20660, 19386, { 24132, 24133, 27068, 49011, 49012, 0, 0, 0 } },              // Wyvern Sting
                    // Deterrence — belt-and-braces: the marker arm above strips 19263 unconditionally;
                    // this row covers a hunter whose marker arm has not run yet (e.g. 948280 absent).
                    { 20650, 19263, { 0, 0, 0, 0, 0, 0, 0, 0 } },
                    { 20617, 19574, { 0, 0, 0, 0, 0, 0, 0, 0 } },                                  // Bestial Wrath
                    { 20620, 34692, { 0, 0, 0, 0, 0, 0, 0, 0 } },                                  // The Beast Within passive (A.6)
                    { 20663, 23989, { 0, 0, 0, 0, 0, 0, 0, 0 } },                                  // Readiness (A.6)
                    { 20640, 34490, { 0, 0, 0, 0, 0, 0, 0, 0 } },                                  // Silencing Shot (A.6)
                    { 20632, 19503, { 0, 0, 0, 0, 0, 0, 0, 0 } },                                  // Scatter Shot (clone 948382; gap 11 half CLOSED)
                };
                for (const HunterStockSwap& sw : kEraHunTbcStockSwaps)
                {
                    if (!EraNodeReplacesStock(ERA_TBC, CLASS_HUNTER, sw.node, sw.stockR1))
                        continue;
                    if (p->HasSpell(sw.stockR1))
                        p->removeSpell(sw.stockR1, SPEC_MASK_ALL, false);
                    for (uint32 spell : sw.ranks)
                        if (spell != 0 && p->HasSpell(spell))
                            p->removeSpell(spell, SPEC_MASK_ALL, false);
                }
            }
        }

        // ---------------- Rogue ----------------
        // NO HEMORRHAGE ARM IS NEEDED IN EITHER MANAGED ERA. Vanilla's clone chain 932985-987 is a
        // band id set: r1 is node-granted by 18449 and r2/r3 are spell_ranks members above it, so the
        // band sweep (EraBandClassifier) removes the whole chain from a rogue who is no longer a
        // Vanilla rogue holding the talent. TBC (Phase 5, 2026-09-02) needs nothing either — spec
        // Amendment A.4 and the survival sweep in era-data/_ref/tbc/rogue-spells.yaml
        // (capstone_chains.hemorrhage): stock 16511/17347/17348/26864 are LIVE_MATCH on all fourteen
        // verdict families, so 3.3.5a never reworked Hemorrhage, TBC node 20459 GRANTS STOCK 16511,
        // and the stock TrainerId-9 rows already gate the trained ranks exactly as TBC did (no TBC
        // clone chain, no trainer SQL, and the Vanilla clone chain is not reused or extended).
        if (p->getClass() == CLASS_ROGUE)
        {
            // Preparation grant change (2026-08-18): node 18447 now grants the era clone 932983
            // (resets Blind too) instead of stock 14185. A rogue who learned Preparation before this
            // change keeps stock 14185 (WotLK behavior, no Blind) alongside the clone — strip it. A
            // Vanilla rogue never legitimately has stock 14185 (it's talent-granted only, never
            // trainer-taught), so strip it whenever the char is Vanilla-era; ReapplyOnLogin re-grants
            // the clone from the node. Same grant-change strip pattern as the priest VE/PI rebuild.
            if (era == ERA_VANILLA && p->HasSpell(14185))
                p->removeSpell(14185, SPEC_MASK_ALL, false);

            // Adrenaline Rush grant change (2026-08-18): node 18434 now grants the era clone 932770
            // (Vanilla 5-min cd) instead of stock 13750 (WotLK 3-min cd). A rogue who learned Adrenaline
            // Rush before this change keeps stock 13750 alongside the clone — strip it. 13750 is
            // talent-granted only (never trainer-taught), so a Vanilla rogue never legitimately has it;
            // ReapplyOnLogin re-grants the clone from the node. Era-gated so WotLK rogues keep stock
            // Adrenaline Rush (3-min cd). Same grant-change strip as the Preparation 14185 strip above.
            if (era == ERA_VANILLA && p->HasSpell(13750))
                p->removeSpell(13750, SPEC_MASK_ALL, false);

            // Riposte grant change (FIX ROUND 2026-09-03): node 18423 now grants the era clone
            // 932989 (Vanilla's 6-sec DISARM) instead of stock 14251. The Vanilla tree shipped
            // granting stock 14251 on a FALSE "unchanged Vanilla->WotLK" reading; live 14251 is
            // WotLK 3.0.2's retune (Effect_2 = aura 138 MOD_MELEE_HASTE -20% for 30 s with
            // EffectMechanic 8 SLOW_ATTACK, plus an Effect_3 = 80 ADD_COMBO_POINTS Vanilla never
            // had), so a Vanilla rogue who learned Riposte before this fix keeps stock 14251
            // alongside the clone — strip it. 14251 is talent-granted only (no trainer_spell row —
            // live table queried 2026-09-03; SkillLineAbility 7816 AcquireMethod 0), so a Vanilla
            // rogue never legitimately has it; ReapplyOnLogin re-grants the clone from the node.
            // Era-gated so WotLK rogues keep stock Riposte (the slow + combo point) — same
            // grant-change strip as the Preparation 14185 / Adrenaline Rush 13750 strips above.
            // NOTE this arm is deliberately NOT readiness-gated the way kEraRogTbcStockSwaps below
            // is: the Vanilla rogue tree is long since live and in the implemented-era allowlist, so
            // node 18423 always has a replacement to hand over — there is no display-only window in
            // which stripping would leave a Vanilla rogue with neither version. The TBC side of the
            // same swap is the {20428, 14251} row in kEraRogTbcStockSwaps and needs no change here.
            if (era == ERA_VANILLA && p->HasSpell(14251))
                p->removeSpell(14251, SPEC_MASK_ALL, false);

            // Premeditation grant change (FINAL-REVIEW FIX ROUND 2026-09-06, G-39): node 18451 now
            // grants the era clone 932984 (Vanilla's 10-sec combo-point retain window) instead of
            // stock 14183. The Vanilla tree shipped granting stock 14183 with a NAMED MINOR
            // DEVIATION -- live's DurationIndex 18 = 20 s retain window against Vanilla's 10 s --
            // which the TBC slice's verified 947787 clone made unnecessary to keep. A rogue who
            // learned Premeditation before this change keeps stock 14183 alongside the clone --
            // strip it. 14183 is talent-granted only (no trainer_spell row; SkillLineAbility 8077
            // AcquireMethod 0), so a Vanilla rogue never legitimately has it; ReapplyOnLogin
            // re-grants the clone from the node. Era-gated so WotLK rogues keep stock Premeditation
            // (the 20 s window) -- same grant-change strip as the Preparation 14185 / Adrenaline
            // Rush 13750 / Riposte 14251 strips above, and deliberately NOT readiness-gated for the
            // same reason: the Vanilla rogue tree is long since live and in the implemented-era
            // allowlist, so node 18451 always has a replacement to hand over. The TBC side of the
            // same swap is the {20463, 14183} row in kEraRogTbcStockSwaps and needs no change here.
            if (era == ERA_VANILLA && p->HasSpell(14183))
                p->removeSpell(14183, SPEC_MASK_ALL, false);

            // --- TBC stock version-swap strips (Phase 5, 2026-09-02) ---------------------------
            // Spec Amendments A.7 + A.14 and era-data/_ref/tbc/rogue-spells.yaml
            // preparation_resolution / capstone_chains.mutilate: all FOUR of these diverge in TBC
            // and each gets its own FRESH 947xxx clone (no Vanilla clone is reusable — see
            // reuse_ledger, 0 of 5 reused):
            //   * Preparation  (node 20457) — TBC RecoveryTime 600000 (10 min) vs live 480000, and
            //     TBC additionally resets Premeditation, which the core script never does. The
            //     Vanilla clone 932983 is 10 min too but resets BLIND and not Premeditation — the
            //     exact inverse of TBC — so it must not be reused or rebound.
            //   * Adrenaline Rush (node 20441) — sole diff RecoveryTime 300000 (5 min) vs 180000.
            //   * Premeditation  (node 20463) — TBC 10 s retain window / 2-min cooldown vs live's
            //     20 s / 20-s rework.
            //   * MUTILATE (node 20420) — ADDED at Task 6b, spec Amendment **A.14**, which
            //     SUPERSEDES A.5's grant-stock disposition (the Cold Blood note at the end of this
            //     block is corrected too). A.5's DBC finding stands — the whole 12-spell chain is
            //     data-identical — but TBC Mutilate's "+50% vs Poisoned" and "Must be behind the
            //     target" are CORE-side, not spell data (SpellEffects.cpp:3454-3475 applies +20%;
            //     SpellMgr.cpp:3590's by-id CU-attribute switch never lists Mutilate), so the node
            //     now grants the era clone 947773 and two module scripts restore the behaviour.
            //   * RIPOSTE (node 20428) — ADDED at Task 7. TBC Riposte disarms for 6 s; live 14251
            //     is WotLK's 3.0.2 retune (a 30 s -20% melee-attack-speed slow, plus an
            //     ADD_COMBO_POINTS slot TBC does not have), so the node grants the era clone 947777.
            //     Stock 14251 is a NATIVE WotLK talent spell, which is exactly why it belongs here:
            //     a TBC-band character built BEFORE the Combat tab shipped fell through to the
            //     native WotLK talent path and can be holding it.
            // All FIVE stock spells are TALENT-GRANTED ONLY (no trainer_spell of their own — stock
            // 1329's ranks 2-4 have trainer rows but rank 1 does not, and its SkillLineAbility
            // AcquireMethod is 0 — and no baseline path; baseline_leaks: ZERO for this tree), so a
            // managed-era rogue whose node hands over a clone can never legitimately hold the stock
            // id; this is the same grant-change strip as the Vanilla arms above, and lesson 17f's
            // "only STOCK grants need an explicit strip" (the band sweep (EraBandClassifier) already
            // owns every managed-band id).
            //
            // WHY READINESS-GATED RATHER THAN A PLAIN `era == ERA_TBC` (do not "simplify" this):
            // these strips ship one task ahead of the clones. An ungated TBC arm is exactly the
            // strip-without-replacement defect the Phase-4 final review found and fixed
            // (2026-09-03) — while the TBC rogue tree is a display-only seed, or while the era is
            // out of the implemented-era allowlist, a TBC rogue is on the NATIVE WotLK talent frame
            // and can legitimately hold stock 14185/13750/14183/1329/14251 from a native talent
            // (and, for Mutilate, the trained ranks bought off it). Stripping them there leaves the
            // character with neither version and no way to get one back.
            // EraNodeReplacesStock keeps each arm dormant until its node actually grants a
            // non-stock replacement, then self-arms with no further code change. NOTE it is also
            // what makes the Premeditation arm safe against the VANILLA tree, whose node 18451
            // GRANTS STOCK 14183 (a deliberate named deviation — see era-data/vanilla/rogue.yaml):
            // Premeditation gets NO Vanilla strip, unlike its two neighbours.
            struct RogueStockSwap { uint32 node; uint32 stockId; };
            static const RogueStockSwap kEraRogTbcStockSwaps[] =
            {
                { 20457, 14185 },   // Preparation
                { 20441, 13750 },   // Adrenaline Rush
                { 20463, 14183 },   // Premeditation
                { 20420, 1329  },   // Mutilate rank 1 (Amendment A.14, Task 6b)
                { 20428, 14251 },   // Riposte (Task 7 — node grants the 6 s-disarm clone 947777)
            };
            if (era == ERA_TBC)
                for (const RogueStockSwap& sw : kEraRogTbcStockSwaps)
                    if (p->HasSpell(sw.stockId) &&
                        EraNodeReplacesStock(ERA_TBC, CLASS_ROGUE, sw.node, sw.stockId))
                        p->removeSpell(sw.stockId, SPEC_MASK_ALL, false);

            // Mutilate's STOCK TRAINED ranks (34411 L50 / 34412 L60 / 34413 L70) — the LOGIN
            // SAFETY-NET. It has no band-sweep equivalent precisely because these are STOCK ids.
            // The normal path already covers them: Player::removeSpell walks GetNextSpellInChain and
            // recursively removes every NON-TALENT higher rank (Player.cpp:3506-3510), and the stock
            // spell_ranks chain is rooted at 1329 with 34411-34413 (and the WotLK 48663/48666) as
            // plain trained ranks — `GetTalentSpellPos` is non-null only for 1329 itself, the single
            // native Talent.dbc rank. So the swap-table strip of 1329 directly above CASCADES
            // through the whole stock ladder on the same call.
            // This loop is what catches the case where the root is ALREADY gone and the cascade
            // therefore never runs: a character stripped of 1329 by an earlier reconcile (or by a
            // native respec) while still holding a rank it had trained. That is precisely the
            // orphan shape `.eratalents doctor` would otherwise keep reporting, and it costs three
            // HasSpell probes on a rogue login.
            // Gated on the SAME readiness predicate as the swap table so it can never strip without
            // replacement while the TBC tree is still a display-only seed (see the
            // "WHY READINESS-GATED" note above).
            // ERA-SCOPED on purpose: a NATIVE WotLK rogue trains these legitimately (the stock rows
            // stay in trainer_spell — 2026_09_04_02_..._mutilate_trainers.sql deletes nothing, and
            // deleting them would strand the WotLK-only ranks 48663/48666 whose ReqAbility1 chains
            // through 34413), and a Vanilla rogue has no Mutilate node at all.
            static const uint32 kRogueTbcMutilateStockRanks[] = { 34411, 34412, 34413 };
            if (era == ERA_TBC && EraNodeReplacesStock(ERA_TBC, CLASS_ROGUE, 20420, 1329))
                for (uint32 spell : kRogueTbcMutilateStockRanks)
                    if (p->HasSpell(spell))
                        p->removeSpell(spell, SPEC_MASK_ALL, false);

            // Cold Blood 14177 is DELIBERATELY ABSENT from the swap table and is stripped nowhere:
            // Amendment A.7 grants it as STOCK in TBC despite an O-pass mask diff, because two
            // by-id core scripts key off 14177 — spell_rog_cold_blood (bound to the positive id)
            // and spell_rog_mutilate, which does caster->GetAura(14177) with the hardcoded id and
            // then reaches INTO the Cold Blood script instance to decide whether to consume the
            // charge. A band clone breaks that interaction in a way scriptBindings on the clone
            // cannot repair (the hardcoded lookup lives in the OTHER script). Accepted gap: live
            // Cold Blood's spellmod mask drops Shiv coverage.
            // NB the same reasoning does NOT extend to Mutilate itself (an earlier revision of this
            // comment said it granted stock 1329 per A.5 — Amendment A.14 superseded that). Cloning
            // the four VISIBLE Mutilate casts is safe precisely BECAUSE Cold Blood stays stock: the
            // hardcoded GetAura(14177) still resolves, spell_rog_cold_blood's CheckProc still keys
            // on the un-cloned hidden halves' family bits, and `spell_rog_mutilate` is simply
            // REBOUND onto 947773-947776 by scriptBindings.
        }

        // Warrior Bloodthirst / Last Stand / Shield Slam. Originally (2026-08-18) a Vanilla-only pair
        // of grant-change strips; TBC Phase 4 (2026-09-02) makes them THREE-WAY, the same shape as the
        // druid Nature's Grasp arms below. Governing decision: spec Amendment A.2 + the survival sweep
        // in era-data/_ref/tbc/warrior-spells.yaml (capstone_chains.{bloodthirst,last_stand,shield_slam},
        // baseline_leaks). All three DIVERGE in TBC and get their OWN fresh 947xxx clones — there is no
        // Vanilla-clone reuse anywhere in the warrior tree (reuse_ledger: ALL FRESH):
        //   * Bloodthirst — TBC is a SIX-rank ladder (40/48/54/60/66/70) at 45% AP / 30 rage / 6 s
        //     CATEGORY cooldown with a FLAT per-swing heal; live is 50% / 20 rage / 4 s / %-of-max-health.
        //     (The category cooldown is what moved ranks 2-6 off LIVE_MATCH — an effects-only compare
        //     would have shipped a 4-second Bloodthirst.)
        //   * Last Stand — TBC cooldown 480000 ms in the RecoveryTime column vs live 180000 ms under
        //     Category 1251; the Vanilla clone 932840 is 10 min, so it is NOT reusable either.
        //   * Shield Slam — every rank EITHER managed era can reach diverges from stock; both eras get
        //     their own clone chain (Vanilla 932841-844 as of final review R7, TBC 947541-546). Arm (4).
        // Final review R7 (2026-09-06) also adds arm (8): the three stance-CORRECTION passives
        // 932845/932846/932847, which are not a swap at all — they ride ALONGSIDE the core's hardcoded
        // stance passives and are granted to both managed eras.
        // Only the STOCK halves of those swaps live here. The clone chains themselves (Vanilla
        // 932828-831 / 932840, TBC 947512-517 / 947530-947549) are band ids rooted at a node grant, so
        // the band sweep (EraBandClassifier) keeps a chain only while the character's current era still
        // grants its r1 — which is why the arm numbering below starts at (2) and skips (5).
        if (p->getClass() == CLASS_WARRIOR)
        {
            // (2) Stock Bloodthirst version-swap. BOTH managed eras replace stock with a clone, so
            //     stock is never legitimate on a managed warrior; only the ranks each era can reach
            //     differ. Vanilla arm UNCHANGED (r1 23881 only — a Vanilla warrior cannot reach any
            //     other stock rank: stock Bloodthirst has NO trainer_spell and NO spell_ranks rows on
            //     this server, verified by the Task-3 sweep). TBC strips the whole stock six-rank
            //     ladder: harmless and future-proof, exactly like the Shield Slam gate keeping its
            //     unreachable r7/r8. 23881 is talent-granted only, so ReapplyOnLogin re-grants the
            //     era clone from the node.
            //     READINESS: the TBC strip is gated on the TBC chain actually existing AND node 20340
            //     being wired — while the TBC tree is a display-only seed, stripping stock here would
            //     leave a TBC warrior with no Bloodthirst and nothing to replace it (the same
            //     strip-without-replacement guard the druid NG/Omen arms use).
            if (era == ERA_VANILLA && p->HasSpell(23881))
                p->removeSpell(23881, SPEC_MASK_ALL, false);
            if (era == ERA_TBC && EraReplacementSpellReady(ERA_TBC, 947512) &&
                EraNodeIsWired(ERA_TBC, CLASS_WARRIOR, 20340))
            {
                static const uint32 kStockWarBt[] = { 23881, 23892, 23893, 23894, 25251, 30335 };
                for (uint32 spell : kStockWarBt)
                    if (p->HasSpell(spell))
                        p->removeSpell(spell, SPEC_MASK_ALL, false);
            }

            // (3) Last Stand, same three-way. Vanilla clone 932840 (node 18541, 10-min cd +
            //     era_last_stand script); TBC clone 947547 (node 20349, 8-min cd — Amendment A.2).
            //     Single rank each, no trained chain. The wrong-era CLONE needs no arm — the band
            //     sweep (EraBandClassifier) strips it; the STOCK 12975 strip is Vanilla-unchanged
            //     plus a readiness-gated TBC arm, mirroring (2).
            if (era == ERA_VANILLA && p->HasSpell(12975))
                p->removeSpell(12975, SPEC_MASK_ALL, false);
            if (era == ERA_TBC && EraReplacementSpellReady(ERA_TBC, 947547) &&
                EraNodeIsWired(ERA_TBC, CLASS_WARRIOR, 20349) && p->HasSpell(12975))
                p->removeSpell(12975, SPEC_MASK_ALL, false);

            // (4) Shield Slam — the TBC half of the kBaselineSpellGates row keyed on Vanilla node
            //     18552. Shield Slam is a TALENT in BOTH managed eras (Vanilla 18552, TBC 20362) and
            //     only becomes baseline in WotLK, but its ungated rank-1 trainer row is globally
            //     DELETED (2026_08_18_21 SQL), so the gate walker's era >= ERA_TBC arm force-grants
            //     stock r1 at L40 — which for a TBC warrior would leak stock Shield Slam AND unlock
            //     the whole stock trained chain behind it (ReqAbility1 = 23922). That is THE one real
            //     baseline leak the warrior sweep found, and it is created by our own substrate:
            //     era-data/_ref/tbc/warrior-spells.yaml baseline_leaks.trained_rank_chains → Shield Slam.
            //     WHY THIS ARM IS NOT NODE-GATED (unlike the druid Faerie Fire (Feral) arm it is
            //     otherwise modelled on): TBC node 20362 grants the CLONE chain 947541-947546, never
            //     the stock ranks (Amendment A.2 — every TBC-reachable stock rank's damage diverges,
            //     224/263/302/341/380/419 vs live 293/345/395/446/498/548). So a 20362-SPENT warrior
            //     must not hold stock ranks either: the strip is unconditional over the node rank.
            //     The rank list is read straight off the gate row so the two can never drift apart.
            //     READINESS-gated on the same predicate the walker's skip guard uses, so that while
            //     the TBC chain is a display-only seed today's shipped force-grant behavior stands.
            //     VANILLA HALF (final review R7, 2026-09-06): identical in every respect. Node 18552
            //     used to BOUND-GRANT stock 23922 on the (mistaken) ground that the shield-block-value
            //     scaling was script-bound and therefore unclonable — it is not, `Spell::EffectSchoolDMG`
            //     adds it in the core effect handler gated on `SpellFamilyFlags[1] & 0x200 &&
            //     GetCategory() == 1209`, both of which the clones carry. What actually diverges is the
            //     DAMAGE (r1-r4: 225-235/264-276/303-317/342-358 vs stock's 294-308/346-362/396-416/
            //     447-469), at EVERY rank, so this strip is likewise unconditional over the node rank
            //     rather than node-gated. Its readiness predicate is EraNodeReplacesStock rather than
            //     EraNodeIsWired because the thing that must be true is precisely "18552 grants
            //     something OTHER than 23922" — a half-authored state in which the node still granted
            //     the stock spell must never strip the node's own grant (the priest Divine Spirit
            //     shape). A Vanilla warrior who spent the talent before this change keeps a stale
            //     stock rank until the next reconcile, which is exactly what this arm is.
            if ((era == ERA_TBC && EraReplacementSpellReady(ERA_TBC, 947541) &&
                 EraNodeIsWired(ERA_TBC, CLASS_WARRIOR, 20362)) ||
                (era == ERA_VANILLA && EraReplacementSpellReady(ERA_VANILLA, 932841) &&
                 EraNodeReplacesStock(ERA_VANILLA, CLASS_WARRIOR, 18552, 23922)))
            {
                for (const BaselineSpellGate& g : kBaselineSpellGates)
                    if (g.classId == CLASS_WARRIOR && g.talentId == 18552)
                        for (uint32 spell : g.rankSpells)
                            if (spell != 0 && p->HasSpell(spell))
                                p->removeSpell(spell, SPEC_MASK_ALL, false);
            }

            // (6) TBC Rampage PAIRED grant — the crit-WATCHER passive 947539.
            //     TBC Rampage "can only be used after scoring a critical hit". The era
            //     reconstruction reproduces that window in DATA (the ability clones carry
            //     `CasterAuraSpell = 947540`, a 5 s aura), but SOMETHING has to apply that aura on a
            //     crit — a permanently-held passive whose aura-42 proc (hitMask 2 = crit-only) casts
            //     it. That passive is a SECOND spell the node would have to grant, and
            //     era_talent_rank's PRIMARY KEY is (talentId, `rank`) while
            //     EraTalentContent::rankSpell is one uint32 per rank, so a node grants exactly ONE
            //     spell — a two-element `grants:` list fails db-import with a duplicate-key error.
            //     Same constraint, same shape and same reasoning as the druid kEraDruidTbcPaired arm
            //     below; there is only one warrior pair, so it is written out rather than tabled.
            //     WHY NOT COLLAPSE THE TWO INTO ONE SPELL (the shaman Spirit Weapons 946519 route):
            //     the ability is an ACTIVE the player casts and the watcher is a PERMANENT passive.
            //     One spell cannot be both — the ability's own aura exists only for the 30 s after a
            //     cast, which is exactly the window in which the gate is already open.
            //     LEARNED, not cast: a learned passive self-applies on learn and survives logout
            //     without a re-cast, the same mechanism every other module marker uses (932930 /
            //     932416 / 946280). It is therefore visible to `.eratalents doctor`, which has a
            //     matching orphan predicate.
            //     STRIP: none needed here — the band sweep (EraBandClassifier) removes the watcher
            //     whenever the node is unspent or the character leaves TBC.
            //     READINESS-gated like every other TBC arm so the ids not existing is inert.
            if (era == ERA_TBC && EraReplacementSpellReady(ERA_TBC, 947539) &&
                EraNodeIsWired(ERA_TBC, CLASS_WARRIOR, 20343) &&
                CurrentRank(p, ERA_TBC, 20343) != 0 && !p->HasSpell(947539))
                p->learnSpell(947539);

            // (7) Sweeping Strikes — the one warrior era-swap that still needs an arm here, because
            //     its VANILLA side is a STOCK spell: node 18513 grants **STOCK 12328** while TBC node
            //     20335 grants the clone 947549 (10 charges / 10 s vs live's 5 / 30 s). A TBC warrior
            //     must not keep the stock spell its node no longer hands over (workflow lesson 17f's
            //     retired-grant case).
            //     ERA SCOPE IS LOAD-BEARING: 12328 is a STOCK spell and an ordinary native talent
            //     spell for a WotLK warrior, so it may only be stripped inside TBC, and only once the
            //     replacement clone exists and the node is wired — the same strip-without-replacement
            //     guard arms (2)/(3)/(4) use. In Vanilla it is the Vanilla node's own grant.
            //     Death Wish needs NO arm: both sides are CUSTOM band clones (Vanilla 932808 / TBC
            //     947548), and the band sweep (EraBandClassifier) strips a wrong-era or unspent-node
            //     clone on its own.
            if (era == ERA_TBC && EraReplacementSpellReady(ERA_TBC, 947549) &&
                EraNodeIsWired(ERA_TBC, CLASS_WARRIOR, 20335) && p->HasSpell(12328))
                p->removeSpell(12328, SPEC_MASK_ALL, false);

            // (8) STANCE CORRECTIONS — hidden passives 932845 (Defensive) / 932846 (Berserker) /
            //     932847 (Battle), granted to EVERY warrior in BOTH managed eras (final review R7,
            //     2026-09-06, items G-11/G-30). Not a talent, not a swap: WotLK retuned all three
            //     warrior stances and 1.12.1/2.4.3 agree with each other, so the correction is
            //     era-wide rather than node-gated, and ONE set of ids serves both eras (authored in
            //     era-data/vanilla/warrior.yaml; the paladin Lay on Hands 932668-670 dual-era-reuse
            //     precedent, recorded in era-data/band-allowlist.yaml with eras [vanilla, tbc]).
            //     WHY A SEPARATE PASSIVE INSTEAD OF A CLONE: the stance passives 7376/7381/21156 are
            //     cast BY THE CORE from `AuraEffect::HandleShapeshiftBoosts` off hardcoded ids, so
            //     they cannot be swapped for an era clone the way a talent grant can. Each of these
            //     three is instead an ordinary hidden LEARNED passive carrying a `ShapeshiftMask`,
            //     which the same core function applies on stance ENTRY (its m_spells loop takes any
            //     SPELL_ATTR0_DO_NOT_DISPLAY spell whose Stances bit matches) and
            //     `Aura::IsRemovedOnShapeLost` drops on stance EXIT — the shipped Defiance
            //     924352-356 mechanism (`stances: 131072`), real-client-verified 2026-08-24. Their
            //     amounts are authored as MULTIPLIERS onto the stock values, because the core
            //     accumulates MOD_DAMAGE_PERCENT_DONE/TAKEN and MOD_THREAT multiplicatively; the
            //     per-stance numbers and the sub-0.5pp rounding residual are documented on the
            //     helpers themselves.
            //     LEARNED, not cast, for the same reason as the Rampage watcher (6): a learned
            //     passive survives logout and is visible to `.eratalents doctor`.
            //     NO STRIP ARM: a WotLK warrior's copies are removed by the band sweep
            //     (EraBandClassifier) — the allowlist entries name only vanilla/tbc, so the sweep
            //     classifies them ORPHAN_ALLOWLIST there and strips them. Never add a
            //     removeSpell(<band id>) here.
            //     READINESS-gated per id, so a not-yet-generated row (or an era outside the
            //     EraHasTalentTrees allowlist) is simply inert.
            for (uint32 stanceFix : { 932845u, 932846u, 932847u })
                if ((era == ERA_VANILLA || era == ERA_TBC) &&
                    EraReplacementSpellReady(era, stanceFix) && !p->HasSpell(stanceFix))
                    p->learnSpell(stanceFix);
        }

        // ============================ MAGE (Vanilla + TBC Phase 9) ============================
        // Arcane Power / Presence of Mind grant changes. Vanilla node 18016 grants the era clone
        // 932760 (+30% damage / +30% mana cost, 3-min cd) instead of stock 12042 (WotLK +20%/+20%,
        // 2-min cd), and TBC node 20819 grants its own clone 948760 — Wowhead TBC 12042 is ALSO
        // +30%/+30% on a 3-min cd (2026-09-05), so TBC diverges from stock exactly as Vanilla does.
        // Likewise Presence of Mind: 18013 -> 932761, 20813 -> 948761, vs stock 12043 (WotLK 2-min cd;
        // both earlier eras 3-min). 12042/12043 are talent-granted only (never trainer-taught), so
        // neither a Vanilla nor a TBC mage ever legitimately holds them; ReapplyOnLogin re-grants the
        // clone from the node. Era-gated so WotLK mages keep stock (their tree grants 12042/12043).
        // Same grant-change strip as the warrior Bloodthirst 23881 / Last Stand 12975 / rogue
        // Preparation 14185 / priest VE-PI rebuild.
        //
        // The TBC arm is READINESS-GATED (workflow lesson 25: every arm of a table takes the same
        // fallback, and the not-ready truth table must read byte-for-byte pre-phase). With TBC outside
        // the EraHasTalentTrees allowlist — the committed state — tbcMageReady is false, every TBC arm
        // below is inert, and a TBC mage keeps stock everything while playing the NATIVE WotLK tree.
        // Vanilla passes in both states, so the Vanilla behaviour is today's plus the Phase-9 FIX arms.
        if (p->getClass() == CLASS_MAGE)
        {
            bool const tbcMageReady = (era == ERA_TBC && EraReplacementSpellReady(ERA_TBC, 948760)
                                       && EraNodeIsWired(ERA_TBC, CLASS_MAGE, 20819));
            if ((era == ERA_VANILLA || tbcMageReady) && p->HasSpell(12042))
                p->removeSpell(12042, SPEC_MASK_ALL, false);
            if ((era == ERA_VANILLA || tbcMageReady) && p->HasSpell(12043))
                p->removeSpell(12043, SPEC_MASK_ALL, false);

            // ---- TBC stock swaps (lesson 17f). A TBC node that hands over a CLONE must not leave the
            // stock r1 (or its trained ranks) behind. Readiness-gated per row via EraNodeReplacesStock
            // (the hunter/rogue/priest shape): dormant while the node is a display seed or grants stock,
            // so a half-authored tree never strips a spell with nothing to replace it. The stock trainer
            // rows are NOT deleted — native WotLK mages train the whole chain through the stock r1,
            // which a TBC mage no longer holds. The rank lists include the WotLK-only ranks so a
            // character that band-moved DOWN into TBC sheds them too.
            struct MageStockSwap { uint32 node; uint32 stockR1; uint32 ranks[11]; };   // 0 = unused slot
            static const MageStockSwap kEraMagTbcStockSwaps[] =
            {
                { 20830, 11366, { 12505, 12522, 12523, 12524, 12525, 12526, 18809, 27132, 33938, 42890, 42891 } },   // Pyroblast
                { 20837, 11113, { 13018, 13019, 13020, 13021, 27133, 33933, 42944, 42945, 0, 0, 0 } },              // Blast Wave
                { 20844, 31661, { 33041, 33042, 33043, 42949, 42950, 0, 0, 0, 0, 0, 0 } },                          // Dragon's Breath
                { 20863, 11426, { 13031, 13032, 13033, 27134, 33405, 43038, 43039, 0, 0, 0, 0 } },                  // Ice Barrier
                { 20841, 11129, { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },                                              // Combustion -> 948887
                { 20822, 31589, { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },                                              // Slow -> 948773
            };
            if (era == ERA_TBC)
                for (const MageStockSwap& sw : kEraMagTbcStockSwaps)
                {
                    if (!EraNodeReplacesStock(ERA_TBC, CLASS_MAGE, sw.node, sw.stockR1))
                        continue;
                    if (p->HasSpell(sw.stockR1))
                        p->removeSpell(sw.stockR1, SPEC_MASK_ALL, false);
                    for (uint32 spell : sw.ranks)
                        if (spell != 0 && p->HasSpell(spell))
                            p->removeSpell(spell, SPEC_MASK_ALL, false);
                }

            // ---- Vanilla stock swaps (same lesson 17f rule as the TBC table). Combustion 18032 and
            // Cold Snap 18041 have no trained ranks — the clones 932325 / 932335 are single ranks —
            // so those rows list only the stock r1.
            static const MageStockSwap kEraMagVanillaStockSwaps[] =
            {
                { 18024, 11366, { 12505, 12522, 12523, 12524, 12525, 12526, 18809, 27132, 33938, 42890, 42891 } },   // Pyroblast
                { 18030, 11113, { 13018, 13019, 13020, 13021, 27133, 33933, 42944, 42945, 0, 0, 0 } },              // Blast Wave
                { 18049, 11426, { 13031, 13032, 13033, 27134, 33405, 43038, 43039, 0, 0, 0, 0 } },                  // Ice Barrier
                { 18032, 11129, { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },                                              // Combustion -> 932325
                { 18041, 11958, { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },                                              // Cold Snap -> 932335 (final review 2026-09-06)
            };
            if (era == ERA_VANILLA)
                for (const MageStockSwap& sw : kEraMagVanillaStockSwaps)
                {
                    if (!EraNodeReplacesStock(ERA_VANILLA, CLASS_MAGE, sw.node, sw.stockR1))
                        continue;
                    if (p->HasSpell(sw.stockR1))
                        p->removeSpell(sw.stockR1, SPEC_MASK_ALL, false);
                    for (uint32 spell : sw.ranks)
                        if (spell != 0 && p->HasSpell(spell))
                            p->removeSpell(spell, SPEC_MASK_ALL, false);
                }
        }

        // Vanilla seals/Judgement/LoH are TRAINER-TAUGHT (2026-08-20 rework). Reconcile grants ONLY the hidden
        // marker 932749 (the trainer ReqAbility1 anchor); a Vanilla paladin then TRAINS every seal/Judgement/LoH
        // rank. Stock WotLK seals/judgement-buttons/LoH are version-swapped: their trainer rows are deleted
        // (2026_08_20_40 SQL) and they are stripped here in Vanilla / auto-granted by level in higher eras.
        //
        // This REPLACES the original 2026-08-20 seal reconstruction's grant-by-level model (Tasks 1-9): a
        // real-client test found it auto-granted every custom rank while leaving the stock WotLK seals/
        // judgement-buttons/LoH still trainable, cluttering the trainer/spellbook with both sets at once.
        // Seal of the Crusader (932634-639, formerly grant-by-level via kSotcSealRanks) is folded into the
        // same trainer-taught model — it has no talent gate either way, so this is a pure obtain-method swap.
        if (p->getClass() == CLASS_PALADIN)
        {
            static const uint32 kStockPalSwap[] = {   // stock ids stripped in Vanilla (kept/auto-granted in WotLK)
                20154,21084,20164,20165,20166,   // seals (20154 creation-granted; 21084 taught by wrapper 10321)
                20271,53407,53408,         // judgement buttons (20271 + 21084 both come from the stock
                                           // "Judgement" trainer LEARN-WRAPPER 10321 — its rows are deleted in
                                           // 2026_08_25_00 SQL; stripping here self-heals a Vanilla paladin
                                           // who bought it before the fix)
                633,2800,10310,27154,      // Lay on Hands
                31801,53736,               // stock WotLK Seal of Vengeance (Alliance) / Seal of
                                           // Corruption (Horde): their ungated trainer rows are
                                           // deleted (2026_08_30_00 — a TBC paladin could train the
                                           // WotLK-reworked seal next to the era clone 946084/946080);
                                           // stripped in Vanilla AND TBC, faction-granted by level in
                                           // the WotLK arm below
            };
            // TBC seals are FRESH 946xxx clones at wago-2.5.4.44833 values (governing decision 2026-08-27:
            // wago 2.5.4 end-to-end incl. the TBC-Classic ~2x seal rebalance — SotC AP ~doubled, JoR/JoL/JoW
            // retuned — so they are FRESH clones, NOT the reused Vanilla 932xxx clones). Authored in
            // era-data/tbc/paladin.yaml helpers:; per-rank learn levels + mana come from wago SpellLevels/
            // SpellPower. The seal-MODIFYING talents (Improved SoR/SotC, Benediction, Sanctified Seals/
            // Judgement, ...) bind this 946xxx substrate (maskC 1024 SoR / 512 SotC / bit23 Judgement),
            // never stock WotLK seals.
            //
            // TASK 5c (2026-08-27): the 946xxx SEALS are now TRAINER-TAUGHT, mirroring the Vanilla model.
            // 5b's interim auto-granted every seal rank BY LEVEL; the user wants trainer-learnable instead.
            // So the TBC arm now grants ONLY the hidden Era:TBC-Paladin marker 946078 (kTbcPalSealMarker
            // above) — every seal/Judgement (SoR/SoJ/SoL/SoW/SotC + SoC r2-6) rank is a marker-anchored
            // trainer_spell chain in 2026_08_27_02, so a TBC paladin/bot TRAINS them (bots via the factory
            // trainer re-walk that follows InitTalentsTree). STILL grant-by-level (kept as-is, not seals /
            // not rebalanced): the single Judgement button 932746 + Lay on Hands 932668-670 — the reused
            // non-seal actives below. Seal of Command r1 (946027) is granted by its talent node 20049 (a
            // later task); its r2-6 (946028-032) chain off the talent-granted r1 in the trainer SQL, not
            // the marker (mirrors Vanilla SoC 932606 + 932750-753).
            struct TbcSealGrant { uint32 spell; uint8 level; };
            static const TbcSealGrant kTbcPalReusedGrants[] = {
                { 932746,  4 },                                                  // the single Judgement (REUSED)
                { 932668, 10 }, { 932669, 30 }, { 932670, 50 },                  // Lay on Hands r1-3 (REUSED)
            };
            // TBC faction DPS seals (Task 5d): SoB (946080, Horde/BE) is trained ONLY at the Horde
            // paladin trainers (TrainerId 4) and SoV (946084, Alliance) ONLY at the Alliance trainers
            // (TrainerId 3) — so a HUMAN sees only the faction-appropriate seal (trainer reachability).
            // BOTS bypass physical trainers (the factory trainer-walk is class-only-gated) so a bot
            // learns BOTH; StripWrongFactionTbcPalSeal removes the wrong one (below + every bot login).
            // The learned SEAL is the only character_spell row; the JoB/JoV/DoT/self carriers
            // (946081-083/946085-086) are script-cast, never learned. The band sweep
            // (EraBandClassifier) drops both on any move out of TBC.
            // Era: TBC Paladin seal-training marker (hidden passive, spell_dbc in 2026_08_27_02). The
            // TBC analog of the Vanilla 932749 marker: reconcile-granted to every TBC paladin, it is the
            // ReqAbility1 anchor of every 946xxx seal/Judgement trainer_spell chain (2026_08_27_02), so a
            // non-TBC paladin (no marker) can never train them. Stripped on any move out of TBC.
            static const uint32 kTbcPalSealMarker = 946078;
            if (era == ERA_VANILLA)
            {
                if (!p->HasSpell(932749))
                    p->learnSpell(932749);                 // marker -> unlocks seal/Judgement/LoH training
                for (uint32 s : kStockPalSwap)
                    if (p->HasSpell(s))
                        p->removeSpell(s, SPEC_MASK_ALL, false);
            }
            else if (era == ERA_TBC)
            {
                // Grant the TBC seal-training marker (the ReqAbility1 anchor of every 946xxx seal/
                // Judgement trainer chain — a TBC paladin then TRAINS the seals, Task 5c) and strip
                // every stock WotLK seal/judgement-button/LoH (they conflict with the reconstructed
                // substrate). The Vanilla-only marker 932749 and the Vanilla 932xxx seal clones the
                // 946xxx set supersedes are band ids the band sweep (EraBandClassifier) removes. The
                // 946xxx seals are NO LONGER force-granted here — only the reused non-seal actives
                // (Judgement + LoH) are grant-by-level.
                if (!p->HasSpell(kTbcPalSealMarker))
                    p->learnSpell(kTbcPalSealMarker);      // marker -> trainers offer the 946xxx seals
                for (uint32 s : kStockPalSwap)
                    if (p->HasSpell(s))
                        p->removeSpell(s, SPEC_MASK_ALL, false);
                for (const TbcSealGrant& g : kTbcPalReusedGrants)
                    if (p->GetLevel() >= g.level && !p->HasSpell(g.spell))
                        p->learnSpell(g.spell);

                // Faction gate (Task 5d): strip the WRONG-faction DPS seal. Shared with the bot login
                // path (StripWrongFactionTbcPalSeal) — this mid-build call is a no-op for a bot (it
                // hasn't reached the factory trainer-walk yet), but covers the human reset/login path
                // here; the strip re-runs on every bot login to catch the walk's cross-faction teach.
                StripWrongFactionTbcPalSeal(p);

                // Blessing of Kings stays a TALENT in TBC (node 20025), NOT baseline as in WotLK — the
                // ONLY real trainer leak among the TBC paladin nodes (_ref baseline_leaks; its ungated
                // trainer rows were already globally deleted by the Vanilla gate). Grant BoK / Greater BoK
                // on the node, strip otherwise. The generic kBaselineSpellGates below is skipped for these
                // in TBC (the g.talentId==18620 guard) so its WotLK auto-grant can't leak BoK as baseline.
                {
                    bool bokNode = CurrentRank(p, ERA_TBC, 20025) != 0;
                    static const TbcSealGrant kTbcBoK[] = { { 20217, 20 }, { 25898, 60 } };
                    for (const TbcSealGrant& b : kTbcBoK)
                    {
                        if (bokNode && p->GetLevel() >= b.level && !p->HasSpell(b.spell))
                            p->learnSpell(b.spell);
                        else if (!bokNode && p->HasSpell(b.spell))
                            p->removeSpell(b.spell, SPEC_MASK_ALL, false);
                    }
                }
            }
            else
            {
                // Stock seals/judgements/LoH: trainer rows were deleted, so a WotLK paladin can no longer TRAIN
                // them; auto-grant by level instead (the Holy-Nova/Shield-Slam grantHigherEra precedent). 20154/
                // 20271 are core-auto-learned but re-granted here so a Vanilla->WotLK transition restores them
                // immediately. Levels are the real stock trainer ReqLevel / Spell.dbc SpellLevel (confirmed via
                // azerothcore-wotlk/ip-dbc/Spell.dbc + data/sql/base/db_world/trainer_spell.sql for every id).
                struct StockGrant { uint8 level; uint32 spell; };
                static const StockGrant kWotlkPalStock[] = {
                    { 3, 20154 }, { 4, 21084 }, { 22, 20164 }, { 30, 20165 }, { 38, 20166 },
                    { 4, 20271 }, { 28, 53407 }, { 12, 53408 },
                    { 10, 633 }, { 30, 2800 }, { 50, 10310 }, { 69, 27154 },
                };
                for (const StockGrant& r : kWotlkPalStock)
                    if (p->GetLevel() >= r.level && !p->HasSpell(r.spell))
                        p->learnSpell(r.spell);

                // Stock WotLK Seal of Vengeance (31801, Alliance) / Seal of Corruption (53736,
                // Horde): their ungated trainer rows are deleted (2026_08_30_00), so a WotLK
                // paladin gets the faction-appropriate seal by level instead (stock trainer
                // ReqLevel 64); the wrong-faction one is stripped for hygiene.
                {
                    bool horde = p->GetTeamId() == TEAM_HORDE;
                    uint32 sealGrant = horde ? 53736u : 31801u;
                    uint32 sealOther = horde ? 31801u : 53736u;
                    if (p->GetLevel() >= 64 && !p->HasSpell(sealGrant))
                        p->learnSpell(sealGrant);
                    if (p->HasSpell(sealOther))
                        p->removeSpell(sealOther, SPEC_MASK_ALL, false);
                }
            }

            // Holy Shock grant change (Fix 3): node 18614 now grants the era clone 932646 (dual
            // heal/damage at Vanilla magnitudes) instead of stock 20473 (WotLK ~314-340). A paladin
            // who learned Holy Shock before this change keeps stock 20473 alongside the clone —
            // strip it. Era-gated: 20473 is a legit WotLK talent spell, so strip only in Vanilla.
            // ReapplyOnLogin re-grants the clone from the node. (20473 is a STOCK id and no longer in
            // the paladin grant manifest, so no band-sweep rule can ever reach it.)
            //
            // The trained-rank CHAINS off Holy Shock / Holy Shield / Blessing of Sanctuary / Seal of
            // Command / Avenger's Shield need no arm here any more: every rank is a band id rooted at
            // a node-granted r1, so the band sweep (EraBandClassifier) keeps exactly the ranks whose
            // chain root the character's current era still grants and strips the rest.
            if (era == ERA_VANILLA && p->HasSpell(20473))
                p->removeSpell(20473, SPEC_MASK_ALL, false);

            // ---- Stock swaps for the two nodes the 2026-09-06 final review re-pointed onto clones
            // (lesson 17f: a node that hands over a CLONE must not leave the stock spell behind).
            // Both nodes ship the SAME swap in BOTH managed eras, so one table carries an era key
            // rather than duplicating the mage/rogue two-table shape:
            //   * ILLUMINATION — the refund % lives in the passive's OWN Effect_2 and the core
            //     AuraScript spell_pal_illumination reads it, so live 20210/20212-20215 deliver
            //     WotLK's 30% while both era tooltips promise 60% (TBC) / 100% (Vanilla). The nodes
            //     now grant clones (TBC 946141-946145, Vanilla 932678-932682) carrying
            //     EffectBasePoints_2 59 / 99 and the same core script binding.
            //   * REPENTANCE — live 20066 is WotLK's widened form (60 s, TargetCreatureType 118);
            //     both eras are 6 s / Humanoid-only, now clones 946146 (TBC) / 932683 (Vanilla).
            // Stock 20210-20215 and 20066 are TALENT-GRANTED ONLY (no trainer_spell rows of their
            // own), so an era-managed paladin can hold them only from the NATIVE WotLK talent frame
            // — exactly the rogue/mage swap-table case. READINESS-GATED per row via
            // EraNodeReplacesStock: while a node is a display seed or still grants stock, the row is
            // dormant and can never strip without replacement. WotLK is untouched (its own tree
            // grants the stock ids).
            struct PalStockSwap { EraId era; uint32 node; uint32 stockR1; uint32 ranks[4]; };
            static const PalStockSwap kEraPalStockSwaps[] =
            {
                { ERA_TBC,     20008, 20210, { 20212, 20213, 20214, 20215 } },   // Illumination -> 946141-946145
                { ERA_TBC,     20060, 20066, { 0, 0, 0, 0 } },                   // Repentance   -> 946146
                { ERA_VANILLA, 18609, 20210, { 20212, 20213, 20214, 20215 } },   // Illumination -> 932678-932682
                { ERA_VANILLA, 18644, 20066, { 0, 0, 0, 0 } },                   // Repentance   -> 932683
            };
            for (const PalStockSwap& sw : kEraPalStockSwaps)
            {
                if (era != sw.era)
                    continue;
                if (!EraNodeReplacesStock(sw.era, CLASS_PALADIN, sw.node, sw.stockR1))
                    continue;
                if (p->HasSpell(sw.stockR1))
                    p->removeSpell(sw.stockR1, SPEC_MASK_ALL, false);
                for (uint32 spell : sw.ranks)
                    if (spell != 0 && p->HasSpell(spell))
                        p->removeSpell(spell, SPEC_MASK_ALL, false);
            }
        }

        // Druid fix round (2026-08-21). Mirrors the paladin CLASS block: NG stock version-swap
        // (issue 1), Enrage version-swap (issue 7), stale-Omen strip (issue 3). The NG trained-chain
        // strip that used to open this block is gone — both clone chains are band ids rooted at a node
        // grant, so the band sweep (EraBandClassifier) owns them; the arms below therefore start at (2).
        // (Faerie Fire (Feral) issue 2 is handled by the kBaselineSpellGates row below — a standard
        // grantHigherEra gate, not a custom-clone swap — PLUS arm (5) here, which supplies the TBC
        // half of that gate (it is still a TALENT in TBC, node 20133).)
        // TBC Phase 2 (2026-08-30): these arms are THREE-way, not two-way. Only Enrage (932513) is
        // shared substrate between Vanilla and TBC; Nature's Grasp and Omen of Clarity get their own
        // fresh TBC clones because the value check failed (spec Amendment A.3/A.5: TBC NG is FREE to
        // cast, TBC Omen is a 30-min melee-only buff). So each era keeps its own chain and strips the
        // other era's — that asymmetry is intentional, not a copy/paste bug.
        if (p->getClass() == CLASS_DRUID)
        {
            // (2) Stock Nature's Grasp chain version-swap. The stock NG trainer rows are DELETED globally
            //     (2026_08_21_00 SQL), so:
            //       * Vanilla AND TBC: strip any stock NG rank the char has (leak cleanup +
            //         era-transition net). BOTH managed eras replace the stock chain with a custom clone
            //         chain (Vanilla 932505/932507-511, TBC 946270-275 — Amendment A.3/A.5), so both must
            //         strip the stock ranks; only the clone chain the band sweep spares differs. The TBC half
            //         is READINESS-GATED on its rank-1 clone existing: the TBC chain is authored later in
            //         the phase, and stripping the stock chain before then would leave a TBC druid with
            //         no Nature's Grasp at all. Until 946270 exists a TBC druid falls through to the
            //         WotLK level-grant below, i.e. keeps working stock NG.
            //       * WotLK: auto-grant stock NG by level (the trainer no longer offers it — the
            //         grantHigherEra precedent). Documented tradeoff: NG becomes effectively baseline in
            //         WotLK (learned at level rather than chosen at the trainer), consistent with how this
            //         server already exposed it (ungated trainer row) and the other 7 classes' precedent.
            {
                static const uint32 kStockDruidNG[] =
                    { 16689, 16810, 16811, 16812, 16813, 17329, 27009, 53312 };
                if (era == ERA_VANILLA ||
                    (era == ERA_TBC && EraReplacementSpellReady(ERA_TBC, 946270)
                                    && EraNodeIsWired(ERA_TBC, CLASS_DRUID, 20101)))
                {
                    for (uint32 s : kStockDruidNG)
                        if (p->HasSpell(s))
                            p->removeSpell(s, SPEC_MASK_ALL, false);
                }
                else
                {
                    struct StockGrant { uint8 level; uint32 spell; };
                    static const StockGrant kWotlkDruidNG[] = {
                        { 10, 16689 }, { 18, 16810 }, { 28, 16811 }, { 38, 16812 },
                        { 48, 16813 }, { 58, 17329 }, { 68, 27009 }, { 78, 53312 },
                    };
                    for (const StockGrant& r : kWotlkDruidNG)
                        if (p->GetLevel() >= r.level && !p->HasSpell(r.spell))
                            p->learnSpell(r.spell);
                }
            }

            // (3) Enrage version-swap (no talent gate — pure obtain swap like paladin Lay on Hands). The
            //     stock 5229 trainer row is DELETED globally (2026_08_21_00 SQL). Custom 932513 = the
            //     Vanilla Enrage (20 rage over 10s, no built-in instant chunk); stock 5229 = the WotLK
            //     Enrage (10 over time + 20 instant). Both are baseline (L12), so grant/strip by level:
            //       * Vanilla AND TBC: grant 932513 at L12, strip stock 5229. This is the ONE druid row
            //         where TBC genuinely REUSES the Vanilla clone: Task 3's survival map found the TBC
            //         2.4.3 Enrage byte-identical to the Vanilla one this clone reconstructs (Amendment
            //         A.3's reuse ledger; the armor-drawback omission stays an accepted gap, spec §6.3).
            //       * WotLK: grant stock 5229 at L12 (trainer row deleted). The custom 932513 is not
            //         stripped here — the generic band sweep below owns that strip now (Task 5).
            if (era == ERA_VANILLA || era == ERA_TBC)
            {
                if (p->GetLevel() >= 12 && !p->HasSpell(932513))
                    p->learnSpell(932513);
                if (p->HasSpell(5229))
                    p->removeSpell(5229, SPEC_MASK_ALL, false);
            }
            else
            {
                if (p->GetLevel() >= 12 && !p->HasSpell(5229))
                    p->learnSpell(5229);
            }

            // (4) Omen of Clarity grant change: node 18708 grants the custom castable clone 932512
            //     (Vanilla 10-min activated buff) instead of stock 16864 (WotLK hidden passive). A druid
            //     who learned Omen before this change keeps stock 16864 alongside the clone — strip it.
            //     16864 is a hidden passive the core can auto-apply; ReapplyOnLogin re-grants the clone
            //     from the node.
            //     THREE-way (Amendment A.3/A.5): BOTH managed eras replace stock 16864 with a castable
            //     clone, so both strip the stock spell — but they use DIFFERENT clones, because TBC Omen
            //     is a 30-minute melee-only buff (ProcTypeMask 20, 10s ICD) where the Vanilla clone is a
            //     10-minute 81924-mask buff. TBC node 20149 grants its own clone 946269. The wrong-era
            //     CLONE (932512 / 946269) needs no arm here — the band sweep (EraBandClassifier) strips
            //     it — so only the STOCK 16864 strip lives here. Its TBC half is READINESS-GATED on
            //     946269 existing (it is authored later in the phase): stripping 16864 before its
            //     replacement exists would leave a TBC druid with no Omen of Clarity at all.
            if ((era == ERA_VANILLA ||
                 (era == ERA_TBC && EraReplacementSpellReady(ERA_TBC, 946269)
                                 && EraNodeIsWired(ERA_TBC, CLASS_DRUID, 20149))) && p->HasSpell(16864))
                p->removeSpell(16864, SPEC_MASK_ALL, false);

            // (5) Faerie Fire (Feral) (stock 16857) — the TBC half of the kBaselineSpellGates row keyed
            //     on Vanilla node 18729. FF(Feral) is a TALENT in BOTH managed eras (Vanilla 18729, TBC
            //     20133) and only becomes baseline in WotLK, but its ungated druid-trainer row is
            //     globally DELETED (2026_08_21_00 SQL), so the generic gate walker's era >= ERA_TBC arm
            //     would force-grant it at L18 to a TBC druid — leaking it as baseline one era early.
            //     When TBC node 20133 IS wired, the walker SKIPS that row for a TBC druid (its
            //     g.talentId == 18729 guard, which uses this identical predicate) and this arm supplies
            //     the strip, mirroring the walker's Vanilla arm but keyed on the TBC node.
            //     READINESS: while node 20133 is still a display-only seed (empty rankSpell), CurrentRank
            //     can never be non-zero, so an ungated strip here would remove FF(Feral) from every TBC
            //     druid permanently. Both this arm and the walker guard therefore require the node to be
            //     wired; until then the walker's force-grant runs unchanged and FF(Feral) keeps working.
            //     This is STRIP-ONLY, unlike the paladin Blessing of Kings TBC arm it is otherwise
            //     modelled on: BoK is grant+strip and so self-heals on any reconcile, whereas the grant
            //     side here is left to the node's own rank grant + ReapplyOnLogin (exactly as the Vanilla
            //     arm does). The weaker guarantee: a 20133-spent druid who loses 16857 mid-session is not
            //     repaired until the next login. That matches the shipped Vanilla behavior on purpose —
            //     do not "fix" one era's shape without the other.
            if (era == ERA_TBC && EraNodeIsWired(ERA_TBC, CLASS_DRUID, 20133) &&
                CurrentRank(p, ERA_TBC, 20133) == 0 && p->HasSpell(16857))
                p->removeSpell(16857, SPEC_MASK_ALL, false);

            // (6) TBC Feral PAIRED grants — the "second spell" of a node that hands out one ability
            //     PER SHAPESHIFT FORM. Two TBC Feral nodes need this because TBC merged what Vanilla
            //     split across two separate nodes:
            //       * 20131 Primal Fury — the node grants the BEAR half (16958 r1 / 16961 r2,
            //         ShapeshiftMask 144, aura-42 -> 16959 = +5 rage on a bear crit); this arm adds
            //         the CAT half (16952 / 16954 "Blood Frenzy", ShapeshiftMask 1, aura-42 ->
            //         16953 = +1 combo point). Vanilla splits these across nodes 18727 and 18726.
            //       * 20141 Mangle — the node grants Mangle (Bear) r1 946258; this arm adds
            //         Mangle (Cat) r1 946261.
            //     WHY NOT JUST GRANT TWO SPELLS FROM THE NODE: era_talent_rank's PRIMARY KEY is
            //     (talentId, rank) and EraTalentContent::rankSpell is one uint32 per rank, so a node
            //     grants exactly ONE spell per rank — a two-element `grants:` list fails db-import
            //     with a duplicate-key error. Nor can the pair be collapsed into one custom spell:
            //     one spell carries one ShapeshiftMask, and an aura-42 PROC_TRIGGER_SPELL is cast
            //     TRIGGERED_FULL_MASK (TRIGGERED_IGNORE_SHAPESHIFT), so a single mask-145 Primal Fury
            //     carrier would give bear crits combo points and cat crits rage. A
            //     SPELL_EFFECT_LEARN_SPELL wrapper is out too — Player::addSpell REFUSES any spell
            //     carrying that effect (Player.cpp:3310); only the NATIVE talent path
            //     (_addTalentAurasAndSpells) can process one, and era characters never use it.
            //     SHAPE: rank-exact. Grant the rank the character actually holds, strip every OTHER
            //     rank of the pair, and strip them all when the node is unspent — so respec, rank
            //     changes and era transitions all converge from every reconcile site (login / level /
            //     era transition / TryLearn / Reset). removeSpell cascades via GetNextSpellInChain,
            //     so stripping 946261 also strips the trainer-taught 946262/946263 (mirroring how
            //     Reset()'s removeSpell(946258) cascades the Bear line).
            //     ERA SCOPE IS LOAD-BEARING: the STOCK ids 16952/16954 are also granted by the
            //     VANILLA node 18726 and are ordinary native talent spells for a WotLK druid, so this
            //     arm must never touch them outside TBC. Only the CUSTOM clone 946261 (which no other
            //     era can legitimately hold) is stripped era-wide.
            {
                struct EraDruidPairedGrant { uint32 nodeId; uint8 maxRank; uint32 ranks[2]; bool custom; };
                //       * 20161 Tree of Life — the node grants the STOCK form 33891; this arm adds
                //         the HIDDEN form-passive 946265 (PASSIVE|DO_NOT_DISPLAY, ShapeshiftMask 2 =
                //         FORM_TREE) whose TRIGGER_SPELL is the TBC party aura 946266. The split is
                //         the same one Leader of the Pack needs (HandleShapeshiftBoosts only
                //         re-casts a PASSIVE|DO_NOT_DISPLAY spell whose Stances include the form),
                //         and 946265 doubles as the era gate era_dru_tree_of_life_suppress reads to
                //         suppress the core's hardcast WotLK aura 34123 — so it must be present for
                //         exactly the era druids that have the talent, which is what this arm does.
                static const EraDruidPairedGrant kEraDruidTbcPaired[] = {
                    { 20131, 2, { 16952, 16954 }, false },   // Blood Frenzy (cat combo point) r1/r2
                    { 20141, 1, { 946261, 0 },    true  },   // Mangle (Cat) r1 (clone)
                    { 20161, 1, { 946265, 0 },    true  },   // Tree of Life hidden form-passive (clone)
                };
                for (const EraDruidPairedGrant& g : kEraDruidTbcPaired)
                {
                    bool tbcManaged = (era == ERA_TBC && EraNodeIsWired(ERA_TBC, CLASS_DRUID, g.nodeId));
                    uint8 have = tbcManaged ? CurrentRank(p, ERA_TBC, g.nodeId) : 0;
                    for (uint8 r = 1; r <= g.maxRank; ++r)
                    {
                        uint32 s = g.ranks[r - 1];
                        if (!s)
                            continue;
                        if (tbcManaged && r == have)
                        {
                            if (!p->HasSpell(s))
                                p->learnSpell(s);
                        }
                        // Outside TBC the STOCK ids must never be touched (see ERA SCOPE above), and
                        // the CUSTOM clones need no strip at all: they are era-allowlisted band ids the
                        // band sweep (EraBandClassifier) removes once the node is unspent or the
                        // character leaves TBC.
                        else if (tbcManaged && !g.custom && p->HasSpell(s))
                            p->removeSpell(s, SPEC_MASK_ALL, false);
                    }
                }
            }

            // (7) Tree of Life FORM version-swap (fix round 2026-08-31, real-client defect 3).
            //     TBC node 20161 now grants the CLONE 946276 instead of stock 33891 — the mechanics
            //     are byte-identical and the swap exists purely so the client tooltip stops rendering
            //     stock 33891's hardcoded "$34123s1%" / "$34123a1 yards" (which the client resolves
            //     against the WotLK 34123 row: +6% to party AND raid within 100 yd) over a mechanic
            //     that actually ships TBC's 25%-of-Spirit party aura. Stock 33891 is therefore the
            //     OTHER era's version for a managed-era druid and must be stripped:
            //       * MIGRATION is the real case — every druid the PREVIOUS build granted 33891 to
            //         still holds it, and nothing else would remove it: 33891 is a STOCK id, so the
            //         band sweep (EraBandClassifier) never looks at it, and it is in no era node's
            //         rankSpell list any more either. Left alone the
            //         druid would carry two Tree of Life forms, one with the wrong tooltip.
            //       * STRIP-ONLY, and deliberately NOT gated on node 20161's rank: the clone is the
            //         only legitimate form in a managed era at any rank, so stock 33891 is wrong
            //         whether or not the talent is spent.
            //       * era < ERA_WOTLK is load-bearing. 33891 is an ordinary native talent spell for a
            //         WotLK druid, so this must never reach that era. (Vanilla is included as
            //         belt-and-braces: no Vanilla node grants it, but a WotLK->Vanilla transition that
            //         somehow left it behind should not keep a castable WotLK-talent form.)
            if (era < ERA_WOTLK && p->HasSpell(33891))
                p->removeSpell(33891, SPEC_MASK_ALL, false);

            // (8) Era: TBC Heart of the Wild marker (946280, DUMMY misc 18) — arms core patch 0023.
            //     TBC's Heart of the Wild (node 20135) gives Bear/Dire Bear Stamina equal to the FULL
            //     Intellect %, but the core halves it (AuraEffect::HandleShapeshiftBoosts,
            //     `HotWMod = amount / 2`) because WotLK retuned it — a core-computed magnitude the era
            //     spell-diff pipeline cannot see, so the node shipped promising 4/8/12/16/20% while
            //     delivering 2/4/6/8/10%. Patch 0023 skips the halving for the BEAR spell (24899) when
            //     the caster has this marker. Not fixable in data: the bonus derives from the stock
            //     global Intellect aura, and a stance-gated top-up cannot hit the numbers because
            //     MOD_TOTAL_STAT_PERCENTAGE auras combine MULTIPLICATIVELY (10% + 9% = 19.9%, not 20%).
            //     Node-gated on 20135's rank (unlike arm (7)'s strip-only): a druid who has not spent
            //     the talent has no Heart of the Wild aura for the core to scan, so the marker would be
            //     inert anyway — gating keeps it honest and makes a respec drop it. Cat is untouched;
            //     TBC cat attack power really is the halved value, and Vanilla's own node 18730 tooltip
            //     states the halved Stamina too, so this must never arm outside TBC.
            {
                bool wantHotW = (era == ERA_TBC && EraNodeIsWired(ERA_TBC, CLASS_DRUID, 20135)
                                 && CurrentRank(p, ERA_TBC, 20135) != 0);
                if (wantHotW && !p->HasSpell(946280))
                    p->learnSpell(946280);
            }

            // (9) Insect Swarm stock-swap (final review deferred-gaps round 2026-09-07, G-51). Both
            //     managed eras now grant a CLONE r1 (Vanilla 932514 via node 18738, TBC 946281 via node
            //     20107) with trainer-taught clone ranks above it, so an existing character's stock
            //     5570 + trained 24974-24977/27013/48468 must go (lesson 17f: a node that hands over a
            //     clone must not leave the stock spell behind). READINESS-GATED per era via
            //     EraNodeReplacesStock — dormant while the node still grants stock 5570, so it can never
            //     strip without replacement. No trainer rows are deleted: the stock chain self-gates on
            //     5570, which a managed-era druid no longer holds. WotLK is untouched (its native tree
            //     grants 5570 and its trainer sells the ranks). The clone chains themselves are band
            //     ids owned by the band sweep (rule 1 for r1, rule 2 for the spell_ranks ranks).
            {
                static const uint32 kStockInsectSwarm[] = { 5570, 24974, 24975, 24976, 24977, 27013, 48468 };
                bool swap = (era == ERA_VANILLA && EraNodeReplacesStock(ERA_VANILLA, CLASS_DRUID, 18738, 5570))
                         || (era == ERA_TBC     && EraNodeReplacesStock(ERA_TBC,     CLASS_DRUID, 20107, 5570));
                if (swap)
                    for (uint32 spell : kStockInsectSwarm)
                        if (p->HasSpell(spell))
                            p->removeSpell(spell, SPEC_MASK_ALL, false);
            }
        }

        // Plan 2 Phase A scaffold: hidden presence-only era-totem markers 932416 (Vanilla) /
        // 932417 (post-Vanilla). Mirrors the paladin "Seal Training" 932749 marker pattern above —
        // grant the era's own marker, strip the other era's marker. No stock-spell version-swap or
        // custom-clone strip/grant lists yet; those land with the totem content itself.
        if (p->getClass() == CLASS_SHAMAN)
        {
            // Quest-taught FIRST totems (Stoneskin / Searing / Healing Stream): stock r1 has NO
            // trainer_spell row — it is granted by the Call of Earth/Fire/Water quests (learn-wrappers
            // 8073/2075/5396) or PlayerbotFactory::InitClassSpells for bots, never a trainer. Each
            // era's version of the totem is therefore unreachable by training when the previous era's
            // version is stripped/swept (the Vanilla clone is only sold at the CITY trainer 14 anyway,
            // so a starter-zone shaman couldn't reach even that), so every era arm re-grants ITS
            // version when the character has EARNED the totem in any era: still knows the stock id,
            // knows any era's clone, or completed a granting quest (all races). Higher ranks stay
            // trainer-taught (their stock/clone counterparts have rows rooted on r1 — 6363 roots on
            // 3599, so a missing 3599 also bricks the whole TBC/WotLK Searing chain).
            // tbcId 0 = TBC keeps the stock id.
            struct QuestTotem { uint32 stockId; uint32 vanillaId; uint32 tbcId; uint32 quests[3]; };
            static const QuestTotem kQuestTotems[] = {
                { 8071, 931020, 947020, { 1518, 1521, 9451 } },  // Stoneskin      <- Call of Earth (Orc/Tauren/Draenei)
                { 3599, 931100,      0, { 1527, 9555,    0 } },  // Searing        <- Call of Fire   (TBC/WotLK: stock)
                { 5394, 931180, 947170, {   96, 9509,    0 } },  // Healing Stream <- Call of Water
            };
            auto grantQuestTotems = [&](EraId e)
            {
                for (auto const& qt : kQuestTotems)
                {
                    uint32 const target = e == ERA_VANILLA ? qt.vanillaId
                                        : e == ERA_TBC     ? (qt.tbcId ? qt.tbcId : qt.stockId)
                                        :                    qt.stockId;
                    if (p->HasSpell(target))
                        continue;

                    bool earned = p->HasSpell(qt.stockId) || p->HasSpell(qt.vanillaId)
                               || (qt.tbcId && p->HasSpell(qt.tbcId));
                    for (uint32 q : qt.quests)
                        earned = earned || (q && p->GetQuestRewardStatus(q));
                    if (!earned)
                        continue;

                    p->learnSpell(target);
                    if (EraTalentBots::IsBot(p))
                        LOG_DEBUG("module", "[mod-era-talents] {} granted quest-taught totem {} (era {}, stock {})",
                                 p->GetName(), target, uint32(e), qt.stockId);
                    else
                        LOG_INFO("module", "[mod-era-talents] {} granted quest-taught totem {} (era {}, stock {})",
                                 p->GetName(), target, uint32(e), qt.stockId);
                }
            };

            // Vanilla shaman totems: the stock totems being replaced (stripped in Vanilla) and the
            // post-Vanilla totems to hide (stripped in Vanilla). The custom 931xxx clones need no list
            // here — they are era-allowlisted band ids, so the band sweep (EraBandClassifier) removes
            // them from any shaman that is no longer Vanilla.
            static const uint32 kStockShaSwap[] = {
                2484, 3599, 5394, 5675, 5730, 6363, 6364, 6365, 6375, 6377, 6390, 6391, 6392, 8071, 8075,
                8143, 8154, 8155, 8160, 8161, 8177, 8181, 8184, 8190, 8227, 8249, 10406, 10407, 10408,
                10427, 10428, 10437, 10438, 10442, 10462, 10463, 10478, 10479, 10495, 10496, 10497, 10526,
                10537, 10538, 10585, 10586, 10587, 10595, 10600, 10601, 16387,
                // Stock WotLK Windfury Totem summons (mechanic fully reworked for Vanilla into the
                // custom 931363-365 chain above) — 10613/10614/25585/25587 are absent from this
                // server's 3.3.5a Spell.dbc extract (no trainer_spell row either, verified against
                // the base dump) but are listed here anyway for completeness/future-proofing; only
                // 8512 has an actual stock trainer row to re-gate.
                8512, 10613, 10614, 25585, 25587,
                16164,  // stock Elemental Focus — stale TBC-node grant on a bracket-down move (see the kStockShaSwapTbc entry); talent-only, never legit below WotLK era
                6495    // stock Sentry (fix round 2026-09-02): Vanilla now trains clone 931390 at 1.12's 80-flat-mana/Nature
                        //   values (live 6495 is 2%-of-base/Physical); the stock trainer row is re-gated behind 932417
            };
            static const uint32 kEraShaHide[] = { 3738, 2894, 2062, 8170, 1535 };  // WrathOfAir, Fire/Earth Elemental, Cleansing Totem, Fire Nova spell

            // TBC shaman totems (Plan 3b Task 6, the three-way split of triage row 142). The TBC
            // trained-custom and stock-swap lists are SUPERSETS of Vanilla's, not the same lists:
            //   * the trainer-taught 947xxx summons behind marker 947440
            //     (2026_08_31_20_era_totem_tbc_trainers.sql) need no list: they are era-allowlisted
            //     band ids the band sweep (EraBandClassifier) removes from any non-TBC shaman.
            //   * kStockShaSwapTbc = the stock ids TBC replaces. Vanilla's list MINUS the Searing
            //     chain 3599/6363/6364/6365/10437/10438 (grant-stock — TBC KEEPS them; the un-gated
            //     trainer chain in 2026_08_31_21 depends on this arm never stripping them), PLUS
            //     the 61-70 stock ranks TBC replaces with fresh clones, PLUS 6495 Sentry (its
            //     trainer row stays ungated for Vanilla's sake — this strip is what suppresses the
            //     wrong-cost stock version for TBC, _ref ungated_trainer_rows 6495), PLUS
            //     3738/2894/2062 (Wrath of Air + the two Elementals: hidden in Vanilla, but TBC
            //     CONTENT replaced by clones 947300/947150/947080, so for TBC they are version-
            //     swaps, not hides). 25359/25577 (Grace of Air r3 / Windwall r4) are absent from
            //     this server's Spell.dbc outright and are not listed.
            //   * kEraShaHideTbc = what stays post-TBC: 8170 (the merged WotLK Cleansing Totem —
            //     TBC splits it in two) and 1535 (the WotLK instant Fire Nova SPELL occupying
            //     TBC's Fire Nova Totem r1 slot).
            // Reused 931xxx clones are pulses/passives/creature-cast spells, never player-known,
            // so no arm needs a keep-list for them; every player-KNOWN custom is era-exclusive.
            static const uint32 kStockShaSwapTbc[] = {
                2484, 5394, 5675, 5730, 6375, 6377, 6390, 6391, 6392, 8071, 8075,
                8143, 8154, 8155, 8160, 8161, 8177, 8181, 8184, 8190, 8227, 8249, 10406, 10407, 10408,
                10427, 10428, 10442, 10462, 10463, 10478, 10479, 10495, 10496, 10497, 10526,
                10537, 10538, 10585, 10586, 10587, 10595, 10600, 10601, 16387,
                8512, 10613, 10614, 25585, 25587,                                // Windfury Totem stock summons
                6495,                                                            // Sentry (wrong-cost live version; clone 947260 replaces it)
                3738, 2894, 2062,                                                // Wrath of Air + Elementals: TBC version-swaps
                25361, 25508, 25509, 25525, 25528, 25533, 25546, 25547,          // the 61-70 stock ranks
                25552, 25557, 25560, 25563, 25567, 25570, 25574,                 //   TBC replaces with fresh clones
                25505,                                                           // Windfury WEAPON r5 (cost-model diverged; clone 947441 replaces it — r1-r4 stock are KEPT)
                16164                                                            // stock Elemental Focus (fix round 2026-09-02: node 20205 now grants clone 947444; the stale stock
                                                                                 //   grant would otherwise run BOTH proc shells — the band sweep can't see a STOCK id, and 16164 left the
                                                                                 //   node table. Talent-only in every era (no trainer row); WotLK arm never strips, so a native
                                                                                 //   WotLK shaman's real talent is untouched)
            };
            static const uint32 kEraShaHideTbc[] = { 8170, 1535 };

            if (era == ERA_VANILLA)
            {
                grantQuestTotems(ERA_VANILLA);

                if (!p->HasSpell(932416))
                    p->learnSpell(932416);

                for (uint32 s : kStockShaSwap)
                    if (p->HasSpell(s))
                        p->removeSpell(s, SPEC_MASK_ALL, false);
                for (uint32 s : kEraShaHide)
                    if (p->HasSpell(s))
                        p->removeSpell(s, SPEC_MASK_ALL, false);
            }
            else if (era == ERA_TBC)
            {
                grantQuestTotems(ERA_TBC);

                if (!p->HasSpell(947440))
                    p->learnSpell(947440);

                for (uint32 s : kStockShaSwapTbc)
                    if (p->HasSpell(s))
                        p->removeSpell(s, SPEC_MASK_ALL, false);
                for (uint32 s : kEraShaHideTbc)
                    if (p->HasSpell(s))
                        p->removeSpell(s, SPEC_MASK_ALL, false);
            }
            else
            {
                grantQuestTotems(ERA_WOTLK);

                if (!p->HasSpell(932417))
                    p->learnSpell(932417);

                // The quest-taught first totems re-granted above are the ONE exception: their stock r1
                // (8071/3599/5394) has no trainer_spell row at all, so a character advancing out of an
                // earlier era would lose them outright when the band sweep removes that era's clone.
                // Every other stock shaman totem stays trainer-taught via the re-gated trainer rows
                // Phase G adds (unlike paladin seals, which WotLK auto-learns).
            }
        }

        // Generic baseline-spell gate table — runs for ALL classes (each row self-selects on its
        // own classId). Hoisted above the priest-only tail below so the WARRIOR/MAGE baseline-leak
        // gates fire too; the priest Holy Nova entry behaves exactly as before.
        for (const BaselineSpellGate& g : kBaselineSpellGates)
        {
            if (g.classId != p->getClass())
                continue;

            // Blessing of Kings / Greater BoK (both keyed on Vanilla talent 18620) are TRAINER-baseline
            // in WotLK but remain a TALENT in TBC (node 20025). Skip their WotLK-style force-grant for a
            // TBC paladin — the CLASS_PALADIN TBC arm above node-gates them; the generic auto-grant would
            // otherwise leak BoK as baseline. Consecration (talent 18606) IS baseline in TBC and is
            // untouched by this guard.
            if (era == ERA_TBC && g.classId == CLASS_PALADIN && g.talentId == 18620)
                continue;

            // Faerie Fire (Feral) (keyed on Vanilla druid talent 18729) is likewise still a TALENT in
            // TBC (node 20133), not baseline until WotLK. Skip its WotLK-style force-grant for a TBC
            // druid — the CLASS_DRUID TBC arm (5) above node-gates it (strip when 20133 is unspent),
            // exactly as the paladin BoK guard defers to the CLASS_PALADIN TBC arm.
            // Readiness-gated on the SAME predicate arm (5) uses: while node 20133 is a display-only
            // seed there is no talent that can grant 16857, so skipping the force-grant here would take
            // FF(Feral) away from every TBC druid with nothing to replace it. Until the node is wired,
            // TBC keeps the WotLK-style force-grant below (i.e. today's shipped behavior).
            if (era == ERA_TBC && g.classId == CLASS_DRUID && g.talentId == 18729 &&
                EraNodeIsWired(ERA_TBC, CLASS_DRUID, 20133))
                continue;

            // Shield Slam (keyed on Vanilla warrior talent 18552) is likewise still a TALENT in TBC
            // (node 20362), not baseline until WotLK — but with a twist the paladin/druid guards above
            // do not have: TBC node 20362 grants a CLONE chain (947541-947546), never the stock ranks
            // (spec Amendment A.2 — every TBC-reachable stock rank's damage diverges). So for a TBC
            // warrior this row is wrong in BOTH directions: the force-grant would hand out stock r1 at
            // L40 for free and unlock the stock trained chain behind it (the one real baseline leak in
            // the warrior tree, era-data/_ref/tbc/warrior-spells.yaml baseline_leaks), and the strip
            // must fire regardless of whether the node is spent. Skip the row here; the CLASS_WARRIOR
            // arm (4) above owns the (unconditional) TBC strip, exactly as this guard defers to the
            // CLASS_DRUID / CLASS_PALADIN TBC arms.
            // Readiness-gated on the SAME predicate that arm uses: while node 20362 is a display-only
            // seed there is no talent that can grant Shield Slam, so skipping the force-grant would
            // take it away from every TBC warrior with nothing to replace it. Until the TBC chain is
            // authored, TBC keeps the WotLK-style force-grant below (i.e. today's shipped behavior).
            if (era == ERA_TBC && g.classId == CLASS_WARRIOR && g.talentId == 18552 &&
                EraReplacementSpellReady(ERA_TBC, 947541) && EraNodeIsWired(ERA_TBC, CLASS_WARRIOR, 20362))
                continue;

            // Holy Nova (keyed on Vanilla priest talent 18121) is still a TALENT in TBC (node 20527),
            // not baseline until WotLK. Skip its WotLK-style force-grant for a TBC priest — the
            // CLASS_PRIEST TBC block below node-gates it (strip when 20527 is unspent), exactly as
            // the druid Faerie Fire (Feral) guard defers to arm (5). Readiness-gated: while 20527 is
            // a display-only seed, TBC keeps today's shipped force-grant.
            if (era == ERA_TBC && g.classId == CLASS_PRIEST && g.talentId == 18121 &&
                EraNodeIsWired(ERA_TBC, CLASS_PRIEST, 20527))
                continue;

            // Divine Spirit + Prayer of Spirit (BOTH rows keyed on Vanilla priest talent 18113) are
            // still a TALENT in TBC (node 20513) — and, like Shield Slam, the TBC node grants a CLONE
            // chain (948024-948030), never the stock ranks: live 14752 lost the two hidden effects
            // (aura 174/175 at base 0) that TBC Improved Divine Spirit 33174/33182 spellmods
            // (SPELLMOD_EFFECT2/3), so a stock grant can never carry Improved DS. The force-grant
            // would hand out stock r1 at L30 AND unlock the stock trained chain behind it (and Prayer
            // of Spirit behind that). Skip both rows; the CLASS_PRIEST TBC block owns the
            // unconditional strip. Gated via EraNodeReplacesStock so the guard arms only once node
            // 20513 actually grants a NON-stock replacement — a half-authored state where 948024
            // exists but the node still grants stock 14752 must not skip the force-grant with nothing
            // to replace it.
            // Helper ids 948024-948033 are FIXED by the plan; Tasks 6-8 must allocate exactly these
            // or update this site in the same commit.
            if (era == ERA_TBC && g.classId == CLASS_PRIEST && g.talentId == 18113 &&
                EraNodeReplacesStock(ERA_TBC, CLASS_PRIEST, 20513, 14752))
                continue;

            if (era >= ERA_TBC)
            {
                // grantHigherEra=true (Holy Nova, and now Shield Slam/Ice Block/Divine Spirit whose
                // ungated rank-1 trainer rows were deleted): force-grant rank 1 once the player is high
                // enough level, since the trainer no longer offers it; the player then trains the higher
                // ranks (which chain ReqAbility1 on rank 1). A grantHigherEra=false gate would be
                // strip-only here (left for a future spell that stays freely trainable in higher eras).
                if (!g.grantHigherEra)
                    continue;
                uint32 rank1 = g.rankSpells[0];
                if (rank1 != 0 && p->GetLevel() >= g.minLevel && !p->HasSpell(rank1))
                    p->learnSpell(rank1);
            }
            else
            {
                // Vanilla: talent-only. If the era talent hasn't been learned, strip any ranks
                // the player somehow has (e.g. carried over from an era transition, or bought from
                // the WotLK-era trainer rows — the baseline leak). If the talent IS learned, leave
                // the spells alone — the talent grants rank 1 and the player can train the rest.
                if (CurrentRank(p, ERA_VANILLA, g.talentId) == 0)
                {
                    for (uint32 spell : g.rankSpells)
                        if (spell != 0 && p->HasSpell(spell))
                            p->removeSpell(spell, SPEC_MASK_ALL, false);
                }
            }
        }

        // Generic band sweep (Phase 9.5) runs for ALL classes, after every strip arm above (the priest
        // tail below is grant-only), so it sees the final justified state. It strips every held [920000,950000) spell that is not
        // the current era's node grant, a spell_ranks rank above a node-granted r1, or a
        // band-allowlist.yaml match — in every era including WotLK, for players and bots. The rule
        // lives in EraBandClassifier.cpp; the doctor calls the same Classify().
        EraBandClassifier::Sweep(p, era);

        if (p->getClass() != CLASS_PRIEST)
            return;

        // VE/PI Vanilla rebuild. In Vanilla, Power Infusion (10060) and Vampiric Embrace (15286)
        // are replaced by era-safe CUSTOM castables (920920, 921152) — ReapplyOnLogin already grants
        // the custom spell for a recorded talent rank, but nothing strips the stale stock spell, so a
        // priest who learned the talent before the rebuild keeps the WotLK-behavior spell on their bar
        // (PI casting-speed, VE self-buff). Strip the stock ids for Vanilla priests. And keep the
        // hidden VE heal proc-passive (932950) present iff the VE talent is learned: it MUST be a
        // permanent learned passive (like Shadow Weaving / Blessed Recovery) to proc the party heal —
        // a temp buff applied via TRIGGER_SPELL did not proc.
        if (era < ERA_TBC)
        {
            for (uint32 obsolete : { 10060u, 15286u })
                if (p->HasSpell(obsolete))
                    p->removeSpell(obsolete, SPEC_MASK_ALL, false);

            bool hasVE = CurrentRank(p, ERA_VANILLA, 18144) >= 1;
            if (hasVE && !p->HasSpell(932950))
                p->learnSpell(932950);
        }

        // ---------------- TBC priest (Phase 6) ----------------
        // Every arm is readiness-gated so it is inert while era-data/tbc/priest.yaml is a
        // display-only seed and self-arms when the node actually grants its replacement.
        // HELPER IDS 948024-948033 ARE FIXED BY THE PHASE-6 PLAN and are allocated by Tasks 6-8;
        // if a later task allocates different ids it MUST update these constants in the same commit.
        if (era == ERA_TBC)
        {
            // (1) Holy Nova — still a TALENT in TBC (node 20527 grants STOCK 15237). The gate walker
            //     skips the WotLK force-grant for a TBC priest; strip the stock ranks when the node
            //     is unspent (druid Faerie Fire (Feral) shape — strip-only, grant side is the node's
            //     own rank grant + ReapplyOnLogin). removeSpell(15237) cascades down the stock
            //     spell_ranks chain (Holy Nova is baseline in 3.3.5a and not in Talent.dbc;
            //     Player::removeSpell cascades down the spell_ranks chain because each NEXT rank is a
            //     non-talent rank — Player.cpp GetTalentSpellPos test), so higher trained ranks go too;
            //     the explicit list is the login safety-net for a char whose r1 is already gone.
            if (EraNodeIsWired(ERA_TBC, CLASS_PRIEST, 20527) && CurrentRank(p, ERA_TBC, 20527) == 0)
                for (uint32 spell : { 15237u, 15430u, 15431u, 27799u, 27800u, 27801u, 25331u, 48077u, 48078u })
                    if (p->HasSpell(spell))
                        p->removeSpell(spell, SPEC_MASK_ALL, false);

            // (2) Divine Spirit + Prayer of Spirit — node 20513 grants the CLONE chain (948024 r1;
            //     r2-r5 948025-948028 + Prayer of Spirit 948029/948030 trainer-taught, hand SQL
            //     2026_09_05_02). Every stock rank is wrong for TBC (no aura 174/175 effects), so the
            //     strip is UNCONDITIONAL over the node rank (warrior Shield Slam shape) and reads
            //     BOTH 18113 gate rows so the lists can never drift. The Vanilla arm is untouched.
            //     Gated via EraNodeReplacesStock so this arm arms only once node 20513 actually grants
            //     a NON-stock replacement — a half-authored state where 948024 exists but the node
            //     still grants stock 14752 must not strip the node's own grant.
            if (EraNodeReplacesStock(ERA_TBC, CLASS_PRIEST, 20513, 14752))
                for (const BaselineSpellGate& g : kBaselineSpellGates)
                    if (g.classId == CLASS_PRIEST && g.talentId == 18113)
                        for (uint32 spell : g.rankSpells)
                            if (spell != 0 && p->HasSpell(spell))
                                p->removeSpell(spell, SPEC_MASK_ALL, false);

            // (3) Grant-change stock swaps — a TBC node hands over a CLONE, so the stock id must go
            //     (lesson 17f: the band sweep (EraBandClassifier) only sees managed-band ids, never a
            //     stock one). Readiness-gated per
            //     row via EraNodeReplacesStock (rogue kEraRogTbcStockSwaps shape): while the node is a
            //     seed or grants stock, the arm is dormant.
            struct PriestStockSwap { uint32 node; uint32 stockId; };
            static const PriestStockSwap kEraPriTbcStockSwaps[] =
            {
                { 20518, 10060 },   // Power Infusion  -> TBC clone (3 min cd; Amendment A.2)
                { 20555, 15286 },   // Vampiric Embrace -> TBC castable clone
                { 20521, 33206 },   // Pain Suppression -> TBC clone (2 min cd)
                { 20563, 34914 },   // Vampiric Touch r1 -> TBC clone (party mana return)
                { 20542, 34861 },   // Circle of Healing r1 -> TBC clone (party of target, no cd)
                { 20519, 33201 },   // Reflective Shield r1 -> TBC clone ranks (see EraTalentProcScripts)
                { 20539, 724   },   // Lightwell r1 -> TBC clone (Amendment A.3)
                { 20550, 15407 },   // Mind Flay r1 -> TBC clone chain (Amendment A.8)
                { 20554, 15487 },   // Silence -> TBC clone (Amendment A.9)
                { 20560, 15473 },   // Shadowform -> TBC clone (Amendment A.9)
            };
            for (const PriestStockSwap& sw : kEraPriTbcStockSwaps)
                if (p->HasSpell(sw.stockId) && EraNodeReplacesStock(ERA_TBC, CLASS_PRIEST, sw.node, sw.stockId))
                    p->removeSpell(sw.stockId, SPEC_MASK_ALL, false);

            // (3b) STOCK trained ranks behind a swapped r1 — the login safety-net (rogue Mutilate
            //      kRogueTbcMutilateStockRanks shape). The r1 strip above cascades through the stock
            //      spell_ranks chain; this catches a char whose r1 is already gone. Stock rows in
            //      trainer_spell are NOT deleted (native priests train them; deleting would strand the
            //      WotLK ranks — orphaned-higher-rank check, _ref capstone_chains).
            //      EXCEPTION: Reflective Shield — 33202 is a WotLK TALENT rank, so the cascade from
            //      33201 stops there and this row is the ONLY strip path for it (nobody trains
            //      Reflective Shield); the row is load-bearing, not a safety net.
            struct PriestStockChain { uint32 node; uint32 stockR1; uint32 ranks[6]; };   // 0 = unused slot
            static const PriestStockChain kEraPriTbcStockChains[] =
            {
                { 20542, 34861, { 34863, 34864, 34865, 34866 } },                 // Circle of Healing r2-5
                { 20563, 34914, { 34916, 34917 } },                               // Vampiric Touch r2-3
                { 20519, 33201, { 33202, 33203, 33204, 33205 } },                 // Reflective Shield r2-5
                { 20550, 15407, { 17311, 17312, 17313, 17314, 18807, 25387 } },   // Mind Flay r2-7
                { 20539, 724,   { 27870, 27871, 28275 } },                        // Lightwell r2-4
            };
            for (const PriestStockChain& ch : kEraPriTbcStockChains)
                if (EraNodeReplacesStock(ERA_TBC, CLASS_PRIEST, ch.node, ch.stockR1))
                    for (uint32 spell : ch.ranks)
                        if (spell != 0 && p->HasSpell(spell))
                            p->removeSpell(spell, SPEC_MASK_ALL, false);

            // (4) TBC Vampiric Embrace hidden proc-passive 948032 — present iff node 20555 is spent
            //     (the exact shape of the Vanilla 932950 arm above; a PERMANENT learned passive is
            //     what procs the party heal — a TRIGGER_SPELL temp buff did not). The VANILLA
            //     proc-passive 932950 needs no strip here either: it is an era-allowlisted band id
            //     the band sweep (EraBandClassifier) removes from a TBC priest.
            if (EraReplacementSpellReady(ERA_TBC, 948031) && EraReplacementSpellReady(ERA_TBC, 948032) &&
                EraNodeIsWired(ERA_TBC, CLASS_PRIEST, 20555))
            {
                bool hasVeTbc = CurrentRank(p, ERA_TBC, 20555) >= 1;
                if (hasVeTbc && !p->HasSpell(948032))
                    p->learnSpell(948032);
            }
        }
    }

    void ReapplyOnLogin(Player* p, EraId era)
    {
        // Reads via the rank cache (one load), which also primes it for the whole session.
        // Iterate a COPY: learnSpell below fires hooks that re-take the cache lock, so the
        // lock can't be held across the loop.
        std::unordered_map<uint32, uint8> ranks;
        {
            std::lock_guard<std::mutex> guard(g_rankCacheMutex);
            ranks = LoadRanks(p, era);
        }
        for (auto const& [talentId, rank] : ranks)
        {
            const EraTalentNode* n = sEraTalentContent->Node(talentId);
            if (!n || rank == 0 || uint32(rank - 1) >= n->rankSpell.size())
                continue;

            uint32 spell = n->rankSpell[rank - 1];
            if (spell != 0 && !p->HasSpell(spell))
                p->learnSpell(spell);
        }
    }
}
