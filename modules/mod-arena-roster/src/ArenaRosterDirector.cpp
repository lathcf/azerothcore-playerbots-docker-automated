#include "ArenaRosterDirector.h"
#include "ArenaRosterConfig.h"
#include "ArenaRosterComp.h"
#include "ArenaRosterGear.h"
#include "ArenaTeam.h"
#include "ArenaTeamMgr.h"
#include "Battleground.h"
#include "BattlegroundMgr.h"
#include "BattlegroundQueue.h"
#include "CharacterCache.h"
#include "Creature.h"
#include "DBCStores.h"
#include "DatabaseEnv.h"
#include "Field.h"
#include "Group.h"
#include "GroupMgr.h"
#include "Log.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "Opcodes.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotFactory.h"
#include "EraTalentBots.h"   // era-managed bots get era builds at sync (mod-era-talents)
#include "Playerbots.h"
#include "QueryResult.h"
#include "RandomPlayerbotMgr.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "Mgr/Guild/PlayerbotGuildMgr.h"
#include <algorithm>
#include <map>
#include <tuple>
#include <type_traits>
#include <unordered_set>

namespace
{
// Opponent ladder tiers. One pinned rating per tier comes from config — keep them in lockstep.
constexpr uint8 TIER_COUNT = 4;
static_assert(std::tuple_size_v<decltype(g_ArTierRatings)> == TIER_COUNT,
              "g_ArTierRatings must define exactly one rating per opponent tier");

// Season of each opponent tier's gear (index = tier - 1). Deliberately skips Furious so the
// tier-to-tier gear jump is visible: T1 fresh-80 blues, T2 S5 arena, T3 S7, T4 S8 endgame.
constexpr ArenaSeason TIER_SEASON[TIER_COUNT] = { SEASON_SAVAGE, SEASON_DEADLY, SEASON_RELENTLESS, SEASON_WRATHFUL };

// PrepareBots per-tick batch: the gear engine runs ~40 (cached, but still per-bot scored) item
// probes + a factory Randomize per bot — 3/tick keeps the world-tick spike bounded.
constexpr uint8  PREPARE_BATCH_PER_TICK = 3;
constexpr uint32 LOGIN_WAVE_TIMEOUT_MS  = 60 * 1000;
constexpr uint32 PREPARE_TIMEOUT_MS     = 180 * 1000;

constexpr size_t TIER_SIZE = ARENA_POOL_ROSTER.size();   // bots per tier (15)
constexpr size_t POOL_SIZE = TIER_COUNT * TIER_SIZE;     // total pool bots (60)
constexpr size_t TEAMS_PER_TIER =                        // 7 + 5 + 3 = 15 teams per tier
    ARENA_POOL_2V2.size() + ARENA_POOL_3V3.size() + ARENA_POOL_5V5.size();

// Engagement timeouts: every state either progresses or aborts — the machine can never hang.
constexpr uint32 ENG_LOGIN_TIMEOUT_MS = 45 * 1000;
// No opponent paired -> abort + drain. Pairing inside this window leans on the setup.sh
// matchmaker knobs (Arena.MaxRatingDifference=500, Arena.RatingDiscardTimer=60000): with the
// stock core values (150 / 600000) the MMR gate wouldn't discard until long after this
// timeout and most engagements would abort unpaired. Task 9 documents those knobs as
// load-bearing for this module.
constexpr uint32 ENG_QUEUE_TIMEOUT_MS = 120 * 1000;
constexpr uint32 ENG_FIGHT_TIMEOUT_MS = 60 * 60 * 1000;   // hard cap; arena curfew is ~47 min

std::string PoolTeamName(uint8 tier, char const* baseName)
{
    return "T" + std::to_string(tier) + " " + baseName;
}

// 2/3/5 -> arena slot (a.k.a. bracket index) 0/1/2. Callers validate arenaType first.
uint8 ArenaSlotFor(uint8 arenaType)
{
    return arenaType == ARENA_TYPE_2v2 ? 0 : (arenaType == ARENA_TYPE_3v3 ? 1 : 2);
}
}

ArenaRosterDirector* ArenaRosterDirector::instance = nullptr;

ArenaRosterDirector::ArenaRosterDirector() : WorldScript("ArenaRosterDirector")
{
    // Size invariants between the comp tables, the store row and this class's scratch state
    // (asserted here because the private members are only visible in class scope).
    static_assert(std::tuple_size_v<decltype(_teamIds)> == ARENA_POOL_ROSTER.size(),
                  "_teamIds must have one entry per ARENA_POOL_ROSTER slot");
    static_assert(std::extent_v<decltype(ArenaPoolRow::team)> == BRACKET_COUNT,
                  "ArenaPoolRow::team must have one id per bracket (2v2/3v3/5v5)");
    static_assert(std::tuple_size_v<decltype(_lastServedTeam)> == TIER_COUNT,
                  "_lastServedTeam must have one row per opponent tier");
    instance = this;
}

bool ArenaRosterDirector::PoolBusy() const
{
    return _poolPhase != PoolPhase::Idle && _poolPhase != PoolPhase::Done && _poolPhase != PoolPhase::Failed;
}

char const* ArenaRosterDirector::PhaseName(PoolPhase phase)
{
    switch (phase)
    {
        case PoolPhase::Idle:        return "idle";
        case PoolPhase::PinBots:     return "pinning bots";
        case PoolPhase::LoginWave:   return "waiting for logins";
        case PoolPhase::PrepareBots: return "leveling/gearing bots";
        case PoolPhase::CreateTeams: return "creating teams";
        case PoolPhase::LogoutWave:  return "logging tier out";
        case PoolPhase::Done:        return "DONE";
        case PoolPhase::Failed:      return "FAILED";
    }
    return "?";
}

void ArenaRosterDirector::OnUpdate(uint32 diff)
{
    // Enable-flag discipline: a registered WorldScript ticks even when the module is
    // "disabled" — bail before touching ANY state.
    if (!g_ArEnable)
        return;

    _accumMs += diff;
    if (_accumMs < 1000)
        return;
    uint32 elapsed = _accumMs;
    _accumMs = 0;

    if (PoolBusy())
        TickPoolInit(elapsed);

    TickQueueWatch(elapsed);
}

// ---------------------------------------------------------------------------
// Task 8: queue director — watch, serve, teardown, renormalize.
// ---------------------------------------------------------------------------

void ArenaRosterDirector::TickQueueWatch(uint32 elapsedMs)
{
    // One engagement at a time: while one runs, only advance it (no scan — the player's queue
    // entry would otherwise re-trigger a serve every second).
    if (_eng.active)
    {
        TickEngagement(elapsedMs);
        return;
    }

    // Ordering vs the pool machine: a running poolinit owns the pool bots (logins, gearing,
    // team churn), so never serve mid-job — the scan resumes once it settles in
    // Idle/Done/Failed. The reverse guard lives in StartPoolInit.
    if (PoolBusy())
        return;

    // Scan REAL players for a rated-arena queue join — mirrors the real-player pass of
    // RandomPlayerbotMgr::CheckBgQueue (RandomPlayerbotMgr.cpp:936-990). GetPlayers() holds
    // every non-random-bot session, which includes addclass bot logins — the
    // GET_PLAYERBOT_AI check drops all bots.
    for (Player* p : sRandomPlayerbotMgr.GetPlayers())
    {
        if (!p || GET_PLAYERBOT_AI(p))
            continue;
        if (p->InArena() || !p->InBattlegroundQueue())
            continue;
        if (p->IsInvitedForBattlegroundInstance())
            continue;   // match already found — too late to serve this queue

        for (uint8 i = 0; i < PLAYER_MAX_BATTLEGROUND_QUEUES; ++i)
        {
            BattlegroundQueueTypeId qtid = p->GetBattlegroundQueueTypeId(i);
            if (qtid == BATTLEGROUND_QUEUE_NONE)
                continue;
            uint8 arenaType = BattlegroundMgr::BGArenaType(qtid);
            if (!arenaType)
                continue;   // a plain BG queue
            GroupQueueInfo ginfo;
            if (!sBattlegroundMgr->GetBattlegroundQueue(qtid).GetPlayerGroupInfoData(p->GetGUID(), &ginfo))
                continue;
            if (!ginfo.IsRated)
                continue;   // skirmish — stock bot ambiance handles it

            // The player's team rating for this bracket picks the opponent tier.
            uint32 teamId = p->GetArenaTeamId(ArenaSlotFor(arenaType));
            ArenaTeam* team = teamId ? sArenaTeamMgr->GetArenaTeamById(teamId) : nullptr;
            if (!team)
                continue;
            uint8 tier = uint8(ArenaRosterTierFor(team->GetRating())) + 1;   // 1-based

            std::string err;
            if (ServeOpponents(tier, arenaType, err))
                LOG_INFO("playerbots", "[ArenaRoster] {} queued rated {}v{} at team rating {} -> serving tier {} opponents.",
                         p->GetName(), arenaType, arenaType, team->GetRating(), tier);
            else if (g_ArDebug)
                LOG_INFO("playerbots", "[ArenaRoster] {} queued rated {}v{} but no opponents served: {}",
                         p->GetName(), arenaType, arenaType, err);
            return;   // serve at most one lineup per tick — and stop the scan either way
        }
    }
}

bool ArenaRosterDirector::ForceQueue(uint8 tier, uint8 arenaType, std::string& err)
{
    // TEST-ONLY thin wrapper for `.arenaroster forcequeue` — all guards (engagement already
    // active, poolinit running, bad tier/bracket, empty pool) live in ServeOpponents.
    return ServeOpponents(tier, arenaType, err);
}

bool ArenaRosterDirector::ServeOpponents(uint8 tier, uint8 arenaType, std::string& err)
{
    if (_eng.active)
    {
        err = "an engagement is already active (one at a time)";
        return false;
    }
    // Poolinit <-> engagement mutual exclusion (serve side; the reverse lives in StartPoolInit).
    if (PoolBusy())
    {
        err = "a poolinit job is running (watch .arenaroster poolstatus)";
        return false;
    }
    if (tier < 1 || tier > TIER_COUNT)
    {
        err = "tier must be 1-" + std::to_string(TIER_COUNT);
        return false;
    }
    if (arenaType != ARENA_TYPE_2v2 && arenaType != ARENA_TYPE_3v3 && arenaType != ARENA_TYPE_5v5)
    {
        err = "arena type must be 2, 3 or 5";
        return false;
    }

    // The tier's lineups for this bracket, straight from the store (LoadPool ONCE per serve —
    // never per tick). std::map keeps team ids ordered for the rotation below.
    uint8 bracketIdx = ArenaSlotFor(arenaType);
    std::map<uint32, std::vector<ObjectGuid>> lineups;
    for (ArenaPoolRow const& row : ArenaRosterStore::LoadPool())
        if (row.tier == tier && row.team[bracketIdx])
            lineups[row.team[bracketIdx]].push_back(ObjectGuid::Create<HighGuid::Player>(row.botGuid));
    if (lineups.empty())
    {
        err = "no tier-" + std::to_string(tier) + " " + std::to_string(arenaType) + "v" +
              std::to_string(arenaType) + " teams in the pool (run .arenaroster poolinit)";
        return false;
    }

    // Rotate away from the last lineup served for this (tier, bracket): first team id above
    // it, wrap. Keyed per tier so alternating tiers in one bracket still rotates each tier.
    // A pool row whose team id no longer resolves (team disbanded/deleted since poolinit) is
    // SKIPPED — advancing the rotation cursor past it — so a single dead id never wedges every
    // serve on the same failure; only when NO team of this tier/bracket resolves do we report
    // the re-run-poolinit error.
    auto it = lineups.upper_bound(_lastServedTeam[tier - 1][bracketIdx]);
    ArenaTeam* team = nullptr;
    for (size_t attempts = 0; attempts < lineups.size(); ++attempts)
    {
        if (it == lineups.end())
            it = lineups.begin();
        team = sArenaTeamMgr->GetArenaTeamById(it->first);
        if (team)
            break;
        LOG_INFO("playerbots", "[ArenaRoster] pool team #{} no longer exists — skipping "
                 "(stale mod_arena_pool row; re-run poolinit to clean up).", it->first);
        _lastServedTeam[tier - 1][bracketIdx] = it->first;   // rotate past the dead id
        ++it;
    }
    if (!team)
    {
        err = "no tier-" + std::to_string(tier) + " " + std::to_string(arenaType) + "v" +
              std::to_string(arenaType) + " pool team still exists (re-run poolinit)";
        return false;
    }

    _eng = Engagement{};
    _eng.active = true;
    _eng.arenaType = arenaType;
    _eng.tier = tier;
    _eng.poolTeamId = it->first;
    _eng.captainGuid = team->GetCaptain();
    _eng.bots = it->second;
    _lastServedTeam[tier - 1][bracketIdx] = it->first;

    // Masterless random-bot-managed logins — same call as poolinit's PhasePinBots (addclass
    // accounts sit outside random-bot churn, so nothing logs them out under us).
    for (ObjectGuid g : _eng.bots)
        sRandomPlayerbotMgr.AddPlayerBot(g, 0);

    LOG_INFO("playerbots", "[ArenaRoster] serving tier {} {}v{}: <{}> (team #{}, {} bots logging in).",
             tier, arenaType, arenaType, team->GetName(), _eng.poolTeamId, _eng.bots.size());
    return true;
}

void ArenaRosterDirector::TickEngagement(uint32 elapsedMs)
{
    _eng.stateMs += elapsedMs;

    switch (_eng.st)
    {
        case Engagement::St::LoggingIn:
        {
            uint32 online = 0;
            for (ObjectGuid g : _eng.bots)
            {
                Player* bot = ObjectAccessor::FindConnectedPlayer(g);
                if (bot && bot->IsInWorld())
                    ++online;
                else
                    // Re-arm logins that silently didn't stick (AddPlayerBot early-outs while
                    // a login holder is in flight) — same pattern as poolinit's LoginWave.
                    sRandomPlayerbotMgr.AddPlayerBot(g, 0);
            }
            if (online == _eng.bots.size())
            {
                _eng.st = Engagement::St::Queueing;
                _eng.stateMs = 0;
                return;
            }
            if (_eng.stateMs > ENG_LOGIN_TIMEOUT_MS)
                AbortEngagement("login wave timed out with " + std::to_string(online) + "/" +
                                std::to_string(_eng.bots.size()) + " bots online");
            return;
        }
        case Engagement::St::Queueing:
        {
            Player* captain = ObjectAccessor::FindConnectedPlayer(_eng.captainGuid);
            if (!captain || !captain->IsInWorld())
            {
                AbortEngagement("captain went offline while queueing");
                return;
            }
            if (_eng.stateMs > ENG_QUEUE_TIMEOUT_MS)
            {
                // Sub-step flags make a dead engagement diagnosable post-hoc: bm=n means the
                // battlemaster never resolved, grouped=n the group step, queued=n the join
                // packet; all y = we queued fine but nobody paired with us.
                std::string detail = std::string("bm=") + (_eng.bmGuid ? "y" : "n") +
                                     " grouped=" + (_eng.grouped ? "y" : "n") +
                                     " queued=" + (_eng.queued ? "y" : "n");
                AbortEngagement("no match within " + std::to_string(ENG_QUEUE_TIMEOUT_MS / 1000) +
                                "s [" + detail + "]" +
                                (_eng.queued ? " (opponent left the queue?)" : ""));
                return;
            }
            TickEngagementQueueing(captain);
            return;
        }
        case Engagement::St::Fighting:
        {
            // No DB and no pool reads here — everything needed is cached in _eng.
            Player* captain = ObjectAccessor::FindConnectedPlayer(_eng.captainGuid);
            if (captain && (captain->InArena() || captain->InBattlegroundQueue()) &&
                _eng.stateMs <= ENG_FIGHT_TIMEOUT_MS)
                return;   // match (or its short invite gap) still running

            LOG_INFO("playerbots", "[ArenaRoster] tier {} {}v{} match over — renormalizing and draining (team #{}).",
                     _eng.tier, _eng.arenaType, _eng.arenaType, _eng.poolTeamId);
            RenormalizeServedTeam();
            _eng.st = Engagement::St::Draining;
            _eng.stateMs = 0;
            _eng.graceMs = 0;
            return;
        }
        case Engagement::St::Draining:
        {
            _eng.graceMs += elapsedMs;
            if (_eng.graceMs < g_ArGraceLogoutSecs * 1000)
                return;
            for (ObjectGuid g : _eng.bots)
                sRandomPlayerbotMgr.LogoutPlayerBot(g);   // no-op for already-offline bots
            LOG_INFO("playerbots", "[ArenaRoster] engagement done: tier {} team #{} logged out after {}s grace.",
                     _eng.tier, _eng.poolTeamId, g_ArGraceLogoutSecs);
            _eng = Engagement{};
            return;
        }
    }
}

// Queueing sub-steps, one per 1s tick so each world-visible action settles before the next:
// (1) resolve an arena battlemaster near the captain (teleporting him to one if his map has
// none), (2) form the arena-team group, (3) send the rated CMSG_BATTLEMASTER_JOIN_ARENA, then
// wait for captain->InArena(). Bounded by ENG_QUEUE_TIMEOUT_MS in the caller.
void ArenaRosterDirector::TickEngagementQueueing(Player* captain)
{
    if (!_eng.queued)
    {
        // (1) Battlemaster. GetBattleMasterGUID only returns units on the CAPTAIN'S map with
        // loaded grids (RandomPlayerbotMgr.cpp:3089) — and the join handler itself resolves
        // the guid via GetPlayer()->GetMap()->GetCreature (BattleGroundHandler.cpp:688), so
        // same-map is a hard requirement, not bot lore. Pool bots idle at race starts (some on
        // map 609, which has no battlemaster at all) — teleport the captain to a known arena
        // battlemaster spawn once, then keep re-resolving until the grid loads.
        if (!_eng.bmGuid)
        {
            _eng.bmGuid = sRandomPlayerbotMgr.GetBattleMasterGUID(captain, BATTLEGROUND_AA);
            if (!_eng.bmGuid)
            {
                if (!_eng.bmTeleported)
                    TeleportCaptainToBattlemaster(captain);
                return;
            }
        }

        // (2) Group the team around the captain, then let it settle a tick.
        if (!_eng.grouped)
        {
            if (!GroupEngagementTeam(captain))
                AbortEngagement("couldn't group the pool team around its captain");
            else
                _eng.grouped = true;
            return;
        }

        // (3) Rated queue — mirror BGJoinAction::JoinQueue (BattleGroundJoinAction.cpp:546).
        Creature* bm = captain->GetMap()->GetCreature(_eng.bmGuid);
        if (!bm)
        {
            _eng.bmGuid.Clear();   // grid unloaded under us — re-resolve next tick
            return;
        }
        WorldPacket* packet = new WorldPacket(CMSG_BATTLEMASTER_JOIN_ARENA, 20);
        *packet << bm->GetGUID() << ArenaSlotFor(_eng.arenaType) << uint8(1) /*asGroup*/
                << uint8(1) /*isRated*/;
        captain->GetSession()->QueuePacket(packet);

        // Bookkeeping so stock shouldJoinBg sees this rated queue as already served
        // (BattleGroundJoinAction.cpp:262 does the same TeamSize add for its own joins).
        BattlegroundQueueTypeId qtid = BattlegroundMgr::BGQueueTypeId(BATTLEGROUND_AA, _eng.arenaType);
        if (Battleground* bgt = sBattlegroundMgr->GetBattlegroundTemplate(BATTLEGROUND_AA))
            if (PvPDifficultyEntry const* pvpDiff = GetBattlegroundBracketByLevel(bgt->GetMapId(), captain->GetLevel()))
                sRandomPlayerbotMgr.BattlegroundData[qtid][pvpDiff->GetBracketId()].ratedArenaBotCount += _eng.arenaType;

        _eng.queued = true;
        LOG_INFO("playerbots", "[ArenaRoster] <{}> (team #{}) queued rated {}v{} — waiting for the match.",
                 captain->GetName(), _eng.poolTeamId, _eng.arenaType, _eng.arenaType);
        return;
    }

    if (captain->InArena())
    {
        LOG_INFO("playerbots", "[ArenaRoster] tier {} {}v{} match started (team #{}).",
                 _eng.tier, _eng.arenaType, _eng.arenaType, _eng.poolTeamId);
        _eng.st = Engagement::St::Fighting;
        _eng.stateMs = 0;
    }
    // Not in arena yet: keep waiting (invite/teleport in flight, or the handler rejected the
    // join — the state timeout aborts that case).
}

// Mirror of mod-playerbots' gatherArenaTeam group block (BattleGroundJoinAction.cpp:129-178):
// disband the captain's old group, Create+AddGroup a fresh one, pull every member in (dropping
// their stale groups) and teleport them to the captain.
bool ArenaRosterDirector::GroupEngagementTeam(Player* captain)
{
    if (captain->GetGroup())
        captain->GetGroup()->Disband(true);

    Group* group = new Group();
    if (!group->Create(captain))
    {
        delete group;
        return false;
    }
    sGroupMgr->AddGroup(group);

    for (ObjectGuid g : _eng.bots)
    {
        if (g == captain->GetGUID())
            continue;
        Player* member = ObjectAccessor::FindConnectedPlayer(g);
        if (!member)
        {
            // LoggingIn had everyone online — someone dropped; abort. The group is already
            // registered with GroupMgr, so bare `return` would orphan it (bot logouts never
            // run the leave-group path — bot sessions have no socket). Disband() is the same
            // registered-group cleanup gatherArenaTeam's own failure path uses
            // (BattleGroundJoinAction.cpp:190): it clears the partial membership (offline-
            // member-safe) + DB rows, removes the group from GroupMgr and deletes it.
            group->Disband();
            return false;
        }
        if (member->GetGroup())
            member->GetGroup()->RemoveMember(member->GetGUID());
        if (!group->AddMember(member))
        {
            group->Disband();   // same registered-group cleanup as above
            return false;
        }
        if (PlayerbotAI* memberAI = GET_PLAYERBOT_AI(member))
            memberAI->Reset();
        member->RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_TELEPORTED | AURA_INTERRUPT_FLAG_CHANGE_MAP);
        member->TeleportTo(captain->GetMapId(), captain->GetPositionX(), captain->GetPositionY(),
                           captain->GetPositionZ(), 0);
    }
    return true;
}

// No arena battlemaster resolvable from the captain's position: send him to the next cached
// arena battlemaster spawn of his faction (else neutral). After a successful teleport
// (bmTeleported latches) the Queueing tick keeps re-resolving until the destination grid is
// loaded; a FAILED teleport leaves the latch clear and bumps bmCacheIdx so the next tick tries
// the next cache entry. An exhausted/empty cache aborts immediately — the queue step cannot
// possibly succeed, so waiting out the 120s timeout would just burn time.
void ArenaRosterDirector::TeleportCaptainToBattlemaster(Player* captain)
{
    auto cache = sRandomPlayerbotMgr.getBattleMastersCache();
    std::vector<uint32> entries = cache[captain->GetTeamId()][BATTLEGROUND_AA];
    std::vector<uint32> const& neutral = cache[TEAM_NEUTRAL][BATTLEGROUND_AA];
    entries.insert(entries.end(), neutral.begin(), neutral.end());

    if (entries.empty())
    {
        AbortEngagement("no arena battlemaster spawns cached at all — cannot queue");
        return;
    }

    while (_eng.bmCacheIdx < entries.size())
    {
        uint32 entry = entries[_eng.bmCacheIdx++];
        CreatureData const* data = sRandomPlayerbotMgr.GetCreatureDataByEntry(entry);
        if (!data)
            continue;
        captain->RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_TELEPORTED | AURA_INTERRUPT_FLAG_CHANGE_MAP);
        if (!captain->TeleportTo(data->mapid, data->posX, data->posY, data->posZ, data->orientation))
        {
            LOG_WARN("playerbots", "[ArenaRoster] teleporting <{}> to battlemaster entry {} (map {}) FAILED — trying the next cache entry.",
                     captain->GetName(), entry, data->mapid);
            return;   // bmTeleported stays false; next tick advances to the next entry
        }
        LOG_INFO("playerbots", "[ArenaRoster] no arena battlemaster on captain's map ({}) — teleported <{}> to battlemaster entry {} (map {}).",
                 captain->GetMapId(), captain->GetName(), entry, data->mapid);
        _eng.bmTeleported = true;
        return;
    }

    AbortEngagement("no cached arena battlemaster spawn could be reached — cannot queue");
}

// Post-match: if the served team's rating drifted out of its tier band, pin it back to the
// tier rating so the ladder stays stratified no matter how often one tier gets farmed.
void ArenaRosterDirector::RenormalizeServedTeam()
{
    ArenaTeam* team = sArenaTeamMgr->GetArenaTeamById(_eng.poolTeamId);
    if (!team)
    {
        LOG_WARN("playerbots", "[ArenaRoster] served team #{} vanished before renormalize.", _eng.poolTeamId);
        return;
    }

    // Tier band mirrors ArenaRosterTierFor: tier t (1-based) owns [ceilings[t-2], ceilings[t-1]),
    // open-ended below tier 1 and above tier 4 (3500 = the practical rating ceiling).
    uint8 tier = _eng.tier;
    uint32 lo = tier >= 2 ? g_ArTierCeilings[tier - 2] : 0;
    uint32 hi = tier <= 3 ? g_ArTierCeilings[tier - 1] : 3500;
    uint32 rating = team->GetRating();
    if (rating >= lo && rating < hi)
        return;

    PinTeamRating(team, g_ArTierRatings[tier - 1]);
    LOG_INFO("playerbots", "[ArenaRoster] renormalized <{}> (team #{}) from {} back to {} (tier {} band [{},{})).",
             team->GetName(), team->GetId(), rating, g_ArTierRatings[tier - 1], tier, lo, hi);
}

// Abort always drains: pull any still-queued bot out of the queue (mirror BGLeaveAction,
// BattleGroundJoinAction.cpp:677 — field layout verified against HandleBattleFieldPortOpcode),
// then hand off to Draining so LogoutPlayerBot runs after the grace. No stranded bots.
void ArenaRosterDirector::AbortEngagement(std::string why)
{
    LOG_WARN("playerbots", "[ArenaRoster] engagement ABORTED (tier {} {}v{}, team #{}): {}",
             _eng.tier, _eng.arenaType, _eng.arenaType, _eng.poolTeamId, why);

    for (ObjectGuid g : _eng.bots)
    {
        Player* bot = ObjectAccessor::FindConnectedPlayer(g);
        if (!bot || !bot->InBattlegroundQueue())
            continue;
        WorldPacket* packet = new WorldPacket(CMSG_BATTLEFIELD_PORT, 20);
        *packet << uint8(_eng.arenaType) << uint8(0) << uint32(BATTLEGROUND_AA)
                << uint16(0x1F90) << uint8(0) /*leave queue*/;
        bot->GetSession()->QueuePacket(packet);
    }

    _eng.st = Engagement::St::Draining;
    _eng.stateMs = 0;
    _eng.graceMs = 0;
}

void ArenaRosterDirector::LogoutPoolBots(std::vector<ArenaPoolRow> const& rows, uint8 tierFilter)
{
    for (ArenaPoolRow const& row : rows)
        if (!tierFilter || row.tier == tierFilter)
            // No-op for offline bots (GetPlayerBot miss inside LogoutPlayerBot).
            sRandomPlayerbotMgr.LogoutPlayerBot(ObjectGuid::Create<HighGuid::Player>(row.botGuid));
}

void ArenaRosterDirector::Fail(std::string msg)
{
    LOG_WARN("playerbots", "[ArenaRoster] poolinit FAILED (tier {}): {}", _tierInProgress, msg);
    // Never strand the in-progress tier's bots: they're masterless logins OUTSIDE random-bot
    // churn (see PhasePinBots), so nothing else would ever log them out — and a re-run's
    // PinBots skips online characters, silently shrinking the re-pinnable pool.
    LogoutPoolBots(_rows, _tierInProgress);
    _poolError = std::move(msg);
    _poolPhase = PoolPhase::Failed;
}

ArenaPoolRow& ArenaRosterDirector::TierRow(uint8 rosterIdx)
{
    // PinBots appends exactly TIER_SIZE rows per tier in roster order, so the tier in
    // progress occupies a fixed window of _rows.
    return _rows[size_t(_tierInProgress - 1) * TIER_SIZE + rosterIdx];
}

bool ArenaRosterDirector::StartPoolInit(std::string& err)
{
    if (PoolBusy())
    {
        err = "a poolinit job is already running (watch .arenaroster poolstatus)";
        return false;
    }
    if (_eng.active)
    {
        // Mutual exclusion with the queue director (its side lives in ServeOpponents): a
        // rebuild would disband/re-pin the very teams an engagement is fielding.
        err = "an arena engagement is in progress — wait for it to finish draining";
        return false;
    }

    // Idempotency probe: a complete pool (60 rows, each on at least one team — roster slot 11
    // is benched in 2v2 but every slot plays 3v3 and 5v5) means poolinit already succeeded.
    std::vector<ArenaPoolRow> existing = ArenaRosterStore::LoadPool();
    bool complete = existing.size() == POOL_SIZE;
    for (ArenaPoolRow const& row : existing)
        if (!row.team[0] && !row.team[1] && !row.team[2])
        {
            complete = false;
            break;
        }
    if (complete)
    {
        err = "already initialized (" + std::to_string(POOL_SIZE) +
              " pool bots, teams present). Truncate mod_arena_pool to force a rebuild.";
        return false;
    }

    // Partial pool = a previous run crashed/failed mid-job. Rebuild from a clean slate: log
    // out any stale-row bot still online (a crash can strand them — see Fail()), drop the
    // rows, AND disband any leftover "T<tier> <name>" teams (Task 5 finding: core Create's
    // duplicate-name check is dead code — it compares the still-empty member TeamName — so our
    // name probe is the only guard, and stale teams would otherwise be "reused" with the OLD
    // bots on them). The no-session Disband() overload is offline-member-safe: its DelMember
    // path null-checks FindConnectedPlayer.
    if (!existing.empty())
    {
        LOG_WARN("playerbots", "[ArenaRoster] poolinit: partial pool found ({} rows) — rebuilding from scratch.",
                 existing.size());
        LogoutPoolBots(existing, 0);
    }
    ArenaRosterStore::ClearPool();
    for (uint8 tier = 1; tier <= TIER_COUNT; ++tier)
    {
        for (char const* base : ARENA_POOL_2V2_NAMES)
            if (ArenaTeam* team = sArenaTeamMgr->GetArenaTeamByName(PoolTeamName(tier, base), ARENA_TEAM_2v2))
                team->Disband();
        for (char const* base : ARENA_POOL_3V3_NAMES)
            if (ArenaTeam* team = sArenaTeamMgr->GetArenaTeamByName(PoolTeamName(tier, base), ARENA_TEAM_3v3))
                team->Disband();
        for (char const* base : ARENA_POOL_5V5_NAMES)
            if (ArenaTeam* team = sArenaTeamMgr->GetArenaTeamByName(PoolTeamName(tier, base), ARENA_TEAM_5v5))
                team->Disband();
    }

    _rows.clear();
    _tierInProgress = 1;
    _poolPhaseTimerMs = 0;
    _prepIdx = 0;
    _bracketStep = 0;
    _poolError.clear();
    _poolPhase = PoolPhase::PinBots;
    LOG_INFO("playerbots", "[ArenaRoster] poolinit started: {} tiers x {} bots.", TIER_COUNT, TIER_SIZE);
    return true;
}

std::string ArenaRosterDirector::PoolStatusText() const
{
    std::string out = "poolinit: ";
    out += PhaseName(_poolPhase);
    if (_poolPhase != PoolPhase::Idle)
        out += " (tier " + std::to_string(_tierInProgress) + ")";
    out += " - pool rows " + std::to_string(ArenaRosterStore::LoadPool().size()) +
           "/" + std::to_string(POOL_SIZE);
    if (_poolPhase == PoolPhase::PrepareBots)
        out += ", geared " + std::to_string(_prepIdx) + "/" + std::to_string(TIER_SIZE) + " of this tier";
    if (!_poolError.empty())
        out += " - error: " + _poolError;
    return out;
}

void ArenaRosterDirector::TickPoolInit(uint32 elapsedMs)
{
    switch (_poolPhase)
    {
        case PoolPhase::PinBots:     PhasePinBots();               break;
        case PoolPhase::LoginWave:   PhaseLoginWave(elapsedMs);    break;
        case PoolPhase::PrepareBots: PhasePrepareBots(elapsedMs);  break;
        case PoolPhase::CreateTeams: PhaseCreateTeams();           break;
        case PoolPhase::LogoutWave:  PhaseLogoutWave();            break;
        default:                                                   break;
    }
}

void ArenaRosterDirector::PhasePinBots()
{
    // Same pinning filter as HandleCreate: offline, not pinned by us (partner pool + pool rows
    // persisted so far, incl. earlier tiers of THIS job) or by mod-raid-roster, and not in a
    // real (player) guild. Faction alternates per tier: odd = Horde, even = Alliance.
    std::unordered_set<uint32> pinned = ArenaRosterStore::AllPinnedBots();
    if (QueryResult r = CharacterDatabase.Query("SELECT bot_guid FROM mod_raid_roster"))
        do { pinned.insert(r->Fetch()[0].Get<uint32>()); } while (r->NextRow());

    bool isAlliance = (_tierInProgress % 2) == 0;
    std::vector<ArenaPoolRow> tierRows;
    tierRows.reserve(TIER_SIZE);
    for (uint8 idx = 0; idx < TIER_SIZE; ++idx)
    {
        ArenaPoolSlot const& slot = ARENA_POOL_ROSTER[idx];
        uint8 key = RandomPlayerbotMgr::GetTeamClassIdx(isAlliance, slot.cls);
        ObjectGuid chosen;
        for (ObjectGuid g : sRandomPlayerbotMgr.addclassCache[key])
        {
            if (ObjectAccessor::FindConnectedPlayer(g)) continue;
            if (pinned.count(g.GetCounter())) continue;
            ObjectGuid::LowType guildId = sCharacterCache->GetCharacterGuildIdByGuid(g);
            if (guildId && PlayerbotGuildMgr::instance().IsRealGuild(guildId)) continue;
            chosen = g;
            break;
        }
        if (!chosen)
        {
            Fail("addclass pool exhausted: no free " + std::string(isAlliance ? "Alliance" : "Horde") +
                 " character of class " + std::to_string(slot.cls) +
                 " (raise AiPlayerbot.AddClassAccountPoolSize and restart, then re-run poolinit)");
            return;
        }
        pinned.insert(chosen.GetCounter());
        ArenaPoolRow row;
        row.tier = _tierInProgress;
        row.rosterIdx = idx;
        row.botGuid = chosen.GetCounter();
        row.cls = slot.cls;
        row.specTab = slot.specTab;
        row.team[0] = row.team[1] = row.team[2] = 0;
        tierRows.push_back(row);
    }

    // Persist BEFORE the logins: the rows are what makes a crashed job re-derivable (a re-run
    // sees a partial pool and rebuilds cleanly).
    for (ArenaPoolRow const& row : tierRows)
        _rows.push_back(row);
    ArenaRosterStore::ReplacePool(_rows);

    // Masterless random-bot-managed login — the exact call mod-playerbots' gatherArenaTeam
    // uses (BattleGroundJoinAction.cpp). These bots are NOT in the random-bot rotation list
    // (currentBots holds RNDbot type-1 accounts; addclass accounts are type 2), so the churn
    // in RandomPlayerbotMgr::ProcessBot never logs them out mid-job.
    for (ArenaPoolRow const& row : tierRows)
        sRandomPlayerbotMgr.AddPlayerBot(ObjectGuid::Create<HighGuid::Player>(row.botGuid), 0);

    LOG_INFO("playerbots", "[ArenaRoster] poolinit tier {}: pinned {} {} bots, logging in.",
             _tierInProgress, TIER_SIZE, isAlliance ? "Alliance" : "Horde");
    _poolPhaseTimerMs = 0;
    _poolPhase = PoolPhase::LoginWave;
}

void ArenaRosterDirector::PhaseLoginWave(uint32 elapsedMs)
{
    _poolPhaseTimerMs += elapsedMs;

    uint32 online = 0;
    for (uint8 idx = 0; idx < TIER_SIZE; ++idx)
    {
        ObjectGuid g = ObjectGuid::Create<HighGuid::Player>(TierRow(idx).botGuid);
        Player* bot = ObjectAccessor::FindConnectedPlayer(g);
        if (bot && bot->IsInWorld())
            ++online;
        else
            // Re-submit each tick: AddPlayerBot early-outs while the login holder is still in
            // flight (botLoading), so this only re-arms logins that silently didn't stick.
            sRandomPlayerbotMgr.AddPlayerBot(g, 0);
    }

    if (online == TIER_SIZE)
    {
        _prepIdx = 0;
        _poolPhaseTimerMs = 0;
        _poolPhase = PoolPhase::PrepareBots;
        return;
    }
    if (_poolPhaseTimerMs > LOGIN_WAVE_TIMEOUT_MS)
        Fail("login wave timed out with " + std::to_string(online) + "/" +
             std::to_string(TIER_SIZE) + " bots online");
}

void ArenaRosterDirector::PhasePrepareBots(uint32 elapsedMs)
{
    _poolPhaseTimerMs += elapsedMs;
    if (_poolPhaseTimerMs > PREPARE_TIMEOUT_MS)
    {
        Fail("prepare phase timed out at bot " + std::to_string(_prepIdx) + "/" +
             std::to_string(TIER_SIZE) + " (a bot went offline?)");
        return;
    }

    uint8 batched = 0;
    while (_prepIdx < TIER_SIZE && batched < PREPARE_BATCH_PER_TICK)
    {
        ArenaPoolRow& row = TierRow(_prepIdx);
        ObjectGuid g = ObjectGuid::Create<HighGuid::Player>(row.botGuid);
        Player* bot = ObjectAccessor::FindConnectedPlayer(g);
        if (!bot || !bot->IsInWorld())
        {
            // Dropped between waves — re-login and retry next tick (bounded by the phase timer).
            sRandomPlayerbotMgr.AddPlayerBot(g, 0);
            return;
        }

        // Mirror HandleSync's proven sequence, pinned at level 80 (EquipSeason's floor):
        // 1) Randomize(false) levels to 80 and (re)learns spells/skills (its random gearing is
        //    thrown away by EquipSeason). 2) Force the slot's PvP spec. 3) Re-derive strategies.
        // 4) Season set AFTER the talent force — the gear scorer weights by active talents.
        PlayerbotFactory factory(bot, 80, ITEM_QUALITY_EPIC, 0);
        factory.Randomize(false);
        // Era-managed bots get an era build (see RaidRosterCommand SyncBotToSpec step 2).
        // Ladder bots are pinned at 80 -> WotLK band -> this is false and native runs.
        if (!EraTalentBots::FactoryReconcile(bot, row.specTab))
            PlayerbotFactory::InitTalentsBySpecNo(bot, row.specTab, true);
        if (PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot))
            botAI->ResetStrategies(false);
        if (!ArenaRosterGear::EquipSeason(bot, row.specTab, TIER_SEASON[_tierInProgress - 1]))
            LOG_WARN("playerbots", "[ArenaRoster] poolinit tier {}: {} got an incomplete {} set (continuing).",
                     _tierInProgress, bot->GetName(), ArenaRosterGear::SeasonPrefix(TIER_SEASON[_tierInProgress - 1]));

        ++_prepIdx;
        ++batched;
    }

    if (_prepIdx >= TIER_SIZE)
    {
        _bracketStep = 0;
        _teamIds = {};
        _poolPhase = PoolPhase::CreateTeams;
    }
}

bool ArenaRosterDirector::CreateOneTeam(uint8 type, uint8 bracketSlot, char const* baseName,
                                        uint8 const* indices, size_t count, std::string& err)
{
    std::string name = PoolTeamName(_tierInProgress, baseName);

    // Name probe first. A hit is defensive only — it cannot occur in the current single-pass
    // flow (each bracket runs exactly once per tier, StartPoolInit disbanded stale teams, and
    // any mid-bracket failure aborts the whole job) — but reusing instead of re-creating keeps
    // this function safely re-runnable if a future change re-ticks a bracket.
    ArenaTeam* team = sArenaTeamMgr->GetArenaTeamByName(name, type);
    if (!team)
    {
        // Captain = first index of the team; LoginWave + PrepareBots guarantee it's online,
        // which is ArenaTeam::Create's real precondition (its duplicate-name check is dead
        // code — our GetArenaTeamByName probe above IS the guard).
        ObjectGuid captain = ObjectGuid::Create<HighGuid::Player>(TierRow(indices[0]).botGuid);
        team = new ArenaTeam();
        if (!team->Create(captain, type, name, 0, 0, 0, 0, 0))
        {
            delete team;
            err = "ArenaTeam::Create failed for <" + name + "> (captain offline?)";
            return false;
        }
        sArenaTeamMgr->AddArenaTeam(team);
    }

    for (size_t i = 1; i < count; ++i)
    {
        ObjectGuid g = ObjectGuid::Create<HighGuid::Player>(TierRow(indices[i]).botGuid);
        if (!team->IsMember(g) && !team->AddMember(g))
        {
            err = "couldn't add roster slot " + std::to_string(indices[i]) + " to <" + name +
                  "> (already in another team of this bracket?)";
            return false;
        }
    }

    PinTeamRating(team, g_ArTierRatings[_tierInProgress - 1]);

    for (size_t i = 0; i < count; ++i)
        _teamIds[indices[i]][bracketSlot] = team->GetId();
    return true;
}

// Pin the rating on the team AND both per-member ratings. SetRatingForAll covers team rating +
// PersonalRating but NOT MatchMakerRating (verified in ArenaTeam.cpp:1122) — the matchmaker
// value is what the queue matches on, so set it explicitly. SaveToDB(true) persists members
// even with WeekGames=0 (PersonalRating -> arena_team_member, MMR -> character_arena_stats).
// Shared by CreateOneTeam (initial pin) and RenormalizeServedTeam (post-match re-pin).
void ArenaRosterDirector::PinTeamRating(ArenaTeam* team, uint32 rating)
{
    // Rating fields downstream are uint16 (ArenaTeamMember::PersonalRating/MatchMakerRating,
    // ArenaTeamStats::Rating) while the config value is uint32 — clamp, don't truncate.
    if (rating > 0xFFFF)
    {
        LOG_WARN("playerbots", "[ArenaRoster] rating {} exceeds uint16 — clamping to 65535 (fix ArenaRoster.TierRatings).",
                 rating);
        rating = 0xFFFF;
    }

    team->SetRatingForAll(rating);
    for (ArenaTeamMember& member : team->GetMembers())
    {
        member.MatchMakerRating = uint16(rating);
        member.MaxMMR = std::max<uint16>(member.MaxMMR, uint16(rating));
    }
    team->SaveToDB(true);
}

void ArenaRosterDirector::PhaseCreateTeams()
{
    // One bracket per tick (AddMember runs a synchronous MMR DB query per member — keep the
    // per-tick burst to one bracket's worth).
    std::string err;
    bool ok = true;
    switch (_bracketStep)
    {
        case 0:
            for (size_t i = 0; ok && i < ARENA_POOL_2V2.size(); ++i)
                ok = CreateOneTeam(ARENA_TEAM_2v2, 0, ARENA_POOL_2V2_NAMES[i],
                                   ARENA_POOL_2V2[i].data(), ARENA_POOL_2V2[i].size(), err);
            break;
        case 1:
            for (size_t i = 0; ok && i < ARENA_POOL_3V3.size(); ++i)
                ok = CreateOneTeam(ARENA_TEAM_3v3, 1, ARENA_POOL_3V3_NAMES[i],
                                   ARENA_POOL_3V3[i].data(), ARENA_POOL_3V3[i].size(), err);
            break;
        default:
            for (size_t i = 0; ok && i < ARENA_POOL_5V5.size(); ++i)
                ok = CreateOneTeam(ARENA_TEAM_5v5, 2, ARENA_POOL_5V5_NAMES[i],
                                   ARENA_POOL_5V5[i].data(), ARENA_POOL_5V5[i].size(), err);
            break;
    }
    if (!ok)
    {
        Fail(err);
        return;
    }

    if (++_bracketStep < BRACKET_COUNT)
        return;

    // All three brackets done: record each bot's team ids (store + in-memory rows).
    for (uint8 idx = 0; idx < TIER_SIZE; ++idx)
    {
        ArenaPoolRow& row = TierRow(idx);
        row.team[0] = _teamIds[idx][0];
        row.team[1] = _teamIds[idx][1];
        row.team[2] = _teamIds[idx][2];
        ArenaRosterStore::SetPoolTeams(row.tier, idx, row.team[0], row.team[1], row.team[2]);
    }
    LOG_INFO("playerbots", "[ArenaRoster] poolinit tier {}: {} teams ready at rating {}.",
             _tierInProgress, TEAMS_PER_TIER, g_ArTierRatings[_tierInProgress - 1]);
    _poolPhase = PoolPhase::LogoutWave;
}

void ArenaRosterDirector::PhaseLogoutWave()
{
    LogoutPoolBots(_rows, _tierInProgress);

    if (_tierInProgress < TIER_COUNT)
    {
        ++_tierInProgress;
        _poolPhaseTimerMs = 0;
        _poolPhase = PoolPhase::PinBots;
        return;
    }
    _poolPhase = PoolPhase::Done;
    LOG_INFO("playerbots", "[ArenaRoster] poolinit complete: {} tiers, {} bots, {} teams.",
             TIER_COUNT, POOL_SIZE, TIER_COUNT * TEAMS_PER_TIER);
}
