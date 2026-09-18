// modules/mod-battleground-bots/tests/test_assignment.cpp
// Standalone: g++ -std=c++17 -Wall -Wextra -Werror -o /tmp/bgtest modules/mod-battleground-bots/tests/test_assignment.cpp && /tmp/bgtest
#include "../src/BgAssignmentEngine.h"
#include <cstdio>

using namespace BgEngine;

static int g_fails = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); ++g_fails; } } while (0)

static BotIn Bot(u32 key, Role role, float x, float y, bool alive = true, bool inCombat = false, bool human = false, bool isFc = false)
{ BotIn b; b.key = key; b.alive = alive; b.inCombat = inCombat; b.human = human; b.isFc = isFc; b.role = role; b.x = x; b.y = y; return b; }
static ObjIn Obj(u32 id, float x, float y, Owner owner, u32 fNear = 0, u32 eNear = 0, u32 lastSeen = 0)
{ ObjIn o; o.id = id; o.x = x; o.y = y; o.owner = owner; o.friendlyNear = fNear; o.enemyNear = eNear; o.lastEnemySeenMs = lastSeen; o.stageX = x - 60.f; o.stageY = y; return o; }
static u32 CountJob(TeamState const& st, Job j, u32 obj = 0)
{ u32 n = 0; for (auto const& [k, a] : st.assign) if (a.job == j && (obj == 0 || a.objectiveId == obj)) ++n; return n; }

int main()
{
    Params p;                       // defaults = conf defaults
    p.roamPct = 0;                  // most tests want a deterministic pool; roam tested on its own
    p.guardMaxPct = 100;            // the team guard cap is exercised on its own in group 16; every
                                    // other group predates it and wants plain per-objective arithmetic
    u32 const T0 = p.openingMs + 1000;   // past the opening window

    // 1. Posture: opening window holds, then 2-tick hysteresis in both directions.
    {
        TeamState st;
        CHECK(ComputePosture(st, -5, 0, p, 1000) == Posture::BALANCED);          // opening: no change
        CHECK(ComputePosture(st, -5, 0, p, T0) == Posture::BALANCED);            // pending tick 1
        CHECK(ComputePosture(st, -5, 0, p, T0) == Posture::OFFENSIVE);           // tick 2 flips
        CHECK(ComputePosture(st, 0, 0, p, T0) == Posture::OFFENSIVE);            // pending BALANCED
        CHECK(ComputePosture(st, 5, 0, p, T0) == Posture::OFFENSIVE);            // pending switched to DEFENSIVE, tick 1
        CHECK(ComputePosture(st, 5, 0, p, T0) == Posture::DEFENSIVE);            // tick 2 flips
        CHECK(ComputePosture(st, 5, -1, p, T0) == Posture::DEFENSIVE);           // objMargin<0 => want OFFENSIVE, pending
        CHECK(ComputePosture(st, 5, -1, p, T0) == Posture::OFFENSIVE);
    }

    // 2. Guard quota scales with posture and contestability.
    {
        ObjIn quiet = Obj(1, 0, 0, Owner::US);
        CHECK(GuardQuota(quiet, Posture::BALANCED,  p, T0, 1e9f) == 2);
        CHECK(GuardQuota(quiet, Posture::DEFENSIVE, p, T0, 1e9f) == 3);
        CHECK(GuardQuota(quiet, Posture::OFFENSIVE, p, T0, 1e9f) == 1);
        ObjIn seen = Obj(1, 0, 0, Owner::US, 0, 0, T0 - 5000);
        CHECK(GuardQuota(seen, Posture::BALANCED, p, T0, 1e9f) == 3);            // enemy seen 5s ago
        CHECK(GuardQuota(quiet, Posture::BALANCED, p, T0, 250.f) == 3);          // enemy node within 300y
        Params off = p; off.guardMin = 1;
        CHECK(GuardQuota(quiet, Posture::OFFENSIVE, off, T0, 1e9f) == 1);        // floor of 1
    }

    // 3. Roam share is deterministic per key.
    {
        Params r = p; r.roamPct = 15;
        CHECK(IsRoam(5, r)); CHECK(!IsRoam(50, r)); CHECK(!IsRoam(5, p));
    }

    // 4. Guards fill nearest-first and stick across ticks (epoch unchanged when nothing moved).
    {
        TeamState st;
        std::vector<ObjIn> objs = { Obj(1, 0, 0, Owner::US), Obj(2, 1000, 0, Owner::US), Obj(3, 500, 800, Owner::ENEMY) };
        std::vector<BotIn> bots = { Bot(1, Role::MELEE, 10, 0), Bot(2, Role::RANGED, 20, 0), Bot(3, Role::HEAL, 990, 0),
                                    Bot(4, Role::MELEE, 980, 0), Bot(5, Role::MELEE, 500, 400), Bot(6, Role::HEAL, 500, 410) };
        Tick(st, bots, objs, FlagIn{}, p, T0);
        // obj 3 is ~943y from both held objectives (> ContestDistance), so each quota is the plain 2.
        CHECK(CountJob(st, Job::GUARD, 1) == 2); CHECK(CountJob(st, Job::GUARD, 2) == 2);
        CHECK(st.assign[1].job == Job::GUARD && st.assign[1].objectiveId == 1);
        CHECK(st.assign[3].job == Job::GUARD && st.assign[3].objectiveId == 2);
        CHECK(st.assign[5].job == Job::ASSAULT && st.assign[5].objectiveId == 3 && st.assign[5].stance == Stance::STAGE);
        u32 e1 = st.assign[1].epoch, e5 = st.assign[5].epoch;
        std::swap(bots[0], bots[3]);   // input order must not matter
        Tick(st, bots, objs, FlagIn{}, p, T0 + 2000);
        CHECK(st.assign[1].epoch == e1); CHECK(st.assign[5].epoch == e5);
        CHECK(st.assign[1].objectiveId == 1 && st.assign[3].objectiveId == 2);
    }

    // 5. A human standing on the node counts as a guard; humans/FC never get assignments.
    {
        TeamState st;
        std::vector<ObjIn> objs = { Obj(1, 0, 0, Owner::US), Obj(3, 900, 0, Owner::ENEMY) };
        std::vector<BotIn> bots = { Bot(1, Role::MELEE, 5, 0), Bot(2, Role::MELEE, 6, 0),
                                    Bot(99, Role::MELEE, 3, 0, true, false, true), Bot(7, Role::MELEE, 400, 0, true, false, false, true) };
        Tick(st, bots, objs, FlagIn{}, p, T0);
        CHECK(CountJob(st, Job::GUARD, 1) == 1);          // quota 2 (enemy node 900y away) minus 1 human
        CHECK(st.assign.count(99) == 0); CHECK(st.assign.count(7) == 0);
    }

    // 6/7. Squads: healers spread, STAGE until quorum or timeout, then ASSAULT ON_FLAG; captured => dissolve.
    {
        TeamState st;
        std::vector<ObjIn> objs = { Obj(1, 0, 0, Owner::US), Obj(2, 1000, 0, Owner::ENEMY, 0, 1), Obj(3, 1000, 1000, Owner::ENEMY, 0, 6) };
        std::vector<BotIn> bots;
        for (u32 k = 1; k <= 10; ++k) bots.push_back(Bot(k, k <= 2 ? Role::HEAL : Role::MELEE, 5.f * k, 0));
        Tick(st, bots, objs, FlagIn{}, p, T0);
        CHECK(CountJob(st, Job::GUARD, 1) == 2);
        CHECK(CountJob(st, Job::ASSAULT, 2) == 8);        // focus = obj 2 (fewer defenders); obj 3 untouched
        CHECK(CountJob(st, Job::ASSAULT, 3) == 0);
        CHECK(st.squads.size() == 2);                     // 8 bots / squadSize 4
        // (the two healers, nearest to (0,0), became the guards; healer spreading is checked in 6b)
        for (auto& a : st.assign) CHECK(a.second.job != Job::ASSAULT || a.second.stance == Stance::STAGE);
        // move 3 members of the first squad onto the stage point -> that squad pushes, the other stays staged
        u32 sid = 0; for (auto const& [k, a] : st.assign) if (a.job == Job::ASSAULT) { sid = a.squadId; break; }
        u32 moved = 0;
        for (BotIn& b : bots) if (moved < 3 && st.assign[b.key].squadId == sid) { b.x = objs[1].stageX; b.y = objs[1].stageY; ++moved; }
        Tick(st, bots, objs, FlagIn{}, p, T0 + 2000);
        for (auto const& [k, a] : st.assign) if (a.job == Job::ASSAULT) CHECK(a.stance == (a.squadId == sid ? Stance::ON_FLAG : Stance::STAGE));
        // timeout pushes the other squad
        Tick(st, bots, objs, FlagIn{}, p, T0 + p.stageTimeoutMs + 2000);
        for (auto const& [k, a] : st.assign) if (a.job == Job::ASSAULT) CHECK(a.stance == Stance::ON_FLAG);
        // capture obj 2 -> squads dissolve, refocus onto obj 3
        objs[1].owner = Owner::US;
        Tick(st, bots, objs, FlagIn{}, p, T0 + p.stageTimeoutMs + 4000);
        CHECK(CountJob(st, Job::ASSAULT, 2) == 0);
        CHECK(CountJob(st, Job::ASSAULT, 3) > 0);
    }

    // 6b. Healers are spread across squads (one per squad when possible).
    {
        TeamState st;
        std::vector<ObjIn> objs = { Obj(2, 1000, 0, Owner::ENEMY) };     // nothing to guard
        std::vector<BotIn> bots;
        for (u32 k = 1; k <= 8; ++k) bots.push_back(Bot(k, k <= 2 ? Role::HEAL : Role::MELEE, 5.f * k, 0));
        Tick(st, bots, objs, FlagIn{}, p, T0);
        CHECK(st.squads.size() == 2);
        CHECK(st.assign[1].squadId != st.assign[2].squadId);
    }

    // 8. Reinforce is capped.
    {
        TeamState st;
        std::vector<ObjIn> objs = { Obj(1, 0, 0, Owner::US, 1, 10), Obj(2, 2000, 0, Owner::ENEMY) };
        std::vector<BotIn> bots; for (u32 k = 1; k <= 12; ++k) bots.push_back(Bot(k, Role::MELEE, 300 + k, 0));
        Tick(st, bots, objs, FlagIn{}, p, T0);
        CHECK(CountJob(st, Job::REINFORCE, 1) == p.reinforceMax);
    }

    // 9. A just-lost objective is not re-targeted inside the refocus cooldown.
    {
        TeamState st;
        std::vector<ObjIn> objs = { Obj(1, 0, 0, Owner::US), Obj(2, 500, 0, Owner::ENEMY, 0, 5), Obj(3, 3000, 0, Owner::ENEMY, 0, 5) };
        std::vector<BotIn> bots; for (u32 k = 1; k <= 6; ++k) bots.push_back(Bot(k, Role::MELEE, 5.f * k, 0));
        Tick(st, bots, objs, FlagIn{}, p, T0);
        objs[0].owner = Owner::ENEMY;                     // we lose obj 1 (closest, so otherwise the obvious focus)
        Tick(st, bots, objs, FlagIn{}, p, T0 + 2000);
        CHECK(CountJob(st, Job::ASSAULT, 1) == 0);
        CHECK(CountJob(st, Job::ASSAULT, 2) > 0);
        // Squads are sticky, so the cooldown only steers NEW squads: capture obj 2 (and clear its
        // attackers, or the reinforce rule would rightly pull everyone there) to dissolve them.
        objs[1].owner = Owner::US; objs[1].enemyNear = 0;
        Tick(st, bots, objs, FlagIn{}, p, T0 + 2000 + p.refocusCdMs + 1);
        CHECK(CountJob(st, Job::ASSAULT, 1) > 0);         // cooldown over; obj 1 (enemyNear 0, nearest) is the focus
    }

    // 10. Flag jobs: escorts (healer first), chasers.
    {
        TeamState st;
        std::vector<ObjIn> objs = { Obj(1, 0, 0, Owner::US), Obj(2, 1500, 0, Owner::ENEMY) };
        std::vector<BotIn> bots = { Bot(1, Role::MELEE, 700, 0), Bot(2, Role::HEAL, 720, 0), Bot(3, Role::MELEE, 710, 0),
                                    Bot(4, Role::MELEE, 400, 0), Bot(5, Role::MELEE, 410, 0), Bot(6, Role::MELEE, 420, 0), Bot(7, Role::MELEE, 5, 0), Bot(8, Role::MELEE, 6, 0) };
        FlagIn f; f.ourFcAlive = true; f.ourFcX = 700; f.ourFcY = 0; f.enemyFcAlive = true; f.enemyFcX = 400; f.enemyFcY = 0;
        Tick(st, bots, objs, f, p, T0);
        CHECK(CountJob(st, Job::ESCORT_FC) == p.escortN); CHECK(st.assign[2].job == Job::ESCORT_FC);
        CHECK(CountJob(st, Job::CHASE_FC) == p.chaseN);
        CHECK(CountJob(st, Job::GUARD, 1) == 2);
    }

    // 11. Locked bots keep their assignment; roam is emitted as ROAM.
    {
        TeamState st;
        Params r = p; r.roamPct = 15;
        std::vector<ObjIn> objs = { Obj(1, 0, 0, Owner::US), Obj(2, 1500, 0, Owner::ENEMY) };
        // keys >= 15 so none of them fall in the 15% roam share
        std::vector<BotIn> bots = { Bot(20, Role::MELEE, 5, 0), Bot(21, Role::MELEE, 6, 0), Bot(22, Role::MELEE, 800, 0), Bot(23, Role::MELEE, 810, 0) };
        Tick(st, bots, objs, FlagIn{}, r, T0);
        Assignment before22 = st.assign[22];
        CHECK(before22.job == Job::ASSAULT);
        bots[2].inCombat = true; bots[3].alive = false;
        objs[1].owner = Owner::US;                        // would normally re-seat everyone
        Tick(st, bots, objs, FlagIn{}, r, T0 + 2000);
        CHECK(st.assign[22].job == before22.job && st.assign[22].objectiveId == before22.objectiveId);
        CHECK(st.assign[23].job == before22.job);
        CHECK(st.assign[20].job == Job::GUARD && st.assign[21].job == Job::GUARD);
        std::vector<BotIn> roamers = { Bot(5, Role::MELEE, 0, 0) };   // 5 % 100 < 15
        TeamState st2; Tick(st2, roamers, objs, FlagIn{}, r, T0);
        CHECK(st2.assign[5].job == Job::ROAM);
    }

    // 12. Choke stance: guards hold verified slots by role once the objective has enough guards,
    //     fall back to ON_FLAG when outnumbered by the pullback margin, slots never double-booked.
    {
        TeamState st;
        ObjIn o = Obj(1, 0, 0, Owner::US); o.slots[0] = 1; o.slots[1] = 1; o.slots[2] = 1;   // 1 melee, 1 ranged, 1 healer slot
        std::vector<ObjIn> objs = { o, Obj(2, 2000, 0, Owner::ENEMY) };
        Params q = p; q.guardMin = 3;
        std::vector<BotIn> bots = { Bot(1, Role::MELEE, 5, 0), Bot(2, Role::RANGED, 6, 0), Bot(3, Role::HEAL, 7, 0), Bot(4, Role::MELEE, 8, 0), Bot(5, Role::MELEE, 900, 0) };
        Tick(st, bots, objs, FlagIn{}, q, T0);
        CHECK(CountJob(st, Job::GUARD, 1) == 3);
        CHECK(st.assign[1].stance == Stance::HOLD_CHOKE && st.assign[1].slotKind == 0 && st.assign[1].slotIndex == 0);
        CHECK(st.assign[2].stance == Stance::HOLD_CHOKE && st.assign[2].slotKind == 1);
        CHECK(st.assign[3].stance == Stance::HOLD_CHOKE && st.assign[3].slotKind == 2);
        // a 4th melee guard would find no free melee slot -> ON_FLAG (raise quota to see it)
        Params q4 = q; q4.guardMin = 4;
        TeamState st4; Tick(st4, bots, objs, FlagIn{}, q4, T0);
        CHECK(st4.assign[4].job == Job::GUARD && st4.assign[4].stance == Stance::ON_FLAG);
        // outnumbered => everyone back on the flag
        objs[0].friendlyNear = 3; objs[0].enemyNear = 5;
        Tick(st, bots, objs, FlagIn{}, q, T0 + 2000);
        for (u32 k = 1; k <= 3; ++k) CHECK(st.assign[k].stance == Stance::ON_FLAG);
        // below ChokeMinGuards => ON_FLAG even with slots
        Params q1 = p; q1.guardMin = 1; objs[0].friendlyNear = 0; objs[0].enemyNear = 0;
        TeamState st1; Tick(st1, bots, objs, FlagIn{}, q1, T0);
        CHECK(CountJob(st1, Job::GUARD, 1) == 1);
        for (auto const& [k, a] : st1.assign) if (a.job == Job::GUARD) CHECK(a.stance == Stance::ON_FLAG);
    }

    // 13. A dead objective (destroyed AV tower) draws no guards, reinforcements or assaults, even
    //     though the director hands it to us as Owner::US with the enemy swarming it.
    {
        TeamState st;
        ObjIn live = Obj(1, 0, 0, Owner::US);
        ObjIn gone = Obj(2, 300, 0, Owner::US, 0, 5);   // enemyNear 5 > friendlyNear 0: would beg for REINFORCE
        gone.dead = true;
        std::vector<ObjIn> objs = { live, gone, Obj(3, 1500, 0, Owner::ENEMY) };
        std::vector<BotIn> bots = { Bot(1, Role::MELEE, 5, 0), Bot(2, Role::RANGED, 6, 0), Bot(3, Role::HEAL, 290, 0),
                                    Bot(4, Role::MELEE, 295, 0), Bot(5, Role::MELEE, 1400, 0), Bot(6, Role::RANGED, 1410, 0) };
        Tick(st, bots, objs, FlagIn{}, p, T0);
        CHECK(CountJob(st, Job::GUARD, 2) == 0);
        CHECK(CountJob(st, Job::REINFORCE, 2) == 0);
        CHECK(CountJob(st, Job::ASSAULT, 2) == 0);
        for (auto const& [k, a] : st.assign) CHECK(a.objectiveId != 2);
        CHECK(CountJob(st, Job::GUARD, 1) == 2);                 // the live US objective still gets its quota
        CHECK(CountJob(st, Job::ASSAULT, 3) == 4);               // everyone else squads onto the enemy node
        for (auto const& [sid, sq] : st.squads) CHECK(sq.objectiveId == 3);
        // and with no live enemy objective the dead one is not the "nearest held" fallback either
        TeamState st2;
        std::vector<ObjIn> only = { live, gone };
        Tick(st2, bots, only, FlagIn{}, p, T0);
        for (auto const& [k, a] : st2.assign) CHECK(!(a.job == Job::GUARD && a.objectiveId == 2));
    }

    // 14. Leftover bots join an existing squad instead of founding a one-bot squad. A shrinking guard
    //     quota releases one guard per tick; a squad of one never reaches quorum and only ever pushes
    //     on the stage timeout (seen live: Horde `assault=3 squads=3`, three squads of one bot).
    {
        TeamState st;
        std::vector<ObjIn> objs = { Obj(1, 0, 0, Owner::US), Obj(2, 1000, 0, Owner::ENEMY) };
        Params q = p; q.guardMin = 2;
        std::vector<BotIn> bots;
        for (u32 k = 1; k <= 7; ++k) bots.push_back(Bot(k, Role::MELEE, 5.f * static_cast<float>(k), 0));
        Tick(st, bots, objs, FlagIn{}, q, T0);
        CHECK(CountJob(st, Job::GUARD, 1) == 2);
        CHECK(CountJob(st, Job::ASSAULT, 2) == 5);
        CHECK(st.squads.size() == 2);                            // 5 bots / squadSize 4 -> 2 squads

        q.guardMin = 1;                                          // quota shrinks: one guard is released
        Tick(st, bots, objs, FlagIn{}, q, T0 + 2000);
        CHECK(CountJob(st, Job::GUARD, 1) == 1);
        CHECK(CountJob(st, Job::ASSAULT, 2) == 6);
        CHECK(st.squads.size() == 2);                            // joined an existing squad, founded none
        std::map<u32, u32> perSquad;
        for (auto const& [k, a] : st.assign) if (a.squadId != 0) ++perSquad[a.squadId];
        for (auto const& [sid, n] : perSquad) { (void)sid; CHECK(n > 1); }

        // ...and with no existing squads the plain split still applies: 8 bots -> 2 squads of 4.
        TeamState fresh;
        std::vector<ObjIn> enemyOnly = { Obj(2, 1000, 0, Owner::ENEMY) };
        std::vector<BotIn> eight;
        for (u32 k = 1; k <= 8; ++k) eight.push_back(Bot(k, Role::MELEE, 5.f * static_cast<float>(k), 0));
        Tick(fresh, eight, enemyOnly, FlagIn{}, q, T0);
        CHECK(CountJob(fresh, Job::ASSAULT, 2) == 8);
        CHECK(fresh.squads.size() == 2);
        std::map<u32, u32> freshPer;
        for (auto const& [k, a] : fresh.assign) if (a.squadId != 0) ++freshPer[a.squadId];
        for (auto const& [sid, n] : freshPer) { (void)sid; CHECK(n == 4); }
    }

    // 15. Choke seating counts LIVING guards only. A corpse carried over by step 2 keeps its orders
    //     (step-2 contract) but stops being a defender: it neither holds its slot nor counts toward
    //     ChokeMinGuards. An in-combat living guard is the opposite case — untouched, but still seated.
    {
        // 15a. The melee slot holder dies -> it keeps its carried assignment, the slot passes to the
        //      next living melee guard (guardMin 4 so a 4th guard is on the node to take it).
        TeamState st;
        ObjIn o = Obj(1, 0, 0, Owner::US); o.slots[0] = 1; o.slots[1] = 1; o.slots[2] = 1;
        std::vector<ObjIn> objs = { o, Obj(2, 2000, 0, Owner::ENEMY) };
        Params q = p; q.guardMin = 4;
        std::vector<BotIn> bots = { Bot(1, Role::MELEE, 5, 0), Bot(2, Role::RANGED, 6, 0), Bot(3, Role::HEAL, 7, 0),
                                    Bot(4, Role::MELEE, 8, 0), Bot(5, Role::MELEE, 900, 0) };
        Tick(st, bots, objs, FlagIn{}, q, T0);
        CHECK(CountJob(st, Job::GUARD, 1) == 4);
        CHECK(st.assign[1].stance == Stance::HOLD_CHOKE && st.assign[1].slotKind == 0 && st.assign[1].slotIndex == 0);
        CHECK(st.assign[4].job == Job::GUARD && st.assign[4].stance == Stance::ON_FLAG);   // melee slot taken
        bots[0].alive = false;
        Tick(st, bots, objs, FlagIn{}, q, T0 + 2000);
        CHECK(st.assign[1].job == Job::GUARD && st.assign[1].stance == Stance::HOLD_CHOKE
              && st.assign[1].slotKind == 0 && st.assign[1].slotIndex == 0);               // carried, untouched
        CHECK(st.assign[4].stance == Stance::HOLD_CHOKE && st.assign[4].slotKind == 0 && st.assign[4].slotIndex == 0);
    }
    {
        // 15b. One living + one dead guard with ChokeMinGuards 2: the survivor stays ON the flag
        //      instead of walking out to the choke alone.
        TeamState st;
        ObjIn o = Obj(1, 0, 0, Owner::US); o.slots[0] = 2;
        std::vector<ObjIn> objs = { o };
        Params q = p; q.guardMin = 2; q.chokeMinGuards = 2;
        std::vector<BotIn> bots = { Bot(1, Role::MELEE, 5, 0), Bot(2, Role::MELEE, 6, 0) };
        Tick(st, bots, objs, FlagIn{}, q, T0);
        CHECK(st.assign[1].stance == Stance::HOLD_CHOKE && st.assign[2].stance == Stance::HOLD_CHOKE);
        bots[0].alive = false;
        Tick(st, bots, objs, FlagIn{}, q, T0 + 2000);
        CHECK(st.assign[2].job == Job::GUARD && st.assign[2].stance == Stance::ON_FLAG);
    }
    {
        // 15c. An in-combat guard sitting in melee slot 0 keeps exactly that slot and nobody else is
        //      seated on it — even when a LOWER key takes the seat freed by a guard that left, which
        //      the old key-ordered re-pack handed slot 0 while shuffling the in-combat bot off it.
        TeamState st;
        ObjIn o = Obj(1, 0, 0, Owner::US); o.slots[0] = 2;
        std::vector<ObjIn> objs = { o, Obj(2, 2000, 0, Owner::ENEMY) };
        Params q = p; q.guardMin = 3;
        std::vector<BotIn> bots = { Bot(2, Role::MELEE, 5, 0), Bot(3, Role::MELEE, 6, 0), Bot(4, Role::MELEE, 7, 0),
                                    Bot(9, Role::MELEE, 1900, 0) };
        Tick(st, bots, objs, FlagIn{}, q, T0);
        CHECK(st.assign[2].stance == Stance::HOLD_CHOKE && st.assign[2].slotKind == 0 && st.assign[2].slotIndex == 0);
        CHECK(st.assign[3].stance == Stance::HOLD_CHOKE && st.assign[3].slotIndex == 1);
        CHECK(st.assign[4].stance == Stance::ON_FLAG);
        // slot-0 holder goes into combat (locked by step 2), guard 4 leaves the BG, a lower key arrives
        bots = { Bot(2, Role::MELEE, 5, 0, true, true), Bot(3, Role::MELEE, 6, 0),
                 Bot(1, Role::MELEE, 8, 0), Bot(9, Role::MELEE, 1900, 0) };
        Tick(st, bots, objs, FlagIn{}, q, T0 + 2000);
        CHECK(CountJob(st, Job::GUARD, 1) == 3);
        CHECK(st.assign[2].stance == Stance::HOLD_CHOKE && st.assign[2].slotKind == 0 && st.assign[2].slotIndex == 0);
        CHECK(st.assign[1].stance == Stance::HOLD_CHOKE && st.assign[1].slotKind == 0 && st.assign[1].slotIndex == 1);
        u32 onSlot0 = 0;
        for (auto const& [k, a] : st.assign) if (a.stance == Stance::HOLD_CHOKE && a.slotKind == 0 && a.slotIndex == 0) ++onSlot0;
        CHECK(onSlot0 == 1);
    }

    // 16. Team guard cap. Three held nodes, each within ContestDistance of the enemy node, each want
    //     2 + the contest bonus = 3 guards. In live AB every node is within 300y of another, so the
    //     bonus is always on and the team sank 9 of 15 bots into defence. The cap shaves the LEAST
    //     exposed node first, so the front line keeps its defenders.
    {
        std::vector<ObjIn> objs = { Obj(1, 0, 0, Owner::US), Obj(2, 0, 100, Owner::US), Obj(3, 0, 200, Owner::US),
                                    Obj(4, 0, 250, Owner::ENEMY) };   // obj 3 is nearest the enemy, obj 1 furthest
        std::vector<BotIn> twelve;
        for (u32 k = 1; k <= 12; ++k) twelve.push_back(Bot(k, Role::MELEE, 0, 500.f + static_cast<float>(k)));

        Params q = p; q.guardMin = 2; q.guardMaxPct = 50;
        TeamState st;
        Tick(st, twelve, objs, FlagIn{}, q, T0);
        u32 total = CountJob(st, Job::GUARD);
        CHECK(total <= 6);                                     // 12 assignable * 50%
        CHECK(total >= 3);                                     // never below one guard per held node
        CHECK(CountJob(st, Job::GUARD, 3) >= CountJob(st, Job::GUARD, 1));   // most exposed keeps the most

        // An odd budget makes the ordering strict: 11 bots -> budget 5 -> 2/2/1, the furthest shaved.
        std::vector<BotIn> eleven(twelve.begin(), twelve.begin() + 11);
        TeamState st11;
        Tick(st11, eleven, objs, FlagIn{}, q, T0);
        CHECK(CountJob(st11, Job::GUARD) == 5);
        CHECK(CountJob(st11, Job::GUARD, 3) == 2);
        CHECK(CountJob(st11, Job::GUARD, 2) == 2);
        CHECK(CountJob(st11, Job::GUARD, 1) == 1);

        // Cap off: the uncapped per-objective arithmetic (3 per node) returns.
        Params wide = q; wide.guardMaxPct = 100;
        TeamState stw;
        Tick(stw, twelve, objs, FlagIn{}, wide, T0);
        CHECK(CountJob(stw, Job::GUARD) == 9);
        for (u32 id = 1; id <= 3; ++id) CHECK(CountJob(stw, Job::GUARD, id) == 3);
    }
    {
        // 16b. Double-fill: guards that enter combat leave the pool but keep guarding via the step-2
        //      carry-over. Their seats must count toward the quota, or a whole fresh quota is seated
        //      on top of them (3 guards silently became 6).
        TeamState st;
        Params q = p; q.guardMin = 3; q.guardMaxPct = 100;
        std::vector<ObjIn> objs = { Obj(1, 0, 0, Owner::US), Obj(2, 2000, 0, Owner::ENEMY) };
        std::vector<BotIn> bots;
        for (u32 k = 1; k <= 9; ++k) bots.push_back(Bot(k, Role::MELEE, 5.f * static_cast<float>(k), 0));
        Tick(st, bots, objs, FlagIn{}, q, T0);
        CHECK(CountJob(st, Job::GUARD, 1) == 3);
        for (BotIn& b : bots) if (st.assign[b.key].job == Job::GUARD) b.inCombat = true;
        Tick(st, bots, objs, FlagIn{}, q, T0 + 2000);
        CHECK(CountJob(st, Job::GUARD, 1) == 3);
    }

    if (g_fails) { std::printf("%d check(s) FAILED\n", g_fails); return 1; }
    std::printf("all assignment-engine checks passed\n");
    return 0;
}
