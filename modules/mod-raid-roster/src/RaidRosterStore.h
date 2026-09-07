#ifndef MOD_RAID_ROSTER_STORE_H
#define MOD_RAID_ROSTER_STORE_H
#include "ObjectGuid.h"
#include <vector>
#include <unordered_set>
#include <utility>
#include <cstdint>

struct RaidRosterRow
{
    uint32 botGuid;
    uint8  cls;
    uint8  role;     // 0 tank, 1 heal, 2 dps
    uint8  specTab;
    uint8  slot;
    uint8  band;     // RaidCompBand (RaidRosterComp.h): 0 any, 1 WotLK-only, 2 pre-WotLK-only
};

namespace RaidRosterStore
{
    bool Exists(uint32 ownerGuid);
    std::vector<RaidRosterRow> Load(uint32 ownerGuid);                 // ordered by slot
    void Replace(uint32 ownerGuid, std::vector<RaidRosterRow> const&); // delete+insert (one roster)
    void Append(uint32 ownerGuid, std::vector<RaidRosterRow> const&);  // insert only (top-up)
    // (slot_index, band) pairs -> UPDATE band for those slots of one owner, one transaction.
    void UpdateBands(uint32 ownerGuid, std::vector<std::pair<uint8, uint8>> const&);
    void Clear(uint32 ownerGuid);
    std::unordered_set<uint32> AllPinnedBots();                        // every bot_guid pinned by any owner
}
#endif
