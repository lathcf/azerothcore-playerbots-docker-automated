// modules/mod-raid-roster/src/RaidRosterLoginPlan.h
#ifndef MOD_RAID_ROSTER_LOGIN_PLAN_H
#define MOD_RAID_ROSTER_LOGIN_PLAN_H
// Pure arithmetic behind `.raidroster login N`. Deliberately free of AzerothCore includes so
// tests/test_login_plan.cpp can compile it standalone with g++ (the fork's module CMake globs
// src/ only, so tests/ never enters the worldserver build).
//
// Roles index 0 tank / 1 heal / 2 dps everywhere, matching RaidRosterRow::role.
#include <cstdint>
#include <array>
#include <algorithm>

using RoleCounts = std::array<uint8_t, 3>;

struct LoginPlan
{
    RoleCounts need{};        // bots to log in per role; already capped at the offline pool
    uint8_t    surplus   = 0; // group members over N; the caller dismisses that many roster bots
    uint8_t    shortfall = 0; // open seats no eligible offline roster bot can fill
};

inline uint32_t RoleSum(RoleCounts const& c) { return uint32_t(c[0]) + c[1] + c[2]; }

// RAID_SUBCOMPS counts bots only (N-1); the master's own seat is an implicit dps, so the
// N-member target comp is the table row plus one dps.
inline RoleCounts LoginTargetComp(uint8_t tanks, uint8_t heals, uint8_t dps)
{
    return { tanks, heals, uint8_t(dps + 1) };
}

// Fill/trim preference: dps first, then heal, then tank — used both to trim inflated deficits
// down to the open seats and to backfill seats a roster cap freed.
static constexpr int kLoginRoleOrder[3] = { 2, 1, 0 };

// size:    N, the wanted group size including the master
// target:  the N-member comp (LoginTargetComp)
// present: roles already seated in the group, master included
// avail:   eligible OFFLINE roster bots per role (the only pool new logins draw from)
inline LoginPlan ComputeLoginPlan(uint32_t size, RoleCounts const& target, RoleCounts const& present,
                                  RoleCounts const& avail)
{
    LoginPlan p;
    uint32_t const seated = RoleSum(present);
    uint32_t const open   = size > seated ? size - seated : 0;
    p.surplus = seated > size ? uint8_t(seated - size) : 0;

    // Per-role deficit. An over-subscribed role (3 healers in a 5-man) inflates the other roles'
    // deficits past the open seats, so trim dps -> heal -> tank until the sum fits.
    for (int r = 0; r < 3; ++r)
        p.need[r] = target[r] > present[r] ? uint8_t(target[r] - present[r]) : 0;
    for (int r : kLoginRoleOrder)
        while (RoleSum(p.need) > open && p.need[r] > 0)
            --p.need[r];

    // Roster cap, then backfill the freed seats from whatever role still has offline bots.
    for (int r = 0; r < 3; ++r)
        p.need[r] = std::min(p.need[r], avail[r]);
    bool progress = true;
    while (RoleSum(p.need) < open && progress)
    {
        progress = false;
        for (int r : kLoginRoleOrder)
            if (p.need[r] < avail[r]) { ++p.need[r]; progress = true; break; }
    }
    p.shortfall = uint8_t(open - RoleSum(p.need));
    return p;
}
#endif
