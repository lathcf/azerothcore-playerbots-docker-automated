# TBC Phase 3 (Shaman) — PHASE CLOSED 2026-09-02

> **Generation superseded (2026-09-06 final review):** the current stamp is the closing row of docs/verification/era-talents-final-review-tbc-vanilla.md §10 — reinstall patch-V.mpq + restage the addon before any real-client pass.

**USER SIGNED OFF 2026-09-02 at generation `19e3a621`.** Close-out artifact:
`docs/verification/era-talents-tbc-phase3-shaman.md` (§11 is the sign-off). This doc is now a
CLOSED working-state record — kept for the Working rules below, which the next class phase
(WARRIOR, Phase 4) inherits. **Branch `feat/era-talents`, merged to master 2026-09-07** (all nine classes
+ final review + Vanilla regression before the merge; this phase's bundled Vanilla fixes ride
that same prod push).

This doc was deliberately CUT DOWN on 2026-09-01 from a 700-line narrative to this working state.
The full batch-by-batch reasoning (reviews, retractions, sweeps) is in git history at `f979e4c` and
in the `_ref` files' `open_questions:`/`accepted_gaps:`/`task3:` entries, which are the maintained
home of every finding. Do not grow this file back — see Working rules below.

## Working rules for the rest of this phase (2026-09-01 reset, user-directed)

The phase stalled once on process recursion — reviews of reviews, record-fix commits about prose in
earlier records, retractions of retractions. These rules exist so that cannot recur:

1. **One review per task, not per batch** — at the end of a task's implementation, before its
   commit is declared done. No standing second review unless the first finds a real data defect.
2. **Evidence = a clean `era_audit.py` (bare AND targeted) + a spot-check of the emitted SQL /
   live DBC for the fields the audit cannot see** (inherited masks, triggers, costs, schools).
   A reasoning essay is not evidence and is not written.
3. **Never fix prose in past narratives.** A wrong sentence in a closed batch note stays wrong;
   correct only files a future author executes from — the dataset comments, the `_ref` ledger,
   the plan. If a claim must be retracted, retract it at its ORIGIN entry in the `_ref`, one line.
4. **This doc gets the status table updated and at most one line per completed task.** Findings go
   in the `_ref`'s `open_questions:`/`accepted_gaps:`; decisions go in the dataset block comment at
   the site the next author reads. Nothing is recorded twice.
5. **Ids: take exactly what the ledger reserves; a borrow is one line in `lent_to` + one in the
   borrower's row.** The ledger's `task3:`-style receipt (rename `reserved_for_*` to a receipt key,
   enumerate ids into `used:`) is how a family records completion.

## Read these first, in order

1. `docs/superpowers/specs/2026-08-31-era-talents-tbc-phase3-shaman-content-design.md` — **the
   Amendments at the end (A.1–A.13, B.1–B.8.1) supersede the body wherever they conflict.**
2. `docs/superpowers/plans/2026-08-31-era-talents-tbc-phase3a-shaman-tree.md` — COMPLETE.
3. `docs/superpowers/plans/2026-08-31-era-talents-tbc-phase3b-shaman-totems.md` — Tasks 1–7
   COMPLETE; Tasks 8–9 remain.
4. `era-data/_ref/tbc/shaman-spells.yaml` — Plan 3a's reference (gaps, censuses, real-client queue).
5. `era-data/_ref/tbc/shaman-totems-tbc.yaml` — Plan 3b's reference. Its `id_allocation:` ledger is
   the id authority; its `open_questions:`/`accepted_gaps:` are the findings authority.

## Status

| | State |
|---|---|
| **Plan 3a (tree)** | **COMPLETE** — 9/9 tasks |
| **Plan 3b Task 1** (totem reference) | **COMPLETE** |
| **Plan 3b Task 2** (fresh TBC clones, 21 families) | **COMPLETE** — batches 1/2/3/4a/4b, all reviews actioned |
| **Plan 3b Task 3** (TBC-added 61–70 ranks) | **COMPLETE** (`bcbd127`) — all 12 families with added ranks; 30 spell ids + 14 creatures; ledger receipts in every family row. spell_ranks chains deliberately deferred to Task 5's hand SQL (the Vanilla precedent: they live in the trainer SQL) |
| **Plan 3b Task 4** (Totem of Wrath + Mana Tide) | **COMPLETE** (`361ee63`) — all 61 nodes wired; Mana Tide keeps the STOCK 10467→16191→39610 chain (LIVE_MATCH, `_ref` 16191/39610 settled grant-stock); `ResolveDetonateDelay` extended to 937664/937665 (gap 7 closed) |
| **Plan 3b Task 5** (marker + trainers + spell_ranks) | **COMPLETE** (`791b2a3`) — marker 947440; 85 trainer rows + 73 spell_ranks rows; Searing stock r2-r6 un-gated to prev-rank-only (grant-stock made trainable); 14 of 15 stock 61-70 rows re-gated behind 932417 (25361 = IP's row, chain-gated regardless); 36936 closed already-correct |
| **Plan 3b Task 6** (three-way reconcile split + doctor) | **COMPLETE** (`d9f8abf`) — TBC arm grants 947440; Searing kept, 6495 + 61-70 stock ranks + 3738/2894/2062 stripped for TBC; Vanilla doctor false-positive defect closed. **Runtime three-era doctor pass rides Task 8's rebuild** |
| **Plan 3b Task 7** (Windfury Weapon) | **COMPLETE** (`a6d2b58`) — measured GRANT-STOCK r1-r4 (A.6's "one genuine leak" closed as no-leak); one clone 947441 at r5 (cost model), riding the stock enchant/33757/core-script mechanism; e2 AP slot carried at live's value per B.6 |
| **Plan 3b Task 8** (rebuild + headless sweep + record) | **COMPLETE** (2026-09-02) — sweep all-green; three-era doctor at L48/60/70/80 ZERO orphans, exactly one marker each; 13 CLASS_SHAMAN `kDiscriminators` rows added and they immediately caught + fixed the factory stock-totem re-learn (totems `_ref` OQ id 18); live totem-drop proof incl. Enhancing SoE +15% firing (86→98). Record: `docs/verification/era-talents-tbc-phase3-shaman.md` |
| **Tooltip-honesty fix round** (user-directed, 2026-09-02) | **COMPLETE** — generation **`94fde52c`**. Closed: FT imbue proc restored both eras (enchant re-point + 947135-39 + era script + vehicle 947442), NG real heal (era script + 947443), Clearcasting damage-only (947444/947445), Stoneclaw taunt ex3 (947062-67, both eras), SR stun bit, totem silence gating (`preventionType` key, both eras), Sentry linger (core patch **0024**), Vanilla yards prose + GoA/WW icons + FT school. Record §10; `_ref` receipts at every origin. Vanilla changes ship in the same prod push as TBC. Generator hardened: band-id scriptBindings now bare-DELETE (rebind convergence). Post-round real-client finds folded in: Elemental Focus client tooltip (947444 description, gen ea7f316f) and the Sentry double-trainer-row (Vanilla clone 931390 + stock re-gate, gen **19e3a621** — the CURRENT stamp) |
| **Plan 3b Task 9** (real-client pass, user-run) | **COMPLETE — SIGNED OFF 2026-09-02**, gen `19e3a621`. Two in-pass finds fixed: Elemental Focus client tooltip (947444 description) + Sentry double trainer row (Vanilla clone 931390, stock 6495 re-gated). Record §11 |
| Nodes wired | **61 of 61** |
| Audit / tests | `era_audit.py` 0 findings (bare + targeted); `pytest tools/` 199 passed |
| Helper high-water | 947351 of 947439 + marker 947440 + Windfury Weapon r5 clone 947441 (947442–947511 free); creatures 920580 of 920999 |
| DUMMY-marker misc | next free **19** |

## What is next — PHASE 4: WARRIOR

Start from `docs/era-talents-tbc-workflow.md` §3 (per-class phase process) + §4b (lessons — the
shaman fix-round classes of finding are now standing sweep items). Helper slice: next free TBC
helper id **947512** per the workflow doc. Honor this doc's Working rules from day one.

## Standing facts a resumer must know

- **`modules/mod-era-talents/src/EraTalentIP.cpp` is DELIBERATELY dirty** (uncommitted):
  `EraHasTalentTrees` → `return era == ERA_VANILLA || era == ERA_TBC;`. This is the TBC ship gate —
  it stays unstaged until all nine TBC classes are authored and verified. Verify it is still
  unstaged at every task close.
- **The headless pass covers structure and the bot era path only.** The player era path needs the
  real client (`.ip set` is unusable headlessly — Amendment A.11); headless era-setting uses a
  playerbot's level band (`.character level <Bot> 70`).
- **`kAuthoredOrders` is keyed (class, specTab) with no era dimension** — TBC bot builds log
  `authored build order skipped` and greedy-fill. Known cross-class gap; the warnings name the
  *Vanilla* ids, so grepping for `202xx` looks like a pass and is not.
- **Verify generation by extracting Spell.dbc from the MPQ, never by build stdout.**
  `gen_era_talents.py` is hashed into the stamp, so a generator-only change reships the MPQ.
- **Sweep methodology** (earned during 3a/3b, keep for the six remaining TBC classes): a survival
  sweep must cover all five field families — effects, non-effect fields
  (Proc*/Duration/ManaCost), triggered chains, attributes (all eight words), class-mask word 0,
  SpellPower/mana. When a sweep says "everything matches", suspect its column list. Read the prior
  era's `_ref` before sweeping. Never write "not knowable" about a value that is in the wago CSVs.
- **`acore_world.spell_dbc` is the 6.8k-row custom-override table** — test stock-spell presence
  against `azerothcore-wotlk/ip-dbc/Spell.dbc` (50 011 rows), never against the DB table.

## Still-open VANILLA defects — mirrored so they survive this phase

Shipped, merged, real-client-verified Vanilla content. **Do not fix from inside the TBC phase**;
each needs its own regression pass and a fresh MPQ. Canonical home:
`era-data/_ref/tbc/shaman-totems-tbc.yaml` `open_questions:` (ids cited).

1. **Vanilla Earthbind snares for ~5 s of its 45 s** (id 5) — creature 920100 casts the slow field
   directly, no periodic driver. Fix mirrors TBC's 947001.
2. **Vanilla Magma lands one tick of ten** (id 8) — same defect class; creatures 920160-920163 cast
   the damage clone directly. Fix together with 1 (the OQ-9 sweep proved the class has exactly
   these two members).
3. **Clones 931148-151 carry SchoolMask 32** (shadow) where both eras are 4 (fire) (id 1).
4. **Vanilla clones miss `SpellInfoCorrections` by-id fixes** (id 11) — enforced by
   `era_audit.py::check_spellfix_inheritance`, acknowledged per-clone via `spellFixAck:`. Functional
   ones: Flametongue 931148-151 lose IGNORE_LINE_OF_SIGHT; Stormstrike 932405 loses
   DOT_STACKING_RULE; paladin 932733-736/932743-745 lose SUPPRESS_CASTER_PROCS on Judgement.
5. **Vanilla totem tooltips say "20 yards" against radius index 10 = 30 yd** (id 6) — cosmetic.
6. **Vanilla Grace of Air / Windwall clones carry wrong client icons** (id 13) — 691/1397 instead
   of the measured 337/174. Cosmetic, needs a fresh MPQ.
7. **`932802` Improved Hamstring may carry an unintended AoE root** (targetA/B = 6/15) — spotted in
   passing, never verified.

## Deferred, filed, non-blocking

- Provenance headers for the three TBC skeleton YAMLs.
- `mod-individual-progression`'s `.ip set` discards its player argument (Amendment A.11).
- Inline `_ref` drift across the TBC datasets (druid 6 sections, paladin 1, shaman 2). NB the
  inline block uniquely carries the `spells:` identity table the generator resolves `affects:`
  through — a `ref:` pointer breaks generation.
- An `era_audit` check for `SpellInfoCorrections` inheritance exists; the audit is now the
  enforcement point (nothing further owed this phase).
