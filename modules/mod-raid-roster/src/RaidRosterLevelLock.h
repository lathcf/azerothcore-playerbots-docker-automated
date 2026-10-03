#ifndef MOD_RAID_ROSTER_LEVEL_LOCK_H
#define MOD_RAID_ROSTER_LEVEL_LOCK_H
#include "Define.h"
#include <vector>

// Pinned roster bots never gain XP: their level changes only through `.raidroster sync`, which
// levels via PlayerbotFactory/GiveLevel, not GiveXP. Enforced by the OnPlayerGiveXP hook in
// RaidRosterLoader.cpp. (A no-XP-gain player-flag lock does not survive: mod-playerbots'
// OnBotLogin removes that flag on every random-account bot login when randomBotFixedLevel=0.)
//
// Storage is an immutable snapshot swapped copy-on-write: writers run on the world thread only,
// IsLocked may be called from any map thread.
namespace RaidRosterLevelLock
{
    void LoadFromDB();                                // world thread, startup
    void Add(std::vector<uint32> const& botGuids);    // world thread, after create/top-up pins rows
    void Remove(std::vector<uint32> const& botGuids); // world thread, after remove
    bool IsLocked(uint32 botGuidLow);                 // any thread
}
#endif
