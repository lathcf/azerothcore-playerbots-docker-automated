#include "RaidRosterCommand.h"
#include "RaidRosterConfig.h"
#include "RaidRosterComp.h"
#include "RaidRosterStore.h"
#include "Chat.h"
#include "CommandScript.h"
#include "RBAC.h"
#include "Player.h"
#include "PlayerbotMgr.h"
#include "Playerbots.h"
#include "RandomPlayerbotMgr.h"
#include "Mgr/Guild/PlayerbotGuildMgr.h"
#include "ObjectAccessor.h"
#include "CharacterCache.h"
#include "PlayerbotFactory.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "InstanceSaveMgr.h"
#include "SharedDefines.h"
#include "Containers.h"
#include <set>
#include <unordered_set>
#include <vector>
#include <map>
#include <cctype>

using namespace Acore::ChatCommands;

ChatCommandTable RaidRosterCommand::GetCommands() const
{
    // SEC_PLAYER (a security level, not an RBAC perm) makes these usable by any
    // logged-in character — same as mod-playerbots' own `.playerbot` command — so a
    // non-GM main can run them. The command framework treats values below the RBAC
    // threshold as a security level (GetSecurity() >= SEC_PLAYER is true for everyone);
    // Console::Yes still allows the worldserver console (handled before that check).
    static ChatCommandTable sub =
    {
        { "create", HandleCreate, SEC_PLAYER, Console::Yes },
        { "login",  HandleLogin,  SEC_PLAYER, Console::Yes },
        { "sync",   HandleSync,   SEC_PLAYER, Console::Yes },
        { "logout", HandleLogout, SEC_PLAYER, Console::Yes },
        { "reset",  HandleReset,  SEC_PLAYER, Console::Yes },
        { "remove", HandleRemove, SEC_PLAYER, Console::Yes },
        { "status", HandleStatus, SEC_PLAYER, Console::Yes },
    };
    static ChatCommandTable root = { { "raidroster", sub } };
    return root;
}

bool RaidRosterCommand::HandleCreate(ChatHandler* handler)
{
    if (!g_RaidRosterEnable) { handler->SendSysMessage("RaidRoster is disabled (set RaidRoster.Enable=1)."); return true; }

    Player* master = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
    if (!master) { handler->SendSysMessage("Run this in-world as a player."); return true; }

    uint32 owner = master->GetGUID().GetCounter();
    if (RaidRosterStore::Exists(owner))
    { handler->SendSysMessage("You already have a roster. Use .raidroster remove confirm first."); return true; }

    bool isAlliance = (master->GetTeamId(true) == TEAM_ALLIANCE);

    // Load every GUID already in any roster so we don't double-pin (would hit uk_bot).
    std::unordered_set<uint32> pinned = RaidRosterStore::AllPinnedBots();

    // Take a working copy of the addclass pools per class so we can pop chosen GUIDs.
    std::map<uint8, std::vector<ObjectGuid>> avail;
    for (RaidCompSlot const& slot : RAID_COMP)
    {
        if (avail.count(slot.cls)) continue;
        uint8 key = RandomPlayerbotMgr::GetTeamClassIdx(isAlliance, slot.cls);
        std::unordered_set<ObjectGuid> const& pool = sRandomPlayerbotMgr.addclassCache[key];
        std::vector<ObjectGuid> v;
        for (ObjectGuid g : pool)
        {
            if (ObjectAccessor::FindConnectedPlayer(g)) continue;                 // already online
            if (pinned.count(g.GetCounter())) continue;                          // already in some roster
            ObjectGuid::LowType guildId = sCharacterCache->GetCharacterGuildIdByGuid(g);
            // Skip only REAL (player) guilds; playerbots auto-guilds bots into synthetic
            // bot-guilds, which addclass (PlayerbotMgr.cpp:1176) treats as available.
            if (guildId && PlayerbotGuildMgr::instance().IsRealGuild(guildId)) continue;
            v.push_back(g);
        }
        avail[slot.cls] = std::move(v);
    }

    std::vector<RaidRosterRow> rows;
    rows.reserve(RAID_COMP.size());
    for (uint8 i = 0; i < static_cast<uint8>(RAID_COMP.size()); ++i)
    {
        RaidCompSlot const& slot = RAID_COMP[i];
        std::vector<ObjectGuid>& v = avail[slot.cls];
        if (v.empty())
        {
            handler->PSendSysMessage("Not enough addclass characters of class {} ({} faction). "
                "Raise AiPlayerbot.AddClassAccountPoolSize and restart.", uint32(slot.cls),
                isAlliance ? "Alliance" : "Horde");
            return true; // nothing persisted yet
        }
        ObjectGuid g = v.back(); v.pop_back();
        RaidRosterRow row;
        row.botGuid = g.GetCounter();
        row.cls = slot.cls; row.role = slot.role; row.specTab = slot.specTab; row.slot = i;
        rows.push_back(row);
    }

    RaidRosterStore::Replace(owner, rows);
    handler->PSendSysMessage("Created roster of {} bots (4 tank / 9 heal / 27 dps). "
        "Use .raidroster login [5|10|25|40] then .raidroster sync.", (uint32)rows.size());
    return true;
}

bool RaidRosterCommand::HandleLogin(ChatHandler* handler, Optional<uint32> sizeArg, Optional<std::string> roleArg)
{
    if (!g_RaidRosterEnable) { handler->SendSysMessage("RaidRoster is disabled (set RaidRoster.Enable=1)."); return true; }

    Player* master = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
    if (!master) { handler->SendSysMessage("Run this in-world as a player."); return true; }

    uint32 size = sizeArg ? *sizeArg : 40;
    SubComp const* sc = nullptr;
    for (SubComp const& s : RAID_SUBCOMPS) if (s.size == size) { sc = &s; break; }
    if (!sc) { handler->SendSysMessage("Size must be 5, 10, 25, or 40."); return true; }

    // Which slot the player fills: explicit override, else auto-detect from active spec.
    // 0 = tank, 1 = heal, 2 = dps. IsTank/IsHeal(bySpec=true) read the talent tab and work on
    // a human player.
    int playerRole;
    if (roleArg)
    {
        std::string rs = *roleArg;
        for (char& c : rs) c = (char)std::tolower((unsigned char)c);
        if (rs == "tank") playerRole = 0;
        else if (rs == "heal" || rs == "healer") playerRole = 1;
        else if (rs == "dps" || rs == "dd") playerRole = 2;
        else { handler->SendSysMessage("Role must be tank, heal, or dps."); return true; }
    }
    else
        playerRole = PlayerbotAI::IsTank(master, true) ? 0 : (PlayerbotAI::IsHeal(master, true) ? 1 : 2);

    uint32 owner = master->GetGUID().GetCounter();
    std::vector<RaidRosterRow> rows = RaidRosterStore::Load(owner);
    if (rows.empty()) { handler->SendSysMessage("No roster. Use .raidroster create."); return true; }

    // Roster capacity per role (caps how many of each we can actually field).
    uint8 availT = 0, availH = 0, availD = 0;
    for (RaidRosterRow const& r : rows)
    { if (r.role == 0) ++availT; else if (r.role == 1) ++availH; else ++availD; }

    // Bot sub-comp for this size; then let the player take their slot: drop one bot of the
    // player's role and backfill the freed slot (prefer dps, then heal, then tank) within roster
    // capacity, so the group stays full. A DPS player leaves the comp unchanged.
    uint8 needT = sc->tanks, needH = sc->heals, needD = sc->dps;
    bool tookSlot = false;
    if (playerRole == 0 && needT) { --needT; tookSlot = true; }
    else if (playerRole == 1 && needH) { --needH; tookSlot = true; }
    if (tookSlot)
    {
        if (needD < availD) ++needD;
        else if (needH < availH) ++needH;
        else if (needT < availT) ++needT;
    }
    uint8 const botT = needT, botH = needH, botD = needD;   // final line-up, for the report

    PlayerbotMgr* mgr = GET_PLAYERBOT_MGR(master);
    if (!mgr) { handler->SendSysMessage("Playerbot manager unavailable."); return true; }
    uint32 acct = master->GetSession()->GetAccountId();

    // Pick `need` bots per role: keep those already online first (so re-running login doesn't churn
    // the current group), then fill the remainder from a random shuffle of the role's offline bots
    // — so a fresh session brings a different assortment each time. Role balance is unchanged; we
    // only vary *which* reserved bots of a role get picked (each still runs its pinned tank/heal/dps
    // spec once you .raidroster sync).
    std::vector<RaidRosterRow> want;
    auto selectRole = [&](uint8 role, uint8 need)
    {
        std::vector<RaidRosterRow> online, offline;
        for (RaidRosterRow const& r : rows)
        {
            if (r.role != role) continue;
            ObjectGuid g = ObjectGuid::Create<HighGuid::Player>(r.botGuid);
            (mgr->GetPlayerBot(g) ? online : offline).push_back(r);
        }
        Acore::Containers::RandomShuffle(offline);
        for (RaidRosterRow const& r : online)  { if (!need) break; want.push_back(r); --need; }
        for (RaidRosterRow const& r : offline) { if (!need) break; want.push_back(r); --need; }
    };
    selectRole(0, needT);   // tanks
    selectRole(1, needH);   // heals
    selectRole(2, needD);   // dps

    // Set of wanted guids for trim-down.
    std::set<uint32> wantSet;
    for (RaidRosterRow const& r : want) wantSet.insert(r.botGuid);

    // Trim: log out every online roster bot that is NOT in the wanted set
    // (membership-based, so iteration order is irrelevant).
    uint32 dismissed = 0;
    for (RaidRosterRow const& r : rows)
    {
        if (wantSet.count(r.botGuid)) continue;
        ObjectGuid g = ObjectGuid::Create<HighGuid::Player>(r.botGuid);
        if (mgr->GetPlayerBot(g)) { mgr->LogoutPlayerBot(g); ++dismissed; }
    }

    // Add wanted bots that aren't online yet.
    for (RaidRosterRow const& r : want)
    {
        ObjectGuid g = ObjectGuid::Create<HighGuid::Player>(r.botGuid);
        if (mgr->GetPlayerBot(g)) continue;
        mgr->AddPlayerBot(g, acct);   // OnBotLogin auto-invites + converts group to raid at >=5
    }

    // Re-scan for honest online count: AddPlayerBot is async, so a just-submitted bot may not
    // appear as online on this same tick. Report the current count; it will rise momentarily.
    uint32 online = 0;
    for (RaidRosterRow const& r : want)
        if (mgr->GetPlayerBot(ObjectGuid::Create<HighGuid::Player>(r.botGuid))) ++online;

    // `size` is the raid size incl. you; you fill one slot, bots fill the rest.
    char const* roleName = playerRole == 0 ? "TANK" : (playerRole == 1 ? "HEALER" : "DPS");
    handler->PSendSysMessage("Raid {} — you fill the {} slot; {} bots ({} tank / {} heal / {} dps): "
        "{} online now, dismissed {}. (Bots load async; re-check or see .raidroster status.)",
        size, roleName, (uint32)want.size(), (uint32)botT, (uint32)botH, (uint32)botD, online, dismissed);
    return true;
}

bool RaidRosterCommand::HandleSync(ChatHandler* handler)
{
    if (!g_RaidRosterEnable) { handler->SendSysMessage("RaidRoster is disabled (set RaidRoster.Enable=1)."); return true; }

    Player* master = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
    if (!master) { handler->SendSysMessage("Run this in-world as a player."); return true; }

    uint32 owner = master->GetGUID().GetCounter();
    std::vector<RaidRosterRow> rows = RaidRosterStore::Load(owner);
    if (rows.empty()) { handler->SendSysMessage("No roster. Use .raidroster create."); return true; }

    PlayerbotMgr* mgr = GET_PLAYERBOT_MGR(master);
    if (!mgr) { handler->SendSysMessage("Playerbot manager unavailable."); return true; }

    // Mirror the init=auto ceiling formula (Bot/PlayerbotMgr.cpp:799-803).
    uint32 ceiling = (uint32)(PlayerbotAI::GetMixedGearScore(master, true, false, 12)
                              * sPlayerbotAIConfig.autoInitEquipLevelLimitRatio);
    if (ceiling == 0) ceiling = 1;

    uint32 synced = 0, offline = 0;
    for (RaidRosterRow const& r : rows)
    {
        ObjectGuid g = ObjectGuid::Create<HighGuid::Player>(r.botGuid);
        Player* bot = mgr->GetPlayerBot(g);
        if (!bot) { ++offline; continue; }

        // 1) Level to master + learn spells/skills + gear to ceiling (re-rolls spec, fixed next).
        //    With upstream defaults AiPlayerbot.EquipAndSpecPersistence=true /
        //    EquipAndSpecPersistenceLevel=1 (PlayerbotAIConfig.cpp:579-580), Randomize(false)
        //    already itemizes the bot for its *randomly-rolled* spec. We intentionally throw that
        //    gearing away: the spec it geared for is not the spec this slot must run. Step 4's
        //    explicit InitEquipment re-gears for the pinned spec (see why there).
        PlayerbotFactory factory(bot, master->GetLevel(), ITEM_QUALITY_LEGENDARY, ceiling);
        factory.Randomize(false);
        // 2) Force the pinned talent spec (0-based tab).
        PlayerbotFactory::InitTalentsBySpecNo(bot, (int)r.specTab, true);
        // 3) Re-derive tank/heal/dps strategies from the new spec.
        if (PlayerbotAI* ai = GET_PLAYERBOT_AI(bot)) ai->ResetStrategies(false);
        // 4) Re-itemize for the forced spec under the same ceiling. REQUIRED, not redundant:
        //    step 1's Randomize geared for a *random* spec; StatsWeightCalculator weights gear by
        //    the bot's active talent tab (StatsWeightCalculator.cpp:65), so once step 2 forces the
        //    pinned spec this re-gears correctly for it. Do not delete this thinking it duplicates
        //    step 1 — it would silently break role-correct gear.
        factory.InitEquipment(false);
        ++synced;
    }

    handler->PSendSysMessage("Synced {} online bot(s) to your level/gear and roles; {} offline (login them first).",
        synced, offline);
    return true;
}

bool RaidRosterCommand::HandleLogout(ChatHandler* handler)
{
    if (!g_RaidRosterEnable) { handler->SendSysMessage("RaidRoster is disabled (set RaidRoster.Enable=1)."); return true; }
    Player* master = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
    if (!master) { handler->SendSysMessage("Run this in-world as a player."); return true; }

    uint32 owner = master->GetGUID().GetCounter();
    std::vector<RaidRosterRow> rows = RaidRosterStore::Load(owner);
    if (rows.empty()) { handler->SendSysMessage("No roster."); return true; }

    PlayerbotMgr* mgr = GET_PLAYERBOT_MGR(master);
    uint32 out = 0;
    for (RaidRosterRow const& r : rows)
    {
        ObjectGuid g = ObjectGuid::Create<HighGuid::Player>(r.botGuid);
        if (mgr && mgr->GetPlayerBot(g)) { mgr->LogoutPlayerBot(g); ++out; }
    }
    handler->PSendSysMessage("Logged out {} roster bot(s).", out);
    return true;
}

// Unbind every saved instance for `guid` except the one on `skipMapId` (you cannot unbind the
// instance you are standing in). `online` is the live Player* if the character is logged in
// (the master, or an online bot) so its client gets the lockout-cleared update; pass nullptr
// for an offline bot — the DB row is deleted regardless (deleteFromDB=true). Returns the count.
static uint32 UnbindAll(ObjectGuid guid, Player* online, uint32 skipMapId)
{
    uint32 count = 0;
    for (uint8 i = 0; i < MAX_DIFFICULTY; ++i)
    {
        BoundInstancesMap const& binds = sInstanceSaveMgr->PlayerGetBoundInstances(guid, Difficulty(i));
        for (BoundInstancesMap::const_iterator itr = binds.begin(); itr != binds.end();)
        {
            if (itr->first != skipMapId)
            {
                sInstanceSaveMgr->PlayerUnbindInstance(guid, itr->first, Difficulty(i), true, online);
                itr = binds.begin();   // PlayerUnbindInstance mutated `binds`; restart iteration
                ++count;
            }
            else
                ++itr;
        }
    }
    return count;
}

bool RaidRosterCommand::HandleReset(ChatHandler* handler)
{
    if (!g_RaidRosterEnable) { handler->SendSysMessage("RaidRoster is disabled (set RaidRoster.Enable=1)."); return true; }
    Player* master = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
    if (!master) { handler->SendSysMessage("Run this in-world as a player."); return true; }

    uint32 owner = master->GetGUID().GetCounter();
    std::vector<RaidRosterRow> rows = RaidRosterStore::Load(owner);
    if (rows.empty()) { handler->SendSysMessage("No roster. Use .raidroster create."); return true; }

    uint32 const skipMapId = master->GetMapId();   // can't unbind the instance you're standing in

    // Master first (pass the live Player* so the client lockout list updates).
    uint32 total = UnbindAll(master->GetGUID(), master, skipMapId);

    // Then every roster bot — online ones get their live Player*, offline ones unbind DB-only.
    PlayerbotMgr* mgr = GET_PLAYERBOT_MGR(master);
    uint32 bots = 0;
    for (RaidRosterRow const& r : rows)
    {
        ObjectGuid g = ObjectGuid::Create<HighGuid::Player>(r.botGuid);
        Player* online = mgr ? mgr->GetPlayerBot(g) : nullptr;
        total += UnbindAll(g, online, skipMapId);
        ++bots;
    }

    handler->PSendSysMessage("Reset {} instance lock(s) across you + {} roster bot(s).", total, bots);
    return true;
}

bool RaidRosterCommand::HandleRemove(ChatHandler* handler, Optional<std::string> confirm)
{
    if (!g_RaidRosterEnable) { handler->SendSysMessage("RaidRoster is disabled (set RaidRoster.Enable=1)."); return true; }
    Player* master = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
    if (!master) { handler->SendSysMessage("Run this in-world as a player."); return true; }

    if (!confirm || *confirm != "confirm")
    { handler->SendSysMessage("This deletes your roster (characters return to the addclass pool). "
                              "Run: .raidroster remove confirm"); return true; }

    uint32 owner = master->GetGUID().GetCounter();
    std::vector<RaidRosterRow> rows = RaidRosterStore::Load(owner);
    if (rows.empty()) { handler->SendSysMessage("No roster to remove."); return true; }

    // Log out any that are online, then drop the rows. We do NOT delete the characters.
    PlayerbotMgr* mgr = GET_PLAYERBOT_MGR(master);
    for (RaidRosterRow const& r : rows)
    {
        ObjectGuid g = ObjectGuid::Create<HighGuid::Player>(r.botGuid);
        if (mgr && mgr->GetPlayerBot(g)) mgr->LogoutPlayerBot(g);
    }
    RaidRosterStore::Clear(owner);
    handler->SendSysMessage("Roster removed. Characters are back in the shared addclass pool.");
    return true;
}

bool RaidRosterCommand::HandleStatus(ChatHandler* handler)
{
    if (!g_RaidRosterEnable) { handler->SendSysMessage("RaidRoster is disabled (set RaidRoster.Enable=1)."); return true; }

    Player* master = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
    if (!master) { handler->SendSysMessage("This command must be run in-world by a player."); return true; }

    uint32 owner = master->GetGUID().GetCounter();
    std::vector<RaidRosterRow> rows = RaidRosterStore::Load(owner);
    if (rows.empty()) { handler->SendSysMessage("No roster. Use .raidroster create."); return true; }

    uint32 tanks = 0, heals = 0, dps = 0, online = 0, stale = 0;
    PlayerbotMgr* mgr = GET_PLAYERBOT_MGR(master);
    for (RaidRosterRow const& row : rows)
    {
        if (row.role == 0) ++tanks; else if (row.role == 1) ++heals; else ++dps;
        ObjectGuid bg = ObjectGuid::Create<HighGuid::Player>(row.botGuid);
        if (mgr && mgr->GetPlayerBot(bg)) ++online;
        // Stale: character deleted or no longer in the addclass pool.
        if (!sCharacterCache->GetCharacterAccountIdByGuid(bg) ||
            !sRandomPlayerbotMgr.IsAddclassBot(row.botGuid))
            ++stale;
    }
    handler->PSendSysMessage("Roster: {} bots ({} tank / {} heal / {} dps), {} online.",
        (uint32)rows.size(), tanks, heals, dps, online);
    if (stale)
        handler->PSendSysMessage("WARNING: {} roster slot(s) are stale (character deleted or left "
            "the addclass pool). Run .raidroster remove confirm then .raidroster create.", stale);
    return true;
}
