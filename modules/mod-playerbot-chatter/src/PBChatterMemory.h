#ifndef MOD_PB_CHATTER_MEMORY_H
#define MOD_PB_CHATTER_MEMORY_H

#include <cstdint>
#include <deque>
#include <string>
#include <utility>

namespace PBChatterMemory
{
    // One exchange = (player message, bot reply).
    using Exchange = std::pair<std::string, std::string>;

    void LoadAllFromDB();                                  // call once at startup (world thread)
    std::deque<Exchange> Recent(uint64_t botGuid, uint64_t playerGuid); // copy of last-K (any thread)
    void Append(uint64_t botGuid, uint64_t playerGuid,
                std::string const& playerMsg, std::string const& botReply); // worker thread
    void FlushToDB();                                      // periodic + shutdown (world thread)
}

#endif
