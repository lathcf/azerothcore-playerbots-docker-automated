# Era Talents — TBC Phase 5 (Rogue) verification record

> **Generation superseded (2026-09-06 final review):** the current stamp is the closing row of docs/verification/era-talents-final-review-tbc-vanilla.md §10 — reinstall patch-V.mpq + restage the addon before any real-client pass.

**Status: HEADLESS-COMPLETE, AWAITING REAL-CLIENT SIGN-OFF (generation `f5c228df` — the
2026-09-03 ref-text fix round re-stamped the identical data; §10's sweep numbers were taken at
`5d69f170` and are unchanged by it).** All 67 nodes
authored and wired, `era_audit.py` clean, 214/214 host tests green, the four-rung band-move doctor
ladder zero-orphan on a live bot, and a 281-bot broad doctor sweep with zero `known=NO` /
`spellInfo=MISSING` rows. Branch `feat/era-talents`, **merged to master 2026-09-07** (TBC merges as a set after all
nine classes + final review + a Vanilla regression pass). The `EraTalentIP.cpp` `ERA_TBC` allowlist
flip remains an **uncommitted dev-box edit** (verified ` M` and unstaged at close-out; no phase
commit touches the file — the last two commits to it are `312d69b` / `0fbd788`, both pre-phase).

~~**One item is REPORTED, NOT FIXED** and needs an orchestrator decision before real-client
sign-off: a by-id bot-AI site on Sword Specialization (§11).~~ **RESOLVED 2026-09-03 (Task 9b,
commit `679110b`) — see §17:** the site is now routed through the patch-0021 bridge and needed a
`kEraNameAliases` row for WotLK's "Hack and Slash" rename. It was a *bot gear-weighting* defect,
pre-dating this phase, not a player-facing one.

**Spec/plan:**
`docs/superpowers/specs/2026-09-02-era-talents-tbc-phase5-rogue-content-design.md`
(**Amendments A and A.14 supersede much of the body** — A.14 in particular reverses §3a/A.5 on
Mutilate) + `docs/superpowers/plans/2026-09-02-era-talents-tbc-phase5-rogue.md`.
**Findings authority:** `era-data/_ref/tbc/rogue-spells.yaml` (4041 lines — `families_compared:` /
`survival_map:` / `id_repurposing:` / `poison_chains:` / `script_census:` / `proc_data_census:` /
`capstone_chains:` / `preparation_resolution:` / `reuse_ledger:` / `node_findings:` /
`accepted_gaps:` / `open_questions:` / `id_allocation:`) — this record summarizes; the `_ref` is
normative.

**Phase commits:** `dbfabaa` T2 (tooltip evidence) · `d8aecfb` Amendment A · `c1f603e` T4 (wiring) ·
`c1bf2a5` T5 (substrate) · `5ea5e23` T6 (Assassination) · `435d2da` A.14 · `74bbcc7` T6b (Mutilate
package) · `8cc6939` T7 (Combat) · `c145fcd` T8 (Subtlety) · `133567a` + `0b0c121` T8b (the Vanilla
Riposte cross-era fix + its record) · `5903380` T8c (bot-login reconcile) · *this commit* T9.

---

## 1. What shipped

The complete **TBC 2.4.3 Rogue talent tree** — **67 nodes** (Assassination 21 / Combat 24 /
Subtlety 22, ids **20400–20466**, era 1) — plus the Mutilate era package (Amendment A.14), the Find
Weakness reconstruction, three module scripts, and three new generator keys. Value basis:
**wago 2.5.4** end-to-end (the standing TBC basis). **Zero new core patches this phase.**

- **Class 4, spell family 8.** Family guard clean: 0 rows in either band with `SpellClassSet`
  outside `{0, 8}`.
- **Auto-passives:** **113 rows in `939200–939724`** (`936000 + (nodeId−20000)*8 + (rank−1)`; band
  ceiling 939735). No overlap with paladin / druid / shaman / warrior.
- **Hand helpers: 20 ids, `947768–947787`** — a *grant-heavy* tree (35 of 67 nodes are plain
  grants), which is why 67 nodes needed roughly a third (20/56) of the warrior phase's 56 helpers.
  **Free within the rogue slice: `947788–948023`.** Next class claims from **948024**.
- **NO DUMMY-marker misc consumed.** No era script needed to read a value off another spell's
  marker — the two Mutilate scripts key on clone OWNERSHIP (`HasSpell(947773)`), which is the
  cheaper and more robust form. Registry next-free stays **19**.
- **SQL:** `2026_09_04_00_..._data.sql` + `_01_..._custom_spells.sql` (generated);
  `2026_09_04_02_..._mutilate_trainers.sql` (hand — 3 `trainer_spell` rows on TrainerId 9 at
  L50/60/70 + one 4-link `spell_ranks` chain). **Nothing deleted, nothing re-gated.** The plan's
  reserved `_03` Hemorrhage trainer SQL was never written: TBC Hemorrhage is LIVE_MATCH on all
  fourteen families, so node 20449 grants stock 16511 and the stock trained ranks are already
  correct (and the "deleting a stock trainer row orphans higher ranks" hazard therefore never
  arose — the file says so explicitly and warns against "tidying" it).
- **C++:** `EraTalents.cpp` (the `CLASS_ROGUE` reconcile arm — `kEraRogTbcStockSwaps`, five
  readiness-gated rows), `EraTalentProcScripts.cpp` (`era_rog_preparation_tbc`,
  `era_rog_mutilate_poison`, `era_rog_mutilate_behind`; plus verifying `era_rog_preparation` and
  `era_rog_sap_stealth` stay Vanilla-inert), `EraTalentBots.cpp` (the T8c login reconcile, §12).
  `EraTalentsCommand.cpp` needed **no** new doctor predicate — the one trained-rank chain
  (Mutilate 947774–947776) roots at a node-granted r1 whose ids are all inside the generic
  `NodesFor()` resolution, and the sweep confirmed zero rogue orphans in all three bands.
- **Generator:** three purely-additive keys, all TDD-covered and byte-neutral for already-shipped
  data — `proc.affectsMask` (Blade Twisting's ability mask), per-effect `mechanic:` (the Riposte
  clone's `EffectMechanic_2 = 3` DISARM), and node-effect `perLevel:` (Serrated Blades' float
  `EffectRealPointsPerLevel`). The `perLevel:` key now has a **row-level** test as well as the
  three `_effect_fields`-level ones (`test_node_effect_per_level_reaches_the_custom_spell_row`) —
  a Task-8 review note: the original three would all have passed if the row writer had dropped the
  column or emitted it as an int, silently shipping `-2` for `-2.67`.

## 2. Per-tab node counts and disposition tallies

All 67 wired, 0 display-only (live DB: `wired` = 67). Two views — the **shipped mechanic** (from
`era-data/tbc/rogue.yaml` at generation `5d69f170`) and the **`_ref` disposition bucket** (from
`node_findings:`).

| Tab | Nodes | ids | grant | spellmod | stat | multi | proc |
|---|---|---|---|---|---|---|---|
| Assassination | 21 | 20400–20420 | 11 | 4 | 2 | 3 | 1 |
| Combat | 24 | 20421–20444 | 12 | 3 | 4 | 3 | 2 |
| Subtlety | 22 | 20445–20466 | 12 | 1 | 2 | 6 | 1 |
| **Total** | **67** | | **35** | **8** | **8** | **12** | **4** |

`_ref` disposition buckets (`node_findings.tally:`), **corrected for A.14**:

| Bucket | Count | Notes |
|---|---|---|
| `grant_stock` | **29** | TBC == live on all fourteen families |
| `authored` | 29 | generated passive, no helper |
| `authored_plus_helper` | 2 | Mace Specialization (20433), Blade Twisting (20437) |
| `clone` | **6** | Riposte 20428, Adrenaline Rush 20441, Setup 20453, Preparation 20457, Premeditation 20463, **Mutilate 20420** |
| `reconstruction` | 1 | Find Weakness 20419 |

The two views reconcile exactly: the 35 `mechanic: grant` nodes = 29 `grant_stock` + the 6 `clone`
nodes (a clone is still delivered by a `grants:` row, it just points at a `947xxx` id instead of a
stock one). The remaining 32 nodes are generated passives (`spellmod` 8 / `stat` 8 / `multi` 12 /
`proc` 4), which the `_ref` splits into `authored` 29 + `authored_plus_helper` 2 + `reconstruction` 1.

**The `_ref`'s own machine tally reads `grant_stock: 30 / clone: 5`** because node 20420's raw
`bucket:` field still records the DBC sweep's verdict (`grant_stock`, `1 DIVERGED`). Amendment A.14
overturned it **on behaviour grounds, not data grounds** — see §6. The corrected numbers above are
the shipped truth; the `_ref` keeps the raw verdict on purpose so the sweep's own reasoning stays
auditable.

## 3. Fourteen-family survival-map verdict distribution

The standing fourteen-family column list (warrior A.9), replicated verbatim from
`era-data/_ref/tbc/warrior-spells.yaml`'s `families_compared:` and re-recorded in this phase's
`_ref` at lines 31–75. **351 ids** swept (talent rank spells + every triggered/implied spell the
tree reaches, including all six poison chains).

| Verdict | Count | Meaning |
|---|---|---|
| `DIVERGED` | **189** | at least one of the fourteen families differs |
| `LIVE_MATCH` | **108** | byte-identical across all fourteen |
| `WAGO_ONLY` | **29** | exists in TBC 2.5.4 data, absent from the 3.3.5a DBC |
| `LIVE_ONLY` | **25** | exists live, absent from TBC |
| **Total** | **351** | |

Families: **A** effects (incl. `EffectRealPointsPerLevel` as a 4-dp float, `EffectMechanic`, both
implicit targets) · **B** proc options (`ProcChance`/`ProcCharges`/`ProcTypeMask`/`CumulativeAura`)
· **C** misc (`DurationIndex`, `CastingTimeIndex`, `RangeIndex`, `SchoolMask`, `Speed` as float, and
**all eight** attribute words) · **D** categories (`Category`, `DefenseType`, `DispelType`,
`Mechanic`, `PreventionType`, `StartRecoveryCategory`) · **E** power (`ManaCost`/`ManaCostPct` —
rogue **energy rides `ManaCost` 1:1**) · **F** class options (`SpellClassSet` + identity mask
**word 0 only**) · **G** cooldowns (`RecoveryTime`, `CategoryRecoveryTime`, `StartRecoveryTime`) ·
**H** aura restrictions (all eight caster/target aura-state and aura-spell columns) · **I**
shapeshift · **J** levels · **K** equipped item · **L** interrupts · **M** targets · **N** identity
(name, as an id-repurposing detector).

**Family G alone moved several nodes off `grant_stock`** — Preparation, Adrenaline Rush and
Premeditation differ from live in *nothing but* cooldown (and, for Premeditation, `DurationIndex`).
An effects-only sweep would have granted stock on all three and shipped WotLK cooldowns.

**Family N earned its place three times** (`id_repurposing.hazards:`): 13706/13804–13807 "Dagger
Specialization" is live **"Close Quarters Combat"** with `EquippedItemSubclass` widened to
dagger|fist; 13960–13964 "Sword Specialization" is live **"Hack and Slash"** with the trigger
repointed to 66923 and the subclass widened to sword1h|axe1h; 31125 "Dazed" is live
**"Blade Twisting"** at −70%/4s + `SUPPRESS_TARGET_PROCS`. Granting any of those stock ids would
have handed a TBC rogue a bonus WotLK never gave TBC and mislabelled the tooltip. All three are
authored instead. (5530 "Mace Stun Effect" is the fourth: live it is a pure +proficiency spell with
**no stun at all**, so the stun payload had to be minted fresh — 947784, the TBC twin of the
Vanilla 932988 conclusion.)

### A.9 — the proc-row census (a sweep the DBC alone cannot do)

`proc_data_census:` (13 entries) records every `spell_proc` / `spell_proc_event` row in
`acore_world` keyed to a spell in the tree's id set. **A negative-id row there OVERRIDES the DBC's
proc contract**, so a fourteen-family DBC sweep can be wrong in both directions. It changed three
verdicts:

- **Combat Potency** (`-35541`, `spell_proc_event`): its own row already overrides `ProcTypeMask`
  to 8388608, neutralising the node's ONLY divergence → **grant stock**, core script kept.
- **Sword Specialization** (`-13960`, `spell_proc`): the row is **all zeros**, which a clone or an
  authored passive does **not** inherit → the generator must write the proc row or the talent
  silently never fires.
- **Setup** (`-13983`): `procEx 24` lives in the row, not the DBC; the shipped disposition binds the
  core script to the three generated auto-passives instead of cloning (§7).

Also load-bearing negatives that stay untouched because their node grants stock: `-31244` Quick
Recovery, `-14186` Seal Fate, `-14156` Ruthlessness, `-14143` Remorseless Attacks, `-13754`
Improved Kick, `-16511` Hemorrhage (an all-zero row), `31221/31222/31223` Master of Subtlety.

### A.12 — the `EffectSpellClassMask` column trap

`EffectSpellClassMask{A,B,C}_{1,2,3}` is **`{effect}_{word}`**, not `{word}_{effect}`: the LETTER is
the effect index (A = Effect_1) and the numeric suffix is the family mask WORD (`_1` = word0).
Reading it the other way transposes every multi-effect spellmod comparison, manufacturing
divergences that are not there and hiding real ones. This governs the generator's per-effect
`maskA`/`maskB`/`maskC` keys and `affectsMask {a,b,c}` identically, and is distinct from the
helper-level `mask`/`maskB`/`maskC` scalars (which set the spell's OWN identity flags). The
per-effect mask pass is tracked separately in `effect_class_mask_diffs:` as **supplementary
(letter O)** and deliberately does **not** feed a verdict — 24 hits, of which 4 are load-bearing
and the rest encoding drift.

## 4. Poison chains — map and dispositions

Six chains, 32 payload spells, all swept. **Nothing in the poison chains is cloned or overridden.**
The pattern is uniform and is the reason for accepted gaps 1 and 2: the *applier* (the enchant-path
spell the rogue casts on the weapon) is LIVE_MATCH or trivially divergent, and the *payload* (the
spell that actually hits) is DIVERGED on magnitude — and a payload is a **global stock spell**,
which a per-character era cannot legally override.

| Chain | Stock ranks | Applier verdicts | Payload verdicts | Disposition |
|---|---|---|---|---|
| Instant Poison | 6947 → 21927 (7) | 7 × LIVE_MATCH | 7 × DIVERGED (damage + `RangeIndex`) | accepted gap 1 |
| Deadly Poison | 2892 → 22054 (7) | 7 × LIVE_MATCH | 7 × DIVERGED (per-tick damage + `RangeIndex`) | accepted gap 1; core `spell_rog_deadly_poison` (−2818) 5-stack refresh untouched |
| Wound Poison | 10918 → 22055 (5) | 5 × DIVERGED (`ProcChance` only — **inert on the enchant path**) | 5 × DIVERGED (**behavioural**: TBC −10% × 5 stacks vs live flat −50% no-stack) | accepted gap 2 |
| Mind-numbing Poison | 5237 → 9186 (3) | 3 × LIVE_MATCH | 3 × DIVERGED (proc/attribute/range metadata only — the cast-speed debuff is identical) | no gap |
| Crippling Poison | 3775 → 3776 (2) | 2 × LIVE_MATCH | 2 × DIVERGED (metadata only — snare values identical) | no gap |
| Anesthetic Poison | 21835 (1) | DIVERGED (`ProcChance` 20→50, inert on the enchant path) | DIVERGED (metadata only — the 133 threat reduction is identical) | no gap |

The three poison **talents** are all authored, because none has a usable stock carrier at TBC
values: **Vile Poisons** (20409 — live 16513–16515 DIVERGED, TBC's 16719/16720 are WAGO_ONLY),
**Improved Poisons** (20410 — all five live ranks DIVERGED on mask *and* values), **Master
Poisoner** (20416 — live 31226/31227 are the WotLK rework, a DoT-poison proc; TBC's is a plain
passive and not a proc at all).

## 5. Reuse ledger — outcome

**0 of 5** offered Vanilla helpers reused. (Framework rule: a `932xxx` helper is granted as-is or
not at all; a needed change makes a fresh `947xxx` clone.)

| Offered | Verdict | Reason |
|---|---|---|
| 932983 Preparation clone | **FRESH** | The *data* is TBC-equal, but Preparation is ~100% script behaviour bound by id: 932983's script resets **Blind** (Vanilla-only) and **excludes Premeditation** — the exact inverse of TBC's list. Its client Description is Vanilla-authored and may not be edited. → 947786 + `era_rog_preparation_tbc`. |
| 932985 / 932986 / 932987 Hemorrhage r1–r3 | **MOOT** | Neither reuse nor clone: TBC Hemorrhage is LIVE_MATCH on all fourteen families, so node 20449 grants stock 16511 and the Vanilla clones are irrelevant to the TBC node. |
| 932988 Mace-Spec stun payload | **FRESH** | 9 field differences. The functional stun half matches, but TBC's 5530 carries a vestigial weapon-skill half plus differing metadata that do not belong in a clean rebuild. → 947784. |

## 6. Fresh-clone id table (used sub-range `947768–947787`, 20 ids)

| id(s) | What | Node | Why not a grant |
|---|---|---|---|
| 947768–947772 | **Find Weakness** r1–r5 self-buffs, 10 s | 20419 | **Reconstruction.** TBC delivers the talent's armour-ignore as a finisher-armed buff; no stock carrier survives at TBC's shape. Each rank carries TBC 31234–31238's two spellmod effects. Word-1 `0x10f` covers the two "half" bits, which is also what Cold Blood 14177 binds through — dropping those words would silently unhook every one of them. |
| 947773–947776 | **Mutilate** visible casts r1–r4 (templates 1329/34411/34412/34413) | 20420 | Amendment A.14 — see below. r1 node-granted; r2–r4 trainer-taught (TrainerId 9, L50/60/70) via the phase's one hand SQL. |
| 947777 | **Riposte** (template 14251) | 20428 | Live 14251 is WotLK's *slow attack + combo point*; TBC is a **6 s disarm**. `durationIndex 32` (6000 ms vs live 9 = 30000 ms), Effect_2 re-authored as aura 67 `MOD_DISARM` with **`mechanic: 3` DISARM** — inheriting the template's `EffectMechanic 8` (SLOW_ATTACK) would test a disarm-immune target for slow-attack immunity and put the disarm in the wrong DR group. |
| 947778 | **Adrenaline Rush** (template 13750) | 20441 | `RecoveryTime 300000` (TBC 5 min) vs live 180000. The ONLY divergence — the exact TBC twin of the Vanilla clone 932770. |
| 947779–947783 | **Mace Specialization** r1–r5 proc passives (templates 13709/13800–13803) | 20433 | Two halves on one passive: e1 aura 42 `PROC_TRIGGER_SPELL` (`ProcChance` 1/2/3/4/6, `ProcTypeMask 20`) → the stun, plus the crit-damage half. Live's chain is the WotLK retune. |
| 947784 | **Mace stun payload**, 3 s (from scratch) | 20433 | Live 5530 "Mace Stun Effect" was repurposed into a pure `SPELL_EFFECT_PROFICIENCY` spell with **no stun at all** — there is no stock carrier on 3.3.5a. |
| 947785 | **Blade Twisting daze payload** (from scratch) | 20437 | Live 31125 is "Blade Twisting" at −70% / 4 s + `SUPPRESS_TARGET_PROCS`; TBC's "Dazed" is −50% / 8 s. WotLK also repointed the talent's trigger to a new id 51585 (LIVE_ONLY). |
| 947786 | **Preparation** (template 14185) | 20457 | `RecoveryTime 600000` (TBC 10 min) vs live 480000 (8 min) — the only DBC divergence — **plus** the whole reset list is script behaviour (§7). |
| 947787 | **Premeditation** (template 14183) | 20463 | BOTH halves moved: `DurationIndex 1` (10 s combo-retain window) vs live 18 (20 s), **and** `RecoveryTime 120000` (2 min) vs live 20000 (20 s). |

Every one of the five clones templated off a stock spell was checked for the two clone traps:
`spell_script_names` rows to rebind (§7) and `SpellInfoCorrections.cpp` by-id fixes to
`spellFixAck:` — grepped 2026-09-03 against `azerothcore-wotlk/src/server/game/Spells/
SpellInfoCorrections.cpp`; **none** of 1329/34411/34412/34413/14251/14183/14185/13750/
13709/13800-13803 carries an entry there, so no `spellFixAck:` is due on any of them (14185's
missing reset behaviour is a `spell_script_names` rebind, §7 — a different mechanism, not a
`SpellInfoCorrections` fix). `client.description` **and**
`client.auraDescription` are authored on every visible clone (lesson 17d) — without the aura text
the Riposte debuff tooltip would have read live's "Melee attack speed slowed by $s2%."

### Mutilate (Amendment A.14) — what is proven and what is not

**Finding.** Mutilate reads `grant_stock` on all fourteen DBC families, and the disposition is still
wrong, because the era-relevant magnitude is **not in the DBC at all**: `SpellEffects.cpp`
(≈3454–3475) hardcodes the "+20% damage if the target is poisoned" rider behind
`m_spellInfo->SpellFamilyFlags[1] & 0x6` — the hidden hand-halves' (5374/27576/34414–34419) own
family bits, not the visible cast's id — and the *absence* of TBC's "must be behind the target"
requirement is likewise a core fact (`SpellMgr.cpp:3590`'s by-id switch for
`SPELL_ATTR0_CU_REQ_CASTER_BEHIND_TARGET` never lists Mutilate). TBC's number is **+50%**. This is
the same finding class as core patch 0023's Heart of the Wild, and it is now workflow lesson 20.

**Shipped package** (user-locked disposition; supersedes spec §3a / A.5):

1. **Four visible casts cloned** (947773–947776) with authored TBC tooltips. The visible cast
   carries no damage effect at all (e1 combo points, e2/e3 trigger the hidden halves) — there is
   nothing on it for the hardcoded branch to dodge. The clone exists for (i) the TBC
   `client.description` (tooltip honesty), (ii) the trainer-chain root for r2–r4, and (iii) the
   behind-gate `era_rog_mutilate_behind` bind; the damage top-up lives on the STOCK halves via
   `era_rog_mutilate_poison` (point 2).
2. **The eight hidden hand-halves stay STOCK** (5374/27576/34414–34419) — they carry the actual
   damage (`Effect_1 = 121 NORMALIZED_WEAPON_DMG`) and are what the core's +20% branch keys on.
   `era_rog_mutilate_poison` is bound to those eight stock ids and multiplies by **×1.25** on a
   poisoned target (1.20 × 1.25 = TBC's 1.50). It is a strict no-op unless the caster **owns clone
   r1 947773**, so a Vanilla or WotLK rogue's Mutilate is byte-identical.
3. **`era_rog_mutilate_behind`** is bound to the four clones and re-adds the positional
   `CheckCast`. Clone-bound, therefore inherently TBC-only.
4. **The core's `spell_rog_mutilate`** (the AfterCast that consumes a Cold Blood charge) is
   **rebound** onto all four clones by positive per-rank binds. Without it a TBC rogue's Cold Blood
   charge would survive a Mutilate and the *next* ability would crit for free. **Cold Blood stays
   STOCK 14177** (A.7) precisely so the script's hardcoded `caster->GetAura(14177)` lookup and
   `spell_rog_cold_blood`'s family-bit `CheckProc` keep working verbatim.
5. **Trainer chain**: 3 `trainer_spell` rows + a 4-link `spell_ranks` chain. Nothing deleted.

**Structurally proven headlessly:** the clone chain exists and is learnable (r1 granted, doctor
`known=yes spellInfo=yes`); the trainer rows and `spell_ranks` chain load without error; all
twenty `spell_script_names` rows are present and no `Script named '…' is not assigned` warning
remains; the stock ids 1329 and the eight halves are untouched for non-TBC characters (the
ownership guard is a `HasSpell` on a band id).

**UNOBSERVED — real-client items:** the **+25% top-up actually firing** (needs a poisoned target and
a damage comparison), and the **behind-gate's effect on Assassination bots' AI** (a bot that cannot
reposition behind a target will simply fail the `CheckCast`; whether the AI then falls through to
another ability cleanly, or spins, has not been observed — no bot build in the sweep spent the
Assassination capstone, see §10).

**Residual imprecision, documented not fixed** (accepted gap 8): the +25% top-up lands *after*
`MeleeDamageBonusDone/Taken`, so flat addend components (e.g. Hemorrhage's +38) get scaled too.
Single-digit.

## 7. Script census (16 core rows + 2 module) and rebinds

From `script_census:`, re-grepped against the live `spell_script_names` table 2026-09-03.

**Kept free (node grants stock, script keeps working):** `spell_rog_blade_flurry` (13877),
`spell_rog_cheat_death` (−31228 — all three families LIVE_MATCH, which *refutes* spec §3a),
`spell_rog_combat_potency` (−35541), `spell_rog_master_of_subtlety` (31666),
`spell_rog_quick_recovery` (−31244), `spell_gen_remove_impairing_auras` (30918, the Improved Sprint
payload), `spell_rog_cold_blood` (14177). **Untouched baseline:** `spell_rog_deadly_poison` (−2818),
`spell_rog_pickpocket` (921), `spell_rog_rupture` (−1943), `spell_rog_vanish` (−1856),
`spell_rog_vanish_purge` (18461). **Not needed:** `spell_rog_nerves_of_steel` (−31130 — node 20442
is authored because live is the WotLK rework).

**Rebinds and new binds — 20 `spell_script_names` rows, all emitted by the generator's
`scriptBindings:` block:**

| Rows | Script | Bound to | Why |
|---|---|---|---|
| 4 | `spell_rog_mutilate` (core) | 947773–947776 | Core binding is `-1329` = the stock chain only; the clones would get no Cold Blood bookkeeping. **Positive per-rank binds, not one negative `-947773`** — a negative id requires the bound spell to be the first rank of a real `spell_ranks` chain *at the time* `ObjectMgr::LoadSpellScriptNames` runs, and this chain's rows live in a hand migration. A positive bind has no load-order coupling. |
| 4 | `era_rog_mutilate_behind` (new) | 947773–947776 | TBC's positional requirement; clone-bound = inherently TBC-only. |
| 8 | `era_rog_mutilate_poison` (new) | 5374, 27576, 34414–34419 | The +25% top-up, on the STOCK halves that carry the damage. Bound to stock ids — the `era_rog_sap_stealth` idiom; the generator's `(id, ScriptName)`-qualified DELETE for a non-band id leaves the core's own bindings on those spells untouched. |
| 1 | `era_rog_preparation_tbc` (new) | 947786 | `spell_rog_preparation` is bound by POSITIVE id to stock 14185 only, so the clone starts with **no script** and its DUMMY effect would reset nothing (button fires, GCD spent, nothing happens). The new script does the same family-flag walk with mask `flags[1] & 0x260 \| flags[0] & 0x860` — the stock pair **plus Premeditation's word-1 bit 5 (0x20)**, which clone 947787 carries. **The core script is deliberately NOT also bound**: it is a superset-minus-one, not a complement, and it additionally carries the Glyph-of-Preparation branch — glyphs are WotLK-era-only for everyone under `EraGlyphGate`, so a TBC rogue must not reach it. One bind, one walk. |
| 3 | `spell_rog_setup` (core) | 939624–939626 (auto-passives) | Bound `-13983` = the stock chain, so the band passives get nothing. **Genuinely load-bearing, not decoration:** an aura-42 TAKEN proc casts its trigger at `eventInfo.GetProcTarget()` — the ATTACKER — so without the `GetActor() == GetSelectedUnit()` check a rogue who dodges mob B while fighting mob A banks a combo point on B and splits the combo pool. Positive per-rank binds (auto-passives are never a `spell_ranks` root). |

**Verified era-inert for TBC** (the two Vanilla module scripts): `era_rog_preparation` is bound to
932983 **only**, and `era_rog_sap_stealth` to the stock Sap ranks with an `era < ERA_TBC` gate — TBC
Sap does not break stealth (baseline), so the Vanilla hook must stay dormant, and does.

**Predicted by the plan but ABSENT from the live table** (so nothing to rebind): `spell_rog_shiv`
(bound to non-talent 5938, outside the tree's id set) and any Blade Flurry clone binding (13877 is
granted).

## 8. Preparation resolution

A genuine three-way, resolved by `preparation_resolution:`:

| | Cooldown | Resets |
|---|---|---|
| **TBC 2.4.3** | 10 min | Evasion, Sprint, Vanish, Cold Blood, Shadowstep, **Premeditation** |
| **Live 3.3.5a** (core `spell_rog_preparation`) | 8 min | the same five — **misses Premeditation** |
| **Shipped Vanilla clone 932983** | 10 min | the five plus **Blind**, no Premeditation |

→ **fresh clone 947786** (cooldown 600000) + **new script arm** `era_rog_preparation_tbc`, bound
only to that clone; `era_rog_preparation` is explicitly *not* widened or reused, because doing so
would change shipped Vanilla-rogue behaviour. The clone's `client.description` names all **six**
TBC resets (stock 14185's own text names only five).

**UNOBSERVED headlessly:** the reset itself. Nothing in a bot build casts Preparation, and a
console test cannot put six abilities on cooldown. This is real-client checklist items 7 and 15
(of 17, §15).

## 9. Accepted gaps

From `accepted_gaps:` — 9 entries, of which **4 are closed/settled** and **5 genuinely ship as
gaps**. Every gap's client tooltip describes the *shipped* behaviour, not the TBC ideal (the
"gaps read honestly" checklist item).

| # | Gap | What ships instead | State |
|---|---|---|---|
| 1 | **Poison payload magnitudes** (all 32 payloads DIVERGED) | Nothing changes — payloads are global stock spells and a per-character era may not override them. The Vile Poisons talent scales whatever the live base is. | **SHIPS AS GAP** |
| 2 | **Wound Poison stacking model** — TBC −10% × 5 stacks vs live flat −50% no-stack | Nothing changes; same stock-data rule as gap 1. This is the one *behavioural* poison gap, not just a number. | **SHIPS AS GAP** |
| 3 | **Cold Blood does not apply to Shiv** (the mask lost that bit in the WotLK rework) | GRANT stock 14177 as-is. Cloning it would break the hardcoded Mutilate ↔ Cold Blood charge interaction (§6.4) for a mask bit. | **SHIPS AS GAP** |
| 4 | Precision's weapon set includes 1H axes | **CLOSED as not-reachable** — code-verified INERT: aura 54 has no equip-fit predicate, so node 20426 grants stock at zero behavioural cost. | closed |
| 5 | **Stealth r1 movement penalty** (50%→30%) and **Vanish base cooldown** (5→3 min) | Nothing changes — both are *baseline* spells, not talents. Camouflage / Elusiveness / Preparation still work correctly as spellmods against the live base. | **SHIPS AS GAP** |
| 6 | Sword Specialization's 500 ms internal cooldown | **SETTLED** — confirmed real (per-character, 500 ms) via wowsims/tbc source; shipped as `proc.cooldown: 500`, pure data. | closed |
| 7 | Serrated Blades' per-level armour penetration | **SETTLED** — the float is carried, not flattened, via the new `perLevel:` node-effect key (`EffectRealPointsPerLevel` −2.67/−5.34/−8.00). | closed |
| 8 | **Mutilate's +50%-vs-poisoned and lost positional requirement** | **CLOSED by A.14** — clone chain + two era scripts + trainer SQL, zero core patches (§6). Residual: the +25% top-up lands after `MeleeDamageBonusDone/Taken`, so flat addends get scaled too — single-digit imprecision, documented not fixed. | closed w/ residual |
| 9 | **Mace Specialization** — three sub-items | (a) the **crit-damage half is not weapon-gated**: it applies with any weapon. Core behaviour; gating it would need a core patch, disproportionate for the effect size. (b) the stun payload's `DefenseType` stays 0 — no generator key, and the field is inert for a triggered stun. (c) TBC's `PREVENTS_ANIM` bit is not carried — an explicit "keep LIVE" adjudication, not an oversight. | **SHIPS AS GAP** |

**A note that belongs with gap 7 and is easy to misread:** Serrated Blades' shipped tooltip states
the **level-70** armour value, while the effect is genuinely **level-scaled**
(`EffectRealPointsPerLevel`). The two agree at 70 and disagree below it — which is correct
behaviour, not a defect, because the aura **amount is fixed at application time**: the core
computes it once from the caster's level when the passive is applied. A character who levels while
holding the talent keeps the old amount until the aura is re-cast, which
`ReconcileBaselineSpells` does on **learn / reset / login / level / era transition** — so in
practice it re-casts on the level-up itself and the value tracks. Do not "fix" this by flattening
the float.

## 10. Headless sweep (generation `5d69f170`)

`tools/era-regen.sh --sync-fork` re-run at Task 9 produced **generation `5d69f170`, unchanged** —
i.e. byte-identical to Task 8b's regen, with a clean `git status` afterwards (only the untracked
allowlist edit). Task 8c changed C++ only, no data. The merged client patch carries **809** custom
client spell rows; the merged `Spell.dbc` has **50820** records.

| Check | Result |
|---|---|
| `era_audit.py` | **0 findings** |
| `pytest tools/ -q` | **214 passed** (was 213; +1 = the new `perLevel:` row-level test) |
| `era_talent` wired, era 1 / class 4 | **67** |
| Family guard: `939200–939735` ∪ `947768–948023` with `SpellClassSet NOT IN (0,8)` | **no rows** |
| Outside-slice: `spell_dbc` `948024–949999` | **no rows** |
| Helper ids actually present in `947768–948023` | exactly **20** (947768–947787), names as §6 |
| Auto-passives present in `939200–939735` | **113**, min 939200 max 939724 |
| `era_talent_meta` | `generation = 5d69f170` |

Full `era_talent` group-by (all prior rows unchanged, plus `1 4 67`):

```
era 0 (Vanilla): 1:52  2:44  3:46  4:51  5:47  7:46  8:49  9:50  11:47
era 1 (TBC):     1:66  2:64  4:67  7:61  11:62
```

## 11. Bot AI by-id check — **one unrouted site found, NOT fixed here**

**Method note first, because the plan's command is stale.** Plan Task 9 Step 2 greps
`.../mod-playerbots/src/strategy/rogue/` and `src/strategy/actions/` — **neither directory exists
any more** (upstream reorganised to `src/Ai/Class/<Class>/`, `src/Ai/Base/`, `src/Mgr/Item/`). Run
as written the grep returns *nothing* and looks clean. The sweep below was re-run over the **whole**
of `azerothcore-wotlk/modules/mod-playerbots/src` (the fork with patches 0020/0021 applied).

Swept ids: every stock id this phase cloned or replaced — 1329 / 34411–34413 (Mutilate),
14251 (Riposte), 13750 (Adrenaline Rush), 14185 (Preparation), 14183 (Premeditation),
31233–31242 (Find Weakness), 13706 + 13804–13807 (Dagger Spec), 13960–13964 (Sword Spec),
13709 + 13800–13803 (Mace Spec), 13707 + 13966–13969 (Fist Spec), 31124 / 31126 (Blade Twisting).
Plus an exhaustive extraction of every 3–5-digit integer literal in `Ai/Class/Rogue/` and
`Mgr/Item/StatsWeightCalculator.cpp`.

**Result: exactly one live hit** (plus one false positive — `Ai/Base/Actions/BattleGroundTactics.cpp:323`
is the float coordinate `1329.33f`, not a spell id).

### DEFECT (reported, not fixed) — `Mgr/Item/StatsWeightCalculator.cpp:37` + `:711`

> **FIXED 2026-09-03 — see §17.** Kept verbatim below as the finding of record; the routed
> disposition and its runtime evidence live in §17.

```cpp
constexpr uint32 SPELL_ROGUE_SWORD_SPECIALIZATION = 13964;
...
if (cls == CLASS_ROGUE && player_->HasAura(SPELL_ROGUE_SWORD_SPECIALIZATION) &&
    (proto->SubClass == ITEM_SUBCLASS_WEAPON_SWORD || proto->SubClass == ITEM_SUBCLASS_WEAPON_AXE))
{
    weight_ *= 1.1;
}
```

The rogue Sword Specialization node ships as a **generated auto-passive** in **both** managed eras
(TBC node 20435 → 939480–939484; Vanilla node 18430 → 923440–923444), so an era-managed rogue bot
never holds aura 13964 and the sword/axe gear preference (×1.1) is **permanently lost**. Confirmed
live in the ladder run: the TBC-70 rogue's doctor shows `node 20435 rank 5: grant 939484`, and the
Vanilla-60 run shows `node 18430 rank 5: grant 923444`.

- **This is the exact warrior Poleaxe Specialization case**, three lines below in the same function,
  which patch 0021 already routes: `player_->HasAura(EraKnown(player_, SPELL_POLEAXE_SPECIALIZATION))`.
  The rogue line above it was not.
- **Pre-existing, NOT rogue-TBC-caused.** The Vanilla rogue tree (in `master`) already authors this
  node, so the check has been false for Vanilla-band rogue bots since the Vanilla rogue phase.
- **Severity: bot gear weighting only.** No player-facing effect, no crash, no wrong spell cast —
  a rogue bot values swords/axes 10% less than it should relative to other weapon types.
- **Routing it needs a `kEraNameAliases` entry as well as the bridge call** — the Commanding
  Presence lesson, and this one is worse. Stock 13960–13964's live name is **"Hack and Slash"**
  (console-verified: `.lookup spell Hack and Slash` → `13960 … 13964 - Hack and Slash, rank 1..5
  [talent] [passive]`), while the era clones are named `"Sword Specialization (Rank N)"`. So
  `EraTalentBots_ResolveSpellId(bot, 13964)` alone would still return **0**; it needs
  `{ "hack and slash", "sword specialization" }` in `EraTalentBots.cpp`'s alias table.
- **Extending patch 0021 is an orchestrator decision** — the file is already in 0021's file list
  (the Poleaxe line), so this is a one-hunk growth plus the alias, then
  `tools/regen-eraai-patch.sh` (**baseline-aware**: `Bot/PlayerbotAI.cpp` is shared with
  0003/0004/0011/0014/0016; every other 0021 file takes a pristine baseline). A doctor
  `AI resolve` discriminator for `13964` should land with it.

### Everything else is era-safe by NAME

The rogue AI resolves the clones **by name**, which the clones preserve exactly:
`Ai/Class/Rogue/Action/RogueComboActions.h:31/:37` construct `CastSpellAction(botAI, "mutilate")`
and `"riposte"`; `RogueActions.h:123` / `RogueTriggers.h:35` / `RogueAiObjectContext.cpp:80,112,113,130`
/ `DpsRogueStrategy.cpp:383` / `AssassinationRogueStrategy.cpp:16,26,91` all key on the strings
`"mutilate"` / `"riposte"` / `"adrenaline rush"`. The clones' `spell_dbc` names are verbatim
`Mutilate` / `Riposte` / `Adrenaline Rush` / `Preparation` / `Premeditation`, so the spellbook
name scan resolves them with no bridge involvement and no alias.

**Poisons are era-safe too, for a different reason:** `Ai/Base/Actions/ImbueAction.cpp` selects
poisons by **ITEM** id (`INSTANT_POISON_IX`, `DEADLY_POISON_*`, via `FindConsumable`), not by spell
id — and this phase clones no poison spell and no poison item. There is no poison handling in
`Mgr/Item/` at all.

**No rogue `AI resolve` doctor discriminators exist yet**, which is why the ladder's doctor output
has no `AI resolve ->` lines for this class. That is *correct as shipped* — there is nothing to
route until the 13964 decision lands.

## 12. Bot era fidelity

### 12a. Band-move ladder (rogue bot `Syllastor`)

`tools/wgconsole.py` drove 60 → 70 → **80** → 70 with `.eratalents doctor` at every stop and
`.raidroster syncone` at the 70 stop. Output: `/tmp/claude-1000/tbc-rogue/ladder.out`.

| Level | `era=` | `available` / `spent` | Result |
|---|---|---|---|
| 60 | Vanilla | 0 / **51** | Vanilla build; grants include `932989` (the Task-8b Riposte clone) and `923444` (Sword Spec r5) |
| 70 (+ `syncone`) | TBC | 0 / **61** | TBC build; grants include clone `947777` (Riposte), `947783` (Mace Spec r5), `939484` (Sword Spec r5) — all `known=yes spellInfo=yes`, all 10 band auras `applied=yes` |
| 80 | WotLK | **71** / 0 | `(no era talents spent this era)` — era rows torn down, native WotLK path |
| 70 | TBC | 0 / **61** | rebuilt cleanly |

**`grep -icE "ORPHAN"` = 0** at every rung, and 0 `known=NO` / `spellInfo=MISSING`. `storedEra`
reads `Vanilla` throughout, which is expected and not a defect: bot IP quest progression is 0, so
`EraFromIP` is always Vanilla and `EraTalentBots::EraFor` derives the era from the LEVEL BAND
instead (the canonical helper).

### 12b. Broad doctor sweep — **281 bots, all classes, Vanilla + TBC bands**

Every online bot at level ≤ 70 (`characters WHERE online=1 AND level<=70`, 281 rows out of 500
online), swept with `.eratalents doctor` in 8 batches of 40. Driver:
`/tmp/claude-1000/tbc-rogue/sweep.py` — the `wgconsole.py` PTY mechanism with a per-command
completion check on `== doctor done ==` (so a short pump cannot silently truncate a report).
**281 headers, 281 `doctor done`, 0 incomplete.** Aggregated: `sweep.clean`.

| Class | Vanilla-band | TBC-band |
|---|---|---|
| 1 warrior | 24 | 13 |
| 2 paladin | 23 | 6 |
| 3 hunter | 27 | 1 |
| 4 **rogue** | 28 | 12 |
| 5 priest | 25 | 1 |
| 6 DK | 4 | 6 |
| 7 shaman | 29 | 1 |
| 8 mage | 24 | 4 |
| 9 warlock | 22 | 4 |
| 11 druid | 23 | 4 |

| Signal | Count | Verdict |
|---|---|---|
| `known=NO` | **0** | clean |
| `spellInfo=MISSING` | **0** | clean |
| `ORPHAN ERA SPELL` | **127** | see below — **0 rogue, 0 rogue-phase-caused** |
| pet band aura `applied=NO` | **1** | see below |
| `AI resolve -> 0` | **431** | all explained; see below |

**Bot-build node coverage is PARTIAL and this is the phase's biggest headless blind spot.** Across
the 12 TBC-band rogue bots the sweep observed grants for exactly **39 of the 67 nodes** — the
contiguous run **20400–20419** and **20421–20439**. Nodes **20420 and 20440–20466 were never
spent by any bot**, i.e. the entire **Subtlety tab** and the tail of Combat. Cause: the era-blind
`kAuthoredOrders` (§14) makes every TBC build fall through to a greedy **node-id-ordered** fill,
which exhausts 61 points before it reaches them. Consequence for this record: **Mutilate (947773),
Preparation (947786), Premeditation (947787) and Adrenaline Rush (947778) were never granted to a
bot at all**, so nothing about them is runtime-observed here beyond the ladder's clean teardown —
they rest entirely on the real-client checklist (§15 items 3, 7, 14, 15). Clones that *were*
exercised: **947777** Riposte (8 bots) and **947783** Mace Spec r5 (16 bots), both
`known=yes spellInfo=yes` with their band auras applied.

**All 127 ORPHAN lines are hunter (class 3) and warlock (class 9), and every one is the doctor's
missing-predicate false positive on a Vanilla-phase *baseline version-swap* chain.** Zero rogue
lines, in either band.

| Ids | What | Holders | Triage |
|---|---|---|---|
| 932300–932303 (37) | Scorpid Sting Vanilla clones r1–r4 | Vanilla-band hunters | **Trainer-taught** behind marker 932304, never node-granted → the generic `NodesFor()` check can never resolve them. Same pre-existing gap class as the Vanilla Bloodthirst / Hemorrhage rows the doctor *does* have predicates for (both of which its own comments describe as predating `feat/era-talents`). |
| 932304 (27) | "Era: Vanilla Hunter" marker | all 27 Vanilla-band hunters | **Reconcile-granted era marker**, structurally never talent-granted → always reported. |
| 932922–932928 (58) | Create Firestone / Spellstone Vanilla clones | Vanilla-band warlocks | Same shape: trainer-taught behind marker 932939. |
| 932305 (1) | "Era: WotLK Hunter" marker | 1 **TBC-band** hunter | Correct *as coded*, and a deferred fidelity item — see below. |
| 932939 (4) | "Era: WotLK Warlock" marker | 4 **TBC-band** warlocks | Same. |

**The 5 TBC-band marker lines are a separate, real observation — a deferred item for the future TBC
hunter and warlock phases, not a defect of this phase.** Both version swaps are coded as an
explicit two-way `era == ERA_VANILLA` / `else`, with the code comments framing the `else` as
"a post-Vanilla (TBC/WotLK) warlock" (`EraTalents.cpp:606`) and the hunter arm granting 932305 in
the same `else` (`:798-801`). So a TBC-band hunter gets WotLK's hit-chance Scorpid Sting and a
TBC-band warlock gets WotLK's stones. That is exactly what
`docs/era-talents-tbc-reconcile-triage.md`'s hunter and warlock rows already flag as
`TBC->hunter` / `TBC->warlock` and describe as *"accidentally correct only if TBC's version matches
WotLK's; unverified"* — TBC's Scorpid Sting is in fact the pre-3.0 stat drain, so those rows will
need a third arm when their class phases run. **Recorded, correctly out of scope here.**

**The single `applied=NO`** is warlock **932901 Soul Link** on a Felhunter (pet entry 417), one bot
out of 22 warlocks and 34 registered pet band auras (the other 33 all `applied=yes`). Soul Link is
an activated buff, so an un-cast state is the likely benign explanation, but it was not chased —
warlock-phase territory, out of scope, no rogue involvement.

**The 431 `AI resolve -> 0` lines are all documented-correct zeros**, and there are no rogue
discriminators to zero (§11). Two families:

- **Node-gated** (0 is correct when the build skipped the node): 12785 Poleaxe Spec (31/37 — and the
  discriminator's own comment says 0 is *always* correct in Vanilla), 12861 Commanding Presence
  (30/37), 30706 Totem of Wrath (30/30), 16190 Mana Tide (27/30), 16864 Omen of Clarity (27/27),
  16164 Elemental Focus (25/30 — the 5 non-zeros resolve to the Vanilla clone 926440), 15286
  Vampiric Embrace (25/28), 20911 Blessing of Sanctuary (24/29), 20244 / 20042 Improved BoW / BoM
  (24 / 21), 16931 Thick Hide (19/27, 6 resolve to 925764 — proof the resolver works for druids).
- **Era-correct-by-expansion**: 8170 Cleansing Totem (30/30 — WotLK-only), 3738 Wrath of Air
  (30/30 — TBC L64+).
- **The eight shaman totem gates the comment says "must never be 0 on a totem-trained shaman"**
  were checked against level, not just counted, and every zero is an *untrained* totem: for each
  gate the highest zero-reporting level sits strictly **below** the lowest resolving level —
  Stoneclaw 5730 (7 / 8), Searing 3599 (8 / 13), Healing Stream 5394 (18 / 20), Mana Spring 5675
  (24 / 27), Nature Resistance 10595 (27 / 35), Flametongue 8227 (27 / 35), Windfury 8512
  (27 / 35), Grounding 8177 (27 / 35). No contradiction anywhere.

### 12c. The T8c bot-login reconcile

`EraTalentBots::OnBotLogin` now calls `EraTalents::ReconcileBaselineSpells` after
`ReapplyOnLogin` (commit `5903380`). `ReapplyOnLogin` only re-*grants* clones from the persisted
`era_character_talent` rows — it evaluates none of reconcile's grant-change strips, baseline gates
or marker grants — so before this a managed bot kept its stale stock spells until the next factory
reconcile or level change.

**Measured 2026-09-03 (Task 8b), on a boot right after the Riposte rework landed: 7 Vanilla-band
rogue bots still held stock Riposte 14251 (`specMask 1`) alongside the new clone 932989, and 4 held
stock 13750 with no clone. A `.character level` on one cleared it instantly.** That matters beyond
cosmetics because `EraTalentBots_ResolveSpellId`'s `HasSpell` fast path returns the **stock** id
whenever the bot still knows it — i.e. the AI casts the wrong-era spell.

**Cost: one direct call, zero DB queries.** Deliberately **not** wrapped in `BotBuildScope`: that
scope exists to collapse a factory build's ~40 per-`TryLearn` reconciles into one, there are no
`TryLearn` calls on this path, and its destructor would *add* a synchronous
`REPLACE INTO era_character_talent` plus a second reconcile per login. Reconcile itself issues no
query — it reads rank state only via `CurrentRank`, whose lazy per-`(guid,era)` SELECT was already
spent by the `TeardownStale` + `SpentPoints` calls above it. Ordering is after `ReapplyOnLogin` so
the era grants exist before the arms evaluate them (several arms are "strip X *unless* the era
clone is present"). The 281-bot sweep above ran against a worldserver carrying this change and
found no stale stock rogue spell in either band.

**WotLK-band gate: KEPT.** The reconcile call is guarded by `EraHasTalentTrees(era)`, so a
WotLK-band bot gets no reconcile at login. That is deliberate — `TeardownStale(bot, era)` runs
**unconditionally** immediately above it and is what strips era rows for an out-of-band bot, which
is the only thing a WotLK-band bot needs. The ladder's L80 rung confirms it: `available=71
spent=0`, `(no era talents spent this era)`, zero orphans.

## 13. Cross-era finding (found here, FIXED and bundled)

**Vanilla Riposte was shipping WotLK behaviour.** While resolving the TBC Riposte disposition, the
Vanilla tree's node 18423 was found to grant **stock 14251** — which on 3.3.5a is WotLK's
*slow attack + combo point* rework, not the 6-second **disarm** that both Vanilla and TBC have. A
Vanilla rogue on `master` therefore had a Riposte that did the wrong thing.

Fixed in the same phase (commits `133567a` + record `0b0c121`): a Vanilla clone **932989**
(`durationIndex` = 6000 ms, Effect_2 re-authored as aura 67 `MOD_DISARM` with `EffectMechanic 3`),
node 18423 re-pointed to it, and a grant-change strip added for the now-retired stock grant (lesson
17f — `StripOrphanedGrants` only sees CURRENT node-table ids). Verified live at the ladder's L60
rung: `node 18423 rank 1: grant 932989 known=yes spellInfo=yes`, no stock 14251 anywhere.

**This ships as part of the TBC set, not separately** — it is a Vanilla-behaviour change and
belongs in the Vanilla regression pass that gates the TBC merge. It is also why the framework
registry now records 932989 (and 932988, which was in use but had never been written down).

## 14. Known open items (not this phase's defects)

- ~~**`kAuthoredOrders` is era-blind**~~ — **CLOSED 2026-09-03** (corrected 2026-09-06 in the
  final-review fix round, item M-19). What this record observed was real at the time: every
  TBC-band rogue build logged a wall of
  `[mod-era-talents] authored build order skipped N (talent 18xxx) for <bot> tab0 — order/tree
  drift, regenerate kAuthoredOrders`, because the table held only **Vanilla** node ids
  (18401–18421 in the captured run) and every TBC bot fell through to the greedy node-id-ordered
  fill. `kAuthoredOrders` has carried **(TBC, rogue, tab 0/1/2)** entries since 2026-09-03, and
  `tools/gen_bot_builds.py` now FAILS on any manifest dataset missing orders for specTabs 0-2, so
  the condition cannot silently return. Re-observed clean: **0 drift lines**, and a bot build does
  now spend the tier-9 capstone (**20444** observed). §6's Mutilate behind-gate and §8's
  Preparation reset therefore no longer lack a bot-AI path for the same reason — see the
  final-review record for what has since been observed.
- **TBC-band hunter and warlock hold the WotLK-side version swap** (§12b) — a deferred item for
  those classes' own TBC phases; already flagged in the reconcile triage doc.
- **The doctor has no orphan predicate for the hunter Scorpid Sting / warlock stone version-swap
  chains or their era markers** (§12b) — 127 benign lines of diagnostic noise across 60 bots, the
  same gap class the doctor's own comments already document for Vanilla Bloodthirst and Hemorrhage.
  Worth closing so the sweep signal stays readable, but it is a *diagnostic* gap, not a behaviour
  one.
- **Warlock Soul Link 932901 not applied on one Felhunter** (§12b) — one instance, unchased,
  warlock-phase territory.

## 15. What remains UNVERIFIED — REAL-CLIENT CHECKLIST

Headless green is not sign-off. Nothing headless can catch the rogue-specific mechanics: bots never
stealth-open meaningfully, never apply poisons through the talent path, no bot build reached a
tier-9 capstone, and the *player* era path is untestable from the console (`.ip set` is UB from
there — it discards its player argument and acts on target-or-self).

To be run by the user on a **TBC-era rogue at level 70** with a fresh `patch-V.mpq` at generation
`f5c228df` (the handed-off `patch-V.mpq`, built 2026-09-03 13:55, matches `era_talent_meta`).
Items 1–13 are plan Task 10 Step 2; **14–17 are added by this record** to cover what
§6/§8/§13 left unobserved.

1. **Frame** — all three tabs render real parchment (`RogueAssassination` / `RogueCombat` /
   `RogueSubtlety`); tier 9 reachable in the scroll; point pool reads **61**; every talent icon
   renders (a blank button = the `classic_` icon trap, lesson 5).
2. **Poisons** — with Vile Poisons maxed, poison damage ticks match the +%; Improved Poisons
   visibly raises apply frequency; poison buffs are not dispel-trivial.
3. **Mutilate** — requires daggers; both weapons strike; the damage bonus vs a poisoned target
   matches the tooltip; trained ranks purchasable only with r1 known.
4. **Shadowstep** — teleports behind the target at the TBC cost/cooldown; the post-step damage
   rider matches the tooltip.
5. **Surprise Attacks** — Sinister Strike / Backstab / Shiv / Gouge cannot be dodged (best-effort
   observation) and the +damage applies.
6. **Stealth openers** — Master of Subtlety's damage buff shows on breaking stealth and lingers its
   TBC duration; Initiative / Improved Ambush proc as tooltipped; **Sap does NOT break stealth**
   (TBC baseline — the Vanilla `era_rog_sap_stealth` hook must be inert).
7. **Preparation / Adrenaline Rush / Cold Blood** — Preparation resets exactly the shipped TBC list
   and nothing more; AR / CB cooldowns match the shipped era values.
8. **Cheat Death** — a lethal-ish hit procs the survival effect at TBC numbers.
9. **Combat Potency** — off-hand white hits energize; main-hand hits do not.
10. **Weapon specs** — Mace Spec stun procs; Sword Spec extra attacks fire; Weapon Expertise shows
    its expertise on the character sheet.
11. **Hemorrhage** — damage + charge count per the shipped (grant-stock) disposition; trained ranks
    gated on r1.
12. **Accepted gaps read honestly** — every gap in §9 has a tooltip describing shipped behaviour.
13. **Era transition** — in-game (self-targeted!) `.ip set` to a Vanilla stage and back to 8: the
    tree resets cleanly both ways, no orphan spells (the managed→managed player path).
14. **Mutilate's +50%-vs-poisoned AND its behind-target requirement** (§6 — both UNOBSERVED).
    Compare a Mutilate hit on a poisoned vs an unpoisoned target: the ratio must be **1.50 / 1.00**,
    not WotLK's 1.20. Then face the target head-on: the cast must be **refused** with a positional
    error, and must succeed from behind.
15. **Preparation resets exactly six abilities** (§8 — UNOBSERVED): Evasion, Sprint, Vanish, Cold
    Blood, Shadowstep, **Premeditation**. Put all six plus Blind and Kick on cooldown, cast
    Preparation, and confirm those six reset and **Blind and Kick do not** (Blind is the Vanilla
    list's extra; Kick would mean the Glyph branch was reached).
16. **Riposte is a 6-second DISARM in BOTH eras** (§13). On the TBC rogue (clone 947777) *and* on a
    **Vanilla-era rogue** (clone 932989): the target's weapon must be disarmed for 6 s — no melee
    haste debuff, no combo point.
17. **Sword Specialization's extra attacks are internally cooled at 500 ms** (gap 6, `proc.cooldown:
    500`): during sustained autoattack the extra attacks must never occur more than once per 500 ms.

Carry forward the shaman fix-round's standing sweep classes (workflow §4b lesson 17) and this
phase's additions (lessons 18–20).

## 16. Real-client sign-off

**SIGNED OFF 2026-09-03 by the user at generation `f5c228df` — zero findings** against the §15
checklist (17 items, incl. the four headless-unreachable ones: Mutilate 1.50× vs poisoned + the
behind-target refusal, Preparation's exact six-ability reset, Riposte's 6 s disarm in both eras,
Sword Spec's 500 ms internal cooldown). No fix round was needed after §17 (which landed before
the pass). Phase 5 is CLOSED; TBC Phase 6 (priest) is next, entered via the workflow doc §3 —
helpers from **948024**, carry lessons 17–20 and the §3 fourteen-family + proc-row sweep. TBC still
merges only as a full nine-class set after the all-class review + Vanilla regression pass.

## 17. Fix round 2026-09-03 — the §11 by-id site is ROUTED

**Issue.** §11's single defect: `Mgr/Item/StatsWeightCalculator.cpp:711` gated the rogue sword/axe
gear preference (×1.1) on `HasAura(SPELL_ROGUE_SWORD_SPECIALIZATION /* 13964 */)`, a stock id no
era-managed rogue ever holds — so the preference was permanently dead in both managed bands.

**Root cause (two layers, not one).** Routing the check through patch 0021's bridge is necessary
but was **not sufficient**: WotLK RENAMED the talent, so stock 13960–13964 are named
**"Hack and Slash"** (console `lookup spell Hack and Slash` → 13960..13964, rank 1..5) while both
era chains keep the era-authentic name — `923440`/`923444` and `939480`/`939484` are
`Sword Specialization (Rank 1)`/`(Rank 5)` (verified in `spell_dbc`). The resolver matches by NAME,
so without a `kEraNameAliases` row it returns 0 and the routed check is *still* always false. This
is the second instance of the Commanding Presence / Improved Battle Shout shape.

**Fix (3 files, commit `679110b`).**
1. `patches/0021-playerbot-era-ai.patch` — the rogue line now reads
   `player_->HasAura(EraKnown(player_, SPELL_ROGUE_SWORD_SPECIALIZATION))`, following the Poleaxe
   Specialization line three lines below verbatim. **Rank semantics deliberately unchanged:** the
   single rank-5 id is resolved (the resolver requires `cloneRank >= stockRank`, and
   `GetSpellRank(13964) == 5`), so the bonus needs a MAXED node in every era — exactly upstream's
   own semantics. No OR over ranks 1–4, which would have given era rogues a bonus native rogues
   do not get.
2. `modules/mod-era-talents/src/EraTalentBots.cpp` — `kEraNameAliases` gains
   `{ "hack and slash", "sword specialization" }`.
3. `modules/mod-era-talents/src/EraTalentsCommand.cpp` — a `CLASS_ROGUE, 13964` doctor
   discriminator, so the tell is visible (§11 closed its own "no rogue `AI resolve` lines" note).

**Regen + apply.** `tools/regen-eraai-patch.sh` (baseline-aware) re-cut 0021; the only delta versus
the committed patch is the rogue hunk (`git diff` on the patch = 13 insertions, all in the
`StatsWeightCalculator.cpp` hunk) — no contamination from 0003/0004/0011/0014/0016, and the script's
own gates (18 files, no `perfMonEnabled`, no `wg siege`) passed. Verified by a **full ordered apply
on a pristine pinned baseline** (core `190184a0`, mod-playerbots `ba46fcde`, mod-multibot-bridge
`5e5ff759`, materialised by `git archive` of every patched path into a scratch tree): all 22
patches `0001…0024` applied cleanly in numeric order, the new expression is present exactly once,
zero bare `HasAura(SPELL_ROGUE_SWORD_SPECIALIZATION)` remain, and the resulting
`mod-playerbots/src` tree is byte-identical to the live fork worktree.

**Runtime evidence** (dev-box worldserver rebuilt + recreated; binary canary: the strings
`hack and slash`, `sword specialization`, `improved battle shout` and the doctor label
`Sword Spec r5` are all present in the shipped binary).

| bot | class/level | band | `AI resolve: 13964 -> ` |
|---|---|---|---|
| Vievelia | rogue 55 | Vanilla | **923444** (and `band aura on player: 923444 … applied=yes`) |
| Filysae | rogue 70 | TBC | **939484** (and `band aura on player: 939484 … applied=yes`) |
| Laviano | rogue 80 | WotLK | 0 — correct: **no** rogue at level ≥71 anywhere in `character_spell`
  knows 13960–13964, so upstream's own `HasAura(13964)` is equally false. Behaviour unchanged. |
| Catarista | rogue 35 | Vanilla | 0 — control: build did not take node 18430 |

The two non-zero rows are the real functional proof: the resolved clone is an *applied* aura on
that bot, so `HasAura(EraKnown(...))` is now genuinely true and the ×1.1 sword/axe preference fires.

**Fast-path + no-regression controls.** A WotLK-band L80 shaman (Julaan) still resolves stock ids
straight through (`8071 -> 8071`, `3599 -> 3599`, `8177 -> 8177`), confirming the `HasSpell` fast
path is untouched. A TBC-band warrior (Selalah) still resolves `12861 -> 938644`, so the added
alias row does not disturb the Commanding Presence alias (the loop only tries an alias whose
`liveName` equals the stock name).

**Not verified here:** the stock-13964 passthrough for a rogue specifically — no rogue at level ≥71
in this database has taken Hack and Slash to rank 5, so the fast path could not be exercised on
that id. It is class-agnostic code, unchanged by this commit, and demonstrated on the shaman rows
above. The ×1.1 weight's downstream effect on actual gear picks was not measured (it is one factor
among many in `StatsWeightCalculator`).
