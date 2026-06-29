#!/usr/bin/env bash
# Download/refresh the client-side bot-management addons into ./client-addons/.
# These are CLIENT addons: copy each resulting folder into every player's
#   World of Warcraft/Interface/AddOns/
# directory (the 3.3.5a client), NOT onto the server.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEST="$ROOT/client-addons"
mkdir -p "$DEST"

# Steps below need curl + unzip (fetch/extract) and zip (build the bundle);
# fail early with a clear message.
command -v curl  >/dev/null || { echo "ERROR: 'curl' is required (e.g. apt install curl)";  exit 1; }
command -v unzip >/dev/null || { echo "ERROR: 'unzip' is required (e.g. apt install unzip)"; exit 1; }
command -v zip   >/dev/null || { echo "ERROR: 'zip' is required (e.g. apt install zip)";     exit 1; }

# staging dir name | source URL
#   *.git -> cloned, then refreshed with git fetch/reset on re-run
#   *.zip -> downloaded + extracted; the dir is wiped and re-extracted on re-run
# Single-folder addons (one .toc at the folder root) need the dir named to match
# the .toc — MultiBot's .toc requires exactly "MultiBot"; Questie-335 likewise.
# AtlasLoot / Atlas / Grid2 / WDM-addons are *multi-folder* (the repo/zip holds
# several addon folders) — there the dir name is just a staging container; copy
# every inner folder that has a .toc (see the install note printed at the end).
ADDONS=(
  "MultiBot|https://github.com/Wishmaster117/MultiBot-Chatless.git"
  "PlayerBotManager|https://github.com/Lichborne-AC/PlayerbotManager.git"
  # Questing: Questie shows quest givers/objectives on the map+minimap
  # (quest data aligned to AzerothCore).
  "Questie-335|https://github.com/sloan-008/Questie-335.git"
  # Dungeon/raid maps for Vanilla + TBC + WotLK instances, plus boss loot tables.
  # Atlas (the maps addon) has no clean dedicated 3.3.5a git repo, so it comes
  # from a curated 3.3.5a addon pack as a zip; AtlasLoot is its loot companion.
  "AtlasLoot|https://github.com/Gescht/AtlasLoot3.3.5a.git"
  "Atlas|https://github.com/NoM0Re/WoW-3.3.5a-Addons/raw/main/src/Addons/Atlas.zip"
  # Raid/party unit frames: Grid2 r736 ported for WotLK (the generic Grid/Grid2
  # builds error out because they target retail). Multi-folder.
  "Grid2|https://github.com/bkader/Grid2-WoTLK.git"
  # World Dungeon Maps companion addons (dungeon labels + coords that pair with
  # the WDM .MPQ data patch staged further below). Multi-folder: the parts that
  # matter are WDM, !Astrolabe and LibMapData-1.0 (Mapster/GatherMate/QuestHelper
  # are optional extras in the same repo).
  "WDM-addons|https://github.com/Trimitor/WDM-addons.git"
)

for entry in "${ADDONS[@]}"; do
  name="${entry%%|*}"; url="${entry#*|}"
  dir="$DEST/$name"
  if [[ "$url" == *.zip ]]; then
    echo "==> Downloading $name (zip)"
    tmp="$(mktemp -d)"
    curl -fsSL "$url" -o "$tmp/addon.zip"
    rm -rf "$dir"; mkdir -p "$dir"
    unzip -q "$tmp/addon.zip" -d "$dir"
    rm -rf "$tmp"
  elif [[ -d "$dir/.git" ]]; then
    echo "==> Updating $name"
    branch="$(git -C "$dir" rev-parse --abbrev-ref HEAD)"
    git -C "$dir" fetch --depth 1 origin "$branch"
    git -C "$dir" reset --hard "origin/$branch"
  else
    echo "==> Cloning $name"
    git clone --depth 1 "$url" "$dir"
  fi
done

# Locally-authored addons (version-controlled under client-addons-src/, not
# downloaded). Staged into $DEST alongside the rest so they ship the same way.
if [[ -d "$ROOT/client-addons-src" ]]; then
  for src in "$ROOT"/client-addons-src/*/; do
    [[ -d "$src" ]] || continue
    name="$(basename "$src")"
    echo "==> Staging local addon $name"
    rm -rf "${DEST:?}/$name"
    cp -a "$src" "$DEST/$name"
  done
fi

# --- Client DATA patch (NOT an addon): World Dungeon Maps --------------------
# WDM injects real Classic/TBC dungeon maps into the 3.3.5a client so the
# default map (M) renders the dungeon layout WITH the player-position arrow,
# like an outdoor zone. This is an .MPQ that belongs in
#   World of Warcraft/Data/<lang>/         (NOT Interface/AddOns/)
# Its companion addons (WDM/!Astrolabe/LibMapData-1.0) are staged by the list
# above. Override the client language with WDM_LANG (default enUS); set it empty
# to skip the patch. Pinned-tip note: known-good release at time of writing was
# Trimitor/WDM-patch 2.4.5-stable; "latest" is fetched so it stays current.
WDM_LANG="${WDM_LANG:-enUS}"
if [[ -n "$WDM_LANG" ]]; then
  DATADIR="$DEST/_data-patches"
  mkdir -p "$DATADIR"
  echo "==> Downloading WDM dungeon-map data patch (patch-${WDM_LANG}-M.MPQ)"
  curl -fsSL "https://github.com/Trimitor/WDM-patch/releases/latest/download/patch-${WDM_LANG}-M.MPQ" \
    -o "$DATADIR/patch-${WDM_LANG}-M.MPQ"
fi

# --- Build the ready-to-unzip addons bundle ---------------------------------
# Produce client-addons.zip at the repo root, pre-structured so a player unzips
# it directly into their World of Warcraft base folder:
#   Interface/AddOns/<addon>/   -- every folder that has a .toc at its top level
#   Data/<lang>/patch-<lang>-M.MPQ
# Folder rule (two levels only, matching the install note above): a staging dir
# WITH a .toc at its root is itself one addon; a staging dir WITHOUT one is a
# container whose immediate subdirs that have a .toc are each an addon. We stop
# at immediate children so an embedded library deeper inside an addon rides
# along inside its parent rather than being hoisted out.
echo "==> Building client-addons.zip"
BUNDLE="$DEST/_bundle"
rm -rf "$BUNDLE"
mkdir -p "$BUNDLE/Interface/AddOns"

# has_toc <dir> -> true if <dir> contains a *.toc directly at its top level.
has_toc() { compgen -G "$1/*.toc" >/dev/null 2>&1; }

for entry in "$DEST"/*/; do
  entry="${entry%/}"
  name="$(basename "$entry")"
  # Skip our own staging/data dirs.
  [[ "$name" == "_bundle" || "$name" == "_data-patches" ]] && continue
  if has_toc "$entry"; then
    cp -a "$entry" "$BUNDLE/Interface/AddOns/$name"
  else
    for sub in "$entry"/*/; do
      sub="${sub%/}"
      [[ -d "$sub" ]] || continue
      has_toc "$sub" && cp -a "$sub" "$BUNDLE/Interface/AddOns/$(basename "$sub")"
    done
  fi
done

# Place the WDM data patch under Data/<lang>/ (lang parsed from patch-<lang>-M.MPQ).
if compgen -G "$DEST/_data-patches/patch-*-M.MPQ" >/dev/null 2>&1; then
  for mpq in "$DEST"/_data-patches/patch-*-M.MPQ; do
    base="$(basename "$mpq")"          # patch-enUS-M.MPQ
    lang="${base#patch-}"; lang="${lang%-M.MPQ}"
    mkdir -p "$BUNDLE/Data/$lang"
    cp -a "$mpq" "$BUNDLE/Data/$lang/$base"
  done
fi

rm -f "$ROOT/client-addons.zip"
# Only zip the trees that exist: Data is absent when WDM_LANG="" skipped the patch.
targets=(Interface)
[[ -d "$BUNDLE/Data" ]] && targets+=(Data)
( cd "$BUNDLE" && zip -qr "$ROOT/client-addons.zip" "${targets[@]}" )
rm -rf "$BUNDLE"
echo "    Wrote $ROOT/client-addons.zip"

cat <<EOF

==================================================================
 Client addons are staged in: $DEST

 Or hand a player ONE file: $ROOT/client-addons.zip -- they unzip it directly
 into their "World of Warcraft" folder and everything lands in the right place
 (Interface/AddOns/ and Data/<lang>/). The registration site serves this zip
 via its "Download bot addons" button.

 Not every entry is one ready-to-copy folder: MultiBot, PlayerBotManager
 and Questie-335 are single-folder addons, while AtlasLoot, Atlas, Grid2 and
 WDM-addons each unpack to SEVERAL addon folders. The rule is the same either
 way -- copy every folder that has a .toc at its top level. List them with:
     find "$DEST" -name '*.toc'

 Install on EACH player's machine:
   1. Copy each addon folder (one with a .toc at its root) into:
        World of Warcraft/Interface/AddOns/
      (e.g. .../AddOns/MultiBot/MultiBot.toc)
   2. At the character-select screen, click "AddOns" and enable them.

 SEPARATE STEP -- the World Dungeon Maps DATA patch (NOT an addon):
   The .MPQ staged in $DEST/_data-patches/ does NOT go in AddOns/. Copy it to:
        World of Warcraft/Data/<lang>/        (e.g. Data/enUS/patch-enUS-M.MPQ)
   This is what makes the default M map show dungeon maps + your position.

 What they do:
   - MultiBot / PlayerBotManager : bot management. MultiBot opens via its
     button/slash command; PlayerBotManager via /pmb or its minimap button.
   - Atlas        : dungeon & raid maps for Vanilla, TBC and WotLK instances.
                    Opens in its OWN window (/atlas or its minimap button) --
                    it does NOT replace the default M map.
   - AtlasLoot    : boss loot tables, browsable from the Atlas window.
   - Questie-335  : quest givers / objectives on the map + minimap
                    (quest data aligned to AzerothCore).
   - Grid2        : compact raid/party unit frames (r736, WotLK build).
                    Copy Grid2, Grid2Options and Grid2AoeHeals.
   - WDM          : World Dungeon Maps -- pairs with the .MPQ data patch so the
                    DEFAULT map (M) shows dungeon layouts with your position,
                    like a normal zone. Copy WDM, !Astrolabe and LibMapData-1.0;
                    then install the .MPQ (see the SEPARATE STEP above).

 NOTE: MultiBot needs the server-side mod-multibot-bridge module,
 which setup.sh already builds into the server.
==================================================================
EOF
