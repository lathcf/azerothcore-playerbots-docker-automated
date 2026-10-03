#include "AHPriceCalc.h"
#include "AuctionHouseBot.h"
#include "ItemTemplate.h"

AHPriceBand AHPriceComputeBand(ItemTemplate const* proto)
{
    AHBotItemPricing pricing;
    if (!proto || !auctionbot->GetItemPricing(proto->ItemId, pricing))
        return { 0, 0 };
    return { pricing.sellMin, pricing.sellMax };
}
