#ifndef MOD_PB_CHATTER_EVENTS_H
#define MOD_PB_CHATTER_EVENTS_H

#include <cstdint>
#include <string>

class PlayerScript;

namespace PBChatterEvents
{
    // World thread. If the bot has a fresh (<=120s) event seed, copies its phrase into
    // outHint and returns true; consumes the seed either way. botGuidCounter = counter.
    bool Take(uint64_t botGuidCounter, uint32_t nowMs, std::string& outHint);
}

// Registered by the loader. Stamps a short-lived "last event" phrase on bots.
PlayerScript* PBChatterMakeEventScript();

#endif
