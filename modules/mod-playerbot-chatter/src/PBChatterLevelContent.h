#ifndef MOD_PB_CHATTER_LEVEL_CONTENT_H
#define MOD_PB_CHATTER_LEVEL_CONTENT_H
#include <cstdint>
#include <string>
#include <vector>

namespace PBChatterLevelContent
{
    // Real, level-appropriate content names for the bot's level band. team: 0 Alliance, 1 Horde
    // (matches TeamId). Returns a few names; empty only for nonsensical input.
    std::vector<std::string> Dungeons(uint8_t level);            // includes raids ONLY at 80
    std::vector<std::string> Zones(uint8_t level, uint8_t team); // faction-aware where it matters
}
#endif
