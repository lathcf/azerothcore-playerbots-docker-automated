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
        PlayerbotAI* ai = GET_PLAYERBOT_AI(p);
        return ai && !ai->IsRealPlayer();
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

bool PBChatterClassifier::IsCommand(std::string const& msg)
{
    std::string m = Lower(msg);
    size_t a = m.find_first_not_of(" \t");
    if (a == std::string::npos)
        return true; // empty -> nothing to reply to
    m = m.substr(a);

    // Control prefixes used by playerbots/MultiBot/console. '@' is MultiBot's
    // role/name target prefix (e.g. "@tank do attack my target", "@dps ...").
    if (!m.empty() && (m[0] == '.' || m[0] == '+' || m[0] == '-' || m[0] == '!' || m[0] == '#' || m[0] == '@'))
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

bool PBChatterClassifier::IsRealPlayerSender(Player* sender)
{
    if (!sender)
        return false;
    PlayerbotAI* ai = GET_PLAYERBOT_AI(sender);
    return !ai || ai->IsRealPlayer();
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
