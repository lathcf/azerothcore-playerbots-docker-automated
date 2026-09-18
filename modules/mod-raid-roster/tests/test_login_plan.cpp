// modules/mod-raid-roster/tests/test_login_plan.cpp
// Standalone: g++ -std=c++17 -Wall -Wextra -Werror -o <out> modules/mod-raid-roster/tests/test_login_plan.cpp && <out>
#include "../src/RaidRosterLoginPlan.h"
#include <cstdio>

static int g_fails = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); ++g_fails; } } while (0)

static RoleCounts const kFullAvail = { 4, 9, 27 };   // whole roster offline, one era band

// RAID_SUBCOMPS values (bots only, N-1): 5={1,1,2} 10={2,2,5} 25={2,6,16} 40={4,8,27}
static RoleCounts const T5  = LoginTargetComp(1, 1, 2);
static RoleCounts const T40 = LoginTargetComp(4, 8, 27);

int main()
{
    // LoginTargetComp adds the master's implicit dps seat.
    CHECK(T5  == (RoleCounts{ 1, 1, 3 }));
    CHECK(T40 == (RoleCounts{ 4, 8, 28 }));

    // 1. Solo dps player, login 5 -> today's comp, unchanged.
    { LoginPlan p = ComputeLoginPlan(5, T5, { 0, 0, 1 }, kFullAvail);
      CHECK(p.need == (RoleCounts{ 1, 1, 2 })); CHECK(p.surplus == 0); CHECK(p.shortfall == 0); }

    // 2. Solo tank player, login 5 -> the tank bot is dropped and the seat goes to dps.
    { LoginPlan p = ComputeLoginPlan(5, T5, { 1, 0, 0 }, kFullAvail);
      CHECK(p.need == (RoleCounts{ 0, 1, 3 })); }

    // 3. Solo tank player, login 40 -> dps target 28 exceeds the 27 offline dps; the freed seat
    //    backfills to the 9th healer (matches the pre-change behaviour exactly).
    { LoginPlan p = ComputeLoginPlan(40, T40, { 1, 0, 0 }, kFullAvail);
      CHECK(p.need == (RoleCounts{ 3, 9, 27 })); CHECK(p.shortfall == 0); }

    // 4. Party of 2: master dps + human healer, login 5 -> 1 tank + 2 dps, no healer.
    { LoginPlan p = ComputeLoginPlan(5, T5, { 0, 1, 1 }, kFullAvail);
      CHECK(p.need == (RoleCounts{ 1, 0, 2 })); CHECK(p.surplus == 0); }

    // 5. Over-subscribed role: 3 healers + master dps (4 present), login 5 -> exactly 1 tank.
    //    Raw deficits are {1,0,2} (3 seats) but only 1 seat is open: trim dps first.
    { LoginPlan p = ComputeLoginPlan(5, T5, { 0, 3, 1 }, kFullAvail);
      CHECK(p.need == (RoleCounts{ 1, 0, 0 })); CHECK(p.surplus == 0); }

    // 6. Already 10 present, login 5 -> nothing to add, 5 surplus.
    { LoginPlan p = ComputeLoginPlan(5, T5, { 2, 2, 6 }, kFullAvail);
      CHECK(p.need == (RoleCounts{ 0, 0, 0 })); CHECK(p.surplus == 5); CHECK(p.shortfall == 0); }

    // 7. Re-running the same login: group already at target -> all zero.
    { LoginPlan p = ComputeLoginPlan(5, T5, { 1, 1, 3 }, kFullAvail);
      CHECK(p.need == (RoleCounts{ 0, 0, 0 })); CHECK(p.surplus == 0); CHECK(p.shortfall == 0); }

    // 8. Roster cap with backfill: no tanks/healers offline -> dps fills all 4 open seats.
    { LoginPlan p = ComputeLoginPlan(5, T5, { 0, 0, 1 }, { 0, 0, 27 });
      CHECK(p.need == (RoleCounts{ 0, 0, 4 })); CHECK(p.shortfall == 0); }

    // 9. Roster exhausted: only 1 offline dps -> 1 added, 3 seats short.
    { LoginPlan p = ComputeLoginPlan(5, T5, { 0, 0, 1 }, { 0, 0, 1 });
      CHECK(p.need == (RoleCounts{ 0, 0, 1 })); CHECK(p.shortfall == 3); }

    // 10. Backfill order is dps -> heal -> tank: dps exhausted, both heal and tank available -> heal.
    { LoginPlan p = ComputeLoginPlan(5, T5, { 0, 0, 1 }, { 4, 9, 2 });
      CHECK(p.need == (RoleCounts{ 1, 1, 2 }));   // no cap bites here
      LoginPlan q = ComputeLoginPlan(5, T5, { 0, 0, 1 }, { 4, 9, 1 });
      CHECK(q.need == (RoleCounts{ 1, 2, 1 })); } // dps capped at 1, freed seat -> heal, not tank

    if (g_fails) { std::printf("%d check(s) FAILED\n", g_fails); return 1; }
    std::printf("all login-plan checks passed\n");
    return 0;
}
