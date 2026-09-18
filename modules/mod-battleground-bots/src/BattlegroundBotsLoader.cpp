// modules/mod-battleground-bots/src/BattlegroundBotsLoader.cpp
#include "Log.h"
#include "BattlegroundBotsConfig.h"
#include "BattlegroundBotsDirector.h"

void AddBattlegroundBotsCommandScripts();

void Addmod_battleground_botsScripts()
{
    sBattlegroundBotsConfig->Load();
    LOG_INFO("server.loading", "[BattlegroundBots] Registering scripts (director v1).");   // canary token: "director v1"
    new BattlegroundBotsDirector();      // AllBattlegroundScript self-registers with ScriptMgr
    AddBattlegroundBotsCommandScripts();
}
