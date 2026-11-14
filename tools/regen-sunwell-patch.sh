#!/usr/bin/env bash
# regen-sunwell-patch.sh — re-cut patches/0014-playerbot-sunwell.patch from the fork
# working tree at azerothcore-wotlk/modules/mod-playerbots.
#
# BASELINE-AWARE for src/Bot/PlayerbotAI.cpp: that file is also modified by patches
# 0003 (wg invite) and 0004 (wg siege), which are applied
# UNSTAGED — a naive `git diff` would embed their hunks into 0014. For that one file we
# diff against a baseline of pristine+0003+0004 built in the git index (same
# mechanism as regen-arena-patch.sh). The SWP/ dir is new (rides along via `add -N`) and
# the three other wiring files are touched by no other patch.
#
# LITERAL PATHS ONLY on every git/cp line — zsh does not word-split unquoted vars
# (a `$FILES` loop silently copies nothing and cuts an empty patch).
#
# Re-runnable; leaves the fork tree and index exactly as found.

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
AC="$ROOT/azerothcore-wotlk"
PB="$AC/modules/mod-playerbots"
OUT="$ROOT/patches/0014-playerbot-sunwell.patch"
P0003="$ROOT/patches/0003-playerbot-wintergrasp.patch"
P0004="$ROOT/patches/0004-playerbot-wintergrasp-siege.patch"

[[ -d "$PB/.git" ]] || { echo "ERROR: $PB is not a git clone (run setup.sh first)" >&2; exit 1; }
[[ -f "$P0003" && -f "$P0004" ]] \
  || { echo "ERROR: baseline patches 0003/0004 missing from patches/" >&2; exit 1; }

# The SWP work must be present in the worktree.
[[ -d "$PB/src/Ai/Raid/SWP" ]] \
  || { echo "ERROR: src/Ai/Raid/SWP missing — sunwell edits not in the fork worktree" >&2; exit 1; }
grep -q 'case 580' "$PB/src/Bot/PlayerbotAI.cpp" \
  || { echo "ERROR: PlayerbotAI.cpp has no case 580 — sunwell wiring not applied?" >&2; exit 1; }

TMP="$(mktemp -d)"
restore() {
  if [[ -f "$TMP/PlayerbotAI.cpp" ]]; then cp "$TMP/PlayerbotAI.cpp" "$PB/src/Bot/PlayerbotAI.cpp"; fi
  git -C "$PB" reset -q -- \
    src/Bot/PlayerbotAI.cpp \
    src/Ai/Raid/SWP \
    src/Ai/Raid/RaidStrategyContext.h \
    src/Bot/Engine/BuildSharedTriggerContexts.cpp \
    src/Bot/Engine/BuildSharedActionContexts.cpp 2>/dev/null || true
  rm -rf "$TMP"
}
trap restore EXIT

# 1. Save the current (0014-bearing) copy of the shared file.
cp "$PB/src/Bot/PlayerbotAI.cpp" "$TMP/PlayerbotAI.cpp"

# 2. Pristine, then rebuild the baseline for the shared file: 0003 -> 0004,
#    each restricted to its PlayerbotAI.cpp hunks.
git -C "$PB" checkout -- src/Bot/PlayerbotAI.cpp
git -C "$AC" apply --include=modules/mod-playerbots/src/Bot/PlayerbotAI.cpp "$P0003"
git -C "$AC" apply --include=modules/mod-playerbots/src/Bot/PlayerbotAI.cpp "$P0004"

# 3. Stage the baseline (index = pristine+0003+0004 for the shared file).
git -C "$PB" add -- src/Bot/PlayerbotAI.cpp

# 4. Restore the 0014 worktree state; mark the new SWP dir intent-to-add.
cp "$TMP/PlayerbotAI.cpp" "$PB/src/Bot/PlayerbotAI.cpp"
git -C "$PB" add -N -- src/Ai/Raid/SWP

# 5. Index(baseline) vs worktree(0014) == exactly the 0014 hunks.
git -C "$PB" diff \
  --src-prefix=a/modules/mod-playerbots/ \
  --dst-prefix=b/modules/mod-playerbots/ \
  -- \
  src/Ai/Raid/SWP \
  src/Ai/Raid/RaidStrategyContext.h \
  src/Bot/Engine/BuildSharedTriggerContexts.cpp \
  src/Bot/Engine/BuildSharedActionContexts.cpp \
  src/Bot/PlayerbotAI.cpp \
  > "$TMP/0014.patch"

# 6. Gates — a bad patch plus a setup.sh reset would orphan the work.
[[ -s "$TMP/0014.patch" ]] || { echo "GATE FAIL: generated patch is empty" >&2; exit 1; }
for needle in "RaidSunwellPlateauStrategy" "kalecgos heal kalec" "case 580" \
              "kalecgos enter spectral rift" "KalecgosDpsBalanceMultiplier" \
              "brutallus taunt boss" "GetBrutallusStation" \
              "GetBrutallusTankRole" "MEMBER_FLAG_MAINASSIST" "anchorSet" \
              "brutallus purge burn" "burnBearing" \
              "felmyst leave fog lane" "FELMYST_LANE_LETHAL_WEST" \
              "IsFelmystAirborne" "GetFelmystActiveLane" "IsInFelmystFogLane" \
              "felmyst flee encapsulate" "FelmystShouldFleeEncapsulate" \
              "felmyst mass dispel gas nova" "SPELL_MASS_DISPEL" \
              "felmyst attack charmed raider" "FindFelmystCharmedRaider" "FELMYST_REFUGE_STEPS" \
              "felmyst regroup" "GetFelmystRaidCentroid" "FELMYST_VAPOR_KITE_RANGE" \
              "twins flee conflagration" "FindTwinsConflagrationTarget" \
              "twins clear conflagration" "TWINS_CONFLAG_RADIUS" \
              "twins focus sacrolash" "NPC_SACROLASH" "SPELL_DARK_TOUCHED" \
              "twins tank alythess" "twins blaze footwork" "GetTwinsBlazeStation" \
              "twins seek shadow blades" "TWINS_SHADOW_SEEK_RANGE" \
              "twins dispel pyrogenics" "SPELL_PYROGENICS" \
              "TwinsShouldSuppressGenericMovement" "TwinsMovementMultiplier" \
              "twins relief taunt sacrolash" "TwinsShouldReliefTaunt" \
              "SPELL_CONFOUNDING_BLOW" "SacrolashRelief" "GetTwinsTankRole" \
              "twins pull sacrolash back" "TWINS_LEASH_RANGE" \
              "twins stack on alythess" "TwinsIsAlythessSoloPhase" "TWINS_STACK_RANGE" \
              "muru clear darkness" "MURU_DARKNESS_RADIUS" "SPELL_MURU_DARKNESS_PRE" \
              "muru dispel dark fiend" "FindMuruDarkFiend" "SPELL_DARK_FIEND_APPEARANCE" \
              "SwpOffensiveDispelSpell" "muru clear void zone" "NPC_ENTROPIUS_ZONE" \
              "muru flee singularity" "SPELL_BLACK_HOLE_PASSIVE" "MURU_SINGULARITY_FLEE" \
              "muru leave shadow pulse" "MURU_PULSE_RADIUS" "FindMuruVoidSentinel" \
              "muru tank add" "FindMuruUntankedAdd" "NPC_VOID_SENTINEL" \
              "muru focus target" "GetMuruFocusTarget" "GetMuruKillOrder" "MURU_FOCUS_HEALTH_BUCKET" "MURU_CLEAR_SECTORS" "BuildMuruEscapeSpot" \
              "MuruIsHandedOff" "MuruShouldSuppressGenericMovement" "MuruMovementMultiplier" \
              "MuruTargetHoldMultiplier" "DpsAssistAction" "AttackRtiTargetAction" \
              "kj clear armageddon" "FindKjArmageddonMarker" "NPC_KJ_ARMAGEDDON" \
              "kj drive drake" "GetKjDrake" "NPC_KJ_BLUE_DRAKE" \
              "SPELL_KJ_SHIELD_OF_THE_BLUE" "SPELL_KJ_VENGEANCE_BLUE" "AddSpellCooldown" \
              "kj stack for darkness" "KjDarknessWindow" "SPELL_KJ_DARKNESS" \
              "KJ_STACK_RING_OUTER" "KJ_SHIELD_LEAD_MS" \
              "kj claim orb" "FindKjEmpoweredOrb" "GO_KJ_ORB_1" "GO_FLAG_NOT_SELECTABLE" \
              "KjIsDesignatedPilot" "KjRaidHasDrake" "KjShouldRefreshPossession" \
              "kj quarantine fire bloom" "SPELL_KJ_FIRE_BLOOM" "KJ_FIRE_BLOOM_RANGE" \
              "kj hold anchor" "KJ_ANCHOR_X" "kj hold station" "KJ_RANGED_RING" \
              "kj focus target" "GetKjKillOrder" "NPC_KJ_SHIELD_ORB" "NPC_KJ_HAND" \
              "KjShouldSuppressGenericMovement" "KjMovementMultiplier" \
              "KjTargetHoldMultiplier" "SwpRankAmong" \
              "SWP_DATA_MURU" "GetBossState" "KjShouldDeferTargeting" "KjHazardMoveOwed"; do
  grep -q "$needle" "$TMP/0014.patch" || { echo "GATE FAIL: patch missing: $needle" >&2; exit 1; }
done
# Foreign content must never leak in (contamination canaries).
#
# "Aq40" is here because it ALREADY HAPPENED and broke a production update.sh. This patch modifies three
# SHARED wiring files - RaidStrategyContext.h, BuildSharedActionContexts.cpp, BuildSharedTriggerContexts.cpp
# - and a regen diffs whatever the worktree holds. An unrelated in-progress raid strategy had registered
# itself in those same files, so its 7 wiring lines rode into this patch while its SOURCES were captured by
# no patch at all. Result: setup.sh/update.sh reset the fork, cleaned the untracked sources away, applied
# 0014, and the build died on `fatal error: 'Aq40Strategy.h' file not found`.
#
# The general rule: any patch that touches a shared registration file needs a canary per foreign feature
# that could be registered there. Grep the shared files, not just this feature's own directory.
for banned in "SMSG_BATTLEFIELD_MGR_ENTRY_INVITE" "isBFGroup" "PerfMonitorOperation" "Aq40" "aq40"; do
  if grep -q "$banned" "$TMP/0014.patch"; then
    echo "GATE FAIL: patch contaminated with earlier-patch content: $banned" >&2; exit 1
  fi
done

cp "$TMP/0014.patch" "$OUT"
echo "OK: wrote $OUT ($(wc -l < "$OUT") lines)"
git -C "$PB" apply --stat "$OUT" | sed 's/^/    /'
