#!/usr/bin/env bash
# regen-sentry-unsummon-patch.sh — re-cut patches/0024-core-sentry-era-unsummon.patch from the
# fork working tree at azerothcore-wotlk.
#
# CORE-ONLY patch (the 0019 recipe): it touches src/server/game/Entities/Totem/Totem.cpp in the
# fork itself, so a plain `git -C "$AC" diff` with NO prefix rewriting is correct — the a/src/...
# paths already resolve from the fork root where apply_patches runs `git apply`. Do NOT copy the
# --src-prefix/--dst-prefix flags from the module regen scripts.
#
# NOT baseline-aware: no other patch touches Totem.cpp (verified at authoring time:
#   grep -l "Totem.cpp" patches/*.patch  => no match).
# If a FUTURE patch starts touching Totem.cpp, make this baseline-aware (reverse the other patch
# out before diffing) or it will silently embed the other patch's hunks.
#
# WHAT 0024 IS: Totem::UnSummon clears the Sentry camera-bind buff with a HARDCODED 6495
# (TotemSpellIds::SentryTotemSpell). The mod-era-talents Sentry clone 947260 summons the same
# creature entry 3968 but applies its own owner-side buff, which that call never names — so an
# EARLY unsummon (totem killed, replaced, Totemic Recall) left the buff running out its 5-minute
# duration and spell_sha_sentry_totem's bind-sight teardown fired late. The patch adds
# owner->RemoveAurasDueToSpell(947260) inside the same entry-gated block. Inert when the aura is
# absent (any non-era character). Closes shaman-totems-tbc.yaml accepted_gaps id 12.
#
# LITERAL PATHS ONLY on every git line — the shell does not word-split an unquoted $var.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
AC="$ROOT/azerothcore-wotlk"
OUT="$ROOT/patches/0024-core-sentry-era-unsummon.patch"

[[ -d "$AC/.git" ]] || { echo "ERROR: $AC is not a git clone (run setup.sh first)" >&2; exit 1; }
[[ -f "$AC/src/server/game/Entities/Totem/Totem.cpp" ]] \
  || { echo "ERROR: missing core Totem.cpp in the fork worktree" >&2; exit 1; }

grep -q "era-talents (patch 0024)" "$AC/src/server/game/Entities/Totem/Totem.cpp" \
  || { echo "ERROR: Totem.cpp has no era-Sentry hunk — 0024 not applied to the worktree?" >&2; exit 1; }

git -C "$AC" diff -- src/server/game/Entities/Totem/Totem.cpp > "$OUT"

[[ -s "$OUT" ]] || { echo "GATE FAIL: generated patch is empty" >&2; exit 1; }
[[ "$(grep -c '^diff --git' "$OUT")" -eq 1 ]] || { echo "GATE FAIL: expected exactly 1 file in the patch" >&2; exit 1; }
grep -q "b/src/server/game/Entities/Totem/Totem.cpp" "$OUT" \
  || { echo "GATE FAIL: patch path is not fork-root relative — did you add --dst-prefix?" >&2; exit 1; }
for needle in "947260" "931390" "SENTRY_TOTEM_ENTRY" "RemoveAurasDueToSpell"; do
  grep -q "$needle" "$OUT" || { echo "GATE FAIL: patch missing expected content: $needle" >&2; exit 1; }
done

echo "OK: wrote $OUT ($(wc -l < "$OUT") lines)"
git -C "$AC" apply --stat "$OUT" | sed 's/^/    /'
