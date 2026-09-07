# Vanilla Priest Talent Tree — Full Fidelity Audit

**Scope:** all 47 nodes of `era-data/vanilla/priest.yaml` (Discipline 15 / Holy 16 / Shadow 16),
graded against authentic Vanilla 1.12 (Daribon 1.12 calculator as the tooltip source).
**Report-only** — this audit changes no talent data, SQL, or C++. Every FLAGGED item below is a
*recommendation* for the user to approve, not an applied change.

**Verification date:** 2026-08-14. Live sources used: `tools/dump_spell_effects.py` /
`tools/dump_spell_family.py` against `azerothcore-wotlk/ip-dbc/Spell.dbc`; `tools/wgconsole.py
lookup spell` on the running dev-box worldserver; core source under
`azerothcore-wotlk/src/server/game/Spells/`; live `acore_world` reachable.

## Resolution — priest SHIPPED AS-IS (2026-08-14)

The user reviewed all 5 FLAGGED items and elected to **ship the priest tree as-is**, accepting the
current WotLK-stock behavior for every flagged node. None is a broken mechanic — each is a
fidelity-vs-effort nicety (tooltip text, cost/radius coverage, or Vanilla-vs-WotLK magnitude). No
talent data, SQL, or C++ was changed as a result of this audit. The two open design points below
(PI mana-cost reduction; VE heal distribution) are likewise left at their current models.

**Lightwell (18131) note:** during this pass the click→heal mechanic was reported broken on the
real client, investigated end-to-end (stock `npc_spellclick_spells` 31897→60123, condition, npcflag,
`npc_pet_pri_lightwell` AI + 59907 charges, `spell_pri_lightwell`/`_renew` scripts — all present and
correct in the **live** DB; nothing in mod-era-talents or IP touches the chain), and then
**confirmed working live** by the user (transient issue, not reproduced). The only standing item is
the heal-magnitude fidelity flag, accepted as-is.

## Grade summary

| Grade | Count | Nodes |
|-------|-------|-------|
| **FIXED** | 3 (+6 in the 2026-09-06 round) | 18115 Power Infusion, 18144 Vampiric Embrace, 18145 Improved Vampiric Embrace — plus **18106 Martyrdom, 18123 Inspiration, 18132 Spirit Tap, 18133 Blackout, 18136 Shadow Focus** and a second pass on 18115, all fixed in the 2026-09-06 final-review round (§1b) |
| **FLAGGED** | 5 | 18110 Mental Agility, 18124 Holy Reach, 18128 Spirit of Redemption, 18131 Lightwell, 18137 Improved Psychic Scream |
| **FAITHFUL** | 39 → 34 | all others (incl. 18103 Silent Resolve, 18104 Improved PW:Fortitude; **18123 Inspiration was cleared here against the wrong arbiter and is now FIXED — see §1**) |

Three of the eight flagged *candidates* were settled to **no change needed** by live data
(Silent Resolve, Improved PW:Fortitude, Inspiration); they are graded FAITHFUL and documented in
the investigation section below so the reasoning is on record. **Inspiration's clearance was
overturned on 2026-09-06** — it was settled against the live WotLK row rather than the era record;
see §1 and the round table in §1b.

## Full node table

### Discipline (tab 1)

| id | name | mechanic | implements | grade |
|----|------|----------|------------|-------|
| 18101 | Unbreakable Will | stat | aura 117 MOD_MECHANIC_RESISTANCE ×3 (stun/fear/silence) 3–15% | FAITHFUL |
| 18102 | Wand Specialization | stat | aura 79 MOD_DAMAGE_PERCENT_DONE, physical, wand-equip gated, 5–25% | **RETRACTED 2026-09-04 — INERT.** Premise wrong twice: `equipSubclass` 262144 is CROSSBOW (wand = 524288), and a Physical misc never reaches a magic-school wand shot (`EffectWeaponDmg` skips ranged TOTAL_PCT for non-physical). **Fix implemented 2026-09-04 (core patch 0025 + `school: 126`/`equipSubclass: 524288`, gen `bdad4294`), pending real-client sign-off** — `era-talents-phase3-priest.md` Fix round 1 |
| 18103 | Silent Resolve | spellmod | op 2 threat −4…−20% + op 28 resistDispel +10…+50% on 18 priest spells | FAITHFUL (settled) |
| 18104 | Improved PW:Fortitude | spellmod | op 8 allEffects +15/30% on PW:Fortitude (mask 8) | FAITHFUL (settled) |
| 18105 | Improved PW:Shield | spellmod | op 8 allEffects +5/10/15% on PW:Shield | FAITHFUL |
| 18106 | Martyrdom | proc | 50/100% on melee/ranged crit taken → Focused Casting 14743/27828 | **FIXED 2026-09-06** (I-21: proc flags 136 → 680, white-hits-only) |
| 18107 | Inner Focus | grant | 14751 (−100% mana + 25% crit next spell) | FAITHFUL (M-71: keeps 14751's two WotLK-only proc-permission bits — `_ref/tbc` accepted_gaps 13) |
| 18108 | Meditation | stat | aura 134 MOD_MANA_REGEN_INTERRUPT 5/10/15% | FAITHFUL |
| 18109 | Improved Inner Fire | spellmod | op 8 allEffects +10/20/30% on Inner Fire | FAITHFUL |
| 18110 | Mental Agility | spellmod | op 14 cost −2…−10% on 7 instant spells | **FLAGGED** (coverage gap) |
| 18111 | Improved Mana Burn | spellmod | op 10 castTime −250/−500ms on Mana Burn | FAITHFUL |
| 18112 | Mental Strength | stat | aura 132 MOD_INCREASE_ENERGY_PERCENT (max mana) 2–10% | FAITHFUL |
| 18113 | Divine Spirit | grant | 14752 (+17 Spirit) | FAITHFUL |
| 18114 | Force of Will | stat | aura 79 spell dmg +1…5% + aura 71 spell crit +1…5% | FAITHFUL |
| 18115 | Power Infusion | grant | **920920** custom Vanilla PI (+20% spell dmg & healing, 3min cd) | **FIXED** (+ 2026-09-06 I-20 `manaCostPct: 0`, I-30 `spell_group` 1122/1123) |

### Holy (tab 2)

| id | name | mechanic | implements | grade |
|----|------|----------|------------|-------|
| 18116 | Healing Focus | spellmod | op 9 notLoseCastingTime 35/70% on 6 heals (op 9 consumed in Spell.cpp) | FAITHFUL |
| 18117 | Improved Renew | spellmod | op 22 dot +5/10/15% on Renew | FAITHFUL |
| 18118 | Holy Specialization | stat | aura 71 MOD_SPELL_CRIT_CHANCE_SCHOOL holy 1–5% | FAITHFUL |
| 18119 | Spell Warding | stat | aura 87 MOD_DAMAGE_PERCENT_TAKEN all-magic −2…−10% | FAITHFUL |
| 18120 | Divine Fury | spellmod | op 10 castTime −100…−500ms on Smite/Holy Fire/Heal/Greater Heal | FAITHFUL |
| 18121 | Holy Nova | grant | 15237 | FAITHFUL |
| 18122 | Blessed Recovery | proc-scripted | era_blessed_recovery 8/16/25% HoT over 6s | FAITHFUL |
| 18123 | Inspiration | proc | aura 101 MOD_RESISTANCE_PCT misc 1 (armor) **+8/+16/+25%** (helpers 920987-9) | **FIXED 2026-09-06** (F-7 proc flags + I-23 payload + M-70 client rows) |
| 18124 | Holy Reach | multi spellmod | op 5 range Smite/Holy Fire + op 6 radius **Prayer of Healing only** | **FLAGGED** (missing Holy Nova radius) |
| 18125 | Improved Healing | spellmod | op 14 cost −5/10/15% on Lesser/Heal/Greater Heal | FAITHFUL |
| 18126 | Searing Light | spellmod | op 0 damage +5/10% on Smite/Holy Fire | FAITHFUL |
| 18127 | Improved Prayer of Healing | spellmod | op 14 cost −10/20% on Prayer of Healing | FAITHFUL |
| 18128 | Spirit of Redemption | grant | 20711 (correct behavior incl. +5% Spirit) | **FLAGGED** (tooltip-only) |
| 18129 | Spiritual Guidance | stat | aura 174 spell-dmg-of-Spirit + aura 175 heal-of-Spirit 5–25% | FAITHFUL |
| 18130 | Spiritual Healing | stat | aura 136 MOD_HEALING_DONE_PERCENT +2…10% | FAITHFUL |
| 18131 | Lightwell | grant | 724 (summons well); heal via 7001 Lightwell Renew | **FLAGGED** (heal magnitude) |

### Shadow (tab 3)

| id | name | mechanic | implements | grade |
|----|------|----------|------------|-------|
| 18132 | Spirit Tap | proc | 20…100% on XP kill → 15271 (+100% Spirit + regen-while-casting, 15s) | **FIXED 2026-09-06** (I-22: `attrMask: 1` REQ_EXP_OR_HONOR — was proccing off greys) |
| 18133 | Blackout | proc | 2…10% shadow-dmg → custom stun 932970 (stock 15269 absent) | **FIXED 2026-09-06** (M-69: `defenseType: 1` — the stun was unresistable — + the missing client text) |
| 18134 | Shadow Affinity | spellmod | op 2 threat −8/16/25% on SW:Pain/Mind Blast/Mind Flay | FAITHFUL |
| 18135 | Improved Shadow Word: Pain | spellmod | op 1 duration +3/6s on SW:Pain | FAITHFUL |
| 18136 | Shadow Focus | spellmod | op 16 resistMiss +2…10% on the 7-bit Shadow mask | **FIXED 2026-09-06** (I-34: aura 55 is summed bare, so the `school: 32` filter was a no-op and the talent buffed EVERY school) |
| 18137 | Improved Psychic Scream | spellmod | op 11 cooldown −2/−4s on Psychic Scream | **FLAGGED** (value confirmation) |
| 18138 | Improved Mind Blast | spellmod | op 11 cooldown −0.5…−2.5s on Mind Blast | FAITHFUL |
| 18139 | Mind Flay | grant | 15407 (channel + 50% slow + damage) | FAITHFUL |
| 18140 | Improved Fade | spellmod | op 11 cooldown −3/−6s on Fade | FAITHFUL |
| 18141 | Shadow Reach | spellmod | op 5 range +6/13/20% on shadow spells | FAITHFUL |
| 18142 | Shadow Weaving | proc | 20…100% shadow-dmg → 932960 Shadow Vulnerability (3%×5) | FAITHFUL |
| 18143 | Silence | grant | 15487 | FAITHFUL |
| 18144 | Vampiric Embrace | grant | **921152** custom debuff-anchored castable + C++ heal | **FIXED** |
| 18145 | Improved Vampiric Embrace | stat (dummy) | aura 4 DUMMY marker (misc 4) feeding era_vampiric_embrace, +5/10% | **FIXED** |
| 18146 | Darkness | stat | aura 79 MOD_DAMAGE_PERCENT_DONE shadow +2…10% | FAITHFUL |
| 18147 | Shadowform | grant | 15473 (+15% shadow / −15% phys taken) | FAITHFUL |

All stat-aura indices were confirmed against
`azerothcore-wotlk/src/server/game/Spells/Auras/SpellAuraDefines.h`; all SPELLMOD op numbers
against `.../Spells/SpellDefines.h`. Grant behaviors were dumped from `Spell.dbc` (established
FAITHFUL list) — no divergence found in any authored SPELLMOD/stat/proc node beyond the flagged
items.

---

## FLAGGED-item investigations (8 candidates)

### 1. Inspiration (18123) — armor-proxy → ~~RESOLVED FAITHFUL, keep as-is~~ → **OVERTURNED 2026-09-06 (I-23): re-authored as real armor%**

> **CORRECTION, priest final-review round R5 (2026-09-06).** The clearance below is **wrong, and the
> reason it is wrong is the arbiter it used.** It dumped **live 15359** — a **WotLK** row — and read
> the WotLK *retune* (aura 87, −3/−7/−10% physical damage taken) as if it were the era record. The
> era record for 14893/15357/15359 is **aura 101 `MOD_RESISTANCE_PCT`, MiscValue 1 (ARMOR),
> +8/+16/+25** — see `era-data/_ref/tbc/priest-spells.yaml` `reuse_ledger` entry
> `"920987_920988_920989"` and `node_findings[20529]`, and the TBC clones 948057-948059, which have
> shipped that encoding since Phase 6. Aura 101 **is** implemented on this core
> (`AuraEffect::HandleModResistancePercent`, `SpellAuraEffects.cpp:4157`), so the "proxy" was never
> necessary. It also *over*-delivered by a wide margin: a flat −25% physical damage taken is far
> stronger than +25% armor, which mitigates nonlinearly — the exact inverse of the "it would
> *under*-perform" argument below, which is only true if the tooltip's 25% is read as a mitigation
> percentage rather than as an armor percentage.
>
> **Shipped fix:** helpers 920987/920988/920989 re-authored to
> `{ effect: 6, aura: 101, basePoints: 7/15/24, dieSides: 1, misc: 1, targetA: 21 }` (val−1/ds1, so
> the rolled value is exactly 8/16/25), plus the `client:` block they never had (icon 1463 = stock
> 14893's `SpellIconID`, per-rank `description`/`auraDescription` "Increases the target's armor by
> N%."). Grade moves from FAITHFUL (settled) to **FIXED**. See also **F-7** below — the node's proc
> could not fire at all, so this payload had never been observed in play.

*Original 2026-08-14 investigation, retained for the record:*

- **Current impl:** helpers 920987/8/9 use aura **87 MOD_DAMAGE_PERCENT_TAKEN**, MiscValue 1
  (physical school mask), amount −8 / −16 / −25%, 15s, triggered on crit of
  Flash Heal/Heal/Greater Heal/Prayer of Healing.
- **Live check:** dumped the stock Inspiration data-spell **15359** —
  `Effect_1=6 (APPLY_AURA) EffectAura_1=87 EffectMiscValue_1=1`. The stock spell uses the **exact
  same aura** (MOD_DAMAGE_PERCENT_TAKEN, physical), *not* real armor% (aura 142
  MOD_BASE_RESISTANCE_PCT). So the "proxy" is in fact Blizzard's own implementation of Inspiration.
  — **WRONG ARBITER: that is the live WotLK row, not the era record. See the correction above.**
- **Divergence:** none in mechanic. Values −8/−16/−25% match the Vanilla tooltip's "8/16/25%".
- **If it were real armor% (aura 142):** it would *under*-perform badly — +25% armor yields far
  less than 25% physical mitigation (armor mitigation is nonlinear), and it would contradict both
  the authored tooltip numbers and the stock spell.
- **Recommendation:** **no change.** Keep aura 87. **Confidence: high** (stock spell dumped).
  — **OVERTURNED, see above.**

### 1b. 2026-09-06 final-review round R5 — the rest of the priest items

Recorded here because this file holds the Vanilla per-node table; the TBC-side items live in
`era-talents-tbc-phase6-priest.md`.

| Item | Node / id | Defect | Fix |
|---|---|---|---|
| **F-7** | 18123 Inspiration | **CRITICAL — the talent could never fire.** `proc.flags` was `1024` = `PROC_FLAG_DONE_SPELL_NONE_DMG_CLASS_POS`; all four named heals are `SPELL_DAMAGE_CLASS_MAGIC`, so no cast ever raised that flag | `flags: 16384` (`DONE_SPELL_MAGIC_DMG_CLASS_POS`, a member of the TBC twin 20529's own `87376`) **plus** `typeMask: 2` (`SPELL_TYPE_MASK_HEAL`) — the generator defaults `SpellTypeMask` to 1/DAMAGE and a heal event carries 2, so `2 & 1 == 0` would still have rejected every proc |
| **I-20** | 920920 Power Infusion | The clone inherited stock 10060's `ManaCostPct 16` on top of the authored flat 182, and the core charges **both** | `manaCostPct: 0` |
| **I-21** | 18106 Martyrdom | `proc.flags` was `136` (`0x88`) = **white hits only**; a caster in a mob's ability rotation never got Focused Casting | `flags: 680` (`0x2A8`, adding `TAKEN_SPELL_MELEE_DMG_CLASS` + `TAKEN_SPELL_RANGED_DMG_CLASS`) — what stock 14531/14774, sibling node 18122 and TBC twin 20505 all carry |
| **I-22** | 18132 Spirit Tap | No `attrMask`, so `PROC_FLAG_KILL` fired on grey mobs — the tooltip says "yields experience" | `attrMask: 1` (`PROC_ATTR_REQ_EXP_OR_HONOR`), matching TBC twin 20543 |
| **I-34** | 18136 Shadow Focus | Shipped as `stat` **aura 55** (`MOD_SPELL_HIT_CHANCE`) + `school: 32`. **The school was a no-op filter** — `StatSystem.cpp` sums aura 55 bare, with no school or family test, so the talent granted +2…10% hit with **every** school (Holy Fire, Smite, Mana Burn included) | re-authored as `mechanic: spellmod`, `op: resistMiss` (`SPELLMOD_RESIST_MISS_CHANCE`, misc 16), `kind: flat`, vals 2/4/6/8/10 — the era-authentic encoding and the shape TBC twin 20547 already ships. Mask `{a: 0x203A004, b: 0, c: 0x40}`, every bit censused against family 6 in `ip-dbc/Spell.dbc`: SW:Pain, Mind Blast, Psychic Scream, Mind Flay and Devouring Plague own their bits outright; Vampiric Embrace's bit2 and Mind Control's bit17 are additionally carried only by AV/world-event buffs and NPC mind-control spells that no priest can cast. **Nothing Holy or Discipline intersects** |
| **M-69** | 932970 Blackout stun | `DefenseType 0` (unresistable — `Unit::SpellHitResult` returns `SPELL_MISS_NONE`) and no client text, so the stun icon had a blank tooltip | `defenseType: 1` + `client.description`/`auraDescription` "Stunned." Closes `_ref/tbc` accepted_gaps id 12 |
| **M-70** | 920987-920989 | No `client:` block at all — no client Spell.dbc row, so the buff rendered untooltipped | icon 1463 + per-rank text (see the I-23 correction above) |
| **M-71** | 18107 Inner Focus | *Not a defect — recorded disposition.* The granted stock 14751 carries **WotLK-only** `ATTR3 CAN_PROC_FROM_PROCS` (`0x04000000`) and `ATTR4 ALLOW_PROC_WHILE_SITTING` (`0x00080000`); Vanilla had neither. Both only widen when the buff may be consumed | **KEEP** — the same disposition the TBC twin 20507 takes. Recorded as `_ref/tbc` accepted_gaps **id 13** |
| **I-30** | 920920 / 948039 / 920987 / 948057 / 948040 | `spell_group` and `spell_linked_spell` membership is by spell **id**, so band clones inherit none of it: both Power Infusions could stack with Bloodlust/Heroism and with a mage's Arcane Power, Inspiration r1 could stack with Ancestral Fortitude, and the TBC Pain Suppression clone lost its −5% threat link | hand SQL `modules/mod-era-talents/data/sql/world/base/2026_09_06_03_era_talent_final_review_priest.sql` — PI clones → groups 1122 + 1123, Inspiration **r1** clones → group 1095 (stock groups r1 only; mirrored per rank), and `(948040, 44416, 2)` mirroring the stock `(33206, 44416, 2)` row |

### 2. Improved Power Word: Fortitude (18104) — Prayer of Fortitude reach → **RESOLVED FAITHFUL, no change**

- **Current impl:** SPELLMOD op 8 (allEffects) `affects: [Power Word: Fortitude]`, which resolves
  to PW:Fortitude's family mask `flagsA=8` (from `_ref`).
- **Live check:** `dump_spell_family` — **Prayer of Fortitude** rank-1 = **21562**,
  `family=6 mask=(8,0,1024)`; **PW:Fortitude** 1243 = `mask=(8,0,1024)`. They **share flagsA bit 8**.
  A SPELLMOD whose class mask is word1 bit 8 therefore matches **both** spells.
- **Divergence:** none — the current mask already reaches Prayer of Fortitude, satisfying the
  tooltip ("…and Prayer of Fortitude spells by 15/30%"). Bit 8 is unique to the two Fortitude
  spells in the priest family, so there is no over-match risk.
- **Recommendation:** **no change.** Prayer of Fortitude does **not** need to be added to `affects`
  or `_ref` for this node to work. **Confidence: high** (masks dumped).

### 3. Holy Reach (18124) — Holy Nova radius missing → **FLAGGED, recommend add**

- **Current impl:** two SPELLMODs — op 5 range on Smite/Holy Fire, and op 6 radius on
  **Prayer of Healing only**.
- **Authentic 1.12 / tooltip:** the node's own tooltip says it boosts "the radius of your
  **Prayer of Healing and Holy Nova** spells by 10/20%." Holy Nova radius is currently **not**
  modified.
- **Live check:** `dump_spell_family` — Holy Nova 15237 = `family=6 mask=(4194304,0,0)`
  (flagsA = 4194304). Prayer of Healing's flagsA is 512, so the current mask does not include it.
- **Recommendation:** add **Holy Nova (15237)** to the op 6 radius `affects` list. Holy Nova is
  not yet in `era-data/_ref/priest-spells.yaml` — add
  `"Holy Nova": { id: 15237, flagsA: 4194304, flagsB: 0, flagsC: 0 }` and reference it.
  **Confidence: high** (tooltip explicit + mask dumped).

### 4. Improved Psychic Scream (18137) — cooldown value → **FLAGGED, await user confirmation**

- **Current impl:** op 11 cooldown −2000 / −4000 ms (2 ranks). Matches the node's authored
  Daribon-1.12 tooltip ("−2 sec / −4 sec").
- **Uncertainty:** this is a pure talent-magnitude question that cannot be settled from `Spell.dbc`
  or `acore_world` (a talent-side SPELLMOD amount, not a stored spell field). Some community 1.12
  references list Improved Psychic Scream as **−1 sec / −2 sec** per the two ranks instead.
- **Recommendation:** retain −2/−4s (implementation already equals its stated Daribon source), but
  **await user confirmation** of the authentic 1.12 value vs the −1/−2s alternative before treating
  it as final. **Confidence: low** (source conflict; no live-data arbiter).

### 5. Lightwell (18131) — heal magnitude → **FLAGGED**

- **Current impl:** grants stock **724**, which summons the Lightwell (Effect_1 = 28 SUMMON,
  MiscValue 31897; DurationIndex 25 = 3 min — matches "lasts for 3 min"). The clickable heal is the
  Lightwell Renew chain, rank 1 = **7001**.
- **Live dump (7001):** `EffectAura_1 = 8 (PERIODIC_HEAL)`, `EffectBasePoints_1 = 266`,
  `EffectDieSides_1 = 1` → **267 HP per tick**; `EffectAuraPeriod_1 = 2000` (every 2s);
  `DurationIndex = 32`.
- **Duration index 32:** no `SpellDuration.dbc` is shipped locally, so index 32 was triangulated
  from spells whose duration is known (Renew 139 @ idx 8 = 15s; Mind Flay 15407 @ idx 27 = 3s;
  Silence 15487 @ idx 28 = 5s; Lightwell summon 724 @ idx 25 = 3 min). Index 32 is a 854-spell
  bucket that includes **First Aid rank 1 (746)**, a bandage — WotLK bandages channel **8 s** —
  so index 32 ≈ **8000 ms**. That gives WotLK 7001 = 4 ticks × 267 ≈ **1068 HP over 8 s**.
- **Divergence:** authentic Vanilla 1.12 Lightwell (rank 1) restores **800 HP over 10 s**
  (5 ticks × 160 HP every 2s) — which is exactly what the node's own tooltip states. The granted
  WotLK spell delivers a **larger per-tick heal (267 vs 160)** over a **shorter window (8s vs 10s)**,
  i.e. it heals more and faster than the tooltip promises.
- **Recommendation:** for full fidelity, ship a **custom era Lightwell Renew** (or override
  724→7001) tuned to **160 HP/tick × 2s × 10s = 800 HP**, matching the tooltip. Alternatively (lower
  effort) correct the tooltip to the WotLK value. **Confidence: high** on the WotLK dump; **medium**
  on the exact index-32 = 8000 ms (First-Aid-anchored, not read from SpellDuration.dbc) — the
  per-tick/period divergence holds regardless of the exact window.

### 6. Mental Agility (18110) — instant-spell coverage gap → **FLAGGED, recommend additions**

- **Current impl:** op 14 cost −2…−10% with `affects:` = Power Word: Shield, Renew,
  Shadow Word: Pain, Fade, Psychic Scream, Inner Fire, Power Word: Fortitude (7 spells; and via the
  shared flagsA-8 mask, Prayer of Fortitude is auto-covered too — see item 2).
- **Authentic 1.12:** Vanilla Mental Agility reduced the cost of **all instant-cast** priest spells.
  Missing from the current list: **Devouring Plague, Touch of Weakness, Fear Ward, Desperate Prayer,
  Abolish Disease, Shadowguard** (and Shadow Protection / Prayer of Shadow Protection, both instant).
- **Live check (masks, rank-1 where resolvable):** Devouring Plague 2944 `mask=(33554432,4096,1024)`;
  Fear Ward 6346 `mask=(-2147483648,33558528,1024)`; Abolish Disease 552 `mask=(0,1,1024)`;
  Shadowguard 38379 `family=0 mask=(0,0,0)` — **not in the priest family**, so it cannot be reached
  by a priest-family class-mask SPELLMOD and would need a different mechanism if fidelity demands it.
  Devouring Plague / Touch of Weakness rank-1 ids were not all present under the expected ids on this
  server (some resolved only at rank 2+), so exact `_ref` entries must be pinned when implementing.
- **Recommendation:** add the era-relevant instant spells (Devouring Plague, Fear Ward,
  Desperate Prayer, Abolish Disease, Touch of Weakness) to `affects`, adding matching `_ref` entries
  first. Note that several are **race-gated** racials (Fear Ward, Desperate Prayer, Touch of Weakness,
  Devouring Plague, Shadowguard) and Shadowguard is family-0 (untargetable this way). **Confidence:
  medium** — the exact authentic instant-spell set varies by source; the core instant spells are
  already covered.

### 7. Silent Resolve (18103) — op-28 dispel-resist working? → **RESOLVED FAITHFUL, op 28 is LIVE**

- **Current impl:** two SPELLMODs on 18 priest spells — op 2 threat −4…−20% and op **28**
  resistDispel +10…+50%.
- **Codebase check:** `SPELLMOD_RESIST_DISPEL_CHANCE = 28` (`SpellDefines.h:104`) is **consumed**
  in `Aura::CalcDispelChance` (`Spells/Auras/SpellAuras.cpp:1142`):
  ```
  if (Unit* caster = GetCaster())
      if (Player* modOwner = caster->GetSpellModOwner())
          modOwner->ApplySpellMod(GetId(), SPELLMOD_RESIST_DISPEL_CHANCE, resistChance);
  ```
  When any of the priest's buffs is targeted for dispel, the core keys the mod on that buff's
  spell id; if it matches Silent Resolve's class mask (Renew, PW:Shield, PW:Fortitude, Inner Fire,
  etc. are all in `affects`), `resistChance` rises and the dispel is resisted.
- **Divergence:** none — the op is **not inert**; it reduces the dispel chance of the priest's own
  buffs exactly as the tooltip says. (Offensive spells in the same `affects` list simply never
  produce a dispellable self-buff, so their inclusion is harmless.)
- **Recommendation:** **no change.** Grade FAITHFUL. **Confidence: high** (core code path traced).

### 8. Spirit of Redemption (18128) — tooltip omits +5% Spirit → **FLAGGED, tooltip-only fix**

- **Current impl:** grants stock **20711**. `dump_spell_effects --ids 20711` shows a second effect
  `Effect_2 = 6 (APPLY_AURA) EffectAura_2 = 137 (MOD_TOTAL_STAT_PERCENTAGE) EffectMiscValue_2 = 4
  (Spirit) EffectBasePoints_2 = 4, DieSides 1` → a permanent **+5% Spirit** passive, in addition to
  the on-death Spirit form.
- **Divergence:** behavior is correct; the node's **tooltip omits the +5% Spirit** line.
- **Recommendation:** append to the tooltip, e.g. "*Also increases your total Spirit by 5%.*"
  **Text-only — no data change.** **Confidence: high** (effect dumped).

---

## Open points carried from the design (for the user)

These are magnitude/behavior questions the VE/PI rework deliberately left for the user — recorded
here so they aren't lost, not resolved by this audit:

- **(a) Power Infusion mana-cost reduction.** Vanilla PI is modeled as +20% spell damage & healing
  (920920). Some accounts of Vanilla PI also had it **reduce the target's mana cost** for the
  duration. This is **not currently modeled**. Confirm whether authentic 1.12 PI included a mana-cost
  reduction; if so, PI needs a cost SPELLMOD component added.
- **(b) Vampiric Embrace heal distribution.** The rework heals a **flat 20% of shadow damage to the
  priest's subgroup (including the priest)**. There is an open question whether authentic Vanilla VE
  healed a flat 20% to **every** party member, or used a **self vs party split** (e.g. a larger self
  portion + smaller party portion). Confirm the authentic distribution before treating the flat-20%
  model as final.

## What was checked with live data (quick index)

- **Settled by live data:** Silent Resolve op-28 (core `CalcDispelChance` — live/working);
  Spirit of Redemption +5% Spirit (20711 dump); Improved PW:Fortitude reach (Prayer of Fortitude
  21562 shares flagsA 8); ~~Inspiration proxy is authentic (stock 15359 uses aura 87)~~ **— OVERTURNED
  2026-09-06: 15359 is the LIVE WotLK row, not the era record; see the correction in §1**; Lightwell
  numbers (7001 = 267/tick @2s vs Vanilla 160/tick @2s over 10s); Holy Nova radius mask (15237
  flagsA 4194304); all 47 stat-aura/SPELLMOD-op indices verified against core headers.
- **Await user confirmation (no live arbiter):** Improved Psychic Scream −2/−4s vs −1/−2s;
  exact index-32 duration (First-Aid-anchored ≈ 8s); Mental Agility's exact authentic instant-spell
  set; PI mana-cost (open point a); VE heal distribution (open point b).

## 2026-08-14 live verification — VE + PI CONFIRMED WORKING (real 3.3.5a client)

Verified by the user on the dev realm (Priestest, level 60, era=Vanilla, generation 9bca554c
end-to-end: DB `era_talent_meta` == client sentinel 932999, addon canary green):

- **Vampiric Embrace (18144 → 921152 + 932950 + 932951):** cast on a mob applies the debuff with
  era tooltip text; Shadow damage to that mob heals the priest (solo = self). Confirmed after the
  two root-cause fixes below. Also re-confirmed after a full in-session reset → re-learn cycle.
- **Power Infusion (18115 → 920920):** buff applies with vanilla aura text ("Spell damage and
  healing increased by 20%."); user confirms damage behavior working.

### The two defects this pass found and fixed (beyond the earlier HitMask/AuraDescription fixes)

1. **`spell_proc.HitMask = 8` (PROC_HIT_FULL_RESIST)** on the VE proc-carrier 932950 — the
   generator's old default when a YAML `proc:` block omitted `hitMask`. The proc engine requires
   `eventHitMask & procEntry.HitMask`; a normal hit is 1, so the heal could literally never proc.
   Fixed: generator default is now 0 (core default), VE's YAML states `hitMask: 0` explicitly,
   and an explicitly-authored unusable mask is now a hard generator error.
2. **Reconcile-on-learn gap:** 932950 is granted by `ReconcileBaselineSpells`, which only ran at
   login/level-up/era-transition — learning the VE talent mid-session left the priest without the
   proc-carrier until relog (live-confirmed: 18144 recorded + 921152 known + 932950 absent).
   Fixed in commit a4f83a3: `TryLearn` and `Reset` now reconcile immediately. Regression-tested
   live: reset → re-learn VE mid-session → heal works with NO relog. Rule recorded in
   `docs/era-talents-framework.md`: never rely on relog-to-apply for reconcile-managed spells.

The client-side "buff shows the WotLK spell" symptom was `AuraDescription_Lang_enUS` leaking from
the template clone (buff tooltips render AuraDescription, not Description) — fixed pipeline-wide
with `client.auraDescription` + a leak lint; see the framework doc's field-semantics table.
