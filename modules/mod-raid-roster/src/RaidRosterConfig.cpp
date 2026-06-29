#include "RaidRosterConfig.h"
#include "Config.h"
#include "Log.h"

bool g_RaidRosterEnable = false;

void RaidRosterLoadConfig()
{
    g_RaidRosterEnable = sConfigMgr->GetOption<bool>("RaidRoster.Enable", false);
    LOG_INFO("server.loading", "[RaidRoster] Enable={}", g_RaidRosterEnable ? 1 : 0);
}
