#include "PBChatterLore.h"
#include "PBChatterConfig.h"
#include "PBChatterContext.h"
#include "PBChatterHttp.h"
#include "PBChatterMemory.h"
#include "Log.h"
#include "Player.h"
#include "ObjectMgr.h"
#include "QuestDef.h"
#include "ItemTemplate.h"
#include "CreatureData.h"
#include "GameObjectData.h"
#include "SharedDefines.h"
#include "DBCStores.h"
#include "World.h"
#include <nlohmann/json.hpp>

namespace
{
    char const* ClassName(uint8 c)
    {
        switch (c)
        {
            case CLASS_WARRIOR: return "warrior";    case CLASS_PALADIN: return "paladin";
            case CLASS_HUNTER:  return "hunter";     case CLASS_ROGUE:   return "rogue";
            case CLASS_PRIEST:  return "priest";     case CLASS_DEATH_KNIGHT: return "death knight";
            case CLASS_SHAMAN:  return "shaman";     case CLASS_MAGE:    return "mage";
            case CLASS_WARLOCK: return "warlock";    case CLASS_DRUID:   return "druid";
            default: return "adventurer";
        }
    }
    char const* RaceName(uint8 r)
    {
        switch (r)
        {
            case RACE_HUMAN: return "Human";          case RACE_ORC: return "Orc";
            case RACE_DWARF: return "Dwarf";          case RACE_NIGHTELF: return "Night Elf";
            case RACE_UNDEAD_PLAYER: return "Undead"; case RACE_TAUREN: return "Tauren";
            case RACE_GNOME: return "Gnome";          case RACE_TROLL: return "Troll";
            case RACE_BLOODELF: return "Blood Elf";   case RACE_DRAENEI: return "Draenei";
            default: return "traveler";
        }
    }

    // The sender's real quest log with per-objective progress. Kill/GO objectives read the
    // quest-log slot counters; item objectives read inventory count. Names resolved in-memory.
    nlohmann::json BuildPlayerQuests(Player* sender)
    {
        nlohmann::json quests = nlohmann::json::array();
        for (uint8 slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot)
        {
            uint32 qid = sender->GetQuestSlotQuestId(slot);
            if (!qid)
                continue;
            Quest const* q = sObjectMgr->GetQuestTemplate(qid);
            if (!q)
                continue;

            nlohmann::json objectives = nlohmann::json::array();

            // Kill / interact (creature or gameobject) objectives.
            for (uint8 i = 0; i < QUEST_OBJECTIVES_COUNT; ++i)
            {
                int32 reqNpcOrGo = q->RequiredNpcOrGo[i];
                uint32 need = q->RequiredNpcOrGoCount[i];
                if (reqNpcOrGo == 0 || need == 0)
                    continue;
                uint32 have = sender->GetQuestSlotCounter(slot, i);
                std::string text;
                if (reqNpcOrGo > 0)
                {
                    if (CreatureTemplate const* ct = sObjectMgr->GetCreatureTemplate((uint32)reqNpcOrGo))
                        text = ct->Name;
                }
                else if (GameObjectTemplate const* gt = sObjectMgr->GetGameObjectTemplate((uint32)(-reqNpcOrGo)))
                    text = gt->name;
                objectives.push_back({ {"text", text}, {"have", have}, {"need", need}, {"done", have >= need} });
            }

            // Item-collection objectives (progress = inventory count).
            for (uint8 j = 0; j < QUEST_ITEM_OBJECTIVES_COUNT; ++j)
            {
                uint32 reqItem = q->RequiredItemId[j];
                uint32 need = q->RequiredItemCount[j];
                if (reqItem == 0 || need == 0)
                    continue;
                uint32 have = sender->GetItemCount(reqItem, false);
                std::string text;
                if (ItemTemplate const* it = sObjectMgr->GetItemTemplate(reqItem))
                    text = it->Name1;
                objectives.push_back({ {"text", text}, {"have", have}, {"need", need}, {"done", have >= need} });
            }

            quests.push_back({ {"id", qid}, {"title", q->GetTitle()}, {"objectives", objectives} });
        }
        return quests;
    }
}

std::string PBChatterLore::BuildPayload(Player* bot, Player* sender, std::string const& question)
{
    std::string area = "the wilds";
    if (AreaTableEntry const* a = sAreaTableStore.LookupEntry(bot->GetAreaId()))
        if (char const* nm = a->area_name[sWorld->GetDefaultDbcLocale()])
            area = nm;

    nlohmann::json quests = nlohmann::json::array();
    for (uint8 slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot)
    {
        uint32 qid = bot->GetQuestSlotQuestId(slot);
        if (!qid)
            continue;
        if (Quest const* q = sObjectMgr->GetQuestTemplate(qid))
            quests.push_back({ {"id", qid}, {"title", q->GetTitle()} });
    }

    nlohmann::json recent = nlohmann::json::array();
    for (auto const& ex : PBChatterMemory::Recent(bot->GetGUID().GetCounter(), sender->GetGUID().GetCounter()))
        recent.push_back({ {"player", ex.first}, {"bot", ex.second} });

    nlohmann::json body = {
        {"bot", {
            {"name", bot->GetName()},
            {"level", bot->GetLevel()},
            {"class", ClassName(bot->getClass())},
            {"race", RaceName(bot->getRace())},
            {"faction", (bot->GetTeamId() == TEAM_ALLIANCE) ? "Alliance" : "Horde"},
            {"map", bot->GetMapId()},
            {"zone", area},
            {"x", bot->GetPositionX()},
            {"y", bot->GetPositionY()},
            {"z", bot->GetPositionZ()},
            {"active_quests", quests},
            {"snapshot", PBChatterContext::BuildSnapshot(bot)},
        }},
        {"player", sender->GetName()},
        {"player_quests", BuildPlayerQuests(sender)},
        {"recent", recent},
        {"question", question},
    };
    return body.dump();
}

std::string PBChatterLore::Ask(std::string const& payload)
{
    if (!g_PBChatLoreEnable)
        return "";

    if (g_PBChatDebug)
        LOG_INFO("server.loading", "[PlayerbotChatter] -> Lore: {}", payload);

    std::string raw = PBChatterHttp::Post(g_PBChatLoreUrl, payload, (int)g_PBChatLoreTimeout);
    if (raw.empty())
        return "";

    std::string reply;
    try
    {
        nlohmann::json j = nlohmann::json::parse(raw);
        if (j.value("ok", false) && j.contains("reply") && j["reply"].is_string())
            reply = j["reply"].get<std::string>();
    }
    catch (std::exception const& e)
    {
        LOG_WARN("server.loading", "[PlayerbotChatter] Lore JSON parse failed: {}", e.what());
        return "";
    }

    if (g_PBChatDebug)
        LOG_INFO("server.loading", "[PlayerbotChatter] <- Lore reply: {}", reply);
    return reply;
}
