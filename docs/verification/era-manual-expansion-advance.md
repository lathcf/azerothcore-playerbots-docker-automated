# Verification — Era Talents manual expansion advance (patch 0027 + EraAdvanceGossip)

**Date:** 2026-09-07 · **Branch:** `feat/era-talents` at `e175a5b` · **Dev-box image:**
`acore/ac-wotlk-worldserver:master` sha256 `8999e21e…` built 2026-09-07T16:12Z · startup canary
`[mod-era-talents] startup (enable=true)` · live confs `IndividualProgression.ManualAdvanceStates = "8 13"`,
`EraTalents.AdvanceGossip = 1`. Test character: **Eratester** (Alliance).
Spec: `docs/superpowers/specs/2026-09-07-era-manual-expansion-advance-design.md`.

| # | Spec test | Result | Evidence |
|---|-----------|--------|----------|
| 1 | Stage 7: Anduin offers the option, Thrall does not; Cancel = no change; Accept → 8 + TBC transition | PASS (owner, real client) | `[EraAdvance] Eratester 7->8 via Anduin Wrynn (player-confirmed expansion advance)` |
| 2 | Stage 12 without achievement 698: no option; with it: option + WotLK text; Accept → 13 | PASS (owner) | `[EraAdvance] Eratester 12->13 via Anduin Wrynn (player-confirmed expansion advance)` |
| 3 | Hold: stage 12 + achievement, relog → achievement backfill clamped, state stays 12 | PASS (owner) | `[IP] manual-advance hold: Eratester auto-advance to 13 clamped to 12 (advance via the faction leader)` ×2 |
| 4 | Stage 7, "Into the Breach" 10259 turned in → stays 7 | PASS (owner-reported) | no `clamped to 7` line in the current container log — owner confirmed the state stayed 7; the log line was not captured by the orchestrator |
| 5 | `.ip set 13` (Force path) still advances | PASS (owner-reported) | GM path is untouched by 0027 (Force never routes through `ClampHeldAdvance`) |
| 6 | Fresh Death Knight lands at stage 13, no hold line | PASS (owner-reported) | no `manual-advance hold` line for any name other than Eratester in the log |
| 7 | Build + ordered patch check | PASS | build exit 0 / 0 errors; every patch `applied:`/`appliable:` except the pre-existing 0003/0014 per-patch reverse-check artifact on shared files (hunks present, compiled) |

Owner sign-off: "everything looks good" (2026-09-07). Trap recorded: exposing `EraTalentBots::IsBot` in
the header while its definition sat in an anonymous namespace produced a link error (fixed `e175a5b`).
