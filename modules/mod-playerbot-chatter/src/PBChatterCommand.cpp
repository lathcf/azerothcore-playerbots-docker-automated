#include "PBChatterCommand.h"
#include "PBChatterAreaBackfill.h"
#include "Chat.h"
#include "CommandScript.h"
#include "RBAC.h"

using namespace Acore::ChatCommands;

ChatCommandTable PBChatterCommand::GetCommands() const
{
    static ChatCommandTable chatterTable =
    {
        { "backfillareas",  HandleBackfillAreasCommand,  rbac::RBAC_PERM_COMMAND_RELOAD, Console::Yes },
        { "backfillstatus", HandleBackfillStatusCommand, rbac::RBAC_PERM_COMMAND_RELOAD, Console::Yes },
    };
    static ChatCommandTable commandTable =
    {
        { "chatter", chatterTable },
    };
    return commandTable;
}

bool PBChatterCommand::HandleBackfillAreasCommand(ChatHandler* handler)
{
    handler->SendSysMessage(PBChatterAreaBackfill::Start().c_str());
    return true;
}

bool PBChatterCommand::HandleBackfillStatusCommand(ChatHandler* handler)
{
    handler->SendSysMessage(PBChatterAreaBackfill::StatusString().c_str());
    return true;
}
