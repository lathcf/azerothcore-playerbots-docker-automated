// modules/mod-ah-bot-plus/tests/test_ahbot_pricing.cpp
// Standalone: g++ -std=c++17 -Wall -Wextra -Werror -o <out> modules/mod-ah-bot-plus/tests/test_ahbot_pricing.cpp
//   modules/mod-ah-bot-plus/src/AHBotRoles.cpp modules/mod-ah-bot-plus/src/AHBotPricing.cpp && <out>
#include "../src/AHBotRoles.h"
#include "../src/AHBotPricing.h"
#include <cmath>
#include <cstdio>
#include <string>

static int g_fails = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); ++g_fails; } } while (0)

using namespace AHBotRoles;

static void TestRoles()
{
    // Materials beats Crafted: bars/leather/cloth are crafted but are mats.
    CHECK(Classify(7, true) == ROLE_MATERIALS);
    CHECK(Classify(5, false) == ROLE_MATERIALS);
    CHECK(Classify(9, false) == ROLE_MATERIALS);
    CHECK(Classify(4, true) == ROLE_CRAFTED);
    CHECK(Classify(0, true) == ROLE_CRAFTED);
    CHECK(Classify(2, false) == ROLE_GEAR);
    CHECK(Classify(15, false) == ROLE_GEAR);

    std::string err;
    RoleArray s = ParseSellers("Materials:1501, crafted:1502,GEAR:1503", err);
    CHECK(s[ROLE_MATERIALS] == 1501 && s[ROLE_CRAFTED] == 1502 && s[ROLE_GEAR] == 1503);
    CHECK(err.empty());
    err.clear();
    s = ParseSellers("Materials:1501,Bogus:7", err);
    CHECK(s[ROLE_MATERIALS] == 1501 && s[ROLE_CRAFTED] == 0 && s[ROLE_GEAR] == 0);
    CHECK(!err.empty());
    err.clear();
    s = ParseSellers("", err);
    CHECK(s[0] == 0 && s[1] == 0 && s[2] == 0 && err.empty());
    err.clear();
    s = ParseSellers("Gear:abc", err);
    CHECK(s[ROLE_GEAR] == 0 && !err.empty());

    err.clear();
    RoleArray sh = ParseShares("45,30,25", err);
    CHECK(sh[0] == 45 && sh[1] == 30 && sh[2] == 25 && err.empty());
    err.clear();
    sh = ParseShares("10, 20 ,70", err);
    CHECK(sh[0] == 10 && sh[1] == 20 && sh[2] == 70 && err.empty());
    err.clear();
    sh = ParseShares("50,50", err);
    CHECK(sh[0] == 45 && sh[1] == 30 && sh[2] == 25 && !err.empty());
    err.clear();
    sh = ParseShares("0,0,0", err);
    CHECK(sh[0] == 45 && !err.empty());

    RoleArray t = Targets(25000, RoleArray{ 45, 30, 25 });
    CHECK(t[0] == 11250 && t[1] == 7500 && t[2] == 6250);
    t = Targets(1000, RoleArray{ 50, 50, 50 });           // normalized: shares need not sum to 100
    CHECK(t[0] == 333 && t[1] == 333 && t[2] == 333);

    // Empty house: proportional to deficit, remainder to the largest deficit.
    RoleArray a = AllocateListings(RoleArray{ 0, 0, 0 }, RoleArray{ 11250, 7500, 6250 }, 150);
    CHECK(a[0] == 68 && a[1] == 45 && a[2] == 37);
    // Everything fits: each role gets exactly its deficit.
    a = AllocateListings(RoleArray{ 11200, 7500, 6200 }, RoleArray{ 11250, 7500, 6250 }, 150);
    CHECK(a[0] == 50 && a[1] == 0 && a[2] == 50);
    // A role over target gets nothing.
    a = AllocateListings(RoleArray{ 12000, 0, 0 }, RoleArray{ 11250, 7500, 6250 }, 150);
    CHECK(a[0] == 0 && a[1] == 82 && a[2] == 68);
    a = AllocateListings(RoleArray{ 0, 0, 0 }, RoleArray{ 10, 10, 10 }, 0);
    CHECK(a[0] == 0 && a[1] == 0 && a[2] == 0);
}

static void TestPick()
{
    // Scarcest-first: fewest current listings wins; ties keep the earliest draw; 0 draws are skipped.
    CHECK(PickLeastListed({ 5, 7, 9 }, { { 5, 3 }, { 7, 1 }, { 9, 1 } }) == 7);
    CHECK(PickLeastListed({ 4 }, {}) == 4);
    CHECK(PickLeastListed({ 0, 0 }, {}) == 0);
    CHECK(PickLeastListed({}, {}) == 0);
    CHECK(PickLeastListed({ 8, 3 }, {}) == 8);
    CHECK(PickLeastListed({ 2, 2, 6 }, { { 2, 5 } }) == 6);
    CHECK(PickLeastListed({ 0, 11 }, { { 11, 4 } }) == 11);
}

using namespace AHBotPricing;

static void AddItem(Inputs& in, uint32_t id, uint32_t cls, uint32_t q, uint32_t ilvl, uint32_t sell,
                    uint64_t formula, bool isOverride = false)
{
    ItemFacts f;
    f.itemClass = cls; f.quality = q; f.itemLevel = ilvl; f.sellPrice = sell;
    f.formulaValue = formula; f.isOverride = isOverride;
    in.items[id] = f;
}

static void AddRecipe(Inputs& in, uint32_t spell, uint32_t product, double yield, std::vector<Reagent> reagents)
{
    Recipe r;
    r.spellId = spell; r.product = product; r.yield = yield; r.reagents = std::move(reagents);
    in.recipes.push_back(r);
}

static bool Near(double a, double b) { return std::fabs(a - b) < 1e-9; }

static void TestPricing()
{
    Inputs in;
    // Mats (class 7 = trade goods): the anchor, never rolled up.
    AddItem(in, 100, 7, 1, 50, 100, 1000);                 // ore
    AddItem(in, 101, 7, 2, 55, 50, 500);                   // bar, crafted from ore but still Materials
    AddItem(in, 102, 7, 1, 40, 10, 300);                   // flux, sold by a gold vendor for 50
    in.vendorGold[102] = 50;
    AddRecipe(in, 9001, 101, 1.0, { { 100, 2 } });

    // Crafted epic weapon: cheapest of two recipes, markup applied (2 = weapon, 4 = armor).
    AddItem(in, 200, 2, 4, 60, 1000, 3000);
    AddRecipe(in, 9002, 200, 1.0, { { 101, 8 }, { 102, 2 } });   // 8*500 + 2*50 = 4100
    AddRecipe(in, 9003, 200, 1.0, { { 100, 10 } });              // 10000
    // Crafted epic armor whose roll-up is below the formula: formula floor holds.
    AddItem(in, 201, 4, 4, 61, 2000, 9000);
    AddRecipe(in, 9004, 201, 1.0, { { 100, 1 } });
    // Multi-yield consumable (class 0).
    AddItem(in, 202, 0, 1, 40, 5, 100);
    AddRecipe(in, 9005, 202, 5.0, { { 101, 1 } });               // 500 / 5 = 100
    // Second crafted epic weapon, for the K band.
    AddItem(in, 203, 2, 4, 62, 1000, 100);
    AddRecipe(in, 9012, 203, 1.0, { { 100, 10 } });              // 10000
    // Recipe loop between two consumables.
    AddItem(in, 300, 0, 1, 30, 1, 400);
    AddItem(in, 301, 0, 1, 30, 1, 600);
    AddRecipe(in, 9006, 300, 1.0, { { 301, 1 } });
    AddRecipe(in, 9007, 301, 1.0, { { 300, 1 } });
    // Missing reagent template -> recipe unusable.
    AddItem(in, 400, 0, 1, 30, 1, 250);
    AddRecipe(in, 9008, 400, 1.0, { { 99999, 1 } });
    // CompleteItemValueOverride item: never rolled up.
    AddItem(in, 500, 0, 1, 30, 1, 12345, true);
    AddRecipe(in, 9009, 500, 1.0, { { 100, 1 } });
    // Crafted non-material intermediate feeding crafted armor.
    AddItem(in, 600, 0, 1, 30, 1, 10);
    AddRecipe(in, 9010, 600, 1.0, { { 100, 1 } });               // 1000 -> 1150
    AddItem(in, 601, 4, 2, 30, 1000, 10);
    AddRecipe(in, 9011, 601, 1.0, { { 600, 2 } });               // 2300 -> 2645
    // Non-crafted drop gear.
    AddItem(in, 700, 2, 4, 63, 1000, 100);                       // band 12
    AddItem(in, 701, 2, 4, 90, 2000, 100);                       // band 18: nearest qualifying band 12
    AddItem(in, 702, 2, 3, 40, 100, 900);                        // no rare samples at all
    AddItem(in, 703, 4, 2, 30, 1000, 10);                        // band 6 has 1 sample: quality-wide
    AddItem(in, 704, 4, 4, 60, 10, 5000);                        // K price below formula

    // Smoothing: band 20 ratios {2.3, 2.3}, band 21 {9.2, 9.2}; a band-21 drop uses the 20..22 window.
    AddItem(in, 801, 2, 5, 100, 1000, 1); AddRecipe(in, 9020, 801, 1.0, { { 100, 2 } });   // 2300 -> 2.3
    AddItem(in, 802, 2, 5, 101, 1000, 1); AddRecipe(in, 9021, 802, 1.0, { { 100, 2 } });
    AddItem(in, 803, 2, 5, 105, 1000, 1); AddRecipe(in, 9022, 803, 1.0, { { 100, 8 } });   // 9200 -> 9.2
    AddItem(in, 804, 2, 5, 106, 1000, 1); AddRecipe(in, 9023, 804, 1.0, { { 100, 8 } });
    AddItem(in, 805, 2, 5, 107, 1000, 1);                                                    // drop, band 21

    Params params;
    params.craftMarkup = 1.15;
    params.gearMinSamples = 2;
    Table t = Build(in, params);

    // Materials stay at formula even though the bar's recipe costs more.
    CHECK(t.prices.at(101).role == AHBotRoles::ROLE_MATERIALS);
    CHECK(t.prices.at(101).finalValue == 500 && t.prices.at(101).source == SRC_FORMULA);
    CHECK(t.prices.at(101).recipeIndex == -1);
    CHECK(t.prices.at(100).role == AHBotRoles::ROLE_MATERIALS);

    // Roll-up: cheapest recipe, vendor ceiling on flux, markup.
    ItemPrice const& hammer = t.prices.at(200);
    CHECK(hammer.role == AHBotRoles::ROLE_CRAFTED);
    CHECK(hammer.source == SRC_ROLLUP);
    CHECK(hammer.matsValue == 4100);
    CHECK(hammer.finalValue == 4715);
    CHECK(hammer.recipeIndex >= 0 && t.recipes[hammer.recipeIndex].spellId == 9002);
    CHECK(t.reagentValue.at(102) == 50);
    CHECK(t.reagentValue.at(101) == 500);

    // Formula floor.
    CHECK(t.prices.at(201).finalValue == 9000 && t.prices.at(201).source == SRC_FORMULA);
    CHECK(t.prices.at(201).matsValue == 1000 && t.prices.at(201).recipeIndex >= 0);

    // Yield.
    CHECK(t.prices.at(202).matsValue == 100 && t.prices.at(202).finalValue == 115);
    CHECK(t.prices.at(202).source == SRC_ROLLUP);

    // Loop: the inner item costs its partner at formula; outer resolves from the inner's final.
    CHECK(t.prices.at(301).finalValue == 600 && t.prices.at(301).source == SRC_FORMULA);
    CHECK(t.prices.at(300).finalValue == 690 && t.prices.at(300).source == SRC_ROLLUP);

    // Missing reagent.
    CHECK(t.prices.at(400).role == AHBotRoles::ROLE_CRAFTED);
    CHECK(t.prices.at(400).recipeIndex == -1 && t.prices.at(400).matsValue == 0);
    CHECK(t.prices.at(400).finalValue == 250);

    // Override wins.
    CHECK(t.prices.at(500).source == SRC_OVERRIDE && t.prices.at(500).finalValue == 12345);

    // Crafted intermediate is costed at its own crafted value.
    CHECK(t.prices.at(600).finalValue == 1150);
    CHECK(t.prices.at(601).matsValue == 2300 && t.prices.at(601).finalValue == 2645);

    // Gear K: epic band 12 samples {4.5, 4.715, 11.5} -> median 4.715.
    ItemPrice const& drop = t.prices.at(700);
    CHECK(drop.role == AHBotRoles::ROLE_GEAR);
    CHECK(drop.source == SRC_GEAR_K && drop.finalValue == 4715);
    CHECK(drop.kBand == 12 && drop.kSamples == 3 && Near(drop.k, 4.715));
    CHECK(t.prices.at(701).finalValue == 9430 && t.prices.at(701).kBand == 13);
    CHECK(t.prices.at(702).source == SRC_FORMULA && t.prices.at(702).k == 0.0);
    CHECK(t.prices.at(702).finalValue == 900);
    CHECK(t.prices.at(703).kBand == KBAND_QUALITY_WIDE && t.prices.at(703).kSamples == 1);
    CHECK(t.prices.at(703).finalValue == 2645);
    CHECK(t.prices.at(704).source == SRC_FORMULA && t.prices.at(704).finalValue == 5000);
    // Window median {2.3,2.3,9.2,9.2} = 5.75, not band 21's own 9.2.
    CHECK(t.prices.at(805).kBand == 21 && t.prices.at(805).kSamples == 4);
    CHECK(t.prices.at(805).finalValue == 5750 && t.prices.at(805).source == SRC_GEAR_K);

    // Buyer: crafted items at mats only; everything else at final value.
    CHECK(BuyerMax(hammer, 0.5) == 2050);
    CHECK(BuyerMax(drop, 0.5) == 2357);
    CHECK(BuyerMax(t.prices.at(400), 0.5) == 125);              // no recipe: final value

    CHECK(Near(ClampBuyerCeiling(0.80, 0.15), 0.80));
    CHECK(Near(ClampBuyerCeiling(0.90, 0.15), 0.84));
    CHECK(Near(ClampBuyerCeiling(-1.0, 0.15), 0.0));
    CHECK(Near(ClampBuyerCeiling(0.5, 1.2), 0.0));

    auto bounds = SellBounds(1000, 0.15, 0.25);
    CHECK(bounds.first == 850 && bounds.second == 1250);
}

int main()
{
    TestRoles();
    TestPick();
    TestPricing();
    if (g_fails) { std::printf("%d check(s) failed\n", g_fails); return 1; }
    std::printf("all checks passed\n");
    return 0;
}
