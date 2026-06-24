#!/usr/bin/env bash
# Gracefully stop the AzerothCore server. Run ON THE SERVER.
# Gives the worldserver time to save state (important with hundreds of bots).
# Usage: ./stop.sh [GRACE_SECONDS]   (default 120)
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
AC_DIR="$ROOT/azerothcore-wotlk"
GRACE="${1:-120}"

if [[ ! -d "$AC_DIR/.git" ]]; then
  echo "Not installed yet — run ./setup.sh first." >&2
  exit 1
fi

cd "$AC_DIR"
echo "Stopping (allowing up to ${GRACE}s for a clean save)…"
docker compose stop -t "$GRACE"

echo "Stopped. Database and characters are preserved. Resume with ./start.sh"
