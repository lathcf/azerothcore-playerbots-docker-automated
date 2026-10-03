#include "ScriptMgr.h"
#include "Log.h"
#include "Player.h"
#include "RaidRosterCommand.h"
#include "RaidRosterConfig.h"
#include "RaidRosterLevelLock.h"

class RaidRosterWorld : public WorldScript
{
public:
    RaidRosterWorld() : WorldScript("RaidRosterWorld") { }
    void OnAfterConfigLoad(bool /*reload*/) override { RaidRosterLoadConfig(); }
    void OnStartup() override { RaidRosterLevelLock::LoadFromDB(); }
};

// Level lock: a pinned roster bot keeps the level sync gave it. Covers kill, quest, LFG, explore
// and BG XP (all route through this core hook). Runs on map threads; IsLocked reads an immutable
// snapshot.
class RaidRosterPlayer : public PlayerScript
{
public:
    RaidRosterPlayer() : PlayerScript("RaidRosterPlayer", { PLAYERHOOK_ON_GIVE_EXP }) { }

    void OnPlayerGiveXP(Player* player, uint32& amount, Unit* /*victim*/, uint8 /*xpSource*/) override
    {
        if (!g_RaidRosterEnable || !player || !amount)   // bail FIRST, before touching anything
            return;
        if (RaidRosterLevelLock::IsLocked(player->GetGUID().GetCounter()))
            amount = 0;   // roster bots keep the level sync gave them (spec 2026-10-01)
    }
};

void Addmod_raid_rosterScripts()
{
    LOG_INFO("server.loading", "[RaidRoster] Registering scripts.");
    new RaidRosterWorld();
    new RaidRosterPlayer();
    new RaidRosterCommand();
}
