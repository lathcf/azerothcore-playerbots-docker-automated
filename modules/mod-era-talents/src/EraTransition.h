#ifndef MOD_ERA_TRANSITION_H
#define MOD_ERA_TRANSITION_H
#include "EraTalentIP.h"
class Player;
namespace EraTransition
{
    void Run(Player* p, EraId from, EraId to);   // full reset of `from`, pin/unpin for `to`
    void Detect(Player* p);                      // compare stored lastEra vs EraFromIP, Run if changed
    void FlushCombat(Player* p);                 // apply a deferred transition on leave-combat
    EraId StoredEra(Player* p);                  // era_talent_char_state.lastEra (default = EraFromIP)
    void  SetStoredEra(Player* p, EraId era);
    void StripNativeTalents(Player* p, EraId era); // same-era login backstop: wipe leaked core talents
}
#endif
