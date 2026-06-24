#include "PBChatterConfig.h"
#include "Config.h"
#include "Log.h"
#include <cctype>
#include <sstream>

bool        g_PBChatEnable        = false;
bool        g_PBChatDebug         = false;
std::string g_PBChatUrl           = "http://localhost:11434/api/generate";
std::string g_PBChatModel         = "gemma4:e4b";
bool        g_PBChatThink         = false;
std::string g_PBChatSystemPrompt  =
    "You're a real person playing World of Warcraft (Wrath of the Lich King, 3.3.5a), "
    "hanging out in-game and chatting with another player. Talk like a normal, friendly "
    "gamer typing in chat: relaxed, easygoing, happy to help or just make small talk. "
    "Keep it short and casual; light WoW slang is fine (lol, gg, lfg, ding, brb). You're "
    "playing a character, but you're the person behind the keyboard, not the in-game "
    "character and not an NPC, so skip the heavy fantasy roleplay voice or accent. "
    "You're easygoing and mostly relaxed, but you've got a real personality and a "
    "sense of humor, not a chipper customer-service bot. Often enough to notice, though "
    "not every line, let some edge show: be dry or sarcastic, gripe about the usual WoW "
    "pain (bad RNG, repair bills, wipes, endless rep and daily grinds), rib another "
    "player good-naturedly, or crack a dumb joke. Keep it light: tease, don't insult; "
    "never actually mean, hostile, or nasty toward the person you're talking to, and "
    "still genuinely help if someone asks (a little sarcasm about it is fine). Vary how "
    "the humor lands so you don't sound one-note. Never say you're an AI, "
    "assistant, bot, or game master. No markdown, no emojis, no asterisk-actions, no quotes.";
uint32_t    g_PBChatReplyMaxLen   = 200;
uint32_t    g_PBChatMaxConcurrent = 4;

bool        g_PBChatLoreEnable    = false;
std::string g_PBChatLoreUrl       = "http://ac-lore:8091/ask";
uint32_t    g_PBChatLoreTimeout   = 60;

float       g_PBChatSayRange      = 40.0f;
uint32_t    g_PBChatSayMaxBots    = 2;
uint32_t    g_PBChatSayChance     = 35;
uint32_t    g_PBChatGroupMaxBots  = 2;
uint32_t    g_PBChatGroupChance   = 50;
bool        g_PBChatGroupGuaranteeOne = true;
uint32_t    g_PBChatWhisperChance = 100;
uint32_t    g_PBChatHistoryLen    = 10;

bool        g_PBChatAmbientEnable        = false;
bool        g_PBChatAmbientGeneral       = true;
bool        g_PBChatAmbientGroup         = true;
bool        g_PBChatAmbientGuild         = true;
uint32_t    g_PBChatAmbientSeedMin       = 60;
uint32_t    g_PBChatAmbientSeedMax       = 90;
uint32_t    g_PBChatAmbientFollowMin     = 8;
uint32_t    g_PBChatAmbientFollowMax     = 20;
uint32_t    g_PBChatAmbientActiveWindow  = 45;
uint32_t    g_PBChatAmbientBotStreakMax  = 4;
uint32_t    g_PBChatAmbientCooldown      = 75;
uint32_t    g_PBChatAmbientPerBotCooldown= 240;
uint32_t    g_PBChatAmbientMaxPerMin     = 14;
uint32_t    g_PBChatAmbientBufferLen     = 8;
uint32_t    g_PBChatAmbientWGeneric      = 55;
uint32_t    g_PBChatAmbientWReact        = 25;
uint32_t    g_PBChatAmbientWFlavor       = 12;
uint32_t    g_PBChatAmbientWEvent        = 8;

std::vector<std::string> g_PBChatCommandKeywords;

static std::vector<std::string> SplitCsvLower(std::string const& csv)
{
    std::vector<std::string> out;
    std::stringstream ss(csv);
    std::string tok;
    while (std::getline(ss, tok, ','))
    {
        // trim
        size_t a = tok.find_first_not_of(" \t");
        size_t b = tok.find_last_not_of(" \t");
        if (a == std::string::npos)
            continue;
        std::string t = tok.substr(a, b - a + 1);
        for (char& c : t)
            c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
        if (!t.empty())
            out.push_back(t);
    }
    return out;
}

void PBChatterLoadConfig()
{
    g_PBChatEnable        = sConfigMgr->GetOption<bool>("PlayerbotChatter.Enable", false);
    g_PBChatDebug         = sConfigMgr->GetOption<bool>("PlayerbotChatter.Debug", false);
    g_PBChatUrl           = sConfigMgr->GetOption<std::string>("PlayerbotChatter.Url", "http://localhost:11434/api/generate");
    g_PBChatModel         = sConfigMgr->GetOption<std::string>("PlayerbotChatter.Model", "gemma4:e4b");
    g_PBChatThink         = sConfigMgr->GetOption<bool>("PlayerbotChatter.Think", false);
    g_PBChatSystemPrompt  = sConfigMgr->GetOption<std::string>("PlayerbotChatter.SystemPrompt", g_PBChatSystemPrompt);
    g_PBChatReplyMaxLen   = sConfigMgr->GetOption<uint32_t>("PlayerbotChatter.ReplyMaxLen", 200);
    g_PBChatMaxConcurrent = sConfigMgr->GetOption<uint32_t>("PlayerbotChatter.MaxConcurrent", 4);

    g_PBChatLoreEnable    = sConfigMgr->GetOption<bool>("PlayerbotChatter.LoreEnable", false);
    g_PBChatLoreUrl       = sConfigMgr->GetOption<std::string>("PlayerbotChatter.LoreUrl", "http://ac-lore:8091/ask");
    g_PBChatLoreTimeout   = sConfigMgr->GetOption<uint32_t>("PlayerbotChatter.LoreTimeout", 60);

    g_PBChatSayRange      = sConfigMgr->GetOption<float>("PlayerbotChatter.SayRange", 40.0f);
    g_PBChatSayMaxBots    = sConfigMgr->GetOption<uint32_t>("PlayerbotChatter.SayMaxBots", 2);
    g_PBChatSayChance     = sConfigMgr->GetOption<uint32_t>("PlayerbotChatter.SayChance", 35);
    g_PBChatGroupMaxBots  = sConfigMgr->GetOption<uint32_t>("PlayerbotChatter.GroupMaxBots", 2);
    g_PBChatGroupChance   = sConfigMgr->GetOption<uint32_t>("PlayerbotChatter.GroupChance", 50);
    g_PBChatGroupGuaranteeOne = sConfigMgr->GetOption<bool>("PlayerbotChatter.GroupGuaranteeOne", true);
    g_PBChatWhisperChance = sConfigMgr->GetOption<uint32_t>("PlayerbotChatter.WhisperChance", 100);
    g_PBChatHistoryLen    = sConfigMgr->GetOption<uint32_t>("PlayerbotChatter.HistoryLen", 10);

    g_PBChatAmbientEnable        = sConfigMgr->GetOption<bool>("PlayerbotChatter.AmbientEnable", false);
    g_PBChatAmbientGeneral       = sConfigMgr->GetOption<bool>("PlayerbotChatter.AmbientGeneral", true);
    g_PBChatAmbientGroup         = sConfigMgr->GetOption<bool>("PlayerbotChatter.AmbientGroup", true);
    g_PBChatAmbientGuild         = sConfigMgr->GetOption<bool>("PlayerbotChatter.AmbientGuild", true);
    g_PBChatAmbientSeedMin       = sConfigMgr->GetOption<uint32_t>("PlayerbotChatter.AmbientSeedMin", 60);
    g_PBChatAmbientSeedMax       = sConfigMgr->GetOption<uint32_t>("PlayerbotChatter.AmbientSeedMax", 90);
    g_PBChatAmbientFollowMin     = sConfigMgr->GetOption<uint32_t>("PlayerbotChatter.AmbientFollowMin", 8);
    g_PBChatAmbientFollowMax     = sConfigMgr->GetOption<uint32_t>("PlayerbotChatter.AmbientFollowMax", 20);
    g_PBChatAmbientActiveWindow  = sConfigMgr->GetOption<uint32_t>("PlayerbotChatter.AmbientActiveWindow", 45);
    g_PBChatAmbientBotStreakMax  = sConfigMgr->GetOption<uint32_t>("PlayerbotChatter.AmbientBotStreakMax", 4);
    g_PBChatAmbientCooldown      = sConfigMgr->GetOption<uint32_t>("PlayerbotChatter.AmbientCooldown", 75);
    g_PBChatAmbientPerBotCooldown= sConfigMgr->GetOption<uint32_t>("PlayerbotChatter.AmbientPerBotCooldown", 240);
    g_PBChatAmbientMaxPerMin     = sConfigMgr->GetOption<uint32_t>("PlayerbotChatter.AmbientMaxPerMin", 14);
    g_PBChatAmbientBufferLen     = sConfigMgr->GetOption<uint32_t>("PlayerbotChatter.AmbientBufferLen", 8);
    g_PBChatAmbientWGeneric      = sConfigMgr->GetOption<uint32_t>("PlayerbotChatter.AmbientWeightGeneric", 55);
    g_PBChatAmbientWReact        = sConfigMgr->GetOption<uint32_t>("PlayerbotChatter.AmbientWeightReact", 25);
    g_PBChatAmbientWFlavor       = sConfigMgr->GetOption<uint32_t>("PlayerbotChatter.AmbientWeightFlavor", 12);
    g_PBChatAmbientWEvent        = sConfigMgr->GetOption<uint32_t>("PlayerbotChatter.AmbientWeightEvent", 8);

    std::string kw = sConfigMgr->GetOption<std::string>("PlayerbotChatter.CommandKeywords",
        "follow,stay,flee,grind,attack,tank attack,do attack,accept,talk,reset,runaway,summon,"
        "q,c,u,e,ue,t,nt,s,b,r,rep,items,inv,pvp stats,add all loot,move from group,"
        "enter vehicle,leave vehicle,buy,sell,trade,cast,co,nc,rti,los,ll");
    g_PBChatCommandKeywords = SplitCsvLower(kw);

    LOG_INFO("server.loading", "[PlayerbotChatter] Config loaded (enabled={}).", g_PBChatEnable);
}
