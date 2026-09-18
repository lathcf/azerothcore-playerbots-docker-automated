// modules/mod-battleground-bots/tests/test_killrank.cpp
// Standalone: g++ -std=c++17 -Wall -Wextra -Werror -o /tmp/krtest modules/mod-battleground-bots/tests/test_killrank.cpp && /tmp/krtest
#include "../src/BgKillRank.h"
#include <cstdio>

using namespace BgKillRank;
static int g_fails = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); ++g_fails; } } while (0)

static Cand C(bool fc, bool healer, bool unreach, float hp, bool peel, float dist, bool hint = false,
              bool victim = false, bool capturing = false, bool vehicle = false, bool attackingMe = false)
{ Cand c; c.isFc = fc; c.healer = healer; c.unreachable = unreach; c.hpPct = hp; c.attackingProtectee = peel; c.dist = dist; c.isHint = hint; c.isVictim = victim; c.capturing = capturing; c.vehicle = vehicle; c.attackingMe = attackingMe; return c; }

// The three situations the ranking has to tell apart: a ranged bot (a healer 35y out is a real
// target), a melee bot (12y or it is a walk, not a kill), and a bot already being beaten on.
static RankCtx Ctx(float healerMaxDist, bool underPressure = false, bool healerPriority = true)
{ RankCtx x; x.healerPriority = healerPriority; x.healerMaxDist = healerMaxDist; x.underPressure = underPressure; return x; }

int main()
{
    RankCtx const ranged    = Ctx(35.0f);
    RankCtx const melee     = Ctx(12.0f);
    RankCtx const pressured = Ctx(35.0f, true);
    RankCtx const noHealers = Ctx(35.0f, false, false);

    CHECK(Tier(C(true,  false, false, 100, false, 10), ranged) == 1);
    CHECK(Tier(C(false, true,  false, 100, false, 10), ranged) == 3);
    CHECK(Tier(C(false, true,  true,  100, false, 10), ranged) == 0);    // unreachable healer: skip the tier entirely
    CHECK(Tier(C(false, true,  false, 100, false, 10), noHealers) == 0); // healer priority off
    CHECK(Tier(C(false, false, false, 30,  false, 10), ranged) == 4);
    CHECK(Tier(C(false, false, false, 90,  true,  10), ranged) == 5);
    CHECK(Tier(C(false, false, false, 90,  false, 10), ranged) == 0);

    // FC beats a healer; healer beats low HP; unreachable healer at low HP still ranks by HP.
    CHECK(Best({ C(false, true, false, 100, false, 5), C(true, false, false, 100, false, 30) }, ranged) == 1);
    CHECK(Best({ C(false, false, false, 20, false, 5), C(false, true, false, 100, false, 30) }, ranged) == 1);
    CHECK(Best({ C(false, true, true, 20, false, 5), C(false, false, false, 90, true, 30) }, ranged) == 0);
    // Tie inside a tier: the team hint wins, then distance.
    CHECK(Best({ C(false, true, false, 100, false, 5), C(false, true, false, 100, false, 30, true) }, ranged) == 1);
    CHECK(Best({ C(false, true, false, 100, false, 25), C(false, true, false, 100, false, 10) }, ranged) == 1);
    // Commitment tie-break: within a tier, the bot's CURRENT victim beats a nearer newcomer.
    CHECK(Best({ C(false, true, false, 100, false, 10), C(false, true, false, 100, false, 30, false, true) }, ranged) == 1);
    // ...but the team hint still outranks commitment.
    CHECK(Best({ C(false, true, false, 100, false, 30, true), C(false, true, false, 100, false, 10, false, true) }, ranged) == 0);
    // Nothing qualifies => -1 (stock dps assist keeps its target).
    CHECK(Best({ C(false, false, false, 90, false, 5) }, ranged) == -1);
    CHECK(Best({}, ranged) == -1);

    // A node/banner capture channeller is tier 1 (same as the FC) and beats a healer outright,
    // even a nearer one — any hit interrupts the cast, so it is the highest-value interrupt.
    CHECK(Tier(C(false, false, false, 100, false, 10, false, false, true), ranged) == 1);
    CHECK(Best({ C(false, true, false, 100, false, 5), C(false, false, false, 100, false, 30, false, false, true) }, ranged) == 1);
    // Capturer vs FC: same tier, so the ordinary hint/victim/distance rules decide.
    CHECK(Best({ C(true, false, false, 100, false, 30), C(false, false, false, 100, false, 10, false, false, true) }, ranged) == 1);
    CHECK(Best({ C(true, false, false, 100, false, 30, true), C(false, false, false, 100, false, 10, false, false, true) }, ranged) == 0);

    // A hostile siege vehicle is tier 1 too: it is the only unit that breaks a keep gate, and its
    // driver is untargetable, so it beats a healer outright even when the healer is much nearer.
    CHECK(Tier(C(false, false, false, 100, false, 10, false, false, false, true), ranged) == 1);
    CHECK(Best({ C(false, true, false, 100, false, 5), C(false, false, false, 100, false, 30, false, false, false, true) }, ranged) == 1);
    // Vehicle vs FC: same tier, so the ordinary hint/victim/distance rules decide.
    CHECK(Best({ C(true, false, false, 100, false, 30), C(false, false, false, 100, false, 10, false, false, false, true) }, ranged) == 1);
    CHECK(Best({ C(true, false, false, 100, false, 30, true), C(false, false, false, 100, false, 10, false, false, false, true) }, ranged) == 0);

    // --- 2026-09-15 realism rework -------------------------------------------------------------
    // Healer reach. A melee bot does not cross the field for a healer 30y away (the live
    // complaint); the same healer is a fine target for a ranged bot, and a melee bot still takes
    // one standing next to it.
    CHECK(Tier(C(false, true, false, 100, false, 30), melee) == 0);
    CHECK(Best({ C(false, true, false, 100, false, 30) }, melee) == -1);
    CHECK(Tier(C(false, true, false, 100, false, 10), melee) == 3);
    CHECK(Tier(C(false, true, false, 100, false, 30), ranged) == 3);

    // Under pressure the healer tier is off entirely, even for a healer in melee range...
    CHECK(Tier(C(false, true, false, 100, false, 10), pressured) == 0);
    CHECK(Best({ C(false, true, false, 100, false, 10) }, pressured) == -1);
    // ...and whoever is swinging at us is tier 2 — ahead of a perfectly reachable healer even when
    // we are NOT under pressure yet (fight back first, chase the healer after).
    CHECK(Tier(C(false, false, false, 100, false, 20, false, false, false, false, true), ranged) == 2);
    CHECK(Best({ C(false, true, false, 100, false, 5),
                 C(false, false, false, 100, false, 20, false, false, false, false, true) }, ranged) == 1);

    // Commitment: a target already below the low-HP line is finished, not traded for a healer...
    CHECK(Best({ C(false, true, false, 100, false, 5),
                 C(false, false, false, 20, false, 20, false, true) }, ranged) == 1);
    // ...but a tier-1 candidate (FC here) is time-critical and still wins.
    CHECK(Best({ C(true, false, false, 100, false, 30),
                 C(false, false, false, 20, false, 20, false, true),
                 C(false, true, false, 100, false, 5) }, ranged) == 0);
    // Commitment also survives an attacker on us and holds with healer priority off.
    CHECK(Best({ C(false, false, false, 100, false, 5, false, false, false, false, true),
                 C(false, false, false, 20, false, 20, false, true) }, ranged) == 1);
    CHECK(Best({ C(false, false, false, 20, false, 20, false, true) }, noHealers) == 0);
    CHECK(CommitIndex({ C(false, false, false, 20, false, 20, false, true) }) == 0);
    // A full-HP current victim is NOT a commitment — only the tie-break above applies.
    CHECK(CommitIndex({ C(false, false, false, 100, false, 20, false, true) }) == -1);

    if (g_fails) { std::printf("%d check(s) FAILED\n", g_fails); return 1; }
    std::printf("all kill-rank checks passed\n");
    return 0;
}
