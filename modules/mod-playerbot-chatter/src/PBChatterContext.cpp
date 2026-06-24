#include "PBChatterContext.h"
#include "PBChatterLevelContent.h"
#include "Player.h"
#include "Unit.h"
#include "Bag.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "DBCStores.h"
#include "ObjectMgr.h"
#include "QuestDef.h"
#include "SharedDefines.h"
#include "World.h"
#include "StringFormat.h"
#include <algorithm>
#include <map>
#include <unordered_map>
#include <vector>

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
            case RACE_HUMAN: return "Human";       case RACE_ORC: return "Orc";
            case RACE_DWARF: return "Dwarf";       case RACE_NIGHTELF: return "Night Elf";
            case RACE_UNDEAD_PLAYER: return "Undead"; case RACE_TAUREN: return "Tauren";
            case RACE_GNOME: return "Gnome";       case RACE_TROLL: return "Troll";
            case RACE_BLOODELF: return "Blood Elf"; case RACE_DRAENEI: return "Draenei";
            default: return "traveler";
        }
    }
}

std::string PBChatterContext::TopBagItem(Player* bot)
{
    std::unordered_map<uint32, uint32> counts;
    auto addItem = [&](Item* it) { if (it && it->GetTemplate()) counts[it->GetTemplate()->ItemId] += it->GetCount(); };
    for (uint8 s = INVENTORY_SLOT_ITEM_START; s < INVENTORY_SLOT_ITEM_END; ++s)
        addItem(bot->GetItemByPos(INVENTORY_SLOT_BAG_0, s));
    for (uint8 bag = INVENTORY_SLOT_BAG_START; bag < INVENTORY_SLOT_BAG_END; ++bag)
        if (Bag* pBag = bot->GetBagByPos(bag))
            for (uint32 s = 0; s < pBag->GetBagSize(); ++s)
                addItem(pBag->GetItemByPos(s));

    uint32 bestId = 0, bestQty = 0;
    for (auto const& [id, qty] : counts)
        if (qty > bestQty) { bestQty = qty; bestId = id; }
    if (!bestId)
        return "";
    if (ItemTemplate const* t = sObjectMgr->GetItemTemplate(bestId))
        return t->Name1;
    return "";
}

std::string PBChatterContext::BuildSnapshot(Player* bot)
{
    std::string faction = (bot->GetTeamId() == TEAM_ALLIANCE) ? "Alliance" : "Horde";

    std::string area = "the wilds";
    if (AreaTableEntry const* a = sAreaTableStore.LookupEntry(bot->GetAreaId()))
        if (char const* nm = a->area_name[sWorld->GetDefaultDbcLocale()])
            area = nm;

    std::string activity;
    if (bot->IsInCombat())
    {
        if (Unit* v = bot->GetVictim())
            activity = Acore::StringFormat("fighting {} ({}% hp)", v->GetName(), (int)v->GetHealthPct());
        else
            activity = "in combat";
    }
    else
        activity = "out adventuring";

    uint32 copper = bot->GetMoney();
    uint32 gold = copper / 10000; copper %= 10000;
    uint32 silver = copper / 100;  copper %= 100;

    // Aggregate inventory: backpack + equipped bags.
    std::unordered_map<uint32, uint32> counts;
    auto addItem = [&](Item* it) { if (it && it->GetTemplate()) counts[it->GetTemplate()->ItemId] += it->GetCount(); };
    for (uint8 s = INVENTORY_SLOT_ITEM_START; s < INVENTORY_SLOT_ITEM_END; ++s)
        addItem(bot->GetItemByPos(INVENTORY_SLOT_BAG_0, s));
    for (uint8 bag = INVENTORY_SLOT_BAG_START; bag < INVENTORY_SLOT_BAG_END; ++bag)
        if (Bag* pBag = bot->GetBagByPos(bag))
            for (uint32 s = 0; s < pBag->GetBagSize(); ++s)
                addItem(pBag->GetItemByPos(s));

    std::vector<std::pair<uint32, uint32>> items(counts.begin(), counts.end());
    std::sort(items.begin(), items.end(), [](auto const& x, auto const& y) { return x.second > y.second; });
    std::string bags;
    int shown = 0;
    for (auto const& [id, qty] : items)
    {
        if (shown >= 6) break;
        if (ItemTemplate const* t = sObjectMgr->GetItemTemplate(id))
        {
            if (shown) bags += ", ";
            bags += Acore::StringFormat("{}x {}", qty, t->Name1);
            ++shown;
        }
    }
    if (bags.empty())
        bags = "almost nothing";

    std::string snap = Acore::StringFormat(
        "You're playing {}, your level {} {} {} ({}). Right now you're in {}, {}. Health {}%",
        bot->GetName(), bot->GetLevel(), RaceName(bot->getRace()), ClassName(bot->getClass()),
        faction, area, activity, (int)bot->GetHealthPct());

    if (bot->GetMaxPower(POWER_MANA) > 0)
        snap += Acore::StringFormat(", mana {}%", (int)bot->GetPowerPct(POWER_MANA));

    snap += Acore::StringFormat(". You've got {}g {}s {}c. In your bags: {}.", gold, silver, copper, bags);
    return snap;
}

std::string PBChatterContext::ActiveQuestLine(Player* bot)
{
    for (uint8 slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot)
    {
        uint32 qid = bot->GetQuestSlotQuestId(slot);
        if (!qid)
            continue;
        if (Quest const* q = sObjectMgr->GetQuestTemplate(qid))
            return Acore::StringFormat(" You're currently working on the quest '{}'.", q->GetTitle());
    }
    return "";
}

std::string PBChatterContext::BuildBrief(Player* bot)
{
    std::string area = "the world";
    if (AreaTableEntry const* a = sAreaTableStore.LookupEntry(bot->GetAreaId()))
        if (char const* nm = a->area_name[sWorld->GetDefaultDbcLocale()])
            area = nm;
    return Acore::StringFormat("You're a level {} {} in {}.{}",
                               bot->GetLevel(), ClassName(bot->getClass()), area,
                               ActiveQuestLine(bot));
}

std::string PBChatterContext::BuildGroundedBrief(Player* bot)
{
    std::string area = "the world";
    if (AreaTableEntry const* a = sAreaTableStore.LookupEntry(bot->GetAreaId()))
        if (char const* nm = a->area_name[sWorld->GetDefaultDbcLocale()])
            area = nm;

    std::string brief = Acore::StringFormat("You're a level {} {} in {}.{}",
        bot->GetLevel(), ClassName(bot->getClass()), area, ActiveQuestLine(bot));

    // One real character detail: gold, and the item you're carrying most of.
    uint32 gold = bot->GetMoney() / 10000;
    std::string item = TopBagItem(bot);
    if (gold > 0 && !item.empty())
        brief += Acore::StringFormat(" You've got about {}g on you and a stack of {}.", gold, item);
    else if (gold > 0)
        brief += Acore::StringFormat(" You've got about {}g on you.", gold);
    else if (!item.empty())
        brief += Acore::StringFormat(" You're carrying a stack of {}.", item);

    // Level-appropriate content the bot could naturally bring up.
    auto join = [](std::vector<std::string> const& v, int n) {
        std::string out; int shown = 0;
        for (auto const& s : v) { if (shown >= n) break; if (shown) out += ", "; out += s; ++shown; }
        return out;
    };
    std::string dungeons = join(PBChatterLevelContent::Dungeons(bot->GetLevel()), 2);
    std::string zones    = join(PBChatterLevelContent::Zones(bot->GetLevel(), (uint8)bot->GetTeamId()), 2);
    if (!dungeons.empty())
        brief += Acore::StringFormat(" Dungeons around your level: {}.", dungeons);
    if (!zones.empty())
        brief += Acore::StringFormat(" Zones your level: {}.", zones);
    return brief;
}
