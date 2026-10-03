#ifndef MOD_RAIDROSTER_ERA_H
#define MOD_RAIDROSTER_ERA_H

#include "Define.h"   // uint32 — RaidRosterEra.cpp includes this header first

class Player;

// Isolated in its own translation unit: mod-individual-progression's IndividualProgression.h and the
// playerbots headers (PlayerbotAI.h) BOTH define a global unscoped `GENERAL` enumerator, so including
// both in one .cpp is a redefinition error. RaidRosterCommand.cpp needs the playerbots headers, so
// every mod-individual-progression call lives here, where no playerbots header is included.
namespace RaidRosterEra
{
    // Set `bot`'s IP era to match `master`'s ("bots match whatever era the parent is"). No-op unless
    // both units are in world.
    void SyncBotToMaster(Player* master, Player* bot);

    // Teach `bot` every class-book spell (IP makes these book-only below 61/71 by re-gating their
    // trainer rows) that its IP progression tier and level have unlocked. Call AFTER
    // SyncBotToMaster — the tier read is the bot's own. Returns the number of spells learned.
    uint32 LearnBookSpells(Player* bot);

    // The master's IP progression state as a gear-tier cap: live state, else the level-band
    // fallback SyncBotToMaster uses (>=71 WotLK / >=61 TBC / else Vanilla), clamped to IP's
    // ProgressionLimit. IP disabled -> RaidRosterTierRules::kTierUncapped (0xFF, allow all).
    uint8 MasterTier(Player* master);
}

#endif // MOD_RAIDROSTER_ERA_H
