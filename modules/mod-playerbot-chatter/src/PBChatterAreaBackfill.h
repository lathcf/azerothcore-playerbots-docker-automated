#ifndef MOD_PB_CHATTER_AREA_BACKFILL_H
#define MOD_PB_CHATTER_AREA_BACKFILL_H
#include <cstdint>
#include <string>

// One-time, operator-triggered, throttled backfill of mod_chatter_npc_area.
// All calls are world-thread only (Map/DBC access).
namespace PBChatterAreaBackfill
{
    // Load the work list (creature entries with a spawn, not yet in the table) and begin.
    // Returns a human-readable summary for the command output.
    std::string Start();
    // Process up to a small batch of entries; called every world tick. No-op when idle.
    void Tick(uint32_t diff);
    // Human-readable progress/idle summary for the status command.
    std::string StatusString();
}
#endif
