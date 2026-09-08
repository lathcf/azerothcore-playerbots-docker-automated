#!/usr/bin/env bash
# AzerothCore + Playerbots LAN server bootstrap. Run ON THE SERVER. Idempotent.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
AC_DIR="$ROOT/azerothcore-wotlk"

FORK_URL="https://github.com/mod-playerbots/azerothcore-wotlk.git"
FORK_BRANCH="Playerbot"

# Server-side modules compiled into the build (name|git-url):
#   mod-playerbots ................. the bot engine (required)
#   mod-aoe-loot ................... (DISABLED 2026-06-21 — broke group-loot rolls, see below)
#   mod-junk-to-gold .............. auto-sell gray trash (less bag clutter for bots/players)
#   mod-multibot-bridge ........... server half of the in-game "MultiBot" control addon
#   mod-individual-progression .... per-character Vanilla→TBC→WotLK era progression (gated content/gear/difficulty)
MODULES=(
  "mod-playerbots|https://github.com/mod-playerbots/mod-playerbots.git"
  # mod-aoe-loot DISABLED 2026-06-21: its area-loot aggregation invalidates pending
  # group-loot rolls when looting a pile of corpses at once — an item you roll Need on
  # vanishes from the corpse and nobody receives it (cf. mod-aoe-loot#43/#44). The
  # reconcile below prunes the existing clone on the next run. Uncomment to restore.
  # "mod-aoe-loot|https://github.com/azerothcore/mod-aoe-loot.git"
  "mod-junk-to-gold|https://github.com/noisiver/mod-junk-to-gold.git"
  "mod-multibot-bridge|https://github.com/Wishmaster117/mod-multibot-bridge.git"
  "mod-ah-bot-plus|https://github.com/NathanHandley/mod-ah-bot-plus.git"
  "mod-individual-progression|https://github.com/ZhengPeiRu21/mod-individual-progression.git"
  # Era talents (own repo since 2026-09-07): requires IP above; its playerbots/bridge patches and bot
  # layer switch on automatically because those modules are present. Pinned in repo-pins.txt.
  "mod-era-talents|https://github.com/lathcf/azerothcore-mod-era-talents.git"
)

# Modules we author and ship from THIS repo (copied in, not git-cloned). Kept by the reconcile.
LOCAL_MODULES=( "mod-playerbot-chatter" "mod-raid-roster" "mod-ahbot-price" "mod-wintergrasp-bots" "mod-arena-roster" )

# Optional commit pins (repo-pins.txt): freeze the fork and/or a module at a known-good commit
# instead of its branch tip — used to hold a stable upstream when the latest HEAD is broken.
# update.sh honors the same file. Applied right after ensuring each repo exists.
PINS_FILE="$ROOT/repo-pins.txt"
pin_for () {
  [[ -f "$PINS_FILE" ]] || return 0
  awk -v r="$1" '!/^[[:space:]]*#/ && NF>=2 && $1==r {print $2; exit}' "$PINS_FILE"
}
apply_pin () {  # $1 = repo dir, $2 = basename used in repo-pins.txt
  local dir="$1" name="$2" pin; pin="$(pin_for "$name")"
  [[ -n "$pin" && -d "$dir/.git" ]] || return 0
  echo "    Pinning $name to $pin (repo-pins.txt)"
  git -C "$dir" cat-file -e "${pin}^{commit}" 2>/dev/null || git -C "$dir" fetch --depth 1 origin "$pin"
  git -C "$dir" reset --hard "$pin"
  # Scrub patch-CREATED (untracked) files so apply_patches never hits "already exists" after a
  # reset. src/ only — patches never create files elsewhere; env/, config/, modules/ must survive.
  git -C "$dir" clean -fd -- src/ 2>/dev/null || true
}

# Tracked source patches for the upstream fork. The fork is gitignored/regenerated, so any core
# change we depend on lives as a patches/*.patch here and is re-applied after the fork is
# cloned/pinned and before the build. Idempotent: an already-applied patch is skipped; one that
# no longer applies (upstream moved that code) aborts loudly rather than silently building
# without it. MUST run after apply_pin — a pin's `reset --hard` wipes a previously-applied patch.
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
  # mod-era-talents ships its own patch tree (core + IP always; playerbots/bridge when present).
  # Runs AFTER ours so 0012's Unit.cpp hunk lands before its Shatter/Wand/Molten Fury hunks —
  # the order every one of its Unit.cpp patches was cut against.
  if [[ -x "$AC_DIR/modules/mod-era-talents/apply-patches.sh" ]]; then
    "$AC_DIR/modules/mod-era-talents/apply-patches.sh" "$AC_DIR"
  fi
}

echo "==> 1/10 Cloning AzerothCore playerbots fork (if missing)"
[[ -d "$AC_DIR/.git" ]] || git clone "$FORK_URL" --branch="$FORK_BRANCH" "$AC_DIR"
apply_pin "$AC_DIR" "azerothcore-wotlk"

echo "==> 2/10 Cloning modules (if missing)"
for entry in "${MODULES[@]}"; do
  name="${entry%%|*}"; url="${entry#*|}"
  # A module dir WITHOUT .git is a stale copy from when it was a LOCAL_MODULE (cp -a'd in), e.g.
  # mod-era-talents before it moved to its own repo (2026-09-07). `git clone` refuses a non-empty
  # dir, so replace it — module dirs are regenerable; any DB data lives in the DB volume.
  if [[ -d "$AC_DIR/modules/$name" && ! -d "$AC_DIR/modules/$name/.git" ]]; then
    echo "    Replacing non-git copy of $name with a clone"
    rm -rf "$AC_DIR/modules/$name"
  fi
  [[ -d "$AC_DIR/modules/$name/.git" ]] || git clone "$url" "$AC_DIR/modules/$name"
  apply_pin "$AC_DIR/modules/$name" "$name"
done

# Patches must apply AFTER the module clones/pins: several (0003+) target files inside
# modules/mod-playerbots, which doesn't exist yet on a fresh install at fork-clone time —
# applying earlier made a fresh install abort on a perfectly good patch.
apply_patches

# Reconcile: the build compiles EVERY module dir under modules/ (CMake globs the tree), so a
# module dropped from MODULES above must be physically removed or it keeps getting compiled.
# Prune any module clone that's no longer listed, making MODULES the source of truth. Module
# dirs are just git clones (regenerable); any DB data a module created lives in the DB volume
# and is left untouched. The git-managed mod-playerbots-required 'mod-eluna' etc. would be
# listed too, so nothing essential is caught here.
if [[ -d "$AC_DIR/modules" ]]; then
  for moddir in "$AC_DIR"/modules/*/; do
    [[ -d "$moddir" ]] || continue
    mod="$(basename "$moddir")"
    keep=0
    for entry in "${MODULES[@]}"; do [[ "${entry%%|*}" == "$mod" ]] && { keep=1; break; }; done
    for lm in "${LOCAL_MODULES[@]}"; do [[ "$lm" == "$mod" ]] && { keep=1; break; }; done
    if [[ "$keep" -eq 0 ]]; then
      echo "    Pruning unlisted module: $mod"
      rm -rf "$moddir"
    fi
  done
fi

# Copy the .env-enabled IP optional SQL files into IP's auto-import dir so they apply on the next
# DB import; remove any that are disabled so toggling OFF actually reverts on rebuild. Idempotent.
# IP uses the older data/sql/world/base layout (NOT db-world) — recon-confirmed.
apply_ip_optional_sql () {
  local modroot="$AC_DIR/modules/mod-individual-progression"
  local optdir="$modroot/optional/sql/world"
  local dest="$modroot/data/sql/world/base"
  [[ -d "$optdir" && -d "$dest" ]] || { echo "    IP optional SQL: dir missing, skipping"; return 0; }
  # flag env var -> optional filename
  local map=(
    "IP_OPT_SPELL_DMG_HEALING|zz_optional_spell_damage_and_healing.sql"
    "IP_OPT_CREATURE_STATS|zz_optional_creature_stats.sql"
    "IP_OPT_VANILLA_REGEN|zz_optional_vanilla_regen_values.sql"
    "IP_OPT_ITEM_STACK_SIZES|zz_optional_item_stack_sizes.sql"
    "IP_OPT_AMMO_STACK|zz_optional_ammo_stack_size.sql"
    "IP_OPT_VANILLA_MODELS|zz_optional_vanilla_models.sql"
    "IP_OPT_PHASING|zz_optional_phasing.sql"
    "IP_OPT_REMOVE_HEIRLOOMS|zz_optional_remove_heirlooms.sql"
    "IP_OPT_VANILLA_CRAFTING|zz_optional_vanilla_crafting_requirements.sql"
    "IP_OPT_CRAFTING_CD|zz_optional_restore_crafting_cd_timers.sql"
    "IP_OPT_POTION_CD|zz_optional_restore_potion_cd.sql"
    "IP_OPT_ROGUE_POISONS|zz_optional_restore_rogue_poisons.sql"
    "IP_OPT_LIMIT_SPELLS|zz_optional_limit_spells_to_expansion.sql"
    "IP_OPT_VANILLA_TRANSPORTS|zz_optional_vanilla_transports.sql"
    "IP_OPT_SMALL_GROUP_ADJ|zz_optional_small_group_adjustments.sql"
    "IP_OPT_UNOBTAINABLE_ITEMS|zz_optional_unobtainable_items.sql"
    "IP_OPT_WOTLK_HP_FOR_TBC_RAIDS|zz_optional_wotlk_hp_values_for_tbc_raids.sql"
    "IP_OPT_AQ_QUEST_NERF|zz_optional_aq_quest_nerf.sql"
    "IP_OPT_AV_LANDMINES|zz_optional_av_landmines.sql"
    "IP_OPT_TBC_HEROIC_KEYS_NERF|zz_optional_tbc_heroic_dungeon_keys_nerf.sql"
    "IP_OPT_TBC_PVP_PRICES|zz_optional_tbc_pvp_prices.sql"
    "IP_OPT_STACKABLE_BUFF_SCROLLS|zz_optional_stackable_buff_scrolls.sql"
  )
  local pair var file on
  for pair in "${map[@]}"; do
    var="${pair%%|*}"; file="${pair#*|}"; on="${!var:-0}"
    if [[ "$on" == "1" && "${IP_ENABLE:-1}" == "1" ]]; then
      [[ -f "$optdir/$file" ]] && cp -f "$optdir/$file" "$dest/$file" && echo "    IP optional ON:  $file"
    else
      [[ -f "$dest/$file" ]] && rm -f "$dest/$file" && echo "    IP optional OFF: $file (removed)"
    fi
  done
  # Local supplement: heal-weight the TBC healer items IP's spell_damage_and_healing optional missed
  # (rides that optional's flag + the same patch-V/dbc prereq). See sql/ip-supplemental/ and docs.
  local suppl="$ROOT/sql/ip-supplemental/zzz_ip_healer_spell_weighting.sql"
  if [[ "${IP_OPT_SPELL_DMG_HEALING:-0}" == "1" && "${IP_ENABLE:-1}" == "1" && -f "$suppl" ]]; then
    cp -f "$suppl" "$dest/zzz_ip_healer_spell_weighting.sql" && echo "    IP supplement: healer spell-weighting fix copied"
  else
    [[ -f "$dest/zzz_ip_healer_spell_weighting.sql" ]] && rm -f "$dest/zzz_ip_healer_spell_weighting.sql" && echo "    IP supplement: healer fix removed"
  fi
  return 0   # never let a final false [[ -f ]] (disabled optional) trip set -e at the call site
}

# Extract IP's SERVER-side DBCs (patch-V Spell.dbc + dbc.7z SkillLine*.dbc) into $AC_DIR/ip-dbc so
# they can be bind-mounted over the worldserver's data/dbc. Client .mpq halves are ignored here
# (patch-V.mpq is staged by fetch-client-addons.sh; patch-J/U are cosmetic and skipped). Idempotent:
# the dir is rebuilt each run, so toggling a flag off removes the DBC next rebuild.
extract_ip_server_dbc () {
  local modopt="$AC_DIR/modules/mod-individual-progression/optional"
  local out="$AC_DIR/ip-dbc"
  rm -rf "$out"
  [[ "${IP_ENABLE:-1}" == "1" ]] || return 0
  { [[ "${IP_CLIENT_PATCH_V:-1}" == "1" || "${IP_CLIENT_DBC:-1}" == "1" ]]; } || return 0
  command -v 7z >/dev/null || { echo "    IP DBC overlay: '7z' (p7zip) not found — skipping server DBC overlay"; return 0; }
  [[ -d "$modopt" ]] || { echo "    IP DBC overlay: $modopt missing (module not cloned yet) — skipping"; return 0; }
  mkdir -p "$out"
  local tmp; tmp="$(mktemp -d)"
  [[ "${IP_CLIENT_PATCH_V:-1}" == "1" && -f "$modopt/patch-V.7z" ]] && 7z x -y -o"$tmp/v" "$modopt/patch-V.7z" >/dev/null
  [[ "${IP_CLIENT_DBC:-1}"     == "1" && -f "$modopt/dbc.7z"     ]] && 7z x -y -o"$tmp/d" "$modopt/dbc.7z"     >/dev/null
  # Copy ONLY *.dbc (the server halves); archives may nest them in subdirs.
  find "$tmp" -type f -iname '*.dbc' -exec cp -f {} "$out/" \;
  rm -rf "$tmp"
  if compgen -G "$out"/*.dbc >/dev/null 2>&1; then
    echo "    IP DBC overlay: staged $(ls "$out"/*.dbc | wc -l) server DBC(s) into ip-dbc/"
  fi
}

# Sync in-repo modules into the build tree (fresh copy each run so edits propagate).
for lm in "${LOCAL_MODULES[@]}"; do
  if [[ -d "$ROOT/modules/$lm" ]]; then
    echo "    Syncing local module: $lm"
    rm -rf "$AC_DIR/modules/$lm"
    cp -a "$ROOT/modules/$lm" "$AC_DIR/modules/$lm"
  fi
done
# NB: apply_ip_optional_sql is called LATER, AFTER `source "$AC_DIR/.env"` — the IP_OPT_* flags it
# reads are unset until then (calling it here silently disabled every optional).

# Persist KEY=VALUE into the repo-root .env (the source of truth, copied to the live env on
# every run) AND the live $AC_DIR/.env (used by THIS run's containers). Replaces an existing
# line or appends. Used for autogenerated secrets that must survive copy-every-run.
persist_env() {
  local key="$1" val="$2" f
  for f in "$ROOT/.env" "$AC_DIR/.env"; do
    [[ -f "$f" ]] || continue
    if grep -q "^${key}=" "$f"; then
      sed -i "s|^${key}=.*|${key}=${val}|" "$f"
    else
      # Lead with \n so we never concatenate onto a file missing its trailing newline
      # (a stray editor save of $ROOT/.env) — a resulting blank line is harmless to `source`.
      printf '\n%s=%s\n' "$key" "$val" >> "$f"
    fi
  done
}

echo "==> 3/10 Creating/refreshing .env and container UID/GID"
# Repo-root .env is the SOURCE OF TRUTH, copied over the live env on EVERY run — edit
# ./.env, re-run setup.sh, always in sync. Bootstrap repo-root .env if absent:
#   - existing install (live env present): PROMOTE it to ./.env so its secrets and DB
#     password carry over — never clobber a working install with template defaults.
#   - fresh install where the operator skipped `cp .env.example .env`: seed from template.
if [[ ! -f "$ROOT/.env" ]]; then
  if [[ -f "$AC_DIR/.env" ]]; then
    cp "$AC_DIR/.env" "$ROOT/.env"
    echo "    Promoted existing azerothcore-wotlk/.env to ./.env (new source of truth)."
  else
    cp "$ROOT/.env.example" "$ROOT/.env"
    echo "    Created ./.env from .env.example — set DOCKER_DB_ROOT_PASSWORD before public exposure."
  fi
fi
cp "$ROOT/.env" "$AC_DIR/.env"

# Pick the UID/GID the container builds/runs as. The image CREATES this user
# (addgroup --gid <GID>), so it must NOT be 0 — GID 0 collides with the root group and the
# build fails with "The GID '0' is already in use". When setup runs as root (common in an
# LXC), fall back to 1000 and make the bind-mounted dirs writable by it. Override via AC_UID.
HOST_UID="$(id -u)"; HOST_GID="$(id -g)"
if [[ "$HOST_UID" -eq 0 ]]; then
  HOST_UID="${AC_UID:-1000}"; HOST_GID="${AC_GID:-1000}"
  echo "    Running as root -> container will use UID/GID ${HOST_UID}/${HOST_GID} (GID 0 can't be used)."
fi
sed -i "s/^DOCKER_USER_ID=.*/DOCKER_USER_ID=${HOST_UID}/"  "$AC_DIR/.env"
sed -i "s/^DOCKER_GROUP_ID=.*/DOCKER_GROUP_ID=${HOST_GID}/" "$AC_DIR/.env"

# The container user (UID above) must be able to write the bind-mounted config/log dirs.
mkdir -p "$AC_DIR/env/dist/etc" "$AC_DIR/env/dist/logs"
chown -R "${HOST_UID}:${HOST_GID}" "$AC_DIR/env/dist/etc" "$AC_DIR/env/dist/logs" 2>/dev/null || true

# Load .env now so the generated configs below can read user settings (and so we can talk to the
# DB later with the right password). The docker-compose override heredoc keeps its ${...} literal
# for Compose to substitute; the MySQL tuning file and the worldserver/module confs further down
# are written with the values expanded HERE, so they need .env in scope first.
set -a
# shellcheck disable=SC1091
source "$AC_DIR/.env"
set +a

# Mount the modules tree into the runtime containers. Modules compile statically into the
# binaries, but their SQL/data files are NOT in the runtime image — the worldserver needs
# modules/mod-playerbots/data/sql/... at runtime to populate the acore_playerbots DB (without
# this it crashes on boot), and db-import needs module SQL for world/character updates.
#
# This override ALSO hardens the published ports (the upstream compose binds them to 0.0.0.0):
#   - ac-database  : MySQL bound to LOOPBACK only — never reachable off-box. setup.sh talks to
#                    the DB via `docker compose exec`, so it doesn't need a routable host port.
#   - ac-worldserver: publish ONLY the game world port; SOAP (7878) is dropped — it's a remote
#                    GM-command HTTP API and has no business being reachable. (authserver's 3724
#                    is published by the base compose and is the only other port we want open.)
# `ports: !override` REPLACES the base port list instead of appending to it — requires Docker
# Compose v2.24+ (Jan 2024). If `docker compose version` is older, update it.
# Copy the .env-enabled IP optional SQL into IP's auto-import dir (must be AFTER `source .env` above,
# so the IP_OPT_* flags are set) and before the build/db-import below reads the module SQL.
apply_ip_optional_sql
extract_ip_server_dbc
# One bind-mount line per staged DBC, overlaying the worldserver's data/dbc (paths relative to
# $AC_DIR, where compose runs). Empty when IP/flags are off → no mounts added.
IP_DBC_MOUNT_LINES=""
if compgen -G "$AC_DIR"/ip-dbc/*.dbc >/dev/null 2>&1; then
  for _dbc in "$AC_DIR"/ip-dbc/*.dbc; do
    _b="$(basename "$_dbc")"
    IP_DBC_MOUNT_LINES+="      - ./ip-dbc/${_b}:/azerothcore/env/dist/data/dbc/${_b}:ro"$'\n'
  done
fi
cat > "$AC_DIR/docker-compose.override.yml" <<YAML
# Generated by setup.sh — do not edit by hand.
services:
  ac-worldserver:
    environment:
      TZ: "\${SERVER_TZ:-Etc/UTC}"
    ports: !override
      - "\${DOCKER_WORLD_EXTERNAL_PORT:-8085}:8085"
    volumes:
      - ./modules:/azerothcore/modules:ro
${IP_DBC_MOUNT_LINES}  ac-db-import:
    environment:
      TZ: "\${SERVER_TZ:-Etc/UTC}"
    volumes:
      - ./modules:/azerothcore/modules:ro
  ac-database:
    environment:
      TZ: "\${SERVER_TZ:-Etc/UTC}"
    ports: !override
      - "127.0.0.1:\${DOCKER_DB_EXTERNAL_PORT:-3306}:3306"
    volumes:
      - ./config/mysql-tuning.cnf:/etc/mysql/conf.d/zz-acore-tuning.cnf:ro
YAML

# MySQL/MariaDB performance tuning, bind-mounted into ac-database above. The official mysql/
# mariadb images both `!includedir /etc/mysql/conf.d/`, and the `zz-` prefix makes this load
# LAST so it overrides the image defaults. The defaults assume the DB SHARES RAM with a bot-heavy
# worldserver, so they size the buffer pool to the working set + headroom rather than the
# mod-playerbots wiki's "buffer pool = 50% of RAM" rule (which assumes a DEDICATED DB host and can
# OOM a shared box) — still vastly above the stock 128M default. All values are .env-driven (DB_*);
# scale them to your RAM (buffer pool) and disk speed (io capacity). Only broadly-compatible innodb
# options are used (no innodb_use_fdatasync / skip-log-bin) so a bad option can't put the DB in a
# boot loop. If the DB ever fails to start, delete config/mysql-tuning.cnf and the mount and re-run.
mkdir -p "$AC_DIR/config"
cat > "$AC_DIR/config/mysql-tuning.cnf" <<CNF
# Generated by setup.sh from .env (DB_* knobs) — do not edit by hand; edit .env and re-run setup.sh.
[mysqld]
innodb_buffer_pool_size      = ${DB_BUFFER_POOL_SIZE:-8G}
innodb_buffer_pool_instances = ${DB_BUFFER_POOL_INSTANCES:-8}
innodb_io_capacity           = ${DB_IO_CAPACITY:-500}
innodb_io_capacity_max       = ${DB_IO_CAPACITY_MAX:-2500}
innodb_log_buffer_size       = 32M
transaction_isolation        = READ-COMMITTED
# 2 = flush the redo log to disk ~once/second instead of at every commit. Thousands of bot saves
# make per-commit fsyncs the DB's dominant cost; the trade is losing up to ~1s of the most recent
# transactions if the HOST OS crashes (a mysqld-only crash loses nothing) — fine for a LAN game
# server. Set DB_FLUSH_LOG_AT_TRX_COMMIT=1 for full ACID durability.
innodb_flush_log_at_trx_commit = ${DB_FLUSH_LOG_AT_TRX_COMMIT:-2}
CNF
# Must NOT be world-writable or mysqld ignores it ("World-writable config file is ignored").
chmod 0644 "$AC_DIR/config/mysql-tuning.cnf"

# Safety: never let the server go public with the placeholder DB password. PUBLIC_REALM_ADDRESS
# being set is our "exposing this to the internet" signal — if it's set, the root password must
# have been changed from the shipped default first.
if [[ -n "${PUBLIC_REALM_ADDRESS:-}" ]]; then
  if [[ "${DOCKER_DB_ROOT_PASSWORD:-}" == "changeme_db_password" || -z "${DOCKER_DB_ROOT_PASSWORD:-}" ]]; then
    echo "ERROR: PUBLIC_REALM_ADDRESS is set (external exposure) but DOCKER_DB_ROOT_PASSWORD is" >&2
    echo "       still the shipped default. Set a strong, unique password in $ROOT/.env first." >&2
    exit 1
  fi
fi

cd "$AC_DIR"

echo "==> 4/10 Building & starting the stack (first run compiles core+modules,"
echo "          imports the DB, and downloads client data — this can take a while)"
# An upstream client-data version bump makes ac-client-data-init re-download the data archive into
# the ac-client-data VOLUME. That write fails ("Permission denied") when a PRE-EXISTING volume is
# root-owned while the container runs as the non-root UID above — which cascades to ac-worldserver
# never starting (it depends on client-data-init completing). Chown the volume to the container user
# so the download can write. Only touch it if it already exists: a fresh install has no volume yet
# and inherits the image's (acore-owned) data dir on first mount. Idempotent; safe to re-run.
DATA_VOL_BASE="${DOCKER_VOL_DATA:-ac-client-data}"
DATA_VOL="$(docker volume ls --format '{{.Name}}' | grep -E "(^|_)${DATA_VOL_BASE}$" | head -1 || true)"
if [[ -n "$DATA_VOL" ]]; then
  echo "    Ensuring client-data volume ($DATA_VOL) is writable by UID ${HOST_UID} (survives client-data version bumps)"
  docker run --rm -v "${DATA_VOL}:/d" alpine chown -R "${HOST_UID}:${HOST_GID}" /d 2>/dev/null || true
fi

# --remove-orphans: the override was just rewritten above WITHOUT ac-webreg (it's appended
# later, once its secrets exist), so a webreg container from a prior run looks orphaned here
# and Compose warns. Drop it now; the ac-webreg step below recreates it from the fresh config.
docker compose up -d --build --remove-orphans

echo "==> 5/10 Waiting for generated config files"
# The entrypoint creates worldserver.conf (the app config) directly in env/dist/etc, and copies
# module *.conf.dist into the env/dist/etc/modules subdir. Wait for worldserver.conf as the
# "entrypoint ran" marker.
ETC="env/dist/etc"
MODETC="env/dist/etc/modules"
for _ in $(seq 1 360); do
  [[ -f "$ETC/worldserver.conf" ]] && break
  sleep 10
done
if [[ ! -f "$ETC/worldserver.conf" ]]; then
  echo "ERROR: config files never appeared. Check: docker compose logs ac-worldserver" >&2
  exit 1
fi

# Module .conf files are NOT auto-created (only worldserver.conf is) and live in the modules/
# subdir. Create each from its .dist so our overrides sit on top of a COMPLETE config.
ensure_conf () { [[ -f "$MODETC/$1.conf" ]] || { [[ -f "$MODETC/$1.conf.dist" ]] && cp "$MODETC/$1.conf.dist" "$MODETC/$1.conf"; }; }
# Instantiate EVERY module's .conf from its shipped .conf.dist. The entrypoint drops the .dist
# files here but does not rename them, and the worldserver only loads .conf — so any module
# without a .conf has its options absent from the loaded config, and sConfigMgr->GetOption logs
# a "Missing property ..." warning on EVERY read (e.g. mod-aoe-loot spamming AOELoot.Enable /
# AOELoot.Message on each loot). Looping covers every module, present or future, instead of a
# hand-maintained list that silently misses new ones.
# The .dist copies in $MODETC are FROZEN at first install: the image entrypoint populates
# env/dist/etc with `cp -rn` (no clobber), so a module whose .conf.dist gained keys after the
# first boot never refreshes there. Refresh each from the module SOURCE tree first (the .dist is
# never loaded by the server, so overwriting it is harmless), then instantiate/merge from it.
for _src in "$AC_DIR"/modules/*/conf/*.conf.dist; do
  [[ -e "$_src" ]] || continue
  cp -f "$_src" "$MODETC/$(basename "$_src")"
done
for _dist in "$MODETC"/*.conf.dist; do
  [[ -e "$_dist" ]] || continue           # glob didn't match (no .dist files yet) -> skip
  _name="$(basename "$_dist" .conf.dist)"
  ensure_conf "$_name"
  # Merge keys ADDED to the .dist since the .conf was first instantiated (ensure_conf only copies
  # a MISSING .conf, so an existing install otherwise never sees new keys and the core logs
  # "Missing property ..." on every read). Appends the .dist line verbatim; never touches a key
  # the .conf already has, so set_conf overrides below and hand edits are preserved.
  _conf="$MODETC/$_name.conf"
  [[ -f "$_conf" ]] || continue
  grep -E '^[A-Za-z][A-Za-z0-9_.]*[[:space:]]*=' "$_dist" | while IFS= read -r _line; do
    _key="${_line%%=*}"; _key="${_key//[[:space:]]/}"
    grep -qE "^[[:space:]]*${_key}[[:space:]]*=" "$_conf" || printf '%s\n' "$_line" >> "$_conf"
  done
done

WS_CONF="$ETC/worldserver.conf"
PB_CONF="$MODETC/playerbots.conf"
PBCHAT_CONF="$MODETC/mod_playerbot_chatter.conf"
IP_CONF="$MODETC/individualProgression.conf"

# Idempotent ini setter: replace KEY's value, or append if absent.
set_conf () {
  local key="$1" val="$2" file="$3"
  if grep -qE "^[[:space:]]*${key}[[:space:]]*=" "$file"; then
    sed -i -E "s|^[[:space:]]*${key}[[:space:]]*=.*|${key} = ${val}|" "$file"
  else
    printf '%s = %s\n' "$key" "$val" >> "$file"
  fi
}

echo "==> 6/10 Applying boosted rates + bot population"
# Boosted rates (XP-focused; loot balance left mostly intact). All .env-tunable — see the
# "Gameplay rates" block in .env.example; the defaults below match the values this server
# shipped with, so an .env that omits a knob keeps the old behavior.
set_conf "Rate.XP.Kill"         "${XP_KILL_RATE:-3}"    "$WS_CONF"
set_conf "Rate.XP.Quest"        "${XP_QUEST_RATE:-3}"   "$WS_CONF"
set_conf "Rate.XP.Explore"      "${XP_EXPLORE_RATE:-3}" "$WS_CONF"
set_conf "Rate.XP.Pet"          "${XP_PET_RATE:-3}"     "$WS_CONF"
set_conf "Rate.Drop.Money"      "${MONEY_DROP_RATE:-2}" "$WS_CONF"
set_conf "Rate.Reputation.Gain" "${REPUTATION_RATE:-5}" "$WS_CONF"
# Honor gain multiplier (PvP/BG honor points). 5x by default so BG/world-PvP grinding fills
# out PvP gear fast on a LAN/bot server.
set_conf "Rate.Honor"           "${HONOR_RATE:-5}"      "$WS_CONF"
# Rested-XP pool fill rate (the blue "rested" bonus that doubles kill XP until spent).
# InGame = while logged in resting in an inn/city; Offline = while logged off (tavern/city
# vs wilderness). 3x so the pool refills fast and more of your killing is doubled. MaxBonus
# (default 1.5) is the *cap* on banked rested XP, not a speed — raise it if you want a bigger
# buffer to store while away.
set_conf "Rate.Rest.InGame"                 "${REST_INGAME_RATE:-3}"             "$WS_CONF"
set_conf "Rate.Rest.Offline.InTavernOrCity" "${REST_OFFLINE_TAVERN_RATE:-3}"     "$WS_CONF"
set_conf "Rate.Rest.Offline.InWilderness"   "${REST_OFFLINE_WILDERNESS_RATE:-3}" "$WS_CONF"
# Respawn timers: stock dynamic-respawn defaults. The dynamic system already scales respawn
# time down as a zone's player+bot count rises (bots count as players), which with 1500 bots
# is plenty fast on its own — making it more aggressive (rate < 1 / lower minimum) caused
# near-instant respawns. Pinned to defaults explicitly so re-running setup.sh restores them.
set_conf "Respawn.DynamicRateCreature"      "1"  "$WS_CONF"
set_conf "Respawn.DynamicMinimumCreature"   "10" "$WS_CONF"
set_conf "Respawn.DynamicRateGameObject"    "1"  "$WS_CONF"  # herbs/ore/chests
set_conf "Respawn.DynamicMinimumGameObject" "10" "$WS_CONF"
# Save characters every 5 min (default 15) so a power loss costs less progress.
set_conf "PlayerSaveInterval"   "300000" "$WS_CONF"
# Instant mail delivery (default 3600 = 1h). Auction proceeds are paid via mail with this
# delay (AuctionHouseMgr uses CONFIG_MAIL_DELIVERY_DELAY), so 0 = sold-auction gold and
# won items arrive immediately. Also makes player-to-player item/gold mail instant (handy
# for alts). Does not shorten auction duration — only the post-sale mail wait.
set_conf "MailDeliveryDelay"    "0" "$WS_CONF"
# No Dungeon Deserter debuff (spell 71041). Default 1 casts a 30-min "can't re-queue" aura on
# anyone who leaves an LFG dungeon early (LFGScripts.cpp gates it on CONFIG_LFG_CAST_DESERTER).
# 0 = leave a dungeon whenever you want and immediately re-queue — friendlier for a LAN/bot server.
set_conf "DungeonFinder.CastDeserter" "0" "$WS_CONF"
# Same for battlegrounds: default 1 casts a Deserter spell on anyone who leaves a BG in progress.
# 0 = leave a BG early with no penalty (matches the dungeon choice above; friendlier for LAN/bots).
set_conf "Battleground.CastDeserter"  "0" "$WS_CONF"
# ── Individual Progression (mod-individual-progression) ──────────────────────
if [[ "${IP_ENABLE:-1}" == "1" ]]; then
  # Core prerequisites required by IP's install doc.
  set_conf "Updates.EnableDatabases" "7" "$WS_CONF"
  set_conf "EnablePlayerSettings"    "1" "$WS_CONF"

  set_conf "IndividualProgression.Enable"                      "1"                              "$IP_CONF"
  set_conf "IndividualProgression.EnforceGroupRules"           "${IP_ENFORCE_GROUP_RULES:-0}"   "$IP_CONF"
  set_conf "IndividualProgression.VanillaPowerAdjustment"      "${IP_VANILLA_POWER_ADJ:-1}"     "$IP_CONF"
  set_conf "IndividualProgression.VanillaHealingAdjustment"    "${IP_VANILLA_HEALING_ADJ:-1}"   "$IP_CONF"
  set_conf "IndividualProgression.TBCPowerAdjustment"          "${IP_TBC_POWER_ADJ:-1}"         "$IP_CONF"
  set_conf "IndividualProgression.TBCHealingAdjustment"        "${IP_TBC_HEALING_ADJ:-1}"       "$IP_CONF"
  set_conf "IndividualProgression.BotOnlyAdjustments"          "${IP_BOT_ONLY_ADJUSTMENTS:-0}"  "$IP_CONF"
  set_conf "IndividualProgression.QuestXPFix"                  "${IP_QUEST_XP_FIX:-1}"          "$IP_CONF"
  set_conf "IndividualProgression.DisableRDF"                  "${IP_DISABLE_RDF:-0}"           "$IP_CONF"
  set_conf "IndividualProgression.DisableQuestMarkers"         "${IP_DISABLE_QUEST_MARKERS:-1}" "$IP_CONF"
  set_conf "IndividualProgression.MaxMonsterSight"             "${IP_MAX_MONSTER_SIGHT:-1}"     "$IP_CONF"
  set_conf "IndividualProgression.FishingFix"                  "${IP_FISHING_FIX:-1}"           "$IP_CONF"
  set_conf "IndividualProgression.StartingProgression"         "${IP_STARTING_PROGRESSION:-0}"  "$IP_CONF"
  set_conf "IndividualProgression.ProgressionLimit"            "${IP_PROGRESSION_LIMIT:-0}"     "$IP_CONF"
  # Manual expansion advance (patch 0027 + mod-era-talents gossip): hold the two talent-wiping
  # stages (8 = TBC, 13 = WotLK) so a boss kill / quest turn-in never crosses them; the player
  # advances by choice at Anduin/Thrall. "" = stock automatic progression.
  if [[ "${IP_MANUAL_ERA_ADVANCE:-1}" == "1" ]]; then
    set_conf "IndividualProgression.ManualAdvanceStates" "8 13" "$IP_CONF"
  else
    set_conf "IndividualProgression.ManualAdvanceStates" ""     "$IP_CONF"
  fi
  set_conf "IndividualProgression.DeathKnightUnlockProgression" "${IP_DK_UNLOCK_STAGE:-13}"     "$IP_CONF"
  set_conf "IndividualProgression.DeathKnightStartingProgression" "${IP_DK_START_STAGE:-13}"    "$IP_CONF"
  set_conf "IndividualProgression.TbcRacesUnlockProgression"   "${IP_TBC_RACES_UNLOCK:-8}"      "$IP_CONF"
  set_conf "IndividualProgression.tbcRacesStartingProgression" "${IP_TBC_RACES_START:-8}"       "$IP_CONF"
  set_conf "IndividualProgression.RequireNaxxStrathEntrance"   "${IP_REQUIRE_NAXX_STRATH:-0}"   "$IP_CONF"
  set_conf "IndividualProgression.BotAccountsRegex"            "${IP_BOT_ACCOUNTS_REGEX:-^RNDBOT.*}" "$IP_CONF"
  set_conf "IndividualProgression.BotAccountsMaxLevel"         "${IP_BOT_ACCOUNTS_MAX_LEVEL:-80}" "$IP_CONF"
  set_conf "IndividualProgression.ExcludedAccountsRegex"       "${IP_EXCLUDED_ACCOUNTS_REGEX:-}" "$IP_CONF"
  echo "    IP: era progression ON (EnforceGroupRules=${IP_ENFORCE_GROUP_RULES:-0})."
else
  set_conf "IndividualProgression.Enable" "0" "$IP_CONF"
  echo "    IP: DISABLED (IP_ENABLE!=1)."
fi
# ── Era Talents (mod-era-talents) ────────────────────────────────────────────
# Era-authentic talent windows for era-gated chars; talents modify stock WotLK spells.
# Meaningless without IP (eras come from Individual Progression) — force OFF if IP is off.
if [[ "${ERATALENTS_ENABLE:-1}" == "1" && "${IP_ENABLE:-1}" != "1" ]]; then
  echo "    EraTalents: ERATALENTS_ENABLE=1 but IP_ENABLE!=1 — era talents need IP; forcing OFF."
  ERATALENTS_ENABLE=0
fi
ERATALENTS_CONF="$MODETC/mod_era_talents.conf"
set_conf "EraTalents.Enable" "${ERATALENTS_ENABLE:-1}" "$ERATALENTS_CONF"
set_conf "EraTalents.Debug"  "${ERATALENTS_DEBUG:-0}"  "$ERATALENTS_CONF"
set_conf "EraTalents.BotTalents" "${ERATALENTS_BOTS:-0}" "$ERATALENTS_CONF"
set_conf "EraTalents.GlyphGate" "${ERATALENTS_GLYPHGATE:-1}" "$ERATALENTS_CONF"
# The faction-leader "Progress to the next expansion" gossip rides the same .env knob as the IP
# hold above — on without the hold would be harmless but pointless, hold without the gossip
# would strand players at 7/12, so they are one switch. Forced off with era talents.
if [[ "${ERATALENTS_ENABLE:-1}" == "1" && "${IP_MANUAL_ERA_ADVANCE:-1}" == "1" ]]; then
  set_conf "EraTalents.AdvanceGossip" "1" "$ERATALENTS_CONF"
else
  set_conf "EraTalents.AdvanceGossip" "0" "$ERATALENTS_CONF"
fi
echo "    EraTalents: $([[ "${ERATALENTS_ENABLE:-1}" == "1" ]] && echo ON || echo OFF)."
# PvP realm (GameType 1). Drives World::IsPvPRealm(): players are auto-flagged for PvP in
# contested/enemy territory, like a classic PvP server. (0=Normal/PvE, 6=RP, 8=RPPvP, 16=FFA.)
# The realmlist 'icon' below is set to match so the realm-select screen also shows "PvP".
set_conf "GameType"                   "1" "$WS_CONF"
# Keep the noisy playerbot INFO logging (BG-queue spam, etc.) OFF the live console so the
# worldserver console stays usable for admin commands; route warnings to Playerbots.log.
set_conf "Logger.playerbots"    "3,Playerbots" "$WS_CONF"
# mod-multibot-bridge has no .conf of its own, so it logs a "Missing property
# MultiBotBridge.EnableConsoleLogs" notice on every console-log call until the property is
# defined somewhere it reads (worldserver.conf is accepted). Define it as 0: silences that
# spam AND keeps the bridge off the console, matching the Logger.playerbots choice above.
set_conf "MultiBotBridge.EnableConsoleLogs" "0" "$WS_CONF"
# Core 413bea61+ (2026-09-07 bump) added clustering support (feat(Core) #16832) and reads
# Cluster.Enabled at startup; the entrypoint-created worldserver.conf predates the key on existing
# installs, so define it as 0 (single realm, no sidecar) to silence the "Missing property" notice.
set_conf "Cluster.Enabled" "0" "$WS_CONF"
# Roaming bot population (from .env MAX_RANDOM_BOTS, default 2000). Account count 0 = automatic.
# Only ~BotActiveAlone% run full AI when no real player is near, and SmartScale auto-throttles
# if the server tick gets heavy — but more online bots still cost more RAM and CPU.
BOTS="${MAX_RANDOM_BOTS:-2000}"
set_conf "AiPlayerbot.Enabled"            "1"      "$PB_CONF"
set_conf "AiPlayerbot.RandomBotAutologin" "1"      "$PB_CONF"
set_conf "AiPlayerbot.MinRandomBots"      "$BOTS"  "$PB_CONF"
set_conf "AiPlayerbot.MaxRandomBots"      "$BOTS"  "$PB_CONF"
# Required by the level-bracket balancer (AiPlayerbot.LevelBrackets.*): bots must keep their random levels.
set_conf "AiPlayerbot.DisableRandomLevels" "0"   "$PB_CONF"
# Disable gear/spec persistence: it's incompatible with the level-bracket balancer.
# The bracket balancer constantly re-levels bots to follow the player population, including
# DEMOTING them (e.g. 45 -> 7) via PlayerbotFactory::Randomize(false). With persistence ON
# (the playerbots default), Randomize skips ClearAllItems() for any bot at/above the
# persistence level, so a demoted bot keeps its old high-level gear; InitEquipment only
# swaps slots where it happens to find a level-appropriate item, leaving the rest overleveled
# (the "level 7 bot in level 45 gear" symptom). Off = every re-level re-gears from scratch.
set_conf "AiPlayerbot.EquipAndSpecPersistence" "0" "$PB_CONF"
# Battlegrounds & arenas.
# On-demand (the key setting): with RandomBotJoinBG=1, when YOU queue any BG/arena at ANY
# level, bots at your bracket fill it — and the level-bracket balancer keeps bots at every
# level, so BGs pop at all brackets, not just 80. This is what lets you join whenever you want.
set_conf "AiPlayerbot.RandomBotJoinBG" "1" "$PB_CONF"
# Always-on ambiance: bots also run their own BGs even with no humans. Kept light at level 80
# (WS/AB/EY) to avoid the module's documented over-queuing and draining the open world.
# AV/IC (40v40) are left on-demand-only (count 0) — they still pop when you queue for them.
set_conf "AiPlayerbot.RandomBotAutoJoinBG"        "1" "$PB_CONF"
set_conf "AiPlayerbot.RandomBotAutoJoinBGWSCount" "1" "$PB_CONF"  # Warsong Gulch
set_conf "AiPlayerbot.RandomBotAutoJoinBGABCount" "1" "$PB_CONF"  # Arathi Basin
set_conf "AiPlayerbot.RandomBotAutoJoinBGEYCount" "1" "$PB_CONF"  # Eye of the Storm
set_conf "AiPlayerbot.RandomBotAutoJoinBGAVCount" "0" "$PB_CONF"  # Alterac Valley (on-demand)
set_conf "AiPlayerbot.RandomBotAutoJoinBGICCount" "0" "$PB_CONF"  # Isle of Conquest (on-demand)
# Rated arena (level 80 only — lower brackets need core code changes per module docs).
# Bot arena teams (RandomBotArenaTeam*Count defaults: 10/10/5) provide the opposition.
# NOTE: the RatedArena*Count knobs themselves are set further below, gated on
# ARENAROSTER_ENABLE (mod-arena-roster's premade ladder replaces this stock ambiance
# when the module is on).
# Fast rated-arena pops: the core matchmaker only pairs teams within Arena.MaxRatingDifference
# MMR (default 150) of each other until a team has waited Arena.RatingDiscardTimer ms
# (default 600000 = TEN minutes), after which rating is ignored (BattlegroundQueue.cpp).
# With only a handful of bot teams to draw from, an out-of-band team sits most of that
# window before a pop. Widen the band and cut the discard so any queued bot team becomes
# a legal opponent within ~a minute.
set_conf "Arena.MaxRatingDifference" "500"   "$WS_CONF"
set_conf "Arena.RatingDiscardTimer"  "60000" "$WS_CONF"
# Arena points: the core default is Arena.AutoDistributePoints=0 — points are NEVER paid out
# automatically. Enable it on a daily cycle (retail was weekly). The first payout happens
# shortly after the next restart (the stored next-distribution worldstate starts in the past),
# then every AutoDistributeInterval days. Arena.GamesRequired is per-CYCLE (default 10, tuned
# for a week): a daily payout would demand 10 games/day, so relax it to 3. The core also
# requires each member to have played >=30% of the team's games that cycle (hardcoded).
set_conf "Arena.AutoDistributePoints"   "1" "$WS_CONF"
set_conf "Arena.AutoDistributeInterval" "1" "$WS_CONF"
set_conf "Arena.GamesRequired"          "3" "$WS_CONF"

# Arena roster (mod-arena-roster): per-player partner pool + premade opponent ladder +
# arena coordination AI (fork patch 0005). When enabled, the module's director serves
# premade tiered teams into the player's rated queue — the stock random-bot rated
# ambiance would race it into matches at random gear/ratings, so pin it off.
# NOTE: the Arena.MaxRatingDifference=500 / Arena.RatingDiscardTimer=60000 knobs above
# are LOAD-BEARING for this module: the director's 120s queue window assumes them
# (stock 150/600000 would abort most engagements before the discard timer pairs them).
AR_CONF="$MODETC/mod_arena_roster.conf"
if [[ "${ARENAROSTER_ENABLE:-0}" == "1" ]]; then
  set_conf "ArenaRoster.Enable" "1" "$AR_CONF"
  set_conf "AiPlayerbot.RandomBotAutoJoinBGRatedArena2v2Count" "0" "$PB_CONF"
  set_conf "AiPlayerbot.RandomBotAutoJoinBGRatedArena3v3Count" "0" "$PB_CONF"
  set_conf "AiPlayerbot.RandomBotAutoJoinBGRatedArena5v5Count" "0" "$PB_CONF"
else
  set_conf "ArenaRoster.Enable" "0" "$AR_CONF"
  set_conf "AiPlayerbot.RandomBotAutoJoinBGRatedArena2v2Count" "1" "$PB_CONF"
  set_conf "AiPlayerbot.RandomBotAutoJoinBGRatedArena3v3Count" "1" "$PB_CONF"
  set_conf "AiPlayerbot.RandomBotAutoJoinBGRatedArena5v5Count" "1" "$PB_CONF"
fi

# Performance / scaling tuning (mod-playerbots "Playerbot Configuration" wiki + the 2026-07 source
# audit for 4000-5000 bots). These are the throughput knobs, separate from the gameplay rates above.
# The CPU/threading knobs are .env-driven (scale them to your host's cores, RAM, and bot count);
# the rest are sane fixed defaults.
#   worldserver.conf:
#   - MapUpdate.Threads (.env MAP_UPDATE_THREADS): stock is 1 — the single biggest CPU lever with a
#     high bot count. One map = one thread's work (no intra-map parallelism), and open-world bots
#     live on ~5 continent maps, so past ~8 extra threads only serve instanced maps.
#   - MapUpdateInterval/MinWorldUpdateTime: keep the world tick responsive under bot load.
#   - CharacterDatabase.WorkerThreads/SynchThreads (.env CHAR_DB_WORKER_THREADS /
#     CHAR_DB_SYNCH_THREADS): stock is 1/1 — every bot save funnels through ONE async connection,
#     and an undersized synch pool BUSY-SPINS on the map threads (GetFreeConnection).
#   - Visibility.Distance.Continents (.env VISIBILITY_DISTANCE_CONTINENTS): unit/object draw
#     distance, GLOBAL (affects real players too — units pop in at this range; terrain is
#     client-side and unaffected). Visibility notification cost is O(mutually-visible-players^2),
#     so this is the cheapest big CPU win with dense bots. 80 is barely noticeable on the ground;
#     stock is 100.
#   - PreloadAllNonInstancedMapGrids (.env PRELOAD_MAP_GRIDS): pin every continent grid in RAM at
#     boot (~9 GB). Grids never unload in this fork anyway, so with wandering bots you pay the RAM
#     regardless — preloading just moves the grid+mmap+vmap file I/O out of the map threads'
#     mid-tick path. Needs the RAM headroom; set 0 on small hosts.
#   - Quests.IgnoreAutoAccept=1: skip the auto-accept path bots otherwise hammer.
set_conf "MapUpdate.Threads"               "${MAP_UPDATE_THREADS:-8}"  "$WS_CONF"
set_conf "MapUpdateInterval"               "10" "$WS_CONF"
set_conf "MinWorldUpdateTime"              "1"  "$WS_CONF"
set_conf "CharacterDatabase.WorkerThreads" "${CHAR_DB_WORKER_THREADS:-4}" "$WS_CONF"
set_conf "CharacterDatabase.SynchThreads"  "${CHAR_DB_SYNCH_THREADS:-4}"  "$WS_CONF"
set_conf "Visibility.Distance.Continents"  "${VISIBILITY_DISTANCE_CONTINENTS:-80}" "$WS_CONF"
set_conf "PreloadAllNonInstancedMapGrids"  "${PRELOAD_MAP_GRIDS:-1}"  "$WS_CONF"
set_conf "Quests.IgnoreAutoAccept"         "1"  "$WS_CONF"
#   playerbots.conf:
#   - BotActiveAlone + botActiveAloneSmartScale (.env BOT_ACTIVE_ALONE / BOT_ACTIVE_ALONE_SMART_SCALE):
#     the wiki's "Profile 1 (best for high bot counts)". Only ~BotActiveAlone% of bots run full AI
#     when no real player is near, and SmartScale auto-throttles that further if the tick gets heavy.
#   - botActiveAloneSmartScaleDiffLimitCeiling (.env BOT_SMART_SCALE_CEILING): the world-tick ms at
#     which SmartScale throttles bot activity to ZERO (scaling starts at the 50ms floor). Stock 200
#     silently mass-idles bots once the tick sits there; 300 trades a laggier tick for bots that
#     still do things at high populations.
#   - PlayerbotsDatabase.WorkerThreads/SynchThreads (.env PLAYERBOTS_DB_WORKER_THREADS /
#     PLAYERBOTS_DB_SYNCH_THREADS): stock 1/1; all bot-event writes serialize onto one worker.
#   - RandomBotsPerInterval (.env RANDOM_BOTS_PER_INTERVAL): hard cap on bots the random-bot
#     manager touches per ~20s cycle (logins + maintenance). Stock 60 means a full roster sweep
#     takes N/45*20s (~15 min at 2000 bots, ~37 min at 5000) — scale it with the bot count or
#     teleport/randomize maintenance starves. Runs on the world thread, so raise gradually.
#   - RandomBot*Interval: the module authors' tuned cadence for the random-bot manager.
set_conf "AiPlayerbot.BotActiveAlone"               "${BOT_ACTIVE_ALONE:-10}"   "$PB_CONF"
set_conf "AiPlayerbot.botActiveAloneSmartScale"     "${BOT_ACTIVE_ALONE_SMART_SCALE:-1}"    "$PB_CONF"
set_conf "AiPlayerbot.botActiveAloneSmartScaleDiffLimitCeiling" "${BOT_SMART_SCALE_CEILING:-300}" "$PB_CONF"
set_conf "PlayerbotsDatabase.WorkerThreads"         "${PLAYERBOTS_DB_WORKER_THREADS:-2}"    "$PB_CONF"
set_conf "PlayerbotsDatabase.SynchThreads"          "${PLAYERBOTS_DB_SYNCH_THREADS:-2}"    "$PB_CONF"
set_conf "AiPlayerbot.RandomBotsPerInterval"        "${RANDOM_BOTS_PER_INTERVAL:-150}" "$PB_CONF"
set_conf "AiPlayerbot.RandomBotUpdateInterval"      "20"   "$PB_CONF"
set_conf "AiPlayerbot.RandomBotCountChangeMinInterval" "1800" "$PB_CONF"
set_conf "AiPlayerbot.RandomBotCountChangeMaxInterval" "7200" "$PB_CONF"

# Era-align bot riding to Individual Progression. IP restores original riding REQUIREMENTS at the
# trainer (trainer_spell.ReqLevel: Apprentice 40 / Journeyman 60 / Expert-fly 70), but bots never
# use trainers — PlayerbotFactory learns the riding spells directly and CheckMountStateAction gates
# USE, both keyed off these Use*MountAtMinLevel knobs (stock WotLK 20/40/60/70). Left at stock, bots
# ride at level 20 under IP. Set them to the era values ONLY when IP is on; with IP off the stock
# WotLK levels are correct, so we leave the conf.dist defaults untouched. (See .env IP_BOT_*_LEVEL.)
if [[ "${IP_ENABLE:-1}" == "1" ]]; then
  set_conf "AiPlayerbot.UseGroundMountAtMinLevel"     "${IP_BOT_GROUND_MOUNT_LEVEL:-40}"      "$PB_CONF"
  set_conf "AiPlayerbot.UseFastGroundMountAtMinLevel" "${IP_BOT_FAST_GROUND_MOUNT_LEVEL:-60}" "$PB_CONF"
  set_conf "AiPlayerbot.UseFlyMountAtMinLevel"        "${IP_BOT_FLY_MOUNT_LEVEL:-70}"         "$PB_CONF"
  set_conf "AiPlayerbot.UseFastFlyMountAtMinLevel"    "${IP_BOT_FAST_FLY_MOUNT_LEVEL:-70}"    "$PB_CONF"
fi

# Level-bracket distribution (native to mod-playerbots since upstream 6cc5e3a, which absorbed the
# former mod-player-bot-level-brackets module — that module is no longer cloned): concentrate bots
# around the level(s) real players are at, so your bracket feels busy (instead of 2000 bots spread
# thin across all levels). Re-evaluated every few minutes, so the crowd follows you as you level.
# Weight 10 = strong solo focus (the original module recommended 10-15); SyncFactions makes BOTH
# factions gather at your level too. Upstream ships Enabled=0, so this block is what turns it on.
set_conf "AiPlayerbot.LevelBrackets.Enabled"                        "1"  "$PB_CONF"
set_conf "AiPlayerbot.LevelBrackets.Dynamic.UseDynamicDistribution" "1"  "$PB_CONF"
set_conf "AiPlayerbot.LevelBrackets.Dynamic.RealPlayerWeight"       "10" "$PB_CONF"
set_conf "AiPlayerbot.LevelBrackets.Dynamic.SyncFactions"           "1"  "$PB_CONF"
# The retired module's conf is dead weight once its clone is pruned; drop it so nobody tunes it.
rm -f "$MODETC/mod_player_bot_level_brackets.conf" "$MODETC/mod_player_bot_level_brackets.conf.dist"


# Auction-house economy (mod-ah-bot-plus): a dedicated AH character lists goods and buys
# fairly-priced player auctions. Needs that character's GUID(s) in .env (AHBOT_GUIDS).
AH_CONF="$MODETC/mod_ahbot.conf"
if [[ -f "$AH_CONF" ]]; then
  if [[ -n "${AHBOT_GUIDS:-}" ]]; then
    set_conf "AuctionHouseBot.GUIDs"                        "${AHBOT_GUIDS}" "$AH_CONF"
    set_conf "AuctionHouseBot.EnableSeller"                 "true"           "$AH_CONF"
    set_conf "AuctionHouseBot.Buyer.Enabled"               "true"           "$AH_CONF"
    # 1.0 = pays roughly the item's calculated value; raise (e.g. 1.25) to be more generous.
    set_conf "AuctionHouseBot.Buyer.AcceptablePriceModifier" "1"            "$AH_CONF"

    # Stock depth: more total listings so bought-out goods reappear sooner
    # (refill is ItemsPerCycle=150/min toward this cap; there is no per-item restock).
    set_conf "AuctionHouseBot.Alliance.MaxItems" "25000" "$AH_CONF"
    set_conf "AuctionHouseBot.Horde.MaxItems"    "25000" "$AH_CONF"
    set_conf "AuctionHouseBot.Neutral.MaxItems"  "25000" "$AH_CONF"

    # Listing mix (relative weights per category/quality roll): bias the AH toward a
    # consumable/crafting economy — gems, glyphs, trade goods, reagents up; the
    # weapon/armor flood down. Glyph weight is sized so ~all ~350 WotLK glyphs are
    # statistically always in stock (no hard per-item guarantee exists in the module).
    set_conf "AuctionHouseBot.ListProportion.CategoryGem.QualityUncommon"  "40" "$AH_CONF"
    set_conf "AuctionHouseBot.ListProportion.CategoryGem.QualityRare"      "20" "$AH_CONF"
    set_conf "AuctionHouseBot.ListProportion.CategoryGem.QualityEpic"      "8"  "$AH_CONF"
    set_conf "AuctionHouseBot.ListProportion.CategoryGlyph.QualityNormal"  "60" "$AH_CONF"
    set_conf "AuctionHouseBot.ListProportion.CategoryConsumable.QualityNormal"   "60" "$AH_CONF"
    set_conf "AuctionHouseBot.ListProportion.CategoryConsumable.QualityUncommon" "15" "$AH_CONF"
    set_conf "AuctionHouseBot.ListProportion.CategoryTradeGood.QualityNormal"    "120" "$AH_CONF"
    set_conf "AuctionHouseBot.ListProportion.CategoryTradeGood.QualityUncommon"  "25" "$AH_CONF"
    set_conf "AuctionHouseBot.ListProportion.CategoryTradeGood.QualityRare"      "12" "$AH_CONF"
    set_conf "AuctionHouseBot.ListProportion.CategoryReagent.QualityNormal"      "20" "$AH_CONF"
    set_conf "AuctionHouseBot.ListProportion.CategoryWeapon.QualityNormal"   "5"  "$AH_CONF"
    set_conf "AuctionHouseBot.ListProportion.CategoryWeapon.QualityUncommon" "15" "$AH_CONF"
    set_conf "AuctionHouseBot.ListProportion.CategoryWeapon.QualityRare"     "6"  "$AH_CONF"
    set_conf "AuctionHouseBot.ListProportion.CategoryWeapon.QualityEpic"     "2"  "$AH_CONF"
    set_conf "AuctionHouseBot.ListProportion.CategoryArmor.QualityPoor"      "0"  "$AH_CONF"
    set_conf "AuctionHouseBot.ListProportion.CategoryArmor.QualityNormal"    "10" "$AH_CONF"
    set_conf "AuctionHouseBot.ListProportion.CategoryArmor.QualityUncommon"  "20" "$AH_CONF"
    set_conf "AuctionHouseBot.ListProportion.CategoryArmor.QualityRare"      "10" "$AH_CONF"
    set_conf "AuctionHouseBot.ListProportion.CategoryArmor.QualityEpic"      "3"  "$AH_CONF"
    echo "    AHBot ON (char GUIDs: ${AHBOT_GUIDS}) -> lists goods and buys fairly-priced player auctions."
    echo "      (consumable/crafting-weighted mix: gems+glyphs+trade goods up, weapons/armor down; 25k listings/house)"
  else
    set_conf "AuctionHouseBot.EnableSeller"  "false" "$AH_CONF"
    set_conf "AuctionHouseBot.Buyer.Enabled" "false" "$AH_CONF"
    echo "    AHBOT_GUIDS not set in .env -> AHBot disabled (module still built)."
  fi
fi

# Bot chattiness (.env BOT_CHATTER: low | normal | off; default low). Dials the base
# playerbots canned channel broadcasts (loot/kill/quest spam in World/General/Trade/LFG)
# and the noisiest local talk knobs. Lower = far less channel spam.
case "${BOT_CHATTER:-low}" in
  off)
    PB_BCAST=0;   PB_CHANCE=0;     PB_TALK=0; PB_SUGGEST=0; PB_GUILDRATE=0
    CHATTER_MSG="OFF (bots only reply when you talk to them; no ambient/bot-to-bot chatter)" ;;
  normal)
    PB_BCAST=1;   PB_CHANCE=30000; PB_TALK=1; PB_SUGGEST=1; PB_GUILDRATE=100
    CHATTER_MSG="NORMAL (stock module defaults — very chatty)" ;;
  mild)
    PB_BCAST=1;   PB_CHANCE=7000;  PB_TALK=1; PB_SUGGEST=0; PB_GUILDRATE=40
    CHATTER_MSG="MILD (a notch above low — a bit more ambient + bot-to-bot, still far below normal)" ;;
  *)  # low (default)
    PB_BCAST=1;   PB_CHANCE=3000;  PB_TALK=1; PB_SUGGEST=0; PB_GUILDRATE=25
    CHATTER_MSG="LOW (some flavor, ~90% less channel spam)" ;;
esac
# Base playerbots: enable flag + every global channel chance (0-30000 scale), plus the
# noisiest local talk knobs. All channels share one value so volume scales uniformly
# (zeroing a single channel just reroutes its broadcasts to the others).
set_conf "AiPlayerbot.EnableBroadcasts"        "$PB_BCAST"     "$PB_CONF"
for ch in Guild World General Trade LFG LocalDefense WorldDefense GuildRecruitment; do
  set_conf "AiPlayerbot.BroadcastTo${ch}GlobalChance" "$PB_CHANCE" "$PB_CONF"
done
set_conf "AiPlayerbot.RandomBotTalk"           "$PB_TALK"      "$PB_CONF"
set_conf "AiPlayerbot.RandomBotSuggestDungeons" "$PB_SUGGEST"  "$PB_CONF"
set_conf "AiPlayerbot.GuildRepliesRate"        "$PB_GUILDRATE" "$PB_CONF"
echo "    Bot chatter: ${CHATTER_MSG}"

# ── AI bot chat (mod-playerbot-chatter) ───────────────────────────────────────
# Driven by .env CHATTER_* knobs. When enabled, we PIN AiPlayerbot.RandomBotTalk=0 so
# playerbots' built-in canned ChatReplyAction can't double-answer over the AI (it is
# orthogonal to the command path, so commands/MultiBot are unaffected).
CHATTER_ENABLE="${CHATTER_ENABLE:-0}"
if [[ -f "$PBCHAT_CONF" ]]; then
  set_conf "PlayerbotChatter.Enable"        "$CHATTER_ENABLE"                       "$PBCHAT_CONF"
  set_conf "PlayerbotChatter.Url"           "${CHATTER_URL:-http://${OLLAMA_IP:-localhost}:11434/api/generate}" "$PBCHAT_CONF"
  set_conf "PlayerbotChatter.Model"         "${CHATTER_MODEL:-llama3.1:8b}"         "$PBCHAT_CONF"
  set_conf "PlayerbotChatter.Think"         "${CHATTER_THINK:-0}"                   "$PBCHAT_CONF"
  _CHATTER_SP_DEFAULT='You'"'"'re a real person playing WoW: Wrath of the Lich King (3.3.5a, level cap 80), hanging out in-game and chatting with other players. Type like a normal gamer in chat: short, relaxed, light slang (lol, gg, lfg, lfm, ding, brb, gz, ty, wtb, wts, pst). You'"'"'re here to hang out, not to narrate your play session — talk about the game, the server, other players, your class, hot takes and gripes, the community, or whatever is on your mind, and the occasional totally off-topic real-life aside (tired, it is late, need coffee) is fine too. Don'"'"'t invent fake WoW content or activities that do not exist. Each message quietly tells you your current level, as background only: never announce or tack your level onto what you say (no lvl-42 or level-42 tags), and keep any specific game references to things you would actually know by that level — if you are low or mid level you have NOT been to Northrend, run heroics, or raided (Naxxramas/Ulduar/ToC/ICC) and you never talk as if you have; only level-80 characters talk about heroics, raids, dailies, rep grinds, or endgame PvP. You'"'"'re the person behind the keyboard, not the in-game character or an NPC — no fantasy roleplay voice. Vary how you start; never open with the word anyone. You'"'"'ve got a real personality: easygoing but opinionated, with a sense of humor, not a chipper customer-service bot. Be cranky, dry, and sarcastic when it fits — gripe about bad RNG, repair bills, wipes, grindy rep and dailies, class balance, the dungeon finder, blizzard, other servers, or the community; rib other players and share blunt opinions. Keep a floor though: no slurs, no real-world politics, and never genuinely hostile toward or targeting the person you are talking to — cranky and sarcastic is fine, cruel is not. Still help if someone actually asks (a little sarcasm about it is fine). Vary how the humor lands so you do not sound one-note. Never say you'"'"'re an AI, bot, or game master. No markdown, emojis, asterisk-actions, or quotation marks.'
  CHATTER_SYSTEM_PROMPT="${CHATTER_SYSTEM_PROMPT:-${_CHATTER_SP_DEFAULT}}"
  set_conf "PlayerbotChatter.SystemPrompt" "\"${CHATTER_SYSTEM_PROMPT}\"" "$PBCHAT_CONF"
  set_conf "PlayerbotChatter.MaxConcurrent" "${CHATTER_MAX_CONCURRENT:-3}"          "$PBCHAT_CONF"
  set_conf "PlayerbotChatter.StyleExamplesFile" "${CHATTER_STYLE_EXAMPLES_FILE:-/azerothcore/modules/mod-playerbot-chatter/data/style-examples.txt}" "$PBCHAT_CONF"
  set_conf "PlayerbotChatter.SayRange"      "${CHATTER_SAY_RANGE:-40}"              "$PBCHAT_CONF"
  set_conf "PlayerbotChatter.SayMaxBots"    "${CHATTER_SAY_MAX_BOTS:-2}"            "$PBCHAT_CONF"
  set_conf "PlayerbotChatter.SayChance"     "${CHATTER_SAY_CHANCE:-35}"             "$PBCHAT_CONF"
  set_conf "PlayerbotChatter.GroupMaxBots"  "${CHATTER_GROUP_MAX_BOTS:-2}"          "$PBCHAT_CONF"
  set_conf "PlayerbotChatter.GroupChance"   "${CHATTER_GROUP_CHANCE:-50}"           "$PBCHAT_CONF"
  set_conf "PlayerbotChatter.GroupGuaranteeOne" "${CHATTER_GROUP_GUARANTEE_ONE:-1}" "$PBCHAT_CONF"
  set_conf "PlayerbotChatter.WhisperChance" "${CHATTER_WHISPER_CHANCE:-100}"        "$PBCHAT_CONF"
  set_conf "PlayerbotChatter.HistoryLen"    "${CHATTER_HISTORY_LEN:-10}"            "$PBCHAT_CONF"
  set_conf "PlayerbotChatter.ReplyMaxLen"   "${CHATTER_REPLY_MAXLEN:-200}"          "$PBCHAT_CONF"
  set_conf "PlayerbotChatter.LoreEnable"  "${LORE_ENABLE:-0}"                                   "$PBCHAT_CONF"
  set_conf "PlayerbotChatter.LoreUrl"     "http://ac-lore:${LORE_PORT:-8091}/ask"               "$PBCHAT_CONF"
  set_conf "PlayerbotChatter.LoreTimeout" "${LORE_TIMEOUT:-60}"                                 "$PBCHAT_CONF"
  set_conf "PlayerbotChatter.AmbientEnable"     "${CHATTER_AMBIENT_ENABLE:-$CHATTER_ENABLE}"      "$PBCHAT_CONF"
  set_conf "PlayerbotChatter.AmbientGeneral"    "${CHATTER_AMBIENT_GENERAL:-1}"                   "$PBCHAT_CONF"
  set_conf "PlayerbotChatter.AmbientGroup"      "${CHATTER_AMBIENT_GROUP:-1}"                     "$PBCHAT_CONF"
  set_conf "PlayerbotChatter.AmbientGuild"      "${CHATTER_AMBIENT_GUILD:-1}"                     "$PBCHAT_CONF"
  set_conf "PlayerbotChatter.AmbientSeedMin"    "${CHATTER_AMBIENT_SEED_MIN:-60}"                 "$PBCHAT_CONF"
  set_conf "PlayerbotChatter.AmbientSeedMax"    "${CHATTER_AMBIENT_SEED_MAX:-90}"                 "$PBCHAT_CONF"
  set_conf "PlayerbotChatter.AmbientFollowMin"  "${CHATTER_AMBIENT_FOLLOWUP_MIN:-4}"              "$PBCHAT_CONF"
  set_conf "PlayerbotChatter.AmbientFollowMax"  "${CHATTER_AMBIENT_FOLLOWUP_MAX:-9}"             "$PBCHAT_CONF"
  set_conf "PlayerbotChatter.AmbientActiveWindow" "${CHATTER_AMBIENT_ACTIVE_WINDOW:-75}"          "$PBCHAT_CONF"
  set_conf "PlayerbotChatter.AmbientBotStreakMax" "${CHATTER_AMBIENT_BOT_STREAK_MAX:-4}"          "$PBCHAT_CONF"
  set_conf "PlayerbotChatter.AmbientCooldown"   "${CHATTER_AMBIENT_COOLDOWN:-75}"                 "$PBCHAT_CONF"
  set_conf "PlayerbotChatter.AmbientPerBotCooldown" "${CHATTER_AMBIENT_PER_BOT_COOLDOWN:-120}"    "$PBCHAT_CONF"
  set_conf "PlayerbotChatter.AmbientMaxPerMin"  "${CHATTER_AMBIENT_MAX_PER_MIN:-25}"              "$PBCHAT_CONF"
  set_conf "PlayerbotChatter.AmbientBufferLen"  "${CHATTER_AMBIENT_BUFFER_LEN:-8}"                "$PBCHAT_CONF"
  set_conf "PlayerbotChatter.AmbientWeightBanter" "${CHATTER_AMBIENT_W_BANTER:-40}"               "$PBCHAT_CONF"
  set_conf "PlayerbotChatter.AmbientWeightGeneric" "${CHATTER_AMBIENT_W_GENERIC:-10}"             "$PBCHAT_CONF"
  set_conf "PlayerbotChatter.AmbientWeightReact" "${CHATTER_AMBIENT_W_REACT:-35}"                 "$PBCHAT_CONF"
  set_conf "PlayerbotChatter.AmbientWeightFlavor" "${CHATTER_AMBIENT_W_FLAVOR:-3}"                "$PBCHAT_CONF"
  set_conf "PlayerbotChatter.AmbientWeightEvent" "${CHATTER_AMBIENT_W_EVENT:-8}"                  "$PBCHAT_CONF"

  if [[ "$CHATTER_ENABLE" == "1" ]]; then
    # Hand the ENTIRE self-initiated voice to mod-playerbot-chatter: silence playerbots'
    # canned chat-reply engine (RandomBotTalk), its event/flavor broadcaster
    # (EnableBroadcasts: ding/quest/loot/kill), and unsolicited masterless talk. These are
    # all orthogonal to the command path, so follow/trade/MultiBot are unaffected. Physical
    # emotes (RandomBotEmote) are left as-is — they are not chat.
    set_conf "AiPlayerbot.RandomBotTalk"            "0" "$PB_CONF"
    set_conf "AiPlayerbot.EnableBroadcasts"         "0" "$PB_CONF"
    set_conf "AiPlayerbot.RandomBotSayWithoutMaster" "0" "$PB_CONF"
    echo "    AI chat ON -> pinned RandomBotTalk=0, EnableBroadcasts=0, RandomBotSayWithoutMaster=0"
    echo "      (playerbots' canned + broadcast voice off; mod-playerbot-chatter owns chatter)."
  fi
fi

# ── Raid roster prerequisites (mod-raid-roster) ──────────────────────────────
# These addclass / gear-ceiling / max-bots knobs live in playerbots.conf ($PB_CONF)
# but are prerequisites for mod-raid-roster's .raidroster commands.
#   AddClassCommand=1          — enables the addclass pool that .raidroster create draws from.
#   AddClassAccountPoolSize    — characters per faction per class in that pool; must hold at
#                                least 5 chars per class for a 40-man (2×Warrior, 2×Druid, …).
#                                Default 50 gives comfortable headroom.
#   AutoInitEquipLevelLimitRatio — the gear-ceiling multiplier used by .raidroster sync and
#                                init=auto: 1.0 = match your gear score exactly (prevents bots
#                                ever being overgeared relative to you).
#   MaxAddedBots               — hard cap on bots you can have online at once; must be ≥ 39
#                                (the bots in a 40-man), 60 leaves headroom for manual adds.
# NOTE: RandomBotAccountCount is NOT explicitly set by this setup.sh (left to the core default
# = automatic). The playerbots.conf.dist caveat requires it to be >=  MaxRandomBots/10 +
# AddClassAccountPoolSize. With the core auto-calc this should hold for typical bot counts,
# but if you pin RandomBotAccountCount manually in .env, verify it satisfies that formula.
# Gated: only touch playerbots pool/bot-count config when the module is enabled, so a
# disabled mod-raid-roster never mutates an existing install's playerbots settings.
if [[ "${RAIDROSTER_ENABLE:-0}" == "1" ]]; then
  set_conf "AiPlayerbot.AddClassCommand"              "1"                          "$PB_CONF"
  set_conf "AiPlayerbot.AddClassAccountPoolSize"      "${ADDCLASS_POOL_SIZE:-50}"  "$PB_CONF"
  set_conf "AiPlayerbot.AutoInitEquipLevelLimitRatio" "${GEAR_MATCH_RATIO:-1.0}"   "$PB_CONF"
  set_conf "AiPlayerbot.MaxAddedBots"                 "${MAX_ADDED_BOTS:-60}"      "$PB_CONF"
fi

RAID_CONF="$MODETC/mod_raid_roster.conf"
if [[ -f "$RAID_CONF" ]]; then
  set_conf "RaidRoster.Enable" "${RAIDROSTER_ENABLE:-0}" "$RAID_CONF"
fi

# ── AH price lookup (mod-ahbot-price) ────────────────────────────────────────
# Read-only .ahprice command + AHPrice addon: shows the buy-value RANGE the AH bot
# will pay for an item, computed from the live AuctionHouseBot.* config. Built always;
# inert unless enabled. Useful whether or not the AHBot buyer itself is on.
AHPRICE_CONF="$MODETC/mod_ahbot_price.conf"
if [[ -f "$AHPRICE_CONF" ]]; then
  set_conf "AHBotPrice.Enable"            "${AHPRICE_ENABLE:-0}"              "$AHPRICE_CONF"
  set_conf "AHBotPrice.HideUnauctionable" "${AHPRICE_HIDE_UNAUCTIONABLE:-1}"  "$AHPRICE_CONF"
fi

# ── Wintergrasp bots (mod-wintergrasp-bots) ──────────────────────────────────
# Self-sustaining playerbot participation in Wintergrasp: a world-thread director
# pulls eligible random bots into the battle so a solo player has enemies to fight
# and can rank up. Built always; inert unless enabled.
WGBOTS_CONF="$MODETC/mod_wintergrasp_bots.conf"
if [[ -f "$WGBOTS_CONF" ]]; then
  set_conf "WintergraspBots.Enable"     "${WGBOTS_ENABLE:-0}"       "$WGBOTS_CONF"
  set_conf "WintergraspBots.PerFaction" "${WGBOTS_PER_FACTION:-15}" "$WGBOTS_CONF"
  set_conf "WintergraspBots.MinLevel"   "${WGBOTS_MIN_LEVEL:-75}"   "$WGBOTS_CONF"
  set_conf "WintergraspBots.TickMs"     "${WGBOTS_TICK_MS:-5000}"   "$WGBOTS_CONF"
  set_conf "WintergraspBots.Debug"      "${WGBOTS_DEBUG:-0}"        "$WGBOTS_CONF"
  set_conf "WintergraspBots.WorkshopCapture" "${WGBOTS_WORKSHOP_CAPTURE:-1}" "$WGBOTS_CONF"
  set_conf "WintergraspBots.ArriveRadius"    "${WGBOTS_ARRIVE_RADIUS:-8.0}"  "$WGBOTS_CONF"
  set_conf "WintergraspBots.StuckSeconds"    "${WGBOTS_STUCK_SECONDS:-12}"   "$WGBOTS_CONF"
  set_conf "WintergraspBots.StuckEpsilon"    "${WGBOTS_STUCK_EPSILON:-3.0}"  "$WGBOTS_CONF"
  set_conf "WintergraspBots.ReassertMove"    "${WGBOTS_REASSERT_MOVE:-1}"    "$WGBOTS_CONF"
  set_conf "WintergraspBots.EngageVehicleRadius" "${WGBOTS_ENGAGE_VEHICLE_RADIUS:-80}" "$WGBOTS_CONF"
  set_conf "WintergraspBots.DefenseShare"    "${WGBOTS_DEFENSE_SHARE:-60}"   "$WGBOTS_CONF"
  set_conf "WintergraspBots.FieldGarrison"   "${WGBOTS_FIELD_GARRISON:-1}"   "$WGBOTS_CONF"
  set_conf "WintergraspBots.TurretCrewMax"   "${WGBOTS_TURRET_CREW_MAX:-4}"    "$WGBOTS_CONF"
  set_conf "WintergraspBots.RallyPerVehicle" "${WGBOTS_RALLY_PER_VEHICLE:-3}"  "$WGBOTS_CONF"
  set_conf "WintergraspBots.ThreatRadius"    "${WGBOTS_THREAT_RADIUS:-350}"    "$WGBOTS_CONF"
  set_conf "WintergraspBots.PatrolRadius"    "${WGBOTS_PATROL_RADIUS:-12}"     "$WGBOTS_CONF"
  set_conf "WintergraspBots.SortieQuorum"    "${WGBOTS_SORTIE_QUORUM:-4}"      "$WGBOTS_CONF"
  set_conf "WintergraspBots.CaptureSquad"    "${WGBOTS_CAPTURE_SQUAD:-5}"      "$WGBOTS_CONF"
  set_conf "WintergraspBots.SiegeEnable"     "${WGBOTS_SIEGE_ENABLE:-1}"        "$WGBOTS_CONF"
  set_conf "WintergraspBots.HumanReserve"    "${WGBOTS_SIEGE_HUMAN_RESERVE:-2}" "$WGBOTS_CONF"
  set_conf "WintergraspBots.RecrewRadius"       "${WGBOTS_RECREW_RADIUS:-40}"        "$WGBOTS_CONF"
  set_conf "WintergraspBots.VehicleReapSeconds" "${WGBOTS_VEHICLE_REAP_SECONDS:-30}" "$WGBOTS_CONF"
  set_conf "WintergraspBots.KeepScreen"         "${WGBOTS_KEEP_SCREEN:-4}"           "$WGBOTS_CONF"
fi

echo "==> 7/10 Hardening auth/world for external exposure"
# Authserver brute-force lockout. The shipped default is WrongPass.MaxCount=0 — i.e. UNLIMITED
# password guesses, which is fine on a trusted LAN but unacceptable once 3724 faces the internet.
# Lock the ACCOUNT (not the IP) after a few bad passwords so a fat-fingered login can't lock your
# friend's whole IP out; the firewall ACL is what keeps strangers off the port in the first place.
AS_CONF="$ETC/authserver.conf"
if [[ -f "$AS_CONF" ]]; then
  set_conf "WrongPass.MaxCount" "5"   "$AS_CONF"
  set_conf "WrongPass.BanTime"  "600" "$AS_CONF"
  set_conf "WrongPass.BanType"  "0"   "$AS_CONF"   # 0 = ban account, 1 = ban IP
  set_conf "WrongPass.Logging"  "1"   "$AS_CONF"
  echo "    authserver: account locked for 600s after 5 wrong passwords (logged)."
else
  echo "    NOTE: $AS_CONF not present yet — set WrongPass.* by hand if it appears later."
fi
# SOAP is a remote GM-command HTTP console. We already don't publish its port; also keep it
# disabled and loopback-bound in-config as defense in depth.
set_conf "SOAP.Enabled" "0"             "$WS_CONF"
set_conf "SOAP.IP"      "\"127.0.0.1\"" "$WS_CONF"

echo "==> 8/10 Detecting LAN IP and setting the realm address"
# LAN_IP can be pre-set (e.g. by the Windows installer or in .env) to force the realm address;
# otherwise autodetect from the default route. Inside WSL2, autodetect returns the VM's NAT IP,
# so the Windows installer sets LAN_IP to the real Windows host IP.
LAN_IP="${LAN_IP:-$(ip route get 1.1.1.1 2>/dev/null | awk '{print $7; exit}')}"
[[ -n "${LAN_IP:-}" ]] || LAN_IP="$(hostname -I | awk '{print $1}')"
echo "    Server LAN IP: $LAN_IP"
# realmlist hands each connecting client an address to reach the worldserver. With
# PUBLIC_REALM_ADDRESS set (your WAN IP, or a DDNS hostname if your WAN IP is dynamic), REMOTE
# clients get that address while LAN clients still get the private IP via localAddress/
# localSubnetMask — so opening up for a friend doesn't break players at home via NAT hairpin.
# Keep your forwarded EXTERNAL world port equal to 8085 (the realm port is single-valued and is
# used by both LAN and remote clients). Leave PUBLIC_REALM_ADDRESS blank to stay LAN-only.
REALM_ADDR="${PUBLIC_REALM_ADDRESS:-$LAN_IP}"
docker compose exec -T ac-database \
  mysql -uroot -p"${DOCKER_DB_ROOT_PASSWORD}" \
  -e "UPDATE acore_auth.realmlist
        SET address='${REALM_ADDR}', localAddress='${LAN_IP}',
            localSubnetMask='255.255.255.0', port=${DOCKER_WORLD_EXTERNAL_PORT:-8085},
            icon=1
      WHERE id=1;"
if [[ -n "${PUBLIC_REALM_ADDRESS:-}" ]]; then
  echo "    Realm address: external='${REALM_ADDR}' (world port ${DOCKER_WORLD_EXTERNAL_PORT:-8085}), LAN='${LAN_IP}'."
else
  echo "    Realm address: ${LAN_IP} (LAN-only; set PUBLIC_REALM_ADDRESS in .env to expose)."
fi

# Dual-spec trainer cost (.env FREE_DUAL_SPEC). The 1000g is data, not core: it's the
# BoxMoney on the "Purchase a Dual Talent Specialization" gossip option (OptionType 18).
# Idempotent + reversible — flip the knob and re-run to restore the retail cost.
if [[ "${FREE_DUAL_SPEC:-1}" == "1" ]]; then
  DUALSPEC_COST=0;        echo "    Dual spec FREE (1000g trainer cost removed)."
else
  DUALSPEC_COST=10000000; echo "    Dual spec at retail 1000g cost."
fi
docker compose exec -T ac-database \
  mysql -uroot -p"${DOCKER_DB_ROOT_PASSWORD}" \
  -e "UPDATE acore_world.gossip_menu_option SET BoxMoney=${DUALSPEC_COST} WHERE OptionType=18;"

# Molten Core douse cooldown (.env MC_DOUSE_NO_COOLDOWN). Eternal Quintessence
# (22754) ships a 3600000ms (1h) cooldown that blocks dousing all 7 Runes of
# Warding in a single clear to reach Majordomo. Aqual Quintessence (17333) already
# has none (gated by charges instead), so only 22754 needs touching.
# Idempotent + reversible — flip the knob and re-run to restore the retail cooldown.
if [[ "${MC_DOUSE_NO_COOLDOWN:-1}" == "1" ]]; then
  MC_DOUSE_CD=0;       MC_DOUSE_CATCD=-1;      echo "    MC douse cooldown REMOVED (Eternal Quintessence)."
else
  MC_DOUSE_CD=3600000; MC_DOUSE_CATCD=3600000; echo "    MC douse cooldown at retail 1h."
fi
docker compose exec -T ac-database \
  mysql -uroot -p"${DOCKER_DB_ROOT_PASSWORD}" \
  -e "UPDATE acore_world.item_template
        SET spellcooldown_1=${MC_DOUSE_CD}, spellcategorycooldown_1=${MC_DOUSE_CATCD}
      WHERE entry=22754;"

# Darkmoon Faire continuous rotation (.env DARKMOON_CONTINUOUS). Stock 3.3.5a runs
# the Faire ~1 week/month with a ~3-week gap, holiday-linked to the client calendar.
# This detaches the three Faire events from the holiday calendar (holiday=0) and
# phase-offsets their start_time by 7 days each, so exactly one location is always
# up: Elwynn (event 4) -> Mulgore (5) -> Terokkar (3), 1 week each on a 21-day cycle,
# no gap/overlap. length=10080 (7d), occurence=30240 (21d). The three "Building"
# setup pre-events (23/71/77) are disabled (start pushed to 2038 + holiday detached)
# so each Faire appears fully-formed. end_time=NULL => End=now+2yr, recomputed each
# boot, so it never expires on a server that restarts. Idempotent + reversible: flip
# the knob and re-run. game_event reloads on the step 9/10 worldserver restart below.
# The Calendar UI won't advertise it (client-side Holidays.dbc, unpatched); the Faire
# is fully present in-world regardless.
if [[ "${DARKMOON_CONTINUOUS:-1}" == "1" ]]; then
  echo "    Darkmoon Faire CONTINUOUS (Elwynn->Mulgore->Terokkar, 1 week each, no gap)."
  docker compose exec -T ac-database \
    mysql -uroot -p"${DOCKER_DB_ROOT_PASSWORD}" acore_world <<'SQL'
UPDATE game_event SET start_time='2024-01-01 00:00:00', end_time=NULL, occurence=30240, length=10080, holiday=0, holidayStage=0 WHERE eventEntry=4;
UPDATE game_event SET start_time='2024-01-08 00:00:00', end_time=NULL, occurence=30240, length=10080, holiday=0, holidayStage=0 WHERE eventEntry=5;
UPDATE game_event SET start_time='2024-01-15 00:00:00', end_time=NULL, occurence=30240, length=10080, holiday=0, holidayStage=0 WHERE eventEntry=3;
UPDATE game_event SET start_time='2038-01-01 00:00:00', end_time=NULL, holiday=0, holidayStage=0 WHERE eventEntry IN (23,71,77);
SQL
else
  echo "    Darkmoon Faire at retail cadence (monthly, holiday-linked)."
  docker compose exec -T ac-database \
    mysql -uroot -p"${DOCKER_DB_ROOT_PASSWORD}" acore_world <<'SQL'
UPDATE game_event SET start_time=NULL, end_time=NULL, occurence=131040, length=10079, holiday=374, holidayStage=2 WHERE eventEntry=4;
UPDATE game_event SET start_time=NULL, end_time=NULL, occurence=131040, length=10079, holiday=375, holidayStage=2 WHERE eventEntry=5;
UPDATE game_event SET start_time=NULL, end_time=NULL, occurence=131040, length=10079, holiday=376, holidayStage=2 WHERE eventEntry=3;
UPDATE game_event SET start_time=NULL, end_time=NULL, occurence=131040, length=4320, holiday=374, holidayStage=1 WHERE eventEntry=23;
UPDATE game_event SET start_time=NULL, end_time=NULL, occurence=131040, length=4320, holiday=375, holidayStage=1 WHERE eventEntry=71;
UPDATE game_event SET start_time=NULL, end_time=NULL, occurence=131040, length=4320, holiday=376, holidayStage=1 WHERE eventEntry=77;
SQL
fi

# "Stave of the Ancients" fast respawn (.env STAVE_FAST_RESPAWN). The four demonic
# corrupters for the mage quest (7636) are killed by talking to a neutral "disguise"
# NPC that then turns hostile: Simone the Inconspicuous (14527, + her cat Precious
# 14528), Franklin the Friendly (14529), Artorius the Amiable (14531), Nelson the
# Nice (14536). The demons themselves are transforms, not spawns, so the disguise
# NPC's spawntimesecs governs the wait after a failed solo attempt (retail 600s, or
# 900s for Franklin). This shrinks it to 30s for quick retries. (Live schema keys
# the creature spawn table on `id`, not `id1`.) Idempotent + reversible: flip the
# knob and re-run to restore the retail timers.
if [[ "${STAVE_FAST_RESPAWN:-1}" == "1" ]]; then
  echo "    Stave of the Ancients demons respawn FAST (30s)."
  docker compose exec -T ac-database \
    mysql -uroot -p"${DOCKER_DB_ROOT_PASSWORD}" \
    -e "UPDATE acore_world.creature SET spawntimesecs=30
          WHERE id IN (14527,14528,14529,14531,14536);"
else
  echo "    Stave of the Ancients demons at retail respawn timers."
  docker compose exec -T ac-database \
    mysql -uroot -p"${DOCKER_DB_ROOT_PASSWORD}" acore_world <<'SQL'
UPDATE creature SET spawntimesecs=600 WHERE id IN (14527,14528,14531,14536);
UPDATE creature SET spawntimesecs=900 WHERE id=14529;
SQL
fi

# --- Registration website (ac-webreg) ----------------------------------------
# Opt-in: only wired up when WEBREG_ADMIN_PASS is set in .env. Adds a container
# to the generated override, a least-privilege MySQL user, and autogenerated
# secrets. The site is reached via the operator's Cloudflare tunnel.
if [[ -n "${WEBREG_ADMIN_PASS:-}" ]]; then
  echo "==> Configuring registration website (ac-webreg)"

  # Autogenerate secrets if blank (idempotent). persist_env writes BOTH the repo-root .env
  # (source of truth — survives the next run's copy) and the live env (used this run).
  if [[ -z "${WEBREG_SESSION_SECRET:-}" ]]; then
    WEBREG_SESSION_SECRET="$(openssl rand -hex 32)"
    persist_env WEBREG_SESSION_SECRET "$WEBREG_SESSION_SECRET"
  fi
  if [[ -z "${WEBREG_DB_PASS:-}" ]]; then
    WEBREG_DB_PASS="$(openssl rand -hex 24)"
    persist_env WEBREG_DB_PASS "$WEBREG_DB_PASS"
  fi

  # Least-privilege MySQL user: rights only on acore_auth.account.
  docker compose exec -T ac-database \
    mysql -uroot -p"${DOCKER_DB_ROOT_PASSWORD}" <<SQL
CREATE USER IF NOT EXISTS 'webreg'@'%' IDENTIFIED BY '${WEBREG_DB_PASS}';
ALTER USER 'webreg'@'%' IDENTIFIED BY '${WEBREG_DB_PASS}';
GRANT SELECT, INSERT, UPDATE ON acore_auth.account TO 'webreg'@'%';
GRANT SELECT, INSERT, UPDATE ON acore_auth.account_banned TO 'webreg'@'%';
FLUSH PRIVILEGES;
SQL

  # Append the service to the generated override (the file already exists).
  cat >> "$AC_DIR/docker-compose.override.yml" <<YAML
  ac-webreg:
    build: ./../webreg
    restart: unless-stopped
    # Join the base compose's network so 'ac-database' resolves via Docker DNS.
    # Without this the service lands on the default network, the DB name fails to
    # resolve, and the resolver may return an ISP catch-all IP.
    networks:
      - ac-network
    depends_on:
      - ac-database
    environment:
      WEBREG_LISTEN: "0.0.0.0:8090"
      WEBREG_SITE_NAME: "\${WEBREG_SITE_NAME:-WoW Server}"
      WEBREG_ADMIN_USER: "\${WEBREG_ADMIN_USER:-admin}"
      WEBREG_ADMIN_PASS: "\${WEBREG_ADMIN_PASS}"
      WEBREG_SESSION_SECRET: "\${WEBREG_SESSION_SECRET}"
      WEBREG_DB_HOST: "ac-database"
      WEBREG_DB_PASS: "\${WEBREG_DB_PASS}"
      WEBREG_CLIENT_ZIP_PATH: "/data/client.zip"
      WEBREG_CLIENT_ZIP_LABEL: "\${CLIENT_ZIP_LABEL:-Download client}"
      WEBREG_ADDONS_ZIP_PATH: "/data/addons.zip"
      WEBREG_ADDONS_ZIP_LABEL: "\${ADDONS_ZIP_LABEL:-Download bot addons}"
      WEBREG_BOT_PREFIX: "\${WEBREG_BOT_PREFIX:-rndbot}"
    ports:
      - "\${WEBREG_LAN_PORT:-8090}:8090"
    volumes:
      - "\${CLIENT_ZIP_PATH:-/dev/null}:/data/client.zip:ro"
      - "\${ADDONS_ZIP_PATH:-/dev/null}:/data/addons.zip:ro"
YAML
  # The main `docker compose up` (above) ran before this service was appended and
  # before its secrets existed, so it must be built + started now. Idempotent:
  # re-running reconciles the container with the regenerated override.
  # If fetch-client-addons.sh has produced the bundle, mount it by default so the
  # "Download bot addons" button works without editing .env. An explicit
  # ADDONS_ZIP_PATH still wins; absent, the compose default (/dev/null) applies.
  if [[ -z "${ADDONS_ZIP_PATH:-}" && -f "$ROOT/client-addons.zip" ]]; then
    export ADDONS_ZIP_PATH="$ROOT/client-addons.zip"
  fi
  echo "    Building and starting ac-webreg..."
  docker compose up -d --build ac-webreg
  echo "    ac-webreg up (LAN port ${WEBREG_LAN_PORT:-8090}); point your Cloudflare tunnel here."
else
  # Tear down a previously-enabled site if it was turned off (override no longer
  # declares it; remove any lingering container by name).
  docker rm -f ac-webreg >/dev/null 2>&1 || true
  echo "==> Registration website disabled (WEBREG_ADMIN_PASS blank); skipping ac-webreg."
fi

# --- Lore sidecar (ac-lore) ---------------------------------------------------
# Opt-in via .env LORE_ENABLE=1. Adds a Python container that answers whispered
# factual questions from real world data. Least-privilege read-only MySQL user;
# reaches the host's Ollama via OLLAMA_IP, same as the chatter module.
if [[ "${LORE_ENABLE:-0}" == "1" ]]; then
  echo "==> Configuring lore sidecar (ac-lore)"

  # Autogenerate the sidecar DB password if blank (idempotent). persist_env writes BOTH the
  # repo-root .env (source of truth) and the live env (used this run).
  if [[ -z "${LORE_DB_PASS:-}" ]]; then
    LORE_DB_PASS="$(openssl rand -hex 24)"
    persist_env LORE_DB_PASS "$LORE_DB_PASS"
  fi

  # Least-privilege MySQL user: read-only on acore_world.
  docker compose exec -T ac-database \
    mysql -uroot -p"${DOCKER_DB_ROOT_PASSWORD}" <<SQL
CREATE USER IF NOT EXISTS 'lore'@'%' IDENTIFIED BY '${LORE_DB_PASS}';
ALTER USER 'lore'@'%' IDENTIFIED BY '${LORE_DB_PASS}';
GRANT SELECT ON acore_world.* TO 'lore'@'%';
FLUSH PRIVILEGES;
SQL

  # Append the service to the generated override (the file already exists).
  cat >> "$AC_DIR/docker-compose.override.yml" <<YAML
  ac-lore:
    build: ./../lore-sidecar
    restart: unless-stopped
    # Join the base compose network so 'ac-database' resolves and the worldserver
    # can reach this service by name (ac-lore).
    networks:
      - ac-network
    depends_on:
      - ac-database
    environment:
      LORE_LISTEN: "0.0.0.0:${LORE_PORT:-8091}"
      LORE_DB_HOST: "ac-database"
      LORE_DB_USER: "lore"
      LORE_DB_PASS: "\${LORE_DB_PASS}"
      LORE_DB_WORLD: "acore_world"
      LORE_OLLAMA_URL: "http://\${OLLAMA_IP:-host.docker.internal}:11434"
      LORE_MODEL: "\${LORE_MODEL:-llama3.1:8b}"
      LORE_DEBUG: "\${LORE_DEBUG:-0}"
      LORE_GEN_TIMEOUT: "\${LORE_GEN_TIMEOUT:-60}"
YAML

  echo "    Building and starting ac-lore..."
  docker compose up -d --build ac-lore
  echo "    ac-lore up (worldserver reaches it at http://ac-lore:${LORE_PORT:-8091}/ask)."
else
  docker rm -f ac-lore >/dev/null 2>&1 || true
  echo "==> Lore sidecar disabled (LORE_ENABLE != 1); skipping ac-lore."
fi

echo "==> 9/10 Restarting worldserver to apply config"
docker compose restart ac-worldserver

echo "==> 10/10 Installing nightly database-backup cron job"
# Idempotent: a marked line is replaced on re-run. Skip with BACKUP_CRON=0.
# Change timing by editing BACKUP_SCHEDULE (standard cron syntax).
BACKUP_SCHEDULE="${BACKUP_SCHEDULE:-0 4 * * *}"
mkdir -p "$ROOT/backups"
if [[ "${BACKUP_CRON:-1}" != "1" ]]; then
  echo "    BACKUP_CRON=0 -> skipped. Run ./backup.sh manually or add cron later."
elif command -v crontab >/dev/null 2>&1; then
  CRON_MARK="# azerothcore-backup"
  # cron runs commands with a minimal PATH; docker usually lives in /usr/local/bin
  # or /usr/bin, neither guaranteed present. Pin a PATH so backup.sh finds it.
  CRON_LINE="${BACKUP_SCHEDULE} PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin $ROOT/backup.sh >> $ROOT/backups/backup.log 2>&1 ${CRON_MARK}"
  # Drop any previous marked line, then add the current one.
  # `|| true` is load-bearing: on a host with no existing crontab, `crontab -l`
  # and the following grep both exit non-zero, which under `set -euo pipefail`
  # would abort this subshell *before* the echo and silently install nothing
  # (the original "no backup cron was ever created" bug).
  { crontab -l 2>/dev/null | grep -vF "$CRON_MARK" || true; echo "$CRON_LINE"; } | crontab -
  echo "    Nightly backup scheduled: '${BACKUP_SCHEDULE}' (edit with: crontab -e)"
else
  echo "    NOTE: 'crontab' not found — install cron, or run ./backup.sh on your own schedule."
fi

cat <<EOF

==================================================================
 Server is up. Realm address: ${LAN_IP}
 On each WoW 3.3.5a client, set realmlist.wtf to:
     set realmlist ${LAN_IP}
 Create accounts:  docker attach ac-worldserver   (then see README)
==================================================================
EOF
