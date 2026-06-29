#include "PBChatterAmbientPrompt.h"
#include "PBChatterAmbient.h"   // for AMB_* kind constants
#include "PBChatterContext.h"
#include "Player.h"
#include "Random.h"
#include "StringFormat.h"
#include <string>
#include <vector>

namespace
{
    char const* ChannelWord(uint8_t kind)
    {
        switch (kind)
        {
            case AMB_ZONE:  return "the zone's General channel";
            case AMB_GROUP: return "your party/raid chat";
            case AMB_GUILD: return "guild chat";
            default:        return "chat";
        }
    }

    // Authentic WotLK player-chat lines, used as few-shot STYLE anchors only (the model is told
    // to match the tone/length, not the content). Deliberately varied — statements, LFG, AH,
    // gripes, congrats, plus dry/sarcastic, teasing, and goofy lines — so a small model doesn't
    // collapse into one rut (e.g. opening every line with "anyone ...?") AND so the edge in the
    // system prompt has concrete patterns to imitate. A couple stay warm/wholesome on purpose so
    // the blend reads mostly chill, not all-snark.
    char const* const kExamples[] = {
        "ugh wiped on the last boss in heroic HoL again, healer went oom",
        "wts [Titansteel Bar] x5, pst with an offer",
        "lf1m heroic gundrak, need a tank then we're good",
        "gz on the mount drop man, so jealous",
        "saronite prices on the AH are nuts right now",
        "grizzly hills has the best zone music in the game imo",
        "finally hit 80, what a grind lol",
        "brb gotta fly back and repair, totally broke",
        "ty for the summon",
        "this sons of hodir rep grind is killing me",
        "tank dc'd mid-pull, classic",
        "man the cooking daily in dalaran takes forever",
        "oh good, another escort quest, hope the npc walks extra slow this time",
        "loot table really said no again, cool cool cool",
        "0 for 40 on this drop, the rng hates me specifically",
        "respecced, hated it, respecced back, there goes my gold",
        "gg tank, bold pull, real bold",
        "nice of the healer to finally show up lol",
        "pretty sure my hearthstone cooldown is shorter than my attention span",
        "i would lose a 1v1 to a single murloc right now ngl",
    };

    // Topics valid at any level — variety that's never level-inappropriate.
    char const* const kTopicsAny[] = {
        "where you are right now or a quest you're working on",
        "your class or spec — a talent choice, a new ability, how it plays",
        "a profession you're leveling or some mats you need",
        "gold, the auction house, or saving up for something",
        "an alt you're leveling or thinking about rolling",
        "a run of bad luck — a drop that won't drop, a tough quest, repair bills",
        "looking for a group or a couple more for a dungeon your level",
    };

    // Content topics GATED to the bot's level band, so a level-30 never gets handed a raid topic
    // (the main cause of level-30 bots chatting about Naxx). Each band only references content
    // that bracket actually plays.
    char const* const kTopics_1_19[] = {
        "a low-level dungeon like the Deadmines, Wailing Caverns, or Shadowfang Keep",
        "looking forward to your first mount at level 20",   // WotLK: Apprentice Riding @20, cheap
        "where to quest next as you level up",
        "a tough elite or group quest you could use a hand with",
    };
    char const* const kTopics_20_39[] = {
        "a dungeon like Scarlet Monastery, Razorfen Kraul, or Uldaman",
        "a profession you're skilling up as you level",
        "which zone to level in next",
        "a nasty elite or escort quest",
    };
    char const* const kTopics_40_59[] = {
        "a dungeon like Zul'Farrak, Maraudon, or the Sunken Temple",
        "gear or gold you're picking up while questing",
        "the high-level dungeons (Blackrock Depths, Stratholme, Scholomance)",
        "thinking about heading to Outland soon",
    };
    char const* const kTopics_60_69[] = {
        "leveling through Outland — Hellfire Peninsula or Zangarmarsh",
        "an Outland dungeon like Hellfire Ramparts or the Blood Furnace",
        "pushing through Outland toward Northrend",
    };
    char const* const kTopics_70_79[] = {
        "leveling through Northrend and which zone is next",
        "a Northrend dungeon like Utgarde Keep, The Nexus, or Gundrak",
        "pushing the last stretch to level 80",
        "gear and rep on the way up",
    };
    char const* const kTopics_80[] = {
        "a heroic dungeon run and the emblems from it",
        "a raid you're working on (Naxxramas, Ulduar, Trial of the Crusader, or ICC)",
        "a gear upgrade or a drop you're chasing",
        "daily quests or a rep grind (Sons of Hodir, the Argent Crusade)",
        "PvP — Wintergrasp, a battleground, or arena",
    };

    // A few example lines as a style block. Picks a random run so it varies call to call.
    std::string Examples(int n)
    {
        int const total = (int)(sizeof(kExamples) / sizeof(kExamples[0]));
        if (n > total) n = total;
        int start = urand(0, total - 1);
        std::string out = "\nMatch this tone and length (don't reuse the content):";
        for (int i = 0; i < n; ++i)
            out += Acore::StringFormat("\n- {}", kExamples[(start + i) % total]);
        return out;
    }

    // Pick a topic appropriate to the bot's level: ~half the time a universal topic, otherwise a
    // level-banded content topic.
    char const* PickTopic(uint8_t level)
    {
        char const* const* band; int bandN;
        if      (level <= 19) { band = kTopics_1_19;  bandN = (int)(sizeof(kTopics_1_19)  / sizeof(*kTopics_1_19)); }
        else if (level <= 39) { band = kTopics_20_39; bandN = (int)(sizeof(kTopics_20_39) / sizeof(*kTopics_20_39)); }
        else if (level <= 59) { band = kTopics_40_59; bandN = (int)(sizeof(kTopics_40_59) / sizeof(*kTopics_40_59)); }
        else if (level <= 69) { band = kTopics_60_69; bandN = (int)(sizeof(kTopics_60_69) / sizeof(*kTopics_60_69)); }
        else if (level <= 79) { band = kTopics_70_79; bandN = (int)(sizeof(kTopics_70_79) / sizeof(*kTopics_70_79)); }
        else                  { band = kTopics_80;    bandN = (int)(sizeof(kTopics_80)    / sizeof(*kTopics_80)); }

        int const anyN = (int)(sizeof(kTopicsAny) / sizeof(*kTopicsAny));
        if (urand(0, 1) == 0)
            return kTopicsAny[urand(0, anyN - 1)];
        return band[urand(0, bandN - 1)];
    }

    // Shared tail: every ambient prompt asks for exactly one short line and forbids meta-talk.
    std::string Tail()
    {
        return "\n\nWrite exactly ONE short chat line and nothing else. Don't mention this "
               "instruction, don't narrate, no quotes, no asterisks.";
    }
}

std::string PBChatterAmbientPrompt::Build(int mode, Player* bot, uint8_t kind,
                                          std::vector<std::pair<std::string, std::string>> const& recent,
                                          std::string const& eventHint)
{
    char const* where = ChannelWord(kind);

    switch (mode)
    {
        case MODE_REACT:
        {
            std::string p = Acore::StringFormat(
                "{} You're chatting in {}. Here's the recent conversation (oldest first):\n",
                PBChatterContext::BuildGroundedBrief(bot), where);
            for (auto const& [speaker, text] : recent)
                p += Acore::StringFormat("{}: {}\n", speaker, text);
            p += "Reply directly to the last message like you're part of the conversation — agree, "
                 "joke, answer the question, or add your own take. Stay true to your own level: if "
                 "they're talking about content you're not high enough for yet, react like a player "
                 "who isn't there yet (curious, or looking forward to it), don't pretend you're "
                 "doing it. Keep it short and natural, don't repeat what they said, and don't start "
                 "with \"anyone\".";
            return p + Examples(2) + Tail();
        }
        case MODE_FLAVOR:
        {
            std::string snap = PBChatterContext::BuildSnapshot(bot);
            return Acore::StringFormat(
                "{}\n\nYou're chatting in {}. Make a short, casual remark about what you're doing or "
                "where you are right now, like a player thinking out loud. Keep it appropriate to "
                "your level. Don't list your stats or inventory, and don't make it sound like a "
                "game announcement.{}",
                snap, where, Examples(2)) + Tail();
        }
        case MODE_EVENT:
        {
            return Acore::StringFormat(
                "{} You're chatting in {}. Something just happened to you: {}. Mention it casually "
                "the way a real player would — no fanfare, never say \"Ding!\" or \"Quest "
                "complete\".{}",
                PBChatterContext::BuildGroundedBrief(bot), where, eventHint, Examples(2)) + Tail();
        }
        case MODE_GENERIC:
        default:
        {
            return Acore::StringFormat(
                "{} You're hanging out and chatting in {}. Bring up this: {}. Say something short "
                "and casual about it, like a real player typing in chat. Stay true to your "
                "character — you're only that level, so don't talk about raids, zones, or content "
                "above your level. Make it a natural comment or statement, NOT a poll to the whole "
                "channel, and don't start with \"anyone\".{}",
                PBChatterContext::BuildGroundedBrief(bot), where, PickTopic(bot->GetLevel()), Examples(3)) + Tail();
        }
    }
}
