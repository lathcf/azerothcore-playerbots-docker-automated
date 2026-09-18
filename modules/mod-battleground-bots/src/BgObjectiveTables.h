// modules/mod-battleground-bots/src/BgObjectiveTables.h
// Static per-BG objective tables + the adapters that read live ownership/score/flag state.
// INVARIANT: ObjectiveDef::id == its index in BgTable::objectives (the director relies on it).
#ifndef MOD_BATTLEGROUND_BOTS_OBJECTIVE_TABLES_H
#define MOD_BATTLEGROUND_BOTS_OBJECTIVE_TABLES_H
#include "Define.h"
#include "ObjectGuid.h"
#include "Position.h"
#include "SharedDefines.h"   // TeamId, BattlegroundTypeId
#include <vector>

class Battleground;

namespace BgTables
{
    enum ObjKind : uint8 { KIND_NODE = 0, KIND_FLAG_BASE, KIND_TOWER, KIND_GRAVEYARD, KIND_GATE };
    enum SlotKind : uint8 { SLOT_MELEE = 0, SLOT_RANGED, SLOT_HEALER };

    struct ChokeSlot { Position pos; SlotKind kind; bool verified; };   // Phase 3 fills these

    struct ObjectiveDef
    {
        uint32      id;
        char const* name;
        ObjKind     kind;
        Position    pos;
        TeamId      homeSide;    // TEAM_NEUTRAL for contestable nodes
        std::vector<ChokeSlot> chokes;
    };

    struct BgTable
    {
        BattlegroundTypeId type;
        std::vector<ObjectiveDef> objectives;
        Position home[2];        // indexed by TeamId: each team's spawn/base, used to aim staging points
    };

    BgTable const* Find(BattlegroundTypeId type);            // nullptr = BG not supported
    BattlegroundTypeId RealType(Battleground* bg);           // resolves BATTLEGROUND_RB to the real type

    // Per-instance copy of a table with runtime-resolved positions (IC banners) and homes
    // (Battleground::GetTeamStartPosition). Objectives are NEVER dropped: an IC banner whose GO cannot
    // be resolved falls back to its literal position. It has to be that way, because an objective id
    // doubles as a core node index (GetAVNodeInfo(id), GetNodeState(id), nodePointInitial[id]) — a
    // compaction would silently shift every later objective onto the wrong core node. The trailing
    // re-numbering loop is therefore a GUARANTEE of `id == index`, not a compaction.
    bool Resolve(Battleground* bg, BgTable const& source, BgTable& out);

    // Absolute owner of each objective, index-aligned with table.objectives. false = adapter missing.
    // `dead` marks objectives that are permanently out of play (AV POINT_DESTROYED towers): nothing to
    // capture and nothing to defend. Only AV ever sets it HERE; every other BG fills it with false.
    // (The director adds two per-team cases of its own on top: the carried EY centre flag, and the
    // ENEMY side's IC keep gate — both are relative to the asking team, so they cannot live here.)
    bool ReadOwners(Battleground* bg, BgTable const& t, std::vector<TeamId>& owners, std::vector<bool>& dead);
    // True once THIS team should stop being directed toward objectives and fall back to the stock
    // mod-playerbots win-condition logic (AV: enemy captain + boss; IC: enemy keep boss, gunship and
    // siege vehicles). AV needs (optionally) the enemy captain dead AND `avEndgameTowers` of the enemy's four towers
    // POINT_DESTROYED; IC needs the enemy keep portcullis open OR — when `icSiegeRelease` is set — this
    // team controlling any vehicle node (Hangar gunship, Workshop siege engines/demolishers, Docks
    // glaive throwers/catapults). Every other BG is always false (their win condition IS the objectives).
    bool EndgameActive(Battleground* bg, TeamId team, uint32 avEndgameTowers, bool icSiegeRelease, bool avNeedsCaptain);

    // WS captures / AB-EY resources via Battleground::GetTeamScore. AV/IC return 0/0 (posture then
    // rides on the held-objective margin alone — their reinforcement counters are private in core).
    void ReadScore(Battleground* bg, TeamId team, int32& ours, int32& theirs);

    struct FlagLive { bool ourFc = false; Position ourFcPos; ObjectGuid ourFcGuid; bool enemyFc = false; Position enemyFcPos; };
    void ReadFlags(Battleground* bg, TeamId team, FlagLive& out);   // WS (two flags) and EY (one)

    // `dist` yards from the objective toward the team's home (2D; z copied from the objective).
    Position StagingPoint(ObjectiveDef const& o, Position const& home, float dist = 60.0f);
}
#endif
