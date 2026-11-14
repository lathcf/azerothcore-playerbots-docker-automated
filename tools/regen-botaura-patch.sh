#!/usr/bin/env bash
# regen-botaura-patch.sh — re-cut patches/0012-core-bot-aura-batching.patch from the fork
# working tree at azerothcore-wotlk.
#
# CORE-ONLY patch (like 0008/0009/0010, unlike every module patch): it touches
# src/server/game/Entities/Unit/Unit.{h,cpp} in the fork itself. A plain `git -C "$AC" diff`
# with NO prefix rewriting is therefore correct — the a/src/... paths already resolve from the
# fork root, which is where setup.sh's apply_patches invokes `git -C "$AC_DIR" apply`. Do NOT
# copy the --src-prefix/--dst-prefix flags from the module regen scripts; they would corrupt
# the paths. The gate below enforces this.
#
# BASELINE-AWARE (it was not, and that claim went stale): Unit.cpp is now touched by FOUR other
# patches, all of which land AFTER 0012 in the pipeline, so a naive whole-file `git diff` of the
# full-stack worktree would silently embed their hunks into 0012 (the trap that bit the 0004
# regen). The later patches on this file are:
#   * patches/0018-core-modifyaurastate-iterator-safety.patch (overlay, applied after 0012)
#   * mod-era-talents' core patches 01-shatter-crit-vs-frozen / 05-wand-spec-era-no-spell-leak /
#     06-molten-fury-era-window (applied after ALL overlay patches, by that module's own
#     apply-patches.sh)
# Because 0012 is the EARLIEST patch on these files, the right tool is a REVERSE-APPLY of the
# later patches (not an index baseline like regen-arena-patch.sh uses for a middle patch): strip
# them out of the worktree, diff, then restore the saved files. Unit.h is 0012's alone — no
# other patch touches it — but it is saved/restored with Unit.cpp so the pair stays consistent.
# The era module is a CLONE and may be absent; a missing patch file is skipped, not fatal.
# Leaves the fork worktree exactly as found.
#
# WHAT 0012 IS: at 3200 bots a host perf profile showed ~20% of all worldserver CPU in the
# per-tick aura sweeps (Aura::UpdateOwner + AuraEffect::Update + Unit::_UpdateSpells + the
# rb-tree iteration of the aura multimaps) — thousands of fully-buffed bots walking every owned
# aura every map tick to find timers that aren't due. All aura timing is diff-driven, so 0012
# accumulates the diff for BOT sessions (WorldSession::IsHeadless(), core 92fed92e; was IsBot()) and runs the sweeps at ~100ms
# cadence with the summed diff: identical timing semantics, up to ~90ms added latency on
# periodic ticks/expiry, real players untouched. The current-spell pointer bookkeeping at the
# top of _UpdateSpells stays per-tick — it is order-sensitive with the event system (see the
# WARNING in Unit::Update).
#
# LITERAL PATHS ONLY on every git line — the shell does not word-split an unquoted $var, so a
# `for f in $FILES` loop would cut an empty patch (this trap has bitten the WG/publish recipes).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
AC="$ROOT/azerothcore-wotlk"
OUT="$ROOT/patches/0012-core-bot-aura-batching.patch"
P0018="$ROOT/patches/0018-core-modifyaurastate-iterator-safety.patch"
ERA="$AC/modules/mod-era-talents/patches/core"

[[ -d "$AC/.git" ]] || { echo "ERROR: $AC is not a git clone (run setup.sh first)" >&2; exit 1; }

# The edit must actually be present in BOTH files, or we would cut a partial/empty patch over a
# good one and silently drop the batching on the next fork reset.
grep -q "m_botAuraUpdateTimer" "$AC/src/server/game/Entities/Unit/Unit.h" \
  || { echo "ERROR: Unit.h has no m_botAuraUpdateTimer member — 0012 not applied?" >&2; exit 1; }
grep -q "BOT_AURA_UPDATE_INTERVAL" "$AC/src/server/game/Entities/Unit/Unit.cpp" \
  || { echo "ERROR: Unit.cpp has no batching gate — 0012 not applied?" >&2; exit 1; }

TMP="$(mktemp -d)"
restore() {
  # Always leave the fork exactly as found: full-stack worktree.
  # (`if` not `&&` — under set -e a failing `[[ ]] && cmd` compound aborts the trap.)
  if [[ -f "$TMP/Unit.cpp" ]]; then cp "$TMP/Unit.cpp" "$AC/src/server/game/Entities/Unit/Unit.cpp"; fi
  if [[ -f "$TMP/Unit.h" ]];   then cp "$TMP/Unit.h"   "$AC/src/server/game/Entities/Unit/Unit.h"; fi
  rm -rf "$TMP"
}
trap restore EXIT

cp "$AC/src/server/game/Entities/Unit/Unit.cpp" "$TMP/Unit.cpp"
cp "$AC/src/server/game/Entities/Unit/Unit.h"   "$TMP/Unit.h"

# Strip the LATER patches' Unit.cpp hunks out of the worktree so the diff is 0012 alone.
if [[ -f "$P0018" ]]; then
  git -C "$AC" apply -R --include=src/server/game/Entities/Unit/Unit.cpp "$P0018"
else
  echo "NOTE: $P0018 absent — skipped in reverse-apply" >&2
fi
if [[ -f "$ERA/01-shatter-crit-vs-frozen.patch" ]]; then
  git -C "$AC" apply -R --include=src/server/game/Entities/Unit/Unit.cpp "$ERA/01-shatter-crit-vs-frozen.patch"
else
  echo "NOTE: mod-era-talents core patch 01 absent — skipped in reverse-apply" >&2
fi
if [[ -f "$ERA/05-wand-spec-era-no-spell-leak.patch" ]]; then
  git -C "$AC" apply -R --include=src/server/game/Entities/Unit/Unit.cpp "$ERA/05-wand-spec-era-no-spell-leak.patch"
else
  echo "NOTE: mod-era-talents core patch 05 absent — skipped in reverse-apply" >&2
fi
if [[ -f "$ERA/06-molten-fury-era-window.patch" ]]; then
  git -C "$AC" apply -R --include=src/server/game/Entities/Unit/Unit.cpp "$ERA/06-molten-fury-era-window.patch"
else
  echo "NOTE: mod-era-talents core patch 06 absent — skipped in reverse-apply" >&2
fi

git -C "$AC" diff -- src/server/game/Entities/Unit/Unit.h src/server/game/Entities/Unit/Unit.cpp > "$TMP/0012.patch"

# (worktree restoration happens in the EXIT trap)

# Gates.
[[ -s "$TMP/0012.patch" ]] || { echo "GATE FAIL: generated patch is empty" >&2; exit 1; }
[[ "$(grep -c '^diff --git' "$TMP/0012.patch")" -eq 2 ]] || { echo "GATE FAIL: expected exactly 2 files in the patch" >&2; exit 1; }
grep -q "b/src/server/game/Entities/Unit/Unit.cpp" "$TMP/0012.patch" \
  || { echo "GATE FAIL: patch path is not fork-root relative — did you add --dst-prefix?" >&2; exit 1; }
for needle in "m_botAuraUpdateTimer" "BOT_AURA_UPDATE_INTERVAL" "IsHeadless()"; do
  grep -q -- "$needle" "$TMP/0012.patch" || { echo "GATE FAIL: patch missing expected content: $needle" >&2; exit 1; }
done
# Scope: 0012 must not touch the order-sensitive current-spell bookkeeping or auto-repeat path.
# Only inspect added CODE lines (comments may legitimately mention them).
if grep -E '^\+[[:space:]]*[^/[:space:]+]' "$TMP/0012.patch" | grep -qE "m_currentSpells|_UpdateAutoRepeatSpell"; then
  echo "GATE FAIL: patch modifies current-spell handling — out of scope for 0012" >&2; exit 1
fi
# The gate must be inside _UpdateSpells (hunk header shows the enclosing function).
grep -q "^@@.*_UpdateSpells" "$TMP/0012.patch" \
  || { echo "GATE FAIL: Unit.cpp hunk is not inside Unit::_UpdateSpells" >&2; exit 1; }
# CONTAMINATION — fail if the reverse-apply missed and a later patch's hunks leaked into 0012.
for canary in "snapshot.reserve" "mod-era-talents" "Molten Fury" "Wand Specialization"; do
  if grep -qF -- "$canary" "$TMP/0012.patch"; then
    echo "GATE FAIL: patch contaminated with a later patch's content: $canary" >&2; exit 1
  fi
done

cp "$TMP/0012.patch" "$OUT"
echo "OK: wrote $OUT ($(wc -l < "$OUT") lines)"
git -C "$AC" apply --stat "$OUT" | sed 's/^/    /'
