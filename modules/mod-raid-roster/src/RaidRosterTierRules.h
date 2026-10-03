#ifndef MOD_RAID_ROSTER_TIER_RULES_H
#define MOD_RAID_ROSTER_TIER_RULES_H

// Pure, core-free rules for the sync gear era gate (spec:
// docs/superpowers/specs/2026-10-01-raidroster-era-gear-and-level-lock-design.md). Tiers are IP
// ProgressionState values: the stage at which a piece of content OPENS. An item is allowed iff
// its tier <= the master's state. RaidRosterItemTier.cpp only gathers facts from SQL/DBC into
// these shapes; every decision lives here so tests/test_tier_rules.cpp can cover it standalone.

#include <cstdint>
#include <unordered_map>
#include <unordered_set>

namespace RaidRosterTierRules
{
    constexpr uint8_t kTierUnset = 0xFF;     // item fold accumulator: no source seen yet
    constexpr uint8_t kTierUncapped = 0xFF;  // master cap when IP is disabled: allow everything

    // IP ProgressionState values this header needs, as plain constants so it stays core-free.
    // RaidRosterItemTier.cpp static_asserts them against IP's enum.
    constexpr uint8_t kTierPreAq = 4;    // PROGRESSION_PRE_AQ
    constexpr uint8_t kTierPreTbc = 8;   // PROGRESSION_PRE_TBC
    constexpr uint8_t kTierWotlk = 13;   // PROGRESSION_TBC_TIER_5 (WotLK entry)

    // Within ONE source every gate must pass: the highest tier wins.
    inline void FoldSource(uint8_t& acc, uint8_t tier) { if (tier > acc) acc = tier; }
    // Across sources any one suffices: the lowest tier wins.
    inline void FoldItem(uint8_t& acc, uint8_t tier) { if (tier < acc) acc = tier; }

    // No source found (mostly crafted gear): the item's level band, same as RaidRosterEra's
    // BookTier. `level` = max(RequiredLevel, equipCacheNew key level).
    inline uint8_t LevelBandTier(uint32_t level)
    {
        return level <= 60 ? 0 : level <= 70 ? kTierPreTbc : kTierWotlk;
    }

    inline bool Allowed(uint8_t itemTier, uint8_t cap) { return itemTier <= cap; }

    // Everything the data says about one quest; each field is already a tier (0 = no gate).
    struct QuestFacts
    {
        bool hasStarter = false;   // any creature/gameobject/item starter row at all
        uint8_t starterTier = 0;   // MIN over starters (meaningful only when hasStarter)
        uint8_t sortTier = 0;      // TierForMap(map of the QuestSortID zone), QuestSortID > 0
        uint32_t prevQuest = 0;    // abs(quest_template_addon.PrevQuestID); 0 = none
        uint8_t reqItemTier = 0;   // MAX over RequiredItemId1-6 of their drop/chest/vendor tier
        uint8_t condTier = 0;      // IP `conditions` (SourceType 19, rewarded 66000+N)
        uint8_t repTier = 0;       // Brood of Nozdormu rep requirement -> PRE_AQ
        uint8_t overrideTier = 0;  // kQuestTierOverride; 0 = none
    };

    // Quest tier = MAX of every rule plus the prev-chain's tier. Memoised; a cycle back-edge
    // contributes 0 (a quest on a cycle can memoise a partial value — acceptable, IP's chains are
    // acyclic in practice). A quest is a reward SOURCE only if it has a starter (spec: quests with
    // no starter, e.g. 6202 "<UNUSED> Good and Evil", are not obtainable).
    class QuestTierResolver
    {
    public:
        explicit QuestTierResolver(std::unordered_map<uint32_t, QuestFacts> const& facts) : _facts(facts) { }

        bool IsSource(uint32_t quest) const
        {
            auto it = _facts.find(quest);
            return it != _facts.end() && it->second.hasStarter;
        }

        uint8_t Tier(uint32_t quest)
        {
            if (auto m = _memo.find(quest); m != _memo.end())
                return m->second;
            auto it = _facts.find(quest);
            if (it == _facts.end() || _visiting.count(quest))
                return 0;   // unknown quest, or a cycle back-edge
            _visiting.insert(quest);
            QuestFacts const& q = it->second;
            uint8_t t = 0;
            if (q.hasStarter)
                FoldSource(t, q.starterTier);
            FoldSource(t, q.sortTier);
            FoldSource(t, q.reqItemTier);
            FoldSource(t, q.condTier);
            FoldSource(t, q.repTier);
            FoldSource(t, q.overrideTier);
            if (q.prevQuest)
                FoldSource(t, Tier(q.prevQuest));
            _visiting.erase(quest);
            _memo[quest] = t;
            return t;
        }

    private:
        std::unordered_map<uint32_t, QuestFacts> const& _facts;
        std::unordered_map<uint32_t, uint8_t> _memo;
        std::unordered_set<uint32_t> _visiting;
    };
}

#endif // MOD_RAID_ROSTER_TIER_RULES_H
