# Shaman Totem Talent Fix-Round Investigation (read-only)

Root-cause + recommended-fix report for three runtime bugs a player found testing the Vanilla
Shaman talents against the newly-rebuilt Vanilla totems (mod-era-talents, branch `feat/era-talents`).
All three concern whether a talent SPELLMOD reaches a totem's behavior. Investigation is source-only
(no edits); where a runtime behavior cannot be proven from source it is called out explicitly with
the test that would settle it.

## How Vanilla buff totems work at runtime (shared background)

Every friendly-target totem is: a player-cast **SUMMON** spell (era clone in `[931000,931399]`,
`effect 28` SPELL_EFFECT_SUMMON, `misc` = the totem `creature_template.entry` in `[920100,920290]`)
that plants a **totem NPC**; the NPC's `creature_template_spell` Index 0 = a **PULSE** spell
(another era clone) loaded into the totem's `m_spells[0]` and cast by core `Totem::InitSummon` /
`TotemAI`. The buff is delivered by that pulse: `effect 65` SPELL_EFFECT_APPLY_AREA_AURA_RAID
(or `35` PARTY) with `targetA 1` (self) — the totem casts a **unit area aura on itself** that
propagates to grouped members inside the effect's radius (confirmed: `SharedDefines.h:801/831`).

Two runtime facts govern every question below (both verified in the fork):

- **A totem's `GetSpellModOwner()` returns the shaman.** `Unit::GetSpellModOwner()`
  (`Entities/Unit/Unit.cpp:12983`) returns `GetOwner()->ToPlayer()`; a summoned totem's owner is the
  caster. Proof it is wired: the buffs reach party members at all, and `UnitAura::FillTargetMap`
  (`Spells/Auras/SpellAuras.cpp:2838`) selects targets via `AnyGroupedUnitInObjectRangeCheck` keyed
  off the totem's owner-group — which only works because the owner link resolves.
- **The buff **amount** lives on the PULSE, the buff **radius** lives on the PULSE, the summon carries
  neither.** The summon only plants the creature (`effect 28`).

Whether a talent SPELLMOD reaches a pulse therefore turns on: (a) does the mod's
`EffectSpellClassMask` intersect the **pulse** spell's family flags (`SpellInfo::IsAffected`,
`SpellInfo.cpp:1326`; `IsAffectedBySpellMod`, `:1345`), and (b) does the runtime path that computes
that quantity call it with a caster whose `GetSpellModOwner` is the shaman.

---

## Bug 1 — Totemic Mastery (node 18838, passive 926704): CONFIRMED BROKEN (inert)

**Verdict: real bug, data-only fixable by rebinding to the pulse spells.**

### Root cause (proven)
Totemic Mastery is authored `op: radius, kind: flat` with `affectsMask` = the OR of the five totem
**SUMMON** identities (`era-data/vanilla/shaman.yaml:1111-1114`). The generated passive 926704
(`.../2026_08_21_61_era_talent_shaman_custom_spells.sql:886`) is aura 107 ADD_FLAT_MODIFIER, misc **6**
(SPELLMOD_RADIUS), basePoints 9 → +10, **EffectSpellClassMaskA_1 = 537399320** (= Stoneclaw bit3 |
Searing bit4 | Magma bit12 | Mana Spring bit19 | SoE bit29).

The buff reach is the PULSE area-aura's radius, and each buff pulse carries its **own** family bits,
none of which is in 537399320:

| Totem | pulse id | family-11 bits |
|---|---|---|
| Stoneskin | 931030 (tmpl 8072) | A bit15 (32768) |
| Strength of Earth | 931050 (tmpl 8076) | A bit16 (65536) |
| Mana Spring | 931210 (tmpl 5677) | A bit14 (16384) |
| Healing Stream | 931190 (tmpl 5672) | A bit13 (8192) |
| Frost / Fire / Nature Resistance | 931170/931230/931310 (tmpl 8182/8185/10596) | A bit26 (67108864), shared by all three |
| Grace of Air | 931330 (from-scratch) | C bit15 (32768, minted) |
| Windwall | 931350 (from-scratch) | C bit16 (65536, minted) |

`IsAffected(11, 537399320)` against any pulse above = 0 intersection → the pulse is **not affected**
→ `SPELLMOD_RADIUS` is never applied to it. The node's own comment already flagged this exact risk.
The bind is fully inert on buff radius (the SUMMON's SPELL_EFFECT_SUMMON has no meaningful radius).

### Does SPELLMOD_RADIUS provably reach the pulse if rebound? YES.
`SpellEffectInfo::CalcRadius` (`SpellInfo.cpp:553-568`) applies the mod via
`caster->GetSpellModOwner()->ApplySpellMod(pulseId, SPELLMOD_RADIUS, radius)`, and
`UnitAura::FillTargetMap` recomputes `radius = Effects[i].CalcRadius(caster)` on **every** target
scan (`SpellAuras.cpp:2828`) with `caster` = the totem → modOwner = shaman. So a pulse-scoped
`SPELLMOD_RADIUS` flat +10 grows the party/raid selection radius live. The fix is provably effective.

### Recommended fix (data-only rebind — no C++)
Rebind Totemic Mastery from the summons to the friendly-target **pulses**. Exact corrected mask
covering every friendly buff/heal totem (OR of the bits in the table above):

```yaml
affects: [Stoneskin Totem Effect, Strength of Earth, Mana Spring Totem, Healing Stream Totem,
          Frost Resistance, Fire Resistance, Nature Resistance, Grace of Air, Windwall]  # display
affectsMask: { a: 67231744, c: 98304 }
# a = 32768|65536|16384|8192|67108864 ; c = 32768|65536
```
`_ref` will need entries for the vanilla pulse ids/bits not yet catalogued (8182/8185/10596 all A26;
5677 A14; 5672 A13) to satisfy the `affects:`/lint path; the DBC mask is what actually binds.
A26 (67108864) among the era pulse set is carried **only** by the three resistance buffs (the era
Mana Spring/Healing Stream use A14/A13, not the WotLK A26 pulses), so binding A26 is clean.

Cleaner long-term alternative: mint one shared "friendly totem pulse" family bit (e.g. a free
word-C bit) onto every friendly-totem pulse clone and bind Totemic Mastery to that single bit —
avoids the heterogeneous mask entirely. (Grounding's SPELL_MAGNET pulse 931290 is optional scope; a
radius mod on it is harmless if the reconstruction wants Vanilla parity.)

---

## Bug 2 — Guardian Totems (node 18817, passive 926536/926537)

**Verdict: Effect_2 (Grounding cooldown) works as reported. Effect_1 (Stoneskin/Windwall
damage-reduction) is NOT a binding bug — the data and runtime path are correct and
SPELLMOD_ALL_EFFECTS provably reaches the pulse amount. The "doesn't work" report is a
perception/verification artifact (integer truncation at low ranks + static tooltip). Needs a
combat-log measurement to close, not a code change.**

### Data is exactly as designed (verified in generated SQL)
Passive 926536 (`...custom_spells.sql:661`): Effect_1 = aura 108 ADD_PCT_MODIFIER, misc **8**
(SPELLMOD_ALL_EFFECTS), basePoints 9 → +10%, mask **A=32768 (Stoneskin pulse bit15) + C=65536
(Windwall pulse bit16)**; Effect_2 = aura 107 ADD_FLAT_MODIFIER, misc **11** (SPELLMOD_COOLDOWN),
basePoints -1001 → -1000ms, mask A=262144 (Grounding summon bit18). Rank 2 = +20% / -2000ms. All
bits match the intended targets exactly.

Both pulses are spellmod-eligible and correctly family-tagged (dumped from generated SQL):
- Stoneskin pulse 931030: SpellFamilyName 11, A bit15; `AttributesEx3 = 0` → `IsAffectedBySpellMods()`
  true (`SpellInfo.cpp:1340`). (The inherited `AttributesEx6=67108864` is
  SPELL_ATTR6_VEHICLE_IMMUNITY_CATEGORY, irrelevant to spellmods.)
- Windwall pulse 931350 (from-scratch): SpellFamilyName **11**, C bit16 — the family authoring is
  correct, so the Windwall half is not silently stranded.

### Why Effect_2 works and Effect_1 *also* reaches its target
- **Effect_2 (Grounding cd)** applies when the **player** casts the summon 931280; player is the direct
  caster/modOwner → cooldown mod applies. Player confirms it. This also proves the passive aura and
  its two SpellModifiers are live on the shaman.
- **Effect_1 (pulse amount)**: the amount is computed once at aura creation,
  `AuraEffect::CalculateAmount(caster)` → `SpellEffectInfo::CalcValue(caster, ...)`
  (`SpellAuraEffects.cpp:393,456` → `SpellInfo.cpp:410`) → `Unit::ApplyEffectModifiers`
  (`Unit.cpp:11589`), which calls `GetSpellModOwner()->ApplySpellMod(pulseId, SPELLMOD_ALL_EFFECTS, value)`.
  `caster` here is the **totem** (the area aura's caster; `AuraEffect` ctor is passed the aura caster),
  and `totem->GetSpellModOwner()` = the shaman, who holds the mod, and `IsAffected` matches bit15/bit16.
  **So the +10/20% is applied to the pulse amount.** SPELLMOD_ALL_EFFECTS provably reaches it.

### Why the player nonetheless sees "no effect"
1. **Integer truncation** (`Player::ApplySpellMod`, `Player.cpp:10080`; `CalcValue` returns
   `int32(value)`, `SpellInfo.cpp:521`). Stoneskin rank-1 reduction is **-3**: `-3 × 1.10 = -3.3 →
   -3` (no change); `-3 × 1.20 = -3.6 → -3`. The mod is literally invisible at low ranks and only
   becomes a whole-point change at high ranks (rank 6: `-27 → -32`).
2. The buff's **AuraDescription tooltip is static** ("Melee/Ranged damage taken reduced by N") — a
   spellmod never rewrites it, so inspecting the buff shows no change regardless.
3. A few-point flat melee/ranged damage-taken reduction is nearly imperceptible in live combat.

### Recommendation
No code/data change to the binding. Verify with a **combat-log measurement** at rank 2 using a
**high-rank** Stoneskin (e.g. reduction 27 → expect ~32) or high-rank Windwall — compare incoming
melee/ranged hit amounts with the talent at 0 vs 2 points. If (and only if) the amount provably does
not move at high magnitude in a combat log, escalate — but every code path inspected says it moves.
(If a guaranteed-visible effect is wanted at all ranks, that is a design change — e.g. make the
reduction a % — not a bug fix.)

---

## Bug 3 — Improved Fire Totems (node 18808, passive 926464/926465)

Two halves; they have opposite verdicts.

### 3a. Magma −threat (Effect_1): NOT broken — data + path are correct
`op: threat, kind: pct`, `affectsMask {a:4}` (`shaman.yaml:276`). Passive 926464/926465
(`...custom_spells.sql:595,598`): aura 108 ADD_PCT_MODIFIER, misc **2** (SPELLMOD_THREAT),
basePoints -26/-51 → **-25/-50%**, mask A=4 (Magma pulse bit2).

The era Magma pulse **931130 is `template: 8187`** and inherits 8187's mask `1073741828`
(bit2 | bit30) — so it **carries bit2** (dumped: `8187 mask=(1073741828,0,0)`). The threat path
`ThreatManager::CalculateModifiedThreat` (`Combat/ThreatManager.cpp:699-709`) applies
`victim->GetSpellModOwner()->ApplySpellMod(spell->Id, SPELLMOD_THREAT, threat)`, where `victim` is
the threat-**generating** unit (`AddThreat(target,...)` → `CalculateModifiedThreat(amount, target,...)`,
`:419`) = the Magma totem, whose `GetSpellModOwner` = the shaman, and `spell->Id` = 931130 which
carries bit2. So the −25/−50% applies to threat generated by Magma's AoE ticks. Mechanism is sound.

Caveat: cannot be proven from source that the resulting threat delta is player-visible (Magma tick
threat is small); confirm with a threat meter / `Logger` in a controlled pull. This is an
observability question, not a binding bug.

### 3b. Fire Nova detonation delay (Effect_2 half): CONFIRMED BROKEN — now fixable by script
Root cause: `modules/mod-era-talents/src/EraTalentTotemScripts.cpp:83,91,113` — the scripted totem
`npc_era_fire_nova_totem` uses a **hardcoded constant** `FIRE_NOVA_DETONATE_MS = 4000` and **never
reads the owner's talent**. Improved Fire Totems does nothing to the delay. The node comment
(`shaman.yaml:268-271`) documented this as a data gap ("no SPELLMOD op for a totem detonation timer")
— true for a spellmod, but the timer is now an **AI constant**, and the AI can read the talent
directly. So the gap is closable **without** spellmods/DBC/core patch.

Recommended fix (C++, `EraTalentTotemScripts.cpp`): in the CreatureAI, resolve the owner and subtract
per rank. `Totem::GetOwner()` returns the shaman (`Entities/Totem/Totem.cpp`); the talent passives are
live auras on the player (926464 r1 / 926465 r2 — same aura that makes 3a's mod work). Sketch:

```cpp
void Reset() override {
    uint32 reduce = 0;
    if (Unit* owner = me->GetOwner()) {
        if (owner->HasAura(926465))      reduce = 2000;   // Improved Fire Totems rank 2
        else if (owner->HasAura(926464)) reduce = 1000;   // rank 1
    }
    _timer = (FIRE_NOVA_DETONATE_MS > reduce) ? (FIRE_NOVA_DETONATE_MS - reduce) : 0;
    _fired = false;
}
```
(If the owner link is not yet established at `Reset()`, read it on the first `UpdateAI` tick instead.
`HasSpell(926464/926465)` on the owner is an equivalent guard.) No new DUMMY-marker registry value is
needed — the AI reads the talent's own passive ids.

---

## One-line verdicts

- **Totemic Mastery (18838):** BROKEN & inert — SPELLMOD_RADIUS is bound to the totem SUMMONS but buff
  radius lives on the PULSE area-aura; data-only fix = rebind to the pulses (`affectsMask {a:67231744, c:98304}`).
- **Guardian Totems (18817):** Grounding-cd half works; the Stoneskin/Windwall damage-reduction half is
  correctly bound and SPELLMOD_ALL_EFFECTS *provably reaches* the pulse amount (totem→shaman modOwner) —
  the report is truncation-at-low-rank + static-tooltip perception; verify by combat-log at high rank, no code change.
- **Improved Fire Totems (18808):** Magma −threat half is correctly bound and reaches the threat calc
  (needs a threat-meter to *observe*); the Fire Nova delay half is genuinely dead — the CreatureAI
  hardcodes 4000ms and ignores the talent, fixable now by having `npc_era_fire_nova_totem` read the
  owner's rank (926464/926465) and shorten the timer.
