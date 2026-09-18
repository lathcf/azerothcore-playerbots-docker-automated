// modules/mod-battleground-bots/src/BattlegroundBotsConfig.cpp
#include "BattlegroundBotsConfig.h"
#include "Config.h"
#include "Log.h"

BattlegroundBotsConfig* BattlegroundBotsConfig::instance()
{
    static BattlegroundBotsConfig instance;
    return &instance;
}

void BattlegroundBotsConfig::Load()
{
    enable   = sConfigMgr->GetOption<bool>("BattlegroundBots.Enable", true);
    debug    = sConfigMgr->GetOption<bool>("BattlegroundBots.Debug", false);
    tickMs   = sConfigMgr->GetOption<uint32>("BattlegroundBots.TickMs", 2000);
    openingS = sConfigMgr->GetOption<uint32>("BattlegroundBots.OpeningSeconds", 60);
    postureMarginWS = sConfigMgr->GetOption<uint32>("BattlegroundBots.PostureMargin.WS", 1);
    postureMarginAB = sConfigMgr->GetOption<uint32>("BattlegroundBots.PostureMargin.AB", 300);
    postureMarginAV = sConfigMgr->GetOption<uint32>("BattlegroundBots.PostureMargin.AV", 150);
    postureMarginEY = sConfigMgr->GetOption<uint32>("BattlegroundBots.PostureMargin.EY", 300);
    postureMarginIC = sConfigMgr->GetOption<uint32>("BattlegroundBots.PostureMargin.IC", 100);
    guardMin     = sConfigMgr->GetOption<uint32>("BattlegroundBots.GuardMin", 2);
    guardMaxPct  = sConfigMgr->GetOption<uint32>("BattlegroundBots.GuardMaxPercent", 50);
    avEndgameTowers = sConfigMgr->GetOption<uint32>("BattlegroundBots.AV.EndgameTowers", 2);
    avEndgameNeedsCaptain = sConfigMgr->GetOption<bool>("BattlegroundBots.AV.EndgameNeedsCaptain", false);
    icSiegeRelease = sConfigMgr->GetOption<bool>("BattlegroundBots.IC.SiegeRelease", true);
    contestS     = sConfigMgr->GetOption<uint32>("BattlegroundBots.ContestSeconds", 30);
    contestDist  = sConfigMgr->GetOption<float>("BattlegroundBots.ContestDistance", 300.0f);
    escortN      = sConfigMgr->GetOption<uint32>("BattlegroundBots.EscortCount", 2);
    chaseN       = sConfigMgr->GetOption<uint32>("BattlegroundBots.ChaseCount", 3);
    reinforceMax = sConfigMgr->GetOption<uint32>("BattlegroundBots.ReinforceMax", 4);
    squadSize    = sConfigMgr->GetOption<uint32>("BattlegroundBots.SquadSize", 4);
    squadMinPresent = sConfigMgr->GetOption<uint32>("BattlegroundBots.SquadMinPresent", 3);
    stageTimeoutS = sConfigMgr->GetOption<uint32>("BattlegroundBots.StageTimeoutSeconds", 20);
    refocusCdS   = sConfigMgr->GetOption<uint32>("BattlegroundBots.RefocusCooldownSeconds", 45);
    roamPct      = sConfigMgr->GetOption<uint32>("BattlegroundBots.RoamPercent", 15);
    chokeMinGuards = sConfigMgr->GetOption<uint32>("BattlegroundBots.ChokeMinGuards", 2);
    pullbackMargin = sConfigMgr->GetOption<uint32>("BattlegroundBots.PullbackMargin", 2);
    dismountRange = sConfigMgr->GetOption<float>("BattlegroundBots.DismountRange", 30.0f);
    healerPriority = sConfigMgr->GetOption<bool>("BattlegroundBots.HealerPriority", true);
    healerRangeMelee = sConfigMgr->GetOption<float>("BattlegroundBots.HealerRangeMelee", 12.0f);
    healerRangeRanged = sConfigMgr->GetOption<float>("BattlegroundBots.HealerRangeRanged", 35.0f);
    pressureAttackers = sConfigMgr->GetOption<uint32>("BattlegroundBots.PressureAttackers", 2);
    pressureHpPct = sConfigMgr->GetOption<uint32>("BattlegroundBots.PressureHpPct", 50);
    retargetCdMs = sConfigMgr->GetOption<uint32>("BattlegroundBots.RetargetCooldownMs", 3000);

    LOG_INFO("server.loading",
        "[BattlegroundBots] Enable={} Debug={} TickMs={} OpeningS={} GuardMin={} GuardMaxPct={} ContestS={} ContestDist={} "
        "AVEndgameTowers={} AVEndgameNeedsCaptain={} ICSiegeRelease={} Escort={} Chase={} ReinforceMax={} Squad={}/{} StageTimeoutS={} RefocusCdS={} RoamPct={} "
        "ChokeMinGuards={} PullbackMargin={} DismountRange={} HealerPriority={} HealerRangeMelee={} HealerRangeRanged={} "
        "PressureAttackers={} PressureHpPct={} RetargetCdMs={}",
        enable ? 1 : 0, debug ? 1 : 0, tickMs, openingS, guardMin, guardMaxPct, contestS, contestDist,
        avEndgameTowers, avEndgameNeedsCaptain ? 1 : 0, icSiegeRelease ? 1 : 0, escortN, chaseN, reinforceMax, squadSize, squadMinPresent, stageTimeoutS, refocusCdS, roamPct,
        chokeMinGuards, pullbackMargin, dismountRange, healerPriority ? 1 : 0, healerRangeMelee, healerRangeRanged,
        pressureAttackers, pressureHpPct, retargetCdMs);
}
