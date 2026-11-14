// Standalone: g++ -std=c++17 -Wall -Wextra -Werror -o /tmp/nx40pw tests/test_naxx40_patchwerk_rules.cpp && /tmp/nx40pw
// Run from the overlay repo root AFTER the 0022 patch is applied to the fork (the header lives there).
#include "../azerothcore-wotlk/modules/mod-playerbots/src/Ai/Raid/Naxx/Naxx40PatchwerkRules.h"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <vector>

using namespace Naxx40PatchwerkRules;

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

// Pin the constants to the spec.
static_assert(PATCHWERK_ENTRY == 351028 && BOSS_STATE_ID == 0 && SPELL_HATEFUL_STRIKE == 41926);
static_assert(ENRAGE_MS == 420000 && OPENER_FAIL_OPEN_MS == 20000);
static_assert(REFRESH_INTERVAL_MS == 500 && DIAG_INTERVAL_MS == 5000);
static_assert(SOAKER_HEALERS_MAX == 8 && MT_HEALERS_MAX == 2 && MANA_REFILL_BELOW_PCT == 25);
static_assert(STRIKE_DROP_MIN == 4000);
static_assert(THREAT_HEADROOM == 1500.0f && SOAKER_LIMIT == 1.00f && SOAKER_STOP == 1.05f);
static_assert(DPS_LIMIT == 0.80f && DPS_STOP == 0.90f && HEAL_RANGE == 35.0f);
static_assert(SOAKER_FLOOR == 0.80f && SPARE_FLOOR == 0.90f && BOOST_MIN_DEFICIT == 1000.0f);
static_assert(PRECAST_CANCEL_MS == 300 && PRECAST_CANCEL_PCT == 95.0f);

enum : uint64_t { A = 1, B, C, D, E };

static TankInfo T(uint64_t g, bool alive, float threat, uint32_t hp = 8000, bool melee = true)
{
    return TankInfo{g, alive, threat, hp, melee};
}

static std::vector<ThreatEntry> Threat(std::vector<bool> const& melee)
{
    std::vector<ThreatEntry> v;
    uint64_t g = 1;
    for (bool m : melee)
        v.push_back(ThreatEntry{g++, 1000.0f, m});
    return v;
}

static void TestStrikePair()
{
    StrikePair p = StrikeCandidates(Threat({true, true, true, true}));
    CHECK(p.a == 2 && p.b == 3);
    p = StrikeCandidates(Threat({true, false, true, true}));
    CHECK(p.a == 3 && p.b == 4);
    p = StrikeCandidates(Threat({true, true}));
    CHECK(p.a == 2 && p.b == 0);
    p = StrikeCandidates(Threat({true}));
    CHECK(p.a == 1 && p.b == 0);
    p = StrikeCandidates(Threat({false, false}));
    CHECK(p.a == 0 && p.b == 0);
    p = StrikeCandidates({});
    CHECK(p.a == 0 && p.b == 0);
}

static void TestPriority()
{
    std::vector<uint64_t> const p = TankPriority({T(A, true, 0, 9000), T(B, true, 0, 8000), T(C, true, 0, 8500),
                                                  T(D, true, 0, 7000), T(E, false, 0, 9500)});
    CHECK((p == std::vector<uint64_t>{A, C, B, D}));
    std::vector<uint64_t> const q = TankPriority({T(C, true, 0, 8000), T(A, true, 0, 8000), T(B, true, 0, 8000)});
    CHECK((q == std::vector<uint64_t>{A, B, C}));
}

static std::vector<TankInfo> Four()
{
    return {T(A, true, 5000), T(B, true, 3000), T(C, true, 2000), T(D, true, 1000)};
}

static void TestRolesBasic()
{
    Roles const r = DeriveRoles({A, B, C, D}, Four(), A);
    CHECK(r.mt == A && r.soaker[0] == B && r.soaker[1] == C);
    CHECK((r.spares == std::vector<uint64_t>{D}));
}

static void TestRolesSoakerDies()
{
    std::vector<TankInfo> live = Four();
    live[1].alive = false;
    Roles const r = DeriveRoles({A, B, C, D}, live, A);
    CHECK(r.mt == A && r.soaker[0] == C && r.soaker[1] == D);
    CHECK(r.spares.empty());
}

static void TestRolesMtDies()
{
    std::vector<TankInfo> live = Four();
    live[0].alive = false;
    Roles const r = DeriveRoles({A, B, C, D}, live, B);
    CHECK(r.mt == B && r.soaker[0] == C && r.soaker[1] == D);
    CHECK(r.spares.empty());
}

static void TestRolesVictimNotTank()
{
    std::vector<TankInfo> live = {T(A, true, 4000), T(B, true, 3000), T(C, true, 9000), T(D, true, 1000)};
    Roles const r = DeriveRoles({A, B, C, D}, live, 777);
    CHECK(r.mt == C && r.soaker[0] == A && r.soaker[1] == B);
    CHECK((r.spares == std::vector<uint64_t>{D}));
}

// Pull 1: soakers follow LIVE threat, not the latched (max-health) priority.
static void TestRolesByLiveThreat()
{
    // B is first in priority (highest max health) but was never struck, so its threat starved: it is the spare.
    std::vector<TankInfo> live = {T(A, true, 9000), T(B, true, 500), T(C, true, 4000), T(D, true, 3000)};
    Roles r = DeriveRoles({A, B, C, D}, live, A);
    CHECK(r.mt == A && r.soaker[0] == C && r.soaker[1] == D);
    CHECK((r.spares == std::vector<uint64_t>{B}));

    // A resurrected tank comes back at 0 threat: it is the spare, behind a living spare with threat.
    live = {T(A, true, 9000), T(B, true, 0), T(C, true, 4000), T(D, true, 3000), T(E, true, 2000)};
    r = DeriveRoles({A, B, C, D, E}, live, A);
    CHECK(r.mt == A && r.soaker[0] == C && r.soaker[1] == D);
    CHECK((r.spares == std::vector<uint64_t>{E, B}));

    // Equal threat keeps priority order (the opener: everyone at 0).
    live = {T(A, true, 5000), T(B, true, 0), T(C, true, 0), T(D, true, 0)};
    r = DeriveRoles({A, B, C, D}, live, A);
    CHECK(r.soaker[0] == B && r.soaker[1] == C && (r.spares == std::vector<uint64_t>{D}));
    live = {T(A, true, 5000), T(B, true, 2000), T(C, true, 3000), T(D, true, 3000)};
    r = DeriveRoles({A, B, C, D}, live, A);
    CHECK(r.soaker[0] == C && r.soaker[1] == D && (r.spares == std::vector<uint64_t>{B}));

    // LowerSoaker is the third-highest tank.
    live = {T(A, true, 9000), T(B, true, 500), T(C, true, 4000), T(D, true, 3000)};
    r = DeriveRoles({A, B, C, D}, live, A);
    Cap const c = CapFor(CapRole::RangedDps, 99, r, live);
    CHECK(c.capped && c.refThreat == 3000.0f);
}

static void TestThreatBoost()
{
    // Revived soaker: lifted to 0.80 x the MT.
    std::vector<TankInfo> live = {T(A, true, 10000), T(B, true, 6000), T(C, true, 1000), T(D, true, 500)};
    Roles roles;
    roles.mt = A;
    roles.soaker[0] = B;
    roles.soaker[1] = C;
    roles.spares = {D};
    CHECK(ThreatBoostFor(C, roles, live) == 7000.0f);  // 8000 - 1000
    CHECK(ThreatBoostFor(B, roles, live) == 2000.0f);  // 8000 - 6000

    // Starved spare: lifted to 0.90 x the lower soaker (C, 1000 -> floor 900, deficit 400: under the threshold).
    CHECK(ThreatBoostFor(D, roles, live) == 0.0f);
    live[2].threat = 5000.0f;  // lower soaker now C at 5000 -> floor 4500
    CHECK(ThreatBoostFor(D, roles, live) == 4000.0f);

    // No boost under the deficit threshold.
    live = {T(A, true, 10000), T(B, true, 7100), T(C, true, 7001), T(D, true, 5402)};
    CHECK(ThreatBoostFor(B, roles, live) == 0.0f);   // deficit 900
    CHECK(ThreatBoostFor(C, roles, live) == 0.0f);   // deficit 999
    CHECK(ThreatBoostFor(D, roles, live) == 0.0f);   // 0.90 x 7001 - 5402 < 1000
    live[2].threat = 7000.0f;                        // deficit exactly 1000 counts
    CHECK(ThreatBoostFor(C, roles, live) == 1000.0f);

    // The MT, a non-tank and a dead tank are never boosted.
    live = {T(A, true, 10000), T(B, true, 0), T(C, false, 0), T(D, true, 0)};
    CHECK(ThreatBoostFor(A, roles, live) == 0.0f);
    CHECK(ThreatBoostFor(99, roles, live) == 0.0f);
    CHECK(ThreatBoostFor(C, roles, live) == 0.0f);

    // Missing reference: a soaker with a dead MT, a spare with no living soaker.
    live = {T(A, false, 10000), T(B, true, 0), T(C, false, 0), T(D, true, 0)};
    CHECK(ThreatBoostFor(B, roles, live) == 0.0f);
    live = {T(A, true, 10000), T(B, false, 9000), T(C, false, 9000), T(D, true, 0)};
    CHECK(ThreatBoostFor(D, roles, live) == 0.0f);
}

static void TestRolesFewTanks()
{
    Roles r = DeriveRoles({A, B}, {T(A, true, 5000), T(B, true, 3000)}, A);
    CHECK(r.mt == A && r.soaker[0] == B && r.soaker[1] == 0 && r.spares.empty());
    r = DeriveRoles({A}, {T(A, true, 5000)}, A);
    CHECK(r.mt == A && r.soaker[0] == 0 && r.soaker[1] == 0 && r.spares.empty());
    r = DeriveRoles({}, {}, 0);
    CHECK(r.mt == 0 && r.soaker[0] == 0 && r.soaker[1] == 0 && r.spares.empty());
}

static void TestCapDeadReference()
{
    // B (soaker) is dead but still listed in the role table: references must skip it.
    std::vector<TankInfo> live = {T(A, true, 5000), T(B, false, 9999), T(C, true, 2000), T(D, true, 1000)};
    Roles roles;
    roles.mt = A;
    roles.soaker[0] = B;
    roles.soaker[1] = C;
    roles.spares = {D};
    Cap c = CapFor(CapRole::Spare, D, roles, live);
    CHECK(c.capped && c.refThreat == 2000.0f);
    c = CapFor(CapRole::RangedDps, 99, roles, live);
    CHECK(c.capped && c.refThreat == 2000.0f);
    // Both soakers dead -> MT.
    live[2].alive = false;
    c = CapFor(CapRole::Spare, D, roles, live);
    CHECK(c.capped && c.refThreat == 5000.0f);
    c = CapFor(CapRole::RangedDps, 99, roles, live);
    CHECK(c.capped && c.refThreat == 5000.0f);
    c = CapFor(CapRole::MeleeDps, 99, roles, live);  // spare D is in melee with threat 1000 -> spare reference
    CHECK(c.capped && c.refThreat == 1000.0f);
}

static void TestCapTable()
{
    std::vector<TankInfo> live = {T(A, true, 5000), T(B, true, 3000), T(C, true, 2000), T(D, true, 1000)};
    Roles roles = DeriveRoles({A, B, C, D}, live, A);

    Cap c = CapFor(CapRole::None, 99, roles, live);
    CHECK(!c.capped);
    // The MT is a Soaker-less role: it has no CapRole row, so None.

    // Soaker caps against the MT only, 1.00 / 1.05; never itself.
    c = CapFor(CapRole::Soaker, B, roles, live);
    CHECK(c.capped && c.refThreat == 5000.0f && c.limit == 1.00f && c.stop == 1.05f);
    c = CapFor(CapRole::Soaker, C, roles, live);
    CHECK(c.capped && c.refThreat == 5000.0f);
    c = CapFor(CapRole::Soaker, A, roles, live);  // self == MT: no usable reference
    CHECK(!c.capped);

    // Spare: lower living soaker (C, 2000), 0.80 / 0.90.
    c = CapFor(CapRole::Spare, D, roles, live);
    CHECK(c.capped && c.refThreat == 2000.0f && c.limit == 0.80f && c.stop == 0.90f);

    // Ranged DPS: lower soaker.
    c = CapFor(CapRole::RangedDps, 99, roles, live);
    CHECK(c.capped && c.refThreat == 2000.0f && c.limit == 0.80f && c.stop == 0.90f);

    // Melee DPS: spare in melee with threat > 0.
    c = CapFor(CapRole::MeleeDps, 99, roles, live);
    CHECK(c.capped && c.refThreat == 1000.0f && c.limit == 0.80f && c.stop == 0.90f);

    // Spare out of melee -> lower soaker.
    live[3].inMelee = false;
    c = CapFor(CapRole::MeleeDps, 99, roles, live);
    CHECK(c.capped && c.refThreat == 2000.0f);
    // Spare in melee but threat 0 -> lower soaker.
    live[3].inMelee = true;
    live[3].threat = 0.0f;
    c = CapFor(CapRole::MeleeDps, 99, roles, live);
    CHECK(c.capped && c.refThreat == 2000.0f);

    // Second spare takes over when the first does not qualify.
    live.push_back(T(E, true, 700));
    roles = DeriveRoles({A, B, C, D, E}, live, A);
    c = CapFor(CapRole::MeleeDps, 99, roles, live);
    CHECK(c.capped && c.refThreat == 700.0f);

    // No soaker alive -> MT, for everyone.
    std::vector<TankInfo> lone = {T(A, true, 5000)};
    Roles const r1 = DeriveRoles({A}, lone, A);
    c = CapFor(CapRole::MeleeDps, 99, r1, lone);
    CHECK(c.capped && c.refThreat == 5000.0f);
    c = CapFor(CapRole::RangedDps, 99, r1, lone);
    CHECK(c.capped && c.refThreat == 5000.0f);
    c = CapFor(CapRole::Spare, 99, r1, lone);
    CHECK(c.capped && c.refThreat == 5000.0f);

    // No living reference at all -> uncapped.
    Roles const none = DeriveRoles({}, {}, 0);
    c = CapFor(CapRole::RangedDps, 99, none, {});
    CHECK(!c.capped);
    c = CapFor(CapRole::Soaker, B, none, {});
    CHECK(!c.capped);
}

static void TestVetoAndStop()
{
    Cap cap;
    cap.capped = true;
    cap.refThreat = 10000.0f;
    cap.limit = DPS_LIMIT;
    cap.stop = DPS_STOP;
    CHECK(!ThreatVetoed(6499.0f, cap));
    CHECK(ThreatVetoed(6500.0f, cap));
    CHECK(!ThreatStopped(8999.0f, cap));
    CHECK(ThreatStopped(9000.0f, cap));

    cap.refThreat = 0.0f;
    CHECK(ThreatVetoed(0.0f, cap));
    CHECK(ThreatStopped(0.0f, cap));

    Cap const off;
    CHECK(!ThreatVetoed(1e9f, off));
    CHECK(!ThreatStopped(1e9f, off));
}

static std::vector<uint64_t> Seq(uint64_t n)
{
    std::vector<uint64_t> v;
    for (uint64_t i = n; i >= 1; --i)  // descending, so the sort is exercised
        v.push_back(i);
    return v;
}

static void TestHealerSplit()
{
    HealerSplit s = SplitHealers(Seq(12));
    CHECK(s.soaker.size() == 8 && s.mt.size() == 2);
    CHECK(s.soaker.front() == 1 && s.soaker.back() == 8 && s.mt.front() == 9 && s.mt.back() == 10);
    s = SplitHealers(Seq(9));
    CHECK(s.soaker.size() == 6 && s.mt.size() == 2);
    s = SplitHealers(Seq(5));
    CHECK(s.soaker.size() == 3 && s.mt.size() == 2);
    s = SplitHealers(Seq(1));
    CHECK(s.soaker.size() == 1 && s.mt.empty());
    s = SplitHealers({});
    CHECK(s.soaker.empty() && s.mt.empty());
}

static void TestSoakerTarget()
{
    CHECK(PickSoakerTarget(A, 90.0f, 0, B, 20.0f, 1) == A);   // fewer claims wins over lower pct
    CHECK(PickSoakerTarget(A, 90.0f, 2, B, 95.0f, 1) == B);
    CHECK(PickSoakerTarget(A, 90.0f, 1, B, 60.0f, 1) == B);   // equal claims: lower pct
    CHECK(PickSoakerTarget(A, 50.0f, 1, B, 60.0f, 1) == A);
    CHECK(PickSoakerTarget(A, 50.0f, 1, B, 50.0f, 1) == A);   // full tie: first
    CHECK(PickSoakerTarget(0, 0.0f, 0, B, 60.0f, 3) == B);
    CHECK(PickSoakerTarget(A, 60.0f, 3, 0, 0.0f, 0) == A);
    CHECK(PickSoakerTarget(0, 0.0f, 0, 0, 0.0f, 0) == 0);
}

static void TestCancelPrecast()
{
    CHECK(ShouldCancelPrecast(300, 95.0f));
    CHECK(!ShouldCancelPrecast(301, 100.0f));
    CHECK(!ShouldCancelPrecast(0, 94.9f));
    CHECK(ShouldCancelPrecast(250, 100.0f));
}

static void TestDamageSample()
{
    DamageSample s;
    AddSample(s, 8000, 1000);
    CHECK(s.dmg == 7000 && s.hits == 1);
    AddSample(s, 8000, 6000);
    CHECK(s.dmg == 9000 && s.hits == 1);
    AddSample(s, 6000, 8000);
    CHECK(s.dmg == 9000 && s.hits == 1);
    AddSample(s, 5000, 5000);
    CHECK(s.dmg == 9000 && s.hits == 1);
    AddSample(s, 8000, 4000);  // exactly STRIKE_DROP_MIN counts
    CHECK(s.dmg == 13000 && s.hits == 2);
}

int main()
{
    TestStrikePair();
    TestPriority();
    TestRolesBasic();
    TestRolesSoakerDies();
    TestRolesMtDies();
    TestRolesVictimNotTank();
    TestRolesByLiveThreat();
    TestThreatBoost();
    TestRolesFewTanks();
    TestCapDeadReference();
    TestCapTable();
    TestVetoAndStop();
    TestHealerSplit();
    TestSoakerTarget();
    TestCancelPrecast();
    TestDamageSample();
    if (g_fail)
    {
        std::printf("%d check(s) FAILED\n", g_fail);
        return 1;
    }
    std::printf("test_naxx40_patchwerk_rules: all checks passed\n");
    return 0;
}
