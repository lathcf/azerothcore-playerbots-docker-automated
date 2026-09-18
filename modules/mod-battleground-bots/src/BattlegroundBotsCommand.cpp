// modules/mod-battleground-bots/src/BattlegroundBotsCommand.cpp
#include "BattlegroundBotsDirector.h"
#include "Chat.h"
#include "ChatCommand.h"
#include "CommandScript.h"
#include "Map.h"
#include "MapMgr.h"
#include "ObjectAccessor.h"
#include "PathGenerator.h"
#include "Player.h"
#include "ScriptMgr.h"
#include <cmath>

using namespace Acore::ChatCommands;

class BattlegroundBotsCommandScript : public CommandScript
{
public:
    BattlegroundBotsCommandScript() : CommandScript("BattlegroundBotsCommandScript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable sub =
        {
            { "status", HandleStatus, SEC_GAMEMASTER, Console::Yes },
            { "probe",  HandleProbe,  SEC_GAMEMASTER, Console::Yes },
        };
        static ChatCommandTable root = { { "bgbots", sub } };
        return root;
    }

    static bool HandleStatus(ChatHandler* handler, Optional<uint32> instanceId)
    {
        BattlegroundBotsDirector* dir = BattlegroundBotsDirector::instance();
        if (!dir) { handler->SendSysMessage("BattlegroundBots director is not loaded."); return true; }
        handler->SendSysMessage(dir->StatusText(instanceId.value_or(0)));
        return true;
    }

    // .bgbots probe <mapId> <x> <y> <z> <fromX> <fromY> <fromZ>
    // Ground-Z sanity at both ends + PathGenerator verdict FROM the objective TO the slot, using a
    // player currently on that map as the path owner (join the BG on a GM character first).
    static bool HandleProbe(ChatHandler* handler, uint32 mapId, float x, float y, float z, float fx, float fy, float fz)
    {
        Map* map = sMapMgr->FindMap(mapId, 0);
        if (!map)
        {
            // BG maps are instanced: find any instance of this map through the players in it.
            for (auto const& [guid, p] : ObjectAccessor::GetPlayers())
                if (p && p->GetMapId() == mapId && p->IsInWorld()) { map = p->GetMap(); break; }
        }
        if (!map) { handler->PSendSysMessage("probe: no loaded instance of map {} — join the BG first", mapId); return true; }
        Player* owner = nullptr;
        for (auto const& ref : map->GetPlayers()) if (ref.GetSource() && ref.GetSource()->IsInWorld()) { owner = ref.GetSource(); break; }
        if (!owner) { handler->SendSysMessage("probe: no player on that map to own the path"); return true; }

        float gz  = map->GetHeight(PHASEMASK_NORMAL, x, y, z + 5.0f, true, 50.0f);
        float gfz = map->GetHeight(PHASEMASK_NORMAL, fx, fy, fz + 5.0f, true, 50.0f);
        if (gz <= INVALID_HEIGHT + 1.0f || gfz <= INVALID_HEIGHT + 1.0f)
        { handler->PSendSysMessage("probe: NO GROUND at an endpoint (slot z={} from z={})", gz, gfz); return true; }
        if (std::fabs(gz - z) > 2.5f) handler->PSendSysMessage("probe: WARN slot z {} vs ground {} (fix the authored z)", z, gz);

        PathGenerator pg(owner);
        bool ok = pg.CalculatePath(fx, fy, gfz, x, y, gz, false);
        uint32 type = pg.GetPathType();
        bool bad = (type & (PATHFIND_NOPATH | PATHFIND_SHORTCUT | PATHFIND_NOT_USING_PATH | PATHFIND_INCOMPLETE)) != 0;
        handler->PSendSysMessage("probe: {} calc={} type=0x{:x} points={} end=({:.1f},{:.1f},{:.1f})",
            (ok && !bad) ? "REACHABLE" : "BLOCKED", ok ? 1 : 0, type, pg.GetPath().size(),
            pg.GetActualEndPosition().x, pg.GetActualEndPosition().y, pg.GetActualEndPosition().z);
        return true;
    }
};

void AddBattlegroundBotsCommandScripts() { new BattlegroundBotsCommandScript(); }
