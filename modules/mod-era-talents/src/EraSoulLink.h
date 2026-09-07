#ifndef MOD_ERA_TALENTS_SOUL_LINK_H
#define MOD_ERA_TALENTS_SOUL_LINK_H

#include <cstdint>

// The (visible buff, pet-cast split carrier) pairs of the era Soul Link implementation — ONE table
// shared by the three places that must agree on it:
//   * era_soul_link          (EraTalentProcScripts.cpp) — casts/strips the carrier on buff apply/remove
//   * era_soul_link_relink   (EraTalentProcScripts.cpp) — re-links the carrier to a resummoned demon
//   * era_talent_pin's 2s poll (EraTalentPin.cpp)       — drops a buff with no live demon and strips
//                                                          a carrier orphaned from its buff
// Adding an era means adding ONE row here. Both scripts and the pin walk the whole table, so a
// warlock can only ever hold the pair of the era whose node granted it (the buff ids are distinct
// band spells and ReconcileBaselineSpells strips the other era's on a band move).
//
// Why the split is PET-cast at all (both eras): Unit.cpp's SPLIT_DAMAGE_PCT loop finds split auras
// ON THE VICTIM and routes the redirected damage to each aura's CASTER, skipping caster==victim —
// so a warlock self-cast split would redirect nothing. The carrier is a pet SELF-cast whose
// SPELL_EFFECT_APPLY_AREA_AURA_OWNER (143) effect radiates the split to the owner with the pet as
// caster. See era-data/vanilla/warlock.yaml (932900/932901) and era-data/tbc/warlock.yaml
// (948550/948551) for the full data-side reasoning.
namespace EraSoulLink
{
    struct Pair
    {
        uint32 buff;    // the visible warlock self-buff the node grants (DUMMY amount = the split %)
        uint32 split;   // the hidden carrier the ACTIVE demon self-casts (effect 143 -> the owner)
    };

    // `constexpr` at namespace scope has internal linkage, so each TU gets its own copy — no ODR
    // concern, same idiom as EraTalents.h's band constants.
    constexpr Pair kPairs[] =
    {
        { 932900, 932901 },   // Vanilla (node 18233) — 30% split, no damage bonus
        { 948550, 948551 },   // TBC     (node 20739) — 20% split + 5% damage on both sides
    };
}

#endif // MOD_ERA_TALENTS_SOUL_LINK_H
