# Era-Talents — Paladin "Judgement" trainer wrapper leak (spell 10321)

**Date:** 2026-08-25 · **Branch:** `feat/era-talents` · **Spec:**
`docs/superpowers/specs/2026-08-25-paladin-judgement-wrapper-leak-design.md`
No YAML/generator change — NO regen, NO generation-stamp bump, addon untouched.

## What was broken

A Vanilla-era paladin saw "Judgement" at every paladin trainer; buying it granted WotLK
Judgement of Light (20271) AND WotLK Seal of Righteousness (21084). JoL was stripped at next
login (rebuyable forever); SoR persisted (21084 was in no module list — kStockPalSwap carried
20154, the old creation-granted id).

## Root cause

Stock base data ships ungated `trainer_spell` rows for spell **10321** on TrainerIds 3/4/5/6.
10321 is an Effect-36 LEARN-WRAPPER named "Judgement" teaching 20271 + 21084. The 2026-08-20
rework searched trainer_spell for the TAUGHT ids (found nothing) and excluded TrainerId 6
(actually the starter-zone trainer, creatures 925/926). The legacy `npc_trainer` table also
holds judgement/seal rows (templates 200003/200004) but is loaded NOWHERE in this fork — dead
data, a red herring for id-based searches. A full Spell.dbc sweep of every trainer_spell
SpellId confirmed 10321 is the ONLY class-spell learn-wrapper in the database (all others are
profession/riding wrappers) — this was a paladin-only exposure.

## Fix (commits 2ef399c + 1fc62b7)

- `2026_08_25_00_era_talent_paladin_judgement_wrapper.sql`: delete the 10321 rows
  (TrainerIds 3/4/5/6); mirror the 932749-marker-gated custom seal/Judgement/LoH set + the
  SoC R2-5 chain onto TrainerId 6 (starter-zone parity, user-approved — 30 gated rows).
- `EraTalents.cpp`: `kStockPalSwap` += 21084 (Vanilla login self-heal);
  `kWotlkPalStock` += {4, 21084} (WotLK-stage paladins keep the real WotLK SoR — the wrapper
  was its only source on this core; creation grants only old-id 20154).
- `era_audit.py` `check_trainer_wrapper_leaks`: rebuilds the effective trainer_spell table
  (base dump − module DELETEs, incl. SpellId-only trainer-wide deletes, + module INSERTs) and
  fails on any UNGATED row teaching — directly or via Effect-36 — a stock id the module
  strips; fails loud on any trainer_spell DELETE it cannot decompose. Known limitation
  (documented in the check header): `UPDATE ... SET ReqAbility1` gates are not replayed.

## Verified (dev box, 2026-08-25)

- era_audit red→green across the fix: 8 findings (TrainerIds 3/4/5/6 × {20271, 21084} via
  wrapper 10321) with the SQL absent; 0 findings with it present. Harvest canary: 240
  deleted ids collected, zero parse findings.
- Two-stage subagent review on both commits (spec compliance + code quality), including a
  review round that hardened the sweep's DELETE harvester (SpellId-only deletes were being
  skipped — 16 stripped ids were invisible to the invariant).
- db-import applied the SQL: `trainer_spell` rows for 10321 = 0; TrainerId-6 custom rows = 30;
  updates-table row present.
- Worldserver rebuilt from committed HEAD (module synced via `git archive`, deliberately
  excluding a concurrent session's uncommitted edits) — binary canary confirms the new
  kStockPalSwap dword sequence (20154,21084) in the running binary; World initialized in 23s,
  module loaded 432 nodes, no errors.
- Saved character state: Paladintest (guid 1314) holds none of 20271/21084/20154.

### Pending (real-client, user)

- Vanilla paladin: no "Judgement" at any paladin trainer; custom SoR R1/Judgement trainable
  at a starter-zone trainer; if 20271/21084 were re-acquired pre-fix, both gone after relog.
- WotLK-stage check (`.ip set`): paladin auto-granted 20271 + 21084 + 20154 at levels 4/4/3.
- Framework-doc gotcha pointer (trainer rows can be Effect-36 learn-wrappers) DEFERRED — the
  concurrent class-review session holds `docs/era-talents-framework.md` dirty; add after it
  lands.
