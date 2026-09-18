// modules/mod-battleground-bots/src/BgObjectiveTables.cpp
#include "BgObjectiveTables.h"
#include "Battleground.h"
#include "BattlegroundAB.h"
#include "BattlegroundAV.h"
#include "BattlegroundEY.h"
#include "BattlegroundIC.h"
#include "GameObject.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include <cmath>

namespace BgTables
{
namespace
{
    // WS: the two flag rooms; homes are the teams' spawn areas. Coordinates mirror mod-playerbots
    // BattleGroundTactics.cpp WS_FLAG_POS_* / WS_WAITING_POS_*_1.
    // Choke slots (Task 11) are CANDIDATES sourced from the fork's hand-walked mod-playerbots waypoint
    // vectors (BattleGroundTactics.cpp vPath_WSG_* / WS_FLAG_HIDE_*): the two flag-room entrances for
    // melee, the balcony + base-roof edge overlooking them for ranged, a healer tucked beside the
    // balcony ranged. `verified` stays FALSE on every one until `.bgbots probe` clears it in-world.
    BgTable const kWS = {
        BATTLEGROUND_WS,
        {
            { 0, "Silverwing flag room", KIND_FLAG_BASE, Position(1539.219f, 1481.747f, 352.458f), TEAM_ALLIANCE, {
                { Position(1517.530f, 1468.790f, 352.033f), SLOT_MELEE,  true },   // candidate vPath_WSG_AllianceTunnel_to_AllianceFlagRoom point 13 (tunnel exit), VERIFIED 2026-09-14 (.bgbots probe REACHABLE)
                { Position(1508.270f, 1493.170f, 352.005f), SLOT_MELEE,  true },   // candidate vPath_WSG_AllianceFlagRoom_to_AllianceGraveyard point 1 (graveyard-ramp top), VERIFIED 2026-09-14 (.bgbots probe REACHABLE)
                { Position(1529.249f, 1456.470f, 353.040f), SLOT_RANGED, true },   // candidate WS_FLAG_HIDE_ALLIANCE_1 (ramp-side overlook), VERIFIED 2026-09-14 (.bgbots probe REACHABLE)
                { Position(1500.630f, 1472.890f, 373.707f), SLOT_RANGED, true },   // candidate vPath_WSG_AllianceTunnel_to_AllianceBaseRoof point 28 (roof-ramp edge), VERIFIED 2026-09-14 (.bgbots probe REACHABLE)
                { Position(1533.900f, 1454.635f, 353.040f), SLOT_HEALER, true },   // candidate 5y beside WS_FLAG_HIDE_ALLIANCE_1, back from the entrance side (interpolated, no waypoint there), VERIFIED 2026-09-14 (.bgbots probe REACHABLE)
            } },
            { 1, "Warsong flag room",    KIND_FLAG_BASE, Position(915.958f, 1433.925f, 346.193f),  TEAM_HORDE,    {
                { Position(937.479f, 1451.120f, 345.553f),  SLOT_MELEE,  true },   // candidate vPath_WSG_HordeTunnel_to_HordeFlagRoom point 12 (tunnel exit), VERIFIED 2026-09-14 (.bgbots probe REACHABLE)
                { Position(944.859f, 1423.050f, 345.437f),  SLOT_MELEE,  true },   // candidate vPath_WSG_HordeFlagRoom_to_HordeGraveyard point 1 (graveyard-ramp top), VERIFIED 2026-09-14 (.bgbots probe REACHABLE)
                { Position(925.166f, 1458.084f, 355.966f),  SLOT_RANGED, true },   // candidate WS_FLAG_HIDE_HORDE_2 (balcony above the flag room), VERIFIED 2026-09-14 (.bgbots probe REACHABLE)
                { Position(952.708f, 1445.010f, 367.604f),  SLOT_RANGED, true },   // candidate vPath_WSG_HordeTunnel_to_HordeBaseRoof point 25 (roof-ramp edge), VERIFIED 2026-09-14 (.bgbots probe REACHABLE)
                { Position(920.494f, 1459.865f, 355.966f),  SLOT_HEALER, true },   // candidate 5y beside WS_FLAG_HIDE_HORDE_2, back from the entrance side (interpolated, no waypoint there), VERIFIED 2026-09-14 (.bgbots probe REACHABLE)
            } },
        },
        { Position(1510.502f, 1493.385f, 351.995f), Position(944.981f, 1423.478f, 345.434f) },
    };

    BgTable BuildAB()
    {
        BgTable t;
        t.type = BATTLEGROUND_AB;
        static char const* const kNames[BG_AB_DYNAMIC_NODES_COUNT] = { "Stables", "Blacksmith", "Farm", "Lumber Mill", "Gold Mine" };
        // Choke slots (Task 11) are CANDIDATES sourced from the fork's hand-walked mod-playerbots
        // waypoint vectors (BattleGroundTactics.cpp vPath_AB_*): the road/bridge/ramp head that an
        // attacker must come through for melee, and the ranged + healer pulled back toward the banner.
        // Index-aligned with kNames. `verified` stays FALSE on every one until `.bgbots probe` clears it.
        std::vector<ChokeSlot> const kChokes[BG_AB_DYNAMIC_NODES_COUNT] = {
            {   // Stables — road south toward the Blacksmith (the map centre)
                { Position(1169.256f, 1180.285f, -56.379f), SLOT_MELEE,  true },   // candidate vPath_AB_Stables_to_Blacksmith points 1-2 interpolated to 20y (no waypoint lands in 15-25y), VERIFIED 2026-09-14 (.bgbots probe REACHABLE)
                { Position(1171.366f, 1190.579f, -56.330f), SLOT_RANGED, true },   // candidate vPath_AB_Stables_to_Blacksmith points 0-1 interpolated to 10y, +3.5y lateral, VERIFIED 2026-09-14 (.bgbots probe REACHABLE)
                { Position(1164.409f, 1189.807f, -56.330f), SLOT_HEALER, true },   // candidate vPath_AB_Stables_to_Blacksmith points 0-1 interpolated to 10y, -3.5y lateral, VERIFIED 2026-09-14 (.bgbots probe REACHABLE)
            },
            {   // Blacksmith — the island's two bridge heads, ranged/healer mid-island covering one each
                { Position(990.890f, 1039.300f, -42.766f),  SLOT_MELEE,  true },   // candidate vPath_AB_Stables_to_Blacksmith point 20 (NE bridge head), VERIFIED 2026-09-14 (.bgbots probe REACHABLE)
                { Position(961.550f, 1030.810f, -45.814f),  SLOT_MELEE,  true },   // candidate vPath_AB_Farm_to_Blacksmith point 13 (SW bridge head), VERIFIED 2026-09-14 (.bgbots probe REACHABLE)
                { Position(984.092f, 1042.885f, -43.767f),  SLOT_RANGED, true },   // candidate banner->NE bridge head interpolated to 8y (mid-island), VERIFIED 2026-09-14 (.bgbots probe REACHABLE)
                { Position(971.421f, 1040.898f, -45.173f),  SLOT_HEALER, true },   // candidate banner->SW bridge head interpolated to 8y (mid-island), VERIFIED 2026-09-14 (.bgbots probe REACHABLE)
            },
            {   // Farm — road north-east toward the Blacksmith (the map centre)
                { Position(811.597f, 893.525f, -58.010f),   SLOT_MELEE,  true },   // candidate vPath_AB_Farm_to_Blacksmith points 1-2 interpolated to 20y (no waypoint lands in 15-25y), VERIFIED 2026-09-14 (.bgbots probe REACHABLE)
                { Position(803.858f, 884.609f, -56.810f),   SLOT_RANGED, true },   // candidate vPath_AB_Farm_to_Blacksmith points 0-1 interpolated to 10y, +3.5y lateral, VERIFIED 2026-09-14 (.bgbots probe REACHABLE)
                { Position(810.811f, 883.802f, -56.810f),   SLOT_HEALER, true },   // candidate vPath_AB_Farm_to_Blacksmith points 0-1 interpolated to 10y, -3.5y lateral, VERIFIED 2026-09-14 (.bgbots probe REACHABLE)
            },
            {   // Lumber Mill — the plateau is only reachable up a ramp; ranged/healer hold the edge
                { Position(864.740f, 1163.780f, 12.385f),   SLOT_MELEE,  true },   // candidate vPath_AB_AllianceBase_to_LumberMill point 18 (NE ramp head), VERIFIED 2026-09-14 (.bgbots probe REACHABLE)
                { Position(861.646f, 1158.426f, 11.953f),   SLOT_RANGED, true },   // candidate banner->NE ramp head interpolated to 11y (plateau edge), VERIFIED 2026-09-14 (.bgbots probe REACHABLE)
                { Position(853.480f, 1159.575f, 10.729f),   SLOT_HEALER, true },   // candidate banner->NW ramp head (vPath_AB_Stables_to_LumberMill point 19) interpolated to 11y (plateau edge), VERIFIED 2026-09-14 (.bgbots probe REACHABLE)
            },
            {   // Gold Mine — road north toward the map centre
                { Position(1148.890f, 871.391f, -111.960f), SLOT_MELEE,  true },   // candidate vPath_AB_AllianceBase_to_GoldMine point 15 (mine road entrance, 23y), VERIFIED 2026-09-14 (.bgbots probe REACHABLE)
                { Position(1143.578f, 861.219f, -111.242f), SLOT_RANGED, true },   // candidate vPath_AB_AllianceBase_to_GoldMine points 15-16 interpolated to 13y, +3.5y lateral, VERIFIED 2026-09-14 (.bgbots probe REACHABLE)
                { Position(1150.577f, 861.136f, -111.242f), SLOT_HEALER, true },   // candidate vPath_AB_AllianceBase_to_GoldMine points 15-16 interpolated to 13y, -3.5y lateral, VERIFIED 2026-09-14 (.bgbots probe REACHABLE)
            },
        };
        for (uint32 i = 0; i < BG_AB_DYNAMIC_NODES_COUNT; ++i)
            t.objectives.push_back({ i, kNames[i], KIND_NODE,
                Position(BG_AB_NodePositions[i][0], BG_AB_NodePositions[i][1], BG_AB_NodePositions[i][2]), TEAM_NEUTRAL, kChokes[i] });
        t.home[TEAM_ALLIANCE] = Position(BG_AB_SpiritGuidePos[5][0], BG_AB_SpiritGuidePos[5][1], BG_AB_SpiritGuidePos[5][2]);
        t.home[TEAM_HORDE]    = Position(BG_AB_SpiritGuidePos[6][0], BG_AB_SpiritGuidePos[6][1], BG_AB_SpiritGuidePos[6][2]);
        return t;
    }
    BgTable const kAB = BuildAB();

    // AV: the 15 nodes, indexed by BG_AV_Nodes (so ObjectiveDef::id doubles as the node index that
    // GetAVNodeInfo takes); positions mirror mod-playerbots BattleGroundTactics.h AVNodeMovementTargets.
    BgTable BuildAV()
    {
        BgTable t; t.type = BATTLEGROUND_AV;
        struct N { char const* name; ObjKind kind; float x, y, z; };
        static N const k[BG_AV_NODES_MAX] = {
            { "Stormpike Aid Station",  KIND_GRAVEYARD, 640.364f,  -36.535f,  45.625f },
            { "Stormpike Graveyard",    KIND_GRAVEYARD, 665.598f,  -292.976f, 30.291f },
            { "Stonehearth Graveyard",  KIND_GRAVEYARD, 76.108f,   -399.602f, 45.730f },
            { "Snowfall Graveyard",     KIND_GRAVEYARD, -201.298f, -119.661f, 78.291f },
            { "Iceblood Graveyard",     KIND_GRAVEYARD, -617.858f, -400.654f, 59.692f },
            { "Frostwolf Graveyard",    KIND_GRAVEYARD, -1083.803f, -341.520f, 55.304f },
            { "Frostwolf Relief Hut",   KIND_GRAVEYARD, -1405.678f, -309.108f, 89.377f },
            { "Dun Baldar South",       KIND_TOWER, 556.551f,  -77.240f,  51.931f },
            { "Dun Baldar North",       KIND_TOWER, 670.664f,  -142.031f, 63.666f },
            { "Icewing Bunker",         KIND_TOWER, 200.310f,  -361.232f, 56.387f },
            { "Stonehearth Bunker",     KIND_TOWER, -156.302f, -440.032f, 40.403f },
            { "Iceblood Tower",         KIND_TOWER, -569.702f, -265.362f, 75.009f },
            { "Tower Point",            KIND_TOWER, -767.439f, -360.200f, 90.895f },
            { "Frostwolf East Tower",   KIND_TOWER, -1303.737f, -314.070f, 113.868f },
            { "Frostwolf West Tower",   KIND_TOWER, -1300.648f, -267.356f, 114.151f },
        };
        for (uint32 i = 0; i < BG_AV_NODES_MAX; ++i)
            t.objectives.push_back({ i, k[i].name, k[i].kind, Position(k[i].x, k[i].y, k[i].z), TEAM_NEUTRAL, {} });
        t.home[TEAM_ALLIANCE] = Position(640.364f, -36.535f, 45.625f);       // fallback; Resolve() prefers GetTeamStartPosition
        t.home[TEAM_HORDE]    = Position(-1405.678f, -309.108f, 89.377f);
        return t;
    }

    // EY: four towers (core BG_EY_TriggerPositions) + the centre flag as a neutral FLAG_BASE (id 4).
    BgTable BuildEY()
    {
        BgTable t; t.type = BATTLEGROUND_EY;
        static char const* const kNames[EY_POINTS_MAX] = { "Fel Reaver Ruins", "Blood Elf Tower", "Draenei Ruins", "Mage Tower" };
        for (uint32 i = 0; i < EY_POINTS_MAX; ++i)
            t.objectives.push_back({ i, kNames[i], KIND_TOWER,
                Position(BG_EY_TriggerPositions[i][0], BG_EY_TriggerPositions[i][1], BG_EY_TriggerPositions[i][2]), TEAM_NEUTRAL, {} });
        t.objectives.push_back({ EY_POINTS_MAX, "Netherstorm flag", KIND_FLAG_BASE, Position(2174.782f, 1569.055f, 1160.362f), TEAM_NEUTRAL, {} });
        t.home[TEAM_ALLIANCE] = Position(2523.686f, 1596.597f, 1269.347f);   // fallback only
        t.home[TEAM_HORDE]    = Position(1807.735f, 1539.415f, 1267.627f);
        return t;
    }

    // IC: the five capturable nodes, indexed by ICNodePointType (so ObjectiveDef::id doubles as the
    // node type GetNodeState takes), THEN the two keep gates as KIND_GATE objectives (ids 5/6);
    // positions are resolved from the banner/portcullis GameObjects at BG start (Resolve), the
    // literals here are the core spawn table (BG_IC_ObjSpawnlocs) as a fallback.
    // DANGER: `id` doubles as an ICNodePointType only for KIND_NODE. MAX_NODE_TYPES is SEVEN (the
    // five nodes plus NODE_TYPE_GRAVEYARD_A/H), so the gate ids 5 and 6 pass an `id < MAX_NODE_TYPES`
    // test and would silently index the two GRAVEYARD node types in nodePointInitial[] and
    // GetNodeState(). Every ICNodePointType-indexed path must therefore gate on `kind == KIND_NODE`.
    BgTable BuildIC()
    {
        BgTable t; t.type = BATTLEGROUND_IC;
        struct N { char const* name; float x, y, z; };
        static N const k[NODE_TYPE_WORKSHOP + 1] = {
            { "Refinery", 1269.5f,  -400.809f, 37.6253f },   // NODE_TYPE_REFINERY
            { "Quarry",   251.016f, -1159.32f, 17.2376f },   // NODE_TYPE_QUARRY
            { "Docks",    726.385f, -360.205f, 17.8153f },   // NODE_TYPE_DOCKS
            { "Hangar",   807.78f,  -1000.07f, 132.381f },   // NODE_TYPE_HANGAR
            { "Workshop", 776.229f, -804.283f, 6.45052f },   // NODE_TYPE_WORKSHOP
        };
        for (uint32 i = 0; i <= NODE_TYPE_WORKSHOP; ++i)
            t.objectives.push_back({ i, k[i].name, KIND_NODE, Position(k[i].x, k[i].y, k[i].z), TEAM_NEUTRAL, {} });
        // The two keep gates (ids 5/6 — NOT ICNodePointType values, see the DANGER note above). A keep
        // is not a capture point, so the core spawns no guards there and nothing in the objective table
        // ever drew a defender to it: observed live at 40v40, a demolisher could sit on the Horde gate
        // unopposed. As KIND_GATE objectives each gate belongs permanently to its keep's owner
        // (ReadOwners -> homeSide), so a team guards its OWN gate and the director marks the ENEMY's
        // `dead` (never an assault target — a closed portcullis has no capture point; the stock IC
        // logic sieges it after the endgame release).
        // Fallback literals = the core spawn table BG_IC_ObjSpawnlocs (BattlegroundIC.h): the ALLIANCE
        // keep gate is BG_IC_GO_DOODAD_PORTCULLISACTIVE02 (x~273) and the Horde one is
        // BG_IC_GO_HORDE_KEEP_PORTCULLIS (x~1283) — the same faction mapping EndgameActive documents.
        t.objectives.push_back({ uint32(NODE_TYPE_WORKSHOP) + 1, "Alliance keep gate", KIND_GATE,
                                 Position(273.033f, -832.199f, 51.4109f), TEAM_ALLIANCE, {} });
        t.objectives.push_back({ uint32(NODE_TYPE_WORKSHOP) + 2, "Horde keep gate", KIND_GATE,
                                 Position(1283.05f, -765.878f, 50.8297f), TEAM_HORDE, {} });
        t.home[TEAM_ALLIANCE] = Position(299.153f, -784.589f, 48.9162f);     // alliance keep banner (fallback)
        t.home[TEAM_HORDE]    = Position(1284.76f, -705.668f, 48.9163f);     // horde keep banner (fallback)
        return t;
    }

    BgTable const kAV = BuildAV();
    BgTable const kEY = BuildEY();
    BgTable const kIC = BuildIC();
}

BattlegroundTypeId RealType(Battleground* bg)
{
    BattlegroundTypeId t = bg->GetBgTypeID();
    if (t == BATTLEGROUND_RB) t = bg->GetBgTypeID(true);
    return t;
}

BgTable const* Find(BattlegroundTypeId type)
{
    switch (type)
    {
        case BATTLEGROUND_WS: return &kWS;
        case BATTLEGROUND_AB: return &kAB;
        case BATTLEGROUND_AV: return &kAV;
        case BATTLEGROUND_EY: return &kEY;
        case BATTLEGROUND_IC: return &kIC;
        default: return nullptr;
    }
}

bool Resolve(Battleground* bg, BgTable const& source, BgTable& out)
{
    out = source;
    for (int t = 0; t < 2; ++t)
        if (Position const* start = bg->GetTeamStartPosition(static_cast<TeamId>(t)))
            out.home[t] = *start;
    if (source.type == BATTLEGROUND_IC)
    {
        // The banner GOs are spawned by BattlegroundIC::SetupBattleground (at BG creation, before
        // OnBattlegroundStart) and their `gameobject_type` slot never changes across captures — only
        // `gameobject_entry` does, via DelObject/AddObject on the same type index.
        // The keep portcullis GOs are spawned by the same SetupBattleground pass and never move.
        // GATE on `kind`, never on the id range: MAX_NODE_TYPES is 7, so a gate id (5/6) passes
        // `id < MAX_NODE_TYPES` and would resolve against nodePointInitial[NODE_TYPE_GRAVEYARD_*].
        std::vector<ObjectiveDef> kept;
        for (ObjectiveDef const& d : source.objectives)
        {
            if (d.kind == KIND_NODE && d.id < MAX_NODE_TYPES)
            {
                if (GameObject* go = bg->GetBGObject(nodePointInitial[d.id].gameobject_type))
                {
                    ObjectiveDef r = d;
                    r.pos = go->GetPosition();
                    kept.push_back(r);
                    continue;
                }
            }
            else if (d.kind == KIND_GATE)
            {
                uint32 const gate = d.homeSide == TEAM_ALLIANCE ? uint32(BG_IC_GO_DOODAD_PORTCULLISACTIVE02)
                                                                : uint32(BG_IC_GO_HORDE_KEEP_PORTCULLIS);
                if (GameObject* go = bg->GetBGObject(gate))
                {
                    ObjectiveDef r = d;
                    r.pos = go->GetPosition();
                    kept.push_back(r);
                    continue;
                }
            }
            kept.push_back(d);   // fallback literal
        }
        out.objectives = kept;
    }
    for (std::size_t i = 0; i < out.objectives.size(); ++i) out.objectives[i].id = static_cast<uint32>(i);
    return !out.objectives.empty();
}

bool ReadOwners(Battleground* bg, BgTable const& t, std::vector<TeamId>& owners, std::vector<bool>& dead)
{
    owners.assign(t.objectives.size(), TEAM_NEUTRAL);
    dead.assign(t.objectives.size(), false);
    switch (t.type)
    {
        case BATTLEGROUND_WS:
            for (std::size_t i = 0; i < t.objectives.size(); ++i) owners[i] = t.objectives[i].homeSide;
            return true;
        case BATTLEGROUND_AB:
        {
            BattlegroundAB* ab = static_cast<BattlegroundAB*>(bg);
            for (uint32 i = 0; i < BG_AB_DYNAMIC_NODES_COUNT; ++i)
            {
                // A contested node still belongs to whoever is ticking it for guard purposes.
                switch (ab->GetCapturePointInfo(i)._state)
                {
                    case BG_AB_NODE_STATE_ALLY_OCCUPIED:
                    case BG_AB_NODE_STATE_ALLY_CONTESTED:  owners[i] = TEAM_ALLIANCE; break;
                    case BG_AB_NODE_STATE_HORDE_OCCUPIED:
                    case BG_AB_NODE_STATE_HORDE_CONTESTED: owners[i] = TEAM_HORDE; break;
                    default:                               owners[i] = TEAM_NEUTRAL; break;
                }
            }
            return true;
        }
        case BATTLEGROUND_AV:
        {
            BattlegroundAV* av = static_cast<BattlegroundAV*>(bg);
            for (std::size_t i = 0; i < t.objectives.size(); ++i)
            {
                if (t.objectives[i].id >= BG_AV_NODES_MAX) continue;
                BG_AV_NodeInfo const& n = av->GetAVNodeInfo(t.objectives[i].id);
                // Core AV's AssaultNode() sets OwnerId to the ASSAULTING team, so CONTROLLED and
                // ASSAULTED both read OwnerId (verified: BattlegroundAV.cpp AssaultNode).
                owners[i] = (n.State == POINT_CONTROLLED || n.State == POINT_ASSAULTED) ? n.OwnerId : TEAM_NEUTRAL;
                // A destroyed tower keeps its OwnerId but can never be recaptured or re-burned: it is
                // out of play for both sides. The director publishes it as US to BOTH teams (so it is
                // never an assault target), gives it no choke slots, and drops it from the held margin.
                if (n.State == POINT_DESTROYED) { owners[i] = TEAM_NEUTRAL; dead[i] = true; }
            }
            return true;
        }
        case BATTLEGROUND_EY:
        {
            BattlegroundEY* ey = static_cast<BattlegroundEY*>(bg);
            for (std::size_t i = 0; i < t.objectives.size(); ++i)
                owners[i] = (t.objectives[i].kind == KIND_TOWER && t.objectives[i].id < EY_POINTS_MAX)
                          ? ey->GetCapturePointInfo(t.objectives[i].id)._ownerTeamId : TEAM_NEUTRAL;   // id 4 = the neutral flag
            return true;
        }
        case BATTLEGROUND_IC:
        {
            BattlegroundIC* ic = static_cast<BattlegroundIC*>(bg);
            for (std::size_t i = 0; i < t.objectives.size(); ++i)
            {
                // A keep gate is not a capture point: it belongs permanently to its keep's owner,
                // the same rule the WS flag rooms use.
                if (t.objectives[i].kind == KIND_GATE) { owners[i] = t.objectives[i].homeSide; continue; }
                // GATE on `kind`, never on the id range alone: MAX_NODE_TYPES is 7, so a gate id
                // (5/6) passes `< MAX_NODE_TYPES` and would read a GRAVEYARD node's state.
                if (t.objectives[i].kind != KIND_NODE || t.objectives[i].id >= MAX_NODE_TYPES) continue;
                // A contested node still belongs to whoever is ticking it (same rule as AB).
                switch (ic->GetNodeState(static_cast<uint8>(t.objectives[i].id)))
                {
                    case NODE_STATE_CONTROLLED_A: case NODE_STATE_CONFLICT_A: owners[i] = TEAM_ALLIANCE; break;
                    case NODE_STATE_CONTROLLED_H: case NODE_STATE_CONFLICT_H: owners[i] = TEAM_HORDE;    break;
                    default:                                                  owners[i] = TEAM_NEUTRAL;  break;
                }
            }
            return true;
        }
        default:
            return false;
    }
}

bool EndgameActive(Battleground* bg, TeamId team, uint32 avEndgameTowers, bool icSiegeRelease, bool avNeedsCaptain)
{
    if (!bg) return false;
    TeamId const enemy = team == TEAM_ALLIANCE ? TEAM_HORDE : TEAM_ALLIANCE;
    switch (RealType(bg))
    {
        case BATTLEGROUND_AV:
        {
            BattlegroundAV* av = static_cast<BattlegroundAV*>(bg);
            // Same captain test the stock BGTactics AV branch runs:
            // av->IsCaptainAlive(team == TEAM_HORDE ? TEAM_ALLIANCE : TEAM_HORDE). The index IS the
            // TeamId (BattlegroundAV.cpp sets m_CaptainAlive[0] on the Alliance captain's death, [1]
            // on the Horde captain's), so a plain enemy TeamId is the right argument.
            // Captain kill is optional — bots rarely go for it; the tower count alone releases by default.
            if (avNeedsCaptain && av->IsCaptainAlive(static_cast<uint8>(enemy))) return false;
            // The ENEMY's towers: the ones this team burns on the way in (and that gate the enemy
            // boss's damage buff). Our own towers falling is the opposite signal.
            constexpr BG_AV_Nodes kAllianceTowers[4] = { BG_AV_NODES_DUNBALDAR_SOUTH, BG_AV_NODES_DUNBALDAR_NORTH,
                                                            BG_AV_NODES_ICEWING_BUNKER,  BG_AV_NODES_STONEHEART_BUNKER };
            constexpr BG_AV_Nodes kHordeTowers[4]    = { BG_AV_NODES_ICEBLOOD_TOWER,   BG_AV_NODES_TOWER_POINT,
                                                            BG_AV_NODES_FROSTWOLF_ETOWER, BG_AV_NODES_FROSTWOLF_WTOWER };
            BG_AV_Nodes const* enemyTowers = enemy == TEAM_ALLIANCE ? kAllianceTowers : kHordeTowers;
            uint32 destroyed = 0;
            for (uint32 i = 0; i < 4; ++i)
                if (av->GetAVNodeInfo(enemyTowers[i]).State == POINT_DESTROYED) ++destroyed;
            return destroyed >= avEndgameTowers;
        }
        case BATTLEGROUND_IC:
        {
            // Mirrors the stock IC `gateOpen` test in BattleGroundTactics.cpp verbatim, including which
            // portcullis belongs to which faction: the HORDE branch watches BG_IC_GO_DOODAD_PORTCULLISACTIVE02
            // (the ALLIANCE keep gate, x~273) and the ALLIANCE branch watches BG_IC_GO_HORDE_KEEP_PORTCULLIS
            // (x~1283). So each side tests the ENEMY keep's gate.
            uint32 const gate = team == TEAM_ALLIANCE ? BG_IC_GO_HORDE_KEEP_PORTCULLIS : BG_IC_GO_DOODAD_PORTCULLISACTIVE02;
            GameObject* go = bg->GetBGObject(gate);
            if (go && go->isSpawned() && go->getLootState() == GO_ACTIVATED)
                return true;
            if (!icSiegeRelease)
                return false;
            // Siege capability unlocked — stock IC logic boards the gunship / takes vehicles and breaks
            // the gate; the director keeps only guards. Each of the three vehicle nodes grants it:
            // Hangar gunship, Workshop siege engines/demolishers, Docks glaive throwers/catapults.
            // The gate test alone releases too late: nobody sieges the gate while every non-guard is
            // still directed at nodes, so it never opens.
            BattlegroundIC* ic = static_cast<BattlegroundIC*>(bg);
            uint32 const ours = team == TEAM_ALLIANCE ? uint32(NODE_STATE_CONTROLLED_A) : uint32(NODE_STATE_CONTROLLED_H);
            return ic->GetNodeState(static_cast<uint8>(NODE_TYPE_HANGAR))   == ours
                || ic->GetNodeState(static_cast<uint8>(NODE_TYPE_WORKSHOP)) == ours
                || ic->GetNodeState(static_cast<uint8>(NODE_TYPE_DOCKS))    == ours;
        }
        default:
            return false;   // WS/AB/EY: capturing the objectives IS the win condition
    }
}

void ReadScore(Battleground* bg, TeamId team, int32& ours, int32& theirs)
{
    ours = theirs = 0;
    TeamId enemy = team == TEAM_ALLIANCE ? TEAM_HORDE : TEAM_ALLIANCE;
    switch (RealType(bg))
    {
        case BATTLEGROUND_WS:
        case BATTLEGROUND_AB:
        case BATTLEGROUND_EY:
            ours   = static_cast<int32>(bg->GetTeamScore(team));
            theirs = static_cast<int32>(bg->GetTeamScore(enemy));
            break;
        default:
            break;   // AV/IC: counters are private in core; held-node margin drives posture
    }
}

void ReadFlags(Battleground* bg, TeamId team, FlagLive& out)
{
    out = FlagLive();
    BattlegroundTypeId const type = RealType(bg);
    if (type == BATTLEGROUND_EY)
    {
        // EY has ONE neutral flag, so GetFlagPickerGUID ignores its team argument: the side is
        // decided by the carrier's own team.
        ObjectGuid carrier = bg->GetFlagPickerGUID();
        if (carrier.IsEmpty()) return;
        Player* p = ObjectAccessor::GetPlayer(bg->GetBgMap(), carrier);
        if (!p || !p->IsAlive()) return;
        if (p->GetBgTeamId() == team) { out.ourFc = true; out.ourFcPos = p->GetPosition(); out.ourFcGuid = carrier; }
        else                          { out.enemyFc = true; out.enemyFcPos = p->GetPosition(); }
        return;
    }
    if (type != BATTLEGROUND_WS) return;
    TeamId enemy = team == TEAM_ALLIANCE ? TEAM_HORDE : TEAM_ALLIANCE;
    // Battleground::GetFlagPickerGUID(T) is the carrier OF TEAM T'S FLAG — a player on the other
    // team (mod-playerbots PvpValues.cpp reads it the same way). So OUR carrier holds the enemy flag.
    ObjectGuid ourFc   = bg->GetFlagPickerGUID(enemy);
    ObjectGuid enemyFc = bg->GetFlagPickerGUID(team);
    if (!ourFc.IsEmpty())
        if (Player* p = ObjectAccessor::GetPlayer(bg->GetBgMap(), ourFc))
            if (p->IsAlive()) { out.ourFc = true; out.ourFcPos = p->GetPosition(); out.ourFcGuid = ourFc; }
    if (!enemyFc.IsEmpty())
        if (Player* p = ObjectAccessor::GetPlayer(bg->GetBgMap(), enemyFc))
            if (p->IsAlive()) { out.enemyFc = true; out.enemyFcPos = p->GetPosition(); }
}

Position StagingPoint(ObjectiveDef const& o, Position const& home, float dist)
{
    float dx = home.GetPositionX() - o.pos.GetPositionX();
    float dy = home.GetPositionY() - o.pos.GetPositionY();
    float len = std::sqrt(dx * dx + dy * dy);
    if (len < 1.0f) return o.pos;
    return Position(o.pos.GetPositionX() + dx / len * dist, o.pos.GetPositionY() + dy / len * dist, o.pos.GetPositionZ());
}
}
