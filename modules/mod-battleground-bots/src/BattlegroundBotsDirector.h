// modules/mod-battleground-bots/src/BattlegroundBotsDirector.h
#ifndef MOD_BATTLEGROUND_BOTS_DIRECTOR_H
#define MOD_BATTLEGROUND_BOTS_DIRECTOR_H
#include "ScriptMgr.h"
#include "ObjectGuid.h"
#include "SharedDefines.h"
#include "BattlegroundBotsApi.h"
#include "BgAssignmentEngine.h"
#include "BgObjectiveTables.h"
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

class Battleground;
class Player;

// World-thread director: per BG instance and team, reads live objective/score/flag state on a
// BGBOTS_TICK_MS cadence, runs BgEngine::Tick, and publishes per-bot assignments behind _lock for
// the fork seam (patch 0019) to read from map threads. Publishes DATA only — never moves a bot.
class BattlegroundBotsDirector : public AllBattlegroundScript
{
public:
    BattlegroundBotsDirector();
    static BattlegroundBotsDirector* instance();   // the loader's `new`, or nullptr

    void OnBattlegroundStart(Battleground* bg) override;
    void OnBattlegroundUpdate(Battleground* bg, uint32 diff) override;
    void OnBattlegroundEnd(Battleground* bg, TeamId winner) override;
    void OnBattlegroundDestroy(Battleground* bg) override;
    void OnBattlegroundRemovePlayerAtLeave(Battleground* bg, Player* player) override;

    // API backing — all take _lock, safe from map threads.
    bool GetAssignment(Player* bot, BattlegroundBotsApi::BgAssignment& out) const;
    bool GetSquadAnchor(Player* bot, ObjectGuid& anchor) const;
    ObjectGuid GetTeamKillHint(Player* bot) const;
    void ReportKillPick(Player* bot, ObjectGuid target);
    std::string StatusText(uint32 instanceId) const;   // 0 = list instances

private:
    struct InstanceState
    {
        BgTables::BgTable const* table = nullptr;   // the static table; kept for its type only
        BgTables::BgTable resolved;                 // per-instance copy: live positions + real homes
        uint32 accumMs = 0;
        uint32 elapsedMs = 0;
        BgEngine::TeamState team[2];
        std::vector<uint32> lastEnemySeenMs[2];                                   // per objective, per team view
        std::vector<BgEngine::ObjIn> lastObjs[2];                                 // last tick's inputs (status)
        std::unordered_map<uint32, BattlegroundBotsApi::BgAssignment> published;  // guid low -> assignment
        std::unordered_map<uint32, ObjectGuid> squadAnchor;                       // published squad id -> anchor
        std::unordered_map<ObjectGuid, uint32> picks[2];                          // reported kill picks this tick
        ObjectGuid killHint[2];
        bool endgame[2] = { false, false };   // AV/IC: this team's non-guards are released to stock logic
    };

    mutable std::mutex _lock;
    std::unordered_map<uint32, InstanceState> _inst;   // by BG instance id

    void TickTeam(Battleground* bg, InstanceState& st, TeamId team, uint32 nowMs);
    static BgEngine::Params ParamsFor(BattlegroundTypeId type);
    static uint32 PublishedSquadId(uint32 engineId, TeamId team) { return engineId ? engineId * 2 + static_cast<uint32>(team) : 0; }
};
#endif
