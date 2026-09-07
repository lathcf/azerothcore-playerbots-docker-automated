#include "EraTalents.h"
#include "EraTransition.h"
#include "EraTalentContent.h"
#include "EraBandClassifier.h"
#include "EraTalentIP.h"
#include "EraTalentBots.h"   // EraTalentBots::EraFor — GM commands can target a BOT by name
#include "ScriptMgr.h"
#include "Chat.h"
#include "Player.h"
#include "SpellMgr.h"
#include "DatabaseEnv.h"
#include "QueryResult.h"
#include "Field.h"
#include "Pet.h"
#include "SpellAuraEffects.h"
#include "Playerbots.h"      // GET_PLAYERBOT_AI — the doctor's "is this target a bot" label
#include <string>
#include <vector>

using namespace Acore::ChatCommands;

// `.eratalents` admin/inspection command (GM + worldserver console). Lets an admin drive the
// server-authoritative engine directly — independent of the client addon — for testing and
// support: inspect a char's era/points/spent nodes, force a learn, or reset the current era.
// All three resolve the target from a name arg (Console) or the selected/self player (in-game).
class era_talents_commandscript : public CommandScript
{
public:
    era_talents_commandscript() : CommandScript("era_talents_commandscript") {}

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable etTable =
        {
            { "status", HandleStatus, SEC_GAMEMASTER, Console::Yes },
            { "learn",  HandleLearn,  SEC_GAMEMASTER, Console::Yes },
            { "reset",  HandleReset,  SEC_GAMEMASTER, Console::Yes },
            { "doctor", HandleDoctor, SEC_GAMEMASTER, Console::Yes },
            { "bandsweep", HandleBandSweep, SEC_GAMEMASTER, Console::Yes },
        };
        static ChatCommandTable commandTable =
        {
            { "eratalents", etTable },
        };
        return commandTable;
    }

    static Player* Resolve(ChatHandler* handler, Optional<PlayerIdentifier>& player)
    {
        if (!player)
            player = PlayerIdentifier::FromTargetOrSelf(handler);
        if (!player)
            return nullptr;
        return player->GetConnectedPlayer();   // nullptr if the named char is offline
    }

    static const char* EraName(EraId era)
    {
        return era == ERA_VANILLA ? "Vanilla" : era == ERA_TBC ? "TBC" : "WotLK";
    }

    static bool HandleStatus(ChatHandler* handler, Optional<PlayerIdentifier> player)
    {
        Player* t = Resolve(handler, player);
        if (!t)
        {
            handler->SendSysMessage("eratalents: target not online (pass a name from console).");
            return true;
        }
        EraId era = EraTalentBots::EraFor(t);   // bot targets read their level-band era
        handler->PSendSysMessage("EraTalents {}: era={} available={} spent={}",
            t->GetName(), EraName(era), EraTalents::AvailablePoints(t, era), EraTalents::SpentPoints(t, era));
        if (QueryResult r = CharacterDatabase.Query(
                "SELECT talentId, `rank` FROM era_character_talent WHERE guid={} AND eraId={} ORDER BY talentId",
                t->GetGUID().GetCounter(), uint8(era)))
        {
            do
            {
                Field* f = r->Fetch();
                handler->PSendSysMessage("  node {} rank {}", f[0].Get<uint32>(), uint32(f[1].Get<uint8>()));
            } while (r->NextRow());
        }
        else
        {
            handler->SendSysMessage("  (no era talents spent this era)");
        }
        return true;
    }

    static bool HandleLearn(ChatHandler* handler, Optional<PlayerIdentifier> player, uint32 talentId)
    {
        Player* t = Resolve(handler, player);
        if (!t)
        {
            handler->SendSysMessage("eratalents: target not online (pass a name from console).");
            return true;
        }
        std::string err;
        if (EraTalents::TryLearn(t, talentId, err))
            handler->PSendSysMessage("Learned node {} for {} (now rank {}).",
                talentId, t->GetName(), uint32(EraTalents::CurrentRank(t, EraTalentBots::EraFor(t), talentId)));
        else
            handler->PSendSysMessage("Learn rejected: {}", err);
        return true;
    }

    static bool HandleReset(ChatHandler* handler, Optional<PlayerIdentifier> player)
    {
        Player* t = Resolve(handler, player);
        if (!t)
        {
            handler->SendSysMessage("eratalents: target not online (pass a name from console).");
            return true;
        }
        EraId era = EraTalentBots::EraFor(t);   // bot targets read their level-band era
        EraTalents::Reset(t, era);
        handler->PSendSysMessage("Reset era talents for {} (era {}).", t->GetName(), EraName(era));
        return true;
    }

    // Phase 9.5: run the shared band sweep on demand (the same EraBandClassifier::Sweep reconcile
    // calls) and echo every strip with its verdict + reason. GM/console only.
    static bool HandleBandSweep(ChatHandler* handler, Optional<PlayerIdentifier> player)
    {
        Player* t = Resolve(handler, player);
        if (!t)
        {
            handler->SendSysMessage("eratalents: target not online (pass a name from console).");
            return true;
        }
        EraId era = EraTalentBots::EraFor(t);   // bot targets read their level-band era
        handler->PSendSysMessage("== eratalents bandsweep: {} (class {} level {}, era {}) ==",
            t->GetName(), uint32(t->getClass()), uint32(t->GetLevel()), EraName(era));
        uint32 n = EraBandClassifier::Sweep(t, era, [handler](uint32 id, EraBandClassifier::Result const& r)
        {
            handler->PSendSysMessage("  stripped {}: {} — {}", id, EraBandClassifier::VerdictName(r.verdict), r.reason);
        });
        handler->PSendSysMessage("== bandsweep done: {} stripped ==", n);
        return true;
    }

    // Read-only diagnostic: era state, learned era talents + their grants, stale stock spells,
    // and per-custom-spell data-layer sanity (spell_dbc / spell_proc / spell_script_names).
    // NB: Player::HasSpell is the authority on "knows the spell" — character_spell rows persist
    // with specMask=0 after removeSpell on talent-cost spells, so raw row checks mislead.
    static bool HandleDoctor(ChatHandler* handler, Optional<PlayerIdentifier> player)
    {
        Player* t = Resolve(handler, player);
        if (!t)
        {
            handler->SendSysMessage("eratalents: target not online (pass a name from console).");
            return true;
        }

        EraId era = EraTalentBots::EraFor(t);   // bot targets read their level-band era
        std::string const& gen = sEraTalentContent->GenerationStamp();
        handler->PSendSysMessage("== eratalents doctor: {} (class {} level {}) ==",
            t->GetName(), uint32(t->getClass()), uint32(t->GetLevel()));
        // storedEra is written ONLY by EraTransition, whose every entry point bails on a bot — so
        // for a bot target it is a meaningless leftover (usually the default), NOT a disagreement
        // with `era` above. Label it rather than let a reader chase a phantom mismatch (M-73).
        bool const targetIsBot = t->GetSession() && (t->GetSession()->IsBot() || GET_PLAYERBOT_AI(t) != nullptr);
        handler->PSendSysMessage("era={} storedEra={}{} available={} spent={} generation={}{}",
            EraName(era), EraName(EraTransition::StoredEra(t)),
            targetIsBot ? " (player-path only)" : "",
            EraTalents::AvailablePoints(t, era), EraTalents::SpentPoints(t, era),
            gen.empty() ? "<unset>" : gen,
            gen.empty() ? "  <-- PROBLEM (era_talent_meta not imported; client canary silent)" : "");

        // Learned era talents and their grants.
        if (QueryResult r = CharacterDatabase.Query(
                "SELECT talentId, `rank` FROM era_character_talent WHERE guid={} AND eraId={} ORDER BY talentId",
                t->GetGUID().GetCounter(), uint8(era)))
        {
            do
            {
                Field* f = r->Fetch();
                uint32 tid = f[0].Get<uint32>();
                uint8 rank = f[1].Get<uint8>();
                const EraTalentNode* n = sEraTalentContent->Node(tid);
                if (!n)
                {
                    handler->PSendSysMessage("  node {} rank {}: NOT IN CONTENT (stale row?)", tid, uint32(rank));
                    continue;
                }
                uint32 grant = (rank >= 1 && uint32(rank - 1) < n->rankSpell.size()) ? n->rankSpell[rank - 1] : 0;
                if (grant == 0)
                {
                    handler->PSendSysMessage("  node {} rank {}: display-only", tid, uint32(rank));
                    continue;
                }
                bool known = t->HasSpell(grant);
                bool inStore = sSpellMgr->GetSpellInfo(grant) != nullptr;
                handler->PSendSysMessage("  node {} rank {}: grant {} known={} spellInfo={}{}",
                    tid, uint32(rank), grant, known ? "yes" : "NO", inStore ? "yes" : "MISSING",
                    (!known || !inStore) ? "  <-- PROBLEM" : "");

                // Data-layer sanity for custom-band grants.
                if (grant >= EraTalents::ERA_CUSTOM_BAND_LOW && grant < EraTalents::ERA_CUSTOM_BAND_HIGH)
                {
                    if (QueryResult pr = WorldDatabase.Query(
                            "SELECT ProcFlags, HitMask, SpellTypeMask, SpellPhaseMask, Chance "
                            "FROM spell_proc WHERE SpellId={}", grant))
                    {
                        Field* pf = pr->Fetch();
                        uint32 hitMask = pf[1].Get<uint32>();
                        handler->PSendSysMessage("    spell_proc: flags={} hitMask={}{} typeMask={} phaseMask={} chance={}",
                            pf[0].Get<uint32>(), hitMask,
                            // hitMask 8 = PROC_HIT_FULL_RESIST. NOT a defect (M-61): core sets it
                            // on SPELL_MISS_RESIST (Unit.cpp:210), and the Magic Absorption chains
                            // (Vanilla 932326-330 / TBC 948762-766) use it exactly that way — they
                            // are SUPPOSED to fire only on a full resist. Annotated, never flagged.
                            hitMask == 8 ? " (FULL_RESIST-only — fires on resists)" : "",
                            pf[2].Get<uint32>(), pf[3].Get<uint32>(), pf[4].Get<float>());
                    }
                    if (QueryResult sr = WorldDatabase.Query(
                            "SELECT ScriptName FROM spell_script_names WHERE spell_id={}", grant))
                        handler->PSendSysMessage("    script: {}", (*sr)[0].Get<std::string>());
                }
            } while (r->NextRow());
        }
        else
            handler->SendSysMessage("  (no era talents spent this era)");

        // Stale stock spells a Vanilla character must not actively know. PI (10060) and VE (15286)
        // are unconditionally replaced by era-safe custom castables; the Holy Nova stock ranks are
        // legitimate when the era talent (18121) IS learned — reconcile leaves them then — so only
        // flag those when the talent is absent.
        if (era < ERA_TBC)
        {
            for (uint32 stock : { 10060u, 15286u })
                if (t->HasSpell(stock))
                    handler->PSendSysMessage("  STALE STOCK SPELL ACTIVE: {}  <-- PROBLEM (reconcile should strip)", stock);
            if (EraTalents::CurrentRank(t, ERA_VANILLA, 18121) == 0)
                for (uint32 stock : { 15237u, 15430u, 15431u })
                    if (t->HasSpell(stock))
                        handler->PSendSysMessage("  STALE STOCK SPELL ACTIVE: {}  <-- PROBLEM (reconcile should strip)", stock);

            // Baseline-leak gates (trainer-baseline in WotLK, talent-only in Vanilla). Legitimate
            // only when the gating talent IS learned — reconcile strips them otherwise. Class-scoped
            // so we only flag the ranks that apply to this character's class.
            struct LeakGate { uint8 cls; uint32 talentId; std::vector<uint32> ranks; };
            static const LeakGate kLeakGates[] =
            {
                { CLASS_WARRIOR, 18552, { 23922, 23923, 23924, 23925, 25258, 30356, 47487, 47488 } },
                { CLASS_MAGE,    18046, { 45438 } },
                { CLASS_PRIEST,  18113, { 14752, 14818, 14819, 27841, 25312, 48073 } },
            };
            for (const LeakGate& lg : kLeakGates)
                if (lg.cls == t->getClass() && EraTalents::CurrentRank(t, ERA_VANILLA, lg.talentId) == 0)
                    for (uint32 stock : lg.ranks)
                        if (t->HasSpell(stock))
                            handler->PSendSysMessage("  STALE STOCK SPELL ACTIVE: {}  <-- PROBLEM (baseline-leak; reconcile should strip)", stock);


            // Vanilla mage trained CLONE chains (TBC Phase 9's Vanilla re-audit FIX rows, spec A.4
            // item 3 + A.7; band sweep rule 2 via spell_ranks, hand SQL 2026_09_08_02).
            // Same shape as the TBC mage block below: r1 is NODE-granted, r2+ are trainer-taught on
            // TrainerId 16 behind a ReqAbility1 chain off r1, so the orphan sweep's generic
            // NodesFor() fallback can only ever resolve r1. Node-gated — a mage who respecced OUT
            // but still holds a rank is the lesson-21 strip gap, visible only after a RESPEC.
            if (t->getClass() == CLASS_MAGE)
            {
                // Arcane Power / Presence of Mind: the Vanilla clones own these, so a Vanilla mage
                // must not hold the stock ids. Mirrors the TBC block's 948760/948761 lines (same
                // KNOWN-TRANSIENT caveat: in a half-authored window where the clone row exists but
                // the node still grants STOCK, reconcile correctly keeps the stock spell).
                {
                    struct StaleStock { uint32 clone; uint32 stock; } const stale[] =
                        { { 932760, 12042 }, { 932761, 12043 } };
                    for (StaleStock const& s : stale)
                        if (t->HasSpell(s.stock) && sSpellMgr->GetSpellInfo(s.clone))
                            handler->PSendSysMessage("  STALE STOCK SPELL ACTIVE: {}  <-- PROBLEM (Vanilla clone {} owns this; reconcile should strip)", s.stock, s.clone);
                }

                struct VanChain { uint32 node; char const* name; uint32 r1; uint32 ranks[7]; };
                static const VanChain kMageChainsVanilla[] =
                {
                    { 18024, "Pyroblast",   932310, { 932311, 932312, 932313, 932314, 932315, 932316, 932317 } },
                    { 18030, "Blast Wave",  932320, { 932321, 932322, 932323, 932324, 0, 0, 0 } },
                    { 18049, "Ice Barrier", 932331, { 932332, 932333, 932334, 0, 0, 0, 0 } },
                };
                for (VanChain const& c : kMageChainsVanilla)
                {
                    if (!sSpellMgr->GetSpellInfo(c.r1))
                        continue;
                    uint8 const rank = EraTalents::CurrentRank(t, ERA_VANILLA, c.node);
                    uint32 known = t->HasSpell(c.r1) ? 1 : 0;
                    uint32 top = t->HasSpell(c.r1) ? 1 : 0;
                    for (uint32 i = 0; i < 7; ++i)
                        if (c.ranks[i] && t->HasSpell(c.ranks[i]))
                        {
                            ++known;
                            top = i + 2;
                        }
                    handler->PSendSysMessage("  chain {} (node {}): talent rank {}, clone r1 {}, ranks known {} (highest r{})",
                        c.name, c.node, uint32(rank), t->HasSpell(c.r1) ? "known" : "MISSING", known, top);
                    if (rank != 0 && !t->HasSpell(c.r1))
                        handler->PSendSysMessage("    <-- PROBLEM: node {} is spent but clone r1 {} is NOT known (grant gap)", c.node, c.r1);
                    if (rank == 0 && known != 0)
                        handler->PSendSysMessage("    <-- PROBLEM: node {} is at rank 0 but {} clone rank(s) are still known (respec strip gap, lesson 21)", c.node, known);
                }

                // Shatter (Vanilla node 18045) — the exact mirror of the TBC 20857 predicate below,
                // for the Vanilla auto-passives 920360-920364 (SPELL_AURA_DUMMY, misc 1, amount
                // 10-50) that core patch 0018 reads in Unit::SpellTakenCritChance. Same invariant:
                // a `mechanic: stat` node holds exactly ONE rankSpell (the current rank's), so the
                // expected aura count is 1 while ANY rank is spent and 0 at rank 0 — never one per
                // rank. Two auras = 0018 doubles the crit bonus; zero with the node spent = the
                // passive never self-applied.
                uint8 const vanShatterRank = EraTalents::CurrentRank(t, ERA_VANILLA, 18045);
                uint32 vanShatterAuras = 0;
                for (AuraEffect const* eff : t->GetAuraEffectsByType(SPELL_AURA_DUMMY))
                {
                    uint32 const id = eff->GetSpellInfo()->Id;
                    if (eff->GetMiscValue() == 1 && id >= 920360 && id <= 920364)
                        ++vanShatterAuras;
                }
                handler->PSendSysMessage("  Shatter: node 18045 rank {}, misc-1 auras held = {}{}",
                    uint32(vanShatterRank), vanShatterAuras,
                    (vanShatterAuras == (vanShatterRank ? 1u : 0u)) ? "" : "  <-- PROBLEM (a stat node holds ONE misc-1 rank passive while any rank is spent, none at rank 0)");
            }
            // Reconcile-managed VE proc-passive: learned VE without 932950 = the heal engine
            // is absent (the exact live bug this line exists to catch).
            if (EraTalents::CurrentRank(t, ERA_VANILLA, 18144) >= 1 && !t->HasSpell(932950))
                handler->SendSysMessage("  VE talent learned but proc-passive 932950 NOT KNOWN  <-- PROBLEM (heal cannot fire; reconcile gap)");

            // Reconcile-managed Corruption era cast-time marker (932930, patch 0019). A Vanilla warlock
            // MUST carry it or Corruption stays instant (the exact gap this line exists to catch).
            if (t->getClass() == CLASS_WARLOCK)
                handler->PSendSysMessage("  cast-time marker 932930: known={} aura={}{}",
                    t->HasSpell(932930) ? "yes" : "NO", t->HasAura(932930) ? "yes" : "NO",
                    (t->HasSpell(932930) && t->HasAura(932930)) ? "" : "  <-- PROBLEM (Corruption stays instant; reconcile gap)");
        }
        else if (era == ERA_TBC && t->getClass() == CLASS_PRIEST)
        {
            // TBC priest diagnostics (Phase 6). Mirrors the Vanilla block above for the TBC ids.
            // Helper ids 948024-948033 are FIXED by the plan; Tasks 6-8 must allocate exactly these
            // or update this site in the same commit.
            if (EraTalents::CurrentRank(t, ERA_TBC, 20555) >= 1 && t->HasSpell(948031) && !t->HasSpell(948032))
                handler->SendSysMessage("  TBC VE talent learned but proc-passive 948032 NOT KNOWN  <-- PROBLEM (heal cannot fire; reconcile gap)");
            // Stale-DS gate on READINESS (sSpellMgr->GetSpellInfo), not ownership (t->HasSpell(948024)):
            // the problem case is a priest STILL holding stock Divine Spirit ranks while the clone
            // chain exists in spell_dbc, whether or not this particular character happens to own
            // 948024 yet. Mirrors the two 18113 rows of kBaselineSpellGates in EraTalents.cpp (that
            // table is file-static there, so the hand list here must be kept in sync) — keep in sync.
            if (sSpellMgr->GetSpellInfo(948024) && EraHasTalentTrees(ERA_TBC))
                for (uint32 stock : { 14752u, 14818u, 14819u, 27841u, 25312u, 48073u, 27681u, 32999u, 48074u })
                    if (t->HasSpell(stock))
                        handler->PSendSysMessage("  STALE STOCK SPELL ACTIVE: {}  <-- PROBLEM (TBC Divine Spirit clone chain owns this; reconcile should strip)", stock);

            // Holy Nova (node 20527): stock ranks are legitimate only while the TBC talent is spent
            // (reconcile arm (1) strips them otherwise). Only meaningful once the node is wired — a
            // display-only seed (all-zero rankSpell) must not false-positive on the current seed.
            const EraTalentNode* holyNovaNode = sEraTalentContent->Node(20527);
            bool holyNovaWired = false;
            if (holyNovaNode)
                for (uint32 s : holyNovaNode->rankSpell)
                    if (s != 0)
                    {
                        holyNovaWired = true;
                        break;
                    }
            if (holyNovaWired && EraHasTalentTrees(ERA_TBC) && EraTalents::CurrentRank(t, ERA_TBC, 20527) == 0)
                for (uint32 stock : { 15237u, 15430u, 15431u, 27799u, 27800u, 27801u, 25331u, 48077u, 48078u })
                    if (t->HasSpell(stock))
                        handler->PSendSysMessage("  STALE STOCK SPELL ACTIVE: {}  <-- PROBLEM (Holy Nova rank without the TBC Holy Nova talent 20527; reconcile should strip)", stock);
        }
        else if (era == ERA_TBC && t->getClass() == CLASS_HUNTER)
        {
            // TBC hunter diagnostics (Phase 7). Marker state + stale stock spells the swaps should have
            // removed. EraTalents::EraReplacementSpellReady is file-static in EraTalents.cpp and not
            // exported, so the readiness gate is spelled out inline exactly as the priest block above
            // does it: the clone exists in spell_dbc AND the era owns trees.
            // ALL of these gate on markerReady — the SAME predicate the reconcile TBC arm gates on
            // (EraTalents.cpp: `era == ERA_TBC && EraReplacementSpellReady(ERA_TBC, 948280)`). With TBC
            // outside the EraHasTalentTrees allowlist, or 948280 not yet in spell_dbc, a TBC hunter
            // legitimately falls through to the WotLK arm and legitimately holds 932305 + stock 3043 +
            // stock 19263. Reporting those as PROBLEM would be a pure false positive on exactly the
            // committed configuration. EraTalents::EraReplacementSpellReady is file-static over there
            // and not exported, so the predicate is spelled out inline as the priest block above does.
            // NB: a scoped `if`, never an early `return` — the orphan sweep, band-aura dump and pet
            // section below are part of the same report and must still run.
            bool markerReady = EraHasTalentTrees(ERA_TBC) && sSpellMgr->GetSpellInfo(948280) != nullptr;
            if (markerReady)
            {
                if (!t->HasSpell(948280))
                    handler->SendSysMessage("  TBC hunter without marker 948280  <-- PROBLEM (reconcile gap; Scorpid trainer row unreachable)");
                for (uint32 wrong : { 932304u, 932305u })
                    if (t->HasSpell(wrong))
                        handler->PSendSysMessage("  WRONG-ERA MARKER HELD: {}  <-- PROBLEM (three-way arm should have stripped it)", wrong);
                if (t->HasSpell(19263))
                    handler->SendSysMessage("  STALE STOCK SPELL ACTIVE: 19263 Deterrence  <-- PROBLEM (TBC talent 20650 owns Deterrence; WotLK-only trainer row)");
                // Each row fires only once its OWN era clone is live, so an unshipped tab never reports a
                // stale stock spell the character legitimately still needs (3043 above all).
                // KNOWN TRANSIENT, shared with the priest block's stale-DS rows: in the HALF-AUTHORED window
                // where the clone row exists in spell_dbc but its node still grants STOCK, reconcile
                // correctly KEEPS the stock spell (EraNodeReplacesStock is false) while this row reports it
                // as STALE. The gate is deliberately readiness, not ownership — the problem case is a hunter
                // holding stock while the clone chain exists, whether or not this character owns the clone.
                // Expect these to appear and clear as Tasks 7-8 wire each node.
                struct StaleStock { uint32 clone; uint32 stock; } const stale[] =
                    { { 948370, 19434 }, { 948377, 19506 }, { 948338, 19306 }, { 948330, 19386 }, { 948281, 3043 } };
                for (StaleStock const& s : stale)
                    if (t->HasSpell(s.stock) && sSpellMgr->GetSpellInfo(s.clone))
                        handler->PSendSysMessage("  STALE STOCK SPELL ACTIVE: {}  <-- PROBLEM (TBC clone {} owns this; reconcile should strip)", s.stock, s.clone);
            }
        }
        else if (era == ERA_TBC && t->getClass() == CLASS_WARLOCK)
        {
            // TBC warlock diagnostics (Phase 8). The four-marker chain (EraTalents.cpp CLASS_WARLOCK
            // arm): 932930 shared pre-Wrath cast times / 932994 Vanilla stone gate / 948410 TBC stone
            // gate / 932939 WotLK stone gate. Readiness is spelled out inline exactly as the priest
            // and hunter blocks do it (EraTalents::EraReplacementSpellReady is file-static over there):
            // with TBC outside the EraHasTalentTrees allowlist, or 948410 not yet in spell_dbc, a TBC
            // warlock legitimately falls back to 932939 + the stock stones, so reporting that as a
            // PROBLEM would be a pure false positive on the committed configuration.
            handler->PSendSysMessage("  cast-time marker 932930: known={} aura={}{}",
                t->HasSpell(932930) ? "yes" : "NO", t->HasAura(932930) ? "yes" : "NO",
                (t->HasSpell(932930) && t->HasAura(932930)) ? "" : "  <-- PROBLEM (Corruption stays instant; reconcile gap)");

            bool markerReady = EraHasTalentTrees(ERA_TBC) && sSpellMgr->GetSpellInfo(948410) != nullptr;
            if (markerReady)
            {
                if (!t->HasSpell(948410))
                    handler->SendSysMessage("  TBC warlock without marker 948410  <-- PROBLEM (reconcile gap; TBC stone trainer rows unreachable)");
                for (uint32 wrong : { 932994u, 932939u })
                    if (t->HasSpell(wrong))
                        handler->PSendSysMessage("  WRONG-ERA MARKER HELD: {}  <-- PROBLEM (four-marker arm should have stripped it)", wrong);
                // Grant-change swaps (kEraWlTbcStockSwaps). KNOWN TRANSIENT, shared with the priest and
                // hunter blocks: in the half-authored window where the clone exists in spell_dbc but its
                // node still grants STOCK, reconcile correctly KEEPS the stock spell while this row
                // reports it stale. The gate is readiness, not ownership, on purpose.
                struct StaleStock { uint32 clone; uint32 stock; } const stale[] =
                    { { 948451, 18265 }, { 948650, 17962 }, { 948470, 30108 }, { 948670, 30283 },
                      { 948450, 18288 }, { 948570, 18708 }, { 948550, 19028 } };
                for (StaleStock const& sw : stale)
                    if (t->HasSpell(sw.stock) && sSpellMgr->GetSpellInfo(sw.clone))
                        handler->PSendSysMessage("  STALE STOCK SPELL ACTIVE: {}  <-- PROBLEM (TBC clone {} owns this; reconcile should strip)", sw.stock, sw.clone);
            }
        }
        else if (era == ERA_TBC && t->getClass() == CLASS_MAGE)
        {
            // TBC mage diagnostics (Phase 9). Readiness is spelled out inline exactly as the priest /
            // hunter / warlock blocks do it (EraTalents::EraReplacementSpellReady is file-static over
            // there and not exported): with TBC outside the EraHasTalentTrees allowlist, or the clone
            // absent from spell_dbc, a TBC mage legitimately keeps stock everything while playing the
            // NATIVE WotLK tree, so reporting that as a PROBLEM would be a false positive on exactly
            // the committed configuration. NB a scoped `if`, never an early `return` — the orphan
            // sweep and the aura dumps below are part of the same report.
            bool const mageReady = EraHasTalentTrees(ERA_TBC) && sSpellMgr->GetSpellInfo(948760) != nullptr;
            if (mageReady)
            {
                // Arcane Power / Presence of Mind: the TBC clones own these, so a TBC mage must not
                // hold the stock ids. KNOWN TRANSIENT, shared with the priest/hunter/warlock blocks:
                // in a half-authored window where the clone row exists but its node still grants
                // STOCK, reconcile correctly KEEPS the stock spell while this row reports it stale.
                struct StaleStock { uint32 clone; uint32 stock; } const stale[] =
                    { { 948760, 12042 }, { 948761, 12043 } };
                for (StaleStock const& s : stale)
                    if (t->HasSpell(s.stock) && sSpellMgr->GetSpellInfo(s.clone))
                        handler->PSendSysMessage("  STALE STOCK SPELL ACTIVE: {}  <-- PROBLEM (TBC clone {} owns this; reconcile should strip)", s.stock, s.clone);

                // The four TBC trained CLONE chains (band sweep rule 2 via spell_ranks + hand SQL
                // 2026_09_08_02). r1 is NODE-granted, r2+ are trainer-taught behind a ReqAbility1 chain
                // off r1, so the orphan sweep's generic NodesFor() fallback can only ever resolve r1.
                // Node-gated (the warlock chain-predicate shape): a mage who respecced OUT of the node
                // but still holds a rank is the lesson-21 strip gap these lines exist to catch — and it
                // shows up only AFTER a respec, never after a band move (which strips the whole band).
                struct ChainState { uint32 node; char const* name; uint32 r1; uint32 ranks[9]; };
                static const ChainState kMageChainsTbc[] =
                {
                    { 20830, "Pyroblast",       948870, { 948871, 948872, 948873, 948874, 948875, 948876, 948877, 948878, 948879 } },
                    { 20837, "Blast Wave",      948880, { 948881, 948882, 948883, 948884, 948885, 948886, 0, 0, 0 } },
                    { 20844, "Dragon's Breath", 948890, { 948891, 948892, 948893, 0, 0, 0, 0, 0, 0 } },
                    { 20863, "Ice Barrier",     948964, { 948965, 948966, 948967, 948968, 948969, 0, 0, 0, 0 } },
                };
                for (ChainState const& c : kMageChainsTbc)
                {
                    if (!sSpellMgr->GetSpellInfo(c.r1))
                        continue;
                    uint8 const rank = EraTalents::CurrentRank(t, ERA_TBC, c.node);
                    uint32 known = t->HasSpell(c.r1) ? 1 : 0;
                    uint32 top = t->HasSpell(c.r1) ? 1 : 0;
                    for (uint32 i = 0; i < 9; ++i)
                        if (c.ranks[i] && t->HasSpell(c.ranks[i]))
                        {
                            ++known;
                            top = i + 2;
                        }
                    handler->PSendSysMessage("  chain {} (node {}): talent rank {}, clone r1 {}, ranks known {} (highest r{})",
                        c.name, c.node, uint32(rank), t->HasSpell(c.r1) ? "known" : "MISSING", known, top);
                    if (rank != 0 && !t->HasSpell(c.r1))
                        handler->PSendSysMessage("    <-- PROBLEM: node {} is spent but clone r1 {} is NOT known (grant gap)", c.node, c.r1);
                    if (rank == 0 && known != 0)
                        handler->PSendSysMessage("    <-- PROBLEM: node {} is at rank 0 but {} clone rank(s) are still known (respec strip gap, lesson 21)", c.node, known);
                }

                // Shatter (node 20857): the node grants NOTHING — it AUTHORs five SPELL_AURA_DUMMY
                // auto-passives 942856-942860 carrying misc 1 / amount 10-50, which core patch 0018
                // reads in Unit::SpellTakenCritChance. The band it honours was widened to
                // [920000, 950000) in this phase precisely so these ids arm it.
                // EXPECTED COUNT IS 1 WHEN THE NODE IS SPENT AT ANY RANK, 0 OTHERWISE — not "one per
                // rank": a `mechanic: stat` node holds exactly ONE rankSpell, the current rank's
                // (EraTalents.cpp:256 grants rankSpell[newRank-1]; :282 removes the previous rank's
                // on every rank-up/reset), so a healthy 5/5 Shatter shows a single 942860 aura at
                // amount 50. Two auras means two ranks' passives are both applied and 0018 doubles
                // the bonus; zero with the node spent means the passive never self-applied.
                uint8 const shatterRank = EraTalents::CurrentRank(t, ERA_TBC, 20857);
                uint32 shatterAuras = 0;
                for (AuraEffect const* eff : t->GetAuraEffectsByType(SPELL_AURA_DUMMY))
                {
                    uint32 const id = eff->GetSpellInfo()->Id;
                    if (eff->GetMiscValue() == 1 && id >= 942856 && id <= 942860)
                        ++shatterAuras;
                }
                handler->PSendSysMessage("  Shatter: node 20857 rank {}, misc-1 auras held = {}{}",
                    uint32(shatterRank), shatterAuras,
                    (shatterAuras == (shatterRank ? 1u : 0u)) ? "" : "  <-- PROBLEM (a stat node holds ONE misc-1 rank passive while any rank is spent, none at rank 0)");
            }
        }

        // Orphaned era-band spells (Phase 9.5): ONE rule, shared with reconcile's Sweep — see
        // EraBandClassifier.h. Every non-legit held band id prints with the classifier's reason;
        // UNCLASSIFIED is tagged PROBLEM because on a clean build it means a band-allowlist.yaml
        // miss (era_audit check_band_ownership should have caught it), not a character defect.
        for (uint32 id : EraBandClassifier::HeldBandSpells(t))
        {
            EraBandClassifier::Result r = EraBandClassifier::Classify(t, era, id);
            if (EraBandClassifier::IsLegit(r.verdict))
                continue;
            handler->PSendSysMessage("  ORPHAN ERA SPELL: {} ({}){}", id, r.reason,
                r.verdict == EraBandClassifier::Verdict::UNCLASSIFIED
                    ? "  <-- PROBLEM (unclassified = allowlist miss)" : "");
        }

        // Live band-gated DUMMY markers on the player (`mechanic: scripted` talents). A learned
        // scripted talent whose marker is absent here means the passive's aura never self-applied.
        for (AuraEffect const* eff : t->GetAuraEffectsByType(SPELL_AURA_DUMMY))
        {
            uint32 id = eff->GetSpellInfo()->Id;
            if (id >= EraTalents::ERA_CUSTOM_BAND_LOW && id < EraTalents::ERA_CUSTOM_BAND_HIGH)
                handler->PSendSysMessage("  band DUMMY marker: spell {} misc {} amount {}",
                    id, eff->GetMiscValue(), eff->GetAmount());
        }

        // All era-band auras ON the player (not just DUMMY markers): catches script-cast carriers
        // like the Soul Link split (932901 — must be present with the PET as caster while 932900 is
        // up) and the Demonic Sacrifice benefit buffs (932903-6, amount0 = the Vanilla magnitude).
        for (auto const& kv : t->GetOwnedAuras())
        {
            uint32 id = kv.first;
            if (id < EraTalents::ERA_CUSTOM_BAND_LOW || id >= EraTalents::ERA_CUSTOM_BAND_HIGH)
                continue;
            Aura* aura = kv.second;
            int32 amt0 = aura->GetEffect(EFFECT_0) ? aura->GetEffect(EFFECT_0)->GetAmount() : 0;
            handler->PSendSysMessage("  band aura on player: {} amount0 {} caster {} applied={}",
                id, amt0, aura->GetCasterGUID().ToString(), t->HasAura(id) ? "yes" : "NO");
        }

        // Applied-but-NOT-owned band auras on the player. An area-aura application radiated from a
        // PET-owned aura — the Spirit Bond 119 carriers (932968/932969) and the Soul Link split
        // (932901, 143) — never appears in the player's GetOwnedAuras(): the aura OBJECT lives on
        // the pet (Aura::UpdateTargetMap pushes the owner as an extra APPLICATION target,
        // SpellAuras.cpp SPELL_EFFECT_APPLY_AREA_AURA_PET/_OWNER). Without this scan the doctor is
        // structurally blind to pet->owner radiation (found during the hunter Spirit Bond pass).
        for (auto const& kv : t->GetAppliedAuras())
        {
            uint32 id = kv.first;
            if (id < EraTalents::ERA_CUSTOM_BAND_LOW || id >= EraTalents::ERA_CUSTOM_BAND_HIGH)
                continue;
            if (t->GetOwnedAura(id))
                continue;   // player-owned — already printed above
            Aura* aura = kv.second->GetBase();
            int32 amt0 = aura->GetEffect(EFFECT_0) ? aura->GetEffect(EFFECT_0)->GetAmount() : 0;
            handler->PSendSysMessage("  band aura APPLIED on player (owned by caster): {} amount0 {} caster {}",
                id, amt0, aura->GetCasterGUID().ToString());
        }

        // patch-0021 AI bridge state. The bot AI keeps a handful of HARDCODED stock spell ids as
        // behavioural discriminators; on an era-managed character the native spell is gone and the
        // AI must reach the era clone through EraTalentBots_ResolveSpellId. A 0 here means the
        // discriminator is permanently FALSE for this character — which is invisible everywhere
        // else (HasAura(0) is just false), and is exactly how the druid Thick Hide bear
        // discriminator and the paladin Improved Blessing of Might/Wisdom checks stayed broken
        // through two phases: those talents ship as GENERATED auto-passives named
        // "<node> (Rank N)" while the resolver only accepted the verbatim stock name.
        // Only meaningful for a BOT (the bridge gates on that); shown as n/a for a player.
        {
            struct Discriminator { uint8 cls; uint32 stockId; char const* what; };
            static const Discriminator kDiscriminators[] = {
                { CLASS_DRUID,   16931, "Thick Hide r3 (bear discriminator)" },
                { CLASS_DRUID,   16864, "Omen of Clarity" },
                { CLASS_PALADIN, 20042, "Improved Blessing of Might r1" },
                { CLASS_PALADIN, 20244, "Improved Blessing of Wisdom r1" },
                { CLASS_PALADIN, 20911, "Blessing of Sanctuary" },
                { CLASS_PRIEST,  15286, "Vampiric Embrace" },
                // WARRIOR (TBC Phase 4 Task 9 follow-up): the two by-id sites patch 0021 now
                // routes through the bridge. Both ship as GENERATED auto-passives, so a 0 means
                // the AI check is permanently false — Battle Shout AP understated by up to 25%
                // (BattleShoutTrigger) / the axe-polearm gear preference lost (StatsWeightCalculator).
                // 0 is CORRECT only when the build did not take the node: Commanding Presence is
                // Fury tier 2 (Vanilla node 18526 / TBC 20330) and Poleaxe Spec is TBC-only
                // (Arms node 20311; Vanilla splits it into Axe Spec 18512 / Polearm Spec 18516,
                // so 0 is ALWAYS correct for a Vanilla warrior there). 12861 exercises the
                // era-rename ALIAS path in the resolver — a Vanilla warrior's clone is named
                // "Improved Battle Shout (Rank N)", not "Commanding Presence (Rank N)".
                { CLASS_WARRIOR, 12861, "Commanding Presence r5 (0 unless node 18526/20330 at r5)" },
                { CLASS_WARRIOR, 12785, "Poleaxe Specialization r5 (0 ALWAYS correct in Vanilla)" },
                // ROGUE (TBC Phase 5 Task 9b, 2026-09-03): the one rogue by-id site — patch
                // 0021's StatsWeightCalculator sword/axe gear preference. Ships as GENERATED
                // auto-passives (Vanilla 923440-444 / TBC 939480-484) under the era-authentic
                // name, so 13964 resolves ONLY through the "hack and slash" -> "sword
                // specialization" rename alias; a 0 on a MAXED node means that alias is gone.
                // 0 is correct whenever the build did not take Sword Spec to rank 5 (Combat
                // tier 4/5: Vanilla node 18430 / TBC node 20435) — the by-id check is a
                // rank-5-only test upstream too.
                { CLASS_ROGUE,   13964, "Sword Spec r5 / Hack and Slash (0 unless node 18430/20435 at r5)" },
                // SHAMAN (Plan 3b Task 8): the patch-0021 totem subsystem — the four no-totem
                // gates, the strategy discriminators, and the UNIT_CREATED_BY_SPELL identity
                // walks. A 0 is NOT automatically a defect here: Cleansing Totem 8170 is
                // WotLK-only (0 is CORRECT pre-WotLK — the strategy falls back to Mana Spring),
                // Wrath of Air 3738 is TBC L64+ (0 is CORRECT in Vanilla), and the talent-gated
                // ids (Totem of Wrath 30706 node 20219, Mana Tide 16190 node 20256) read 0
                // whenever the build skips the node. The four gates + Grounding must never be 0
                // on a totem-trained shaman of any era.
                { CLASS_SHAMAN, 8071,  "Stoneskin Totem r1 (no-earth-totem gate)" },
                { CLASS_SHAMAN, 3599,  "Searing Totem r1 (no-fire-totem gate)" },
                { CLASS_SHAMAN, 5394,  "Healing Stream Totem r1 (no-water-totem gate)" },
                { CLASS_SHAMAN, 10595, "Nature Resistance Totem r1 (no-air-totem gate)" },
                { CLASS_SHAMAN, 8177,  "Grounding Totem (stock every era — control row)" },
                { CLASS_SHAMAN, 8512,  "Windfury Totem r1 (air strategy pick)" },
                { CLASS_SHAMAN, 8227,  "Flametongue Totem r1 (ToW-strategy fallback)" },
                { CLASS_SHAMAN, 5675,  "Mana Spring Totem r1 (cleansing-strategy fallback)" },
                { CLASS_SHAMAN, 3738,  "Wrath of Air Totem (0 CORRECT in Vanilla; TBC L64+)" },
                { CLASS_SHAMAN, 8170,  "Cleansing Totem (0 CORRECT pre-WotLK)" },
                { CLASS_SHAMAN, 30706, "Totem of Wrath r1 (0 unless node 20219 taken)" },
                { CLASS_SHAMAN, 16190, "Mana Tide Totem (identity walk; 0 unless node 20256 taken)" },
                { CLASS_SHAMAN, 5730,  "Stoneclaw Totem r1 (identity walk)" },
                // HUNTER (TBC Phase 7 Task 8, 2026-09-05): RESOLVER SANITY rows, not patch-0021
                // sites. `grep -c hunter patches/0021-playerbot-era-ai.patch` is **0** — the hunter
                // AI has no hardcoded by-id checks on any of these three, so nothing is broken today
                // if they read 0. They are here because the Marksmanship tab hands a TBC hunter
                // three CLONE chains whose whole era-safety argument is that the bridge resolves
                // them BY NAME (the clones keep the stock names), and this is the only headless way
                // to see that argument hold — the same evidence a future 0021 hunter arm would need
                // before it could key on a stock id. 0 is CORRECT whenever the build did not take
                // the node (Aimed Shot 20627, Trueshot Aura 20637, Silencing Shot 20640 are all
                // optional Marksmanship picks) and on a Vanilla-band hunter for Silencing Shot,
                // which is TBC-only. A VANILLA-band hunter resolves Aimed Shot / Trueshot to the
                // VANILLA chains 932952-957 / 932961-963 instead — and to whichever rank of them the
                // character actually KNOWS, not necessarily rank 1: the bridge scans the spellbook by
                // NAME, so a bot that trained the chain reads back the top rank. Measured 2026-09-05
                // on a level-60 Izelzenn: `19434 -> 932957` (rank 6), not 932952.
                { CLASS_HUNTER, 19434, "Aimed Shot r1 (TBC 948370 / Vanilla 932952; 0 unless node 20627/18321 taken)" },
                { CLASS_HUNTER, 19506, "Trueshot Aura r1 (TBC 948377 / Vanilla 932961; 0 unless node 20637/18330 taken)" },
                { CLASS_HUNTER, 34490, "Silencing Shot (TBC 948383; 0 ALWAYS correct in Vanilla)" },
                // WARLOCK (TBC Phase 8 Task 6, 2026-09-05): RESOLVER SANITY rows, not patch-0021
                // sites — `grep -cE "17962|18265|30108|30283|19028|18288|18708" patches/0021-*.patch`
                // is 0, and the ONE hardcoded by-id warlock site in the AI (WrongPetTrigger's pet
                // table, WarlockTriggers.cpp:176, `{"felguard", 30146, 17252}`) keys on a spell BOTH
                // eras GRANT unchanged, so nothing is broken today if these read 0. They are here
                // because the warlock phase hands a managed character six CLONE/RECONSTRUCT chains
                // whose entire era-safety argument is that the bridge resolves them BY NAME (every
                // clone keeps the stock name — verified: 948450 "Amplify Curse", 948451 "Siphon
                // Life", 948470 "Unstable Affliction", 948550/551 "Soul Link", 948570 "Fel
                // Domination", 948650 "Conflagrate", 948670 "Shadowfury"), so no kEraNameAliases row
                // is needed and THIS is the only headless way to see that argument hold.
                // 0 is CORRECT whenever the build did not take the node, and ALWAYS correct for
                // Unstable Affliction / Shadowfury on a Vanilla-band warlock (both are TBC-only).
                // 18265 has NO live SpellInfo (WotLK deleted the spell when it folded Siphon
                // Life into Corruption; the ref records all six TBC ranks as WAGO_ONLY), so the
                // resolver returns 0 at its `GetSpellInfo(stockSpellId)` guard NO MATTER WHAT the
                // character knows — measured 2026-09-05 on a TBC warlock holding node 20713 r1 and
                // the clone 948451 ("known=yes"). Kept as a DOCUMENTED always-0 row: it is the
                // standing evidence that no by-id AI check can ever reach Siphon Life, so a future
                // 0021 arm must key on the era clone (or a name), never on 18265.
                { CLASS_WARLOCK, 18265, "Siphon Life r1 (TBC 948451 / Vanilla 932902 — 18265 has no live SpellInfo, so 0 is ALWAYS correct)" },
                { CLASS_WARLOCK, 18288, "Amplify Curse (TBC 948450 / Vanilla 932918; 0 unless node 20708/18209 taken)" },
                { CLASS_WARLOCK, 30108, "Unstable Affliction r1 (TBC 948470; 0 ALWAYS correct in Vanilla)" },
                { CLASS_WARLOCK, 19028, "Soul Link (TBC 948550 / Vanilla 932900; 0 unless node 20739/18233 taken)" },
                { CLASS_WARLOCK, 18708, "Fel Domination (TBC clone 948570; Vanilla GRANTS stock 18708)" },
                { CLASS_WARLOCK, 17962, "Conflagrate r1 (TBC 948650 / Vanilla 932780; 0 unless node 20760/18250 taken)" },
                { CLASS_WARLOCK, 30283, "Shadowfury r1 (TBC 948670; 0 ALWAYS correct in Vanilla)" },
                { CLASS_WARLOCK, 18220, "Dark Pact r1 (TBC GRANTS stock 18220 / Vanilla clone 932784; 0 unless node 20717/18217 taken)" },
                { CLASS_WARLOCK, 17877, "Shadowburn r1 (stock GRANT in BOTH eras — control row)" },
                { CLASS_WARLOCK, 30146, "Summon Felguard (stock GRANT; the one warlock by-id AI site, WarlockTriggers.cpp:176)" },
                // MAGE (TBC Phase 9): RESOLVER SANITY rows, same purpose as the warlock set above —
                // the whole era-safety argument for the mage tree's seven clone chains is that
                // EraTalentBots_ResolveSpellId finds them BY NAME (every clone keeps its stock
                // name), and these lines are the only headless way to see that argument hold. 0 is
                // CORRECT whenever the build did not take the node, and ALWAYS correct where the row
                // says so (a spell that does not exist in that era). Ids are the Task 5 ledger's.
                { CLASS_MAGE, 12042, "Arcane Power (TBC 948760 / Vanilla 932760; 0 unless node 20819/18016 taken)" },
                { CLASS_MAGE, 12043, "Presence of Mind (TBC 948761 / Vanilla 932761; 0 unless node 20813/18013 taken)" },
                { CLASS_MAGE, 29441, "Magic Absorption r1 (TBC 948762 / Vanilla 932326; 0 unless node 20804/18005 taken)" },
                { CLASS_MAGE, 11366, "Pyroblast r1 (TBC 948870 / Vanilla 932310; 0 unless node 20830/18024 taken)" },
                { CLASS_MAGE, 11113, "Blast Wave r1 (TBC 948880 / Vanilla 932320; 0 unless node 20837/18030 taken)" },
                { CLASS_MAGE, 31661, "Dragon's Breath r1 (TBC 948890; 0 ALWAYS correct in Vanilla)" },
                { CLASS_MAGE, 11426, "Ice Barrier r1 (TBC 948964 / Vanilla 932331; 0 unless node 20863/18049 taken)" },
                { CLASS_MAGE, 11129, "Combustion (TBC 948887 / Vanilla 932325; 0 unless node 20841/18032 taken)" },
                { CLASS_MAGE, 31589, "Slow (TBC clone 948773; 0 ALWAYS correct in Vanilla)" },
                { CLASS_MAGE, 12472, "Icy Veins (stock GRANT in TBC; 0 ALWAYS correct in Vanilla)" },
                { CLASS_MAGE, 11958, "Cold Snap (TBC stock GRANT; Vanilla -> 932335)" },
                { CLASS_MAGE, 31687, "Summon Water Elemental (stock GRANT in TBC; 0 ALWAYS correct in Vanilla)" },
                { CLASS_MAGE, 45438, "Ice Block (baseline at 30 in TBC and WotLK; Vanilla node 18046 grant)" },
                // Fix-round probe (2026-09-02): node 20205 swapped its grant from stock 16164 to
                // clone 947444. -> 16164 = the stale stock grant is STILL PRESENT (kStockShaSwapTbc
                // strip failed); -> 947444 = clean (resolver name-match); -> 0 = node not taken.
                { CLASS_SHAMAN, 16164, "Elemental Focus (16164 = STALE STOCK; 947444 = clean)" },
            };
            for (Discriminator const& d : kDiscriminators)
            {
                if (d.cls != t->getClass())
                    continue;
                uint32 got = EraTalentBots_ResolveSpellId(t, d.stockId);
                handler->PSendSysMessage("  AI resolve: {} ({}) -> {}", d.stockId, d.what,
                    got ? std::to_string(got) : std::string("0 (NOT KNOWN — AI check is always false)"));
            }
        }

        // spell_pet_auras chain state: how many PetAuras the player has REGISTERED (m_petAuras is
        // filled by Spell::EffectDummy at the pet-talent passive's learn-cast) and what each maps
        // for the current pet entry. Registered-but-no-pet-aura = the CastPetAura cast is failing;
        // zero registered with a learned pet talent = the EffectDummy registration never ran.
        {
            uint32 petEntry = t->GetPet() ? t->GetPet()->GetEntry() : 0;
            handler->PSendSysMessage("  m_petAuras registered: {}", uint32(t->m_petAuras.size()));
            for (PetAura const* pa : t->m_petAuras)
                handler->PSendSysMessage("    petAura -> buff {} for current pet entry {}",
                    pa->GetAura(petEntry), petEntry);
        }

        // Live pet section: stats + every era-band aura on the pet. The pet-buff spells a
        // `mechanic: pet` talent maps via spell_pet_auras live in the 928000+ sub-band; a learned
        // pet talent with a summoned demon and no matching aura here = the spell_pet_auras chain
        // is broken somewhere between learn-cast and Pet::CastPetAuras.
        if (Pet* pet = t->GetPet())
        {
            handler->PSendSysMessage("  pet: entry {} level {} hp {}/{} mana {}/{} int {} sta {}",
                pet->GetEntry(), uint32(pet->GetLevel()),
                pet->GetHealth(), pet->GetMaxHealth(),
                pet->GetPower(POWER_MANA), pet->GetMaxPower(POWER_MANA),
                uint32(pet->GetStat(STAT_INTELLECT)), uint32(pet->GetStat(STAT_STAMINA)));
            bool any = false;
            for (auto const& kv : pet->GetOwnedAuras())
            {
                uint32 id = kv.first;
                if (id < EraTalents::ERA_CUSTOM_BAND_LOW || id >= EraTalents::ERA_CUSTOM_BAND_HIGH)
                    continue;
                any = true;
                Aura* aura = kv.second;
                int32 amt0 = aura->GetEffect(EFFECT_0) ? aura->GetEffect(EFFECT_0)->GetAmount() : 0;
                // owned vs APPLIED matters: an owned aura with no application runs no handlers
                // (HasAura reads applications; GetAuraEffect reads applied effects).
                handler->PSendSysMessage("    pet band aura: {} amount0 {} applied={} appliedEff0={}",
                    id, amt0, pet->HasAura(id) ? "yes" : "NO",
                    pet->GetAuraEffect(id, EFFECT_0) ? "yes" : "NO");
            }
            if (!any)
                handler->SendSysMessage("    pet band aura: (none)");
        }
        else
            handler->SendSysMessage("  pet: (none summoned)");

        handler->SendSysMessage("== doctor done ==");
        return true;
    }
};

void AddSC_era_talents_commandscript() { new era_talents_commandscript(); }
