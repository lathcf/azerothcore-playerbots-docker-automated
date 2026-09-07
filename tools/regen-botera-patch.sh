#!/usr/bin/env bash
# regen-botera-patch.sh — re-cut patches/0020-playerbot-factory-era-talents.patch from the
# fork working tree at azerothcore-wotlk/modules/mod-playerbots.
#
# FOUR files, and one of them is SHARED — so this is the BASELINE-AWARE recipe:
#   * src/Bot/Factory/PlayerbotFactory.cpp — touched by NO other patch: baseline = pristine.
#   * src/Ai/Base/Actions/AutoMaintenanceOnLevelupAction.cpp — touched by NO other patch: baseline
#     = pristine. Added 2026-09-06: this AI-tick action calls InitAvailableSpells() AFTER the whole
#     factory pass, so it is the last site to re-teach era-illegal trainer spells to a bot.
#   * src/Bot/Factory/AiFactory.cpp — ALSO touched by 0003 + 0004: baseline = pristine + those
#     two patches' AiFactory hunks. A pristine-baseline diff here would EMBED the WG hunks into
#     0020 (the regen-contamination trap). NB 0014 (Sunwell) was listed here until 2026-09-06 and
#     was WRONG: it has ZERO AiFactory.cpp hunks (`grep '^diff --git' patches/0014*.patch` shows
#     no such file — its only 'AiFactory' hit is an #include context line), so the apply below was
#     a silent no-op. Verify a candidate with the file-scoped diff list, not a loose grep.
#
# Baselines are reconstructed from pristine + the earlier patches, applied FILE-SCOPED from
# the fork root (exactly how setup.sh's apply_patches applies them). The --src/--dst prefix
# rewrite makes the output apply from the FORK ROOT like every other mod-playerbots patch.
#
# LITERAL PATHS ONLY on every git/cp line — zsh does not word-split an unquoted $var.
# Re-runnable from the edited working-tree state; leaves the worktree at 0020, index pristine.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
AC="$ROOT/azerothcore-wotlk"
PB="$AC/modules/mod-playerbots"
OUT="$ROOT/patches/0020-playerbot-factory-era-talents.patch"
REL1="src/Bot/Factory/PlayerbotFactory.cpp"
REL2="src/Bot/Factory/AiFactory.cpp"
REL3="src/Mgr/Item/RandomItemMgr.cpp"   # Phase 3: touched by NO other patch -> pristine baseline
REL4="src/Ai/Base/Actions/AutoMaintenanceOnLevelupAction.cpp"   # touched by NO other patch -> pristine

[[ -d "$PB/.git" ]] || { echo "ERROR: $PB is not a git clone (run setup.sh first)" >&2; exit 1; }

# The 0020 edits must actually be present in the worktree.
grep -q "EraTalentBots_FactoryReconcile" "$PB/$REL1" \
  || { echo "ERROR: $REL1 has no era hook — 0020 not applied to the worktree?" >&2; exit 1; }
grep -q "EraTalentBots_SpecTabs" "$PB/$REL2" \
  || { echo "ERROR: $REL2 has no spec-tab hook — 0020 not applied to the worktree?" >&2; exit 1; }
grep -q "maxReqLevel" "$PB/$REL3" \
  || { echo "ERROR: $REL3 GetRandomPotion has no era filter — Task 5 edits not applied to the worktree?" >&2; exit 1; }
grep -q "IsEraLegalConsumable" "$PB/$REL1" \
  || { echo "ERROR: $REL1 has no IsEraLegalConsumable — Task 5 edits not applied?" >&2; exit 1; }
grep -q "EraTalentBots_PostTrainerWalk" "$PB/$REL4" \
  || { echo "ERROR: $REL4 has no post-trainer-walk hook — 0020 not applied to the worktree?" >&2; exit 1; }

TMP="$(mktemp -d)"
restore() {
  # Always leave the fork exactly as found: 0020 worktree state, pristine index.
  if [[ -f "$TMP/PlayerbotFactory.cpp" ]]; then cp "$TMP/PlayerbotFactory.cpp" "$PB/$REL1"; fi
  if [[ -f "$TMP/AiFactory.cpp" ]]; then cp "$TMP/AiFactory.cpp" "$PB/$REL2"; fi
  if [[ -f "$TMP/RandomItemMgr.cpp" ]]; then cp "$TMP/RandomItemMgr.cpp" "$PB/$REL3"; fi
  if [[ -f "$TMP/AutoMaintenanceOnLevelupAction.cpp" ]]; then cp "$TMP/AutoMaintenanceOnLevelupAction.cpp" "$PB/$REL4"; fi
  git -C "$PB" reset -q -- "$REL1" "$REL2" "$REL3" "$REL4" 2>/dev/null || true
  rm -rf "$TMP"
}
trap restore EXIT

# 1. Save the current (0020-bearing) copies.
cp "$PB/$REL1" "$TMP/PlayerbotFactory.cpp"
cp "$PB/$REL2" "$TMP/AiFactory.cpp"
cp "$PB/$REL3" "$TMP/RandomItemMgr.cpp"
cp "$PB/$REL4" "$TMP/AutoMaintenanceOnLevelupAction.cpp"

# 2. Rebuild the baselines: pristine, then the earlier patches' hunks for these files only.
git -C "$PB" checkout -- "$REL1" "$REL2" "$REL3" "$REL4"
# AiFactory.cpp baseline needs 0003 + 0004 (file-scoped; PlayerbotFactory.cpp matches nothing in
# them, so the same two commands double as its no-op). If a future patch also edits either file,
# add it here or the gates below abort.
git -C "$AC" apply --include="modules/mod-playerbots/src/Bot/Factory/AiFactory.cpp" "$ROOT/patches/0003-playerbot-wintergrasp.patch"
git -C "$AC" apply --include="modules/mod-playerbots/src/Bot/Factory/AiFactory.cpp" "$ROOT/patches/0004-playerbot-wintergrasp-siege.patch"

# 3. Baseline gates: earlier hunks present, 0020 hunks absent. (if-form, not `grep && fail` —
#    the && shape is the set -e trap the other regen scripts warn about.)
grep -q "wg siege" "$PB/$REL2" \
  || { echo "GATE FAIL: AiFactory baseline is missing the 0004 hunk" >&2; exit 1; }
if grep -q "EraTalentBots_SpecTabs" "$PB/$REL2"; then
  echo "GATE FAIL: AiFactory baseline still carries the 0020 hunk" >&2; exit 1
fi
if grep -q "EraTalentBots_FactoryReconcile" "$PB/$REL1"; then
  echo "GATE FAIL: PlayerbotFactory baseline still carries the 0020 hunk" >&2; exit 1
fi
# RandomItemMgr baseline is pristine: the 0020 tell is the per-band RequiredLevel ceiling
# (maxReqLevel) inside GetRandomPotion, absent from the pristine cache-keyed-on-req-only version.
if awk '/GetRandomPotion/,/^}/' "$PB/$REL3" | grep -q "maxReqLevel"; then
  echo "GATE FAIL: RandomItemMgr baseline still carries the 0020 potion hunk" >&2; exit 1
fi
if grep -q "EraTalentBots_PostTrainerWalk" "$PB/$REL4"; then
  echo "GATE FAIL: AutoMaintenanceOnLevelupAction baseline still carries the 0020 hunk" >&2; exit 1
fi

# 4. Stage the baselines (index = baseline), then restore the 0020 worktree state.
git -C "$PB" add -- "$REL1" "$REL2" "$REL3" "$REL4"
cp "$TMP/PlayerbotFactory.cpp" "$PB/$REL1"
cp "$TMP/AiFactory.cpp" "$PB/$REL2"
cp "$TMP/RandomItemMgr.cpp" "$PB/$REL3"
cp "$TMP/AutoMaintenanceOnLevelupAction.cpp" "$PB/$REL4"

# 5. Index(baseline) vs worktree(0020) == exactly the 0020 hunks, on top of the full stack.
git -C "$PB" diff \
  --src-prefix=a/modules/mod-playerbots/ \
  --dst-prefix=b/modules/mod-playerbots/ \
  -- "$REL1" "$REL2" "$REL3" "$REL4" \
  > "$TMP/0020.patch"

# (index/worktree restoration happens in the EXIT trap)

# 6. Output gates.
[[ -s "$TMP/0020.patch" ]] || { echo "GATE FAIL: generated patch is empty" >&2; exit 1; }
[[ "$(grep -c '^diff --git' "$TMP/0020.patch")" -eq 4 ]] \
  || { echo "GATE FAIL: expected exactly 4 files in the patch, got $(grep -c '^diff --git' "$TMP/0020.patch")" >&2; exit 1; }
for needle in "EraTalentBots_FactoryReconcile" "EraTalentBots_SpecTabs" "EraTalentBots_PostTrainerWalk" "randomClassSpecIndex" "IsEraLegalConsumable" "maxReqLevel" "EraGlyphGate_BotGlyphsAllowed" "Traveler's Backpack"; do
  grep -q -- "$needle" "$TMP/0020.patch" \
    || { echo "GATE FAIL: patch missing expected content: $needle" >&2; exit 1; }
done
if grep -q "wg siege" "$TMP/0020.patch"; then
  echo "GATE FAIL: patch contaminated with a 0004 hunk (baseline was wrong)" >&2; exit 1
fi

cp "$TMP/0020.patch" "$OUT"
echo "OK: wrote $OUT ($(wc -l < "$OUT") lines)"
git -C "$AC" apply --stat "$OUT" | sed 's/^/    /'
