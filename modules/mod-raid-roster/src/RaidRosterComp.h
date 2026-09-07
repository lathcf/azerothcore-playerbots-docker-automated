#ifndef MOD_RAID_ROSTER_COMP_H
#define MOD_RAID_ROSTER_COMP_H
#include "SharedDefines.h"   // CLASS_WARRIOR ... CLASS_DRUID
#include "PlayerbotAI.h"     // *_TAB_* enums
#include <array>
#include <cstdint>

// Era band a comp slot is eligible in. Persisted verbatim as mod_raid_roster.band.
enum RaidCompBand : uint8
{
    BAND_ANY            = 0,   // eligible at every master level
    BAND_WOTLK_ONLY     = 1,   // master level >= kWotlkBandMinLevel  (the 4 Death Knights)
    BAND_PRE_WOTLK_ONLY = 2,   // master level <  kWotlkBandMinLevel  (the 4 DK substitutes)
};

// Mirrors mod-era-talents' EraTalentBots::BotEra WotLK floor (1-60 Vanilla / 61-70 TBC / 71+
// WotLK). Roster bots are levelled to the master at sync, so the master's level IS the bots'
// era band. Deliberately a local constant — one integer is not worth a header dependency on
// era-talents. If that band ever moves, change both.
static constexpr uint8 kWotlkBandMinLevel = 71;

struct RaidCompSlot { uint8 cls; uint8 role; uint8 specTab; uint8 band; }; // role 0=tank 1=heal 2=dps

// The ONE eligibility rule. Unknown band values fail open (eligible) so a bot is never silently
// lost; .raidroster status reports them as mis-banded and .raidroster create rewrites them.
constexpr bool RaidCompEligible(uint8 band, uint8 masterLevel)
{
    bool const wotlk = masterLevel >= kWotlkBandMinLevel;
    switch (band)
    {
        case BAND_WOTLK_ONLY:     return wotlk;
        case BAND_PRE_WOTLK_ONLY: return !wotlk;
        default:                  return true;
    }
}

// 44 slots. In EITHER band exactly 4 tanks / 9 healers / 27 dps are eligible (asserted below):
// slots 0-39 are the original comp (its 4 Death Knights now WotLK-only), 40-43 are the
// pre-WotLK substitutes that fill the DK roles below level 71. Unambiguous tank specs only.
// DO NOT reorder slots 0-39: a live roster's rows are matched to this table by slot_index.
static constexpr std::array<RaidCompSlot, 44> RAID_COMP = {{
    // --- Tanks (4 + 1 substitute) ---
    { CLASS_WARRIOR,      0, WARRIOR_TAB_PROTECTION,  BAND_ANY },            // 0
    { CLASS_WARRIOR,      0, WARRIOR_TAB_PROTECTION,  BAND_ANY },            // 1
    { CLASS_PALADIN,      0, PALADIN_TAB_PROTECTION,  BAND_ANY },            // 2
    { CLASS_DEATH_KNIGHT, 0, DEATH_KNIGHT_TAB_BLOOD,  BAND_WOTLK_ONLY },     // 3
    // --- Healers (9) ---
    { CLASS_PRIEST,  1, PRIEST_TAB_HOLY,         BAND_ANY },                 // 4
    { CLASS_PRIEST,  1, PRIEST_TAB_HOLY,         BAND_ANY },                 // 5
    { CLASS_PRIEST,  1, PRIEST_TAB_HOLY,         BAND_ANY },                 // 6
    { CLASS_PALADIN, 1, PALADIN_TAB_HOLY,        BAND_ANY },                 // 7
    { CLASS_PALADIN, 1, PALADIN_TAB_HOLY,        BAND_ANY },                 // 8
    { CLASS_SHAMAN,  1, SHAMAN_TAB_RESTORATION,  BAND_ANY },                 // 9
    { CLASS_SHAMAN,  1, SHAMAN_TAB_RESTORATION,  BAND_ANY },                 // 10
    { CLASS_DRUID,   1, DRUID_TAB_RESTORATION,   BAND_ANY },                 // 11
    { CLASS_DRUID,   1, DRUID_TAB_RESTORATION,   BAND_ANY },                 // 12
    // --- DPS (27 + 3 substitutes) ---
    { CLASS_ROGUE,        2, ROGUE_TAB_COMBAT,          BAND_ANY },          // 13
    { CLASS_ROGUE,        2, ROGUE_TAB_COMBAT,          BAND_ANY },          // 14
    { CLASS_ROGUE,        2, ROGUE_TAB_COMBAT,          BAND_ANY },          // 15
    { CLASS_ROGUE,        2, ROGUE_TAB_COMBAT,          BAND_ANY },          // 16
    { CLASS_MAGE,         2, MAGE_TAB_FROST,            BAND_ANY },          // 17
    { CLASS_MAGE,         2, MAGE_TAB_FROST,            BAND_ANY },          // 18
    { CLASS_MAGE,         2, MAGE_TAB_FIRE,             BAND_ANY },          // 19
    { CLASS_MAGE,         2, MAGE_TAB_FIRE,             BAND_ANY },          // 20
    { CLASS_WARLOCK,      2, WARLOCK_TAB_AFFLICTION,    BAND_ANY },          // 21
    { CLASS_WARLOCK,      2, WARLOCK_TAB_AFFLICTION,    BAND_ANY },          // 22
    { CLASS_WARLOCK,      2, WARLOCK_TAB_DESTRUCTION,   BAND_ANY },          // 23
    { CLASS_WARLOCK,      2, WARLOCK_TAB_DESTRUCTION,   BAND_ANY },          // 24
    { CLASS_HUNTER,       2, HUNTER_TAB_MARKSMANSHIP,   BAND_ANY },          // 25
    { CLASS_HUNTER,       2, HUNTER_TAB_MARKSMANSHIP,   BAND_ANY },          // 26
    { CLASS_HUNTER,       2, HUNTER_TAB_BEAST_MASTERY,  BAND_ANY },          // 27
    { CLASS_HUNTER,       2, HUNTER_TAB_BEAST_MASTERY,  BAND_ANY },          // 28
    { CLASS_DEATH_KNIGHT, 2, DEATH_KNIGHT_TAB_FROST,    BAND_WOTLK_ONLY },   // 29
    { CLASS_DEATH_KNIGHT, 2, DEATH_KNIGHT_TAB_UNHOLY,   BAND_WOTLK_ONLY },   // 30
    { CLASS_DEATH_KNIGHT, 2, DEATH_KNIGHT_TAB_UNHOLY,   BAND_WOTLK_ONLY },   // 31
    { CLASS_WARRIOR,      2, WARRIOR_TAB_FURY,          BAND_ANY },          // 32
    { CLASS_WARRIOR,      2, WARRIOR_TAB_FURY,          BAND_ANY },          // 33
    { CLASS_SHAMAN,       2, SHAMAN_TAB_ELEMENTAL,      BAND_ANY },          // 34
    { CLASS_SHAMAN,       2, SHAMAN_TAB_ENHANCEMENT,    BAND_ANY },          // 35
    { CLASS_DRUID,        2, DRUID_TAB_BALANCE,         BAND_ANY },          // 36
    { CLASS_DRUID,        2, DRUID_TAB_BALANCE,         BAND_ANY },          // 37
    { CLASS_PRIEST,       2, PRIEST_TAB_SHADOW,         BAND_ANY },          // 38
    { CLASS_PRIEST,       2, PRIEST_TAB_SHADOW,         BAND_ANY },          // 39
    // --- Pre-WotLK substitutes for the 4 Death Knight slots (1 tank + 3 dps) ---
    { CLASS_DRUID,        0, DRUID_TAB_FERAL,           BAND_PRE_WOTLK_ONLY }, // 40 bear tank
    { CLASS_PALADIN,      2, PALADIN_TAB_RETRIBUTION,   BAND_PRE_WOTLK_ONLY }, // 41
    { CLASS_WARRIOR,      2, WARRIOR_TAB_ARMS,          BAND_PRE_WOTLK_ONLY }, // 42
    { CLASS_SHAMAN,       2, SHAMAN_TAB_ENHANCEMENT,    BAND_PRE_WOTLK_ONLY }, // 43
}};

static_assert(RAID_COMP.size() <= 255, "slot_index is TINYINT UNSIGNED; comp must fit in uint8");

// Compile-time guard: whichever band the master is in, the eligible slots give the same
// 4 / 9 / 27 role totals that RAID_SUBCOMPS assumes.
constexpr uint8 RaidCompRoleCount(uint8 role, uint8 masterLevel)
{
    uint8 n = 0;
    for (RaidCompSlot const& s : RAID_COMP)
        if (s.role == role && RaidCompEligible(s.band, masterLevel)) ++n;
    return n;
}
static_assert(RaidCompRoleCount(0, kWotlkBandMinLevel) == 4 && RaidCompRoleCount(1, kWotlkBandMinLevel) == 9 &&
              RaidCompRoleCount(2, kWotlkBandMinLevel) == 27, "WotLK-band comp must be 4 tank / 9 heal / 27 dps");
static_assert(RaidCompRoleCount(0, kWotlkBandMinLevel - 1) == 4 && RaidCompRoleCount(1, kWotlkBandMinLevel - 1) == 9 &&
              RaidCompRoleCount(2, kWotlkBandMinLevel - 1) == 27, "pre-WotLK comp must be 4 tank / 9 heal / 27 dps");

// Per raid size: how many of each role to bring (tank, heal, dps).
// `size` is the TOTAL raid size including the player; the mod fields `size-1` bots
// (WotLK MAXRAIDSIZE=40 is a hard cap, so `login 40` = you + 39 bots).
struct SubComp { uint32 size; uint8 tanks; uint8 heals; uint8 dps; };
static constexpr std::array<SubComp, 4> RAID_SUBCOMPS = {{
    { 5,  1, 1, 2 },   // 4 bots + you
    { 10, 2, 2, 5 },   // 9 bots + you
    { 25, 2, 6, 16 },  // 24 bots + you
    { 40, 4, 8, 27 },  // 39 bots + you (all 4 tanks, 8 of 9 healers, all 27 dps)
}};
#endif
