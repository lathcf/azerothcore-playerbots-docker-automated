#!/usr/bin/env bash
# regen-owner-trade-patch.sh — re-cut patches/0017-playerbot-owner-trade-bypass.patch from the
# fork working tree at azerothcore-wotlk/modules/mod-playerbots.
#
# NOT baseline-aware: src/Ai/Base/Actions/TradeStatusAction.cpp is touched by NO other patch,
# so a plain scoped diff of the worktree edit against the pristine index IS exactly the 0017
# hunk. (Contrast regen-perfmon/arena/wg-siege, which share a file with earlier patches and must
# rebuild a baseline first.) If a future patch ever also edits this file, this script must be
# upgraded to the baseline-aware recipe — see regen-perfmon-patch.sh.
#
# LITERAL PATHS ONLY on every git/cp line — zsh does not word-split an unquoted $var (a `for f
# in $FILES` loop silently copies nothing and cuts an empty patch; this trap has bitten before).
#
# Re-runnable from both the edited working-tree state AND a post-setup.sh state (0017 already
# applied to the worktree). Leaves the fork worktree at 0017 and the index pristine.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
AC="$ROOT/azerothcore-wotlk"
PB="$AC/modules/mod-playerbots"
OUT="$ROOT/patches/0017-playerbot-owner-trade-bypass.patch"
REL="src/Ai/Base/Actions/TradeStatusAction.cpp"

[[ -d "$PB/.git" ]] || { echo "ERROR: $PB is not a git clone (run setup.sh first)" >&2; exit 1; }

# The edit must actually be present in the worktree.
grep -q "ownerTrade" "$PB/$REL" \
  || { echo "ERROR: $REL has no ownerTrade bypass — 0017 not applied to the worktree?" >&2; exit 1; }

TMP="$(mktemp -d)"
restore() {
  # Always leave the fork exactly as found: 0017 worktree state, pristine index.
  # (`if` not `&&` — under set -e a failing `[[ ]] && cmd` compound aborts the trap.)
  if [[ -f "$TMP/TradeStatusAction.cpp" ]]; then cp "$TMP/TradeStatusAction.cpp" "$PB/$REL"; fi
  git -C "$PB" reset -q -- "$REL" 2>/dev/null || true
  rm -rf "$TMP"
}
trap restore EXIT

# 1. Save the current (0017-bearing) copy.
cp "$PB/$REL" "$TMP/TradeStatusAction.cpp"

# 2. Pristine baseline (no other patch touches this file, so pristine == the correct baseline).
git -C "$PB" checkout -- "$REL"

# 3. Stage the pristine baseline (index == HEAD for this path).
git -C "$PB" add -- "$REL"

# 4. Restore the 0017 worktree state.
cp "$TMP/TradeStatusAction.cpp" "$PB/$REL"

# 5. Index(pristine) vs worktree(0017) == exactly the 0017 hunk. Prefixes make the patch apply
#    from the fork root, matching how apply_patches invokes `git -C "$AC_DIR" apply`.
git -C "$PB" diff \
  --src-prefix=a/modules/mod-playerbots/ \
  --dst-prefix=b/modules/mod-playerbots/ \
  -- "$REL" \
  > "$TMP/0017.patch"

# (index/worktree restoration happens in the EXIT trap)

# 6. Gates.
[[ -s "$TMP/0017.patch" ]] || { echo "GATE FAIL: generated patch is empty" >&2; exit 1; }
[[ "$(grep -c '^diff --git' "$TMP/0017.patch")" -eq 1 ]] \
  || { echo "GATE FAIL: expected exactly 1 file in the patch" >&2; exit 1; }
for needle in "ownerTrade" "PLAYERBOT_SECURITY_ALLOW_ALL" "IsRealPlayer(GetMaster())"; do
  grep -q -- "$needle" "$TMP/0017.patch" \
    || { echo "GATE FAIL: patch missing expected content: $needle" >&2; exit 1; }
done

cp "$TMP/0017.patch" "$OUT"
echo "OK: wrote $OUT ($(wc -l < "$OUT") lines)"
git -C "$PB" apply --stat "$OUT" | sed 's/^/    /'
