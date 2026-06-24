#ifndef MOD_PB_CHATTER_AMBIENT_PROMPT_H
#define MOD_PB_CHATTER_AMBIENT_PROMPT_H

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

class Player;

namespace PBChatterAmbientPrompt
{
    // Content modes (match the weight knobs).
    enum Mode { MODE_GENERIC = 0, MODE_REACT = 1, MODE_FLAVOR = 2, MODE_EVENT = 3 };

    // World thread (MODE_FLAVOR reads live state via ContextBuilder).
    //   kind:    AMB_ZONE / AMB_GROUP / AMB_GUILD (names the channel for tone)
    //   recent:  rolling buffer as (speaker, text), oldest first (used by MODE_REACT)
    //   eventHint: phrase for MODE_EVENT (e.g. "you just dinged level 40")
    std::string Build(int mode, Player* bot, uint8_t kind,
                      std::vector<std::pair<std::string, std::string>> const& recent,
                      std::string const& eventHint);
}

#endif
