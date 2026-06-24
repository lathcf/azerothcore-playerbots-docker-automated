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
    return
  fi
  local branch
  branch="$(git -C "$dir" rev-parse --abbrev-ref HEAD)"
  echo "==> Updating $label ($branch)"
  git -C "$dir" fetch --depth 1 origin "$branch"
  git -C "$dir" reset --hard "origin/$branch"
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
apply_patches
# Update every module present under modules/ (so any added module is covered).
for moddir in "$AC_DIR"/modules/*/; do
  [[ -d "$moddir/.git" ]] || continue
  update_repo "$moddir" "$(basename "$moddir")"
done

# Re-sync in-repo modules (mod-playerbot-chatter) so source edits land before rebuild.
for lm in mod-playerbot-chatter; do
  if [[ -d "$ROOT/modules/$lm" ]]; then
    echo "==> Syncing local module: $lm"
    rm -rf "$AC_DIR/modules/$lm"
    cp -a "$ROOT/modules/$lm" "$AC_DIR/modules/$lm"
  fi
done

cd "$AC_DIR"
echo "==> Rebuilding & restarting"
echo "    (recompiles only what changed; ac-db-import re-runs to apply new DB migrations)"
docker compose up -d --build

cat <<EOF

==================================================================
 Update complete.
 Watch the world come back up:  docker compose logs -f ac-worldserver

 Note: new config options added by an update are NOT auto-merged into
 your existing env/dist/etc/*.conf (they keep compiled defaults). To pick
 up brand-new settings, compare against the .conf.dist files in that dir.
==================================================================
EOF
