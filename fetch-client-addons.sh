#!/usr/bin/env bash
# Download/refresh the client-side bot-management addons into ./client-addons/.
# These are CLIENT addons: copy each resulting folder into every player's
#   World of Warcraft/Interface/AddOns/
# directory (the 3.3.5a client), NOT onto the server.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEST="$ROOT/client-addons"
mkdir -p "$DEST"

# AddOn folder name (must match the addon's .toc) | git URL
# MultiBot's .toc requires the folder be named exactly "MultiBot".
ADDONS=(
  "MultiBot|https://github.com/Wishmaster117/MultiBot-Chatless.git"
  "PlayerBotManager|https://github.com/Lichborne-AC/PlayerbotManager.git"
)

for entry in "${ADDONS[@]}"; do
  name="${entry%%|*}"; url="${entry#*|}"
  dir="$DEST/$name"
  if [[ -d "$dir/.git" ]]; then
    echo "==> Updating $name"
    branch="$(git -C "$dir" rev-parse --abbrev-ref HEAD)"
    git -C "$dir" fetch --depth 1 origin "$branch"
    git -C "$dir" reset --hard "origin/$branch"
  else
    echo "==> Cloning $name"
    git clone --depth 1 "$url" "$dir"
  fi
done

cat <<EOF

==================================================================
 Client addons are staged in: $DEST

 Some repos keep the addon in a subfolder. Make sure the folder you
 copy contains a .toc file at its top level. Check with:
     find "$DEST" -name '*.toc'

 Install on EACH player's machine:
   1. Copy the addon folder into:
        World of Warcraft/Interface/AddOns/
      (e.g. .../AddOns/MultiBot/MultiBot.toc)
   2. At the character-select screen, click "AddOns" and enable them.
   3. In-game: MultiBot opens via its button/slash command;
      PlayerBotManager opens with /pmb or its minimap button.

 NOTE: MultiBot needs the server-side mod-multibot-bridge module,
 which setup.sh already builds into the server.
==================================================================
EOF
