#!/usr/bin/env bash
# regen-ip-manual-advance-patch.sh — re-cut patches/0027-ip-manual-advance-states.patch from the
# fork working tree at azerothcore-wotlk/modules/mod-individual-progression.
#
# FOUR files, touched by NO other patch (verified 2026-09-07: 0019 names IP only in a comment
# line, no 'diff --git' for any IP file) -> baseline = pristine clone HEAD for every file.
# The --src/--dst prefix rewrite makes the output apply from the FORK ROOT, exactly how
# setup.sh's apply_patches applies every patch (git -C "$AC_DIR" apply).
#
# LITERAL PATHS ONLY on every git/cp line — zsh does not word-split an unquoted $var.
# Re-runnable from the edited working-tree state; leaves the worktree at 0027, index pristine.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
AC="$ROOT/azerothcore-wotlk"
IP="$AC/modules/mod-individual-progression"
OUT="$ROOT/patches/0027-ip-manual-advance-states.patch"
REL1="src/IndividualProgression.h"
REL2="src/IndividualProgression.cpp"
REL3="src/IndividualProgressionPlayer.cpp"
REL4="conf/individualProgression.conf.dist"

[[ -d "$IP/.git" ]] || { echo "ERROR: $IP is not a git clone (run setup.sh first)" >&2; exit 1; }

# No other patch may diff an IP file (pristine-baseline assumption).
for p in "$ROOT"/patches/*.patch; do
  [[ "$(basename "$p")" == "0027-ip-manual-advance-states.patch" ]] && continue
  if grep '^diff --git' "$p" | grep -q "mod-individual-progression"; then
    echo "GATE FAIL: $(basename "$p") also diffs mod-individual-progression — 0027 is no longer pristine-baseline; make this script baseline-aware first" >&2
    exit 1
  fi
done

# The 0027 edits must actually be present in the worktree.
grep -q "ClampHeldAdvance" "$IP/$REL1" || { echo "ERROR: $REL1 has no ClampHeldAdvance — 0027 not applied to the worktree?" >&2; exit 1; }
grep -q "LoadManualAdvanceStates(sConfigMgr" "$IP/$REL2" || { echo "ERROR: $REL2 has no ManualAdvanceStates config load" >&2; exit 1; }
[[ "$(grep -c "ClampHeldAdvance(killer" "$IP/$REL2")" -eq 3 ]] || { echo "ERROR: $REL2 must clamp at exactly 3 auto-advance sites" >&2; exit 1; }
[[ "$(grep -c "ClampHeldAdvance(player" "$IP/$REL3")" -eq 3 ]] || { echo "ERROR: $REL3 must clamp all 3 quest cases" >&2; exit 1; }
grep -q "IndividualProgression.ManualAdvanceStates" "$IP/$REL4" || { echo "ERROR: $REL4 lacks the ManualAdvanceStates key" >&2; exit 1; }

TMP="$(mktemp -d)"
restore() {
  if [[ -f "$TMP/IndividualProgression.h" ]]; then cp "$TMP/IndividualProgression.h" "$IP/$REL1"; fi
  if [[ -f "$TMP/IndividualProgression.cpp" ]]; then cp "$TMP/IndividualProgression.cpp" "$IP/$REL2"; fi
  if [[ -f "$TMP/IndividualProgressionPlayer.cpp" ]]; then cp "$TMP/IndividualProgressionPlayer.cpp" "$IP/$REL3"; fi
  if [[ -f "$TMP/individualProgression.conf.dist" ]]; then cp "$TMP/individualProgression.conf.dist" "$IP/$REL4"; fi
  git -C "$IP" reset -q -- "$REL1" "$REL2" "$REL3" "$REL4" 2>/dev/null || true
  rm -rf "$TMP"
}
trap restore EXIT

# 1. Save the current (0027-bearing) copies.
cp "$IP/$REL1" "$TMP/IndividualProgression.h"
cp "$IP/$REL2" "$TMP/IndividualProgression.cpp"
cp "$IP/$REL3" "$TMP/IndividualProgressionPlayer.cpp"
cp "$IP/$REL4" "$TMP/individualProgression.conf.dist"

# 2. Baseline = pristine; stage it, then restore the 0027 worktree state.
git -C "$IP" checkout -- "$REL1" "$REL2" "$REL3" "$REL4"
if grep -q "ClampHeldAdvance" "$IP/$REL1"; then
  echo "GATE FAIL: pristine baseline already carries the 0027 hunk (upstream absorbed it?)" >&2; exit 1
fi
git -C "$IP" add -- "$REL1" "$REL2" "$REL3" "$REL4"
cp "$TMP/IndividualProgression.h" "$IP/$REL1"
cp "$TMP/IndividualProgression.cpp" "$IP/$REL2"
cp "$TMP/IndividualProgressionPlayer.cpp" "$IP/$REL3"
cp "$TMP/individualProgression.conf.dist" "$IP/$REL4"

# 3. Index(baseline) vs worktree(0027) == exactly the 0027 hunks.
git -C "$IP" diff \
  --src-prefix=a/modules/mod-individual-progression/ \
  --dst-prefix=b/modules/mod-individual-progression/ \
  -- "$REL1" "$REL2" "$REL3" "$REL4" \
  > "$TMP/0027.patch"

# 4. Output gates.
[[ -s "$TMP/0027.patch" ]] || { echo "GATE FAIL: generated patch is empty" >&2; exit 1; }
[[ "$(grep -c '^diff --git' "$TMP/0027.patch")" -eq 4 ]] || { echo "GATE FAIL: expected exactly 4 files in the patch" >&2; exit 1; }
grep -q "ManualAdvanceStates" "$TMP/0027.patch" || { echo "GATE FAIL: patch missing ManualAdvanceStates" >&2; exit 1; }
if grep -q "^[-+].*UpdateProgressionState(player, static_cast<ProgressionState>(sIndividualProgression->" "$TMP/0027.patch"; then
  echo "GATE FAIL: patch touches a login-time starting-progression call — those must stay unclamped" >&2; exit 1
fi

cp "$TMP/0027.patch" "$OUT"
echo "OK: wrote $OUT ($(wc -l < "$OUT") lines)"
git -C "$AC" apply --stat "$OUT" | sed 's/^/    /'
