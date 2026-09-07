#!/usr/bin/env bash
# regen-hotw-patch.sh — re-cut patches/0023-core-hotw-era-bear-stamina.patch from the fork working
# tree at azerothcore-wotlk.
#
# CORE-ONLY patch (like 0008/0010/0012/0013/0018/0019, unlike every module patch): it touches
# src/server/game/Spells/Auras/SpellAuraEffects.cpp in the fork itself. A plain `git -C "$AC" diff`
# with NO prefix rewriting is therefore correct — the a/src/... paths already resolve from the fork
# root, which is where setup.sh's apply_patches invokes `git -C "$AC_DIR" apply`. Do NOT copy the
# --src-prefix/--dst-prefix flags from the module regen scripts; they would corrupt the paths. The
# gate below enforces this.
#
# NOT baseline-aware: no other patch touches Auras/SpellAuraEffects.cpp, so a scoped diff of that one
# path emits exactly 0023's hunk. Verified at authoring time (2026-08-31):
#   grep -l 'Auras/SpellAuraEffects\.cpp' patches/*.patch  => (no match)
# NB a loose `grep -l SpellAuraEffects patches/*.patch` DOES match 0019 and 0021 — but only on their
# `#include "SpellAuraEffects.h"` context lines, not on the .cpp. Match the full path when you re-check.
# If a FUTURE patch starts touching this .cpp, this script MUST become baseline-aware (reverse the other
# patch out before diffing) or it will silently embed the other patch's hunks — the trap that bit the
# 0004 regen. See tools/regen-shatter-patch.sh for the baseline-aware pattern.
#
# WHAT 0023 IS: TBC's Heart of the Wild (era-talents TBC druid node 20135) grants Bear/Dire Bear Stamina
# equal to the FULL Intellect percentage; WotLK retuned that to half and computes it in core
# (AuraEffect::HandleShapeshiftBoosts, `HotWMod = amount / 2`) rather than in spell data — so the era
# spell-diff pipeline could not see it and the talent shipped promising 4/8/12/16/20% while delivering
# 2/4/6/8/10%. It cannot be fixed in data (the stat auras are stock global spells, eras are
# per-character; and a second top-up aura cannot hit the numbers because MOD_TOTAL_STAT_PERCENTAGE
# auras combine multiplicatively). The module grants hidden marker aura 946280 (SPELL_AURA_DUMMY,
# misc 18 — DUMMY-marker registry #18) to a TBC-era druid holding the talent; the patch skips the
# halving for the BEAR spell (24899) only when that marker is present. Cat (24900) is deliberately
# untouched — TBC cat attack power really is the halved 2/4/6/8/10%. Inert for any unmarked druid.
#
# WHY IT MUST NOT ARM FOR VANILLA, stated correctly (corrected 2026-09-06, final review M-68): do NOT
# justify it with "Vanilla's own node 18730 tooltip states the halved Stamina too" — that tooltip is
# OUR authored rewrite (era-data/vanilla/druid.yaml, node 18730, `# FIX ROUND (2026-08-21, issue 5/8)`
# rewrote it to describe the shipped WotLK behaviour), so citing it is citing ourselves. The real
# reasons: Vanilla's per-form bonus is the FULL Int% as Stamina in bear and as STRENGTH in cat, and
# the Strength half is unreachable by this patch at all (0023 only skips a division inside the
# existing bear/cat Stamina/AP casts — it cannot change WHICH stat cat gets); and the remaining
# Vanilla bear-half divergence (Int%/2 instead of Int%) is a USER-ACCEPTED Task-10 gap, recorded in
# that same node comment. TBC is the only era 0023 arms for, via marker 946280.
#
# LITERAL PATHS ONLY on every git line — the shell does not word-split an unquoted $var, so a
# `for f in $FILES` loop would cut an empty patch (this trap has bitten the WG/publish recipes).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
AC="$ROOT/azerothcore-wotlk"
OUT="$ROOT/patches/0023-core-hotw-era-bear-stamina.patch"

[[ -d "$AC/.git" ]] || { echo "ERROR: $AC is not a git clone (run setup.sh first)" >&2; exit 1; }
[[ -f "$AC/src/server/game/Spells/Auras/SpellAuraEffects.cpp" ]] \
  || { echo "ERROR: missing core SpellAuraEffects.cpp in the fork worktree" >&2; exit 1; }

# The edit must actually be present, or we would cut an empty patch over a good one and silently drop
# the bear-stamina fix on the next fork reset.
grep -q "era-talents patch 0023" "$AC/src/server/game/Spells/Auras/SpellAuraEffects.cpp" \
  || { echo "ERROR: SpellAuraEffects.cpp has no era-HotW hunk — 0023 not applied to the worktree?" >&2; exit 1; }

git -C "$AC" diff -- src/server/game/Spells/Auras/SpellAuraEffects.cpp > "$OUT"

# Gates.
[[ -s "$OUT" ]] || { echo "GATE FAIL: generated patch is empty" >&2; exit 1; }
[[ "$(grep -c '^diff --git' "$OUT")" -eq 1 ]] || { echo "GATE FAIL: expected exactly 1 file in the patch" >&2; exit 1; }
grep -q "b/src/server/game/Spells/Auras/SpellAuraEffects.cpp" "$OUT" \
  || { echo "GATE FAIL: patch path is not fork-root relative — did you add --dst-prefix?" >&2; exit 1; }
for needle in "946280" "24899" "HotWMod" "HasAura"; do
  grep -q "$needle" "$OUT" || { echo "GATE FAIL: patch missing expected content: $needle" >&2; exit 1; }
done
# The cat half must NOT be touched: TBC cat AP really is the halved value, so added CODE mentioning
# 24900 means someone widened the fix past its era-correct scope. Comment lines are excluded — the
# hunk's own commentary explains why cat is left alone, and matching that was a false positive.
if grep '^+' "$OUT" | grep -vE '^\+\s*(//|\*|/\*)' | grep -q '24900'; then
  echo "GATE FAIL: patch adds CODE touching the CAT branch (24900) — TBC cat AP is deliberately halved" >&2
  exit 1
fi

echo "OK: wrote $OUT ($(wc -l < "$OUT") lines)"
git -C "$AC" apply --stat "$OUT" | sed 's/^/    /'
