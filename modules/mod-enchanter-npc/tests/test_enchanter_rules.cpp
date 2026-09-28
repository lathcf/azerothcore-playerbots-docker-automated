// modules/mod-enchanter-npc/tests/test_enchanter_rules.cpp
// Standalone: g++ -std=c++17 -Wall -Wextra -Werror -o <out> modules/mod-enchanter-npc/tests/test_enchanter_rules.cpp && <out>
#include "../src/EnchanterRules.h"
#include <cstdio>

using namespace EnchanterRules;

static int g_fails = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); ++g_fails; } } while (0)

int main()
{
    // Recipe era = the expansion whose skill cap first admits the rank.
    CHECK(EraForRecipeRank(0)   == ERA_VANILLA);
    CHECK(EraForRecipeRank(300) == ERA_VANILLA);
    CHECK(EraForRecipeRank(301) == ERA_TBC);
    CHECK(EraForRecipeRank(375) == ERA_TBC);
    CHECK(EraForRecipeRank(376) == ERA_WOTLK);
    CHECK(EraForRecipeRank(450) == ERA_WOTLK);

    // Material era: a recipe is at least as late as the expansion that introduced its reagents.
    CHECK(MaterialEra(22445) == ERA_TBC);       // Arcane Dust
    CHECK(MaterialEra(22450) == ERA_TBC);       // Void Crystal
    CHECK(MaterialEra(34054) == ERA_WOTLK);     // Infinite Dust
    CHECK(MaterialEra(34057) == ERA_WOTLK);     // Abyss Crystal
    CHECK(MaterialEra(10940) == ERA_VANILLA);   // Strange Dust
    CHECK(MaterialEra(20725) == ERA_VANILLA);   // Nexus Crystal

    // Upgrade item era = the mod-playerbots gear proxy (required level AND item-id threshold).
    CHECK(EraForUpgradeItem(60, 23727) == ERA_VANILLA);
    CHECK(EraForUpgradeItem(60, 23728) == ERA_TBC);
    CHECK(EraForUpgradeItem(61, 20000) == ERA_TBC);
    CHECK(EraForUpgradeItem(70, 35569) == ERA_TBC);
    CHECK(EraForUpgradeItem(70, 35570) == ERA_WOTLK);
    CHECK(EraForUpgradeItem(71, 30000) == ERA_WOTLK);

    // Item-level rule, floor: RequiredLevel if set, else ItemLevel, must reach BaseLevel.
    CHECK(!PassesItemLevelRule(0, 55, 60, 0, false, false));
    CHECK( PassesItemLevelRule(0, 60, 60, 0, false, false));
    CHECK(!PassesItemLevelRule(35, 60, 60, 0, false, false));   // RequiredLevel wins over ItemLevel
    CHECK( PassesItemLevelRule(35, 60, 60, 0, true,  false));   // SPELL_ATTR2_ALLOW_LOW_LEVEL_BUFF
    // Ceiling: only for casts from an item (upgrade items), never recipes.
    CHECK(!PassesItemLevelRule(80, 200, 60, 150, false, true));
    CHECK( PassesItemLevelRule(80, 200, 60, 150, false, false));
    CHECK( PassesItemLevelRule(80, 200, 60, 0,   false, true));

    // Recipe rank: trainer ReqSkillRank, else formula RequiredSkillRank, else SkillLineAbility rank.
    CHECK(PickRecipeRank(225, 300, 1) == 225);
    CHECK(PickRecipeRank(225, 0,   1) == 225);
    CHECK(PickRecipeRank(0,   300, 1) == 300);
    CHECK(PickRecipeRank(0,   0,   1) == 1);
    CHECK(PickRecipeRank(0,   0,   0) == 0);
    CHECK(PickRecipeRankSource(225, 300) == RANK_TRAINER);
    CHECK(PickRecipeRankSource(225, 0)   == RANK_TRAINER);
    CHECK(PickRecipeRankSource(0,   300) == RANK_FORMULA);
    CHECK(PickRecipeRankSource(0,   0)   == RANK_SKILLLINE);

    // Upgrade item rank on the 0-450 skill scale.
    CHECK(RankForUpgradeItem(0)  == 0);
    CHECK(RankForUpgradeItem(60) == 337);
    CHECK(RankForUpgradeItem(80) == 450);
    CHECK(RankForUpgradeItem(90) == 450);

    // Quadratic labor: 5g at 450, ~2g22s at 300, ~55s at 150, ~6s at 50.
    CHECK(LaborCopper(450, 50000) == 50000);
    CHECK(LaborCopper(300, 50000) == 22222);
    CHECK(LaborCopper(150, 50000) == 5556);
    CHECK(LaborCopper(50,  50000) == 617);
    CHECK(LaborCopper(0,   50000) == 0);
    CHECK(LaborCopper(500, 50000) == 50000);

    // Fee = mats * pct / 100 + labor, min 1 copper, capped to int32.
    CHECK(Fee(1000, 100, 500) == 1500);
    CHECK(Fee(1000, 50, 0)    == 500);
    CHECK(Fee(0, 100, 0)      == 1);
    CHECK(Fee(0xFFFFFFFFFFull, 100, 0) == uint32_t(kMaxFee));

    // Cheapest per-unit buyout; bid-only (buyout 0) and empty listings never count.
    { uint64_t best = 0;
      FoldListing(best, 100, 1);  CHECK(best == 100);
      FoldListing(best, 150, 3);  CHECK(best == 50);
      FoldListing(best, 0, 5);    CHECK(best == 50);
      FoldListing(best, 10, 0);   CHECK(best == 50);
      FoldListing(best, 1, 5);    CHECK(best == 1); }   // sub-copper unit rounds up to 1

    // Source tier = lowest tier over sources; no source seen = open.
    { uint8_t acc = kTierUnset;
      CHECK(ResolveTier(acc) == 0);
      FoldTier(acc, 4); CHECK(acc == 4);
      FoldTier(acc, 9); CHECK(acc == 4);
      FoldTier(acc, 0); CHECK(acc == 0); }
    CHECK(ResolveTier(9) == 9);

    // Paging: 30 per page.
    CHECK(PageCount(0)  == 1);
    CHECK(PageCount(30) == 1);
    CHECK(PageCount(31) == 2);
    CHECK(PageBegin(1, 31) == 30);
    CHECK(PageEnd(1, 31)   == 31);
    CHECK(PageBegin(5, 31) == 31);   // out-of-range page is empty, not UB
    CHECK( HasNextPage(0, 31));
    CHECK(!HasNextPage(1, 31));
    CHECK(!HasNextPage(0, 30));

    // Gossip action encoding.
    CHECK( IsPageAction(kActionPageBase));
    CHECK( IsPageAction(kActionPageBase + 5));
    CHECK(!IsPageAction(kActionBack));
    CHECK(!IsPageAction(42));

    // Sender carries the per-visit "use my materials" toggle as a flag bit (set = OFF).
    for (uint32_t base : { kSenderTop, kSenderSlotBase + 18 })
    {
        CHECK( SenderUsesMats(EncodeSender(base, true)));
        CHECK(!SenderUsesMats(EncodeSender(base, false)));
        CHECK(SenderBase(EncodeSender(base, true))  == base);
        CHECK(SenderBase(EncodeSender(base, false)) == base);
    }
    CHECK(EncodeSender(kSenderTop, true) == kSenderTop);   // ON keeps the legacy encoding
    CHECK(!IsPageAction(kActionToggleMats));
    CHECK(kActionToggleMats != kActionBack);

    // Missing mats = what the bags can't cover.
    CHECK(MissingCount(4, 0) == 4);
    CHECK(MissingCount(4, 3) == 1);
    CHECK(MissingCount(4, 4) == 0);
    CHECK(MissingCount(4, 9) == 0);

    // Money string.
    CHECK(MoneyString(0)      == "0c");
    CHECK(MoneyString(5)      == "5c");
    CHECK(MoneyString(105)    == "1s 5c");
    CHECK(MoneyString(10000)  == "1g 0s 0c");
    CHECK(MoneyString(120405) == "12g 4s 5c");

    if (g_fails) { std::printf("%d failure(s)\n", g_fails); return 1; }
    std::printf("test_enchanter_rules: all passed\n");
    return 0;
}
