#ifndef MOD_PB_CHATTER_AMBIENT_H
#define MOD_PB_CHATTER_AMBIENT_H

#include <cstdint>
#include <string>

// Ambient context kinds (also stored in PBChatJob::ambientKind).
enum PBChatterAmbientKind : uint8_t
{
    AMB_ZONE  = 0,   // a zone's General channel
    AMB_GROUP = 1,   // party / raid
    AMB_GUILD = 2,   // guild chat
};

namespace PBChatterAmbient
{
    // World thread only. Drive the director once per world update.
    void Tick(uint32_t diff);

    // Monotonic ms clock owned by the director (advanced by Tick). Used by the
    // event-seed store to timestamp/age events on the same timeline.
    uint32_t NowMs();

    // World thread. A real player posted a line into an ambient context. Creates the
    // context if needed, appends to its buffer, resets the bot streak/cooldown, and
    // re-energizes the thread so a bot can answer quickly.
    //   kind/ident: AMB_* + zoneId / group raw guid / guildId
    //   anchorPlayerGuid: that player's GUID counter
    void OnPlayerLine(uint8_t kind, uint64_t ident, uint64_t anchorPlayerGuid,
                      std::string const& speaker, std::string const& text);

    // World thread. An ambient bot line was actually delivered on a channel. Appends to
    // the context buffer, records the bot as a thread participant, and advances the
    // consecutive-bot streak: the thread ends (cooldown) at the BotStreakMax ceiling or,
    // past ThreadMinLen, with ThreadEndPct% per line; otherwise the next follow-up is
    // scheduled from now. botGuidCounter = bot's GUID counter.
    void OnBotLineDispatched(uint8_t kind, uint64_t ident, uint64_t botGuidCounter,
                             std::string const& speaker, std::string const& text);

    // World thread. An ambient job for this context came back from the worker, whether or
    // not a line was produced/sent. Releases the context's single-flight gate and, if its
    // next emit is already due, pushes it a few seconds out so a failed reply doesn't
    // trigger an instant retry burst. Call BEFORE OnBotLineDispatched for the same result.
    void OnAmbientJobFinished(uint8_t kind, uint64_t ident);
}

#endif
