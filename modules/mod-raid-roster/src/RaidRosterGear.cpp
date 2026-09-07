#include "RaidRosterGear.h"
#include "DBCStores.h"
#include "ItemTemplate.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "SharedDefines.h"
#include "Mgr/Item/RandomItemMgr.h"
#include "Mgr/Item/StatsWeightCalculator.h"
#include <algorithm>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace
{

// ilvl window half-width for off-piece picks (level >= 50), and how far above the
// target a set's ilvl may sit and still qualify. Compile-time by design (spec: YAGNI —
// promote to conf only if tuning proves necessary).
constexpr int32 OFF_WINDOW = 13;
constexpr int32 SET_TOLERANCE = 6;

// Sub-50 ilvl ceiling: bot level + this. Below 50, real WotLK itemization sits at
// requiredLevel + 5 almost exactly (measured across the whole 5-49 band in acore_world:
// mean gap 5.0, worst-case gap 15), so this admits every legitimately-itemized piece for
// the level. It exists as a BACKSTOP for the ~560 uncommon+ equippables whose
// item_template.RequiredLevel is a flat 0 despite being endgame gear (ilvl-232 Onyxia
// trinkets, ilvl-174 Northrend greens, ilvl-60 classic raid pieces): those cache under
// key 0, so the required-level gate below cannot catch them and CanUseItem waves them
// through. Verified against the live world DB: it excludes 4 keyed items (all QA-test
// entries at key 1) out of ~4500 legitimate sub-50 candidates.
constexpr int32 LOW_LEVEL_ILVL_HEADROOM = 15;

// A qualifying set must cover at least this many distinct body-armor equipment slots —
// keeps tier-like sets (5-9 pieces) and stops 2-piece oddities (Devilsaur Armor, ring
// pairs) from hijacking the set stage. WotLK/TBC tiers cover 5, classic tiers 8-9.
constexpr size_t MIN_SET_SLOTS = 4;

// PvE raid sync must never assemble PvP sets, even when their ilvl band matches the
// target (Deadly Gladiator ilvl 213 == T7.25's). Resilience-based detection misses the
// classic (pre-resilience) rank sets, so exclude by ItemSet.dbc set-name marker. These
// markers cover every classic rank set (both factions, rare + epic), TBC/WotLK arena
// ("Gladiator") and honor sets; none collide with a PvE set name (checked against
// tier/dungeon/ZG/AQ set names).
bool IsPvpSetName(char const* name)
{
    if (!name)
        return false;
    static char const* const kPvpMarkers[] = {
        "Gladiator", "Marshal", "Warlord", "General", "Champion",
        "Lieutenant", "Knight-", "Blood Guard", "Legionnaire",
    };
    for (char const* marker : kPvpMarkers)
        if (strstr(name, marker))
            return true;
    return false;
}

// Body-armor inventory types (the slots tier sets cover) -> equipment slot; -1 for
// everything else (jewelry, cloaks, weapons — not set-stage material).
int32 BodySlotForInvType(uint32 invType)
{
    switch (invType)
    {
        case INVTYPE_HEAD:      return EQUIPMENT_SLOT_HEAD;
        case INVTYPE_SHOULDERS: return EQUIPMENT_SLOT_SHOULDERS;
        case INVTYPE_CHEST:
        case INVTYPE_ROBE:      return EQUIPMENT_SLOT_CHEST;
        case INVTYPE_LEGS:      return EQUIPMENT_SLOT_LEGS;
        case INVTYPE_HANDS:     return EQUIPMENT_SLOT_HANDS;
        case INVTYPE_WAIST:     return EQUIPMENT_SLOT_WAIST;
        case INVTYPE_FEET:      return EQUIPMENT_SLOT_FEET;
        case INVTYPE_WRISTS:    return EQUIPMENT_SLOT_WRISTS;
        default:                return -1;
    }
}

// Best armor subclass for a class at a level. Level-aware, unlike arena-roster's 80-only
// mapping: warriors/paladins train plate at 40 (mail before), hunters/shamans mail at 40
// (leather before). Used as a 3x score PREFERENCE for off-pieces (never a hard filter,
// so low-level pools can't go empty) and as a HARD filter for set pieces (the set stage
// only runs at >= 50, where the endgame mapping is already correct).
uint8 BestArmorSubclassFor(uint8 cls, uint8 level)
{
    switch (cls)
    {
        case CLASS_WARRIOR:
        case CLASS_PALADIN:
            return level >= 40 ? ITEM_SUBCLASS_ARMOR_PLATE : ITEM_SUBCLASS_ARMOR_MAIL;
        case CLASS_DEATH_KNIGHT:
            return ITEM_SUBCLASS_ARMOR_PLATE;
        case CLASS_HUNTER:
        case CLASS_SHAMAN:
            return level >= 40 ? ITEM_SUBCLASS_ARMOR_MAIL : ITEM_SUBCLASS_ARMOR_LEATHER;
        case CLASS_ROGUE:
        case CLASS_DRUID:
            return ITEM_SUBCLASS_ARMOR_LEATHER;
        default:
            return ITEM_SUBCLASS_ARMOR_CLOTH;  // mage/warlock/priest
    }
}

struct SetInfo
{
    std::vector<uint32> pieces;  // uncommon+ body-armor entries of this ItemSet
};

// Bot-independent gear index, built once on first sync (world thread only, like every
// caller in this module — no locking).
//   byInvType: invType -> (ItemLevel, entry) sorted by ItemLevel, for ilvl-window range
//     scans. Sourced from mod-playerbots' curated equipCacheNew (its IsValidItem pass
//     drops name-matched test/deprecated/unobtainable items and everything above epic —
//     but has NO quality floor, so we additionally drop below-uncommon at the build loop
//     below), flattened across all of that cache's level keys.
//   reqLevel: entry -> the equipCacheNew KEY it was cached under, i.e. its EFFECTIVE
//     required level: max(RequiredLevel, questLevel) for quest rewards, RequiredLevel
//     otherwise. Flattening byInvType destroys that key, and it is the only place the
//     quest-reward level exists — ~1900 uncommon+ equippables in acore_world carry
//     item_template.RequiredLevel = 0 while being level-70/80 quest rewards, so
//     Player::CanUseItem's `GetLevel() < RequiredLevel` test waves them straight through
//     to a level-15 bot. Keeping the key restores the level semantics upstream's factory
//     gets for free by querying the cache per level (PlayerbotFactory.cpp).
//   sets: ItemSet id -> body-armor pieces, from item_template directly (set items are
//     all real droppable items); PvP sets excluded by name at build time.
struct GearIndex
{
    std::unordered_map<uint32, std::vector<std::pair<uint16, uint32>>> byInvType;
    std::unordered_map<uint32, uint8> reqLevel;
    std::unordered_map<uint32, SetInfo> sets;
};

GearIndex const& GetIndex()
{
    static GearIndex const index = []()
    {
        GearIndex idx;
        constexpr uint32 kMaxLevel = 80;  // WotLK cap (DEFAULT_MAX_LEVEL)
        static constexpr InventoryType kInvTypes[] = {
            INVTYPE_HEAD, INVTYPE_NECK, INVTYPE_SHOULDERS, INVTYPE_CHEST, INVTYPE_ROBE,
            INVTYPE_WAIST, INVTYPE_LEGS, INVTYPE_FEET, INVTYPE_WRISTS, INVTYPE_HANDS,
            INVTYPE_FINGER, INVTYPE_TRINKET, INVTYPE_CLOAK,
            INVTYPE_WEAPON, INVTYPE_2HWEAPON, INVTYPE_WEAPONMAINHAND,
            INVTYPE_WEAPONOFFHAND, INVTYPE_SHIELD, INVTYPE_HOLDABLE,
            INVTYPE_RANGED, INVTYPE_RANGEDRIGHT, INVTYPE_THROWN, INVTYPE_RELIC,
        };
        for (uint32 lvl = 0; lvl <= kMaxLevel; ++lvl)
            for (InventoryType invType : kInvTypes)
                for (uint32 itemId : sRandomItemMgr.GetEquipmentNew(lvl, invType))
                {
                    ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemId);
                    // GetEquipmentNew's IsValidItem pass has NO quality floor: it keeps
                    // grey/white items, and its name-based test filter (IsInternalItem)
                    // matches "Test"/"Deprecated"/... but MISSES dev items like the
                    // 'Twain Random Sword' (entry 6174) — a grey (POOR) ilvl-20 2H with
                    // absurd damage that outscored real weapons and got equipped by the
                    // sub-50 best-in-slot path (which, unlike the set stage, applies no
                    // quality filter of its own). Restrict the whole pool to uncommon..epic
                    // here, the single chokepoint feeding both RankedCandidates and PickSet,
                    // matching the set stage's floor and the module's "real gear = uncommon+"
                    // design. Above-epic is already dropped by GetEquipmentNew; the upper
                    // bound is belt-and-braces.
                    if (!proto || proto->Quality < ITEM_QUALITY_UNCOMMON ||
                        proto->Quality > ITEM_QUALITY_EPIC)
                        continue;
                    idx.byInvType[invType].push_back({ uint16(proto->ItemLevel), itemId });
                    // Each entry is cached under exactly one key (the quest pass
                    // dedupes, the template pass skips what the quest pass took), so
                    // this assignment is unambiguous.
                    idx.reqLevel[itemId] = uint8(lvl);
                }
        for (auto& [invType, v] : idx.byInvType)
        {
            std::sort(v.begin(), v.end());
            v.erase(std::unique(v.begin(), v.end()), v.end());
        }

        // Set pieces must ALSO be in the curated pool (IsValidItem-filtered) — a raw
        // item_template scan would let deprecated/unobtainable set-piece duplicates
        // win PickSet with items the off-piece stage would refuse to touch.
        std::unordered_set<uint32> curated;
        for (auto const& [invType, v] : idx.byInvType)
            for (auto const& [ilvl, entry] : v)
                curated.insert(entry);

        for (auto const& [entryId, proto] : *sObjectMgr->GetItemTemplateStore())
        {
            if (!proto.ItemSet || proto.Class != ITEM_CLASS_ARMOR)
                continue;
            if (proto.Quality < ITEM_QUALITY_UNCOMMON || proto.Quality > ITEM_QUALITY_EPIC)
                continue;
            if (BodySlotForInvType(proto.InventoryType) < 0)
                continue;
            if (!curated.count(entryId))
                continue;
            ItemSetEntry const* setEntry = sItemSetStore.LookupEntry(proto.ItemSet);
            if (!setEntry || IsPvpSetName(setEntry->name[0]))
                continue;
            idx.sets[proto.ItemSet].pieces.push_back(entryId);
        }

        LOG_INFO("playerbots", "[RaidRoster] Gear index built: {} inventory types, {} candidate sets.",
                 uint32(idx.byInvType.size()), uint32(idx.sets.size()));
        return idx;
    }();
    return index;
}

// True when `entry` is legitimately available to a character of `level` — i.e. the level
// it was cached under is at or below the bot's. Unknown entries (never in the curated
// pool) report level 0 and pass; every caller here iterates the pool, so that can't
// happen in practice.
bool MeetsLevelRequirement(GearIndex const& idx, uint32 entry, uint8 level)
{
    auto it = idx.reqLevel.find(entry);
    return it == idx.reqLevel.end() || it->second <= level;
}

// Probe + equip one item entry via the core's own leak-free pair (same idiom as
// arena-roster): CanEquipNewItem creates and deletes its temporary Item internally,
// EquipNewItem creates the real one only on success. NULL_SLOT lets the core pick the
// destination, which naturally fills finger2/trinket2/offhand on the second call.
bool EquipEntry(Player* bot, uint32 entry)
{
    uint16 dest = 0;
    if (bot->CanEquipNewItem(NULL_SLOT, dest, entry, false) != EQUIP_ERR_OK)
        return false;
    return bot->EquipNewItem(dest, entry, true) != nullptr;
}

// Usable candidates of the given inventory types, ranked best spec-score first.
// floorIlvl/ceilIlvl of 0 mean unbounded on that side (the sub-50 path passes 0/0).
// Preferred-armor-subclass body pieces get factory-style 3x preference; items scoring
// <= 0 (nothing useful for this spec) are dropped outright.
std::vector<std::pair<float, uint32>> RankedCandidates(
    Player* bot, StatsWeightCalculator& calc, std::initializer_list<uint32> invTypes,
    int32 floorIlvl, int32 ceilIlvl)
{
    GearIndex const& idx = GetIndex();
    uint32 const classMask = 1u << (bot->getClass() - 1);
    uint8 const preferred = BestArmorSubclassFor(bot->getClass(), bot->GetLevel());

    std::vector<std::pair<float, uint32>> out;
    for (uint32 invType : invTypes)
    {
        auto it = idx.byInvType.find(invType);
        if (it == idx.byInvType.end())
            continue;
        std::vector<std::pair<uint16, uint32>> const& v = it->second;
        auto lo = floorIlvl > 0
            ? std::lower_bound(v.begin(), v.end(),
                               std::pair<uint16, uint32>{ uint16(floorIlvl), 0u })
            : v.begin();
        auto hi = ceilIlvl > 0
            ? std::upper_bound(v.begin(), v.end(),
                               std::pair<uint16, uint32>{ uint16(ceilIlvl), 0xFFFFFFFFu })
            : v.end();
        for (auto itr = lo; itr != hi; ++itr)
        {
            ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itr->second);
            if (!proto || !(proto->AllowableClass & classMask))
                continue;
            if (!MeetsLevelRequirement(idx, itr->second, bot->GetLevel()))
                continue;
            if (bot->CanUseItem(proto) != EQUIP_ERR_OK)  // level/skill/faction gate
                continue;
            float score = calc.CalculateItem(itr->second);
            if (score <= 0.f)
                continue;
            if (proto->Class == ITEM_CLASS_ARMOR && BodySlotForInvType(invType) >= 0 &&
                proto->SubClass == preferred)
                score *= 3.0f;
            out.push_back({ score, itr->second });
        }
    }
    std::sort(out.begin(), out.end(),
              [](std::pair<float, uint32> const& a, std::pair<float, uint32> const& b)
              { return a.first != b.first ? a.first > b.first : a.second < b.second; });
    return out;
}

struct ChosenSet
{
    uint32 setId = 0;
    char const* name = "";
    uint16 maxIlvl = 0;
    uint16 medianIlvl = 0;                    // median piece ilvl (the set's window key)
    uint8 quality = 0;                        // max piece quality (the set's quality tier)
    std::vector<uint32> pieces;               // per-bot legal pieces, one per slot
    std::vector<uint16> pieceIlvls;           // parallel ilvls, for the median
    std::unordered_set<int32> coveredSlots;   // equipment slots those pieces fill
};

// Pick the tier set for this bot: pieces filtered to class-legal, best-subclass,
// CanUseItem-legal; set qualifies if it covers >= MIN_SET_SLOTS body slots and its MEDIAN
// piece ilvl sits in [target - OFF_WINDOW, target + SET_TOLERANCE]. The window keys on the
// median, not the max, because a real tier set spans ilvls: a TBC T6 set runs 146 on its
// five tier tokens up to 154 on its belt/boots/bracers, so a max-ilvl gate excluded the
// whole epic set for a 147 (T6-geared) master by one point — and left a lower green/blue
// set (Frostwoven, max 150) as the only qualifier. The median (146 for T6) tracks where the
// set's bulk sits, so the set qualifies for the band it belongs to without over-reaching
// into the next tier (T10 median 264 still fails a 245 master's ceiling).
// Selection is QUALITY-FIRST: the highest quality tier present wins, then the highest
// median-ilvl within that quality, then the summed spec score breaks remaining ties.
// Quality leads because same-ilvl gear is NOT equivalent — a WotLK green leveling/crafted
// set sits at the same ilvl as a TBC epic tier set, and a pure highest-ilvl rule handed the
// green the win before quality or spec ever mattered; leading with quality keeps a tank
// slot in its epic tier set (the score tiebreak still separates same-ilvl tank vs dps
// versions — Scourgeborne Plate vs Battlegear — with no name tables).
bool PickSet(Player* bot, StatsWeightCalculator& calc, int32 target, ChosenSet& out)
{
    GearIndex const& idx = GetIndex();
    uint32 const classMask = 1u << (bot->getClass() - 1);
    uint8 const subclass = BestArmorSubclassFor(bot->getClass(), bot->GetLevel());

    std::vector<ChosenSet> qualifying;
    for (auto const& [setId, info] : idx.sets)
    {
        ChosenSet cand;
        cand.setId = setId;
        for (uint32 entry : info.pieces)
        {
            ItemTemplate const* proto = sObjectMgr->GetItemTemplate(entry);
            if (!proto || !(proto->AllowableClass & classMask))
                continue;
            if (proto->SubClass != subclass)
                continue;
            if (!MeetsLevelRequirement(idx, entry, bot->GetLevel()))
                continue;
            if (bot->CanUseItem(proto) != EQUIP_ERR_OK)
                continue;
            int32 slot = BodySlotForInvType(proto->InventoryType);
            if (slot < 0 || cand.coveredSlots.count(slot))
                continue;  // one piece per equipment slot
            cand.coveredSlots.insert(slot);
            cand.pieces.push_back(entry);
            cand.pieceIlvls.push_back(uint16(proto->ItemLevel));
            cand.maxIlvl = std::max(cand.maxIlvl, uint16(proto->ItemLevel));
            cand.quality = std::max(cand.quality, uint8(proto->Quality));
        }
        if (cand.coveredSlots.size() < MIN_SET_SLOTS)
            continue;
        // Median of the legal pieces — the set's window key (see PickSet header).
        {
            std::vector<uint16> il = cand.pieceIlvls;
            std::nth_element(il.begin(), il.begin() + il.size() / 2, il.end());
            cand.medianIlvl = il[il.size() / 2];
        }
        if (int32(cand.medianIlvl) > target + SET_TOLERANCE)
            continue;
        // No reach-down past the off-piece window either: a mid-band target (fresh 80 at
        // ~ilvl 187 sits between T6 and T7) must produce NO set — the off-piece path
        // covers every slot at the right ilvl — not the previous era's set 30+ ilvls low.
        if (int32(cand.medianIlvl) < target - OFF_WINDOW)
            continue;
        qualifying.push_back(std::move(cand));
    }
    if (qualifying.empty())
        return false;

    // Quality-first: a lower-quality set can never win, whatever its ilvl (this is what
    // keeps a WotLK green off a level-70 bot that a TBC epic set fits). Then the highest
    // median-ilvl within the winning quality tier.
    uint8 bestQuality = 0;
    for (ChosenSet const& s : qualifying)
        bestQuality = std::max(bestQuality, s.quality);

    uint16 bestIlvl = 0;
    for (ChosenSet const& s : qualifying)
        if (s.quality == bestQuality)
            bestIlvl = std::max(bestIlvl, s.medianIlvl);

    // Score ONLY the top quality+ilvl band (ties): CalculateItem is not cheap, and lower
    // bands can never win anyway.
    ChosenSet* winner = nullptr;
    float bestScore = std::numeric_limits<float>::lowest();
    for (ChosenSet& s : qualifying)
    {
        if (s.quality != bestQuality || s.medianIlvl != bestIlvl)
            continue;
        float score = 0.f;
        for (uint32 entry : s.pieces)
            score += calc.CalculateItem(entry);
        if (score > bestScore || (winner && score == bestScore && s.setId < winner->setId))
        {
            bestScore = score;
            winner = &s;
        }
    }
    out = std::move(*winner);
    if (ItemSetEntry const* setEntry = sItemSetStore.LookupEntry(out.setId))
        out.name = setEntry->name[0];
    return true;
}

} // namespace

namespace RaidRosterGear
{

bool EquipForSpec(Player* bot, Player* master, int specTab)
{
    if (!bot || !master)
        return false;
    // Gate BEFORE the strip — never leave a bot naked because it couldn't be geared.
    if (!bot->IsInWorld() || bot->GetLevel() < 5)
    {
        LOG_WARN("playerbots", "[RaidRoster] Not gearing {}: {} (level {}).", bot->GetName(),
                 !bot->IsInWorld() ? "not in world" : "below level 5", bot->GetLevel());
        return false;
    }

    uint8 const level = bot->GetLevel();
    bool const targeted = level >= 50;
    // Master's raw average equipped ilvl (the DF variant sums plain ItemLevel and counts
    // heirlooms as level-equivalent gear; the non-DF variant mixes quality multipliers
    // into the value and would skew the window). Sub-50 keeps the uncapped
    // best-in-slot-for-level special case: no target at all.
    int32 const target = targeted ? int32(master->GetAverageItemLevelForDF() + 0.5f) : 0;

    // Strip everything except the cosmetic shirt/tabard (factory second_chance style).
    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
    {
        if (slot == EQUIPMENT_SLOT_BODY || slot == EQUIPMENT_SLOT_TABARD)
            continue;
        if (bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
            bot->DestroyItem(INVENTORY_SLOT_BAG_0, slot, true);
    }

    // Spec-aware scorer — valid only because the caller already pinned the talents.
    StatsWeightCalculator calc(bot);
    uint32 equipped = 0;

    // ---- Set stage (level >= 50): assemble the tier set for the target band. ----
    ChosenSet set;
    bool const haveSet = targeted && PickSet(bot, calc, target, set);
    if (haveSet)
    {
        // Track which slots actually EQUIPPED — a piece that fails the equip probe
        // must leave its slot to the off-piece stage, not silently empty.
        std::unordered_set<int32> equippedSlots;
        for (uint32 entry : set.pieces)
            if (EquipEntry(bot, entry))
            {
                ++equipped;
                if (ItemTemplate const* proto = sObjectMgr->GetItemTemplate(entry))
                    equippedSlots.insert(BodySlotForInvType(proto->InventoryType));
            }
        LOG_INFO("playerbots", "[RaidRoster] {}: equipped {}/{} piece(s) of '{}' (set ilvl {}, target {}).",
                 bot->GetName(), equipped, uint32(set.pieces.size()), set.name,
                 uint32(set.maxIlvl), target);
        set.coveredSlots = std::move(equippedSlots);
    }

    // ---- Off-piece stage: everything the set didn't cover. ----
    // Window: +/- OFF_WINDOW around the target; when the chosen set sits below the
    // target, the floor rises to the target so the bot's overall average recovers (the
    // make-up rule). Sub-50 keeps an open floor (take the best there is) but caps the
    // ceiling at the bot's level + LOW_LEVEL_ILVL_HEADROOM — "best in slot FOR THE
    // BOT'S LEVEL". An empty window widens its floor downward (never the ceiling — bots
    // must not out-gear the master's band) until something equips or the floor bottoms
    // out.
    int32 floorIlvl = 0, ceilIlvl = 0;
    if (targeted)
    {
        floorIlvl = (haveSet && int32(set.maxIlvl) < target) ? target
                                                             : std::max(1, target - OFF_WINDOW);
        ceilIlvl = target + OFF_WINDOW;
    }
    else
    {
        ceilIlvl = int32(level) + LOW_LEVEL_ILVL_HEADROOM;
    }

    auto covered = [&](int32 eqSlot)
    { return haveSet && set.coveredSlots.count(eqSlot) != 0; };

    // Fill one logical slot with the `count` best-scored candidates (count=2 for rings/
    // trinkets; NULL_SLOT probing lands the second copy in finger2/trinket2).
    auto fillSlot = [&](char const* slotName, std::initializer_list<uint32> invTypes,
                        uint32 count = 1)
    {
        uint32 const wanted = count;
        uint32 got = 0;
        int32 curFloor = floorIlvl;
        while (true)
        {
            uint32 want = count;
            for (auto const& [score, entry] :
                 RankedCandidates(bot, calc, invTypes, curFloor, ceilIlvl))
            {
                if (EquipEntry(bot, entry) && --want == 0)
                    break;
            }
            got += count - want;
            equipped += count - want;
            if (want == 0 || !targeted || curFloor <= 1)
            {
                if (got == 0)
                    LOG_WARN("playerbots", "[RaidRoster] {}: no usable {} candidate (target ilvl {}).",
                             bot->GetName(), slotName, target);
                else if (got < wanted)
                    LOG_DEBUG("playerbots", "[RaidRoster] {}: filled {}/{} {} slot(s).",
                              bot->GetName(), got, wanted, slotName);
                return;
            }
            count = want;                                        // only chase the remainder
            curFloor = std::max<int32>(1, curFloor - OFF_WINDOW);  // widen and retry
        }
    };

    // Body slots the set stage may have covered. Level gates mirror the factory's
    // (no head/neck < 30, no rings < 20, no trinkets < 50) so low-level syncs don't
    // warn about slots that have no real itemization.
    if (!covered(EQUIPMENT_SLOT_HEAD) && level >= 30)
        fillSlot("head", { INVTYPE_HEAD });
    if (!covered(EQUIPMENT_SLOT_SHOULDERS))
        fillSlot("shoulders", { INVTYPE_SHOULDERS });
    if (!covered(EQUIPMENT_SLOT_CHEST))
        fillSlot("chest", { INVTYPE_CHEST, INVTYPE_ROBE });
    if (!covered(EQUIPMENT_SLOT_LEGS))
        fillSlot("legs", { INVTYPE_LEGS });
    if (!covered(EQUIPMENT_SLOT_HANDS))
        fillSlot("hands", { INVTYPE_HANDS });
    if (!covered(EQUIPMENT_SLOT_WAIST))
        fillSlot("waist", { INVTYPE_WAIST });
    if (!covered(EQUIPMENT_SLOT_FEET))
        fillSlot("feet", { INVTYPE_FEET });
    if (!covered(EQUIPMENT_SLOT_WRISTS))
        fillSlot("wrists", { INVTYPE_WRISTS });

    // Never set-covered.
    fillSlot("cloak", { INVTYPE_CLOAK });
    if (level >= 30)
        fillSlot("neck", { INVTYPE_NECK });
    if (level >= 20)
        fillSlot("rings", { INVTYPE_FINGER }, 2);
    if (level >= 50)
        fillSlot("trinkets", { INVTYPE_TRINKET }, 2);

    // Weapons / offhand / ranged / relic: greedy best-first across all weapon inventory
    // types (arena-roster pattern) — a pick blocked by an earlier one just fails the
    // equip probe, which naturally yields 2H vs dual-wield vs weapon+shield vs
    // weapon+held per spec, plus a class-legal ranged/relic/wand. Widen the floor only
    // while the main hand is still empty.
    {
        int32 curFloor = floorIlvl;
        while (true)
        {
            for (auto const& [score, entry] : RankedCandidates(bot, calc,
                     { INVTYPE_WEAPON, INVTYPE_2HWEAPON, INVTYPE_WEAPONMAINHAND,
                       INVTYPE_WEAPONOFFHAND, INVTYPE_SHIELD, INVTYPE_HOLDABLE,
                       INVTYPE_RANGED, INVTYPE_RANGEDRIGHT, INVTYPE_THROWN, INVTYPE_RELIC },
                     curFloor, ceilIlvl))
            {
                if (EquipEntry(bot, entry))
                    ++equipped;
            }
            if (bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND) ||
                !targeted || curFloor <= 1)
                break;
            curFloor = std::max<int32>(1, curFloor - OFF_WINDOW);
        }
        if (!bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND))
            LOG_WARN("playerbots", "[RaidRoster] {}: main hand left empty (target ilvl {}).",
                     bot->GetName(), target);
    }
    bot->AutoUnequipOffhandIfNeed();

    LOG_INFO("playerbots", "[RaidRoster] Geared {} (spec tab {}, level {}): {} piece(s), target ilvl {}, ilvl ceiling {}{}.",
             bot->GetName(), specTab, level, equipped, target, ceilIlvl,
             haveSet ? ", tier set assembled" : "");
    return equipped >= 8;
}

} // namespace RaidRosterGear
