# Era-Specific Talents — Verification Record (Vanilla Mage slice)

> **Generation superseded (2026-09-06 final review):** the current stamp is the closing row of docs/verification/era-talents-final-review-tbc-vanilla.md §10 — reinstall patch-V.mpq + restage the addon before any real-client pass.

**Date:** 2026-08-12
**Branch:** `feat/era-talents`
**Environment:** dev box (disposable local stack, project `azerothcore-wotlk`, realm data separate from production).
**Spec/plan:** `docs/superpowers/specs/2026-08-12-era-specific-talents-design.md`, `docs/superpowers/plans/2026-08-12-era-specific-talents.md`.

## Host-side tests (no server)

- **Generator unit tests:** `tools/test_gen_era_talents.py` — 5 passed (SQL emission, addon-Lua schema, lint: prereqs / grid collision / rank contiguity / tab-key consistency). Run: `uv run --with pytest --with pyyaml -- python3 -m pytest tools/test_gen_era_talents.py -q`.
- **Addon Lua parse:** every `client-addons-src/EraTalents/*.lua` + `data/generated/MageVanilla.lua` pass `luac5.1 -p`.
- **Comms headless test:** `lua5.1 client-addons-src/EraTalents/test_comms.lua` → `comms OK` (ParseSync; confirms no file-scope WoW API).

## Dev-box build + boot (Task 15 headless)

Build (surgical, same project): synced `modules/mod-era-talents` → `azerothcore-wotlk/modules/`, then `docker compose build ac-worldserver ac-db-import`.

- **Compiles + links — PASS.** cmake picked up the module (`Modules configuration (static): ... mod-era-talents`), resolved the IP include path (`[mod-era-talents] mod-individual-progression headers on include path`), and all 8 `.cpp` compiled with no errors; both images built (exit 0). This was the primary risk (no compiler in the authoring session).
- **DB import — PASS.** `ac-db-import` applied `2026_08_12_00_era_talent_schema.sql`, `2026_08_12_10_era_talent_data.sql` (world) and `2026_08_12_00_era_character_talent.sql` (characters); exit 0. Tables + rows confirmed:
  - `era_talent`: `18001` (Fire, 5 ranks), `18002` (Fire, 2 ranks, display-only).
  - `era_talent_rank`: `18001` → `11069,12338,12339,12340,12341` (after the fix below).
  - `era_character_talent`, `era_talent_char_state` created.
- **Boot + content load — PASS.** Worldserver logs `[mod-era-talents] startup (enable=true)` and `[mod-era-talents] loaded 2 talent nodes`; `World Initialized In 0 Minutes 16 Seconds`.
- **Stability — PASS.** Server survived a ~75s post-init window still `running` (guards against the segfault-after-boot pattern from prior module work; our hooks bail on the enable flag before touching arguments).

## Bug found + fixed by the headless spell-ID check

The plan flagged "verify the stock passive IDs before trusting them." The live spell store (`lookup spell improved fireball` via `tools/wgconsole.py`) returned:

```
11069 - Improved Fireball, rank 1 [talent] [passive]
12338 - Improved Fireball, rank 2 [talent] [passive]
12339 - Improved Fireball, rank 3 [talent] [passive]
12340 - Improved Fireball, rank 4 [talent] [passive]
12341 - Improved Fireball, rank 5 [talent] [passive]
```

WotLK Improved Fireball is a **5-rank** passive (−0.1s each, 0.5s at 5/5), not the 3-rank chain the dataset first assumed. The binding was corrected to a clean **1:1** map (`1→11069 … 5→12341`) — data-only fix, re-imported and content-reloaded on the dev box. Commit `f4355a3`.

## Behavioral verification — engine proven headless via `.eratalents`

A `.eratalents` admin command (GM + console; `status`/`learn`/`reset` by name) was added to drive the server-authoritative engine directly, independent of the client addon. Run against a Vanilla-era Mage bot (`Zarice`, guid 228, level 40) via `tools/wgconsole.py`:

- `eratalents status Zarice` → `era=Vanilla available=31 spent=0` (era detection + point pool correct: 40−9=31).
- `eratalents learn Zarice 18001` ×5 → accepted, "now rank 1..5".
- `eratalents status Zarice` → `available=26 spent=5`, `node 18001 rank 5` (**point accounting correct**).
- `character_spell` after `saveall`: `11069/12338/12339/12340 → specMask=0` (deactivated), `12341 → specMask=1` (**active**). Strip-then-learn leaves ONLY the rank-5 passive active — **no modifier over-stacking**.
- `era_character_talent` = `(228,0,18001,5)`; `era_talent_char_state` = `(228, lastEra=0)` (**stamp persisted — the reviewed reconcile fix**).
- `eratalents reset Zarice` → `available=31 spent=0`, `era_character_talent` rows deleted, `12341 → specMask=0` (**deactivated on reset**).

This proves the core engine end-to-end on a live server: validation, correct per-rank stock-passive grant, strip-on-rank-up, point accounting, persistence, and reset — everything except the addon UI and the cast-time measurement.

## Still requiring the real client (Task 16)

The engine is proven above; what still needs the client is the **UI + client-side path**: the addon panel rendering (custom era frame replacing the Blizzard frame), the addon-sent `LEARN`/`HELLO`/`SYNC` round-trip (the C++ comms handler, distinct from the engine it calls), the point-pin *visible* in the stock frame, and the actual **Fireball cast-time drop** (stock WotLK behavior once `12341` is active — not directly measured headless). These need a real 3.3.5a client and a casting session.

### Task 16 checklist (real 3.3.5a client)

1. Copy `client-addons/EraTalents` into the client's `Interface/AddOns/`.
2. On a **Vanilla-era Mage** (`.ip set <char> 0`): open Talents (micro-button AND `N`) → the custom 3-tab era panel appears (not the Blizzard frame); the stock frame's points are pinned to 0.
3. Learn **Improved Fireball** in-panel → the rank pip advances on the server's `SYNC`; `SELECT * FROM era_character_talent WHERE talentId=18001` shows the rank; the char knows the matching `11069…12341` passive; **stock Fireball cast time is measurably reduced** vs before.
4. `/reload` → state re-pulls via `HELLO`.
5. `.ip set <char> 13` (WotLK) in-session → panel goes inert, stock frame returns with a full 71-pt pool, `era_character_talent` rows for the Vanilla era gone, `era_talent_char_state.lastEra = 2`.
6. On a **WotLK-era Mage**: the stock Blizzard frame is untouched and talents are NOT reset on login (regression check for the reviewed login-reset bug).

## Real 3.3.5a client (Task 16) — PASS (2026-08-13)

Verified on a live client with the `EraTalents` addon installed, on Mage `Magetest`:
- Custom **Vanilla era panel renders** and replaces the Blizzard frame; WotLK-band char keeps the stock frame.
- **Learning Improved Fireball reduces the stock WotLK Fireball cast time and updates the tooltip** — the DoD money shot, no custom spell.
- **Level-up point count updates live** (after the same-era-level-up `SYNC` fix; a stale count was the first bug found and fixed).
- **Mid-session era transition works both directions with no relog:** `.ip set` Vanilla↔WotLK swaps the panel/frame live within ~2s and grants the correct WotLK pool (after the `InitTalentForLevel` fix; the panel-swap needed the periodic reconcile + transition `SYNC`, and the WotLK point grant needed `InitTalentForLevel`).

Three real bugs were surfaced by the client test and fixed in-session (commits on `feat/era-talents`): stale point count on same-era level-up, panel not swapping mid-session, and 0 WotLK points on the up-transition until relog.

## Status

**Vanilla Mage slice — COMPLETE & fully verified** across host tests, dev-box build/boot, the headless engine, and the real client. The machinery is proven; remaining work is content (full Mage tree, TBC, then all classes) — see the plan's NEXT PHASE section. Branch `feat/era-talents`, merged to master 2026-08-25.

---

## Phase 9 re-audit (2026-09-05) — 14 FIX rows shipped, Magic Absorption rider CLOSED

The full 49-node Vanilla mage tree was re-audited under every standing check class as the scope of
**TBC Phase 9 (mage)** — the first class ever authored, re-graded against the fourteen-family
survival map and the live `spell_proc` / `script_census` dumps. Verdict: **35 CLEAN / 14 FIX**, and
**all fourteen shipped in generation `1de3388d`**, riding the same regen as the TBC mage tree (one
`patch-V.mpq` reship, already implied by that phase).

**The fourteen rows:** 18005 Magic Absorption, 18008 Improved Arcane Explosion, 18011 Improved
Counterspell, 18022 Incinerate, 18023 Improved Flamestrike, 18024 Pyroblast, 18026 Improved Scorch,
18029 Critical Mass, 18030 Blast Wave, 18032 Combustion, 18036 Ice Shards, 18039 Perma Frost →
Permafrost, 18048 Winter's Chill, 18049 Ice Barrier.

**Headline defect:** Vanilla mage was the ONLY dataset in the repo using `op: crit, kind: pct`, and
a PCT crit spellmod is effectively inert (`Player::ApplySpellMod` skips PCT when the base value is
0) — **four of the fourteen rows (18008 / 18022 / 18023 / 18029) are that one defect**, plus 18036
which is the mirror (`critDamage` needed `pct`, not `flat`).

- **Magic Absorption's mana-on-full-resist rider is CLOSED.** It had never been wired: node 18005
  now grants a Vanilla clone chain **932326-932330** with per-rank POSITIVE
  `spell_mage_magic_absorption` bindings and the row's own `spell_proc` contract (`typeMask 7`,
  `hitMask 8` FULL_RESIST, `cooldown 1000`). The node's former `mechanic: stat` auto-passives
  **920040-920044** are consequently stranded in no era's rank table and are stripped by Phase 9.5's
  generic band-orphan sweep (`EraBandClassifier::Sweep`, driven by the `stranded:` block of
  `era-data/band-allowlist.yaml`) plus a `DELETE FROM spell_dbc` — both verified live. (There is no
  per-class `CLASS_MAGE` strip arm for them; every such arm was deleted in Phase 9.5.)
- **Ice Barrier is base-only again** (node 18049 → clone chain **932331-932334**, r2-r4
  trainer-taught at L46/52/58). Live's `spell_mage_ice_barrier` adds 80.68% of spell power to the
  stock chain; 1.12 had no absorb scaling at all (cmangos `mangos-classic` has no
  `spell_bonus_data` subsystem, `SPELL_AURA_SCHOOL_ABSORB` maps to the empty
  `HandleNoImmediateEffect`, and `CalculateDamageAbsorbAndResist` reads the stored base amount).
  The clones deliberately carry **no** script binding — a clone outside the `-11426` chain is never
  touched by that script, which IS the 1.12 behaviour.
- Three further whole clone chains ship for the same reason (an era-diverged field with no data
  expression on the stock spell): **932310-932317** Pyroblast (6 s cast), **932320-932324** Blast
  Wave (no WotLK knockback), **932325** Combustion (3-min cooldown, bare DUMMY effect 1).
- Payload clones **932762** (Improved Counterspell silence, 4 s), **932763** (Fire Vulnerability,
  +3% ×5), **932764** (Winter's Chill, +2% Frost crit taken ×5, `singleAuraStack`).

**Real-client L60 re-check SIGNED OFF 2026-09-05** (zero findings, tbc-phase9-mage §14c) — the item list is
`docs/verification/era-talents-tbc-phase9-mage.md` **§14c** (items 13-23), one line per FIX row. The
Vanilla mage slice's earlier sign-off (2026-08-13, and the Vanilla merge of 2026-08-25) stands for
everything these fourteen rows do not touch.
