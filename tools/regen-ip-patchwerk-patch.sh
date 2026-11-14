#!/usr/bin/env bash
# regen-ip-patchwerk-patch.sh — re-cut patches/0024-ip-naxx40-patchwerk-hateful-strike.patch from the fork
# working tree at azerothcore-wotlk/modules/mod-individual-progression.
#
# 0024: IP's boss_patchwerk_40 casts Hateful Strike (41926) with a custom base point (17680-18280 doable,
# 22100-22850 otherwise), but the core still adds the spell's 1..4501 die roll on top, so strikes landed up to
# 4500 higher than IP intends. 0024 adds spell_patchwerk_hateful_strike_40 (gated on caster entry 351028) that
# sets the effect value back to the boss's own number, plus the spell_script_names row binding it to 41926.
#
# The row goes into IP's EXISTING naxx40_spells.sql (every statement there is DELETE+INSERT, so the updater's
# re-apply on the changed hash is idempotent), not a new file: setup.sh/update.sh only `git clean` untracked
# files under src/ after a reset, so a patch-created file under data/sql would survive the reset and make the
# next apply_patches fail with "already exists".
#
# NOT baseline-aware: 0024 touches ONLY src/naxx40Scripts/boss_patchwerk_40.cpp and
# data/sql/world/base/naxx40_spells.sql, and no other overlay or mod-era-talents patch touches either
# (checked 2026-10-31). A plain scoped diff therefore emits exactly 0024's hunks. The prefix
# flags make the patch apply from the fork root, matching how setup.sh's apply_patches runs
# `git -C "$AC_DIR" apply`.
#
# LITERAL PATHS ONLY on every git line. zsh does not word-split an unquoted $var, so a
# `for f in $FILES` loop would cut an empty patch (this trap has bitten the WG/publish recipes).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
AC="$ROOT/azerothcore-wotlk"
IP="$AC/modules/mod-individual-progression"
OUT="$ROOT/patches/0024-ip-naxx40-patchwerk-hateful-strike.patch"

[[ -d "$IP/.git" ]] || { echo "ERROR: $IP is not a git clone (run setup.sh first)" >&2; exit 1; }
git -C "$IP" diff \
  --src-prefix=a/modules/mod-individual-progression/ \
  --dst-prefix=b/modules/mod-individual-progression/ \
  -- src/naxx40Scripts/boss_patchwerk_40.cpp data/sql/world/base/naxx40_spells.sql \
  > "$OUT"

# Gates. A bad patch here plus a module reset would orphan the work.
[[ -s "$OUT" ]] || { echo "GATE FAIL: generated patch is empty" >&2; exit 1; }
if grep '^diff --git' "$OUT" \
    | grep -v ' a/modules/mod-individual-progression/src/naxx40Scripts/boss_patchwerk_40.cpp ' \
    | grep -v ' a/modules/mod-individual-progression/data/sql/world/base/naxx40_spells.sql '; then
  echo "GATE FAIL: patch touches unexpected files (listed above)" >&2; exit 1
fi
grep -q '^+.*RegisterSpellScript(spell_patchwerk_hateful_strike_40);' "$OUT" \
  || { echo "GATE FAIL: patch does not register the Hateful Strike spell script" >&2; exit 1; }
grep -q "^+INSERT INTO \`spell_script_names\`.*41926, 'spell_patchwerk_hateful_strike_40'" "$OUT" \
  || { echo "GATE FAIL: patch does not bind 41926 in spell_script_names" >&2; exit 1; }
# The worktree must contain exactly this patch: reverse-applying it must be clean.
git -C "$AC" apply --reverse --check "$OUT" \
  || { echo "GATE FAIL: patch does not reverse-apply against the fork worktree" >&2; exit 1; }

echo "OK: wrote $OUT ($(wc -l < "$OUT") lines)"
git -C "$AC" apply --stat "$OUT" | sed 's/^/    /'
