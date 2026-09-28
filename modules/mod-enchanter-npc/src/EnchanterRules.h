#ifndef MOD_ENCHANTER_RULES_H
#define MOD_ENCHANTER_RULES_H

// Pure rules for mod-enchanter-npc. No core headers, so tests/test_enchanter_rules.cpp builds this
// standalone. Everything here is deterministic arithmetic over plain integers.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

namespace EnchanterRules
{
    // Numeric values deliberately equal mod-era-talents' EraId (ERA_VANILLA=0, ERA_TBC=1,
    // ERA_WOTLK=2) so the two convert with a plain cast.
    enum Era : uint8_t { ERA_VANILLA = 0, ERA_TBC = 1, ERA_WOTLK = 2 };

    // Profession skill caps per expansion: a recipe's rank says which expansion added it.
    constexpr uint32_t kVanillaSkillCap = 300;
    constexpr uint32_t kTbcSkillCap     = 375;
    constexpr uint32_t kMaxSkill        = 450;

    // mod-playerbots' gear-era proxy (RandomItemMgr / IsEraLegalConsumable): first TBC and first
    // WotLK item entry.
    constexpr uint32_t kFirstTbcItemId   = 23728;
    constexpr uint32_t kFirstWotlkItemId = 35570;

    constexpr uint32_t kMaxLevel = 80;

    inline Era EraForRecipeRank(uint32_t rank)
    {
        if (rank <= kVanillaSkillCap) return ERA_VANILLA;
        if (rank <= kTbcSkillCap)     return ERA_TBC;
        return ERA_WOTLK;
    }

    // Enchanting materials an expansion introduced. A recipe that consumes one cannot predate that
    // expansion, whatever its learn rank says (several late TBC/WotLK recipes carry a low trainer rank).
    constexpr uint32_t kTbcMaterials[] =
        { 22445, 22446, 22447, 22448, 22449, 22450 };   // Arcane Dust, Greater/Lesser Planar Essence,
                                                        // Small/Large Prismatic Shard, Void Crystal
    constexpr uint32_t kWotlkMaterials[] =
        { 34052, 34053, 34054, 34055, 34056, 34057 };   // Dream Shard, Small Dream Shard, Infinite Dust,
                                                        // Greater/Lesser Cosmic Essence, Abyss Crystal

    inline Era MaterialEra(uint32_t itemId)
    {
        for (uint32_t m : kWotlkMaterials)
            if (m == itemId) return ERA_WOTLK;
        for (uint32_t m : kTbcMaterials)
            if (m == itemId) return ERA_TBC;
        return ERA_VANILLA;
    }

    inline Era EraForUpgradeItem(uint32_t reqLevel, uint32_t itemId)
    {
        if (reqLevel <= 60 && itemId < kFirstTbcItemId)   return ERA_VANILLA;
        if (reqLevel <= 70 && itemId < kFirstWotlkItemId) return ERA_TBC;
        return ERA_WOTLK;
    }

    // Copy of Spell::CheckCast's SPELL_EFFECT_ENCHANT_ITEM item-level rule (Spell.cpp). The floor
    // applies unless the spell has SPELL_ATTR2_ALLOW_LOW_LEVEL_BUFF; the ceiling applies only to
    // casts FROM an item (the core gates it on m_CastItem), i.e. upgrade items, never recipes.
    inline bool PassesItemLevelRule(uint32_t itemReqLevel, uint32_t itemLevel, uint32_t baseLevel,
                                    uint32_t maxLevel, bool allowLowLevel, bool castFromItem)
    {
        if (!allowLowLevel)
        {
            uint32_t req = itemReqLevel ? itemReqLevel : itemLevel;
            if (req < baseLevel)
                return false;
        }
        if (castFromItem && maxLevel > 0 && itemLevel > maxLevel)
            return false;
        return true;
    }

    // A recipe's real learn requirement (0 = no such source): the trainer's ReqSkillRank, else
    // the formula item's RequiredSkillRank, else SkillLineAbility.MinSkillLineRank. The last is only
    // meaningful for skill-up-learned recipes: it is 1 for nearly every recipe in this DBC.
    enum RankSource : uint8_t { RANK_TRAINER = 0, RANK_FORMULA = 1, RANK_SKILLLINE = 2 };

    inline uint32_t PickRecipeRank(uint32_t trainerRank, uint32_t formulaRank, uint32_t skillLineRank)
    {
        if (trainerRank) return trainerRank;
        if (formulaRank) return formulaRank;
        return skillLineRank;
    }

    inline RankSource PickRecipeRankSource(uint32_t trainerRank, uint32_t formulaRank)
    {
        if (trainerRank) return RANK_TRAINER;
        if (formulaRank) return RANK_FORMULA;
        return RANK_SKILLLINE;
    }

    // Upgrade items have no skill rank; map their required level onto the 0-450 skill scale.
    inline uint32_t RankForUpgradeItem(uint32_t reqLevel)
    {
        return std::min(reqLevel, kMaxLevel) * kMaxSkill / kMaxLevel;
    }

    // Quadratic labor tip: laborAtMax at rank 450, ~44% of it at 300, ~11% at 150.
    inline uint64_t LaborCopper(uint32_t rank, uint64_t laborAtMax)
    {
        double f = double(std::min(rank, kMaxSkill)) / double(kMaxSkill);
        return uint64_t(std::llround(double(laborAtMax) * f * f));
    }

    // Player::ModifyMoney takes int32 and the gossip popup's money is uint32; cap at int32 max.
    constexpr uint64_t kMaxFee = 0x7FFFFFFF;

    inline uint32_t Fee(uint64_t matsCopper, uint32_t matsPct, uint64_t laborCopper)
    {
        uint64_t fee = matsCopper * matsPct / 100 + laborCopper;
        fee = std::max<uint64_t>(fee, 1);
        return uint32_t(std::min(fee, kMaxFee));
    }

    // Fold one AH listing into the running cheapest per-unit buyout (0 = none yet). Bid-only
    // listings (buyout 0) never count: nobody can actually buy at that price.
    inline void FoldListing(uint64_t& best, uint32_t buyout, uint32_t count)
    {
        if (!buyout || !count)
            return;
        uint64_t unit = std::max<uint64_t>(buyout / count, 1);
        if (!best || unit < best)
            best = unit;
    }

    // Source-derived IP tier: the LOWEST tier over every place a recipe/item comes from, so one
    // open source (trainer, quest, world drop) keeps it open. kTierUnset = no source seen = open.
    constexpr uint8_t kTierUnset = 0xFF;
    inline void FoldTier(uint8_t& acc, uint8_t tier) { if (tier < acc) acc = tier; }
    inline uint8_t ResolveTier(uint8_t acc) { return acc == kTierUnset ? 0 : acc; }

    // GOSSIP_MAX_MENU_ITEMS is 32: 30 offers + Next page + Back.
    constexpr uint32_t kPageSize = 30;
    inline uint32_t PageCount(uint32_t n) { return n == 0 ? 1 : (n + kPageSize - 1) / kPageSize; }
    inline uint32_t PageBegin(uint32_t page, uint32_t n) { return std::min(page * kPageSize, n); }
    inline uint32_t PageEnd(uint32_t page, uint32_t n) { return std::min((page + 1) * kPageSize, n); }
    inline bool HasNextPage(uint32_t page, uint32_t n) { return PageEnd(page, n) < n; }

    // Gossip encoding. sender: kSenderTop, or kSenderSlotBase + equipment slot.
    // action: an offer's catalog index, kActionPageBase + page, or kActionBack.
    constexpr uint32_t kSenderTop      = 1;
    constexpr uint32_t kSenderSlotBase = 100;
    constexpr uint32_t kActionPageBase = 0xFFFF0000u;
    constexpr uint32_t kActionBack     = 0xFFFFFF00u;
    inline bool IsPageAction(uint32_t action) { return action >= kActionPageBase && action < kActionBack; }

    // The per-visit "Use my materials" toggle rides in `sender` as a flag bit, SET when the toggle
    // is OFF, so an ON sender equals the plain encoding. The server keeps no per-player state.
    constexpr uint32_t kSenderNoMatsFlag = 0x10000;
    inline uint32_t EncodeSender(uint32_t base, bool useMats) { return useMats ? base : base | kSenderNoMatsFlag; }
    inline bool SenderUsesMats(uint32_t sender) { return !(sender & kSenderNoMatsFlag); }
    inline uint32_t SenderBase(uint32_t sender) { return sender & ~kSenderNoMatsFlag; }

    // Top-menu toggle line's action. Above kActionBack, so never a page action.
    constexpr uint32_t kActionToggleMats = 0xFFFFFF01u;

    // Reagents the player's bags can't cover.
    inline uint32_t MissingCount(uint32_t need, uint32_t have) { return have >= need ? 0 : need - have; }

    inline std::string MoneyString(uint64_t copper)
    {
        uint64_t g = copper / 10000, s = (copper / 100) % 100, c = copper % 100;
        std::string out;
        if (g)      out += std::to_string(g) + "g ";
        if (g || s) out += std::to_string(s) + "s ";
        out += std::to_string(c) + "c";
        return out;
    }
}

#endif // MOD_ENCHANTER_RULES_H
