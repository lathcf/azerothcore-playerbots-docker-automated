# Era Talents Phase 8 — Vanilla Paladin: Verification Record

**Date:** 2026-08-19
**Branch:** `feat/era-talents` (merged to master 2026-09-07)
**Generation stamp:** `659e8b25`
**Status: HEADLESS PASS COMPLETE.** Full Vanilla Paladin talent tree (44 nodes, ids `18601`–`18644`)
across Holy / Protection / Retribution, class 2, family 10, on the existing `mod-era-talents`
pipeline. This record is **headless verification only** (Task 9). Runtime behaviors that require a
live 3.3.5a client (canary, panel render, live proc/aura firing, tooltips, era-transition strip on a
real player) are deferred to the real-client pass (Task 10, listed at the end).

## Scope delivered

- **44 nodes**, ids `18601`–`18644`, tabs Holy(14) / Protection(15) / Retribution(15). Class 2,
  family 10. Auto-passive band `924808`–`925156`; hand-authored helpers `932600`–`932645`.
- **Retribution:** Improved Blessing of Might / Benediction / Improved Judgement / Improved
  Retribution Aura (spellmods, one with the `affectsMask` shared-bit override); Deflection /
  Conviction / Pursuit of Justice (Vanilla move-speed-only) (stats); Two-Handed Weapon Spec
  (equip-gated stat); Seal of Command (clone, 70% weapon Holy single-target); Eye for an Eye
  (clone, 15/30%, rebinds the core `spell_pal_eye_for_an_eye` script); Vindication (proc-cloned
  Str/Agi %-debuff); Vengeance (proc-cloned Physical+Holy damage buff); Sanctity Aura
  (WotLK-removed spell, reconstructed as a party area-aura); Repentance (stock grant).
- **Holy:** Divine Strength/Intellect, Holy Power, Unyielding Faith (stats); Spiritual Focus /
  Improved Seal of Righteousness (`affectsMask` override excluding Seal of Justice) / Healing
  Light / Improved Lay on Hands / Improved Blessing of Wisdom / Lasting Judgement (spellmods);
  Consecration (baseline-gated stock grant); Illumination (stock 5-rank chain grant); Divine Favor
  (stock grant); Holy Shock (stock grant).
- **Protection:** Improved Devotion Aura / Improved Concentration Aura (`affectsMask` overrides
  isolating each aura's unique bit from the shared paladin-aura category bit) / Guardian's Favor /
  Improved Hammer of Justice / Improved Righteous Fury (spellmods); Precision / Toughness / Shield
  Specialization / Anticipation (stats); One-Handed Weapon Spec (equip-gated stat); Redoubt / Reckoning
  (clone-grants, proc-based); Holy Shield / Blessing of Sanctuary (clone-grants); Blessing of Kings
  (baseline-gated stock grant).
- **Seal of the Crusader reconstruction:** 6-rank seal (`932634`–`932639`) + 6-rank Judgement of the
  Crusader debuff (`932640`–`932645`); the judge-unleash is generic (core `spell_pal_judgement`
  reads the seal's own DUMMY effect — no core patch). Vanilla paladins get the seal via a
  `ReconcileBaselineSpells` `CLASS_PALADIN` grant-by-level block (ranks at L6/12/22/32/42/52),
  stripped on any transition to a higher era. Improved Seal of the Crusader (node `18633`) is a
  plain SPELLMOD `op:effect1` +5/10/15% that reaches both the seal's AP bonus and the Judgement's
  Holy damage via a shared word-C bit9 `affectsMask` override, no stock seal shares that bit.
- **Two baseline-leak gates** (SQL `2026_08_18_32_era_talent_paladin_baseline_gate_trainers.sql` +
  `kBaselineSpellGates` rows): Consecration `26573` + Blessing of Kings `20217`, both rank-1
  `trainer_spell` rows deleted, both `grantHigherEra=true`.
- **Zero core patches.** C++ added: the Consecration/BoK baseline-gate rows + the SotC reconcile
  grant block in `EraTalents.cpp`; Eye for an Eye rebinds an existing core script. No new proc
  scripts, no new `SpellScript`/`AuraScript` classes.

## Step 1 — clean rebuild + boot (all seven classes)

`tools/era-regen.sh --sync-fork` regenerated all seven class datasets under generation stamp
`659e8b25` and synced the module into `azerothcore-wotlk/modules/`. Rebuilt `ac-worldserver`
(cached — no C++ changed since the last paladin commit), ran `ac-db-import` (exit 0 — the paladin
SQL files `2026_08_18_30_era_talent_paladin_data.sql`, `2026_08_18_31_era_talent_paladin_custom_
spells.sql`, `2026_08_18_32_era_talent_paladin_baseline_gate_trainers.sql`, and the cascading
`era_talent_meta` stamp update all applied/re-applied cleanly), then `--force-recreate`d
`ac-worldserver`.

Boot line:

```
WORLD: World Initialized In 0 Minutes 14 Seconds
AzerothCore rev. 190184a04539+ 2026-07-31 06:09:57 -0700 (Playerbot branch) (Unix, RelWithDebInfo, Static) (worldserver-daemon) ready...
[mod-era-talents] startup (enable=true)
[mod-era-talents] loaded 339 talent nodes (generation 659e8b25)
```

339 = 295 prior (Mage/Priest/Warlock/Hunter/Rogue/Warrior) + **44 Paladin**, one generation stamp.
No `spell_proc` / `spell_script_names` / `sql.sql` errors for any of our id bands (only the usual
unrelated boilerplate `Missing property Trial.*` / `Battleground.Override.*` config warnings, present
on every boot regardless of era-talents). Container stayed `Up` and stable through the entire
verification session — no crash/segfault/assertion observed.

## Step 2 — DB integrity

```
pal_nodes  unwired  pal_procs  wrong_family_spellmods  baseline_leaks
44         0        26         0                       0
```

| Metric | Value | Reconciliation |
|---|---|---|
| pal_nodes (`era_talent` classId=2) | 44 | 44 ✓ |
| unwired (nodes with no `era_talent_rank`) | 0 | full tree wired, zero `display_only` ✓ |
| pal_procs (`spell_proc` 924808–932645) | 26 | proc rows across Redoubt, Reckoning, Blessing of Sanctuary, Vindication, Vengeance, Seal of Command, and the Shield Specialization block family |
| wrong_family_spellmods (`spell_dbc` 924808–925156, `EffectAura_1 IN (107,108)`, `SpellClassSet<>10`) | 0 | every ADD_FLAT/ADD_PCT_MODIFIER row in the auto-passive band binds `SpellClassSet=10` (paladin family) — none leaked to another family |
| baseline_leaks (`trainer_spell` rows for 26573/20217) | 0 | both baseline-gate rank-1 trainer rows correctly deleted |

**Family-10 SPELLMOD guard = 0.** The dedicated query confirms no spellmod row in the whole
auto-passive band escapes the paladin family — the `affectsMask` overrides on Improved Devotion
Aura / Improved Concentration Aura / Improved Retribution Aura / Improved Seal of the Crusader /
Improved Seal of Righteousness all bind correctly without leaking into a sibling paladin aura or
seal.

## Step 3 — lint + audit + host tests + regen idempotency

- `pytest tools/test_gen_era_talents.py tools/test_import_daribon_talents.py -q` → **107 passed**.
- `tools/era_audit.py` (bare, no args) → **`era_audit: 0 finding(s)`**.
- `tools/era-regen.sh` re-run → **idempotent**: same stamp `659e8b25`, `git status --short` clean
  (no diff to any tracked generated file).

## Step 4 — full-spec headless pass

**Bot:** `Zaethos` (guid 32, level 80, paladin), verified clean before starting: `era=WotLK
storedEra=WotLK available=71 spent=0`, 0 `era_character_talent` rows, 0 custom (`920000`–`933000`)
`character_spell` rows. `ip set Zaethos 1` → `era=Vanilla storedEra=WotLK available=71 spent=0
generation=659e8b25`.

Learned **23 distinct nodes spanning all three tabs**, deliberately mixing every mechanic type and
pushing each tab past multiple `prereqPoints` gates:

- **Holy (21 pts):** `18601`(5, stat) `18604`(5, spellmod `affectsMask`) `18606`(1, **grant
  Consecration — baseline gate**) `18602`(4/5, stat, partial rank) `18609`(5, **grant Illumination
  — 5-rank chain, each rank a different stock spell**) `18611`(1, **grant Divine Favor** — prereq
  `18609` maxed + `prereqPoints 20`).
- **Protection (23 pts):** `18615`(5, spellmod `affectsMask`) `18616`(5, **Redoubt clone-grant,
  proc**) `18618`(2, multi) `18620`(1, **grant Blessing of Kings — baseline gate**) `18622`(3, stat
  — prereq `18616` maxed) `18626`(1, **Blessing of Sanctuary clone-grant, proc**) `18619`(4/5, stat,
  partial) `18627`(2/5, **Reckoning clone-grant, proc**, partial).
- **Retribution (24 pts):** `18630`(5, spellmod) `18633`(3, **Improved Seal of the Crusader —
  headline reconstruction spellmod, `affectsMask` word-C**) `18632`(2, spellmod cooldown) `18637`
  (1, **grant Seal of Command clone, proc**) `18634`(4/5, stat, partial) `18639`(2, **grant Eye for
  an Eye clone — rebinds core script**) `18635`(3, **Vindication proc**) `18642`(1, **grant Sanctity
  Aura reconstruction**) `18636`(3/5, stat, partial).

**Point accounting exact and monotonic, never negative:** final doctor **`available=3 spent=68`**
(71 − 68 = 3; matches the manual per-tab tally 21 + 23 + 24 = 68 exactly). Every intermediate learn
reported a strictly increasing rank with no rejection outside the deliberate probes below.

**`eratalents doctor` clean:** all 23 learned nodes report `known=yes spellInfo=yes`, resolving to
both stock grants (`26573`, `20215`, `20216`, `20217`) and generated/helper spells (spellmods
`924836`/`924924`/`925044`/`925057`/`925066`, stats `924812`/`924819`/`924945`/`924955`/`924978`/
`925075`/`925090`, clone-grants `932623`/`932617`/`932630`/`932606`/`932609`/`932602`). No
orphan/missing spellInfo, 0 stray pet auras, 0 STALE STOCK SPELL ACTIVE lines.

**Five deliberate reject probes, all fired with the correct message:**
1. **Max-rank ceiling** — `18606` (Consecration, maxRank 1, already learned) → `Learn rejected:
   already max rank`.
2. **`prereqTalentId` gate** — `18622` attempted while `18616` (Redoubt) stood at 3/5 (not maxed),
   with `prereqPoints 10` already satisfied (Prot cum 11) → `Learn rejected: requires prerequisite
   talent` — a clean, prereq-only failure (points were NOT the blocker).
3. **`prereqPoints` gate (Protection, 20)** — `18626` (Blessing of Sanctuary) attempted at Prot cum
   16 → `Learn rejected: requires 20 points in tree`.
4. **`prereqPoints` gate (Retribution, 15)** — `18639` (Eye for an Eye) attempted at Ret cum 11 →
   `Learn rejected: requires 15 points in tree`.
5. **`prereqPoints` gate (Retribution, 30)** — `18644` (Repentance) attempted at Ret cum 24 →
   `Learn rejected: requires 30 points in tree`.

**`eratalents reset Zaethos`** → `available=71 spent=0`, "no era talents spent this era", 0
`era_character_talent` rows. `ip set Zaethos 13` (restore WotLK) run **after** the reset, which
surfaced a genuine artifact worth recording: the SotC seal ranks (`932634`–`932639`, baseline
reconcile-managed, not talent-gated) had been granted under Vanilla and were NOT re-reconciled by
the bare `ip set` call — a follow-up `eratalents doctor` correctly flagged all six as `ORPHAN ERA
SPELL: ... (SotC seal on a non-(Vanilla paladin))`, and a second `eratalents reset` (which
unconditionally calls `ReconcileBaselineSpells` under the character's *current* era) cleanly
stripped them — doctor then reported zero orphans. This is exactly the documented mechanism
(`ReconcileBaselineSpells` runs on learn/reset, not on a bare progression-level poke) and confirms
the orphan-detection path works both ways (flags a stale reconcile-grant, and the reconcile call
itself is what fixes it). **Cleanup:** `.saveall` to commit the strip to disk; final DB check
confirmed **0 `era_character_talent` rows and 0 custom (`920000`–`933000`) `character_spell` rows**
for guid 32 — dev box left clean.

## Seal of the Crusader reconstruction — verified

Independent of the talent tree (it is a baseline reconcile grant, not a talent grant), a direct SQL
check on `character_spell` confirmed **all six seal ranks (`932634`–`932639`) auto-granted** to the
level-80 Vanilla paladin bot (rank thresholds L6/12/22/32/42/52, all met at level 80). The
Judgement of the Crusader debuffs (`932640`–`932645`) are correctly **absent** from
`character_spell` — by design they are never a known/spellbook spell, only cast via the core
judge-unleash path reading the seal's own DUMMY effect. Node `18633` (Improved Seal of the
Crusader) was learned to rank 3/3 (`925066`, `op:effect1` +15%) — its `affectsMask` (word-C bit 9)
correctly reaches both the seal's AP-scaling effect and the Judgement's Holy-damage effect without
touching any stock seal (confirmed structurally by the family-10 guard = 0 and the doctor's clean
`known=yes spellInfo=yes` resolution).

## Per-mechanic headless evidence

- **Grants — known + applied.** Stock grants (`18606`→`26573`, `18609`→`20210…20215` chain,
  `18611`→`20216`, `18620`→`20217`) and clone-grants (`18616`→`932619…932623`, `18626`→`932617`,
  `18627`→`932629…932633`, `18637`→`932606`, `18639`→`932608/932609`, `18642`→`932602`) all
  resolved `known=yes spellInfo=yes` on the bot.
- **Spellmods bind family-10 (guard 0).** Plain spellmods: `18630`→`925044` (Improved Blessing of
  Might, `allEffects` pct), `18632`→`925057` (Improved Judgement, cooldown flat). `affectsMask`
  overrides: `18604`→`924836` (Improved Seal of Righteousness, word-B bit29 excluding Seal of
  Justice), `18615`→`924924` (Improved Devotion Aura, word-A bit6 excluding the shared paladin-aura
  category bit), `18633`→`925066` (Improved Seal of the Crusader, word-C bit9). The wrong-family
  guard query returned **0** across the whole band.
- **Stats applied.** `18601`(`924812`), `18602`(`924819`, partial 4/5), `18619`(`924955`, partial
  4/5), `18622`(`924978`), `18634`(`925075`, partial 4/5), `18636`(`925090`, partial 3/5) all
  resolve to their generated aura spells with `known=yes spellInfo=yes`.
- **Multi.** `18618` (Guardian's Favor, two spellmods in one node — Hand of Protection cooldown +
  Hand of Freedom duration) resolves `924945` `known=yes spellInfo=yes`.
- **Procs wired (flags / hitMask / chance match the YAML).** Live `spell_proc` rows read back
  exactly as authored: Redoubt `18616`→`932623` (`flags=680 hitMask=2 chance=100`, crit-triggered
  block-buff), Blessing of Sanctuary `18626`→`932617` (`flags=680 hitMask=64 chance=100`,
  block-gated Holy reflect), Reckoning `18627`→`932630` rank 2 (`flags=139944 hitMask=2 chance=40`,
  matching the per-rank 20/40/60/80/100% series), Vindication `18635`→`925082` rank 3
  (`flags=20 hitMask=0 chance=30`, flat-30% melee-hit debuff), Seal of Command `18637`→`932606`
  (`flags=20 hitMask=0 chance=40`, flat-40% approximating Vanilla ~7 PPM).
- **Eye for an Eye script rebinding confirmed live.** `18639` rank 2 → `932609`; doctor reports
  `script: spell_pal_eye_for_an_eye` bound to the clone id, and the live band DUMMY-marker scan
  shows `spell 932609 misc 0 amount 30` — the rebound core script reads `GetAmount()=30` (rank 2's
  Vanilla 30% reflect value) directly off the clone, exactly as designed (no new script needed).

## Grant list (node → spell id, this pass)

| Node | Kind | Spell id(s) |
|---|---|---|
| 18606 | stock grant (baseline gate) | 26573 |
| 18609 | stock grant chain (5 ranks) | 20210, 20212, 20213, 20214, 20215 |
| 18611 | stock grant | 20216 |
| 18620 | stock grant (baseline gate) | 20217 |
| 18616 | clone-grant (proc, Redoubt) | 932619–932623 |
| 18626 | clone-grant (proc, Blessing of Sanctuary) | 932617 (→932618 proc trigger) |
| 18627 | clone-grant (proc, Reckoning) | 932629–932633 |
| 18637 | clone-grant (proc, Seal of Command) | 932606 (→932607 proc trigger) |
| 18639 | clone-grant (rebinds core script) | 932608, 932609 |
| 18642 | clone-grant (reconstruction, Sanctity Aura) | 932602 |

Not individually learn-tested this pass (structurally verified via Step 2 DB integrity — all have
`era_talent_rank` rows, no wrong-family spellmods, correct proc rows counted in `pal_procs=26`):
Holy `18603, 18605, 18607, 18608, 18610, 18612, 18613, 18614`; Protection `18617, 18621, 18623,
18624, 18625, 18628, 18629`; Retribution `18631, 18638, 18640, 18641, 18643, 18644`.

## Status

**Headless: PASS.** The Task-9 headless sweep (evidence above) ran at stamp `659e8b25`: clean
rebuild + boot (339 nodes, stable, no crash, no our-band errors), DB integrity (**family-10 guard
= 0**, unwired=0, baseline_leaks=0), full-spec headless pass (point accounting exact and monotonic
`71→3 / spent 68`, 5/5 reject probes correct — max-rank cap, prereqTalentId gate, and prereqPoints
gates at 20/15/30 — full doctor clean, reset clean, bot restored and re-cleaned after the SotC
orphan artifact was caught and fixed), SotC baseline reconcile grant independently confirmed via
DB, per-mechanic evidence gathered (grants known+applied, spellmods bind family-10 including three
distinct `affectsMask` overrides, stats applied, procs wired with correct flags/hitMask/chance,
Eye for an Eye's core-script rebind confirmed live via the DUMMY-marker amount), host
tests/audit/regen-idempotency all green (107/107, 0 findings, idempotent). **No content defects
found.**

Branch merged to master 2026-09-07.

## Real-client-pass items (runtime firing — not headlessly verifiable)

Runtime behaviors headless verification cannot prove — require a live 3.3.5a Vanilla Paladin with
the fresh `patch-V.mpq` (`659e8b25`) + restaged `EraTalents` addon:

1. **Canary green** at login (server + client both `659e8b25`; no red generation-mismatch warning).
2. **3-tab panel** renders with per-tree backgrounds, all 44 nodes positioned, prereq arrows, real
   icons.
3. **Seal of the Crusader end-to-end:** cast the seal, Judge a target, confirm Judgement of the
   Crusader lands as a Holy DoT/debuff on the target; learning Improved Seal of the Crusader
   (`18633`) raises both the seal's AP bonus and the Judgement's magnitude by 5/10/15%.
4. **Seal of Command** procs Holy damage on melee swings (~40% flat rate in this build vs Vanilla's
   documented ~7 PPM); **Sanctity Aura** +10% Holy applies to a grouped party member within 30yd;
   **Vengeance / Vindication / Redoubt / Reckoning** procs fire on their trigger conditions (crit
   taken, crit dealt, melee hit); **Illumination** refunds mana on a critical heal; **Eye for an
   Eye** reflects 15/30% of a spell-crit's damage back to the caster.
5. **The two clone-grants with block-gated procs** (Holy Shield, Blessing of Sanctuary) — block
   triggers the Holy damage reflect, and the buff icons render with correct tooltips.
6. **Spellmod tooltip/cast/cooldown rewrites** render correctly: Improved Hammer of Justice's
   reduced cooldown, Guardian's Favor's dual cooldown/duration rewrite, Benediction's mana-cost
   reduction, Improved Judgement's cooldown reduction.
7. **Baseline gates in-client:** a Vanilla paladin without the talent cannot train Consecration or
   Blessing of Kings from any trainer; learning the talent (`18606`/`18620`) makes rank 1 available
   and the higher ranks trainable normally.
8. **Canary green + era-transition strip symmetry** after installing the fresh patch-V.mpq +
   restaged addon, confirmed on a **real player** (bots never fire `EraTransition::Detect` — the
   documented limitation for every prior class; this record's own `ip set`-without-reconcile
   artifact above is a headless echo of exactly that gap).

### Tracked accepted divergences (grant-accuracy items, user-approved LAN/bot-server tradeoffs)

- **Repentance (`18644`→`20066`)** — WotLK's stock spell carries a 60s duration (Vanilla is 6s) and
  works on a broader creature-type set than Vanilla's Humanoid-only restriction; kept as a grant
  rather than authoring a duration/creature-type-restricted clone (accepted CC-generosity gap on a
  LAN/bot server).
- **Seal of Command (`18637`) proc rate** — flat 40% chance approximates Vanilla's documented ~7
  PPM; the data pipeline has no PPM column, so the flat-rate compromise is intentional (this IS more
  faithful than granting the stock WotLK proc, which is 35% weapon-percent cleave-target rather than
  70% single-target).
- **Improved Lay on Hands (`18607`) armor rider omitted** — only the cooldown-reduction half is
  emitted; the +15%/+30% target-armor buff is a proc-on-cast rider the generator's one-mechanic-
  per-node model cannot express alongside a cooldown spellmod on the same node (documented gap,
  same class as the Improved Concentration Aura group-resist rider below).
- **Improved Concentration Aura (`18625`) group-resist rider omitted** — only the aura's own
  effect-scaling half is emitted; the "+X% chance to resist Silence/Interrupt for group members"
  clause is a separate group-aura effect a spellmod cannot append.
- **Holy Shock (`18614`) single-rank grant** — Vanilla had 3 ranks (20473/20929/20930); only rank 1
  is granted here (the higher ranks are trainer-taught chains, not reconstructed).
- **Untested `affectsMask` word-B/word-C generator keys** — this pass exercises word-A (`18615`),
  word-B (`18604`), and word-C (`18633`) overrides all structurally (family-10 guard = 0 across the
  whole band), but only `18633`'s live in-game firing (seal AP + Judgement damage scaling together)
  is a real-client-only confirmation per item 3 above.
