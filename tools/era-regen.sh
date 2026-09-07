#!/usr/bin/env bash
# era-regen.sh — regenerate EVERY era-talents artifact from the dataset YAMLs, in order, with
# one generation stamp. This is the ONLY supported way to ship a YAML change; partial manual
# regens are how the client and server ended up running different generations of the same spell.
#
# Legs: per-dataset era_talent/era_talent_rank SQL + custom spell_dbc/spell_proc SQL, the
# era_talent_meta stamp SQL, the addon Lua (build-addon.sh), and — when the MPQ tools and the
# staged patch-V exist — the merged client patch (merge-into-patch.sh embeds the same stamp).
#
# Usage: tools/era-regen.sh [--sync-fork]
#   --sync-fork  also rm -rf + cp -a the module into azerothcore-wotlk/modules/ (dev-box builds
#                compile THAT copy; without this a rebuild silently ships the old module).
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MOD="$ROOT/modules/mod-era-talents"
SQLDIR="$MOD/data/sql/world/base"
MANIFEST="$ROOT/era-data/datasets.txt"
# Pre-check field count over the WHOLE manifest before parsing a single line — matches the
# pytest helper's `assert len(parts) == 3`. Must run before the read loop below, not inside it,
# so a bad line anywhere aborts before any dataset is processed.
awk 'NF==0 { next } $1 ~ /^#/ { next } NF!=3 { print "BAD manifest line (need 3 fields): " $0; exit 1 }' \
  "$MANIFEST" || exit 1

DATASETS=()
declare -A DATA_SQL CUSTOM_SQL
# `|| [[ -n "$rel" ]]` is the standard idiom for processing a final line with no trailing
# newline — plain `read` returns non-zero for it, so a bare `while read; do ... done` silently
# DROPS an unterminated last line (era-regen.sh would then under-generate while era_audit.py/
# pytest, which both use splitlines(), still see every dataset). Deliberately NOT `IFS= read`:
# that disables whitespace splitting entirely, collapsing all 3 fields into $rel.
while read -r rel data custom || [[ -n "$rel" ]]; do
  [[ -z "$rel" || "$rel" == \#* ]] && continue
  [[ -n "${DATA_SQL[$rel]:-}" ]] && { echo "DUPLICATE manifest entry: $rel" >&2; exit 1; }
  DATASETS+=("$ROOT/era-data/$rel")
  DATA_SQL["$rel"]="$data"
  CUSTOM_SQL["$rel"]="$custom"
done < "$MANIFEST"

for ds in "${DATASETS[@]}"; do
  rel="${ds#"$ROOT/era-data/"}"
  echo "==> $rel: era_talent SQL + custom-spell SQL"
  uv run --with pyyaml python "$ROOT/tools/gen_era_talents.py" --sql "$ds" \
    -o "$SQLDIR/${DATA_SQL[$rel]}"
  uv run --with pyyaml python "$ROOT/tools/gen_era_talents.py" --custom-sql "$ds" \
    -o "$SQLDIR/${CUSTOM_SQL[$rel]}"
done

echo "==> band-allowlist header (EraBandAllowlist.gen.h)"
uv run --with pyyaml python "$ROOT/tools/gen_band_allowlist.py" "$ROOT/era-data/band-allowlist.yaml" \
  -o "$MOD/src/EraBandAllowlist.gen.h"

echo "==> era_talent_meta generation stamp"
uv run --with pyyaml python "$ROOT/tools/gen_era_talents.py" \
  --meta-sql "${DATASETS[@]}" -o "$SQLDIR/2026_08_14_11_era_talent_meta.sql"

echo "==> addon Lua"
bash "$ROOT/client-addons-src/EraTalents/build-addon.sh"

TARGET="$ROOT/client-addons/_data-patches/patch-V.mpq"
if [[ -x "$ROOT/client-patch/mpqpack" && -x "$ROOT/client-patch/mpqread" && -f "$TARGET" ]]; then
  echo "==> client patch (merge into staged patch-V.mpq, in place)"
  bash "$ROOT/client-patch/merge-into-patch.sh" "$TARGET" "$ROOT/client-patch/out/patch-V.mpq"
  cp -f "$ROOT/client-patch/out/patch-V.mpq" "$TARGET"
else
  echo "NOTE: mpq tools or staged patch-V.mpq missing — client patch NOT rebuilt"
fi

if [[ "${1:-}" == "--sync-fork" ]]; then
  if [[ -d "$ROOT/azerothcore-wotlk/modules" ]]; then
    echo "==> syncing module into azerothcore-wotlk/modules/ (the copy the build compiles)"
    rm -rf "$ROOT/azerothcore-wotlk/modules/mod-era-talents"
    cp -a "$MOD" "$ROOT/azerothcore-wotlk/modules/"
  else
    echo "NOTE: azerothcore-wotlk/modules missing — fork sync skipped"
  fi
fi

STAMP="$(uv run --with pyyaml python - "$ROOT/tools" "${DATASETS[@]}" <<'PY'
import sys
sys.path.insert(0, sys.argv[1])
import gen_era_talents as G
print(G.generation_stamp(sys.argv[2:]))
PY
)"
echo "==> DONE. Generation stamp: $STAMP"
echo "    Server: applied on next db-import + worldserver restart."
echo "    Client: install client-addons/_data-patches/patch-V.mpq to each client's Data/ and relaunch."
