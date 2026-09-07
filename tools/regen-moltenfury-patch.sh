#!/usr/bin/env bash
# regen-moltenfury-patch.sh — re-cut patches/0026-core-molten-fury-era-window.patch from the fork
# working tree at azerothcore-wotlk.
#
# CORE-ONLY patch (like 0008/0010/0018/0025): touches src/server/game/Entities/Unit/Unit.cpp in the
# fork itself, so a plain `git -C "$AC" diff` with NO prefix rewriting is correct (apply_patches runs
# `git -C "$AC_DIR" apply` from the fork root). Do NOT copy --src-prefix/--dst-prefix from module regens.
#
# BASELINE-AWARE: Unit.cpp is ALSO edited by 0012 (core-bot-aura-batching), 0018 (shatter crit vs
# frozen) and 0025 (wand-spec leak). Reverse all three (reverse numeric order), diff, re-apply all
# three (numeric order). If a NEW patch starts touching Unit.cpp, add it here AND to the other three
# Unit.cpp regen scripts.
#
# WHAT 0026 IS: the `case 4920: case 4919:` Molten Fury arm in Unit::SpellPctDamageModsDone gates on
# AURA_STATE_HEALTHLESS_35_PERCENT (WotLK). TBC's window is 20%. For an OVERRIDE_CLASS_SCRIPTS aura
# whose spell id is in the reserved era band [920000, 950000) the arm uses
# AURA_STATE_HEALTHLESS_20_PERCENT instead; stock auras are unchanged. mod-era-talents TBC node 20842.
#
# LITERAL PATHS ONLY on every git line (zsh does not word-split an unquoted $var).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
AC="$ROOT/azerothcore-wotlk"
OUT="$ROOT/patches/0026-core-molten-fury-era-window.patch"
UNIT="src/server/game/Entities/Unit/Unit.cpp"

[[ -d "$AC/.git" ]] || { echo "ERROR: $AC is not a git clone (run setup.sh first)" >&2; exit 1; }
[[ -f "$AC/$UNIT" ]] || { echo "ERROR: missing core Unit.cpp in the fork worktree" >&2; exit 1; }

grep -q "TBC Molten Fury's window is 20%" "$AC/$UNIT" \
  || { echo "ERROR: Unit.cpp has no era Molten Fury hunk — 0026 not applied?" >&2; exit 1; }

CONTAMINATOR_0012="$ROOT/patches/0012-core-bot-aura-batching.patch"
CONTAMINATOR_0018="$ROOT/patches/0018-core-shatter-crit-vs-frozen.patch"
CONTAMINATOR_0025="$ROOT/patches/0025-core-wand-spec-era-no-spell-leak.patch"
for f in "$CONTAMINATOR_0012" "$CONTAMINATOR_0018" "$CONTAMINATOR_0025"; do
  [[ -f "$f" ]] || { echo "ERROR: missing $f" >&2; exit 1; }
done

git -C "$AC" apply --reverse "$CONTAMINATOR_0025" \
  || { echo "ERROR: could not reverse 0025 to isolate 0026 — resolve overlap first" >&2; exit 1; }
git -C "$AC" apply --reverse "$CONTAMINATOR_0018" \
  || { echo "ERROR: could not reverse 0018 — restoring 0025" >&2; git -C "$AC" apply "$CONTAMINATOR_0025"; exit 1; }
git -C "$AC" apply --reverse "$CONTAMINATOR_0012" \
  || { echo "ERROR: could not reverse 0012 — restoring 0018/0025" >&2; git -C "$AC" apply "$CONTAMINATOR_0018"; git -C "$AC" apply "$CONTAMINATOR_0025"; exit 1; }

git -C "$AC" diff -- "$UNIT" > "$OUT"

git -C "$AC" apply "$CONTAMINATOR_0012" \
  || { echo "ERROR: FAILED to re-apply 0012 — working tree is now missing it! re-run setup.sh" >&2; exit 1; }
git -C "$AC" apply "$CONTAMINATOR_0018" \
  || { echo "ERROR: FAILED to re-apply 0018 — working tree is now missing it! re-run setup.sh" >&2; exit 1; }
git -C "$AC" apply "$CONTAMINATOR_0025" \
  || { echo "ERROR: FAILED to re-apply 0025 — working tree is now missing it! re-run setup.sh" >&2; exit 1; }

if grep -q "BOT_AURA_UPDATE_INTERVAL\|_UpdateSpells" "$OUT"; then
    echo "ERROR: 0026 patch still contains 0012 hunks — isolation failed" >&2; exit 1
fi
if grep -q "crit chance vs FROZEN" "$OUT"; then
    echo "ERROR: 0026 patch still contains 0018 hunks — isolation failed" >&2; exit 1
fi
if grep -q "a wand SHOT takes the equipped wand" "$OUT"; then
    echo "ERROR: 0026 patch still contains 0025 hunks — isolation failed" >&2; exit 1
fi
grep -q "TBC Molten Fury's window is 20%" "$OUT" \
  || { echo "ERROR: 0026 patch is empty/missing the Molten Fury hunk" >&2; exit 1; }
echo "wrote $OUT ($(wc -l < "$OUT") lines)"
