#ifndef MOD_PB_CHATTER_QUEUE_H
#define MOD_PB_CHATTER_QUEUE_H

#include <cstdint>
#include <string>
#include <vector>

enum class PBChatChannel : uint8_t { Whisper, Say, Party, Raid, General, Guild };

struct PBChatJob
{
    uint64_t     botGuid;       // bot GUID counter
    uint64_t     playerGuid;    // human GUID counter (0 for ambient)
    std::string  playerName;    // whisper target name (the human) for delivery ("" for ambient)
    PBChatChannel channel;
    std::string  systemPrompt;
    std::string  prompt;        // snapshot + history + message
    std::string  playerMessage; // for memory append (reactive only)

    // Lore (Tier-2 factual Q&A). When lore=true RunJob tries the sidecar first and
    // falls back to the reactive prompt below on any miss.
    bool         lore = false;
    std::string  lorePayload; // prebuilt /ask JSON body (world thread)

    // Ambient (self-initiated) extras. ambient=false => reactive (unchanged path).
    bool         ambient = false;
    uint8_t      ambientKind = 0;   // 0 zone, 1 group, 2 guild
    uint64_t     ambientIdent = 0;  // zoneId / group raw guid / guildId
    uint64_t     anchorPlayerGuid = 0; // a present real player's GUID counter
};

struct PBChatResult
{
    uint64_t     botGuid;
    uint64_t     playerGuid;
    std::string  playerName;
    PBChatChannel channel;
    std::string  reply;

    bool         ambient = false;
    uint8_t      ambientKind = 0;
    uint64_t     ambientIdent = 0;
    uint64_t     anchorPlayerGuid = 0;
};

namespace PBChatterQueue
{
    void Submit(PBChatJob job);                 // world thread -> spins/queues a worker
    // Ambient submit with reactive-first headroom: only admits the job when a worker
    // slot is free AND nothing is already pending, so ambient never delays a reactive
    // reply. Returns true if enqueued, false if dropped.
    bool TrySubmitAmbient(PBChatJob job);
    std::vector<PBChatResult> DrainResults();    // world thread (OnUpdate)
}
#endif
