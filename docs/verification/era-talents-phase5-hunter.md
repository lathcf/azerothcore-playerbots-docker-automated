# Era Talents Phase 5 — Vanilla Hunter: Verification Record

> **Generation superseded (2026-09-06 final review):** the current stamp is the closing row of docs/verification/era-talents-final-review-tbc-vanilla.md §10 — reinstall patch-V.mpq + restage the addon before any real-client pass.

**Date:** 2026-08-17
**Branch:** `feat/era-talents` (merged to master 2026-09-07)
**Generation stamp:** `41c1d3b0` (server meta + client patch-V sentinel reconciled)

## Scope delivered

Full Vanilla Hunter talent tree (46 nodes, ids 18301–18346) across Beast Mastery / Marksmanship /
Survival, on the existing `mod-era-talents` pipeline. Reuses the warlock pet path
(`mechanic: pet` + `spell_pet_auras`), adds the per-dataset `petBuffBase: 929000` band + the
`petBuffIds:` hand-authored-carrier override (Spirit Bond's 119 area-aura shape), the Siphon-Life
trainer-rank-chain pattern (Aimed Shot R2–R6, Trueshot Aura R2–R3, Counterattack R2–R3), and the
custom-clone pattern (Bestial Wrath 932967, Deterrence 932964, Wyvern Sting 932965/966). Zero core
patches; no new C++ scripts beyond the reused `spell_hun_bestial_wrath` (IAotH took the data-only
path, so the plan's one optional CheckProc was not needed).

## Step 1 — clean rebuild + boot (four classes at the final stamp)

`tools/era-regen.sh --sync-fork` was idempotent at the committed content (stamp `41c1d3b0`, git
tree clean). Rebuilt (`ac-worldserver` image — C++ cache-hit, unchanged since HEAD), re-ran
`ac-db-import` (exit 0; all three hunter SQL files + trainer_ranks applied, no errors), and
force-recreated `ac-worldserver`.

Boot line:

```
[mod-era-talents] loaded 192 talent nodes (generation 41c1d3b0)
```

= 49 Mage + 47 Priest + 50 Warlock + 46 Hunter, one generation stamp. No
`spell_proc` / `spell_pet_auras` / `spell_ranks` / `trainer` / `sql.sql` errors for our id bands;
no generic table-load errors. Stable post-boot window verified (worldserver ran the full sweep,
Up 36 min at close, **0 crash / segfault / assertion indicators** in the log).

## Step 2 — DB integrity

```
hunter_nodes  unwired  hunter_procs  hunter_petauras  trainer_rows
46            0        17            23               9
```

| Metric | Value | Reconciliation |
|---|---|---|
| hunter_nodes (`era_talent` classId=3) | 46 | 46 ✓ |
| unwired (nodes with no `era_talent_rank`) | 0 | every node wired ✓ |
| hunter proc rows (`spell_proc` 922408–922775) | 17 | Improved Mend Pet 2 + Improved Concussive Shot 5 + Entrapment 5 + Improved Wing Clip 5 |
| hunter pet-aura rows (`spell_pet_auras` 922408–922775) | 23 | per-rank mappings across the pet nodes (incl. the `petBuffIds` Spirit Bond carriers) |
| trainer_rows (`trainer_spell` 932952–932963) | **9** | Aimed Shot R2–R6 (5) + Trueshot Aura R2–R3 (2) + **Counterattack R2–R3 (2)** |

**trainer_rows reconciliation (plan said 7, actual 9):** the plan's baseline of 7 was Aimed +
Trueshot only; Task 7 found Vanilla Counterattack had trainer ranks, so Task 8 added the
Counterattack R2–R3 chain (ids 932958/932959, which fall inside the 932952–963 query window). The
hand-written `2026_08_16_22_era_talent_hunter_trainer_ranks.sql` intends exactly these 9 rows, all
present with correct `ReqAbility1` chaining and Vanilla levels/costs:

```
SpellId  ReqAbility1  ReqLevel  MoneyCost
932953   932952       28        400     Aimed Shot R2  (req talent-granted R1)
932954   932953       36        700     Aimed Shot R3
932955   932954       44        1300    Aimed Shot R4
932956   932955       52        2000    Aimed Shot R5
932957   932956       60        2500    Aimed Shot R6
932958   932979       42        1200    Counterattack R2 (req talent-granted R1 932979)
932959   932958       54        2100    Counterattack R3
932962   932961       50        8000    Trueshot Aura R2 (req talent-granted R1 932961)
932963   932962       60        16000   Trueshot Aura R3
```

`spell_ranks` cascade chains load cleanly (no boot errors): `first_spell_id` 932952 (Aimed, ranks
1–6), 932961 (Trueshot, ranks 1–3), 932979 (Counterattack, ranks 1–3). All three chains are
mirrored in `EraTalents.cpp`'s `kHunterTrainedChains[]` strip table (per-talent gating: 18321→Aimed
R2-6, 18330→Trueshot R2-3, 18344→Counterattack R2-3).

## Step 2 — full-spec headless (bot `Zaraneda`, level 80, `ip set 1` → era=Vanilla)

**71 talent points available** at level 80 (Vanilla pool). Learned a full 71-point spec across all
three tabs (BM 31, MM 31, SV 9), one rank per `eratalents learn` call:

- **Point accounting never negative:** available walked 71 → 0 monotonically; final
  `available=0 spent=71`.
- **Prereqs enforced:** a deliberate probe of `learn 18316` (Bestial Wrath) with 0 points spent was
  rejected — `Learn rejected: requires 30 points in tree` — and later succeeded once the 30-in-tab +
  18313 prereqs were met. Tier gates and node prereqs (18315←18311, 18316←18313, 18325←18320,
  18330←18327) all held.
- **`eratalents doctor` clean at full spec:** all 21 learned nodes report `grant … known=yes
  spellInfo=yes`. Grants resolve correctly to stock ids (19577 Intimidation, 19625 Frenzy, 19431
  Lethal Shots, 19490 Mortal Shots, 19503 Scatter Shot) and custom clones (932967 Bestial Wrath +
  `spell_hun_bestial_wrath` script, 932952 Aimed Shot, 932961 Trueshot Aura). No orphan/missing
  spellInfo.
- **Reset clears everything:** `eratalents reset` returned `available=71 spent=0`, "no era talents
  spent this era", and stripped the pet band auras (see below).

### Pet-aura path (application, fresh-summon persistence, strip)

- **Application (full spec):** the live pet (entry 1996) carried all pet-node band auras
  `applied=yes` — 929001, 929196, 932969, 932977 — with `m_petAuras registered: 5` mapping each buff
  to the current pet entry. The DUMMY-marker carriers for the pet passives (922417/922476/922492/
  922497) are present on the player.
- **Spirit Bond 119 carrier (18312, `petBuffIds: [932968, 932969]`):** the rank-2 helper 932969
  radiates from the **pet** — doctor shows `932969 applied on player (owned by caster … Type: Pet)`
  AND `932969 applied=yes` on the pet itself. This is the correct 119 area-aura semantics (the owner
  gains the radiated aura; caster = the pet).
- **Fresh-summon persistence (the mandatory resummon check) — PASS:** during the sweep the bot
  dismissed its original pet (entry 1996) and summoned a **new, different pet (entry 3124)**. The
  fresh pet automatically carried buff **929001** (Endurance Training, node 18302) `applied=yes`,
  with max HP raised to 10697 (the health-% buff applied on summon). A brand-new pet receiving the
  pet-aura buff with no manual intervention is the core `Pet::CastPetAuras`-on-summon path proven
  end-to-end: `m_petAuras` persisted across the dismiss and re-cast onto the new summon.
- **Strip (measurable):** after `eratalents reset` the pet's band auras dropped to `(none)` and the
  pet's **max HP fell 10363 → 9776** — the Endurance Training health-% aura visibly removed. A
  re-learn re-registered the mapping (immediate-reapply path, `EraTalents.cpp:151`).

### Aimed Shot cast time / cooldown (932952)

`spell_dbc`: `CastingTimeIndex=14`, `RecoveryTime=6000`, `RangeIndex=114`. Client
`SpellCastTimes.dbc` (extracted from the running server) row 14 = **3000 ms base** — authoritatively
confirmed, not assumed. So Aimed Shot R1 = **3 s cast, 6 s cooldown** (Vanilla-faithful; the WotLK
stock 20900 is instant with a 10 s category cooldown — the divergence the clone exists for). Bestial
Wrath 932967 = instant + 120000 ms (2 min) cooldown; Trueshot 932961 = instant aura. (The visible
cast bar is a Task 10 client check.)

### Trap proc (Entrapment 18334) — the `attrMask: 2` / trap-activation shape

`spell_proc` rows 922672–922676: `ProcFlags=2097152` (0x200000 `PROC_FLAG_DONE_TRAP_ACTIVATION`),
`SpellPhaseMask=4` (FINISH), chance 5/10/15/20/25. This matches the framework's trap-proc spec
exactly (the FINISH phase + trap-activation flag the stock DBC-only proc cannot scope). The live
root-lands-on-target fire is a Task 10 client check.

### Era-transition strip — expected bot limitation (see Findings)

## Step 3 — lint + audit + host tests + regen idempotency

- `pytest tools/test_gen_era_talents.py tools/test_import_daribon_talents.py` → **95 passed**.
- `tools/era_audit.py` → **0 findings** (Hunter family 9 predicates green).
- `tools/era-regen.sh` re-run → **git tree clean** (idempotent, no stamp/artifact churn).

## Findings

### Not a defect — era-transition strip on a bot leaves an ORPHAN (documented bot-hook limitation)

After `ip set Zaraneda 13` (Vanilla→WotLK), `eratalents doctor` reported
`ORPHAN ERA SPELL: 922417 (known but no learned talent grants it)` — the Vanilla Endurance Training
passive stayed on the character in WotLK era.

**Root cause is the documented "bots skip transition hooks" limitation, not a strip-logic bug:**

- `EraTransition::Run` (`EraTransition.cpp:59–63`) correctly strips the leaving era on a real
  crossing: `if (from != to) … if (from != ERA_WOTLK) EraTalents::Reset(p, from)` removes the granted
  passives and deletes the era's rows.
- The strip fires from `EraTransition::Detect`, which compares `StoredEra` (persisted `lastEra`) vs
  the live IP era. A bot's `.ip set` mutates progression **without** firing the hook that runs
  `Detect`, so `storedEra` stayed **WotLK the entire time** (confirmed in every doctor:
  `storedEra=WotLK` even while `era=Vanilla`). The return to WotLK therefore reads as
  `stored == now` → **no crossing detected** → strip never invoked. The doctor's `ORPHAN ERA SPELL`
  diagnostic exists precisely to surface this.
- On a **real player**, entering Vanilla fires `Detect` (stored WotLK ≠ now Vanilla → `Run` → Pin,
  `storedEra=Vanilla`); leaving to WotLK fires `Detect` again (stored Vanilla ≠ now WotLK → `Run` →
  `Reset(from=Vanilla)` strips the passives + trained ranks). The strip mechanism is sound in code;
  only its live behavioral proof is deferred to the Task 10 real-player pass.

Confirmed by cleanup: returning the bot to Vanilla and running `eratalents reset` removed 922417 and
all rows (0 residue), proving the `Reset(from)` path that a real crossing invokes does strip cleanly.

**No content or code defects found in this pass.** Improved Aspect of the Hawk (18301) has no
`spell_proc` row and that is correct, not a gap — it is a plain `mechanic: spellmod` node.

**CORRECTED 2026-09-06.** This paragraph originally described 18301 as a `mechanic: multi` node with
TWO spellmods — one raising Aspect of the Hawk 13165's own aura-42 proc chance, one supplying Quick
Shots 6150's haste. That model was RETIRED on **2026-08-24 (fix F-4)** as not-Vanilla: it reproduced
the WotLK mechanic off a 1.15 source. 1.12.1's Improved Aspect of the Hawk 19552-19556 is a SINGLE
Effect_1 (`aura 108 ADD_PCT_MODIFIER`, op `SPELLMOD_ALL_EFFECTS`, masked to Aspect of the Hawk's
family bit 0x100000, basePoints 3/7/11/15/19 + dieSides 1), and base Aspect of the Hawk 13165 in
1.12.1 carries no aura-42 proc at all — there is no "Quick Shots" in the 1.12.1 data (6150 is a
portal spell there; a full-DBC name search finds none). What ships today is that single
`op: allEffects` +4/8/12/16/20% spellmod on the aspect's ranged-attack-power bonus, and the Quick
Shots haste the old model delivered is deliberately gone (full-accuracy policy). The rationale lives
in the node's block comment in `era-data/vanilla/hunter.yaml`.

## Named tracked gaps / accepted deviations (carried from Tasks 6–8)

All named-in-YAML, none silently shipped:

1. **Improved Feign Death (18342) — behaviorally INERT on this core.** Rows are era-faithful
   (`op: resistMiss` +2/4%), but `AuraEffect::HandleFeignDeath` computes no resist roll at all
   (`SpellAuraEffects.cpp:2931`, the resisted branch is commented out — as retail WotLK ships). The
   two points buy nothing today; the talent would engage if the core restored the roll. Mirrors the
   stock 37484 shape.
2. **Bestial Swiftness (18308) — no outdoor gate.** Vanilla gated the +30% pet speed to outdoors; a
   `mechanic: pet` buff carries fixed passive attributes (no attribute hook) and the core only
   area-gates PLAYER auras, so the +30% also applies indoors (STRONGER than 1.12 — which is how
   WotLK's Boar's Speed also dropped the gate). Tooltip keeps the 1.12 wording.
3. **Improved Scorpid Sting (18328) — CLOSED 2026-08-24, the authentic mechanic ships.** This
   entry originally recorded a pragmatic approximation (`op: allEffects` +10/20/30% on the WotLK
   Scorpid Sting's effect), because Vanilla's real mechanic — reduce the target's Stamina by a % of
   the Strength that Vanilla Scorpid Sting removed — has nothing to bind to when the hunter trains
   the WotLK hit-chance rework. The 2026-08-24 rework removed that premise: the module now ships the
   authentic Vanilla Scorpid Sting itself (clones **932300-932303**, `aura 29 MOD_STAT` Str/Agi
   reduction), and this node is `mechanic: scripted` — a band-gated hidden DUMMY marker per rank
   (**misc 16**, amount 10/20/30) that self-applies on learn, read off the casting hunter by the
   AuraScript **`era_hunter_scorpid_sting`** (bound to all four clones), which sets the clone's
   Effect_3 (`MOD_STAT` Stamina) to -pct% of the Strength it removed, and to 0 without the talent.
   Not a deviation any more.
   **One accepted, owner-confirmed (2026-08-25) authentic behaviour remains:** the Stamina reduction
   lowers max health for PLAYER targets only. On creatures it has no health effect —
   `Creature::UpdateMaxHealth` reads `UNIT_MOD_HEALTH` and `Creature::UpdateStats` is a no-op, so a
   `MOD_STAT` Stamina never touches a mob's health pool. That matches real Vanilla (Improved Scorpid
   Sting was effectively a PvP talent); authenticity was chosen over a PvE-visible deviation.
4. **Efficiency (18318) does not reach the Aimed Shot clone.** The Aimed Shot clone carries **family
   0** (to escape the HUNTER-family cooldown corrections sweep and keep its 6 s `RecoveryTime`), so
   Efficiency's family-mask cost SPELLMOD cannot reach it — the era Aimed Shot's mana is not reduced
   by Efficiency (Vanilla's was). All other Vanilla shots/stings are covered; the 6 s cooldown
   fidelity was judged worth the lost cost reduction.
   **RETIRED 2026-09-05 (TBC Phase 7 back-port, commit `2a414c4`):** the six clones carry family 9 +
   word2 bit 21 and Efficiency's mask reaches them; the 6 s cooldown survives because the correction
   keys on word0 `0x20000`, which the clones never carried. The TBC hunter phase's free-bit census
   found word0 saturated and word2 bits 21-30 free in BOTH stores, which is what made the third
   option — keep the family, move the identity — available where the Vanilla phase saw only
   "family 9 with the trapped bit" vs "family 0". Ids are unchanged, so no `character_spell`
   migration was needed. Live confirmation at gen `76709510`: 932952/932957 read `SpellClassSet 9`,
   mask `(0, 0, 2097152)`, `CastingTimeIndex 14` (3 s — Vanilla's own), `RecoveryTime 6000`;
   Efficiency 922544-922548 read mask `(64000, 128, 2097152)`. Real-client re-check items are in
   `docs/verification/era-talents-tbc-phase7-hunter.md` §19 items 27-28 (the second is the
   separately bundled Trueshot 932962/932963 re-price to 1800/2500).
5. **Lethal Shots (18320) & Ranged Weapon Specialization (18329) — all-weapon-crit / all-school
   scope.** Lethal Shots grants stock aura 52 (recomputes ALL weapon crit, so it also raises melee
   crit); RWS uses aura 79 misc 127 (all-school damage %, not ranged-weapon-only). No ranged-only
   weapon-crit-% or ranged-weapon-only damage aura exists in 3.3.5a — this is the same scope the
   stock chains ship.
6. **Frost Trap initial slow (67035) unreachable by spellmods.** 67035 carries an all-zero family
   mask (`_ref.trapEffects`), so no Clever Traps / Trap Mastery spellmod can stretch its per-target
   10 s slow duration or resist chance. It rides Frost Trap Aura 13810's own (masked) duration, so
   the era-visible ground-cloud duration IS extended; only that one single-target application stays
   stock. Named gap on both Clever Traps (18337) and Trap Mastery (18340).
7. **Trueshot Aura (18330) caster-extra omitted.** Wowhead Classic renders an extra caster-only
   "+101/151/201 ranged AP" line per rank; this era version implements the documented party +AP
   tooltip only (the area aura already applies to the caster; the doubled caster figure is not in the
   Daribon tooltip).

## DEFERRED TO REAL-CLIENT PASS (Task 10)

Runtime behaviors headless verification cannot prove — require a live 3.3.5a Vanilla Hunter with the
fresh `patch-V.mpq` + restaged `EraTalents` addon:

1. **Canary green** at login (server + client both `41c1d3b0`; no red generation-mismatch warning).
2. **3-tab panel** renders with per-tree backgrounds (not identical), all 46 nodes positioned,
   prereq arrows, real icons (no "?").
3. **Proc fires** (headless can only confirm the `spell_proc` rows, not the fire on a normal hit):
   Improved Concussive Shot stun on a Concussive hit; Entrapment root on a sprung trap; Improved Wing
   Clip root; Improved Mend Pet dispel on a Mend Pet tick; Frenzy pet-crit haste buff on the pet.
   (The "**Quick Shots** proc during sustained auto-shot with Hawk up" item was DELETED 2026-09-06 —
   it is not runnable: the F-4 fix of 2026-08-24 retired the Quick Shots model entirely, so there is
   no proc to watch for. Improved Aspect of the Hawk is now a plain spellmod and is verified in item
   6's family instead: the aspect's +RAP on the character sheet moves by 4/8/12/16/20%.)
4. **Cast-bar visuals & tooltips:** Aimed Shot 3 s cast bar; era spellbook tooltips (Description) and
   buff tooltips (AuraDescription) read era text; spellbook collapses to highest rank with "Rank N"
   label in the correct tab.
5. **Trainer purchase:** the hunter trainer sells Aimed Shot R2+, Trueshot R2+, Counterattack R2+ at
   the right levels; ranks chain via `ReqAbility1`.
6. **Party-AP on a real member:** Trueshot Aura grants a grouped player flat +RAP (character sheet).
7. **Pet-stat / Spirit Bond in the client:** pet sheet/damage moves; Spirit Bond buff icon on hunter
   AND pet, both tick 1–2% health/10 s; Bestial Wrath 18 s duration + era tooltip.
8. **Era-transition strip on a real player** — the definitive proof for the Finding above: entering
   Vanilla and leaving to WotLK fires `EraTransition::Detect`/`Run`, stripping the Vanilla talent
   grants AND the trained Aimed Shot / Trueshot / Counterattack ranks (no relog needed). Bots bypass
   this hook (they never move `storedEra`), so it can only be validated on a real player.
9. **Multi-chunk SYNC** at full-tree scale. The SYNC payload is whispered over the addon channel
   (`LANG_ADDON`) — not observable headlessly. The chunking logic is present
   (`EraTalentsComms::BuildSync`, 230-char body budget with `<seq>/<total>` sentinels); a 21-node /
   71-point spec fits in a single chunk (~177 chars), so multi-chunk requires a spread spec touching
   ~28+ distinct nodes. Verify the accumulate-across-chunks behavior client-side.
10. **Slaying talents** damage rise vs a Beast (before/after); routine spellmod tooltip rewrites
    (Efficiency / Mortal Shots / Hawk Eye); priest **Force of Will** icon rider now
    `spell_nature_slowingtotem`.

## Status

**Headless: PASS.** Boot (192 nodes @ `41c1d3b0`, stable, no crash), DB integrity (46/0/17/23/9,
trainer_rows reconciled to 9 via Counterattack), full 71-point spec (point accounting, prereq gates,
reset), pet-aura application + **fresh-summon persistence (pet 3124 auto-buffed)** + measurable
strip, Spirit Bond 119 carrier both-sides, Aimed Shot 3 s/6 s (client DBC-confirmed), Entrapment
trap-proc shape, trainer + `spell_ranks` chains, lint/audit/tests/regen-idempotency all green. One
**Finding** (era-transition ORPHAN on a bot) diagnosed as the documented bot-hook-bypass limitation,
not a defect — the strip code path is sound and cleans on `Reset`. No content or code defects found.

**Real-client (Task 10): PASS (user-verified 2026-08-17).** The user ran the full checklist on a real
3.3.5a Vanilla Hunter: canary green, 3-tab panel + icons + backgrounds, Aimed Shot cast/trainer ranks,
pet path (Spirit Bond, pet-stat), procs, era transition — all confirmed working ("everything looks
good now"). Dev box left clean: bot talents reset, `era_character_talent` rows deleted (0 residue),
orphan grant scrubbed, era restored to WotLK; `playerbots.conf` `BotActiveAlone` restored to 10.

**Real-client issue found + fixed — pet talent tree access (addon-only, commits `65675f5`, `dcd39b4`,
`7a7946a`, `920bade`):** the EraTalents addon replaces the ENTIRE `PlayerTalentFrame` for era chars,
but the WotLK hunter PET talent tree (Ferocity/Cunning/Tenacity) lives as a side spec-tab INSIDE that
same stock frame — so an era hunter had no route to it (pet talents were deliberately left on the
stock WotLK system, but access went down with the suppressed frame). Fix: `ET.OpenStockTalents`
reopens the stock frame past the era guard (one-shot `_allowStock` flag), exposed as a hunter-only
"Pet Talents" button on the era frame + a `/pettalents` slash. It gates on `UnitExists("pet")`,
auto-clicks the pet spec tab (`PlayerSpecTab` whose `specIndex` is a `"petspec..."` string — Blizzard's
`GetPetTalentTree()` then shows the ACTIVE pet's own tree, so it tracks pet swaps with no hardcoding),
and confines the view to pet-only by alpha/EnableMouse-hiding the player spec tab(s) on every frame
update (NOT `Hide()` — that flips `IsShown()` which Blizzard's layout fights, and is more taint-prone),
restored on frame close. Pet-talent learning (stock protected action) confirmed unaffected. Addon-only:
no generation-stamp/MPQ/server change (the canary stays green).

Branch merged to master 2026-09-07.
