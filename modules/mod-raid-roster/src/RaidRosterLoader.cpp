#include "ScriptMgr.h"
#include "Log.h"
#include "RaidRosterCommand.h"
#include "RaidRosterConfig.h"

class RaidRosterWorld : public WorldScript
{
public:
    RaidRosterWorld() : WorldScript("RaidRosterWorld") { }
    void OnAfterConfigLoad(bool /*reload*/) override { RaidRosterLoadConfig(); }
};

void Addmod_raid_rosterScripts()
{
    LOG_INFO("server.loading", "[RaidRoster] Registering scripts.");
    new RaidRosterWorld();
    new RaidRosterCommand();
}
