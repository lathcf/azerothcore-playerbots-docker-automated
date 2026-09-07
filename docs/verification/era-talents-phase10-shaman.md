# Era Talents Phase 10 — Vanilla Shaman: Verification Record

> **Generation superseded (2026-09-06 final review):** the current stamp is the closing row of docs/verification/era-talents-final-review-tbc-vanilla.md §10 — reinstall patch-V.mpq + restage the addon before any real-client pass.

**Date:** 2026-08-23
**Branch:** `feat/era-talents` (merged to master 2026-09-07)
**Generation stamp:** `d32d055c`
**Status: HEADLESS PASS COMPLETE.** Full Vanilla Shaman talent tree (46 nodes, ids `18800`–`18845`)
across Elemental / Enhancement / Restoration, class 7, spell family 11, on the existing
`mod-era-talents` pipeline. **Zero core patches, zero hand-written C++ scripts, zero hand-SQL this
entire phase** — every node resolved to grant / spellmod / stat / multi / proc / clone, all
generator-authored data. This record is **headless verification only** (Task 9). Runtime behaviors
that require a live 3.3.5a client (canary, panel render, live proc/aura firing, form/imbue-in-combat
magnitudes, tooltips) are deferred to the real-client pass (Task 10, listed at the end).

## Scope delivered

- **46 nodes**, ids `18800`–`18845`, tabs Elemental(15) / Enhancement(16) / Restoration(15). Class 7,
  spell family 11 — the mirror-inversion of Druid's class 11 / family 7. Hand-authored helper band
  `932400`–`932415` (16 ids used, `932416`–`932499` free). No auto-passive band was needed (every
  Shaman node authored directly, no `helpers:`-adjacent per-rank auto-clones the way Druid's
  `925600`-range worked) — the 16 helper ids above are the entirety of this phase's custom-spell
  footprint.

## Per-tab node table

### Elemental (15 nodes, `18800`–`18814`)

| Id | Name | Disposition / mechanic | Evidence |
|---|---|---|---|
| 18800 | Convection | spellmod op:cost, -2/4/6/8/10% on LB/CL/3 shocks | authored fresh (WotLK 16039 diverges) |
| 18801 | Concussion | spellmod op:allEffects, +1/2/3/4/5% dmg on same 5-spell set | authored (WotLK 16035 splits damage/dot) |
| 18802 | Earth's Grasp | multi: allEffects +25/50% Stoneclaw health (bit3) + radius +10/20% Earthbind slow (B bit0) | authored, stock 16043 proves both bindings |
| 18803 | Elemental Warding | stat, aura 87 school-mask 28 (Fire/Frost/Nature), -4/7/10% dmg taken | authored (WotLK 28996 all-school, diff values) |
| 18804 | Call of Flame | spellmod op:allEffects on fire-totem DAMAGE spells, affectsMask {a:1073741824,b:262144}, +5/10/15% | authored, code/DBC-verified bit30/B-bit18 binding |
| 18805 | Elemental Focus | proc → Clearcasting clone 932415, flags 65536, school 28, hitMask 0, chance 10 | rebuilt (WotLK reworked to on-crit) |
| 18806 | Reverberation | spellmod op:cooldown, -200..-1000ms on 3 shocks | Vanilla-exact vs WotLK 16040 |
| 18807 | Call of Thunder | spellmod op:crit, +1/2/3/4/6% on LB/CL | authored (WotLK collapsed to 1 rank) |
| 18808 | Improved Fire Totems | spellmod op:threat affectsMask{a:4}, -25/50% Magma threat | authored; Fire Nova delay half is an ACCEPTED GAP (see appendix) |
| 18809 | Eye of the Storm | spellmod op:notLoseCastingTime, 33/66/100% on elemental set | DIVERGED from proc-on-crit (see appendix) |
| 18810 | Elemental Devastation | grant, stock chain 30160/29179/29180 | self-contained/ungated, exact Vanilla values |
| 18811 | Storm Reach | spellmod op:range, +3/6yd on LB/CL | authored (no surviving stock spell) |
| 18812 | Elemental Fury | grant, stock 60188 (WotLK rank-5 id = exact Vanilla +100%) | self-contained/ungated |
| 18813 | Lightning Mastery | spellmod op:castTime, -200..-1000ms on LB/CL | Vanilla-exact vs WotLK 16578 |
| 18814 | Elemental Mastery | grant clone 932413 (ONE buff: FLAT crit + free-cast) | reconstruction, see below |

### Enhancement (16 nodes, `18815`–`18830`)

| Id | Name | Disposition / mechanic | Evidence |
|---|---|---|---|
| 18815 | Ancestral Knowledge | stat, aura 132 misc 0 (POWER_MANA), +1..5% max mana | authored (WotLK reworked to +Int%) |
| 18816 | Shield Specialization | multi: stat aura 51 (block%) + stat aura 150 (block value%) | authored, both auras stock-proven |
| 18817 | Guardian Totems | multi: allEffects +10/20% Stoneskin (bit15) + cooldown -1/2s Grounding (affectsMask{a:262144}) | Windwall half ABSENT — ACCEPTED GAP |
| 18818 | Thundering Strikes | stat, aura 52, +1..5% weapon crit | Vanilla-exact half of WotLK 16255 |
| 18819 | Improved Ghost Wolf | spellmod op:castTime, -1/-2s | Vanilla-exact vs WotLK 16262 |
| 18820 | Improved Lightning Shield | spellmod op:damage on Lightning Shield (bit10), +5/10/15% | mirrors stock Improved Shields 16261 mechanism |
| 18821 | Enhancing Totems | spellmod op:allEffects on Strength of Earth (bit16), +8/15% | Grace of Air half ABSENT — ACCEPTED GAP |
| 18822 | Two-Handed Axes and Maces | grant clone 932406 (proficiency) | reconstruction, see below |
| 18823 | Anticipation | stat, aura 49 (dodge, NOT defense-skill), +1..5% | tooltip-verified, Vanilla-exact half of WotLK 16254 |
| 18824 | Flurry | proc (flags 20, hitMask 2 crit-only) → clones 932400-404 | reconstruction, see below |
| 18825 | Toughness | stat, aura 142 misc 1 (armor from items), +2..10% | identical pattern to Warrior Toughness 18539 |
| 18826 | Improved Weapon Totems | multi: allEffects +15/30% WF pulse (B bit21) + +6/12% FT pulse (A bit25) | authored, unique-bit totem-pulse binding |
| 18827 | Elemental Weapons | multi: allEffects +13/27/40% Windfury Attack (bit23) + +5/10/15% FT/FB Attack (bits21/24) | REDESIGNED 2026-08-23; Rockbiter half is a CODE-VERIFIED INERT gap |
| 18828 | Parry | grant stock 3127 (enables parry mechanic) | verified: no shaman trainer teaches it, real gate |
| 18829 | Weapon Mastery | stat, aura 79 misc 1 (physical), +2..10% weapon dmg | authored (WotLK dropped the shaman talent) |
| 18830 | Stormstrike | grant clone 932405 | reconstruction, see below |

### Restoration (15 nodes, `18831`–`18845`)

| Id | Name | Disposition / mechanic | Evidence |
|---|---|---|---|
| 18831 | Improved Healing Wave | spellmod op:castTime, -100..-500ms on Healing Wave | Vanilla-exact vs stock chain 16182/16226-9 |
| 18832 | Tidal Focus | spellmod op:cost, -1..-5% on HW/LHW/CH (mask 448) | Vanilla-exact vs stock chain 16179/16214-7 |
| 18833 | Improved Reincarnation | spellmod op:cooldown, -10/-20min on Reincarnation | cd half authored; HP/mana-return half ACCEPTED GAP |
| 18834 | Ancestral Healing | proc (flags 16384, typeMask 2, hitMask 2) → clones 932408-410 | reconstruction, see below |
| 18835 | Totemic Focus | spellmod op:cost on all totems (mask 537399320), -5..-25% | Vanilla-exact vs stock chain 16173/16222-5 |
| 18836 | Nature's Guidance | multi: stat aura 54 (melee hit) + stat aura 55 (spell hit), +1..3% | authored (no surviving stock spell) |
| 18837 | Healing Focus | spellmod op:notLoseCastingTime on HW/LHW/CH, 14..70% | authored fresh (WotLK retuned to 3 ranks) |
| 18838 | Totemic Mastery | spellmod op:radius on all totems, +10yd | pulse-vs-summon radius is a Task-10 runtime-verify item |
| 18839 | Healing Grace | spellmod op:threat on HW/LHW/CH, -5/10/15% | Vanilla-exact threat half vs stock 29187/89/91 |
| 18840 | Restorative Totems | spellmod op:allEffects affectsMask{a:24576} on Mana Spring/Healing Stream pulses, +5..25% | authored (5 Vanilla ranks vs WotLK's 3) |
| 18841 | Tidal Mastery | spellmod op:crit on 6-spell set (mask 1475), +1..5% | Vanilla-exact vs stock chain 16194/16218-21 |
| 18842 | Healing Way | proc (flags 16384, typeMask 2, affects Healing Wave, hitMask 0) → stacking buff 932411 | reconstruction, see below |
| 18843 | Nature's Swiftness | grant clone 932412 | reconstruction, see below |
| 18844 | Purification | stat, aura 136 misc 127 (all schools), +2..10% healing done | Vanilla-exact vs stock chain 16178/16210-3 |
| 18845 | Mana Tide Totem | grant clone 932407 | reconstruction, see below |

All 46 nodes confirmed via the live DB (`era_talent` / `era_talent_rank` join, Step 2) to carry a
non-zero `grantedSpellId` at every rank — **zero `display_only` leftovers**.

## Reconstruction list (helper clones, band `932400`–`932415`)

| Helper id(s) | Name | Node | Template | Shape |
|---|---|---|---|---|
| 932400-404 | Flurry haste buffs r1-5 | 18824 | 16257 | aura 138 MOD_MELEE_HASTE +10/15/20/25/30%, inherits ProcCharges 3 / ProcTypeMask 4 (consume-on-swing) verbatim; node's data proc is crit-only (hitMask 2) |
| 932405 | Stormstrike clone | 18830 | 17364 | override RecoveryTime 20000 (Vanilla 20s cd) + flat ManaCost 319 + ManaCostPct 0; aura-271 nature scope (A=1049603/A_2=8192) + weapon-strike triggers 32175/32176 INHERITED (this helper authors no per-effect maskA/B/C, so the templated masks pass through) |
| 932406 | Two-Handed Axes and Maces | 18822 | 197 | hidden passive, forced Effect_1 = SPELL_EFFECT_PROFICIENCY (60), `equipClass:2` (weapon) / `equipSubclass:34` (axe2\|mace2) — new generator `_HELPER_SCALARS` |
| 932407 | Mana Tide Totem clone | 18845 | 16190 | byte-identical Effect_1 SUMMON (misc 10467, bp4=5hp, miscB 82 SummonProperties); id≠16190 dodges the hardcoded 10%-caster-HP override (`SpellEffects.cpp:2433`); summons the STOCK NPC 10467 so the pulse stays WotLK's %-max-mana `spell_sha_mana_tide_totem` script — ACCEPTED BENEFICIAL DIVERGENCE (flat-170 pulse not built, see appendix) |
| 932408-410 | Ancestral Healing armor buffs r1-3 | 18834 | from-scratch | aura 142 MOD_BASE_RESISTANCE_PCT misc 1, +8/16/25% of the HEAL TARGET's item armor, 15s, targetA 21 (the paladin LoH-rider precedent) |
| 932411 | Healing Way stacking buff | 18842 | 29206 (mask-inherit only) | aura 283 MOD_HEALING_RECEIVED +6%, `stackAmount:3`→CumulativeAura 3, `attributes:0` un-hides the passive, targetA 21, 15s; inherits Effect_1 EffectSpellClassMaskA_1=64 (Healing Wave scope) from the template rather than authoring maskA/B/C |
| 932412 | Nature's Swiftness clone | 18843 | 16188 | inherits the nature-set effect mask (2499) + instant + DurationIndex 21; category/categoryCooldown NEUTRALIZED to 0 + RecoveryTime 180000 = Vanilla 3-min cd (WotLK's 2-min lived on a shared Category) |
| 932413 | Elemental Mastery (single buff) | 18814 | 16166 | ONE self-buff carrying BOTH effects via per-effect masks: Effect_1 FLAT crit (aura 107 misc 7 bp99 dieSides1 = +100 guaranteed, Divine Favor 20216 / Cold Blood 14177 encoding) + Effect_2 PCT cost (aura 108 misc 14 SPELLMOD_COST -100%), each authored `maskA -1877999613 / maskB 0 / maskC 0` = the fire/frost/nature damage set {0,1,20,28,31} (Effect_1 flag96 → A_1/A_2/A_3, Effect_2 flag96 → B_1/B_2/B_3); category/categoryCooldown neutralized + RecoveryTime 180000; ProcCharges 1 (one shared charge). Collapsed 2026-08-23 from the old 932413 crit + 932414 cost split (two visible buffs); 932414 retired and freed |
| 932415 | Elemental Focus "Clearcasting" buff | 18805 | 16039 Convection | aura 108 misc 14 SPELLMOD_COST -100%, inherits Convection's damage-only class mask {0,1,20,28,31} (NO heal bits); `attributes:327680` (visible buff, overriding 16039's hidden-passive attrs); 15s; 1 charge — the shaman Omen-of-Clarity analog; node 18805's data proc fires it (chance 10, hitMask 0) |

**EM single-buff + root-cause fix (2026-08-23, fix-round after the initial headless pass):** the
live bug was that 932413's crit effect used `aura 108` (ADD_PCT_MODIFIER) for
SPELLMOD_CRITICAL_CHANCE — `Player::ApplySpellMod` deliberately SKIPS a PCT crit mod unless the
same aura already applied another mod to the spell (the "Surge of Light" exception), so the crit
never applied and its charge only counted down. Fixed to a FLAT crit (aura 107, bp99 + dieSides1 =
+100), matching the stock FLAT-crit precedents Divine Favor 20216 / Cold Blood 14177. EM was ALSO
split into two helpers (932413 crit + 932414 cost) = two visible buffs; Vanilla EM is ONE buff.
Collapsed to a single 932413 once the generator gained **per-effect helper masks**
(`maskA`/`maskB`/`maskC` on an effect entry = family words 0/1/2, written under that effect's letter
via `MASK_PREFIX`): both effects author the same fire/frost/nature damage-set mask on one spell, so
932414 was deleted and its id freed. Verified by decoding the generated row against the flag96[3]
layout — Effect_1 flag96 = {-1877999613, 0, 0} (cols A_1/A_2/A_3) and Effect_2 flag96 =
{-1877999613, 0, 0} (cols B_1/B_2/B_3), the cost mask NON-zero and equal to the damage set (an
all-zero effect flag96 would make `SpellInfo::IsAffected` treat it as "affects the whole family",
reintroducing the heal-leak).

**Heal-leak fix (2026-08-23, earlier this phase's history):** 932415 (and, before the collapse, the
separate cost buffs) originally templated WotLK Clearcasting `16246`, whose Effect_1 class mask A
`-1743781437` includes bits {0,1,6,7,8,20,27,28,31} — bits 6/7/8 are Healing Wave/Lesser Healing
Wave/Chain Heal, so the -100% cost leaked onto heals (`SpellInfo::IsAffected` matches on any
nonzero-word intersection). 932415 re-templated to `16039` (Convection, clean 5-spell elemental
DAMAGE set); 932413 now authors its OWN per-effect cost mask explicitly (no donor-inheritance
dependency). Step 1's clean regen + 0-finding audit reconfirms it is stable.

**Elemental Weapons redesign (2026-08-23, also pre-existing this phase's history):** the original
18827 binding used the imbue-*passive* identity bits (Rockbiter 8017 bit22, FT/FB shared word-B
bit11) and was traced against the runtime call graph
(`Player::UpdateDamageDoneMods`/`Player::CastItemCombatSpell`, `spell_sha_flametongue_weapon`,
`spell_sha_windfury_weapon`, `SpellEffectInfo::CalcValue`) to be a mis-bind on all three imbues —
Rockbiter's AP is a flat `SpellItemEnchantment.dbc` value never routed through `CalcValue` at all
(code-verified inert), and the old FT/FB bit11 binding was leaking onto Windfury Weapon's own
bit11-carrying `CalcValue(player)` call (measured stacking: 18/37/55% total instead of the intended
separate 5/10/15 and 13/27/40). The current shipped shape binds Windfury Attack's own unique bit23
(unchanged) and Flametongue Attack (10444, bit21) / Frostbrand Attack (8034, bit24) — the actual
damage-dealing spells, each on a bit unique among live family-11 spells. This redesign predates this
verification task but its correctness is reconfirmed by the DB integrity check (Step 2 below).

## Step 1 — full regen + audit + tests + 9-class clean boot

```
tools/era-regen.sh --sync-fork
```
→ **stamp `d32d055c`** (unchanged — regen is idempotent against the tracked generated artifacts).

- `git diff --stat 64ac4f5..HEAD -- modules/mod-era-talents/src/` → **EMPTY**. Confirms zero C++
  changed this entire phase; the `docker compose build` step was correctly skipped per the task
  directive.
- `uv run --with pyyaml python tools/era_audit.py` → **`era_audit: 0 finding(s)`**.
- `uv run --with pyyaml --with pytest -- python3 -m pytest tools/ -q` → **124 passed**.
- `docker compose up -d ac-db-import` → exit 0, no errors in the logs.
- `docker compose up -d --force-recreate ac-worldserver` → clean boot:

```
[mod-era-talents] loaded 432 talent nodes (generation d32d055c)
```

432 = 386 prior (Mage/Priest/Warlock/Hunter/Rogue/Warrior/Paladin/Druid) + **46 Shaman**, one
generation stamp, matching the plan's expectation exactly.
`docker compose logs ac-worldserver | grep -iE 'talent nodes|generation|error|spell_proc|spell_script_names'`
returned **only** the `[mod-era-talents] loaded 432 talent nodes` line — **zero errors on any
`9324xx` id, era script, `spell_proc`, or `spell_dbc` row**. A broader `grep -iE
"error|9324[0-9]{2}|9264[0-9]{2}"` across the full boot log returned only the benign, unrelated
`Can't set process priority class, error: Permission denied` container-permissions notice present
on every boot (same as every prior phase's record) — not error-worthy.

`era_talent` classId breakdown (all 9 classes, one query):

| classId | class | count |
|---|---|---|
| 1 | Warrior | 52 |
| 2 | Paladin | 44 |
| 3 | Hunter | 46 |
| 4 | Rogue | 51 |
| 5 | Priest | 47 |
| **7** | **Shaman** | **46** |
| 8 | Mage | 49 |
| 9 | Warlock | 50 |
| 11 | Druid | 47 |

Sum = 432, matching the boot line exactly. All eight prior classes unchanged from their own
verification records; **classId 7 = 46** as required.

## Step 2 — display-only + helper-inventory confirmation

- `grep -c "display_only: true" era-data/vanilla/shaman.yaml` → **0**. All 46 nodes wired.
- `grep -rn "9324[0-9][0-9]" era-data/vanilla/shaman.yaml | grep -oE "9324[0-9][0-9]" | sort -u` →
  `932400`-`932415`, plus two comment-only references to the free-band boundary (`932416`, `932499`
  — appearing solely in prose noting "932416-499 remain free", not as claimed helper ids). **Actual
  claimed helper ids: exactly `932400`-`932415` (16 ids)**, matching the expected range.
- DB cross-check (`era_talent` LEFT JOIN `era_talent_rank`, classId=7): all 46 node ids (`18800`-
  `18845`) show a non-zero `grantedSpellId` for every rank — **zero accidental display-only
  leftovers**, confirmed against the live database, not just the YAML.

## Baseline divergences / accepted-gaps appendix

Collected from the framework registry row (`docs/era-talents-framework.md` line 68) and each node's
YAML comment. For each: what's shipped, what's missing, and why.

1. **Guardian Totems (`18817`) — Windwall Totem half ABSENT.** Windwall Totem does not exist in
   3.3.5a (`missingSpells`). Shipped: the Stoneskin Totem damage-reduction half (+10/20%, bit15
   unique) + the Grounding Totem cooldown half (-1/2s, affectsMask bit18). **USER DECISION
   2026-08-21: document now, remake the totem in a later session.**
2. **Enhancing Totems (`18821`) — Grace of Air Totem half ABSENT.** WotLK merged Grace of Air into
   Strength of Earth; Grace of Air does not exist standalone in 3.3.5a. Shipped: the Strength of
   Earth effect bonus (+8/15%, bit16 unique). **USER DECISION 2026-08-21: document now, remake the
   totem in a later session.**
3. **Elemental Weapons (`18827`) — Rockbiter Weapon half CODE-VERIFIED INERT.** Rockbiter's AP bonus
   is a flat `SpellItemEnchantment.dbc` value consumed directly by
   `Player::UpdateDamageDoneMods`'s `case ITEM_ENCHANTMENT_TYPE_DAMAGE` — no spell is ever cast on
   this path, so `SpellEffectInfo::CalcValue`/`Unit::ApplyEffectModifiers` (what any spellmod rides)
   never runs. A spellmod bound to Rockbiter's identity would be a silent +0%. Not reconstructable
   in data; the only route is a core patch teaching `UpdateDamageDoneMods` to consult a spellmod.
   **USER DECISION: document as a gap, no core patch written.** Windfury (+13/27/40% on Windfury
   Attack 25504, bit23) and Flametongue+Frostbrand (+5/10/15% on Flametongue Attack 10444 / Frostbrand
   Attack 8034, bits21/24) are both shipped and code-verified live.
4. **Mana Tide Totem (`18845`) — flat-170-mana pulse NOT built.** The clone (932407) is Vanilla-exact
   on health (5hp, via the id≠16190 hardcode-dodge), duration (12s), and cooldown (5-min), but the
   totem summons the stock NPC 10467 whose pulse is bound in C++ (`spell_sha_mana_tide_totem`) to
   WotLK's %-max-mana formula rather than Vanilla's flat 170-mana/3s. The totem→pulse link is data
   (`creature_template_spell`), not hardcoded, so a flat pulse IS theoretically reconstructable — but
   only via a bespoke custom totem creature (new `creature_template` + `creature_template_spell` row
   → custom periodic aura → custom flat `SPELL_EFFECT_ENERGIZE` party pulse), which is outside the
   era-talents generator's scope (SQL/proc/script/era_talent tables only) and headless-untestable.
   **USER-CONSISTENT DECISION: ship the %-mana interim, document the faithful-flat-pulse gap for a
   later, larger-scoped session.**
5. **Improved Reincarnation (`18833`) — HP/mana-return half ACCEPTED GAP.** The cooldown reduction
   (-10/-20min) is authored and Vanilla-exact. The +10/20% health/mana-returned-on-res half lives on
   a *separate* spell (21169, SPELL_EFFECT_SELF_RESURRECT) with `SpellClassMask 0` — no family
   identity bit to bind a spellmod to without over-boosting every family-11 spell (the exact stock
   16184 Effect_2 over-bind this project deliberately does not replicate). A faithful fix needs a
   core patch/script; disproportionate for a minor beneficial-loss. **RUNTIME-VERIFY the cooldown
   reduction at Task 10** (self-res is hard to test headlessly).
6. **Totemic Mastery (`18838`) — radius-on-pulse is a Task-10 runtime-verify item, not a confirmed
   gap yet.** The generator binds the same all-totem summon mask used by Totemic Focus (per the
   task's explicit directive), but a buff totem's effect *radius* actually lives on its pulse/area-
   aura spell, not the summon. If the real client shows the radius mod doesn't move buff radii, it
   becomes a documented gap at that point — flagged now as an open risk, not asserted as broken.
7. **Eye of the Storm (`18809`) — proc-vs-spellmod divergence.** The imported 1.12 tooltip describes
   a proc (33/66/100% chance, on being the victim of a melee/ranged crit, to gain a 6s "Focused
   Casting" pushback-immunity buff). WotLK's own re-encoding of this exact talent (29062/64/65) ships
   it instead as a permanent per-cast `SPELLMOD_NOT_LOSE_CASTING_TIME` at the same 33/66/100% values,
   scoped to the Elemental damage set. This project ships the stock-proven spellmod form (a
   headless-untestable "victim of a crit" proc + short buff was avoided) — a minor, arguably
   beneficial simplification (the mod is unconditional rather than crit-gated-and-timed).
8. **Improved Fire Totems (`18808`) — Fire Nova Totem activation-delay half is NOT data-expressible.**
   The delayed-detonation timer is a totem creature/AI property; there is no SPELLMOD op for a
   totem's activation delay. Only the Magma Totem threat-reduction half (-25/50%, affectsMask{a:4})
   is shipped; the delay half is dropped, mirroring the Guardian/Enhancing Totems accepted-gap
   pattern.
9. **Stormstrike (`18830`) — single-vs-both-weapon-strike divergence.** The clone (932405) is
   Vanilla-exact on the defining mechanic (+20% nature damage debuff, 20s cd, flat 319 mana — all
   authored overrides on top of the inherited aura-271 nature scope). Effect_2/3 keep the stock
   WotLK weapon-strike triggers (32175/32176 — two weapon-damage triggers = "extra attack with both
   weapons"), whereas Vanilla Stormstrike granted a single extra attack. Reconstructing single-strike
   is a trigger-effect change (dropping one of 32175/32176), not a mask change — so the per-effect
   `maskA/B/C` capability doesn't help here; the strike-count difference is accepted as a minor
   beneficial gap, not reconstructable without a core patch.

## Deferred to Task 10 (real-client only)

Runtime behaviors headless verification cannot prove — require a live 3.3.5a Vanilla Shaman with the
fresh `patch-V.mpq` (`d32d055c`) + restaged `EraTalents` addon:

1. **Canary green** at login (server + client both `d32d055c`; no red generation-mismatch warning).
2. **3-tab panel** renders with per-tree backgrounds (`ShamanElementalCombat`/`ShamanEnhancement`/
   `ShamanRestoration`), all 46 nodes positioned, prereq arrows, real icons.
3. **All proc-firings:** Flurry (haste buff on a melee crit, correct 3-swing consumption), Ancestral
   Healing (armor buff on a healing crit), Healing Way (stacking received-heal buff on a Healing Wave
   cast, up to 3 stacks), Elemental Focus (Clearcasting on any elemental damage cast at 10% chance).
4. **Elemental Mastery end-to-end:** the guaranteed crit + free cast actually apply to the next Fire/
   Frost/Nature damage spell and are consumed correctly (not leaking onto a heal — the damage-only
   mask fix needs a live confirm).
5. **Imbue/totem %-move in the combat log:** Windfury Attack / Flametongue Attack / Frostbrand
   Attack magnitudes actually move under Elemental Weapons; Call of Flame / Improved Fire Totems /
   Restorative Totems / Improved Weapon Totems pulse magnitudes actually move under their respective
   spellmods; Totemic Mastery's radius (see appendix item 6).
6. **Spellmod tooltip/cast/cooldown rewrites** render correctly: Convection/Tidal Focus/Totemic Focus
   cost reduction, Improved Ghost Wolf/Lightning Mastery/Improved Healing Wave cast-time reduction,
   Reverberation/Improved Reincarnation cooldown reduction, Storm Reach range increase.
7. **Two-Handed Axes and Maces / Parry** grants: equip check for a 2H axe/mace succeeds only with the
   talent; a shaman without Parry cannot parry, one with it can.
8. **Confirm the EM/Elemental Focus cost charge is spent only on a damage spell**, never a heal — the
   headless doctor pass can confirm the DBC mask is heal-free (Step 1/2 above), but only a live cast
   sequence proves the charge consumption behaves correctly in practice.
9. **Canary green + era-transition strip symmetry** on a **real player** (bots never fire
   `EraTransition::Detect` — the documented limitation for every prior class).

### Accepted-divergence quick reference (see the appendix above for detail)

Guardian Totems (Windwall absent) and Enhancing Totems (Grace of Air absent) are both **USER DECISION
2026-08-21: document now, remake later**. Elemental Weapons' Rockbiter half is **CODE-VERIFIED INERT,
USER DECISION: document, no core patch**. Mana Tide Totem's flat-pulse is a **USER-CONSISTENT
DECISION: %-mana interim, faithful flat pulse deferred** (needs a custom totem creature). Improved
Reincarnation's HP/mana-return half, Improved Fire Totems' activation-delay half, and Stormstrike's
strike-count are smaller accepted gaps (spellmod-inexpressible without a core patch). Eye of the
Storm's proc-vs-spellmod divergence follows WotLK's own stock re-encoding of the same talent.
Totemic Mastery's radius binding is an **open runtime-verify item**, not yet a confirmed gap.

## DUMMY-marker and helper-id allocations (Shaman, this phase)

- **Helper band `932400`-`932499` (Shaman's reserved slice):** `932400`-`932413` + `932415` **used**
  (15 ids: Flurry r1-5, Stormstrike, Two-Handed Axes/Maces proficiency, Mana Tide Totem, Ancestral
  Healing r1-3, Healing Way buff, Nature's Swiftness, Elemental Mastery single buff,
  Elemental Focus/Clearcasting buff). **`932414` RETIRED 2026-08-23** (was the EM free-cast cost buff;
  folded into 932413 as a second effect once per-effect helper masks existed) — now **free again**.
  `932414` + `932416`-`932499` remain free for any future Shaman fix round.
- **DUMMY-marker registry:** **none claimed** this phase — no Shaman node reads a DUMMY marker by
  registry misc value (unlike, e.g., Druid's Predatory Strikes, which reads by icon instead of a
  registry slot). **Next free stays 16.**
- **Zero core patches, zero C++ scripts, zero hand-SQL.** Every node resolved to grant / spellmod /
  stat / multi / proc / clone via the generator's existing `helpers:`/`proc:`/`multi:` blocks; no new
  `SpellScript`/`AuraScript` class, no `patches/*.patch` entry, no `spell_script_names` row beyond the
  module's existing generic infrastructure, no hand-authored migration file.

The framework's helper-id registry table (`docs/era-talents-framework.md`, Shaman row, line 68)
already recorded this exact range/disposition from the per-tab authoring commits (Tasks 6-8); this
pass confirmed it against the live DB and generation stamp and left it unchanged.

## Family-11 integration note

Shaman (class id 7, spell family 11 — the mirror-inversion of Druid's class-id/family-id pair,
Druid = class 11 / family 7) is wired into the pipeline at these sites:

1. **`tools/gen_era_talents.py`** — `CLASS_TOKENS[7] = "SHAMAN"` (keyed by class id, used as the
   addon's displayed class-name fallback).
2. **`tools/era_audit.py`** — `DEFAULT_DATASETS` includes `era-data/vanilla/shaman.yaml`.
3. **`tools/era_audit.py`** — `_TRANSCRIBED_SWEEP_FAMILIES` includes `11`. No family-wide
   `SpellFamilyName == SPELLFAMILY_SHAMAN` pattern gate exists anywhere in the fork's
   `SpellInfoCorrections.cpp` (`grep -nE "SPELLFAMILY_SHAMAN"` returns zero matches, and a
   case-insensitive `shaman` grep over the whole file turns up only one unrelated id-pinned match) —
   so adding 11 to the vetted set is the entirety of Shaman's audit coverage; there is no
   family-wide sweep predicate to transcribe.
4. **`tools/era-regen.sh`** (and its `client-patch/merge-into-patch.sh` mirror) — the `DATASETS`
   array includes `era-data/vanilla/shaman.yaml`, and the per-dataset SQL-filename maps register
   `2026_08_21_60_era_talent_shaman_data.sql` / `2026_08_21_61_era_talent_shaman_custom_spells.sql`.
5. **`client-addons-src/EraTalents/build-addon.sh`** — three `gen_era_talents.py` invocations
   (`--sql`, `--lua`, `--custom-sql`) targeting `era-data/vanilla/shaman.yaml`, emitting the world SQL
   pair above plus `client-addons-src/EraTalents/data/generated/ShamanVanilla.lua`.
6. **`client-addons-src/EraTalents/EraTalents.toc`** — lists `data\generated\ShamanVanilla.lua` so
   the addon loads the generated Shaman talent data client-side.

All six sites were wired in Task 1 (`aed39fe`, "wire Shaman dataset (era-regen + build-addon + toc +
client-patch), 9-class boot") and Task 6 (`64ac4f5`, "era_audit covers Shaman (DEFAULT_DATASETS +
SWEEP_PREDICATES family 11)"); this pass re-confirms all six against the live tree with no drift.

## Status

**Headless: PASS.** The Task-9 full-tree verification sweep (evidence above) ran at stamp
`d32d055c`: `git diff --stat` confirmed zero C++ changed this phase (build correctly skipped), clean
rebuild + boot (432 nodes, stable, no crash, no our-band errors, all 9 classes' `era_talent` counts
reconciled), lint/audit/tests all green (0 findings, 124/124 passing), zero `display_only` leftovers
confirmed across all 46 nodes (both YAML grep and live-DB cross-check), the helper-id inventory
matches exactly (`932400`-`932415` used, `932416`-`932499` free, no DUMMY-marker slot consumed), and
the baseline-gap appendix collected and reconciled against the framework registry row and each
node's own YAML comment. **No content defects found** — this pass is verification-only and made no
authoring changes.

Branch merged to master 2026-09-07. **Shaman (Phase 10) headless-complete; the real-client pass (Task 10) is
next and last for this class.**

---

## Real-client CLOSE-OUT — 2026-08-24 (COMPLETE & VERIFIED)

Shaman is **complete and real-client verified** (user: "that is all good. Lets close out this class").
Final live generation `71272694`, built and run locally on the dev box. The real-client pass ran
across four fix rounds (the Vanilla totems were rebuilt in a separate session between rounds, which is
why several earlier "accepted gaps" — Grace of Air / Windwall / Mana Tide flat / Fire Nova Totem — are
now real totems the talents bind to):

| Round | Gen | What |
|-------|-----|------|
| 1 | `d32d055c`→`6ace860c` | Elemental Mastery collapsed to ONE buff + FLAT guaranteed crit (was aura-108 PCT, silently skipped by `ApplySpellMod`); Elemental Devastation + Healing Way icons de-`classic_`-prefixed; 2H Axes&Maces confirmed NOT a bug. Added generator per-effect helper class masks (`maskA/B/C`). |
| 2 | `6ace860c`→`2c4caaed` | Totemic Mastery radius rebound from summons to pulses; 2H equip-gate PlayerScript (revokes 2H on respec, module-side, no core patch); Improved Fire Totems Fire Nova detonation-delay now reads the talent. Guardian Totems confirmed already-working (low-rank int truncation). |
| 3 | `2c4caaed`→`71272694` | Totemic Mastery now also widens the **custom Windfury Totem** (its pulse 931366-368 carries word-B bit21 — added `b:2097152` to node 18838's `affectsMask`). |
| 4 | `71272694` (C++ only) | Windfury Totem no longer stacks with a Windfury Weapon self-imbue (`spell_era_windfury_totem::CheckProc` bails when the swinging hand carries a WF Weapon temp enchant). Scope: Windfury-only (user-approved). |

**Documented client-side limitation (not a defect):** Convection / Totemic Focus spellbook tooltips
show a stale mana cost (the 3.3.5a client caches the spellmod-modified cost, locking the displayed
value to the first rank hovered). The **actual mana consumed is correct**; our % `SPELLMOD_COST`
encoding mirrors the stock WotLK cost-reduction talents, so this is not fixable from server/DBC data.

**By-design behavior:** the 2H equip-gate also runs at login inventory-load, so respeccing away from
the Two-Handed talent while wielding a 2H axe/mace unequips and **mails** it on next login
(non-destructive — returned, not lost).

**All 9 Vanilla classes were completed here and MERGED to `master` on 2026-08-25.** (This line
previously read "Branch `feat/era-talents` merged to master 2026-09-07", which stopped being true at that
merge; TBC authoring continues on `feat/era-talents`.)

## Cross-era coupling — the Vanilla shaman does NOT stand alone any more

Two Vanilla totem chains now reach into the **TBC** id band at runtime. Both were introduced by the
2026-09-02 fix round and are load-bearing: **a TBC re-mint must keep these ids, and a Vanilla-only
regression pass will not exercise them unless the TBC dataset is also loaded.**

* **Flametongue Totem imbue.** The Vanilla chain is passives **931144-931147** -> effect clones
  **931148-931151** (`SPELL_EFFECT_ENCHANT_HELD_ITEM`) -> stock enchants **124 / 285 / 543 / 1683**.
  The module's `spellitemenchantment_dbc` overlay
  (`2026_09_02_00_era_flametongue_enchant_procs.sql`) re-points those four enchants' combat spell
  onto the **TBC-band** proc DUMMYs **947135-947138**, each bound to
  `era_sha_flametongue_totem_proc`. The magnitudes are **wago 2.5.4** values, used for BOTH eras
  because 1.12's `SpellItemEnchantment` is absent from this repo's extract — a deliberate, recorded
  substitution, not drift. So a Vanilla shaman's Flametongue damage comes out of a TBC-band spell.
* **Stoneclaw Totem taunt.** The Vanilla passives **931070-931075** trigger the threat effects
  **947062-947067** (TBC band), not stock 5729 — live 5729 carries `AttributesEx3` 131072
  ("no initial aggro"), which made the taunt add ZERO threat to an unengaged mob. The clones ship
  `attributesEx3: 0`.

Practical consequence: deleting or renumbering 947062-947067 or 947135-947138 silently breaks
**merged Vanilla content**. Neither range is free.
