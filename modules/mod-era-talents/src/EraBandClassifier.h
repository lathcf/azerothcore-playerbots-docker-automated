#pragma once
// Phase 9.5 (2026-09-05): ONE shared rule for "may this character legitimately KNOW this era-band
// spell?" — used by ReconcileBaselineSpells (Sweep, replacing StripOrphanedGrants + every per-class
// cross-era strip list) and by `.eratalents doctor` / `.eratalents bandsweep`, so reconcile and the
// doctor can never disagree. Three legitimacy sources, in order (spec §2 + Amendment A.1):
//   1. node grant  — a reverse index (band id -> owners across EVERY era/class) built once at
//                    startup; legit iff an owner has the CURRENT era, the character's class, and
//                    CurrentRank(node) == owner.rank.
//   2. chain member — sSpellMgr->GetFirstSpellInChain(id) is a band id that is legit by rule 1
//                    (trained ranks above a node-granted r1).
//   3. allowlist   — era-data/band-allowlist.yaml via the generated EraBandAllowlist.gen.h (markers,
//                    trainer-rooted chains, partners, level grants, proc passives), matched on era,
//                    class and an optional gating node.
// Legit from ANY source wins; otherwise the orphan verdict is chosen by priority
// ORPHAN_NODE > ORPHAN_CHAIN > ORPHAN_ALLOWLIST > UNCLASSIFIED. Sweep strips every non-legit id.
// era_audit.py check_band_ownership guarantees the allowlist is complete, which is what makes
// stripping UNCLASSIFIED safe.
#include "Define.h"
#include "EraTalentIP.h"
#include <functional>
#include <string>
#include <vector>

class Player;

namespace EraBandClassifier
{
    enum class Verdict : uint8
    {
        LEGIT_NODE = 0, LEGIT_CHAIN, LEGIT_ALLOWLIST,
        ORPHAN_NODE, ORPHAN_CHAIN, ORPHAN_ALLOWLIST, UNCLASSIFIED
    };
    struct Result { Verdict verdict; std::string reason; };

    inline bool IsLegit(Verdict v) { return v <= Verdict::LEGIT_ALLOWLIST; }
    char const* VerdictName(Verdict v);

    // Call ONCE after sEraTalentContent->Load(). Read-only afterwards (safe on map threads).
    void BuildIndex();
    size_t IndexedIds();          // boot canary
    size_t AllowlistEntries();    // boot canary
    char const* AllowlistHash();  // boot canary

    // spellId must be in [ERA_CUSTOM_BAND_LOW, ERA_CUSTOM_BAND_HIGH). ASSERTs (core Errors.h, live in every build) if BuildIndex never ran.
    Result Classify(Player* p, EraId era, uint32 spellId);

    // Every band id the player currently HasSpell(), ascending. Snapshot (safe to strip while iterating).
    std::vector<uint32> HeldBandSpells(Player* p);

    // Strip every non-legit held band spell; logs one line per strip (INFO for ORPHAN_*, WARN for
    // UNCLASSIFIED); calls onStrip (if given) per strip; returns the count.
    using StripCallback = std::function<void(uint32 spellId, Result const&)>;
    uint32 Sweep(Player* p, EraId era, StripCallback const& onStrip = nullptr);
}
