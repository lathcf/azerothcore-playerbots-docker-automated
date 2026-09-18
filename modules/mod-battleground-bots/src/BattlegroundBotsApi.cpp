// modules/mod-battleground-bots/src/BattlegroundBotsApi.cpp
#include "BattlegroundBotsApi.h"
#include "BattlegroundBotsConfig.h"
#include "BattlegroundBotsDirector.h"

namespace BattlegroundBotsApi
{
    bool Enabled() { return sBattlegroundBotsConfig->enable && BattlegroundBotsDirector::instance() != nullptr; }

    bool GetAssignment(Player* bot, BgAssignment& out)
    {
        if (!Enabled()) return false;
        return BattlegroundBotsDirector::instance()->GetAssignment(bot, out);
    }
    bool GetSquadAnchor(Player* bot, ObjectGuid& anchor)
    {
        if (!Enabled()) return false;
        return BattlegroundBotsDirector::instance()->GetSquadAnchor(bot, anchor);
    }
    ObjectGuid GetTeamKillHint(Player* bot)
    {
        if (!Enabled()) return ObjectGuid::Empty;
        return BattlegroundBotsDirector::instance()->GetTeamKillHint(bot);
    }
    void ReportKillPick(Player* bot, ObjectGuid target)
    {
        if (!Enabled()) return;
        BattlegroundBotsDirector::instance()->ReportKillPick(bot, target);
    }

    // BY VALUE: a function-local static rewritten on every call would be a data race across map
    // threads. The config fields are write-once at load, so reading them unlocked is safe.
    BgKnobs Knobs()
    {
        BgKnobs k;
        k.healerPriority     = sBattlegroundBotsConfig->healerPriority;
        k.retargetCooldownMs = sBattlegroundBotsConfig->retargetCdMs;
        k.dismountRange      = sBattlegroundBotsConfig->dismountRange;
        k.healerRangeMelee   = sBattlegroundBotsConfig->healerRangeMelee;
        k.healerRangeRanged  = sBattlegroundBotsConfig->healerRangeRanged;
        k.pressureAttackers  = sBattlegroundBotsConfig->pressureAttackers;
        k.pressureHpPct      = sBattlegroundBotsConfig->pressureHpPct;
        return k;
    }

    char const* JobName(BgJob j)
    {
        switch (j)
        {
            case BgJob::GUARD: return "GUARD";       case BgJob::ASSAULT: return "ASSAULT";
            case BgJob::ESCORT_FC: return "ESCORT_FC"; case BgJob::CHASE_FC: return "CHASE_FC";
            case BgJob::REINFORCE: return "REINFORCE"; case BgJob::ROAM: return "ROAM";
            default: return "NONE";
        }
    }
    char const* StanceName(BgStance s)
    {
        switch (s) { case BgStance::HOLD_CHOKE: return "HOLD_CHOKE"; case BgStance::STAGE: return "STAGE"; default: return "ON_FLAG"; }
    }
}
