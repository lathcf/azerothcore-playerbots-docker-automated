#!/usr/bin/env bash
# regen-bgbots-patch.sh — re-cut patches/0019-playerbot-battleground-director.patch from the fork
# working tree at azerothcore-wotlk/modules/mod-playerbots.
#
# BASELINE-AWARE (same method as regen-arena-patch.sh): every modified file below is ALSO touched by
# an earlier overlay patch, and the fork's patches are applied unstaged, so a naive `git diff` would
# embed their hunks and break setup.sh's ordered apply. For each modified file the index is set to
# pristine + the earlier patches' hunks for THAT file (0004 then 0005 where both apply), then
# `git diff` (index vs worktree) yields exactly the 0019 hunks. New files ride along via `git add -N`.
#   ActionContext.h            <- 0004, 0005      TriggerContext.h        <- 0004
#   ValueContext.h             <- 0005            BattleGroundTactics.cpp <- 0005
#   BattlegroundStrategy.cpp   <- 0005
# ChooseTargetActions.{h,cpp}, CheckMountStateAction.cpp and PvpValues.h are ALSO modified, but by
# NO earlier patch (verified: no overlay or mod-era-talents patch carries a `diff --git` for any of
# them), so their baseline is plain pristine — they are in the save/checkout/add/restore/reset/diff
# lists but get no `--include` step.
# LITERAL PATHS ONLY on every git/cp line — zsh does not word-split unquoted vars.
# Re-runnable; leaves the fork tree (0019 state) and index (pristine) exactly as found.

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
AC="$ROOT/azerothcore-wotlk"
PB="$AC/modules/mod-playerbots"
OUT="$ROOT/patches/0019-playerbot-battleground-director.patch"
P0004="$ROOT/patches/0004-playerbot-wintergrasp-siege.patch"
P0005="$ROOT/patches/0005-playerbot-arena-coordination.patch"

[[ -d "$PB/.git" ]] || { echo "ERROR: $PB is not a git clone (run setup.sh first)" >&2; exit 1; }
[[ -f "$P0004" && -f "$P0005" ]] || { echo "ERROR: missing 0004/0005 (needed for the baseline)" >&2; exit 1; }
grep -q "BattlegroundBotsApi" "$PB/src/Ai/Base/Actions/BattleGroundTactics.cpp" \
  || { echo "ERROR: BattleGroundTactics.cpp has no BattlegroundBotsApi seam — 0019 edits not present" >&2; exit 1; }

TMP="$(mktemp -d)"
restore() {
  if [[ -f "$TMP/ActionContext.h" ]];          then cp "$TMP/ActionContext.h"          "$PB/src/Ai/Base/ActionContext.h"; fi
  if [[ -f "$TMP/TriggerContext.h" ]];         then cp "$TMP/TriggerContext.h"         "$PB/src/Ai/Base/TriggerContext.h"; fi
  if [[ -f "$TMP/ValueContext.h" ]];           then cp "$TMP/ValueContext.h"           "$PB/src/Ai/Base/ValueContext.h"; fi
  if [[ -f "$TMP/BattleGroundTactics.cpp" ]];  then cp "$TMP/BattleGroundTactics.cpp"  "$PB/src/Ai/Base/Actions/BattleGroundTactics.cpp"; fi
  if [[ -f "$TMP/BattlegroundStrategy.cpp" ]]; then cp "$TMP/BattlegroundStrategy.cpp" "$PB/src/Ai/Base/Strategy/BattlegroundStrategy.cpp"; fi
  if [[ -f "$TMP/ChooseTargetActions.h" ]];    then cp "$TMP/ChooseTargetActions.h"    "$PB/src/Ai/Base/Actions/ChooseTargetActions.h"; fi
  if [[ -f "$TMP/ChooseTargetActions.cpp" ]];  then cp "$TMP/ChooseTargetActions.cpp"  "$PB/src/Ai/Base/Actions/ChooseTargetActions.cpp"; fi
  if [[ -f "$TMP/CheckMountStateAction.cpp" ]]; then cp "$TMP/CheckMountStateAction.cpp" "$PB/src/Ai/Base/Actions/CheckMountStateAction.cpp"; fi
  if [[ -f "$TMP/PvpValues.h" ]];              then cp "$TMP/PvpValues.h"              "$PB/src/Ai/Base/Value/PvpValues.h"; fi
  git -C "$PB" reset -q -- \
    src/Ai/Base/ActionContext.h \
    src/Ai/Base/TriggerContext.h \
    src/Ai/Base/ValueContext.h \
    src/Ai/Base/Actions/BattleGroundTactics.cpp \
    src/Ai/Base/Strategy/BattlegroundStrategy.cpp \
    src/Ai/Base/Actions/ChooseTargetActions.h \
    src/Ai/Base/Actions/ChooseTargetActions.cpp \
    src/Ai/Base/Actions/CheckMountStateAction.cpp \
    src/Ai/Base/Value/PvpValues.h \
    src/Ai/Base/Value/BgKillTargetValue.h \
    src/Ai/Base/Value/BgKillTargetValue.cpp \
    src/Ai/Base/Actions/BgPriorityActions.h \
    src/Ai/Base/Actions/BgPriorityActions.cpp \
    src/Ai/Base/Trigger/BgDirectorTriggers.h \
    src/Ai/Base/Trigger/BgDirectorTriggers.cpp 2>/dev/null || true
  rm -rf "$TMP"
}
trap restore EXIT

# 1. Save the 0019-bearing copies of the shared files.
cp "$PB/src/Ai/Base/ActionContext.h"                    "$TMP/ActionContext.h"
cp "$PB/src/Ai/Base/TriggerContext.h"                   "$TMP/TriggerContext.h"
cp "$PB/src/Ai/Base/ValueContext.h"                     "$TMP/ValueContext.h"
cp "$PB/src/Ai/Base/Actions/BattleGroundTactics.cpp"    "$TMP/BattleGroundTactics.cpp"
cp "$PB/src/Ai/Base/Strategy/BattlegroundStrategy.cpp"  "$TMP/BattlegroundStrategy.cpp"
cp "$PB/src/Ai/Base/Actions/ChooseTargetActions.h"      "$TMP/ChooseTargetActions.h"
cp "$PB/src/Ai/Base/Actions/ChooseTargetActions.cpp"    "$TMP/ChooseTargetActions.cpp"
cp "$PB/src/Ai/Base/Actions/CheckMountStateAction.cpp"  "$TMP/CheckMountStateAction.cpp"
cp "$PB/src/Ai/Base/Value/PvpValues.h"                  "$TMP/PvpValues.h"

# 2. Pristine, then rebuild the baseline = pristine + earlier patches' hunks for these files.
git -C "$PB" checkout -- \
  src/Ai/Base/ActionContext.h \
  src/Ai/Base/TriggerContext.h \
  src/Ai/Base/ValueContext.h \
  src/Ai/Base/Actions/BattleGroundTactics.cpp \
  src/Ai/Base/Strategy/BattlegroundStrategy.cpp \
  src/Ai/Base/Actions/ChooseTargetActions.h \
  src/Ai/Base/Actions/ChooseTargetActions.cpp \
  src/Ai/Base/Actions/CheckMountStateAction.cpp \
  src/Ai/Base/Value/PvpValues.h
git -C "$AC" apply --include=modules/mod-playerbots/src/Ai/Base/ActionContext.h  --include=modules/mod-playerbots/src/Ai/Base/TriggerContext.h "$P0004"
git -C "$AC" apply --include=modules/mod-playerbots/src/Ai/Base/ActionContext.h  --include=modules/mod-playerbots/src/Ai/Base/ValueContext.h \
                   --include=modules/mod-playerbots/src/Ai/Base/Actions/BattleGroundTactics.cpp \
                   --include=modules/mod-playerbots/src/Ai/Base/Strategy/BattlegroundStrategy.cpp "$P0005"

# 3. Stage the baseline.
git -C "$PB" add -- \
  src/Ai/Base/ActionContext.h \
  src/Ai/Base/TriggerContext.h \
  src/Ai/Base/ValueContext.h \
  src/Ai/Base/Actions/BattleGroundTactics.cpp \
  src/Ai/Base/Strategy/BattlegroundStrategy.cpp \
  src/Ai/Base/Actions/ChooseTargetActions.h \
  src/Ai/Base/Actions/ChooseTargetActions.cpp \
  src/Ai/Base/Actions/CheckMountStateAction.cpp \
  src/Ai/Base/Value/PvpValues.h

# 4. Restore the 0019 worktree; intent-to-add whichever new files exist (Phase 2 adds them).
cp "$TMP/ActionContext.h"          "$PB/src/Ai/Base/ActionContext.h"
cp "$TMP/TriggerContext.h"         "$PB/src/Ai/Base/TriggerContext.h"
cp "$TMP/ValueContext.h"           "$PB/src/Ai/Base/ValueContext.h"
cp "$TMP/BattleGroundTactics.cpp"  "$PB/src/Ai/Base/Actions/BattleGroundTactics.cpp"
cp "$TMP/BattlegroundStrategy.cpp" "$PB/src/Ai/Base/Strategy/BattlegroundStrategy.cpp"
cp "$TMP/ChooseTargetActions.h"    "$PB/src/Ai/Base/Actions/ChooseTargetActions.h"
cp "$TMP/ChooseTargetActions.cpp"  "$PB/src/Ai/Base/Actions/ChooseTargetActions.cpp"
cp "$TMP/CheckMountStateAction.cpp" "$PB/src/Ai/Base/Actions/CheckMountStateAction.cpp"
cp "$TMP/PvpValues.h"              "$PB/src/Ai/Base/Value/PvpValues.h"
if [[ -f "$PB/src/Ai/Base/Value/BgKillTargetValue.h" ]];      then git -C "$PB" add -N -- src/Ai/Base/Value/BgKillTargetValue.h; fi
if [[ -f "$PB/src/Ai/Base/Value/BgKillTargetValue.cpp" ]];    then git -C "$PB" add -N -- src/Ai/Base/Value/BgKillTargetValue.cpp; fi
if [[ -f "$PB/src/Ai/Base/Actions/BgPriorityActions.h" ]];    then git -C "$PB" add -N -- src/Ai/Base/Actions/BgPriorityActions.h; fi
if [[ -f "$PB/src/Ai/Base/Actions/BgPriorityActions.cpp" ]];  then git -C "$PB" add -N -- src/Ai/Base/Actions/BgPriorityActions.cpp; fi
if [[ -f "$PB/src/Ai/Base/Trigger/BgDirectorTriggers.h" ]];   then git -C "$PB" add -N -- src/Ai/Base/Trigger/BgDirectorTriggers.h; fi
if [[ -f "$PB/src/Ai/Base/Trigger/BgDirectorTriggers.cpp" ]]; then git -C "$PB" add -N -- src/Ai/Base/Trigger/BgDirectorTriggers.cpp; fi

# 5. Index(baseline) vs worktree(0019) == exactly the 0019 hunks.
git -C "$PB" diff \
  --src-prefix=a/modules/mod-playerbots/ \
  --dst-prefix=b/modules/mod-playerbots/ \
  -- \
  src/Ai/Base/ActionContext.h \
  src/Ai/Base/TriggerContext.h \
  src/Ai/Base/ValueContext.h \
  src/Ai/Base/Actions/BattleGroundTactics.cpp \
  src/Ai/Base/Strategy/BattlegroundStrategy.cpp \
  src/Ai/Base/Actions/ChooseTargetActions.h \
  src/Ai/Base/Actions/ChooseTargetActions.cpp \
  src/Ai/Base/Actions/CheckMountStateAction.cpp \
  src/Ai/Base/Value/PvpValues.h \
  src/Ai/Base/Value/BgKillTargetValue.h \
  src/Ai/Base/Value/BgKillTargetValue.cpp \
  src/Ai/Base/Actions/BgPriorityActions.h \
  src/Ai/Base/Actions/BgPriorityActions.cpp \
  src/Ai/Base/Trigger/BgDirectorTriggers.h \
  src/Ai/Base/Trigger/BgDirectorTriggers.cpp \
  > "$TMP/0019.patch"

# 6. Gates.
[[ -s "$TMP/0019.patch" ]] || { echo "GATE FAIL: generated patch is empty" >&2; exit 1; }
grep -q "BattlegroundBotsApi" "$TMP/0019.patch" || { echo "GATE FAIL: patch missing BattlegroundBotsApi seam" >&2; exit 1; }
if [[ -f "$PB/src/Ai/Base/Value/BgKillTargetValue.cpp" ]]; then
  for needle in "bg kill target" "attack bg priority target" "bg peel for ally" "bg healer follow squad" \
                "BgDirectorOwnsVictim" "bg dismount time"; do
    grep -q "$needle" "$TMP/0019.patch" || { echo "GATE FAIL: patch missing expected content: $needle" >&2; exit 1; }
  done
fi
# Contamination canaries. They are checked on CHANGED lines only (`^[-+]`, excluding the `+++`/`---`
# file headers): an earlier patch's line appearing as diff CONTEXT is expected and correct — patch
# 0019 inserts its BattlegroundBotsApi.h include directly above 0005's ArenaCoordValues.h include,
# so that line is legitimate context proving 0019 applies AFTER 0005. Contamination is an earlier
# patch's hunk being ADDED or REMOVED by this patch, which is exactly what `^[-+]` catches.
CHANGED="$TMP/0019.changed"
grep -E '^[-+]' "$TMP/0019.patch" | grep -Ev '^(\+\+\+|---) ' > "$CHANGED" || true
for canary in "ArenaCoordValues" "arena kill target" "arena pvp trinket" "wg siege" "Wintergrasp"; do
  if grep -q "$canary" "$CHANGED"; then
    echo "GATE FAIL: patch contaminated with earlier-patch content: $canary" >&2; exit 1
  fi
done

cp "$TMP/0019.patch" "$OUT"
echo "OK: wrote $OUT ($(wc -l < "$OUT") lines)"
git -C "$PB" apply --stat "$OUT" | sed 's/^/    /'
