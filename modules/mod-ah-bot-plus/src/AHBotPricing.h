/*
 * Copyright (C) 2026 azerothcore-playerbots-docker-automated contributors
 * Part of the in-house mod-ah-bot-plus fork; GNU AGPL v3 — see ../LICENSE-AGPL3 and ../NOTICE.
 */

#ifndef AHBOT_PRICING_H
#define AHBOT_PRICING_H

#include "AHBotRoles.h"

#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

// Price engine (in-house; see NOTICE). Pure: AuctionHouseBot gathers the inputs from the server. Materials
// are the anchor (today's formula, never rolled up); Crafted items cost their reagents plus a
// markup; non-crafted gear is calibrated against crafted gear of the same quality/item level.
namespace AHBotPricing
{
    constexpr uint32_t CLASS_WEAPON = 2;
    constexpr uint32_t CLASS_ARMOR  = 4;
    constexpr uint32_t KBAND_QUALITY_WIDE = 0xFFFFFFFFu;   // K pooled over the whole quality

    enum Source : uint8_t { SRC_FORMULA = 0, SRC_OVERRIDE = 1, SRC_ROLLUP = 2, SRC_GEAR_K = 3 };
    char const* SourceName(Source s);

    struct ItemFacts
    {
        uint32_t itemClass = 0;
        uint32_t quality = 0;
        uint32_t itemLevel = 0;
        uint32_t sellPrice = 0;
        uint64_t formulaValue = 0;   // today's formula with the variance removed
        bool isOverride = false;     // CompleteItemValueOverride: final, never rolled up/calibrated
    };

    struct Reagent
    {
        uint32_t itemId = 0;
        uint32_t count = 0;
    };

    struct Recipe
    {
        uint32_t spellId = 0;
        uint32_t product = 0;
        double yield = 1.0;          // average items created per cast
        std::vector<Reagent> reagents;
    };

    struct Inputs
    {
        std::unordered_map<uint32_t, ItemFacts> items;       // every item template
        std::vector<Recipe> recipes;                         // profession CREATE_ITEM spells
        std::unordered_map<uint32_t, uint64_t> vendorGold;   // per-unit price at an unlimited gold vendor
    };

    struct Params
    {
        double craftMarkup = 1.15;
        uint32_t gearMinSamples = 5;
    };

    struct ItemPrice
    {
        AHBotRoles::Role role = AHBotRoles::ROLE_GEAR;
        Source source = SRC_FORMULA;
        uint64_t formulaValue = 0;
        uint64_t finalValue = 0;     // listing center
        uint64_t matsValue = 0;      // Crafted with a usable recipe: reagent cost, no markup; else 0
        int32_t recipeIndex = -1;    // Table::recipes index of the cheapest usable recipe
        double k = 0.0;              // gear: value/vendor ratio used (0 = none)
        uint32_t kBand = 0;          // gear: centre of the 3-band window K came from, or KBAND_QUALITY_WIDE
        uint32_t kSamples = 0;       // gear: crafted samples behind K
    };

    struct Table
    {
        Params params;
        std::vector<Recipe> recipes;
        std::unordered_map<uint32_t, ItemPrice> prices;
        std::unordered_map<uint32_t, uint64_t> reagentValue;  // what each reagent was costed at
    };

    // Prices every item in `in.items`. Deterministic for a given input.
    Table Build(Inputs const& in, Params const& params);

    // Buyer ceiling held strictly below the seller floor (1 - sellReduce) by 0.01, and >= 0.
    double ClampBuyerCeiling(double requested, double sellReduce);

    // Most the buyer bot pays per item: Crafted items at mats value, everything else at final.
    uint64_t BuyerMax(ItemPrice const& p, double clampedCeiling);

    // Listing buyout range per item, before the below-vendor guard: {final*(1-r), final*(1+a)}.
    std::pair<uint64_t, uint64_t> SellBounds(uint64_t finalValue, double sellReduce, double sellAdd);
}

#endif
