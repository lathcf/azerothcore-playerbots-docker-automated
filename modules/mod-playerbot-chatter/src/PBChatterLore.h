#ifndef MOD_PB_CHATTER_LORE_H
#define MOD_PB_CHATTER_LORE_H
#include <string>
class Player;
namespace PBChatterLore
{
    // World-thread only: assemble the /ask JSON body. `sender` is the human who whispered —
    // used for the player's name AND their real quest log with per-objective progress.
    std::string BuildPayload(Player* bot, Player* sender, std::string const& question);

    // Worker-thread only: POST the payload to the sidecar; returns the phrased
    // reply, or "" when the sidecar is disabled/unreachable/ok:false (-> caller
    // falls back to the normal reactive reply).
    std::string Ask(std::string const& payload);
}
#endif
