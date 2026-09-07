#include "ArenaRosterCommand.h"
#include "ArenaRosterConfig.h"
#include "ArenaRosterComp.h"
#include "ArenaRosterDirector.h"
#include "ArenaRosterGear.h"
#include "ArenaRosterStore.h"
#include "Chat.h"
#include "RBAC.h"
#include "Player.h"
#include "ObjectAccessor.h"
#include "CharacterCache.h"
#include "PlayerbotMgr.h"
#include "Playerbots.h"
#include "RandomPlayerbotMgr.h"
#include "Mgr/Guild/PlayerbotGuildMgr.h"
#include "ArenaTeamMgr.h"
#include "ArenaTeam.h"
#include "DatabaseEnv.h"
#include "QueryResult.h"
#include "Field.h"
#include "PlayerbotAI.h"
#include "PlayerbotFactory.h"
#include "EraTalentBots.h"   // era-managed bots get era builds at sync (mod-era-talents)
#include "Tokenize.h"
#include <algorithm>
#include <cctype>
#include <unordered_set>

using namespace Acore::ChatCommands;

// NOTE: the top-level command word is "arenaroster", NOT "arena" as sketched in the plan.
// Core already ships a builtin `.arena` CommandScript (src/server/scripts/Commands/cs_arena.cpp,
// `arena_commandscript`) with its own `.arena create` sub-command (arena TEAM creation, a
// different signature). ScriptMgr::GetChatCommands() flattens every registered CommandScript's
// table into one vector and ChatCommandNode::LoadCommandsIntoMap folds same-named top-level
// commands into the SAME node/sub-map (map[name] find-or-create), so a second "create" handler
// under "arena" hits `ASSERT(!_invoker, "Duplicate blank sub-command.")` in
// ChatCommandNode::LoadFromBuilder (ChatCommand.cpp) — a live WPAssert in this build (not
// compiled out; only PERFORMANCE_PROFILING disables ASSERT), so it aborts the world thread the
// first time the lazily-built command map is touched (i.e. the first chat command run after
// startup) instead of a friendly in-game error. "arenaroster" avoids the whole class of
// collision and matches the sibling mod-raid-roster module's own top-level word (`.raidroster`,
// not `.raid`).
ChatCommandTable ArenaRosterCommand::GetCommands() const
{
    // SEC_PLAYER (a security level, not an RBAC perm) makes these usable by any logged-in
    // character — same rationale as mod-raid-roster's `.raidroster` — so a non-GM main can run
    // them. Console::Yes still allows the worldserver console (handled before that check).
    static ChatCommandTable sub =
    {
        { "create",     HandleCreate,     SEC_PLAYER, Console::Yes },
        { "go",         HandleGo,         SEC_PLAYER, Console::Yes },
        { "sync",       HandleSync,       SEC_PLAYER, Console::Yes },
        { "spec",       HandleSpec,       SEC_PLAYER, Console::Yes },
        { "logout",     HandleLogout,     SEC_PLAYER, Console::Yes },
        { "status",     HandleStatus,     SEC_PLAYER, Console::Yes },
        { "remove",     HandleRemove,     SEC_PLAYER, Console::Yes },
        { "poolinit",   HandlePoolInit,   SEC_PLAYER, Console::Yes },
        { "poolstatus", HandlePoolStatus, SEC_PLAYER, Console::Yes },
        { "forcequeue", HandleForceQueue, SEC_PLAYER, Console::Yes },
    };
    static ChatCommandTable root = { { "arenaroster", sub } };
    return root;
}

// Enable gate shared by EVERY handler (including the Task 5-8 stubs) — the single copy of the
// disabled message. Called FIRST, before touching any argument, so every code path
// (create/status/remove now, stubs today, real logic later) consistently reports the same
// disabled message. (A ScriptMgr-registered CommandScript's handler only runs when someone
// actually types the command, so there's no "hook fires even when disabled" trap here — that
// trap is for passive hooks like OnPlayerLootItem.)
static bool CheckEnabled(ChatHandler* handler)
{
    if (g_ArEnable)
        return true;
    handler->SendSysMessage("ArenaRoster is disabled (set ArenaRoster.Enable=1).");
    return false;
}

// Dual gate for per-player commands: enable check first, then require an in-world player.
static Player* RequireEnabledMaster(ChatHandler* handler)
{
    if (!CheckEnabled(handler)) return nullptr;
    Player* master = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
    if (!master) handler->SendSysMessage("Run this in-world as a player.");
    return master;
}

bool ArenaRosterCommand::HandleCreate(ChatHandler* handler)
{
    Player* master = RequireEnabledMaster(handler);
    if (!master) return true;

    uint32 owner = master->GetGUID().GetCounter();
    if (ArenaRosterStore::PartnersExist(owner))
    { handler->SendSysMessage("You already have arena partners. Use .arenaroster remove confirm first."); return true; }

    bool isAlliance = (master->GetTeamId(true) == TEAM_ALLIANCE);

    // Load every GUID already pinned (our own partner/pool tables) OR pinned by the sibling
    // raid-roster module, so we never double-pin a character (would hit uk_ar_bot or steal a
    // raid-roster bot out from under it).
    std::unordered_set<uint32> pinned = ArenaRosterStore::AllPinnedBots();
    if (QueryResult r = CharacterDatabase.Query("SELECT bot_guid FROM mod_raid_roster"))
        do { pinned.insert(r->Fetch()[0].Get<uint32>()); } while (r->NextRow());

    std::vector<ArenaPartnerRow> rows;
    rows.reserve(ARENA_PARTNER_DEFAULTS.size());
    for (ArenaPartnerDef const& def : ARENA_PARTNER_DEFAULTS)
    {
        uint8 key = RandomPlayerbotMgr::GetTeamClassIdx(isAlliance, def.cls);
        ObjectGuid chosen;
        for (ObjectGuid g : sRandomPlayerbotMgr.addclassCache[key])
        {
            if (ObjectAccessor::FindConnectedPlayer(g)) continue;                 // already online
            if (pinned.count(g.GetCounter())) continue;                          // already pinned somewhere
            ObjectGuid::LowType guildId = sCharacterCache->GetCharacterGuildIdByGuid(g);
            // Skip only REAL (player) guilds; playerbots auto-guilds bots into synthetic
            // bot-guilds, which addclass (PlayerbotMgr.cpp:1176) treats as available.
            if (guildId && PlayerbotGuildMgr::instance().IsRealGuild(guildId)) continue;
            chosen = g;
            break;
        }
        if (!chosen)
        {
            handler->PSendSysMessage("Not enough addclass characters of class {} ({}). "
                "Raise AiPlayerbot.AddClassAccountPoolSize and restart.", uint32(def.cls),
                isAlliance ? "Alliance" : "Horde");
            return true;  // nothing persisted yet
        }
        pinned.insert(chosen.GetCounter());
        rows.push_back({chosen.GetCounter(), def.cls, def.specTab});
    }

    ArenaRosterStore::ReplacePartners(owner, rows);
    handler->PSendSysMessage("Pinned {} arena partners (one per class). "
        "Use .arenaroster go <2v2|3v3|5v5> <class...> then .arenaroster sync.", (uint32)rows.size());
    return true;
}

bool ArenaRosterCommand::HandleStatus(ChatHandler* handler)
{
    Player* master = RequireEnabledMaster(handler);
    if (!master) return true;

    std::vector<ArenaPartnerRow> rows = ArenaRosterStore::LoadPartners(master->GetGUID().GetCounter());
    if (rows.empty()) { handler->SendSysMessage("No partners. Run .arenaroster create."); return true; }

    for (ArenaPartnerRow const& row : rows)
    {
        ObjectGuid g = ObjectGuid::Create<HighGuid::Player>(row.botGuid);
        Player* bot = ObjectAccessor::FindConnectedPlayer(g);
        std::string name;
        sCharacterCache->GetCharacterNameByGuid(g, name);
        handler->PSendSysMessage("class {}: {} [{}] spec-tab {}", uint32(row.cls), name,
                                 bot ? "online" : "offline", uint32(row.specTab));
    }
    for (uint8 slot = 0; slot < 3; ++slot)
        if (uint32 teamId = master->GetArenaTeamId(slot))
            if (ArenaTeam* team = sArenaTeamMgr->GetArenaTeamById(teamId))
            {
                uint32 type = team->GetType();   // bracket size: NvN
                handler->PSendSysMessage("{}v{}: <{}> rating {} -> opponent tier {}",
                    type, type, team->GetName(), team->GetRating(),
                    uint32(ArenaRosterTierFor(team->GetRating())) + 1);
            }
    return true;
}

bool ArenaRosterCommand::HandleRemove(ChatHandler* handler, Optional<std::string> confirm)
{
    Player* master = RequireEnabledMaster(handler);
    if (!master) return true;
    if (!confirm || *confirm != "confirm")
    { handler->SendSysMessage("This unpins all your partners. Run .arenaroster remove confirm."); return true; }

    // Clean arena-team membership BEFORE dropping the rows: unpinned partners would otherwise
    // stay members of the owner's teams — after remove+create the old bots are "strangers"
    // HandleGo won't evict (they aren't in the new partners table, wedging the roster cap), and
    // a freed bot returning to the addclass candidate set can fail a later poolinit's AddMember
    // ("already in a team of this bracket"). DelMember(guid, true) is offline-safe — the same
    // call HandleGo's Phase 2 uses.
    std::vector<ArenaPartnerRow> partners = ArenaRosterStore::LoadPartners(master->GetGUID().GetCounter());
    uint32 cleaned = 0;
    for (ArenaPartnerRow const& p : partners)
    {
        ObjectGuid g = ObjectGuid::Create<HighGuid::Player>(p.botGuid);
        for (uint8 slot = 0; slot < 3; ++slot)
            if (uint32 teamId = master->GetArenaTeamId(slot))
                if (ArenaTeam* team = sArenaTeamMgr->GetArenaTeamById(teamId))
                    if (team->IsMember(g))
                    { team->DelMember(g, true); ++cleaned; }
    }

    ArenaRosterStore::ClearPartners(master->GetGUID().GetCounter());
    handler->PSendSysMessage("Partners unpinned (characters return to the addclass pool); "
        "{} arena-team membership(s) cleaned.", cleaned);
    return true;
}

// --- Shared helpers for the per-outing handlers below. ---

// Class-name token -> CLASS_* id (accepts a few common short forms). 0 = unrecognized.
static uint8 ClassFromToken(std::string tok)
{
    // Plain ::tolower is UB for negative char values (and overload-ambiguous in a transform);
    // the unsigned-char lambda is the well-defined idiom.
    std::transform(tok.begin(), tok.end(), tok.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (tok == "warrior" || tok == "warr") return CLASS_WARRIOR;
    if (tok == "paladin" || tok == "pala") return CLASS_PALADIN;
    if (tok == "hunter" || tok == "hunt") return CLASS_HUNTER;
    if (tok == "rogue") return CLASS_ROGUE;
    if (tok == "priest") return CLASS_PRIEST;
    if (tok == "dk" || tok == "deathknight") return CLASS_DEATH_KNIGHT;
    if (tok == "shaman" || tok == "sham") return CLASS_SHAMAN;
    if (tok == "mage") return CLASS_MAGE;
    if (tok == "warlock" || tok == "lock") return CLASS_WARLOCK;
    if (tok == "druid") return CLASS_DRUID;
    return 0;
}

// Bot name for player-facing messages (offline-safe via the character cache).
static std::string BotNameOf(uint32 lowGuid)
{
    std::string name = "<unknown>";
    sCharacterCache->GetCharacterNameByGuid(ObjectGuid::Create<HighGuid::Player>(lowGuid), name);
    return name;
}

bool ArenaRosterCommand::HandleGo(ChatHandler* handler, std::string bracket, Tail classes)
{
    Player* master = RequireEnabledMaster(handler);
    if (!master) return true;

    uint8 type = 0, slot = 0;
    if (bracket == "2v2")      { type = ARENA_TEAM_2v2; slot = ARENA_SLOT_2v2; }
    else if (bracket == "3v3") { type = ARENA_TEAM_3v3; slot = ARENA_SLOT_3v3; }
    else if (bracket == "5v5") { type = ARENA_TEAM_5v5; slot = ARENA_SLOT_5v5; }
    else { handler->SendSysMessage("Usage: .arenaroster go <2v2|3v3|5v5> <class> [class...]"); return true; }

    std::vector<ArenaPartnerRow> partners = ArenaRosterStore::LoadPartners(master->GetGUID().GetCounter());
    if (partners.empty()) { handler->SendSysMessage("No partners. Run .arenaroster create first."); return true; }

    // Resolve requested class tokens -> partner rows (distinct classes, exactly type-1 of them).
    // Tail is a std::string_view, so Tokenize consumes it directly.
    std::vector<ArenaPartnerRow> picked;
    std::unordered_set<uint8> seen;
    for (std::string_view tokView : Acore::Tokenize(classes, ' ', false))
    {
        uint8 cls = ClassFromToken(std::string(tokView));
        if (!cls)
        { handler->PSendSysMessage("Unknown class '{}'. Use warrior/paladin/hunter/rogue/priest/dk/shaman/mage/warlock/druid.", std::string(tokView)); return true; }
        if (!seen.insert(cls).second) continue;   // duplicate token: keep the first
        bool found = false;
        for (ArenaPartnerRow const& p : partners)
            if (p.cls == cls) { picked.push_back(p); found = true; break; }
        if (!found)
        { handler->PSendSysMessage("You have no pinned partner of class '{}'. See .arenaroster status.", std::string(tokView)); return true; }
    }
    if (picked.size() != size_t(type) - 1)
    { handler->PSendSysMessage("Need exactly {} distinct partner classes for {}.", uint32(type) - 1, bracket); return true; }

    PlayerbotMgr* mgr = GET_PLAYERBOT_MGR(master);
    if (!mgr) { handler->SendSysMessage("Playerbot manager unavailable."); return true; }

    // --- Phase 1: ensure the player's arena team for this bracket exists. ---
    // ArenaTeam::Create's only preconditions are captain-online (he is: he's running the command)
    // and a name-taken check — but that core check compares the still-empty member TeamName, not
    // the argument, so it never actually fires (ArenaTeam.cpp:57). Our GetArenaTeamByName probe
    // below is therefore the REAL duplicate-name guard: append a numeral until the name is free.
    uint32 teamId = master->GetArenaTeamId(slot);
    ArenaTeam* team = teamId ? sArenaTeamMgr->GetArenaTeamById(teamId) : nullptr;
    if (!team)
    {
        std::string base = master->GetName() + std::string("'s ") + bracket;
        std::string name = base;
        for (uint32 n = 2; sArenaTeamMgr->GetArenaTeamByName(name); ++n)
            name = base + " " + std::to_string(n);

        team = new ArenaTeam();
        // Create also AddMembers the captain (us) — safe: we just verified this slot is free.
        if (!team->Create(master->GetGUID(), type, name, 0, 0, 0, 0, 0))
        { delete team; handler->SendSysMessage("Failed to create your arena team."); return true; }
        sArenaTeamMgr->AddArenaTeam(team);
        handler->PSendSysMessage("Created arena team <{}>.", name);
    }

    // --- Phase 2: remove-then-add (simplest provably-correct order). ---
    // ArenaTeam::AddMember itself enforces the roster cap (GetMembersSize() >= GetType() * 2,
    // ArenaTeam.cpp:98), so we don't count slots ourselves: first DelMember every CURRENT member
    // that is one of OUR pinned partner bots but not picked for this outing, then AddMember the
    // picked ones and treat any AddMember failure as a player-facing error. Removal touches ONLY
    // guids present in our partners table — never the captain/master and never a stranger (another
    // real player's character stays put even if it somehow shares the team).
    std::unordered_set<uint32> partnerGuids, pickedGuids;
    for (ArenaPartnerRow const& p : partners) partnerGuids.insert(p.botGuid);
    for (ArenaPartnerRow const& p : picked)   pickedGuids.insert(p.botGuid);

    std::vector<ObjectGuid> toRemove;   // collect first: DelMember erases from the list we iterate
    for (ArenaTeamMember const& m : team->GetMembers())
    {
        if (m.Guid == master->GetGUID() || m.Guid == team->GetCaptain()) continue;
        if (!partnerGuids.count(m.Guid.GetCounter())) continue;   // not ours: never kick strangers
        if (pickedGuids.count(m.Guid.GetCounter())) continue;     // staying for this outing
        toRemove.push_back(m.Guid);
    }
    for (ObjectGuid g : toRemove)
        team->DelMember(g, true);

    // --- Phase 3: add picked partners not already on the team. AddMember works for offline
    // characters via the character cache; it fails when the roster is full or when the bot is
    // already in ANOTHER team of this bracket (both reported, not silently skipped). ---
    std::vector<ArenaPartnerRow> ready;   // membership confirmed -> safe to log in
    for (ArenaPartnerRow const& p : picked)
    {
        ObjectGuid g = ObjectGuid::Create<HighGuid::Player>(p.botGuid);
        if (team->IsMember(g) || team->AddMember(g))
        { ready.push_back(p); continue; }
        handler->PSendSysMessage("Couldn't add {} to <{}> (roster full, or already in another {} team).",
            BotNameOf(p.botGuid), team->GetName(), bracket);
    }
    // (No SaveToDB needed: AddMember inserts and DelMember(guid, true) deletes its own
    // arena_team_member row synchronously.)

    if (ready.empty())
    {
        handler->PSendSysMessage("No partners could join <{}> — nothing to log in. "
            "Fix the errors above (roster full / conflicting {} teams) and re-run .arenaroster go.",
            team->GetName(), bracket);
        return true;
    }

    // --- Phase 4: log the ready partners in under the player's account (raid-roster pattern:
    // AddPlayerBot is async; OnBotLogin queues a group invite — <=4 bots + you stays a PARTY,
    // PlayerbotMgr.cpp:554-577 only raid-converts at >=5 members). ---
    uint32 loggingIn = 0;
    for (ArenaPartnerRow const& p : ready)
    {
        ObjectGuid g = ObjectGuid::Create<HighGuid::Player>(p.botGuid);
        if (!mgr->GetPlayerBot(g))
        { mgr->AddPlayerBot(g, master->GetSession()->GetAccountId()); ++loggingIn; }
    }
    handler->PSendSysMessage("{} crew forming: {} partner(s) on <{}> ({} logging in and joining your group). "
        "Run .arenaroster sync, then queue rated {} at any battlemaster.",
        bracket, uint32(ready.size()), team->GetName(), loggingIn, bracket);

    // Bracket-switch leftovers: partners still online from a previous outing that this one
    // didn't pick. If they're still in the party, the queue sees a wrong-sized group.
    uint32 leftovers = 0;
    for (ArenaPartnerRow const& p : partners)
        if (!pickedGuids.count(p.botGuid) && mgr->GetPlayerBot(ObjectGuid::Create<HighGuid::Player>(p.botGuid)))
            ++leftovers;
    if (leftovers)
        handler->PSendSysMessage("Note: {} unpicked partner(s) still online from a previous outing — "
            "kick them from the party (or .arenaroster logout and re-run go) before queueing.", leftovers);
    return true;
}

bool ArenaRosterCommand::HandleSync(ChatHandler* handler)
{
    Player* master = RequireEnabledMaster(handler);
    if (!master) return true;

    std::vector<ArenaPartnerRow> partners = ArenaRosterStore::LoadPartners(master->GetGUID().GetCounter());
    if (partners.empty()) { handler->SendSysMessage("No partners. Run .arenaroster create first."); return true; }

    PlayerbotMgr* mgr = GET_PLAYERBOT_MGR(master);
    if (!mgr) { handler->SendSysMessage("Playerbot manager unavailable."); return true; }

    ArenaSeason season = ArenaRosterGear::SeasonForItemLevel(master->GetAverageItemLevel());

    uint32 synced = 0, offline = 0, undergeared = 0;
    for (ArenaPartnerRow const& p : partners)
    {
        Player* bot = mgr->GetPlayerBot(ObjectGuid::Create<HighGuid::Player>(p.botGuid));
        if (!bot) { ++offline; continue; }   // only online partners sync

        // Mirror mod-raid-roster's proven sync sequence (RaidRosterCommand.cpp SyncBotToSpec):
        // 1) Randomize(false) levels the bot to the master AND (re)learns spells/skills — a bare
        //    GiveLevel would leave a freshly-leveled bot without its trainer spells. Its random
        //    PvE gearing is deliberately thrown away by EquipSeason below.
        PlayerbotFactory factory(bot, master->GetLevel(), ITEM_QUALITY_EPIC, 0);
        factory.Randomize(false);
        // 2) Force the pinned PvP spec (0-based tab), then re-derive strategies from it.
        //    Era-managed bots get an era build (see RaidRosterCommand SyncBotToSpec step 2).
        if (!EraTalentBots::FactoryReconcile(bot, p.specTab))
            PlayerbotFactory::InitTalentsBySpecNo(bot, p.specTab, true);
        if (PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot))
            botAI->ResetStrategies(false);
        // 3) Season PvP set replaces the factory gearing. AFTER the talent force on purpose:
        //    EquipSeason's weapon ranking weights by the bot's active talent tab.
        if (!ArenaRosterGear::EquipSeason(bot, p.specTab, season))
            ++undergeared;
        ++synced;
    }

    handler->PSendSysMessage("Synced {} partner(s): level {}, {} gear (season tier {}). {} offline "
        "(only online partners sync — .arenaroster go logs them in).",
        synced, master->GetLevel(), ArenaRosterGear::SeasonPrefix(season), uint32(season), offline);
    if (undergeared)
        handler->PSendSysMessage("WARNING: {} partner(s) got fewer than 8 set pieces "
            "(below level 80 or no usable set items) — check the worldserver log.", undergeared);
    return true;
}

// Spec-name token -> 0-based talent tab for a class. Accepts 0|1|2 and per-class names
// (with the common short forms). -1 = unrecognized.
static int SpecTabFromToken(uint8 cls, std::string spec)
{
    std::transform(spec.begin(), spec.end(), spec.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (spec.size() == 1 && spec[0] >= '0' && spec[0] <= '2')
        return spec[0] - '0';
    switch (cls)
    {
        case CLASS_WARRIOR:
            if (spec == "arms") return WARRIOR_TAB_ARMS;
            if (spec == "fury") return WARRIOR_TAB_FURY;
            if (spec == "prot" || spec == "protection") return WARRIOR_TAB_PROTECTION;
            break;
        case CLASS_PALADIN:
            if (spec == "holy") return PALADIN_TAB_HOLY;
            if (spec == "prot" || spec == "protection") return PALADIN_TAB_PROTECTION;
            if (spec == "ret" || spec == "retribution") return PALADIN_TAB_RETRIBUTION;
            break;
        case CLASS_HUNTER:
            if (spec == "bm" || spec == "beastmastery") return HUNTER_TAB_BEAST_MASTERY;
            if (spec == "mm" || spec == "marksmanship") return HUNTER_TAB_MARKSMANSHIP;
            if (spec == "survival" || spec == "surv") return HUNTER_TAB_SURVIVAL;
            break;
        case CLASS_ROGUE:
            if (spec == "assassination" || spec == "assa" || spec == "mut") return ROGUE_TAB_ASSASSINATION;
            if (spec == "combat") return ROGUE_TAB_COMBAT;
            if (spec == "sub" || spec == "subtlety") return ROGUE_TAB_SUBTLETY;
            break;
        case CLASS_PRIEST:
            if (spec == "disc" || spec == "discipline") return PRIEST_TAB_DISCIPLINE;
            if (spec == "holy") return PRIEST_TAB_HOLY;
            if (spec == "shadow") return PRIEST_TAB_SHADOW;
            break;
        case CLASS_DEATH_KNIGHT:
            if (spec == "blood") return DEATH_KNIGHT_TAB_BLOOD;
            if (spec == "frost") return DEATH_KNIGHT_TAB_FROST;
            if (spec == "unholy") return DEATH_KNIGHT_TAB_UNHOLY;
            break;
        case CLASS_SHAMAN:
            if (spec == "ele" || spec == "elemental") return SHAMAN_TAB_ELEMENTAL;
            if (spec == "enh" || spec == "enhancement") return SHAMAN_TAB_ENHANCEMENT;
            if (spec == "resto" || spec == "restoration") return SHAMAN_TAB_RESTORATION;
            break;
        case CLASS_MAGE:
            if (spec == "arcane") return MAGE_TAB_ARCANE;
            if (spec == "fire") return MAGE_TAB_FIRE;
            if (spec == "frost") return MAGE_TAB_FROST;
            break;
        case CLASS_WARLOCK:
            if (spec == "affliction" || spec == "affli") return WARLOCK_TAB_AFFLICTION;
            if (spec == "demo" || spec == "demonology") return WARLOCK_TAB_DEMONOLOGY;
            if (spec == "destro" || spec == "destruction") return WARLOCK_TAB_DESTRUCTION;
            break;
        case CLASS_DRUID:
            if (spec == "balance" || spec == "boomkin") return DRUID_TAB_BALANCE;
            if (spec == "feral") return DRUID_TAB_FERAL;
            if (spec == "resto" || spec == "restoration") return DRUID_TAB_RESTORATION;
            break;
    }
    return -1;
}

bool ArenaRosterCommand::HandleSpec(ChatHandler* handler, std::string className, std::string specName)
{
    Player* master = RequireEnabledMaster(handler);
    if (!master) return true;

    uint8 cls = ClassFromToken(className);
    if (!cls)
    {
        handler->PSendSysMessage("Unknown class '{}'. Use warrior/paladin/hunter/rogue/priest/dk/"
            "shaman/mage/warlock/druid.", className);
        return true;
    }

    int specTab = SpecTabFromToken(cls, specName);
    if (specTab < 0)
    {
        handler->PSendSysMessage("Unknown spec '{}' for {}. Use a talent tab 0|1|2 or its name "
            "(e.g. warrior arms/fury/prot, priest disc/holy/shadow, shaman ele/enh/resto).",
            specName, className);
        return true;
    }

    std::vector<ArenaPartnerRow> partners = ArenaRosterStore::LoadPartners(master->GetGUID().GetCounter());
    ArenaPartnerRow const* row = nullptr;
    for (ArenaPartnerRow const& p : partners)
        if (p.cls == cls) { row = &p; break; }
    if (!row)
    {
        handler->PSendSysMessage("You have no pinned partner of class '{}'. Run .arenaroster create "
            "(or see .arenaroster status).", className);
        return true;
    }

    ArenaRosterStore::SetPartnerSpec(master->GetGUID().GetCounter(), cls, uint8(specTab));
    handler->PSendSysMessage("{} ({}) now runs talent tab {} ('{}'). Run .arenaroster sync to apply "
        "talents and re-gear for it.", BotNameOf(row->botGuid), className, specTab, specName);
    return true;
}

bool ArenaRosterCommand::HandleLogout(ChatHandler* handler)
{
    Player* master = RequireEnabledMaster(handler);
    if (!master) return true;

    std::vector<ArenaPartnerRow> partners = ArenaRosterStore::LoadPartners(master->GetGUID().GetCounter());
    if (partners.empty()) { handler->SendSysMessage("No partners pinned; nothing to dismiss."); return true; }

    PlayerbotMgr* mgr = GET_PLAYERBOT_MGR(master);
    if (!mgr) { handler->SendSysMessage("Playerbot manager unavailable."); return true; }

    // Log out every currently-online partner bot (team membership is untouched — the next
    // .arenaroster go reuses it).
    uint32 dismissed = 0;
    for (ArenaPartnerRow const& p : partners)
    {
        ObjectGuid g = ObjectGuid::Create<HighGuid::Player>(p.botGuid);
        if (mgr->GetPlayerBot(g)) { mgr->LogoutPlayerBot(g); ++dismissed; }
    }
    handler->PSendSysMessage("Dismissed {} arena partner(s).", dismissed);
    return true;
}

// --- Opponent-pool commands: thin wrappers over the ArenaRosterDirector state machine (pool
// creation must run across world ticks — logins are async and ArenaTeam::Create needs the
// captain online). Server-wide (no per-player roster), so only the enable gate applies —
// no RequireEnabledMaster/in-world-player check. ---

bool ArenaRosterCommand::HandlePoolInit(ChatHandler* handler)
{
    if (!CheckEnabled(handler)) return true;
    std::string err;
    if (!ArenaRosterDirector::instance)
        handler->SendSysMessage("poolinit not started: director not loaded.");
    else if (!ArenaRosterDirector::instance->StartPoolInit(err))
        handler->PSendSysMessage("poolinit not started: {}", err);
    else
        handler->SendSysMessage("poolinit started - watch .arenaroster poolstatus.");
    return true;
}

bool ArenaRosterCommand::HandlePoolStatus(ChatHandler* handler)
{
    if (!CheckEnabled(handler)) return true;
    if (!ArenaRosterDirector::instance)
        handler->SendSysMessage("director not loaded.");
    else
        handler->PSendSysMessage("{}", ArenaRosterDirector::instance->PoolStatusText());
    return true;
}

// TEST-ONLY: `.arenaroster forcequeue <tier 1-4> [2|3|5]` fields a tier lineup without a real
// player queueing — the engagement queues rated and (with nobody to fight) aborts at the
// Queueing timeout, exercising serve -> group -> queue -> abort -> drain end to end.
bool ArenaRosterCommand::HandleForceQueue(ChatHandler* handler, uint8 tier, Optional<uint8> type)
{
    if (!CheckEnabled(handler)) return true;
    if (!ArenaRosterDirector::instance)
    {
        handler->SendSysMessage("forcequeue rejected: director not loaded.");
        return true;
    }
    std::string err;
    uint8 arenaType = type.value_or(2);
    if (!ArenaRosterDirector::instance->ForceQueue(tier, arenaType, err))
        handler->PSendSysMessage("forcequeue rejected: {}", err);
    else
        handler->PSendSysMessage("TEST-ONLY: serving tier {} {}v{} opponents — they log in, group and "
            "queue rated (worldserver log tracks the engagement; without a real opponent it aborts "
            "and drains after the queue timeout).", tier, arenaType, arenaType);
    return true;
}
