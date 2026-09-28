#ifndef MOD_ENCHANTER_CONFIG_H
#define MOD_ENCHANTER_CONFIG_H

#include "Define.h"

extern bool   g_EnchanterEnable;
extern uint32 g_EnchanterMatsPct;
extern uint32 g_EnchanterLaborAtMax;
extern uint32 g_EnchanterPriceRefreshSec;
extern bool   g_EnchanterDebug;

void EnchanterLoadConfig();

#endif
