// modules/mod-battleground-bots/src/BgAssignmentEngine.h
// Pure assignment arithmetic for the battleground director. NO AzerothCore includes — compiled
// standalone by tests/test_assignment.cpp. Positions are 2D map yards; times are ms since BG start.
// The director (BattlegroundBotsDirector.cpp) builds the inputs from live state and translates the
// output into BattlegroundBotsApi::BgAssignment (3D target, jitter, z lookup).
#ifndef MOD_BATTLEGROUND_BOTS_ASSIGNMENT_ENGINE_H
#define MOD_BATTLEGROUND_BOTS_ASSIGNMENT_ENGINE_H
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <map>
#include <set>
#include <vector>

namespace BgEngine
{
using u32 = std::uint32_t;
using i32 = std::int32_t;

enum class Job : std::uint8_t { NONE = 0, GUARD, ASSAULT, ESCORT_FC, CHASE_FC, REINFORCE, ROAM };
enum class Stance : std::uint8_t { ON_FLAG = 0, HOLD_CHOKE, STAGE };
enum class Role : std::uint8_t { MELEE = 0, RANGED, HEAL, TANK };
enum class Posture : std::uint8_t { OFFENSIVE = 0, BALANCED, DEFENSIVE };
enum class Owner : std::uint8_t { NEUTRAL = 0, US, ENEMY };

struct Params
{
    u32   guardMin        = 2;
    u32   guardMaxPct     = 50;   // max share of the assignable bots that may be GUARD, team-wide
    u32   contestMs       = 30000;
    float contestDist     = 300.0f;
    u32   escortN         = 2;
    u32   chaseN          = 3;
    u32   reinforceMax    = 4;
    u32   squadSize       = 4;
    u32   squadMinPresent = 3;
    u32   stageTimeoutMs  = 20000;
    u32   refocusCdMs     = 45000;
    u32   roamPct         = 15;
    u32   postureMargin   = 1;
    u32   openingMs       = 60000;
    u32   chokeMinGuards  = 2;
    u32   pullbackMargin  = 2;
};

struct BotIn  { u32 key = 0; bool alive = true; bool inCombat = false; bool human = false; bool isFc = false; Role role = Role::MELEE; float x = 0, y = 0; };
struct ObjIn  { u32 id = 0; float x = 0, y = 0; Owner owner = Owner::NEUTRAL; u32 friendlyNear = 0, enemyNear = 0; u32 lastEnemySeenMs = 0; float stageX = 0, stageY = 0;
                u32 slots[3] = {0, 0, 0};   // verified choke slots per SlotKind (0 melee, 1 ranged, 2 healer)
                bool dead = false;          // destroyed AV tower / enemy IC keep gate: nothing to guard, attack or reinforce
              };
struct FlagIn { bool ourFcAlive = false; float ourFcX = 0, ourFcY = 0; bool enemyFcAlive = false; float enemyFcX = 0, enemyFcY = 0; };

struct Assignment { Job job = Job::NONE; Stance stance = Stance::ON_FLAG; u32 objectiveId = 0; u32 squadId = 0; float x = 0, y = 0; u32 epoch = 0;
                    std::uint8_t slotKind = 0; std::uint8_t slotIndex = 0; };
struct Squad      { u32 objectiveId = 0; u32 stageStartMs = 0; bool pushing = false; };

struct TeamState
{
    Posture posture = Posture::BALANCED;
    Posture pending = Posture::BALANCED;
    std::uint8_t pendingTicks = 0;
    std::map<u32, Assignment> assign;      // by bot key (GUID low)
    std::map<u32, Squad> squads;           // by squad id
    u32 nextSquadId = 1;
    std::map<u32, u32> lostAtMs;           // objective id -> when we lost it
    std::map<u32, Owner> lastOwner;        // objective id -> owner last tick
    u32 epoch = 0;
};

inline float Dist2D(float ax, float ay, float bx, float by) { float dx = ax - bx, dy = ay - by; return std::sqrt(dx * dx + dy * dy); }

// Posture from live score. Two consecutive ticks must agree before it changes; frozen during the opening window.
inline Posture ComputePosture(TeamState& st, i32 scoreMargin, i32 objMargin, Params const& p, u32 nowMs)
{
    if (nowMs < p.openingMs) return st.posture;
    Posture want = Posture::BALANCED;
    if (scoreMargin < -static_cast<i32>(p.postureMargin) || objMargin < 0) want = Posture::OFFENSIVE;
    else if (scoreMargin > static_cast<i32>(p.postureMargin) && objMargin >= 0) want = Posture::DEFENSIVE;
    if (want == st.posture) { st.pendingTicks = 0; st.pending = want; return st.posture; }
    if (want == st.pending) { if (++st.pendingTicks >= 2) { st.posture = want; st.pendingTicks = 0; } }
    else { st.pending = want; st.pendingTicks = 1; }
    return st.posture;
}

// Posture-scaled guard quota WITHOUT the contest bonus (the first thing the team guard cap sheds).
inline u32 GuardQuotaBase(Posture posture, Params const& p)
{
    float q = static_cast<float>(p.guardMin);
    if (posture == Posture::DEFENSIVE) q *= 1.5f;
    if (posture == Posture::OFFENSIVE) q *= 0.5f;
    return std::max<u32>(1u, static_cast<u32>(std::lround(q)));
}

inline u32 GuardQuota(ObjIn const& o, Posture posture, Params const& p, u32 nowMs, float nearestEnemyObjDist)
{
    u32 quota = GuardQuotaBase(posture, p);
    bool recentlySeen = o.lastEnemySeenMs != 0 && nowMs >= o.lastEnemySeenMs && nowMs - o.lastEnemySeenMs <= p.contestMs;
    if (recentlySeen || nearestEnemyObjDist <= p.contestDist) ++quota;
    return quota;
}

inline bool IsRoam(u32 key, Params const& p) { return p.roamPct > 0 && (key % 100u) < p.roamPct; }

inline bool SameTarget(Assignment const& a, Assignment const& b)
{ return a.job == b.job && a.stance == b.stance && a.objectiveId == b.objectiveId && a.squadId == b.squadId
      && a.slotKind == b.slotKind && a.slotIndex == b.slotIndex; }

inline void Tick(TeamState& st, std::vector<BotIn> const& bots, std::vector<ObjIn> const& objs, FlagIn const& flags, Params const& p, u32 nowMs)
{
    // 0. Ownership bookkeeping (feeds the refocus cooldown).
    for (ObjIn const& o : objs)
    {
        auto it = st.lastOwner.find(o.id);
        if (it != st.lastOwner.end() && it->second == Owner::US && o.owner != Owner::US) st.lostAtMs[o.id] = nowMs;
        st.lastOwner[o.id] = o.owner;
    }

    // 1. Prune bots that left.
    std::set<u32> present;
    for (BotIn const& b : bots) present.insert(b.key);
    for (auto it = st.assign.begin(); it != st.assign.end();)
        it = present.count(it->first) ? std::next(it) : st.assign.erase(it);
    std::map<u32, Assignment> const prev = st.assign;
    std::map<u32, Assignment> next;

    // 2. Assignable pool. Humans and the flag carrier get nothing (stock code runs them); dead or
    //    in-combat bots keep last tick's assignment untouched; the roam share is ROAM.
    std::vector<BotIn const*> pool;
    for (BotIn const& b : bots)
    {
        if (b.human || b.isFc) continue;
        if (!b.alive || b.inCombat) { auto it = prev.find(b.key); if (it != prev.end()) next[b.key] = it->second; continue; }
        if (IsRoam(b.key, p)) { Assignment a; a.job = Job::ROAM; next[b.key] = a; continue; }
        pool.push_back(&b);
    }

    auto take = [](std::vector<BotIn const*>& from, float x, float y, auto pred) -> BotIn const*
    {
        std::size_t best = from.size(); float bestD = 0.f;
        for (std::size_t i = 0; i < from.size(); ++i)
        {
            if (!pred(*from[i])) continue;
            float d = Dist2D(from[i]->x, from[i]->y, x, y);
            if (best == from.size() || d < bestD) { best = i; bestD = d; }
        }
        if (best == from.size()) return nullptr;
        BotIn const* b = from[best];
        from.erase(from.begin() + static_cast<std::ptrdiff_t>(best));
        return b;
    };
    auto any    = [](BotIn const&) { return true; };
    auto isHeal = [](BotIn const& b) { return b.role == Role::HEAL; };
    auto prevJobAt = [&](u32 key, Job job, u32 objId) { auto it = prev.find(key); return it != prev.end() && it->second.job == job && it->second.objectiveId == objId; };
    auto objById = [&](u32 id) -> ObjIn const* { for (ObjIn const& o : objs) if (o.id == id) return &o; return nullptr; };
    auto humansNear = [&](ObjIn const& o) { u32 n = 0; for (BotIn const& b : bots) if (b.human && b.alive && Dist2D(b.x, b.y, o.x, o.y) <= 40.f) ++n; return n; };
    auto nearestEnemyObjDist = [&](ObjIn const& o) { float best = 1e9f; for (ObjIn const& e : objs) if (e.owner == Owner::ENEMY) best = std::min(best, Dist2D(o.x, o.y, e.x, e.y)); return best; };
    auto emit = [&](u32 key, Job job, Stance stance, u32 objId, u32 squadId, float x, float y)
    { Assignment a; a.job = job; a.stance = stance; a.objectiveId = objId; a.squadId = squadId; a.x = x; a.y = y; next[key] = a; };

    // 3. Guards: sticky first, then nearest-first fill; humans on the node count toward the quota.
    //    Carried GUARD assignments (step 2's locked bots, which keep last tick's orders) count toward
    //    `kept` as well. Without that an in-combat guard left the pool, `kept` restarted at 0 and a
    //    whole fresh quota was seated ON TOP of the originals, which keep guarding via the carry-over
    //    (seen live in AB: 9 of 15 bots guarding).
    //    A team-wide budget then caps the total: guardMaxPct percent of the ASSIGNABLE bots (the free
    //    pool plus the living locked bots), never below one guard per held objective. Objectives are
    //    ordered most-exposed-first (nearest enemy objective) and shaved from the BACK: the +1 contest
    //    bonus goes first, then guardMin, then 1 — so the front line keeps its defenders. Live AB made
    //    this mandatory: every node is within ContestDistance of another, so the bonus was always on.
    std::vector<ObjIn const*> held;
    for (ObjIn const& o : objs) if (!o.dead && o.owner == Owner::US) held.push_back(&o);
    std::stable_sort(held.begin(), held.end(),
                     [&](ObjIn const* a, ObjIn const* b) { return nearestEnemyObjDist(*a) < nearestEnemyObjDist(*b); });

    u32 assignable = static_cast<u32>(pool.size());
    for (BotIn const& b : bots) if (!b.human && !b.isFc && b.alive && b.inCombat) ++assignable;
    u32 const guardBudget = std::max<u32>(static_cast<u32>(held.size()), assignable * p.guardMaxPct / 100u);

    std::vector<u32> full(held.size(), 0), humans(held.size(), 0), carried(held.size(), 0), tier(held.size(), 0);
    for (std::size_t i = 0; i < held.size(); ++i)
    {
        full[i]   = GuardQuota(*held[i], st.posture, p, nowMs, nearestEnemyObjDist(*held[i]));
        humans[i] = humansNear(*held[i]);
        for (auto const& kv : next) if (kv.second.job == Job::GUARD && kv.second.objectiveId == held[i]->id) ++carried[i];
    }
    u32 const quotaBase = GuardQuotaBase(st.posture, p);
    auto botQuota = [&](std::size_t i) -> u32
    {
        u32 q = full[i];
        if (tier[i] >= 1) q = std::min(q, quotaBase);      // drop the contest bonus
        if (tier[i] >= 2) q = std::min(q, p.guardMin);     // drop the posture scaling
        if (tier[i] >= 3) q = std::min<u32>(q, 1u);        // token holder
        return q > humans[i] ? q - humans[i] : 0u;
    };
    auto guardTotal = [&]() { u32 t = 0; for (std::size_t i = 0; i < held.size(); ++i) t += botQuota(i); return t; };
    for (u32 step = 1; step <= 3 && guardTotal() > guardBudget; ++step)
        for (std::size_t i = held.size(); i-- > 0 && guardTotal() > guardBudget;) tier[i] = step;

    for (std::size_t i = 0; i < held.size(); ++i)
    {
        ObjIn const& o = *held[i];
        u32 const quota = botQuota(i);
        u32 kept = carried[i];
        for (std::size_t j = 0; j < pool.size() && kept < quota;)
        {
            if (prevJobAt(pool[j]->key, Job::GUARD, o.id))
            { emit(pool[j]->key, Job::GUARD, Stance::ON_FLAG, o.id, 0, o.x, o.y); pool.erase(pool.begin() + static_cast<std::ptrdiff_t>(j)); ++kept; }
            else ++j;
        }
        while (kept < quota)
        {
            BotIn const* b = take(pool, o.x, o.y, any);
            if (!b) break;
            emit(b->key, Job::GUARD, Stance::ON_FLAG, o.id, 0, o.x, o.y); ++kept;
        }
    }

    // 3b. Choke stance for guards: with enough LIVING guards and no enemy surplus, hand out verified
    //     slots by role (melee/tank -> melee slot, ranged -> ranged, heal -> healer), lowest key first
    //     so the seating is stable; anyone without a free slot, or an outnumbered objective, is ON_FLAG.
    //     Only LIVING bots are defenders: a corpse carried over by step 2 would otherwise keep its seat
    //     and count toward chokeMinGuards, sending a lone survivor out to the choke on its own.
    //     An in-combat living guard is never re-seated (step-2 contract keeps it untouched), but if it
    //     is already holding a choke slot HERE that exact slot index is reserved for it and it counts
    //     as a defender.
    for (ObjIn const& o : objs)
    {
        if (o.dead) continue;
        if (o.owner != Owner::US) continue;
        if (o.enemyNear >= o.friendlyNear + p.pullbackMargin) continue;   // outnumbered: ON_FLAG already
        auto botByKey = [&](u32 key) -> BotIn const*
        { for (BotIn const& c : bots) if (c.key == key) return &c; return nullptr; };
        std::vector<u32> guards;     // living, free to be (re-)seated
        u32 lockedHolders = 0;       // in-combat living guards already sitting in a choke slot here
        u32 taken[3] = {0, 0, 0};    // bitmask of slot indices already held, per SlotKind
        for (auto const& [key, a] : next)
        {
            if (a.job != Job::GUARD || a.objectiveId != o.id) continue;
            BotIn const* b = botByKey(key);
            if (!b || !b->alive) continue;
            if (b->inCombat)
            {
                if (a.stance == Stance::HOLD_CHOKE && a.slotKind < 3 && a.slotIndex < 32)
                { taken[a.slotKind] |= 1u << a.slotIndex; ++lockedHolders; }
                continue;
            }
            guards.push_back(key);
        }
        if (guards.size() + lockedHolders < p.chokeMinGuards) continue;   // ON_FLAG already
        std::sort(guards.begin(), guards.end());
        for (u32 key : guards)
        {
            BotIn const* b = botByKey(key);
            if (!b) continue;
            std::uint8_t kind = (b->role == Role::HEAL) ? 2 : (b->role == Role::RANGED) ? 1 : 0;
            u32 idx = 0;
            while (idx < 32 && idx < o.slots[kind] && (taken[kind] & (1u << idx))) ++idx;
            if (idx >= 32 || idx >= o.slots[kind]) continue;              // no free slot of this kind
            taken[kind] |= 1u << idx;
            Assignment& a = next[key];
            a.stance = Stance::HOLD_CHOKE; a.slotKind = kind; a.slotIndex = static_cast<std::uint8_t>(idx);
        }
    }

    // 4. Flag jobs (WS/EY): escorts (healer preferred for the first seat), chasers.
    if (flags.ourFcAlive)
        for (u32 n = 0; n < p.escortN; ++n)
        {
            BotIn const* b = (n == 0) ? take(pool, flags.ourFcX, flags.ourFcY, isHeal) : nullptr;
            if (!b) b = take(pool, flags.ourFcX, flags.ourFcY, any);
            if (!b) break;
            emit(b->key, Job::ESCORT_FC, Stance::ON_FLAG, 0, 0, flags.ourFcX, flags.ourFcY);
        }
    if (flags.enemyFcAlive)
        for (u32 n = 0; n < p.chaseN; ++n)
        {
            BotIn const* b = take(pool, flags.enemyFcX, flags.enemyFcY, any);
            if (!b) break;
            emit(b->key, Job::CHASE_FC, Stance::ON_FLAG, 0, 0, flags.enemyFcX, flags.enemyFcY);
        }

    // 5. Reinforce objectives where the enemy outnumbers us.
    for (ObjIn const& o : objs)
    {
        if (o.dead) continue;
        if (o.owner != Owner::US || o.enemyNear <= o.friendlyNear) continue;
        u32 need = std::min<u32>(o.enemyNear - o.friendlyNear + 1, p.reinforceMax);
        for (u32 n = 0; n < need; ++n)
        {
            BotIn const* b = take(pool, o.x, o.y, any);
            if (!b) break;
            emit(b->key, Job::REINFORCE, Stance::ON_FLAG, o.id, 0, o.x, o.y);
        }
    }

    // 6a. Dissolve squads whose objective is ours or whose members are all gone/dead.
    for (auto it = st.squads.begin(); it != st.squads.end();)
    {
        ObjIn const* o = objById(it->second.objectiveId);
        bool anyAlive = false;
        for (BotIn const& b : bots)
        {
            auto pa = prev.find(b.key);
            if (b.alive && pa != prev.end() && pa->second.squadId == it->first) { anyAlive = true; break; }
        }
        if (!o || o->owner == Owner::US || !anyAlive) it = st.squads.erase(it); else ++it;
    }

    // 6b. Focus: the enemy/neutral objective with the fewest defenders, nearest to our free bots,
    //     skipping one we lost inside the refocus cooldown.
    float cx = 0.f, cy = 0.f;
    if (!pool.empty()) { for (BotIn const* b : pool) { cx += b->x; cy += b->y; } cx /= static_cast<float>(pool.size()); cy /= static_cast<float>(pool.size()); }
    ObjIn const* focus = nullptr; float bestScore = 0.f;
    for (ObjIn const& o : objs)
    {
        if (o.dead) continue;   // explicit: never a focus even if the director stops forcing owner=US
        if (o.owner == Owner::US) continue;
        auto lost = st.lostAtMs.find(o.id);
        if (lost != st.lostAtMs.end() && nowMs - lost->second < p.refocusCdMs) continue;
        float score = static_cast<float>(o.enemyNear) * 3.0f + Dist2D(cx, cy, o.x, o.y) / 100.0f;
        if (!focus || score < bestScore) { focus = &o; bestScore = score; }
    }

    // 6c. Members of surviving squads stay in them.
    std::map<u32, std::vector<BotIn const*>> members;
    for (std::size_t i = 0; i < pool.size();)
    {
        auto pa = prev.find(pool[i]->key);
        if (pa != prev.end() && pa->second.squadId != 0 && st.squads.count(pa->second.squadId))
        { members[pa->second.squadId].push_back(pool[i]); pool.erase(pool.begin() + static_cast<std::ptrdiff_t>(i)); }
        else ++i;
    }

    // 6d. Whoever is left. LEFTOVER RULE: if squads for the focus objective already exist and the
    //     leftovers are too few to fill a squad of their own, they JOIN the smallest of those squads
    //     (healers first) and no squad is created. A squad of one can never reach quorum, so it stages
    //     alone and only pushes on the stage timeout — and a shrinking guard quota releases exactly one
    //     guard per tick, so the old unconditional split founded a fresh one-bot squad every time
    //     (seen live: `assault=3 squads=3`). Otherwise: new squads of squadSize, healers dealt
    //     round-robin first so each squad gets one.
    if (focus && !pool.empty())
    {
        std::vector<BotIn const*> heals, others;
        for (BotIn const* b : pool) (b->role == Role::HEAL ? heals : others).push_back(b);

        std::vector<u32> onFocus;
        for (auto const& kv : members)
        {
            auto s = st.squads.find(kv.first);
            if (s != st.squads.end() && s->second.objectiveId == focus->id) onFocus.push_back(kv.first);
        }

        if (!onFocus.empty() && pool.size() < p.squadSize)
        {
            auto smallest = [&]() -> std::vector<BotIn const*>&
            {
                u32 best = onFocus[0];
                for (u32 sid : onFocus) if (members[sid].size() < members[best].size()) best = sid;
                return members[best];
            };
            for (BotIn const* b : heals)  smallest().push_back(b);
            for (BotIn const* b : others) smallest().push_back(b);
        }
        else
        {
            u32 k = std::max<u32>(1u, static_cast<u32>((pool.size() + p.squadSize - 1) / std::max<u32>(1u, p.squadSize)));
            std::vector<u32> ids;
            for (u32 i = 0; i < k; ++i) { u32 sid = st.nextSquadId++; Squad s; s.objectiveId = focus->id; s.stageStartMs = nowMs; st.squads[sid] = s; ids.push_back(sid); }
            u32 r = 0;
            for (BotIn const* b : heals)  members[ids[r++ % k]].push_back(b);
            for (BotIn const* b : others) members[ids[r++ % k]].push_back(b);
        }
        pool.clear();
    }
    else if (!pool.empty())
    {
        // Nothing attackable: guard the nearest held objective, else roam.
        for (BotIn const* b : pool)
        {
            ObjIn const* nearest = nullptr; float bd = 0.f;
            for (ObjIn const& o : objs) if (o.owner == Owner::US && !o.dead) { float d = Dist2D(b->x, b->y, o.x, o.y); if (!nearest || d < bd) { nearest = &o; bd = d; } }
            if (nearest) emit(b->key, Job::GUARD, Stance::ON_FLAG, nearest->id, 0, nearest->x, nearest->y);
            else         emit(b->key, Job::ROAM, Stance::ON_FLAG, 0, 0, 0.f, 0.f);
        }
        pool.clear();
    }

    // 6e. Squad stance: STAGE at the staging point until quorum (or everyone) is there or the
    //     timeout passes, then ASSAULT the objective. `pushing` latches for the squad's life.
    for (auto& [sid, mem] : members)
    {
        Squad& s = st.squads[sid];
        ObjIn const* o = objById(s.objectiveId);
        if (!o) continue;
        if (!s.pushing)
        {
            u32 presentN = 0;
            for (BotIn const* b : mem) if (Dist2D(b->x, b->y, o->stageX, o->stageY) <= 20.f) ++presentN;
            if (presentN >= p.squadMinPresent || presentN >= mem.size() || nowMs - s.stageStartMs >= p.stageTimeoutMs) s.pushing = true;
        }
        for (BotIn const* b : mem)
        {
            if (s.pushing) emit(b->key, Job::ASSAULT, Stance::ON_FLAG, o->id, sid, o->x, o->y);
            else           emit(b->key, Job::ASSAULT, Stance::STAGE,   o->id, sid, o->stageX, o->stageY);
        }
    }

    // 7. Epochs: unchanged target keeps its epoch; anything else bumps the team counter.
    for (auto& [key, a] : next)
    {
        auto it = prev.find(key);
        if (it != prev.end() && SameTarget(it->second, a)) a.epoch = it->second.epoch;
        else a.epoch = ++st.epoch;
    }
    st.assign = std::move(next);
}
} // namespace BgEngine
#endif
