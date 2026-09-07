// Totem behavior scripts for mod-era-talents' Vanilla + TBC totems.
//  - spell_era_windfury_totem : Windfury Totem's party weapon-imbue proc (one on-melee extra attack).
//    The totem's passive party pulse (931366-368) applies a DUMMY area aura to each party member; on a
//    white melee swing it casts Windfury Attack (25504/33750) once, with per-rank bonus AP (carried in
//    the pulse's Effect_1) scaled by the TOTEM OWNER's spellmods so Improved Weapon Totems applies.
//    Mirrors core spell_sha_windfury_weapon, but NOT cast-item bound and 1 extra attack (Vanilla), not 2.
//  - npc_era_fire_nova_totem : Fire Nova Totem's delayed self-destruct AoE (creature_template
//    ScriptName on 920280-284; overrides factory TotemAI so the totem waits then self-blasts
//    instead of seeking an enemy).
#include "ScriptMgr.h"
#include "SpellScript.h"
#include "SpellAuraEffects.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "Player.h"
#include "Item.h"               // Item::GetEnchantmentId + TEMP_ENCHANTMENT_SLOT (WF Weapon non-stack)
#include "ScriptedCreature.h"   // ScriptedAI
#include "EraTalentsConfig.h"   // sEraTalentsConfig->Enabled()

enum EraTotemSpells
{
    SPELL_ERA_WF_ATTACK_MH = 25504,
    SPELL_ERA_WF_ATTACK_OH = 33750,
    // Improved Fire Totems (node 18808) auto-passive auras carried by the shaman OWNER when the
    // talent is learned (auto-passive id formula 920000 + (nodeId-18000)*8 + rank-1; verified vs the
    // generated spell_dbc — aura 108 SPELLMOD_THREAT -25/-50% on Magma pulse bit2). The AI reads these
    // to shorten the Fire Nova detonation delay by 1s/rank (data has no spellmod op for a totem timer).
    SPELL_ERA_IMP_FIRE_TOTEMS_R1 = 926464,
    SPELL_ERA_IMP_FIRE_TOTEMS_R2 = 926465,
    // The TBC tree's Improved Fire Totems is node 20208 (same delay halves, same formula:
    // 920000 + (20208-18000)*8 + rank-1). A TBC-era shaman carries these instead of the Vanilla pair.
    SPELL_ERA_TBC_IMP_FIRE_TOTEMS_R1 = 937664,
    SPELL_ERA_TBC_IMP_FIRE_TOTEMS_R2 = 937665,
};

// Windfury Weapon self-imbue temporary-enchant ids — every rank 8232/8235/10486/16362/25505/58801/
// 58803/58804 applies one of these via SPELL_EFFECT_ENCHANT_ITEM_TEMPORARY (dump 2026-08-24). Vanilla
// makes Windfury Totem and a Windfury Weapon self-imbue MUTUALLY EXCLUSIVE on the same weapon (both are
// the "Windfury" weapon buff), so the totem must not add a second extra-attack proc to a hand already
// carrying a WF Weapon enchant.
static bool IsWindfuryWeaponEnchant(uint32 enchantId)
{
    switch (enchantId)
    {
        case 283: case 284: case 525: case 1669:
        case 2636: case 3785: case 3786: case 3787:
            return true;
        default:
            return false;
    }
}

class spell_era_windfury_totem : public AuraScript
{
    PrepareAuraScript(spell_era_windfury_totem);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_ERA_WF_ATTACK_MH, SPELL_ERA_WF_ATTACK_OH });
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        // Only white melee auto-attacks proc. A non-null GetSpellInfo() means the hit came from a
        // spell (Windfury Attack itself, Stormstrike, Lava Lash, ...) — never proc off those.
        Player* swinger = eventInfo.GetActor() ? eventInfo.GetActor()->ToPlayer() : nullptr;
        if (!swinger)
            return false;
        if (eventInfo.GetSpellInfo())
            return false;
        bool mainHand = (eventInfo.GetTypeMask() & PROC_FLAG_DONE_MAINHAND_ATTACK) != 0;
        bool offHand  = (eventInfo.GetTypeMask() & PROC_FLAG_DONE_OFFHAND_ATTACK) != 0;
        if (!mainHand && !offHand)
            return false;
        // Vanilla non-stacking: a hand already imbued with Windfury Weapon does NOT also get the
        // Windfury Totem proc (the self-imbue and the totem buff are the same "Windfury" weapon effect).
        if (Item* weapon = swinger->GetWeaponForAttack(mainHand ? BASE_ATTACK : OFF_ATTACK, true))
            if (IsWindfuryWeaponEnchant(weapon->GetEnchantmentId(TEMP_ENCHANTMENT_SLOT)))
                return false;
        return true;
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& eventInfo)
    {
        PreventDefaultAction();
        Player* player = eventInfo.GetActor()->ToPlayer();   // the party member swinging
        Unit* target = eventInfo.GetActionTarget();
        if (!player || !target)
            return;

        bool mainHand = (eventInfo.GetTypeMask() & PROC_FLAG_DONE_MAINHAND_ATTACK) != 0;
        uint32 spellId = mainHand ? SPELL_ERA_WF_ATTACK_MH : SPELL_ERA_WF_ATTACK_OH;

        // Bonus ATTACK POWER rides the pulse's Effect_1 (Vanilla Windfury Totem = +122/229/315 AP on
        // the extra swing). Compute from the TOTEM OWNER's spellmods (GetCaster() is the totem; its
        // owner is the shaman) so Improved Weapon Totems (+15/30%) scales it.
        Unit* casterTotem = GetCaster();
        Unit* ownerUnit = casterTotem ? casterTotem->GetOwner() : nullptr;
        Player* totemOwner = ownerUnit ? ownerUnit->ToPlayer() : player;   // fall back to the swinger
        int32 ap = GetSpellInfo()->Effects[EFFECT_1].CalcValue(totemOwner);
        // Convert bonus AP -> added swing damage the WotLK way: (AP / 14) per second of weapon speed.
        // Feeding raw AP * speed (no /14) overshot ~14x (a 122-AP rank-1 proc hit ~408 vs a ~24 normal
        // swing) — the review's I3. Unlike the stock Windfury WEAPON script, whose Effect carries a
        // pre-divided per-second coefficient, our pulse Effect_1 is the raw MOD_ATTACK_POWER value.
        int32 bonus = int32(ap / 14.0f * player->GetAttackTime(mainHand ? BASE_ATTACK : OFF_ATTACK) / 1000.f);

        // ONE extra attack (Vanilla totem = 1; the personal Windfury Weapon casts twice for 2).
        player->CastCustomSpell(spellId, SPELLVALUE_BASE_POINT0, bonus, target, true, nullptr, aurEff);
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_era_windfury_totem::CheckProc);
        OnEffectProc += AuraEffectProcFn(spell_era_windfury_totem::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// npc_era_fire_nova_totem : Fire Nova Totem's Vanilla "detonate then consume" behavior. The DB
// ScriptName (set on creatures 920280-284 in creature_template) overrides the factory TotemAI,
// which is required here: TotemAI's active mode makes a totem seek and blast the nearest enemy,
// but Fire Nova Totem is a fixed-position totem that waits ~4s then self-detonates in place.
static constexpr uint32 FIRE_NOVA_DETONATE_MS = 4000;   // ~4s Vanilla base detonation delay (AI constant; no spell-data hook)

struct npc_era_fire_nova_totem : public ScriptedAI
{
    npc_era_fire_nova_totem(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override
    {
        _timer = FIRE_NOVA_DETONATE_MS;
        _fired = false;
        _delayResolved = false;
    }

    // Improved Fire Totems shortens the detonation delay by 1s/rank (rank1 -> 3000ms, rank2 -> 2000ms).
    // Resolved from the totem OWNER's talent. Fail safe: module disabled / no owner / owner not a
    // player / no talent -> the 4000ms base. The delay-half is NOT data-expressible (a spellmod has no
    // op for a totem detonation timer), so the AI reads the talent's own passive aura directly.
    uint32 ResolveDetonateDelay() const
    {
        if (!sEraTalentsConfig->Enabled())
            return FIRE_NOVA_DETONATE_MS;

        Unit* owner = me->GetOwner();
        Player* shaman = owner ? owner->ToPlayer() : nullptr;
        if (!shaman)
            return FIRE_NOVA_DETONATE_MS;

        uint32 reduce = 0;
        if (shaman->HasAura(SPELL_ERA_IMP_FIRE_TOTEMS_R2) ||
            shaman->HasAura(SPELL_ERA_TBC_IMP_FIRE_TOTEMS_R2))      reduce = 2000;   // Improved Fire Totems rank 2 (Vanilla 18808 / TBC 20208)
        else if (shaman->HasAura(SPELL_ERA_IMP_FIRE_TOTEMS_R1) ||
                 shaman->HasAura(SPELL_ERA_TBC_IMP_FIRE_TOTEMS_R1)) reduce = 1000;   // rank 1
        return (FIRE_NOVA_DETONATE_MS > reduce) ? (FIRE_NOVA_DETONATE_MS - reduce) : FIRE_NOVA_DETONATE_MS;
    }

    void UpdateAI(uint32 diff) override
    {
        if (_fired)
            return;

        // Resolve the talent-shortened delay ONCE, on the first tick. The owner link is not always
        // established at Reset() (the investigation's note), so reading it here is reliable — the base
        // was already latched in Reset(), so a null owner just leaves the 4000ms default in place.
        if (!_delayResolved)
        {
            _timer = ResolveDetonateDelay();
            _delayResolved = true;
        }

        if (_timer <= diff)
        {
            _fired = true;
            uint32 nova = me->m_spells[0];          // the rank's Fire Nova blast (creature_template_spell Index 0)
            if (nova)
                me->CastSpell(me, nova, true);      // AoE fire centered on the totem (blast targetA=22 hits enemies in radius)
            me->DespawnOrUnsummon(200ms);           // consumed on detonation
        }
        else
            _timer -= diff;
    }

private:
    uint32 _timer = FIRE_NOVA_DETONATE_MS;
    bool _fired = false;
    bool _delayResolved = false;
};

void AddSC_era_talent_totem_scripts()
{
    RegisterSpellScript(spell_era_windfury_totem);
    RegisterCreatureAI(npc_era_fire_nova_totem);
}
