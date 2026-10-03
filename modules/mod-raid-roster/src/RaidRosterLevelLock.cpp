#include "RaidRosterLevelLock.h"
#include "RaidRosterStore.h"
#include "Log.h"
#include <atomic>
#include <memory>
#include <unordered_set>

namespace
{
    using Snapshot = std::unordered_set<uint32>;

    // Never mutated after publication. Read with std::atomic_load, replaced with std::atomic_store
    // (C++17 free functions) — map threads only ever see a complete, immutable set.
    std::shared_ptr<Snapshot const> g_Locked = std::make_shared<Snapshot const>();
}

namespace RaidRosterLevelLock
{
    void LoadFromDB()
    {
        auto next = std::make_shared<Snapshot const>(RaidRosterStore::AllPinnedBots());
        std::size_t const n = next->size();
        std::atomic_store(&g_Locked, std::shared_ptr<Snapshot const>(std::move(next)));
        LOG_INFO("server.loading", "[RaidRoster] Level lock loaded for {} pinned bot(s).", n);
    }

    // Add/Remove update the snapshot directly instead of re-querying: RaidRosterStore's writes are
    // async, so a query right after one would race it.
    void Add(std::vector<uint32> const& botGuids)
    {
        if (botGuids.empty()) return;
        auto next = std::make_shared<Snapshot>(*std::atomic_load(&g_Locked));
        next->insert(botGuids.begin(), botGuids.end());
        std::atomic_store(&g_Locked, std::shared_ptr<Snapshot const>(std::move(next)));
    }

    void Remove(std::vector<uint32> const& botGuids)
    {
        if (botGuids.empty()) return;
        auto next = std::make_shared<Snapshot>(*std::atomic_load(&g_Locked));
        for (uint32 g : botGuids)
            next->erase(g);
        std::atomic_store(&g_Locked, std::shared_ptr<Snapshot const>(std::move(next)));
    }

    bool IsLocked(uint32 botGuidLow)
    {
        std::shared_ptr<Snapshot const> snap = std::atomic_load(&g_Locked);
        return snap && snap->count(botGuidLow) != 0;
    }
}
