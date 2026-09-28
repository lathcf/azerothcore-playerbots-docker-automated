#ifndef MOD_ENCHANTER_CATALOG_H
#define MOD_ENCHANTER_CATALOG_H

#include "Define.h"
#include <string>
#include <utility>
#include <vector>

class Item;
class Player;

enum class EnchantSource : uint8 { RECIPE = 0, UPGRADE_ITEM = 1 };

struct EnchantOffer
{
    uint32 enchantId = 0;          // SpellItemEnchantment id
    uint32 spellId = 0;            // recipe spell, or the upgrade item's on-use spell
    uint32 itemId = 0;             // upgrade item entry; 0 for recipes
    EnchantSource kind = EnchantSource::RECIPE;
    uint32 skillLine = 0;          // recipe SkillLine; 0 for upgrade items
    uint32 rank = 0;               // recipe learn rank (EnchanterRules::PickRecipeRank), or RankForUpgradeItem(RequiredLevel)
    uint8  rankSource = 0;         // EnchanterRules::RankSource of a recipe rank; unused for upgrade items
    uint8  era = 0;                // EnchanterRules::Era
    uint8  ipTier = 0;             // ProgressionState required; 0 = none
    uint32 perkSkill = 0;          // 0 = anyone; else GetSkillValue(perkSkill) >= perkSkillValue
    uint32 perkSkillValue = 0;
    std::vector<std::pair<uint32, uint32>> reagents;   // (item entry, count)
    std::string name;
};

namespace EnchanterCatalog
{
    // World thread, once, from WorldScript::OnStartup. Immutable afterwards, so readers on any
    // thread (gossip hello can run on a map thread) need no lock.
    void Build();

    std::vector<EnchantOffer> const& Offers();

    // Every item entry an offer prices (reagents + upgrade items): the price cache's key set.
    std::vector<uint32> const& PricedItems();

    // True when some npc_vendor sells `entry` for plain gold in unlimited supply (ExtendedCost = 0,
    // maxcount = 0). Built with the catalog.
    bool SoldByVendorForGold(uint32 entry);

    // Every gate from the design, for one offer on one equipped item. `playerEra` =
    // EnchanterEra::PlayerEra(player), computed once by the caller.
    bool Eligible(Player* player, uint8 playerEra, Item* item, EnchantOffer const& offer);

    // Eligible offer indices for `item`, strongest first (rank desc, then name), one per enchant id.
    std::vector<uint32> EligibleFor(Player* player, Item* item);
}

#endif
