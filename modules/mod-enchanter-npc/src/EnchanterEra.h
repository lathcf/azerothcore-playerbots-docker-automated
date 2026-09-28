#ifndef MOD_ENCHANTER_ERA_H
#define MOD_ENCHANTER_ERA_H

#include "Define.h"

class Player;

// Isolated translation unit for every mod-individual-progression / mod-era-talents call:
// IndividualProgression.h defines global unscoped enumerators that collide with other headers
// (the same reason RaidRosterEra.h exists). Nothing else in this module includes it.
namespace EnchanterEra
{
    // EnchanterRules::Era of a real player (EraTalentBots::EraFor); ERA_WOTLK when IP is disabled.
    uint8 PlayerEra(Player* player);

    // tier 0 or IP disabled -> true; else hasPassedProgression(player, tier).
    bool TierUnlocked(Player* player, uint8 tier);

    // Raid map -> the ProgressionState at which that raid opens; 0 = always open.
    uint8 TierForMap(uint32 mapId);
}

#endif
