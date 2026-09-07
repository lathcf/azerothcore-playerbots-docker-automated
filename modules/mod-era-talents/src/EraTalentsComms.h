#ifndef MOD_ERA_TALENTS_COMMS_H
#define MOD_ERA_TALENTS_COMMS_H
class Player;
namespace EraTalentsComms
{
    // Push the authoritative ERATAL SYNC (era + managed-flag + points + spent ranks) to a player's addon.
    // Called on demand (HELLO/LEARN) and after a mid-session era transition so the
    // client panel flips isEraChar without a relog.
    void SendSync(Player* p);
}
#endif
