// modules/mod-battleground-bots/src/BattlegroundBotsApi.h
// The ONLY header fork code (patch 0019) includes. Everything here is fail-open: a false/empty
// result means "run stock mod-playerbots behaviour". Safe to call from map threads.
#ifndef MOD_BATTLEGROUND_BOTS_API_H
#define MOD_BATTLEGROUND_BOTS_API_H
#include "Define.h"
#include "ObjectGuid.h"
#include "Position.h"

class Player;

namespace BattlegroundBotsApi
{
    enum class BgJob : uint8 { NONE = 0, GUARD, ASSAULT, ESCORT_FC, CHASE_FC, REINFORCE, ROAM };
    enum class BgStance : uint8 { ON_FLAG = 0, HOLD_CHOKE, STAGE };

    struct BgAssignment
    {
        uint32   objectiveId = 0;
        BgJob    job         = BgJob::NONE;
        BgStance stance      = BgStance::ON_FLAG;
        Position target;                 // where to stand / go (already jittered per bot)
        uint32   squadId     = 0;        // 0 = none
        uint32   epoch       = 0;        // bumps when this bot's assignment changes (debug/status)
    };

    struct BgKnobs
    {
        bool   healerPriority   = true;
        uint32 retargetCooldownMs = 3000;
        float  dismountRange    = 30.0f;
        // Kill-target realism: how far a healer is worth walking for (by the bot's own role), and
        // what counts as "I am under pressure and must fight back instead of chasing a healer".
        float  healerRangeMelee  = 12.0f;
        float  healerRangeRanged = 35.0f;
        uint32 pressureAttackers = 2;
        uint32 pressureHpPct     = 50;
    };

    bool Enabled();
    // false => no assignment (module off, ROAM share, unknown BG, bot not in census, bot is the FC).
    bool GetAssignment(Player* bot, BgAssignment& out);
    // The living melee/tank the bot's squad rallies on; false when the bot has no squad or no anchor.
    bool GetSquadAnchor(Player* bot, ObjectGuid& anchor);
    // Team-wide focus hint (the enemy most bots reported this tick); Empty when none.
    ObjectGuid GetTeamKillHint(Player* bot);
    void ReportKillPick(Player* bot, ObjectGuid target);
    // BY VALUE on purpose: called from map threads, so there must be no shared mutable storage.
    BgKnobs Knobs();

    char const* JobName(BgJob j);
    char const* StanceName(BgStance s);
}
#endif
