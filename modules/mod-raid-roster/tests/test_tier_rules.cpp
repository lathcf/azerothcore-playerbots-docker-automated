// modules/mod-raid-roster/tests/test_tier_rules.cpp
// Standalone: g++ -std=c++17 -Wall -Wextra -Werror -o <out> modules/mod-raid-roster/tests/test_tier_rules.cpp && <out>
#include "../src/RaidRosterTierRules.h"
#include <cstdio>

using namespace RaidRosterTierRules;

static int g_fails = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); ++g_fails; } } while (0)

int main()
{
    // Within one source every gate must pass -> highest wins.
    { uint8_t s = 0; FoldSource(s, 1); FoldSource(s, 4); FoldSource(s, 0); CHECK(s == 4); }
    // Across sources any one suffices -> lowest wins; unset until the first source.
    { uint8_t i = kTierUnset; FoldItem(i, 4); FoldItem(i, 1); FoldItem(i, 6); CHECK(i == 1); }
    { uint8_t i = kTierUnset; CHECK(i == kTierUnset); }

    // Level-band fallback (BookTier's bands).
    CHECK(LevelBandTier(0) == 0);
    CHECK(LevelBandTier(60) == 0);
    CHECK(LevelBandTier(61) == kTierPreTbc);
    CHECK(LevelBandTier(70) == kTierPreTbc);
    CHECK(LevelBandTier(71) == kTierWotlk);
    CHECK(LevelBandTier(80) == kTierWotlk);

    // Gate: tier <= cap; the IP-disabled sentinel allows everything.
    CHECK(Allowed(1, 2));
    CHECK(Allowed(2, 2));
    CHECK(!Allowed(4, 2));
    CHECK(Allowed(17, kTierUncapped));

    std::unordered_map<uint32_t, QuestFacts> f;
    { QuestFacts q; q.hasStarter = true; f[100] = q; }                              // open-world quest
    { QuestFacts q; q.hasStarter = true; q.sortTier = 4; f[101] = q; }              // AQ40 sort zone
    { QuestFacts q; q.hasStarter = true; q.prevQuest = 101; f[102] = q; }           // chain inherits 4
    { QuestFacts q; f[103] = q; }                                                   // no starter (<UNUSED>)
    { QuestFacts q; q.hasStarter = true; q.overrideTier = 4; f[104] = q; }          // authored override
    { QuestFacts q; q.hasStarter = true; q.prevQuest = 106; f[105] = q; }           // cycle 105 <-> 106
    { QuestFacts q; q.hasStarter = true; q.prevQuest = 105; q.condTier = 2; f[106] = q; }
    { QuestFacts q; q.hasStarter = true; q.starterTier = 1; q.reqItemTier = 3; q.repTier = 4; f[107] = q; }
    { QuestFacts q; q.hasStarter = true; q.prevQuest = 999; f[108] = q; }           // prev unknown -> 0
    { QuestFacts q; q.sortTier = 6; f[109] = q; }                                   // starterless prereq...
    { QuestFacts q; q.hasStarter = true; q.prevQuest = 109; f[110] = q; }           // ...still raises its follow-up
    { QuestFacts q; q.hasStarter = true; q.prevQuest = 111; q.sortTier = 1; f[111] = q; } // self-loop

    QuestTierResolver r(f);
    CHECK(r.IsSource(100));
    CHECK(r.Tier(100) == 0);
    CHECK(r.Tier(101) == 4);
    CHECK(r.Tier(102) == 4);
    CHECK(!r.IsSource(103));
    CHECK(!r.IsSource(555));                       // unknown quest
    CHECK(r.Tier(104) == 4);
    CHECK(r.Tier(105) == 2);                       // cycle terminates
    CHECK(r.Tier(106) == 2);
    CHECK(r.Tier(107) == 4);
    CHECK(r.Tier(108) == 0);
    CHECK(!r.IsSource(109));
    CHECK(r.Tier(110) == 6);
    CHECK(r.Tier(111) == 1);
    CHECK(r.Tier(555) == 0);

    if (g_fails == 0)
        std::printf("test_tier_rules: all passed\n");
    return g_fails == 0 ? 0 : 1;
}
