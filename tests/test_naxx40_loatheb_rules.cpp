// Standalone: g++ -std=c++17 -Wall -Wextra -Werror -o /tmp/nx40lb tests/test_naxx40_loatheb_rules.cpp && /tmp/nx40lb
// Run from the overlay repo root AFTER the Loatheb patch is applied to the fork (the header lives there).
#include "../azerothcore-wotlk/modules/mod-playerbots/src/Ai/Raid/Naxx/Naxx40LoathebRules.h"
#include <cstdint>
#include <cstdio>
#include <vector>

using namespace Naxx40LoathebRules;

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

// Pin every constant to the brief's values.
static_assert(LOATHEB_ENTRY == 351020 && SPORE_ENTRY == 351066 && BOSS_STATE_ID == 5);
static_assert(DOOM_SPELL == 29204);
static_assert(CM_PRIMER[0] == 29194 && CM_PRIMER[1] == 29196 && CM_PRIMER[2] == 29198);
static_assert(CM_LOCKOUT[0] == 29195 && CM_LOCKOUT[1] == 29197 && CM_LOCKOUT[2] == 29199);
static_assert(HEAL_LOCKOUT_MS == 20000);
static_assert(POTION_ITEM == 13459 && SHADOW_ABSORB_SPELL == 17548);
static_assert(HEALTHSTONE_ITEMS[0] == 9421 && HEALTHSTONE_ITEMS[1] == 19012 && HEALTHSTONE_ITEMS[2] == 19013);
static_assert(BANDAGE_ITEM == 14530 && RECENTLY_BANDAGED_SPELL == 11196);
static_assert(POISON_SHOCK_RADIUS == 5.0f && BANDAGE_CLEAR == 8.0f && FUNGAL_RADIUS == 10.0f);
static_assert(SPORE_ENGAGE == 8.0f && SPORE_TANK_EXCLUDE == 12.0f && SPORE_CHOOSER_VETO == 1.5f);
static_assert(SPORE_ENGAGE < SPORE_TANK_EXCLUDE && FUNGAL_RADIUS == 10.0f);
static_assert(SPORE_STACK_RADIUS == 4.0f && SPORE_STRAY_MS == 20000 && SPORE_STACK_RADIUS < FUNGAL_RADIUS);
static_assert(ROTATION_SIZE == 6 && ROTATION_MIN_SPACING_MS == 2000);
static_assert(HEALER_DPS_MANA_PCT == 30.0f && MANA_REFILL_BELOW_PCT == 25.0f);
static_assert(STOP_BEFORE_MS == 2000 && AFTER_DOOM_WINDOW_MS == 8000);
static_assert(HEALTHSTONE_BELOW_PCT == 40.0f && BANDAGE_BELOW_PCT == 80.0f);
static_assert(EMERGENCY_TANK_PCT == 50.0f && TANK_FULL_PCT == 95.0f);
static_assert(MT_TOPUP_BELOW_PCT == 60.0f && EMERGENCY_TANK_PCT < MT_TOPUP_BELOW_PCT);
static_assert(JUST_HEALED_MS == 3000 && SHIELD_WALL_PCT == 40.0f && LAST_STAND_PCT == 30.0f);
static_assert(HEAL_REACH == 36.0f);
static_assert(DIAG_INTERVAL_MS == 5000 && REFRESH_INTERVAL_MS == 500);
static_assert(RANGE_SPOT_X == 2896.96f && RANGE_SPOT_Y == -3980.61f && DOOM_FAST_FROM == 6);
static_assert(PRAYER_RADIUS == 30.0f && PRAYER_RADIUS < HEAL_REACH);

static bool Has(std::vector<uint64_t> const& v, uint64_t g)
{
    for (uint64_t x : v)
        if (x == g)
            return true;
    return false;
}

// Spec §4.8 item 2: the rotation is exactly ROTATION_SIZE (6) bot healers (paladins, druids, priests, shamans); every
// other bot healer is a raid healer (plan.group).
static void TestAssignHealers()
{
    // Member order deliberately mixes classes: the class order wins, member order only breaks ties within a class.
    std::vector<HealerMember> m = {
        {5, HealerClass::Priest, 0, false},  {8, HealerClass::Shaman, 5, false},
        {1, HealerClass::Paladin, 0, false}, {6, HealerClass::Priest, 4, false},
        {3, HealerClass::Druid, 2, false},   {2, HealerClass::Paladin, 1, false},
        {7, HealerClass::Priest, 4, false},  {4, HealerClass::Druid, 3, false},
    };
    HealerPlan p = AssignHealers(m);
    CHECK(p.rotation.size() == 6);
    CHECK(p.rotation[0] == 1 && p.rotation[1] == 2 && p.rotation[2] == 3 && p.rotation[3] == 4);
    CHECK(p.rotation[4] == 5 && p.rotation[5] == 6);
    CHECK(p.group.size() == 2);
    CHECK(p.group[0] == 7 && p.group[1] == 8);

    // One paladin, no druid: priests fill the rotation before any shaman.
    std::vector<HealerMember> m2 = {
        {1, HealerClass::Shaman, 0, false}, {2, HealerClass::Priest, 1, false}, {3, HealerClass::Paladin, 2, false},
        {4, HealerClass::Priest, 3, false}, {5, HealerClass::Priest, 4, false}, {6, HealerClass::Shaman, 5, false},
        {7, HealerClass::Priest, 6, false}, {8, HealerClass::Shaman, 7, false},
    };
    HealerPlan p2 = AssignHealers(m2);
    CHECK(p2.rotation.size() == 6);
    CHECK(p2.rotation[0] == 3 && p2.rotation[1] == 2 && p2.rotation[2] == 4 && p2.rotation[3] == 5);
    CHECK(p2.rotation[4] == 7 && p2.rotation[5] == 1);
    CHECK(p2.group.size() == 2 && p2.group[0] == 6 && p2.group[1] == 8);

    // The owner's ~12 healers: 6 on the tank, 6 raid healers.
    std::vector<HealerMember> m3;
    for (uint64_t g = 1; g <= 12; ++g)
        m3.push_back({g, g <= 3 ? HealerClass::Paladin : g <= 9 ? HealerClass::Priest : HealerClass::Shaman,
                      uint8_t(g % 8), false});
    HealerPlan p3 = AssignHealers(m3);
    CHECK(p3.rotation.size() == 6 && p3.group.size() == 6);
    CHECK(p3.rotation[0] == 1 && p3.rotation[5] == 6 && p3.group[0] == 7 && p3.group[5] == 12);
}

static void TestAssignHealersTinyRaid()
{
    std::vector<HealerMember> m = {
        {1, HealerClass::Priest, 0, false}, {2, HealerClass::Priest, 1, false}, {3, HealerClass::Priest, 2, false}};
    HealerPlan p = AssignHealers(m);
    CHECK(p.rotation.size() == 3);
    CHECK(p.group.empty());

    HealerPlan e = AssignHealers({});
    CHECK(e.rotation.empty() && e.group.empty());
    CHECK(RotationSpacingMs(0) == 20000);
}

static void TestAssignHealersHumansExcluded()
{
    std::vector<HealerMember> m = {
        {10, HealerClass::Paladin, 2, true}, {11, HealerClass::Priest, 2, false},
        {1, HealerClass::Paladin, 0, false}, {2, HealerClass::Paladin, 0, false},
        {3, HealerClass::Paladin, 1, false}, {4, HealerClass::Paladin, 1, false},
        {5, HealerClass::Paladin, 2, false}, {6, HealerClass::Paladin, 3, false},
    };
    HealerPlan p = AssignHealers(m);
    CHECK(!Has(p.rotation, 10) && !Has(p.group, 10));
    CHECK(p.rotation.size() == 6 && !Has(p.rotation, 11));
    CHECK(p.group.size() == 1 && p.group[0] == 11);
}

static void TestSpacing()
{
    CHECK(RotationSpacingMs(4) == 5000);
    CHECK(RotationSpacingMs(ROTATION_SIZE) == 3333);
    CHECK(RotationSpacingMs(5) == 4000);
    CHECK(RotationSpacingMs(20) == 2000);
}

static void TestRotationCastNow()
{
    CHECK(!RotationCastNow(4000, 5000, 50.0f));
    CHECK(RotationCastNow(5000, 5000, 50.0f));
    CHECK(!RotationCastNow(5000, 5000, 97.0f));
    CHECK(RotationCastNow(10000, 5000, 97.0f));
    CHECK(RotationCastNow(2000, 5000, 25.0f));
    // Spec §4.9 item 2: the emergency line is 50 %, and even below it the rotation keeps ROTATION_MIN_SPACING_MS
    // between tank heals (else all six heals start within a second and the rotation sits locked out for 20 s).
    CHECK(RotationCastNow(2000, 5000, 49.9f));
    CHECK(!RotationCastNow(100, 5000, 49.9f));
    CHECK(!RotationCastNow(1999, 5000, 25.0f));
    CHECK(RotationCastNow(UINT32_MAX, 5000, 25.0f));   // no tank heal yet this attempt
    CHECK(!RotationCastNow(2000, 5000, 50.0f));
}

static void TestRotationSkipsUnreadyHealer()
{
    std::vector<bool> ready = {true, false, true, true};
    CHECK(RotationUp(0, ready, 1000, 5000) == 1);
    CHECK(RotationUp(0, ready, 5000, 5000) == 2);
    CHECK(RotationUp(0, {false, false, false}, 5000, 5000) == SIZE_MAX);
    CHECK(RotationUp(0, {}, 5000, 5000) == SIZE_MAX);
    CHECK(RotationUp(0, {true, false}, 5000, 5000) == 0);   // lastCaster itself comes last
}

static void TestMsSinceOwnHeal()
{
    CHECK(MsSinceOwnHeal(19000) == 1000);
    CHECK(MsSinceOwnHeal(-1) == UINT32_MAX);
    CHECK(MsSinceOwnHeal(25000) == 0);
}

static void TestHealerMayDps()
{
    CHECK(HealerMayDps(31.0f, 2500));
    CHECK(!HealerMayDps(30.0f, 9000));
    CHECK(!HealerMayDps(80.0f, 2000));
    // With a cast time the cast must end STOP_BEFORE_MS (2000) before the heal is due: due > cast + 2000.
    CHECK(HealerMayDps(80.0f, 5000, 2500));
    CHECK(!HealerMayDps(80.0f, 5000, 3500));
    CHECK(!HealerMayDps(80.0f, 4000, 2500));
    CHECK(!HealerMayDps(80.0f, 4000, 3500));
    CHECK(HealerMayDps(80.0f, 4000, 1500));
    CHECK(!HealerMayDps(80.0f, 4000, 2000));
    CHECK(HealerMayDps(80.0f, UINT32_MAX, 3500));
    CHECK(!HealerMayDps(25.0f, UINT32_MAX, 0));
}

static void TestNextDoom()
{
    CHECK(DoomIntervalMs(5) == 30000 && DoomIntervalMs(6) == 15000);
    CHECK(RaidHealerMayDps(5) && !RaidHealerMayDps(6) && !RaidHealerMayDps(13));
    CHECK(MsToNextDoomResolve(4200, 1000, 3) == 4200);           // a Doom ticks: its remaining time
    CHECK(MsToNextDoomResolve(UINT32_MAX, 12000, 3) == 18000);   // lastResolve + 30 s
    CHECK(MsToNextDoomResolve(UINT32_MAX, 5000, 6) == 10000);    // lastResolve + 15 s from the 6th on
    CHECK(MsToNextDoomResolve(UINT32_MAX, 40000, 2) == 0);       // overdue
    CHECK(MsToNextDoomResolve(UINT32_MAX, UINT32_MAX, 0) == UINT32_MAX);
}

static void TestChooseRaidHeal()
{
    // Priest: Prayer of Healing on 2+ injured in an unclaimed party, else the big heal on the raid target.
    CHECK(ChooseRaidHeal(HealerClass::Priest, 3000, 2, false, true) == RaidHeal::Prayer);
    CHECK(ChooseRaidHeal(HealerClass::Priest, 3000, 2, false, false) == RaidHeal::Prayer);
    CHECK(ChooseRaidHeal(HealerClass::Priest, 3000, 2, true, true) == RaidHeal::Big);
    CHECK(ChooseRaidHeal(HealerClass::Priest, 3000, 1, false, true) == RaidHeal::Big);
    CHECK(ChooseRaidHeal(HealerClass::Priest, 3000, 1, false, false) == RaidHeal::None);
    // Shaman: Chain Heal, never Prayer; paladin / druid: the big heal.
    CHECK(ChooseRaidHeal(HealerClass::Shaman, 3000, 5, false, true) == RaidHeal::Chain);
    CHECK(ChooseRaidHeal(HealerClass::Shaman, 3000, 5, false, false) == RaidHeal::None);
    CHECK(ChooseRaidHeal(HealerClass::Paladin, 3000, 5, false, true) == RaidHeal::Big);
    CHECK(ChooseRaidHeal(HealerClass::Druid, 3000, 0, false, true) == RaidHeal::Big);
    // Outside the after-Doom window: nothing.
    CHECK(ChooseRaidHeal(HealerClass::Priest, 9000, 4, false, true) == RaidHeal::None);
    CHECK(ChooseRaidHeal(HealerClass::Shaman, UINT32_MAX, 4, false, true) == RaidHeal::None);
}

// Spec §4.9 item 1: the first raid heal of an after-Doom window tops up a main tank below 60 %.
static void TestMainTankTopUp()
{
    CHECK(MainTankTopUp(3000, 59.9f, true, false));
    CHECK(MainTankTopUp(AFTER_DOOM_WINDOW_MS, 10.0f, true, false));
    CHECK(!MainTankTopUp(3000, 60.0f, true, false));          // at or above 60 %: the raid as before
    CHECK(!MainTankTopUp(3000, 40.0f, false, false));         // out of reach or sight
    CHECK(!MainTankTopUp(3000, 40.0f, true, true));           // already claimed for this Doom
    CHECK(!MainTankTopUp(AFTER_DOOM_WINDOW_MS + 1, 40.0f, true, false));
    CHECK(!MainTankTopUp(UINT32_MAX, 40.0f, true, false));    // before the first Doom resolved
    // The top-up beats Prayer and needs no raid target: Chain Heal for a shaman, the big heal for everyone else.
    CHECK(ChooseRaidHeal(HealerClass::Priest, 3000, 4, false, false, true) == RaidHeal::Big);
    CHECK(ChooseRaidHeal(HealerClass::Shaman, 3000, 0, false, false, true) == RaidHeal::Chain);
    CHECK(ChooseRaidHeal(HealerClass::Paladin, 3000, 0, false, false, true) == RaidHeal::Big);
    CHECK(ChooseRaidHeal(HealerClass::Druid, 3000, 0, false, false, true) == RaidHeal::Big);
    CHECK(ChooseRaidHeal(HealerClass::Priest, 9000, 4, false, true, true) == RaidHeal::None);
    // Without the top-up the old choice stands.
    CHECK(ChooseRaidHeal(HealerClass::Priest, 3000, 4, false, true, false) == RaidHeal::Prayer);
}

static ConsumableInput Base()
{
    ConsumableInput in{};
    in.msSinceDoomResolved = UINT32_MAX;
    in.msSinceOwnHeal = UINT32_MAX;
    in.healthPct = 100.0f;
    in.distToBoss = 20.0f;
    return in;
}

static void TestChooseConsumable()
{
    ConsumableInput in = Base();
    in.hasDoom = true;
    in.potionReady = true;
    CHECK(ChooseConsumable(in) == Consumable::Potion);

    in.hasShadowAbsorb = true;
    CHECK(ChooseConsumable(in) == Consumable::None);

    in = Base();
    in.hasDoom = true;
    in.potionReady = true;
    in.isHealer = true;
    in.msSinceOwnHeal = 5000;
    CHECK(ChooseConsumable(in) == Consumable::None);
    in.msSinceOwnHeal = 1000;
    CHECK(ChooseConsumable(in) == Consumable::Potion);

    in = Base();
    in.msSinceDoomResolved = 2000;
    in.healthPct = 35.0f;
    in.healthstoneReady = true;
    CHECK(ChooseConsumable(in) == Consumable::Healthstone);

    in.healthstoneClaimed = true;
    in.bandageReady = true;
    in.distToBoss = 15.0f;
    CHECK(ChooseConsumable(in) == Consumable::Bandage);
    in.distToBoss = 3.0f;
    CHECK(ChooseConsumable(in) == Consumable::StepOut);

    in = Base();
    in.msSinceDoomResolved = 2000;
    in.healthPct = 50.0f;
    in.isBossVictim = true;
    in.bandageReady = true;
    CHECK(ChooseConsumable(in) == Consumable::None);
    in.healthPct = 35.0f;
    in.healthstoneReady = true;
    CHECK(ChooseConsumable(in) == Consumable::Healthstone);

    // Spec §4.7 item 1: a tank never bandages nor steps out, but still drinks and uses its healthstone.
    in = Base();
    in.isTank = true;
    in.msSinceDoomResolved = 2000;
    in.healthPct = 50.0f;
    in.bandageReady = true;
    CHECK(ChooseConsumable(in) == Consumable::None);
    in.distToBoss = 3.0f;
    CHECK(ChooseConsumable(in) == Consumable::None);
    in.healthPct = 35.0f;
    in.healthstoneReady = true;
    CHECK(ChooseConsumable(in) == Consumable::Healthstone);
    in = Base();
    in.isTank = true;
    in.hasDoom = true;
    in.potionReady = true;
    CHECK(ChooseConsumable(in) == Consumable::Potion);

    // Spec §4.8 item 1: a rotation healer never bandages nor steps out (just healed, so the lockout gate is open), but
    // still drinks and uses its healthstone; a raid healer in the same state still bandages.
    in = Base();
    in.isHealer = true;
    in.isRotationHealer = true;
    in.msSinceOwnHeal = 1000;
    in.msSinceDoomResolved = 2000;
    in.healthPct = 50.0f;
    in.bandageReady = true;
    CHECK(ChooseConsumable(in) == Consumable::None);
    in.distToBoss = 3.0f;
    CHECK(ChooseConsumable(in) == Consumable::None);
    in.healthPct = 35.0f;
    in.healthstoneReady = true;
    CHECK(ChooseConsumable(in) == Consumable::Healthstone);
    in.healthstoneReady = false;
    in.hasDoom = true;
    in.potionReady = true;
    CHECK(ChooseConsumable(in) == Consumable::Potion);
    in.hasDoom = false;
    in.isRotationHealer = false;
    in.distToBoss = 20.0f;
    CHECK(ChooseConsumable(in) == Consumable::Bandage);
    in.distToBoss = 3.0f;
    CHECK(ChooseConsumable(in) == Consumable::StepOut);
}

static bool Near(float a, float b) { return a - b < 0.01f && b - a < 0.01f; }

static void TestStepOutPoint()
{
    // Far from the ranged spot: reach yards from the boss along the line to it.
    Point2 p = StepOutPoint(RANGE_SPOT_X - 30.0f, RANGE_SPOT_Y, 10.0f);
    CHECK(Near(p.x, RANGE_SPOT_X - 20.0f) && Near(p.y, RANGE_SPOT_Y));
    p = StepOutPoint(RANGE_SPOT_X, RANGE_SPOT_Y + 40.0f, 12.0f);
    CHECK(Near(p.x, RANGE_SPOT_X) && Near(p.y, RANGE_SPOT_Y + 28.0f));
    // Upstream's tank spot (2877.57, -3967.00): ~23.7 y out, so the point is reach yards toward the spot.
    p = StepOutPoint(2877.57f, -3967.00f, 10.0f);
    float const dx = p.x - 2877.57f;
    float const dy = p.y + 3967.00f;
    CHECK(Near(dx * dx + dy * dy, 100.0f) && dx > 0.0f && dy < 0.0f);
    // Closer to the spot than reach: the spot itself.
    p = StepOutPoint(RANGE_SPOT_X + 3.0f, RANGE_SPOT_Y - 4.0f, 10.0f);
    CHECK(Near(p.x, RANGE_SPOT_X) && Near(p.y, RANGE_SPOT_Y));
}

// Spec §4.9 item 3 (with §4.7 item 6's victim-only exclusion). Args: toBot, toStack, ageMs, toVictim, engaged.
static void TestSporeEngages()
{
    CHECK(SporeNearVictim(11.9f));
    CHECK(!SporeNearVictim(12.0f));
    // At the ranged stack (within 4 y of RANGE_SPOT): any DPS bot engages, wherever it stands, however young the spore.
    CHECK(SporeEngages(30.0f, 3.9f, 0, 20.0f, false));
    CHECK(SporeEngages(2.0f, 4.0f, 0, 12.0f, false));
    CHECK(!SporeEngages(2.0f, 4.1f, 0, 20.0f, false));       // §4.8's 8 y engage is gone: still walking to the stack
    CHECK(!SporeEngages(7.9f, 6.0f, 19999, 20.0f, false));   // not a stray yet
    CHECK(!SporeEngages(3.0f, 0.0f, 0, 11.9f, false));       // near Loatheb's victim: left alone even at the stack
    CHECK(SporeEngages(30.0f, 0.0f, 0, 1e9f, false));        // no victim (caller passes a huge distance)
    // Fallback: alive 20 s and outside the stack zone, the DPS within 8 y engage.
    CHECK(SporeEngages(8.0f, 15.0f, 20000, 20.0f, false));
    CHECK(!SporeEngages(8.1f, 15.0f, 60000, 20.0f, false));
    CHECK(!SporeEngages(3.0f, 15.0f, 60000, 11.9f, false));
    // Keep an engaged spore out to FUNGAL_RADIUS (10 y) of the bot, or anywhere in the stack zone; the victim
    // exclusion still drops it.
    CHECK(SporeEngages(9.9f, 6.0f, 0, 20.0f, true));
    CHECK(SporeEngages(10.0f, 6.0f, 0, 20.0f, true));
    CHECK(!SporeEngages(10.1f, 6.0f, 0, 20.0f, true));
    CHECK(SporeEngages(25.0f, 3.0f, 0, 20.0f, true));        // a melee bot walking in to a stacked spore keeps it
    CHECK(!SporeEngages(9.9f, 6.0f, 0, 20.0f, false));       // the same spore, not engaged yet, is not picked up
    CHECK(!SporeEngages(3.0f, 0.0f, 0, 11.9f, true));
    // Distance to the stack.
    CHECK(DistToRangeSpot(RANGE_SPOT_X, RANGE_SPOT_Y) == 0.0f);
    CHECK(Near(DistToRangeSpot(RANGE_SPOT_X + 3.0f, RANGE_SPOT_Y - 4.0f), 5.0f));
}

static void TestTankCd()
{
    CHECK(ChooseTankCd(35.0f, true, true) == TankCd::ShieldWall);
    CHECK(ChooseTankCd(25.0f, true, true) == TankCd::LastStand);
    CHECK(ChooseTankCd(25.0f, true, false) == TankCd::ShieldWall);
    CHECK(ChooseTankCd(50.0f, true, true) == TankCd::None);
}

int main()
{
    TestAssignHealers();
    TestAssignHealersTinyRaid();
    TestAssignHealersHumansExcluded();
    TestSpacing();
    TestRotationCastNow();
    TestRotationSkipsUnreadyHealer();
    TestMsSinceOwnHeal();
    TestHealerMayDps();
    TestNextDoom();
    TestChooseRaidHeal();
    TestMainTankTopUp();
    TestChooseConsumable();
    TestStepOutPoint();
    TestSporeEngages();
    TestTankCd();
    if (g_fail)
    {
        std::printf("%d failure(s)\n", g_fail);
        return 1;
    }
    std::printf("ok\n");
    return 0;
}
