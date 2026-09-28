#include "EnchanterPrice.h"
#include "AHPriceCalc.h"
#include "AuctionHouseMgr.h"
#include "Bag.h"
#include "EnchanterCatalog.h"
#include "EnchanterConfig.h"
#include "EnchanterRules.h"
#include "ItemTemplate.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "Player.h"
#include <algorithm>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

namespace
{
    using QuoteMap = std::unordered_map<uint32, EnchanterPrice::Quote>;

    std::mutex g_lock;          // guards g_quotes: gossip menus can be built on map threads
    QuoteMap   g_quotes[2];     // [0] = Alliance AH, [1] = Horde AH
    uint32     g_sinceRebuildMs = 0;   // world thread only
    bool       g_warnedMissing = false;

    int TeamIndex(TeamId team) { return team == TEAM_HORDE ? 1 : 0; }

    EnchanterPrice::Quote Fallback(uint32 entry)
    {
        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(entry);
        if (!proto)
            return {};
        // A gold vendor's price is a hard ceiling on what anyone pays, so it beats the AH-bot floor.
        // BuyPrice alone is not: many never-vendored items (rep/token rewards) carry one.
        if (proto->BuyPrice > 0 && EnchanterCatalog::SoldByVendorForGold(entry))
            return { uint64(proto->BuyPrice), EnchanterPrice::SRC_VENDOR };
        if (uint64 floor = AHPriceComputeBand(proto).minCopper)
            return { floor, EnchanterPrice::SRC_BOT_FLOOR };
        if (proto->SellPrice > 0)
            return { uint64(proto->SellPrice) * 4, EnchanterPrice::SRC_SELL };
        return {};
    }
}

char const* EnchanterPrice::SourceName(Source s)
{
    switch (s)
    {
        case SRC_AH:        return "AH";
        case SRC_BOT_FLOOR: return "floor";
        case SRC_VENDOR:    return "vendor";
        case SRC_SELL:      return "sell";
        default:            return "none";
    }
}

void EnchanterPrice::Rebuild()
{
    std::vector<uint32> const& items = EnchanterCatalog::PricedItems();
    std::unordered_set<uint32> const wanted(items.begin(), items.end());

    std::unordered_map<uint32, uint64> lowest[2];
    AuctionHouseId const houses[2] = { AuctionHouseId::Alliance, AuctionHouseId::Horde };
    for (int t = 0; t < 2; ++t)
        if (AuctionHouseObject* ah = sAuctionMgr->GetAuctionsMapByHouseId(houses[t]))
            for (auto const& [id, a] : ah->GetAuctions())
                if (a && wanted.count(a->item_template))
                    EnchanterRules::FoldListing(lowest[t][a->item_template], a->buyout, a->itemCount);

    QuoteMap fresh[2];
    std::vector<uint32> missing;
    for (uint32 entry : items)
    {
        Quote const fb = Fallback(entry);
        bool anyPrice = fb.source != SRC_NONE;
        for (int t = 0; t < 2; ++t)
        {
            auto it = lowest[t].find(entry);
            if (it != lowest[t].end() && it->second)
            {
                fresh[t][entry] = { it->second, SRC_AH };
                anyPrice = true;
            }
            else
                fresh[t][entry] = fb;
        }
        if (!anyPrice)
            missing.push_back(entry);
    }

    {
        std::lock_guard<std::mutex> guard(g_lock);
        g_quotes[0].swap(fresh[0]);
        g_quotes[1].swap(fresh[1]);
    }
    g_sinceRebuildMs = 0;

    if (!missing.empty() && !g_warnedMissing)
    {
        g_warnedMissing = true;
        std::string ids;
        for (size_t i = 0; i < missing.size() && i < 20; ++i)
            ids += (i ? "," : "") + std::to_string(missing[i]);
        LOG_WARN("module", "[Enchanter] {} priced item(s) have no price source (priced 0): {}{}",
            missing.size(), ids, missing.size() > 20 ? ",..." : "");
    }
    if (g_EnchanterDebug)
        LOG_INFO("module", "[Enchanter] Price cache rebuilt: {} items, {} unpriced", items.size(), missing.size());
}

void EnchanterPrice::Tick(uint32 diff)
{
    g_sinceRebuildMs += diff;
    if (g_sinceRebuildMs >= g_EnchanterPriceRefreshSec * IN_MILLISECONDS)
        Rebuild();
}

EnchanterPrice::Quote EnchanterPrice::UnitQuote(uint32 itemEntry, TeamId team)
{
    std::lock_guard<std::mutex> guard(g_lock);
    QuoteMap const& m = g_quotes[TeamIndex(team)];
    auto it = m.find(itemEntry);
    return it != m.end() ? it->second : Quote{};
}

uint64 EnchanterPrice::MatsCopper(EnchantOffer const& offer, TeamId team)
{
    uint64 total = 0;
    for (auto const& [entry, count] : offer.reagents)
        total += UnitQuote(entry, team).unitCopper * count;
    return total;
}

uint32 EnchanterPrice::BagItemCount(Player* p, uint32 entry)
{
    uint32 n = 0;
    auto add = [&](Item* it) { if (it && it->GetEntry() == entry && !it->IsInTrade()) n += it->GetCount(); };
    for (uint8 s = INVENTORY_SLOT_ITEM_START; s < INVENTORY_SLOT_ITEM_END; ++s)
        add(p->GetItemByPos(INVENTORY_SLOT_BAG_0, s));
    for (uint8 b = INVENTORY_SLOT_BAG_START; b < INVENTORY_SLOT_BAG_END; ++b)
        if (Bag* bag = p->GetBagByPos(b))
            for (uint32 s = 0; s < bag->GetBagSize(); ++s)
                add(bag->GetItemByPos(uint8(s)));
    return n;
}

EnchanterPrice::MatsPlan EnchanterPrice::PlanMats(EnchantOffer const& offer, TeamId team, Player* ownFrom)
{
    // Aggregate by entry first, so a reagent listed twice never double-counts the bags.
    std::vector<std::pair<uint32, uint32>> need;
    for (auto const& [entry, count] : offer.reagents)
    {
        auto it = std::find_if(need.begin(), need.end(), [e = entry](auto const& p) { return p.first == e; });
        if (it != need.end())
            it->second += count;
        else
            need.emplace_back(entry, count);
    }

    MatsPlan plan;
    for (auto const& [entry, count] : need)
    {
        uint32 const have = ownFrom ? BagItemCount(ownFrom, entry) : 0;
        uint32 const missing = EnchanterRules::MissingCount(count, have);
        uint32 const used = count - missing;
        plan.buyCopper += UnitQuote(entry, team).unitCopper * missing;
        if (used > 0)
            plan.useOwn.emplace_back(entry, used);
    }
    return plan;
}

uint32 EnchanterPrice::FeeForPlan(EnchantOffer const& offer, MatsPlan const& plan)
{
    return EnchanterRules::Fee(plan.buyCopper, g_EnchanterMatsPct,
        EnchanterRules::LaborCopper(offer.rank, g_EnchanterLaborAtMax));
}

uint32 EnchanterPrice::OfferFee(EnchantOffer const& offer, TeamId team, Player* ownFrom)
{
    return FeeForPlan(offer, PlanMats(offer, team, ownFrom));
}
