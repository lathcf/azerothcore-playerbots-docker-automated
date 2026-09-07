#!/usr/bin/env bash
# Pull the latest AzerothCore fork + modules and rebuild. Run ON THE SERVER.
# Safe to re-run. Your config (env/dist/etc/*.conf) and database volume are preserved.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
AC_DIR="$ROOT/azerothcore-wotlk"
PINS_FILE="$ROOT/repo-pins.txt"

if [[ ! -d "$AC_DIR/.git" ]]; then
  echo "Not installed yet — run ./setup.sh first." >&2
  exit 1
fi

# repo-pins.txt: lines "<repo-dir-basename> <commit>" freeze a repo at a commit instead of
# tracking its branch tip. Use it to hold a known-good upstream when HEAD is broken; delete the
# line to resume normal updates. Echoes the pinned commit for "$1", or nothing.
pin_for () {
  [[ -f "$PINS_FILE" ]] || return 0
  awk -v r="$1" '!/^[[:space:]]*#/ && NF>=2 && $1==r {print $2; exit}' "$PINS_FILE"
}

# Update a repo to its branch tip — UNLESS it's pinned, in which case freeze it at that exact
# commit (fetched by SHA if not already present locally).
# (These clones hold no local commits, so reset is safe and avoids shallow-merge issues.)
update_repo () {
  local dir="$1" label="$2"
  if [[ ! -d "$dir/.git" ]]; then
    echo "    Skipping $label (not present)."
    return
  fi
  local pin; pin="$(pin_for "$(basename "$dir")")"
  if [[ -n "$pin" ]]; then
    echo "==> Pinning $label to $pin (repo-pins.txt)"
    git -C "$dir" cat-file -e "${pin}^{commit}" 2>/dev/null \
      || git -C "$dir" fetch --depth 1 origin "$pin"
    git -C "$dir" reset --hard "$pin"
    git -C "$dir" clean -fd -- src/ 2>/dev/null || true
    return
  fi
  local branch
  branch="$(git -C "$dir" rev-parse --abbrev-ref HEAD)"
  echo "==> Updating $label ($branch)"
  git -C "$dir" fetch --depth 1 origin "$branch"
  git -C "$dir" reset --hard "origin/$branch"
  # reset --hard reverts tracked files but LEAVES patch-CREATED files (untracked), which then
  # block the next apply_patches with "already exists". Scrub untracked files under src/ only —
  # that's the only place patches create files; env/, config/, modules/ etc. must survive.
  git -C "$dir" clean -fd -- src/ 2>/dev/null || true
}

# Re-apply tracked source patches (patches/*.patch) to the fork. update_repo just reset it to the
# branch tip, wiping any prior patch, so this MUST run after the fork update and before the build.
# Idempotent; aborts loudly if a patch no longer applies (upstream moved the code it touches).
apply_patches () {
  local pdir="$ROOT/patches"
  [[ -d "$pdir" && -d "$AC_DIR/.git" ]] || return 0
  local patch name
  for patch in "$pdir"/*.patch; do
    [[ -e "$patch" ]] || continue
    name="$(basename "$patch")"
    if git -C "$AC_DIR" apply --reverse --check "$patch" >/dev/null 2>&1; then
      echo "    Patch already applied: $name"
    elif git -C "$AC_DIR" apply --check "$patch" >/dev/null 2>&1; then
      git -C "$AC_DIR" apply "$patch"
      echo "    Applied patch: $name"
    else
      echo "    ERROR: $name no longer applies (upstream moved?). Regenerate it against the" >&2
      echo "           current fork or remove it from patches/. Refusing to build without it." >&2
      exit 1
    fi
  done
}

update_repo "$AC_DIR" "AzerothCore (playerbots fork)"
# Update every module present under modules/ (so any added module is covered).
for moddir in "$AC_DIR"/modules/*/; do
  [[ -d "$moddir/.git" ]] || continue
  update_repo "$moddir" "$(basename "$moddir")"
done
# Patches must apply AFTER the module updates: several (0003+) target files inside
# modules/mod-playerbots, and update_repo's reset --hard would wipe hunks applied earlier.
apply_patches

# Re-sync in-repo modules so source edits land before rebuild (same list as setup.sh's
# LOCAL_MODULES — a module missing here rebuilds from a stale copy after every update).
for lm in mod-playerbot-chatter mod-raid-roster mod-ahbot-price mod-wintergrasp-bots mod-arena-roster mod-era-talents; do
  if [[ -d "$ROOT/modules/$lm" ]]; then
    echo "==> Syncing local module: $lm"
    rm -rf "$AC_DIR/modules/$lm"
    cp -a "$ROOT/modules/$lm" "$AC_DIR/modules/$lm"
  fi
done

cd "$AC_DIR"

# An upstream client-data version bump makes ac-client-data-init re-download the data archive into
# the ac-client-data VOLUME. That write fails ("data.zip: Permission denied") when a pre-existing
# volume is root-owned but the container runs as the non-root UID, and Compose then reports the
# misleading "dependency ... failed to start" while ac-worldserver never boots. Chown the volume to
# the container UID (read from the live .env that setup.sh wrote) so the download can write. Mirrors
# the same guard in setup.sh; idempotent and best-effort.
CD_UID="$(grep -E '^DOCKER_USER_ID='  "$AC_DIR/.env" 2>/dev/null | cut -d= -f2)"; CD_UID="${CD_UID:-1000}"
CD_GID="$(grep -E '^DOCKER_GROUP_ID=' "$AC_DIR/.env" 2>/dev/null | cut -d= -f2)"; CD_GID="${CD_GID:-1000}"
DATA_VOL_BASE="${DOCKER_VOL_DATA:-ac-client-data}"
DATA_VOL="$(docker volume ls --format '{{.Name}}' | grep -E "(^|_)${DATA_VOL_BASE}$" | head -1 || true)"
if [[ -n "$DATA_VOL" ]]; then
  echo "==> Ensuring client-data volume ($DATA_VOL) is writable by UID ${CD_UID} (survives client-data version bumps)"
  docker run --rm -v "${DATA_VOL}:/d" alpine chown -R "${CD_UID}:${CD_GID}" /d 2>/dev/null || true
fi

echo "==> Rebuilding & restarting"
echo "    (recompiles only what changed; ac-db-import re-runs to apply new DB migrations)"
docker compose up -d --build

# Trim stale Docker build cache. Each rebuild leaves multi-GB cache layers that are never
# reclaimed on their own (they ballooned to 284 GB once). Keep a 7-day window so recent layers
# still speed up incremental rebuilds. Only touches build cache — never images or volumes (the
# ac-database volume holds character/world data). Best-effort: never fail the update over it.
echo "==> Pruning Docker build cache older than 7 days"
docker builder prune -f --filter until=168h || echo "    (build-cache prune skipped)"

cat <<EOF

==================================================================
 Update complete.
 Watch the world come back up:  docker compose logs -f ac-worldserver

 Note: new config options added by an update are NOT auto-merged into
 your existing env/dist/etc/*.conf (they keep compiled defaults). To pick
 up brand-new settings, compare against the .conf.dist files in that dir.
==================================================================
EOF
