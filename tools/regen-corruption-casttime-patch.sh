#!/usr/bin/env bash
# regen-corruption-casttime-patch.sh — re-cut patches/0019-core-corruption-era-casttime.patch from the
# fork working tree at azerothcore-wotlk.
#
# CORE-ONLY patch (like 0008/0010/0012/0013/0018, unlike every module patch): it touches
# src/server/game/Spells/SpellInfo.cpp in the fork itself. A plain `git -C "$AC" diff` with NO prefix
# rewriting is therefore correct — the a/src/... paths already resolve from the fork root, which is where
# setup.sh's apply_patches invokes `git -C "$AC_DIR" apply`. Do NOT copy the --src-prefix/--dst-prefix
# flags from the module regen scripts; they would corrupt the paths. The gate below enforces this.
#
# NOT baseline-aware: no other patch touches SpellInfo.cpp (0018 touches Unit.cpp, 0008 Item.cpp, etc.),
# so a scoped diff of that one path emits exactly 0019's hunk. Verified at authoring time:
#   grep -l "SpellInfo.cpp" patches/*.patch  => (no match)
# If a FUTURE patch starts touching SpellInfo.cpp, this script MUST become baseline-aware (reverse the
# other patch out before diffing) or it will silently embed the other patch's hunks — the trap that bit
# the 0004 regen. See tools/regen-shatter-patch.sh for the baseline-aware pattern.
#
# WHAT 0019 IS: restores Corruption's Vanilla 2s base cast time for a Vanilla-era warlock, so the
# Improved Corruption talent (node 18202) cast-time SPELLMOD has something to reduce. The era-talents
# module grants a hidden marker aura 932930 (SPELL_AURA_DUMMY, misc 5 — DUMMY-marker registry #5) to
# Vanilla-era warlocks; SpellInfo::CalcCastTime checks HasAura(932930) — gated on the Corruption spell id
# FIRST so non-Corruption casts pay only a map lookup — and overrides the base cast time to 2000ms before
# ModSpellCastTime. Inert for any non-marked caster or non-Corruption spell.
#
# LITERAL PATHS ONLY on every git line — the shell does not word-split an unquoted $var, so a
# `for f in $FILES` loop would cut an empty patch (this trap has bitten the WG/publish recipes).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
AC="$ROOT/azerothcore-wotlk"
OUT="$ROOT/patches/0019-core-corruption-era-casttime.patch"

[[ -d "$AC/.git" ]] || { echo "ERROR: $AC is not a git clone (run setup.sh first)" >&2; exit 1; }
[[ -f "$AC/src/server/game/Spells/SpellInfo.cpp" ]] \
  || { echo "ERROR: missing core SpellInfo.cpp in the fork worktree" >&2; exit 1; }

# The edit must actually be present, or we would cut an empty patch over a good one and silently drop the
# Corruption cast-time restore on the next fork reset.
grep -q "era-talents patch 0019" "$AC/src/server/game/Spells/SpellInfo.cpp" \
  || { echo "ERROR: SpellInfo.cpp has no era-Corruption hunk — 0019 not applied to the worktree?" >&2; exit 1; }

git -C "$AC" diff -- src/server/game/Spells/SpellInfo.cpp > "$OUT"

# Gates.
[[ -s "$OUT" ]] || { echo "GATE FAIL: generated patch is empty" >&2; exit 1; }
[[ "$(grep -c '^diff --git' "$OUT")" -eq 1 ]] || { echo "GATE FAIL: expected exactly 1 file in the patch" >&2; exit 1; }
grep -q "b/src/server/game/Spells/SpellInfo.cpp" "$OUT" \
  || { echo "GATE FAIL: patch path is not fork-root relative — did you add --dst-prefix?" >&2; exit 1; }
for needle in "932930" "s_eraVanillaCastTime" "HasAura" "eraBase"; do
  grep -q "$needle" "$OUT" || { echo "GATE FAIL: patch missing expected content: $needle" >&2; exit 1; }
done

echo "OK: wrote $OUT ($(wc -l < "$OUT") lines)"
git -C "$AC" apply --stat "$OUT" | sed 's/^/    /'
