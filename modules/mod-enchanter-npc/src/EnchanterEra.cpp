#include "EnchanterEra.h"
#include "EnchanterRules.h"
#include "EraTalentBots.h"
#include "IndividualProgression.h"
#include "Player.h"

uint8 EnchanterEra::PlayerEra(Player* player)
{
    // EraFromIP (inside EraFor) has no enabled guard: with IP off every human reads tier 0 = Vanilla.
    if (!sIndividualProgression->enabled)
        return EnchanterRules::ERA_WOTLK;
    return uint8(EraTalentBots::EraFor(player));
}

bool EnchanterEra::TierUnlocked(Player* player, uint8 tier)
{
    if (!tier || !sIndividualProgression->enabled)
        return true;
    return sIndividualProgression->hasPassedProgression(player, ProgressionState(tier));
}

uint8 EnchanterEra::TierForMap(uint32 mapId)
{
    switch (mapId)
    {
        case 469: return PROGRESSION_MOLTEN_CORE;                                    // Blackwing Lair
        case 309: return uint8(sIndividualProgression->RequiredZulGurubProgression); // Zul'Gurub
        case 509:                                                                    // Ruins of Ahn'Qiraj
        case 531: return PROGRESSION_PRE_AQ;                                         // Temple of Ahn'Qiraj
        case 533: return PROGRESSION_AQ;          // Naxxramas (the WotLK copy's loot is era-gated anyway)
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
        case 530: return PROGRESSION_PRE_TBC;     // Outland: opens with the TBC era; stops Outland re-issues of
                                                  // Vanilla raid formulas (Threat/Subtlety) from opening them early
        case 571: return PROGRESSION_TBC_TIER_5;  // Northrend: opens with the WotLK era
        default:  return 0;                       // Azeroth open world, dungeons, Molten Core, Onyxia
    }
}
