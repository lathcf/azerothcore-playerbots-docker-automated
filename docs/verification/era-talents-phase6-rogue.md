# Era Talents Phase 6 — Vanilla Rogue: Verification Record

**Date:** 2026-08-17
**Branch:** `feat/era-talents` (merged to master 2026-09-07)
**Generation stamp:** `34d60a11`

## Real-client verification: PASS (2026-08-18)

The user ran the full real-client checklist (Task 10, listed near the end of this record) on a
real 3.3.5a Vanilla Rogue and confirmed everything works: canary green, the 3-tab panel (51 nodes,
per-tree backgrounds, real icons, prereq arrows) rendered correctly, and every mechanic behaved as
designed — weapon-spec procs firing only with the matching weapon equipped, Preparation's ability
reset, Hemorrhage's charge debuff, Premeditation's combo points, Initiative/Setup procs, spellbook
and buff tooltips, Improved Sap's stealth refresh, Weapon Expertise's character-sheet effect, and
the era-transition strip.

Three refinements were requested during the pass and verified live, superseding parts of the
headless record below:

1. **Hemorrhage** — reworked from the stock-16511 grant (recorded as "faithful" in Step 2 below)
   into the full 3-rank Vanilla trainer chain: clones `932985/986/987` at +3/+5/+7 stacking debuff,
   100% chance off ANY weapon (no dagger-only scaling), 30 charges/15s, +1 combo point on
   application. Trainer teaches R2 at level 46 / R3 at level 58 off rogue `TrainerId 9`, mirroring
   the hunter/warlock trainer-rank-chain pattern. Commit `b58d49a` (+ a generator `procCharges`
   scalar fix, needed to express the 30-charge count).
2. **Preparation** — now resets all 5 abilities including Blind (previously gap #3 in the list
   below: the granted stock 14185 only reset 4 of 5). Reworked to a clone (`932983`) + a new C++
   `SpellScript` (`era_rog_preparation` in `EraTalentProcScripts.cpp`) that walks all 5 cooldowns.
   Commit `6576653`.
3. **Sap** — now breaks stealth in the Vanilla era (authentic Vanilla behavior: breaks on the
   CAST, including on a resist — the `AfterCast` hook point is intentional and documented in-code,
   not a bug). This activates the previously near-inert Improved Sap (gap #5 below) — its 30/60/90%
   chance to re-stealth after Sap now does real, visible work instead of firing behind a
   stealth-immune Sap. Node 18444 (Improved Sap) was converted from a bare proc to a scripted
   misc-14 DUMMY marker read by `era_rog_sap_stealth`. Commits `6576653` + `500d50c`.

The **`era_audit.py` `DEFAULT_DATASETS` fix** (commit `5bdc63b`, described in Step 3 below) is
carried by this PASS too: rogue was silently un-audited by the bare `era_audit.py` invocation
until that commit added it, and the guard test `test_era_audit_default_datasets_match_regen` now
covers this class of omission for every future class (see `docs/era-talents-framework.md`).

**Warlock duration fixes verified in the same client pass:** Siphon Life (30s), Improved Shadow
Bolt debuff (12s), and Amplify Curse (30s) — all `DurationIndex` mapping bugs (18 vs 9 confused),
fixed in commits `b431d1e` and `6a62357`.

**Gaps carried, still accepted as designed:** Weapon Expertise's skill→expertise mapping, Mace
Specialization's vestigial "+skill" half, and Serrated Blades' per-level→flat armor-penetration
mapping (gaps #1, #2, #6 below) are deliberate documented mappings, not defects, and real-client
verification doesn't change that call. Improved Sap's baseline-moot gap (#5 below) IS resolved by
the Sap refinement above.

## Scope delivered

Full Vanilla Rogue talent tree (51 nodes, ids 18401–18451) across Assassination / Combat /
Subtlety, on the existing `mod-era-talents` pipeline. Ships with **zero core patches, zero new
C++ scripts, and only one generator change** (the equip-gate on proc/multi rows, used by the four
weapon-specialization nodes + Weapon Expertise) — the simplest class shipped so far. Three
talents (Preparation, Hemorrhage, Premeditation) rework into **grants of stock spells** rather
than clones, per the framework's "an active whose effect is script-driven or otherwise
irreproducible off stock data must be granted" rule.

## Step 1 — clean rebuild + boot (all five classes)

`tools/era-regen.sh --sync-fork` generated stamp `34d60a11` across all five class datasets.
Rebuilt `ac-worldserver` (clean compile), ran `ac-db-import` (exit 0 — the rogue SQL files
(`2026_08_17_10_era_talent_rogue_data.sql`, `2026_08_17_11_era_talent_rogue_custom_spells.sql`)
applied along with the cascading `era_talent_meta` stamp updates; no errors), then
`--force-recreate`d `ac-worldserver`.

Boot line:

```
[mod-era-talents] loaded 243 talent nodes (generation 34d60a11)
```

= 49 Mage + 47 Priest + 50 Warlock + 46 Hunter + **51 Rogue**, one generation stamp. No
`spell_proc` / `spell_script_names` / `sql.sql` errors for any of our id bands. `WORLD: World
Initialized In 0 Minutes 13 Seconds`, `... ready...`, container stayed `Up` with no crash/segfault/
assertion across the full verification session (~15+ min at the point of writing).

## Step 2 — DB integrity

```
rogue_nodes  unwired  rogue_procs  weapon_gated  rogue_scripted
51           0        19           22            0
```

| Metric | Value | Reconciliation |
|---|---|---|
| rogue_nodes (`era_talent` classId=4) | 51 | 51 ✓ |
| unwired (nodes with no `era_talent_rank`) | 0 | every node wired ✓ |
| rogue proc rows (`spell_proc` 923208–923615) | 19 | weapon-spec procs (Mace stun, Sword extra-attack, plus Setup/Initiative/Improved Sap-style triggers) across the tree |
| weapon_gated (`spell_dbc` 923208–923615, `EquippedItemClass=2`) | 22 | Dagger Spec 5 + Fist Weapon Spec 5 + Sword Spec 5 + Mace Spec 5 + Weapon Expertise 2 = 22 ✓ |
| rogue_scripted (`spell_script_names`, rogue's own bands) | 0 | **see note below** |

**`rogue_scripted` note — the plan's literal query range needs a caveat.** The plan's query
(`spell_id BETWEEN 923208 AND 933000`) spans **all five classes'** custom-id bands, not just
rogue's; run literally it returns **16**, but every one of those 16 rows is a pre-existing
non-rogue script (`era_soul_link`, four `era_firestone_*`, three `era_spellstone_absorb`,
`era_demonic_sacrifice`, `era_vampiric_embrace` — all warlock — plus `era_wyvern_sting` and
`spell_hun_bestial_wrath` — hunter). Re-querying against rogue's **actual** bands (core
923208–923615 AND helper 932983–932998) both return **0** — confirming the "actives were granted,
not cloned" claim holds exactly as stated. Recorded both the literal-query number (16, all
attributable to other classes) and the correct scoped number (0) for the record.

### Full-spec headless pass

**Bot selection note:** the first candidate (`Elern`, guid 214) was found carrying residue from
earlier Tasks 6–8 dev testing — 6 orphan custom spells (`923210/923241/923265/923276/923281/
923306`) in `character_spell` with **zero** backing `era_character_talent` rows (an incomplete
manual cleanup from authoring, not a pipeline bug — `era_character_talent` was already empty). To
avoid conflating pre-existing contamination with this task's fresh verification, switched to
**`Artemard`** (guid 34, level 80, verified 0 `era_character_talent` rows / 0 custom `character_spell`
rows before starting).

- `ip set Artemard 1` → clean baseline: `era=Vanilla storedEra=WotLK available=71 spent=0`, no
  orphans, no pet auras.
- Learned **15 nodes across all three tabs** (partial and maxed ranks mixed, to also exercise the
  rank-cap ceiling): Assassination `18401`(3/3) `18402`(2/2) `18403`(5/5) `18409`(2/5); Combat
  `18416`(2/3) `18417`(1/2) `18418`(3/5) `18420`(5/5) `18423`(1/1); Subtlety `18435`(3/5)
  `18436`(2/5) `18437`(2/2) `18438`(2/2) `18439`(1/5) `18440`(3/3).
- **Five deliberate reject probes**, all fired with the correct message:
  1. Max-rank ceiling on `18401` (4th rank attempt) — rejected.
  2. `prereqTalentId` gate: `18409` (Lethality, requires `18403` maxed) attempted at `18403`=3/5 —
     rejected (`requires 10 points in tree` — both the points gate and the unmaxed prereq talent
     were unmet simultaneously).
  3. `prereqPoints` gate at 10: `18423` (Riposte, prereq `18420` + gate 10) attempted at
     cum=8 — **`Learn rejected: requires 10 points in tree`**.
  4. `prereqPoints` gate at 30: `18415` (Vigor) attempted at cum=12 — rejected.
  5. Combined gate at 30: `18451` (Premeditation, gate 30 + prereq `18447` unlearned) attempted at
     cum=13 — **`Learn rejected: requires 30 points in tree`**.
- **Point accounting never negative, monotonic:** final doctor —
  **`available=34 spent=37`** (71 − 37 = 34, exact match to the manual tally of the 15 learned
  nodes' rank costs: 3+2+5+2 + 2+1+3+5+1 + 3+2+2+2+1+3 = 37).
- **`eratalents doctor` clean at the partial spec:** all 15 learned nodes report `known=yes
  spellInfo=yes`, with grants resolving correctly to both stock ids (14148 Remorseless Attacks R2,
  14142 Malice R5, 13732 Improved Sinister Strike R1, 14251 Riposte R1) and custom clones (923210,
  923273, 923329, 923346, 923364, 923482, 923489, 923497, 923505, 923512, 923522). No orphan/missing
  spellInfo. Doctor also surfaced a structural `spell_proc` dump on the Initiative grant (`flags=16
  hitMask=0 typeMask=0 phaseMask=2 chance=75`), confirming the proc row resolves correctly off the
  learned node.
- **`eratalents reset Artemard`** → `available=71 spent=0`, "no era talents spent this era", 0
  residual band auras, 0 pet auras.
- **Cleanup:** confirmed `era_character_talent` = 0 rows for guid 34 post-reset; restored the bot's
  era via `ip set Artemard 13` (WotLK) to leave the dev box clean.

## Per-mechanic headless evidence (summarized from Tasks 6–8, not re-derived here)

- **Combo-point / energy procs** (Task 6): Seal Fate (18414) → 14189 fires on an effect-80
  finishing-move crit (structural `spell_proc` row confirmed at generation time); Relentless
  Strikes (18407) → 14181 energize on finisher confirmed via the same generator-level proc dump.
  This task's own headless pass additionally observed Initiative's (18440) proc row resolving
  cleanly through `eratalents doctor` (see above).
- **Weapon-gate enforcement** (Task 7): proven **structurally**, not by live combat — `spell_dbc`
  rows for Dagger/Fist/Sword/Mace Specialization + Weapon Expertise all carry the correct
  `EquippedItemClass=2` + `EquippedItemSubclass` bitmask (per weapon type), and the core's
  `Aura::CheckProc` (`SpellAuras.cpp:2237`) checks the ATTACKING hand's weapon subclass against
  that mask — the mechanism a proc-vs-weapon-type gate needs is confirmed present and wired to the
  correct masks. Actual proc-firing-only-with-matching-weapon is a Task 10 live check (below).
- **Reworked actives GRANTED, not cloned** (Task 8): `Preparation` (18447) grants stock **14185**
  — script-driven (`spell_rog_preparation`, hardcoded C++ `OnEffectHitTarget` cooldown-reset walk);
  a clone would silently lose the script and become an inert DUMMY shell. `Hemorrhage` (18449)
  grants stock **16511** — verified (via DBC dump, not the task's original premise) to already be
  the correct charge-based flat-physical-damage-taken debuff design in 3.3.5a, so the grant is
  fully faithful, not a compromise. `Premeditation` (18451) grants stock **14183** — data-driven
  (ADD_COMBO_POINTS + RETAIN_COMBO_POINTS + stealth ShapeshiftMask gate), correct 20yd range.

## Helper-id usage reality

Rogue's reserved helper band is **932983–932989 + 932992–932998**. Of that, only
**`932988` (Mace Specialization's authored 3-second stun)** was actually used — confirmed by
reading the dataset's `helpers:` block (a single entry). The three reworked actives were granted
rather than cloned (see above), so **932983–932987, 932989, and 932992–932998 remain unused/free**.
Noted for the record only — the framework's band reservation for rogue is unchanged; this is not
a request to shrink it.

> **SUPERSEDED (2026-09-03).** This section's "only 932988 was used" snapshot no longer holds:
> 932983 (Preparation Vanilla clone) and 932985–932987 (Hemorrhage r1–r3) shipped in later Vanilla
> rounds, and the 2026-09-03 fix round claimed **932989** for the Vanilla Riposte clone (see the
> fix-round section below). For the current Vanilla-rogue helper ledger read
> `era-data/_ref/rogue-spells.yaml` (its header id table) and the framework registry row; the TBC
> rogue tree's own helpers are a separate slice recorded in
> `docs/verification/era-talents-tbc-phase5-rogue.md`.

## Named tracked gaps / documented mappings (carried from Tasks 6–8)

All named-in-YAML (grepped for `DEVIATION`/`DOCUMENTED MAPPING`/`TRACKED`/`moot`/`no Blind`), none
silently shipped:

1. **Weapon Expertise (18432) — skill→expertise, aura 240, DOCUMENTED MAPPING.** Vanilla's
   +3/+5 weapon **skill** with Swords/Fist/Daggers has no live analog in 3.3.5a (weapon skill was
   removed as a combat stat — a skill-delta aura would be a silent no-op). Mapped to
   `SPELL_AURA_MOD_EXPERTISE` (aura 240, the WotLK functional successor, confirmed live by stock
   30919/30920) at Vanilla's own +3/+5 magnitude (not stock's +5/+10), equip-gated to the same
   three weapon types. Tooltip still reads "skill" (Vanilla text); delivered effect is expertise.
2. **Mace Specialization (18428) — "+N skill with Maces" half is vestigial.** The 3-second stun
   is authored from scratch (helper 932988, since WotLK's stock Mace Spec chain has no
   stun/trigger at all). The accompanying "+N weapon skill" tooltip clause maps to nothing —
   weapon skill has no live combat effect on this core, so even stock behavior would be a no-op;
   only the stun is functionally reproduced.
3. ~~**Preparation (18447) — no Blind reset (4 of 5 abilities).**~~ **CLOSED 2026-08-18**
   (status corrected here 2026-09-06, final-review item M-45). As recorded, granted stock 14185 is
   script-driven (`spell_rog_preparation`) and resets Cold Blood/Shadowstep/Vanish/Evasion/Sprint
   while Vanilla's set also reset **Blind**. The premise that "a clone can't carry the C++ script
   logic" is what turned out to be wrong: the module can ship its OWN script. Node 18447 now grants
   the era clone **932983** (template 14185 at Vanilla's 10-min cooldown) with the module SpellScript
   **`era_rog_preparation`** bound to it, which replays the stock reset mask OR'd with Blind's
   `flags[0]` bit `0x01000000` — so all FIVE Vanilla abilities reset. An EraTalents.cpp CLASS_ROGUE
   arm strips a stale stock 14185 off a Vanilla rogue.
4. ~~**Premeditation (18451) — 20s combo-retention window vs Vanilla's 10s.**~~ **CLOSED
   2026-09-06** (final-review item G-39). Stock 14183's only divergence from Vanilla is
   `DurationIndex 18` (20s) instead of Vanilla's 10s, and the clone was originally rejected because
   its `client:` block could not be verified headlessly. The TBC slice has since shipped exactly
   that clone (**947787**) with a real-client-verified tooltip, so the objection no longer holds:
   node 18451 now grants the Vanilla clone **932984** (template 14183, `durationIndex: 1` =
   10 000 ms), mirroring 947787 field for field — the stealth gate, range and identity bit all
   inherit from the template. An EraTalents.cpp CLASS_ROGUE arm strips a stale stock 14183.
5. ~~**Improved Sap (18444) — Sap doesn't break stealth on this core, so the benefit is
   near-moot.**~~ **CLOSED 2026-08-18** (status corrected here 2026-09-06, final-review item M-45).
   The recorded design — a proc giving a 30/60/90% chance to re-trigger Stealth 1784 — was indeed
   near-inert, because Sap on this core carries `ATTR1_ALLOW_WHILE_STEALTHED` and never breaks the
   rogue's own stealth, so the proc only ever refreshed an already-active stealth. The node was
   reworked to `mechanic: scripted`: it mints a band-gated hidden **DUMMY marker (misc 14)** per
   rank carrying 30/60/90, and the module SpellScript **`era_rog_sap_stealth`** — bound to stock Sap
   2070/6770/11297/51724 and era-gated to Vanilla — makes Sap BREAK stealth and then reads the
   misc-14 amount off the caster as the chance to REMAIN stealthed (0, i.e. always break, without
   the talent). The script owns the whole break/keep decision, so the old proc is gone entirely and
   there is no double-apply or ordering hazard. WotLK/TBC rogues' Sap is untouched.
   *(The retired proc's three `spell_proc` rows 923552-923554 were finally deleted 2026-09-06 by
   `2026_09_06_01_era_talent_final_review_rogue.sql` — the generator only DELETEs ids it re-inserts.)*
6. ~~**Serrated Blades (18445) — per-level armor-ignore → flat armor-penetration%.**~~ **CLOSED
   2026-09-06** (final-review item G-38). The Rupture damage-bonus half (SPELLMOD_DOT +10/20/30%,
   stock 14171–14173) was and is Vanilla-exact. The armor half was mapped to a flat +3/6/9%
   `MOD_ARMOR_PENETRATION_PCT` on the premise that 3.3.5a has **no per-level-scaling aura
   primitive**. That premise was false, and TBC's own row is the counter-example: the mechanic is
   **aura 123 `SPELL_AURA_MOD_TARGET_RESISTANCE`** with `EffectMiscValue` 1
   (`SPELL_SCHOOL_MASK_NORMAL` = armor) and the magnitude on the FLOAT column
   `EffectRealPointsPerLevel`, which `Unit::CalcArmorReducedDamage` adds to the victim's armor. The
   node now ships `perLevel: [-1.67, -3.34, -5.0]` with `basePoints` 0 — literally "ignore N armor
   per level" — yielding −100/−200/−300 armor at the Vanilla cap of 60 and scaling down correctly
   for a levelling rogue. Same encoding as the TBC sibling node 20455.

## Cosmetic items (non-blocking, from code review — no functional impact)

1. **Comment-placement style differs from the other four class files.** `rogue.yaml`'s authoring
   comments are structured slightly differently (placement/verbosity) than mage/priest/
   warlock/hunter's. Output is byte-identical either way; a cross-class comment-style consistency
   pass is optional cleanup, not a defect.
2. **Improved Kidney Shot (18413) was authored as a `mechanic: spellmod`** bound to stock Kidney
   Shot (408) rather than expressed as a plain grant — functionally identical either way since the
   node is Vanilla-exact and fully expressible off stock data (no gap). Noted only because a grant
   would have worked equally well here; the chosen authoring style carries zero functional
   difference.

## Step 3 — lint + audit + host tests + regen idempotency

- Initial run of `pytest tools/test_gen_era_talents.py tools/test_import_daribon_talents.py -q`
  found **1 failure**: `test_era_audit_default_datasets_match_regen` — `tools/era_audit.py`'s
  `DEFAULT_DATASETS` list (a hand-maintained mirror of `era-regen.sh`'s dataset list) had **not**
  been updated to include `era-data/vanilla/rogue.yaml`. Since every documented invocation of
  `era_audit.py` in the plans/docs is bare (no args), this meant the audit tool was **silently
  skipping rogue entirely** while still reporting `0 finding(s)` — the exact false-clean scenario
  the guard test exists to catch (its own docstring cites the identical historical miss for
  warlock). Confirmed rogue's data was never the problem: running `era_audit.py` with all five
  dataset paths passed explicitly also returned `0 finding(s)` — rogue passes every audit predicate
  on its own merits, the tool just never reached it by default.
  **Fix:** added `era-data/vanilla/rogue.yaml` to `DEFAULT_DATASETS` (matching hunter.yaml's exact
  form/placement), committed separately as `5bdc63b` *before* this record, per the coordinator's
  explicit direction that this was in-scope (a required tooling-wiring fix, not "content").
- Re-ran the suite after the fix: **98 passed** (was 97 passed / 1 failed).
- Re-ran `tools/era_audit.py` bare (no args): **`era_audit: 0 finding(s)`** — now genuinely
  covering all 5 datasets (`len(DEFAULT_DATASETS) == 5`, rogue.yaml confirmed present), not a
  silent skip.
- `tools/era-regen.sh` re-run → **idempotent**: same stamp `34d60a11`, `git status --short` clean
  (no diff).

## Status

**Headless: PASS.** Boot (243 nodes @ `34d60a11`, stable, no crash), DB integrity (51/0/19/22/0,
with the `rogue_scripted` literal-query caveat explained and the correctly-scoped number
confirmed 0), partial-spec headless pass (point accounting exact and monotonic, 5/5 reject probes
correct — max-rank cap, `prereqTalentId` gate, `prereqPoints` gates at 10/30, combined gate — full
doctor clean, reset clean), lint/audit/regen-idempotency all green (98/98 after the
`era_audit.py` `DEFAULT_DATASETS` fix). No content defects found; six named tracked
gaps/documented mappings all carried from Tasks 6–8, all harmless and previously reasoned; two
cosmetic (non-functional) items noted for optional cleanup.

**Tooling fix shipped alongside this record:** `tools/era_audit.py`'s `DEFAULT_DATASETS` now
includes rogue (commit `5bdc63b`) — the bare-invocation audit gate now actually covers all five
shipped classes.

## DEFERRED TO REAL-CLIENT PASS (Task 10)

Runtime behaviors headless verification cannot prove — require a live 3.3.5a Vanilla Rogue with
the fresh `patch-V.mpq` + restaged `EraTalents` addon:

1. **Canary green** at login (server + client both `34d60a11`; no red generation-mismatch
   warning).
2. **3-tab panel** renders with per-tree backgrounds, all 51 nodes positioned, prereq arrows, real
   icons.
3. **Weapon-spec procs fire only with the matching weapon equipped:** Sword Specialization →
   extra attack only with a sword in hand; Mace Specialization → 3s stun only with a mace;
   Fist Weapon Specialization's effect only with fist weapons; Dagger Specialization only with a
   dagger. Headless proved the structural gate (`EquippedItemClass`/`Subclass` + `Aura::CheckProc`);
   only live combat with each weapon type proves the fire.
4. **Preparation's 5(4)-ability cooldown reset** firing correctly on cast (Cold Blood/Shadowstep/
   Vanish/Evasion/Sprint reset; Blind intentionally NOT reset — confirm this reads as expected in
   play, not a bug report).
5. **Hemorrhage's charge debuff** landing and consuming charges correctly on subsequent physical
   hits (30 charges / 15s per the DBC dump).
6. **Premeditation's +2 combo points** applying from stealth, and the 20s retention window
   (Vanilla-lenient vs the 10s original) not causing any player-visible confusion.
7. **Initiative / Setup combo-point procs** firing on their trigger conditions in live combat
   (headless only confirmed the `spell_proc` rows resolve structurally through doctor).
8. **Cast-bar visuals & tooltips:** era spellbook tooltips (Description) and buff tooltips
   (AuraDescription) read era text; spellbook collapses to highest rank with "Rank N" label in the
   correct tab.
9. **Improved Sap's re-stealth proc** — confirm it is a harmless no-op/refresh in the common case
   (Sap no longer breaks stealth on this core) and does real work only when un-stealthed at Sap
   time, matching the documented deviation.
10. **Weapon Expertise's expertise (not skill) effect** shows correctly on the character sheet with
    the mapped weapon types equipped.
11. **Era-transition strip on a real player** (the same bot-hook-bypass limitation documented for
    every prior class applies here too — bots never fire `EraTransition::Detect`, so this can only
    be validated on a real player).

Branch merged to master 2026-09-07.

---

## FIX ROUND — 2026-09-03: Riposte shipped the WotLK spell (node 18423)

Found by the TBC Combat-tab survival sweep (Phase 5 Task 7), which recorded it as a cross-era
report rather than fixing it out of scope. User-approved to bundle with the TBC branch. Generation
stamp after the fix: **`5d69f170`** (752 nodes). Commit `133567a`.

**Issue.** Node 18423 (Riposte) granted **stock 14251**. A Vanilla-era rogue therefore received
WotLK 3.0.2's Riposte — a **30-second, -20% melee attack-speed slow** plus a **free combo point** —
behind a panel tooltip promising a 6-second disarm. The talent's whole point (the disarm) was
absent.

**Root cause — a wrong RATIONALE, not a wrong lookup (lesson 15).** The Task-8 bind verdict read:
"Stock Riposte is unchanged Vanilla->WotLK (150% weapon damage, 6s disarm, becomes active after a
parry)." The same comment recorded `DurationIndex 9` off the same DBC row — 30000 ms — and still
concluded "6s disarm". The dumped value contradicted the sentence it was written under, and the
decision (grant an active castable) looked right, so review passed it. `era-data/_ref/rogue-spells.yaml`
`grants.riposte` carried the same claim; its header now warns that resolving a stock id is step one,
not a survival verdict, and names the two other grants (Preparation, Adrenaline Rush) that were
later re-checked and replaced by clones for the same reason.

Raw live values (`azerothcore-wotlk/ip-dbc/Spell.dbc`, re-dumped 2026-09-03):

| Slot | Live 14251 (WotLK) | Vanilla 1.12.1 |
|------|--------------------|----------------|
| Effect_1 | 31 `WEAPON_PERCENT_DAMAGE`, bp 149 / die 1 = 150% | same |
| Effect_2 | 6 `APPLY_AURA`, aura **138** `MOD_MELEE_HASTE`, bp -21 / die 1 = **-20%**, `EffectMechanic_2` = **8** `MECHANIC_SLOW_ATTACK`, `DurationIndex` **9** = 30000 ms | aura **67** `MOD_DISARM`, `EffectMechanic` **3** `MECHANIC_DISARM`, `DurationIndex` **32** = 6000 ms |
| Effect_3 | 80 `ADD_COMBO_POINTS`, bp 0 / die 1 = **+1 combo point** | **absent** |

Vanilla evidence: `era-data/_skeletons/vanilla-rogue.skeleton.yaml` node 18423 (the Daribon 1.12.1
import that seeded this dataset) reads verbatim "10 Energy 5 yd range Instant 6 sec cooldown A
strike that becomes active after parrying an opponent's attack. This attack deals 150% weapon
damage and disarms the target for 6 sec." — which is also the node's own shipped `tooltip:`. Every
header number in that line matches stock 14251 exactly (ManaCost 10 / PowerType 3 energy /
RangeIndex 2 / CastingTimeIndex 1 / RecoveryTime 6000), so the ONLY divergences are Effect_2, its
EffectMechanic, the DurationIndex, and the presence of Effect_3. `DurationIndex` 32 = 6000 ms was
read out of the live client table (`SpellDuration.dbc` pulled from `ac-worldserver`: 32 -> 6000,
9 -> 30000).

**Fix.** Vanilla helper clone **932989** (`era-data/vanilla/rogue.yaml`), `template: 14251` so the
parry gate (`CasterAuraState 1` = `AURA_STATE_DEFENSE`), energy, range, instant cast, 6-second
cooldown, GCD category, attributes and icon are all inherited; `durationIndex: 32`; Effect_1 kept
byte-identical; Effect_2 replaced with aura 67 `MOD_DISARM` carrying the **per-effect** `mechanic: 3`
(without it the clone inherits live's `EffectMechanic` 8, so a disarm-immune target would be tested
for slow-attack immunity and the disarm would land in `DIMINISHING_SLOW_ATTACK` instead of
`DIMINISHING_DISARM`); Effect_3 dropped. `client.description` + `client.auraDescription` authored
(lesson 17d) — verified in the merged client `Spell.dbc`, see evidence below. `client.skillLine: 38`
(Combat) so the granted active does not land in the General spellbook tab. Values are identical to
the TBC clone 947777 shipped at Task 7 (TBC did not change Riposte either).

Node 18423 now grants 932989; `EraTalents.cpp`'s `CLASS_ROGUE` reconcile arm gained the lesson-17f
grant-change strip, in the exact shape of the Preparation 14185 / Adrenaline Rush 13750 strips
that sit immediately above it:

```cpp
if (era == ERA_VANILLA && p->HasSpell(14251))
    p->removeSpell(14251, SPEC_MASK_ALL, false);
```

Not readiness-gated (unlike `kEraRogTbcStockSwaps`): the Vanilla tree is long since live and in the
implemented-era allowlist, so node 18423 always has a replacement to hand over — there is no
display-only window in which stripping could leave a rogue with neither version. The TBC side of
the same swap was already covered by the `{20428, 14251}` row in `kEraRogTbcStockSwaps`; it was not
touched.

**A second wrong rationale, caught in self-review and corrected.** The first draft of the 932989
comment copied the TBC clone's framing and claimed the inherited `SpellClassMask_2` 256 identity bit
"is what lets the era Lethality/Aggression spellmods keep binding". False for this tree, and
measured: `SELECT ... FROM spell_dbc WHERE SpellClassSet=8 AND (EffectSpellClassMask{A,B,C}_2 & 256)`
on the live world DB returns **only** the five TBC Find Weakness helpers 947768-947772 — no
Vanilla-band rogue spell binds word-2 bit 8 at all. Vanilla Lethality (node 18409) authors its mask
from `affects:` NAMES (Sinister Strike / Gouge / Backstab / Ghostly Strike / Hemorrhage), which is
Riposte-free and correct for 1.12.1; Aggression's mask is word-1 only. The bit is kept because it is
the faithful inheritance, not because anything needs it. While verifying this, a pre-existing `_ref`
imprecision surfaced and was corrected in place: `spellmodPrecedents.Lethality` glossed word-2 bits
[1,2,8] as "Mutilate etc." when bit 8 is **Riposte** (that same file's `spells:` table says so). The
conclusion was right, the label was wrong, and a wrong label is what stops the next reader checking.

### Verification evidence (headless, dev box)

* `tools/era-regen.sh --sync-fork` -> generation `5d69f170`; merged client patch reports
  `+809 custom client spell rows` / `+1 custom SkillLineAbility rows` (the one new row is 932989).
* `era_audit.py`: **0 findings**. Generator tests: **201 passed**.
* Generated diff is minimal and Vanilla-only — `era_talent_rogue_data.sql` one line
  (`(18423,1,14251)` -> `(18423,1,932989)`), `era_talent_rogue_custom_spells.sql` +3/-0 (the 932989
  DELETE+INSERT), `RogueVanilla.lua` one line (`grant={ [1]=932989 }`), plus the meta stamp. **No TBC
  file and no other class changed.**
* No generated SQL writes stock 14251, and `SELECT COUNT(*) FROM spell_dbc WHERE ID=14251` = **0** —
  stock Riposte is served purely from the untouched DBC, so WotLK-stage characters are unaffected by
  construction.
* Rebuild + `ac-db-import` + `--force-recreate ac-worldserver`: **clean boot**,
  `loaded 752 talent nodes (generation 5d69f170)`, no `sql.sql`/`spell_proc` errors, no
  script-not-assigned warning for the rogue scripts. (The pre-existing `authored build order
  skipped ... regenerate kAuthoredOrders` warnings are workflow lesson 10, unrelated.)
* DB after import: `era_talent_rank(18423,1)` = **932989**; `spell_dbc` 932989 = `DurationIndex 32,
  Effect_1 31, Effect_2 6, Effect_3 0, EffectAura_2 67, EffectMechanic_2 3, ManaCost 10,
  RecoveryTime 6000, CasterAuraState 1`.
* Merged **client** `Spell.dbc` (extracted back out of `patch-V.mpq`) row 932989:
  `Description_Lang_enUS` = "A strike that becomes active after parrying an opponent's attack.  This
  attack deals $s1% weapon damage and disarms the target for $d.",
  `AuraDescription_Lang_enUS` = "Disarmed." — and the merged `SkillLineAbility.dbc` carries
  `932989 -> SkillLine 38`.
* No core script or by-id correction is lost: `spell_script_names` has no row for 14251 (live table
  queried), `SpellInfoCorrections.cpp` has no entry for 14251 (grepped), and `spell_required` /
  `spell_ranks` have **no** rows referencing 14251 — so `removeSpell(14251)` has no recursive
  collateral.
* Strip safety: 14251 has **no `trainer_spell` row** (live table queried) and
  `SkillLineAbility` 7816 has `AcquireMethod 0`, so a Vanilla rogue can never legitimately hold it.
* **Live bot, rogue `Reno` (guid 364), `.character level` walked across all three bands** (doctor +
  `saveall` after each):
  * **L60 (Vanilla)** — `era=Vanilla spent=51`, `node 18423 rank 1: grant 932989 known=yes
    spellInfo=yes`, **zero orphans**. `character_spell`: `932989 specMask 255`, `14251 specMask 0`
    (the strip fired; the row persists at 0 rather than being deleted because 14251 is a talent
    spell — `Player::removeSpell` keeps talent rows, Player.cpp:3531).
  * **L70 (TBC)** — `era=TBC spent=61`, `node 20428 rank 1: grant 947777`, **zero orphans**; the
    Vanilla band is fully torn down (`932989` row **gone** from `character_spell`,
    `era_character_talent` holds only `eraId 1` rows).
  * **L80 (WotLK)** — `era=WotLK spent=0`, "(no era talents spent this era)", zero era rows, both
    clones gone; native talent frame restored.
* **WotLK-band untouched (positive control).** No rogue bot in the world currently takes native
  Riposte, so a direct "L80 keeps 14251 at specMask 1" observation was not available. The equivalent
  control is decisive: **15 rogues at level 71/80 hold stock Adrenaline Rush 13750 at
  `specMask <> 0`**, i.e. an `era == ERA_VANILLA && HasSpell(X)` strip of the identical shape
  (the arm two lines above the new one) demonstrably does not reach out-of-band characters.

### Finding for the orchestrator — PRE-EXISTING, not introduced here

A **plain bot login** does not run `EraTalents::ReconcileBaselineSpells`: `EraTalentBots::OnBotLogin`
(`EraTalentBots.cpp`) calls `TeardownStale` + `ReapplyOnLogin` only. So a Vanilla-band bot whose era
rows already record node 18423 gets the clone re-granted at login while the stale **stock** spell is
not stripped until its next factory reconcile (randomize / level change / bracket move), which runs
the reconcile at `BotBuildScope` scope end. Measured after the fix boot: 7 Vanilla-band rogue bots
still held `14251 specMask 1` alongside `932989 specMask 255`, and `.character level` on one of them
cleared it immediately. **This is not specific to Riposte** — the same is true of the shipped 14185 /
13750 strips (4 Vanilla-band rogues hold `13750 specMask 1` with no era clone at all, having never
run an era build). It matters because `EraTalentBots_ResolveSpellId` takes a `HasSpell` fast path, so
a bot holding both would use the stock id until the next reconcile. Fixing it means adding a
reconcile to the bot login path, which is a design decision on a 500-bot hot path — left to the
orchestrator, deliberately not changed here.

### Still requires the real client

The panel/spellbook text and the disarm itself (a debuff on a live target, and its DR group) cannot
be proven headlessly. Add to the real-client pass: node 18423's panel tooltip and the 932989
spellbook tooltip read the Vanilla prose ("disarms the target for 6 sec", no combo-point line), the
debuff tooltip reads "Disarmed.", the spell appears in the **Combat** spellbook tab, and casting it
after a parry disarms the target for 6 seconds and awards **no** combo point.
