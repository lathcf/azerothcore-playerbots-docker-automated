#include "EraTalentIP.h"
#include "IndividualProgression.h"   // sIndividualProgression, ProgressionState
#include "Player.h"

EraId EraFromIP(Player* p)
{
    if (!p)
        return ERA_WOTLK;
    uint8 state = sIndividualProgression->GetPlayerProgressionFromQuests(p);
    if (state <= PROGRESSION_NAXX40)        // 0..7
        return ERA_VANILLA;
    if (state <= PROGRESSION_TBC_TIER_4)    // 8..12
        return ERA_TBC;
    return ERA_WOTLK;                        // 13+
}

bool EraHasTalentTrees(EraId era)
{
    // Vanilla + TBC trees are authored and shipped (all nine TBC classes real-client signed off
    // 2026-09-05). Add ERA_WOTLK only if WotLK trees are ever authored — until then a WotLK
    // character keeps the native talent frame; see the header for why.
    return era == ERA_VANILLA || era == ERA_TBC;
}

static_assert(EraIP::kStateNaxx40   == PROGRESSION_NAXX40,     "EraIP state constant drifted from IP");
static_assert(EraIP::kStatePreTBC   == PROGRESSION_PRE_TBC,    "EraIP state constant drifted from IP");
static_assert(EraIP::kStateTbcTier4 == PROGRESSION_TBC_TIER_4, "EraIP state constant drifted from IP");
static_assert(EraIP::kStateTbcTier5 == PROGRESSION_TBC_TIER_5, "EraIP state constant drifted from IP");
static_assert(EraIP::kAchievementKilJaeden == KIL_JAEDEN_KILL, "EraIP achievement constant drifted from IP");

uint8 EraIP::State(Player* p)
{
    if (!p || !p->IsInWorld())
        return 0;
    return sIndividualProgression->GetPlayerProgressionFromQuests(p);
}

void EraIP::ForceState(Player* p, uint8 state)
{
    if (!p || !p->IsInWorld())
        return;
    // Same pair cs_individualProgression's `.ip set` runs: the boss-kill path never re-phases,
    // so without checkIPPhasing the player keeps the old zone phase until the next zone change.
    IndividualProgression::ForceUpdateProgressionState(p, static_cast<ProgressionState>(state));
    sIndividualProgression->checkIPPhasing(p, p->GetAreaId());
}
