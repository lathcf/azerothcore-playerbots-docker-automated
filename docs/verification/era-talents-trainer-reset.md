# Era-Talents — Trainer-driven talent reset (dev Reset button removed)

**Date:** 2026-08-25 · **Branch:** `feat/era-talents` · **Spec:**
`docs/superpowers/specs/2026-08-25-era-talents-trainer-reset-design.md`
No YAML/generator change — NO regen, generation stays 8a350a1a, no patch-V.mpq work.

## What changed (commits 512b6e2, 90d842f, 5c052fd)

- `EraTalentPin.cpp`: new `OnPlayerTalentsReset` override — on the trainer's talent-wipe
  confirm, a managed-era character is charged `resetTalentsCost()` (exactly what the confirm
  dialog displayed; honors CONFIG_NO_RESET_TALENT_COST; refuses with the standard
  not-enough-money buy error), era talents reset (`EraTalents::Reset`), untalent visual 14867
  cast, addon resynced. `noCost` callers (GM `.reset talents`, AT_LOGIN_RESET_TALENTS) reset
  free. EraTransition's internal `resetTalents(true)` re-entry is a no-op by existing guards
  (SpentPoints==0 entering a managed era / EraHasTalentTrees bails leaving one) —
  review-traced through both crossing paths. Known limitation (pre-existing, documented in
  the spec + tracked separately): a WotLK dual-spec char down-transitioned to Vanilla keeps
  stock talents in the inactive spec, which can double-charge a later trainer reset; root fix
  belongs in `EraTransition::Run`.
- `EraTalentsComms.cpp` (+ `.h` verb-list comment): `RESET` addon verb deleted — a spoofed
  `ERATAL\tRESET` falls to the else-return and is fully ignored (no reset, no SYNC reply).
  Reset paths are now: trainer (paid) and GM `.eratalents reset`.
- Addon: dev Reset button + `ET.ResetTalents` removed; Pet Talents button takes the vacated
  top-right slot (geometry checked against the close button — no overlap); headless
  regression asserts added to `test_comms.lua` (`ET.ResetTalents == nil`, `ET.Learn` intact).

Each task passed a two-stage subagent review (spec compliance + code quality); all approved.

## Verified (dev box, 2026-08-25)

- `lua5.1 client-addons-src/EraTalents/test_comms.lua` red→green across the removal
  (red: `ET.ResetTalents must be removed`; green: `comms OK / GEN canary OK /
  MISSING sentinel OK / test_comms OK`, exit 0). Both lua files parse under `luac5.1 -p`.
- `python3 tools/era_audit.py` — 0 findings.
- Module synced into `azerothcore-wotlk/modules/mod-era-talents` before the build; grep
  canaries on the synced copy: `OnPlayerTalentsReset` = 1, `RESET` in EraTalentsComms.cpp = 0.
- Worldserver image rebuilt (compose exit 0, container created 2026-08-26T00:49Z), booted
  clean: `World Initialized In 0 Minutes 15 Seconds`, `[mod-era-talents] startup
  (enable=true)`, `loaded 432 talent nodes (generation 8a350a1a)` — generation unchanged.
- `./fetch-client-addons.sh` restaged; staged copy carries the removal
  (`client-addons/EraTalents/Comms.lua` has no ResetTalents; TalentUI.lua has no `f.reset`,
  petTalents anchored TOPRIGHT −34,−44).

## Pending (real-client, user)

- Copy the restaged `client-addons/EraTalents/` addon to the client's Interface/AddOns.
- Vanilla-era char with spent points: trainer "I wish to unlearn my talents" → confirm →
  charged the displayed cost, panel zeroes live, era passives gone, untalent visual fires.
- Same char broke (< 1g): standard "not enough money" error; talents intact, no charge.
- WotLK-stage char: trainer reset behaves 100% stock.
- `.ip set` era transition down/up: still clean, no double reset.
- Spoof macro `/run SendChatMessage("ERATAL\tRESET", "WHISPER", nil, UnitName("player"))`:
  nothing happens.
- Talent panel: no Reset button; hunter's Pet Talents button sits top-right; no Lua errors.
