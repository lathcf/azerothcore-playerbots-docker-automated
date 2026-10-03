#include "RaidRosterEra.h"
#include "RaidRosterTierRules.h"
#include "IndividualProgression.h"   // pulls Player.h etc.; see RaidRosterEra.h for why this is isolated
#include "ObjectMgr.h"
#include "ItemTemplate.h"
#include "SpellMgr.h"
#include "Log.h"
#include <unordered_set>
#include <vector>

namespace
{
    // One class-book item: the LEARN_SPELL_ID trigger slot's taught spell, the classes it's
    // usable by, and the level it requires.
    struct ClassBook
    {
        uint32 item;
        uint32 spell;       // the taught spell (the LEARN_SPELL_ID trigger slot)
        uint32 classMask;   // AllowableClass & CLASSMASK_ALL_PLAYABLE
        uint32 reqLevel;
    };

    // Every class-book recipe in acore_world, built once from the item template store (world
    // thread only — sync is a chat command, never called off it). A "class book" is an
    // ITEM_CLASS_RECIPE / ITEM_SUBCLASS_BOOK item whose AllowableClass names a SINGLE class
    // (professions/misc books carry -1 / CLASSMASK_ALL_PLAYABLE and are excluded) that has a
    // LEARN_SPELL_ID trigger slot teaching a spell that actually exists in Spell.dbc (a handful
    // of stock books, e.g. Tablet of Grace of Air Totem III's spell 25359, are not). Under
    // mod-individual-progression these are book-only below level 61/71 — IP's own
    // class_trainers.sql re-inserts them on the trainer at ReqLevel 61/71, so playerbots'
    // trainer-driven InitAvailableSpells never teaches them to a Vanilla/TBC-band roster bot.
    std::vector<ClassBook> const& GetClassBooks()
    {
        static const std::vector<ClassBook> books = []()
        {
            std::vector<ClassBook> out;
            for (auto const& [entry, tmpl] : *sObjectMgr->GetItemTemplateStore())
            {
                if (tmpl.Class != ITEM_CLASS_RECIPE || tmpl.SubClass != ITEM_SUBCLASS_BOOK)
                    continue;

                uint32 taught = 0;
                for (auto const& s : tmpl.Spells)
                    if (s.SpellTrigger == ITEM_SPELLTRIGGER_LEARN_SPELL_ID && s.SpellId > 0)
                    {
                        taught = uint32(s.SpellId);
                        break;
                    }
                if (!taught)
                    continue;

                uint32 mask = uint32(tmpl.AllowableClass) & uint32(CLASSMASK_ALL_PLAYABLE);
                if (!mask || mask == uint32(CLASSMASK_ALL_PLAYABLE))
                    continue;   // profession/misc book (usable by every class) — not a class book

                if (!sSpellMgr->GetSpellInfo(taught))
                    continue;   // taught spell absent from this client's Spell.dbc

                out.push_back({entry, taught, mask, tmpl.RequiredLevel});
            }
            return out;
        }();
        return books;
    }

    // The IP ProgressionState tier a book unlocks at. Most books gate on RequiredLevel; a few
    // are pinned to a specific IP raid/zone gate regardless of level.
    uint8 BookTier(ClassBook const& b)
    {
        // AQ20 rank-upgrade books: reference-loot drops off Ruins of Ahn'Qiraj bosses (map 509),
        // gated the same as AQ20/AQ40 entry itself (IndividualProgressionPlayer.cpp:627-636).
        // Entry list derived by hand from a reference_loot_template query against acore_world.
        static const std::unordered_set<uint32> kAq20Books =
        {
            21214, 21279, 21280, 21281, 21282, 21283, 21284, 21285, 21287, 21288,
            21289, 21290, 21291, 21292, 21293, 21294, 21295, 21296, 21297, 21298,
            21299, 21300, 21302, 21303, 21304, 21306, 21307,
        };
        if (kAq20Books.count(b.item))
            return uint8(PROGRESSION_PRE_AQ);

        // Tome of Polymorph: Turtle drops from Gahz'ranka in Zul'Gurub — gated on IP's own
        // ZG progression config, not a ProgressionState.
        if (b.item == 22739)
            return uint8(sIndividualProgression->RequiredZulGurubProgression);

        // WotLK-added Polymorph tomes (Black Cat / Rabbit / Turkey) — Northrend-era drops.
        if (b.item == 44709 || b.item == 44793 || b.item == 44811)
            return uint8(PROGRESSION_TBC_TIER_5);

        // Everything else gates purely on the book's own RequiredLevel band.
        if (b.reqLevel <= 60)
            return 0;                              // Vanilla world/dungeon/MC/BWL/Onyxia drops
        if (b.reqLevel <= 70)
            return uint8(PROGRESSION_PRE_TBC);      // TBC world drops, Outland gate
        return uint8(PROGRESSION_TBC_TIER_5);       // WotLK
    }

    // Whether `bot`'s current IP progression clears `tier`. hasPassedProgression treats state 0
    // as "no requirement given" and always returns false for it, so tier 0 (Vanilla / always
    // available) is special-cased to always pass here. With IP disabled, only the level gate
    // (RequiredLevel, already checked by the caller) applies. hasPassedProgression also honours
    // IP's own ProgressionLimit, so a bot never gets ahead of that.
    bool TierUnlocked(Player* bot, uint8 tier)
    {
        if (tier == 0 || !sIndividualProgression->enabled)
            return true;
        return sIndividualProgression->hasPassedProgression(bot, static_cast<ProgressionState>(tier));
    }

    // Master's era: live IP state, or the level-band fallback when it reads unset (0) — a fresh
    // overlay character whose era has not been hand-set yet (<=60 Vanilla / 61-70 TBC / 71-80 WotLK).
    // Shared by SyncBotToMaster and MasterTier so the bot's era and the gear cap never disagree.
    uint8 MasterState(Player* master)
    {
        uint8 state = sIndividualProgression->GetPlayerProgressionFromQuests(master);
        if (state == 0)
        {
            uint8 lvl = master->GetLevel();
            state = (lvl >= 71) ? PROGRESSION_TBC_TIER_5   // 13 = WotLK entry
                  : (lvl >= 61) ? PROGRESSION_PRE_TBC      // 8  = TBC entry
                  :               PROGRESSION_START;        // 0  = Vanilla
        }
        return state;
    }
}

// Set a synced bot's IP era to match the master's, so the bot shares the master's content tier,
// difficulty scaling, and grouping eligibility. IP stores progression as rewarded hidden quests
// 66000+stage; both getter and setter require the unit to be in world (bots are, since
// .raidroster sync runs on logged-in bots). This file hard-depends on IP being in the build
// (setup.sh always clones it).
void RaidRosterEra::SyncBotToMaster(Player* master, Player* bot)
{
    if (!master->IsInWorld() || !bot->IsInWorld())
        return;
    // Source of truth = master's live era, with the level-band fallback (see MasterState).
    uint8 state = MasterState(master);
    if (state == 0)
    {
        // ForceUpdateProgressionState early-returns on stage 0 (landmine), so demote a
        // previously-advanced bot to Vanilla by clearing the hidden progression quests directly.
        // A freshly-created roster bot has none, so this is normally a no-op.
        for (uint32 i = 1; i <= 18; ++i)
            if (bot->IsQuestRewarded(66000 + i))
                bot->RemoveRewardedQuest(66000 + i);
    }
    else
    {
        sIndividualProgression->ForceUpdateProgressionState(bot, static_cast<ProgressionState>(state));
    }
}

uint8 RaidRosterEra::MasterTier(Player* master)
{
    if (!master || !sIndividualProgression->enabled)
        return RaidRosterTierRules::kTierUncapped;
    uint8 state = MasterState(master);
    if (sIndividualProgression->progressionLimit && state > sIndividualProgression->progressionLimit)
        state = uint8(sIndividualProgression->progressionLimit);
    return state;
}

// Teach every class-book spell the bot's class/level/IP-tier has unlocked. Book-only spells
// (see GetClassBooks) never reach a Vanilla/TBC-band bot via the trainer under IP, since IP's
// class_trainers.sql moves their trainer row to ReqLevel 61/71.
uint32 RaidRosterEra::LearnBookSpells(Player* bot)
{
    if (!bot || !bot->IsInWorld())
        return 0;

    uint32 classMask = bot->getClassMask();
    uint32 learned = 0;
    for (ClassBook const& b : GetClassBooks())
    {
        if (!(b.classMask & classMask))
            continue;
        if (bot->GetLevel() < b.reqLevel)
            continue;
        if (bot->HasSpell(b.spell))
            continue;
        if (!TierUnlocked(bot, BookTier(b)))
            continue;
        // Stock Strength of Earth Totem r5 (Tablet of Strength of Earth Totem V): mod-era-talents
        // rebuilds Vanilla shaman totems as clones (ranks 1-4 only) and its TBC arm strips this
        // stock rank (EraTalents.cpp ~1783, kStockShaSwapTbc), so an era-managed shaman must never
        // relearn it from the book. WotLK-band shamans still get it from the trainer at 61+.
        if (b.spell == 25361 && (bot->HasSpell(932416) || bot->HasSpell(947440)))
            continue;

        bot->learnSpell(b.spell);
        ++learned;
    }

    if (learned)
        LOG_INFO("playerbots", "[RaidRoster] {}: learned {} book spell(s) (IP tier {}).",
            bot->GetName(), learned, uint32(sIndividualProgression->GetPlayerProgressionFromQuests(bot)));

    return learned;
}
