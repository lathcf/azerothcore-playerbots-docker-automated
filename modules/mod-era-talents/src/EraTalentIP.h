#ifndef MOD_ERA_TALENT_IP_H
#define MOD_ERA_TALENT_IP_H
#include "Define.h"
class Player;
enum EraId : uint8 { ERA_VANILLA = 0, ERA_TBC = 1, ERA_WOTLK = 2 };
EraId EraFromIP(Player* p);   // maps IP progression band -> EraId

// Implemented-era allowlist: does this era have authored custom talent trees?
// A character whose era is NOT in the allowlist is left on the native WotLK talent frame
// (stock talents intact, points unpinned) exactly like a WotLK character — this is the
// deliberate fallback for eras whose trees aren't built yet. Only Vanilla is implemented
// today; TBC characters therefore keep their WotLK talents until the TBC trees ship. When
// TBC (and eventually WotLK) content is authored + deployed, add its EraId here (one edit,
// the single source of truth for every "is this a managed era character" gate).
bool EraHasTalentTrees(EraId era);
// Manual expansion advance (spec 2026-09-07): the IP state surface the faction-leader gossip
// reads/writes. Lives here so EraTalentIP.cpp stays the ONLY TU that includes IP's header
// (its GENERAL enum clashes with mod-playerbots headers). Values are static_asserted against
// IP's own enum/achievement constants in EraTalentIP.cpp.
namespace EraIP
{
    constexpr uint8  kStateNaxx40   = 7;    // PROGRESSION_NAXX40   — Kel'Thuzad-40 down
    constexpr uint8  kStatePreTBC   = 8;    // PROGRESSION_PRE_TBC  — TBC unlocked
    constexpr uint8  kStateTbcTier4 = 12;   // PROGRESSION_TBC_TIER_4 — Sunwell open
    constexpr uint8  kStateTbcTier5 = 13;   // PROGRESSION_TBC_TIER_5 — WotLK unlocked
    constexpr uint32 kAchievementKilJaeden = 698;   // IP's KIL_JAEDEN_KILL
    uint8 State(Player* p);                     // GetPlayerProgressionFromQuests; 0 for null
    void  ForceState(Player* p, uint8 state);   // ForceUpdateProgressionState + checkIPPhasing (the .ip set pair)
}
#endif
