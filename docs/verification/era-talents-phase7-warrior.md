# Era Talents Phase 7 — Vanilla Warrior: Verification Record

**Date:** 2026-08-18
**Branch:** `feat/era-talents` (merged to master 2026-09-07)
**Generation stamp:** `89d7035d` (after the grant-accuracy pass)
**Status: COMPLETE — REAL-CLIENT VERIFIED (2026-08-18, generation `89d7035d`).**
The user ran the full real-client pass and confirmed everything works, including the grant-accuracy
reworks (Bloodthirst, Conflagrate, Last Stand, Arcane Power, Adrenaline Rush) and — after a
follow-up fix — the three baseline-leak gates. The leak-gate correction: the initial reconcile-strip
left the rank-1 `trainer_spell` rows in place so the trainer still OFFERED Shield Slam / Ice Block /
Divine Spirit (the user could still train them); the fix (commit `4cd9543`) DELETES those rank-1
trainer rows (the proven Holy Nova pattern — the higher ranks chain `ReqAbility1` on rank 1, gating
the whole chain behind the talent) and flips the gates to `grantHigherEra=true` so WotLK-stage chars
auto-learn rank 1 at level. Verified in-client: the trainer no longer offers the three spells to a
Vanilla character. Accepted minor divergences: mage Cold Snap / Combustion cooldowns (script-bound;
documented below).

_(Historical note — this record was previously reopened for the grant-accuracy pass; that pass is
now complete and re-verified.)_
An initial real-client pass at `005de6c5` looked good for the panel/mechanics/procs, then the user
found that granted stock ACTIVES shipped WotLK behavior/availability, not Vanilla. A full
two-dimension audit (grant-accuracy + baseline-leak) of the warrior tree AND the five prior classes
followed; all findings are now fixed (user chose full accuracy). Every fix is headless-verified
(audit 0, 6-class boot at `89d7035d`, all grant swaps confirmed in the live DB); the runtime FIRING
of the new actives (Bloodthirst damage/heal, Conflagrate cast/consume, etc.) is a real-client-only
check that remains for the RE-verification pass.

**Grant-accuracy fixes shipped this pass:**
- **Bloodthirst (warrior 18535)** — was WotLK grant `23881` (50% AP, no heal, 1 rank). Now a custom
  4-rank reconstruction: `era_bloodthirst_vanilla` SpellScript (45% AP), heal-on-next-5-swings buff
  (flat 10/13/17/20 per rank), clones `932828-839`, trainer chain R2-R4 (L48/54/60) + strip mirror.
- **Shield Slam (warrior 18552)** / **Ice Block (mage 18046)** / **Divine Spirit (priest 18113)** —
  BASELINE LEAKS (trainer-buyable without the talent). Fixed by `ReconcileBaselineSpells`: the
  priest-only early return was hoisted so `kBaselineSpellGates` runs per-class, and a `grantHigherEra`
  flag was added (Holy Nova = auto-baseline → force-grant to higher eras; these three =
  trainer-baseline → strip-only in Vanilla-without-talent, higher eras untouched). Server-only.
- **Last Stand (warrior 18541)** — was WotLK 3-min cd. Now clone `932840` at Vanilla 10-min cd +
  `era_last_stand` SpellScript (the +30% HP was script-bound, so a bare clone would give 0 HP).
- **Arcane Power (mage 18016)** — was +20%/+20%. Now clone `932760` at Vanilla +30%/+30% (both the
  direct and the DoT spellmod effects) + 3-min cd.
- **Adrenaline Rush (rogue 18434)** — was 3-min cd. Now clone `932770` at Vanilla 5-min cd.
- **Conflagrate (warlock 18250)** — was WotLK `17962` (instant, %-of-Immolate, adds a DoT, no
  consume). Now a 1.12-authentic 4-rank chain (clones `932780-783`): instant cast, flat Fire damage
  (240-307/316-397/383-480/447-558), consumes Immolate (DBC-native via `TargetAuraState 14`, no
  script), no DoT, trainer chain R2-R4 + strip mirror.
- **ACCEPTED minor divergences (user-approved):** mage **Cold Snap** (server 8-min vs Vanilla 10-min)
  and **Combustion** (server 2-min vs Vanilla 3-min) — both are script-bound actives (frost-cd reset /
  crit-stacking in core C++), so a cd-only clone would break them; the gaps are small and in the
  player's favor, so the stock spells are kept rather than replicating core scripts for a cooldown.
- **Audit reassurance:** the hidden-C++-coefficient problem (Bloodthirst) is warrior-only; every
  other class's granted actives resolve from DBC. No other baseline leak or behavioral divergence was
  found across the six classes beyond those listed.

## Scope delivered

Full Vanilla Warrior talent tree (52 nodes, ids `18501`–`18552`) across Arms / Fury /
Protection, class 1, SpellFamilyName 4, on the existing `mod-era-talents` pipeline. **52 of 52
nodes wired, zero `display_only`** (38 mechanic nodes + 14 grant nodes — 11 of stock spells, 3 of
authored clones). Ships **two new C++ scripts** — `era_deep_wounds` (an `AuraScript` that computes
the Vanilla "% of average weapon damage" bleed and applies helper `932800`) and
`era_improved_berserker_rage` (a `SpellScript` that energizes +5/10 rage `AfterCast` on stock
Berserker Rage `18499`, era-gated on a DUMMY marker) — the first scripts this class needed. No core
patches.

**DUMMY-marker registry:** misc value **15** is now claimed by Improved Berserker Rage
(`era_improved_berserker_rage` gates on it); next free misc = **16**.

This record is **headless verification only** (Task 9). Runtime behaviors that require a live
3.3.5a client (canary, panel render, live proc/bleed/stun firing, tooltips, era-transition strip)
are deferred to the real-client pass (Task 10, listed at the end).

## Step 1 — clean rebuild + boot (all six classes)

`tools/era-regen.sh --sync-fork` generated stamp `819de7d9` across all six class datasets and
synced the module into `azerothcore-wotlk/modules/`. Rebuilt `ac-worldserver` (clean compile
incl. the `era_deep_wounds` script), ran `ac-db-import` (exit 0 — the warrior SQL files
`2026_08_18_10_era_talent_warrior_data.sql` + `2026_08_18_11_era_talent_warrior_custom_spells.sql`
applied/re-applied along with the cascading `era_talent_meta` stamp updates; no errors), then
`--force-recreate`d `ac-worldserver`.

Boot line:

```
[mod-era-talents] startup (enable=true)
[mod-era-talents] loaded 295 talent nodes (generation 819de7d9)
```

= 49 Mage + 47 Priest + 50 Warlock + 46 Hunter + 51 Rogue + **52 Warrior** = **295**, one
generation stamp (per-class DB counts confirmed: `1→52, 3→46, 4→51, 5→47, 8→49, 9→50`). No
`spell_proc` / `spell_script_names` / `sql.sql` errors for any of our id bands. `WORLD: World
Initialized In 0 Minutes 14 Seconds`, `... ready...`, container stayed `Up` with no
crash/segfault/assertion across the full verification session (`Up 7 minutes` at the stability
check, 0 recent error/fatal lines).

**`era_deep_wounds` registration confirmed** — `spell_script_names` carries three rows binding the
script to the three Deep Wounds ranks (`924072`, `924073`, `924074`); the module compiled and
booted with the script linked in.

## Step 2 — DB integrity

```
warr_nodes  unwired  warr_procs  weapon_gated  scripted
52          1        39          30            3
```

| Metric | Value | Reconciliation |
|---|---|---|
| warr_nodes (`era_talent` classId=1) | 52 | 52 ✓ |
| unwired (nodes with no `era_talent_rank`) | 1 | **exactly node `18533`** (Fury, tierRow 5, maxRank 2, prereqPoints 25 = Improved Berserker Rage) — the documented `display_only` gap ✓ |
| warr_procs (`spell_proc` 924008–924420) | 39 | proc rows across the tree (weapon-spec procs, Flurry/Enrage triggers, Deep Wounds crit trigger, Mace/Hamstring/Revenge stun-root procs, etc.) |
| weapon_gated (`spell_dbc` 924008–924420, `EquippedItemClass=2`) | 30 | the weapon-specialization node family — nodes `18510/18512/18514/18515/18516/18551`, 5 ranks each = 30 ✓ |
| scripted (`spell_script_names` 924008–932829) | 3 | all three = `era_deep_wounds` on ids `924072/924073/924074` (Deep Wounds R1/R2/R3) ✓ |

**Family-4 SPELLMOD guard (wrong-family SPELLMOD rows) = 0.**
`SELECT COUNT(*) FROM spell_dbc WHERE ID BETWEEN 924008 AND 924420 AND EffectAura_1 IN (107,108)
AND SpellClassSet<>4` returned **0** — every ADD_FLAT_MODIFIER (107) / ADD_PCT_MODIFIER (108)
warrior spellmod row binds to `SpellClassSet=4` (warrior family), none leaked to another family.

### Full-spec headless pass

**Bot:** `Dilkiz` (guid 11, level 80, warrior), verified clean before starting (0
`era_character_talent` rows, 0 custom `character_spell` rows in the warrior band). `ip set Dilkiz
1` → clean baseline: `era=Vanilla storedEra=WotLK available=71 spent=0 generation=819de7d9`, no
orphans, no pet auras.

- Learned **47 ranks across 14 nodes in all three tabs**, deliberately mixing every mechanic type
  and pushing Arms deep enough to clear the 30-point gate:
  - **Arms (32 pts):** `18502`(5, stat) `18503`(3, spellmod) `18501`(2, spellmod) `18509`(3,
    **Deep Wounds — proc-scripted**) `18505`(2, Tactical Mastery clone-grant) `18510`(5, weapon-spec
    stat) `18512`(5, weapon-spec stat) `18513`(1, grant→stock 12328) `18516`(5, weapon-spec stat)
    `18518`(1, grant→stock 12294 Mortal Strike).
  - **Fury (8 pts):** `18519`(5, multi) `18522`(3, proc).
  - **Prot (7 pts):** `18536`(5, stat — Shield Specialization) `18540`(2, multi).
- **Point accounting exact and monotonic, never negative:** final doctor **`available=24
  spent=47`** (71 − 47 = 24; matches the manual rank tally 32 + 8 + 7 = 47). Every intermediate
  learn reported a strictly increasing rank with no rejection.
- **`eratalents doctor` clean:** all 14 learned nodes report `known=yes spellInfo=yes`, resolving
  correctly to both stock grants (`12328`, `12294`) and generated/helper clones (spellmods
  `924009`/`924026`, stats `924020`/`924084`/`924100`/`924132`/`924292`, Deep Wounds top rank
  `924074`, Tactical Mastery helper `932804`, multi `924156`/`924321`, proc `924178`). No
  orphan/missing spellInfo, 0 stray pet auras.
- **Four deliberate reject probes, all fired with the correct message:**
  1. **Max-rank ceiling** — `18503` (already 3/3), 4th rank → `Learn rejected: already max rank`.
  2. **`prereqTalentId` gate** — `18508` (requires `18505` maxed 5/5; it stood at 2/5) →
     `Learn rejected: requires prerequisite talent`.
  3. **`prereqPoints` gate at 25** — `18551` (Prot, gate 25) attempted with Prot cum 7 →
     `Learn rejected: requires 25 points in tree`.
  4. **`prereqPoints` gate at 30** — `18535` (Fury, gate 30) attempted with Fury cum 8 →
     `Learn rejected: requires 30 points in tree`.
- **`eratalents reset Dilkiz`** → `available=71 spent=0`, "no era talents spent this era", 0 band
  auras, 0 pet auras; DB `era_character_talent` = **0 rows** for guid 11 post-reset.
- **Cleanup:** restored the bot to WotLK (`ip set Dilkiz 13`); final check confirmed 0
  `era_character_talent` and 0 orphan `character_spell` rows for guid 11 — dev box left clean.

## Per-mechanic headless evidence

- **Grants — known + applied.** Doctor resolved every grant node to a live spell with
  `known=yes spellInfo=yes`: stock grants (`18513`→`12328`, `18518`→`12294`) and the direct
  clone-grants (`18505`→`932804` Tactical Mastery). See the full grant/clone tables below.
- **Spellmods bind family-4 (guard 0).** The learned spellmod clones carry
  `SpellClassSet=4` with `EffectAura_1 IN (107,108)`: e.g. `924009` (node `18501`) = aura **107**
  (ADD_FLAT_MODIFIER), `924026` (node `18503`) = aura **108** (ADD_PCT_MODIFIER), both
  `SpellClassSet=4`. The wrong-family guard query returned **0** across the whole band — no
  spellmod escapes the warrior family.
- **Stats applied.** Stat nodes (`18502`, `18510`, `18512`, `18516`, `18536`) resolve to their
  generated aura spells and report `known=yes spellInfo=yes` on the bot.
- **Procs wired (flags / hitMask).** `spell_proc` rows resolve for the learned proc nodes:
  Deep Wounds (`924072/073/074`) = `SpellTypeMask=1 ProcFlags=20 HitMask=2 (crit-only) Chance=100`
  on all 3 ranks — the crit→bleed trigger; the `18522` proc (`924176/177/178`) = chance-scaled
  `8/16/24%` per rank. The proc mechanism (row + script/trigger) is present and correctly keyed.
- **Deep Wounds passive live on the bot.** Node `18509` (proc-scripted, `script: era_deep_wounds`)
  learned to rank 3; doctor reports the top-rank passive `924074` `known=yes spellInfo=yes`. The
  three bleed-carrier helper clones `932800`-family (`924072/073/074` carry the trigger; the
  script casts helper `932800`) hold the **Vanilla per-rank amount**: `EffectBasePoints_1` =
  **19 / 39 / 59** → the classic **20 / 40 / 60 %** of average weapon damage, as a
  `PERIODIC_DAMAGE (aura 3)` bleed over 12 s (`durationIndex 29`, 12×1000 ms ticks). The script
  is registered (3 `spell_script_names` rows) and the passive is on the bot.
  - **Live crit→bleed FIRING is deferred to Task 10 (real-client).** Reason: there are no grinding
    combat bots on this dev stack, so a crit that would trigger the `era_deep_wounds` cast cannot
    be observed headlessly. Headless proves the passive is learned, the proc row is crit-keyed
    (HitMask=2, chance 100), the script is bound, and the bleed carrier holds the right amounts —
    everything up to the actual in-combat fire.

## Grant list (node → stock spell id)

Nodes whose ranks resolve to stock (`<900000`) spell ids — 11 nodes:

| Node | Stock id(s) (per rank) |
|---|---|
| 18508 | 12296 |
| 18511 | 16493, 16494 |
| 18513 | 12328 |
| 18518 | 12294 (Mortal Strike) |
| 18520 | 12320, 12852, 12853, 12855, 12856 |
| 18523 | 12329, 12950, 20496 |
| 18524 | 12323 |
| 18527 | 23584, 23585, 23586, 23587, 23588 |
| 18535 | 23881 (Bloodthirst) |
| 18541 | 12975 (Last Stand) |
| 18552 | 23922 (Shield Slam) |

## Clone list (node → helper id) and helper-id claims (932800–932822)

Three nodes **directly grant** an authored clone (the node learns the helper), and six nodes
**trigger/cast** a helper from a proc or script. All 21 claimed helper ids, their owning node,
and mechanism:

| Helper id(s) | Name | Node | Mechanism |
|---|---|---|---|
| 932800 | Deep Wounds bleed | 18509 | proc-scripted (`era_deep_wounds` casts it) |
| 932801 | Mace Specialization stun (3 s) | 18514 | proc trigger |
| 932802 | Improved Hamstring root (5 s) | 18517 | proc trigger |
| 932803–807 | Tactical Mastery (r1–5) | 18505 | **direct clone-grant** |
| 932808 | Death Wish | 18531 | **direct clone-grant** |
| 932810–814 | Flurry haste buffs (r1–5) | 18534 | proc trigger |
| 932815–819 | Enrage melee-damage buffs (r1–5) | 18529 | proc trigger |
| 932821 | Improved Revenge stun (3 s) | 18543 | proc trigger |
| 932822 | Concussion Blow | 18549 | **direct clone-grant** |

`932809` and `932820` are documented as left free (see the `helpers:` block header). The three
direct clone-grants (`18505` Tactical Mastery, `18531` Death Wish, `18549` Concussion Blow) are
the 3 clone-grant nodes that, with the 11 stock-grant nodes above, make up the tree's **14 grant
nodes**.

## Tracked gaps — both CLOSED (Tasks 11–12)

The two gaps recorded at the Task-9 headless pass (stamp `819de7d9`) were subsequently closed by
Tasks 11 and 12; the tree is now **52/52 wired, zero `display_only`** at the current stamp
`005de6c5`. Their live FIRING remains a real-client (Task-10) item — see the checklist.

**(a) Node `18533` Improved Berserker Rage — CLOSED (Task 11).** Re-authored from `display_only`
to `mechanic: scripted`: a `SPELL_EFFECT_DUMMY` marker (misc value **15**) plus the new
`era_improved_berserker_rage` `SpellScript`. The script hooks `AfterCast` on **stock Berserker
Rage `18499`**, and — era-gated on the presence of the misc-15 DUMMY marker aura — energizes the
caster **+5 rage (rank 1) / +10 rage (rank 2)**. The node is now wired (has `era_talent_rank`
rows) and no longer inert. Live rage-on-cast fire is a Task-10 real-client check.

**(b) Shield Specialization (`18536`) — rage-on-block half CLOSED (Task 12).** In addition to the
block-chance stat aura, each rank now grants a per-rank helper (`932823`–`932827`): a
block%-increase aura carrying an **aura-42 (proc-triggered spell)** effect that fires stock
**`12964` (+1 rage)** on a successful block. Made block-gated by a data-driven `spell_proc` row —
`ProcFlags 680`, `HitMask 64` (`PROC_HIT_BLOCK`), `SpellTypeMask 7`, per-rank
`Chance 20/40/60/80/100`. No C++ needed (pure data proc). Live rage-on-block fire is a Task-10
real-client check.

**(c) Minor sanctioned divergences (both documented in-YAML):**
- **Improved Taunt effective cooldown** — the WotLK base Taunt cooldown is 8 s vs Vanilla's 10 s,
  so the reduced cd differs slightly from Vanilla's absolute value.
(The second divergence recorded here — "Defiance always-on vs Defensive-Stance-gated" — was CLOSED
on 2026-08-24 at the Warrior final review: ranks 924352-356 carry `stances: 131072`
(FORM_DEFENSIVESTANCE), so the threat aura applies only in Defensive Stance, matching both the node
tooltip and stock 12303. Bullet removed here at the R7 final review, 2026-09-06.)
The Improved Taunt mapping above is sanctioned/documented, not a defect.

**(d) All live FIRING + cosmetics deferred to real-client Task 10** — Deep Wounds crit→bleed fire,
Improved Berserker Rage rage-on-cast (`era_improved_berserker_rage` on stock `18499`), Shield
Specialization rage-on-block (helpers `932823`–`932827` → `12964`), every proc's in-combat fire
(Flurry, Enrage, weapon-spec procs, Mace/Hamstring/Revenge stuns), tooltips/icons, and authored
stun/root durations. Headless proves structure (rows, scripts, amounts, family binding,
learnability, accounting); only a live client + combat proves the fire. No grinding bots on this
stack → no headless combat observation possible.

## Step 3 — lint + audit + host tests + regen idempotency

- `pytest tools/test_gen_era_talents.py tools/test_import_daribon_talents.py -q` → **101 passed**
  (incl. the two derived guards, one of which — `test_era_audit_default_datasets_match_regen` —
  confirms `era-data/vanilla/warrior.yaml` is present in `era_audit.py`'s `DEFAULT_DATASETS`).
- `tools/era_audit.py` (bare, no args) → **`era_audit: 0 finding(s)`** — now covering all six
  datasets (warrior.yaml confirmed in `DEFAULT_DATASETS`, wired in Task 5).
- `tools/era-regen.sh` re-run → **idempotent**: same stamp `819de7d9`, `git status --short` clean
  (no diff to any tracked generated file).

## Status

**Headless: PASS.** The Task-9 headless sweep (evidence above) ran at stamp `819de7d9`: clean
rebuild + boot (295 nodes, `era_deep_wounds` registered on 3 ranks, stable, no crash), DB integrity
(**family-4 guard = 0**), full-spec headless pass (point accounting exact and monotonic
`71→24 / spent 47`, 4/4 reject probes correct — max-rank cap, `prereqTalentId` gate, `prereqPoints`
gates at 25 and 30 — full doctor clean, reset clean, bot restored), per-mechanic evidence gathered
(grants known+applied, spellmods bind family-4, stats applied, procs wired with crit HitMask/chance,
Deep Wounds passive live with 20/40/60 % per-rank amounts), host tests/audit/regen-idempotency all
green (101/101, 0 findings, idempotent). **No content defects found.**

**Both tracked gaps subsequently CLOSED (Tasks 11–12):** Improved Berserker Rage (`18533`) via the
`era_improved_berserker_rage` `SpellScript` + misc-15 DUMMY marker, and Shield Specialization
(`18536`) rage-on-block via block-gated helpers `932823`–`932827` → `12964`. The tree is now
**52/52 wired, zero `display_only`** and ships **two C++ scripts** (`era_deep_wounds` +
`era_improved_berserker_rage`). A fresh `tools/era-regen.sh` at the current committed stamp
`005de6c5` is **idempotent** (no tracked-file diff). All live proc/bleed/rage/stun FIRING plus
cosmetics remain deferred to the real-client Task-10 checklist below.

Branch merged to `master` 2026-08-25.

## DEFERRED TO REAL-CLIENT PASS (Task 10)

Runtime behaviors headless verification cannot prove — require a live 3.3.5a Vanilla Warrior with
the fresh `patch-V.mpq` (`005de6c5`) + restaged `EraTalents` addon:

1. **Canary green** at login (server + client both `005de6c5`; no red generation-mismatch warning).
2. **3-tab panel** renders with per-tree backgrounds, all 52 nodes positioned (all now wired,
   including `18533`), prereq arrows, real icons.
3. **Deep Wounds** — a physical crit applies the bleed for 20/40/60 % of average weapon damage
   over 12 s (the `era_deep_wounds` script fire).
4. **Weapon-specialization procs** fire only with the matching weapon equipped (the
   `EquippedItemClass=2` gate on nodes 18510/18512/18514/18515/18516/18551).
5. **Authored stuns / root land with the right duration** — Mace Specialization 3 s stun
   (`932801`), Improved Revenge 3 s stun (`932821`), Improved Hamstring 5 s root (`932802`), with
   visible client debuff icons.
5a. **Improved Berserker Rage (`18533`, now wired)** — casting stock Berserker Rage (`18499`)
    energizes **+5 rage rank 1 / +10 rage rank 2** (the `era_improved_berserker_rage` `AfterCast`
    fire, era-gated on the misc-15 DUMMY marker); no rage granted when the talent is unlearned.
5b. **Shield Specialization (`18536`) rage-on-block** — a successful block grants **+1 rage**
    (`12964`), scaling in proc chance with rank (`20/40/60/80/100 %`, `HitMask 64 = PROC_HIT_BLOCK`
    via helpers `932823`–`932827`); rage fires only on a block, not on any other hit.
6. **Flurry (`932810–814`) and Enrage (`932815–819`)** buffs apply and fall off correctly on their
   triggers.
7. **Tactical Mastery (`932803–807`)** retains rage across a stance change; **Death Wish**
   (`932808`) and **Concussion Blow** (`932822`) clone-grants behave like their Vanilla originals.
8. **Cast-bar visuals & tooltips:** era spellbook tooltips (Description) and buff tooltips
   (AuraDescription) read era text; spellbook collapses to highest rank with correct tab.
9. **Improved Berserker Rage (`18533`)** now renders as a fully-wired, learnable node (no longer
   `display_only`) — its live rage-on-cast fire is item 5a above.
10. **Era-transition strip on a real player** (bots never fire `EraTransition::Detect`, so this can
    only be validated on a real player — the documented limitation for every prior class).

Branch merged to `master` 2026-08-25.
