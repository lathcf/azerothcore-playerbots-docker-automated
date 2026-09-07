#!/usr/bin/env bash
# regen-wandspec-patch.sh — re-cut patches/0025-core-wand-spec-era-no-spell-leak.patch from the
# fork working tree at azerothcore-wotlk.
#
# CORE-ONLY patch (like 0008/0010/0018, unlike every module patch): it touches
# src/server/game/Entities/Unit/Unit.cpp in the fork itself. A plain `git -C "$AC" diff` with NO
# prefix rewriting is therefore correct — the a/src/... paths already resolve from the fork root,
# which is where setup.sh's apply_patches runs `git -C "$AC_DIR" apply`. Do NOT copy the
# --src-prefix/--dst-prefix flags from the module regen scripts; they would corrupt the paths.
#
# BASELINE-AWARE (this is the trap that bit the 0004 regen): Unit.cpp is ALSO edited by patch 0012
# (core-bot-aura-batching, in _UpdateSpells), patch 0018 (core-shatter-crit-vs-frozen, in
# SpellTakenCritChance) AND patch 0026 (core-molten-fury-era-window, also in
# SpellPctDamageModsDone). A
# naive whole-file `git diff -- Unit.cpp` embeds ALL of their hunks into
# this patch. To emit ONLY 0025's hunk we reverse ALL THREE out of the working tree first (all hunks
# are far from ours in SpellPctDamageModsDone, so they reverse/re-apply cleanly with 0025 present),
# diff, then re-apply ALL THREE in numeric order (0012, 0018, 0026) to match apply_patches. If a NEW
# patch starts touching Unit.cpp, add it to CONTAMINATORS below.
#
# WHAT 0025 IS: a band-gated core guard for mod-era-talents "Wand Specialization". The era passive is
# aura 79 MOD_DAMAGE_PERCENT_DONE gated to ITEM_SUBCLASS_WEAPON_WAND with a multi-school misc (126),
# because a wand SHOT takes the equipped wand's magic school. The stock weapon guard in
# Unit::SpellPctDamageModsDone only skips a weapon-gated aura for a non-weapon spell when misc ==
# NORMAL, so a misc-126 wand aura slips past it and would boost every same-school SPELL cast while a
# wand is held. This hunk skips such an aura for non-weapon spells when its id is in the reserved era
# band [920000, 950000) and it is wand-subclass gated. Wand shots are unaffected (they go through
# MeleeDamageBonusDone, not this function). Inert for every stock aura.
#
# LITERAL PATHS ONLY on every git line — the shell does not word-split an unquoted $var, so a
# `for f in $FILES` loop would cut an empty patch (this trap has bitten the WG/publish recipes).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
AC="$ROOT/azerothcore-wotlk"
OUT="$ROOT/patches/0025-core-wand-spec-era-no-spell-leak.patch"

[[ -d "$AC/.git" ]] || { echo "ERROR: $AC is not a git clone (run setup.sh first)" >&2; exit 1; }
[[ -f "$AC/src/server/game/Entities/Unit/Unit.cpp" ]] \
  || { echo "ERROR: missing core Unit.cpp in the fork worktree" >&2; exit 1; }

# The edit must actually be present, or we would cut an empty patch over a good one and silently
# drop the wand-leak guard on the next fork reset.
grep -q "a wand SHOT takes the equipped wand" "$AC/src/server/game/Entities/Unit/Unit.cpp" \
  || { echo "ERROR: Unit.cpp has no era wand-leak hunk — 0025 not applied?" >&2; exit 1; }

# Other applied patches that also edit Unit.cpp — reverse them so the diff is 0025-only, then
# restore. LITERAL paths (no $var word-splitting).
CONTAMINATOR_0012="$ROOT/patches/0012-core-bot-aura-batching.patch"
CONTAMINATOR_0018="$ROOT/patches/0018-core-shatter-crit-vs-frozen.patch"
CONTAMINATOR_0026="$ROOT/patches/0026-core-molten-fury-era-window.patch"
[[ -f "$CONTAMINATOR_0012" ]] || { echo "ERROR: missing $CONTAMINATOR_0012" >&2; exit 1; }
[[ -f "$CONTAMINATOR_0018" ]] || { echo "ERROR: missing $CONTAMINATOR_0018" >&2; exit 1; }
[[ -f "$CONTAMINATOR_0026" ]] || { echo "ERROR: missing $CONTAMINATOR_0026" >&2; exit 1; }

git -C "$AC" apply --reverse "$CONTAMINATOR_0026" \
  || { echo "ERROR: could not reverse 0026 to isolate 0025 — resolve overlap first" >&2; exit 1; }
git -C "$AC" apply --reverse "$CONTAMINATOR_0018" \
  || { echo "ERROR: could not reverse 0018 to isolate 0025 — resolve overlap first" >&2; git -C "$AC" apply "$CONTAMINATOR_0026"; exit 1; }
git -C "$AC" apply --reverse "$CONTAMINATOR_0012" \
  || { echo "ERROR: could not reverse 0012 to isolate 0025 — resolve overlap first" >&2; git -C "$AC" apply "$CONTAMINATOR_0018"; git -C "$AC" apply "$CONTAMINATOR_0026"; exit 1; }

git -C "$AC" diff -- src/server/game/Entities/Unit/Unit.cpp > "$OUT"

# Re-apply ALL THREE in numeric order so the working tree is whole again (build/runtime depend on them).
git -C "$AC" apply "$CONTAMINATOR_0012" \
  || { echo "ERROR: FAILED to re-apply 0012 — working tree is now missing it! re-run setup.sh" >&2; exit 1; }
git -C "$AC" apply "$CONTAMINATOR_0018" \
  || { echo "ERROR: FAILED to re-apply 0018 — working tree is now missing it! re-run setup.sh" >&2; exit 1; }
git -C "$AC" apply "$CONTAMINATOR_0026" \
  || { echo "ERROR: FAILED to re-apply 0026 — working tree is now missing it! re-run setup.sh" >&2; exit 1; }

# Guard: the isolated patch must contain ONLY the wand-leak hunk (no 0012 / 0018 / 0026 lines).
if grep -q "BOT_AURA_UPDATE_INTERVAL\|_UpdateSpells" "$OUT"; then
    echo "ERROR: 0025 patch still contains 0012 hunks — isolation failed" >&2
    exit 1
fi
if grep -q "crit chance vs FROZEN" "$OUT"; then
    echo "ERROR: 0025 patch still contains 0018 hunks — isolation failed" >&2
    exit 1
fi
if grep -q "TBC Molten Fury's window is 20%" "$OUT"; then
    echo "ERROR: 0025 patch still contains 0026 hunks — isolation failed" >&2
    exit 1
fi
grep -q "a wand SHOT takes the equipped wand" "$OUT" \
  || { echo "ERROR: 0025 patch is empty/missing the wand-leak hunk" >&2; exit 1; }
echo "wrote $OUT ($(wc -l < "$OUT") lines)"
