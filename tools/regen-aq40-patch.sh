#!/usr/bin/env bash
# regen-aq40-patch.sh — re-cut patches/0016-playerbot-aq40.patch from the fork
# working tree at azerothcore-wotlk/modules/mod-playerbots.
#
# BASELINE-AWARE for FOUR shared wiring files (plus the LATER era-talents 02-era-ai.patch on
# PlayerbotAI.cpp, stripped from the working copy by reverse-apply). Patches are applied UNSTAGED, so a naive
# `git diff` would embed their hunks into 0016:
#   * src/Bot/PlayerbotAI.cpp                        also 0003, 0004, 0011, 0014
#   * src/Ai/Raid/RaidStrategyContext.h              also 0014
#   * src/Bot/Engine/BuildSharedTriggerContexts.cpp  also 0014
#   * src/Bot/Engine/BuildSharedActionContexts.cpp   also 0014
# For those we rebuild a baseline in the git index and diff against it. The Aq40/ dir is new and
# rides along via `add -N`.
#
# LITERAL PATHS ONLY on every git/cp line — neither zsh nor fish word-splits an unquoted var,
# so a `for f in $FILES` loop silently copies nothing and cuts an empty patch.
#
# Re-runnable; leaves the fork tree and index exactly as found.

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
AC="$ROOT/azerothcore-wotlk"
PB="$AC/modules/mod-playerbots"
OUT="$ROOT/patches/0016-playerbot-aq40.patch"
P0003="$ROOT/patches/0003-playerbot-wintergrasp.patch"
P0004="$ROOT/patches/0004-playerbot-wintergrasp-siege.patch"
P0011="$ROOT/patches/0011-playerbot-perfmon-hotpath.patch"
P0014="$ROOT/patches/0014-playerbot-sunwell.patch"

[[ -d "$PB/.git" ]] || { echo "ERROR: $PB is not a git clone (run setup.sh first)" >&2; exit 1; }
for p in "$P0003" "$P0004" "$P0011" "$P0014"; do
  [[ -f "$p" ]] || { echo "ERROR: baseline patch $(basename "$p") missing" >&2; exit 1; }
done

[[ -d "$PB/src/Ai/Raid/Aq40" ]] \
  || { echo "ERROR: src/Ai/Raid/Aq40 missing — AQ40 edits not in the fork worktree" >&2; exit 1; }
grep -q 'case 531' "$PB/src/Bot/PlayerbotAI.cpp" \
  || { echo "ERROR: PlayerbotAI.cpp has no case 531" >&2; exit 1; }
grep -q 'Aq40TriggerContext' "$PB/src/Bot/Engine/BuildSharedTriggerContexts.cpp" \
  || { echo "ERROR: trigger context not wired — that failure is SILENT at runtime" >&2; exit 1; }
grep -q 'Aq40ActionContext' "$PB/src/Bot/Engine/BuildSharedActionContexts.cpp" \
  || { echo "ERROR: action context not wired — that failure is SILENT at runtime" >&2; exit 1; }

TMP="$(mktemp -d)"
restore() {
  [[ -f "$TMP/PlayerbotAI.cpp" ]] && cp "$TMP/PlayerbotAI.cpp" "$PB/src/Bot/PlayerbotAI.cpp"
  [[ -f "$TMP/RaidStrategyContext.h" ]] && cp "$TMP/RaidStrategyContext.h" "$PB/src/Ai/Raid/RaidStrategyContext.h"
  [[ -f "$TMP/BuildSharedTriggerContexts.cpp" ]] && cp "$TMP/BuildSharedTriggerContexts.cpp" "$PB/src/Bot/Engine/BuildSharedTriggerContexts.cpp"
  [[ -f "$TMP/BuildSharedActionContexts.cpp" ]] && cp "$TMP/BuildSharedActionContexts.cpp" "$PB/src/Bot/Engine/BuildSharedActionContexts.cpp"
  git -C "$PB" reset -q -- \
    src/Bot/PlayerbotAI.cpp \
    src/Ai/Raid/RaidStrategyContext.h \
    src/Bot/Engine/BuildSharedTriggerContexts.cpp \
    src/Bot/Engine/BuildSharedActionContexts.cpp \
    src/Ai/Raid/Aq40 2>/dev/null || true
  rm -rf "$TMP"
}
trap restore EXIT

cp "$PB/src/Bot/PlayerbotAI.cpp" "$TMP/PlayerbotAI.cpp"
cp "$PB/src/Ai/Raid/RaidStrategyContext.h" "$TMP/RaidStrategyContext.h"
cp "$PB/src/Bot/Engine/BuildSharedTriggerContexts.cpp" "$TMP/BuildSharedTriggerContexts.cpp"
cp "$PB/src/Bot/Engine/BuildSharedActionContexts.cpp" "$TMP/BuildSharedActionContexts.cpp"

git -C "$PB" checkout -- src/Bot/PlayerbotAI.cpp
git -C "$AC" apply --include=modules/mod-playerbots/src/Bot/PlayerbotAI.cpp "$P0003"
git -C "$AC" apply --include=modules/mod-playerbots/src/Bot/PlayerbotAI.cpp "$P0004"
git -C "$AC" apply --include=modules/mod-playerbots/src/Bot/PlayerbotAI.cpp "$P0011"
git -C "$AC" apply --include=modules/mod-playerbots/src/Bot/PlayerbotAI.cpp "$P0014"

git -C "$PB" checkout -- src/Ai/Raid/RaidStrategyContext.h
git -C "$AC" apply --include=modules/mod-playerbots/src/Ai/Raid/RaidStrategyContext.h "$P0014"

git -C "$PB" checkout -- src/Bot/Engine/BuildSharedTriggerContexts.cpp
git -C "$AC" apply --include=modules/mod-playerbots/src/Bot/Engine/BuildSharedTriggerContexts.cpp "$P0014"

git -C "$PB" checkout -- src/Bot/Engine/BuildSharedActionContexts.cpp
git -C "$AC" apply --include=modules/mod-playerbots/src/Bot/Engine/BuildSharedActionContexts.cpp "$P0014"

git -C "$PB" add -- \
  src/Bot/PlayerbotAI.cpp \
  src/Ai/Raid/RaidStrategyContext.h \
  src/Bot/Engine/BuildSharedTriggerContexts.cpp \
  src/Bot/Engine/BuildSharedActionContexts.cpp

cp "$TMP/PlayerbotAI.cpp" "$PB/src/Bot/PlayerbotAI.cpp"
# LATER patches on PlayerbotAI.cpp: mod-era-talents' 02-era-ai.patch is applied AFTER the overlay's
# patches/*.patch loop, so it is in the working tree but belongs in no baseline — strip it from the
# working copy by reverse-apply, or 0016 absorbs its hunk (happened on the 2026-09-25 C'Thun re-cut).
# The EXIT trap restores the real file from $TMP.
ERA_AI="$AC/modules/mod-era-talents/patches/playerbots/02-era-ai.patch"
if [[ -f "$ERA_AI" ]]; then
  if git -C "$AC" apply -R --check --include=modules/mod-playerbots/src/Bot/PlayerbotAI.cpp "$ERA_AI" 2>/dev/null; then
    git -C "$AC" apply -R --include=modules/mod-playerbots/src/Bot/PlayerbotAI.cpp "$ERA_AI"
  else
    echo "WARN: era 02-era-ai.patch does not reverse-apply on PlayerbotAI.cpp — contamination canary decides" >&2
  fi
fi
cp "$TMP/RaidStrategyContext.h" "$PB/src/Ai/Raid/RaidStrategyContext.h"
cp "$TMP/BuildSharedTriggerContexts.cpp" "$PB/src/Bot/Engine/BuildSharedTriggerContexts.cpp"
cp "$TMP/BuildSharedActionContexts.cpp" "$PB/src/Bot/Engine/BuildSharedActionContexts.cpp"
git -C "$PB" add -N -- src/Ai/Raid/Aq40

git -C "$PB" diff --src-prefix=a/modules/mod-playerbots/ --dst-prefix=b/modules/mod-playerbots/ -- \
  src/Ai/Raid/Aq40 \
  src/Ai/Raid/RaidStrategyContext.h \
  src/Bot/PlayerbotAI.cpp \
  src/Bot/Engine/BuildSharedTriggerContexts.cpp \
  src/Bot/Engine/BuildSharedActionContexts.cpp > "$OUT"

# CONTAMINATION CANARY, the mirror of the "Aq40" canary in regen-sunwell-patch.sh. Cross-patch
# leaks (a baseline-rebuild miss embedding another patch's hunk) can ONLY land in the four SHARED
# wiring files — a brand-new file cannot carry another patch's pre-existing hunk. So the scan is
# scoped to non-Aq40 file blocks: the Aq40/ sources legitimately mention "SWP"/"sunwell" in comments
# explaining the name-prefixing, and scanning them would be a gate that fires on our own docs.
# Scoped further to ADDED/REMOVED lines only — grepping context lines makes a gate that can never
# fail, which has happened three times in this project.
SHARED_HUNKS="$(awk '/^diff --git/ { skip = ($0 ~ /src\/Ai\/Raid\/Aq40\//) } !skip' "$OUT")"
for banned in "SWP" "sunwell" "case 580" "SMSG_BATTLEFIELD_MGR_ENTRY_INVITE" "isBFGroup" \
              "PerfMonitorOperation" "EraTalentBots"; do
  if printf '%s\n' "$SHARED_HUNKS" | grep -E '^[+-]' | grep -v '^[+-][+-][+-]' | grep -q -- "$banned"; then
    echo "ERROR: 0016 shared-file hunks contain '$banned' — another patch's hunks leaked in." >&2
    rm -f "$OUT"
    exit 1
  fi
done

echo "==> cut $(basename "$OUT") ($(wc -l < "$OUT") lines)"
