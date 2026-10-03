#include "PBChatterAreaBackfill.h"
#include "PBChatterConfig.h"
#include "Define.h"
#include "DatabaseEnv.h"
#include "QueryResult.h"
#include "Field.h"
#include "MapMgr.h"
#include "DBCStores.h"
#include "ObjectMgr.h"
#include "CreatureData.h"
#include "World.h"
#include "Log.h"
#include "StringFormat.h"
#include <set>
#include <vector>

namespace
{
    struct Spawn { uint32 entry; uint16 map; float x, y, z; };

    std::vector<Spawn>  g_work;       // new entries: area + zone + team -> INSERT
    std::vector<uint32> g_teamWork;   // existing rows with team IS NULL -> UPDATE team only
    size_t g_cursor = 0;
    size_t g_teamCursor = 0;
    bool   g_active = false;
    bool   g_hasTeamCol = false; // mod_chatter_npc_area.team present (SQL update applied)
    uint32 g_done = 0;     // rows written this run (inserts)
    uint32 g_teamDone = 0; // team values filled on existing rows this run
    uint32 g_total = 0;    // work-list size this run (inserts)
    uint32 g_teamTotal = 0;

    constexpr uint32 kBatchPerTick = 20;     // keep each tick short; run off-peak
    constexpr uint32 kTeamBatchPerTick = 200; // team-only fills: DBC lookups, <=4 UPDATEs/tick
    constexpr uint32 kProgressEvery = 2000;  // log a progress line roughly every N entries

    // mod_chatter_npc_area.team values (see data/sql/world/base/*_team.sql).
    constexpr uint8 kTeamAlliance = 0, kTeamHorde = 1, kTeamBoth = 2, kTeamNeither = 3;
    constexpr uint32 kAlliancePlayerTemplate = 1; // FactionTemplate.dbc: Human player
    constexpr uint32 kHordePlayerTemplate    = 2; // FactionTemplate.dbc: Orc player

    // Which player faction can USE this creature entry: usable-by-X means the NPC's faction
    // template is NOT hostile to X's player template (FactionTemplateEntry::IsHostileTo — the
    // template-level half of Unit::GetFactionReactionTo). Unknown template -> kTeamBoth: the
    // sidecar fails open on unknown, and writing a value keeps the row out of the next run.
    uint8 TeamFor(uint32 entry)
    {
        static FactionTemplateEntry const* alliance = sFactionTemplateStore.LookupEntry(kAlliancePlayerTemplate);
        static FactionTemplateEntry const* horde    = sFactionTemplateStore.LookupEntry(kHordePlayerTemplate);
        CreatureTemplate const* ct = sObjectMgr->GetCreatureTemplate(entry);
        FactionTemplateEntry const* ft = ct ? sFactionTemplateStore.LookupEntry(ct->faction) : nullptr;
        if (!ft || !alliance || !horde)
            return kTeamBoth;
        bool forAlliance = !ft->IsHostileTo(*alliance);
        bool forHorde    = !ft->IsHostileTo(*horde);
        if (forAlliance && forHorde)
            return kTeamBoth;
        if (forAlliance)
            return kTeamAlliance;
        if (forHorde)
            return kTeamHorde;
        return kTeamNeither;
    }

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
    g_teamWork.clear();
    g_cursor = 0;
    g_teamCursor = 0;
    g_done = 0;
    g_teamDone = 0;

    // The team column arrives with a later SQL update; probe for it so a DB that hasn't run
    // that update keeps the old area-only behavior (a query naming a missing column aborts).
    g_hasTeamCol = false;
    if (QueryResult tc = WorldDatabase.Query(
            "SELECT COUNT(1) FROM information_schema.COLUMNS "
            "WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'mod_chatter_npc_area' "
            "AND COLUMN_NAME = 'team'"))
        g_hasTeamCol = (*tc)[0].Get<uint64>() > 0;

    // One representative spawn per creature entry that isn't already resolved.
    // Dedup by entry in C++ (first spawn wins) to avoid ONLY_FULL_GROUP_BY issues.
    // Rows whose team is still NULL count as unresolved for the team fill only.
    QueryResult done = WorldDatabase.Query(g_hasTeamCol
        ? "SELECT creature_entry, (team IS NULL) FROM mod_chatter_npc_area"
        : "SELECT creature_entry, 0 FROM mod_chatter_npc_area");
    std::set<uint32> seen;
    if (done)
        do
        {
            uint32 entry = (*done)[0].Get<uint32>();
            seen.insert(entry);
            if (g_hasTeamCol && (*done)[1].Get<int64>() != 0)
                g_teamWork.push_back(entry);
        } while (done->NextRow());

    // The creature-entry column was renamed across AzerothCore versions: older schemas
    // use `id`, newer (multi-spawn) schemas use `id1`. Detect it at runtime so this query
    // works on both — a hardcoded name aborts the world thread with [1054] on the other.
    std::string idCol = "id1";
    if (QueryResult colq = WorldDatabase.Query(
            "SELECT COLUMN_NAME FROM information_schema.COLUMNS "
            "WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'creature' "
            "AND COLUMN_NAME IN ('id', 'id1') ORDER BY COLUMN_NAME LIMIT 1"))
        idCol = (*colq)[0].Get<std::string>();

    std::string spawnSql = "SELECT " + idCol + ", map, position_x, position_y, position_z "
                           "FROM creature ORDER BY " + idCol;
    QueryResult res = WorldDatabase.Query(spawnSql.c_str());
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
    g_teamTotal = (uint32)g_teamWork.size();
    g_active = g_total > 0 || g_teamTotal > 0;
    if (g_active) // quiet on startup when the table is already full
        LOG_INFO("server.loading", "[PlayerbotChatter] Area backfill started: {} entries to resolve, "
                 "{} existing rows need a faction team.", g_total, g_teamTotal);
    return Acore::StringFormat("Area backfill started: {} creature entries to resolve (batch {}/tick), "
                               "{} existing rows to fill team (batch {}/tick){}.",
                               g_total, kBatchPerTick, g_teamTotal, kTeamBatchPerTick,
                               g_hasTeamCol ? "" : " [team column missing: run the SQL update]");
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
        if (g_hasTeamCol)
            WorldDatabase.Execute(Acore::StringFormat(
                "INSERT IGNORE INTO mod_chatter_npc_area (creature_entry, area_name, zone_name, team) "
                "VALUES ({}, '{}', '{}', {})", s.entry, area, zone, (uint32)TeamFor(s.entry)));
        else
            WorldDatabase.Execute(Acore::StringFormat(
                "INSERT IGNORE INTO mod_chatter_npc_area (creature_entry, area_name, zone_name) "
                "VALUES ({}, '{}', '{}')", s.entry, area, zone));
        ++g_done;
    }

    // Team-only fill for rows written before the team column existed: no map work, so a
    // larger batch, grouped into one UPDATE per team value. `team IS NULL` keeps it idempotent.
    if (g_teamCursor < g_teamWork.size())
    {
        std::string lists[4];
        uint32 t = 0;
        for (; g_teamCursor < g_teamWork.size() && t < kTeamBatchPerTick; ++g_teamCursor, ++t)
        {
            uint32 entry = g_teamWork[g_teamCursor];
            std::string& l = lists[TeamFor(entry)];
            if (!l.empty())
                l += ',';
            l += std::to_string(entry);
        }
        for (uint32 team = 0; team < 4; ++team)
            if (!lists[team].empty())
                WorldDatabase.Execute(Acore::StringFormat(
                    "UPDATE mod_chatter_npc_area SET team = {} WHERE team IS NULL AND creature_entry IN ({})",
                    team, lists[team]));
        g_teamDone += t;
        if (g_teamCursor % kProgressEvery < kTeamBatchPerTick)
            LOG_INFO("server.loading", "[PlayerbotChatter] Area backfill: {}/{} team values filled.",
                     g_teamDone, g_teamTotal);
    }

    if (n > 0 && g_cursor % kProgressEvery < kBatchPerTick) // periodic progress
        LOG_INFO("server.loading", "[PlayerbotChatter] Area backfill: {}/{} processed, {} written.",
                 (uint32)g_cursor, g_total, g_done);

    if (g_cursor >= g_work.size() && g_teamCursor >= g_teamWork.size())
    {
        g_active = false;
        LOG_INFO("server.loading", "[PlayerbotChatter] Area backfill complete: {} written from {} entries, "
                 "{} team values filled.", g_done, g_total, g_teamDone);
        g_work.clear();
        g_teamWork.clear();
    }
}

std::string PBChatterAreaBackfill::StatusString()
{
    if (g_active)
        return Acore::StringFormat("Area backfill running: {}/{} processed, {} written; team {}/{} filled.",
                                   (uint32)g_cursor, g_total, g_done, g_teamDone, g_teamTotal);
    if (g_total > 0 || g_teamTotal > 0) // a run has completed this session — report its totals
        return Acore::StringFormat("Area backfill idle (last run wrote {} of {} entries, filled team on {} of {} rows).",
                                   g_done, g_total, g_teamDone, g_teamTotal);
    return "Area backfill idle.";
}
