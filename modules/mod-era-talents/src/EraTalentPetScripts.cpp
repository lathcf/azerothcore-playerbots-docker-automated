// Era-talent PET-related scripts.
//
// 2026-08-16 REWORK (final warlock pass): the first-generation pet scripts here (era_ivw_threat/
// heal/absorb, era_ifb_apply, era_ilop_cooldown) were REPLACED by plain owner-side SPELLMOD
// passives in the dataset — the stock WotLK mechanism. Rationale (all tool-verified against this
// fork's Spell.dbc + core source):
//   * A pet's spell cast resolves the OWNER's spellmods: SpellEffectInfo::CalcValue /
//     SpellInfo::CalcCastTime / Creature::AddSpellCooldown all call GetSpellModOwner()->
//     ApplySpellMod(...), and a warlock demon's GetSpellModOwner() is the warlock.
//   * The pet abilities are family 5 with real class masks (Firebolt 4096, Lash of Pain 8192,
//     all four Voidwalker abilities share 33554432) — dump_spell_family 2026-08-16.
//   * Stock WotLK "Demonic Power" (18126) is EXACTLY this: Effect_1 = castTime spellmod masked to
//     Firebolt, Effect_2 = cooldown spellmod masked to Lash of Pain. Client tooltips follow the
//     owner's spellmods too, which the scripted variants could never show.
// So Improved Firebolt (18239), Improved Lash of Pain (18240) and Improved Voidwalker (18222) are
// now `mechanic: spellmod` nodes in era-data/vanilla/warlock.yaml and need no C++ at all.
//
// What remains here is Master Demonologist (node 18232), which cannot be a spellmod (it grants
// stat/threat/resist effects keyed to WHICH demon is active, one branch at a time).
#include "ScriptMgr.h"
#include "SpellScript.h"
#include "SpellScriptLoader.h"
#include "SpellAuraEffects.h"
#include "SpellMgr.h"          // sSpellMgr — MD_Apply's "is this era's carrier shipped?" guard
#include "SpellInfo.h"
#include "Pet.h"
#include "PetScript.h"
#include "Creature.h"
#include "Player.h"
#include "Log.h"
#include "EraTalentsConfig.h"
#include "EraTalents.h"        // EraTalents::CurrentRank + EraId (ERA_VANILLA / ERA_TBC)
#include "EraTalentBots.h"     // EraTalentBots::EraFor — the canonical per-character era (never bare EraFromIP)

// Reserved era custom-spell band [920000, 950000) — every band-gated marker read here must live in it
// (matches the CLAUDE.md reserved-band invariant and the per-script readers in EraTalentProcScripts.cpp).
static constexpr uint32 ERA_BAND_LOW = EraTalents::ERA_CUSTOM_BAND_LOW, ERA_BAND_HIGH = EraTalents::ERA_CUSTOM_BAND_HIGH;

// The amount from a band-gated era DUMMY marker on `unit` (a `mechanic: scripted` talent passive).
// Returns 0 for a null unit or one without the marker, so every bound script is inert unless the
// warlock carries the Vanilla talent (era-safe — the bindings are global).
static int32 EraBandDummyMarkerAmount(Unit* unit, int32 marker)
{
    if (!unit)
        return 0;
    for (AuraEffect const* eff : unit->GetAuraEffectsByType(SPELL_AURA_DUMMY))
        if (eff->GetMiscValue() == marker)
        {
            uint32 id = eff->GetSpellInfo()->Id;
            if (id >= ERA_BAND_LOW && id < ERA_BAND_HIGH) // reserved era custom-spell band
                return eff->GetAmount();
        }
    return 0;
}

// Master Demonologist (Vanilla node 18232 / TBC node 20737, marker 9 in both). A PetScript that, on
// every demon summon, applies the branch matching the CURRENT active demon. Keys on
// EraTalents::CurrentRank(owner, <the owner's era>, <that era's node id>) + the demon's entry
// (per-branch magnitudes live here, not in the data); marker 9 exists only to make the node
// learnable/inert.
//
// ERA-DRIVEN (2026-09-05, TBC Demonology): the era comes from EraTalentBots::EraFor(owner) — the
// canonical helper (BotEra for bots, EraFromIP for players), NEVER bare EraFromIP, which reads
// Vanilla for every bot and would have silently given TBC-band warlock bots the Vanilla branches.
// Vanilla and TBC differ in BOTH the carriers and the magnitudes, and TBC adds a fifth demon:
//   branch      Vanilla (932907-910)                 TBC (948560-564)
//   Imp         -4%/rank threat                      same shape, carrier 948560
//   Voidwalker  -2%/rank physical damage taken       same shape, carrier 948561
//   Succubus    +2%/rank all damage                  same shape, carrier 948562
//   Felhunter   +0.2*rank*level all resistances      same, but misc 126 (magic only) — carrier 948563
//   Felguard    (does not exist)                     +1%/rank all damage AND 0.1*rank*level resist
// The Felguard carrier 948564 is the only TWO-effect one, so MD_Apply takes an optional second
// amount (BASE_POINT1). Felhunter/Felguard resistance scales with the owner's LEVEL, so
// era_talent_pin's OnPlayerLevelChanged re-invoke keeps both fresh without a resummon.
//
// LIFECYCLE REWORK (2026-08-16): the four helpers 932907-910 are now SPELL_EFFECT_APPLY_AREA_AURA_PET
// (effect 119) rows CAST ON THE PET (pet = caster = target). Effect 119's target fill is
// "the aura holder + its owner" (SpellAuras.cpp UpdateTargetMap), i.e. the demon AND the warlock —
// exactly the stock WotLK Master Demonologist encoding (23759-62, radius index 12 = 100 yd,
// duration -1). Because the aura LIVES ON THE PET, the warlock-side application is auto-removed by
// the area-aura maintenance tick as soon as the demon is dismissed, dies, is sacrificed, or the
// aura is stripped — which fixes the old design's accepted limitation (a warlock-side buff cast on
// the warlock lingered while petless). The old pet-side-only helpers 932911/912 are retired (their
// branches now radiate to both sides from the single per-branch row).
namespace
{
    // 932911/912/915 are retired ids (pre-rework pet-side halves + the Improved Firebolt haste
    // carrier). Keep stripping them so characters that carried them across the upgrade are cleaned.
    // BOTH eras' carriers are stripped unconditionally on every refresh: an era transition (or a
    // bot level-band move) must never leave the other era's branch on the warlock or the demon.
    constexpr uint32 MD_HELPERS[] = {
        932907, 932908, 932909, 932910, 932911, 932912, 932915,   // Vanilla (+ retired ids)
        948560, 948561, 948562, 948563, 948564,                   // TBC (incl. the Felguard branch)
    };

    // Demon creature entries. 17252 (Felguard) exists only from TBC (talent node 20742).
    enum : uint32 { DEMON_IMP = 416, DEMON_FELHUNTER = 417, DEMON_VOIDWALKER = 1860,
                    DEMON_SUCCUBUS = 1863, DEMON_FELGUARD = 17252 };

    // `amount2` (default 0) fills BASE_POINT1 for the one two-effect carrier (TBC Felguard 948564:
    // +damage% AND +resistance). A PetScript has no Validate() hook, so the sSpellMgr lookup here IS
    // the guard: a not-yet-shipped era id (TBC before era-data/tbc/warlock.yaml enters the manifest)
    // is a silent no-op rather than a "spell 948564 does not exist" cast error every summon.
    void MD_Apply(Unit* pet, uint32 id, int32 amount, int32 amount2 = 0)
    {
        if (!sSpellMgr->GetSpellInfo(id))
            return;
        int32 bp1 = amount2;
        pet->CastCustomSpell(pet, id, &amount, amount2 ? &bp1 : nullptr, nullptr, true);   // pet self-cast; area aura radiates to owner
    }

    // Strip all MD helpers (both eras) from both sides, then apply the current demon's branch at the
    // owner's learned rank ON THE PET. Called on demon summon (OnPetAddToWorld), on the owner's
    // level-up (Felhunter/Felguard resist is level-scaled) and — via EraMasterDemonologist_Refresh —
    // from TryLearn/Reset so a respec or rank-up applies without a resummon. Inert when disabled,
    // unlearned, or the pet is not one of the era's demons.
    void MD_Refresh(Pet* pet)
    {
        if (!sEraTalentsConfig->Enabled() || !pet || !pet->IsAlive())
            return;
        Player* wl = pet->GetOwner();
        if (!wl)
            return;
        // EraFor, never EraFromIP: bots derive their era from the level band (IP progression is 0 for
        // every bot, i.e. "Vanilla" even at 70).
        EraId era = EraTalentBots::EraFor(wl);
        uint8 rank = EraTalents::CurrentRank(wl, era, era == ERA_TBC ? 20737 : 18232);
        for (uint32 id : MD_HELPERS) { wl->RemoveAurasDueToSpell(id); pet->RemoveAurasDueToSpell(id); }
        if (!rank)
            return;
        int32 lvl = int32(wl->GetLevel());
        if (era == ERA_TBC)
        {
            switch (pet->GetEntry())
            {
                case DEMON_IMP:        MD_Apply(pet, 948560, -4 * rank); break;   // -threat%
                case DEMON_VOIDWALKER: MD_Apply(pet, 948561, -2 * rank); break;   // -physical taken%
                case DEMON_SUCCUBUS:   MD_Apply(pet, 948562, 2 * rank);  break;   // +all dmg% (both sides)
                case DEMON_FELHUNTER:  MD_Apply(pet, 948563, int32(rank) * lvl / 5); break;  // 0.2*rank*level resist
                case DEMON_FELGUARD:   MD_Apply(pet, 948564, 1 * rank, int32(rank) * lvl / 10); break;
                                                                                  // +1%/rank dmg AND 0.1*rank*level resist
                default: break;
            }
            return;
        }
        switch (pet->GetEntry())
        {
            case DEMON_IMP:        MD_Apply(pet, 932907, -4 * rank); break;       // Imp: -threat%
            case DEMON_VOIDWALKER: MD_Apply(pet, 932908, -2 * rank); break;       // Voidwalker: -phys taken%
            case DEMON_SUCCUBUS:   MD_Apply(pet, 932909, 2 * rank); break;        // Succubus: +all dmg% (both sides)
            case DEMON_FELHUNTER:  MD_Apply(pet, 932910, int32(rank) * lvl / 5); break; // Felhunter: 0.2*rank*level resist
            default: break;                                                       // no Vanilla Felguard branch
        }
    }
}

class era_master_demonologist : public PetScript
{
public:
    era_master_demonologist() : PetScript("era_master_demonologist") {}

    void OnPetAddToWorld(Pet* pet) override { MD_Refresh(pet); }
};

// Re-apply the active demon's branch for the CURRENT talent state. Called from era_talent_pin's
// OnPlayerLevelChanged (Felhunter resist re-scale) and from EraTalents::TryLearn/Reset (a respec
// away from MD must strip the buff immediately — the old design left it until the next resummon,
// the exact "stays when I respec" bug from the 2026-08-16 real-client pass). No-op without a pet:
// with the area-aura rework a petless warlock cannot carry an MD buff in the first place.
void EraMasterDemonologist_Refresh(Player* wl)
{
    if (wl)
        if (Pet* pet = wl->GetPet())
            MD_Refresh(pet);
}

void EraMasterDemonologist_OnLevelUp(Player* wl)
{
    EraMasterDemonologist_Refresh(wl);
}

// Improved Enslave Demon (node 18229, marker 10). Bound (via scriptBindings) to every family-5 Enslave
// Demon rank. The CASTER of Enslave is the WARLOCK (a player), so the marker is read directly off
// GetCaster(). Enslave carries two penalty auras on the enslaved demon: EFFECT_1 =
// SPELL_AURA_MOD_MELEE_HASTE (aura 138, base -30% attack speed) and EFFECT_2 =
// SPELL_AURA_MOD_CASTING_SPEED_NOT_STACK (aura 65, base -20% casting speed) — layout verified
// consistent across ranks 1098/11725/11726/20882/61191. The DoEffectCalcAmount hook shrinks each
// (negative) penalty toward zero by the per-rank pct: CalculatePct(-30,10) = -3, so -30 - (-3) = -27
// (a smaller penalty). The Vanilla talent's "reduces resist chance" half is a documented no-op on
// this core (AzerothCore charm has no periodic break/resist mechanic — nothing to reduce).
// Inert without the warlock's misc-10 marker. Method name Shrink avoids the OnCast/etc HookList shadow.
class era_improved_enslave : public AuraScript
{
    PrepareAuraScript(era_improved_enslave);

    // Leaving canBeRecalculated untouched is safe: CalculateAmount recomputes the base penalty fresh
    // before this hook runs, so the per-rank shrink applies to the base each time and never compounds.
    void Shrink(AuraEffect const* /*aurEff*/, int32& amount, bool& /*canBeRecalculated*/)
    {
        if (!sEraTalentsConfig->Enabled())
            return;
        int32 pct = EraBandDummyMarkerAmount(GetCaster(), 10);   // GetCaster() = the warlock (player)
        if (pct <= 0)
            return;
        amount = amount - CalculatePct(amount, pct);       // penalty is negative; shrink it toward 0
    }

    void Register() override
    {
        DoEffectCalcAmount += AuraEffectCalcAmountFn(era_improved_enslave::Shrink, EFFECT_1, SPELL_AURA_MOD_MELEE_HASTE);
        DoEffectCalcAmount += AuraEffectCalcAmountFn(era_improved_enslave::Shrink, EFFECT_2, SPELL_AURA_MOD_CASTING_SPEED_NOT_STACK);
    }
};

void AddSC_era_talent_pet_scripts()
{
    RegisterSpellScript(era_improved_enslave);
    new era_master_demonologist();
}
