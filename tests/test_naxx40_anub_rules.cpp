// Standalone: g++ -std=c++17 -Wall -Wextra -Werror -o /tmp/nx40anub tests/test_naxx40_anub_rules.cpp && /tmp/nx40anub
// Run from the overlay repo root AFTER the 0022 patch is applied to the fork (the header lives there).
#include "../azerothcore-wotlk/modules/mod-playerbots/src/Ai/Raid/Naxx/Naxx40AnubRules.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <random>
#include <vector>

using namespace Naxx40AnubRules;

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

// Pin the constants to the spec (§2 mechanics, §3 / §5 values).
static_assert(ANUB_ENTRY == 351009 && BOSS_STATE_ID == 6);
static_assert(SPELL_IMPALE == 28783 && SPELL_LOCUST_SWARM == 28785);
static_assert(SWARM_RADIUS == 25.0f && IMPALE_HALF_ANGLE_DEG == 15.0f && IMPALE_LENGTH == 45.0f);
static_assert(ROOM_X == 3272.49f && ROOM_Y == -3476.27f && ROOM_MAX_RADIUS == 42.0f);
static_assert(ARC_HALF_SPAN_DEG == 60.0f && ARC_INNER > SWARM_RADIUS + 4.0f && ARC_OUTER > ARC_INNER);
static_assert(MELEE_FLANK_DEG - IMPALE_HALF_ANGLE_DEG > ARC_HALF_SPAN_DEG + IMPALE_HALF_ANGLE_DEG);
static_assert(MELEE_DIST + MELEE_TOLERANCE < 12.0f);  // inside melee range against his 10 y combat reach
static_assert(AXIS_CENTRE_NONE < AXIS_CENTRE_FULL);
static_assert(DIAG_INTERVAL_MS == 5000 && REFRESH_INTERVAL_MS == 500);

static Point2 OnRoom(float bearing, float d) { return Polar(RoomCentre(), bearing, d); }

static std::vector<Point2> ArcSpots(Point2 boss, float fallback, uint32_t n)
{
    std::vector<Point2> v;
    float const axis = ArcAxis(boss, fallback);
    for (uint32_t i = 0; i < n; ++i)
        v.push_back(ArcSpot(boss, axis, i, n));
    return v;
}

// Spec §3/§5: every arc spot is outside the 25 y swarm and within ~42 y of the room centre, for any boss position the
// kite allows (on the 45 y circle, overshooting it, near the centre) and any latched fallback bearing.
static void TestArcBounds()
{
    uint32_t const counts[] = {1, 2, 8, 24, 38, 40};
    for (float d = 0.0f; d <= 48.0f; d += 1.5f)
        for (int b = 0; b < 36; ++b)
        {
            Point2 const boss = OnRoom(DegToRad(b * 10.0f), d);
            for (int f = 0; f < 12; ++f)
                for (uint32_t n : counts)
                    for (Point2 const& p : ArcSpots(boss, DegToRad(f * 30.0f), n))
                    {
                        float const toBoss = Dist2d(p, boss);
                        CHECK(toBoss >= ARC_INNER - 0.01f);
                        CHECK(toBoss > SWARM_RADIUS + 4.0f);
                        CHECK(Dist2d(p, RoomCentre()) <= ROOM_MAX_RADIUS + 0.01f);
                    }
        }
}

// Spec §3: with him on the kite circle the arc is the room-side ±60° around the line to the centre, rows alternate,
// and neighbours are spread (distinct spots, a few yards apart even with the whole raid on it).
static void TestArcLayoutOnCircle()
{
    for (int b = 0; b < 16; ++b)
    {
        Point2 const boss = OnRoom(DegToRad(b * 22.5f), 45.0f);
        float const axis = ArcAxis(boss, 1.234f);
        CHECK(std::fabs(AngleDiff(axis, CentreBearing(boss))) < 1e-4f);  // centre mode on the circle
        std::vector<Point2> const s = ArcSpots(boss, 1.234f, 38);
        CHECK(std::fabs(Dist2d(s[0], boss) - ARC_INNER) < 0.01f);
        CHECK(std::fabs(Dist2d(s[1], boss) - ARC_OUTER) < 0.01f);
        float const a0 = std::atan2(s.front().y - boss.y, s.front().x - boss.x);
        float const aN = std::atan2(s.back().y - boss.y, s.back().x - boss.x);
        CHECK(std::fabs(std::fabs(AngleDiff(a0, axis)) - DegToRad(60.0f)) < 0.01f);
        CHECK(std::fabs(std::fabs(AngleDiff(aN, axis)) - DegToRad(60.0f)) < 0.01f);
        float minGap = 1e9f;
        for (size_t i = 0; i < s.size(); ++i)
            for (size_t j = i + 1; j < s.size(); ++j)
                minGap = std::min(minGap, Dist2d(s[i], s[j]));
        CHECK(minGap >= 3.0f);
    }
}

// Spec §3: one Impale (30° cone toward any arc bot) catches roughly a quarter of the arc, not everyone.
static void TestImpaleCoverage()
{
    for (uint32_t n = 8; n <= 40; ++n)
    {
        Point2 const boss = OnRoom(0.7f, 45.0f);
        std::vector<Point2> const s = ArcSpots(boss, 0.0f, n);
        uint32_t worst = 0;
        for (Point2 const& t : s)
        {
            uint32_t hit = 0;
            for (Point2 const& p : s)
                hit += InImpaleCone(boss, t, p) ? 1 : 0;
            worst = std::max(worst, hit);
        }
        CHECK(worst <= (n + 3) / 4 + 1);
    }
    // Upstream's clump (everyone within 3 y of the room centre) would take the whole raid in one cone.
    Point2 const boss = OnRoom(0.7f, 45.0f);
    CHECK(InImpaleCone(boss, RoomCentre(), OnRoom(2.0f, 3.0f)));
}

// Spec §3 item 2: melee split onto two flanks, alternating in a stable order; neither flank shares an Impale cone with
// the arc, and the flanks stay in melee range and near the kite circle.
static void TestMeleeFlanks()
{
    std::vector<Member> m;
    for (uint64_t g = 1; g <= 30; ++g)
        m.push_back(Member{g, uint8_t(g % 6), g % 3 == 0});
    Plan const plan = AssignSlots(m);
    CHECK(plan.meleeLeft + plan.meleeRight == 10);
    CHECK(std::abs(int(plan.meleeLeft) - int(plan.meleeRight)) <= 1);
    bool expectLeft = true;
    for (Slot const& s : plan.slots)
        if (s.melee)
        {
            CHECK(s.left == expectLeft);
            expectLeft = !expectLeft;
        }

    for (int b = 0; b < 16; ++b)
    {
        Point2 const boss = OnRoom(DegToRad(b * 22.5f), 45.0f);
        float const axis = ArcAxis(boss, 0.0f);
        Point2 const left = MeleeSpot(boss, axis, true);
        Point2 const right = MeleeSpot(boss, axis, false);
        CHECK(std::fabs(Dist2d(left, boss) - MELEE_DIST) < 0.01f);
        CHECK(Dist2d(left, right) > 10.0f);
        CHECK(Dist2d(left, RoomCentre()) < 47.0f && Dist2d(right, RoomCentre()) < 47.0f);
        std::vector<Point2> const arc = ArcSpots(boss, 0.0f, 38);
        for (Point2 const& a : arc)
        {
            CHECK(!InImpaleCone(boss, a, left) && !InImpaleCone(boss, a, right));
            CHECK(!InImpaleCone(boss, left, a) && !InImpaleCone(boss, right, a));
        }
        CHECK(!InImpaleCone(boss, left, right));

        // SpotFor: melee use the flank outside the swarm and their arc slot inside it.
        for (Slot const& s : plan.slots)
        {
            Point2 const out = SpotFor(s, plan, boss, 0.0f, false);
            Point2 const in = SpotFor(s, plan, boss, 0.0f, true);
            if (s.melee)
            {
                CHECK(Dist2d(out, boss) < SWARM_RADIUS);
                CHECK(ToleranceFor(s, false) == MELEE_TOLERANCE);
            }
            else
                CHECK(Dist2d(out, in) < 0.01f);
            CHECK(Dist2d(in, boss) > SWARM_RADIUS + 4.0f);
            CHECK(ToleranceFor(s, true) == ARC_TOLERANCE);
        }
    }
}

// Spec §5: slots are latched from a stable order (subgroup, guid), independent of member iteration order, distinct,
// and duplicates collapse.
static void TestSlotsStable()
{
    std::vector<Member> m;
    for (uint64_t g = 100; g < 140; ++g)
        m.push_back(Member{g * 7919 % 1000, uint8_t(g % 8), g % 4 == 1});
    Plan const ref = AssignSlots(m);
    std::mt19937 rng(42);
    for (int k = 0; k < 20; ++k)
    {
        std::shuffle(m.begin(), m.end(), rng);
        Plan const p = AssignSlots(m);
        CHECK(p.arcCount == ref.arcCount && p.slots.size() == ref.slots.size());
        for (size_t i = 0; i < p.slots.size(); ++i)
        {
            CHECK(p.slots[i].guid == ref.slots[i].guid);
            CHECK(p.slots[i].arcSlot == i);
            CHECK(p.slots[i].left == ref.slots[i].left);
        }
    }
    // Subgroups are contiguous, in subgroup order; FindSlot returns each bot's own slot.
    auto subgroupOf = [&m](uint64_t guid)
    {
        for (Member const& x : m)
            if (x.guid == guid)
                return int(x.subgroup);
        return -1;
    };
    for (size_t i = 0; i < ref.slots.size(); ++i)
    {
        Slot const* s = FindSlot(ref, ref.slots[i].guid);
        CHECK(s && s->arcSlot == i);
        if (i)
            CHECK(subgroupOf(ref.slots[i - 1].guid) <= subgroupOf(ref.slots[i].guid));
    }
    std::vector<Member> dup = {{5, 0, false}, {5, 0, false}, {6, 1, true}};
    Plan const d = AssignSlots(dup);
    CHECK(d.arcCount == 2 && FindSlot(d, 5) && FindSlot(d, 6) && !FindSlot(d, 7));
}

// The axis never jumps while he is kited round the circle, so spots move only as far as he does.
static void TestAxisContinuity()
{
    for (int f = 0; f < 8; ++f)
    {
        float const fallback = DegToRad(f * 45.0f);
        Point2 prev = OnRoom(0.0f, 45.0f);
        float prevAxis = ArcAxis(prev, fallback);
        for (int step = 1; step <= 720; ++step)
        {
            Point2 const boss = OnRoom(DegToRad(step * 0.5f), 45.0f);
            float const axis = ArcAxis(boss, fallback);
            CHECK(std::fabs(AngleDiff(axis, prevAxis)) < DegToRad(1.0f));
            prevAxis = axis;
            prev = boss;
        }
        // Near the centre the latched fallback bearing is the axis.
        CHECK(std::fabs(AngleDiff(ArcAxis(OnRoom(1.0f, 4.0f), fallback), fallback)) < 1e-4f);
    }
    // FallbackBearing: toward the raid, or toward the centre when the raid stands on him.
    Point2 const boss = OnRoom(0.0f, 44.0f);
    CHECK(std::fabs(AngleDiff(FallbackBearing(boss, Point2{boss.x, boss.y + 20.0f}), PI_F / 2.0f)) < 1e-4f);
    CHECK(std::fabs(AngleDiff(FallbackBearing(boss, boss), CentreBearing(boss))) < 1e-4f);
}

int main()
{
    TestArcBounds();
    TestArcLayoutOnCircle();
    TestImpaleCoverage();
    TestMeleeFlanks();
    TestSlotsStable();
    TestAxisContinuity();
    if (g_fail)
    {
        std::printf("%d check(s) FAILED\n", g_fail);
        return 1;
    }
    std::printf("test_naxx40_anub_rules: all checks passed\n");
    return 0;
}
