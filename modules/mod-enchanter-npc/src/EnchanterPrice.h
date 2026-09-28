#ifndef MOD_ENCHANTER_PRICE_H
#define MOD_ENCHANTER_PRICE_H

#include "Define.h"
#include "SharedDefines.h"
#include <utility>
#include <vector>

class Player;

struct EnchantOffer;

namespace EnchanterPrice
{
    enum Source : uint8 { SRC_AH = 0, SRC_BOT_FLOOR, SRC_VENDOR, SRC_SELL, SRC_NONE };
    char const* SourceName(Source s);

    struct Quote
    {
        uint64 unitCopper = 0;
        Source source = SRC_NONE;
    };

    // World thread only: reads sAuctionMgr (unlocked) and the AH-bot config, then swaps the
    // result into the cache under its mutex.
    void Rebuild();

    // WorldScript::OnUpdate: rebuilds every EnchanterNpc.PriceRefreshSec.
    void Tick(uint32 diff);

    // Any thread: cached lookup under the cache mutex.
    Quote UnitQuote(uint32 itemEntry, TeamId team);
    // Full market value of an offer's reagents (the "own mats OFF" basis): dump + debug log.
    uint64 MatsCopper(EnchantOffer const& offer, TeamId team);

    // What a purchase takes from the player's bags and what it still has to buy.
    struct MatsPlan
    {
        std::vector<std::pair<uint32, uint32>> useOwn;   // (item entry, count) taken from the bags
        uint64 buyCopper = 0;                            // market value of the missing reagents
    };

    // Count of `entry` in the backpack and the equipped bags' contents, skipping items in a trade.
    // Never equipment, keyring/currency or the bank. Player's own thread.
    uint32 BagItemCount(Player* p, uint32 entry);

    // ownFrom == nullptr: the toggle is OFF, nothing is owned. Otherwise reads ownFrom's bags via
    // BagItemCount; call on the player's own thread (gossip runs there).
    MatsPlan PlanMats(EnchantOffer const& offer, TeamId team, Player* ownFrom);

    // Fee for a plan's missing reagents' value plus labor.
    uint32 FeeForPlan(EnchantOffer const& offer, MatsPlan const& plan);

    // Fee for the MISSING reagents' value (all of them when ownFrom is nullptr) plus labor.
    uint32 OfferFee(EnchantOffer const& offer, TeamId team, Player* ownFrom = nullptr);
}

#endif
