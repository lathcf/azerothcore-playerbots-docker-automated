#ifndef MOD_RAID_ROSTER_STORE_H
#define MOD_RAID_ROSTER_STORE_H
#include "ObjectGuid.h"
#include <vector>
#include <unordered_set>
#include <cstdint>

struct RaidRosterRow
{
    uint32 botGuid;
    uint8  cls;
    uint8  role;     // 0 tank, 1 heal, 2 dps
    uint8  specTab;
    uint8  slot;
};

namespace RaidRosterStore
{
    bool Exists(uint32 ownerGuid);
    std::vector<RaidRosterRow> Load(uint32 ownerGuid);                 // ordered by slot
    void Replace(uint32 ownerGuid, std::vector<RaidRosterRow> const&); // delete+insert (one roster)
    void Clear(uint32 ownerGuid);
    std::unordered_set<uint32> AllPinnedBots();                        // every bot_guid pinned by any owner
}
#endif
