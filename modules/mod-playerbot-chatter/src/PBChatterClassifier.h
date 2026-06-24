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
