#include "RaidRosterCommand.h"
#include "RaidRosterConfig.h"
#include "RaidRosterComp.h"
#include "RaidRosterStore.h"
#include "RaidRosterGear.h"
#include "Chat.h"
#include "StringFormat.h"   // Acore::StringFormat for the login era-band suffix
#include "CommandScript.h"
#include "RBAC.h"
#include "Player.h"
#include "PlayerbotMgr.h"
#include "RaidRosterEra.h"           // era-sync shim (isolates mod-individual-progression's headers)
#include "Playerbots.h"
#include "RandomPlayerbotMgr.h"
#include "Mgr/Guild/PlayerbotGuildMgr.h"
#include "ObjectAccessor.h"
#include "CharacterCache.h"
#include "PlayerbotFactory.h"
#include "EraTalentBots.h"   // era-managed bots get era builds at sync (mod-era-talents)
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "InstanceSaveMgr.h"
#include "DatabaseEnv.h"
#include "QueryResult.h"
#include "Field.h"
#include "SharedDefines.h"
#include "Containers.h"
#include <set>
#include <array>
#include <utility>
#include <unordered_set>
#include <vector>
#include <map>
#include <cctype>
#include <algorithm>

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
        { "syncone",HandleSyncOne,SEC_PLAYER, Console::Yes },
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
    bool isAlliance = (master->GetTeamId(true) == TEAM_ALLIANCE);

    // `create` is an idempotent TOP-UP. It pins bots only for comp slots this owner lacks (all 44
    // for a fresh roster; the 4 era substitutes for a roster created before them) and re-bands
    // existing rows whose stored band drifted from the comp (pre-substitute rosters store band 0
    // for their Death Knight slots). Stored rows map to RAID_COMP by slot_index, which is why
    // slots 0-39 of the comp must never be reordered.
    std::vector<RaidRosterRow> existing = RaidRosterStore::Load(owner);
    std::array<bool, RAID_COMP.size()> have{};
    std::vector<std::pair<uint8, uint8>> reband;   // (slot, comp band)
    for (RaidRosterRow const& r : existing)
    {
        if (r.slot >= RAID_COMP.size()) continue;   // slot from a larger past comp: leave it alone
        have[r.slot] = true;
        if (r.band != RAID_COMP[r.slot].band) reband.emplace_back(r.slot, RAID_COMP[r.slot].band);
    }
    std::vector<uint8> missing;
    for (uint8 i = 0; i < static_cast<uint8>(RAID_COMP.size()); ++i)
        if (!have[i]) missing.push_back(i);

    if (missing.empty() && reband.empty())
    {
        handler->PSendSysMessage("Roster is complete ({} slots). Use .raidroster login [5|10|25|40] then .raidroster sync.",
            (uint32)RAID_COMP.size());
        return true;
    }

    // Load every GUID already in any roster so we don't double-pin (would hit uk_bot).
    std::unordered_set<uint32> pinned = RaidRosterStore::AllPinnedBots();

    // Exclude mod-arena-roster pins (partner pool + opponent ladder) — arena bots are
    // offline most of the time and would otherwise look available here. Guarded by a
    // one-time table-existence probe (static local) so installs WITHOUT mod-arena-roster
    // (its SQL never ran) don't log a missing-table query error on every create.
    static bool const arenaTablesExist =
        bool(CharacterDatabase.Query("SHOW TABLES LIKE 'mod_arena_roster'")) &&
        bool(CharacterDatabase.Query("SHOW TABLES LIKE 'mod_arena_pool'"));
    if (arenaTablesExist)
    {
        if (QueryResult r = CharacterDatabase.Query("SELECT bot_guid FROM mod_arena_roster"))
            do { pinned.insert(r->Fetch()[0].Get<uint32>()); } while (r->NextRow());
        if (QueryResult r = CharacterDatabase.Query("SELECT bot_guid FROM mod_arena_pool"))
            do { pinned.insert(r->Fetch()[0].Get<uint32>()); } while (r->NextRow());
    }

    // Take a working copy of the addclass pools per class (only the classes we still need) so we
    // can pop chosen GUIDs.
    std::map<uint8, std::vector<ObjectGuid>> avail;
    for (uint8 i : missing)
    {
        RaidCompSlot const& slot = RAID_COMP[i];
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
    rows.reserve(missing.size());
    for (uint8 i : missing)
    {
        RaidCompSlot const& slot = RAID_COMP[i];
        std::vector<ObjectGuid>& v = avail[slot.cls];
        if (v.empty())
        {
            handler->PSendSysMessage("Not enough addclass characters of class {} ({} faction). "
                "Raise AiPlayerbot.AddClassAccountPoolSize and restart.", uint32(slot.cls),
                isAlliance ? "Alliance" : "Horde");
            return true; // nothing persisted yet (existing rows untouched)
        }
        ObjectGuid g = v.back(); v.pop_back();
        RaidRosterRow row;
        row.botGuid = g.GetCounter();
        row.cls = slot.cls; row.role = slot.role; row.specTab = slot.specTab; row.slot = i; row.band = slot.band;
        rows.push_back(row);
    }

    if (existing.empty())
    {
        RaidRosterStore::Replace(owner, rows);
        handler->PSendSysMessage("Created roster of {} bots (4 tank / 9 heal / 27 dps in every era: 4 Death Knight "
            "slots active at level {}+, 4 substitutes below). Use .raidroster login [5|10|25|40] then .raidroster sync.",
            (uint32)rows.size(), (uint32)kWotlkBandMinLevel);
    }
    else
    {
        RaidRosterStore::Append(owner, rows);         // no-op on empty
        RaidRosterStore::UpdateBands(owner, reband);  // no-op on empty
        handler->PSendSysMessage("Roster topped up: added {} slot(s), re-banded {} existing slot(s). "
            "Run .raidroster login to apply.", (uint32)rows.size(), (uint32)reband.size());
    }
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

    // Era gate: a slot is eligible only in its band for the MASTER's level (bots are levelled to
    // the master at sync, so this is the bots' era band too). Below kWotlkBandMinLevel the 4
    // Death Knight slots are benched and the 4 substitutes are eligible; at/above it the reverse.
    uint8 const masterLevel = master->GetLevel();
    bool const wotlkBand = masterLevel >= kWotlkBandMinLevel;
    auto eligible = [masterLevel](RaidRosterRow const& r) { return RaidCompEligible(r.band, masterLevel); };

    // Roster capacity per role (caps how many of each we can actually field) — eligible rows only.
    uint8 availT = 0, availH = 0, availD = 0;
    for (RaidRosterRow const& r : rows)
    {
        if (!eligible(r)) continue;
        if (r.role == 0) ++availT; else if (r.role == 1) ++availH; else ++availD;
    }

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
            if (r.role != role || !eligible(r)) continue;
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
        "{} online now, dismissed {}. Death Knights {} (Bots load async; re-check or see .raidroster status.)",
        size, roleName, (uint32)want.size(), (uint32)botT, (uint32)botH, (uint32)botD, online, dismissed,
        wotlkBand ? Acore::StringFormat("active (level {}+).", (uint32)kWotlkBandMinLevel)
                  : Acore::StringFormat("benched (below level {}; substitutes active).", (uint32)kWotlkBandMinLevel));
    return true;
}

// Force one bot to the master's level and the given talent spec (0-based tab), re-derive
// role strategies, then deterministically re-gear it for that spec (set-aware, enchanted,
// gemmed) via RaidRosterGear. Shared by full sync and syncone.
static void SyncBotToSpec(Player* master, Player* bot, int specTab)
{
    // 1) Level to master + learn spells/skills/glyphs/pet/consumables. Randomize also
    //    rolls random-spec gear and enchants — ALL of it is replaced in step 4/5; it's
    //    kept purely for the non-gear work.
    PlayerbotFactory factory(bot, master->GetLevel(), ITEM_QUALITY_LEGENDARY, 0);
    factory.Randomize(false);
    // 2) Force the target talent spec (0-based tab). A bot in a managed era band (levelled
    //    to the master in step 1, so BotEra == the master's band) gets an ERA build instead
    //    of the WotLK template; FactoryReconcile also tears down stale era rows on the way
    //    OUT of a band. Inert (returns false) unless EraTalents.BotTalents=1.
    if (!EraTalentBots::FactoryReconcile(bot, specTab))
        PlayerbotFactory::InitTalentsBySpecNo(bot, specTab, true);
    // 3) Re-derive tank/heal/dps strategies from the new spec.
    if (PlayerbotAI* ai = GET_PLAYERBOT_AI(bot)) ai->ResetStrategies(false);
    // 4) Deterministic set-aware re-gear for the forced spec (replaces the old
    //    factory.InitEquipment call, whose hardcoded 25% candidate skip produced
    //    role-wrong picks). MUST run after step 2: all scoring reads the active tab.
    RaidRosterGear::EquipForSpec(bot, master, specTab);
    // 5) Enchant + gem the final gear, and ammo so hunters shoot. Both operate on the
    //    CURRENT gear without replacing it. This step is the "no enchants" fix: any
    //    InitEquipment-style re-gear discards Randomize's enchants and nothing upstream
    //    re-applies them.
    factory.ApplyEnchantAndGemsNew();
    factory.InitAmmo();
    // 6) Unlock the Dungeon Finder for Death Knights. Core hard-gates every DK out of ALL
    //    LFG dungeons until it's been rewarded the DK-intro finale quest — 13188 "Where
    //    Kings Walk" (Alliance) or 13189 "The Warchief's Blessing" (Horde) — see
    //    LFGMgr.cpp (LFG_LOCKSTATUS_QUEST_NOT_COMPLETED). Roster bots are spawned via
    //    addclass and never run the intro chain, so a DK bot in your group locks the
    //    whole queue. SetRewardedQuest flips IsQuestRewarded immediately and flags it
    //    for the next SaveToDB.
    if (bot->IsClass(CLASS_DEATH_KNIGHT))
    {
        uint32 dkQuest = (bot->GetTeamId(true) == TEAM_ALLIANCE) ? 13188 : 13189;
        if (!bot->IsQuestRewarded(dkQuest))
            bot->SetRewardedQuest(dkQuest);
    }

    // 7) Match the bot's IP era to the master's so grouped raids run at the master's tier.
    RaidRosterEra::SyncBotToMaster(master, bot);
}

// Canonical spec tab for a class filling a role (0=tank, 1=heal, 2=dps), matching the
// RAID_COMP defaults. Returns -1 if the class cannot fill that role (e.g. mage tank).
static int SpecTabForRole(uint8 cls, uint8 role)
{
    switch (cls)
    {
        case CLASS_WARRIOR:
            if (role == 0) return WARRIOR_TAB_PROTECTION;
            if (role == 2) return WARRIOR_TAB_FURY;
            return -1;
        case CLASS_PALADIN:
            if (role == 0) return PALADIN_TAB_PROTECTION;
            if (role == 1) return PALADIN_TAB_HOLY;
            if (role == 2) return PALADIN_TAB_RETRIBUTION;
            return -1;
        case CLASS_DEATH_KNIGHT:
            if (role == 0) return DEATH_KNIGHT_TAB_BLOOD;
            if (role == 2) return DEATH_KNIGHT_TAB_UNHOLY;
            return -1;
        case CLASS_DRUID:
            if (role == 0) return DRUID_TAB_FERAL;
            if (role == 1) return DRUID_TAB_RESTORATION;
            if (role == 2) return DRUID_TAB_BALANCE;
            return -1;
        case CLASS_PRIEST:
            if (role == 1) return PRIEST_TAB_HOLY;
            if (role == 2) return PRIEST_TAB_SHADOW;
            return -1;
        case CLASS_SHAMAN:
            if (role == 1) return SHAMAN_TAB_RESTORATION;
            if (role == 2) return SHAMAN_TAB_ENHANCEMENT;
            return -1;
        case CLASS_MAGE:    if (role == 2) return MAGE_TAB_FROST;          return -1;
        case CLASS_WARLOCK: if (role == 2) return WARLOCK_TAB_AFFLICTION;  return -1;
        case CLASS_HUNTER:  if (role == 2) return HUNTER_TAB_MARKSMANSHIP; return -1;
        case CLASS_ROGUE:   if (role == 2) return ROGUE_TAB_COMBAT;        return -1;
    }
    return -1;
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

    uint32 synced = 0, offline = 0;
    for (RaidRosterRow const& r : rows)
    {
        ObjectGuid g = ObjectGuid::Create<HighGuid::Player>(r.botGuid);
        Player* bot = mgr->GetPlayerBot(g);
        if (!bot) { ++offline; continue; }

        SyncBotToSpec(master, bot, (int)r.specTab);
        ++synced;
    }

    handler->PSendSysMessage("Synced {} online bot(s) to your level/gear and roles; {} offline (login them first).",
        synced, offline);
    return true;
}

// Per-bot version of sync: force ONE roster bot to a role's canonical spec (or, with no role,
// back to its stored roster default). Deliberately does NOT rewrite the roster row, so a later
// full `.raidroster sync` reverts this bot (and all others) to the roster defaults.
bool RaidRosterCommand::HandleSyncOne(ChatHandler* handler, std::string name, Optional<std::string> roleArg)
{
    if (!g_RaidRosterEnable) { handler->SendSysMessage("RaidRoster is disabled (set RaidRoster.Enable=1)."); return true; }

    Player* master = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
    if (!master) { handler->SendSysMessage("Run this in-world as a player."); return true; }

    if (name.empty()) { handler->SendSysMessage("Usage: .raidroster syncone <botname> [tank|heal|dps]"); return true; }

    uint32 owner = master->GetGUID().GetCounter();
    std::vector<RaidRosterRow> rows = RaidRosterStore::Load(owner);
    if (rows.empty()) { handler->SendSysMessage("No roster. Use .raidroster create."); return true; }

    PlayerbotMgr* mgr = GET_PLAYERBOT_MGR(master);
    if (!mgr) { handler->SendSysMessage("Playerbot manager unavailable."); return true; }

    // Find the online roster bot by (case-insensitive) name.
    std::string wanted = name;
    std::transform(wanted.begin(), wanted.end(), wanted.begin(), ::tolower);
    Player* bot = nullptr;
    RaidRosterRow const* row = nullptr;
    for (RaidRosterRow const& r : rows)
    {
        ObjectGuid g = ObjectGuid::Create<HighGuid::Player>(r.botGuid);
        Player* b = mgr->GetPlayerBot(g);
        if (!b) continue;
        std::string bn = b->GetName();
        std::transform(bn.begin(), bn.end(), bn.begin(), ::tolower);
        if (bn == wanted) { bot = b; row = &r; break; }
    }
    if (!bot) { handler->PSendSysMessage("No online roster bot named '{}'. Log it in first (.raidroster login).", name); return true; }

    // Target spec: explicit role -> canonical spec for this class; else the bot's roster default.
    int specTab;
    if (roleArg)
    {
        std::string rs = *roleArg;
        std::transform(rs.begin(), rs.end(), rs.begin(), ::tolower);
        uint8 role;
        if (rs == "tank") role = 0;
        else if (rs == "heal" || rs == "healer") role = 1;
        else if (rs == "dps" || rs == "dd") role = 2;
        else { handler->SendSysMessage("Role must be tank, heal, or dps."); return true; }

        specTab = SpecTabForRole(bot->getClass(), role);
        if (specTab < 0) { handler->PSendSysMessage("{} cannot fill the {} role.", bot->GetName(), rs); return true; }
    }
    else
    {
        specTab = (int)row->specTab;   // reset just this bot to its roster default
    }

    SyncBotToSpec(master, bot, specTab);

    handler->PSendSysMessage("Synced {} to your level/gear as {}. (A full .raidroster sync reverts it to the roster default.)",
        bot->GetName(), roleArg ? *roleArg : std::string("its roster role"));
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

    uint8 const masterLevel = master->GetLevel();
    bool const wotlkBand = masterLevel >= kWotlkBandMinLevel;
    uint32 tanks = 0, heals = 0, dps = 0, benched = 0, online = 0, stale = 0, misbanded = 0;
    PlayerbotMgr* mgr = GET_PLAYERBOT_MGR(master);
    for (RaidRosterRow const& row : rows)
    {
        if (RaidCompEligible(row.band, masterLevel))
        { if (row.role == 0) ++tanks; else if (row.role == 1) ++heals; else ++dps; }
        else
            ++benched;
        if (row.slot < RAID_COMP.size() && row.band != RAID_COMP[row.slot].band) ++misbanded;
        ObjectGuid bg = ObjectGuid::Create<HighGuid::Player>(row.botGuid);
        if (mgr && mgr->GetPlayerBot(bg)) ++online;
        // Stale: character deleted or no longer in the addclass pool.
        if (!sCharacterCache->GetCharacterAccountIdByGuid(bg) ||
            !sRandomPlayerbotMgr.IsAddclassBot(row.botGuid))
            ++stale;
    }
    handler->PSendSysMessage("Roster: {} bots; eligible at level {}: {} ({} tank / {} heal / {} dps), benched: {} [{}]; {} online.",
        (uint32)rows.size(), (uint32)masterLevel, tanks + heals + dps, tanks, heals, dps, benched,
        wotlkBand ? "substitutes" : "Death Knights", online);
    uint32 const missingSlots = rows.size() < RAID_COMP.size() ? (uint32)(RAID_COMP.size() - rows.size()) : 0;
    if (missingSlots || misbanded)
        handler->PSendSysMessage("WARNING: roster predates the era substitutes ({} slot(s) missing, {} mis-banded). "
            "Run .raidroster create to top it up.", missingSlots, misbanded);
    if (stale)
        handler->PSendSysMessage("WARNING: {} roster slot(s) are stale (character deleted or left "
            "the addclass pool). Run .raidroster remove confirm then .raidroster create.", stale);
    return true;
}
