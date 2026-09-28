#include "EnchanterConfig.h"
#include "Config.h"
#include "Log.h"
#include <algorithm>

bool   g_EnchanterEnable          = true;
uint32 g_EnchanterMatsPct         = 100;
uint32 g_EnchanterLaborAtMax      = 50000;
uint32 g_EnchanterPriceRefreshSec = 300;
bool   g_EnchanterDebug           = false;

void EnchanterLoadConfig()
{
    g_EnchanterEnable          = sConfigMgr->GetOption<bool>("EnchanterNpc.Enable", true);
    g_EnchanterMatsPct         = sConfigMgr->GetOption<uint32>("EnchanterNpc.MatsPct", 100);
    g_EnchanterLaborAtMax      = sConfigMgr->GetOption<uint32>("EnchanterNpc.LaborAtMax", 50000);
    g_EnchanterPriceRefreshSec = std::max<uint32>(30, sConfigMgr->GetOption<uint32>("EnchanterNpc.PriceRefreshSec", 300));
    g_EnchanterDebug           = sConfigMgr->GetOption<bool>("EnchanterNpc.Debug", false);
    LOG_INFO("server.loading", "[Enchanter] Enable={} MatsPct={} LaborAtMax={} PriceRefreshSec={} Debug={}",
        g_EnchanterEnable ? 1 : 0, g_EnchanterMatsPct, g_EnchanterLaborAtMax, g_EnchanterPriceRefreshSec,
        g_EnchanterDebug ? 1 : 0);
}
