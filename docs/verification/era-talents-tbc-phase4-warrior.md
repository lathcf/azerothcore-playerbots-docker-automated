# Era Talents — TBC Phase 4 (Warrior) verification record

> **Generation superseded (2026-09-06 final review):** the current stamp is the closing row of docs/verification/era-talents-final-review-tbc-vanilla.md §10 — reinstall patch-V.mpq + restage the addon before any real-client pass.

**Status: HEADLESS-COMPLETE, AWAITING REAL-CLIENT SIGN-OFF (generation `85d67983`).** All 66 nodes
authored and wired, `era_audit.py` clean, 204/204 host tests green, the four-rung band-move doctor
ladder zero-orphan on live bots, and all 66 nodes exercised by real bot builds. Branch
`feat/era-talents`, **merged to master 2026-09-07** (TBC merges as a set after all nine classes + final review +
Vanilla regression). The `EraTalentIP.cpp` `ERA_TBC` allowlist flip remains an **uncommitted
dev-box edit** (verified ` M` and unstaged at close-out; no phase commit touches the file).

**Spec/plan:** `docs/superpowers/specs/2026-09-02-era-talents-tbc-phase4-warrior-content-design.md`
(**Amendment A + A.11 + the A.11 Task-8 addendum supersede the body**) +
`docs/superpowers/plans/2026-09-02-era-talents-tbc-phase4-warrior.md`.
**Findings authority:** `era-data/_ref/tbc/warrior-spells.yaml` (`survival_map:` / `id_allocation:` /
`reuse_ledger:` / `script_census:` / `capstone_chains:` / `accepted_gaps:` / `open_questions:`) —
this record summarizes; the `_ref` is normative.

**Phase commits:** `84dc77d` T1 · `2c6473a` T2 · `e090abd` T3 (ref) · `98cf0be` Amendment A ·
`c758fc1` T4 (wiring) · `89a8b60` + `17d6aa5` T5 (substrate) · `c1d14ed` T6 (Arms) · `e6c819e`
A.11 · `51dce26` T7 (Fury) · `901d99a` A.11 addendum · `18ca7e4` T8 (Protection) · `0754a36` T9
(this record + framework/triage updates) · `a994cde` + `ba25633` §11a/§11b (patch 0021 by-id
routing + era-rename alias) · **the C-1 fix round** (final-review findings; see §11c).

---

## 1. What shipped

The complete **TBC 2.4.3 Warrior talent tree** — **66 nodes** (Arms 23 / Fury 21 / Protection 22,
ids **20300–20365**, era 1) — plus four trainer-taught era clone chains (Bloodthirst, Rampage,
Shield Slam, Devastate), the Rampage reconstruction, and two new generator keys. Value basis:
**wago 2.5.4.44833** end-to-end. **Zero new core patches this phase.**

- **Class 1, spell family 4.** Family guard clean: 0 rows in either band with `SpellClassSet`
  outside `{0, 4}`.
- **Auto-passives:** band `938400–938927` (`936000 + (nodeId−20000)*8 + (rank−1)`).
- **Hand helpers:** **56 ids, `947512–947567`** — the whole reserved block `947512–947550` plus
  the Task-6/7/8 claims `947551–947567`. **Free within the warrior slice: `947568–947767`.**
- **NO DUMMY-marker misc consumed.** Node 20337 (Improved Berserker Rage) re-uses the existing
  misc **15** marker that `era_improved_berserker_rage` already reads. Registry next-free stays
  **19**.
- **SQL:** `2026_09_03_00_..._data.sql` + `_01_..._custom_spells.sql` (generated);
  `2026_09_03_02_..._fury_chains.sql` and `_03_..._prot_chains.sql` (hand — 7 `trainer_spell` +
  9 `spell_ranks` rows each). **Nothing deleted and nothing re-gated anywhere** — every stock chain
  this phase touches is already self-gating (its `ReqAbility1` roots at a stock id a TBC warrior
  never holds), so the orphaned-higher-rank check had nothing to flag.
- **C++:** `EraTalents.cpp` (Shield Slam gate three-way; Bloodthirst / Last Stand three-way arms;
  the table-driven cross-era strip arm 5 including the Death Wish / Sweeping Strikes swap rows;
  the Rampage crit-watcher `learnSpell` arm 6), `EraTalentsCommand.cpp` (node-gated doctor
  predicates for the four chains + 947539), `EraTalentProcScripts.cpp` (`era_deep_wounds` given a
  per-era carrier).
- **Generator:** two `_HELPER_SCALARS` entries — `casterAuraSpell` (the Rampage crit gate) and
  `dispelType` (Death Wish / Enrage). Both byte-neutral for every shipped helper; TDD guards
  `test_helper_caster_aura_spell_column`, `test_helper_dispel_type_column`, plus the Task-1
  window guard `test_warrior_and_era1_windows_present`.

## 2. Per-tab node counts and disposition tallies

All 66 wired, 0 display-only. Derived from `era-data/tbc/warrior.yaml` at generation `85d67983`.

| Tab | Nodes | grant | spellmod | stat | multi | proc | scripted |
|---|---|---|---|---|---|---|---|
| Arms (20300–20322) | 23 | 3 | 6 | 5 | 5 | 3 | 1 (`proc-scripted`) |
| Fury (20323–20343) | 21 | 4 | 6 | 3 | 3 | 4 | 1 |
| Protection (20344–20365) | 22 | 6 | 6 | 5 | 3 | 2 | 0 |
| **Total** | **66** | **13** | **18** | **13** | **11** | **9** | **2** |

**Nodes whose grant is a band CLONE (9):** Arms 20312 Death Wish → 947548 · Fury 20335 Sweeping
Strikes → 947549, 20340 Bloodthirst → 947512, 20343 Rampage → 947533 · Protection 20347 Shield
Specialization → 947561–947565, 20349 Last Stand → 947547, 20357 Concussion Blow → 947550,
20362 Shield Slam → 947541, 20365 Devastate → 947530. The remaining four `grant` nodes hand out
**stock** ids (e.g. 20320 Second Wind 29834/29838 — grant-stock is *mandatory* there, see §4).

## 3. Survival-map verdict distribution

**308 ids swept across 14 verdict-bearing field families** (A effects · B proc options · C misc
incl. all eight attribute words · D categories · E power · F class options word 0 · G cooldowns ·
H aura restrictions · I shapeshift · J levels · K equipped item · L interrupts · M targets ·
N identity/name; O per-effect class masks is a supplementary pass recorded in
`effect_class_mask_diffs:` and deliberately does not feed a verdict).

| Verdict | Count |
|---|---|
| DIVERGED | 158 |
| LIVE_MATCH | 100 |
| LIVE_MATCH_UNVERIFIED_SLOT | 12 |
| WAGO_ONLY | 24 |
| LIVE_ONLY | 14 |
| **Total** | **308** |

Three families were **not** in the plan's sweep script and each changed an outcome: **H**
(aura restrictions) found TBC Rampage's `CasterAuraState 11` crit gate; **I** (shapeshift) found
Berserker Rage 18499 carrying `ShapeshiftMask 0x40000` in TBC and 0 live; **K** (equipped item)
found TBC Devastate requiring a **one-handed weapon** where live requires a **shield**. The
`G_cooldowns` family alone moved Bloodthirst ranks 2–6 from LIVE_MATCH to DIVERGED (TBC
`CategoryRecoveryTime` 6000 ms vs live 4000 ms) — an effects-only sweep would have granted stock
and shipped a 4-second Bloodthirst.

## 4. Reuse ledger — outcome

**ALL FRESH or MOOT: zero of the fourteen Vanilla warrior helper families is reused.** Eight
FRESH, two FRESH-but-close (`932815–819` Enrage, `932822` Concussion Blow), four MOOT (the TBC
node grants stock instead).

**The exemplar is Flurry `932810–932814`** — *"MOOT — no clone needed, AND reuse would have been
WRONG."* TBC 12966–12970 are LIVE_MATCH on all fourteen families (+5/10/15/20/25% melee haste),
so node 20338 triggers the **stock** buffs; the shipped Vanilla clones are Vanilla's 1.12.1
+10/15/20/25/30%, so reusing them would have overpaid every rank by 5 percentage points. This is
the ledger's clearest statement of why the default must be FRESH-or-stock and never "the previous
era already built one with this name".

Two entries worth carrying forward: **Deep Wounds `932800`** is FRESH because its
`EffectAuraPeriod` is 1000 ms (12 ticks, inherited from the WotLK carrier the Vanilla clone
templated off) against TBC's 3000 ms (4 ticks) — a difference invisible to an effects-value
comparison; **Mace Spec `932801`** is FRESH because TBC's 5530 carries a second effect
(`ENERGIZE` 7 rage) the Vanilla stun payload has no trace of.

## 5. Fresh-clone id table (used sub-range `947512–947567`)

| Ids | What |
|---|---|
| 947512–947529 | **Bloodthirst r1–r6** — six damage clones (45% AP, 30 rage, 6 s category cd) + six 8 s/5-charge heal buffs + six FLAT heal payloads (10/13/17/20/25/30). Bound to `era_bloodthirst_vanilla`. |
| 947530–947532 | **Devastate r1–r3** — one-handed-weapon gate; the per-Sunder bonus moved to DBC slot 3 (core reads `CalculateSpellDamage(2, …)`). |
| 947533–947540 | **Rampage r1–r3** + three AP stack buffs (+30/40/50, `CumulativeAura 5`) + the crit-watcher passive **947539** + the 5 s "Rampage Ready" aura **947540**. |
| 947541–947546 | **Shield Slam r1–r6** at TBC's damage ranges 225-235 / 264-276 / 303-317 / 342-358 / 381-399 / 420-440. **Category 1209 KEPT** (not neutralised to TBC's 971) because `SpellEffects.cpp:351` gates the shield-block-value bonus on `GetCategory() == 1209`. |
| 947547 | **Last Stand** — category neutralised, `RecoveryTime` 480000 (8 min); `era_last_stand` rebound. |
| 947548 | **Death Wish** — `mechanic: 0` + `dispelType: 0` (TBC's Death Wish is neither an Enrage effect nor Enrage-dispellable). |
| 947549 | **Sweeping Strikes** — 10 charges / 10 s (live is 5 / 30 s); core `spell_warr_sweeping_strikes` rebound (its body is id-agnostic). |
| 947550 | **Concussion Blow** — stun-only, `RecoveryTime` 45000, spell-level Mechanic 12. **No script bind** (deliberate). |
| 947551 | **Deep Wound bleed carrier** — from scratch, 3000 ms × 12 s = 4 ticks, family 4 + maskB 0x10 so Blood Frenzy binds it, without live 12721's two WotLK ignore-modifier attribute bits. |
| 947552 | **Mace Specialization payload** — 3 s stun *plus* TBC's added 7-rage `ENERGIZE`. From scratch (live 5530 is a repurposed id). |
| 947553–947555 | **Blood Craze** per-rank regen payloads (6000/3000/2000 ms tick over a 6 s window). |
| 947556–947560 | **Enrage** buffs at TBC's +5/10/15/20/25% (live is +2/4/6/8/10), `dispelType: 0` + `mechanic: 0` so the era buff is not soothable. |
| 947561–947565 | **Shield Specialization** per-rank passives carrying BOTH halves (+1..5% block *and* the aura-42 proc), each with its own `spell_proc` row at **HitMask 64 (PROC_HIT_BLOCK)** — live's row for −12298 carries HitMask 112, WotLK's widened dodge/parry/block rule. |
| 947566 | The 1-rage `ENERGIZE` payload those five trigger (wago 23602 = 1 rage; live 23602 = 5). |
| 947567 | **Shield Bash − Silenced** — 3 s silence, `preventionType: 1`, `attributesEx2: 0`, `attributesEx3: 0`, `family: 0` (18498's SpellClassSet 8 is a Blizzard oddity and is inert either way; authored 0 to keep the band inside the `{0, 4}` family invariant). |

**Next free in the warrior slice: `947568`. Free-within-slice `947568–947767`. The next class
phase claims from `947768`.**

## 6. Capstone chains

| Capstone | Node | Ids | Trainer rows added (TrainerId 1) |
|---|---|---|---|
| **Rampage** (Fury) | 20343 | 947533–947535 (+ 947536–947540) | 947534 @ **60** (req 947533), 947535 @ **70** (req 947534) |
| **Bloodthirst** (Fury) | 20340 | 947512–947517 | 947513 @ **48**, 947514 @ **54**, 947515 @ **60**, 947516 @ **66**, 947517 @ **70** (each req the previous rank) |
| **Shield Slam** (Prot) | 20362 | 947541–947546 | 947542 @ **48**, 947543 @ **54**, 947544 @ **60**, 947545 @ **66**, 947546 @ **70** |
| **Devastate** (Prot) | 20365 | 947530–947532 | 947531 @ **60** (req 947530), 947532 @ **70** (req 947531) |

**14 trainer rows and 18 `spell_ranks` rows** verified present in `acore_world` and rooted at the
talent-granted r1s (§9). **No DELETE and no re-gate was needed** — stock 30016's `ReqAbility1` is
stock 20243, which a TBC warrior never holds, and stock Bloodthirst has no `trainer_spell` /
`spell_ranks` rows on this server at all.

**The Rampage reconstruction** is the phase's flagged rebuild and is escalation-ladder rung 1
(pure data + one generator key — no module script, no core patch): TBC's `CasterAuraState 11`
crit gate is **dead on this core** (AzerothCore's `AuraStateType` has no 11 and nothing in
`src/server/game` ever sets it), so the 5 s window is reproduced as a real aura (947540) applied
by a crit-only proc passive (947539) with `casterAuraSpell:` pointing at it. TBC's own
indirection through the bare DUMMY 18350 is deliberately not copied (the Mangle-DUMMY lesson).
The one flagged delta — the ready aura surviving the cast, so a second Rampage inside the same
5 s window stayed castable (`accepted_gaps` id 4) — was **CLOSED 2026-09-07** (deferred-gaps
round, G-12) by the module SpellScript `era_rampage_consume_ready`, whose `AfterCast` removes
947540; it is bound to the three Rampage clones 947533/947534/947535.

## 7. Script census (19 rows) and bindings

The census over the 308-id set returned **19** `spell_script_names` rows. Dispositions:

- **1 rebind** — core `spell_warr_sweeping_strikes` reproduced on clone **947549** (its AuraScript
  body references only the live payloads 12723/26654, so it is id-agnostic).
- **1 module-script reuse on a STOCK id** — `era_improved_berserker_rage` is bound to stock
  Berserker Rage 18499 and gated on a band-ranged DUMMY marker with **misc 15**, so it already
  fires for any era that mints that marker. TBC node 20337 mints it at 5/10 rage, needing no new
  C++ and no new binding. *This row was unpredicted by the plan and is what makes 20337 free.*
- **Explicit do-not-rebinds** — `spell_warr_bloodthirst` / `_heal` (their heal is a percent of max
  health; TBC's is flat, so the era route uses `era_bloodthirst_vanilla` + flat-heal helpers),
  `spell_warr_concussion_blow` (forbidden — see `node_buckets.clone[20357]`),
  `spell_warr_deep_wounds` / `_aura` (the WotLK `16*rank/6` formula), `spell_warr_last_stand`
  (replaced by the module's `era_last_stand`, which already carries the era cooldown),
  `spell_gen_proc_from_direct_damage` on the Enrage chain (the era node authors its own proc row),
  and the six baseline scripts (Charge / Execute / Intimidating Shout / Overpower / Rend /
  Retaliation / Slam) whose talents are spellmods.
- **1 HAZARD, not a rebind** — `spell_war_sudden_death_aura` and `spell_warr_extra_proc` are bound
  to −29723, the live chain that occupies TBC's Improved Disciplines ids. A clone gets fresh ids
  and is automatically free of it; the danger was only in *granting stock*.
- **1 LOAD-BEARING grant-stock constraint** — `spell_warr_second_wind` switches on a hardcoded
  `GetId()` with `default: return`, so a clone is **silently inert**. This is why node 20320 must
  grant stock 29834/29838.
- **The census-only miss:** **there is no Devastate script at all.** The core implements Devastate
  inside `Spell::EffectWeaponDamage` keyed on `SpellFamilyFlags[1] & 0x40`
  (`SpellEffects.cpp:3418-3434`), not through `spell_script_names`. A census-only search would have
  concluded "Devastate is unscripted" and shipped a clone that applies no Sunder Armor.

**Bindings actually present in the DB on band ids (11 rows, verified at gen `85d67983`):**
947512–947517 → `era_bloodthirst_vanilla` · 938464–938466 → `era_deep_wounds` (the per-era
carrier) · 947547 → `era_last_stand` · 947549 → `spell_warr_sweeping_strikes`.

## 8. Accepted gaps

Normative copies in `era-data/_ref/tbc/warrior-spells.yaml` `accepted_gaps:`.

| # | Gap | Why it stands |
|---|---|---|
| 1 | Defensive/Berserker **stance passives carry WotLK values** for every character (7376: TBC −10% dmg / +30% threat vs live −5% / +45%; 7381: TBC +10% dmg taken vs live +5%) | Cast by CORE CODE with hardcoded ids (`HandleAuraModShapeshift`). Eras are per-character, spells are global; no talent-side data mechanism reaches them. Closing it needs a module-side stance hook — a design decision, not an authoring one. |
| 2 | **Battle Stance grants +10% armour penetration** TBC did not have (live 2457 carries `aura 280 MOD_ARMOR_PENETRATION_PCT 10`, absent from wago) | Same core-cast mechanism, same fix. |
| 3 | **Sunder Armor is WotLK's for both eras**, so TBC Devastate stacks 58567 | Sunder is a BASELINE ability every warrior of every era trains, so it cannot be era-forked. Cosmetic-to-small; acceptance recommended. |
| 4 | **Rampage's crit window is not consumed on use** | Closing it is a few lines in `EraTalentProcScripts.cpp` (remove the ready aura in `AfterCast`, the `era_improved_berserker_rage` shape). |
| ~~5~~ | ~~Death Wish clone inherits live's `DispelType 9`~~ | **CLOSED at Task 7** — generator gained `("dispelType","DispelType")`; 947548 authors `dispelType: 0`. Paid for itself again immediately on the Enrage buffs 947556–947560. |
| 6 | **Blood Frenzy's debuffs are stock 30069/30070**, which lost TBC's `Mechanic 15` (BLEED) | Everything else is identical, which is why node 20318 triggers stock rather than minting two more clones. Cosmetic-to-nil: nothing in the tree or TBC content keys on MECHANIC_BLEED for a non-periodic aura. |
| 7 | **Mace Specialization's per-rank proc chance is in no table of record** | wago gives `ProcChance 100` on all five ranks (the real rate lived server-side in 2.4.3); the TBC tooltips carry no number; live has no `spell_proc` row. Ships the **Vanilla-verified 1/2/3/4/6%** ladder, on the ground that 2.0 changed this talent only by ADDING the rage half. wowsims/tbc does not settle it (rank-agnostic PPM). |
| 8 | **Unbridled Wrath's per-rank proc chance likewise** | Structurally identical to 7 on a different talent. Ships the **Vanilla-verified 8/16/24/32/40%** flat ladder; live's `spell_proc` rows are PPM 3/6/9/12/15, which is patch 3.0.2's conversion and post-TBC by definition. |
| 9 | **A warrior who leaves Vanilla having spent node 18513 (Sweeping Strikes) keeps stock 12328 in WotLK without paying the native talent** | **Deliberate, and pre-existing since the Vanilla phase — do not "fix" it.** Vanilla node 18513 grants **stock 12328** (not a clone), so the only way to take it back on a WotLK band-move would be to strip a stock spell that is an ordinary native WotLK talent spell — which would break native WotLK talents for every warrior who legitimately trained it. The arm-(7) strip is therefore scoped to TBC only (`vanillaCustom = false`). Cost is one free talent-equivalent ability carried across an era exit; the alternative is a real regression. Recorded so a later sweep does not read the asymmetry as an oversight. |

**Decided open questions (record-only):** Shield Slam **Category 1209 kept** (the block-value gate
at `SpellEffects.cpp:351`); Concussion Blow **`StartRecovery` inherited**; the Protection
**live-only slots dropped**.

## 9. Headless sweep (generation `85d67983`)

`tools/era-regen.sh --sync-fork` reproduced the committed artifacts **byte-for-byte** (`git status`
clean apart from the un-stageable `EraTalentIP.cpp`), and `era_talent_meta` in `acore_world` reads
`85d67983` — the same stamp in the DB and the shipped MPQ.

| Check | Result |
|---|---|
| `era_audit.py` | **0 findings** (exit 0) |
| `pytest tools/ -q` | **204 passed** |
| `era_talent` rows wired (era 1 / class 1, with ≥1 `era_talent_rank`) | **66** |
| Family guard — `SpellClassSet NOT IN (0,4)` over `938400–938927` ∪ `947512–947767` | **no rows** |
| Outside-slice authoring — any `spell_dbc` id in `947768–949999` | **no rows** |
| `era_talent` group-by | nine era-0 rows unchanged (`0 1 52`, `0 2 44`, `0 3 46`, `0 4 51`, `0 5 47`, `0 7 46`, `0 8 49`, `0 9 50`, `0 11 47`) + `1 2 64`, `1 7 61`, `1 11 62` unchanged + **`1 1 66`** |
| `trainer_spell` in `947512–947767` | **14 rows**, all TrainerId 1, each `ReqAbility1` chained off the previous rank (§6) |
| `spell_ranks` rooted in the slice | **18 rows** — 947512×6, 947530×3, 947533×3, 947541×6 |

## 10. Bot era fidelity — band-move ladder and node coverage

**Band-move ladder** on the online warrior bot **Nolainer**, 60 → 70 (+`.raidroster syncone`) → 80
→ 70, one `.eratalents doctor` per rung:

| Level | era reported | spent | ORPHAN lines |
|---|---|---|---|
| 60 | Vanilla | 51 | **0** |
| 70 | TBC | 61 (tab2 Protection) | **0** |
| 80 | WotLK | 0 ("no era talents spent this era") | **0** |
| 70 (back) | TBC | 61 | **0** |

**Teardown-first invariant spot-checked in persistence, not just in memory:** with the bot moved to
L80 and `.saveall` issued, `character_spell` holds **zero** rows in `920000–950000` for that
character. No band spell survives an out-of-band move.

**Node coverage — all 66 nodes exercised, 66/66, measured not assumed** (union over every
`era=TBC` doctor block **at generation `85d67983`**; earlier Task-6/7/8 runs sit in the same
scratch directory at earlier generations and are deliberately EXCLUDED from this count).

*Organic factory builds* (13 warrior bots levelled to 70; `.raidroster syncone` / factory
randomize picked the spec) reached **60/66**, and every organic build stopped at exactly the same
place in its tab:

- **Arms** 20/23 — Shohtop, **20300–20319**.
- **Fury** 19/21 — Krordumm, Nizhege, Selalah, all **20323–20341**.
- **Protection** 21/22 — Kavallus, Nagsqo, Nolainer, Golzistrus, Kaghenran, Onmerie, Paralen, all
  **20344–20364**.

*The same six nodes needed forcing every time, and the reason is a known defect, not a warrior
one.* A build spends in node-id order once `kAuthoredOrders` misses (§12) and exhausts 61 points
before the bottom of any tab, so **no organic build reaches a tier-9 capstone**, and a tier-6/7
node sitting behind a prerequisite chain is skipped with it. Each was force-exercised with
`.eratalents reset` + a prereq-aware learn ladder, and every one came back zero-orphan with
`known=yes spellInfo=yes`:

| Node | Forced result |
|---|---|
| 20343 Rampage (capstone) | grant **947533** known, **and the paired crit-watcher 947539 applied as a live band aura** — the arm-6 `learnSpell` pairing verified end to end |
| 20365 Devastate (capstone) | grant **947530** known |
| 20322 Endless Rage (capstone) | auto-passive **938576** known |
| 20320 Second Wind | grant **29838** (stock, as required by `spell_warr_second_wind`'s hardcoded `GetId()` — §7) |
| 20321 Improved Mortal Strike | auto-passive **938572** at rank 5, reached through its prereq chain 20312 → 20319 → 20321 (the command correctly rejected it as *"requires prerequisite talent"* until 20319 was spent) |
| 20342 Improved Berserker Stance | auto-passive **938740** at rank 5 |

**Zero `ORPHAN ERA SPELL` lines across every run in this section** — the four-rung ladder, thirteen
organic builds, and five forced deep builds (0 matches for `ORPHAN`, case-insensitive, across all
ten console transcripts).

## 11. Bot AI by-id check — **two unrouted sites found, NOT fixed here**

Method: every stock id the TBC warrior tree references (204 ids, extracted from the dataset's
`rankSpells` + stock `grants`) was intersected against **every 3–6-digit integer literal in all
1377 `.cpp`/`.h` files** of `azerothcore-wotlk/modules/mod-playerbots/src` (i.e. the fork with
patches 0020/0021 already applied). The method was sanity-checked first against the known
hardcoded shaman ids 8071/3599/5394, which it finds. Name-based AI resolution is era-safe and was
not swept (clones keep stock names).

**All the ids for spells this phase CLONED are clean** — zero hits for 23881/23892/23893/23894/
25251/30335 (Bloodthirst), 20243/30016/30022 (Devastate), 29801/30030/30033 (Rampage),
23922-23925/25258/30356/47487/47488 (Shield Slam), 12975 (Last Stand), 12292 (Death Wish),
12328 (Sweeping Strikes), 12809 (Concussion Blow), or 12294 (Mortal Strike, granted stock).

The exhaustive pass found **three** live by-id sites, of which **two are real defects**. Neither
file is touched by patch 0021 today (`grep -c` over the patch = 0), so neither is routed through
`EraTalentBots_ResolveSpellId`. **Reported, not fixed** — extending a fork patch is an
orchestrator decision.

**(a) DEFECT — `azerothcore-wotlk/modules/mod-playerbots/src/Ai/Class/Warrior/WarriorTriggers.cpp:17`
and `:132`:**

```cpp
constexpr uint32 SPELL_COMMANDING_PRESENCE_RANKS[] = { 12318, 12857, 12858, 12860, 12861 };
...
        if (bot->HasAura(SPELL_COMMANDING_PRESENCE_RANKS[rank]))
```

In `BattleShoutTrigger::IsActive`, which scales the bot's Battle Shout AP by its Commanding
Presence rank and compares the result against an active Blessing of Might. Node **20330 Commanding
Presence** is `mechanic: spellmod`, so it ships as the generated auto-passives **938640–938644**
("Commanding Presence (Rank 1..5)") and the bot never holds any of the five stock ids →
`cpBonus` is permanently 0 → `effectiveBsAp` is understated by up to 25% → the bot can skip Battle
Shout in favour of a weaker Blessing of Might. **Pre-existing, not a TBC regression:** Vanilla
node 18526 Improved Battle Shout is `mechanic: spellmod` too, so this has been live since the
Vanilla warrior shipped in `master`.

**(b) DEFECT — `azerothcore-wotlk/modules/mod-playerbots/src/Mgr/Item/StatsWeightCalculator.cpp:33`
and `:711`:**

```cpp
constexpr uint32 SPELL_POLEAXE_SPECIALIZATION = 12785;
...
        if (cls == CLASS_WARRIOR && player_->HasAura(SPELL_POLEAXE_SPECIALIZATION) &&
            (proto->SubClass == ITEM_SUBCLASS_WEAPON_POLEARM || proto->SubClass == ITEM_SUBCLASS_WEAPON_AXE2))
            weight_ *= 1.1;
```

Node **20311 Poleaxe Specialization** is `mechanic: stat`, so it ships as auto-passives
**938488–938492** and the stock id is never held → an era-managed warrior loses the axe/polearm
preference in **gear scoring** (the `.raidroster sync` / factory path). TBC-specific in effect:
Vanilla splits this into Axe Spec 18512 / Polearm Spec 18516, so the 12785 check never applied to
a Vanilla warrior in the first place.

**(c) NOT a defect — `.../src/Bot/Factory/PlayerbotFactory.cpp:99–101` and `:4287`:**
`SPELL_SECOND_WIND = 29838`, `SPELL_BLOOD_CRAZE = 16492`, `SPELL_GAG_ORDER = 12958`. These sit
inside `InitGlyphs`, **after** the patch-0020 early return at `:4250`
(`if (!EraGlyphGate_BotGlyphsAllowed(bot)) { … return; }`), so an era-managed bot never reaches
them. Independently, node 20320 Second Wind **grants stock** 29834/29838, so even a reachable
29838 check would be correct.

**Both defects are fixable by the plain patch-0021 idiom** — the resolver already trims a trailing
`" (Rank N)"` and requires the parsed rank ≥ the stock spell's rank (`EraTalentBots.cpp:415-434`,
the 2026-08-31 fix), so `EraTalentBots_ResolveSpellId(bot, 12785)` and `(bot, 12318…12861)` resolve
correctly against the generated auto-passives. If the orchestrator takes it, `tools/regen-eraai-patch.sh`
must be regenerated **baseline-aware** (patch 0021 already shares `Bot/PlayerbotAI.cpp` with
0003/0004/0011/0014/0016; `StatsWeightCalculator.cpp` and `WarriorTriggers.cpp` are pristine
baselines today).

### 11a. RESOLVED 2026-09-02 — both sites routed, patch 0021 extended

Both by-id sites now call the bridge, and `patches/0021-playerbot-era-ai.patch` was regenerated
with `tools/regen-eraai-patch.sh` (both files added to its five literal file lists + the
spot-check gate; the file-count gate went 16 → 18). Neither file is touched by any other patch,
so both took a pristine baseline; the regenerated patch is contamination-free (0 hits for
`perfMonEnabled` / `wg siege` / Heigan / Kologarn / Sunwell / `EraGlyphGate`) and the **full
ordered stack 0001 → 0024 applies cleanly** onto pristine scratch worktrees of the fork +
mod-playerbots + mod-multibot-bridge, `apply_patches`-style.

- `WarriorTriggers.cpp` — `bot->HasAura(EraKnown(bot, SPELL_COMMANDING_PRESENCE_RANKS[rank]))`,
  resolving **each rank id separately**. The bridge's rank compare is "this rank or better", so
  the existing high→low walk still lands on the bot's real rank (a rank-3 bot fails the r5/r4
  probes, matches on the r3 stock id, and takes `cpBonus = 0.15`).
- `StatsWeightCalculator.cpp` — `player_->HasAura(EraKnown(player_, SPELL_POLEAXE_SPECIALIZATION))`.
  The `cls == CLASS_WARRIOR` test short-circuits first, so only warriors pay the resolve; the
  bridge is per-(guid,spell) cached, so the per-candidate gear-scoring loop costs one map lookup.
- Defect **(a) was pre-existing in `master` since the Vanilla warrior phase** — not a TBC
  regression — and (b) is TBC-only in effect (Vanilla splits Poleaxe into Axe/Polearm Spec).
- Native bots are untouched: the bridge returns the input id unchanged when the bot knows the
  stock spell, and `HasAura(0)` is a safe (false) multimap lookup — the same idiom as the existing
  druid Omen-of-Clarity site.

**RESIDUAL — the Vanilla half of (a) is still NOT fixed, by name mismatch (verified on the live
dev-box worldserver, `lookup spell`):** stock 12318…12861 are named **"Commanding Presence"**
(WotLK renamed the talent), but the Vanilla clones 924208-924212 are named
**"Improved Battle Shout (Rank N)"** — the era-authentic Vanilla name. The bridge resolves by
NAME, so it returns 0 for a Vanilla-band warrior and `cpBonus` stays 0 exactly as before (no
regression, no fix). The TBC clones 938640-938644 are "Commanding Presence (Rank N)" and resolve.
Closing the Vanilla half needs a design call the implementer did not take: either a stock-id →
era-name alias in `EraTalentBots_ResolveSpellId`, or renaming the Vanilla node (which costs an
`era-regen.sh` generation bump + client MPQ reship and sacrifices the era-correct name).

### 11b. RESIDUAL CLOSED 2026-09-02 — era-rename alias + warrior doctor predicates

Orchestrator ruling: **alias, never rename** — the 1.12-authentic "Improved Battle Shout" stays.
Module-side only; patch 0021 is unchanged.

**`modules/mod-era-talents/src/EraTalentBots.cpp`** — the spellbook scan inside
`EraTalentBots_ResolveSpellId` was lifted into a `scanForName(wname)` lambda (body byte-identical
bar `resolved` → local `best`) and is now driven twice: once with the stock spell's LIVE name, and
— **only if that misses** — once per matching entry of a new local table

```cpp
struct EraNameAlias { char const* liveName; char const* eraName; };
static constexpr EraNameAlias kEraNameAliases[] = {
    { "commanding presence", "improved battle shout" },   // WotLK rename of the 1.12 talent
};
```

Properties, all deliberate: **band-agnostic** (it only adds candidate NAMES — the clone the bot
actually knows still decides, and the existing rank≥stock compare is unchanged, so a rank-3
Vanilla warrior still fails the r5 probe); **cached identically** (the alias result flows into the
same `g_resolveCache` write, misses included); **cannot shadow a stock spell** (the `HasSpell`
fast path at the top of the function returns the stock id long before any scan); **zero cost on
the common path** (the alias loop is reached only after a miss). A talent any later expansion
renamed now needs one line here — that is the general lesson, not a warrior special case.

**`modules/mod-era-talents/src/EraTalentsCommand.cpp`** — two warrior rows added to
`kDiscriminators`, so `.eratalents doctor` prints `AI resolve ->` for both patch-0021 sites:
`{CLASS_WARRIOR, 12861}` (also the regression test for the alias path) and `{CLASS_WARRIOR, 12785}`.
When 0 is CORRECT is documented inline: Commanding Presence is Fury tier 2 (Vanilla 18526 / TBC
20330) so an Arms/Prot build reads 0, and Poleaxe Spec is TBC-only (Vanilla splits it into Axe
Spec 18512 / Polearm Spec 18516) so 0 is *always* right for a Vanilla warrior.

Rebuilt `ac-worldserver` (both `.cpp` recompiled, exit 0) and re-ran doctor on the live stack:

| Bot | Era / build | `AI resolve: 12861` | `AI resolve: 12785` |
|---|---|---|---|
| **Millani** L35 | Vanilla, node 18526 r5 | **→ 924212** (alias path — was 0) | → 0 (correct, no Vanilla node) |
| **Selalah** L70 | TBC, node 20330 r5 (Fury) | → 938644 (live-name path, unchanged) | → 0 (correct, Fury build) |
| **Sirim** L70 | TBC, node 20311 r5 (Arms) | → 0 (correct, Arms build) | **→ 938492** |
| **Lantandrin** L80 | WotLK native | → 0 | → 0 |

No online L80 warrior currently holds Commanding Presence or Poleaxe Spec natively
(`character_talent` after `.saveall` = no rows), so the warrior-flavoured "stock id returns
itself" case could not be shown directly. The equivalent control was taken on a **native
WotLK-band L80 shaman (Julaan)**, where the doctor's stock-every-era rows come back
`8071 -> 8071`, `3599 -> 3599`, `8177 -> 8177` — i.e. the `HasSpell` fast path returns the stock
id untouched, which is the same code path a native warrior takes and the reason an alias can
never shadow a known stock spell.

### 11c. RESOLVED — C-1: strip-without-replacement at committed HEAD (final review, 2026-09-03)

**The defect.** `EraTalents.cpp`'s era-managed TBC strip arms gate on `EraReplacementSpellReady(id)`
(+ `EraNodeIsWired`), which ask only "does the clone exist" and "is the node wired". Neither
consults **`EraHasTalentTrees(era)`** — the implemented-era allowlist — and `ReconcileBaselineSpells`
runs **unconditionally** for real players (`EraTalentPin.cpp:71` / `:141`). At *committed* HEAD the
allowlist (`EraTalentIP.cpp:21`) is deliberately **Vanilla-only** (deployment policy: TBC characters
keep native WotLK talents until all nine classes ship as a set — the `ERA_TBC` flip is an
uncommitted dev-box edit). So on the committed branch a real **TBC-band player** would have stock
**Shield Slam / Bloodthirst / Last Stand / Sweeping Strikes** — and the TBC druid Nature's
Grasp / Omen of Clarity trainer chains (`:1356` / `:1416`) — **stripped**, while the talent window
that grants the replacement clones is disabled. Both existing conjuncts pass (the rows ship and the
nodes are wired), so nothing stopped it: the character loses the spell and can never spend a point
to get it back.

Note this was **never** reachable on the dev box, which is exactly why every headless run in §9/§10
and the Task-5–9 ladders were green: the flip is present there, so `EraHasTalentTrees(ERA_TBC)` is
true and all three conjuncts hold. The exposure exists only in the shipped-branch state.

**The fix.** The readiness predicate became era-aware —
`EraReplacementSpellReady(EraId era, uint32 spellId)` returning
`EraHasTalentTrees(era) && spellId != 0 && sSpellMgr->GetSpellInfo(spellId) != nullptr` — and all
**eight** call sites (`:865 :887 :907 :974 :1013` warrior, `:1356 :1416` druid, `:1771` gate-walker
skip; pre-fix line numbers) now pass their era. Every one already sat under an `era == ERA_TBC`
conjunct, so the literal `ERA_TBC` is passed, matching the adjacent `EraNodeIsWired(ERA_TBC, …)`
argument. The predicate's comment block gained the allowlist rationale alongside the SQL-migration
safety-net role it already documented.

**Semantics.** With the dev-box flip present `EraHasTalentTrees(ERA_TBC)` is true, so behaviour is
**identical to what Tasks 5–9 verified**. At committed HEAD (flip absent) the TBC arms go **inert**
— no strip, no grant — and a TBC player keeps stock everything, which *is* the implemented-era
policy intent. **Vanilla arms are untouched** in both states (`ERA_VANILLA` passes the allowlist
either way). The fix therefore changes no verified behaviour and closes the shipped-branch hole.

**Re-verified after the fix:** worldserver rebuilt; the L60→70→80→70 band-move doctor ladder re-run
on an online warrior bot reproduced §10 exactly (zero ORPHAN, teardown clean); `era_audit.py` 0
findings; 204/204 host tests green.

**Doctor allowlist gap found and closed in the same round (diagnostic-only, pre-existing).** The
re-run's Vanilla rung drew an *organic build that spends node 18535* (the §10 run drew Protection,
which has no Bloodthirst), which surfaced three `ORPHAN ERA SPELL` lines for **932829–932831** — the
Vanilla Bloodthirst r2–r4, `trainer_spell`-confirmed as legitimately chained off the talent-granted
r1 932828. They are trainer-taught, never node-granted, so `.eratalents doctor`'s generic
`NodesFor()` fallback could never resolve them; `EraTalentsCommand.cpp` allowlisted the *TBC* chain
(947512–947517) but not the Vanilla one. The gap **predates this branch** (0 hits for those ids in
`git show master:…/EraTalentsCommand.cpp`) — no spell was ever lost, only mislabelled. Closed with a
predicate in the Vanilla Nature's Grasp 932507–932511 shape, node-gated on 18535 so a rank-0 warrior
still holding a rank keeps reporting. Re-verified on the same Vanilla-rung Bloodthirst build: the
three lines are gone and the rest of the block is unchanged.

## 12. Known open items (not this phase's defects)

- ~~**`kAuthoredOrders` is era-blind** (workflow §4b lesson 10). Directly observed this phase: every
  TBC-band warrior build logs a wall of
  `[mod-era-talents] authored build order skipped N (talent 18xxx) for <bot> tabN — order/tree drift,
  regenerate kAuthoredOrders` — the table holds **Vanilla** node ids, so every TBC bot falls through
  to the greedy fill. It still reaches 61 points, so builds are legal, but they are node-id-ordered
  rather than authored, which is exactly why **no organic build reaches a tier-9 capstone** (§10).
  This affects the already-shipped TBC paladin/druid/shaman identically. **Not a warrior defect.**~~
  **CLOSED 2026-09-03** (recorded here at the final review R7, 2026-09-06 — the three bullets above
  were stale from the day after they were written). `tools/gen_bot_builds.py` now emits keyed
  `(era, class, specTab)` entries and the manifest gate FAILS on any dataset missing orders for
  specTabs 0-2, so `(TBC, warrior, 0/1/2)` are authored: the drift wall is gone (0 skipped-order
  WARNs) and probe builds reach the tier-9 capstones **20343 (Rampage)** and **20365 (Devastate)**.
  Both the "no organic build reaches a capstone" and the "drift wall" observations in §10 are
  superseded by that.
- **Druid Heart of the Wild marker 946280 has no `.eratalents doctor` predicate** — already spawned
  as a separate task.
- ~~**The two by-id AI sites in §11**, pending the orchestrator's call on extending patch 0021.~~
  **DONE 2026-09-02 (§11a)** — both routed through the bridge, 0021 regenerated. The Vanilla
  Commanding Presence residual is **also closed** (§11b): an era-rename alias in the resolver,
  plus `AI resolve` doctor predicates for both warrior sites.

## 13. What remains UNVERIFIED — REAL-CLIENT CHECKLIST (plan Task 10 Step 2)

Headless green is not sign-off. To be run by the user on a **TBC-era warrior at level 70** with a
fresh `patch-V.mpq` at generation `85d67983`:

1. **Frame** — all three tabs render real parchment (`WarriorArms` / `WarriorFury` /
   `WarriorProtection`); tier 9 reachable in the scroll; point pool reads **61**; every talent icon
   renders (a blank button = the `classic_` icon trap, lesson 5).
2. **Stance dance** — talent passives persist across Battle/Defensive/Berserker swaps; Tactical
   Mastery retains the tooltip's rage amount on swap.
3. **Rampage** — usable only after a recent crit; the AP buff stacks on melee crits to the TBC cap
   and the tooltip's numbers match the buff; trained ranks purchasable only with r1 known.
4. **Devastate** — damage/threat per the shipped disposition; applies/pairs with Sunder per its
   tooltip; trained ranks gated on r1.
5. **Deep Wounds / Flurry / Enrage** — the proc BUFF values (duration, magnitude) match the talent
   tooltips, not just the talent panel.
6. **Second Wind** — procs on stun/immobilize with the TBC magnitudes.
7. **Bloodthirst / Last Stand / Shield Slam** — per their shipped dispositions; Shield Slam absent
   without the talent and present with it (the gate).
8. **Accepted gaps read honestly** — every gap's tooltip describes shipped behaviour.
9. **Era transition** — in-game (self-targeted!) `.ip set` to a Vanilla stage and back to 8: the
   tree resets cleanly both ways, no orphan spells (the managed→managed player path).

Carry forward the shaman fix-round's standing sweep classes (record §10 there): proc roles absent
from `ip-dbc`, `PreventionType`, inherited `AttributesEx` bits, grant-node client descriptions,
and per-era trainer-row OR-gating.

## 14. Real-client sign-off

**SIGNED OFF by the user 2026-09-03** at generation `85d67983` — the full 9-item checklist above
passed on a level-70 TBC-era warrior with the fresh `patch-V.mpq`; no findings, no fix round
needed. Phase 4 is CLOSED. Next class: **rogue (Phase 5)**, helpers from 947768, entry via
`docs/era-talents-tbc-workflow.md` §3 (+ §4b — carry the FOURTEEN-family sweep column list from
this phase's §3/A.9, not the plan-era five).
