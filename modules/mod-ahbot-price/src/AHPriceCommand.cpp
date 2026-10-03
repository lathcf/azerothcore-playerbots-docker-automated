#include "AHPriceCommand.h"
#include "AHPriceConfig.h"
#include "AuctionHouseBot.h"
#include "Chat.h"
#include "CommandScript.h"
#include "RBAC.h"
#include "DatabaseEnv.h"
#include "ObjectMgr.h"
#include "ItemTemplate.h"
#include "StringFormat.h"
#include <string>
#include <cctype>

using namespace Acore::ChatCommands;

ChatCommandTable AHPriceCommand::GetCommands() const
{
    // SEC_PLAYER: usable by any logged-in character (read-only). Console::Yes lets the
    // worldserver console run it too. Reachable over the addon channel via
    // AddonChannelCommandHandler, which pipes SendSysMessage output back to the client.
    static ChatCommandTable sub =
    {
        { "search", HandleSearch, SEC_PLAYER, Console::Yes },
        { "item",   HandleItem,   SEC_PLAYER, Console::Yes },
    };
    static ChatCommandTable root = { { "ahprice", sub } };
    return root;
}

bool AHPriceCommand::HandleSearch(ChatHandler* handler, Tail text)
{
    if (!g_AHPriceEnable) { handler->SendSysMessage("AHPrice is disabled (set AHBotPrice.Enable=1)."); return true; }

    // Sanitize to letters/digits/spaces and cap length (defense-in-depth for the LIKE).
    std::string raw(text);
    std::string clean;
    for (char c : raw)
        if (std::isalnum(static_cast<unsigned char>(c)) || c == ' ')
            clean.push_back(c);
    while (!clean.empty() && clean.front() == ' ') clean.erase(clean.begin());
    while (!clean.empty() && clean.back() == ' ') clean.pop_back();
    if (clean.size() < 3) { handler->SendSysMessage("Search needs at least 3 characters."); return true; }
    if (clean.size() > 40) clean.resize(40);

    std::string esc = clean;
    WorldDatabase.EscapeString(esc);

    // Items that can never be listed on the AH are meaningless here (the bot can't buy
    // them from a player), so hide them from search unless configured otherwise. Mirrors
    // the core's auction-sell checks (AuctionHouseHandler.cpp): soulbound/quest-bound
    // (bonding 1/4/5), conjured (Flags & ITEM_FLAG_CONJURED=0x2), and limited-duration.
    std::string filterClause = g_AHPriceHideUnauctionable
        ? " AND bonding NOT IN (1,4,5) AND (Flags & 2) = 0 AND duration = 0"
        : "";

    QueryResult result = WorldDatabase.Query(
        "SELECT entry, name, Quality, class, SellPrice, stackable FROM item_template "
        "WHERE name LIKE '%{}%'{} ORDER BY Quality DESC, name LIMIT 50", esc, filterClause);

    if (!result)
    {
        handler->SendSysMessage(Acore::StringFormat("N\t{}", clean).c_str()); // N = no results
        return true;
    }

    do
    {
        Field* f = result->Fetch();
        uint32 entry   = f[0].Get<uint32>();
        std::string nm = f[1].Get<std::string>();
        uint32 quality = f[2].Get<uint32>();
        // R\t<itemID>\t<quality>\t<name>
        handler->SendSysMessage(Acore::StringFormat("R\t{}\t{}\t{}", entry, quality, nm).c_str());
    } while (result->NextRow());

    return true;
}

bool AHPriceCommand::HandleItem(ChatHandler* handler, uint32 itemId)
{
    if (!g_AHPriceEnable) { handler->SendSysMessage("AHPrice is disabled (set AHBotPrice.Enable=1)."); return true; }

    ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemId);
    if (!proto) { handler->SendSysMessage(Acore::StringFormat("E\t{}", itemId).c_str()); return true; } // E = not found

    AHBotItemPricing pricing;
    if (!auctionbot->GetItemPricing(itemId, pricing))
    {
        handler->SendSysMessage(Acore::StringFormat("E\t{}", itemId).c_str());
        return true;
    }
    uint32 maxStack = proto->GetMaxStackSize();
    // P\t<itemID>\t<quality>\t<name>\t<sellPrice>\t<buy>\t<buy>\t<maxStack>\t<sellMin>\t<sellMax>
    // Fields 6/7 stay "what the bot pays" so addons predating fields 9/10 still read correctly.
    handler->SendSysMessage(Acore::StringFormat("P\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}",
        proto->ItemId, proto->Quality, proto->Name1, proto->SellPrice,
        pricing.buyMax, pricing.buyMax, maxStack, pricing.sellMin, pricing.sellMax).c_str());
    return true;
}
