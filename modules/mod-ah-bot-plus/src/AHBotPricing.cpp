/*
 * Copyright (C) 2026 azerothcore-playerbots-docker-automated contributors
 * Part of the in-house mod-ah-bot-plus fork; GNU AGPL v3 — see ../LICENSE-AGPL3 and ../NOTICE.
 */

#include "AHBotPricing.h"

#include <algorithm>
#include <map>
#include <unordered_set>

namespace
{
    using namespace AHBotPricing;

    double Median(std::vector<double> v)
    {
        std::sort(v.begin(), v.end());
        size_t n = v.size();
        return (n % 2) ? v[n / 2] : (v[n / 2 - 1] + v[n / 2]) / 2.0;
    }

    uint64_t Round(double v)
    {
        return v <= 0.0 ? 0 : uint64_t(v + 0.5);
    }

    bool IsGearClass(uint32_t itemClass)
    {
        return itemClass == CLASS_WEAPON || itemClass == CLASS_ARMOR;
    }

    struct Builder
    {
        Inputs const& in;
        Table& out;
        std::unordered_map<uint32_t, std::vector<size_t>> byProduct;
        std::unordered_set<uint32_t> visiting;
        std::unordered_set<uint32_t> done;

        Builder(Inputs const& i, Table& o) : in(i), out(o)
        {
            for (size_t r = 0; r < in.recipes.size(); ++r)
                byProduct[in.recipes[r].product].push_back(r);
        }

        // What a crafter pays for one unit of a reagent. False = no item template.
        bool ReagentValue(uint32_t id, uint64_t& value)
        {
            auto p = out.prices.find(id);
            if (p == out.prices.end())
                return false;
            value = p->second.role == AHBotRoles::ROLE_CRAFTED ? CraftedFinal(id) : p->second.formulaValue;
            auto v = in.vendorGold.find(id);
            if (v != in.vendorGold.end() && v->second > 0 && v->second < value)
                value = v->second;                          // a gold vendor is a hard ceiling
            out.reagentValue[id] = value;
            return true;
        }

        uint64_t CraftedFinal(uint32_t id)
        {
            ItemPrice& p = out.prices.at(id);
            if (done.count(id) || p.source == SRC_OVERRIDE)
                return p.finalValue;
            if (visiting.count(id))
                return p.formulaValue;                      // recipe loop: cost it at formula
            visiting.insert(id);

            double best = 0.0;
            int32_t bestIndex = -1;
            auto recipes = byProduct.find(id);
            if (recipes != byProduct.end())
            {
                for (size_t ri : recipes->second)
                {
                    Recipe const& r = in.recipes[ri];
                    if (r.reagents.empty())
                        continue;
                    double cost = 0.0;
                    bool usable = true;
                    for (Reagent const& rg : r.reagents)
                    {
                        uint64_t v = 0;
                        if (!ReagentValue(rg.itemId, v))
                        {
                            usable = false;
                            break;
                        }
                        cost += double(v) * rg.count;
                    }
                    if (!usable)
                        continue;
                    double per = cost / std::max(1.0, r.yield);
                    if (bestIndex < 0 || per < best)
                    {
                        best = per;
                        bestIndex = int32_t(ri);
                    }
                }
            }

            if (bestIndex >= 0)
            {
                p.recipeIndex = bestIndex;
                p.matsValue = std::max<uint64_t>(1, Round(best));
                uint64_t craft = Round(best * out.params.craftMarkup);
                if (craft > p.formulaValue)
                {
                    p.finalValue = craft;
                    p.source = SRC_ROLLUP;
                }
            }
            visiting.erase(id);
            done.insert(id);
            return p.finalValue;
        }
    };
}

namespace AHBotPricing
{
    char const* SourceName(Source s)
    {
        switch (s)
        {
            case SRC_FORMULA:  return "formula";
            case SRC_OVERRIDE: return "override";
            case SRC_ROLLUP:   return "rollup";
            case SRC_GEAR_K:   return "gearK";
        }
        return "?";
    }

    Table Build(Inputs const& in, Params const& params)
    {
        Table out;
        out.params = params;
        out.recipes = in.recipes;
        Builder b(in, out);

        std::vector<uint32_t> ids;
        ids.reserve(in.items.size());
        for (auto const& entry : in.items)
            ids.push_back(entry.first);
        std::sort(ids.begin(), ids.end());                  // deterministic loop resolution

        // Pass 1: role + formula value for every item.
        for (uint32_t id : ids)
        {
            ItemFacts const& f = in.items.at(id);
            ItemPrice p;
            p.role = AHBotRoles::Classify(f.itemClass, b.byProduct.count(id) != 0);
            p.formulaValue = std::max<uint64_t>(1, f.formulaValue);
            p.finalValue = p.formulaValue;
            p.source = f.isOverride ? SRC_OVERRIDE : SRC_FORMULA;
            out.prices.emplace(id, p);
        }

        // Pass 2: recipe roll-up for the Crafted role. Materials are the anchor.
        for (uint32_t id : ids)
            if (out.prices.at(id).role == AHBotRoles::ROLE_CRAFTED)
                b.CraftedFinal(id);

        // Pass 3: value/vendor ratios of crafted gear, by quality then ItemLevel/5 band.
        std::map<uint32_t, std::map<uint32_t, std::vector<double>>> samples;
        for (uint32_t id : ids)
        {
            ItemFacts const& f = in.items.at(id);
            ItemPrice const& p = out.prices.at(id);
            if (p.role == AHBotRoles::ROLE_CRAFTED && p.recipeIndex >= 0 && IsGearClass(f.itemClass) && f.sellPrice > 0)
                samples[f.quality][f.itemLevel / 5].push_back(double(p.finalValue) / f.sellPrice);
        }

        // Pass 3b: K per quality and 3-band window (centre c pools bands c-1..c+1), so one band of
        // unusually expensive crafts can't make neighbouring item levels jump (dev box: 55x vs 11x).
        struct KStat { double k; uint32_t samples; };
        std::map<uint32_t, std::map<uint32_t, KStat>> kByCentre;   // quality -> centre band -> K
        std::map<uint32_t, KStat> kQualityWide;
        for (auto const& [quality, bands] : samples)
        {
            std::vector<double> all;
            for (auto const& entry : bands)
                all.insert(all.end(), entry.second.begin(), entry.second.end());
            kQualityWide[quality] = { Median(all), uint32_t(all.size()) };

            uint32_t lo = bands.begin()->first;
            if (lo > 0)
                --lo;
            uint32_t hi = bands.rbegin()->first + 1;
            for (uint32_t c = lo; c <= hi; ++c)
            {
                std::vector<double> pooled;
                for (uint32_t b = (c == 0 ? 0 : c - 1); b <= c + 1; ++b)
                {
                    auto it = bands.find(b);
                    if (it != bands.end())
                        pooled.insert(pooled.end(), it->second.begin(), it->second.end());
                }
                if (pooled.size() >= params.gearMinSamples)
                    kByCentre[quality][c] = { Median(pooled), uint32_t(pooled.size()) };
            }
        }

        // Pass 4: calibrate non-crafted weapons/armor. Never below the formula.
        for (uint32_t id : ids)
        {
            ItemFacts const& f = in.items.at(id);
            ItemPrice& p = out.prices.at(id);
            if (p.role != AHBotRoles::ROLE_GEAR || !IsGearClass(f.itemClass) || f.sellPrice == 0 || p.source == SRC_OVERRIDE)
                continue;
            auto qw = kQualityWide.find(f.quality);
            if (qw == kQualityWide.end())
                continue;
            uint32_t band = f.itemLevel / 5;
            KStat stat = qw->second;
            uint32_t chosenBand = KBAND_QUALITY_WIDE;
            auto centres = kByCentre.find(f.quality);
            if (centres != kByCentre.end())
            {
                uint32_t bestDist = 0xFFFFFFFFu;
                for (auto const& [c, s] : centres->second)        // ascending, so ties keep the lower centre
                {
                    uint32_t dist = c > band ? c - band : band - c;
                    if (dist < bestDist)
                    {
                        bestDist = dist;
                        stat = s;
                        chosenBand = c;
                    }
                }
            }
            p.k = stat.k;
            p.kBand = chosenBand;
            p.kSamples = stat.samples;
            uint64_t v = Round(double(f.sellPrice) * p.k);
            if (v > p.formulaValue)
            {
                p.finalValue = v;
                p.source = SRC_GEAR_K;
            }
        }
        return out;
    }

    double ClampBuyerCeiling(double requested, double sellReduce)
    {
        double maxAllowed = 1.0 - sellReduce - 0.01;
        if (maxAllowed < 0.0)
            maxAllowed = 0.0;
        if (requested < 0.0)
            requested = 0.0;
        return std::min(requested, maxAllowed);
    }

    uint64_t BuyerMax(ItemPrice const& p, double clampedCeiling)
    {
        uint64_t basis = (p.role == AHBotRoles::ROLE_CRAFTED && p.matsValue > 0)
            ? std::min(p.matsValue, p.finalValue)
            : p.finalValue;
        return uint64_t(double(basis) * clampedCeiling);
    }

    std::pair<uint64_t, uint64_t> SellBounds(uint64_t finalValue, double sellReduce, double sellAdd)
    {
        uint64_t lo = Round(double(finalValue) * (1.0 - sellReduce));
        uint64_t hi = Round(double(finalValue) * (1.0 + sellAdd));
        if (lo < 1)
            lo = 1;
        if (hi < lo)
            hi = lo;
        return { lo, hi };
    }
}
