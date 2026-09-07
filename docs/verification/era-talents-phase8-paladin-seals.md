# Vanilla Paladin Seal & Judgement Reconstruction — Verification Record

**Feature:** the 4 baseline seals (SoR/SoJ/SoL/SoW) reworked to Vanilla + a single consume-on-judge
Judgement, for Vanilla-era paladins. Branch `feat/era-talents`, merged to master 2026-08-25.
**Spec:** `docs/superpowers/specs/2026-08-20-paladin-vanilla-seal-reconstruction-design.md`
**Plan:** `docs/superpowers/plans/2026-08-20-paladin-vanilla-seal-reconstruction.md`
**Generation stamp:** `7415b69e`
**Ids:** 932700–932748 (framework helper registry updated).

## Commits (feat/era-talents)

| Commit | Task | Content |
|---|---|---|
| 3c5b00f / af98f07 | 1 | Seal of Righteousness 8-rank (seal + on-hit + JoR); icon corrected 274→25 |
| 7bdc50c | 2 | `era_pal_seal_of_righteousness` weapon-speed on-hit AuraScript |
| 9934850 | 3 | Seal of Justice (reuses stock stun 20170 + JoJ 20184) |
| c17c555 | 4/5b | Seal of Light + `era_pal_judgement_of_light` FLAT-heal AuraScript |
| c3efd2e | 6 | Seal of Wisdom + `era_pal_judgement_of_wisdom` FLAT-mana AuraScript |
| 007cbfe | 7 | Custom single `Judgement` 932746 + `era_pal_judgement` unleash/consume SpellScript (cd 15s per 1.12.1, effect 77) |
| 883c35b | 8 | Spellmod rebinds (Improved SoR c:1024; Benediction a:41943040 = bit25\|bit23) |
| 62b2394 | 9 | Reconcile grant-by-level + version-swap strip (stock 20154/20164/20165/20166 + 20271/53407/53408 under ERA_VANILLA) + kReconcileOnLearn + doctor |

Each task passed two-stage review (spec compliance + code-quality; all four C++ scripts opus-reviewed
against the fork headers).

## HEADLESS GATE — PASSED (built + booted on the dev box, 2026-08-20)

- **Compiles:** `docker compose build ac-worldserver` exit 0 — all four era C++ scripts
  (`era_pal_judgement`, `era_pal_seal_of_righteousness`, `era_pal_judgement_of_light`,
  `era_pal_judgement_of_wisdom`) + the `ReconcileBaselineSpells`/`kReconcileOnLearn`/doctor changes
  compiled with no errors.
- **Applies:** `ac-db-import` exit 0; reapplied the changed `era_talent_meta` + paladin custom-spells
  SQL. Live-DB confirmation:
  - `spell_dbc`: **49** rows for ID 932700–932748.
  - `spell_proc`: seals (932700-707/932724/932725-728/932737-739) ProcFlags 20 (SoJ chance 25, rest
    100); JoL (932733-736) + JoW (932743-745) ProcFlags **139944** (TAKEN-melee/ranged) chance 100.
  - `spell_script_names`: 932700-707→`era_pal_seal_of_righteousness`, 932733-736→`era_pal_judgement_of_light`,
    932743-745→`era_pal_judgement_of_wisdom`, 932746→`era_pal_judgement`.
- **Boots clean:** worldserver log `[mod-era-talents] loaded 339 talent nodes (generation 7415b69e)`
  and `WORLD: World Initialized In 0 Minutes 14 Seconds`. **Zero** errors mentioning any 9327xx id,
  an era script name, `spell_proc`, or `spell_dbc`.
- **Client patch:** `client-addons/_data-patches/patch-V.mpq` regenerated at 7415b69e (Spell.dbc +
  SkillLineAbility.dbc — the builder emits 17 SkillLineAbility rows for the new castable seals +
  Judgement, skillLine 184, so they land in the Retribution spellbook tab).

## Review-confirmed invariants (do not re-derive)

- **No double-proc:** the modern proc engine fires auras solely from the `spell_proc` table (keyed by
  id), never the clone's inherited DBC ProcFlags — so exactly one proc entry per JoL/JoW clone.
- **Flat delivery:** the JoL/JoW/SoW heal+energize carriers (932747/932748 + on-hit energize) are
  `SPELL_DAMAGE_CLASS_NONE` → the core done-side returns the flat amount (no spellpower inflation);
  only the recipient's own healing-received % applies.
- **Version-swap is era-safe:** the stock-seal/judgement strip and the clone grants both run ONLY under
  `era == ERA_VANILLA` inside the paladin block; a WotLK/TBC paladin never hits the strip, and the
  else-branch strips clones on any higher era (no double-holding either direction).
- **Single Judgement handles all six seals:** `era_pal_judgement` scans for any `SPELL_SPECIFIC_SEAL`
  with an EFFECT_2 DUMMY — covering the four new seals AND the shipped SoC (932606) / SotC (932634-639),
  which all carry that DUMMY. Consume via `RemoveAura(sealId)`.

## Final holistic review — PASSED (no blocking findings)

A cross-cutting seam review (all 9 commits) confirmed: unleash+consume is uniform across all six
seals (four new + SoC + SotC), the reconcile grant/strip/doctor triad is complete and consistent, the
id band is contiguous/collision-free, spellmod rebinds land without over-bind, and every
trigger/DUMMY/carrier id resolves. `era_audit` 0. Two minor, non-blocking findings:

- **Optional polish — multi-rank seals have no `spell_ranks` chain.** SoR (8), SoL (4), SoW (3) each
  grant separate castable spellbook entries, so a capped paladin sees N seal icons rather than one
  collapsed entry. This MATCHES the shipped SotC precedent (also unchained) so it is not a regression,
  and it is functionally harmless (SPELL_SPECIFIC_SEAL mutual exclusion prevents rank-stacking; the
  reconcile strip iterates ranks explicitly, needing no cascade). It diverges from the chained actives
  (LoH/Holy Shock/Holy Shield/BoS). If spellbook cleanliness/Vanilla fidelity is wanted, add chains
  932700→…→707 / 932725→…→728 / 932737→…→739 (and, for consistency, SotC 932634→…→639) — but that
  changes reconcile strip semantics (removeSpell cascades on a chain), so it needs its own runtime
  test. **Deferred pending the user's call after the real-client pass.**
- **SoC/SotC now consume-on-judge** (see checklist item 4) — intended, but re-verify since it's new
  behavior for them.

## REMAINING — real-client pass (user-owned; requires a HUMAN Vanilla paladin)

Runtime proc behavior cannot be exercised headlessly: playerbots are `IsBot`-guarded out of reconcile
(they keep WotLK seals), so only a human era-Vanilla paladin has the reconstructed seals. Checklist for
the in-client pass (install patch-V.mpq gen 7415b69e + restage the EraTalents addon + relaunch; canary
must be green):

1. **Seals:** each seal castable, 30s buff, one-at-a-time (mutual exclusion), correct spellbook tab +
   tooltips (spellbook Description + buff AuraDescription read the era text). Icons: SoR 25, SoJ 307,
   SoL 299, SoW 206, Judgement 205.
2. **On-hit procs:** SoR deals Holy damage per swing that scales with weapon speed (test a slow 2H vs a
   fast 1H — slower should hit harder per swing); SoL heals; SoW restores mana; SoJ chance-stuns.
3. **Judgement:** the single Judgement button applies the active seal's judgement AND removes the seal
   (consume); with no seal up, nothing happens.
4. **SoC/SotC regression:** with the stock judgement buttons stripped, Seal of Command and Seal of the
   Crusader still unleash their judgements through `era_pal_judgement` (JotC debuff / SoC holy hit land;
   seal consumed).
5. **Spellmods:** Improved SoR raises SoR on-hit + JoR damage; Benediction lowers seal + Judgement mana
   cost; Improved Judgement lowers the Judgement cooldown; Lasting Judgement extends the JoL/JoW debuff.
6. **WotLK regression:** a WotLK-era paladin still has all stock seals + 3 judgement buttons and none of
   the clones (era-transition strips both directions).

### Tracked runtime items to confirm/retune during the pass

1. **SoR on-hit spell-power coefficient:** 932708-715 are flat `SPELL_EFFECT_SCHOOL_DAMAGE`; the core
   may add its default instant SP coefficient on top of the weapon-speed base. Measure; if it distorts
   Vanilla values, add a no-done-bonus attribute to the on-hit spells (else document as accepted — Ret
   SP is modest). (The JoL/JoW/SoW paths are already confirmed flat.)
2. **Proc chances:** SoJ stun `chance 25` and JoL/JoW per-hit `chance 100` (vs stock 15 PPM) are
   Vanilla-faithful choices; confirm feel and retune if needed.
3. **Judgement cooldown 15s** is the 1.12.1-canonical value (community memory of 10s is the WotLK value)
   — confirm it's the intended feel.

## v2 — TRAINER-TAUGHT rework (2026-08-20, after the first real-client test) — built + DB-verified

The first in-client test (char "Paladintest", Vanilla) showed the obtain-model was wrong: seals were
**auto-granted** by level and the **stock WotLK seals stayed trainable**. Owner intent: custom seals must
be **trainer-taught**, stock WotLK versions must **not** be trainable. Fixed in commits `923107c` (rework)
+ `7ceeca6` (drop stale paladin stock ids from `kReconcileOnLearn`, kills the WotLK-auto-grant re-entrancy)
+ the doctor-marker fix. Opus-reviewed (no WotLK regression). Model now:

- A hidden Vanilla-only passive **marker 932749** (Attributes 262608) is reconcile-granted to every Vanilla
  paladin. Every seal/Judgement/LoH **rank-1** `trainer_spell` row has `ReqAbility1 = 932749`; higher ranks
  chain off rank 1 → a WotLK paladin (no marker) cannot train the chains. `spell_ranks` collapse each seal
  to one spellbook icon. (Baseline analog of the talent-anchored Holy-Shock/Bloodthirst trainer chains.)
- **All five seals (SoR/SoJ/SoL/SoW/SotC) + Judgement + LoH are trainer-taught** — nothing player-visible is
  auto-granted (only the invisible marker).
- Stock WotLK seals (20164/20165/20166), judgement buttons (53407/53408) and LoH (633/2800/10310/27154)
  trainer rows are **deleted** (`2026_08_20_40_era_talent_paladin_seal_trainers.sql`); TBC/WotLK paladins
  auto-learn the stock spells by level via the reconcile else-branch (`kWotlkPalStock`, incl. the
  core-auto-learned 20154 SoR / 20271 Judgement). Reconcile: Vanilla = grant marker + strip the 11 stock
  ids; else = strip marker + all custom ranks + auto-grant the 11 stock ids by level.

**Built + booted + DB-verified on the dev box (gen `91697bef`):** build exit 0; db-import exit 0; worldserver
`loaded 339 talent nodes (generation 91697bef)` + World Initialized, zero errors on any 9327xx id / era
script / trainer / spell_proc. Live DB confirms: **0** stock seal/judgement/LoH rows remain on trainers
3/4/5; the custom rank rows are present with correct `ReqAbility1` chains (rank 1 → 932749); `spell_ranks`
chains present (SoR 8 / SoL 4 / SoW 3 / SotC 6 / LoH 3); marker 932749 in `spell_dbc`.

### v2 real-client re-test (Paladintest, after relog so reconcile v2 runs)

1. **Trainer:** NO WotLK seals/judgement-buttons/LoH offered; the Vanilla seals + Judgement + LoH are
   **trainable** at the level gates (SoR L1/10/18/26/34/42/50/58, SoJ L22, SoL L30/40/50/60, SoW L38/48/58,
   SotC L6/12/22/32/42/52, Judgement L4, LoH L10/30/50). Each seal shows as ONE spellbook entry (highest
   trained rank), not N icons.
2. **Nothing auto-granted** — a fresh Vanilla paladin has no seals until trained (only the invisible marker).
3. The earlier runtime checklist (on-hit procs, Judgement unleash+consume, SoC/SotC, spellmods) still applies
   once trained.
4. **WotLK paladin** (progression > Naxx40): still has the stock seals/judgement-buttons/LoH (auto-learned),
   none of the customs, no marker.
5. `.eratalents doctor Paladintest` should be clean (marker 932749 + trained seals recognized, no false
   ORPHAN).

## Fix round 3 (2026-08-20, after 2nd real-client test) — built + server-verified (gen `26787c17`, commit `3ce6b73`)

Second in-client test (Paladintest, L60) found: (1) seal mana far above the flat tooltip; (2) SoR on-hit
deals damage but is invisible in the combat log; (3) stock "Judgement of Light" (20271) lingering; (4) doubt
about the 8 SoR ranks. Root causes + fixes:

1. **Mana** — the seal clones inherited `ManaCostPct=14` from template 20375, so the core charged flat +
   14% base mana. Fixed: `manaCostPct: 0` on all seal clones (Judgement 932746 keeps 5% — Vanilla-correct).
   Server-verified: 932700=`ManaCost 20/Pct 0`, 932707=`175/0`, etc.
2. **Invisible on-hit** — 932708-715 (+ SoL 932729-732 / SoW 932740-742) had no `client:` block → not in the
   client Spell.dbc → combat log couldn't name them. Fixed: added `client:{name,icon}` (no skillLine, so no
   spellbook row). Client-side — needs the new patch-V.mpq.
3. **Lingering 20271** — DB proof: Paladintest had the marker + all trained customs; 20154 was correctly
   stripped (the "WotLK SoR in spellbook" was stale/transient); stock 20271 (not on any trainer) was learned
   mid-session after the login strip and not re-caught because commit 7ceeca6 had removed the stock ids from
   `kReconcileOnLearn`. Fixed: added a **re-entrancy guard** (static bool + RAII) to `ReconcileBaselineSpells`
   (the correct fix for what 7ceeca6 worked around) + **restored** the 11 stock ids to `kReconcileOnLearn` for
   instant re-strip. Server-side (worldserver restart + relog).
4. **SoR ranks** — re-confirmed **8** from the 1.12.1 client Spell.dbc (canonical source; same as SotC).

Server-verified: build exit 0, db-import exit 0, boot clean (gen 26787c17, zero id errors), seal ManaCostPct=0.
**Client reinstall required** (patch-V.mpq gen 26787c17 + relog) for the on-hit combat-log names and the mana
tooltip; the actual mana cost and the 20271 re-strip are server-side (live after the restart + relog).

## Fix round 4 (2026-08-20, 3rd real-client test) — built + server-verified (gen `f5df473a`, commit `d68908d`)

Third test found: (1) Seal of Command (talent-granted 932606/932607, PREDATES the rework) had the same
mana + invisible-damage bugs as the new seals; (2) WotLK SoR (20154) still phantomed in the Holy tab.
- **SoC/BoS mana:** 932606 (SoC) inherited `ManaCostPct=14` from template 20375; BoS 932617/661/662/664
  inherited `ManaCostPct=7` from 20911. Fixed: `manaCostPct: 0` on all five. Server-verified 932606=65/0.
- **SoC damage invisible:** proc/judgement helper 932607 had no `client:` block → invisible combat log.
  Fixed: `client:{name: Seal of Command, icon: 561}` (no skillLine → no spellbook row).
- **20154 phantom:** server-side CORRECT (not in char_spell) — it's a client-display race: 20154 is a core
  skill-reward auto-learn (SkillLineAbility AcquireMethod=2, skill 594=Holy → the Holy tab), re-taught every
  login, and `removeSpell` suppresses the client packet during `PlayerLoading` (Player.cpp:3388). Fixed: an
  in-world re-strip of the 11 stock seal/judgement/LoH ids in `OnPlayerUpdate`'s throttled block (packets
  flow in-world → client updates; fires once per login, then no-ops). Server-side (live after restart).
- **SoC ranks:** the 1.12.1 dbc shows Seal of Command had **5 ranks** (20375/20915/20918/20919/20920); ours
  is single-rank. OPEN DECISION with the user (add R2-5 as talent-R1 + trainer chain, like Holy Shock).

Client reinstall (patch-V.mpq gen f5df473a) needed for the SoC combat-log name + the SoC/BoS mana tooltip;
the 20154 in-world strip is server-side.
