# RaidRoster — Era-Gated Death Knights + Substitute Slots — Verification Record

**Date:** 2026-09-07 · **Branch:** `feat/era-talents` at `4f47974` (merged to master 2026-09-07) ·
**Spec:** `docs/superpowers/specs/2026-09-07-raidroster-era-dk-substitutes-design.md` ·
**Plan:** `docs/superpowers/plans/2026-09-07-raidroster-era-dk-substitutes.md` (Task 6 = this checklist)

## Headless / build gates (recorded during implementation)
- Dev-box build of the 44-slot comp (`RaidRosterComp.h` `static_assert`s hold 4/9/27 in both bands).
- `mod_raid_roster.band` ALTER applied; information_schema-guarded so a hash re-apply of the `base/`
  file is a no-op (the unguarded form failed live on the dev box when the file's comment changed —
  see CLAUDE.md, raid-roster section).
- Code review approved; fix round (idempotent SQL, formatted `kWotlkBandMinLevel` login suffix) landed
  in `ca15fd3`.

## Real-client pass (owner, dev box, 2026-09-07)
The owner ran the plan's Task 6 checklist in a real 3.3.5a client and reported every check complete
("all checks are done"). Per-step observed values were not transcribed into this record; the steps
exercised were:

| # | Step | Result |
|---|------|--------|
| 1 | Top-up an existing 40-row roster (`.raidroster status` warning → `.raidroster create` → 44 rows, 4 `band=1` DKs, 4 `band=2` substitutes) | owner-confirmed |
| 2 | Master below 71 → `.raidroster login 40` benches the 4 DKs and fields the 4 substitutes | owner-confirmed |
| 3 | `.raidroster sync` → substitutes receive era builds in the forced tab (`.eratalents doctor`) | owner-confirmed |
| 4 | Master at 80 → `.raidroster login 40` returns the DKs and dismisses the substitutes | owner-confirmed |
| 5 | Small sizes below 71 (`login 5` → 4 bots, `login 10` → 9 bots, no DK, benched suffix) | owner-confirmed |
| 6 | Fresh roster on a second character: `create` → 44 slots; second `create` → "Roster is complete" | owner-confirmed |

## Follow-ups
- Production rosters need ONE `.raidroster create` after the deploy to gain the substitute slots
  (the create is an idempotent top-up).
