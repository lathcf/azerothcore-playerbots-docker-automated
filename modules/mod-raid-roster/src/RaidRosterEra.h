#ifndef MOD_RAIDROSTER_ERA_H
#define MOD_RAIDROSTER_ERA_H

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
}

#endif // MOD_RAIDROSTER_ERA_H
