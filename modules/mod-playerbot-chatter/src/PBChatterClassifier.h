#ifndef MOD_PB_CHATTER_CLASSIFIER_H
#define MOD_PB_CHATTER_CLASSIFIER_H

#include <cstdint>
#include <string>
#include <vector>

class Player;
class Group;

namespace PBChatterClassifier
{
    bool IsCommand(std::string const& msg);                 // true -> leave it for playerbots
    // Narrower: the WHOLE line is a command (control prefix or exactly one keyword, trailing
    // punctuation ignored). For General/guild buffer feeds, where a keyword can legitimately
    // open banter ("trade chat is wild") but a bare "follow" is never conversation.
    bool IsBareCommand(std::string const& msg);
    // Loose heuristic: does this read like a factual question? (ends with '?' or
    // opens with a question word). Used to route whispers to the lore sidecar.
    bool IsLikelyQuestion(std::string const& msg);
    bool IsRealPlayerSender(Player* sender);                // false for bots/null
    // Resolve responding bots for each channel. World thread (reads world state).
    std::vector<Player*> ResolveSayTargets(Player* sender, std::string const& msg);
    std::vector<Player*> ResolveGroupTargets(Player* sender, Group* group, std::string const& msg);
    Player* ResolveWhisperTarget(Player* receiver);         // the bot, or null
}
#endif
