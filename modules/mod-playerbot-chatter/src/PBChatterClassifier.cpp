#include "PBChatterClassifier.h"
#include "PBChatterConfig.h"
#include "Player.h"
#include "Group.h"
#include "Playerbots.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "Random.h"
#include <algorithm>
#include <cctype>

namespace
{
    std::string Lower(std::string s)
    {
        for (char& c : s) c = (char)::tolower((unsigned char)c);
        return s;
    }

    bool IsBot(Player* p)
    {
        // upstream 17214b3 renamed PlayerbotAI::IsRealPlayer() (which meant "selfbot") to the
        // free IsSelfBot(Player*); the free IsRealPlayer(Player*) now means "no bot AI at all".
        PlayerbotAI* ai = GET_PLAYERBOT_AI(p);
        return ai && !IsSelfBot(p);
    }

    bool NameMentioned(Player* bot, std::string const& lowerMsg)
    {
        return lowerMsg.find(Lower(bot->GetName())) != std::string::npos;
    }

    // Random-N + per-bot chance from a candidate pool. When guaranteeOne is set and the
    // chance roll picks nobody (and no bot was named), force one random bot to reply so a
    // small party never gets dead silence after you talk to it.
    std::vector<Player*> PickFanout(std::vector<Player*> pool, std::string const& lowerMsg,
                                    uint32_t maxBots, uint32_t chance, bool guaranteeOne = false)
    {
        std::vector<Player*> chosen;
        // Named bots always reply (do not count against the chance roll, still count to cap).
        for (Player* b : pool)
            if (NameMentioned(b, lowerMsg))
                chosen.push_back(b);

        // Shuffle the rest (Fisher-Yates with urand) then chance-roll up to the cap.
        std::vector<Player*> rest;
        for (Player* b : pool)
            if (std::find(chosen.begin(), chosen.end(), b) == chosen.end())
                rest.push_back(b);
        for (size_t i = rest.size(); i > 1; --i)
            std::swap(rest[i - 1], rest[urand(0, i - 1)]);

        for (Player* b : rest)
        {
            if (chosen.size() >= maxBots)
                break;
            if (roll_chance_i((int)chance))
                chosen.push_back(b);
        }
        // Floor: nobody rolled in, so pick the first shuffled (already-random) candidate.
        if (guaranteeOne && chosen.empty() && !rest.empty() && maxBots > 0)
            chosen.push_back(rest.front());
        if (chosen.size() > maxBots)
            chosen.resize(maxBots);
        return chosen;
    }
}

namespace
{
    // Lowercase, trim surrounding whitespace, and drop trailing punctuation so "Stay!",
    // "Summon." and "follow " read the same as the bare keyword.
    std::string NormalizeForCommand(std::string const& msg)
    {
        std::string m = Lower(msg);
        size_t a = m.find_first_not_of(" \t");
        if (a == std::string::npos)
            return "";
        size_t b = m.find_last_not_of(" \t.!?,");
        if (b == std::string::npos || b < a)
            return m.substr(a, 1); // all punctuation: keep the first char for the prefix test
        return m.substr(a, b - a + 1);
    }

    // Control prefixes used by playerbots/MultiBot/console. '@' is MultiBot's
    // role/name target prefix (e.g. "@tank do attack my target", "@dps ...").
    bool HasControlPrefix(std::string const& m)
    {
        return !m.empty() && (m[0] == '.' || m[0] == '+' || m[0] == '-' || m[0] == '!' || m[0] == '#' || m[0] == '@');
    }

    // Built-in floor of playerbots chat commands (the fork's ChatCommandHandlerStrategy
    // trigger names), matched ONLY against the whole line. Independent of the configurable
    // CommandKeywords so a stale instantiated conf (it isn't regenerated from conf.dist) can
    // never let a bare "summon" through again. Whole-line only, so ambiguous words here
    // ("who", "go", "ready") still count as conversation when they open a sentence.
    char const* const kBareCommands[] = {
        "accept", "attack", "attackers", "aura", "autogear", "b", "bank", "buff", "buy", "bwl",
        "c", "calc", "cast", "castnc", "chat", "cheat", "co", "craft", "cs", "de", "destroy",
        "disperse", "dps", "drink", "drop", "e", "emblems", "emote", "equip", "flag", "flee",
        "follow", "formation", "gb", "ginvite", "glyphs", "go", "grind", "help", "hire", "home",
        "inv", "invite", "items", "leave", "lfg", "ll", "log", "loot", "los", "mail", "naxx",
        "nc", "nt", "outfit", "pet", "position", "pull", "q", "qi", "quests", "r", "ra", "range",
        "ready", "rebuff", "release", "rep", "repair", "reputation", "revive", "reward", "roll",
        "rti", "rtsc", "runaway", "s", "sell", "sendmail", "share", "spell", "spells", "ss",
        "stance", "stats", "stay", "summon", "t", "talents", "talk", "tame", "target", "taxi",
        "teleport", "trade", "trainer", "u", "ue", "unequip", "use", "who", "wipe", "wts",
        // multi-word commands that are commands only as the whole line
        "attack my target", "pull my target", "pull back", "max dps", "save mana", "ready check",
        "give leader", "pet attack", "focus heal", "open items", "force rebuff", "tell attackers",
        "tell target", "equip upgrade", "autogear bis", "spirit healer", "accept quest",
        "item count", "do attack", "tank attack",
    };

    bool IsBuiltInBareCommand(std::string const& m)
    {
        for (char const* c : kBareCommands)
            if (m == c)
                return true;
        return false;
    }
}

bool PBChatterClassifier::IsCommand(std::string const& msg)
{
    std::string m = NormalizeForCommand(msg);
    if (m.empty())
        return true; // empty -> nothing to reply to
    if (HasControlPrefix(m) || IsBuiltInBareCommand(m))
        return true;

    for (std::string const& kw : g_PBChatCommandKeywords)
    {
        if (m == kw)
            return true;
        // first-word / phrase match: keyword followed by a space.
        if (m.size() > kw.size() && m.compare(0, kw.size(), kw) == 0 && m[kw.size()] == ' ')
            return true;
    }
    return false;
}

bool PBChatterClassifier::IsBareCommand(std::string const& msg)
{
    std::string m = NormalizeForCommand(msg);
    if (m.empty() || HasControlPrefix(m) || IsBuiltInBareCommand(m))
        return true;
    for (std::string const& kw : g_PBChatCommandKeywords)
        if (m == kw)
            return true;
    return false;
}

bool PBChatterClassifier::IsRealPlayerSender(Player* sender)
{
    if (!sender)
        return false;
    // "a human at a client": no bot AI at all, or a selfbot (a real person whose own character
    // carries a PlayerbotAI). Upstream 17214b3 split the old PlayerbotAI::IsRealPlayer() member
    // into the free IsRealPlayer(Player*) / IsSelfBot(Player*) pair.
    PlayerbotAI* ai = GET_PLAYERBOT_AI(sender);
    return !ai || IsSelfBot(sender);
}

std::vector<Player*> PBChatterClassifier::ResolveSayTargets(Player* sender, std::string const& msg)
{
    std::vector<Player*> nearby;
    // reqAlive=true: dead bots don't chime in to ambient say. disallowGM=false.
    Acore::AnyPlayerInObjectRangeCheck check(sender, g_PBChatSayRange, true, false);
    Acore::PlayerListSearcher<Acore::AnyPlayerInObjectRangeCheck> searcher(sender, nearby, check);
    Cell::VisitObjects(sender, searcher, g_PBChatSayRange);

    std::vector<Player*> bots;
    for (Player* p : nearby)
        if (p != sender && IsBot(p))
            bots.push_back(p);

    return PickFanout(std::move(bots), Lower(msg), g_PBChatSayMaxBots, g_PBChatSayChance);
}

std::vector<Player*> PBChatterClassifier::ResolveGroupTargets(Player* sender, Group* group, std::string const& msg)
{
    std::vector<Player*> bots;
    if (!group)
        return bots;
    for (GroupReference* r = group->GetFirstMember(); r; r = r->next())
    {
        Player* m = r->GetSource();
        if (m && m != sender && IsBot(m))
            bots.push_back(m);
    }
    return PickFanout(std::move(bots), Lower(msg), g_PBChatGroupMaxBots, g_PBChatGroupChance,
                      g_PBChatGroupGuaranteeOne);
}

Player* PBChatterClassifier::ResolveWhisperTarget(Player* receiver)
{
    if (receiver && IsBot(receiver) && roll_chance_i((int)g_PBChatWhisperChance))
        return receiver;
    return nullptr;
}

bool PBChatterClassifier::IsLikelyQuestion(std::string const& msg)
{
    std::string m = Lower(msg);
    size_t a = m.find_first_not_of(" \t");
    if (a == std::string::npos)
        return false;
    m = m.substr(a);

    // Trailing '?' (ignore trailing spaces) is the strongest signal.
    size_t z = m.find_last_not_of(" \t");
    if (z != std::string::npos && m[z] == '?')
        return true;

    static const char* kQ[] = {"where", "what", "which", "how", "who", "when",
                               "whats", "wheres", "hows"};
    for (char const* q : kQ)
    {
        size_t n = std::char_traits<char>::length(q);
        if (m.compare(0, n, q) == 0 && (m.size() == n || m[n] == ' ' || m[n] == '\''))
            return true;
    }
    return false;
}
