#include "PBChatterAreaBackfill.h"
#include "PBChatterConfig.h"
#include "Define.h"
#include "DatabaseEnv.h"
#include "QueryResult.h"
#include "Field.h"
#include "MapMgr.h"
#include "DBCStores.h"
#include "World.h"
#include "Log.h"
#include "StringFormat.h"
#include <set>
#include <vector>

namespace
{
    struct Spawn { uint32 entry; uint16 map; float x, y, z; };

    std::vector<Spawn> g_work;
    size_t g_cursor = 0;
    bool   g_active = false;
    uint32 g_done = 0;     // rows written this run
    uint32 g_total = 0;    // work-list size this run

    constexpr uint32 kBatchPerTick = 20;   // keep each tick short; run off-peak
    constexpr uint32 kProgressEvery = 2000; // log a progress line roughly every N entries

    std::string AreaName(uint32 id)
    {
        if (!id)
            return "";
        if (AreaTableEntry const* a = sAreaTableStore.LookupEntry(id))
            if (char const* nm = a->area_name[sWorld->GetDefaultDbcLocale()])
                return nm;
        return "";
    }
}

std::string PBChatterAreaBackfill::Start()
{
    if (g_active)
        return "Backfill already running.";

    g_work.clear();
    g_cursor = 0;
    g_done = 0;

    // One representative spawn per creature entry that isn't already resolved.
    // Dedup by entry in C++ (first spawn wins) to avoid ONLY_FULL_GROUP_BY issues.
    QueryResult done = WorldDatabase.Query("SELECT creature_entry FROM mod_chatter_npc_area");
    std::set<uint32> seen;
    if (done)
        do { seen.insert((*done)[0].Get<uint32>()); } while (done->NextRow());

    QueryResult res = WorldDatabase.Query(
        "SELECT id, map, position_x, position_y, position_z FROM creature ORDER BY id");
    if (res)
    {
        do
        {
            Field* f = res->Fetch();
            uint32 entry = f[0].Get<uint32>();
            if (entry == 0 || seen.count(entry))
                continue;
            seen.insert(entry); // first spawn per entry only
            Spawn s;
            s.entry = entry;
            s.map   = f[1].Get<uint16>();
            s.x     = f[2].Get<float>();
            s.y     = f[3].Get<float>();
            s.z     = f[4].Get<float>();
            g_work.push_back(s);
        } while (res->NextRow());
    }

    g_total = (uint32)g_work.size();
    g_active = g_total > 0;
    if (g_active) // quiet on startup when the table is already full
        LOG_INFO("server.loading", "[PlayerbotChatter] Area backfill started: {} entries to resolve.", g_total);
    return Acore::StringFormat("Area backfill started: {} creature entries to resolve (batch {}/tick).",
                               g_total, kBatchPerTick);
}

void PBChatterAreaBackfill::Tick(uint32_t /*diff*/)
{
    if (!g_active)
        return;

    uint32 n = 0;
    for (; g_cursor < g_work.size() && n < kBatchPerTick; ++g_cursor, ++n)
    {
        Spawn const& s = g_work[g_cursor];
        uint32 zoneId = 0, areaId = 0;
        sMapMgr->GetZoneAndAreaId(PHASEMASK_NORMAL, zoneId, areaId, s.map, s.x, s.y, s.z);
        std::string area = AreaName(areaId);
        std::string zone = AreaName(zoneId);
        if (area.empty() && zone.empty())
            continue; // unresolved (e.g. instanced map with no live instance) -> leave for fallback
        WorldDatabase.EscapeString(area);
        WorldDatabase.EscapeString(zone);
        // Fire-and-forget async write: the DB worker pool absorbs the <=20/tick batch,
        // and the backfill is meant to run off-peak, so we don't wait on each INSERT.
        WorldDatabase.Execute(Acore::StringFormat(
            "INSERT IGNORE INTO mod_chatter_npc_area (creature_entry, area_name, zone_name) "
            "VALUES ({}, '{}', '{}')", s.entry, area, zone));
        ++g_done;
    }

    if (g_cursor % kProgressEvery < kBatchPerTick) // periodic progress
        LOG_INFO("server.loading", "[PlayerbotChatter] Area backfill: {}/{} processed, {} written.",
                 (uint32)g_cursor, g_total, g_done);

    if (g_cursor >= g_work.size())
    {
        g_active = false;
        LOG_INFO("server.loading", "[PlayerbotChatter] Area backfill complete: {} written from {} entries.",
                 g_done, g_total);
        g_work.clear();
    }
}

std::string PBChatterAreaBackfill::StatusString()
{
    if (g_active)
        return Acore::StringFormat("Area backfill running: {}/{} processed, {} written.",
                                   (uint32)g_cursor, g_total, g_done);
    if (g_total > 0) // a run has completed this session — report its totals
        return Acore::StringFormat("Area backfill idle (last run wrote {} of {} entries).",
                                   g_done, g_total);
    return "Area backfill idle.";
}
