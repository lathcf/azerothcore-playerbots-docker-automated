#include "PBChatterConfig.h"
#include "Config.h"
#include "Log.h"
#include <cctype>
#include <fstream>
#include <sstream>

bool        g_PBChatEnable        = false;
bool        g_PBChatDebug         = false;
std::string g_PBChatUrl           = "http://localhost:11434/api/generate";
std::string g_PBChatModel         = "gemma4:e4b";
bool        g_PBChatThink         = false;
std::string g_PBChatSystemPrompt  =
    "You're a real person playing WoW: Wrath of the Lich King (3.3.5a, level cap 80), "
    "hanging out in-game and chatting with other players. Type like a normal gamer in chat: "
    "short, relaxed, light slang (lol, gg, lfg, lfm, ding, brb, gz, ty, wtb, wts, pst). You're "
    "here to hang out, not to narrate your play session - talk about the game, the server, other "
    "players, your class, hot takes and gripes, the community, or whatever is on your mind, and "
    "the occasional totally off-topic real-life aside (tired, it is late, need coffee) is fine "
    "too. Don't invent fake WoW content or activities that do not exist. Each message quietly "
    "tells you your current level, as background only: never announce or tack your level onto "
    "what you say (no lvl-42 or level-42 tags), and keep any specific game references to things "
    "you would actually know by that level - if you are low or mid level you have NOT been to "
    "Northrend, run heroics, or raided (Naxxramas/Ulduar/ToC/ICC) and you never talk as if you "
    "have; only level-80 characters talk about heroics, raids, dailies, rep grinds, or endgame "
    "PvP. You're the person behind the keyboard, not the in-game character or an NPC - no fantasy "
    "roleplay voice. Vary how you start; never open with the word anyone. You've got a real "
    "personality: easygoing but opinionated, with a sense of humor, not a chipper "
    "customer-service bot. Be cranky, dry, and sarcastic when it fits - gripe about bad RNG, "
    "repair bills, wipes, grindy rep and dailies, class balance, the dungeon finder, blizzard, "
    "other servers, or the community; rib other players and share blunt opinions. Keep a floor "
    "though: no slurs, no real-world politics, and never genuinely hostile toward or targeting "
    "the person you are talking to - cranky and sarcastic is fine, cruel is not. Still help if "
    "someone actually asks (a little sarcasm about it is fine). Vary how the humor lands so you "
    "do not sound one-note. Never say you're an AI, bot, or game master. No markdown, emojis, "
    "asterisk-actions, or quotation marks.";
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
uint32_t    g_PBChatAmbientWGeneric      = 10;
uint32_t    g_PBChatAmbientWReact        = 35;
uint32_t    g_PBChatAmbientWFlavor       = 3;
uint32_t    g_PBChatAmbientWEvent        = 8;
uint32_t    g_PBChatAmbientWBanter       = 40;

std::vector<std::string> g_PBChatCommandKeywords;

std::string              g_PBChatStyleExamplesFile;
std::vector<std::string> g_PBChatStyleExamples;

// Load few-shot style lines from a plain-text file: one example per line,
// blank lines and '#' comments skipped, surrounding whitespace trimmed. On any
// failure the vector is left empty so callers fall back to the built-in list.
static std::vector<std::string> LoadStyleExamples(std::string const& path)
{
    std::vector<std::string> out;
    if (path.empty())
        return out;

    std::ifstream in(path);
    if (!in.is_open())
    {
        LOG_WARN("server.loading", "[PlayerbotChatter] StyleExamplesFile '{}' could not be opened; using built-in examples.", path);
        return out;
    }

    std::string line;
    while (std::getline(in, line))
    {
        if (!line.empty() && line.back() == '\r')  // tolerate CRLF
            line.pop_back();
        size_t a = line.find_first_not_of(" \t");
        if (a == std::string::npos)
            continue;                               // blank
        if (line[a] == '#')
            continue;                               // comment
        size_t b = line.find_last_not_of(" \t");
        out.push_back(line.substr(a, b - a + 1));
    }
    return out;
}

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
    g_PBChatAmbientWGeneric      = sConfigMgr->GetOption<uint32_t>("PlayerbotChatter.AmbientWeightGeneric", 10);
    g_PBChatAmbientWReact        = sConfigMgr->GetOption<uint32_t>("PlayerbotChatter.AmbientWeightReact", 35);
    g_PBChatAmbientWFlavor       = sConfigMgr->GetOption<uint32_t>("PlayerbotChatter.AmbientWeightFlavor", 3);
    g_PBChatAmbientWEvent        = sConfigMgr->GetOption<uint32_t>("PlayerbotChatter.AmbientWeightEvent", 8);
    g_PBChatAmbientWBanter       = sConfigMgr->GetOption<uint32_t>("PlayerbotChatter.AmbientWeightBanter", 40);

    std::string kw = sConfigMgr->GetOption<std::string>("PlayerbotChatter.CommandKeywords",
        "follow,stay,flee,grind,attack,tank attack,do attack,accept,talk,reset,runaway,summon,"
        "q,c,u,e,ue,t,nt,s,b,r,rep,items,inv,pvp stats,add all loot,move from group,"
        "enter vehicle,leave vehicle,buy,sell,trade,cast,co,nc,rti,los,ll");
    g_PBChatCommandKeywords = SplitCsvLower(kw);

    g_PBChatStyleExamplesFile = sConfigMgr->GetOption<std::string>("PlayerbotChatter.StyleExamplesFile", "");
    g_PBChatStyleExamples     = LoadStyleExamples(g_PBChatStyleExamplesFile);

    LOG_INFO("server.loading", "[PlayerbotChatter] Config loaded (enabled={}, style_examples={}).",
             g_PBChatEnable, g_PBChatStyleExamples.size());
}
