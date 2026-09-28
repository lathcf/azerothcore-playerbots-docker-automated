#ifndef MOD_ENCHANTER_COMMAND_H
#define MOD_ENCHANTER_COMMAND_H

#include "Chat.h"
#include "CommandScript.h"

class EnchanterCommand : public CommandScript
{
public:
    EnchanterCommand() : CommandScript("EnchanterCommand") { }
    Acore::ChatCommands::ChatCommandTable GetCommands() const override;

    static bool HandleDump(ChatHandler* handler, std::string eraName, Acore::ChatCommands::Tail filter);
    static bool HandleReloadPrices(ChatHandler* handler);
};

#endif
