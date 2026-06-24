#!/usr/bin/env bash
# Back up all AzerothCore databases + .env to a timestamped .tar bundle. Run ON THE SERVER.
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
OUT="$BACKUP_DIR/acore-$STAMP.tar"     # one bundle: DB dump + .env
STAGE="$BACKUP_DIR/.stage-$STAMP"      # temp build dir, always cleaned up
mkdir -p "$STAGE"
trap 'rm -rf "$STAGE"' EXIT

cd "$AC_DIR"
echo "[$(date)] Dumping databases -> $OUT"
# --single-transaction = consistent snapshot without locking the live server.
# Stream straight to gzip so the full uncompressed dump never lands on disk.
docker compose exec -T ac-database \
  mysqldump -uroot -p"${DOCKER_DB_ROOT_PASSWORD}" \
    --single-transaction --quick --routines --events \
    --databases acore_auth acore_characters acore_world acore_playerbots \
  | gzip > "$STAGE/database.sql.gz"

# Fail loudly if the dump produced an empty/broken file (the trap removes $STAGE;
# $OUT is not created yet, so there's no partial bundle to clean up).
if [[ ! -s "$STAGE/database.sql.gz" ]]; then
  echo "ERROR: database dump is empty — dump failed. Aborting." >&2
  exit 1
fi

# Bundle .env with the dump. It holds the DB root password plus webreg/lore secrets
# that setup.sh autogenerates and stores nowhere else — unrecoverable from a DB dump.
cp "$AC_DIR/.env" "$STAGE/env"

# One file per backup. NOT re-gzipped: database.sql.gz is already compressed, so an
# outer gzip would only burn CPU — hence the honest .tar extension.
tar -cf "$OUT" -C "$STAGE" database.sql.gz env
chmod 600 "$OUT"   # contains plaintext secrets via env

echo "[$(date)] Backup OK ($(du -h "$OUT" | cut -f1)). Pruning to last $KEEP."
# Delete all but the newest $KEEP bundles.
ls -1t "$BACKUP_DIR"/acore-*.tar 2>/dev/null | tail -n +"$((KEEP + 1))" | xargs -r rm -f

echo "[$(date)] Done. Backups in $BACKUP_DIR:"
ls -1t "$BACKUP_DIR"/acore-*.tar 2>/dev/null | head -n "$KEEP"
