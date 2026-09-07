# Era-Talents — TBC/WotLK talent-frame fallback (implemented-era allowlist)

**Date:** 2026-08-25 · **Branch:** `feat/era-talents` (merged to master 2026-09-07) · **Scope:** mod-era-talents gating only
(no YAML/generator/spell content — NO regen, NO generation-stamp bump).

## Problem

`EraFromIP()` classifies every Individual-Progression character as `ERA_VANILLA(0)`, `ERA_TBC(1)`, or
`ERA_WOTLK(2)`. Only **Vanilla** has authored custom talent trees. But the module used
`era != ERA_WOTLK` everywhere as shorthand for "this is a **managed** era character" (custom talent
window replaces the native frame; stock talents wiped; talent points pinned to 0). That predicate
wrongly swept in **TBC**, which has no tree. A TBC-era character therefore hit, at three layers:

| Layer | Site | Effect on a TBC character (before fix) |
|---|---|---|
| Server – transition | `EraTransition::Run` down-transition `else` | `resetTalents(true)` **wiped stock WotLK talents** |
| Server – pin | `Pin()` + `pin()` + `OnPlayerFreeTalentPointsChanged` | free talent points **pinned to 0** |
| Client – frame | `Comms.lua` `isEraChar = (era ~= 2)` → `Hook.lua` | native WotLK frame hidden, **empty custom window** |

Net: a TBC character had no usable talents at all. (Baseline-spell reconciliation was already correct
for TBC — `era >= ERA_TBC` grants TBC-baseline spells, `era == ERA_VANILLA` strip-gates leave a TBC
spellbook alone — so that path was NOT part of the bug and was left untouched.)

No real player was affected (the IP era feature is not yet in production), so this is preventive —
required before the Vanilla-era production push.

## Fix — explicit implemented-era allowlist (user-chosen, fail-safe)

One predicate is the single source of truth, replacing every "managed era" gate:

```cpp
// EraTalentIP.{h,cpp}
bool EraHasTalentTrees(EraId era) { return era == ERA_VANILLA; }  // add ERA_TBC when its trees ship
```

An era **not** in the allowlist behaves exactly like WotLK: native Blizzard talent frame, stock
talents intact, points unpinned. Fail-safe: importing TBC talent *data* alone can never flip the
window on — the window turns on only when this allowlist is deliberately extended (one edit) and the
matching addon trees are shipped.

Threaded through (all `era != ERA_WOTLK` / `era == ERA_WOTLK` managed-gates replaced):
- `EraTransition.cpp` — `Pin()`, and `Run()`'s crossing branches (now a 3-way: enter-managed / leave-managed-to-native / native↔native no-op).
- `EraTalentPin.cpp` — `OnPlayerLogin` ReapplyOnLogin gate, `OnPlayerLevelChanged` SYNC-refresh gate, `OnPlayerFreeTalentPointsChanged` bail, private `pin()`.

**Client stays server-authoritative.** The SYNC header gained a `managed` (1/0) field:
`SYNC <era> <managed> <availPts> [<seq>/<total>] <id:rank,...>`. The addon sets `isEraChar` from that
flag (`Comms.lua`) instead of computing `era ~= 2` — removing the client's only piece of
non-server-authoritative logic.

## Crossing matrix (behavior after fix)

| Crossing | from managed? | to managed? | Behavior |
|---|---|---|---|
| Vanilla → TBC (progress) | yes | no | strip Vanilla passives; restore native WotLK frame + full talent pool; points NOT pinned |
| TBC → WotLK (progress) | no | no | **no-op** — keeps the WotLK spec spent during TBC |
| WotLK → Vanilla / TBC → Vanilla (GM/back) | no | yes | become era char — stock talents reset, points pinned to 0 |
| Vanilla → WotLK | yes | no | unchanged (native restore) |
| same-era login / first sighting | — | — | no talent mutation; TBC char keeps native talents, points intact |

A previously-broken TBC test character self-heals on next login: with 0 spent talents the core's
`InitTalentForLevel` restores the level-based pool automatically, and `pin()` no longer zeroes it.

## Verification

- Headless Lua: `lua5.1 client-addons-src/EraTalents/test_comms.lua` → PASS. New cases assert the
  new header format and that **era 1 (TBC) with `managed=0` → `isEraChar=false`** (the regression
  guard), plus a future `managed=1` TBC → `isEraChar=true`.
- `lua5.1 client-addons-src/EraTalents/test_tooltip.lua` → PASS.
- C++ include hygiene confirmed (`EraTransition.h` pulls in `EraTalentIP.h`; the other two callers
  include it directly). No bare `era != ERA_WOTLK` / `era == ERA_WOTLK` managed-gate remains.
- Code-review subagent pass on the diff (crossing matrix + protocol).

### Pending (before/at production push)
- Local module rebuild for runtime C++ verification, then a real-client check: create/impersonate a
  TBC-era character and confirm the **native WotLK talent frame opens and points are spendable**;
  confirm a Vanilla char still gets the custom window; confirm a Vanilla→(TBC/WotLK) progression
  crossing restores a spendable WotLK pool. (Deferred: the working tree currently also holds a
  concurrent class-review session's edits — build after that settles.)
- When TBC trees are authored: add `ERA_TBC` to `EraHasTalentTrees`, ship the addon's TBC tree
  Lua, and add a pointer for this fix into `docs/era-talents-framework.md`.
