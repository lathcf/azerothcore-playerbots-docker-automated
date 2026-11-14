#!/usr/bin/env bash
# regen-ip-horsemen-patch.sh — re-cut patches/0023-ip-naxx40-horsemen-doable-mograine.patch from the fork
# working tree at azerothcore-wotlk/modules/mod-individual-progression.
#
# 0023: IP's boss_four_horsemen_40 has a reduced Unholy Shadow for Mograine under doableNaxx40Bosses_4H, but
# upstream commented it out, so Mograine stayed at the full WotLK value with the setting on. 0023 restores it.
#
# NOT baseline-aware: 0023 touches ONLY src/naxx40Scripts/boss_four_horsemen_40.cpp, and no other overlay or
# mod-era-talents patch touches that file (checked 2026-10-29). A plain scoped diff therefore emits exactly
# 0023's hunk. The prefix flags make the patch apply from the fork root, matching how setup.sh's
# apply_patches runs `git -C "$AC_DIR" apply`.
#
# LITERAL PATHS ONLY on every git line. zsh does not word-split an unquoted $var, so a
# `for f in $FILES` loop would cut an empty patch (this trap has bitten the WG/publish recipes).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
AC="$ROOT/azerothcore-wotlk"
IP="$AC/modules/mod-individual-progression"
OUT="$ROOT/patches/0023-ip-naxx40-horsemen-doable-mograine.patch"

[[ -d "$IP/.git" ]] || { echo "ERROR: $IP is not a git clone (run setup.sh first)" >&2; exit 1; }

git -C "$IP" diff \
  --src-prefix=a/modules/mod-individual-progression/ \
  --dst-prefix=b/modules/mod-individual-progression/ \
  -- src/naxx40Scripts/boss_four_horsemen_40.cpp \
  > "$OUT"

# Gates. A bad patch here plus a module reset would orphan the work.
[[ -s "$OUT" ]] || { echo "GATE FAIL: generated patch is empty" >&2; exit 1; }
if grep '^diff --git' "$OUT" | grep -v ' a/modules/mod-individual-progression/src/naxx40Scripts/boss_four_horsemen_40.cpp '; then
  echo "GATE FAIL: patch touches files other than boss_four_horsemen_40.cpp (listed above)" >&2; exit 1
fi
grep -q "^+.*if (sIndividualProgression->doableNaxx40Bosses_4H)" "$OUT" \
  || { echo "GATE FAIL: patch does not restore the custom Unholy Shadow cast" >&2; exit 1; }
# The worktree must contain exactly this patch: reverse-applying it must be clean.
git -C "$AC" apply --reverse --check "$OUT" \
  || { echo "GATE FAIL: patch does not reverse-apply against the fork worktree" >&2; exit 1; }

echo "OK: wrote $OUT ($(wc -l < "$OUT") lines)"
git -C "$AC" apply --stat "$OUT" | sed 's/^/    /'
