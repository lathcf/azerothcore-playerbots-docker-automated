#include "PBChatterMemory.h"
#include "PBChatterConfig.h"
#include "DatabaseEnv.h"
#include "QueryResult.h"  // ResultSet (Fetch/NextRow) — DatabaseEnv.h only forward-declares it
#include "Field.h"        // Field::Get<T>
#include "StringFormat.h"
#include <mutex>
#include <unordered_map>

namespace
{
    std::unordered_map<uint64_t, std::unordered_map<uint64_t, std::deque<PBChatterMemory::Exchange>>> g_cache;
    std::mutex g_mutex;
}

void PBChatterMemory::LoadAllFromDB()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    g_cache.clear();
    if (QueryResult res = CharacterDatabase.Query(
            "SELECT bot_guid, player_guid, player_message, bot_reply FROM "
            "mod_playerbot_chatter_history ORDER BY ts ASC"))
    {
        do
        {
            Field* f = res->Fetch();
            uint64_t bot = f[0].Get<uint64_t>();
            uint64_t plr = f[1].Get<uint64_t>();
            auto& dq = g_cache[bot][plr];
            dq.emplace_back(f[2].Get<std::string>(), f[3].Get<std::string>());
            while (dq.size() > g_PBChatHistoryLen)
                dq.pop_front();
        } while (res->NextRow());
    }
}

std::deque<PBChatterMemory::Exchange> PBChatterMemory::Recent(uint64_t botGuid, uint64_t playerGuid)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    auto bIt = g_cache.find(botGuid);
    if (bIt == g_cache.end())
        return {};
    auto pIt = bIt->second.find(playerGuid);
    if (pIt == bIt->second.end())
        return {};
    return pIt->second; // copy
}

void PBChatterMemory::Append(uint64_t botGuid, uint64_t playerGuid,
                             std::string const& playerMsg, std::string const& botReply)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    auto& dq = g_cache[botGuid][playerGuid];
    dq.emplace_back(playerMsg, botReply);
    while (dq.size() > g_PBChatHistoryLen)
        dq.pop_front();
}

void PBChatterMemory::FlushToDB()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    for (auto const& [botGuid, players] : g_cache)
    {
        for (auto const& [playerGuid, dq] : players)
        {
            for (auto const& ex : dq)
            {
                std::string pm = ex.first;
                std::string br = ex.second;
                CharacterDatabase.EscapeString(pm);
                CharacterDatabase.EscapeString(br);
                CharacterDatabase.Execute(Acore::StringFormat(
                    "INSERT IGNORE INTO mod_playerbot_chatter_history "
                    "(bot_guid, player_guid, ts, player_message, bot_reply) "
                    "VALUES ({}, {}, NOW(), '{}', '{}')",
                    botGuid, playerGuid, pm, br));
            }
        }
    }
    CharacterDatabase.Execute(Acore::StringFormat(
        "DELETE h FROM mod_playerbot_chatter_history h "
        "JOIN (SELECT id, ROW_NUMBER() OVER (PARTITION BY bot_guid, player_guid ORDER BY ts DESC, id DESC) rn "
        "      FROM mod_playerbot_chatter_history) r ON h.id = r.id WHERE r.rn > {}",
        g_PBChatHistoryLen));
}
