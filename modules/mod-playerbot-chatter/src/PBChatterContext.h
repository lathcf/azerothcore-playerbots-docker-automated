#ifndef MOD_PB_CHATTER_CONTEXT_H
#define MOD_PB_CHATTER_CONTEXT_H
#include <string>
class Player;
namespace PBChatterContext
{
    // World-thread only (reads live game state). Returns a one-paragraph snapshot.
    std::string BuildSnapshot(Player* bot);

    // World-thread only. A one-line "level N <class> in <area>" used to keep ambient lines
    // level/zone-appropriate without dumping the full snapshot.
    std::string BuildBrief(Player* bot);

    // World-thread only. "" when the bot has no quests; else e.g.
    // " You're currently working on the quest 'Red Linen Goods'." (leading space).
    std::string ActiveQuestLine(Player* bot);

    // World-thread only. Name of the item the bot holds the most of (backpack + bags),
    // or "" if bags are essentially empty. Used for a small "real character" detail.
    std::string TopBagItem(Player* bot);

    // World-thread only. A grounding paragraph for ambient prompts: who the bot is, its
    // real current quest, a real character detail, and level-appropriate dungeons/zones.
    std::string BuildGroundedBrief(Player* bot);
}
#endif
