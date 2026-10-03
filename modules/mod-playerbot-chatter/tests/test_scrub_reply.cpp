// modules/mod-playerbot-chatter/tests/test_scrub_reply.cpp
// Standalone: g++ -std=c++17 -Wall -Wextra -Werror -o <out> modules/mod-playerbot-chatter/tests/test_scrub_reply.cpp modules/mod-playerbot-chatter/src/PBChatterScrubReply.cpp && <out>
#include "../src/PBChatterAmbientPrompt.h"
#include <cstdio>
#include <string>
#include <vector>

using PBChatterAmbientPrompt::ScrubReply;

static int g_fails = 0;
#define CHECK_EQ(in, want) do { std::string got_ = ScrubReply((in), kNames); \
    if (got_ != (want)) { std::printf("FAIL %s:%d  ScrubReply(\"%s\") = \"%s\", want \"%s\"\n", \
        __FILE__, __LINE__, std::string(in).c_str(), got_.c_str(), std::string(want).c_str()); ++g_fails; } } while (0)

static std::vector<std::string> const kNames = { "Thrandil", "Mora" };

int main()
{
    // (a) leading name, each separator, case-insensitive, optional @.
    CHECK_EQ("Thrandil, that raid is cursed", "that raid is cursed");
    CHECK_EQ("Thrandil: that raid is cursed", "that raid is cursed");
    CHECK_EQ("Thrandil! that raid is cursed", "that raid is cursed");
    CHECK_EQ("Thrandil. that raid is cursed", "that raid is cursed");
    CHECK_EQ("Thrandil - that raid is cursed", "that raid is cursed");
    CHECK_EQ("Thrandil that raid is cursed", "that raid is cursed");
    CHECK_EQ("thrandil: that raid is cursed", "that raid is cursed");
    CHECK_EQ("@Thrandil that raid is cursed", "that raid is cursed");
    CHECK_EQ("@thrandil, that raid is cursed", "that raid is cursed");
    CHECK_EQ("MORA, you up for heroics?", "you up for heroics?");

    // (b) trailing name; trailing . ! ? kept.
    CHECK_EQ("no way that drops, Thrandil", "no way that drops");
    CHECK_EQ("lol thrandil!", "lol!");
    CHECK_EQ("what do you think, Thrandil?", "what do you think?");
    CHECK_EQ("ok @Mora", "ok");

    // (c) one leading affirmation, then (a) again.
    CHECK_EQ("Facts. the drop rate is awful", "the drop rate is awful");
    CHECK_EQ("Facts, Thrandil, the drop rate is awful", "the drop rate is awful");
    CHECK_EQ("exactly, nobody runs that anymore", "nobody runs that anymore");
    CHECK_EQ("fr the queue is forever", "the queue is forever");
    CHECK_EQ("100% - the queue is forever", "the queue is forever");
    CHECK_EQ("That's right Mora, the queue is forever", "the queue is forever");
    CHECK_EQ("agreed! gnomes are cursed", "gnomes are cursed");
    CHECK_EQ("Same. ran it twice today", "ran it twice today");
    CHECK_EQ("True! still worth it though", "still worth it though");
    CHECK_EQ("Facts, facts, it is", "facts, it is");              // ONE token only

    // (e) nothing but an affirmation / a name / a name verdict -> "".
    CHECK_EQ("Facts.", "");
    CHECK_EQ("facts", "");
    CHECK_EQ("Exactly!", "");
    CHECK_EQ("Thrandil is right", "");
    CHECK_EQ("Thrandil is right.", "");
    CHECK_EQ("thrandil nailed it", "");
    CHECK_EQ("Mora is spot on!", "");
    CHECK_EQ("Thrandil!", "");
    CHECK_EQ("Facts, Thrandil.", "");
    CHECK_EQ("Thrandil is right, the drop rate is awful", "the drop rate is awful");

    // Untouched: mid-sentence names, names as word prefixes, ambiguous openers before a space.
    CHECK_EQ("i think Thrandil has the right idea there", "i think Thrandil has the right idea there");
    CHECK_EQ("Thrandilson is a great name for a gnome", "Thrandilson is a great name for a gnome");
    CHECK_EQ("Thrandil's guild cleared it last night", "Thrandil's guild cleared it last night");
    CHECK_EQ("ran with Thrandilson", "ran with Thrandilson");
    CHECK_EQ("this raid is cursed", "this raid is cursed");
    CHECK_EQ("same thing happened to me", "same thing happened to me");
    CHECK_EQ("real ones remember vanilla", "real ones remember vanilla");
    CHECK_EQ("true, but the gold is good", "true, but the gold is good");
    CHECK_EQ("yeah the queue is long", "yeah the queue is long");
    CHECK_EQ("factsheet says otherwise", "factsheet says otherwise");
    CHECK_EQ("fresh 80 here", "fresh 80 here");
    CHECK_EQ("...anyway who's up for heroics", "...anyway who's up for heroics");
    CHECK_EQ("  plain line  ", "plain line");

    // No names known: name rules are inert, affirmations still scrub.
    { std::vector<std::string> const none;
      if (ScrubReply("Thrandil, hi", none) != "Thrandil, hi") { std::printf("FAIL no-names\n"); ++g_fails; }
      if (ScrubReply("Facts.", none) != "") { std::printf("FAIL no-names affirm\n"); ++g_fails; } }

    if (g_fails)
    {
        std::printf("%d failure(s)\n", g_fails);
        return 1;
    }
    std::printf("all scrub-reply tests passed\n");
    return 0;
}
