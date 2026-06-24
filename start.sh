#!/usr/bin/env bash
# Start (or resume) the AzerothCore server. Run ON THE SERVER.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
AC_DIR="$ROOT/azerothcore-wotlk"

if [[ ! -d "$AC_DIR/.git" ]]; then
  echo "Not installed yet — run ./setup.sh first." >&2
  exit 1
fi

cd "$AC_DIR"
# 'up -d' resumes stopped containers and creates any that are missing,
# without rebuilding. Works whether the server was 'stop'ped or 'down'ed.
docker compose up -d

echo "Started. Watch it come up:  docker compose logs -f ac-worldserver"
