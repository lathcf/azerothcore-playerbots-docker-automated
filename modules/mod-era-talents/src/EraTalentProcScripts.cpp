// Registration entry point for era-talent scripted procs (mechanic: proc-scripted in the YAML
// dataset — see docs/superpowers/plans/2026-08-13-era-talents-phase2-mage.md, P2-12b-B5).
//
// Each script is a plain AzerothCore AuraScript registered via RegisterSpellScript against the
// per-rank custom passive id the generator emits into spell_dbc (with a matching spell_script_names
// row), exactly like a core spell_mage_* script — no module-specific machinery. The generator emits
// a proc-scripted passive as a SPELL_AURA_DUMMY effect whose amount is the rank's magnitude (pct),
// and a spell_proc row supplies the proc conditions (family mask + crit hit mask). The script reads
// aurEff->GetAmount() for the pct and casts the real stock trigger spell itself, so no player spells
// are created — we only reuse spells the WotLK client already knows.
#include "ScriptMgr.h"
#include "PetScript.h"
#include "SpellScript.h"
#include "SpellScriptLoader.h"
#include "SpellAuraEffects.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "Player.h"
#include "Pet.h"
#include "Group.h"
#include "Log.h"
#include "Random.h"
#include "EraTalentsConfig.h"
#include "EraTalents.h"         // EraTalents::ERA_CUSTOM_BAND_LOW/HIGH — shared reserved-band bounds
#include "EraTalentIP.h"        // EraFromIP + EraId (ERA_VANILLA) for the Sap era gate
#include "Playerbots.h"         // GET_PLAYERBOT_AI — bot detection (level-band era for bots)
#include "EraTalentBots.h"      // EraTalentBots::BotEra — bots derive era from level band, never EraFromIP
#include "EraSoulLink.h"        // EraSoulLink::kPairs — the (buff, split carrier) table, shared with EraTalentPin.cpp

#include <algorithm>
#include <set>
#include <cmath>                // M_PI — era_rog_mutilate_behind's arc test (core Spell.cpp:5894)

enum EraProcSpells
{
    // Stock WotLK spells the scripts reuse (never authored by this mod):
    SPELL_ERA_IGNITE_DOT            = 12654, // periodic Fire damage (SPELL_AURA_PERIODIC_DAMAGE)
    SPELL_ERA_MOE_ENERGIZE          = 29077, // Master of Elements mana restore
    SPELL_ERA_BLESSED_RECOVERY_HOT  = 27813, // Blessed Recovery HoT, rank 1 (SPELL_AURA_PERIODIC_HEAL,
                                              // 3 ticks) — always rank 1; the magnitude is carried by
                                              // the custom talent's DUMMY amount, not the HoT's rank.
};

// era_ignite — Ignite: a Fire-school spell crit applies a DoT worth `pct`% of the crit damage over
// the DoT's duration. Mirrors core spell_mage_ignite (DUMMY proc + CastDelayedSpellWithPeriodicAmount)
// but reads the per-rank pct from the passive's DUMMY amount instead of a real ranked spell chain.
class era_ignite : public AuraScript
{
    PrepareAuraScript(era_ignite);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_ERA_IGNITE_DOT });
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        if (!eventInfo.GetActor() || !eventInfo.GetProcTarget())
            return false;

        DamageInfo* damageInfo = eventInfo.GetDamageInfo();
        if (!damageInfo || !damageInfo->GetSpellInfo())
            return false;

        if (!(damageInfo->GetSchoolMask() & SPELL_SCHOOL_MASK_FIRE))
            return false;

        if (!(eventInfo.GetHitMask() & PROC_HIT_CRITICAL))
            return false;

        return true;
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& eventInfo)
    {
        PreventDefaultAction();

        SpellInfo const* igniteDot = sSpellMgr->AssertSpellInfo(SPELL_ERA_IGNITE_DOT);
        int32 pct = aurEff->GetAmount();                 // per-rank %, carried in the DUMMY amount
        int32 amount = int32(CalculatePct(eventInfo.GetDamageInfo()->GetDamage(), pct)
                             / igniteDot->GetMaxTicks());

        eventInfo.GetProcTarget()->CastDelayedSpellWithPeriodicAmount(
            eventInfo.GetActor(), SPELL_ERA_IGNITE_DOT, SPELL_AURA_PERIODIC_DAMAGE, amount);
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(era_ignite::CheckProc);
        OnEffectProc += AuraEffectProcFn(era_ignite::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// era_master_of_elements — a Fire/Frost spell crit refunds `pct`% of the spell's base mana cost.
// Mirrors core spell_mage_master_of_elements (DUMMY proc + energize). Fire/Frost restriction comes
// from the generated spell_proc family mask; crit from the spell_proc HitMask (re-checked here).
class era_master_of_elements : public AuraScript
{
    PrepareAuraScript(era_master_of_elements);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_ERA_MOE_ENERGIZE });
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        DamageInfo* damageInfo = eventInfo.GetDamageInfo();
        if (!damageInfo || !damageInfo->GetSpellInfo())
            return false;

        if (!(eventInfo.GetHitMask() & PROC_HIT_CRITICAL))
            return false;

        return true;
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& eventInfo)
    {
        PreventDefaultAction();

        SpellInfo const* spellInfo = eventInfo.GetDamageInfo()->GetSpellInfo();

        // Refund is a fraction of the triggering spell's BASE mana cost (flat + % of base mana).
        int32 mana = spellInfo->ManaCost
                     + int32(CalculatePct(GetTarget()->GetCreateMana(), spellInfo->ManaCostPercentage));
        mana = CalculatePct(mana, aurEff->GetAmount());

        if (mana > 0)
            GetTarget()->CastCustomSpell(SPELL_ERA_MOE_ENERGIZE, SPELLVALUE_BASE_POINT0, mana,
                                         GetTarget(), true, nullptr, aurEff);
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(era_master_of_elements::CheckProc);
        OnEffectProc += AuraEffectProcFn(era_master_of_elements::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// era_ward_reflect — Improved Fire Ward / Frost Warding: while the mage's Fire/Frost Ward buff is
// up, X% chance to reflect spells of that school. The reflect must be conditional on the ward being
// active, so it can't be a static aura on the talent. Instead the talent grants a SPELL_AURA_DUMMY
// marker (misc 2 = fire ward, misc 3 = frost ward; amount = the %); this AuraScript, bound to every
// rank of Fire Ward (-543) and Frost Ward (-6143), applies a school reflect helper (932990/932991)
// for the ward's lifetime when the caster carries the marker, and removes it when the ward ends.
class era_ward_reflect : public AuraScript
{
    PrepareAuraScript(era_ward_reflect);

    enum : uint32
    {
        SPELL_ERA_FIRE_WARD_REFLECT  = 932990,
        SPELL_ERA_FROST_WARD_REFLECT = 932991,
        ERA_BAND_LOW                 = EraTalents::ERA_CUSTOM_BAND_LOW,
        ERA_BAND_HIGH                = EraTalents::ERA_CUSTOM_BAND_HIGH,
    };

    uint32 _reflectSpell = 0;
    int32 _marker = 0;

    bool Load() override
    {
        SpellSchoolMask school = GetSpellInfo()->GetSchoolMask();
        if (school & SPELL_SCHOOL_MASK_FIRE)
        {
            _reflectSpell = SPELL_ERA_FIRE_WARD_REFLECT;
            _marker = 2;
        }
        else if (school & SPELL_SCHOOL_MASK_FROST)
        {
            _reflectSpell = SPELL_ERA_FROST_WARD_REFLECT;
            _marker = 3;
        }
        return _reflectSpell != 0;
    }

    // The reflect % from the caster's ward-reflect DUMMY marker (band-gated, collision-proof).
    int32 MarkerAmount(Unit* caster) const
    {
        for (AuraEffect const* eff : caster->GetAuraEffectsByType(SPELL_AURA_DUMMY))
        {
            if (eff->GetMiscValue() != _marker)
                continue;

            uint32 id = eff->GetSpellInfo()->Id;
            if (id >= ERA_BAND_LOW && id < ERA_BAND_HIGH)
                return eff->GetAmount();
        }
        return 0;
    }

    void OnApply(AuraEffect const* aurEff, AuraEffectHandleModes /*mode*/)
    {
        Unit* target = GetTarget();
        int32 pct = MarkerAmount(target);
        if (pct <= 0)
            return;

        target->CastCustomSpell(target, _reflectSpell, &pct, nullptr, nullptr, true, nullptr, aurEff);
    }

    void OnRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        GetTarget()->RemoveAurasDueToSpell(_reflectSpell);
    }

    void Register() override
    {
        OnEffectApply += AuraEffectApplyFn(era_ward_reflect::OnApply, EFFECT_0, SPELL_AURA_SCHOOL_ABSORB, AURA_EFFECT_HANDLE_REAL);
        OnEffectRemove += AuraEffectRemoveFn(era_ward_reflect::OnRemove, EFFECT_0, SPELL_AURA_SCHOOL_ABSORB, AURA_EFFECT_HANDLE_REAL);
    }
};

// era_blessed_recovery — Blessed Recovery: after being struck by a melee or ranged critical hit,
// heal `pct`% of the damage taken over 6 sec. Mirrors core spell_pri_blessed_recovery (DUMMY proc +
// CastCustomSpell of the stock HoT, amount = CalculatePct(damage, pct) / GetMaxTicks()), but reads
// the per-rank pct (8/16/25, the authentic Vanilla magnitude) from this talent's own DUMMY amount
// instead of the stock talent's WotLK-retuned PROC_TRIGGER_SPELL effect (5/10/15%), and always casts
// HoT 27813 rank 1 — the magnitude lives in the DUMMY amount, not the HoT's rank.
class era_blessed_recovery : public AuraScript
{
    PrepareAuraScript(era_blessed_recovery);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_ERA_BLESSED_RECOVERY_HOT });
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        DamageInfo* damageInfo = eventInfo.GetDamageInfo();
        if (!damageInfo || !damageInfo->GetDamage())
            return false;

        return true;
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& eventInfo)
    {
        PreventDefaultAction();

        Unit* target = GetTarget(); // aura owner == the priest who was struck

        SpellInfo const* hotInfo = sSpellMgr->AssertSpellInfo(SPELL_ERA_BLESSED_RECOVERY_HOT);
        int32 pct = aurEff->GetAmount();                  // per-rank %, carried in the DUMMY amount
        int32 bp = CalculatePct(int32(eventInfo.GetDamageInfo()->GetDamage()), pct);
        bp /= hotInfo->GetMaxTicks();

        target->CastCustomSpell(target, SPELL_ERA_BLESSED_RECOVERY_HOT, &bp, nullptr, nullptr,
                                true, nullptr, aurEff);
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(era_blessed_recovery::CheckProc);
        OnEffectProc += AuraEffectProcFn(era_blessed_recovery::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// era_vampiric_embrace — Vampiric Embrace, BOTH managed eras. The castable puts a DUMMY debuff on
// the target and triggers a hidden proc-buff on the priest; this script is bound to that proc-buff
// (Vanilla 932950, TBC 948032) and is selected per era by the id it is running on. On dealing Shadow
// damage to a mob that carries the priest's VE debuff, heal every party member (incl. the priest)
// for the proc-passive's own DUMMY amount (Vanilla 20%, TBC 15%) + Improved VE (read from the
// caster's band-gated DUMMY marker misc 4, +5/rank in both eras). Damage to an un-debuffed mob heals
// nothing. Vanilla behavior is unchanged: same ids, same amounts, same party scope.
class era_vampiric_embrace : public AuraScript
{
    PrepareAuraScript(era_vampiric_embrace);

    enum : uint32
    {
        ERA_BAND_LOW          = EraTalents::ERA_CUSTOM_BAND_LOW,
        ERA_BAND_HIGH         = EraTalents::ERA_CUSTOM_BAND_HIGH,
        ERA_VE_IMPROVED_MISC  = 4,
    };

    // One row per era: {proc-passive this script is bound to, the castable debuff, the heal payload}.
    // The TBC ids are FIXED BY THE PHASE-6 PLAN and are allocated by Task 8; if that task allocates
    // different ids it MUST update this table in the same commit.
    struct VeIds { uint32 procPassive; uint32 debuff; uint32 heal; };
    static constexpr VeIds kVeIds[] = {
        { 932950, 921152, 932951 },   // Vanilla (Phase 3)
        { 948032, 948031, 948033 },   // TBC (Phase 6)
    };
    static VeIds const* IdsFor(uint32 procPassiveId)
    {
        for (VeIds const& r : kVeIds)
            if (r.procPassive == procPassiveId)
                return &r;
        return nullptr;
    }

    // Party-heal range sanity gate (in addition to the SameSubGroup party-scope check below).
    static constexpr float VE_HEAL_RANGE = 100.0f;

    bool Validate(SpellInfo const* spellInfo) override
    {
        // Validate PER BINDING: _SpellScript::_Validate drops the script from the spell being bound,
        // so a shared {932951, 948033} list would fail the live Vanilla binding whenever the TBC heal
        // is absent. Each proc-passive validates only its own heal payload.
        VeIds const* ids = IdsFor(spellInfo->Id);
        return ids && ValidateSpellInfo({ ids->heal });
    }

    int32 ImprovedBonus(Unit* caster) const
    {
        for (AuraEffect const* eff : caster->GetAuraEffectsByType(SPELL_AURA_DUMMY))
        {
            if (eff->GetMiscValue() != int32(ERA_VE_IMPROVED_MISC))
                continue;
            uint32 id = eff->GetSpellInfo()->Id;
            if (id >= ERA_BAND_LOW && id < ERA_BAND_HIGH)
                return eff->GetAmount();
        }
        return 0;
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        VeIds const* ids = IdsFor(GetId());
        if (!ids)
            return false;

        DamageInfo* damageInfo = eventInfo.GetDamageInfo();
        if (!damageInfo || !damageInfo->GetDamage())
            return false;
        if (!(damageInfo->GetSchoolMask() & SPELL_SCHOOL_MASK_SHADOW))
            return false;

        Unit* victim = eventInfo.GetProcTarget();
        Unit* caster = GetTarget();                 // proc-passive owner == the priest
        return victim && caster && victim->HasAura(ids->debuff, caster->GetGUID());
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& eventInfo)
    {
        PreventDefaultAction();

        VeIds const* ids = IdsFor(GetId());
        if (!ids)
            return;

        Unit* caster = GetTarget();
        int32 pct = aurEff->GetAmount() + ImprovedBonus(caster);   // base (20 Vanilla / 15 TBC) + 0/5/10
        int32 heal = CalculatePct(int32(eventInfo.GetDamageInfo()->GetDamage()), pct);
        if (heal <= 0)
            return;

        Player* priest = caster->ToPlayer();
        Group* group = priest ? priest->GetGroup() : nullptr;
        if (group)
        {
            for (GroupReference* itr = group->GetFirstMember(); itr; itr = itr->next())
            {
                Player* member = itr->GetSource();
                if (member && group->SameSubGroup(priest, member) && member->IsInMap(caster)
                    && member->IsAlive() && caster->GetDistance(member) <= VE_HEAL_RANGE)
                    caster->CastCustomSpell(member, ids->heal, &heal, nullptr, nullptr,
                                            true, nullptr, aurEff);
            }
        }
        else
        {
            caster->CastCustomSpell(caster, ids->heal, &heal, nullptr, nullptr, true,
                                    nullptr, aurEff);
        }
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(era_vampiric_embrace::CheckProc);
        OnEffectProc += AuraEffectProcFn(era_vampiric_embrace::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// era_demonic_sacrifice — Demonic Sacrifice, BOTH eras (Vanilla node 18230 / TBC node 20734,
// Demonology). Each era's node grants its OWN band clone of the stock cast 18788, whose
// SPELL_EFFECT_INSTAKILL(TARGET_UNIT_PET) is the sacrifice; this SpellScript supplies the
// demon->benefit routing the WotLK client lost when the talent was removed (18788 now casts with no
// buff). On cast it reads the active demon's creature entry, casts the matching 30-min benefit buff
// on the warlock, and fully removes the demon. The benefit buffs are CUSTOM band clones of their
// stock analogs (18789-92, and 35701 for the Felguard) at each era's own magnitudes; every clone
// preserves the stock Effect_2 OVERRIDE_CLASS_SCRIPTS (misc 2228) resummon-cancel link. Client aura
// tooltips ride the patch-V MPQ (see the era-data helper blocks).
//
// ERA-TABLE-DRIVEN, keyed by the CAST SPELL ID (kDsRows) rather than by EraFor(caster): the id that
// reached this script IS the era, because it is the clone that era's node granted — no per-character
// era lookup, and a warlock who somehow held both clones would still get the right buff for the one
// he cast. TBC adds the **Felguard (entry 17252)** column; Vanilla has no Felguard, so its row is 0
// there and CheckCast refuses the sacrifice (never consume a demon we cannot reward).
//   Vanilla 932929: Imp 932903 +15% Fire | VW 932905 3% HP/4s | Succ 932904 +15% Shadow | FH 932906 2% mana/4s
//   TBC     948552: Imp 948553 +15% Fire | VW 948554 2% HP/4s | Succ 948555 +15% Shadow | FH 948556 3% mana/4s
//                   | Felguard 948557 +10% Shadow AND 2% mana/4s
// (the HP/mana values really do swap between the eras — wago 2.5.4 matches live, Vanilla does not).
// The demon is removed in OnCast (before Spell::handle_immediate), so 18788's own INSTAKILL self-bails:
// RemovePet -> RemoveFromWorld synchronously removes the pet from the map's object store, so when
// handle_immediate runs Spell::DoAllEffectOnTarget re-resolves the pet's GUID to null and returns
// before EffectInstaKill (INSTAKILL is not a far-unit-target effect) — its handler never runs, no
// dangling deref. The pet is read ONCE into a local Pet* and never touched after RemovePet. Inert
// (bails in every hook) when the module is disabled. RemovePet (not INSTAKILL) is what gives the
// authentic "fully consumed / non-resurrectable" sacrifice — a killed pet would leave a rezzable corpse.
class era_demonic_sacrifice : public SpellScript
{
    PrepareSpellScript(era_demonic_sacrifice);

    enum : uint32
    {
        DEMON_IMP            = 416,
        DEMON_FELHUNTER      = 417,
        DEMON_VOIDWALKER     = 1860,
        DEMON_SUCCUBUS       = 1863,
        DEMON_FELGUARD       = 17252,  // TBC only (node 20742 Summon Felguard)
    };

    // One row per era, keyed by the CAST spell id the node granted. `felguard` is 0 where the era
    // has no Felguard at all (Vanilla) — CheckCast then refuses the cast for that demon.
    struct DsRow { uint32 castSpell; uint32 imp, voidwalker, succubus, felhunter, felguard; };
    static constexpr DsRow kDsRows[] =
    {
        { 932929, 932903, 932905, 932904, 932906, 0      },   // Vanilla (no Felguard)
        { 948552, 948553, 948554, 948555, 948556, 948557 },   // TBC
    };

    static DsRow const* RowFor(uint32 castSpell)
    {
        for (DsRow const& r : kDsRows)
            if (r.castSpell == castSpell)
                return &r;
        return nullptr;
    }

    // The benefit buff for (this era's cast, this demon), or 0 when there is none. Because
    // Validate() cannot cover the TBC ids (see below), the spell's EXISTENCE is checked here too —
    // that way CheckCast and HandleCast make the same decision and a not-yet-shipped era dataset
    // REFUSES the cast instead of consuming the demon for no reward.
    uint32 BuffForDemon(uint32 castSpell, uint32 entry) const
    {
        DsRow const* row = RowFor(castSpell);
        if (!row)
            return 0;
        uint32 id = 0;
        switch (entry)
        {
            case DEMON_IMP:        id = row->imp;        break;
            case DEMON_VOIDWALKER: id = row->voidwalker; break;
            case DEMON_SUCCUBUS:   id = row->succubus;   break;
            case DEMON_FELHUNTER:  id = row->felhunter;  break;
            case DEMON_FELGUARD:   id = row->felguard;   break;   // 0 in the Vanilla row -> refused
            default:               return 0;
        }
        return (id && sSpellMgr->GetSpellInfo(id)) ? id : 0;
    }

    // VANILLA IDS ONLY, deliberately: Validate() failing UNREGISTERS the script for every bound
    // spell, so listing the TBC buffs here would kill Vanilla Demonic Sacrifice on any deployment
    // where era-data/tbc/warlock.yaml has not shipped yet (it is not in era-data/datasets.txt until
    // Task 5). Every era's ids are re-checked at cast time by BuffForDemon's sSpellMgr lookup.
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ kDsRows[0].imp, kDsRows[0].voidwalker,
                                   kDsRows[0].succubus, kDsRows[0].felhunter });
    }

    SpellCastResult CheckCast()
    {
        if (!sEraTalentsConfig->Enabled())
            return SPELL_FAILED_DONT_REPORT;

        Player* wl = GetCaster() ? GetCaster()->ToPlayer() : nullptr;
        Pet* pet = wl ? wl->GetPet() : nullptr;
        if (!wl || !pet)
            return SPELL_FAILED_NO_PET;

        // Guard must AGREE with HandleCast: never sacrifice a demon we can't reward. Without this,
        // a demon this era has no row for (a Felguard under Vanilla, an enslaved demon under either)
        // would let the INSTAKILL consume the pet for no buff.
        if (!BuffForDemon(GetSpellInfo()->Id, pet->GetEntry()))
            return SPELL_FAILED_BAD_TARGETS;

        return SPELL_CAST_OK;
    }

    void HandleCast()
    {
        if (!sEraTalentsConfig->Enabled())
            return;

        Player* wl = GetCaster() ? GetCaster()->ToPlayer() : nullptr;
        if (!wl)
            return;

        Pet* pet = wl->GetPet();                 // resolved ONCE; never dereferenced after RemovePet
        if (!pet)
            return;

        uint32 buffId = BuffForDemon(GetSpellInfo()->Id, pet->GetEntry());
        if (!buffId)
            return;                              // unmapped demon: CheckCast already rejected it, belt-and-suspenders

        wl->CastSpell(wl, buffId, true);         // era benefit buff, cast while the demon is still valid
        wl->RemovePet(pet, PET_SAVE_AS_DELETED); // fully sacrifice the demon (removed from world; INSTAKILL self-bails)
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(era_demonic_sacrifice::CheckCast);
        OnCast += SpellCastFn(era_demonic_sacrifice::HandleCast);
    }
};

// era_soul_link — Soul Link, BOTH eras (Vanilla node 18233 buff 932900 / TBC node 20739 buff
// 948550): while the warlock's visible Soul Link buff is active, a % of all damage the warlock takes
// is redirected to the active demon (Vanilla 30%, TBC 20% + a 5% damage bonus that is pure data on
// the pair). The redirect itself is the core's generic SPELL_AURA_SPLIT_DAMAGE_PCT handler
// (Unit.cpp): it finds split auras ON THE VICTIM (the warlock) and routes the split to each aura's
// CASTER, skipping caster==victim — so the split aura must reach the warlock WITH THE PET AS CASTER.
// 2026-08-16 REWORK: the original bridge had the pet cast the carrier directly AT the owner
// (932901 targetA 21); that pet->owner cast silently never landed the aura (live-diagnosed via
// doctor: 932900 applied, pet in world, both script paths run, no 932901 on the warlock). The
// carrier is now a pet SELF-CAST whose SPELL_EFFECT_APPLY_AREA_AURA_OWNER (143) effect radiates
// the split aura to the owner with the pet as caster — the same mechanism the Master Demonologist
// helpers use, live-verified this pass. On buff-apply the ACTIVE demon self-casts the era's carrier
// with the split % from the buff's DUMMY amount; on buff-remove it is stripped from both sides. The
// area aura dies with the pet, so a dead/dismissed demon never leaves a stale carrier. Crash-safe:
// the pet is resolved freshly here and never stored. OnApply only covers "the buff was applied while
// a demon is already out"; re-linking a resummoned demon (and the login case, where the buff is
// re-applied before LoadPet resummons the stored demon) is era_soul_link_relink below.
// ERA-TABLE-DRIVEN: which carrier to cast comes from EraSoulLink::kPairs keyed on the BUFF id that
// invoked this script (GetId()) — the buff IS the era, so no per-character era lookup is needed.
class era_soul_link : public AuraScript
{
    PrepareAuraScript(era_soul_link);

    // The split carrier paired with the buff that invoked this script, or 0 if it is not one of ours.
    uint32 SplitFor(uint32 buffId) const
    {
        for (EraSoulLink::Pair const& p : EraSoulLink::kPairs)
            if (p.buff == buffId)
                return p.split;
        return 0;
    }

    // VANILLA PAIR ONLY: Validate() failing unregisters the script for EVERY bound spell, and the
    // TBC ids do not exist until era-data/tbc/warlock.yaml enters era-data/datasets.txt — listing
    // them would take Vanilla Soul Link down with them. TBC's carrier is checked at cast time.
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ EraSoulLink::kPairs[0].split });
    }

    void OnApply(AuraEffect const* aurEff, AuraEffectHandleModes /*mode*/)
    {
        if (!sEraTalentsConfig->Enabled())
            return;

        uint32 split = SplitFor(GetId());
        if (!split || !sSpellMgr->GetSpellInfo(split))
            return;                                   // era's carrier not shipped -> inert, no half-link

        Player* wl = GetTarget() ? GetTarget()->ToPlayer() : nullptr;
        Pet* pet = wl ? wl->GetPet() : nullptr;       // resolved ONCE; never stored past this call
        if (!wl || !pet || !pet->IsAlive())
            return;                                   // no live demon out yet: era_soul_link_relink
                                                      // re-links when a demon enters the world

        int32 pct = aurEff->GetAmount();              // 30 (Vanilla) / 20 (TBC), from the buff's DUMMY
        // Pet self-cast; the area-aura-owner effect radiates the split to the warlock (caster = pet).
        pet->CastCustomSpell(pet, split, &pct, nullptr, nullptr, true);
    }

    void OnRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        Unit* wl = GetTarget();
        uint32 split = SplitFor(GetId());
        if (!wl || !split)
            return;
        // The carrier lives ON THE PET (the owner-side application follows it via area-aura
        // maintenance); strip both sides so cancelling Soul Link ends the redirect immediately.
        if (Player* p = wl->ToPlayer())
            if (Pet* pet = p->GetPet())
                pet->RemoveAurasDueToSpell(split);
        wl->RemoveAurasDueToSpell(split);
    }

    void Register() override
    {
        OnEffectApply  += AuraEffectApplyFn(era_soul_link::OnApply,  EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
        OnEffectRemove += AuraEffectRemoveFn(era_soul_link::OnRemove, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

// era_soul_link_relink — the missing half of Soul Link: era_soul_link (above) casts the split
// carrier ONCE at buff-apply, but the split aura's caster is a fixed pet GUID. When the demon dies
// the core safely skips the dead caster (zero redirect), and a RESUMMONED demon never re-casts the
// carrier — so without this the buff would show but redirect nothing for the rest of the session.
// Soul Link re-links to each new demon in every era ("lasts as long as the demon is active"), so
// re-cast the carrier from every demon that enters the world while the buff is active. Strip-first
// makes it idempotent and also fixes the login ordering (ReconcileBaselineSpells re-applies the buff
// before LoadPet resummons the stored demon, so era_soul_link::OnApply sees no pet). Crash-safe: the
// pet is live (just added to world) and never stored past the cast. Walks the SAME EraSoulLink pair
// table as era_soul_link, so a new era needs no edit here.
class era_soul_link_relink : public PetScript
{
public:
    era_soul_link_relink() : PetScript("era_soul_link_relink") {}

    void OnPetAddToWorld(Pet* pet) override
    {
        if (!sEraTalentsConfig->Enabled() || !pet || !pet->IsAlive())
            return;

        Player* wl = pet->GetOwner();
        if (!wl)
            return;

        for (EraSoulLink::Pair const& p : EraSoulLink::kPairs)
        {
            AuraEffect const* slink = wl->GetAuraEffect(p.buff, EFFECT_0);   // this era's buff active?
            if (!slink || !sSpellMgr->GetSpellInfo(p.split))
                continue;

            wl->RemoveAurasDueToSpell(p.split);     // clear any stale owner-side application -> exactly one
            pet->RemoveAurasDueToSpell(p.split);    // idempotent re-summon safety
            int32 pct = slink->GetAmount();         // 30 (Vanilla) / 20 (TBC), from the buff's DUMMY
            pet->CastCustomSpell(pet, p.split, &pct, nullptr, nullptr, true);   // self-cast; area aura -> owner
        }
    }
};

// era_improved_drain_mana — Improved Drain Mana (18212, Affliction). Bound to STOCK Drain Mana (5138).
// On each mana-leech periodic tick, if the warlock (GetCaster()) carries the band-gated marker (misc
// 11, minted by the `scripted` node's hidden DUMMY passive 921696/921697), deal pct% of the mana
// ACTUALLY drained that tick as Shadow damage via carrier 932914. NB: Drain Mana is a percentage
// drain, so aurEff->GetAmount() is the raw base (3), NOT the drained mana — OnPeriodic mirrors the
// core's HandlePeriodicManaLeechAuraTick to recover the real amount. Inert without the marker —
// stock Drain Mana is never mutated (WotLK warlocks unaffected),
// exactly like era_ward_reflect observing stock Fire/Frost Ward.
class era_improved_drain_mana : public AuraScript
{
    PrepareAuraScript(era_improved_drain_mana);

    enum : uint32
    {
        SPELL_ERA_DRAIN_MANA_DMG = 932914,
        ERA_BAND_LOW             = EraTalents::ERA_CUSTOM_BAND_LOW,
        ERA_BAND_HIGH            = EraTalents::ERA_CUSTOM_BAND_HIGH,
        MARKER                   = 11,
    };

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_ERA_DRAIN_MANA_DMG });
    }

    // The pct from the caster's band-gated Improved Drain Mana DUMMY marker (collision-proof).
    int32 MarkerPct(Unit* caster) const
    {
        if (!caster)
            return 0;
        for (AuraEffect const* eff : caster->GetAuraEffectsByType(SPELL_AURA_DUMMY))
        {
            if (eff->GetMiscValue() != int32(MARKER))
                continue;
            uint32 id = eff->GetSpellInfo()->Id;
            if (id >= ERA_BAND_LOW && id < ERA_BAND_HIGH)
                return eff->GetAmount();
        }
        return 0;
    }

    void OnPeriodic(AuraEffect const* aurEff)
    {
        if (!sEraTalentsConfig->Enabled())
            return;

        Unit* caster = GetCaster();
        Unit* target = GetTarget();
        if (!caster || !target)
            return;

        int32 pct = MarkerPct(caster);
        if (pct <= 0)
            return;

        // Drain Mana (5138) is a PERCENTAGE drain (ManaCostPercentage != 0): the core's
        // HandlePeriodicManaLeechAuraTick recomputes the REAL per-tick drain locally
        // (~GetAmount()% of the target's max power) but never writes it back to the aura amount,
        // so aurEff->GetAmount() here is the tiny raw base (3) and pct% of that floors to 0.
        // Mirror the core formula to get the ACTUAL mana drained this tick, then take pct% of THAT.
        // (Resilience reduction vs player targets is omitted — minor, and Drain Mana is PvE here.)
        Powers powerType = Powers(aurEff->GetMiscValue());        // Drain Mana => POWER_MANA
        int32 drain = std::max<int32>(aurEff->GetAmount(), 0);
        SpellInfo const* si = GetSpellInfo();
        if (si && si->ManaCostPercentage)
        {
            int32 maxmana = CalculatePct(caster->GetMaxPower(powerType), drain * 2);
            ApplyPct(drain, target->GetMaxPower(powerType));      // drain = GetAmount()% of target max power
            if (drain > maxmana)
                drain = maxmana;
        }
        // The core caps the drain at the target's current power; OnEffectPeriodic fires BEFORE
        // the core drains this tick, so GetPower() here is the correct pre-drain amount.
        drain = std::min<int32>(drain, int32(target->GetPower(powerType)));

        int32 dmg = CalculatePct(drain, pct);
        if (dmg <= 0)
            return;

        caster->CastCustomSpell(target, SPELL_ERA_DRAIN_MANA_DMG, &dmg, nullptr, nullptr, true, nullptr, aurEff);
    }

    void Register() override
    {
        // Bind to Drain Mana's mana-leech periodic (EFFECT_0, aura 64 SPELL_AURA_PERIODIC_MANA_LEECH —
        // confirmed via tools/dump_spell_effects.py on 5138: Effect_1=APPLY_AURA, EffectAura_1=64).
        OnEffectPeriodic += AuraEffectPeriodicFn(era_improved_drain_mana::OnPeriodic, EFFECT_0, SPELL_AURA_PERIODIC_MANA_LEECH);
    }
};

// era_improved_drain_soul — Improved Drain Soul (Vanilla node 18204 / TBC node 20703, Affliction).
// proc-scripted in BOTH datasets: the generator mints a per-rank hidden DUMMY passive + a spell_proc
// (PROC_FLAG_KILL) + a spell_script_names row bound to this AuraScript. Two halves, both era-specific:
//   (a) On-kill reward — CheckProc gates the kill proc on "the warlock is CURRENTLY channeling Drain
//       Soul" (any rank: family 5 + flag 0x4000). The per-rank chance is rolled by spell_proc.Chance
//       (Vanilla 50%/100%; TBC is unconditional at 100%), so HandleProc just casts the era's reward:
//       Vanilla the +100% mana-regen buff 932913 (10 sec), TBC an ENERGIZE (948457) for 7%/15% of the
//       warlock's maximum mana. Kills while NOT channeling Drain Soul never reward.
//   (b) The always-on half — OnApply casts the era's passive on the warlock when the node passive
//       activates (learn/reset/login/level/era-transition, via ReconcileBaselineSpells re-learning the
//       passive); OnRemove strips it. Vanilla: 932917, 50% mana regen while casting. TBC: 948458, a
//       -5%/-10% SPELLMOD_THREAT over the Affliction set, amount supplied per cast.
// WHY THE TBC MANA HALF IS SCRIPTED AT ALL: TBC's talent e3 (SPELL_EFFECT_DUMMY 7/15) is byte-identical
// to live, but the core reaches it from spell_warl_drain_soul (spell_warlock.cpp:1225) via
// `caster->GetAuraOfRankedSpell(SPELL_WARLOCK_IMPROVED_DRAIN_SOUL_R1 = 18213)` — BY STOCK ID. An era
// warlock holds neither 18213 nor 18372, so the stock path can never see the authored passive.
// Method names (OnApply/OnRemove/CheckProc/HandleProc) match the sibling scripts' signatures exactly
// (cf. era_soul_link OnApply/OnRemove, era_vampiric_embrace CheckProc/HandleProc) — no HookList collision.
namespace
{
    // Era table. The node passive's own DUMMY amount is the row key: the Vanilla dataset ships
    // `vals: [100, 100]` (both magnitudes are fixed) and the TBC dataset ships `vals: [7, 15]` (the
    // rank's mana-return %), so the amount also identifies the RANK without a DB read on the learn
    // path — which matters because OnApply runs inside EraTalents::TryLearn, before the rank cache
    // is necessarily settled.
    struct EraDrainSoulRow
    {
        uint8  era;              // EraId this row serves
        int32  dummyPct;         // the node passive's DUMMY amount (== the dataset `vals:` entry)
        uint32 rewardSpell;      // cast on a qualifying killing blow
        bool   rewardIsManaPct;  // true  -> amount = dummyPct% of the caster's max mana (TBC energize)
                                 // false -> cast with the reward spell's own base points (Vanilla buff)
        uint32 alwaysOnSpell;    // applied while the node passive is active (0 = none)
        int32  alwaysOnAmount;   // 0 -> cast with the spell's own base points
    };
    constexpr EraDrainSoulRow kEraDrainSoulRows[] = {
        { ERA_VANILLA, 100, 932913, false, 932917,   0 },   // node 18204
        { ERA_TBC,       7, 948457, true,  948458,  -5 },   // node 20703 rank 1
        { ERA_TBC,      15, 948457, true,  948458, -10 },   // node 20703 rank 2
    };
    // Every always-on id in the table above. OnRemove strips the WHOLE set rather than just this
    // era's: it is idempotent (removing an absent aura is a no-op) and it also cleans an
    // era-transition leftover, the same reason workflow lesson 21 says to list the whole chain.
    constexpr uint32 kEraDrainSoulAlwaysOn[] = { 932917, 948458 };
}

class era_improved_drain_soul : public AuraScript
{
    PrepareAuraScript(era_improved_drain_soul);

    enum : uint32 { SPELL_REGEN_BUFF = 932913, SPELL_REGEN_WHILE_CAST = 932917 };

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        // DELIBERATELY the Vanilla pair only. A Validate failure disables the script for BOTH eras'
        // bindings, and the TBC helpers 948457/948458 only exist once era-data/tbc/warlock.yaml is in
        // era-data/datasets.txt (its generated SQL is what creates them). A missing id at cast time is
        // a logged no-op in Unit::CastSpell; a failed Validate would silently kill Vanilla too.
        return ValidateSpellInfo({ SPELL_REGEN_BUFF, SPELL_REGEN_WHILE_CAST });
    }

    // The row for this passive: the caster's era AND the passive's own DUMMY amount must both match.
    static EraDrainSoulRow const* RowFor(Player* owner, int32 dummyPct)
    {
        if (!owner)
            return nullptr;
        // EraFor, never bare EraFromIP: a bot's IP progression is 0, so EraFromIP reads every bot as
        // Vanilla and a TBC-band bot would silently take the Vanilla row.
        uint8 const era = static_cast<uint8>(EraTalentBots::EraFor(owner));
        for (EraDrainSoulRow const& row : kEraDrainSoulRows)
            if (row.era == era && row.dummyPct == dummyPct)
                return &row;
        return nullptr;
    }

    void OnApply(AuraEffect const* aurEff, AuraEffectHandleModes /*mode*/)
    {
        if (!sEraTalentsConfig->Enabled())
            return;
        Unit* t = GetTarget();
        if (!t || !aurEff)
            return;
        EraDrainSoulRow const* row = RowFor(t->ToPlayer(), aurEff->GetAmount());
        if (!row || !row->alwaysOnSpell)
            return;
        // The "exactly one always-on aura alive" invariant relies on EraTalents::TryLearn removing the
        // OLD rank BEFORE learning the new one (remove-before-learn ordering in EraTalents.cpp), so the
        // outgoing rank's OnRemove strips it before this OnApply re-adds it. A future refactor to
        // learn-before-remove would leave two passives briefly overlapping and would need OnRemove hardened.
        if (row->alwaysOnAmount)
        {
            int32 amount = row->alwaysOnAmount;
            t->CastCustomSpell(t, row->alwaysOnSpell, &amount, nullptr, nullptr, true);
        }
        else
            t->CastSpell(t, row->alwaysOnSpell, true);
    }

    void OnRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (Unit* t = GetTarget())
        {
            for (uint32 id : kEraDrainSoulAlwaysOn)
                t->RemoveAurasDueToSpell(id);
        }
    }

    bool CheckProc(ProcEventInfo& /*eventInfo*/)
    {
        if (!sEraTalentsConfig->Enabled())
            return false;

        Unit* caster = GetTarget();                          // the warlock (aura owner)
        if (!caster)
            return false;

        Spell* cur = caster->GetCurrentSpell(CURRENT_CHANNELED_SPELL);
        if (!cur)
            return false;

        // Match Drain Soul by family+flag, NOT a single id — a leveled warlock channels a higher rank
        // (1120/8288/8289/11675/27217/47855, all family 5 flag0 0x4000; verified via dump_spell_family).
        SpellInfo const* si = cur->GetSpellInfo();
        return si && si->SpellFamilyName == SPELLFAMILY_WARLOCK && (si->SpellFamilyFlags[0] & 0x4000);
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& /*eventInfo*/)
    {
        Unit* caster = GetTarget();
        if (!caster || !aurEff)
            return;
        EraDrainSoulRow const* row = RowFor(caster->ToPlayer(), aurEff->GetAmount());
        if (!row || !row->rewardSpell)
            return;
        // The per-rank chance was already rolled by spell_proc.Chance.
        if (!row->rewardIsManaPct)
        {
            caster->CastSpell(caster, row->rewardSpell, true);          // Vanilla: a fixed-magnitude buff
            return;
        }
        // TBC: the same computation the core does for stock 18371 (spell_warlock.cpp:1227) —
        // dummyPct% of the warlock's MAXIMUM mana, handed to the energize as base point 0.
        int32 amount = CalculatePct(static_cast<int32>(caster->GetMaxPower(POWER_MANA)), row->dummyPct);
        if (amount <= 0)
            return;
        caster->CastCustomSpell(caster, row->rewardSpell, &amount, nullptr, nullptr, true);
    }

    void Register() override
    {
        OnEffectApply  += AuraEffectApplyFn(era_improved_drain_soul::OnApply,  EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
        OnEffectRemove += AuraEffectRemoveFn(era_improved_drain_soul::OnRemove, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
        DoCheckProc    += AuraCheckProcFn(era_improved_drain_soul::CheckProc);
        OnEffectProc   += AuraEffectProcFn(era_improved_drain_soul::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// era_amplify_curse — Amplify Curse (Vanilla node 18209 / TBC node 20708, Affliction). Each era's node
// grants its own custom ACTIVE (Vanilla 932918, TBC 948450 — stock 18288 is WotLK's hidden PASSIVE and
// cannot be granted), which puts a hidden 30-sec SPELL_AURA_DUMMY marker on the warlock (instant, 3-min
// cooldown). This AuraScript, bound via scriptBindings to every family-5 rank of the curses each era
// amplifies, boosts the curse's magnitude when the caster carries THAT era's marker, then consumes it so
// ONLY the next curse is boosted.
// ERA TABLE (kEraAmplifyRows): the eras differ only in WHICH curses are amplified, so a marker + a pair
// of family-flag masks is the whole difference. Vanilla amplifies Curse of Weakness / Agony at +50% and
// Curse of Exhaustion at +20%; TBC amplifies Curse of Agony and CURSE OF DOOM at +50% and Curse of
// Exhaustion at +20%, and does NOT touch Curse of Weakness. Curse identity is the curse's own family-5
// flags, live-dumped 2026-09-05 (tools/dump_spell_family.py ip-dbc/Spell.dbc): CoW 702 word0 0x8000,
// CoA 980 word0 0x400, CoE 18223 word0 0x400000, Curse of Doom 603 AND 30910 word1 0x2 (both ranks
// family=5 mask=(0,2,0)). A curse the matched row does not name simply takes no boost, so a Vanilla
// warlock casting Curse of Doom — bound by the TBC dataset — is unaffected.
// The DoEffectCalcAmount hook is registered EFFECT_ALL / SPELL_AURA_ANY so a SINGLE script matches every
// curse's differing aura layout (CoW: MOD_ATTACK_POWER at EFFECT_0 + MOD_RESISTANCE_PCT at EFFECT_1; CoA:
// PERIODIC_DAMAGE at EFFECT_0; CoE: MOD_DECREASE_SPEED at EFFECT_0; CoD: PERIODIC_DAMAGE at EFFECT_0) and
// scales each magnitude effect — WITHOUT the per-spell "did not match dbc effect data" boot error a fixed
// (index,auraType) hook would log against the curses that lack that effect (SpellScript.cpp:700).
// `amount += CalculatePct(amount, boost)` deepens negative amounts too (CoW's -AP, CoE's -slow) since
// CalculatePct(-200,50) = -100 — which is exactly why the boost stays in the script rather than in the
// wago spellmod effects the talent itself carries. canBeRecalculated is frozen when boosting so the
// amplified value persists for the debuff's full duration even after the marker is consumed on the same
// cast (CalcAmount runs during aura construction, before OnApply; both see the marker, then OnApply
// consumes it). Inert without a marker — a WotLK warlock's curses are never mutated (the bindings are
// global but marker-gated, era-safe), and it bails first on !Enabled().
namespace
{
    struct EraAmplifyCurseRow
    {
        uint32 marker;     // the era's Amplify Curse self-buff (the DUMMY marker this row is armed by)
        uint32 word0Mask;  // family-5 SpellFamilyFlags[0] bits this row amplifies
        uint32 word1Mask;  // family-5 SpellFamilyFlags[1] bits this row amplifies
    };
    constexpr EraAmplifyCurseRow kEraAmplifyRows[] = {
        // Vanilla 932918: Curse of Weakness | Curse of Agony | Curse of Exhaustion
        { 932918, 0x8000u | 0x400u | 0x400000u, 0x0u },
        // TBC 948450: Curse of Agony | Curse of Exhaustion | Curse of Doom (NO Curse of Weakness)
        { 948450,           0x400u | 0x400000u, 0x2u },
    };
    constexpr uint32 kCurseOfExhaustionFlag = 0x400000;   // family-5 word0 bit unique to Curse of Exhaustion
}

class era_amplify_curse : public AuraScript
{
    PrepareAuraScript(era_amplify_curse);

    // The first row whose marker the caster carries AND whose masks name THIS curse. A warlock can only
    // ever hold one era's marker (the other era's active is not in their spellbook), so "first" is
    // unambiguous; returning null means no boost and nothing to consume.
    EraAmplifyCurseRow const* MatchRow(Unit* caster) const
    {
        SpellInfo const* si = GetSpellInfo();
        if (!caster || !si)
            return nullptr;

        for (EraAmplifyCurseRow const& row : kEraAmplifyRows)
            if (caster->HasAura(row.marker)
                && ((si->SpellFamilyFlags[0] & row.word0Mask) || (si->SpellFamilyFlags[1] & row.word1Mask)))
                return &row;

        return nullptr;
    }

    void CalcBoost(AuraEffect const* /*aurEff*/, int32& amount, bool& canBeRecalculated)
    {
        if (!sEraTalentsConfig->Enabled())
            return;

        if (!MatchRow(GetCaster()))
            return;

        int32 boost = (GetSpellInfo()->SpellFamilyFlags[0] & kCurseOfExhaustionFlag) ? 20 : 50;
        amount += CalculatePct(amount, boost);   // negative amounts deepen: CalculatePct(-200,50) = -100
        canBeRecalculated = false;               // freeze the boosted amount for the debuff's lifetime
    }

    void OnApplyConsume(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (!sEraTalentsConfig->Enabled())
            return;

        Unit* caster = GetCaster();
        if (EraAmplifyCurseRow const* row = MatchRow(caster))
            caster->RemoveAurasDueToSpell(row->marker);   // only the NEXT curse is amplified
    }

    void Register() override
    {
        DoEffectCalcAmount += AuraEffectCalcAmountFn(era_amplify_curse::CalcBoost, EFFECT_ALL, SPELL_AURA_ANY);
        OnEffectApply += AuraEffectApplyFn(era_amplify_curse::OnApplyConsume, EFFECT_0, SPELL_AURA_ANY, AURA_EFFECT_HANDLE_REAL);
    }
};

// --- Vanilla Firestone / Spellstone (nodes 18231 / 18234 + the reconstructed Vanilla stones). ------
// The Vanilla stone ITEMS survive in item_template (1254/13699/13700/13701 firestones, 5522/13602/
// 13603 spellstones) but every behavior spell they referenced (758/17945/17947/17949, 23480-23483,
// 128/17729/17730, 32794/32795) is GONE from the 3.3.5 DBC — the items were dead shells. The module
// reconstructs the behavior as custom band helpers (era-data/vanilla/warlock.yaml: equip passives
// 932931-934 = +fire spell damage + a fiery-weapon-style melee proc via triggers 932935-938; use
// castables 932946-948 = self-dispel + 60s magic absorb; shared equip crit passive 932949) and a
// module SQL migration retargets the items' spellid columns onto them. The Improved talents are
// `mechanic: scripted` markers the wearer carries (12 = Improved Firestone +15/30%, 13 = Improved
// Spellstone +15/30%); these three scripts read the marker off the CASTER — equip/use item spells
// are self-cast by the wearing warlock — and scale the effect. Inert without the marker, so the
// stones themselves work identically for a Vanilla warlock without the talents.
namespace
{
    // Shared band-gated DUMMY-marker reader (same idiom as the per-class readers above).
    int32 EraStoneMarkerPct(Unit* caster, int32 marker)
    {
        if (!caster)
            return 0;
        for (AuraEffect const* eff : caster->GetAuraEffectsByType(SPELL_AURA_DUMMY))
        {
            if (eff->GetMiscValue() != marker)
                continue;
            uint32 id = eff->GetSpellInfo()->Id;
            if (id >= EraTalents::ERA_CUSTOM_BAND_LOW && id < EraTalents::ERA_CUSTOM_BAND_HIGH)
                return eff->GetAmount();
        }
        return 0;
    }
}

// Scales the firestone equip passive's +fire-spell-damage effect (EFFECT_0, aura 13 MOD_DAMAGE_DONE)
// by Improved Firestone's pct. Bound via scriptBindings to 932931-934.
class era_firestone_spellpower : public AuraScript
{
    PrepareAuraScript(era_firestone_spellpower);

    void CalcAmount(AuraEffect const* /*aurEff*/, int32& amount, bool& /*canBeRecalculated*/)
    {
        if (!sEraTalentsConfig->Enabled())
            return;
        int32 pct = EraStoneMarkerPct(GetCaster(), 12);   // marker 12 = Improved Firestone
        if (pct <= 0)
            return;
        amount += CalculatePct(amount, pct);
    }

    void Register() override
    {
        DoEffectCalcAmount += AuraEffectCalcAmountFn(era_firestone_spellpower::CalcAmount, EFFECT_0, SPELL_AURA_MOD_DAMAGE_DONE);
    }
};

// Scales the firestone melee-proc fire damage (932935-938, EFFECT_0 SPELL_EFFECT_SCHOOL_DAMAGE) by
// Improved Firestone's pct. Same OnEffectHitTarget/SetEffectValue pattern as the old era_ivw_threat.
class era_firestone_proc_dmg : public SpellScript
{
    PrepareSpellScript(era_firestone_proc_dmg);

    void ScaleDamage(SpellEffIndex /*effIndex*/)
    {
        if (!sEraTalentsConfig->Enabled())
            return;
        int32 pct = EraStoneMarkerPct(GetCaster(), 12);   // marker 12 = Improved Firestone
        if (pct <= 0)
            return;
        int32 value = GetEffectValue();
        SetEffectValue(value + CalculatePct(value, pct));
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(era_firestone_proc_dmg::ScaleDamage, EFFECT_0, SPELL_EFFECT_SCHOOL_DAMAGE);
    }
};

// Scales the spellstone use-buff's absorb shield (932946-948, EFFECT_1 SPELL_AURA_SCHOOL_ABSORB) by
// Improved Spellstone's pct (marker 13).
class era_spellstone_absorb : public AuraScript
{
    PrepareAuraScript(era_spellstone_absorb);

    void CalcAbsorb(AuraEffect const* /*aurEff*/, int32& amount, bool& /*canBeRecalculated*/)
    {
        if (!sEraTalentsConfig->Enabled())
            return;
        int32 pct = EraStoneMarkerPct(GetCaster(), 13);   // marker 13 = Improved Spellstone
        if (pct <= 0)
            return;
        amount += CalculatePct(amount, pct);
    }

    void Register() override
    {
        DoEffectCalcAmount += AuraEffectCalcAmountFn(era_spellstone_absorb::CalcAbsorb, EFFECT_1, SPELL_AURA_SCHOOL_ABSORB);
    }
};

// era_spellstone_crit_rating — scales a Spellstone EQUIP crit-rating passive (EFFECT_0, aura 189
// MOD_RATING) by the wearer's Improved Spellstone / Master Conjuror marker (misc 13). The Vanilla
// stones deliver +1% crit via 932949 (a MOD_SPELL_CRIT_CHANCE effect era_spellstone_absorb does not
// touch); the TBC Master Spellstone 948433 delivers +20 crit RATING, the value TBC's Master Conjuror
// says it raises by 15/30%. Bound via scriptBindings to 948433 only. Inert without the marker.
class era_spellstone_crit_rating : public AuraScript
{
    PrepareAuraScript(era_spellstone_crit_rating);

    void Scale(AuraEffect const* /*aurEff*/, int32& amount, bool& canBeRecalculated)
    {
        if (!sEraTalentsConfig->Enabled())
            return;
        int32 pct = EraStoneMarkerPct(GetCaster(), 13);   // marker 13 = Improved Spellstone / Master Conjuror
        if (pct <= 0)
            return;
        amount += CalculatePct(amount, pct);
        canBeRecalculated = false;
    }

    void Register() override
    {
        DoEffectCalcAmount += AuraEffectCalcAmountFn(era_spellstone_crit_rating::Scale, EFFECT_0, SPELL_AURA_MOD_RATING);
    }
};

// era_wyvern_sting — Hunter Wyvern Sting, BOTH eras (Vanilla node 18346, TBC node 20660): when the
// 12s sleep aura is removed (the target wakes early from damage or it expires), the caster applies
// the matching era wake-DoT. Mirrors core spell_hun_wyvern_sting (spell_hunter.cpp:139) line for
// line EXCEPT how the DoT is resolved: the core script uses
// sSpellMgr->GetSpellWithRank(24131, GetSpellInfo()->GetRank()), which on a band clone reads rank N
// of the STOCK 24131 chain (100/140/200/314 per 2s over 6s — the WotLK values), so every era sleep
// clone binds THIS script instead (scriptBindings: in era-data/<era>/hunter.yaml).
//
// TABLE-DRIVEN PER ERA (TBC Phase 7): Vanilla has ONE sleep rank, TBC has FOUR, and each TBC rank
// has its own DoT magnitude — so the DoT id cannot be a single constant any more. `DotFor` maps the
// sleep id this instance is running on to its DoT; an unlisted id casts nothing (fail-closed) rather
// than falling back to a wrong rank. VANILLA BEHAVIOUR IS BYTE-IDENTICAL: 932965 -> 932966, exactly
// what the retired SPELL_ERA_WYVERN_STING_DOT constant did.
//
// The DoT ids are AUTHORED band spells, not stock reuses — the deliberate exception to the
// EraProcSpells "stock only" note above, which is why they live in this local table.
class era_wyvern_sting : public AuraScript
{
    PrepareAuraScript(era_wyvern_sting);

    // {sleep clone this script is bound to, the wake-DoT it casts}. TBC ids are the Task-7
    // allocation in era-data/tbc/hunter.yaml (948330-948333 sleep r1-r4, 948334-948337 DoT r1-r4);
    // a re-allocation there MUST update this table in the same commit.
    struct WyvernIds { uint32 sleep; uint32 dot; };
    static constexpr WyvernIds kWyvernIds[] = {
        { 932965, 932966 },   // Vanilla (Phase 7 of the Vanilla project)
        { 948330, 948334 },   // TBC rank 1 — 50 Nature per 2 s over 12 s
        { 948331, 948335 },   // TBC rank 2 — 70
        { 948332, 948336 },   // TBC rank 3 — 100
        { 948333, 948337 },   // TBC rank 4 — 157
    };
    static uint32 DotFor(uint32 sleepId)
    {
        for (WyvernIds const& r : kWyvernIds)
            if (r.sleep == sleepId)
                return r.dot;
        return 0;
    }

    bool Validate(SpellInfo const* spellInfo) override
    {
        // Validate PER BINDING (the era_vampiric_embrace precedent): _SpellScript::_Validate drops
        // the script from the spell being bound, so a shared id list would fail the live Vanilla
        // binding whenever the TBC DoTs are absent (and vice versa). Each sleep validates only its own.
        uint32 dot = DotFor(spellInfo->Id);
        return dot && ValidateSpellInfo({ dot });
    }

    void HandleEffectRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (!sEraTalentsConfig->Enabled())
            return;
        uint32 dot = DotFor(GetId());
        if (!dot)
            return;
        if (Unit* caster = GetCaster())
            caster->CastSpell(GetTarget(), dot, true);
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectRemoveFn(era_wyvern_sting::HandleEffectRemove, EFFECT_0, SPELL_AURA_MOD_STUN, AURA_EFFECT_HANDLE_REAL);
    }
};

// era_hun_expose_weakness_tbc — TBC Expose Weakness (node 20661), bound to the payload clone 948345.
// TBC's debuff is worth "25% of your Agility" in RANGED and MELEE attack power to every attacker of
// the target. Auras 127 SPELL_AURA_RANGED_ATTACK_POWER_ATTACKER_BONUS and 165
// SPELL_AURA_MELEE_ATTACK_POWER_ATTACKER_BONUS take a FLAT amount on this core (Unit.cpp reads
// GetTotalAuraModifier on the VICTIM and adds it to the attacker's AP), so the scaling cannot live in
// spell data — the clone therefore ships basePoints 0 and this script supplies the amount on apply.
// A missing/disabled script leaves the debuff worth ZERO rather than a wrong flat 25 (accepted gap 9).
//
// Snapshot semantics: `canBeRecalculated = false` freezes the amount at apply time, which is what TBC
// did — the debuff is worth 25% of the hunter's Agility AT THE MOMENT THE CRIT LANDED, and does not
// follow the hunter's buffs for the remaining 7 s.
//
// Gated on the module enable flag + a null caster ONLY — never on "is a bot": an era-managed bot
// carries the same band spells and must get the same effect (CLAUDE.md, the bot Judgement no-op).
class era_hun_expose_weakness_tbc : public AuraScript
{
    PrepareAuraScript(era_hun_expose_weakness_tbc);

    static constexpr uint8 EW_AGILITY_PCT = 25;

    void CalcAmount(AuraEffect const* /*aurEff*/, int32& amount, bool& canBeRecalculated)
    {
        if (!sEraTalentsConfig->Enabled())
            return;

        Unit* caster = GetCaster();
        if (!caster)
            return;

        amount = CalculatePct(int32(caster->GetStat(STAT_AGILITY)), EW_AGILITY_PCT);
        canBeRecalculated = false;
    }

    void Register() override
    {
        DoEffectCalcAmount += AuraEffectCalcAmountFn(era_hun_expose_weakness_tbc::CalcAmount,
            EFFECT_0, SPELL_AURA_RANGED_ATTACK_POWER_ATTACKER_BONUS);
        DoEffectCalcAmount += AuraEffectCalcAmountFn(era_hun_expose_weakness_tbc::CalcAmount,
            EFFECT_1, SPELL_AURA_MELEE_ATTACK_POWER_ATTACKER_BONUS);
    }
};

// era_hun_readiness_tbc — TBC Readiness (node 20663), bound to the clone 948351.
//
// WHY A MODULE SCRIPT AND NOT A REBIND OF THE CORE `spell_hun_readiness`. The core script
// (spell_hunter.cpp:668-712) is bound POSITIVELY to stock 23989 and walks the caster's cooldown map
// clearing every SPELLFAMILY_HUNTER spell EXCEPT a three-id exclusion list (23989 / 19574 / 59543).
// The first entry is what protects Readiness's own cooldown: `Spell::SendSpellCooldown()` runs at
// Spell.cpp:3933, BEFORE HandleLaunchPhase()/DoAllEffectOnTarget() where the SpellScript's
// OnEffectHitTarget handler runs, so the caster's own Readiness cooldown is ALREADY in the map when
// the walk happens. Rebinding the core script onto the clone would therefore clear the CLONE's own
// 5-minute cooldown and make it infinitely spammable (_ref core_hardcodes.readiness, spec A.6/A.12).
//
// The era clones are reset for free: they all carry `family: 9` by template inheritance, so the
// family walk finds them without any id list.
//
// EXCLUSION SET — the era Bestial Wrath clone 948290 is deliberately NOT excluded (spec Amendment
// A.17, 2026-09-05, correcting A.6): the cached TBC tooltip for 23989 reads "finishes the cooldown on
// your other Hunter abilities" with NO Bestial Wrath exception — that exception is WotLK's — so
// TBC-authentic Readiness RESETS Bestial Wrath (_ref core_hardcodes.readiness). Only the clone's own
// id plus the core's three stock exclusions stay; stock 19574 is kept in the list for symmetry with the
// core walk (a TBC hunter never holds it).
//
// The core script additionally force-removes SPELL_HUNTER_CHIMERA_SHOT_SCORPID (the WotLK Chimera
// Shot disarm cooldown). That is deliberately not replicated: Chimera Shot is a WotLK spell a TBC-era
// hunter never holds.
//
// Bound to a band clone id, so it is inherently TBC-only and needs NO era check — and it must not
// gate on "is a bot" either. Bails immediately when the module is disabled (hook-guard discipline).
class era_hun_readiness_tbc : public SpellScript
{
    PrepareSpellScript(era_hun_readiness_tbc);

    enum : uint32
    {
        ERA_READINESS_TBC         = 948351,   // this clone — see the SendSpellCooldown note above
        STOCK_READINESS           = 23989,
        STOCK_BESTIAL_WRATH       = 19574,
        DRAENEI_GIFT_OF_THE_NAARU = 59543,
    };

    bool Load() override
    {
        return GetCaster() && GetCaster()->IsPlayer();
    }

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        if (!sEraTalentsConfig->Enabled())
            return;

        Player* caster = GetCaster()->ToPlayer();
        if (!caster)
            return;

        SpellCooldowns& cooldowns = caster->GetSpellCooldownMap();

        std::set<std::pair<uint32, bool>> spellsToRemove;
        std::set<uint32> categoriesToRemove;

        for (auto const& itr : cooldowns)
        {
            SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(itr.first);
            if (!spellInfo || spellInfo->SpellFamilyName != SPELLFAMILY_HUNTER)
                continue;
            if (spellInfo->Id == ERA_READINESS_TBC
                || spellInfo->Id == STOCK_READINESS || spellInfo->Id == STOCK_BESTIAL_WRATH
                || spellInfo->Id == DRAENEI_GIFT_OF_THE_NAARU)
                continue;

            if (spellInfo->RecoveryTime > 0)
                spellsToRemove.insert(std::make_pair(spellInfo->Id, itr.second.needSendToClient));
            if (spellInfo->CategoryRecoveryTime > 0)
                categoriesToRemove.insert(spellInfo->GetCategory());
        }

        // We cannot remove cooldowns while iterating the map.
        for (auto const& pair : spellsToRemove)
            caster->RemoveSpellCooldown(pair.first, pair.second);
        for (uint32 category : categoriesToRemove)
            caster->RemoveCategoryCooldown(category);

        if (sEraTalentsConfig->Debug())
            LOG_INFO("module", "[era_hun_readiness_tbc] {} reset {} hunter cooldown(s), {} categor(ies)",
                     caster->GetName(), uint32(spellsToRemove.size()), uint32(categoriesToRemove.size()));
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(era_hun_readiness_tbc::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// era_hun_beast_within_tbc — TBC The Beast Within (node 20620), riding the TBC Bestial Wrath clone
// 948290 (node 20617). The core delivers the owner buff BY ID: SpellAuras.cpp:1841-1857, inside the
// SPELLFAMILY_HUNTER arm of Aura::HandleAuraSpecificMods,
//
//     case 19574:                                   // Bestial Wrath
//         if (Unit* owner = target->GetOwner())
//             if (owner->HasAura(34692))            // The Beast Within
//                 apply ? owner->CastSpell(owner, 34471, true, 0, GetEffect(0))
//                       : owner->RemoveAurasDueToSpell(34471);
//
// — and all three ids are STOCK. A TBC-era hunter holds NEITHER of the two the arm tests: node 20617
// grants the clone 948290 and node 20620 the clone 948291, with stock 19574/34692 stripped by
// kEraHunTbcStockSwaps. (34471 is not swapped — it is simply never reached.) So the core arm can
// never fire for a TBC-era hunter, in either direction. This AuraScript is
// bound to the BW clone (era-data/tbc/hunter.yaml scriptBindings, alongside the core's own
// spell_hun_bestial_wrath CheckCast) and replicates BOTH halves — the apply-side cast AND the
// remove-side strip, which the core does too and without which the owner would keep the buff for
// its full 18 s after an early Bestial Wrath removal (dispel, pet death, pet dismiss).
//
// The aura lives on the PET (the BW clone's effects all target 5 = TARGET_UNIT_PET), so GetTarget()
// is the pet and GetTarget()->GetOwner() is the hunter. EFFECT_0 of 948290 is aura 61
// SPELL_AURA_MOD_SCALE (inherited from the 19574 template — dumped from the base Spell.dbc,
// 2026-09-04), which is what the two handlers hook.
//
// Gated on the module enable flag + null checks ONLY — never on "is a bot": an era-managed bot
// carries the same band spells and must get the same behaviour (CLAUDE.md, the bot Judgement no-op).
class era_hun_beast_within_tbc : public AuraScript
{
    PrepareAuraScript(era_hun_beast_within_tbc);

    enum : uint32
    {
        TBW_PASSIVE    = 948291,   // TBC The Beast Within passive clone (node 20620 grants it)
        TBW_OWNER_BUFF = 948292,   // TBC 34471 clone: -20% mana cost, +10% damage, stun immunity, 18 s
    };

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ TBW_PASSIVE, TBW_OWNER_BUFF });
    }

    void HandleApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (!sEraTalentsConfig->Enabled())
            return;
        Unit* pet = GetTarget();
        Unit* owner = pet ? pet->GetOwner() : nullptr;
        if (!owner || !owner->HasSpell(TBW_PASSIVE))
            return;
        owner->CastSpell(owner, TBW_OWNER_BUFF, true);
    }

    void HandleRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (!sEraTalentsConfig->Enabled())
            return;
        Unit* pet = GetTarget();
        Unit* owner = pet ? pet->GetOwner() : nullptr;
        if (!owner)
            return;
        // Unconditional strip (the core's own `else` branch is inside the HasAura(34692) test, but a
        // hunter can only ever hold this buff by way of the passive, and stripping a buff nobody has
        // is free) — so a respec mid-Bestial-Wrath cannot strand the owner buff either.
        owner->RemoveAurasDueToSpell(TBW_OWNER_BUFF);
    }

    void Register() override
    {
        AfterEffectApply  += AuraEffectApplyFn(era_hun_beast_within_tbc::HandleApply,   EFFECT_0, SPELL_AURA_MOD_SCALE, AURA_EFFECT_HANDLE_REAL);
        AfterEffectRemove += AuraEffectRemoveFn(era_hun_beast_within_tbc::HandleRemove, EFFECT_0, SPELL_AURA_MOD_SCALE, AURA_EFFECT_HANDLE_REAL);
    }
};

// era_rog_preparation — Vanilla Preparation (talent 18447). Bound via scriptBindings to the era
// clone 932983 (a template clone of stock 14185 carrying its own client row + the DUMMY effect).
// Replicates core spell_rog_preparation (spell_rogue.cpp:455) — walk the caster's rogue spellbook and
// clear the cooldowns of the family-flag-matched abilities — PLUS Blind (2094, family 8, flags[0] bit
// 0x01000000), the one ability WotLK dropped from the stock reset set. The stock script keys off id
// 14185, so a band clone would SILENTLY LOSE it (inert DUMMY shell); this module script restores the
// logic on the clone AND adds Blind. Reset mask mirrors the stock script exactly:
//   flags[1] & 0x00000240  (Cold Blood, Shadowstep)   — SPELLFAMILYFLAG1_ROGUE_COLDB_SHADOWSTEP
//   flags[0] & 0x00000860  (Vanish, Evasion, Sprint)  — SPELLFAMILYFLAG_ROGUE_VAN_EVAS_SPRINT
//   flags[0] & 0x01000000  (Blind)                    — the Vanilla addition
// (Shadowstep's bit is harmless — no Vanilla rogue has it. The stock script's Glyph-of-Preparation
// extras — Kick/Dismantle/Blade Flurry — are intentionally omitted: no era rogue has that glyph.)
// Bound to the era band clone id, so it is inherently Vanilla-only — no era check needed. Bails
// immediately when the module is disabled (hook-guard discipline).
class era_rog_preparation : public SpellScript
{
    PrepareSpellScript(era_rog_preparation);

    enum : uint32
    {
        ROGUE_COLDB_SHADOWSTEP = 0x00000240,   // flags[1]: Cold Blood, Shadowstep
        ROGUE_VAN_EVAS_SPRINT  = 0x00000860,   // flags[0]: Vanish, Evasion, Sprint
        ROGUE_BLIND            = 0x01000000,    // flags[0]: Blind (2094) — the Vanilla addition
    };

    bool Load() override
    {
        return GetCaster() && GetCaster()->IsPlayer();
    }

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        if (!sEraTalentsConfig->Enabled())
            return;

        Player* caster = GetCaster()->ToPlayer();
        if (!caster)
            return;

        uint32 cleared = 0;
        PlayerSpellMap const& spellMap = caster->GetSpellMap();
        for (PlayerSpellMap::const_iterator itr = spellMap.begin(); itr != spellMap.end(); ++itr)
        {
            SpellInfo const* spellInfo = sSpellMgr->AssertSpellInfo(itr->first);
            if (spellInfo->SpellFamilyName != SPELLFAMILY_ROGUE)
                continue;

            if ((spellInfo->SpellFamilyFlags[1] & ROGUE_COLDB_SHADOWSTEP) ||
                (spellInfo->SpellFamilyFlags[0] & (ROGUE_VAN_EVAS_SPRINT | ROGUE_BLIND)))
            {
                SpellCooldowns::iterator citr = caster->GetSpellCooldownMap().find(spellInfo->Id);
                if (citr != caster->GetSpellCooldownMap().end() && citr->second.needSendToClient)
                    caster->RemoveSpellCooldown(spellInfo->Id, true);
                else
                    caster->RemoveSpellCooldown(spellInfo->Id, false);
                ++cleared;
            }
        }

        if (sEraTalentsConfig->Debug())
            LOG_INFO("module", "[era_rog_preparation] {} reset {} rogue cooldown(s) (incl. Blind bit)",
                     caster->GetName(), cleared);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(era_rog_preparation::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// era_rog_preparation_tbc — TBC Preparation (node 20457). Bound via scriptBindings to the TBC era
// clone of stock 14185 (a fresh 947xxx helper at TBC's RecoveryTime 600000 = 10 min); the clone id
// is allocated by the TBC rogue authoring task, so this script binds to nothing until then and is
// inert. Governing decision: spec Amendment A.7 + era-data/_ref/tbc/rogue-spells.yaml
// preparation_resolution.
//
// WHY A SECOND SCRIPT RATHER THAN WIDENING era_rog_preparation'S MASK. The three versions of this
// ability have three different reset sets, and the mask is the whole behaviour:
//   TBC     : Evasion, Sprint, Vanish, Cold Blood, Shadowstep, PREMEDITATION   (10-min cd)
//   live    : Evasion, Sprint, Vanish, Cold Blood, Shadowstep                  (8-min cd)
//   Vanilla : Evasion, Sprint, Vanish, Cold Blood, Shadowstep, BLIND           (10-min cd)
// TBC and Vanilla are the exact INVERSE of each other on the last member, so widening the Vanilla
// script's mask would silently hand a Vanilla rogue a Premeditation reset it must never have
// (Vanilla behavior is frozen). Rebinding the Vanilla clone 932983 is likewise out — its client
// Description is Vanilla-authored (it names neither Blind nor Premeditation) and a shipped 932xxx
// row is immutable. Hence: FRESH clone, FRESH script, Vanilla untouched.
//
// Mechanism mirrors core spell_rog_preparation (spell_rogue.cpp:455) exactly — walk the caster's
// own spellbook and clear the cooldown of every SPELLFAMILY_ROGUE spell whose family flags match.
// It is a FAMILY-FLAG walk, never an id list, which is what makes it era-agnostic about WHICH
// version of each member the rogue holds: a template clone inherits SpellFamilyFlags, so a granted
// stock Cold Blood (14177, granted stock in TBC — Amendment A.7) and an era clone of Premeditation
// both still carry their identity bit and are both reset.
//   flags[1] & 0x00000260  = Cold Blood 0x40 | Shadowstep 0x200 | PREMEDITATION 0x20
//   flags[0] & 0x00000860  = Evasion 0x20 | Sprint 0x40 | Vanish 0x800
// (0x240/0x860 is the stock script's pair; the TBC addition is the single 0x20 bit. The stock
// script's Glyph-of-Preparation extras — Kick/Dismantle/Blade Flurry — are omitted for the same
// reason as in the Vanilla script: glyphs are WotLK-era-only for everyone, EraGlyphGate.)
// Bound to a band clone id, so it is inherently TBC-only and needs NO era check — and it must not
// gate on "is a bot" either (an era-managed bot holds this clone and needs the reset to work).
// Bails immediately when the module is disabled (hook-guard discipline).
class era_rog_preparation_tbc : public SpellScript
{
    PrepareSpellScript(era_rog_preparation_tbc);

    enum : uint32
    {
        // flags[1]: Cold Blood (0x40), Shadowstep (0x200) — the stock pair — plus Premeditation (0x20)
        ROGUE_COLDB_SHADOWSTEP_PREMED = 0x00000260,
        ROGUE_VAN_EVAS_SPRINT         = 0x00000860,   // flags[0]: Vanish, Evasion, Sprint
    };

    bool Load() override
    {
        return GetCaster() && GetCaster()->IsPlayer();
    }

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        if (!sEraTalentsConfig->Enabled())
            return;

        Player* caster = GetCaster()->ToPlayer();
        if (!caster)
            return;

        uint32 cleared = 0;
        PlayerSpellMap const& spellMap = caster->GetSpellMap();
        for (PlayerSpellMap::const_iterator itr = spellMap.begin(); itr != spellMap.end(); ++itr)
        {
            SpellInfo const* spellInfo = sSpellMgr->AssertSpellInfo(itr->first);
            if (spellInfo->SpellFamilyName != SPELLFAMILY_ROGUE)
                continue;

            if ((spellInfo->SpellFamilyFlags[1] & ROGUE_COLDB_SHADOWSTEP_PREMED) ||
                (spellInfo->SpellFamilyFlags[0] & ROGUE_VAN_EVAS_SPRINT))
            {
                SpellCooldowns::iterator citr = caster->GetSpellCooldownMap().find(spellInfo->Id);
                if (citr != caster->GetSpellCooldownMap().end() && citr->second.needSendToClient)
                    caster->RemoveSpellCooldown(spellInfo->Id, true);
                else
                    caster->RemoveSpellCooldown(spellInfo->Id, false);
                ++cleared;
            }
        }

        if (sEraTalentsConfig->Debug())
            LOG_INFO("module", "[era_rog_preparation_tbc] {} reset {} rogue cooldown(s) (incl. Premeditation bit)",
                     caster->GetName(), cleared);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(era_rog_preparation_tbc::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// era_rog_sap_stealth — Vanilla Sap breaks stealth + Improved Sap (node 18444). On this 3.3.5a core
// Sap carries ATTR1_ALLOW_WHILE_STEALTHED and never breaks the rogue's stealth; in Vanilla, casting
// Sap DID break stealth and the Improved Sap talent gave a 30/60/90% chance to REMAIN stealthed.
// Bound via scriptBindings to every stock Sap rank (2070/6770/11297/51724, family 8 bit 128 — _ref).
// DESIGN A (self-contained): this script owns the whole mechanic, so there is no proc-vs-break
// ordering hazard. On a Vanilla-era rogue's Sap it rolls the Improved Sap chance (misc-14 band DUMMY
// marker amount = 30/60/90, minted by node 18444's `scripted` passive; absent = 0) — on success it
// keeps stealth (the core already keeps it), on failure/no-talent it removes the rogue's stealth aura.
// Node 18444 no longer casts its own proc->Stealth (the re-stealth chance is folded in here), so the
// two can't double-apply. Gated on EraFromIP == ERA_VANILLA so TBC/WotLK rogues are UNCHANGED (Sap
// keeps stealth as the core does — per-character era-safe), and bails when the module is disabled.
class era_rog_sap_stealth : public SpellScript
{
    PrepareSpellScript(era_rog_sap_stealth);

    enum : uint32
    {
        ERA_BAND_LOW      = EraTalents::ERA_CUSTOM_BAND_LOW,
        ERA_BAND_HIGH     = EraTalents::ERA_CUSTOM_BAND_HIGH,
        IMPROVED_SAP_MISC = 14,        // DUMMY-marker misc for Improved Sap (band-gated; registry: 14)
    };

    // Improved Sap "stay stealthed" chance from the caster's band-gated misc-14 DUMMY marker (0 if none).
    int32 StayStealthChance(Unit* caster) const
    {
        for (AuraEffect const* eff : caster->GetAuraEffectsByType(SPELL_AURA_DUMMY))
        {
            if (eff->GetMiscValue() != int32(IMPROVED_SAP_MISC))
                continue;
            uint32 id = eff->GetSpellInfo()->Id;
            if (id >= ERA_BAND_LOW && id < ERA_BAND_HIGH)
                return eff->GetAmount();
        }
        return 0;
    }

    void HandleAfterCast()
    {
        if (!sEraTalentsConfig->Enabled())
            return;

        Player* rogue = GetCaster() ? GetCaster()->ToPlayer() : nullptr;
        if (!rogue)
            return;

        // Era gate: ONLY Vanilla-era rogues break stealth on Sap. TBC/WotLK unchanged (per-character
        // safe). EraFor: bots derive era from LEVEL BAND (EraFromIP reads every bot as Vanilla — IP
        // quest progression is 0 for bots even at 80), so a WotLK-band rogue bot keeps stock Sap rules.
        //
        // TBC (Phase 5, 2026-09-02) NEEDS NO ARM HERE and this gate is what guarantees it: verified
        // against era-data/_ref/tbc/rogue-spells.yaml script_census.era_rog_sap_stealth. By 2.x Sap
        // no longer broke stealth, and the 3.3.5a behaviour this early return falls through to IS
        // the TBC behaviour — stock Sap carries ATTR1_ALLOW_WHILE_STEALTHED and keeps the rogue
        // stealthed. (Sap 2070/6770/11297 do diverge, but only on AuraInterruptFlags and
        // MaxTargets — neither touches the stealth question.) Belt-and-braces: the TBC tree has NO
        // analog of Vanilla's Improved Sap node at all, so the misc-14 DUMMY marker this script
        // reads is never minted for a TBC rogue even if the gate above were ever relaxed.
        if (EraTalentBots::EraFor(rogue) != ERA_VANILLA)
            return;

        int32 chance = StayStealthChance(rogue);

        if (sEraTalentsConfig->Debug())
            LOG_INFO("module", "[era_rog_sap_stealth] Vanilla rogue {} cast Sap; Improved Sap stay-stealth chance={}%",
                     rogue->GetName(), chance);

        if (chance > 0 && roll_chance_i(chance))
        {
            if (sEraTalentsConfig->Debug())
                LOG_INFO("module", "[era_rog_sap_stealth] Improved Sap kept {} stealthed", rogue->GetName());
            return;   // Improved Sap: remained in stealth
        }

        rogue->RemoveAurasByType(SPELL_AURA_MOD_STEALTH);   // Vanilla: Sap breaks stealth

        if (sEraTalentsConfig->Debug())
            LOG_INFO("module", "[era_rog_sap_stealth] broke stealth for {}", rogue->GetName());
    }

    void Register() override
    {
        // AfterCast (not AfterHit): authentic Vanilla — casting Sap breaks stealth even on a resist;
        // the Improved Sap chance is the only thing that keeps it. CheckCast failures (target in
        // combat / LoS / not stealthed) never reach AfterCast, so those paths stay safe.
        AfterCast += SpellCastFn(era_rog_sap_stealth::HandleAfterCast);
    }
};

// =================================================================================================
// TBC Mutilate (node 20420) — spec Amendment A.14, _ref accepted_gaps id 8.
//
// TBC Mutilate is "+50% damage against Poisoned targets" and "Must be behind the target". NEITHER
// property is spell data, which is why the phase's fourteen-family DBC sweep could not see them and
// why a clone alone cannot restore them (same finding CLASS as the TBC druid Heart of the Wild, core
// patch 0023 — "the magnitude lives in CORE CODE, not spell data"):
//   * the poisoned-target bonus is hardcoded in src/server/game/Spells/SpellEffects.cpp:3454-3475,
//     inside the SPELLFAMILY_ROGUE arm of Spell::EffectWeaponDmg, keyed on
//     `m_spellInfo->SpellFamilyFlags[1] & 0x6` (the two hidden hand-halves' identity bits) and
//     applying `AddPct(totalDamagePercentMod, 20.0f)` — WotLK's 3.0.2 retune of TBC's +50%. A
//     faithful clone MUST keep those bits (Lethality, Puncturing Wounds, Find Weakness and
//     `spell_rog_cold_blood`'s CheckProc all bind them), so a clone gets +20% too;
//   * the positional requirement is the CU attribute SPELL_ATTR0_CU_REQ_CASTER_BEHIND_TARGET, which
//     SpellMgr.cpp:3590 hands out from a hardcoded by-id switch that never lists Mutilate — so it is
//     absent from the core rather than diverged in the row, and an id-keyed core fix never reaches a
//     clone anyway.
// This phase takes ZERO core patches, so the two scripts below restore the behaviour module-side.
// Both are per-character safe: era-band OWNERSHIP is the discriminator (never "is a bot", never a
// bare EraFromIP), so Vanilla and WotLK rogues are byte-identical to stock.
// =================================================================================================

// era_rog_mutilate_poison — the missing +25% on top of the core's +20% (1.20 x 1.25 = TBC's 1.50).
// Bound (scriptBindings, era-data/tbc/rogue.yaml) to the EIGHT **STOCK** hidden hand-halves
// 5374 / 27576 (rank 1) and 34414-34419 (ranks 2-4) — the spells that carry the actual weapon damage
// (Effect_1 = 121 SPELL_EFFECT_NORMALIZED_WEAPON_DMG) and the ones the core's +20% branch keys on.
// The halves are deliberately NOT cloned: they are invisible to players, era-identical apart from an
// attribute re-encoding of "cannot be avoided", and cloning them would move the very hook this script
// has to attach to. Binding a module script to a STOCK rogue id under a per-character gate is the
// `era_rog_sap_stealth` idiom (bound to stock Sap 2070/6770/11297/51724); the generator emits an
// (id, ScriptName)-qualified DELETE for a non-band id, so the core's own bindings survive.
//
// GATE: the caster must OWN the TBC era clone chain's rank 1, 947773. That is the cleanest possible
// TBC discriminator — a Vanilla rogue has no Mutilate at all (the talent does not exist in that
// tree), a WotLK rogue holds stock 1329, and the band teardown strips 947773 on any era transition.
// It also means a BOT gets the TBC numbers exactly like a player, which is required: an era-managed
// TBC-band rogue bot is granted 947773 by the same node grant (the "era scripts must NOT bail on
// bots" rule — a GET_PLAYERBOT_AI bail here would silently ship bots the WotLK multiplier).
//
// HOOK: OnHit, i.e. Spell::CallScriptOnHitHandlers at Spell.cpp:2754 — after every effect has run
// (weapon damage is computed at LAUNCH inside EffectWeaponDmg and carried into the hit phase via
// TargetInfo::damage) and BEFORE the damage is dealt or crit/armour are applied by
// CalculateSpellDamageTaken. GetHitDamage/SetHitDamage are legal in SPELL_SCRIPT_HOOK_HIT
// (SpellScript::IsInTargetHook), and there is no earlier module-reachable hook: an
// OnEffectHitTarget/OnEffectLaunchTarget handler runs BEFORE the default effect handler, so anything
// it wrote would simply be overwritten by EffectWeaponDmg's `m_damage += eff_damage`.
// ACCEPTED IMPRECISION (documented, not a defect): the core applies its +20% as
// `ApplyPct(weaponDamage, totalDamagePercentMod)` BEFORE MeleeDamageBonusDone/Taken, whereas this
// x1.25 lands after them — so any FLAT melee addend (a SPELL_AURA_MOD_DAMAGE_DONE flat, or a
// MOD_DAMAGE_TAKEN debuff such as Hemorrhage's +38 physical) is scaled by 1.25 as well. On a
// several-hundred-damage hit that is single digits, and no module-side hook exists between the two
// points; reproducing the split exactly would mean reimplementing EffectWeaponDmg.
class era_rog_mutilate_poison : public SpellScript
{
    PrepareSpellScript(era_rog_mutilate_poison);

    enum : uint32
    {
        ERA_TBC_MUTILATE_R1 = 947773,   // era-data/tbc/rogue.yaml helper; node 20420 grants it
        TBC_EXTRA_PCT       = 25,       // 1.20 (core) x 1.25 = 1.50 (TBC)
    };

    // The core's own poison test, transcribed verbatim from SpellEffects.cpp:3454-3475: a fast
    // aura-state probe for the caster's own Deadly Poison, then a full scan of the target's applied
    // auras for ANY spell whose Dispel type is DISPEL_POISON (which is what makes Instant/Wound/
    // Mind-numbing/Crippling/Anesthetic Poison and every non-rogue poison count). Kept identical so
    // the +25% arms on exactly the hits the core's +20% armed on — never a superset.
    static bool TargetIsPoisoned(Unit* target, SpellInfo const* spellInfo, Unit* caster)
    {
        if (target->HasAuraState(AURA_STATE_DEADLY_POISON, spellInfo, caster))
            return true;

        Unit::AuraApplicationMap const& auras = target->GetAppliedAuras();
        for (Unit::AuraApplicationMap::const_iterator itr = auras.begin(); itr != auras.end(); ++itr)
            if (itr->second->GetBase()->GetSpellInfo()->Dispel == DISPEL_POISON)
                return true;

        return false;
    }

    void HandleOnHit()
    {
        // Guard and bail BEFORE touching any hook argument (module hooks fire regardless of the
        // runtime enable flag).
        if (!sEraTalentsConfig->Enabled())
            return;

        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();
        if (!caster || !target)
            return;

        // Era gate = OWNERSHIP of the TBC clone chain's rank 1. Only a player can hold a spell.
        Player* rogue = caster->ToPlayer();
        if (!rogue || !rogue->HasSpell(uint32(ERA_TBC_MUTILATE_R1)))
            return;

        int32 dmg = GetHitDamage();
        if (dmg <= 0)                                   // immune / absorbed-to-zero / miss
            return;

        if (!TargetIsPoisoned(target, GetSpellInfo(), caster))
            return;

        int32 topUp = CalculatePct(dmg, int32(TBC_EXTRA_PCT));
        SetHitDamage(dmg + topUp);

        if (sEraTalentsConfig->Debug())
            LOG_INFO("module", "[era_rog_mutilate_poison] {} Mutilate half {} vs poisoned {}: {} -> {} (+{}%)",
                     rogue->GetName(), GetSpellInfo()->Id, target->GetName(), dmg, dmg + topUp,
                     uint32(TBC_EXTRA_PCT));
    }

    void Register() override
    {
        OnHit += SpellHitFn(era_rog_mutilate_poison::HandleOnHit);
    }
};

// era_rog_mutilate_behind — TBC's "Must be behind the target" requirement, which WotLK dropped.
// Bound (scriptBindings) to the four era CLONE visible casts 947773-947776, so it is inherently
// TBC-only: no Vanilla or WotLK rogue can ever hold one of those ids, and a native rogue's stock
// 1329 chain keeps casting from any angle. The arc test is the core's own, copied from the
// SPELL_ATTR0_CU_REQ_CASTER_BEHIND_TARGET branch at Spell.cpp:5894 — `target->HasInArc(M_PI, caster)`
// is true when the caster stands in the target's FRONT 180 degrees, which is the failure case.
// (Adding the CU attribute to stock 1329 instead is not an option: SpellMgr's switch is by-id and
// global, so it would gate Mutilate for WotLK rogues too — the framework's stock-id non-negotiable.)
class era_rog_mutilate_behind : public SpellScript
{
    PrepareSpellScript(era_rog_mutilate_behind);

    SpellCastResult CheckCast()
    {
        if (!sEraTalentsConfig->Enabled())
            return SPELL_CAST_OK;

        Unit* caster = GetCaster();
        Unit* target = GetExplTargetUnit();
        if (!caster || !target || target == caster)
            return SPELL_CAST_OK;

        if (target->HasInArc(static_cast<float>(M_PI), caster))
            return SPELL_FAILED_NOT_BEHIND;

        return SPELL_CAST_OK;
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(era_rog_mutilate_behind::CheckCast);
    }
};

// era_deep_wounds — Warrior Deep Wounds, BOTH managed eras (Vanilla node 18509 / TBC node 20308,
// both Arms). proc-scripted: the generator mints a per-rank hidden DUMMY passive (amount = the
// era's "% of your average melee weapon damage" — 20/40/60 in both eras) + a spell_proc row
// (crit-only, HitMask 2, Chance 100) + a spell_script_names row bound to this AuraScript. On a
// CRIT the script reads the rank pct from its own DUMMY amount, computes that pct of the warrior's
// average main-hand weapon damage, and applies it as a physical bleed over 12 sec via an authored
// band helper. This is why Deep Wounds is a custom scripted bleed and NOT a grant of the stock
// 12721 chain: the WotLK stock Deep Wounds was retuned to a different formula/duration (6 s), and
// its % is of the TRIGGERING spell's damage, not "% of average weapon damage over 12 s".
//
// THE CARRIER IS PER-ERA — this script does NOT cast one fixed bleed id (2026-09-02, TBC Phase 4
// Arms). The two eras' carriers differ in ways that are not interchangeable:
//   * Vanilla 932800 — a clone of stock 12721 keeping its 1000 ms tick period, re-stretched to
//     12 s (12 ticks), and deliberately FAMILY-LESS so no warrior spellmod touches it.
//   * TBC 947551 — authored from scratch at TBC 12721's own record: 3000 ms period over 12 s
//     (4 ticks), and family 4 + SpellClassMask_2 bit 4 ON PURPOSE, because TBC Blood Frenzy
//     (node 20318) binds an aura-109 mask of (Rend 0x20, Deep Wounds 0x10) and
//     SpellInfo::IsAffected returns false outright for a family-0 spell. It also must NOT inherit
//     live 12721's AttributesEx4 IGNORE_DAMAGE_TAKEN_MODIFIERS / AttributesEx6
//     IGNORE_CASTER_DAMAGE_MODIFIERS, which would make the bleed ignore Blood Frenzy itself.
// Selection is by OWNERSHIP OF THE PROCCING AURA — never an "is a bot" test and never a live era
// lookup: the generator mints a node's auto-passives from the node id, so the TBC node's three
// passives are custom_passive_id(20308, 1..3) = 938464/938465/938466 and nothing else can carry
// them. Same idiom as the Sanctified Judgement block in era_pal_judgement below. An unknown id
// falls back to the Vanilla carrier, which is what every already-shipped binding is.
//
// Modeled on era_blessed_recovery/era_ignite: PreventDefaultAction + read aurEff->GetAmount() +
// CastCustomSpell of the periodic with the per-tick base point. Per-tick = total / GetMaxTicks() —
// the same robust idiom the sibling scripts use (never a hardcoded tick count, which would silently
// desync from the helper's DBC period/duration), and it is what makes ONE script serve both eras'
// cadences. Deep Wounds reads its OWN amount, so no cross-spell DUMMY-marker registry value is
// needed (the registry "next free" stays 19).
class era_deep_wounds : public AuraScript
{
    PrepareAuraScript(era_deep_wounds);

    enum : uint32
    {
        SPELL_ERA_DEEP_WOUNDS_BLEED     = 932800,   // Vanilla carrier: 1000 ms x 12 s = 12 ticks
        SPELL_ERA_DEEP_WOUNDS_BLEED_TBC = 947551,   // TBC carrier: 3000 ms x 12 s = 4 ticks
    };

    // custom_passive_id(20308, 1..3) — the TBC Deep Wounds node's own auto-passives.
    static bool IsTbcPassive(uint32 auraSpellId)
    {
        static constexpr uint32 kTbcDeepWounds[3] = { 938464, 938465, 938466 };
        return std::find(std::begin(kTbcDeepWounds), std::end(kTbcDeepWounds), auraSpellId)
               != std::end(kTbcDeepWounds);
    }

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_ERA_DEEP_WOUNDS_BLEED, SPELL_ERA_DEEP_WOUNDS_BLEED_TBC });
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& eventInfo)
    {
        PreventDefaultAction();

        Unit* caster = eventInfo.GetActor();          // the warrior who landed the crit (aura owner)
        Unit* target = eventInfo.GetProcTarget();     // the victim
        if (!caster || !target || !target->IsAlive())
            return;

        uint32 const bleedId = IsTbcPassive(GetId()) ? SPELL_ERA_DEEP_WOUNDS_BLEED_TBC
                                                     : SPELL_ERA_DEEP_WOUNDS_BLEED;

        float minD = caster->GetWeaponDamageRange(BASE_ATTACK, MINDAMAGE);
        float maxD = caster->GetWeaponDamageRange(BASE_ATTACK, MAXDAMAGE);
        int32 total = int32(((minD + maxD) / 2.0f) * aurEff->GetAmount() / 100.0f);   // pct of avg weapon dmg
        if (total <= 0)
            return;

        SpellInfo const* bleed = sSpellMgr->AssertSpellInfo(bleedId);
        int32 perTick = total / int32(bleed->GetMaxTicks());   // Vanilla 12 ticks / TBC 4 ticks
        if (perTick <= 0)
            return;

        caster->CastCustomSpell(target, bleedId, &perTick, nullptr, nullptr,
                                true, nullptr, aurEff);

        if (sEraTalentsConfig->Debug())
            LOG_INFO("module", "[era_deep_wounds] {} crit (passive {}) -> bleed {} on {} ({}% of avg wpn {:.0f}-{:.0f} = {} total / {} ticks = {}/tick)",
                     caster->GetName(), GetId(), bleedId, target->GetName(),
                     aurEff->GetAmount(), minD, maxD, total, bleed->GetMaxTicks(), perTick);
    }

    void Register() override
    {
        OnEffectProc += AuraEffectProcFn(era_deep_wounds::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// era_improved_berserker_rage — Warrior Improved Berserker Rage in BOTH managed eras (Vanilla node
// 18533 / TBC node 20337, Fury; the TBC node re-uses this very script through the same misc-15 marker,
// so TBC needed no new C++ at all). `mechanic:
// scripted`: the generator mints a per-rank hidden band-gated DUMMY marker passive (misc 15, amount =
// 5/10 — the Vanilla "Berserker Rage generates 5/10 rage when used"; 2 ranks, single effect, verified
// 2026-08-18 warcraft.wiki.gg / Wowhead Classic) that self-applies on learn. This SpellScript is bound
// via the top-level scriptBindings to STOCK Berserker Rage (18499). Data can't express it: on 3.3.5a
// Berserker Rage's three effects are all aura 77 MECHANIC_IMMUNITY with NO energize for a SPELLMOD to
// scale, and "grant rage when THIS spell is cast" has no clean proc-flag — hence a cast-time SpellScript
// (AfterCast, like era_rog_sap_stealth). On cast, if the warrior carries the misc-15 marker it energizes
// `amount` rage attributed to Berserker Rage 18499 (so the client shows the gain). Rage is stored ×10
// internally (Unit::RewardRage: ModifyPower(POWER_RAGE, addRage*10)) and EnergizeBySpell forwards raw
// internal units to ModifyPower, so we pass amount*10. Inert without the marker — a WotLK / untalented
// warrior's Berserker Rage is NEVER mutated (global binding, marker-gated, era-safe). Bails on
// !Enabled() only — era-managed BOTS (EraTalents.BotTalents) cast this too and must not be excluded
// (CLAUDE.md "Era proc/effect scripts must NOT bail on bots"), which is exactly what the code does.
// DUMMY-marker registry: 15 = Improved Berserker Rage.
class era_improved_berserker_rage : public SpellScript
{
    PrepareSpellScript(era_improved_berserker_rage);

    enum : uint32
    {
        ERA_BAND_LOW                 = EraTalents::ERA_CUSTOM_BAND_LOW,
        ERA_BAND_HIGH                = EraTalents::ERA_CUSTOM_BAND_HIGH,
        IMPROVED_BERSERKER_RAGE_MISC = 15,      // DUMMY-marker misc for Improved Berserker Rage (registry: 15)
        SPELL_BERSERKER_RAGE         = 18499,   // stock Berserker Rage — the bound cast + energize attribution
    };

    // Rage generated from the caster's band-gated misc-15 DUMMY marker (0 if absent). Same idiom as the
    // sibling band-DUMMY-marker readers (era_rog_sap_stealth::StayStealthChance / EraStoneMarkerPct).
    int32 MarkerRage(Unit* caster) const
    {
        for (AuraEffect const* eff : caster->GetAuraEffectsByType(SPELL_AURA_DUMMY))
        {
            if (eff->GetMiscValue() != int32(IMPROVED_BERSERKER_RAGE_MISC))
                continue;
            uint32 id = eff->GetSpellInfo()->Id;
            if (id >= ERA_BAND_LOW && id < ERA_BAND_HIGH)
                return eff->GetAmount();
        }
        return 0;
    }

    void HandleAfterCast()
    {
        if (!sEraTalentsConfig->Enabled())
            return;

        Player* warrior = GetCaster() ? GetCaster()->ToPlayer() : nullptr;
        if (!warrior)   // bots included (EraTalents.BotTalents): the misc-15 marker itself is the gate
            return;

        int32 rage = MarkerRage(warrior);   // 5 / 10 by learned rank; 0 = no talent (the era gate)
        if (rage <= 0)
            return;

        // Rage is stored ×10 internally; EnergizeBySpell forwards raw units to ModifyPower -> pass rage*10.
        warrior->EnergizeBySpell(warrior, SPELL_BERSERKER_RAGE, uint32(rage * 10), POWER_RAGE);

        if (sEraTalentsConfig->Debug())
            LOG_INFO("module", "[era_improved_berserker_rage] {} cast Berserker Rage -> +{} rage (marker 15)",
                     warrior->GetName(), rage);
    }

    void Register() override
    {
        // AfterCast (not AfterHit): Berserker Rage is a self-buff, so the rage should land whenever the
        // cast completes. CheckCast failures (e.g. GCD/silence) never reach AfterCast, so those stay safe.
        AfterCast += SpellCastFn(era_improved_berserker_rage::HandleAfterCast);
    }
};

// era_bloodthirst_vanilla — era Warrior Bloodthirst. **The name is historical: this script is
// ID-AGNOSTIC and now serves BOTH managed eras.** Vanilla node 18535 grants clone 932828 (r1;
// 932829/830/831 trainer-taught) and TBC node 20340 grants clone 947512 (r1; 947513-947517
// trainer-taught at 48/54/60/66/70) — ten ids across two eras, all bound via their own dataset's
// top-level scriptBindings. Neither era can grant stock 23881 (WotLK: 50% AP, 20 rage, a 4 s
// CATEGORY cooldown and a percent-of-max-health heal).
// AP scaling is not a DBC value (stock Bloodthirst uses spell_warr_bloodthirst::HandleDamage's ApplyPct),
// so this SpellScript mirrors that — and it reads the COEFFICIENT OFF THE CASTING CLONE'S OWN
// Effect_0 base value rather than hardcoding one, which is exactly why the TBC chain needed no C++
// change at all (both eras happen to author 45). It applies that percentage to the caster's melee
// attack power and then runs the same SpellDamageBonusDone/Taken pipeline as stock. The
// heal-on-swings half is pure DBC — the clone's Effect_1 TRIGGER_SPELL applies the per-rank
// 5-charge heal buff (Vanilla 932832-835 / TBC 947518-947523), which procs the flat heal (Vanilla
// 932836-839 / TBC 947524-947529) on the next 5 melee attacks — so this script owns only the
// damage. Bound only to the custom band ids, so stock 23881 (WotLK warriors) is never touched.
// Bails on !Enabled() and on a non-Player caster ONLY: era-managed BOTS cast this too and must not
// be excluded (the bot-Judgement no-op regression, CLAUDE.md "Era proc/effect scripts must NOT bail
// on bots").
class era_bloodthirst_vanilla : public SpellScript
{
    PrepareSpellScript(era_bloodthirst_vanilla);

    bool Load() override
    {
        return GetCaster() && GetCaster()->IsPlayer();
    }

    void HandleDamage(SpellEffIndex effIndex)
    {
        if (!sEraTalentsConfig->Enabled())
            return;

        Player* warrior = GetCaster() ? GetCaster()->ToPlayer() : nullptr;
        if (!warrior)   // bots included (EraTalents.BotTalents): only era-managed characters know this spell
            return;

        int32 damage = GetEffectValue();   // 45 — the Vanilla AP percentage, authored in the clone's Effect_0
        ApplyPct(damage, warrior->GetTotalAttackPowerValue(BASE_ATTACK));

        if (Unit* target = GetHitUnit())
        {
            damage = warrior->SpellDamageBonusDone(target, GetSpellInfo(), uint32(damage), SPELL_DIRECT_DAMAGE, effIndex);
            damage = target->SpellDamageBonusTaken(warrior, GetSpellInfo(), uint32(damage), SPELL_DIRECT_DAMAGE);
        }
        SetHitDamage(damage);

        if (sEraTalentsConfig->Debug())
            LOG_INFO("module", "[era_bloodthirst_vanilla] {} Bloodthirst (spell {}) -> {} dmg (45% of AP {:.0f})",
                     warrior->GetName(), GetSpellInfo()->Id, damage,
                     warrior->GetTotalAttackPowerValue(BASE_ATTACK));
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(era_bloodthirst_vanilla::HandleDamage, EFFECT_0, SPELL_EFFECT_SCHOOL_DAMAGE);
    }
};

// era_last_stand — Warrior Last Stand in BOTH managed eras (Vanilla node 18541 -> clone 932840 at a
// 10-min cooldown; TBC node 20349 -> clone 947547 at TBC's 8-min cooldown). The +30%-max-HP-for-20s
// MECHANIC is identical Vanilla->WotLK, but it is SCRIPT-BOUND to stock id 12975 (core
// spell_warr_last_stand): 12975's Effect_0 DUMMY (base 29 -> GetEffectValue() 30) casts the triggered
// buff 12976 (aura 34 SPELL_AURA_MOD_INCREASE_HEALTH — FLAT, basePoints 0) with the 30%-of-max-HP
// computed BY THE SCRIPT and passed as custom basePoints. So the +30% HP lives in the SCRIPT, not the
// DBC: a naive clone that merely triggers 12976 would add 0 HP (regression), and a self-contained
// SPELL_AURA_MOD_INCREASE_HEALTH_PERCENT clone would keep the same health% (the core handler heals to
// preserve the pre-buff %, so no emergency current-HP boost — not Last Stand). Only the cooldown
// diverges: Vanilla 10 min vs WotLK's stock 3-min CATEGORY cooldown (Category 1251 /
// CategoryRecoveryTime 180000, RecoveryTime 0), which can't be changed on the global stock spell
// (era-safety forbids it). So node 18541 grants the era clone 932840 (a template of 12975: inherits the
// DUMMY effect + instant cast + free cost; category/categoryCooldown neutralized to 0 and RecoveryTime
// set to 600000 — the Aimed Shot 932952-957 category-neutralize precedent) and THIS script — bound via
// scriptBindings to the era clones ONLY (932840 from the Vanilla dataset, 947547 from the TBC one —
// each dataset binds its own id) — replicates spell_warr_last_stand::HandleDummy verbatim so the clone
// delivers the identical +30% HP via the stock 12976 buff. Stock 12975 (WotLK warriors) is untouched.
// Bails on !Enabled() only — era-managed bots (EraTalents.BotTalents) cast this too.
class era_last_stand : public SpellScript
{
    PrepareSpellScript(era_last_stand);

    enum : uint32 { SPELL_ERA_LAST_STAND_TRIGGERED = 12976 };

    bool Load() override
    {
        return GetCaster() && GetCaster()->IsPlayer();
    }

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        if (!sEraTalentsConfig->Enabled())
            return;

        Player* warrior = GetCaster() ? GetCaster()->ToPlayer() : nullptr;
        if (!warrior)   // bots included (EraTalents.BotTalents): only era-managed characters know this spell
            return;

        int32 healthModSpellBasePoints0 = int32(warrior->CountPctFromMaxHealth(GetEffectValue()));
        warrior->CastCustomSpell(warrior, SPELL_ERA_LAST_STAND_TRIGGERED, &healthModSpellBasePoints0,
                                 nullptr, nullptr, true, nullptr);

        if (sEraTalentsConfig->Debug())
            LOG_INFO("module", "[era_last_stand] {} Last Stand (spell {}) -> +{} max HP for 20s ({}% of {})",
                     warrior->GetName(), GetSpellInfo()->Id, healthModSpellBasePoints0,
                     GetEffectValue(), warrior->GetMaxHealth());
    }

    void Register() override
    {
        OnEffectHit += SpellEffectFn(era_last_stand::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// era_pal_holy_shock — Vanilla Paladin Holy Shock (node 18614). Stock 20473 on this 3.3.5a server is
// the WotLK form: its Effect_0 DUMMY drives core spell_pal_holy_shock to cast 25912 (dmg) / 25914
// (heal), whose WotLK base points + EffectBonusMultiplier make rank 1 ~314-340 (vs Vanilla 204-220).
// Era safety forbids editing global 20473, so node 18614 grants a Vanilla CLONE dummy (932646-648,
// bound HERE via scriptBindings) that branches to the rank's Vanilla-valued damage (932649-651, on an
// enemy) or heal (932652-654, on an ally) — the same friendly/hostile branch as the core script, but
// casting our own clones so the magnitudes and (coefficient-only) spellpower scaling are authentic.
// TBC Paladin (node 20016) reuses this same script via a FRESH single-rank clone 946087 -> dmg 946088
// (277-299) / heal 946089 (351-379); see Resolve.
// Bails on !Enabled() only — era-managed bots (EraTalents.BotTalents) cast this too, like era_bloodthirst_vanilla.
class era_pal_holy_shock : public SpellScript
{
    PrepareSpellScript(era_pal_holy_shock);

    // dummy clone id -> (damage clone, heal clone), one row per Vanilla Holy Shock rank.
    static bool Resolve(uint32 dummy, uint32& dmg, uint32& heal)
    {
        switch (dummy)
        {
            case 932646: dmg = 932649; heal = 932652; return true;   // R1 (204-220)
            case 932647: dmg = 932650; heal = 932653; return true;   // R2 (279-301)
            case 932648: dmg = 932651; heal = 932654; return true;   // R3 (365-395)
            case 946087: dmg = 946088; heal = 946089; return true;   // TBC R1 (277-299 / 351-379)
            case 946116: dmg = 946117; heal = 946118; return true;   // TBC R2 (379-409 / 480-518)
            case 946119: dmg = 946120; heal = 946121; return true;   // TBC R3 (496-536 / 628-680)
            case 946122: dmg = 946123; heal = 946124; return true;   // TBC R4 (614-664 / 777-841)
            case 946125: dmg = 946126; heal = 946127; return true;   // TBC R5 (721-779 / 913-987)
            default: return false;
        }
    }

    bool Load() override
    {
        return GetCaster() && GetCaster()->IsPlayer();
    }

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        if (!sEraTalentsConfig->Enabled())
            return;

        Player* pal = GetCaster() ? GetCaster()->ToPlayer() : nullptr;
        if (!pal)   // bots included (EraTalents.BotTalents): only era-managed characters know this spell
            return;

        Unit* target = GetHitUnit();
        if (!target)
            return;

        uint32 dmg = 0, heal = 0;
        if (!Resolve(GetSpellInfo()->Id, dmg, heal))
            return;

        uint32 cast = pal->IsFriendlyTo(target) ? heal : dmg;
        pal->CastSpell(target, cast, true);

        if (sEraTalentsConfig->Debug())
            LOG_INFO("module", "[era_pal_holy_shock] {} Holy Shock (spell {}) -> {} on {}",
                     pal->GetName(), GetSpellInfo()->Id, cast,
                     pal->IsFriendlyTo(target) ? "ally (heal)" : "enemy (damage)");
    }

    SpellCastResult CheckCast()
    {
        Unit* caster = GetCaster();
        Unit* target = GetExplTargetUnit();
        if (!caster || !target)
            return SPELL_FAILED_BAD_TARGETS;
        if (!caster->IsFriendlyTo(target))
        {
            if (!caster->IsValidAttackTarget(target))
                return SPELL_FAILED_BAD_TARGETS;
            if (!caster->isInFront(target))
                return SPELL_FAILED_UNIT_NOT_INFRONT;
        }
        return SPELL_CAST_OK;
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(era_pal_holy_shock::CheckCast);
        OnEffectHitTarget += SpellEffectFn(era_pal_holy_shock::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// era_pal_seal_of_righteousness — Vanilla Seal of Righteousness on-hit: per-rank base Holy damage
// scaled by mainhand weapon speed (WotLK stock scales by AP/SP instead). Bound (scriptBindings) to the
// SoR seal clones 932700-932707. The seal's Effect_0 is aura 42 PROC_TRIGGER_SPELL -> the rank's on-hit
// spell (932708-932715); we read that on-hit spell's Effect_0 base value (so a future Improved Seal of
// Righteousness SPELLMOD on it still applies via CalcValue), multiply by mainhand speed in seconds, and
// cast it. Bails on !Enabled() only — era-managed bots proc this too, like the other paladin scripts here.
class era_pal_seal_of_righteousness : public AuraScript
{
    PrepareAuraScript(era_pal_seal_of_righteousness);

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& eventInfo)
    {
        PreventDefaultAction();

        if (!sEraTalentsConfig->Enabled())
            return;

        Player* pal = GetCaster() ? GetCaster()->ToPlayer() : nullptr;
        if (!pal)   // bots included (EraTalents.BotTalents): only era-managed characters know this spell
            return;

        Unit* target = eventInfo.GetActionTarget();
        if (!target)
            return;

        uint32 onHitId = aurEff->GetSpellInfo()->Effects[EFFECT_0].TriggerSpell; // 932708..932715
        SpellInfo const* onHit = sSpellMgr->GetSpellInfo(onHitId);
        if (!onHit)
            return;

        // Read the RAW per-rank base (nullptr caster = NO spellmods). The Improved Seal of
        // Righteousness SPELLMOD (op allEffects, +3/6/9/12/15%) then applies EXACTLY ONCE, at cast
        // time below, when the core recomputes the effect value for onHitId (which carries the
        // Improved-SoR word-C bit10 mask). Reading CalcValue(pal) here applied the SPELLMOD a first
        // time (ApplyEffectModifiers) and the cast applied it a second time -> ~+30% at 5/5 instead
        // of +15% (live-confirmed 188 -> 245). One source of truth for the bonus: the cast.
        int32 base = onHit->Effects[EFFECT_0].CalcValue(nullptr);
        float speed = pal->GetAttackTime(BASE_ATTACK) / 1000.0f; // seconds
        int32 bp = std::max<int32>(1, int32(base * speed));
        pal->CastCustomSpell(target, onHitId, &bp, nullptr, nullptr, true, nullptr, aurEff);

        if (sEraTalentsConfig->Debug())
            LOG_INFO("module", "[era_pal_seal_of_righteousness] {} on-hit (spell {}) base {} x speed {} -> {} dmg on {}",
                     pal->GetName(), onHitId, base, speed, bp, target->GetName());
    }

    void Register() override
    {
        OnEffectProc += AuraEffectProcFn(era_pal_seal_of_righteousness::HandleProc, EFFECT_0, SPELL_AURA_PROC_TRIGGER_SPELL);
    }
};

// era_pal_judgement_of_light — Vanilla Judgement of Light: an attacker who lands ANY qualifying hit
// (melee/ranged auto-attack or melee/ranged spell damage — the TAKEN mask on the debuff's own
// spell_proc row, so this proc fires on the debuff HOLDER's own aura with the ATTACKER as
// eventInfo.GetActor()) on the debuffed target heals a FLAT amount, the debuff's EFFECT_0 DUMMY
// value (24/33/48/60). Mirrors core spell_pal_judgement_of_light_heal
// (azerothcore-wotlk/src/server/scripts/Spells/spell_paladin.cpp:1329) line for line EXCEPT the
// amount: WotLK heals CountPctFromMaxHealth(GetAmount()) (a % of the ATTACKER's max HP); Vanilla
// heals a flat number regardless of the attacker's health. Bound (scriptBindings) to the JoL payload
// clones 932733-932736 (era-data/vanilla/paladin.yaml); delivers the heal via the shared carrier
// 932747. No config/bot bail on the ATTACKER — anyone who lands a qualifying hit on a judged target
// benefits, bot or human, paladin or not, exactly like the stock script; era-safety is already
// enforced upstream by the debuff's own existence (932733-936 are only ever applied by a Vanilla-era
// paladin's custom Judgement, so a WotLK/TBC paladin can never be the CASTER of one, though anyone
// — bot or human — may freely be the healed ATTACKER; era-managed BOTS cast it too since
// EraTalents.BotTalents).
class era_pal_judgement_of_light : public AuraScript
{
    PrepareAuraScript(era_pal_judgement_of_light);

    enum : uint32 { SPELL_ERA_JOL_HEAL_CARRIER = 932747 };

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_ERA_JOL_HEAL_CARRIER });
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& eventInfo)
    {
        PreventDefaultAction();

        if (!sEraTalentsConfig->Enabled())
            return;

        Unit* attacker = eventInfo.GetActor();
        if (!attacker)
            return;

        int32 bp = aurEff->GetAmount();   // flat heal/hit (24/33/48/60), carried in the DUMMY amount
        attacker->CastCustomSpell(attacker, SPELL_ERA_JOL_HEAL_CARRIER, &bp, nullptr, nullptr, true,
                                  nullptr, aurEff, GetCasterGUID());

        if (sEraTalentsConfig->Debug())
            LOG_INFO("module", "[era_pal_judgement_of_light] {} judged-target hit -> {} heals self for {}",
                     GetTarget() ? GetTarget()->GetName() : "?", attacker->GetName(), bp);
    }

    void Register() override
    {
        OnEffectProc += AuraEffectProcFn(era_pal_judgement_of_light::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// era_pal_judgement_of_wisdom — Vanilla Judgement of Wisdom: an attacker who lands ANY qualifying hit
// (melee/ranged auto-attack or melee/ranged spell damage — the same TAKEN mask on the debuff's own
// spell_proc row as Judgement of Light, so this proc fires on the debuff HOLDER's own aura with the
// ATTACKER as eventInfo.GetActor()) on the debuffed target restores a FLAT amount of mana, the
// debuff's EFFECT_0 DUMMY value (32/45/58), but ONLY when that attacker is a mana user. Mirrors core
// spell_pal_judgement_of_wisdom_mana
// (azerothcore-wotlk/src/server/scripts/Spells/spell_paladin.cpp:1356) line for line EXCEPT the
// amount: WotLK restores CalculatePct(attacker->GetCreateMana(), GetAmount()) (a % of the ATTACKER's
// base mana); Vanilla restores a flat number regardless of the attacker's mana pool. The
// getPowerType() == POWER_MANA CheckProc gate is carried over VERBATIM from the stock script — a
// rogue/warrior/etc. attacker landing a hit on the judged target does not restore mana, exactly like
// WotLK. Bound (scriptBindings) to the JoW payload clones 932743-932745 (era-data/vanilla/paladin.yaml);
// delivers the mana via the shared carrier 932748. No config/bot bail on the ATTACKER — anyone who
// lands a qualifying hit on a judged target and uses mana benefits, bot or human, paladin or not,
// exactly like the stock script; era-safety is already enforced upstream by the debuff's own
// existence (932743-745 are only ever applied by a Vanilla-era paladin's custom Judgement, so a
// WotLK/TBC paladin or a bot — which never learns era talents — can never be the CASTER of one,
// though either may freely be the mana-restored ATTACKER).
class era_pal_judgement_of_wisdom : public AuraScript
{
    PrepareAuraScript(era_pal_judgement_of_wisdom);

    enum : uint32 { SPELL_ERA_JOW_MANA_CARRIER = 932748 };

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_ERA_JOW_MANA_CARRIER });
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        return eventInfo.GetActor() && eventInfo.GetActor()->getPowerType() == POWER_MANA;
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& eventInfo)
    {
        PreventDefaultAction();

        if (!sEraTalentsConfig->Enabled())
            return;

        Unit* attacker = eventInfo.GetActor();
        if (!attacker)
            return;

        int32 bp = aurEff->GetAmount();   // flat mana/hit (32/45/58), carried in the DUMMY amount
        attacker->CastCustomSpell(attacker, SPELL_ERA_JOW_MANA_CARRIER, &bp, nullptr, nullptr, true,
                                  nullptr, aurEff, GetCasterGUID());

        if (sEraTalentsConfig->Debug())
            LOG_INFO("module", "[era_pal_judgement_of_wisdom] {} judged-target hit -> {} restores {} mana",
                     GetTarget() ? GetTarget()->GetName() : "?", attacker->GetName(), bp);
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(era_pal_judgement_of_wisdom::CheckProc);
        OnEffectProc += AuraEffectProcFn(era_pal_judgement_of_wisdom::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// era_pal_judgement — Vanilla single Judgement (932746, Task 7, the keystone): unleashes whatever
// seal is active and CONSUMES it. Replicates core spell_pal_judgement's own EFFECT_2-DUMMY seal-scan
// (azerothcore-wotlk/src/server/scripts/Spells/spell_paladin.cpp:962-975 — GetAuraEffectsByType
// (SPELL_AURA_DUMMY), filtered to SPELL_SPECIFIC_SEAL + EFFECT_2, first hit wins) VERBATIM, then ADDS
// RemoveAura(seal) — Vanilla's consume-on-judge, which WotLK's own Judgement does NOT do — and OMITS
// every WotLK extra the stock script also does: no SPELL_PALADIN_JUDGEMENT_DAMAGE base hit, no Tier-5
// Improved Judgement mana energize, no Judgements of the Just extra-attack/Seal of Command cleave —
// none of those talents/sets exist for a Vanilla paladin. The scan is fully data-driven off any
// SPELL_SPECIFIC_SEAL aura carrying a DUMMY on EFFECT_2, so this ONE button unleashes ALL of our
// custom seals (SoR/SoJ/SoL/SoW) AND the already-shipped Seal of Command (932606) and Seal of the
// Crusader (932634-639) with zero extra wiring. Bound (scriptBindings) to 932746 only; era-scoped
// because only a Vanilla-era character (human via trainer, bot via the EraTalents.BotTalents
// reconcile) is ever granted 932746, so the Enabled() bail is belt-and-suspenders.
class era_pal_judgement : public SpellScript
{
    PrepareSpellScript(era_pal_judgement);

    bool Load() override
    {
        return GetCaster() && GetCaster()->IsPlayer();
    }

    void HandleScriptEffect(SpellEffIndex /*effIndex*/)
    {
        if (!sEraTalentsConfig->Enabled())
            return;

        Player* pal = GetCaster() ? GetCaster()->ToPlayer() : nullptr;
        if (!pal)   // bots included (EraTalents.BotTalents): only era-managed characters know this spell
            return;

        Unit* target = GetHitUnit();
        if (!target)
            return;

        uint32 judgeId = 0;
        uint32 sealId = 0;

        // Same scan as core spell_pal_judgement (spell_paladin.cpp:962-975): the active seal carries
        // a SPELL_AURA_DUMMY on EFFECT_2 whose amount is the judgement spell id.
        Unit::AuraEffectList const& auras = pal->GetAuraEffectsByType(SPELL_AURA_DUMMY);
        for (Unit::AuraEffectList::const_iterator i = auras.begin(); i != auras.end(); ++i)
        {
            if ((*i)->GetSpellInfo()->GetSpellSpecific() == SPELL_SPECIFIC_SEAL && (*i)->GetEffIndex() == EFFECT_2)
            {
                if (sSpellMgr->GetSpellInfo((*i)->GetAmount()))
                {
                    judgeId = (*i)->GetAmount();
                    sealId = (*i)->GetSpellInfo()->Id;
                    break;
                }
            }
        }

        if (!judgeId)
            return;                              // Vanilla: Judgement requires an active seal

        pal->CastSpell(target, judgeId, true);
        pal->RemoveAura(sealId);                 // consume-on-judge (Vanilla; WotLK's Judgement does not)

        if (sEraTalentsConfig->Debug())
            LOG_INFO("module", "[era_pal_judgement] {} Judgement -> {} on {}, seal {} consumed",
                     pal->GetName(), judgeId, target->GetName(), sealId);

        // Sanctified Judgement (TBC Retribution node 20058, 3 ranks) — a chance to refund 80% of the
        // JUDGED SEAL's mana cost. The talent's auto-passive (DUMMY, EFFECT_0 amount = the 33/66/100%
        // chance) carries the rank; its deterministic ids are custom_passive_id(20058, 1..3) =
        // 936464/936465/936466 (920000 + (20058-18000)*8 + rank-1). Vanilla paladins never carry these
        // ids, so this block is inert for them (no era gate needed beyond the aura presence). The seal's
        // SpellInfo is static and still resolvable after RemoveAura(sealId) above. Reads the seal's mana
        // cost the same way as era_improved_drain_mana / JoW (flat ManaCost + %-of-createMana). This is
        // the ONE place a judge fires with the judged seal in hand — a proc AuraScript would see the seal
        // already consumed. Bots reach this path too (the era_pal_judgement caster gate already admits
        // era-managed bots), matching the "era proc/effect scripts must NOT bail on bots" rule.
        static constexpr uint32 kSanctifiedJudgement[3] = { 936464, 936465, 936466 };
        int32 sjChance = 0;
        for (uint32 sj : kSanctifiedJudgement)
            if (AuraEffect const* e = pal->GetAuraEffect(sj, EFFECT_0))
            {
                sjChance = e->GetAmount();
                break;
            }

        if (sjChance > 0 && roll_chance_i(sjChance))
            if (SpellInfo const* sealInfo = sSpellMgr->GetSpellInfo(sealId))
            {
                int32 sealCost = int32(sealInfo->ManaCost)
                               + int32(CalculatePct(pal->GetCreateMana(), sealInfo->ManaCostPercentage));
                int32 refund = CalculatePct(sealCost, 80);
                if (refund > 0)
                {
                    pal->ModifyPower(POWER_MANA, refund);
                    if (sEraTalentsConfig->Debug())
                        LOG_INFO("module", "[era_pal_judgement] Sanctified Judgement ({}%) -> {} refunds {} mana (seal {} cost {})",
                                 sjChance, pal->GetName(), refund, sealId, sealCost);
                }
            }
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(era_pal_judgement::HandleScriptEffect, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

// ==== TBC FACTION DPS SEALS (Task 5d) — Seal of Blood (Horde/BE) + Seal of Vengeance (Alliance) ====
// Both are FRESH 946xxx clones (era per-character model), NOT the stock/core 31892/31801 scripts. The
// seal aura is SPELL_SPECIFIC_SEAL (template 20375) with E2 DUMMY = the judgement id era_pal_judgement
// (932746) unleashes+consumes. Guard discipline per the framework §"Era proc/effect scripts must NOT
// bail on bots": bail on !Enabled() + null player-caster only — an era-managed BOT owns these band
// spells and must proc them too. Named constants for every 946xxx id + every TBC magnitude.
enum EraPalFactionSeals : uint32
{
    SPELL_ERA_SOB_SEAL              = 946080, // Seal of Blood aura (Horde/BE)
    SPELL_ERA_SOB_JUDGEMENT         = 946081, // Judgement of Blood (flat 294 Holy + 33% self)
    SPELL_ERA_SOB_ONHIT            = 946082, // SoB on-hit Holy carrier (SCHOOL_DAMAGE, base set dynamically)
    SPELL_ERA_SOB_SELF             = 946083, // SoB self-damage carrier (SCHOOL_DAMAGE Holy on self, base dynamic)
    SPELL_ERA_SOV_SEAL             = 946084, // Seal of Vengeance aura (Alliance)
    SPELL_ERA_SOV_HOLY_VENGEANCE   = 946085, // Holy Vengeance DoT (stacks to 5; core scales tick by stack)
    SPELL_ERA_SOV_JUDGEMENT        = 946086, // Judgement of Vengeance (base 120 Holy, +10%/stack)
};

// era_pal_seal_of_blood — on each qualifying melee hit: deal Holy damage = 35% of the swing's damage
// (cast the on-hit carrier 946082 with the computed base) AND take self-damage = 10% of THAT Holy
// damage (cast the self carrier 946083 on the caster). Mirrors the era_pal_seal_of_righteousness proc
// shape (E0 aura42 PROC_TRIGGER_SPELL, PreventDefaultAction + a script-computed CastCustomSpell). The
// self-damage is bounded (10% of 35% of one swing) so it can never one-shot the paladin.
class era_pal_seal_of_blood : public AuraScript
{
    PrepareAuraScript(era_pal_seal_of_blood);

    enum : uint32 { ONHIT_HOLY_PCT = 35, ONHIT_SELF_PCT = 10 };

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_ERA_SOB_ONHIT, SPELL_ERA_SOB_SELF });
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& eventInfo)
    {
        PreventDefaultAction();

        if (!sEraTalentsConfig->Enabled())
            return;

        Player* pal = GetCaster() ? GetCaster()->ToPlayer() : nullptr;
        if (!pal)   // bots included (EraTalents.BotTalents): only era-managed characters know this spell
            return;

        Unit* target = eventInfo.GetActionTarget();
        DamageInfo* damageInfo = eventInfo.GetDamageInfo();
        if (!target || !damageInfo || damageInfo->GetDamage() == 0)
            return;

        int32 holy = std::max<int32>(1, int32(CalculatePct(damageInfo->GetDamage(), ONHIT_HOLY_PCT)));
        pal->CastCustomSpell(target, SPELL_ERA_SOB_ONHIT, &holy, nullptr, nullptr, true, nullptr, aurEff);

        int32 self = int32(CalculatePct(holy, ONHIT_SELF_PCT));
        if (self > 0)
            pal->CastCustomSpell(pal, SPELL_ERA_SOB_SELF, &self, nullptr, nullptr, true, nullptr, aurEff);

        if (sEraTalentsConfig->Debug())
            LOG_INFO("module", "[era_pal_seal_of_blood] {} on-hit -> {} Holy {} on {}, self {}",
                     pal->GetName(), SPELL_ERA_SOB_ONHIT, holy, target->GetName(), self);
    }

    void Register() override
    {
        OnEffectProc += AuraEffectProcFn(era_pal_seal_of_blood::HandleProc, EFFECT_0, SPELL_AURA_PROC_TRIGGER_SPELL);
    }
};

// era_pal_judgement_of_blood — Judgement of Blood deals flat Holy damage (the spell's own EFFECT_0
// SCHOOL_DAMAGE, 294) and then costs the Paladin health equal to 33% of the damage dealt. era_pal_judgement
// (932746) casts this on the target + consumes the seal; this script only adds the self-damage AfterHit.
// Bounded (33% of ~294) — cannot one-shot the caster.
class era_pal_judgement_of_blood : public SpellScript
{
    PrepareSpellScript(era_pal_judgement_of_blood);

    enum : uint32 { JUDGE_SELF_PCT = 33 };

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_ERA_SOB_SELF });
    }

    void HandleAfterHit()
    {
        if (!sEraTalentsConfig->Enabled())
            return;

        Player* pal = GetCaster() ? GetCaster()->ToPlayer() : nullptr;
        if (!pal)   // bots included (EraTalents.BotTalents)
            return;

        int32 dealt = GetHitDamage();
        if (dealt <= 0)
            dealt = GetSpellInfo()->Effects[EFFECT_0].CalcValue(pal);   // fallback: fully absorbed/resisted hit

        int32 self = int32(CalculatePct(dealt, JUDGE_SELF_PCT));
        if (self <= 0)
            return;

        pal->CastCustomSpell(pal, SPELL_ERA_SOB_SELF, &self, nullptr, nullptr, true);

        if (sEraTalentsConfig->Debug())
            LOG_INFO("module", "[era_pal_judgement_of_blood] {} Judgement of Blood dealt {} -> self {}",
                     pal->GetName(), dealt, self);
    }

    void Register() override
    {
        AfterHit += SpellHitFn(era_pal_judgement_of_blood::HandleAfterHit);
    }
};

// era_pal_seal_of_vengeance — on each qualifying melee hit, apply/refresh Holy Vengeance (946085) on the
// struck target, a Holy DoT that stacks up to 5 (the core auto-scales the periodic tick by stack count).
// Deterministic explicit cast (PreventDefaultAction) so the DoT always lands on the correct target.
class era_pal_seal_of_vengeance : public AuraScript
{
    PrepareAuraScript(era_pal_seal_of_vengeance);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_ERA_SOV_HOLY_VENGEANCE });
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& eventInfo)
    {
        PreventDefaultAction();

        if (!sEraTalentsConfig->Enabled())
            return;

        Player* pal = GetCaster() ? GetCaster()->ToPlayer() : nullptr;
        if (!pal)   // bots included (EraTalents.BotTalents)
            return;

        Unit* target = eventInfo.GetActionTarget();
        if (!target)
            return;

        pal->CastSpell(target, SPELL_ERA_SOV_HOLY_VENGEANCE, true, nullptr, aurEff);

        if (sEraTalentsConfig->Debug())
            LOG_INFO("module", "[era_pal_seal_of_vengeance] {} applies Holy Vengeance ({}) to {}",
                     pal->GetName(), SPELL_ERA_SOV_HOLY_VENGEANCE, target->GetName());
    }

    void Register() override
    {
        OnEffectProc += AuraEffectProcFn(era_pal_seal_of_vengeance::HandleProc, EFFECT_0, SPELL_AURA_PROC_TRIGGER_SPELL);
    }
};

// era_pal_judgement_of_vengeance — Judgement of Vengeance's base Holy damage is increased by 10% for
// each stack of the Paladin's own Holy Vengeance (946085) on the target, capped at 5 stacks (+50%).
class era_pal_judgement_of_vengeance : public SpellScript
{
    PrepareSpellScript(era_pal_judgement_of_vengeance);

    enum : uint32 { PER_STACK_PCT = 10, MAX_STACKS = 5 };

    void HandleDamage(SpellEffIndex /*effIndex*/)
    {
        if (!sEraTalentsConfig->Enabled())
            return;

        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();
        if (!caster || !target)
            return;

        uint32 stacks = 0;
        if (Aura* hv = target->GetAura(SPELL_ERA_SOV_HOLY_VENGEANCE, caster->GetGUID()))
            stacks = hv->GetStackAmount();
        if (stacks > MAX_STACKS)
            stacks = MAX_STACKS;
        if (!stacks)
            return;

        int32 dmg = GetHitDamage();
        SetHitDamage(dmg + int32(CalculatePct(dmg, PER_STACK_PCT * stacks)));

        if (sEraTalentsConfig->Debug())
            LOG_INFO("module", "[era_pal_judgement_of_vengeance] {} JoV base {} x {} stacks -> {}",
                     caster->GetName(), dmg, stacks, GetHitDamage());
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(era_pal_judgement_of_vengeance::HandleDamage, EFFECT_0, SPELL_EFFECT_SCHOOL_DAMAGE);
    }
};

// era_pal_crusader_strike — TBC Crusader Strike clone 946114 (node 20063): "...and refreshes all
// Judgements on the target." (wago-2.5.4/TBC-Classic behavior; the 2.3.0 patch added the refresh, so
// the value basis of record HAS it — the original authoring comment claiming TBC CS did not refresh
// was wrong). Refreshes the lingering judgement DEBUFFS on the struck target from ANY paladin (TBC
// refreshed other paladins' judgements too): the era JoL/JoW/JotC debuff clones + the reused stock
// Judgement of Justice 20184 (SoJ's EFFECT_2 points at stock 20184). The instant judgements
// (JoR/JoC/JoB/JoV) leave no debuff, so there is nothing to refresh; the JoL/JoW heal/energize
// CARRIERS (946049/946062) are excluded — they never persist as target auras. Guard discipline per
// framework §"Era proc/effect scripts must NOT bail on bots": bail on !Enabled() + null target only.
class era_pal_crusader_strike : public SpellScript
{
    PrepareSpellScript(era_pal_crusader_strike);

    static bool IsJudgementDebuff(uint32 id)
    {
        switch (id)
        {
            case 946044: case 946045: case 946046: case 946047: case 946048:               // JoL r1-5
            case 946058: case 946059: case 946060: case 946061:                            // JoW r1-4
            case 946071: case 946072: case 946073: case 946074: case 946075: case 946076:
            case 946077:                                                                    // JotC r1-7
            case 20184:                                                                     // JoJ (stock, SoJ reuse)
                return true;
            default:
                return false;
        }
    }

    void HandleRefresh(SpellEffIndex /*effIndex*/)
    {
        if (!sEraTalentsConfig->Enabled())
            return;

        Unit* target = GetHitUnit();
        if (!target)
            return;

        uint32 refreshed = 0;
        for (auto const& [id, app] : target->GetAppliedAuras())
            if (IsJudgementDebuff(id))
            {
                app->GetBase()->RefreshDuration();
                ++refreshed;
            }

        if (refreshed && sEraTalentsConfig->Debug())
            LOG_INFO("module", "[era_pal_crusader_strike] {} refreshed {} judgement debuff(s) on {}",
                     GetCaster() ? GetCaster()->GetName() : "?", refreshed, target->GetName());
    }

    void Register() override
    {
        // EFFECT_1 = WEAPON_PERCENT_DAMAGE (fires only on a landed hit; EFFECT_0 is NORMALIZED_WEAPON_DMG).
        OnEffectHitTarget += SpellEffectFn(era_pal_crusader_strike::HandleRefresh, EFFECT_1, SPELL_EFFECT_WEAPON_PERCENT_DAMAGE);
    }
};

// era_pal_jotc_crit — TBC Improved Seal of the Crusader (node 20045), the crit half of its tooltip:
// "your Judgement of the Crusader spell will also increase the critical strike chance of all attacks
// made against that target by an additional 1/2/3%." (sanity-audit fix 2026-08-30 — the shipped pct
// spellmod covered only the "normal effect" magnitude half; no crit mechanic existed). The JotC
// clones 946071-077 carry EFFECT_1 = aura197 MOD_ATTACKER_SPELL_AND_WEAPON_CRIT_CHANCE base 0 (the
// WotLK Heart-of-the-Crusader mechanism); this AuraScript, bound to all 7 ranks via scriptBindings,
// sets that effect's amount from the casting paladin's Imp SotC auto-passive (deterministic ids
// custom_passive_id(20045, 1..3) = 936360/361/362, EFFECT_0 amount = 1/2/3) — same read-the-rank
// idiom as era_pal_judgement's kSanctifiedJudgement. Without the talent the amount stays 0 (inert).
// Guard discipline per framework §"Era proc/effect scripts must NOT bail on bots": no caster-type
// bail — only a null-caster fallback to 0.
class era_pal_jotc_crit : public AuraScript
{
    PrepareAuraScript(era_pal_jotc_crit);

    void CalcCrit(AuraEffect const* /*aurEff*/, int32& amount, bool& canBeRecalculated)
    {
        canBeRecalculated = false;
        amount = 0;
        Unit* caster = GetCaster();
        if (!caster)
            return;
        static constexpr uint32 kImpSotc[3] = { 936360, 936361, 936362 };
        for (uint32 id : kImpSotc)
            if (AuraEffect const* e = caster->GetAuraEffect(id, EFFECT_0))
            {
                amount = e->GetAmount();
                break;
            }
    }

    void Register() override
    {
        DoEffectCalcAmount += AuraEffectCalcAmountFn(era_pal_jotc_crit::CalcCrit, EFFECT_1, SPELL_AURA_MOD_ATTACKER_SPELL_AND_WEAPON_CRIT_CHANCE);
    }
};

// era_hunter_scorpid_sting — Vanilla Scorpid Sting (helpers 932300-303) reduces Strength + Agility;
// Improved Scorpid Sting (node 18328) makes it ALSO reduce Stamina by 10/20/30% of the Strength it
// removed. Each clone carries Effect_3 = MOD_STAT Stamina (misc 2, base 0); this AuraScript sets that
// effect's amount to -pct% of the Effect_1 Strength reduction, where pct is the casting hunter's
// Improved Scorpid Sting DUMMY marker (misc 16). Without the talent the marker is absent -> pct 0 ->
// amount 0 (base Scorpid Sting stays Str/Agi only). Bound to the 4 clones via scriptBindings.
class era_hunter_scorpid_sting : public AuraScript
{
    PrepareAuraScript(era_hunter_scorpid_sting);

    enum : uint32 { ERA_BAND_LOW = EraTalents::ERA_CUSTOM_BAND_LOW, ERA_BAND_HIGH = EraTalents::ERA_CUSTOM_BAND_HIGH, IMPROVED_MARKER = 16 };

    // Improved Scorpid Sting % from the hunter's band-gated DUMMY marker (misc 16); 0 without the talent.
    int32 MarkerAmount(Unit* caster) const
    {
        if (!caster)
            return 0;
        for (AuraEffect const* eff : caster->GetAuraEffectsByType(SPELL_AURA_DUMMY))
            if (eff->GetMiscValue() == int32(IMPROVED_MARKER))
            {
                uint32 id = eff->GetSpellInfo()->Id;
                if (id >= ERA_BAND_LOW && id < ERA_BAND_HIGH)
                    return eff->GetAmount();
            }
        return 0;
    }

    void CalcStamina(AuraEffect const* /*aurEff*/, int32& amount, bool& canBeRecalculated)
    {
        canBeRecalculated = false;
        amount = 0;
        int32 pct = MarkerAmount(GetCaster());
        if (pct <= 0)
            return;
        // Strength reduced = |Effect_1 value| (21/30/46/69 per rank).
        int32 strReduced = GetSpellInfo()->Effects[EFFECT_0].CalcValue(GetCaster());
        if (strReduced < 0)
            strReduced = -strReduced;
        amount = -(strReduced * pct / 100);
    }

    void Register() override
    {
        DoEffectCalcAmount += AuraEffectCalcAmountFn(era_hunter_scorpid_sting::CalcStamina, EFFECT_2, SPELL_AURA_MOD_STAT);
    }
};

// era_pal_ardent_defender — TBC Paladin Ardent Defender (node 20039). The TBC form is a pure damage
// FLOOR: while the paladin is below 35% health, all damage taken is reduced by X% (6/12/18/24/30 per
// rank). It has NONE of the WotLK cheat-death bundle (aura69 SCHOOL_ABSORB pool + aura4 DUMMY 66233
// death-save). Authored as fresh SCHOOL_ABSORB clones 946090-946094 (template 31850, the WotLK
// cheat-death Effect_2 DUMMY dropped); this AuraScript, bound via scriptBindings to all 5 ranks,
// supplies the <35%-HP reduction through OnEffectAbsorb — the SAME reduction branch as core
// spell_pal_ardent_defender (spell_paladin.cpp:373), MINUS the cheat-death branch. NO core patch: a
// SCHOOL_ABSORB aura's per-damage absorb hook is the standard reachable mechanism for a health-gated
// reduction (the era decision tree's proc/script tier, not the core-patch escape hatch). The per-rank
// reduction % is the clone's Effect_0 SCHOOL_ABSORB base value, read once in Load(); CalcAmount makes
// the absorb pool unlimited (-1), exactly the stock AD idiom. Guard discipline (framework §"Era
// proc/effect scripts must NOT bail on bots"): bail on !Enabled() only — an era-managed BOT owns this
// band spell and must get the reduction too; NEVER an "is a bot" bail.
class era_pal_ardent_defender : public AuraScript
{
    PrepareAuraScript(era_pal_ardent_defender);

    enum : uint32 { AD_HEALTH_THRESHOLD_PCT = 35 };

    int32 _absorbPct = 0;

    bool Load() override
    {
        _absorbPct = GetSpellInfo()->Effects[EFFECT_0].CalcValue();   // per-rank 6/12/18/24/30
        return true;   // players AND era-managed bots (EraTalents.BotTalents) — never bail on bots
    }

    void CalcAmount(AuraEffect const* /*aurEff*/, int32& amount, bool& /*canBeRecalculated*/)
    {
        amount = -1;   // unlimited absorb pool (the stock AD idiom — the script decides how much to absorb)
    }

    void Absorb(AuraEffect* /*aurEff*/, DamageInfo& dmgInfo, uint32& absorbAmount)
    {
        if (!sEraTalentsConfig->Enabled())
            return;

        Unit* victim = GetTarget();
        if (!victim || _absorbPct <= 0)
            return;

        int32 remainingHealth = int32(victim->GetHealth()) - int32(dmgInfo.GetDamage());
        uint32 allowedHealth = victim->CountPctFromMaxHealth(AD_HEALTH_THRESHOLD_PCT);

        // TBC: no cheat-death save. Only reduce damage that brings us under 35% (or all damage if we
        // are already under 35%) by the rank's pct — core spell_pal_ardent_defender's else-branch, verbatim.
        if (remainingHealth < int32(allowedHealth))
        {
            uint32 damageToReduce = (victim->GetHealth() < allowedHealth)
                                    ? dmgInfo.GetDamage()
                                    : allowedHealth - remainingHealth;
            absorbAmount = CalculatePct(damageToReduce, _absorbPct);

            if (sEraTalentsConfig->Debug())
                LOG_INFO("module", "[era_pal_ardent_defender] {} <35% HP: absorb {} of {} dmg ({}% reduction)",
                         victim->GetName(), absorbAmount, dmgInfo.GetDamage(), _absorbPct);
        }
    }

    void Register() override
    {
        DoEffectCalcAmount += AuraEffectCalcAmountFn(era_pal_ardent_defender::CalcAmount, EFFECT_0, SPELL_AURA_SCHOOL_ABSORB);
        OnEffectAbsorb += AuraEffectAbsorbFn(era_pal_ardent_defender::Absorb, EFFECT_0);
    }
};

// ===================================================================================================
// TBC DRUID — Tree of Life (node 20161), spec Amendment A.2 "Route B". TWO scripts, both era-band
// OWNERSHIP-gated. Read _ref.tree_of_life in era-data/tbc/druid.yaml for the full evidence trail.
//
// WHY THIS EXISTS: AuraEffect::HandleShapeshiftBoosts (SpellAuraEffects.cpp:1359) hardcasts WotLK's
// Tree of Life Aura 34123 on FORM_TREE *unconditionally* — the switch is on GetMiscValue(), i.e. the
// FORM, with no HasTalent gate (unlike Leader of the Pack at :1496), and line 1359 is the ONLY
// occurrence of 34123 anywhere in src/server. Live 34123 = APPLY_AREA_AURA_RAID / aura 118
// MOD_HEALING_PCT +6% / radiusIndex 12. TBC 34123 = APPLY_AREA_AURA_PARTY / aura 115 MOD_HEALING
// (a FLAT healing-taken bonus) / radiusIndex 11 (45 yd), with basePoints 0 because the amount was
// computed server-side from the druid's Spirit ("25% of your total Spirit", TBC tooltip).
// So the two auras would DOUBLE-DIP: the era druid's party would get TBC's flat bonus AND WotLK's
// +6%. The TBC aura must SUPPRESS the WotLK one, not coexist with it.
//
// ERA GATE: both scripts key off HasSpell(ERA_TOL_FORM_PASSIVE) — the hidden FORM_TREE form-passive
// 946265, which only an era-managed TBC druid who has spent node 20161 ever owns (it is handed out
// and stripped by EraTalents.cpp's CLASS_DRUID paired-grant arm). That is era-band OWNERSHIP, the
// gate CLAUDE.md's "Era proc/effect scripts must NOT bail on bots" paragraph prescribes: NEVER a
// bare EraFromIP (Vanilla for every bot) and NEVER an "is a bot" bail — an era-managed BOT that
// somehow enters Tree form is treated exactly like a player. A WotLK druid never owns 946265, so
// era_dru_tree_of_life_suppress is inert for it even though it is bound to a stock spell id.
enum : uint32
{
    ERA_TOL_FORM_PASSIVE = 946265,   // hidden PASSIVE|DO_NOT_DISPLAY, ShapeshiftMask 2 (FORM_TREE)
    ERA_TOL_PARTY_AURA   = 946266,   // APPLY_AREA_AURA_PARTY, aura 115 MOD_HEALING, 45 yd
    ERA_TOL_SPIRIT_PCT   = 25,       // TBC: healing received +25% of the druid's total Spirit
};

// era_dru_tree_of_life — bound to 946266. Sets the party aura's amount to 25% of the CASTER's total
// Spirit. GetStat(STAT_SPIRIT) is the fully-buffed value, so Living Spirit (node 20157, aura 137
// MOD_TOTAL_STAT_PERCENTAGE on STAT_SPIRIT) is already folded in — which is how TBC's talent read.
// The DBC basePoints is 0, so if this script ever failed to run the aura would simply do nothing
// (no wrong magnitude can be shipped by accident).
class era_dru_tree_of_life : public AuraScript
{
    PrepareAuraScript(era_dru_tree_of_life);

    void CalcAmount(AuraEffect const* /*aurEff*/, int32& amount, bool& canBeRecalculated)
    {
        if (!sEraTalentsConfig->Enabled())
            return;

        Unit* caster = GetCaster();
        if (!caster)
            return;

        amount = int32(caster->GetStat(STAT_SPIRIT) * ERA_TOL_SPIRIT_PCT / 100.0f);
        // The core never recalculates SPELL_AURA_MOD_HEALING on a stat change, so pin it: the amount
        // is fixed at form entry (documented limitation — re-enter the form after a Spirit swap).
        canBeRecalculated = false;

        if (sEraTalentsConfig->Debug())
            LOG_INFO("module", "[era_dru_tree_of_life] {} Spirit {} -> +{} healing received (party, 45 yd)",
                     caster->GetName(), uint32(caster->GetStat(STAT_SPIRIT)), amount);
    }

    void Register() override
    {
        DoEffectCalcAmount += AuraEffectCalcAmountFn(era_dru_tree_of_life::CalcAmount, EFFECT_0, SPELL_AURA_MOD_HEALING);
    }
};

// era_dru_tree_of_life_suppress — bound to STOCK 34123 (a script binding, NOT a spell_dbc override:
// no stock row and no SpellInfo field is mutated, so the era-safety rule is intact — the same idiom
// as the shipped stock-Sap 18499/5138 and stock-Berserker-Rage bindings).
// PRIMARY suppression is DoCheckAreaTarget returning false: Aura::CanBeAppliedOn -> CheckAreaTarget
// runs for EVERY candidate of an area aura, the caster included (SpellAuras.cpp:1888-1907), so
// rejecting all of them leaves the aura owned but applied to nobody — no +6%, no handler, and no
// duplicate "Tree of Life" buff icon next to 946266's on party frames.
// DoEffectCalcAmount -> 0 is the belt-and-braces fallback for any path that applies the aura without
// consulting CheckAreaTarget; on its own it would leave a visible +0% icon, which is why it is the
// fallback and not the mechanism.
class era_dru_tree_of_life_suppress : public AuraScript
{
    PrepareAuraScript(era_dru_tree_of_life_suppress);

    // True only for an era-managed TBC druid that owns the era Tree of Life form-passive.
    bool EraTreeDruid()
    {
        if (!sEraTalentsConfig->Enabled())
            return false;
        Player* druid = GetCaster() ? GetCaster()->ToPlayer() : nullptr;
        return druid && druid->HasSpell(ERA_TOL_FORM_PASSIVE);
    }

    bool CheckAreaTarget(Unit* /*target*/)
    {
        return !EraTreeDruid();     // WotLK druids: unchanged. Era druids: never applied.
    }

    void CalcAmount(AuraEffect const* /*aurEff*/, int32& amount, bool& /*canBeRecalculated*/)
    {
        if (EraTreeDruid())
            amount = 0;
    }

    void Register() override
    {
        DoCheckAreaTarget += AuraCheckAreaTargetFn(era_dru_tree_of_life_suppress::CheckAreaTarget);
        DoEffectCalcAmount += AuraEffectCalcAmountFn(era_dru_tree_of_life_suppress::CalcAmount, EFFECT_0, SPELL_AURA_MOD_HEALING_PCT);
    }
};

// era_sha_flametongue_totem_proc — the Flametongue Totem imbue's combat proc (both eras).
// Bound to the band proc DUMMYs 947135-947139 (r1-r5, wago 2.5.4 magnitudes 489/697/947/1217/1491
// EFFECTIVE). The chain: era effect clone (Effect 92) applies stock enchant 124/285/543/1683/2637
// -> the module's spellitemenchantment_dbc override re-points the enchant's combat spell from the
// ABSENT stock 8253-series onto these DUMMYs (chance 100, per swing) -> this script does what the
// 2.4.3 server did with the dummy amount: normalize by the striking weapon's speed (bp/100 * speed,
// clipped to [bp/77, bp/25] — the exact math core spell_sha_flametongue_weapon uses for the weapon
// imbue, which shares the mechanic) and deal it as fire damage via the band vehicle 947442
// ("Flametongue Attack", family-zeroed so Elemental Weapons' bit21 spellmod cannot leak onto totem
// procs). Effect 92 is ENCHANT_HELD_ITEM = main hand by construction, so BASE_ATTACK speed is
// always the right one. Closes shaman-totems-tbc accepted_gaps id 7 / open_questions id 7.
//
// Improved Weapon Totems' Flametongue half (TBC node 20231 r1/r2 = auto-passives 937848/937849,
// Vanilla node 18826 r1/r2 = 926608/926609; +6/12% both eras): the ATTACKER's own rank is honored
// here. The totem OWNER's rank is unreachable from this context — the enchant on a party member's
// weapon carries no owner link — so a party member's procs are boosted only by their OWN talent
// (i.e. in practice: the enhancement shaman's own swings, the case that matters). Recorded at the
// node comments; the old spellmod bind on 52109/bit25 stays inert and untouched.
class era_sha_flametongue_totem_proc : public SpellScript
{
    PrepareSpellScript(era_sha_flametongue_totem_proc);

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        if (!sEraTalentsConfig->Enabled())
            return;
        Player* player = GetCaster() ? GetCaster()->ToPlayer() : nullptr;
        Unit* victim = GetHitUnit();
        if (!player || !victim)
            return;

        float bp = float(GetEffectValue());
        float attackSpeed = player->GetAttackTime(BASE_ATTACK) / 1000.f;
        float fireDamage = bp / 100.0f * attackSpeed;
        RoundToInterval(fireDamage, bp / 77.0f, bp / 25.0f);

        // Improved Weapon Totems (attacker's own rank — see the header comment).
        if (player->HasAura(937849) || player->HasAura(926609))
            AddPct(fireDamage, 12);
        else if (player->HasAura(937848) || player->HasAura(926608))
            AddPct(fireDamage, 6);

        player->CastCustomSpell(947442, SPELLVALUE_BASE_POINT0, int32(fireDamage), victim,
                                true, GetCastItem());
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(era_sha_flametongue_totem_proc::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// era_sha_nature_guardian — the era replacement for core spell_sha_nature_guardian on the TBC
// clones 946532-946536 (shaman-spells accepted_gaps id 10, closed 2026-09-02). Identical proc
// logic to the core script — CheckProc reads the authored EFFECT_1 carrier (EFFECTIVE 30) as the
// health threshold, HandleProc heals 10% of max health (EFFECT_0's amount) and drops threat — but
// the heal is delivered through the band clone 947443, a real SPELL_EFFECT_HEAL, instead of stock
// 31616, whose WotLK shape is aura 34 MOD_INCREASE_HEALTH for 10 s (a max-health bump that is
// subtracted again on expiry — the exact tooltip lie this closes). The threat drop keeps stock
// 39301 (era-neutral).
class era_sha_nature_guardian : public AuraScript
{
    PrepareAuraScript(era_sha_nature_guardian);

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        if (!sEraTalentsConfig->Enabled())
            return false;
        DamageInfo* damageInfo = eventInfo.GetDamageInfo();
        if (!damageInfo || !damageInfo->GetDamage())
            return false;
        int32 healthpct = GetSpellInfo()->Effects[EFFECT_1].CalcValue();
        if (Unit* target = eventInfo.GetActionTarget())
            if (target->HealthBelowPctDamaged(healthpct, damageInfo->GetDamage()))
                return true;
        return false;
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& eventInfo)
    {
        PreventDefaultAction();
        Unit* target = eventInfo.GetActionTarget();
        if (!target)
            return;
        int32 bp = CalculatePct(int32(target->GetMaxHealth()), aurEff->GetAmount());
        target->CastCustomSpell(947443, SPELLVALUE_BASE_POINT0, bp, target, true, nullptr, aurEff);
        if (Unit* attacker = eventInfo.GetActor())
            target->CastSpell(attacker, 39301 /*SPELL_SHAMAN_NATURE_GUARDIAN_THREAT*/, true);
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(era_sha_nature_guardian::CheckProc);
        OnEffectProc += AuraEffectProcFn(era_sha_nature_guardian::HandleProc, EFFECT_0, SPELL_AURA_PROC_TRIGGER_SPELL);
    }
};

// ===================================================================================================
// TBC PRIEST — Reflective Shield (node 20519, era clones 948034-948038 = 10/20/30/40/50%).
// Spec 2026-09-03 section 4e; era-data/_ref/tbc/priest-spells.yaml core_hardcodes.reflective_shield.
//
// The core's spell_pri_power_word_shield_aura::ReflectDamage (spell_priest.cpp:690-704) finds the
// talent via GetAuraEffectOfRankedSpell(33201, EFFECT_0) — a spell_ranks walk from STOCK rank 1
// that a clone can never satisfy. MEASURED on this server: acore_world.spell_ranks has no row for
// 33201-33205, so Unit::GetAuraEffectOfRankedSpell (Unit.cpp:5796) inspects exactly the id 33201
// and nothing else. An era priest holds a 948034-948038 clone and never 33201, so the core reflect
// stays inert for them — which is why this second AuraScript can sit on the same stock PW:Shield
// ranks (bound via era-data/tbc/priest.yaml `scriptBindings:`).
// The two scripts key on different units: the core on the SHIELDED unit's stock 33201, this one on
// the CASTER's clone. An era priest never holds 33201 (kEraPriTbcStockSwaps), so a TBC priest
// shielding a TBC or Vanilla target reflects exactly once. ACCEPTED EDGE: a TBC priest shielding a
// WotLK-era priest who owns stock 33201 fires both (core 22/45% off the target's talent + this
// clone % off the caster) — mixed-era, PvE-only, not worth a guard that would silence the TBC
// talent on that target.
// ScriptMgr::CreateAuraScripts (ScriptDefines/SpellScriptLoader.cpp:43) instantiates EVERY
// spell_script_names row bound to a spell, so both scripts register their AfterEffectAbsorb hook
// on EFFECT_0 and both run.
// THE TALENT IS THE SHIELD'S CASTER'S, NOT THE SHIELDED UNIT'S: GetCaster() is the priest who cast
// Power Word: Shield, GetTarget() the unit wearing it. The core reads the talent off GetUnitOwner()
// because WotLK 3.1 made Reflective Shield SELF-only (live 33201's own text: "damage you absorb
// with Power Word: Shield"); TBC's talent belongs to the CASTER and applies to any target the
// priest shields, so this script reads GetCaster(). Both implementations are faithful to their own
// era.
// Inert for any priest without a clone (ClonePct returns 0), for a disabled module, and for a
// self-inflicted hit. Reuses the SAME triggered spell 33619 (LIVE_MATCH, era-neutral) and the same
// infinite-loop guard as the core.
class era_pri_reflective_shield_tbc : public AuraScript
{
    PrepareAuraScript(era_pri_reflective_shield_tbc);
    enum : uint32 { SPELL_REFLECT_TRIGGERED = 33619, CLONE_R1 = 948034, CLONE_R5 = 948038 };

    bool Validate(SpellInfo const* /*spellInfo*/) override { return ValidateSpellInfo({ SPELL_REFLECT_TRIGGERED }); }

    // Highest rank first: a priest can only ever own one of the five (the node grants exactly one
    // per rank and Reset / the band sweep (EraBandClassifier) removes the others), but walking down is cheap and
    // makes a half-migrated character behave as its best rank rather than its worst.
    static int32 ClonePct(Unit* owner)
    {
        for (uint32 id = CLONE_R5; id >= CLONE_R1; --id)
            if (AuraEffect const* eff = owner->GetAuraEffect(id, EFFECT_0))
                return eff->GetAmount();
        return 0;
    }

    void ReflectDamage(AuraEffect* aurEff, DamageInfo& dmgInfo, uint32& absorbAmount)
    {
        if (!sEraTalentsConfig->Enabled())
            return;
        Unit* target = GetTarget();
        if (!target || dmgInfo.GetAttacker() == target)
            return;
        Unit* caster = GetCaster();               // the talent is the CASTER's (TBC), not the shielded unit's
        if (!caster)
            return;
        int32 pct = ClonePct(caster);
        if (pct <= 0)
            return;
        if (dmgInfo.GetSpellInfo() && dmgInfo.GetSpellInfo()->Id == SPELL_REFLECT_TRIGGERED)
            return;                               // xinef's infinite-loop guard, verbatim
        int32 bp = CalculatePct(int32(absorbAmount), pct);
        if (bp > 0)
            target->CastCustomSpell(dmgInfo.GetAttacker(), SPELL_REFLECT_TRIGGERED, &bp, nullptr, nullptr, true, nullptr, aurEff);
    }

    void Register() override
    {
        AfterEffectAbsorb += AuraEffectAbsorbFn(era_pri_reflective_shield_tbc::ReflectDamage, EFFECT_0);
    }
};

// era_pri_vampiric_touch_tbc — TBC Vampiric Touch (node 20563, clones 948060-948062). TBC: 5% of the
// Shadow damage the CASTER deals to the VT TARGET restores mana to the caster's PARTY. WotLK replaced
// that with Replenishment (a Mind-Blast-keyed `-34914` proc row) and added a dispel backlash; neither
// is reproduced here — the absence of the backlash is era-correct, so the core's
// `spell_pri_vampiric_touch` is deliberately NOT rebound (era-data/_ref/tbc/priest-spells.yaml
// script_census, spec section 4c).
//
// THIS IS A TAKEN-SIDE PROC ON THE TARGET'S OWN AURA, which is unusual enough to spell out — it is
// what lets "any Shadow damage the caster deals TO THIS TARGET" be expressed without a second aura on
// the priest. The clones' generated `spell_proc` rows carry ProcFlags 655360 =
// PROC_FLAG_TAKEN_SPELL_MAGIC_DMG_CLASS_NEG (0x20000) | PROC_FLAG_TAKEN_PERIODIC (0x80000), so the
// engine visits this aura whenever its host takes priest Shadow damage. Delivery is code-verified end
// to end at the pinned core:
//   * `Unit::ProcSkillsAndAuras(actor, victim, procAttacker, procVictim, ...)` (Unit.cpp:6799) calls
//     `actor->TriggerAurasProcOnEvent(..., victim, procAttacker, procVictim, ...)`, whose TARGET-side
//     event is `ProcEventInfo(this, actionTarget, this, typeMaskActionTarget, ...)` (Unit.cpp:12946)
//     — so **GetActor() is the DAMAGE DEALER**, not the aura's host. That is why CheckProc compares
//     `eventInfo.GetActor()` against `GetCaster()`: it keeps one priest's VT from feeding off another
//     priest's (or anyone else's) Shadow damage to the same mob.
//   * The periodic path reaches it too: `AuraEffect::HandlePeriodicDamageAurasTick`
//     (SpellAuraEffects.cpp:6442) passes procVictim = PROC_FLAG_TAKEN_PERIODIC, which is how the era
//     Mind Flay / SW:Pain / Devouring Plague / VT's own ticks feed the return.
//   * `Aura::TriggerProcOnEvent` -> `AuraEffect::HandleProc` calls
//     `GetBase()->CallScriptEffectProcHandlers(...)` BEFORE its aura-type switch
//     (SpellAuraEffects.cpp:1287), so an `OnEffectProc` hook runs for ANY aura type — the hook does
//     not need the effect to be one the engine itself knows how to proc.
//
// HOOKED ON **EFFECT_0 / SPELL_AURA_DUMMY**, NOT ON THE PERIODIC-DAMAGE SLOT. The clones' DBC layout
// is `Effect_1` = aura 4 DUMMY (amount 5 = the mana percentage) and `Effect_2` = aura 3
// PERIODIC_DAMAGE, i.e. 0-based EFFECT_0 is the DUMMY. `AuraScript::EffectProcHandler` validates the
// (index, aura-type) pair against the DBC at script load and simply DOES NOT RUN a handler whose pair
// does not match (it logs one error per bound spell), so binding EFFECT_0 to PERIODIC_DAMAGE would
// have been a silent no-op. Hooking the DUMMY also means the percentage is read from the spell data
// (`aurEff->GetAmount()`) rather than hardcoded — the same discipline as era_vampiric_embrace.
// `PreventDefaultAction()` suppresses `HandleProcTriggerSpellAuraProc`, which would otherwise try to
// cast the DUMMY's `EffectTriggerSpell` (0) and log a debug line on every tick.
//
// PARTY, NOT RAID: `group->SameSubGroup(priest, member)` — a bare GetFirstMember() walk spans a
// 40-man raid (the framework's "Party vs raid scope" rule, a real bug caught in review on the VE
// heal). Solo (no group) energizes the priest alone, which is what TBC did.
// The delivery spell 948063 is a clone of stock 34919 narrowed to ImplicitTargetA 21
// (TARGET_UNIT_TARGET_ALLY); stock 34919's own target type is 20 CASTER_AREA_PARTY, so casting IT
// once per member would re-fan to the whole party on every cast.
// Inert for a disabled module, for a caster-less aura, and for damage dealt by anyone but the VT
// caster. Gated on `!Enabled()` + null-caster + caster identity — never on "is a bot" (the era
// proc-script rule; an era-managed BOT owns these clones too).
class era_pri_vampiric_touch_tbc : public AuraScript
{
    PrepareAuraScript(era_pri_vampiric_touch_tbc);
    enum : uint32 { SPELL_VT_ENERGIZE = 948063 };

    // Party-mana range sanity gate, mirroring era_vampiric_embrace's VE_HEAL_RANGE.
    static constexpr float VT_MANA_RANGE = 100.0f;

    bool Validate(SpellInfo const* /*spellInfo*/) override { return ValidateSpellInfo({ SPELL_VT_ENERGIZE }); }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        if (!sEraTalentsConfig->Enabled())
            return false;
        DamageInfo* dmg = eventInfo.GetDamageInfo();
        if (!dmg || !dmg->GetDamage() || !(dmg->GetSchoolMask() & SPELL_SCHOOL_MASK_SHADOW))
            return false;
        Unit* caster = GetCaster();
        // GetActor() is the damage DEALER on a taken-side event (see the banner) — this is the
        // "only MY Vampiric Touch feeds off MY damage" check.
        return caster && eventInfo.GetActor() == caster;
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& eventInfo)
    {
        PreventDefaultAction();

        Unit* caster = GetCaster();
        if (!caster)
            return;
        int32 pct = aurEff->GetAmount();            // the DUMMY's own value: 5 on every TBC rank
        int32 mana = CalculatePct(int32(eventInfo.GetDamageInfo()->GetDamage()), pct);
        if (pct <= 0 || mana <= 0)
            return;

        Player* priest = caster->ToPlayer();
        Group* group = priest ? priest->GetGroup() : nullptr;
        if (group)
        {
            for (GroupReference* itr = group->GetFirstMember(); itr; itr = itr->next())
            {
                Player* member = itr->GetSource();
                if (member && group->SameSubGroup(priest, member) && member->IsInMap(caster)
                    && member->IsAlive() && member->getPowerType() == POWER_MANA
                    && caster->GetDistance(member) <= VT_MANA_RANGE)
                    caster->CastCustomSpell(member, SPELL_VT_ENERGIZE, &mana, nullptr, nullptr,
                                            true, nullptr, aurEff);
            }
        }
        else
        {
            caster->CastCustomSpell(caster, SPELL_VT_ENERGIZE, &mana, nullptr, nullptr, true,
                                    nullptr, aurEff);
        }
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(era_pri_vampiric_touch_tbc::CheckProc);
        OnEffectProc += AuraEffectProcFn(era_pri_vampiric_touch_tbc::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// ===================================================================================================
// TBC MAGE — Ice Barrier (node 20863), the ONE new script of TBC Phase 9. Bound by `scriptBindings:`
// to the six era clones 948964-948969 (era-data/tbc/mage.yaml).
//
// WHY IT EXISTS: Ice Barrier's spell-power coefficient is not DBC data — it is HARDCODED in core
// code. `spell_mage_ice_barrier_aura::CalculateSpellAmount` and `spell_mage_ice_barrier::
// CalculateSpellAmount` (azerothcore-wotlk/src/server/scripts/Spells/spell_mage.cpp:627 and :663)
// both open with `float bonus = 0.8068f;`. TBC's coefficient is **0.1** — cmangos `mangos-tbc`
// `sql/base/mangos.sql:14318` `(11426, 0.1, 0, 0, 0, 'Mage - Ice Barrier')`, a `spell_bonus_data`
// row chain-rooted at 11426 by the same file's `spell_chain` block (lines 14654-14659), so it
// governs all six ranks. An 8x divergence at level-70 spell power; the DBC rows themselves are
// byte-identical r1-r5 (r6 differs only in the mana encoding), so this coefficient WAS the whole
// era divergence and the clone chain exists solely to carry it.
//
// The stock scripts are bound to **-11426**, i.e. the stock chain, so they never see a band clone
// (workflow lesson 6) — a clone with no script of its own would get NO spell-power bonus at all.
// This script therefore supplies the WHOLE bonus, in the stock shape with the coefficient swapped.
//
// The stock AuraScript's OTHER half is deliberately NOT reproduced: `spell_mage_ice_barrier_aura`
// also registers `AfterEffectAbsorb -> spell_mage_incanters_absorbtion_base_AuraScript::Trigger`
// (spell_mage.cpp:651), which feeds **Incanter's Absorption** — a WotLK-only talent
// (SPELL_MAGE_INCANTERS_ABSORBTION_R1) a TBC mage can never hold, so the hook would be inert.
// Nor is the stock SpellScript `spell_mage_ice_barrier` rebound: its CheckCast refuses a recast
// that would replace a stronger barrier (SPELL_FAILED_AURA_BOUNCED) but computes the comparison
// value with its own hardcoded 0.8068, which would not match a 0.1-coefficient clone's real amount
// and would refuse legitimate recasts. Recorded as an accepted divergence, not an oversight.
//
// Gated on `Enabled()` + a live caster ONLY — never on "is a bot" (CLAUDE.md: an era-managed bot
// owns these clones too, and a GET_PLAYERBOT_AI bail would silently zero the bot's shield).
constexpr float ERA_ICE_BARRIER_TBC_COEFF = 0.1f;   // cmangos mangos-tbc spell_bonus_data (11426, 0.1)

class era_mag_ice_barrier_tbc : public AuraScript
{
    PrepareAuraScript(era_mag_ice_barrier_tbc);

    void CalcAmount(AuraEffect const* aurEff, int32& amount, bool& canBeRecalculated)
    {
        canBeRecalculated = false;
        if (!sEraTalentsConfig->Enabled())
            return;

        Unit* caster = GetCaster();
        if (!caster)
            return;

        // spell_mage_ice_barrier_aura::CalculateSpellAmount, verbatim, with TBC's coefficient.
        float bonus = ERA_ICE_BARRIER_TBC_COEFF;
        bonus *= caster->SpellBaseDamageBonusDone(GetSpellInfo()->GetSchoolMask());
        bonus = caster->ApplyEffectModifiers(GetSpellInfo(), aurEff->GetEffIndex(), bonus);
        bonus *= caster->CalculateLevelPenalty(GetSpellInfo());

        amount += int32(bonus);

        if (sEraTalentsConfig->Debug())
            LOG_INFO("module", "[era_mag_ice_barrier_tbc] {} spell {} absorb -> {} (+{} from SP at {})",
                     caster->GetName(), GetSpellInfo()->Id, amount, int32(bonus),
                     ERA_ICE_BARRIER_TBC_COEFF);
    }

    void Register() override
    {
        DoEffectCalcAmount += AuraEffectCalcAmountFn(era_mag_ice_barrier_tbc::CalcAmount, EFFECT_0,
                                                     SPELL_AURA_SCHOOL_ABSORB);
    }
};

// -------------------------------------------------------------------------------------------------
// era_pal_judgement_of_command — Judgement of Command deals DOUBLE damage against a stunned target
// (both eras: 2.4.3 and 1.12.1 tooltips carry the "X Holy damage, or Y if the target is stunned"
// pair; WotLK dropped the mechanic). Bound by scriptBindings to the flat JoC clones the seal-unleash
// path casts: TBC 946103-946108, Vanilla 932673-932677. Runs on the effect's base roll BEFORE
// SpellDamageBonusDone, so the spell-power coefficient scales with the doubling — TBC's stunned
// value was the full value x2. Closes final-review §11 row G-3.
// Gated on Enabled() + a Player caster ONLY — never on "is a bot" (an era-managed bot judges too).
class era_pal_judgement_of_command : public SpellScript
{
    PrepareSpellScript(era_pal_judgement_of_command);

    bool Load() override { return GetCaster() && GetCaster()->IsPlayer(); }

    void HandleDamage(SpellEffIndex /*effIndex*/)
    {
        if (!sEraTalentsConfig->Enabled())
            return;
        Unit* target = GetHitUnit();
        if (!target)
            return;
        if (target->HasAuraWithMechanic(1 << MECHANIC_STUN))
            SetHitDamage(GetHitDamage() * 2);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(era_pal_judgement_of_command::HandleDamage, EFFECT_0, SPELL_EFFECT_SCHOOL_DAMAGE);
    }
};

// -------------------------------------------------------------------------------------------------
// era_rampage_consume_ready — TBC Rampage (947533/947534/947535) is castable only while the caster
// holds the 5 s "Rampage Ready" aura 947540 (casterAuraSpell; applied by the crit-watcher 947539).
// TBC cleared that window on use (wowsims: rampageValidUntil = 0 in ApplyEffects); without this the
// warrior could recast inside the window and every cast adds a stack via the clone's effect 64.
// AfterCast (not AfterHit): Rampage is a self-buff. Closes final-review §11 row G-12 / TBC warrior
// _ref accepted_gaps id 4.
class era_rampage_consume_ready : public SpellScript
{
    PrepareSpellScript(era_rampage_consume_ready);

    enum : uint32 { SPELL_ERA_RAMPAGE_READY = 947540 };

    void HandleAfterCast()
    {
        if (!sEraTalentsConfig->Enabled())
            return;
        if (Unit* caster = GetCaster())
            caster->RemoveAurasDueToSpell(SPELL_ERA_RAMPAGE_READY);
    }

    void Register() override
    {
        AfterCast += SpellCastFn(era_rampage_consume_ready::HandleAfterCast);
    }
};

// -------------------------------------------------------------------------------------------------
// era_dru_leader_of_the_pack — TBC Leader of the Pack party aura 946257 (template 24932). The core
// spell_dru_leader_of_the_pack (spell_druid.cpp) casts the 34299 heal AND, for the aura's own caster
// (= Improved LotP), the 68285 mana return — a WotLK 3.0 addition TBC never had. This is the core
// script's HandleProc verbatim MINUS the mana branch. 946257's scriptBindings line now names this
// script instead of the core one. Closes final-review §11 row G-6 / TBC druid accepted gap
// "Improved LotP mana return".
class era_dru_leader_of_the_pack : public AuraScript
{
    PrepareAuraScript(era_dru_leader_of_the_pack);

    enum : uint32 { SPELL_LOTP_HEAL = 34299 };

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_LOTP_HEAL });
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& /*eventInfo*/)
    {
        PreventDefaultAction();
        if (!sEraTalentsConfig->Enabled())
            return;
        Unit* target = GetTarget();
        int32 healAmount = aurEff->GetAmount();
        if (!target || healAmount <= 0)
            return;

        // 6 second internal cooldown (same as core)
        if (target->IsPlayer() && target->ToPlayer()->HasSpellCooldown(SPELL_LOTP_HEAL))
            return;

        int32 bp = target->CountPctFromMaxHealth(healAmount);
        target->CastCustomSpell(SPELL_LOTP_HEAL, SPELLVALUE_BASE_POINT0, bp, target, true, nullptr, aurEff);

        if (target->IsPlayer())
            target->ToPlayer()->AddSpellCooldown(SPELL_LOTP_HEAL, 0, 6 * IN_MILLISECONDS);
        // NO 68285 mana return: TBC Improved LotP heals only.
    }

    void Register() override
    {
        OnEffectProc += AuraEffectProcFn(era_dru_leader_of_the_pack::HandleProc, EFFECT_1, SPELL_AURA_DUMMY);
    }
};

// -------------------------------------------------------------------------------------------------
// era_mag_cold_snap — Vanilla Cold Snap (node 18041), bound to the era clone 932335.
//
// WHY A MODULE SCRIPT AND NOT A REBIND OF CORE `spell_mage_cold_snap` (spell_mage.cpp:485). The
// core script is bound to the POSITIVE id 11958, so it is absent on a band clone (workflow lesson
// 6) — and re-binding it would be WRONG, not merely insufficient: its reset loop skips exactly one
// spell, `SPELL_MAGE_COLD_SNAP` = 11958. Run on the clone, the loop would find 932335 itself
// (SpellFamilyName MAGE, frost school, RecoveryTime 600000 > 0) and clear COLD SNAP'S OWN cooldown
// — a free infinite reset. This copy excludes BOTH ids and is otherwise the core logic verbatim.
//
// The clone exists only for the cooldown: live 11958 is RecoveryTime 480000 (8 min), Vanilla's Cold
// Snap is 10 min. Bound to a band clone id, so it is inherently Vanilla-only and needs no era check
// — and it must NOT gate on "is a bot" (CLAUDE.md: an era-managed bot owns this clone and needs the
// reset to work). Bails immediately when the module is disabled (hook-guard discipline).
class era_mag_cold_snap : public SpellScript
{
    PrepareSpellScript(era_mag_cold_snap);

    enum : uint32
    {
        ERA_COLD_SNAP_STOCK = 11958,    // core SPELL_MAGE_COLD_SNAP
        ERA_COLD_SNAP_CLONE = 932335,   // this script's own binding — never reset by itself
    };

    bool Load() override
    {
        return GetCaster() && GetCaster()->IsPlayer();
    }

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        if (!sEraTalentsConfig->Enabled())
            return;

        Player* caster = GetCaster()->ToPlayer();
        if (!caster)
            return;

        uint32 cleared = 0;
        PlayerSpellMap const& spellMap = caster->GetSpellMap();
        for (PlayerSpellMap::const_iterator itr = spellMap.begin(); itr != spellMap.end(); ++itr)
        {
            SpellInfo const* spellInfo = sSpellMgr->AssertSpellInfo(itr->first);
            if (spellInfo->SpellFamilyName != SPELLFAMILY_MAGE)
                continue;
            if (!(spellInfo->GetSchoolMask() & SPELL_SCHOOL_MASK_FROST))
                continue;
            if (spellInfo->Id == ERA_COLD_SNAP_STOCK || spellInfo->Id == ERA_COLD_SNAP_CLONE)
                continue;
            if (spellInfo->GetRecoveryTime() == 0)
                continue;

            SpellCooldowns::iterator citr = caster->GetSpellCooldownMap().find(spellInfo->Id);
            if (citr != caster->GetSpellCooldownMap().end() && citr->second.needSendToClient)
                caster->RemoveSpellCooldown(spellInfo->Id, true);
            else
                caster->RemoveSpellCooldown(spellInfo->Id, false);
            ++cleared;
        }

        if (sEraTalentsConfig->Debug())
            LOG_INFO("module", "[era_mag_cold_snap] {} reset {} frost cooldown(s)",
                     caster->GetName(), cleared);
    }

    void Register() override
    {
        OnEffectHit += SpellEffectFn(era_mag_cold_snap::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

void AddSC_era_talent_proc_scripts()
{
    RegisterSpellScript(era_sha_flametongue_totem_proc);
    RegisterSpellScript(era_sha_nature_guardian);
    RegisterSpellScript(era_pri_reflective_shield_tbc);
    RegisterSpellScript(era_pri_vampiric_touch_tbc);
    RegisterSpellScript(era_dru_tree_of_life);
    RegisterSpellScript(era_dru_tree_of_life_suppress);
    RegisterSpellScript(era_hunter_scorpid_sting);
    RegisterSpellScript(era_hun_expose_weakness_tbc);
    RegisterSpellScript(era_hun_readiness_tbc);
    RegisterSpellScript(era_pal_ardent_defender);
    RegisterSpellScript(era_pal_holy_shock);
    RegisterSpellScript(era_pal_seal_of_righteousness);
    RegisterSpellScript(era_pal_judgement_of_light);
    RegisterSpellScript(era_pal_judgement_of_wisdom);
    RegisterSpellScript(era_pal_judgement);
    RegisterSpellScript(era_pal_seal_of_blood);
    RegisterSpellScript(era_pal_judgement_of_blood);
    RegisterSpellScript(era_pal_seal_of_vengeance);
    RegisterSpellScript(era_pal_judgement_of_vengeance);
    RegisterSpellScript(era_pal_crusader_strike);
    RegisterSpellScript(era_pal_jotc_crit);
    RegisterSpellScript(era_deep_wounds);
    RegisterSpellScript(era_improved_berserker_rage);
    RegisterSpellScript(era_bloodthirst_vanilla);
    RegisterSpellScript(era_last_stand);
    RegisterSpellScript(era_wyvern_sting);
    RegisterSpellScript(era_hun_beast_within_tbc);
    RegisterSpellScript(era_rog_preparation);
    RegisterSpellScript(era_rog_preparation_tbc);
    RegisterSpellScript(era_rog_sap_stealth);
    RegisterSpellScript(era_rog_mutilate_poison);
    RegisterSpellScript(era_rog_mutilate_behind);
    RegisterSpellScript(era_firestone_spellpower);
    RegisterSpellScript(era_firestone_proc_dmg);
    RegisterSpellScript(era_spellstone_absorb);
    RegisterSpellScript(era_spellstone_crit_rating);
    RegisterSpellScript(era_ignite);
    RegisterSpellScript(era_master_of_elements);
    RegisterSpellScript(era_mag_ice_barrier_tbc);
    RegisterSpellScript(era_mag_cold_snap);
    RegisterSpellScript(era_pal_judgement_of_command);
    RegisterSpellScript(era_rampage_consume_ready);
    RegisterSpellScript(era_dru_leader_of_the_pack);
    RegisterSpellScript(era_ward_reflect);
    RegisterSpellScript(era_blessed_recovery);
    RegisterSpellScript(era_vampiric_embrace);
    RegisterSpellScript(era_demonic_sacrifice);
    RegisterSpellScript(era_soul_link);
    RegisterSpellScript(era_improved_drain_mana);
    RegisterSpellScript(era_improved_drain_soul);
    RegisterSpellScript(era_amplify_curse);
    new era_soul_link_relink();
    // Additional scripted procs registered here as they are authored:
    //   era_magic_absorption (mana-on-resist rider)
}
