// modules/mod-battleground-bots/src/BattlegroundBotsConfig.h
#ifndef MOD_BATTLEGROUND_BOTS_CONFIG_H
#define MOD_BATTLEGROUND_BOTS_CONFIG_H
#include "Define.h"

class BattlegroundBotsConfig
{
public:
    static BattlegroundBotsConfig* instance();
    void Load();

    bool   enable        = true;
    bool   debug         = false;
    uint32 tickMs        = 2000;
    uint32 openingS      = 60;
    uint32 postureMarginWS = 1, postureMarginAB = 300, postureMarginAV = 150, postureMarginEY = 300, postureMarginIC = 100;
    uint32 guardMin      = 2;
    uint32 guardMaxPct   = 50;
    uint32 avEndgameTowers = 2;
    bool   avEndgameNeedsCaptain = false;
    bool   icSiegeRelease  = true;
    uint32 contestS      = 30;
    float  contestDist   = 300.0f;
    uint32 escortN       = 2;
    uint32 chaseN        = 3;
    uint32 reinforceMax  = 4;
    uint32 squadSize     = 4;
    uint32 squadMinPresent = 3;
    uint32 stageTimeoutS = 20;
    uint32 refocusCdS    = 45;
    uint32 roamPct       = 15;
    uint32 chokeMinGuards = 2;
    uint32 pullbackMargin = 2;
    float  dismountRange = 30.0f;
    bool   healerPriority = true;
    float  healerRangeMelee  = 12.0f;
    float  healerRangeRanged = 35.0f;
    uint32 pressureAttackers = 2;
    uint32 pressureHpPct     = 50;
    uint32 retargetCdMs  = 3000;
};

#define sBattlegroundBotsConfig BattlegroundBotsConfig::instance()
#endif
