#!/usr/bin/env bash
# Build the era-talents client display patch (patch-4.MPQ).
#
# WHY: a few era-talent auras are CUSTOM spells the 3.3.5a client's Spell.dbc doesn't have (e.g. the
# Improved Blizzard "Chilled" debuff, 932980-2). The effect works server-side, but the client can't
# render an icon/name for a spell it doesn't know. This patch ADDS just those custom VISIBLE spells
# to the client Spell.dbc so their debuff/buff shows on the unit frames. It does NOT recreate any
# base player spell (that's the shelved era-wow approach) — only the handful marked `client:` in the
# datasets.
#
# Players drop the resulting patch-4.MPQ into their WoW 3.3.5a Data/ folder. The server needs no
# change (its spell_dbc already carries these ids, from the same generator).
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MPQPACK="$ROOT/client-patch/mpqpack"
BASE_DBC="$ROOT/azerothcore-wotlk/ip-dbc/Spell.dbc"   # pristine 3.3.5a client Spell.dbc (from the fork's ip-dbc)
DATASETS=("$ROOT/era-data/vanilla/mage.yaml" "$ROOT/era-data/vanilla/priest.yaml")  # add a class's yaml here as classes are authored
OUT_DIR="$ROOT/client-patch/out"
OUT_MPQ="$OUT_DIR/patch-4.MPQ"

[ -x "$MPQPACK" ] || { echo "ERROR: $MPQPACK missing/not executable" >&2; exit 1; }
[ -f "$BASE_DBC" ] || { echo "ERROR: base Spell.dbc missing at $BASE_DBC" >&2; exit 1; }

STAGE="$(mktemp -d)"; trap 'rm -rf "$STAGE"' EXIT
mkdir -p "$STAGE/DBFilesClient" "$OUT_DIR"

echo "==> Generating client Spell.dbc (base + custom era-talent visible rows)"
uv run --with pyyaml python "$ROOT/tools/build_client_dbc.py" \
  --dataset "${DATASETS[@]}" --base-dbc "$BASE_DBC" --out "$STAGE/DBFilesClient/Spell.dbc"

echo "==> Generating client SkillLineAbility.dbc (base + custom castable tab mappings)"
uv run --with pyyaml python "$ROOT/tools/build_client_skilllineability.py" \
  --dataset "${DATASETS[@]}" --base-dbc "$ROOT/azerothcore-wotlk/ip-dbc/SkillLineAbility.dbc" \
  --out "$STAGE/DBFilesClient/SkillLineAbility.dbc"

echo "==> Packing $OUT_MPQ"
rm -f "$OUT_MPQ"
"$MPQPACK" "$OUT_MPQ" 'DBFilesClient\Spell.dbc' "$STAGE/DBFilesClient/Spell.dbc"
"$MPQPACK" "$OUT_MPQ" 'DBFilesClient\SkillLineAbility.dbc' "$STAGE/DBFilesClient/SkillLineAbility.dbc"
"$MPQPACK" verify "$OUT_MPQ" 'DBFilesClient\Spell.dbc'

echo "==> Done: $OUT_MPQ"
echo "    Install: copy it to 'World of Warcraft/Data/patch-4.MPQ' on each client, then restart WoW."
