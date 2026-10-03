#ifndef MOD_RAID_ROSTER_COMMAND_H
#define MOD_RAID_ROSTER_COMMAND_H
#include "CommandScript.h"
#include "Chat.h"

class RaidRosterCommand : public CommandScript
{
public:
    RaidRosterCommand() : CommandScript("RaidRosterCommand") { }
    Acore::ChatCommands::ChatCommandTable GetCommands() const override;

    static bool HandleCreate(ChatHandler* handler);
    static bool HandleLogin(ChatHandler* handler, Optional<uint32> size, Optional<std::string> role);
    static bool HandleSync(ChatHandler* handler);
    static bool HandleSyncOne(ChatHandler* handler, std::string name, Optional<std::string> role);
    static bool HandleLogout(ChatHandler* handler);
    static bool HandleReset(ChatHandler* handler);
    static bool HandleRemove(ChatHandler* handler, Optional<std::string> confirm);
    static bool HandleStatus(ChatHandler* handler);
    static bool HandleItemTier(ChatHandler* handler, uint32 itemId);
    static bool HandleTierSweep(ChatHandler* handler, uint32 maxTier, uint32 minIlvl);
};
#endif
