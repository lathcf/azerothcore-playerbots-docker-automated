// Standalone: g++ -std=c++17 -Wall -Wextra -Werror -o /tmp/nx40hm tests/test_naxx40_horsemen_rules.cpp && /tmp/nx40hm
// Run from the overlay repo root AFTER the Four Horsemen patch is applied to the fork (the header lives there).
#include "../azerothcore-wotlk/modules/mod-playerbots/src/Ai/Raid/Naxx/Naxx40HorsemenRules.h"
#include <cmath>
#include <cstdio>
#include <vector>

using namespace Naxx40HorsemenRules;

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static constexpr Horse K = Horse::Korthazz;
static constexpr Horse M = Horse::Mograine;
static constexpr Horse B = Horse::Blaumeux;
static constexpr Horse Z = Horse::Zeliek;
static constexpr Horse N = Horse::None;

// Pin every constant to the brief's values.
static_assert(ENTRY[0] == 351039 && ENTRY[1] == 351037 && ENTRY[2] == 351040 && ENTRY[3] == 351038);
static_assert(MARK_SPELL[0] == 28832 && MARK_SPELL[1] == 28834 && MARK_SPELL[2] == 28833 && MARK_SPELL[3] == 28835);
static_assert(SPIRIT_ENTRY[0] == 16778 && SPIRIT_ENTRY[1] == 16775 && SPIRIT_ENTRY[2] == 16776 &&
              SPIRIT_ENTRY[3] == 16777);
static_assert(VOID_ZONE_ENTRY == 16697);
static_assert(BOSS_STATE_ID == 12);
static_assert(MARK_RADIUS == 55.0f);
static_assert(LEAVE_AT_STACKS == 3);
static_assert(READY_AT_STACKS == 2);

static std::array<bool, 4> const ALL_ALIVE = {true, true, true, true};

static bool Near(float a, float b)
{
    return std::fabs(a - b) < 0.001f;
}

static std::vector<Member> Tanks(uint64_t n)
{
    std::vector<Member> v;
    for (uint64_t g = n; g >= 1; --g)       // reverse order: Assign must sort by guid
        v.push_back(Member{g, Role::Tank, true});
    return v;
}

static void TestHorseOfEntry()
{
    CHECK(HorseOfEntry(351039) == K);
    CHECK(HorseOfEntry(351037) == M);
    CHECK(HorseOfEntry(351040) == B);
    CHECK(HorseOfEntry(351038) == Z);
    CHECK(HorseOfEntry(16063) == N);        // Sir Zeliek's WotLK entry: not the Naxx40 horseman
    CHECK(HorseOfEntry(0) == N);
}

static void TestAssignTanks()
{
    Assignment a = Assign(Tanks(8));
    CHECK(a.active[0] == 1 && a.active[1] == 2 && a.active[2] == 3 && a.active[3] == 4);
    CHECK(a.resting[0] == 5 && a.resting[1] == 6 && a.resting[2] == 7 && a.resting[3] == 8);
    CHECK(a.spareTanks.empty());

    a = Assign(Tanks(6));
    CHECK(a.active[0] == 1 && a.active[3] == 4);
    CHECK(a.resting[0] == 5 && a.resting[1] == 6 && a.resting[2] == 0 && a.resting[3] == 0);

    a = Assign(Tanks(10));
    CHECK(a.spareTanks == (std::vector<uint64_t>{9, 10}));
}

static void TestAssignHealersAndDps()
{
    std::vector<Member> m;
    for (uint64_t g = 9; g >= 1; --g)
        m.push_back(Member{g, Role::Healer, false});
    Assignment a = Assign(m);
    CHECK(a.healers[0] == (std::vector<uint64_t>{1, 5}));
    CHECK(a.healers[1] == (std::vector<uint64_t>{2, 6}));
    CHECK(a.healers[3] == (std::vector<uint64_t>{4, 8}));
    CHECK(a.healers[4] == (std::vector<uint64_t>{9}));

    // 4 melee (guids 10..13) + 4 ranged (guids 1..4): each half gets 2 melee.
    m.clear();
    for (uint64_t g = 1; g <= 4; ++g)
        m.push_back(Member{g, Role::Dps, false});
    for (uint64_t g = 10; g <= 13; ++g)
        m.push_back(Member{g, Role::Dps, true});
    a = Assign(m);
    CHECK(a.dpsHalf[0].size() == 4 && a.dpsHalf[1].size() == 4);
    for (int h = 0; h < 2; ++h)
    {
        int melee = 0;
        for (uint64_t g : a.dpsHalf[h])
            melee += (g >= 10) ? 1 : 0;
        CHECK(melee == 2);
    }
    CHECK(a.dpsHalf[0][0] == 10 && a.dpsHalf[1][0] == 11);    // melee first, then guid
}

static void TestChoose()
{
    // The K <-> M trade.
    CHECK(Choose(DPS_ORDER[0], K, {3, 0, 0, 0}, ALL_ALIVE, true) == M);
    CHECK(Choose(DPS_ORDER[0], K, {2, 0, 0, 0}, ALL_ALIVE, true) == K);

    // Mograine dead: the half due on him opens elsewhere.
    std::array<bool, 4> const mDead = {true, false, true, true};
    CHECK(Choose(DPS_ORDER[1], M, {0, 0, 0, 0}, mDead, true) == K);
    CHECK(Choose(DPS_ORDER[0], K, {3, 0, 0, 0}, mDead, true) == B);

    // Never a back horseman while the front lives.
    CHECK(Choose(DPS_ORDER[0], K, {3, 1, 0, 0}, ALL_ALIVE, true) == N);

    // Last horseman alone: one turn on, off until the stacks drop.
    std::array<bool, 4> const onlyZ = {false, false, false, true};
    CHECK(Choose(DPS_ORDER[0], Z, {0, 0, 0, 3}, onlyZ, true) == N);
    CHECK(Choose(DPS_ORDER[0], N, {0, 0, 0, 0}, onlyZ, true) == Z);
    CHECK(Choose(DPS_ORDER[0], N, {0, 0, 0, 2}, onlyZ, true) == N);

    // Melee DPS never work Zeliek: his slot is None, the rest keep their place.
    CHECK(WithoutZeliek(DPS_ORDER[1]) == (Order{M, K, N, B}));
    CHECK(WithoutZeliek(DPS_ORDER[0]) == (Order{K, M, B, N}));
    CHECK(Choose(WithoutZeliek(DPS_ORDER[0]), B, {0, 0, 3, 0}, {false, false, true, true}, true) == N);

    // Healers (frontGate false): the back pairs work Blaumeux <-> Zeliek while the front lives.
    CHECK(Choose(HealerOrder(2, ALL_ALIVE), N, {0, 0, 0, 0}, ALL_ALIVE, false) == B);
    CHECK(Choose(HealerOrder(2, ALL_ALIVE), B, {0, 0, 3, 0}, ALL_ALIVE, false) == Z);

    // The floater (group 4) follows DPS half 0 and keeps the front gate.
    CHECK(Choose(HealerOrder(4, ALL_ALIVE), K, {3, 1, 0, 0}, ALL_ALIVE, true) == N);
}

static void TestLastFrontUncapped()
{
    // Spec §4.2f (pull 11): with one front horseman dead and doable marks, the survivor has no stack cap.
    std::array<bool, 4> const mDead = {true, false, true, true};
    CHECK(LastFront(mDead) == K);
    CHECK(LastFront(ALL_ALIVE) == N);
    CHECK(LastFront({false, false, true, true}) == N);
    CHECK(LastFront({false, true, true, true}) == M);

    // On him at any stack count: stay.
    CHECK(Choose(DPS_ORDER[0], K, {3, 0, 0, 0}, mDead, true, LEAVE_AT_STACKS, K) == K);
    CHECK(Choose(DPS_ORDER[0], K, {9, 0, 0, 0}, mDead, true, LEAVE_AT_STACKS, K) == K);
    // Carrying his marks elsewhere (the half off the dead horseman, a healer at the safe spot): join him.
    CHECK(Choose(DPS_ORDER[1], M, {5, 1, 0, 0}, mDead, true, LEAVE_AT_STACKS, K) == K);
    CHECK(Choose(HealerOrder(0, mDead), N, {5, 0, 0, 0}, mDead, false, LEAVE_AT_STACKS, K) == K);
    CHECK(Choose(HealerOrder(1, mDead), N, {4, 0, 0, 0}, mDead, false, LEAVE_AT_STACKS, K) == K);
    // A back pair never reaches him: their order has no front horseman.
    CHECK(Choose(HealerOrder(2, mDead), B, {5, 0, 0, 0}, mDead, false, LEAVE_AT_STACKS, K) == B);
    // Without the uncap (marks at full strength) the old rule stands.
    CHECK(Choose(DPS_ORDER[1], M, {5, 1, 0, 0}, mDead, true) == Z);
}

static void TestHealerOrder()
{
    CHECK(HealerOrder(0, ALL_ALIVE) == (Order{K, M, N, N}));
    CHECK(HealerOrder(1, ALL_ALIVE) == (Order{M, K, N, N}));
    CHECK(HealerOrder(2, ALL_ALIVE) == (Order{B, Z, N, N}));
    CHECK(HealerOrder(3, ALL_ALIVE) == (Order{Z, B, N, N}));
    CHECK(HealerOrder(0, {false, false, true, true}) == (Order{B, Z, N, N}));
    CHECK(HealerOrder(1, {false, false, true, true}) == (Order{Z, B, N, N}));
    CHECK(HealerOrder(0, {true, false, true, true}) == (Order{K, M, N, N}));   // one front alive: unchanged
    CHECK(HealerOrder(4, ALL_ALIVE) == DPS_ORDER[0]);
}

static void TestTankDuty()
{
    CHECK(TankDutyFor(true, 3, false, true, 0) == TankDuty::Leave);
    CHECK(TankDutyFor(true, 3, true, true, 0) == TankDuty::Hold);      // still the victim
    CHECK(TankDutyFor(true, 3, false, false, 0) == TankDuty::Hold);    // no partner to take over
    CHECK(TankDutyFor(true, 3, true, true, 3) == TankDuty::Hold);
    CHECK(TankDutyFor(true, 2, false, true, 0) == TankDuty::Hold);
    CHECK(TankDutyFor(false, 0, false, true, 2) == TankDuty::Ready);
    CHECK(TankDutyFor(false, 0, false, true, 3) == TankDuty::Relieve);
    CHECK(TankDutyFor(false, 0, false, false, 0) == TankDuty::Relieve);
    CHECK(TankDutyFor(false, 0, false, true, 1) == TankDuty::Rest);
    // A resting tank still carrying 3+ marks of this horseman never steps in, partner or not (pull 1).
    CHECK(TankDutyFor(false, 3, false, false, 0) == TankDuty::Rest);
    CHECK(TankDutyFor(false, 4, false, true, 3) == TankDuty::Rest);
    CHECK(TankDutyFor(false, 2, false, false, 0) == TankDuty::Relieve);
}

static void TestRefillPairs()
{
    uint64_t active[4] = {1, 2, 3, 4};
    uint64_t resting[4] = {5, 6, 7, 8};
    std::vector<uint64_t> freeTanks = {9, 10};
    RefillPairs(active, resting, freeTanks, ALL_ALIVE, {false, true, true, true}, {0, 0, 0, 0});
    CHECK(active[0] == 5 && resting[0] == 9);
    CHECK(active[1] == 2 && resting[1] == 6);
    CHECK(freeTanks == (std::vector<uint64_t>{10}));

    // A dead horseman's slots are not refilled; empty resting slots fill in Horse order.
    uint64_t a2[4] = {1, 2, 3, 4};
    uint64_t r2[4] = {0, 0, 7, 0};
    std::vector<uint64_t> f2 = {9};
    RefillPairs(a2, r2, f2, {false, true, true, true}, ALL_ALIVE, {0, 0, 0, 0});
    CHECK(r2[0] == 0 && r2[1] == 9 && r2[3] == 0);
    CHECK(f2.empty());

    // Pull 1: the active died while the resting tank carried 4 marks. A free tank takes the horseman; the stacked
    // tank stays resting.
    uint64_t a3[4] = {1, 2, 3, 4};
    uint64_t r3[4] = {5, 6, 7, 8};
    std::vector<uint64_t> f3 = {9};
    RefillPairs(a3, r3, f3, ALL_ALIVE, {false, true, true, true}, {4, 0, 0, 0});
    CHECK(a3[0] == 9 && r3[0] == 5);
    CHECK(f3.empty());

    // No free tank either: the horseman has no active tank until the resting tank's marks run out.
    uint64_t a4[4] = {1, 2, 3, 4};
    uint64_t r4[4] = {5, 6, 7, 8};
    std::vector<uint64_t> f4;
    RefillPairs(a4, r4, f4, ALL_ALIVE, {false, true, true, true}, {3, 0, 0, 0});
    CHECK(a4[0] == 0 && r4[0] == 5);
    RefillPairs(a4, r4, f4, ALL_ALIVE, {false, true, true, true}, {2, 0, 0, 0});  // marks fading: promoted
    CHECK(a4[0] == 5 && r4[0] == 0);
}

static void TestHoldSwitch()
{
    std::array<bool, 4> const allSettled = {true, true, true, true};
    // Both horsemen settled: the switch goes ahead.
    CHECK(HoldSwitch(K, M, {3, 0, 0, 0}, allSettled) == M);
    // The horseman being left still on his 3-stack tank (relief not taunted yet): stay and keep working him.
    CHECK(HoldSwitch(K, M, {3, 0, 0, 0}, {false, true, true, true}) == K);
    // The horseman being joined is not settled: stay too.
    CHECK(HoldSwitch(K, M, {3, 0, 0, 0}, {true, false, true, true}) == K);
    // A 4th mark landed while waiting: go anyway, to the safe spot while the target is still unsettled.
    CHECK(HoldSwitch(K, M, {4, 0, 0, 0}, {false, true, true, true}) == M);
    CHECK(HoldSwitch(K, M, {4, 0, 0, 0}, {false, false, true, true}) == N);
    // To the safe spot (Choose found nothing): only waits on the horseman being left.
    CHECK(HoldSwitch(K, N, {3, 1, 0, 0}, {false, true, true, true}) == K);
    CHECK(HoldSwitch(K, N, {3, 1, 0, 0}, allSettled) == N);
    // From the safe spot or no change: never held.
    CHECK(HoldSwitch(N, K, {0, 0, 0, 0}, {false, false, false, false}) == K);
    CHECK(HoldSwitch(K, K, {1, 0, 0, 0}, {false, false, false, false}) == K);
}

static void TestHorseSettled()
{
    CHECK(HorseSettled(false, false, false, 0));   // dead: nothing to wait for
    CHECK(HorseSettled(true, true, true, 0));      // on his fresh active tank
    CHECK(HorseSettled(true, true, true, 2));
    CHECK(!HorseSettled(true, true, true, 3));     // still on the tank that is about to hand him over
    CHECK(!HorseSettled(true, false, true, 0));    // no active tank
    CHECK(!HorseSettled(true, true, false, 0));    // loose on someone else
}

static void TestHealerMustStay()
{
    CHECK(HealerMustStay(true, true, false, 3));    // last healer on a live, tanked horseman: stays through the 4th mark
    CHECK(HealerMustStay(true, true, false, 0));
    CHECK(!HealerMustStay(true, true, false, 4));   // a 4th mark landed: goes
    CHECK(!HealerMustStay(true, true, true, 3));    // another healer covers his tank: goes
    CHECK(!HealerMustStay(false, true, false, 3));  // horseman dead
    CHECK(!HealerMustStay(true, false, false, 3));  // no living active tank to keep up
}

static void TestHealerEscortAndStop()
{
    CHECK(HealerEscorts(PARK_RANGE + 0.1f));   // the opening drag: escort the tank
    CHECK(!HealerEscorts(PARK_RANGE));         // parked (Blaumeux is measured from her current tank point)
    CHECK(!HealerEscorts(6.0f));

    std::array<bool, 4> const alive{true, true, true, true};
    std::array<float, 4> far{20.0f, 80.0f, 90.0f, 90.0f};   // in K's circle only
    CHECK(MarkSafe(K, far, alive, {3, 0, 0, 0}));          // its own horseman's marks never block
    CHECK(MarkSafe(M, far, alive, {2, 0, 0, 0}));          // in K's circle at 2 K stacks: the next is only the 3rd
    CHECK(!MarkSafe(M, far, alive, {3, 0, 0, 0}));         // ... at 3 it would be the 4th
    std::array<float, 4> edge{MARK_RADIUS + MARK_MARGIN + 0.1f, 80.0f, 90.0f, 90.0f};
    CHECK(MarkSafe(M, edge, alive, {4, 0, 0, 0}));         // just outside K's circle
    std::array<bool, 4> const kDead{false, true, true, true};
    CHECK(MarkSafe(M, far, kDead, {4, 0, 0, 0}));          // a dead horseman marks nobody (his spirit is §4.4)

    CHECK(HealerStopsToHeal(true, 70.0f, 30.0f, true));
    CHECK(!HealerStopsToHeal(true, TANK_HEAL_PCT, 30.0f, true));  // topped up: keep going
    CHECK(!HealerStopsToHeal(true, 70.0f, HEAL_REACH + 0.1f, true));
    CHECK(!HealerStopsToHeal(true, 70.0f, 30.0f, false));        // would take a 4th mark here
    CHECK(!HealerStopsToHeal(false, 70.0f, 30.0f, true));
}

static void TestHealerStagger()
{
    CHECK(HealerLeaveAt(0, true) == 1);                 // seat 0 leaves its first horseman on mark 1
    CHECK(HealerLeaveAt(1, true) == 2);                 // seat 1 on mark 2: neither on the tanks' mark 3
    CHECK(HealerLeaveAt(0, false) == LEAVE_AT_STACKS);  // then full 3-mark turns, one mark apart
    CHECK(HealerLeaveAt(1, false) == LEAVE_AT_STACKS);
    CHECK(HealerLeaveAt(2, true) == LEAVE_AT_STACKS);   // no third seat in a group

    Order const g0 = HealerOrder(0, ALL_ALIVE);
    CHECK(Choose(g0, K, {0, 0, 0, 0}, ALL_ALIVE, false, 1) == K);
    CHECK(Choose(g0, K, {1, 0, 0, 0}, ALL_ALIVE, false, 1) == M);
    CHECK(Choose(g0, K, {1, 0, 0, 0}, ALL_ALIVE, false, 2) == K);
    CHECK(Choose(g0, K, {2, 0, 0, 0}, ALL_ALIVE, false, 2) == M);
    CHECK(Choose(g0, M, {0, 2, 0, 0}, ALL_ALIVE, false) == M);  // default threshold unchanged
}

static void TestDpsStagger()
{
    CHECK(DpsLeaveAt(0, true) == 2);                  // seat 0 leaves its first horseman on mark 2
    CHECK(DpsLeaveAt(1, true) == LEAVE_AT_STACKS);    // seat 1 on mark 3, with the tanks
    CHECK(DpsLeaveAt(0, false) == LEAVE_AT_STACKS);   // then full 3-mark turns, one mark apart
    CHECK(DpsLeaveAt(1, false) == LEAVE_AT_STACKS);
    CHECK(DpsLeaveAt(0xFF, true) == LEAVE_AT_STACKS); // not seated: the old rule

    // Half 0 seat 0 on Korth'azz at K2 moves on to Mograine; seat 1 stays until K3.
    CHECK(Choose(DPS_ORDER[0], K, {2, 0, 0, 0}, ALL_ALIVE, true, DpsLeaveAt(0, true)) == M);
    CHECK(Choose(DPS_ORDER[0], K, {2, 0, 0, 0}, ALL_ALIVE, true, DpsLeaveAt(1, true)) == K);
    CHECK(Choose(DPS_ORDER[0], K, {3, 0, 0, 0}, ALL_ALIVE, true, DpsLeaveAt(1, true)) == M);
}

static void TestSwapHandover()
{
    CHECK(TakesOver(0));
    CHECK(TakesOver(LEAVE_AT_STACKS - 1));
    CHECK(!TakesOver(LEAVE_AT_STACKS));      // pull 4: a 4-stack resting tank was handed Mograine back and died
    CHECK(!TakesOver(LEAVE_AT_STACKS + 1));

    CHECK(ReliefTauntsNow(4.0f, 3.0f, 5.0f));            // beside the active tank on the tank spot
    CHECK(!ReliefTauntsNow(20.0f, 3.0f, 5.0f));          // still walking in: a taunt from here drags him off his spot
    CHECK(ReliefTauntsNow(20.0f, PARK_RANGE + 0.1f, 5.0f));  // he is off his spot anyway: taunt from range
}

static void TestMeleeSpot()
{
    Pt const zs = MeleeSpot(Z, {0.0f, 0.0f}, {-2.0f, 0.0f});
    CHECK(zs.x > 0.0f);
    CHECK(Dist(zs, {-2.0f, 0.0f}) >= ZELIEK_TANK_GAP - 0.001f);
    CHECK(Near(zs.x, 10.0f) && Near(zs.y, 0.0f));                      // the smallest such distance (12 - 2)

    Pt const zm = MeleeSpot(Z, {0.0f, 0.0f}, {-5.0f, 0.0f});           // 5 + 3 < 12: pushed out to the gap
    CHECK(Near(zm.x, 7.0f) && Near(zm.y, 0.0f));
    Pt const zf = MeleeSpot(Z, {0.0f, 0.0f}, {-10.0f, 0.0f});          // far tank: plain MELEE_BEHIND
    CHECK(Near(zf.x, MELEE_BEHIND) && Near(zf.y, 0.0f));

    Pt const ks = MeleeSpot(K, {0.0f, 0.0f}, {-2.0f, 0.0f});
    CHECK(Near(ks.x, 3.0f) && Near(ks.y, 0.0f));
    Pt const bs = MeleeSpot(B, {10.0f, 10.0f}, {10.0f, 5.0f});
    CHECK(Near(bs.x, 10.0f) && Near(bs.y, 13.0f));
}

static void TestAvoidVoid()
{
    Pt const boss = {0.0f, 0.0f};
    Pt const s = AvoidVoid({3.0f, 0.0f}, boss, {{3.0f, 0.0f}});
    CHECK(Dist(s, {3.0f, 0.0f}) >= VOID_CLEAR);
    CHECK(Near(Dist(s, boss), 3.0f));

    Pt const clear = AvoidVoid({3.0f, 0.0f}, boss, {{20.0f, 0.0f}});
    CHECK(Near(clear.x, 3.0f) && Near(clear.y, 0.0f));
    Pt const none = AvoidVoid({3.0f, 0.0f}, boss, {});
    CHECK(Near(none.x, 3.0f) && Near(none.y, 0.0f));
}

static void TestPickBlauPoint()
{
    // A 3x3 grid, 5 y apart, index 0 at the centre (nearest the tank spot first, like the generated table).
    std::vector<Pt> const pts = {{0, 0}, {-5, 0}, {0, 5}, {-5, -5}, {-5, 5}, {-10, 0}, {0, 10}, {-10, -5}, {-10, 5}};
    // Clear: stay.
    CHECK(PickBlauPoint(pts, 0, {0.5f, 0.0f}, true, {}) == 0);
    CHECK(PickBlauPoint(pts, 0, {0.5f, 0.0f}, true, {{20.0f, 0.0f}}) == 0);
    // A zone under the tank on his point: step to the nearest clear point.
    size_t const n = PickBlauPoint(pts, 0, {0.0f, 0.0f}, true, {{0.5f, 0.0f}});
    CHECK(n != 0 && Dist(pts[n], {0.5f, 0.0f}) >= VOID_CLEAR && Near(Dist(pts[n], {0.0f, 0.0f}), 5.0f));
    // A zone near the tank only counts while he stands on the current point.
    CHECK(PickBlauPoint(pts, 0, {-20.0f, 0.0f}, false, {{-17.0f, 0.0f}}) == 0);
    // Pull 6: zones on many points; the first clear point is several steps away, never stay in a zone.
    std::vector<Pt> const zones = {{0, 0}, {-5, 0}, {0, 5}, {-5, 5}, {-5, -5}};
    size_t const far = PickBlauPoint(pts, 0, {0.0f, 0.0f}, true, zones);
    for (Pt const& z : zones)
        CHECK(Dist(pts[far], z) >= VOID_CLEAR);
    // All blocked: the point farthest from its nearest zone, not the current one.
    std::vector<Pt> leaveOne(pts.begin(), pts.end() - 1);
    leaveOne.push_back({-12.0f, 5.0f});   // the last point's zone sits 2 y off it: still blocked, but the best
    CHECK(PickBlauPoint(pts, 0, {0.0f, 0.0f}, true, leaveOne) == 8);
    // A stale index wraps to 0.
    CHECK(PickBlauPoint(pts, 42, {0.0f, 0.0f}, true, {}) == 0);
}

static void TestThreat()
{
    // Pre-cast veto with THREAT_HEADROOM (500): 450 + 500 = 950 > 900; 300 + 500 = 800 < 900; boundary 400 + 500 = 900
    // is not above it.
    CHECK(THREAT_HEADROOM == 500.0f);
    CHECK(ThreatBlocked(450.0f, 1000.0f));
    CHECK(!ThreatBlocked(300.0f, 1000.0f));
    CHECK(!ThreatBlocked(400.0f, 1000.0f));
    CHECK(ThreatBlocked(401.0f, 1000.0f));
    CHECK(!ThreatBlocked(50.0f, 0.0f));
    CHECK(!ThreatBlocked(5000.0f, 0.0f));
    // Hard stop on the plain ratio: above 0.90 only.
    CHECK(ThreatStop(901.0f, 1000.0f));
    CHECK(!ThreatStop(900.0f, 1000.0f));
    CHECK(!ThreatStop(450.0f, 1000.0f));
    CHECK(!ThreatStop(50.0f, 0.0f));
}

int main()
{
    TestHorseOfEntry();
    TestAssignTanks();
    TestAssignHealersAndDps();
    TestChoose();
    TestLastFrontUncapped();
    TestHealerOrder();
    TestTankDuty();
    TestRefillPairs();
    TestHoldSwitch();
    TestHorseSettled();
    TestHealerMustStay();
    TestHealerEscortAndStop();
    TestHealerStagger();
    TestDpsStagger();
    TestSwapHandover();
    TestMeleeSpot();
    TestAvoidVoid();
    TestPickBlauPoint();
    TestThreat();
    if (g_fail)
    {
        std::printf("%d failure(s)\n", g_fail);
        return 1;
    }
    std::printf("ALL PASS\n");
    return 0;
}
