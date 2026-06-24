#ifndef MOD_PB_CHATTER_WORLD_H
#define MOD_PB_CHATTER_WORLD_H

#include "ScriptMgr.h"

class PBChatterWorld : public WorldScript
{
public:
    PBChatterWorld() : WorldScript("PBChatterWorld") {}

    void OnAfterConfigLoad(bool /*reload*/) override;   // load our config
    void OnStartup() override;                          // load history cache
    void OnUpdate(uint32 diff) override;                // drain results + deliver; periodic save
    void OnShutdown() override;                         // final flush

private:
    uint32 _saveTimer = 0;
};

#endif
