#!/usr/bin/env bash
# regen-eraai-patch.sh — re-cut patches/0021-playerbot-era-ai.patch from the fork working
# tree at azerothcore-wotlk/modules/mod-playerbots.
#
# 0021 = era-aware bot AI: routes the AI's hardcoded stock-spell-id checks (shaman totem
# subsystem, druid Thick Hide bear discriminator + Omen of Clarity, paladin judgement
# fallback + Blessing of Sanctuary / Improved-blessing checks, priest Vampiric Embrace
# targeting, warrior Commanding Presence + Poleaxe Specialization) through mod-era-talents'
# EraTalentBots_ResolveSpellId bridge.
#
# BASELINE-AWARE for ONE file: src/Bot/PlayerbotAI.cpp is also touched by patches
# 0003/0004/0011/0014/0016 (verified 2026-08-26; 0007 only mentions it in a comment), so its
# baseline = pristine + those five patches' PlayerbotAI hunks, applied file-scoped in numeric
# order. Every other 0021 file is touched by NO other patch: baseline = pristine.
#
# LITERAL PATHS ONLY on every git/cp line — zsh does not word-split an unquoted $var.
# Re-runnable from the edited working-tree state; leaves the worktree at 0021, index pristine.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
AC="$ROOT/azerothcore-wotlk"
PB="$AC/modules/mod-playerbots"
OUT="$ROOT/patches/0021-playerbot-era-ai.patch"

[[ -d "$PB/.git" ]] || { echo "ERROR: $PB is not a git clone (run setup.sh first)" >&2; exit 1; }

# The 0021 edits must actually be present (spot-check one per class cluster).
grep -q "EraTalentBots_ResolveSpellId" "$PB/src/Ai/Class/Shaman/ShamanTriggers.cpp" \
  || { echo "ERROR: ShamanTriggers.cpp has no era hook — 0021 not applied to the worktree?" >&2; exit 1; }
grep -q "EraTalentBots_ResolveSpellId" "$PB/src/Bot/PlayerbotAI.cpp" \
  || { echo "ERROR: PlayerbotAI.cpp has no era hook — 0021 not applied to the worktree?" >&2; exit 1; }
grep -q "EraTalentBots_ResolveSpellId" "$PB/src/Ai/Class/Priest/PriestTriggers.h" \
  || { echo "ERROR: PriestTriggers.h has no era hook — 0021 not applied to the worktree?" >&2; exit 1; }
grep -q "EraTalentBots_ResolveSpellId" "$PB/src/Ai/Class/Warrior/WarriorTriggers.cpp" \
  || { echo "ERROR: WarriorTriggers.cpp has no era hook — 0021 not applied to the worktree?" >&2; exit 1; }

TMP="$(mktemp -d)"
restore() {
  # Always leave the fork exactly as found: 0021 worktree state, pristine index.
  if [[ -d "$TMP/save" ]]; then cp -a "$TMP/save/." "$PB/"; fi
  git -C "$PB" reset -q -- \
    src/Bot/PlayerbotAI.cpp \
    src/Ai/Base/Actions/LfgActions.cpp \
    src/Ai/Base/Value/SpellIdValue.cpp \
    src/Ai/Base/Actions/WorldBuffAction.cpp \
    src/Ai/Class/Shaman/ShamanTriggers.cpp \
    src/Ai/Class/Shaman/ShamanActions.cpp \
    src/Ai/Class/Shaman/Strategy/TotemsShamanStrategy.cpp \
    src/Ai/Class/Druid/DruidTriggers.h \
    src/Ai/Class/Druid/Strategy/GenericDruidStrategy.cpp \
    src/Ai/Class/Druid/Strategy/FeralDruidStrategy.cpp \
    src/Ai/Class/Paladin/Actions/PaladinActions.cpp \
    src/Ai/Class/Paladin/Actions/PaladinGreaterBlessingAction.cpp \
    src/Ai/Class/Paladin/Strategy/GenericPaladinStrategyActionNodeFactory.h \
    src/Ai/Class/Priest/PriestActions.h \
    src/Ai/Class/Priest/PriestTriggers.h \
    src/Ai/Class/Priest/Strategy/ShadowPriestStrategy.cpp \
    src/Ai/Class/Warrior/WarriorTriggers.cpp \
    src/Mgr/Item/StatsWeightCalculator.cpp \
    2>/dev/null || true
  rm -rf "$TMP"
}
trap restore EXIT

# 1. Save the current (0021-bearing) copies, preserving relative layout.
mkdir -p "$TMP/save"
tar -C "$PB" -cf - \
  src/Bot/PlayerbotAI.cpp \
  src/Ai/Base/Actions/LfgActions.cpp \
  src/Ai/Base/Value/SpellIdValue.cpp \
  src/Ai/Base/Actions/WorldBuffAction.cpp \
  src/Ai/Class/Shaman/ShamanTriggers.cpp \
  src/Ai/Class/Shaman/ShamanActions.cpp \
  src/Ai/Class/Shaman/Strategy/TotemsShamanStrategy.cpp \
  src/Ai/Class/Druid/DruidTriggers.h \
  src/Ai/Class/Druid/Strategy/GenericDruidStrategy.cpp \
  src/Ai/Class/Druid/Strategy/FeralDruidStrategy.cpp \
  src/Ai/Class/Paladin/Actions/PaladinActions.cpp \
  src/Ai/Class/Paladin/Actions/PaladinGreaterBlessingAction.cpp \
  src/Ai/Class/Paladin/Strategy/GenericPaladinStrategyActionNodeFactory.h \
  src/Ai/Class/Priest/PriestActions.h \
  src/Ai/Class/Priest/PriestTriggers.h \
  src/Ai/Class/Priest/Strategy/ShadowPriestStrategy.cpp \
  src/Ai/Class/Warrior/WarriorTriggers.cpp \
  src/Mgr/Item/StatsWeightCalculator.cpp \
  | tar -C "$TMP/save" -xf -

# 2. Rebuild the baselines: pristine everywhere...
git -C "$PB" checkout -- \
  src/Bot/PlayerbotAI.cpp \
  src/Ai/Base/Actions/LfgActions.cpp \
  src/Ai/Base/Value/SpellIdValue.cpp \
  src/Ai/Base/Actions/WorldBuffAction.cpp \
  src/Ai/Class/Shaman/ShamanTriggers.cpp \
  src/Ai/Class/Shaman/ShamanActions.cpp \
  src/Ai/Class/Shaman/Strategy/TotemsShamanStrategy.cpp \
  src/Ai/Class/Druid/DruidTriggers.h \
  src/Ai/Class/Druid/Strategy/GenericDruidStrategy.cpp \
  src/Ai/Class/Druid/Strategy/FeralDruidStrategy.cpp \
  src/Ai/Class/Paladin/Actions/PaladinActions.cpp \
  src/Ai/Class/Paladin/Actions/PaladinGreaterBlessingAction.cpp \
  src/Ai/Class/Paladin/Strategy/GenericPaladinStrategyActionNodeFactory.h \
  src/Ai/Class/Priest/PriestActions.h \
  src/Ai/Class/Priest/PriestTriggers.h \
  src/Ai/Class/Priest/Strategy/ShadowPriestStrategy.cpp \
  src/Ai/Class/Warrior/WarriorTriggers.cpp \
  src/Mgr/Item/StatsWeightCalculator.cpp

# ...then the earlier patches' PlayerbotAI.cpp hunks, file-scoped, in numeric order.
git -C "$AC" apply --include="modules/mod-playerbots/src/Bot/PlayerbotAI.cpp" "$ROOT/patches/0003-playerbot-wintergrasp.patch"
git -C "$AC" apply --include="modules/mod-playerbots/src/Bot/PlayerbotAI.cpp" "$ROOT/patches/0004-playerbot-wintergrasp-siege.patch"
git -C "$AC" apply --include="modules/mod-playerbots/src/Bot/PlayerbotAI.cpp" "$ROOT/patches/0011-playerbot-perfmon-hotpath.patch"
git -C "$AC" apply --include="modules/mod-playerbots/src/Bot/PlayerbotAI.cpp" "$ROOT/patches/0014-playerbot-sunwell.patch"
git -C "$AC" apply --include="modules/mod-playerbots/src/Bot/PlayerbotAI.cpp" "$ROOT/patches/0016-playerbot-aq40-twins.patch"

# 3. Baseline gates (if-form, not `grep && fail` — the set -e trap the other regens warn about).
grep -q "perfMonEnabled" "$PB/src/Bot/PlayerbotAI.cpp" \
  || { echo "GATE FAIL: PlayerbotAI baseline is missing the 0011 hunk" >&2; exit 1; }
if grep -q "EraTalentBots_ResolveSpellId" "$PB/src/Bot/PlayerbotAI.cpp"; then
  echo "GATE FAIL: PlayerbotAI baseline still carries the 0021 hunk" >&2; exit 1
fi
if grep -q "EraTalentBots_ResolveSpellId" "$PB/src/Ai/Class/Shaman/ShamanTriggers.cpp"; then
  echo "GATE FAIL: ShamanTriggers baseline still carries the 0021 hunk" >&2; exit 1
fi

# 4. Stage the baselines (index = baseline), then restore the 0021 worktree state.
git -C "$PB" add -- \
  src/Bot/PlayerbotAI.cpp \
  src/Ai/Base/Actions/LfgActions.cpp \
  src/Ai/Base/Value/SpellIdValue.cpp \
  src/Ai/Base/Actions/WorldBuffAction.cpp \
  src/Ai/Class/Shaman/ShamanTriggers.cpp \
  src/Ai/Class/Shaman/ShamanActions.cpp \
  src/Ai/Class/Shaman/Strategy/TotemsShamanStrategy.cpp \
  src/Ai/Class/Druid/DruidTriggers.h \
  src/Ai/Class/Druid/Strategy/GenericDruidStrategy.cpp \
  src/Ai/Class/Druid/Strategy/FeralDruidStrategy.cpp \
  src/Ai/Class/Paladin/Actions/PaladinActions.cpp \
  src/Ai/Class/Paladin/Actions/PaladinGreaterBlessingAction.cpp \
  src/Ai/Class/Paladin/Strategy/GenericPaladinStrategyActionNodeFactory.h \
  src/Ai/Class/Priest/PriestActions.h \
  src/Ai/Class/Priest/PriestTriggers.h \
  src/Ai/Class/Priest/Strategy/ShadowPriestStrategy.cpp \
  src/Ai/Class/Warrior/WarriorTriggers.cpp \
  src/Mgr/Item/StatsWeightCalculator.cpp
cp -a "$TMP/save/." "$PB/"

# 5. Index(baseline) vs worktree(0021) == exactly the 0021 hunks, on top of the full stack.
git -C "$PB" diff \
  --src-prefix=a/modules/mod-playerbots/ \
  --dst-prefix=b/modules/mod-playerbots/ \
  -- \
  src/Bot/PlayerbotAI.cpp \
  src/Ai/Base/Actions/LfgActions.cpp \
  src/Ai/Base/Value/SpellIdValue.cpp \
  src/Ai/Base/Actions/WorldBuffAction.cpp \
  src/Ai/Class/Shaman/ShamanTriggers.cpp \
  src/Ai/Class/Shaman/ShamanActions.cpp \
  src/Ai/Class/Shaman/Strategy/TotemsShamanStrategy.cpp \
  src/Ai/Class/Druid/DruidTriggers.h \
  src/Ai/Class/Druid/Strategy/GenericDruidStrategy.cpp \
  src/Ai/Class/Druid/Strategy/FeralDruidStrategy.cpp \
  src/Ai/Class/Paladin/Actions/PaladinActions.cpp \
  src/Ai/Class/Paladin/Actions/PaladinGreaterBlessingAction.cpp \
  src/Ai/Class/Paladin/Strategy/GenericPaladinStrategyActionNodeFactory.h \
  src/Ai/Class/Priest/PriestActions.h \
  src/Ai/Class/Priest/PriestTriggers.h \
  src/Ai/Class/Priest/Strategy/ShadowPriestStrategy.cpp \
  src/Ai/Class/Warrior/WarriorTriggers.cpp \
  src/Mgr/Item/StatsWeightCalculator.cpp \
  > "$TMP/0021.patch"

# (index/worktree restoration happens in the EXIT trap)

# 6. Output gates.
[[ -s "$TMP/0021.patch" ]] || { echo "GATE FAIL: generated patch is empty" >&2; exit 1; }
[[ "$(grep -c '^diff --git' "$TMP/0021.patch")" -eq 18 ]] \
  || { echo "GATE FAIL: expected exactly 18 files in the patch, got $(grep -c '^diff --git' "$TMP/0021.patch")" >&2; exit 1; }
grep -q "EraTalentBots_ResolveSpellId" "$TMP/0021.patch" \
  || { echo "GATE FAIL: patch missing the resolver bridge" >&2; exit 1; }
if grep -q "perfMonEnabled" "$TMP/0021.patch"; then
  echo "GATE FAIL: patch contaminated with a 0011 hunk (baseline was wrong)" >&2; exit 1
fi
if grep -q "wg siege" "$TMP/0021.patch"; then
  echo "GATE FAIL: patch contaminated with a 0004 hunk (baseline was wrong)" >&2; exit 1
fi

cp "$TMP/0021.patch" "$OUT"
echo "OK: wrote $OUT ($(wc -l < "$OUT") lines)"
git -C "$AC" apply --stat "$OUT" | sed 's/^/    /'
