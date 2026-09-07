#include "EraTransition.h"
#include "EraTalents.h"
#include "EraTalentsComms.h"
#include "EraTalentsConfig.h"
#include "EraGlyphGate.h"
#include "ScriptMgr.h"
#include "Player.h"
#include "DatabaseEnv.h"
#include "QueryResult.h"
#include "Field.h"
#include "Log.h"
#include <mutex>
#include <unordered_map>

namespace
{
    std::unordered_map<ObjectGuid, EraId> g_pending;   // deferred target era while in combat
    std::mutex g_pendingMutex;   // Detect/FlushCombat run per-player inside Map::Update — maps
                                 // tick in parallel, so this shared map needs the same locking
                                 // as the era caches (prod rehash-race segfault, 2026-08-26)
}

namespace EraTransition
{
    EraId StoredEra(Player* p)
    {
        if (QueryResult r = CharacterDatabase.Query(
                "SELECT lastEra FROM era_talent_char_state WHERE guid={}", p->GetGUID().GetCounter()))
            return EraId((*r)[0].Get<uint8>());
        return EraFromIP(p);   // first sighting: adopt current era, no reset
    }

    void SetStoredEra(Player* p, EraId era)
    {
        CharacterDatabase.DirectExecute(
            "REPLACE INTO era_talent_char_state (guid,lastEra) VALUES ({},{})",
            p->GetGUID().GetCounter(), uint8(era));
    }

    static void Pin(Player* p, EraId era)
    {
        if (EraHasTalentTrees(era))
        {
            p->SetFreeTalentPoints(0);
            p->SendTalentsInfoData(false);
        }
    }

    // Core resetTalents(true) only wipes the ACTIVE spec (it skips any talent whose specMask lacks
    // GetActiveSpecMask()). A WotLK dual-spec character down-transitioned into a managed era therefore
    // keeps stock talents in its INACTIVE spec — and a later spec switch makes m_usedTalentCount
    // nonzero, so a trainer reset would then run BOTH the pin hook's charge AND the core's (double
    // charge / era-reset-paid-but-native-reset-refused), and the inactive spec would hand an era
    // character live WotLK talents. Reset EVERY spec so no native talent survives in either.
    //
    // The dual-spec STRUCTURE is deliberately preserved (specsCount unchanged): a managed era hides
    // and pins the native talent frame, so the second spec is merely dormant plumbing, and an
    // up-transition back to WotLK restores both specs to re-spend (this doesn't strip a thing IP
    // otherwise permits — IP does not gate dual spec by era). ActivateSpec is the battle-tested
    // per-spec teardown/apply path (the dual-spec trainer's own flow); Run() only reaches this branch
    // out of combat (it defers in-combat), which is exactly ActivateSpec's precondition. A single-spec
    // character takes the same path resetTalents(true) always did.
    static void ResetAllSpecs(Player* p)
    {
        uint8 const specs = p->GetSpecsCount();
        if (specs <= 1)
        {
            p->resetTalents(true);
            return;
        }

        uint8 const original = p->GetActiveSpec();
        for (uint8 s = 0; s < specs; ++s)
        {
            if (p->GetActiveSpec() != s)
                p->ActivateSpec(s);
            p->resetTalents(true);   // clears the now-active spec; era re-entry into the pin hook stays inert
        }
        if (p->GetActiveSpec() != original)
            p->ActivateSpec(original);   // leave them on the spec they had active (both are empty now)
    }

    // Heals characters that PREDATE the mod. StoredEra()'s first-sighting fallback adopts the
    // current era with no reset, so a character that leveled with native WotLK talents and was
    // ALREADY inside a managed era when the mod first deployed got stamped without ever
    // crossing — Run()'s down-transition reset never fires for it, and its stock talents stay
    // live under the era trees (a Vanilla priest still proccing Body and Soul). Called from the
    // same-era login path (bot-gated by the caller, and login is never in combat); inert for any
    // character with no live core talent — every normal era char — so it doubles as a permanent
    // backstop against any future path that leaks native talents into a managed era.
    void StripNativeTalents(Player* p, EraId era)
    {
        if (!p || !sEraTalentsConfig->Enabled() || !EraHasTalentTrees(era))
            return;

        bool leaked = false;
        for (auto const& [spellId, talent] : p->GetTalentMap())
            if (talent->State != PLAYERSPELL_REMOVED)
            {
                leaked = true;
                break;
            }
        if (!leaked)
            return;

        ResetAllSpecs(p);   // every spec, same as the down-transition path (see ResetAllSpecs)
        Pin(p, era);
        LOG_INFO("module", "[mod-era-talents] stripped leaked native talents from {} (pre-mod character in a managed era)",
                 p->GetName());
    }

    void Run(Player* p, EraId from, EraId to)
    {
        if (!p || !sEraTalentsConfig->Enabled())
            return;

        if (p->IsInCombat())
        {
            std::lock_guard<std::mutex> guard(g_pendingMutex);
            g_pending[p->GetGUID()] = to;   // don't mutate spellbook mid-fight; flush on leave-combat
            return;
        }

        // Mutate talents ONLY on an actual era crossing. A same-era call (every login, and the
        // first sighting of a new char) must NOT reset — the caller's pin()/ReapplyOnLogin do the
        // idempotent re-grant. Without this guard a plain login would wipe a WotLK main's tree
        // (resetTalents) or delete an era char's spent talents (Reset's DELETE).
        if (from != to)
        {
            bool const fromManaged = EraHasTalentTrees(from);
            bool const toManaged   = EraHasTalentTrees(to);

            // Full reset of the era we are LEAVING: strip its granted passives + delete its rows.
            // Only a managed (tree-bearing) era has anything to strip — an unimplemented era (TBC
            // today) never granted passives, so skip it.
            if (fromManaged)
                EraTalents::Reset(p, from);

            if (toManaged)
            {
                // Entering a managed era: clear any stock WotLK talents so the era char starts
                // clean, in EVERY spec (a dual-spec char keeps talents in the inactive one otherwise
                // — see ResetAllSpecs). Points are then pinned to 0 by Pin() + the deferred-zero PlayerScript.
                ResetAllSpecs(p);
                Pin(p, to);   // deferred-zero PlayerScript keeps it pinned thereafter
            }
            else if (fromManaged)
            {
                // Leaving a managed era for a native one (WotLK, or an era whose trees aren't built
                // yet like TBC): restore the native frame + level-appropriate WotLK pool. resetTalents
                // only REFUNDS spent core talents — an era char has none (its talents were era
                // passives), and its core pool was pinned to 0, so refunding leaves 0.
                // InitTalentForLevel() re-grants the pool (points stick: the target era is not
                // managed, so the pin hook's OnPlayerFreeTalentPointsChanged bails instead of zeroing).
                p->resetTalents(true);
                p->InitTalentForLevel();
                p->SendTalentsInfoData(false);
            }
            // else: native -> native (e.g. TBC <-> WotLK — neither has custom trees). Both already
            // use Blizzard's talent system, so do NOT touch talents — the player keeps the WotLK
            // spec they built. (Once TBC trees ship, TBC becomes managed and this path won't apply.)

            // Tell the addon the era flipped so it swaps panels live (no relog). Safe on login
            // too — the addon just re-parses; on a real crossing this is what updates isEraChar.
            EraTalentsComms::SendSync(p);

            // Reconcile baseline-vs-talent spells (e.g. Holy Nova) for the era being entered:
            // a down-transition strips the WotLK/TBC baseline grant, an up-transition grants it.
            EraTalents::ReconcileBaselineSpells(p, to);

            if (sEraTalentsConfig->Debug())
                LOG_INFO("module", "[mod-era-talents] transition {}->{} for {}", uint8(from), uint8(to), p->GetName());
        }

        // Persist the era stamp in BOTH cases (crossing AND same-era). This is what lets a LATER
        // crossing be detected: StoredEra() falls back to the live IP era when no row exists, so
        // if we never persisted, stored would always equal now and a real crossing would be missed.
        SetStoredEra(p, to);

        // Glyphs are WotLK-only: entering (or being reset into) a pre-WotLK era strips
        // them; entering WotLK is a no-op (StripIfDisallowed checks Allowed internally).
        EraGlyphGate::StripIfDisallowed(p);
    }

    void Detect(Player* p)
    {
        if (!p || !sEraTalentsConfig->Enabled())
            return;
        EraId stored = StoredEra(p);
        EraId now = EraFromIP(p);
        if (stored != now)
            Run(p, stored, now);
    }

    void FlushCombat(Player* p)
    {
        EraId to;
        {
            std::lock_guard<std::mutex> guard(g_pendingMutex);
            auto it = g_pending.find(p->GetGUID());
            if (it == g_pending.end())
                return;
            to = it->second;
            g_pending.erase(it);
        }
        Run(p, StoredEra(p), to);   // outside the lock: Run mutates the spellbook + re-reads ranks
    }
}
