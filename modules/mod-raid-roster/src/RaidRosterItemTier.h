#ifndef MOD_RAID_ROSTER_ITEM_TIER_H
#define MOD_RAID_ROSTER_ITEM_TIER_H

#include "Define.h"
#include <string>
#include <utility>
#include <vector>

// item entry -> IP tier (the ProgressionState at which the item becomes obtainable), from where
// it comes from: MIN over drop / chest / vendor / quest-reward sources, each source the MAX of its
// gates (raid map, IP conditions, quest chain...), level-band fallback when no source exists.
// Spec: docs/superpowers/specs/2026-10-01-raidroster-era-gear-and-level-lock-design.md.
// The .cpp is the only IP-including file besides RaidRosterEra.cpp (see RaidRosterEra.h for the
// GENERAL enumerator clash); this header is IP- and playerbots-free on purpose.
namespace RaidRosterItemTier
{
    // (entry, equipCacheNew key level) for every curated gear-pool entry. World thread, once
    // (called from RaidRosterGear's GearIndex builder); later calls are no-ops. Immutable after.
    void Build(std::vector<std::pair<uint32, uint8>> const& entries);

    // Tier of a pool entry; a non-pool entry gets the level-band fallback from its template.
    uint8 TierOf(uint32 entry);

    // Human-readable breakdown for `.raidroster itemtier <id>`.
    std::vector<std::string> Explain(uint32 entry);

    // Review sweep for `.raidroster tiersweep`: rare+ pool items with ilvl >= minIlvl,
    // RequiredLevel/key <= 60 and tier <= maxTier whose verdict came from a quest or the
    // fallback (the classes the data can mis-tier). Sorted by ilvl desc, capped at 200 lines.
    std::vector<std::string> Sweep(uint8 maxTier, uint32 minIlvl);
}

#endif // MOD_RAID_ROSTER_ITEM_TIER_H
