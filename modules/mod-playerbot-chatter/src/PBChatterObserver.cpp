#include "PBChatterObserver.h"
#include "PBChatterConfig.h"
#include "PBChatterClassifier.h"
#include "PBChatterContext.h"
#include "PBChatterMemory.h"
#include "PBChatterQueue.h"
#include "PBChatterLore.h"
#include "PBChatterAmbient.h"
#include "Player.h"
#include "Group.h"
#include "Guild.h"
#include "Channel.h"
#include "SharedDefines.h"
#include "StringFormat.h"

namespace
{
    std::string BuildPrompt(Player* bot, Player* sender, std::string const& msg)
    {
        std::string p = PBChatterContext::BuildSnapshot(bot);
        auto recent = PBChatterMemory::Recent(bot->GetGUID().GetCounter(), sender->GetGUID().GetCounter());
        if (!recent.empty())
        {
            p += Acore::StringFormat(
                "\n\nYou and {} have talked before. This is your memory of those earlier "
                "chats (oldest first) — you DO remember this person, so stay consistent and "
                "never claim it's the first time you've met:", sender->GetName());
            for (auto const& ex : recent)
                p += Acore::StringFormat("\n{}: {}\nYou: {}", sender->GetName(), ex.first, ex.second);
        }
        p += Acore::StringFormat("\n\n{} just said to you: \"{}\"\nReply briefly, like a normal player chatting back{}.",
                                 sender->GetName(), msg,
                                 recent.empty() ? "" : ", using what you remember above");
        return p;
    }

    void Enqueue(Player* bot, Player* sender, PBChatChannel channel, std::string const& msg)
    {
        PBChatJob job;
        job.botGuid       = bot->GetGUID().GetCounter();
        job.playerGuid    = sender->GetGUID().GetCounter();
        job.playerName    = sender->GetName();
        job.channel       = channel;
        job.systemPrompt  = g_PBChatSystemPrompt;
        job.prompt        = BuildPrompt(bot, sender, msg);
        job.playerMessage = msg;
        PBChatterQueue::Submit(std::move(job));
    }

    // Whisper path: route a likely factual question to the lore sidecar. Carries the
    // normal reactive prompt as the in-worker fallback, so a sidecar miss still replies.
    void EnqueueLore(Player* bot, Player* sender, std::string const& msg)
    {
        PBChatJob job;
        job.botGuid       = bot->GetGUID().GetCounter();
        job.playerGuid    = sender->GetGUID().GetCounter();
        job.playerName    = sender->GetName();
        job.channel       = PBChatChannel::Whisper;
        job.systemPrompt  = g_PBChatSystemPrompt;
        job.prompt        = BuildPrompt(bot, sender, msg);   // reactive fallback
        job.playerMessage = msg;
        job.lore          = true;
        job.lorePayload   = PBChatterLore::BuildPayload(bot, sender, msg);
        PBChatterQueue::Submit(std::move(job));
    }

    // Drop addon traffic (DBM/Recount/the MultiBot control addon, etc.): it rides the
    // same chat channels but must never become a spoken AI reply. lang carries this.
    bool Eligible(Player* sender, uint32 lang, std::string const& msg)
    {
        return g_PBChatEnable
            && lang != LANG_ADDON
            && PBChatterClassifier::IsRealPlayerSender(sender)
            && !PBChatterClassifier::IsCommand(msg);
    }

    // Core ChatChannels.dbc id for the per-zone General channel.
    constexpr uint32 GENERAL_CHANNEL_ID = 1;

    // Lighter gate for ambient buffer feeds: a real player, non-addon line. (Unlike
    // Eligible(), this does NOT drop command-like text — General/guild banter is fair
    // game to react to.)
    bool BufferEligible(Player* sender, uint32 lang)
    {
        return g_PBChatEnable && g_PBChatAmbientEnable
            && lang != LANG_ADDON
            && PBChatterClassifier::IsRealPlayerSender(sender);
    }
}

bool PBChatterObserver::OnPlayerCanUseChat(Player* player, uint32 type, uint32 lang, std::string& msg)
{
    if (Eligible(player, lang, msg) && (type == CHAT_MSG_SAY || type == CHAT_MSG_YELL))
        for (Player* bot : PBChatterClassifier::ResolveSayTargets(player, msg))
            Enqueue(bot, player, PBChatChannel::Say, msg);
    return true; // never block
}

bool PBChatterObserver::OnPlayerCanUseChat(Player* player, uint32 /*type*/, uint32 lang, std::string& msg, Player* receiver)
{
    if (Eligible(player, lang, msg))
        if (Player* bot = PBChatterClassifier::ResolveWhisperTarget(receiver))
        {
            if (g_PBChatLoreEnable && PBChatterClassifier::IsLikelyQuestion(msg))
                EnqueueLore(bot, player, msg);
            else
                Enqueue(bot, player, PBChatChannel::Whisper, msg);
        }
    return true; // never block
}

bool PBChatterObserver::OnPlayerCanUseChat(Player* player, uint32 type, uint32 lang, std::string& msg, Group* group)
{
    if (Eligible(player, lang, msg))
    {
        bool isRaid = (type == CHAT_MSG_RAID || type == CHAT_MSG_RAID_LEADER || type == CHAT_MSG_RAID_WARNING);
        PBChatChannel ch = isRaid ? PBChatChannel::Raid : PBChatChannel::Party;
        for (Player* bot : PBChatterClassifier::ResolveGroupTargets(player, group, msg))
            Enqueue(bot, player, ch, msg);
    }
    if (group && BufferEligible(player, lang))
        PBChatterAmbient::OnPlayerLine(AMB_GROUP, group->GetGUID().GetRawValue(),
                                       player->GetGUID().GetCounter(), player->GetName(), msg);
    return true; // never block
}

bool PBChatterObserver::OnPlayerCanUseChat(Player* player, uint32 /*type*/, uint32 lang, std::string& msg, Guild* guild)
{
    if (guild && BufferEligible(player, lang))
        PBChatterAmbient::OnPlayerLine(AMB_GUILD, guild->GetId(),
                                       player->GetGUID().GetCounter(), player->GetName(), msg);
    return true; // never block
}

bool PBChatterObserver::OnPlayerCanUseChat(Player* player, uint32 /*type*/, uint32 lang, std::string& msg, Channel* channel)
{
    if (channel && channel->GetChannelId() == GENERAL_CHANNEL_ID && BufferEligible(player, lang))
        PBChatterAmbient::OnPlayerLine(AMB_ZONE, player->GetZoneId(),
                                       player->GetGUID().GetCounter(), player->GetName(), msg);
    return true; // never block
}
