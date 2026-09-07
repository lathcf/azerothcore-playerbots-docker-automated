# Era Talents — TBC Phase 1 (Paladin) verification record

> **Generation superseded (2026-09-06 final review):** the current stamp is the closing row of docs/verification/era-talents-final-review-tbc-vanilla.md §10 — reinstall patch-V.mpq + restage the addon before any real-client pass.

**Status: CLOSED 2026-08-30 (generation `f606ab37`, commit `d07d54a`).** Real-client tested through
three fix rounds (§7b/7c/7d — trainer window, stock-SoV swap, tooltip scrub, CS judgement refresh,
full-tree sanity audit) and a 4-agent tooltip-vs-shipped sweep (54/64 nodes audit-clean, 7 defects
fixed, 0 DB orphans). Task-10 items both done: active-rank trainer chains shipped (`f4249e9`/
`3a42e8b`), behavioral verification signed off by the owner 2026-08-30. Branch `feat/era-talents` —
**merged to master 2026-09-07** (TBC merges as a set after its own final all-class review + a Vanilla-regression
check). The `EraTalentIP.cpp` ERA_TBC allowlist flip remains an uncommitted dev-box edit until the
single final-enablement task.

**Spec/plan:** `docs/superpowers/specs/2026-08-27-era-talents-tbc-phase1-paladin-content-design.md`
+ `docs/superpowers/plans/2026-08-27-era-talents-tbc-phase1-paladin.md`.

**Phase commits (feat/era-talents, on top of Phase 0 `282ca65`):**
`1082b6a` helper-slice/guard · `a1b3b17` survival map (`_ref`) · `7a741f2` wire dataset ·
`8961f74` reconcile arm · `ee6b646` standard seals (fresh 946xxx) · `6d1f2ed` seals trainer-taught +
marker + SoR-on-hit wago + SoJ r2 · `410c793` Seal of Blood/Vengeance · `84bc5bd` Holy tab ·
`0dc8105` Protection tab · `9fbdedc` Retribution tab · `9463805` JoC normal-hit + charges-op test.

---

## 1. What shipped

The complete **TBC 2.4.3 paladin talent tree** — 64 nodes (Holy 20 / Protection 22 / Retribution
22, 9 tiers, ids 20000–20063, era 1) — plus a full **TBC seal/Judgement substrate**. **Zero core
patches.** Value basis: **wago 2.5.4.44833 end-to-end** (the governing decision — TBC seals are
fresh clones at TBC-Classic magnitudes, not reused Vanilla 1.12.1 values; see memory
`tbc-era-value-basis-wago-2540`). Helper ids **946000–946114** (117 distinct, within the reserved
`946000–946255` slice). Auto-passives 936000–936xxx (generated). 10 `era_pal_*` scripts.

**The shipped allowlist stays Vanilla-only** (`EraTalentIP.cpp` `EraHasTalentTrees` unchanged in
git). The dev-box ERA_TBC test flip is an **uncommitted working-tree edit** throughout — the real
allowlist change ships at the single final-enablement task after all nine TBC classes. The
paladin reconcile TBC arm this phase committed is therefore dead code in production (guarded
`era == ERA_TBC`, unreachable until enablement) — intended (spec §4).

## 2. Seal / Judgement substrate (fresh clones, wago-2.5.4, trainer-taught)

Reconcile arm (`EraTalents.cpp` CLASS_PALADIN, `era == ERA_TBC`): grants the hidden **"Era: TBC
Paladin" seal-training marker 946078** + strips stock WotLK seals/judgement-buttons + strips the
Vanilla marker 932749 / superseded Vanilla clones; the seals themselves are learned at trainers.
Keeps **reused** (not seals, not rebalanced): the single **Judgement wrapper 932746** (generic
unleash — works on any `SPELL_SPECIFIC_SEAL` with an EFFECT_2 DUMMY, incl. the fresh 946xxx seals)
and **Lay on Hands 932668–670**.

| Seal | Ranks | Seal ids | On-hit | Judgement | Notes |
|---|---|---|---|---|---|
| Righteousness | 9 (r9 L66) | 946000–008 | 946009–017 | JoR 946018–026 | on-hit flat base = wago magnitude (no SP/AP scaling — accepted gap) |
| Command | 6 | 946027–032 | 70% proc 946033 | flat JoC 946103–108 | r1 talent-granted (node 20049); JoC authentic normal-hit (halved) |
| Light | 5 (r5 L69) | 946034–038 | 946039–043 | JoL 946044–048 (+carrier 946049) | |
| Wisdom | 4 (r4 L67) | 946050–053 | 946054–057 | JoW 946058–061 (+carrier 946062) | |
| Justice | 2 (r2 L48) | 946063, 946079 | stock stun 20170 | stock JoJ 20184 | |
| the Crusader | 7 (r7 L61) | 946064–070 | stat auras | JotC 946071–077 | AP ≈2× Vanilla = the TBC-Classic rebalance; Improved SotC (20045) binds via family word-C bit9 |
| **Blood** (Horde/BE) | 1 | 946080 | 946082 + self-dmg 946083 | JoB 946081 | on-hit 35% weapon Holy, self-dmg 10% of that; JoB flat 294 + 33% self-dmg |
| **Vengeance** (Alliance) | 1 | 946084 | Holy Vengeance DoT 946085 | JoV 946086 | DoT `CumulativeAura` 5 × 30/3s; JoV +10%/stack (cap 5) |

Trainer chains (`2026_08_27_02_..._seal_substrate.sql`): 32 seal `trainer_spell` rows/trainer
(TrainerId 3/4/5) behind `ReqAbility1 = 946078` + `spell_ranks` chains. **Faction gate (two
layers):** SoB on the Horde/Silvermoon trainer only, SoV on Alliance only; **plus**
`EraTalents::StripWrongFactionTbcPalSeal(p)` by `GetTeamId()` — wired into the reconcile path AND
**`OnBotLogin`** (the playerbots factory trainer-walk `InitAvailableSpells` is class-only-gated, so
a bot learns both and the strip removes the wrong one). Scripts: `era_pal_seal_of_blood` /
`_judgement_of_blood` (bounded self-damage, cannot one-shot; self-carrier cloned from stock 32221),
`era_pal_seal_of_vengeance` / `_judgement_of_vengeance`.

## 3. Tree node dispositions (per the live-verified `_ref` survival map)

**Holy (20000–20019):** stats — Divine Strength, Divine Intellect, Unyielding Faith (multi), Pure
of Heart (multi), Holy Power, Holy Guidance (multi); spellmods — Spiritual Focus, Improved SoR
(binds SoR on-hit 946009–017), Healing Light, **Aura Mastery** (radius spellmod {c:32}, pure data —
core applies SPELLMOD_RADIUS, no C++), Improved LoH (+ armor procPassive reusing 932671/672),
Improved BoW, Sanctified Light, Purifying Power (multi, TBC shape); grants — Illumination,
Divine Favor, **Light's Grace**, **Blessed Life** (both stock — `_ref` overruled the "proc
reconstruction" prior), Divine Illumination (capstone); clone — **Holy Shock 946087/088/089** (TBC
277-299 dmg / 351-379 heal, reuses `era_pal_holy_shock`).

**Protection (20020–20041):** stats — Precision (multi), Toughness, Anticipation, Shield
Specialization, Spell Warding, One-Handed Weapon Spec, Combat Expertise (multi), Sacred Duty
(multi), Stoicism; spellmods — Improved Devotion Aura ({a:64} to avoid the Devotion C-bit5
over-bind), Guardian's Favor (multi), Improved Righteous Fury (multi), Improved Hammer of Justice,
Improved Concentration Aura ({a:131072}), Improved Holy Shield (multi, charges{b:64}+dmg{c:2048});
grants — Blessing of Kings (20217, node grant — the one real leak, already globally trainer-deleted
by Vanilla), Reckoning (stock chain); **reuse** Vanilla clones — Redoubt (proc buffs 932624–628 +
fresh procs 946097–101), Blessing of Sanctuary (932617); **fresh clones** — Holy Shield 946095/096
(59 Holy), Avenger's Shield 946102 (270–330), **Ardent Defender 946090–094** (SCHOOL_ABSORB clones
+ `era_pal_ardent_defender` reproducing core's <35%-HP reduction minus the cheat-death — **no core
patch**).

**Retribution (20042–20063):** spellmods — Improved BoM, Benediction (cost, Judgement+seal mask),
Improved Judgement (Judgement cooldown), Improved SotC (op effect1 {c:512} → SotC AP + JotC),
Improved Retribution Aura, Improved Sanctity Aura (binds 932602); stats — Deflection, Conviction,
Pursuit of Justice (multi), Crusade (creature-type mask 108: Humanoid/Demon/Undead/Elemental),
Two-Handed Weapon Spec, **Divine Purpose** (crit-damage-TAKEN reduction — corrected from the
brainstorm's stun-proc prior), Sanctified Seals (multi crit + resistDispel), Fanaticism (multi:
op crit on Judgement + threat reduction); grants — Seal of Command (seal r1 946027), Repentance
(20066); **reuse** — Vindication (932603–605 proc), Eye for an Eye (932608/609), Sanctity Aura
(932602); **fresh clones** — Vengeance 946109–113 (+1–5%/stack, 3 stacks, 30s), Crusader Strike
946114 (110% weapon), flat Judgement of Command 946103–108; **reconstruct (C++)** — Sanctified
Judgement (extends `era_pal_judgement`: reads DUMMY passives 936464–466 = 33/66/100% chance, refunds
80% of the judged seal's mana).

## 4. Baseline-leak analysis (Task 3 live-verified — corrected the brainstorm)

Only **Blessing of Kings (20217)** is a genuine baseline leak, and Vanilla **already globally
deleted** its stock trainer rows, so TBC inherits the closed leak — the node simply grants BoK and
the reconcile generic gate skips it for TBC. **Holy Shock 20473 / Seal of Command 20375 / Avenger's
Shield 31935 / Crusader Strike 35395 are NOT leaks** — they are talent-only in WotLK too (verified
against the pristine `trainer_spell.sql` + `kBaselineSpellGates`), so no gates were written for them.
**Consecration 26573** is TBC trainer-baseline (handled by the existing `kBaselineSpellGates`
grant-higher-era; it is not a talent node in the TBC tree). **Blessing of Sanctuary 20911** is
talent-gated in WotLK too → plain node grant, not a leak.

## 5. Reconcile / pin / doctor dispositions (resolves the `TBC->paladin` decision-table rows)

TBC paladins get the **custom-seal reconstruction extended** (fresh 946xxx seals at wago values,
trainer-taught behind marker 946078), implemented as paladin-specific TBC arms:
- `EraTalents.cpp` CLASS_PALADIN reconcile: TBC arm (marker grant + strip + BoK/faction handling).
- `EraTalentPin.cpp` seal-phantom re-strip: widened to fire for a TBC paladin (`ERA_VANILLA || ERA_TBC`).
- `EraTalentsCommand.cpp` `.eratalents doctor`: the paladin orphan checks recognize the TBC-band
  seals (946000–946086) + marker 946078 for a TBC paladin (else 26 false orphans); the 932749
  check stays Vanilla-only.
The **global** three-way predicate split + the doctor stale-stock/leak sweep remain **deferred to
the single final-enablement task** (per `docs/era-talents-tbc-reconcile-triage.md`). This phase
touched only the paladin-specific arms.

## 6. Accepted gaps (documented, not fixed)

- **Judgement of Command stun-doubling** — JoC deals its authentic *normal-hit* value; the
  situational +100% vs a stunned target is not reconstructed. **CLOSED 2026-09-07** —
  `era_pal_judgement_of_command` bound to 946103-946108 (and Vanilla 932673-932677).
- **Seal of Righteousness on-hit** uses the engine's flat base×speed model at wago magnitude — no
  live SP/AP scaling (the engine models no seal with SP/AP scaling; user decision 2026-08-27).
- ~~**Illumination** grants stock → core refunds 30% (WotLK) vs the TBC 60% tooltip~~ — **CLOSED by
  the 2026-09-06 final review (G-1):** node 20008 now grants clones 946141–946145 with
  `EffectBasePoints_2` 59 (= 60 %), each bound to the same core `spell_pal_illumination` AuraScript
  (which reads the refund % off the aura's own Effect_2) with its own clone `spell_proc` row
  mirroring the module's widened `-20210` row. The Vanilla node took the identical fix at 100 %.
- **Stoicism** spell-dispel-resist half, and **Improved Concentration Aura** group
  silence/interrupt-resist rider — no clean bindable set (Vanilla precedent).
- **Active multi-rank abilities shipped at rank 1 only** (Holy Shock, Holy Shield, Blessing of
  Sanctuary, Avenger's Shield) — higher trained ranks are a **Task-10 follow-up** (§7), delivered in
  the 2026-08-30 fix round (946116–946140).
- **Repentance** — closed by the 2026-09-06 final review (G-2): node 20060 grants clone 946146
  (`durationIndex` 32 = 6 s, `targetCreatureType` 64 = Humanoid) instead of stock 20066's WotLK 1-min
  / widened-victim-set form.
- **Seal costs / Crusader Strike cooldown / SoL-SoW proc rate** — closed by the same review
  (I-1/I-2/I-4): SoJ 946063+946079 `manaCostPct` 10, SoB 946080 flat 210, SoV 946084 flat 250,
  Crusader Strike 946114 `cooldown` 6000 (it had been inheriting WotLK 35395's 4 s), and the SoL /
  SoW seals now carry `ppm` 10 / 12 instead of a flat 100 % per-swing chance.

## 7. Task-10 follow-ups (real-client fix round)

1. **Active-rank trainer chains** — Holy Shock r2–4, Holy Shield r2–4, Blessing of Sanctuary r2–4,
   Avenger's Shield r2–3 (fresh clones/reuse + `trainer_spell`/`spell_ranks` behind marker 946078,
   mirroring the Vanilla active-rank trainer file). Deferred per the Vanilla precedent (its
   active-rank chains landed in the real-client fix round).
2. **Behavioral verification (client-only)** — procs firing (Light's Grace, Blessed Life, Vengeance
   stacks); Ardent Defender <35%-HP reduction; seal-modifier binding moves (Improved SotC AP/JotC,
   Improved/Sanctified Judgement); SoB self-damage + SoV Holy Vengeance DoT + JoB/JoV; the flat JoC
   unleash; faction-gating in-client; the 9-tier tree renders with all backgrounds/icons and 61
   points at L70; canary green (no generation-mismatch warning).

## 7b. Real-client fix round #1 (2026-08-30, gen `7801052e`)

First real-client pass reported three issues; dispositions:

1. **Talent tooltips carried Wowhead header junk** ("<Name> Talent Requires Paladin", and on
   actives the mana/range/cast/cooldown lines, plus "(Proc chance: N%)" suffixes). Root cause:
   `tools/import_tbc_talents.py` `_clean_tooltip` flattened the WHOLE Wowhead tooltip HTML; the
   effect prose is only the `<div class="q">` block (the proc-chance suffix sits outside it).
   Fixed the importer (q-div extraction + regression test `test_clean_tooltip_extracts_effect_div_only`)
   so the next 8 TBC classes import clean, and scrubbed all 64 authored tooltip blocks in
   `era-data/tbc/paladin.yaml` to pure effect prose (Vanilla house style).
2. **Stock WotLK Seal of Vengeance still obtainable by a TBC paladin**: 31801 (Alliance,
   TrainerId 3) and Seal of Corruption 53736 (Horde, TrainerId 4) still had UNGATED
   `trainer_spell` rows (ReqLevel 64) — Paladintest had trained 31801 next to the substrate.
   Version-swapped like the Vanilla seals: rows deleted (`2026_08_30_00_era_tbc_paladin_stock_sov_trainer.sql`),
   both ids added to `kStockPalSwap` (stripped in Vanilla AND TBC arms) + `kReconcileOnLearn`,
   and the WotLK arm now faction-grants the appropriate seal at 64 (strips the other).
3. **Class-trainer window: Lua errors + empty list — ROOT-CAUSED by the user's error screenshot
   (gen `db0c8419` fix):** `Blizzard_TrainerUI.lua:319: bad argument #2 to 'format' (string
   expected, got nil)` in `ClassTrainer_SetSelection` ← `ClassTrainer_SelectFirstLearnableSkill`
   ← `ClassTrainerFrame_Show`. Line 319 formats the selected service's **ReqAbility NAME
   unconditionally** (met or unmet); the client resolves that name from ITS OWN Spell.dbc, and
   the seal-training marker 946078 deliberately shipped with **no client row** → nil → the error
   aborts the Show path before the list renders → empty window. The Vanilla marker 932749 had
   the identical latent bug (masked when Lua error display is off — the C-side
   `SelectTrainerService` at line 271 runs before the throw, so training still worked silently).
   **Fix:** both markers now carry `client:` blocks (932749 "Seal Training" / 946078 "TBC Seal
   Training", PASSIVE_ATTR keeps them out of the spellbook — the hunter 932304/305 precedent);
   946078 moved from the hand-SQL byte-copy in `2026_08_27_02` into `era-data/tbc/paladin.yaml`
   so the generator owns both the server and client rows. A sweep of ALL `trainer_spell`
   ReqAbility1/2/3 values ≥920000 against the built client DBC found exactly these two
   offenders — every other era trainer chain (hunter/warlock/shaman/druid/rogue/warrior +
   946xxx ranks) already resolves. **RULE (now in the framework doc): any spell used as a
   trainer_spell ReqAbility MUST carry a `client:` block.**

## 7c. Real-client fix round #2 (2026-08-30, gen `7812b858`) — Crusader Strike judgement refresh

User-reported during final testing; three of four reports were verified-authentic, one was a real gap:

1. **Crusader Strike judgement refresh — REAL GAP, fixed.** wago 2.5.4 (TBC Classic = final 2.4.3
   form; patch 2.3.0 added the refresh) CS tooltip: *"An instant strike that causes 110% weapon
   damage and refreshes all Judgements on the target."* The Task-8 authoring comment claimed
   "TBC CS did NOT refresh judgements" (conflated with the dropped WotLK E3 dummy) and shipped the
   clone 946114 without it — while the imported node tooltip PROMISED the refresh. Fix: new
   `era_pal_crusader_strike` SpellScript (bound via scriptBindings), OnEffectHitTarget EFFECT_1
   (WEAPON_PERCENT_DAMAGE, landed hits only) refreshes every judgement DEBUFF on the target from
   ANY paladin (TBC refreshed other paladins' too): JoL 946044-048, JoW 946058-061, JotC
   946071-077, + reused stock Judgement of Justice 20184. Instant judgements (JoR/JoC/JoB/JoV)
   leave no debuff; the JoL/JoW heal/energize carriers (946049/946062) are excluded. Client
   description for 946114 now carries the refresh clause; stray "Weapon " tooltip prefix removed.
2. **Crusader Strike ranks — AUTHENTIC, no change.** TBC CS is a single-rank 41-point talent;
   multi-rank CS is a later-expansion shape.
3. **Seal of Vengeance single rank — AUTHENTIC, no change.** TBC SoV (and SoB) have exactly one
   rank, trainer-taught at 66 (row `(3, 946084, ..., 66)` gated on marker 946078).
4. **Redemption missing / untrainable — NOT A BUG (stock behavior).** Rank 1 (7328) is a class-
   QUEST reward ("The Tome of Divinity" 1785/1788 / "Redemption" 9600 / "Redeeming the Dead" 9685,
   MinLevel 12) — stock acore ships NO trainer row for it; trainer ranks 2+ (10322→...) chain off
   knowing r1 via ReqAbility1. The test character skipped the quest. Test-char unblock:
   `.learn 7328`, after which the trainer offers r2-r5 (r6/r7 are 72/79 = WotLK levels).

## 7d. Full-tree sanity audit (2026-08-30, gen `d4af0664`) — 4-agent tooltip-vs-shipped sweep

After the CS-refresh class of bug (shipped follows a wrong `_ref`, not the tooltip), all 64 nodes +
the seal substrate + trainer chains + C++ wiring were audited against the imported wago-2.5.4
tooltips. 54 nodes verified clean (incl. every trainer chain, 0 DB orphans, faction gates, reconcile
teardown). Findings fixed this round:

1. **20045 Improved Seal of the Crusader — BROKEN, fixed.** Tooltip promises +1/2/3% crit vs the
   judged target; shipped was a pct spellmod on the seal/JotC magnitudes (Vanilla semantics
   transplanted) and NO crit existed. wago 20335-337 = ONE aura107 flat SPELLMOD_EFFECT2 on a 0-base
   crit effect — TBC's talent is CRIT-ONLY (no magnitude bonus). Now: JotC clones 946071-077 carry
   EFFECT_1 aura197 (MOD_ATTACKER_SPELL_AND_WEAPON_CRIT_CHANCE) base 0; node converted to
   `scripted` marker misc **17**; new `era_pal_jotc_crit` AuraScript sets the amount from the
   node's DUMMY passives 936360-362 (a flat effect2 spellmod would leak onto the seals' +40%
   attack-speed EFFECT_1 — clones share word-C bit9).
2. **20036 One-Handed Weapon Specialization — BROKEN, fixed.** Tooltip "all damage"; shipped misc
   school 1 (physical only — the Vanilla node-18628 shape; a prot paladin's Holy output was
   excluded). Now school 127, matching stock 20196's DBC.
3. **20047 Vindication — fixed.** Tooltip "attributes ... 15 sec"; shipped reused the Vanilla
   Str+Agi/10s clones. Fresh TBC debuffs **946138-940** (aura80 misc -1 = ALL stats, -5/-10/-15%,
   durationIndex 8 = 15s); node procs now trigger them.
4. **20041 Avenger's Shield daze — fixed.** Tooltip/description "Lasts 6 sec"; clones silently
   inherited the WotLK template's 10s. All three ranks (946102/134/135) now durationIndex 32 (6s).
5. **20049 SoC / Judgement of Command values — fixed (user decision: per-level scaling).** Shipped
   flat halved bases were correct only at training level (tooltip r1 says 68-73 at cap; shipped dealt
   47-51 at all levels). Generator extended with helper `spellLevel`/`baseLevel`/`maxLevel` scalars +
   per-effect `perLevel` (EffectRealPointsPerLevel, float; unit test added); JoC 946103-108 now carry
   the halved wago RPPL (2.8/3.05/2.8/3.05/3.05/3.05) inside the wago level windows (20-28 ... 70-78).
   Seal descriptions now use live `$946103s1`-style refs so the client renders the true scaled value.
6. **20008 Illumination / 20011 Divine Favor Holy Shock clauses — DEAD, fixed (final design
   revised same-day, ported to Vanilla).** The era HS executors shipped family 0, so stock
   Illumination's proc row (-20210: family 10, mask0 HL|FoL, mask1 stock-HS-heal bit16, crit) and
   Divine Favor's crit spellmod could never match them. The Vanilla dataset's original "family-less
   is fine — no spell_proc row means no family filter" premise is FALSE on this core (acore ships
   -20210), so Vanilla was equally dead. Final design: ALL era HS executors — TBC dmg
   946088/117/120/123/126 + heal 946089/118/121/124/127, Vanilla dmg 932649-651 + heal 932652-654 —
   carry family 10 + word-A bit21 (0x200000, the stock Holy Shock identity bit), and the module
   overrides -20210 to widen mask0 with bit21 (`2026_08_30_20_era_holy_shock_illumination_proc.sql`).
   Deliberately NOT heal-side word-B bit16: bit16 flips spell_pal_illumination's refund onto the
   GetSpellWithRank(20473) branch, which reads STOCK WotLK rank costs (225/275/...) instead of the
   executors' authored per-rank powerCost (an interim revision of this fix used bit16 — never
   user-tested, replaced before commit). Divine Favor (mask word-A 0x200000 | word-B 0x10000)
   matches both executor kinds via bit21. The widened row is inert for WotLK paladins (stock HS heal
   still matches via the unchanged mask1; a stock HS damage crit now matches but no-ops — the script
   bails without HealInfo and the aura carries no charges).
7. **20005 Aura Mastery — fixed (user decision: +10yd, tooltip rewritten).** wago "to 40 yards"
   presumes TBC's 30yd base, but every stock paladin aura on this core is already RadiusIndex 23 =
   40yd — the authored +10yd overshot to 50 while the tooltip promised the baseline. Kept +10yd
   (40→50) and the tooltip now says "by 10 yards" (documented divergence; a no-op talent point was
   rejected).
8. **20016 Holy Shock r1 spellbook text — fixed.** The r1 dummy inherited stock 20473's
   `$25912s1/$25914s1` description (WotLK numbers); now a literal-value description like r2-5.
9. **Stale band comments fixed** (YAML claimed 946136+ free while BoS r5 946136/946137 shipped);
   framework registry row updated (946000-946140 used, misc 17 consumed).

**Audited-clean-but-noteworthy (accepted, added here as the canonical record):** Holy Shield's "35%
additional threat" clause is unimplemented — but stock WotLK Holy Shield has the identical gap on
this fork (no spell_threat row, no script), so era = stock parity. Repentance grants stock 20066
(1 min, more creature types vs TBC's 6s Humanoid-only) — CC-generosity divergence, same as Vanilla.
Improved Sanctity Aura raises only the Holy% (+10→11/12), not literally "all damage" — accepted in
the _ref. Fanaticism's threat reduction is unconditional (no "except under Righteous Fury" gate).
Vindication proc chance 30% is an authentic approximation (TBC chance undocumented). Sacred Duty's
"attack speed penalty" wording covers this core's Divine Shield damage-done penalty (the penalty
that actually exists). 20017 Blessed Life halves the NEXT hit after the proccing one (serverside
31934 emulation). SoB/SoV are trainer-listed only on the faction trainers (3/4) — the single
neutral trainer (TrainerId 5) deliberately offers neither.

## 8. Testing evidence (headless, dev box, gen `d51bd7d3`)

- **167/167** `tools/` tests pass (incl. the era-1 guard + the new `charges`-op test).
- `era_audit.py` = **0 findings** (both eras + cross-era collision check).
- Clean boot: `loaded 496 talent nodes (generation d51bd7d3)`, zero errors on any 936xxx/946xxx id,
  `spell_proc`, `spell_script_names`, or `spell_dbc`; `(era=1, classId=2) = 64`, the nine era=0 rows
  unchanged.
- Full per-tab trees learned on TBC paladin bots (Fineri L70, Ariton L63): 0 rejections,
  `.eratalents doctor` **CLEAN**; applied-aura amounts match authored TBC magnitudes; seals
  trainer-gated (not auto-granted); SoB/SoV faction-strip verified organically on both a Blood Elf
  and an Alliance bot; WotLK + Vanilla paladins unregressed.
- `EraTalentIP.cpp` ERA_TBC flip confirmed **uncommitted** (last commit `312d69b`, the old one).

## 9. Id / marker allocations (for the framework registry)

- **Node ids** 20000–20063. **Auto-passives** 936000–936xxx (+ Sanctified Judgement DUMMY passives
  936464–466). **Helper band** 946000–946146 used after the 2026-09-06 final review (946115 skipped;
  946147–946255 free within the reserved 946000–946255 paladin slice). The last allocations are
  946116–946140 (active-rank trainer chains + BoS r5 pair + TBC Vindication debuffs) and
  **946141–946145** Illumination 60 % clones + **946146** Repentance 6 s / Humanoid clone.
- **Marker** 946078 "Era: TBC Paladin" (hidden, Attributes 262608 — a byte-copy of Vanilla 932749).
- **DUMMY-marker registry:** misc **17** is consumed by the Improved-SotC crit passives
  **936360–936362** (node 20045, read by `era_pal_jotc_crit`). 17 was the next-free value at the time
  this phase shipped; the registry has since moved on (druid Heart of the Wild took 18, so **19** is
  next free today). An earlier revision of this bullet said "no DUMMY-marker registry misc consumed
  … next-free stays 16", which was already false when the Improved SotC arm landed.
- **Generator change:** `charges` op → SPELLMOD_CHARGES (4), with a unit test.
