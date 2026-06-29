#ifndef MOD_RAID_ROSTER_COMP_H
#define MOD_RAID_ROSTER_COMP_H
#include "SharedDefines.h"   // CLASS_WARRIOR ... CLASS_DRUID
#include "PlayerbotAI.h"     // *_TAB_* enums
#include <array>
#include <cstdint>

struct RaidCompSlot { uint8 cls; uint8 role; uint8 specTab; }; // role 0=tank 1=heal 2=dps

// 4 tanks, 9 healers, 27 dps = 40. Unambiguous tank specs only.
static constexpr std::array<RaidCompSlot, 40> RAID_COMP = {{
    // --- Tanks (4) ---
    { CLASS_WARRIOR,      0, WARRIOR_TAB_PROTECTION },
    { CLASS_WARRIOR,      0, WARRIOR_TAB_PROTECTION },
    { CLASS_PALADIN,      0, PALADIN_TAB_PROTECTION },
    { CLASS_DEATH_KNIGHT, 0, DEATH_KNIGHT_TAB_BLOOD },
    // --- Healers (9) ---
    { CLASS_PRIEST,  1, PRIEST_TAB_HOLY },
    { CLASS_PRIEST,  1, PRIEST_TAB_HOLY },
    { CLASS_PRIEST,  1, PRIEST_TAB_HOLY },
    { CLASS_PALADIN, 1, PALADIN_TAB_HOLY },
    { CLASS_PALADIN, 1, PALADIN_TAB_HOLY },
    { CLASS_SHAMAN,  1, SHAMAN_TAB_RESTORATION },
    { CLASS_SHAMAN,  1, SHAMAN_TAB_RESTORATION },
    { CLASS_DRUID,   1, DRUID_TAB_RESTORATION },
    { CLASS_DRUID,   1, DRUID_TAB_RESTORATION },
    // --- DPS (27) ---
    { CLASS_ROGUE,        2, ROGUE_TAB_COMBAT },
    { CLASS_ROGUE,        2, ROGUE_TAB_COMBAT },
    { CLASS_ROGUE,        2, ROGUE_TAB_COMBAT },
    { CLASS_ROGUE,        2, ROGUE_TAB_COMBAT },
    { CLASS_MAGE,         2, MAGE_TAB_FROST },
    { CLASS_MAGE,         2, MAGE_TAB_FROST },
    { CLASS_MAGE,         2, MAGE_TAB_FIRE },
    { CLASS_MAGE,         2, MAGE_TAB_FIRE },
    { CLASS_WARLOCK,      2, WARLOCK_TAB_AFFLICTION },
    { CLASS_WARLOCK,      2, WARLOCK_TAB_AFFLICTION },
    { CLASS_WARLOCK,      2, WARLOCK_TAB_DESTRUCTION },
    { CLASS_WARLOCK,      2, WARLOCK_TAB_DESTRUCTION },
    { CLASS_HUNTER,       2, HUNTER_TAB_MARKSMANSHIP },
    { CLASS_HUNTER,       2, HUNTER_TAB_MARKSMANSHIP },
    { CLASS_HUNTER,       2, HUNTER_TAB_BEAST_MASTERY },
    { CLASS_HUNTER,       2, HUNTER_TAB_BEAST_MASTERY },
    { CLASS_DEATH_KNIGHT, 2, DEATH_KNIGHT_TAB_FROST },
    { CLASS_DEATH_KNIGHT, 2, DEATH_KNIGHT_TAB_UNHOLY },
    { CLASS_DEATH_KNIGHT, 2, DEATH_KNIGHT_TAB_UNHOLY },
    { CLASS_WARRIOR,      2, WARRIOR_TAB_FURY },
    { CLASS_WARRIOR,      2, WARRIOR_TAB_FURY },
    { CLASS_SHAMAN,       2, SHAMAN_TAB_ELEMENTAL },
    { CLASS_SHAMAN,       2, SHAMAN_TAB_ENHANCEMENT },
    { CLASS_DRUID,        2, DRUID_TAB_BALANCE },
    { CLASS_DRUID,        2, DRUID_TAB_BALANCE },
    { CLASS_PRIEST,       2, PRIEST_TAB_SHADOW },
    { CLASS_PRIEST,       2, PRIEST_TAB_SHADOW },
}};

static_assert(RAID_COMP.size() <= 255, "slot_index is TINYINT UNSIGNED; comp must fit in uint8");

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
