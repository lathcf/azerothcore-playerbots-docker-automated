#!/usr/bin/env bash
# Back up all AzerothCore databases to a timestamped .sql.gz. Run ON THE SERVER.
# Keeps the most recent N backups (default 14). Cron-friendly (no interactive parts).
#
# Manual run:   ./backup.sh
# Nightly cron: 0 4 * * *  /path/to/AzerothCore/backup.sh >> /path/to/AzerothCore/backups/backup.log 2>&1
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
AC_DIR="$ROOT/azerothcore-wotlk"
BACKUP_DIR="$ROOT/backups"
KEEP="${BACKUP_KEEP:-14}"          # how many backups to retain
# Stable, sortable timestamp without relying on locale: YYYYmmdd-HHMMSS
STAMP="$(date +%Y%m%d-%H%M%S)"

if [[ ! -f "$AC_DIR/.env" ]]; then
  echo "Not installed yet — run ./setup.sh first." >&2
  exit 1
fi

# Load DB password from .env.
set -a; # shellcheck disable=SC1091
source "$AC_DIR/.env"; set +a

mkdir -p "$BACKUP_DIR"
OUT="$BACKUP_DIR/acore-$STAMP.sql.gz"

cd "$AC_DIR"
echo "[$(date)] Dumping databases -> $OUT"
# --single-transaction = consistent snapshot without locking the live server.
docker compose exec -T ac-database \
  mysqldump -uroot -p"${DOCKER_DB_ROOT_PASSWORD}" \
    --single-transaction --quick --routines --events \
    --databases acore_auth acore_characters acore_world acore_playerbots \
  | gzip > "$OUT"

# Fail loudly if the dump produced an empty/broken file.
if [[ ! -s "$OUT" ]]; then
  echo "ERROR: backup file is empty — dump failed. Removing $OUT" >&2
  rm -f "$OUT"
  exit 1
fi

echo "[$(date)] Backup OK ($(du -h "$OUT" | cut -f1)). Pruning to last $KEEP."
# Delete all but the newest $KEEP backups.
ls -1t "$BACKUP_DIR"/acore-*.sql.gz 2>/dev/null | tail -n +"$((KEEP + 1))" | xargs -r rm -f

echo "[$(date)] Done. Backups in $BACKUP_DIR:"
ls -1t "$BACKUP_DIR"/acore-*.sql.gz 2>/dev/null | head -n "$KEEP"
