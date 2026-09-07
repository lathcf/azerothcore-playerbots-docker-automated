# Era-Talents — Final All-Class Review (findings report)

> **Generation superseded (2026-09-06 final review):** the current stamp is the closing row of docs/verification/era-talents-final-review-tbc-vanilla.md §10 — reinstall patch-V.mpq + restage the addon before any real-client pass.

**Date:** 2026-08-24 · **Branch:** `feat/era-talents` (merged to master 2026-08-25) · **Generation of record:** `71272694`
**Scope:** the final cross-cutting QA gate across all 9 Vanilla talent trees before the branch merge.
**Method:** subagent fan-out — 1 headless-integrity auditor + 9 per-class auditors (each covering
Vanilla fidelity / baseline-leak / grant-accuracy + an accepted-gaps review), then coordinator
synthesis with 1.12.1 `Spell.dbc` arbitration of every flagged magnitude. Plan:
`docs/superpowers/plans/2026-08-24-era-talents-final-all-class-review-plan.md`.

> **Bottom line:** the branch is in **strong shape**. Headless integrity is clean; there are **no
> baseline leaks, no dangling references, no id collisions, no broken procs**. Every per-class tree
> is fidelity-clean except a small, coherent class of residue: **granted/cloned *stock actives* that
> still carry a WotLK value/cooldown.** Four such items warrant a fix-or-accept decision before
> merge (§2); everything else is cheap optional polish or a documented KEEP gap (§3–§4). Nothing in
> this pass was changed — it is read-only; fixes go through the normal regen/rebuild + review loop
> after your review.

---

## UPDATE 2026-08-24 — all findings dispositioned & FIXED (generation `e0a422f3`)

The four merge-gate items (§2) plus the three cheap closes (§3) and the doc-hygiene items were all
implemented, regenerated, rebuilt, and DB-verified on the dev box. Changes were subsequently
committed to `feat/era-talents` (merged to master 2026-08-25). An independent code review of the diff returned
**CLEAN** (no blocking issues; era-gating in both new reconcile blocks verified sound).

| Item | Fix shipped | DB-verified |
|---|---|---|
| **F-1** Warlock Dark Pact | 3-rank Vanilla clone chain 932784/785/786 (drains 150/200/250, `EffectMultipleValue=1.0` return preserved, pet target); node 18217→932784; R2/R3 trainer-taught (L50/L60, ReqAbility-gated) + spell_ranks; stock chain reconcile-stripped in Vanilla | ✓ |
| **F-2** Mage Presence of Mind | clone 932761 (3-min cd, category neutralized, 12043's effect+mask+ProcCharges inherited); node 18013→932761; stock 12043 reconcile-stripped in Vanilla | ✓ |
| **F-3** Rogue Preparation | helper 932983 `cooldown: 600000` (10 min) + description corrected | ✓ |
| **F-4** Hunter Improved Aspect of the Hawk | re-modeled to the faithful 1.12.1 single spellmod (aura 108 / op ALL_EFFECTS / +4/8/12/16/20% to AotH's RAP bonus; **Quick Shots proc removed** — it is a TBC addition) | ✓ |
| **Close** Warrior Defiance | `stances: 131072` — threat now gated to Defensive Stance (ShapeshiftMask verified) | ✓ |
| **Close** Priest Holy Reach | Holy Nova 15237 added to the radius spellmod (Effect_2 mask 512→4194816) + `_ref` entry | ✓ |
| **Close** Priest Spirit of Redemption | tooltip states the +5% Spirit (text only) | ✓ |
| **Doc** | stale comments fixed (paladin 18633, shaman Fire-Nova delay, warlock 1.5s-cast); CLAUDE.md patch 0019 row added | — |

**Verification:** `era_audit.py` 0 findings · `tools/` pytest 149 passed · regen deterministic (stamp
`e0a422f3`) · worldserver boots `loaded 432 talent nodes (generation e0a422f3)` with no errors on any
new id/script · every generated `spell_dbc`/`trainer_spell`/`spell_ranks`/`era_talent_rank` row
DB-confirmed. **Remaining before merge:** a real-client pass (§5 checklist) on a fresh `patch-V.mpq`
(gen `e0a422f3`) — especially F-4 (Hunter Aspect-of-the-Hawk now scales the AP bonus and no longer
procs Quick Shots haste, a deliberate fidelity correction) and the F-1/F-2 trainer/cooldown behavior;
and `git add` the new untracked SQL (`2026_08_24_10_..._dark_pact_trainer.sql`) before committing.

---

## 0. Headless integrity gate (Phase A) — GREEN

| Check | Result |
|---|---|
| `python3 tools/era_audit.py` | **0 findings** (re-run independently by the Phase-A agent → 0) |
| `tools/` pytest (venv) | **149 passed** |
| Regen determinism (`tools/era-regen.sh` → `git diff`) | **clean tree**, stamp reproduces `71272694` |
| Per-class node counts | 52 / 44 / 46 / 51 / 47 / 46 / 49 / 50 / 47 = **432**; DK (6) absent ✓ |
| `display_only` nodes | **zero** (one stale *comment* only — paladin.yaml:1185) |
| Helper-id registry | **no cross-dataset id collision** (1616 distinct custom ids); sentinel `932999` unused; DUMMY-markers 1–15 accounted for, **16 genuinely free** |
| Totem creature band `[920100,920290]` | 69 creatures, no stock collision, no orphan |
| Dangling references | all 1204 era-band + 153 stock grants, 140 `EffectTriggerSpell`, all `spell_ranks`/`trainer_spell`/`spell_proc` targets, all `affects:`/`affectsMask`, all 25 `spell_script_names` **resolve** |
| `spell_proc` sanity (250 rows) | no `HitMask=8` never-fires, no dead `Chance=0&PPM=0`, heal `SpellTypeMask=2` applied everywhere |

---

## 1. Per-class verdicts

| Class | Nodes | Verdict | Merge-relevant residue |
|---|---|---|---|
| Priest (5) | 47 | **CLEAN** | — (2 cheap optional tooltip closes) |
| Paladin (2) | 44 | **CLEAN** | — (stale docs only; Repentance owner-reconfirm) |
| Druid (11) | 47 | **CLEAN** | — (Thick Hide confirmed correct) |
| Warrior (1) | 52 | **CLEAN** | Defiance stance-gate = cheap CLOSE opportunity |
| Shaman (7) | 46 + 18 totems | **CLEAN** | — (stale comment; unverifiable placeholders) |
| Rogue (4) | 51 | MINOR | **Preparation cd 8→10 min (Important)** |
| Hunter (3) | 46 | MINOR | **Aspect of the Hawk proc-chance modeling (Important, needs decision)** |
| Mage (8) | 49 | MINOR | **Presence of Mind cd 2→3 min (Important)** |
| Warlock (9) | 50 | MINOR | **Dark Pact 304→150 mana (Important)** |

**Cross-cutting theme (regression insight):** the 2026-08-18 grant-accuracy pass — which cloned
WotLK-diverged *granted actives* to Vanilla values (Bloodthirst, Arcane Power, Adrenaline Rush,
Conflagrate, etc.) — was **incomplete**. Two more granted stock actives with the identical profile
(non-script-bound, WotLK-retuned) were missed: **Mage Presence of Mind (12043)** and **Warlock Dark
Pact (18220)**. Both are the exact case that pass existed to catch. This is the single most valuable
result of the review. All other classes' grant-accuracy dimension is clean.

---

## 2. Merge-gate findings (fix-or-explicitly-accept before merge)

All four are the same shape: a talent hands the player a **stock/cloned active that ships a WotLK
value**, while its own tooltip states the Vanilla value. None leaks; none is script-bound. Each is a
clean clone-or-accept decision.

### F-1 · Warlock Dark Pact ships 304 mana drain, not Vanilla 150 — **Important**
- **Node 18217** does `grants: {1: [18220]}`. 3.3.5a `18220` = `POWER_DRAIN 304` (WotLK), no script.
  Tooltip says "Drains 150…". **1.12.1 `Spell.dbc` CONFIRMS Vanilla Dark Pact = 150 / 200 / 250**
  (ranks 18220 / 18937 / 18938). The era grants only rank 1, so an era warlock is stuck at 304
  labelled 150.
- **Why it slipped:** every *other* diverged active in the tree was cloned (Conflagrate→932780,
  Siphon Life→932902, Amplify Curse→932918, Demonic Sacrifice→932929); Dark Pact was left a raw grant.
- **Fix:** clone `18220`→a free warlock-band id (`POWER_DRAIN` base 150, no coefficient, instant,
  20yd, Demonology tab), grant it from 18217, reconcile-strip stale 18220 in the CLASS_WARLOCK block.
  Ideally clone the full 3-rank chain (150/200/250) like Conflagrate. **Or** accept WotLK 304 and
  correct the node tooltip. Evidence: `warlock-findings.md` F1; 1.12.1 dump above.

### F-2 · Mage Presence of Mind ships 2-min cd, not Vanilla 3-min — **Important**
- **Node 18013** grants stock `12043`; 3.3.5a `RecoveryTime = 120000` (2 min); **not script-bound**
  (absent from `spell_mage.cpp`). Node tooltip says "3 min". 1.12.1 confirms 12043 is the
  instant-cast talent; Vanilla cd = 3 min (well-documented; matches the tooltip). This is the exact
  twin of Arcane Power 12042, which the 2026-08-18 pass cloned to 932760 — 12043 was missed.
- **Fix:** clone `12043`→`932761` (band free), `cooldown: 180000`, neutralize inherited Category
  1151, grant from 18013, reconcile-strip stale 12043 — the Arcane-Power recipe verbatim. **Or**
  accept the player-favored 2-min and add an explicit "accepted divergence" comment alongside Cold
  Snap/Combustion. Evidence: `mage-findings.md` #1.

### F-3 · Rogue Preparation ships 8-min cd, not Vanilla 10-min (+ panel/spellbook contradiction) — **Important**
- Clone `932983` inherited stock WotLK 14185's `RecoveryTime = 480000` (8 min) and **never overrode
  `cooldown:`**. **Node 18447 talent-panel tooltip says "10 min"; the clone's own spellbook
  description says "8 min"** — an internal contradiction independent of any source question. Vanilla
  Preparation = 10 min (WotLK shortened it to 8).
- **Fix:** add `cooldown: 600000` to helper 932983 (generator already supports it — Adrenaline Rush
  932770 does exactly this) and set the clone description to "10 min". One-line generator fix +
  regen. Evidence: `rogue-findings.md` #1.

### F-4 · Hunter Improved Aspect of the Hawk — proc-chance modeling doesn't match 1.12.1 — **Important (needs owner decision)**
- The node ships a per-rank **Quick Shots proc chance of 1/2/3/4/5%** + a flat +30% haste. The
  1.12.1 `Spell.dbc` shows Improved Aspect of the Hawk (19552–19556) with a single per-rank scaling
  effect of **4/8/12/16/20%**, described as *"increases the ranged attack power bonus of your Aspect
  of the Hawk by X%"* — i.e. the per-rank value scales the **aspect's AP bonus**, not a proc chance.
  The shipped 1/2/3/4/5 matches **neither** that nor the auditor's hypothesized 3/6/9/12/15.
- **This is the one item that needs an owner call, not a one-line fix.** Options: (a) re-model the
  node to scale Aspect of the Hawk's AP bonus by 4/8/12/16/20% with Quick Shots as a flat-chance
  proc, or (b) keep the proc-chance model but re-derive the correct value. A full 1.12 effect-type
  decode (aura + misc, beyond the verified base-point reader) is advisable before choosing.
  Evidence: `hunter-findings.md` #1; 1.12.1 dump (19552–19556 Effect_1 = 4/8/12/16/20).

---

## 3. Minor findings & cheap optional closes (not merge blockers)

**Cheap CLOSE opportunities (clean, low-risk — fold in if you want strict fidelity):**
- **Warrior Defiance (18544)** — the "always-on threat" gap is now expressible: add `stances: 131072`
  (FORM_DEFENSIVESTANCE) to the `stat` node to restore the Vanilla Defensive-Stance gate byte-for-byte,
  **zero helpers/clones**. The YAML's "needs 5 clones" rationale is stale. (`warrior-findings.md` #1.)
- **Priest Holy Reach (18124)** — op:radius `affects` covers Prayer of Healing only, but the tooltip
  also promises Holy Nova radius; add Holy Nova 15237 to the affects list + `_ref` (`flagsA:4194304`).
  Node currently under-delivers vs its own tooltip. (`priest-findings.md` A.)
- **Priest Spirit of Redemption (18128)** — tooltip omits the real +5% Spirit; one-line text append.

**Accept / document (against-player or cosmetic, low ROI to close):**
- **Mage Ice Block (18046)** grants WotLK 45438 carrying Hypothermia (41425) — blocks a Cold
  Snap→Ice Block re-cast that Vanilla allowed. Niche, against-player; needs a clone to strip. KEEP.
- **Warrior Improved Bloodrage (18538)** — 1.12.1's primary effect is a **−2s/−5s duration
  reduction**; the era models **+2/+5 instant rage**. Different mechanic, ~1-rage impact. KEEP or
  revisit with a full effect decode. (`warrior-findings.md` #2.)
- **Warlock** stale "1.5s cast" Conflagrate comments (data correctly ships instant); Fel Domination
  tooltip "5 sec" vs authentic 5.5s. Cosmetic. (`warlock-findings.md` #2, #3.)
- **Paladin** stale `display_only`/`932600-601 reserved` comment (paladin.yaml:1185 — node 18633 is
  WIRED); `phase8-paladin.md` predates the seal reconstruction and lists now-closed gaps. Doc-only.
- **Shaman** stale "Improved Fire Totems delay = accepted gap" comment (the CreatureAI now implements
  it); totem duration/radius **tooltip text** not DBC-verified (gameplay correct — indices inherited);
  Windfury 20% / Fire Nova ~4s are community-standard placeholders unreadable from the 1.12 extract.
- **Rogue** no stale-strip for stock Hemorrhage 16511 (theoretical only — branch never deployed);
  Master of Deception stealth-level values are an inferred approximation (endpoint +15 preserved).
- **Phase A** — 8 stale orphan `spell_dbc` rows on the *incremental dev DB* (924288-292 / 928131-132
  / 932414), all unreferenced and **absent on a fresh import** (generated SQL uses per-id rather than
  band-wide DELETE); optional generator tweak = leading `DELETE FROM spell_dbc WHERE ID BETWEEN
  920000 AND 932998`. And **`patches/0019` (Corruption era cast-time) is missing from the CLAUDE.md
  patch-inventory table** — add a 0019 row.

**1.12.1-arbitrated — RESOLVED IN THE IMPLEMENTATION'S FAVOR (open questions now closed, no change):**
- **Druid Thick Hide** — 1.12.1 = **5-rank 2/4/6/8/10%** (ids 16929–16933, "in Bear Form"). Era is
  **exactly correct**; the "maybe 3-rank" worry is refuted (the 3-rank chain is the WotLK version).
- **Warlock Pyroclasm** — 1.12.1 = **13% / 26%** stun chance (18096 / 18073). Era correct.
- **Priest Improved Psychic Scream** — 1.12.1 = **−2s / −4s** cd (15392 / 15448). Era correct.
- **Priest Power Infusion** — Vanilla PI is **+20% spell dmg/healing only**; the mana-cost reduction
  is a WotLK addition. The era clone's model is Vanilla-faithful. KEEP.
- **Paladin Repentance** — 1.12.1 confirms **Humanoid-only** (era grant carries WotLK's broadened
  creature types + 60s). Owner-accepted LAN CC-generosity; now cleanly closeable via a clone
  (durationIndex + TargetCreatureType) since TargetCreatureType is a plain DBC field.

---

## 4. Consolidated accepted-gaps ledger (one merge-gate decision)

Every documented gap across all 9 classes, with a KEEP / CLOSE recommendation. **CLOSE (done)** =
already closed in the shipped DB, only older docs are stale. **CLOSE (cheap)** = a clean small fix
worth optionally folding in. **KEEP** = deliberate, negligible, core-patch-only, or runtime-only.

| Class | Gap | Rec | Note |
|---|---|---|---|
| Warrior | Defiance always-on vs Defensive-Stance gate | **CLOSE (cheap)** | `stances: 131072`; zero helpers |
| Warrior | Improved Bloodrage model (instant-rage vs 1.12 duration-reduction) | KEEP | ~1-rage; full decode if strict |
| Warrior | Improved Taunt cd 8→7/6 vs Vanilla 10→9/8 | KEEP | divergence is baseline Taunt; player-favored |
| Warrior | Improved Shield Block duration-model | KEEP | WotLK base Shield Block has no charge cap |
| Warrior | Granted MS/Shield-Slam ranks carry WotLK per-rank tuning | KEEP | mechanic identical; rank-1 Vanilla-exact |
| Paladin | Repentance 60s/broad-creature vs Vanilla Humanoid-only | KEEP (reconsider) | most player-visible; closeable via clone |
| Paladin | Seal of Command flat-40% proc vs ~7 PPM | KEEP | closeable if generator gains a `ppm` key |
| Paladin | Seal of Justice stun-chance 25 placeholder | KEEP | same generator-`ppm` path |
| Paladin | Improved Concentration Aura group-resist rider omitted | KEEP | separate group-aura effect; niche PvP |
| Paladin | Improved LoH armor rider / Holy Shock ranks / SoC ranks / seal spell_ranks | **CLOSE (done)** | shipped; only phase8 doc is stale |
| Paladin | Seal AP flat (per-level scaling dropped, ≤~19 AP at cap) | KEEP | generator design; negligible |
| Hunter | Aspect-of-the-Hawk proc-chance modeling | **see F-4** | needs owner decision |
| Hunter | Improved Feign Death inert (core has no FD-resist roll) | KEEP | core-patch-only; matches retail WotLK |
| Hunter | Bestial Swiftness / Efficiency-vs-Aimed / Frost Trap 67035 / Trueshot caster-extra / Deterrence trainer-strip / slaying scope | KEEP | all reviewed; core/engine limits or negligible |
| Hunter | Intimidation 8%-base vs flat-137 mana / Scatter Shot free vs ~123 | KEEP | behaviorally trivial at 60 |
| Rogue | Preparation cooldown 8→10 min | **see F-3** | Important |
| Rogue | Weapon Expertise / Mace Spec +skill / Serrated Blades armor-pen mappings | KEEP | no live weapon-skill stat on 3.3.5a |
| Rogue | Premeditation 20s window (vs 10s) | KEEP (closeable) | benign; clone if strict |
| Priest | Holy Reach radius / Spirit of Redemption tooltip | **CLOSE (cheap)** | one-line each |
| Priest | Mental Agility instant-set coverage / Lightwell heal / Improved Psychic Scream / PI / VE split | KEEP | Psychic Scream & PI now 1.12.1-confirmed correct |
| Mage | Presence of Mind cd 2→3 min | **see F-2** | Important |
| Mage | Cold Snap / Combustion cd (script-bound) | KEEP | clone would lose behavior; player-favored |
| Mage | Ice Block Hypothermia / Magic Absorption mana rider | KEEP | against-player niche / low-ROI rider |
| Warlock | Dark Pact 304 vs 150 | **see F-1** | Important |
| Warlock | Demonic Sacrifice WotLK magnitudes | **CLOSE (done)** | already Vanilla-valued 932903-906 |
| Warlock | ISB charge-cap / Firestone PPM (closeable) + 8 other pet/school-scope gaps | KEEP | faithful analogs or cosmetic |
| Druid | Heart of the Wild per-form / Moonkin Aura | KEEP | core-hardcoded; core-patch-only |
| Druid | Insect Swarm magnitude / Feline Swiftness outdoor | KEEP (closeable) | fix-round machinery now exists; low value |
| Druid | Improved Enrage stacking | **CLOSE (done)** | clone 932513 makes +5/+10 the only instant |
| Shaman | Grace of Air / Windwall / Mana Tide flat / Fire Nova delay / Totemic Mastery radius | **CLOSE (done)** | totem reconstruction + rebinds shipped |
| Shaman | Rockbiter INERT / Flametongue-Totem boost / Improved Reincarnation / Eye of the Storm / Stormstrike strike-count | KEEP | core-patch-only or beneficial-simplifying |
| Shaman | Convection tooltip client-cache / totem tooltip text / WF 20% / Fire Nova ~4s | KEEP | client/display or unverifiable-by-extract |

---

## 5. Per-class in-game re-test checklist (before merge)

Headless proves structure; these are the **real-client-only** confirmations (procs fire, tooltips/
icons render, trainer gating, no leak on a REAL player — bots skip era-transition hooks). Run on a
human character of each class after installing the current `patch-V.mpq` (gen `71272694`) + restaging
the EraTalents addon. **If any F-1…F-4 fix is applied first, re-stamp and reinstall the MPQ.**

- **All classes:** canary green at login (no red generation warning); tab backgrounds render; a
  talent's SPELLMOD tooltip rewrites; a respec (`.eratalents reset`) leaves no orphaned granted spell.
- **Warrior:** Deep Wounds bleed + Improved Berserker Rage energize + Shield-Spec rage-on-block all
  FIRE in the combat log; Bloodthirst/Last Stand clones behave; (if closed) Defiance threat only in
  Defensive Stance.
- **Paladin:** each seal casts + its Judgement unleashes and consumes; SotC/JotC scale with Improved
  SotC; Holy Shock heals AND damages + Illumination refunds on its crit; block/crit procs fire;
  Consecration/BoK/GBoK NOT trainable without the talent; Repentance CC behaves as accepted.
- **Hunter:** (after the F-4 decision) Aspect-of-the-Hawk value is right; trap procs (Entrapment/
  Wing Clip/Concussive) fire; slaying auras apply; Aimed Shot keeps a 6s cd; pet talents reachable
  via the Pet Talents button; Deterrence not dual-held after purchase.
- **Rogue:** (after F-3) Preparation shows 10-min cd everywhere and resets all 5 incl. Blind; Sap
  breaks stealth in the Vanilla era; Hemorrhage 3-rank chain trains + procs.
- **Priest:** VE party-heal fires off Shadow damage (party only, not raid); Blackout stun icon shows
  + stuns; PI +20% dmg/healing; Divine Spirit/Holy Nova not trainable without the talent.
- **Mage:** (after F-2) PoM shows/uses 3-min cd; Arcane Power +30%/+30%; ward reflect procs; Blizzard
  chill slows; Ice Block not trainable without the talent.
- **Warlock:** (after F-1) Dark Pact drains the Vanilla amount; Conflagrate is instant + consumes
  Immolate; pet-mechanic passives (MD/Soul Link/Improved Firebolt-Lash-Voidwalker) apply + auto-strip
  on pet loss; Firestone/Spellstone use/equip + era-gated trainer.
- **Druid:** form-gating toggles in/out of form; LotP + Moonkin party auras radiate; Omen procs at a
  sane rate (not every hit); Nature's Grasp costs mana + outdoor-only; Enrage 25/30 with Improved.
- **Shaman:** every reconstructed totem summons + pulses; Totemic Mastery widens totem buff radii
  incl. Windfury Totem; Windfury Totem does NOT stack with Windfury Weapon; EM guarantees a crit +
  free cast on the next elemental spell; 2H axe/mace gated on the talent.

---

## 6. Recommendation

The branch is merge-ready **once F-1…F-4 are dispositioned** (fix-or-explicitly-accept — three are
one-liner clones/overrides, F-4 needs an owner modeling call). The cheap closes in §3 (Defiance
stance-gate, Holy Reach, SoR tooltip) and the doc-hygiene items (stale comments, CLAUDE.md patch
0019) are optional polish. Everything else is a documented KEEP. After any data fix: re-run
`tools/era-regen.sh`, confirm `era_audit.py` = 0 and the pytest suite green, rebuild/restart the
worldserver, reinstall the MPQ, and run the §5 checklist for the touched classes. **Do not merge
until you have signed off** — this pass audits and reports only.
