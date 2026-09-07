#!/usr/bin/env bash
# regen-shatter-patch.sh — re-cut patches/0018-core-shatter-crit-vs-frozen.patch from the fork
# working tree at azerothcore-wotlk.
#
# CORE-ONLY patch (like 0008/0010, unlike every module patch): it touches
# src/server/game/Entities/Unit/Unit.cpp in the fork itself. A plain `git -C "$AC" diff` with NO
# prefix rewriting is therefore correct — the a/src/... paths already resolve from the fork root,
# which is where setup.sh's apply_patches runs `git -C "$AC_DIR" apply`. Do NOT copy the
# --src-prefix/--dst-prefix flags from the module regen scripts; they would corrupt the paths.
#
# BASELINE-AWARE (this is the trap that bit the 0004 regen): Unit.cpp is ALSO edited by patch 0012
# (core-bot-aura-batching, in _UpdateSpells), patch 0025 (core-wand-spec-era-
# no-spell-leak, in SpellPctDamageModsDone) AND patch 0026 (core-molten-fury-era-window, also in
# SpellPctDamageModsDone). A naive whole-file `git diff -- Unit.cpp` embeds ALL of
# their hunks into this patch. To emit ONLY 0018's hunk we reverse ALL THREE out of the working tree
# first (all are far from our SpellTakenCritChance hunk, so they reverse/re-apply cleanly with 0018
# present), diff, then re-apply ALL THREE in numeric order (0012, 0025, 0026) to match apply_patches.
# The reverse order is 0026, 0025, THEN 0012 — the mirror of the wand regen's own contaminator shape
# (tools/regen-wandspec-patch.sh). If a NEW patch starts touching Unit.cpp, add it here too.
#
# WHAT 0018 IS: a custom-talent "Shatter" (crit chance vs FROZEN targets) for mod-era-talents. No
# aura or SpellMod expresses conditional crit, so the era-talent passive carries a SPELL_AURA_DUMMY
# marker (MiscValue 1, amount = % bonus) and this hunk in Unit::SpellTakenCritChance adds that amount
# to crit when the victim is in AURA_STATE_FROZEN, honouring only passives in the reserved era spell
# band [920000, 950000) (collision-proof against stock dummy auras). The band was WIDENED from
# 933000 to 950000 in TBC Phase 9 (2026-09-05) so the TBC Shatter node 20857's own auto-passives
# 942856-942860 arm it; that is the same band patch 0025 uses. The Vanilla Shatter passives
# (920360-920364) sit inside the old window and are unaffected.
#
# LITERAL PATHS ONLY on every git line — the shell does not word-split an unquoted $var, so a
# `for f in $FILES` loop would cut an empty patch (this trap has bitten the WG/publish recipes).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
AC="$ROOT/azerothcore-wotlk"
OUT="$ROOT/patches/0018-core-shatter-crit-vs-frozen.patch"

[[ -d "$AC/.git" ]] || { echo "ERROR: $AC is not a git clone (run setup.sh first)" >&2; exit 1; }
[[ -f "$AC/src/server/game/Entities/Unit/Unit.cpp" ]] \
  || { echo "ERROR: missing core Unit.cpp in the fork worktree" >&2; exit 1; }

# The edit must actually be present, or we would cut an empty patch over a good one and silently
# drop Shatter on the next fork reset.
grep -q "crit chance vs FROZEN targets" "$AC/src/server/game/Entities/Unit/Unit.cpp" \
  || { echo "ERROR: Unit.cpp has no era-Shatter hunk — 0018 not applied?" >&2; exit 1; }

# Other applied patches that also edit Unit.cpp — reverse them so the diff is 0018-only, then
# restore. LITERAL paths (no $var word-splitting).
CONTAMINATOR_0012="$ROOT/patches/0012-core-bot-aura-batching.patch"
CONTAMINATOR_0025="$ROOT/patches/0025-core-wand-spec-era-no-spell-leak.patch"
CONTAMINATOR_0026="$ROOT/patches/0026-core-molten-fury-era-window.patch"
[[ -f "$CONTAMINATOR_0012" ]] || { echo "ERROR: missing $CONTAMINATOR_0012" >&2; exit 1; }
[[ -f "$CONTAMINATOR_0025" ]] || { echo "ERROR: missing $CONTAMINATOR_0025" >&2; exit 1; }
[[ -f "$CONTAMINATOR_0026" ]] || { echo "ERROR: missing $CONTAMINATOR_0026" >&2; exit 1; }

git -C "$AC" apply --reverse "$CONTAMINATOR_0026" \
  || { echo "ERROR: could not reverse 0026 to isolate 0018 — resolve overlap first" >&2; exit 1; }
git -C "$AC" apply --reverse "$CONTAMINATOR_0025" \
  || { echo "ERROR: could not reverse 0025 to isolate 0018 — resolve overlap first" >&2; git -C "$AC" apply "$CONTAMINATOR_0026"; exit 1; }
git -C "$AC" apply --reverse "$CONTAMINATOR_0012" \
  || { echo "ERROR: could not reverse 0012 to isolate 0018 — resolve overlap first" >&2; git -C "$AC" apply "$CONTAMINATOR_0025"; git -C "$AC" apply "$CONTAMINATOR_0026"; exit 1; }

git -C "$AC" diff -- src/server/game/Entities/Unit/Unit.cpp > "$OUT"

# Re-apply ALL THREE in numeric order so the working tree is whole again (build/runtime depend on them).
git -C "$AC" apply "$CONTAMINATOR_0012" \
  || { echo "ERROR: FAILED to re-apply 0012 — working tree is now missing it! re-run setup.sh" >&2; exit 1; }
git -C "$AC" apply "$CONTAMINATOR_0025" \
  || { echo "ERROR: FAILED to re-apply 0025 — working tree is now missing it! re-run setup.sh" >&2; exit 1; }
git -C "$AC" apply "$CONTAMINATOR_0026" \
  || { echo "ERROR: FAILED to re-apply 0026 — working tree is now missing it! re-run setup.sh" >&2; exit 1; }

# Guard: the isolated patch must contain ONLY the Shatter hunk (no 0012 / 0025 / 0026 lines).
if grep -q "BOT_AURA_UPDATE_INTERVAL\|_UpdateSpells" "$OUT"; then
    echo "ERROR: 0018 patch still contains 0012 hunks — isolation failed" >&2
    exit 1
fi
if grep -q "ITEM_SUBCLASS_WEAPON_WAND" "$OUT"; then
    echo "ERROR: 0018 patch still contains 0025 hunks — isolation failed" >&2
    exit 1
fi
if grep -q "TBC Molten Fury's window is 20%" "$OUT"; then
    echo "ERROR: 0018 patch still contains 0026 hunks — isolation failed" >&2
    exit 1
fi
grep -q "crit chance vs FROZEN targets" "$OUT" \
  || { echo "ERROR: 0018 patch is empty/missing the Shatter hunk" >&2; exit 1; }
echo "wrote $OUT ($(wc -l < "$OUT") lines)"
