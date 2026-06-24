#ifndef MOD_PB_CHATTER_OBSERVER_H
#define MOD_PB_CHATTER_OBSERVER_H

#include "ScriptMgr.h"
#include <string>

class Guild;
class Channel;

class PBChatterObserver : public PlayerScript
{
public:
    PBChatterObserver() : PlayerScript("PBChatterObserver", {
        PLAYERHOOK_CAN_PLAYER_USE_CHAT,
        PLAYERHOOK_CAN_PLAYER_USE_PRIVATE_CHAT,
        PLAYERHOOK_CAN_PLAYER_USE_GROUP_CHAT,
        PLAYERHOOK_CAN_PLAYER_USE_GUILD_CHAT,
        PLAYERHOOK_CAN_PLAYER_USE_CHANNEL_CHAT,
    }) {}

    bool OnPlayerCanUseChat(Player* player, uint32 type, uint32 lang, std::string& msg) override;            // say/yell
    bool OnPlayerCanUseChat(Player* player, uint32 type, uint32 lang, std::string& msg, Player* receiver) override; // whisper
    bool OnPlayerCanUseChat(Player* player, uint32 type, uint32 lang, std::string& msg, Group* group) override;     // party/raid
    bool OnPlayerCanUseChat(Player* player, uint32 type, uint32 lang, std::string& msg, Guild* guild) override;   // guild
    bool OnPlayerCanUseChat(Player* player, uint32 type, uint32 lang, std::string& msg, Channel* channel) override; // channel (General)
};

#endif
