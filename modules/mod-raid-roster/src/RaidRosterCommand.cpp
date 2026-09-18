#include "RaidRosterCommand.h"
#include "RaidRosterConfig.h"
#include "RaidRosterComp.h"
#include "RaidRosterStore.h"
#include "RaidRosterGear.h"
#include "RaidRosterLoginPlan.h"   // ComputeLoginPlan — the pure login arithmetic (tests/ covers it)
#include "Chat.h"
#include "StringFormat.h"   // Acore::StringFormat for the login era-band suffix
#include "CommandScript.h"
#include "RBAC.h"
#include "Player.h"
#include "Group.h"                 // Group::MemberSlot census + RemoveMember for stale offline seats
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
#include <array>
#include <utility>
#include <unordered_set>
#include <vector>
#include <map>
#include <unordered_map>
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

    PlayerbotMgr* mgr = GET_PLAYERBOT_MGR(master);
    if (!mgr) { handler->SendSysMessage("Playerbot manager unavailable."); return true; }
    uint32 acct = master->GetSession()->GetAccountId();

    std::unordered_map<uint32, RaidRosterRow const*> rowByGuid;   // group member -> roster row (if any)
    for (RaidRosterRow const& r : rows) rowByGuid.emplace(r.botGuid, &r);

    // Census of the master's current group — the master alone when ungrouped. Every ONLINE member
    // holds a seat: a roster bot under its pinned role, everyone else (the master with the override
    // applied, other humans, non-roster bots) by active spec. An OFFLINE non-roster member still
    // holds a seat we can't classify, so it counts as dps. An OFFLINE roster bot (a previous
    // session's raid whose bots logged out with the master) is NOT present: it becomes a
    // preferred candidate below (it needs no re-invite — OnBotLogin keeps a group the master is
    // in) and is removed from the group if the plan doesn't pick it
    // (unless an offline non-roster member is seated — see the unseat pass below).
    Group* grp = master->GetGroup();
    RoleCounts present{};
    uint32 offlineNonRoster = 0;                             // offline member we cannot classify
    std::vector<RaidRosterRow const*> rosterOnlineInGroup;    // surplus-trim candidates
    std::vector<RaidRosterRow const*> rosterOfflineInGroup;   // seat-holding candidates
    std::unordered_set<uint32> seatedRoster;                  // every roster guid with a group seat
    auto census = [&](ObjectGuid guid)
    {
        if (guid == master->GetGUID()) { ++present[playerRole]; return; }
        if (auto it = rowByGuid.find(guid.GetCounter()); it != rowByGuid.end())
        {
            seatedRoster.insert(guid.GetCounter());
            if (mgr->GetPlayerBot(guid)) { ++present[it->second->role]; rosterOnlineInGroup.push_back(it->second); }
            else                           rosterOfflineInGroup.push_back(it->second);
            return;
        }
        Player* p = ObjectAccessor::FindPlayer(guid);
        if (!p) { ++present[2]; ++offlineNonRoster; return; }
        ++present[PlayerbotAI::IsTank(p, true) ? 0 : (PlayerbotAI::IsHeal(p, true) ? 1 : 2)];
    };
    if (grp)
        for (Group::MemberSlot const& slot : grp->GetMemberSlots()) census(slot.guid);
    else
        census(master->GetGUID());

    // Offline pool per role — the only source of new logins. Seat-holding offline roster bots go
    // FIRST (no re-invite needed); the rest are shuffled so a fresh session brings a different
    // assortment. A roster bot that is online but NOT in the group can't be re-invited
    // (AddPlayerBot skips online bots and the fork only auto-invites at login), so it is dismissed
    // and left for a later login.
    std::array<std::vector<RaidRosterRow const*>, 3> pool;
    std::array<std::vector<RaidRosterRow const*>, 3> loose;   // eligible, offline, no seat
    std::vector<ObjectGuid> strayOnline;
    for (RaidRosterRow const& r : rows)
    {
        if (seatedRoster.count(r.botGuid)) continue;
        ObjectGuid g = ObjectGuid::Create<HighGuid::Player>(r.botGuid);
        if (mgr->GetPlayerBot(g)) { strayOnline.push_back(g); continue; }
        if (eligible(r)) loose[r.role].push_back(&r);
    }
    for (RaidRosterRow const* r : rosterOfflineInGroup)
        if (eligible(*r)) pool[r->role].push_back(r);
    RoleCounts avail{};
    for (uint8 role = 0; role < 3; ++role)
    {
        Acore::Containers::RandomShuffle(loose[role]);
        pool[role].insert(pool[role].end(), loose[role].begin(), loose[role].end());
        avail[role] = uint8(std::min<size_t>(pool[role].size(), 255));
    }

    LoginPlan const plan = ComputeLoginPlan(size, LoginTargetComp(sc->tanks, sc->heals, sc->dps), present, avail);

    // Seat-holding offline roster bots the plan didn't pick stop holding seats. Done BEFORE the
    // logins are queued (AddPlayerBot is async, so those bots join on later ticks regardless).
    // Group::RemoveMember disbands the group when, AFTER the removal, fewer than two ONLINE
    // members remain — NOT "one member left", and that is the NORMAL path here rather than an
    // edge case: a master re-entering last session's stale raid is its only online member, so the
    // FIRST removal disbands the group and drops every remaining offline seat at once. Accepted,
    // because the selected bots are logged in via AddPlayerBot either way and OnBotLogin forms a
    // fresh group around a master who has none. It is only safe because the whole pass is SKIPPED
    // when an offline NON-roster member holds a seat (`offlineNonRoster`), so a disband can never
    // drop an offline human. Because the disband frees the Group object, `grp` is re-read after
    // every removal and the loops stop once it is gone.
    uint32 unseated = 0;
    uint32 keptOffline = 0;
    bool disbanded = false;
    auto unseat = [&](uint32 botGuid) -> bool
    {
        if (!grp) return false;
        grp->RemoveMember(ObjectGuid::Create<HighGuid::Player>(botGuid));
        ++unseated;
        grp = master->GetGroup();   // Disband() may have destroyed the group we just called into
        disbanded = (grp == nullptr);
        return grp != nullptr;
    };
    if (offlineNonRoster)
    {
        // An offline non-roster member (a human between sessions) holds a seat: any removal can
        // trip the disband above and drop it. Keep every offline roster seat instead — the ones
        // the plan picked still log in normally, the rest simply stay seated until a later login.
        for (uint8 role = 0; role < 3; ++role)
            for (size_t i = plan.need[role]; i < pool[role].size(); ++i)
                if (seatedRoster.count(pool[role][i]->botGuid)) ++keptOffline;
        for (RaidRosterRow const* r : rosterOfflineInGroup)
            if (!eligible(*r)) ++keptOffline;   // ineligible seat-holders never entered the pool
    }
    else
    {
        bool groupAlive = grp != nullptr;
        for (uint8 role = 0; role < 3 && groupAlive; ++role)
            for (size_t i = plan.need[role]; i < pool[role].size() && groupAlive; ++i)
            {
                RaidRosterRow const* r = pool[role][i];
                if (!seatedRoster.count(r->botGuid)) continue;   // loose bots hold no seat
                groupAlive = unseat(r->botGuid);
            }
        // Ineligible seat-holders (wrong era band) never entered the pool; unseat them too.
        for (RaidRosterRow const* r : rosterOfflineInGroup)
        {
            if (!groupAlive) break;
            if (!eligible(*r)) groupAlive = unseat(r->botGuid);
        }
    }

    // Log in the missing roles. plan.need[role] <= pool[role].size() by construction (roster cap).
    uint32 added = 0;
    for (uint8 role = 0; role < 3; ++role)
        for (uint8 i = 0; i < plan.need[role]; ++i)
        {
            mgr->AddPlayerBot(ObjectGuid::Create<HighGuid::Player>(pool[role][i]->botGuid), acct);   // OnBotLogin joins the master's group; raid-converts only at >=5
            ++added;
        }

    // Dismiss: every stray online roster bot, then `surplus` roster bots FROM the group — dps
    // first, then heal, then tank (kLoginRoleOrder). Humans and non-roster bots are never removed;
    // whatever surplus they account for is reported, not fixed. LogoutPlayerBot queues the fork's
    // BotLogoutGroupCleanupOperation, which drops the bot from the group.
    uint32 dismissed = 0;
    for (ObjectGuid g : strayOnline) { mgr->LogoutPlayerBot(g); ++dismissed; }
    uint8 surplusLeft = plan.surplus;
    for (int role : kLoginRoleOrder)
        for (RaidRosterRow const* r : rosterOnlineInGroup)
        {
            if (!surplusLeft) break;
            if (r->role != role) continue;
            mgr->LogoutPlayerBot(ObjectGuid::Create<HighGuid::Player>(r->botGuid));
            ++dismissed; --surplusLeft;
        }

    char const* roleName = playerRole == 0 ? "TANK" : (playerRole == 1 ? "HEALER" : "DPS");
    std::string extra;
    if (plan.shortfall) extra += Acore::StringFormat("; short {} (roster exhausted)", (uint32)plan.shortfall);
    if (surplusLeft)    extra += Acore::StringFormat("; {} over size (non-roster members, not removed)", (uint32)surplusLeft);
    // A disband dropped every remaining offline seat, not just the one RemoveMember named; in
    // that path every non-master member was an offline roster bot (the skip above rules out a
    // non-roster offline member, and a second ONLINE member would have prevented the disband).
    if (disbanded)      extra += Acore::StringFormat("; stale group disbanded ({} offline seat(s) dropped)", (uint32)rosterOfflineInGroup.size());
    else if (unseated)  extra += Acore::StringFormat("; unseated {} offline bot(s)", unseated);
    if (keptOffline)    extra += Acore::StringFormat("; kept {} offline roster seat(s) (offline group member present)", keptOffline);
    handler->PSendSysMessage("Group {} — you as {}; {} present ({} tank / {} heal / {} dps); adding {} bots "
        "({} tank / {} heal / {} dps), dismissed {}{}. Death Knights {} (Bots load async; re-check or see .raidroster status.)",
        size, roleName, RoleSum(present), (uint32)present[0], (uint32)present[1], (uint32)present[2],
        added, (uint32)plan.need[0], (uint32)plan.need[1], (uint32)plan.need[2], dismissed, extra,
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
