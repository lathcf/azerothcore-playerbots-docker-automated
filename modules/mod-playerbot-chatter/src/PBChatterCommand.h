#ifndef MOD_PB_CHATTER_COMMAND_H
#define MOD_PB_CHATTER_COMMAND_H
#include "CommandScript.h"
#include "Chat.h"

class PBChatterCommand : public CommandScript
{
public:
    PBChatterCommand() : CommandScript("PBChatterCommand") { }
    Acore::ChatCommands::ChatCommandTable GetCommands() const override;

    static bool HandleBackfillAreasCommand(ChatHandler* handler);
    static bool HandleBackfillStatusCommand(ChatHandler* handler);
};
#endif
