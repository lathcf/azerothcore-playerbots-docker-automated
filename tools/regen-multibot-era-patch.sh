#!/usr/bin/env bash
# regen-multibot-era-patch.sh — re-cut patches/0022-multibot-bridge-era-talents.patch from
# the fork working tree at azerothcore-wotlk/modules/mod-multibot-bridge.
#
# ONE file, touched by NO other patch (verified 2026-08-26) -> baseline = pristine clone HEAD.
# The --src/--dst prefix rewrite makes the output apply from the FORK ROOT, exactly how
# setup.sh's apply_patches applies every patch (git -C "$AC_DIR" apply).
#
# LITERAL PATHS ONLY on every git/cp line — zsh does not word-split an unquoted $var.
# Re-runnable from the edited working-tree state; leaves the worktree at 0022, index pristine.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
AC="$ROOT/azerothcore-wotlk"
MB="$AC/modules/mod-multibot-bridge"
OUT="$ROOT/patches/0022-multibot-bridge-era-talents.patch"
REL="src/MultiBotBridge.cpp"

[[ -d "$MB/.git" ]] || { echo "ERROR: $MB is not a git clone (run setup.sh first)" >&2; exit 1; }

# The 0022 edit must actually be present in the worktree.
grep -q "EraTalentBots_SpecTabs" "$MB/$REL" \
  || { echo "ERROR: $REL has no era spec-tab hook — 0022 not applied to the worktree?" >&2; exit 1; }

TMP="$(mktemp -d)"
restore() {
  if [[ -f "$TMP/MultiBotBridge.cpp" ]]; then cp "$TMP/MultiBotBridge.cpp" "$MB/$REL"; fi
  git -C "$MB" reset -q -- "$REL" 2>/dev/null || true
  rm -rf "$TMP"
}
trap restore EXIT

# 1. Save the current (0022-bearing) copy.
cp "$MB/$REL" "$TMP/MultiBotBridge.cpp"

# 2. Baseline = pristine; stage it, then restore the 0022 worktree state.
git -C "$MB" checkout -- "$REL"
if grep -q "EraTalentBots_SpecTabs" "$MB/$REL"; then
  echo "GATE FAIL: pristine baseline already carries the 0022 hunk (upstream absorbed it?)" >&2; exit 1
fi
git -C "$MB" add -- "$REL"
cp "$TMP/MultiBotBridge.cpp" "$MB/$REL"

# 3. Index(baseline) vs worktree(0022) == exactly the 0022 hunk.
git -C "$MB" diff \
  --src-prefix=a/modules/mod-multibot-bridge/ \
  --dst-prefix=b/modules/mod-multibot-bridge/ \
  -- "$REL" \
  > "$TMP/0022.patch"

# 4. Output gates.
[[ -s "$TMP/0022.patch" ]] || { echo "GATE FAIL: generated patch is empty" >&2; exit 1; }
[[ "$(grep -c '^diff --git' "$TMP/0022.patch")" -eq 1 ]] \
  || { echo "GATE FAIL: expected exactly 1 file in the patch" >&2; exit 1; }
grep -q "EraTalentBots_SpecTabs" "$TMP/0022.patch" \
  || { echo "GATE FAIL: patch missing the era spec-tab hook" >&2; exit 1; }

cp "$TMP/0022.patch" "$OUT"
echo "OK: wrote $OUT ($(wc -l < "$OUT") lines)"
git -C "$AC" apply --stat "$OUT" | sed 's/^/    /'
