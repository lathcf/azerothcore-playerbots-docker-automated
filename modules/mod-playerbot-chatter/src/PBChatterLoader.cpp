// mod-playerbot-chatter — natural bot chat via Ollama.
// AGPL v3, same as AzerothCore.
#include "ScriptMgr.h"
#include "Log.h"
#include "PBChatterObserver.h"
#include "PBChatterWorld.h"
#include "PBChatterEvents.h"
#include "PBChatterCommand.h"

void Addmod_playerbot_chatterScripts()
{
    LOG_INFO("server.loading", "[PlayerbotChatter] Registering scripts.");
    new PBChatterWorld();
    new PBChatterObserver();
    new PBChatterCommand();
    PBChatterMakeEventScript();
}
