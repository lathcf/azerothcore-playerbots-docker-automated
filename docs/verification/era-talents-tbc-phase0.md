# Era Talents — TBC Phase 0 (Enablement) Verification Record

**Date:** 2026-08-27
**Branch:** `feat/era-talents` (merged to master 2026-09-07 — all nine TBC classes shipped)
**Final generation stamp:** `c192d2f1` (stub-free, 9 Vanilla datasets)
**Plan:** `docs/superpowers/plans/2026-08-27-era-talents-tbc-phase0-enablement.md`
**Spec:** `docs/superpowers/specs/2026-08-27-era-talents-tbc-phase0-enablement-design.md`

Executed via subagent-driven development (implementer + spec + code-quality review per task).
Tasks 1–8 (code/docs) landed and reviewed; Tasks 9–10 (runtime) verified live on the dev-box
Docker stack with a real WoW client (character `Magetest`, mage, level 75).

## Definition-of-Done results

| # | DoD item | Result | Evidence |
|---|---|---|---|
| 1 | Regen produces distinct, era-correct SQL for `vanilla/mage.yaml` and the same-basename `tbc/mage.yaml` stub (keying fix verified) | ✅ | Stub proved the keying fix by construction; `20800` present only in the TBC SQL, absent from vanilla mage SQL; `test_gen_era_talents.py` green |
| 2 | Nine Vanilla datasets regenerate byte-identical modulo the stamp | ✅ | `md5sum -c /tmp/claude-vanilla-sql-baseline.md5`: 39/40 OK, only `era_talent_meta.sql` (stamp) differs — held across BOTH the stub-add (Task 6) and stub-delete (Task 10) regens |
| 3 | `era_audit` clean incl. the new cross-dataset collision check, with a deliberate collision proven to fail | ✅ | `era_audit.py` exit 0; deliberate stub node `20800→18001` triggered BOTH the cross-dataset collision finding AND the generator's per-era node-window lint error, then reverted |
| 4 | Addon renders the 9-tier stub tree with a reachable tier-9 node + verified tab background | ✅ | Real-client (user-confirmed): 3 stub tabs render; "Stub Ladder" scrolls to the tier-9 Stub Capstone, clickable after 40 pts; addon height derivation 455 (Vanilla)/581 (9-tier). Tab backgrounds = known-good mage tokens |
| 5 | Both Vanilla↔TBC transition directions on a dev char (points reset, pin correct, no orphan era spells, doctor clean) | ✅ | `Magetest`: Vanilla→TBC (era+storedEra→TBC); TBC→Vanilla left `era_character_talent eraId=1` = 0 rows AND `character_spell` in `[936000,943999)` = **0 orphans**, spent=0, doctor clean |
| 6 | Per-era point cap (Vanilla 51 / TBC 61) enforced + observed | ✅ | `.eratalents doctor Magetest` at level 75: `available=51` (Vanilla), `available=61` (TBC) — cap applied both ways (66 raw → capped) |
| 7 | `import_tbc_talents.py` merged with tests; `tbc-paladin.skeleton.yaml` generated with per-rank spell ids + names/icons from cached tooltips | ✅ | 5 importer tests green; `--fetch` produced `era-data/_skeletons/tbc-paladin.skeleton.yaml` — 64 real nodes (Holy/Protection/Retribution, ids 20000–20063, 0 placeholders, real icons + per-rank tooltips), 197 tooltip JSONs cached under `era-data/_ref/tbc/tooltips/` |
| 8 | Reconcile/marker decision-table doc committed with every row tagged | ✅ | `docs/era-talents-tbc-reconcile-triage.md` — 26 rows, every row tagged, zero TBD, all six pre-decided rows code-verified; framework id-registry updated (band → 950000, TBC windows/slices) |
| 9 | Stub dataset deleted; tree state ready for the Phase-1 (paladin) brainstorm | ✅ | Repo stub deleted + wiring reversed (grep-zero, exact line-count mirror of the add commit); dev DB purged; server reloads **432 talent nodes** |

## Runtime session log (dev box, Docker)

- **Build (with temporary TBC flip):** worldserver booted clean in 18s, `loaded 443 talent nodes (generation 8804833c)` (432 Vanilla + 11 stub). Only benign "Missing property EraTalents.GlyphGate/…" config-default warnings.
- **DB structural check:** `era_talent` eraId 0 = 432, eraId 1 = 11 (ladder 20800–20807 @ rank 5, capstone 20808 tier 8 → prereq 20807 @ 40 pts, tabs 2/3 nodes 20809/20810); 51 TBC-band passives `942400–942484`; meta stamp `8804833c`.
- **Spent-state check:** after the user spent the full ladder + capstone, `era_character_talent` = 9 nodes / 41 ranks (20800–20807 @ 5, 20808 @ 1); `character_spell` band `[936000,943999)` = 9; `available=20 spent=41`.
- **Reverse transition:** `.ip set Magetest 1` (Vanilla) → era rows and band spells both stripped to 0, `available=51 spent=0`, doctor clean.
- **Revert + rebuild (committed code, flip reverted, stub deleted):** worldserver rebuilt, `loaded 443` initially (stale incremental-DB stub rows — see gotcha), then **`loaded 432 talent nodes (generation c192d2f1)`** after purging the stale rows.

## Notable findings / gotchas (for future eras + Phase 1)

1. **`.ip set <char> 0` is a no-op for the era mapping** — stage 0 reads as "unset". Use stage ≥ 1 for Vanilla (`.ip set <char> 8` for TBC). Cost the reverse-transition test one iteration.
2. **Deleting a dataset's SQL file does NOT un-apply already-imported rows** on an incremental dev DB — the per-dataset `DELETE FROM era_talent WHERE …` lives *inside* the now-deleted SQL. A **fresh** import yields 432; this dev DB retained 11 stub `era_talent` + 51 `era_talent_rank` + 51 `spell_dbc` (936000–943999) rows until manually purged (`DELETE … WHERE eraId=1 / talentId 20800–20810 / ID 936000–943999`). Same class as the known "stale orphan spell_dbc rows on the incremental dev DB only". Relevant when a class phase renames/removes nodes.
3. **The temporary `EraHasTalentTrees` TBC flip was reverted** (committed code returns `era == ERA_VANILLA`). TBC stays gated native — deployment enablement is the single final-enablement task after all nine TBC classes are authored + verified (per the reconcile-triage doc).
4. **CLAUDE.md's `[920000, 933000)` band mention is intentionally NOT changed** — it describes `master` (Vanilla-only), where the band is still `[920000, 933000)`. The `→ 950000` widening lives on `feat/era-talents` and becomes true for `master` only at merge (end of the TBC buildout). The framework doc (branch state) was updated in Task 8.

## State at close

- Repo: `feat/era-talents` @ close-out commit; final stamp `c192d2f1`; stub gone; paladin skeleton seeded at `era-data/_skeletons/tbc-paladin.skeleton.yaml`; flip reverted.
- Dev server: rebuilt from committed code, 432 nodes, stub purged. (Left running — `docker compose stop` in `azerothcore-wotlk/` when done.)
- **Next:** Phase 1 = **paladin** content brainstorm, seeded from the fetched skeleton (64 nodes) + `docs/era-talents-tbc-workflow.md` §4 per-class workflow.
