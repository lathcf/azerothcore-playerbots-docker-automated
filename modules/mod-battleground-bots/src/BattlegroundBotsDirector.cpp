// modules/mod-battleground-bots/src/BattlegroundBotsDirector.cpp
#include "BattlegroundBotsDirector.h"
#include "BattlegroundBotsConfig.h"
#include "Battleground.h"
#include "Log.h"
#include "Map.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "Random.h"
#include <iterator>
#include <sstream>
#include <unordered_set>

namespace
{
    BattlegroundBotsDirector* g_director = nullptr;
    constexpr float kNearRadius = 40.0f;   // "near an objective" for census counts

    char const* PostureName(BgEngine::Posture p)
    {
        switch (p) { case BgEngine::Posture::OFFENSIVE: return "OFFENSIVE"; case BgEngine::Posture::DEFENSIVE: return "DEFENSIVE"; default: return "BALANCED"; }
    }
}

BattlegroundBotsDirector::BattlegroundBotsDirector() : AllBattlegroundScript("BattlegroundBotsDirector") { g_director = this; }
BattlegroundBotsDirector* BattlegroundBotsDirector::instance() { return g_director; }

BgEngine::Params BattlegroundBotsDirector::ParamsFor(BattlegroundTypeId type)
{
    BattlegroundBotsConfig const* c = sBattlegroundBotsConfig;
    BgEngine::Params p;
    p.guardMin = c->guardMin;               p.contestMs = c->contestS * 1000;          p.contestDist = c->contestDist;
    p.escortN = c->escortN;                 p.chaseN = c->chaseN;                      p.reinforceMax = c->reinforceMax;
    p.squadSize = c->squadSize;             p.squadMinPresent = c->squadMinPresent;    p.stageTimeoutMs = c->stageTimeoutS * 1000;
    p.refocusCdMs = c->refocusCdS * 1000;   p.roamPct = c->roamPct;                    p.openingMs = c->openingS * 1000;
    p.chokeMinGuards = c->chokeMinGuards;   p.pullbackMargin = c->pullbackMargin;      p.guardMaxPct = c->guardMaxPct;
    switch (type)
    {
        case BATTLEGROUND_WS: p.postureMargin = c->postureMarginWS; break;
        case BATTLEGROUND_AB: p.postureMargin = c->postureMarginAB; break;
        case BATTLEGROUND_AV: p.postureMargin = c->postureMarginAV; break;
        case BATTLEGROUND_EY: p.postureMargin = c->postureMarginEY; break;
        case BATTLEGROUND_IC: p.postureMargin = c->postureMarginIC; break;
        default: break;
    }
    return p;
}

void BattlegroundBotsDirector::OnBattlegroundStart(Battleground* bg)
{
    if (!sBattlegroundBotsConfig->enable || !bg || bg->isArena()) return;
    BgTables::BgTable const* table = BgTables::Find(BgTables::RealType(bg));
    if (!table) return;
    // Runtime-resolved copy (IC banner positions, real team start positions). Built before the map
    // entry exists so a failure leaves no half-initialised instance behind.
    BgTables::BgTable resolved;
    if (!BgTables::Resolve(bg, *table, resolved)) return;

    std::lock_guard<std::mutex> guard(_lock);
    InstanceState& st = _inst[bg->GetInstanceID()];
    st = InstanceState();
    st.table = table;
    st.resolved = std::move(resolved);
    for (int t = 0; t < 2; ++t)
    {
        // Random opening posture (stands in for mod-playerbots' one-shot random team strategy) so
        // matches open differently; ComputePosture takes over after the opening window.
        st.team[t].posture = static_cast<BgEngine::Posture>(urand(0, 2));
        st.team[t].pending = st.team[t].posture;
        st.lastEnemySeenMs[t].assign(st.resolved.objectives.size(), 0);
    }
    if (sBattlegroundBotsConfig->debug)
        LOG_INFO("module", "[BGBots] inst {} type {} start: posture A={} H={}", bg->GetInstanceID(), uint32(table->type),
                 PostureName(st.team[TEAM_ALLIANCE].posture), PostureName(st.team[TEAM_HORDE].posture));
}

void BattlegroundBotsDirector::OnBattlegroundUpdate(Battleground* bg, uint32 diff)
{
    if (!sBattlegroundBotsConfig->enable || !bg) return;
    if (bg->GetStatus() != STATUS_IN_PROGRESS) return;

    std::lock_guard<std::mutex> guard(_lock);
    auto it = _inst.find(bg->GetInstanceID());
    if (it == _inst.end()) return;
    InstanceState& st = it->second;
    st.elapsedMs += diff;
    st.accumMs += diff;
    if (st.accumMs < sBattlegroundBotsConfig->tickMs) return;
    st.accumMs = 0;
    TickTeam(bg, st, TEAM_ALLIANCE, st.elapsedMs);
    TickTeam(bg, st, TEAM_HORDE, st.elapsedMs);
}

void BattlegroundBotsDirector::OnBattlegroundEnd(Battleground* bg, TeamId /*winner*/)
{
    if (!bg) return;
    std::lock_guard<std::mutex> guard(_lock);
    _inst.erase(bg->GetInstanceID());
}

void BattlegroundBotsDirector::OnBattlegroundDestroy(Battleground* bg)
{
    if (!bg) return;
    std::lock_guard<std::mutex> guard(_lock);
    _inst.erase(bg->GetInstanceID());
}

void BattlegroundBotsDirector::OnBattlegroundRemovePlayerAtLeave(Battleground* bg, Player* player)
{
    if (!sBattlegroundBotsConfig->enable || !bg || !player) return;
    std::lock_guard<std::mutex> guard(_lock);
    auto it = _inst.find(bg->GetInstanceID());
    if (it == _inst.end()) return;
    uint32 key = player->GetGUID().GetCounter();
    it->second.published.erase(key);
    for (int t = 0; t < 2; ++t) it->second.team[t].assign.erase(key);
}

void BattlegroundBotsDirector::TickTeam(Battleground* bg, InstanceState& st, TeamId team, uint32 nowMs)
{
    BgTables::BgTable const& table = st.resolved;   // per-instance copy (see OnBattlegroundStart)
    TeamId const enemy = team == TEAM_ALLIANCE ? TEAM_HORDE : TEAM_ALLIANCE;
    BgEngine::Params const p = ParamsFor(table.type);
    BgEngine::TeamState& ts = st.team[team];

    // Flag state first: the EY centre flag's objective row depends on whether it is being carried.
    BgTables::FlagLive flags;
    BgTables::ReadFlags(bg, team, flags);

    // Objectives (owner relative to this team; staging aimed at this team's home).
    std::vector<TeamId> owners;
    std::vector<bool> dead;
    if (!BgTables::ReadOwners(bg, table, owners, dead)) return;
    std::vector<BgEngine::ObjIn> objs(table.objectives.size());
    for (std::size_t i = 0; i < objs.size(); ++i)
    {
        BgTables::ObjectiveDef const& d = table.objectives[i];
        BgEngine::ObjIn& o = objs[i];
        o.id = d.id; o.x = d.pos.GetPositionX(); o.y = d.pos.GetPositionY();
        // A dead objective (destroyed AV tower, enemy IC keep gate) is flagged for the engine (no
        // guards, no assault, no reinforcement, never a focus), reads as OURS to this team, and gets
        // no choke slots; it is also left out of the held margin below.
        o.dead = dead[i];
        // Same treatment for the EY centre flag (the only KIND_FLAG_BASE with no home side) while a
        // carrier holds it: the flag is on a player, not on its base, so it is no longer a place to
        // send a squad — the ESCORT_FC/CHASE_FC jobs own it until it is dropped or capped.
        if (d.kind == BgTables::KIND_FLAG_BASE && d.homeSide == TEAM_NEUTRAL && (flags.ourFc || flags.enemyFc))
            o.dead = true;
        // IC keep gates. The ENEMY's gate is dead: a closed portcullis is not a capture point and a
        // squad must never charge it — breaking it is the stock IC logic's job, after the endgame
        // release (EndgameActive). OUR gate is an ordinary US objective, so it draws the guard quota
        // and the REINFORCE job when enemyNear > friendlyNear. That is the whole point of the entry:
        // a keep is not a node, so the core spawns no guards there and nothing ever defended it —
        // an enemy parked at the gate (in a siege vehicle or not) still has a position and is
        // counted by the census below, so the reinforcement fires.
        if (d.kind == BgTables::KIND_GATE && owners[i] == enemy)
            o.dead = true;
        o.owner = o.dead ? BgEngine::Owner::US
                : owners[i] == team ? BgEngine::Owner::US
                : owners[i] == enemy ? BgEngine::Owner::ENEMY : BgEngine::Owner::NEUTRAL;
        Position stage = BgTables::StagingPoint(d, table.home[team]);
        o.stageX = stage.GetPositionX(); o.stageY = stage.GetPositionY();
        if (!o.dead)
            for (auto const& s : d.chokes) if (s.verified) ++o.slots[s.kind];   // only verified slots are seatable
    }

    // Census. Enemy players only feed the per-objective enemy count; our side becomes BotIn rows.
    std::vector<BgEngine::BotIn> bots;
    std::vector<Player*> players;   // index-aligned with bots
    for (auto const& [guid, player] : bg->GetPlayers())
    {
        if (!player) continue;
        bool alive = player->IsAlive();
        if (player->GetBgTeamId() != team)
        {
            if (alive)
                for (std::size_t i = 0; i < objs.size(); ++i)
                    if (player->GetExactDist2d(table.objectives[i].pos) <= kNearRadius) { ++objs[i].enemyNear; st.lastEnemySeenMs[team][i] = nowMs; }
            continue;
        }
        BgEngine::BotIn b;
        b.key = guid.GetCounter();
        b.alive = alive;
        b.inCombat = player->IsInCombat();
        b.human = IsRealPlayer(player) || IsSelfBot(player);   // mod-playerbots free functions (CLAUDE.md: bot-vs-human test)
        b.isFc = flags.ourFc && flags.ourFcGuid == guid;
        b.role = PlayerbotAI::IsHeal(player, true) ? BgEngine::Role::HEAL
               : PlayerbotAI::IsTank(player, true) ? BgEngine::Role::TANK
               : PlayerbotAI::IsRangedDps(player, true) ? BgEngine::Role::RANGED : BgEngine::Role::MELEE;
        b.x = player->GetPositionX(); b.y = player->GetPositionY();
        if (alive)
            for (std::size_t i = 0; i < objs.size(); ++i)
                if (player->GetExactDist2d(table.objectives[i].pos) <= kNearRadius) ++objs[i].friendlyNear;
        bots.push_back(b);
        players.push_back(player);
    }
    for (std::size_t i = 0; i < objs.size(); ++i) objs[i].lastEnemySeenMs = st.lastEnemySeenMs[team][i];

    // A bot that left the BG while OFFLINE never reaches OnBattlegroundRemovePlayerAtLeave with a
    // Player*, so drop published entries that are in neither this census nor the other team's assigns.
    std::unordered_set<uint32> censusKeys;
    for (BgEngine::BotIn const& b : bots) censusKeys.insert(b.key);
    for (auto pit = st.published.begin(); pit != st.published.end();)
        pit = (!censusKeys.count(pit->first) && !st.team[enemy].assign.count(pit->first)) ? st.published.erase(pit) : std::next(pit);

    // Posture from live score + held margin.
    int32 ours = 0, theirs = 0;
    BgTables::ReadScore(bg, team, ours, theirs);
    int32 held = 0, enemyHeld = 0;
    for (std::size_t i = 0; i < objs.size(); ++i)
    {
        if (objs[i].dead) continue;                // out of play: counts for neither side
        if (objs[i].owner == BgEngine::Owner::US) ++held;
        else if (objs[i].owner == BgEngine::Owner::ENEMY) ++enemyHeld;
    }
    BgEngine::ComputePosture(ts, ours - theirs, held - enemyHeld, p, nowMs);

    BgEngine::FlagIn f;
    f.ourFcAlive = flags.ourFc;     f.ourFcX = flags.ourFcPos.GetPositionX();     f.ourFcY = flags.ourFcPos.GetPositionY();
    f.enemyFcAlive = flags.enemyFc; f.enemyFcX = flags.enemyFcPos.GetPositionX(); f.enemyFcY = flags.enemyFcPos.GetPositionY();

    BgEngine::Tick(ts, bots, objs, f, p, nowMs);
    st.lastObjs[team] = objs;

    // Endgame release (AV/IC only): once the BG's real win condition is reachable — AV enemy captain
    // dead + enough enemy towers burned, IC enemy keep gate open or Hangar/Workshop/Docks held —
    // everything except the objectives we are holding is published NO LONGER, so
    // BattlegroundBotsApi::GetAssignment() returns false for those bots and the stock mod-playerbots
    // branch (AV captain/mine/Drek'Thar-Vanndar, IC keep boss + gunship/siege vehicles) runs them.
    // The ENGINE state is deliberately untouched: guards stay sticky and squads keep their
    // stage/push latch, so if this ever reads false again the next tick republishes normally.
    bool const endgame = BgTables::EndgameActive(bg, team, sBattlegroundBotsConfig->avEndgameTowers,
                                                 sBattlegroundBotsConfig->icSiegeRelease,
                                                 sBattlegroundBotsConfig->avEndgameNeedsCaptain);
    st.endgame[team] = endgame;

    // Publish: 3D targets (deterministic per-bot jitter so guards don't stack; z from the map),
    // squad anchors (first living melee/tank per squad), kill hint (most-reported pick).
    Map* map = bg->GetBgMap();
    std::unordered_map<uint32, ObjectGuid> anchors;
    for (std::size_t i = 0; i < bots.size(); ++i)
    {
        auto a = ts.assign.find(bots[i].key);
        if (a == ts.assign.end() || a->second.squadId == 0 || !bots[i].alive) continue;
        if (bots[i].role == BgEngine::Role::MELEE || bots[i].role == BgEngine::Role::TANK)
            anchors.emplace(PublishedSquadId(a->second.squadId, team), players[i]->GetGUID());
    }
    for (auto const& [sid, guid] : anchors) st.squadAnchor[sid] = guid;
    for (auto it = st.squadAnchor.begin(); it != st.squadAnchor.end();)
        it = ((it->first % 2) == static_cast<uint32>(team) && !anchors.count(it->first)) ? st.squadAnchor.erase(it) : std::next(it);

    uint32 counts[7] = {0, 0, 0, 0, 0, 0, 0};
    for (BgEngine::BotIn const& b : bots)
    {
        auto a = ts.assign.find(b.key);
        if (a == ts.assign.end()) { st.published.erase(b.key); continue; }
        BgEngine::Assignment const& e = a->second;
        // Released: only GUARD (hold what we took) and REINFORCE (an objective under attack) stay
        // directed. Erase — never leave a stale entry — so GetAssignment() reports "no assignment".
        if (endgame && e.job != BgEngine::Job::GUARD && e.job != BgEngine::Job::REINFORCE)
        {
            st.published.erase(b.key);
            ++counts[static_cast<uint8>(e.job)];
            continue;
        }
        BattlegroundBotsApi::BgAssignment out;
        out.job = static_cast<BattlegroundBotsApi::BgJob>(e.job);
        out.stance = static_cast<BattlegroundBotsApi::BgStance>(e.stance);
        out.objectiveId = e.objectiveId;
        out.squadId = PublishedSquadId(e.squadId, team);
        out.epoch = e.epoch;
        float x = e.x;
        float y = e.y;
        float zHint = 0.0f;
        if (e.job == BgEngine::Job::ESCORT_FC)      zHint = flags.ourFcPos.GetPositionZ();
        else if (e.job == BgEngine::Job::CHASE_FC)  zHint = flags.enemyFcPos.GetPositionZ();
        else if (e.objectiveId < table.objectives.size()) zHint = table.objectives[e.objectiveId].pos.GetPositionZ();
        // HOLD_CHOKE: seat exactly on the slotIndex-th VERIFIED slot of this bot's slotKind. If the
        // table no longer has it, fall back to the objective position + jitter (ON_FLAG semantics).
        BgTables::ChokeSlot const* slot = nullptr;
        if (e.stance == BgEngine::Stance::HOLD_CHOKE && e.objectiveId < table.objectives.size())
        {
            uint32 seen = 0;
            for (BgTables::ChokeSlot const& s : table.objectives[e.objectiveId].chokes)
            {
                if (!s.verified || static_cast<uint8>(s.kind) != e.slotKind) continue;
                if (seen++ == e.slotIndex) { slot = &s; break; }
            }
        }
        if (slot) { x = slot->pos.GetPositionX(); y = slot->pos.GetPositionY(); zHint = slot->pos.GetPositionZ(); }
        else
        {
            x += (static_cast<int32>(b.key % 13) - 6) * 0.8f;
            y += (static_cast<int32>((b.key / 13) % 13) - 6) * 0.8f;
        }
        float z = map ? map->GetHeight(PHASEMASK_NORMAL, x, y, zHint + 5.0f, true, 50.0f) : zHint;
        if (z <= INVALID_HEIGHT + 1.0f) z = zHint;
        out.target.Relocate(x, y, z);
        st.published[b.key] = out;
        ++counts[static_cast<uint8>(e.job)];
    }

    ObjectGuid best; uint32 bestN = 0;
    for (auto const& [guid, n] : st.picks[team]) if (n > bestN) { bestN = n; best = guid; }
    st.killHint[team] = best;
    st.picks[team].clear();

    if (sBattlegroundBotsConfig->debug)
    {
        uint32 verifiedSlots = 0, chokeHolders = 0;
        for (auto const& o : objs) verifiedSlots += o.slots[0] + o.slots[1] + o.slots[2];
        for (auto const& [k, a] : ts.assign) if (a.stance == BgEngine::Stance::HOLD_CHOKE) ++chokeHolders;
        LOG_INFO("module", "[BGBots] inst {} {} t={}s posture={} endgame={} held={}/{} score={}/{} bots={} guard={} assault={} escort={} chase={} reinforce={} roam={} squads={} slots={}/{}",
                 bg->GetInstanceID(), team == TEAM_ALLIANCE ? "A" : "H", nowMs / 1000, PostureName(ts.posture), endgame ? 1 : 0, held, enemyHeld, ours, theirs,
                 bots.size(), counts[1], counts[2], counts[3], counts[4], counts[5], counts[6], ts.squads.size(), chokeHolders, verifiedSlots);
    }
}

bool BattlegroundBotsDirector::GetAssignment(Player* bot, BattlegroundBotsApi::BgAssignment& out) const
{
    if (!bot) return false;
    std::lock_guard<std::mutex> guard(_lock);
    auto it = _inst.find(bot->GetBattlegroundId());
    if (it == _inst.end()) return false;
    auto a = it->second.published.find(bot->GetGUID().GetCounter());
    if (a == it->second.published.end()) return false;
    if (a->second.job == BattlegroundBotsApi::BgJob::NONE || a->second.job == BattlegroundBotsApi::BgJob::ROAM) return false;
    out = a->second;
    return true;
}

bool BattlegroundBotsDirector::GetSquadAnchor(Player* bot, ObjectGuid& anchor) const
{
    if (!bot) return false;
    std::lock_guard<std::mutex> guard(_lock);
    auto it = _inst.find(bot->GetBattlegroundId());
    if (it == _inst.end()) return false;
    auto a = it->second.published.find(bot->GetGUID().GetCounter());
    if (a == it->second.published.end() || a->second.squadId == 0) return false;
    auto s = it->second.squadAnchor.find(a->second.squadId);
    if (s == it->second.squadAnchor.end() || s->second == bot->GetGUID()) return false;
    anchor = s->second;
    return true;
}

ObjectGuid BattlegroundBotsDirector::GetTeamKillHint(Player* bot) const
{
    if (!bot) return ObjectGuid::Empty;
    std::lock_guard<std::mutex> guard(_lock);
    auto it = _inst.find(bot->GetBattlegroundId());
    if (it == _inst.end()) return ObjectGuid::Empty;
    return it->second.killHint[bot->GetBgTeamId()];
}

void BattlegroundBotsDirector::ReportKillPick(Player* bot, ObjectGuid target)
{
    if (!bot || target.IsEmpty()) return;
    std::lock_guard<std::mutex> guard(_lock);
    auto it = _inst.find(bot->GetBattlegroundId());
    if (it == _inst.end()) return;
    ++it->second.picks[bot->GetBgTeamId()][target];
}

std::string BattlegroundBotsDirector::StatusText(uint32 instanceId) const
{
    std::lock_guard<std::mutex> guard(_lock);
    std::ostringstream ss;
    if (instanceId == 0)
    {
        ss << "[BGBots] " << _inst.size() << " directed instance(s):";
        for (auto const& [id, st] : _inst) ss << " " << id << "(type " << uint32(st.table->type) << ", " << st.elapsedMs / 1000 << "s)";
        return ss.str();
    }
    auto it = _inst.find(instanceId);
    if (it == _inst.end()) return "[BGBots] no such directed instance";
    InstanceState const& st = it->second;
    for (int t = 0; t < 2; ++t)
    {
        BgEngine::TeamState const& ts = st.team[t];
        uint32 counts[7] = {0, 0, 0, 0, 0, 0, 0};
        for (auto const& [k, a] : ts.assign) ++counts[static_cast<uint8>(a.job)];
        ss << (t == TEAM_ALLIANCE ? "\nALLIANCE" : "\nHORDE") << " posture=" << PostureName(ts.posture)
           << " endgame=" << (st.endgame[t] ? 1 : 0) << " bots=" << ts.assign.size() << " guard=" << counts[1] << " assault=" << counts[2] << " escort=" << counts[3]
           << " chase=" << counts[4] << " reinforce=" << counts[5] << " roam=" << counts[6] << " squads=" << ts.squads.size();
        for (auto const& [sid, s] : ts.squads) ss << " [sq" << PublishedSquadId(sid, static_cast<TeamId>(t)) << "->" << s.objectiveId << (s.pushing ? " push]" : " stage]");
        for (auto const& o : st.lastObjs[t])
            ss << "\n  obj " << o.id << " " << st.resolved.objectives[o.id].name << " owner=" << (o.owner == BgEngine::Owner::US ? "US" : o.owner == BgEngine::Owner::ENEMY ? "ENEMY" : "-")
               << " near F/E=" << o.friendlyNear << "/" << o.enemyNear;
    }
    return ss.str();
}
