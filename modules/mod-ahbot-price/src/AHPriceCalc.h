#ifndef MOD_AHBOT_PRICE_CALC_H
#define MOD_AHBOT_PRICE_CALC_H

#include "Define.h"

struct ItemTemplate;

// Per-single-unit AH-bot LISTING range, in copper (the bot's sell floor..ceiling for one unit).
// A thin wrapper over the in-house mod-ah-bot-plus pricing table
// (AuctionHouseBot::GetItemPricing), so every consumer reads the bot's real numbers —
// mod-enchanter-npc uses minCopper as its reagent-price floor. {0,0} until the table is built.
struct AHPriceBand
{
    uint64 minCopper;
    uint64 maxCopper;
};

AHPriceBand AHPriceComputeBand(ItemTemplate const* proto);

#endif
