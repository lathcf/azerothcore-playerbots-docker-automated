# Era Talents Phase 9 — Vanilla Druid: Verification Record

> **Generation superseded (2026-09-06 final review):** the current stamp is the closing row of docs/verification/era-talents-final-review-tbc-vanilla.md §10 — reinstall patch-V.mpq + restage the addon before any real-client pass.

**Date:** 2026-08-20
**Branch:** `feat/era-talents` (merged to master 2026-09-07)
**Generation stamp:** `46d8cbbf`
**Status: HEADLESS PASS COMPLETE.** Full Vanilla Druid talent tree (47 nodes, ids `18700`–`18746`)
across Balance / Feral Combat / Restoration, class 11, family 7, on the existing `mod-era-talents`
pipeline. This record is **headless verification only** (Task 10). Runtime behaviors that require a
live 3.3.5a client (canary, panel render, live proc/aura firing, form-gating in-game, tooltips) are
deferred to the real-client pass (Task 11, listed at the end).

## Scope delivered

- **47 nodes**, ids `18700`–`18746`, tabs Balance(16) / Feral Combat(16) / Restoration(15). Class 11,
  family 7. Auto-passive band `925600`–`925964`; hand-authored helpers **`932500`–`932513` used;
  next free `932514`** (the original six 932500-932505 plus 932506-932513 from the 2026-08-21 fix round).
- **Balance:** Improved Wrath / Improved Entangling Roots / Natural Shapeshifter / Improved Thorns /
  Nature's Reach / Moonglow / Moonfury (spellmods, several with multi-spell `affects:` sets);
  Improved Nature's Grasp (`op=procChance` raising the 18701 clone's base chance); Improved Moonfire
  (multi allEffects+crit); Improved Starfire (spellmod castTime + `procPassive:` stun via stock
  Celestial Focus `16922`, no clone needed); Natural Weapons (stat, aura 79); Nature's Grasp
  (RECONSTRUCTION, clone `932505`, 35%-base/1-charge vs stock 100%/3-charge); Nature's Grace
  (RECONSTRUCTION, proc → clone `932504`, flat -0.5s next-spell buff vs WotLK's %-haste Nature-only
  buff); Omen of Clarity / Vengeance (stock grants, self-contained spellmod-carriers, no HasTalent
  gate); Moonkin Form (stock grant `24858` — the FORM itself is Vanilla-faithful; Moonkin AURA is an
  accepted baseline gap, below).
- **Feral Combat:** Ferocity / Improved Shred (spellmod cost); Feral Aggression (multi allEffects+
  damage); Feral Instinct (multi, un-gated threat+stealth stat); Brutal Impact (spellmod duration);
  Thick Hide / Sharpened Claws (stat, the latter `stances:`-gated cat/bear/direbear); Feline
  Swiftness (multi, `stances:[cat]`); Feral Charge (stock grant `16979`); Predatory Strikes
  (RECONSTRUCTION, clones `932500`–`932502`, icon-1563 DUMMY feral-AP % of level); Blood Frenzy /
  Primal Fury (stock grants, the CONCERN-1 trigger-label swap resolved — cleanly separable stock
  spells); Savage Fury / Faerie Fire (Feral) (stock grants); Heart of the Wild (stock grants
  `17003`–`17006`/`24894`, +Int% Vanilla-faithful, per-form Stamina/Str component an accepted gap
  below); Leader of the Pack (RECONSTRUCTION, clone `932503`, +3% PARTY crit, `stances:[cat,bear,
  direbear]`, drops WotLK's on-crit heal/mana).
- **Restoration:** Improved Mark of the Wild / Nature's Focus / Tranquil Spirit / Improved
  Rejuvenation / Subtlety / Improved Tranquility / Improved Regrowth (spellmods, several `op=threat`/
  `op=allEffects`/`op=crit`); Furor (stock grant chain `17056`/`17058`-`17061`, not HasTalent-gated —
  icon-238 DUMMY read by the core's shapeshift handler); Improved Healing Touch (spellmod castTime);
  Improved Enrage (spellmod `op=effect2`, Enrage's own Energize effect); Reflection (stat, aura 134);
  Insect Swarm (stock grant `5570`, accepted magnitude gap below); Gift of Nature (stat, aura 136 —
  authored as a stat rather than granting the stock chain, which under-covers Rejuvenation/Swiftmend);
  Nature's Swiftness / Swiftmend (stock grants, Vanilla-faithful capstones).

## Reconstruction list (helper clones, band `932500`–`932513`; next free `932514`)

| Helper id | Name | Node | Template | Shape |
|---|---|---|---|---|
| 932500 | Predatory Strikes r1 | 18725 | 16972 | Effect-0 DUMMY basePoints 50, icon 1563 (inherited) — core feral-AP formula reads it as 50% of level |
| 932501 | Predatory Strikes r2 | 18725 | 16974 | Effect-0 DUMMY basePoints 100 |
| 932502 | Predatory Strikes r3 | 18725 | 16975 | Effect-0 DUMMY basePoints 150 |
| 932503 | Leader of the Pack | 18731 | 17007 | `stances:[cat,bear,direbear]` → ShapeshiftMask 145; APPLY_AREA_AURA_PARTY aura 52 +3% crit; drops WotLK on-crit heal/mana |
| 932504 | Nature's Grace (buff) | 18712 (proc trigger) | 16886 | ADD_FLAT_MODIFIER misc10 (castTime) basePoints -500, ProcCharges 1, DurationIndex 21 (persists until consumed); per-effect mask authored ALL-ONES 2026-09-06 (final review I-19) so it binds EVERY family-7 spell, matching 1.12's unrestricted "your next spell" instead of inheriting 16886's WotLK nature-caster mask |
| 932505 | Nature's Grasp (35%-base) | 18701 | 16689 | procChance 35 / procCharges 1 overriding stock 100%/3; inherits Entangling Roots proc-trigger + identity mask so Improved Nature's Grasp binds |
| 932506 | Leader of the Pack (visible buff) | 18731 (triggered by 932503) | 17007 | fix round 2026-08-21: Attributes 0 = VISIBLE aura, APPLY_AREA_AURA_PARTY aura 52 +3% crit, ShapeshiftMask 145, no skillLine |
| 932507-932511 | Nature's Grasp r2-r6 | 18701 (trainer-taught above r1) | 16810/16811/16812/16813/17329 | fix round 2026-08-21: ReqAbility1 chain off 932505 + `spell_ranks` chain; each inherits its rank's Entangling Roots trigger |
| 932512 | Omen of Clarity (castable) | 18708 | 16864 | fix round 2026-08-21: Vanilla castable-clone shape |
| 932513 | Enrage (Vanilla base) | none — level grant (L12), version-swap | 5229 | fix round 2026-08-21: Effect_1 = 20 Rage over 10s (periodic energize), **Effect_2 = ENERGIZE basePoints 0** (no built-in instant chunk; the slot exists only so Improved Enrage's SPELLMOD_EFFECT2 has something to add +5/+10 to), Effect_3 dropped |

All of these clones inherit their template's family/identity masks (only the authored effect/proc
columns — and, for 932504 since 2026-09-06, its per-effect spell-class mask — are overridden) — verified structurally by the family-7 guard (0 wrong-family rows in the whole
auto-passive band, see Step 2) and functionally by the headless doctor pass (Step 4): `932502`'s
DUMMY marker read back `misc 0 amount 150` at rank 3, and `932503`/`932505` both resolved
`known=yes spellInfo=yes` when granted.

**Blood Frenzy / Primal Fury split** (nodes 18726/18727): CONCERN-1 from the content-design phase
resolved — the two WotLK-merged "Primal Fury" (37116/37117) trigger spells (16952/16954 cat-combo,
16958/16961 bear-rage) are cleanly separable stock spells with the `_ref` skeleton's trigger labels
simply swapped; both are plain grants, form-gated by their own `ShapeshiftMask` (1=cat / 144=bear|
direbear), no clone needed.

**Furor** (node 18733): plain grant of the stock 5-rank chain (`17056`,`17058`-`17061`). Not
HasTalent-gated — the core's shapeshift handler reads the Furor chance via
`GetDummyAuraEffect(SPELLFAMILY_DRUID, 238, 0)`, keyed on family + **SpellIconID 238** at effect-0
DUMMY, not on a talent, so a straight grant fires correctly (confirmed live: node 18733 rank 2 →
`17058` `known=yes spellInfo=yes`, doctor Step 4).

## New generator feature: `stances:`

Task 6 added a `stances:` key (any `mechanic: stat` or `multi` node, or a `helpers:` clone) that
resolves a form-name list (`cat`, `bear`, `direbear`, `moonkin`, `travel`, `aquatic`) to a
`ShapeshiftMask` written onto the generated/cloned spell, so the core's own shapeshift passive-loop
applies the aura only while the character is actually in that form. Used by: Sharpened Claws
(18723, cat|bear|direbear), Feline Swiftness (18721, cat), Leader of the Pack clone (932503, cat|
bear|direbear). Headless-confirmed in Step 4: `925769` (Feline Swiftness), `925786` (Sharpened
Claws), and `932502`/`932503` (Predatory Strikes/LotP) all reported `known=yes` but were **absent**
from the "band aura on player" applied list while Chasto was out of any druid form — exactly the
expected stances-gated behavior (known/learned, but only *applied* in-form). Firing correctly
in-form is a real-client check (Task 11) since the headless bot never actually shapeshifts.
Reusable for a future Warrior/Rogue stance-style pass per the framework note.

## Step 1 — clean rebuild + boot (all eight classes)

`tools/era-regen.sh --sync-fork` regenerated all eight class datasets. `git status --short` showed
**only the pre-existing `repo-pins.txt` diff** both before and after the regen — the generation
stamp did **not** move, confirming nothing was uncommitted: stamp `46d8cbbf` matches the tracked
generated SQL/Lua/client-patch from the prior Druid tab commits.

- `uv run --with pyyaml python tools/era_audit.py` → **`era_audit: 0 finding(s)`**.
- `uv run --with pyyaml --with pytest -- python3 -m pytest tools/ -q` → **121 passed** (the plan's
  "expect 118" is a stale count from an earlier task; 121 is the current green baseline, all
  passing).
- `docker compose up -d ac-db-import` → Auth/Character/World DBs all "up-to-date"; the only World
  reapply was the expected `era_talent_meta` stamp row (`50DC2B8` → `7EDC0AF`, generation write),
  exit 0, no errors.
- `docker compose up -d --force-recreate ac-worldserver` → clean boot:

```
WORLD: World Initialized In 0 Minutes 14 Seconds
AzerothCore rev. 190184a04539+ 2026-07-31 06:09:57 -0700 (Playerbot branch) (Unix, RelWithDebInfo, Static) (worldserver-daemon) ready...
[mod-era-talents] startup (enable=true)
[mod-era-talents] loaded 386 talent nodes (generation 46d8cbbf)
```

386 = 339 prior (Mage/Priest/Warlock/Hunter/Rogue/Warrior/Paladin) + **47 Druid**, one generation
stamp. `docker compose logs ac-worldserver | grep -iE "error|assert|segfault|crash"` returned only
the benign, unrelated `Can't set process priority class, error: Permission denied` line (a
container-permissions notice present on every boot, not error-worthy) — **zero errors on any
`9325xx`/`9256xx` id, era script, `spell_proc`, or `spell_dbc` row**. Container stayed `Up` and
stable through the entire verification session.

`era_talent` classId breakdown (all 8 classes, one query):

| classId | class | count |
|---|---|---|
| 1 | Warrior | 52 |
| 2 | Paladin | 44 |
| 3 | Hunter | 46 |
| 4 | Rogue | 51 |
| 5 | Priest | 47 |
| 8 | Mage | 49 |
| 9 | Warlock | 50 |
| **11** | **Druid** | **47** |

Sum = 386, matching the boot line exactly. All seven prior classes unchanged from their own
verification records; **classId 11 = 47** as required.

## Step 2 — DB integrity

```
dru_nodes  unwired  dru_procs(band)  wrong_family_spellmods  helper_wrong_family
47         0        6                0                       0
```

| Metric | Value | Reconciliation |
|---|---|---|
| dru_nodes (`era_talent` classId=11) | 47 | 47 ✓ |
| unwired (nodes with no `era_talent_rank`) | 0 | full tree wired, zero `display_only` ✓ |
| `era_talent_rank` rows for classId=11 | 151 | matches the sum of all 47 nodes' `maxRank` |
| dru_procs (`spell_proc` in the auto-passive band `925600`-`926200`) | 6 | Improved Starfire's 5 `procPassive` stun ranks (`925688`-`925692`, flags 65536, chance 3/6/9/12/15, hitMask 0) + Nature's Grace's proc (`925696`, flags 327680, chance 100, hitMask 2) |
| dru_procs (helper band `932500`-`932505`) | 0 | the helper clones themselves carry no separate `spell_proc` row (932505 is a DBC-native proc via its own `ProcTypeMask`/`ProcChance` columns, not the `spell_proc` table) |
| wrong_family_spellmods (`spell_dbc` in `925600`-`926200`, `EffectAura_{1,2,3} IN (107,108)`, `SpellClassSet<>7`) | 0 | every spellmod row in the auto-passive band binds `SpellClassSet=7` (druid family) |
| helper_wrong_family (`spell_dbc` `932500`-`932505`, `SpellClassSet NOT IN (7,0)`) | 0 | all six helper clones correctly inherit or carry the druid family (or 0/generic for the DUMMY-icon-read clones) |
| auto-passive band range | `925600`-`925964` | matches `920000 + (nodeId-18000)*8 + (rank-1)` for nodeId 18700..18746 |

**Family-7 guard = 0** across both the auto-passive band and the hand-authored helper band — no
spellmod or helper leaked into another class's spell family.

## Step 3 — lint + audit + host tests + regen idempotency

- `tools/era_audit.py` (bare, no args) → **`era_audit: 0 finding(s)`**.
- `pytest tools/ -q` → **121 passed**.
- `tools/era-regen.sh --sync-fork` re-run → **idempotent**: same stamp `46d8cbbf`, `git status
  --short` showed only the pre-existing `repo-pins.txt` diff (no tracked generated file changed).

## Step 4 — full-spec headless pass

**Bot:** `Chasto` (guid 20, level 80, druid), verified clean before starting: `era=WotLK
storedEra=WotLK available=71 spent=0`, 0 `era_character_talent` rows, 0 custom (`920000`–`933000`)
`character_spell` rows. `ip set Chasto 1` → `era=Vanilla storedEra=WotLK available=71 spent=0
generation=46d8cbbf`.

Learned **25 distinct nodes spanning all three tabs**, deliberately mixing every mechanic type
(grant / clone / spellmod / stat / multi / proc / stances) and pushing each tab's `prereqPoints`
gates:

- **Balance (29 pts):** `18700`(5, spellmod castTime) `18701`(1, **grant clone 932505 Nature's
  Grasp**) `18702`(4, spellmod procChance, prereqTalentId `18701` maxed) `18705`(5, stat) `18704`(5,
  multi allEffects+crit) `18706`(2/3, spellmod stances, partial) `18708`(1, **grant Omen of
  Clarity**, prereqTalentId `18705` maxed) `18711`(5, spellmod+`procPassive` stun) `18712`(1,
  **proc → clone 932504 Nature's Grace**, prereqPoints 20).
- **Feral Combat (31 pts):** `18716`(5, spellmod cost) `18717`(5, multi) `18723`(3, stat stances)
  `18718`(2, multi un-gated) `18725`(3, **grant clones 932500-502 Predatory Strikes**) `18726`(2,
  **grant Blood Frenzy**, prereqTalentId `18723` maxed) `18727`(2, **grant Primal Fury**, same
  prereq) `18721`(2, multi stances[cat]) `18720`(1, stat) `18730`(5, **grant Heart of the Wild**,
  prereqTalentId `18725` maxed + prereqPoints 25) `18731`(1, **grant clone 932503 Leader of the
  Pack**, prereqPoints 30).
- **Restoration (11 pts):** `18732`(3/5, spellmod allEffects, partial) `18733`(2/5, **grant Furor**,
  partial) `18736`(2, spellmod effect2) `18734`(3/5, spellmod castTime, partial) `18738`(1, **grant
  Insect Swarm**, prereqPoints 10).

**Point accounting exact and monotonic, never negative:** final doctor **`available=0 spent=71`**
(71 − 71 = 0; matches the manual per-tab tally 29 + 31 + 11 = 71 exactly). Every intermediate learn
reported a strictly increasing rank with no rejection outside the deliberate probes below.

**`eratalents doctor` clean:** all 25 learned nodes report `known=yes spellInfo=yes`, resolving to
both stock grants (`16864`, `16954`, `16961`, `24894`, `17058`, `5570`) and generated/helper spells
(spellmods `925604`/`925619`/`925636`/`925644`/`925649`/`925692`/`925732`/`925740`/`925745`/
`925760`/`925786`/`925858`/`925874`/`925889`, proc carriers `925696`, clones `932505`/`932502`/
`932503`). No orphan/missing spellInfo, 0 stray pet auras.

**Per-mechanic headless evidence, values confirmed exact:**
- `925604` (Improved Wrath r5) amount `-500` = -0.5s castTime ✓
- `925619` (Improved Nature's Grasp r4) amount `65` = +65% procChance ✓
- `925636` (Improved Moonfire r5, multi) amount `10` = +10% (both allEffects+crit halves) ✓
- `925644` (Natural Weapons r5) amount `10` = +10% physical damage ✓
- `925649` (Natural Shapeshifter r2) amount `-20` = -20% shift cost ✓
- `925692` (Improved Starfire r5) amount `-500` = -0.5s castTime; `spell_proc chance=15` = 15% stun ✓
- `925696` (Nature's Grace proc carrier r1) `spell_proc flags=327680 hitMask=2 chance=100` = any
  magic-class spell crit, 100% ✓
- `925732` (Ferocity r5) amount `-5` = -5 rage/energy ✓
- `925740` (Feral Aggression r5, multi) amount `40` = +40% Demo Roar (allEffects half) ✓
- `925745` (Feral Instinct r2, multi) amount `6` = +6% threat (un-gated stat half) ✓
- `925760` (Thick Hide r1) amount `2` = +2% armor-from-items ✓
- `932502` (Predatory Strikes r3 clone) **DUMMY marker `misc 0 amount 150`** = 150% of level, read
  by the core's icon-1563 feral-AP formula ✓
- `925858` (Improved Mark of the Wild r3) amount `21` = 7×3% ✓
- `925874` (Improved Healing Touch r3) amount `-300` = -0.3s castTime ✓
- `925889` (Improved Enrage r2) amount `100` = +10 Rage (rage stored ×10) ✓

**`stances:`-gated passives correctly known-but-unapplied out-of-form:** `925769` (Feline
Swiftness), `925786` (Sharpened Claws), `932502`/`932503` (Predatory Strikes/Leader of the Pack)
all resolved `known=yes spellInfo=yes` but were **absent** from the doctor's "band aura on player"
applied list while Chasto stood in no druid form — exactly the expected `ShapeshiftMask`-gated
behavior (the core's shapeshift passive-loop only applies them in-form). Firing on an actual
shapeshift is a real-client check (Task 11) since the headless bot never shapeshifts.

**Three deliberate reject probes, all fired with the correct message:**
1. **Max-rank ceiling** — `18700` (Improved Wrath, already rank 5/5) → `Learn rejected: already
   max rank`; `18701` (Nature's Grasp, already rank 1/1) → same, on a maxRank-1 node.
2. **`prereqPoints` gate (Feral, 15)** — `18726` (Blood Frenzy) attempted at Feral cum 11 (before
   `18723` reached max rank and before the points threshold) → `Learn rejected: requires 15 points
   in tree`. (This fired on the points gate rather than the talent gate — `EraTalents.cpp` checks
   `prereqPoints` before `prereqTalentId`, and both conditions were simultaneously unmet at that
   point in the sequence; still a valid, correctly-ordered rejection.)
3. **`prereqPoints` gate (Feral, 25)** — `18730` (Heart of the Wild) attempted at Feral cum 24 →
   `Learn rejected: requires 25 points in tree`.
4. **`prereqPoints` gate (Restoration, 10)** — `18738` (Insect Swarm) attempted at Restoration cum 5
   → `Learn rejected: requires 10 points in tree`.

**`eratalents reset Chasto`** → `available=71 spent=0`, "no era talents spent this era", 0
`era_character_talent` rows. `ip set Chasto 13` (restore WotLK) + `.saveall`. Final DB check
confirmed **0 `era_character_talent` rows and 0 custom (`920000`–`933000`) `character_spell` rows**
for guid 20 — dev box left clean, matching the paladin-record precedent (no baseline-reconcile
orphan artifact was hit this pass — unlike Paladin's SotC, no Druid node is a
`ReconcileBaselineSpells` grant-by-level; every Druid grant is talent-tied and stripped directly by
the plain reset).

## Grant list (node → spell id, this pass)

| Node | Kind | Spell id(s) |
|---|---|---|
| 18701 | clone-grant (Nature's Grasp, reconstruction) | 932505 |
| 18708 | stock grant (Omen of Clarity) | 16864 |
| 18712 | proc → clone-grant (Nature's Grace, reconstruction) | 925696 (proc carrier) → 932504 |
| 18725 | clone-grant (Predatory Strikes) | 932500–932502 |
| 18726 | stock grant (Blood Frenzy) | 16952, 16954 |
| 18727 | stock grant (Primal Fury) | 16958, 16961 |
| 18730 | stock grant (Heart of the Wild) | 17003–17006, 24894 |
| 18731 | clone-grant (Leader of the Pack, reconstruction) | 932503 |
| 18733 | stock grant chain (Furor) | 17056, 17058–17061 |
| 18738 | stock grant (Insect Swarm) | 5570 |

Not individually learn-tested this pass (structurally verified via Step 2 DB integrity — all have
`era_talent_rank` rows, no wrong-family spellmods, correct proc rows counted in `dru_procs=6`):
Balance `18703, 18707, 18709, 18710, 18713, 18714, 18715`; Feral `18719, 18722, 18724, 18728,
18729`; Restoration `18735, 18737, 18739, 18740, 18741, 18742, 18743, 18744, 18745, 18746`.

## Baseline divergences / future-effort appendix (Task 10 deliverable)

Harvested by grepping `DIVERGENCE (Task 10 gap)` across `era-data/vanilla/druid.yaml`, plus the two
additional accepted gaps recorded as narrative comments (Moonkin Aura, Insect Swarm) rather than the
literal tag string.

### Three user-accepted fidelity/baseline gaps (decided 2026-08-20)

1. **Heart of the Wild (`18730`) per-form stat — ACCEPTED per user 2026-08-20.** The core applies
   the WotLK formula (Int%/2 as Stamina in Bear, Int%/2 as Attack Power in Cat) instead of Vanilla's
   full Int% as Stamina (Bear) / Strength (Cat). The primary +Intellect% component is fully correct
   and Vanilla-faithful; only the secondary per-form component is at half rate and (in Cat) the
   wrong stat (AP instead of Strength). A faithful fix requires a non-trivial form-change script;
   grant + documented gap was chosen over authoring one for a secondary component.
2. **Moonkin Aura (`18715`'s triggered aura) — ACCEPTED per user 2026-08-20.** The core HARDCASTS
   stock `24907` on `FORM_MOONKIN` entry (`SpellAuraEffects.cpp:1577`, "// Always cast Moonkin
   Aura"), keyed on the **form id** (31), not the granted spell or a talent — WotLK `24907` = +5%
   RAID spell crit + haste rating vs Vanilla's +3% PARTY spell crit only. A form/aura clone CANNOT
   reroute this (whatever spell enters form 31 still triggers stock 24907); only a core patch could
   fix it, which is out of this phase's zero-core-patch scope. The divergence is beneficial
   (over-delivers), so grant stock `24858` and accept the stronger aura.
3. **Insect Swarm (`18738`) magnitude — ACCEPTED per user 2026-08-20.** Stock WotLK `5570` rank 1
   is rebalanced above Vanilla: periodic Nature damage base 144/12s (6 ticks × 24) vs Vanilla's 66,
   and -3% target hit vs Vanilla's -2%. A faithful clone would require rebuilding the whole
   trainer-taught rank chain (`24974`→...→`48468`) plus a re-bound DoT script — disproportionate for
   one offensive node in the heal tree. Granted as-is. **CLOSED 2026-09-07** — clone chain
   932514-932518 at the 1.12 tooltip values (wowhead Classic; the 1.12.1 DBC extract lacks the
   spell).

### Smaller documented gaps (accepted without a separate user sign-off round)

4. **Natural Shapeshifter (`18706`)** — Travel Form and Aquatic Form's shift-cost bits are not in
   the spell `_ref`, so their (already cheap) shift cost isn't reduced by this talent; the combat
   forms (Cat/Bear/Dire Bear) + Moonkin are covered.
5. **Feral Instinct (`18718`)** — the two halves (threat bonus wants Bear-form, stealth-detection
   wants Cat-form/Prowl) cannot share one `stances:` gate on a single multi-effect node, so it ships
   **un-gated** (always-on `MOD_THREAT`), following the Warrior Defiance precedent. The stealth-
   level magnitude (1-5 per rank) is an approximation — the true Vanilla per-rank value is
   undocumented and the effect is minor.
6. **Feline Swiftness (`18721`)** — the "while outdoors" restriction on the movement-speed half is
   not expressible in the generator's stat model, so the speed bonus also applies indoors in Cat
   Form (minor over-delivery).
7. **Faerie Fire (Feral) (`18729`)** — 3.3.5a stock `16857` reduces armor by a **percent** (aura 101
   `MOD_RESISTANCE_PCT`, -5%) whereas Vanilla reduced a **flat** 175 armor. Granted as-is rather than
   cloned, to avoid risking the core's stealth-prevention hook on the same spell.
8. **Furor (`18733`) cat-branch energy semantics** — the Bear/Dire Bear branch (chance to gain 10
   Rage) is Vanilla-faithful; the Cat branch uses WotLK's "retain up to FurorChance% energy on
   shift" mechanic rather than Vanilla's "X% chance to gain a flat 40 Energy" — benign, cat-half
   only, the stored chance percentages are identical either way.
9. **Improved Enrage (`18736`)** — **CLOSED** (fix round 2026-08-21, issue 7). As first recorded:
   this WotLK core's base Enrage (`5229`) grants a pre-existing 20-Rage instant chunk on its own
   Effect_2, so Improved Enrage's +5/+10 Rage stacked on top of a stock baseline amount the talent
   could not edit (editing `5229` would violate the "never touch a stock spell id" era-safety rule).
   The fix did not edit `5229`: a Vanilla-era druid no longer casts it. Base Enrage is now the custom
   clone **`932513`** (a version-swap granted by level, exactly like paladin Lay on Hands), whose
   **Effect_2 is ENERGIZE with basePoints 0** — no built-in instant chunk at all, present only so
   Improved Enrage's `SPELLMOD_EFFECT2` has a slot to add its +5/+10 to. The clone keeps `5229`'s
   word-A bit19 identity, so `18736` binds it unchanged. WotLK-era druids keep stock `5229`.

No other Druid node carries an unresolved fidelity gap; every other node's disposition (grant/
spellmod/stat/multi/clone) was verified against the live Spell.dbc dump (2026-08-20) or the `_ref`
survival map as an exact or better match to the Vanilla tooltip.

## Helper-id and DUMMY-marker allocations (Druid, this phase)

- **Helper band `932500`-`932599` (Druid's reserved slice):** `932500`-`932505` **used** this
  phase (Predatory Strikes r1-3, Leader of the Pack, Nature's Grace buff, Nature's Grasp). **Next
  free: `932506`.** `932506`-`932599` remain free for any future Druid fix round.
- **DUMMY-marker registry:** **none claimed** this phase (Predatory Strikes reads its DUMMY by
  **icon** 1563, not by a registry misc value — no registry slot consumed). **Next free stays 16.**
- **Zero core patches, zero C++ scripts.** Every node resolved to grant / spellmod / stat / multi /
  clone; no new `SpellScript`/`AuraScript` class, no `patches/*.patch` entry, no `spell_script_names`
  row beyond the module's existing generic infrastructure.

The framework's helper-id registry table (`docs/era-talents-framework.md`, Druid row) already
recorded this exact range/disposition from the per-tab authoring commits; this pass confirmed it
against the live DB and left it unchanged (see commit).

## Status

**Headless: PASS.** The Task-10 full-tree verification sweep (evidence above) ran at stamp
`46d8cbbf`: clean rebuild + boot (386 nodes, stable, no crash, no our-band errors, all 8 classes'
`era_talent` counts reconciled), DB integrity (**family-7 guard = 0** across both the auto-passive
and helper bands, unwired=0, `era_talent_rank`=151), lint/audit/tests all green (0 findings, 121/121
passing, idempotent regen), full-spec headless pass (point accounting exact and monotonic
`71→0 / spent 71`, 4/4 reject probes correct, full doctor clean with exact per-mechanism amount
verification, `stances:`-gated passives confirmed known-but-unapplied out-of-form, reset clean, bot
restored and re-cleaned with 0 residual rows), zero `display_only` leftovers confirmed across all 47
nodes, and the baseline-gap appendix harvested and reconciled against the three user-accepted
decisions plus six smaller documented gaps. **No content defects found.**

Branch merged to master 2026-09-07.

## Real-client-pass items (Task 11 — runtime firing, not headlessly verifiable)

Runtime behaviors headless verification cannot prove — require a live 3.3.5a Vanilla Druid with the
fresh `patch-V.mpq` (`46d8cbbf`) + restaged `EraTalents` addon:

1. **Canary green** at login (server + client both `46d8cbbf`; no red generation-mismatch warning).
2. **3-tab panel** renders with per-tree backgrounds, all 47 nodes positioned, prereq arrows, real
   icons.
3. **`stances:`-gated passives fire on an actual shapeshift:** Sharpened Claws / Feline Swiftness /
   Predatory Strikes / Leader of the Pack all apply the moment the character shifts into the
   relevant form and drop the moment they shift out — headless proved the mask bit + amount, not
   the live shapeshift trigger.
4. **Nature's Grasp / Nature's Grace end-to-end:** cast Nature's Grasp, confirm the entangle proc
   fires at the Vanilla 35% base rate (and scales toward 100% with Improved Nature's Grasp ranks);
   any spell crit grants the flat -0.5s next-cast buff, consumed by the very next spell.
5. **Predatory Strikes / Leader of the Pack / Heart of the Wild** all apply their bonuses correctly
   the moment Cat/Bear/Dire Bear form is entered, and Leader of the Pack's +3% crit radiates to
   grouped party members within 45yd.
6. **Procs fire on their trigger conditions:** Improved Starfire's stun (3-15% on a Starfire hit,
   any hit result — not crit-gated), Nature's Grace (any spell crit, 100%), Blood Frenzy/Primal
   Fury (Cat/Bear-form crits on combo-generating abilities).
7. **Spellmod tooltip/cast/cooldown rewrites** render correctly: Improved Wrath/Starfire/Healing
   Touch cast-time reduction, Natural Shapeshifter's reduced shift cost, Moonglow/Tranquil Spirit's
   mana-cost reduction, Subtlety/Improved Tranquility's threat reduction.
8. **Furor** fires its Rage/Energy grant on an actual shapeshift (Bear/Dire Bear → Rage, Cat →
   Energy), and Improved Enrage's extra Rage lands alongside the base Enrage's own Rage chunk.
9. **Moonkin Form** end-to-end: the form transformation, armor bonus, caster-lock, and the
   accepted-stronger Moonkin Aura radiating +5% raid spell crit + haste.
10. **Canary green + era-transition strip symmetry** on a **real player** (bots never fire
    `EraTransition::Detect` — the documented limitation for every prior class).

### Accepted-divergence quick reference (see the full appendix above for detail)

Heart of the Wild per-form stat (half-rate, AP-not-Str in Cat), Moonkin Aura (+5% raid crit/haste
vs Vanilla +3% party), Insect Swarm (WotLK ~144/12s + -3% hit vs Vanilla 66 + -2%) are all
**ACCEPTED per user 2026-08-20**. Natural Shapeshifter (Travel/Aquatic omitted), Feral Instinct
(un-gated, stealth-magnitude approximation), Feline Swiftness (no outdoor restriction), Faerie Fire
(Feral) (percent-vs-flat armor), Furor (cat-branch WotLK energy-retain semantics), and Improved
Enrage (stacks on a pre-existing stock 20-Rage baseline) are smaller documented gaps, none requiring
further action this phase.

---

## Fix-round addendum (2026-08-21, generation `b8f05365`)

Spec: `docs/superpowers/specs/2026-08-21-era-talents-druid-client-fix-round-design.md`. The user's
real-client pass surfaced 8 issues; this round addressed them (helpers `932506`–`932513`).

### Headless verification (all passed)
- `tools/era-regen.sh --sync-fork` → stamp `b8f05365`; `git status` = only intended files (+ the
  pre-existing `repo-pins.txt`, untouched).
- `tools/era_audit.py` → **0 finding(s)**. `pytest tools/ -q` → **121 passed**.
- `db-import` applied `2026_08_21_00_era_talent_druid_ng_trainer.sql` cleanly (4 queries). DB verified:
  NG R2-6 trainer chain on trainer 33 (ReqAbility1 932505→932507→…→932510, ReqLevel 18/28/38/48/58);
  `spell_ranks` 932505→932507→…→932511 (ranks 1-6); stock NG (16689/16810/16811/16812/16813/17329/
  27009/53312), FF-Feral (16857) and Enrage (5229) trainer rows **deleted**; `spell_dbc` rows
  932506-932513 present with correct columns (LotP 932506 Attr 0 / effect 35 aura 52 +3 / dur 21 /
  radius 12; NG 932507-511 procChance 35 / trigger 19974-19970 / mask A 1048576 C 4096; Omen 932512
  Attr 0 / dur 6 (=10min) / ManaCost 120 / trigger 16870 / ProcTypeMask 81924; Enrage 932513
  periodic-energize base 20 (2 rage/tick) / Effect_2 ENERGIZE base 0 / ShapeshiftMask 144 / mask A
  524288 / 1-min cd).

### Resolved-during-implementation values (dump-verified, not guessed)
- **DurationIndex 6 = 600000 ms (10 min)** — confirmed identical across three stock 10-min buffs
  (Dampen Magic 604, Amplify Magic 1008, Unending Breath 5697). Used for Omen 932512.
- **NG per-rank Entangling-Roots triggers:** 16810→19974, 16811→19973, 16812→19972, 16813→19971,
  17329→19970 (SpellLevels 18/28/38/48/58). Stock DurationIndex 22 (45s) inherited by the clones.
- **Stock 16864/16689/16857/5229/17007 are SkillLineAbility AcquireMethod 0** (trainer-taught, NOT
  core-auto-learned) — so the trainer-row-delete + reconcile strip/version-swap is the correct gate;
  no in-world re-strip (the AcquireMethod-2 paladin-seal phantom-icon case does not apply).
- **1.12 Enrage 5229** = periodic energize base 19 (→20 units = 2 rage/tick ×10s = 20 rage over
  time), no instant chunk. A 1.12 Effect aura-101 base -76 could **not** be confidently decoded as
  the Classic ~27%-base-armor drawback, so per the spec the armor penalty is **omitted** (documented
  beneficial divergence).

### Handed to the user for the real-client pass (not headlessly verifiable)
- **⑥ LotP:** the "Leader of the Pack" buff shows on the druid AND grouped party members while in
  Cat/Bear/Dire Bear form, and disappears on leaving form (the 932503→932506 trigger split; the
  in-form shapeshift-cast cannot be exercised headlessly — a bot won't shapeshift).
- **③ Omen of Clarity:** a castable button appears, casting applies a 10-min buff, and Clearcasting
  (16870) procs on attacks.
- **⑦ Enrage:** gives 20 Rage over 10s; with Improved Enrage, +5/+10 instant (total 25/30, not 40);
  stock 5229 absent in Vanilla.
- **① Nature's Grasp:** R2-R6 trainable only with the talent; stock NG not trainable.
- **② Faerie Fire (Feral):** not trainable without the talent.
- **⑤/⑧ Heart of the Wild:** tooltip now matches behavior; Bear Stamina rises ~Int%/2 (stock-core,
  should already work). **④ Predatory Strikes:** Cat-form AP delta ≈ level × 1.5 at rank 3.

### Real-client follow-ups (2026-08-21, final generation `8740b990`)

The real-client pass confirmed the fix round, and surfaced two more clone-fidelity misses — both
resolved and re-verified:

- **Omen of Clarity procced Clearcasting on EVERY hit** (commit `6df9d49`). The castable clone
  `932512` inherited stock 16864's DBC ProcFlags + `ProcChance 100` but had **no `spell_proc` row**,
  so the proc system used the 100% DBC chance. Stock Omen's rate lives in `spell_proc` at
  **`ProcsPerMinute 3.5`** (weapon-speed-normalized, ≈5-6%/hit on a fast weapon) plus the core
  `spell_dru_omen_of_clarity` `AuraScript` (CheckProc filtering; the Clearcasting cast itself is the
  DBC default). Fix = hand SQL migration `2026_08_21_01_era_talent_druid_omen_proc.sql`: a
  `spell_proc` row mirroring stock 16864 (PPM 3.5, ProcFlags 0 ⇒ use the clone's own DBC events,
  Chance 0) + a `spell_script_names` binding to `spell_dru_omen_of_clarity`. Applied live via
  `.reload spell_proc` (rate) + the close-out restart (script binding). **`spell_proc` /
  `spell_script_names` for a clone are hand SQL, not generator output — the generator hardcodes
  `ProcsPerMinute 0`.** USER CONFIRMED working.

- **Nature's Grasp was free and useable indoors** (commit `3e84584`, regen → stamp `8740b990`). The
  clones faithfully copied stock 16689, which in WotLK is **free** and **indoor-ok**; Vanilla NG cost
  mana and was **outdoor-only** (Blizzard dropped the outdoor restriction — stock 16689 / Entangling
  Roots 339 lack `SPELL_ATTR0_ONLY_OUTDOORS`). Fix = YAML helper overrides on all six NG clones
  (`932505`, `932507`-`932511`): `powerCost:` **50/65/80/95/110/125** (r1-6, from the 1.12 client
  Spell.dbc `F_MANA` offset) + `attributes: 98304` (inherited `0x10000` | `SPELL_ATTR0_ONLY_OUTDOORS`
  `0x8000`). DB-verified after db-import (Attr 98304, ManaCost 50…125). The server enforces both
  regardless of the client; the `8740b990` MPQ makes the mana tooltip + client-side indoor block +
  canary match.

### Close-out (2026-08-21)

Worldserver rebuilt (fix round) + gracefully restarted on stamp **`8740b990`** — clean boot, 386
nodes, zero errors on any `9325xx` id / `spell_proc` / `spell_dbc` / `spell_script_names`. All 8
client-pass issues + both follow-ups resolved and **user-confirmed in the real client**. Helper band:
`932500`-`932513` used, **`932514` next free**; no DUMMY-marker registry slot consumed (stays 16).
**Druid (Phase 9) is COMPLETE & real-client verified.** Branch `feat/era-talents` merged to master 2026-09-07
(held until all classes done). **Shaman is the last class.**
