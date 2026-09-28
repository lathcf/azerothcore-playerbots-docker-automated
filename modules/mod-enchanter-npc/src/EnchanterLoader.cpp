#include "EnchanterCatalog.h"
#include "EnchanterCommand.h"
#include "EnchanterConfig.h"
#include "EnchanterPrice.h"
#include "Log.h"
#include "ScriptMgr.h"

void AddEnchanterNpcScript();   // EnchanterNpc.cpp

class EnchanterWorld : public WorldScript
{
public:
    EnchanterWorld() : WorldScript("EnchanterWorld") { }

    void OnAfterConfigLoad(bool /*reload*/) override { EnchanterLoadConfig(); }

    void OnStartup() override
    {
        if (!g_EnchanterEnable)
            return;
        EnchanterCatalog::Build();
        EnchanterPrice::Rebuild();
    }

    void OnUpdate(uint32 diff) override
    {
        if (!g_EnchanterEnable)
            return;
        EnchanterPrice::Tick(diff);
    }
};

void Addmod_enchanter_npcScripts()
{
    LOG_INFO("server.loading", "[Enchanter] Registering scripts.");
    new EnchanterWorld();
    AddEnchanterNpcScript();
    new EnchanterCommand();
}
