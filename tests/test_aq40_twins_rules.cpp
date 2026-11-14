// Standalone: g++ -std=c++17 -Wall -Wextra -Werror -o /tmp/aq40twins tests/test_aq40_twins_rules.cpp && /tmp/aq40twins
// Run from the overlay repo root AFTER patch 0016 is applied to the fork (the header lives there).
#include "../azerothcore-wotlk/modules/mod-playerbots/src/Ai/Raid/Aq40/Aq40TwinsRules.h"
#include <cmath>
#include <cstdio>
#include <utility>
#include <vector>

using namespace Aq40TwinsRules;

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

// WoW class ids.
static constexpr uint8_t WARRIOR = 1, PALADIN = 2, HUNTER = 3, PRIEST = 5, MAGE = 8, WARLOCK = 9, DRUID = 11;

static Member Tank(uint32_t guid, uint8_t cls) { return Member{guid, cls, true, true, false, true}; }
static Member Heal(uint32_t guid, uint8_t cls) { return Member{guid, cls, true, false, true, false}; }
static Member Caster(uint32_t guid, uint8_t cls) { return Member{guid, cls, true, false, false, false}; }

static std::vector<Member> DevRoster()
{
    return {
        Tank(1005, WARRIOR), Tank(1265, WARRIOR), Tank(551, WARRIOR), Tank(1106, PALADIN), Tank(1234, DRUID),
        Caster(659, WARLOCK), Caster(839, WARLOCK), Caster(1013, WARLOCK), Caster(1233, WARLOCK),
        Caster(705, PRIEST), Heal(735, PRIEST), Heal(1139, PRIEST), Heal(1149, PRIEST),
        Caster(1112, MAGE), Caster(1122, MAGE),
    };
}

static Assignment const* Find(std::vector<Assignment> const& v, uint32_t guid)
{
    for (Assignment const& a : v)
        if (a.guid == guid)
            return &a;
    return nullptr;
}

static bool Is(std::vector<Assignment> const& v, uint32_t guid, Role role, Side side, bool spare, bool fillIn)
{
    Assignment const* a = Find(v, guid);
    return a && a->role == role && a->side == side && a->spare == spare && a->fillIn == fillIn;
}

static void TestDevRosterAssignment()
{
    std::vector<Assignment> const v = AssignTwins(DevRoster());
    CHECK(v.size() == 15);
    // Melee tanks: warriors by guid first, then paladin, then druid.
    CHECK(Is(v, 551, Role::MeleeTank, Side::A, false, false));
    CHECK(Is(v, 1005, Role::MeleeTank, Side::B, false, false));
    CHECK(Is(v, 1265, Role::Dps, Side::A, true, false));
    CHECK(Is(v, 1106, Role::Dps, Side::B, true, false));
    CHECK(Is(v, 1234, Role::Dps, Side::A, true, false));
    // Warlock tanks.
    CHECK(Is(v, 659, Role::WarlockTank, Side::A, false, false));
    CHECK(Is(v, 839, Role::WarlockTank, Side::B, false, false));
    CHECK(Is(v, 1013, Role::Dps, Side::None, true, false));
    CHECK(Is(v, 1233, Role::Dps, Side::None, true, false));
    // Healers alternate A, B, A.
    CHECK(Is(v, 735, Role::Healer, Side::A, false, false));
    CHECK(Is(v, 1139, Role::Healer, Side::B, false, false));
    CHECK(Is(v, 1149, Role::Healer, Side::A, false, false));
    // Everyone else is plain DPS.
    CHECK(Is(v, 705, Role::Dps, Side::None, false, false));
    CHECK(Is(v, 1112, Role::Dps, Side::None, false, false));
    CHECK(Is(v, 1122, Role::Dps, Side::None, false, false));
}

static void TestHumansNeverDesignated()
{
    std::vector<Member> roster = DevRoster();
    roster.push_back(Member{1, WARRIOR, false, true, false, true});
    std::vector<Assignment> const v = AssignTwins(roster);
    Assignment const* h = Find(v, 1);
    CHECK(h && h->role == Role::None && h->side == Side::None && !h->spare && !h->fillIn);
    CHECK(Is(v, 551, Role::MeleeTank, Side::A, false, false));
    CHECK(Is(v, 1005, Role::MeleeTank, Side::B, false, false));
}

static void TestWarlockFillIn()
{
    // One warlock: it takes A, the mage fills B.
    {
        std::vector<Assignment> const v = AssignTwins({Caster(50, WARLOCK), Caster(40, MAGE), Caster(30, PRIEST)});
        CHECK(Is(v, 50, Role::WarlockTank, Side::A, false, false));
        CHECK(Is(v, 40, Role::WarlockTank, Side::B, false, true));
        CHECK(Is(v, 30, Role::Dps, Side::None, false, false));
    }
    // No warlocks, no mages: shadow priest fills A, next non-physical non-heal (balance druid) fills B.
    {
        std::vector<Assignment> const v = AssignTwins({Caster(30, PRIEST), Caster(60, DRUID), Tank(10, WARRIOR)});
        CHECK(Is(v, 30, Role::WarlockTank, Side::A, false, true));
        CHECK(Is(v, 60, Role::WarlockTank, Side::B, false, true));
        CHECK(Is(v, 10, Role::MeleeTank, Side::A, false, false));
    }
}

static void TestOneTank()
{
    std::vector<Assignment> const v = AssignTwins({Tank(10, WARRIOR), Caster(20, WARLOCK), Heal(30, PRIEST)});
    CHECK(Is(v, 10, Role::MeleeTank, Side::A, false, false));
    for (Assignment const& a : v)
        CHECK(!(a.role == Role::MeleeTank && a.side == Side::B));
}

static std::vector<std::pair<Assignment, Member>> Living(std::vector<Assignment> const& assigned,
                                                          std::vector<Member> const& members,
                                                          std::vector<uint32_t> const& guids)
{
    std::vector<std::pair<Assignment, Member>> out;
    for (uint32_t g : guids)
    {
        Assignment const* a = Find(assigned, g);
        for (Member const& m : members)
            if (a && m.guid == g)
                out.push_back({*a, m});
    }
    return out;
}

static void TestReplacement()
{
    std::vector<Member> const roster = DevRoster();
    std::vector<Assignment> const v = AssignTwins(roster);
    // Melee A dead: spare on A (1265) beats spare on B (1106) even though 1106 has the lower guid.
    CHECK(PickReplacement(Role::MeleeTank, Side::A, Living(v, roster, {1265, 1106})) == 1265);
    // Only the B spare is alive.
    CHECK(PickReplacement(Role::MeleeTank, Side::A, Living(v, roster, {1106})) == 1106);
    // Lowest guid first among same-side spares (1265 and 1234 are both A).
    CHECK(PickReplacement(Role::MeleeTank, Side::A, Living(v, roster, {1265, 1234})) == 1234);
    // Warlock spares carry no side.
    CHECK(PickReplacement(Role::WarlockTank, Side::A, Living(v, roster, {1233, 1013})) == 1013);
    // Designated (non-spare) bots and other kinds are never promoted.
    CHECK(PickReplacement(Role::WarlockTank, Side::A, Living(v, roster, {659, 839, 1265})) == 0);
    CHECK(PickReplacement(Role::MeleeTank, Side::B, Living(v, roster, {551, 1005, 1013})) == 0);
    CHECK(PickReplacement(Role::MeleeTank, Side::A, {}) == 0);
}

static void TestVeklorSideHysteresis()
{
    CHECK(UpdateVeklorSide(Side::A, 2.0f, 93.0f) == Side::A);
    CHECK(UpdateVeklorSide(Side::A, 93.0f, 3.0f) == Side::B);
    CHECK(UpdateVeklorSide(Side::B, 45.0f, 50.0f) == Side::B);
    CHECK(UpdateVeklorSide(Side::B, 29.0f, 64.0f) == Side::A);
    CHECK(UpdateVeklorSide(Side::None, 45.0f, 50.0f) == Side::None);
}

// Ruling R15: a Twin Teleport always swaps the twins, so the rising edge flips Vek'lor's side even when he lands
// more than VEKLOR_SIDE_RADIUS off both stations (geometry kept the old side).
static void TestSideAfterTeleport()
{
    CHECK(SideAfterTeleport(Side::A, Side::A) == Side::B);
    CHECK(SideAfterTeleport(Side::A, Side::B) == Side::B);
    CHECK(SideAfterTeleport(Side::B, Side::B) == Side::A);
    CHECK(SideAfterTeleport(Side::B, Side::A) == Side::A);
}

static void TestThreat()
{
    // Ruling R13 F2: ratio-only, no headroom (a warlock tank at 798 threat vetoed every DPS with 1000 headroom).
    CHECK(!ThreatVetoed(1500.0f, 2000.0f));
    CHECK(ThreatVetoed(1600.0f, 2000.0f));
    CHECK(ThreatVetoed(500.0f, 600.0f));
    CHECK(!ThreatVetoed(99999.0f, 0.0f));
    CHECK(ThreatHardStop(1800.0f, 2000.0f));
    CHECK(!ThreatHardStop(1700.0f, 2000.0f));
    CHECK(!ThreatHardStop(99999.0f, 0.0f));
}

// Task 7 (spec §5.5, ruling R11): the staging spot S_A is STAGE_A_DIST along the A->B crossing line.
static void TestPointAlong()
{
    Pt2 const p = PointAlong(Pt2{0.0f, 0.0f}, Pt2{90.0f, 0.0f}, 28.0f);
    CHECK(std::fabs(p.x - 28.0f) <= 0.01f && std::fabs(p.y) <= 0.01f);

    // TWINS_STATIONS A and B (tools/aq40-twins-stations.json).
    Pt2 const a{-8868.67f, 1206.24f};
    Pt2 const b{-8988.67f, 1206.24f};
    float const sep = std::hypot(b.x - a.x, b.y - a.y);
    CHECK(sep >= STATION_SEP_MIN && sep <= STATION_SEP_MAX);  // Revision 2 §11.1
    Pt2 const s = PointAlong(a, b, STAGE_A_DIST);
    float const toA = std::hypot(s.x - a.x, s.y - a.y);
    float const toB = std::hypot(s.x - b.x, s.y - b.y);
    CHECK(std::fabs(toA - 28.0f) <= 0.01f);
    CHECK(std::fabs(toB - 92.0f) <= 0.1f);  // Revision 2: 120 y separation
}

// Task 9 (ruling R16 H3): warrior B stages at S_B, 30 y from Vek'nilash's spawn toward station B, on his path when he
// runs at warlock A, and outside the twins' ~23 y aggro (detection_range 20 + 3 levels, centre to centre). The values
// are TWINS_STAGE_B from the tool's C++ block (tools/aq40-twins-stations.json "stage_b"); Aq40Helpers.h needs core
// types, so the test cannot include it.
static void TestStageB()
{
    Pt2 const spawn{-9023.67f, 1176.24f};   // Vek'nilash's spawn
    Pt2 const stageB{-9000.89f, 1195.76f};  // TWINS_STAGE_B
    Pt2 const b{-8988.67f, 1206.24f};       // TWINS_STATIONS B
    float const d = std::hypot(stageB.x - spawn.x, stageB.y - spawn.y);
    CHECK(std::fabs(d - 30.0f) <= 1.5f);
    CHECK(d >= 23.0f);
    // On the spawn -> B line (the exact point was floor; a snapped one would be within 3 y of it).
    Pt2 const ideal = PointAlong(spawn, b, 30.0f);
    CHECK(std::hypot(stageB.x - ideal.x, stageB.y - ideal.y) <= 3.0f);
}

// Revision 2 §11.2: only a twin's victim that is neither its active tank nor a designated tank of the other kind
// is a wrong victim. All 8 combinations of (victim, active tank, other-kind designated tank).
static void TestWrongVictim()
{
    for (int bits = 0; bits < 8; ++bits)
    {
        bool const victim = bits & 4;
        bool const active = bits & 2;
        bool const otherKind = bits & 1;
        bool const want = victim && !active && !otherKind;
        CHECK(IsWrongVictim(victim, active, otherKind) == want);
    }
    CHECK(IsWrongVictim(true, false, false));
    CHECK(!IsWrongVictim(true, true, false));
    CHECK(!IsWrongVictim(true, false, true));
    CHECK(!IsWrongVictim(true, true, true));
    CHECK(!IsWrongVictim(false, false, false));
    CHECK(!IsWrongVictim(false, true, false));
    CHECK(!IsWrongVictim(false, false, true));
    CHECK(!IsWrongVictim(false, true, true));
}

static void TestHoldFire()
{
    CHECK(HoldFireOnTwin(false, 0));
    CHECK(!HoldFireOnTwin(true, 0));
    CHECK(HoldFireOnTwin(false, 7999));   // Revision 5 §14.1: fails open at 8 s
    CHECK(!HoldFireOnTwin(false, 8000));
}

// Revision 3 §12.2: an anchor re-latches only on a twin settled within 3 y of one point for >= 3 s, and only when that
// point is more than 6 y from the current anchor.
static void TestAnchorRelatch()
{
    CHECK(AnchorShouldRelatch(0.0f, 3000, 6.5f));
    CHECK(AnchorShouldRelatch(3.0f, 3000, 6.01f));
    CHECK(AnchorShouldRelatch(1.0f, 60000, 40.0f));
    CHECK(!AnchorShouldRelatch(3.01f, 3000, 10.0f));  // moved off the candidate
    CHECK(!AnchorShouldRelatch(0.0f, 2999, 10.0f));   // not settled long enough
    CHECK(!AnchorShouldRelatch(0.0f, 3000, 6.0f));    // too close to the current anchor
    CHECK(!AnchorShouldRelatch(0.0f, 0, 0.0f));
}

// Revision 3 §12.1, Revision 5 §14.2: melee leave Vek'nilash 28 s after the last landing window opened (or engage).
static void TestPreSwap()
{
    CHECK(!PreSwapDue(0));
    CHECK(!PreSwapDue(27999));
    CHECK(PreSwapDue(28000));
    CHECK(PreSwapDue(45000));
}

// Fix round 1 (R22): anchors are clamped to 12 y from the authored station, so they cannot ratchet outward.
static void TestClampToRadius()
{
    Pt2 const c{100.0f, 200.0f};
    Pt2 const in = ClampToRadius(c, Pt2{105.0f, 204.0f}, 12.0f);  // 6.4 y: unchanged
    CHECK(in.x == 105.0f && in.y == 204.0f);
    Pt2 const edge = ClampToRadius(c, Pt2{112.0f, 200.0f}, 12.0f);  // exactly r: unchanged
    CHECK(edge.x == 112.0f && edge.y == 200.0f);
    Pt2 const out = ClampToRadius(c, Pt2{130.0f, 240.0f}, 12.0f);  // 50 y on bearing (0.6, 0.8)
    CHECK(std::fabs(std::hypot(out.x - c.x, out.y - c.y) - 12.0f) < 1e-3f);
    CHECK(std::fabs(out.x - (100.0f + 0.6f * 12.0f)) < 1e-3f);
    CHECK(std::fabs(out.y - (200.0f + 0.8f * 12.0f)) < 1e-3f);
    Pt2 const same = ClampToRadius(c, c, 12.0f);
    CHECK(same.x == c.x && same.y == c.y);
}

// Revision 4 §13.2 + ruling R25: a healer starts a step-in once its side's tank needs healing and has been beyond
// 36 y of the healer's spot for >= 2000 ms (the clock needs both, every refresh, until then). Once stepping in it ends
// ONLY when the tank is back within 34 y of the spot or dead - not at full health (100 % is transient on a tank).
static void TestHealerStepIn()
{
    // Waiting phase (stepping = false): both conditions, beyond 36 y.
    CHECK(!HealerStepInClockRuns(false, true, 50.0f, 35.0f));
    CHECK(HealerStepInClockRuns(false, true, 50.0f, 36.1f));
    CHECK(!HealerStepInClockRuns(false, true, 100.0f, 40.0f));  // full: never starts
    CHECK(!HealerStepInClockRuns(false, false, 50.0f, 40.0f));  // dead
    // Stepping in: distance (34 y) and life only.
    CHECK(HealerStepInClockRuns(true, true, 50.0f, 35.0f));
    CHECK(HealerStepInClockRuns(true, true, 100.0f, 40.0f));  // full does NOT end it (R25)
    CHECK(!HealerStepInClockRuns(true, true, 50.0f, 34.0f));
    CHECK(!HealerStepInClockRuns(true, false, 50.0f, 40.0f));
    // Due once the clock has run STEP_IN_MS; health plays no part.
    CHECK(HealerStepInDue(true, 36.1f, 2000));
    CHECK(!HealerStepInDue(true, 36.1f, 1999));
    CHECK(!HealerStepInDue(false, 40.0f, 5000));  // dead
    CHECK(!HealerStepInDue(true, 34.0f, 5000));   // back in range
    CHECK(HealerStepInDue(true, 34.1f, 5000));    // still stepping in between 34 and 36 y
}

static float SegDist(Pt2 a, Pt2 b, Pt2 c) { return SegmentDistance(a, b, c); }

// Walks the berth's waypoints from `from` to `to` (each leg is a straight segment, as MoveTo walks it) and returns
// the closest any leg came to `c`, or -1 when `to` is not reached in 32 legs.
static float WalkBerth(Pt2 from, Pt2 to, Pt2 c)
{
    float closest = 1e9f;
    Pt2 pos = from;
    for (int i = 0; i < 32; ++i)
    {
        Pt2 const w = BerthWaypoint(pos, to, c);
        closest = std::min(closest, SegDist(pos, w, c));
        if (w.x == to.x && w.y == to.y)
            return closest;
        pos = w;
    }
    return -1.0f;
}

// Revision 4 §13.3: the strategy's own moves route round a 14 y circle around Vek'lor.
static void TestBerth()
{
    Pt2 const c{0.0f, 0.0f};
    // Clear of the circle: unchanged.
    Pt2 const clear = BerthWaypoint(Pt2{-40.0f, 20.0f}, Pt2{40.0f, 20.0f}, c);
    CHECK(clear.x == 40.0f && clear.y == 20.0f);
    // `to` inside r: unchanged (the destination is deliberately near him).
    Pt2 const near = BerthWaypoint(Pt2{-40.0f, 0.0f}, Pt2{10.0f, 0.0f}, c);
    CHECK(near.x == 10.0f && near.y == 0.0f);
    // Straight through the centre: a point 16 y from him, roughly perpendicular to the segment.
    Pt2 const from{-40.0f, 0.0f};
    Pt2 const to{40.0f, 0.0f};
    Pt2 const w = BerthWaypoint(from, to, c);
    CHECK(std::fabs(std::hypot(w.x, w.y) - 16.0f) < 1e-3f);
    CHECK(std::fabs(w.x) / 16.0f < 0.5f);  // within 30 degrees of perpendicular
    CHECK(w.x < 0.0f && w.y > 0.0f);  // ruling R24: the tangent from `from`, on the from side (tie: counter-clockwise)
    CHECK(SegDist(from, w, c) >= 14.0f);   // the first leg itself clears the circle
    // Deterministic, and stable while the bot walks the leg (a point halfway along gets the same waypoint).
    Pt2 const again = BerthWaypoint(from, to, c);
    CHECK(again.x == w.x && again.y == w.y);
    Pt2 const half{(from.x + w.x) * 0.5f, (from.y + w.y) * 0.5f};
    Pt2 const fromHalf = BerthWaypoint(half, to, c);
    CHECK(std::fabs(fromHalf.x - w.x) < 1e-3f && std::fabs(fromHalf.y - w.y) < 1e-3f);
    // `from` inside r: radially out to 16 y.
    Pt2 const out = BerthWaypoint(Pt2{3.0f, 4.0f}, to, c);
    CHECK(std::fabs(out.x - 9.6f) < 1e-3f && std::fabs(out.y - 12.8f) < 1e-3f);
    // Whole walks reach `to` and never come inside the Arcane Burst trigger; far walks keep the full 14 y.
    CHECK(WalkBerth(Pt2{-40.0f, 0.0f}, Pt2{40.0f, 0.0f}, c) >= 14.0f);
    CHECK(WalkBerth(Pt2{-40.0f, 3.0f}, Pt2{35.0f, -6.0f}, c) >= 14.0f);
    CHECK(WalkBerth(Pt2{-15.0f, 0.0f}, Pt2{17.0f, 0.0f}, c) > ARCANE_BURST_TRIGGER);
    CHECK(WalkBerth(Pt2{0.0f, -15.0f}, Pt2{2.0f, 30.0f}, c) > ARCANE_BURST_TRIGGER);
    CHECK(WalkBerth(Pt2{2.0f, 1.0f}, Pt2{-30.0f, 0.0f}, c) >= 0.0f);  // starts inside: still arrives
    // Off-centre circle, mirrored side: the same shape.
    Pt2 const c2{-8868.0f, 1206.0f};
    CHECK(WalkBerth(Pt2{-8908.0f, 1206.0f}, Pt2{-8828.0f, 1204.0f}, c2) >= 14.0f);
    CHECK(WalkBerth(Pt2{-8828.0f, 1210.0f}, Pt2{-8908.0f, 1212.0f}, c2) >= 14.0f);
}

// Revision 7 §16.1: any bot within 12.5 y (2-D) of a living, non-passive Vek'lor escapes to 16 y.
static void TestBurstRange()
{
    CHECK(InBurstRange(0.0f, true, false));
    CHECK(InBurstRange(12.49f, true, false));
    CHECK(!InBurstRange(12.5f, true, false));    // strict
    CHECK(!InBurstRange(16.0f, true, false));
    CHECK(!InBurstRange(5.0f, false, false));    // dead Vek'lor
    CHECK(!InBurstRange(5.0f, true, true));      // REACT_PASSIVE (Twin Teleport)
    CHECK(!InBurstRange(BURST_ESCAPE_TO, true, false));  // the escape point ends the trigger
}

// Revision 7 §16.2: a physical melee (non-hunter) bot skips a mutated bug within 14 y of a living Vek'lor.
static void TestMeleeSkipsBug()
{
    CHECK(MeleeSkipsBug(true, true, 0.0f));
    CHECK(MeleeSkipsBug(true, true, 13.99f));
    CHECK(!MeleeSkipsBug(true, true, 14.0f));     // strict "within"
    CHECK(!MeleeSkipsBug(false, true, 5.0f));     // ranged still take it
    CHECK(!MeleeSkipsBug(true, false, 5.0f));     // Vek'lor dead
}

// Revision 8 §17.1: a side's pickup basis re-latches to its twin when unset, or when the twin is more than 4 y from it.
static void TestPickupBasis()
{
    CHECK(BasisShouldRelatch(false, 0.0f));       // not valid (latch, window rising edge): always
    CHECK(BasisShouldRelatch(false, 100.0f));
    CHECK(!BasisShouldRelatch(true, 0.0f));
    CHECK(!BasisShouldRelatch(true, 4.0f));       // strict "more than 4 y"
    CHECK(BasisShouldRelatch(true, 4.01f));
    CHECK(BasisShouldRelatch(true, 30.0f));
    CHECK(PICKUP_BASIS_RELATCH == 4.0f);
}

// Revision 9 §18.2: an owned spot inside r of Vek'lor is pushed radially out to r on the bearing centre -> p (+x when
// p is the centre); a spot at or beyond r is unchanged.
static void TestPushOut()
{
    Pt2 const c{10.0f, -5.0f};
    Pt2 const inside = PushOutFrom(c, Pt2{13.0f, -1.0f}, 16.0f);  // 5 y out on (0.6, 0.8)
    CHECK(std::fabs(std::hypot(inside.x - c.x, inside.y - c.y) - 16.0f) < 1e-3f);
    CHECK(std::fabs(inside.x - (10.0f + 0.6f * 16.0f)) < 1e-3f && std::fabs(inside.y - (-5.0f + 0.8f * 16.0f)) < 1e-3f);
    Pt2 const on = PushOutFrom(c, c, 16.0f);  // on top of him: out along +x
    CHECK(std::fabs(on.x - 26.0f) < 1e-3f && std::fabs(on.y + 5.0f) < 1e-3f);
    Pt2 const atR = PushOutFrom(c, Pt2{26.0f, -5.0f}, 16.0f);  // exactly r: unchanged
    CHECK(atR.x == 26.0f && atR.y == -5.0f);
    Pt2 const beyond = PushOutFrom(c, Pt2{-20.0f, 30.0f}, 16.0f);
    CHECK(beyond.x == -20.0f && beyond.y == 30.0f);
    CHECK(BURST_ESCAPE_TO == 16.0f);
}

// Revision 10 §19.1/§19.3: inside the landing window the Vek'nilash pickup engages at once, and healers make no move
// but the Explode / Blizzard / landing-clear dodges.
static void TestLandingWindowRules()
{
    CHECK(PickupEngagesInWindow(true, true));
    CHECK(!PickupEngagesInWindow(true, false));
    CHECK(!PickupEngagesInWindow(false, true));
    CHECK(!PickupEngagesInWindow(false, false));
    CHECK(HealerFrozenInWindow(true, true, false));
    CHECK(!HealerFrozenInWindow(true, true, true));    // a dodge still runs
    CHECK(!HealerFrozenInWindow(true, false, false));  // not a healer
    CHECK(!HealerFrozenInWindow(false, true, false));  // outside the window
}

// Revision 11 §20.1: whoever holds Vek'nilash kites him toward the drag station while the twins are under 100 y, or
// when no active melee tank is alive; otherwise it walks to the active melee tank.
static void TestVictimKites()
{
    CHECK(VictimKites(99.9f, true));
    CHECK(!VictimKites(100.0f, true));  // at the guard: walk to the tank
    CHECK(!VictimKites(130.0f, true));
    CHECK(VictimKites(130.0f, false));  // no living active melee tank: kite anyway
    CHECK(VictimKites(40.0f, false));
}

// Revision 11 §20.2: the LOS step-in. Due once a side tank is alive, below 100 %, within 36 y and out of LOS for
// STEP_IN_LOS_MS; it ends on LOS within 34 y, or the tank's death.
static void TestHealerLosStepIn()
{
    CHECK(HealerLosStepInDue(true, 50.0f, 30.0f, false, 1500));
    CHECK(!HealerLosStepInDue(true, 50.0f, 30.0f, false, 1499));
    CHECK(!HealerLosStepInDue(true, 50.0f, 30.0f, true, 5000));    // in LOS
    CHECK(HealerLosStepInDue(true, 50.0f, 36.0f, false, 5000));    // at 36 y
    CHECK(!HealerLosStepInDue(true, 50.0f, 36.1f, false, 5000));   // beyond: the distance step-in's job
    CHECK(!HealerLosStepInDue(true, 100.0f, 30.0f, false, 5000));  // full: never starts
    CHECK(!HealerLosStepInDue(false, 50.0f, 30.0f, false, 5000));  // dead
    CHECK(HealerLosStepInEnds(true, 34.0f, true));
    CHECK(!HealerLosStepInEnds(true, 34.1f, true));   // LOS but too far
    CHECK(!HealerLosStepInEnds(true, 20.0f, false));  // close but still no LOS
    CHECK(HealerLosStepInEnds(false, 50.0f, false));  // dead
    CHECK(STEP_IN_LOS_MS == 1500);
    CHECK(STEP_IN_LOS_DIST == 20.0f);
}

// Final-review M1: the LOS step-in point is min(STEP_IN_LOS_DIST, half the tank -> home distance) from the tank, so a
// tank nearer than 20 y never puts the point past home (the old fixed 20 y overshot, stepping the healer outward).
static void TestHealerStepInPointDist()
{
    CHECK(HealerStepInPointDist(true, 50.0f) == STEP_IN_LOS_DIST);   // far: the full 20 y
    CHECK(HealerStepInPointDist(true, 40.0f) == STEP_IN_LOS_DIST);
    CHECK(std::fabs(HealerStepInPointDist(true, 36.0f) - 18.0f) <= 0.001f);  // < 40 y: half way
    CHECK(std::fabs(HealerStepInPointDist(true, 30.0f) - 15.0f) <= 0.001f);
    CHECK(std::fabs(HealerStepInPointDist(true, 12.0f) - 6.0f) <= 0.001f);
    CHECK(HealerStepInPointDist(true, 0.0f) == 0.0f);
    CHECK(HealerStepInPointDist(false, 12.0f) == STEP_IN_DIST);  // the distance step-in is unchanged
    CHECK(HealerStepInPointDist(false, 40.0f) == STEP_IN_DIST);
    // The point stays on the tank -> home segment, strictly short of home.
    for (float d = 1.0f; d <= 36.0f; d += 1.0f)
    {
        Pt2 const tank{0.0f, 0.0f};
        Pt2 const home{d, 0.0f};
        Pt2 const p = PointAlong(tank, home, HealerStepInPointDist(true, d));
        CHECK(p.x > 0.0f && p.x < d);
    }
}

// Final-review I1: the active warlock in range but out of LOS scans radii from Vek'lor down its bearing to the bot,
// on a fixed grid WARLOCK_LOS_MIN + k * WARLOCK_LOS_STEP (stable while the bot walks the line), starting at the
// largest one strictly inside botDist - arrive (so a chosen point is always a real walk); < WARLOCK_LOS_MIN = none.
static void TestWarlockLosRadii()
{
    CHECK(WARLOCK_LOS_MIN >= BURST_ESCAPE_TO);
    CHECK(WARLOCK_LOS_FALLBACK >= WARLOCK_LOS_MIN);
    CHECK(WarlockLosTopRadius(30.0f, 1.0f) == 27.0f);  // 17, 19, ..., 27 < 29
    CHECK(WarlockLosTopRadius(30.0f, 3.0f) == 25.0f);  // 27 is not < 27
    CHECK(WarlockLosTopRadius(18.0f, 1.0f) < WARLOCK_LOS_MIN);  // 17 is not < 17
    CHECK(WarlockLosTopRadius(18.1f, 1.0f) == 17.0f);
    CHECK(WarlockLosTopRadius(20.0f, 1.0f) == 17.0f);  // 19 is not < 19
    CHECK(WarlockLosTopRadius(20.1f, 1.0f) == 19.0f);
    CHECK(WarlockLosTopRadius(12.0f, 1.0f) < WARLOCK_LOS_MIN);
    // Stable: walking the line from 30 y to 22 y keeps every remaining candidate on the same grid.
    for (float d = 30.0f; d >= 22.0f; d -= 0.25f)
    {
        float const top = WarlockLosTopRadius(d, 1.0f);
        CHECK(top >= WARLOCK_LOS_MIN && top < d - 1.0f);
        float const k = (top - WARLOCK_LOS_MIN) / WARLOCK_LOS_STEP;
        CHECK(std::fabs(k - std::round(k)) <= 0.001f);
    }
}

int main()
{
    TestDevRosterAssignment();
    TestHumansNeverDesignated();
    TestWarlockFillIn();
    TestOneTank();
    TestReplacement();
    TestVeklorSideHysteresis();
    TestSideAfterTeleport();
    TestThreat();
    TestHoldFire();
    TestWrongVictim();
    TestPointAlong();
    TestStageB();
    TestAnchorRelatch();
    TestPreSwap();
    TestClampToRadius();
    TestHealerStepIn();
    TestBerth();
    TestBurstRange();
    TestMeleeSkipsBug();
    TestPickupBasis();
    TestPushOut();
    TestLandingWindowRules();
    TestVictimKites();
    TestHealerLosStepIn();
    TestHealerStepInPointDist();
    TestWarlockLosRadii();
    if (g_fail == 0)
        std::printf("OK\n");
    return g_fail != 0;
}
