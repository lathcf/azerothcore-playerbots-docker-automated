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
#   mod-player-bot-level-brackets .. spread random bots across level ranges (living world)
#   mod-junk-to-gold .............. auto-sell gray trash (less bag clutter for bots/players)
#   mod-multibot-bridge ........... server half of the in-game "MultiBot" control addon
MODULES=(
  "mod-playerbots|https://github.com/mod-playerbots/mod-playerbots.git"
  # mod-aoe-loot DISABLED 2026-06-21: its area-loot aggregation invalidates pending
  # group-loot rolls when looting a pile of corpses at once — an item you roll Need on
  # vanishes from the corpse and nobody receives it (cf. mod-aoe-loot#43/#44). The
  # reconcile below prunes the existing clone on the next run. Uncomment to restore.
  # "mod-aoe-loot|https://github.com/azerothcore/mod-aoe-loot.git"
  "mod-player-bot-level-brackets|https://github.com/DustinHendrickson/mod-player-bot-level-brackets.git"
  "mod-junk-to-gold|https://github.com/noisiver/mod-junk-to-gold.git"
  "mod-multibot-bridge|https://github.com/Wishmaster117/mod-multibot-bridge.git"
  "mod-ah-bot-plus|https://github.com/NathanHandley/mod-ah-bot-plus.git"
)

# Modules we author and ship from THIS repo (copied in, not git-cloned). Kept by the reconcile.
LOCAL_MODULES=( "mod-playerbot-chatter" )

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
}

echo "==> 1/10 Cloning AzerothCore playerbots fork (if missing)"
[[ -d "$AC_DIR/.git" ]] || git clone "$FORK_URL" --branch="$FORK_BRANCH" "$AC_DIR"
apply_pin "$AC_DIR" "azerothcore-wotlk"
apply_patches

echo "==> 2/10 Cloning modules (if missing)"
for entry in "${MODULES[@]}"; do
  name="${entry%%|*}"; url="${entry#*|}"
  [[ -d "$AC_DIR/modules/$name/.git" ]] || git clone "$url" "$AC_DIR/modules/$name"
  apply_pin "$AC_DIR/modules/$name" "$name"
done

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

# Sync in-repo modules into the build tree (fresh copy each run so edits propagate).
for lm in "${LOCAL_MODULES[@]}"; do
  if [[ -d "$ROOT/modules/$lm" ]]; then
    echo "    Syncing local module: $lm"
    rm -rf "$AC_DIR/modules/$lm"
    cp -a "$ROOT/modules/$lm" "$AC_DIR/modules/$lm"
  fi
done

echo "==> 3/10 Creating/refreshing .env and container UID/GID"
if [[ ! -f "$AC_DIR/.env" ]]; then
  cp "$ROOT/.env.example" "$AC_DIR/.env"
  echo "    Created $AC_DIR/.env — review DOCKER_DB_ROOT_PASSWORD before public exposure."
fi

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
cat > "$AC_DIR/docker-compose.override.yml" <<'YAML'
# Generated by setup.sh — do not edit by hand.
services:
  ac-worldserver:
    environment:
      TZ: "${SERVER_TZ:-Etc/UTC}"
    ports: !override
      - "${DOCKER_WORLD_EXTERNAL_PORT:-8085}:8085"
    volumes:
      - ./modules:/azerothcore/modules:ro
  ac-db-import:
    environment:
      TZ: "${SERVER_TZ:-Etc/UTC}"
    volumes:
      - ./modules:/azerothcore/modules:ro
  ac-database:
    environment:
      TZ: "${SERVER_TZ:-Etc/UTC}"
    ports: !override
      - "127.0.0.1:${DOCKER_DB_EXTERNAL_PORT:-3306}:3306"
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
CNF
# Must NOT be world-writable or mysqld ignores it ("World-writable config file is ignored").
chmod 0644 "$AC_DIR/config/mysql-tuning.cnf"

# Safety: never let the server go public with the placeholder DB password. PUBLIC_REALM_ADDRESS
# being set is our "exposing this to the internet" signal — if it's set, the root password must
# have been changed from the shipped default first.
if [[ -n "${PUBLIC_REALM_ADDRESS:-}" ]]; then
  if [[ "${DOCKER_DB_ROOT_PASSWORD:-}" == "changeme_db_password" || -z "${DOCKER_DB_ROOT_PASSWORD:-}" ]]; then
    echo "ERROR: PUBLIC_REALM_ADDRESS is set (external exposure) but DOCKER_DB_ROOT_PASSWORD is" >&2
    echo "       still the shipped default. Set a strong, unique password in $AC_DIR/.env first." >&2
    exit 1
  fi
fi

cd "$AC_DIR"

echo "==> 4/10 Building & starting the stack (first run compiles core+modules,"
echo "          imports the DB, and downloads client data — this can take a while)"
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
for _dist in "$MODETC"/*.conf.dist; do
  [[ -e "$_dist" ]] || continue           # glob didn't match (no .dist files yet) -> skip
  ensure_conf "$(basename "$_dist" .conf.dist)"
done

WS_CONF="$ETC/worldserver.conf"
PB_CONF="$MODETC/playerbots.conf"
PBCHAT_CONF="$MODETC/mod_playerbot_chatter.conf"

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
# Boosted rates (XP-focused; loot balance left mostly intact). Tune freely.
set_conf "Rate.XP.Kill"         "3" "$WS_CONF"
set_conf "Rate.XP.Quest"        "3" "$WS_CONF"
set_conf "Rate.XP.Explore"      "3" "$WS_CONF"
set_conf "Rate.XP.Pet"          "3" "$WS_CONF"
set_conf "Rate.Drop.Money"      "2" "$WS_CONF"
set_conf "Rate.Reputation.Gain" "5" "$WS_CONF"
# Rested-XP pool fill rate (the blue "rested" bonus that doubles kill XP until spent).
# InGame = while logged in resting in an inn/city; Offline = while logged off (tavern/city
# vs wilderness). 3x so the pool refills fast and more of your killing is doubled. MaxBonus
# (default 1.5) is the *cap* on banked rested XP, not a speed — raise it if you want a bigger
# buffer to store while away.
set_conf "Rate.Rest.InGame"                 "3" "$WS_CONF"
set_conf "Rate.Rest.Offline.InTavernOrCity" "3" "$WS_CONF"
set_conf "Rate.Rest.Offline.InWilderness"   "3" "$WS_CONF"
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
# Roaming bot population (from .env MAX_RANDOM_BOTS, default 2000). Account count 0 = automatic.
# Only ~BotActiveAlone% run full AI when no real player is near, and SmartScale auto-throttles
# if the server tick gets heavy — but more online bots still cost more RAM and CPU.
BOTS="${MAX_RANDOM_BOTS:-2000}"
set_conf "AiPlayerbot.Enabled"            "1"      "$PB_CONF"
set_conf "AiPlayerbot.RandomBotAutologin" "1"      "$PB_CONF"
set_conf "AiPlayerbot.MinRandomBots"      "$BOTS"  "$PB_CONF"
set_conf "AiPlayerbot.MaxRandomBots"      "$BOTS"  "$PB_CONF"
# Required by mod-player-bot-level-brackets: bots must keep their random levels.
set_conf "AiPlayerbot.DisableRandomLevels" "0"   "$PB_CONF"
# Disable gear/spec persistence: it's incompatible with mod-player-bot-level-brackets.
# The brackets module constantly re-levels bots to follow the player population, including
# DEMOTING them (e.g. 45 -> 7) via PlayerbotFactory::Randomize(false). With persistence ON
# (the playerbots default), Randomize skips ClearAllItems() for any bot at/above the
# persistence level, so a demoted bot keeps its old high-level gear; InitEquipment only
# swaps slots where it happens to find a level-appropriate item, leaving the rest overleveled
# (the "level 7 bot in level 45 gear" symptom). Off = every re-level re-gears from scratch.
set_conf "AiPlayerbot.EquipAndSpecPersistence" "0" "$PB_CONF"
# Battlegrounds & arenas.
# On-demand (the key setting): with RandomBotJoinBG=1, when YOU queue any BG/arena at ANY
# level, bots at your bracket fill it — and mod-player-bot-level-brackets keeps bots at every
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
set_conf "AiPlayerbot.RandomBotAutoJoinBGRatedArena2v2Count" "1" "$PB_CONF"
set_conf "AiPlayerbot.RandomBotAutoJoinBGRatedArena3v3Count" "1" "$PB_CONF"
set_conf "AiPlayerbot.RandomBotAutoJoinBGRatedArena5v5Count" "1" "$PB_CONF"

# Performance / scaling tuning (mod-playerbots "Playerbot Configuration" wiki). These are the
# throughput knobs, separate from the gameplay rates above. The CPU/threading knobs are .env-driven
# (scale them to your host's cores, RAM, and bot count); the rest are sane fixed defaults.
#   worldserver.conf:
#   - MapUpdate.Threads (.env MAP_UPDATE_THREADS): stock is 1 — the single biggest CPU lever with a
#     high bot count. The wiki caps its advice around 6 even on big boxes (diminishing returns vs.
#     DB/network/bot-AI threads competing for cores), so the default is 6, not cores-2.
#   - MapUpdateInterval/MinWorldUpdateTime: keep the world tick responsive under bot load.
#   - PreloadAllNonInstancedMapGrids=0: don't pin every map grid in RAM at boot (stock default;
#     pinned here so re-running setup.sh restores it).
#   - Quests.IgnoreAutoAccept=1: skip the auto-accept path bots otherwise hammer.
set_conf "MapUpdate.Threads"               "${MAP_UPDATE_THREADS:-6}"  "$WS_CONF"
set_conf "MapUpdateInterval"               "10" "$WS_CONF"
set_conf "MinWorldUpdateTime"              "1"  "$WS_CONF"
set_conf "PreloadAllNonInstancedMapGrids"  "0"  "$WS_CONF"
set_conf "Quests.IgnoreAutoAccept"         "1"  "$WS_CONF"
#   playerbots.conf:
#   - BotActiveAlone + botActiveAloneSmartScale (.env BOT_ACTIVE_ALONE / BOT_ACTIVE_ALONE_SMART_SCALE):
#     the wiki's "Profile 1 (best for high bot counts)". Only ~BotActiveAlone% of bots run full AI
#     when no real player is near, and SmartScale auto-throttles that further if the tick gets heavy.
#   - PlayerbotsDatabase.WorkerThreads/SynchThreads (.env PLAYERBOTS_DB_WORKER_THREADS /
#     PLAYERBOTS_DB_SYNCH_THREADS): the wiki's recommended bot-DB thread split.
#   - RandomBot*Interval: the module authors' tuned cadence for the random-bot manager.
set_conf "AiPlayerbot.BotActiveAlone"               "${BOT_ACTIVE_ALONE:-10}"   "$PB_CONF"
set_conf "AiPlayerbot.botActiveAloneSmartScale"     "${BOT_ACTIVE_ALONE_SMART_SCALE:-1}"    "$PB_CONF"
set_conf "PlayerbotsDatabase.WorkerThreads"         "${PLAYERBOTS_DB_WORKER_THREADS:-1}"    "$PB_CONF"
set_conf "PlayerbotsDatabase.SynchThreads"          "${PLAYERBOTS_DB_SYNCH_THREADS:-2}"    "$PB_CONF"
set_conf "AiPlayerbot.RandomBotUpdateInterval"      "20"   "$PB_CONF"
set_conf "AiPlayerbot.RandomBotCountChangeMinInterval" "1800" "$PB_CONF"
set_conf "AiPlayerbot.RandomBotCountChangeMaxInterval" "7200" "$PB_CONF"

# Level-bracket distribution: concentrate bots around the level(s) real players are at, so
# your bracket feels busy (instead of 2000 bots spread thin across all levels). Re-evaluated
# every few minutes, so the crowd follows you as you level. Weight 10 = strong solo focus
# (module recommends 10-15); SyncFactions makes BOTH factions gather at your level too.
BR_CONF="$MODETC/mod_player_bot_level_brackets.conf"
if [[ -f "$BR_CONF" ]]; then
  set_conf "BotLevelBrackets.Dynamic.UseDynamicDistribution" "1"  "$BR_CONF"
  set_conf "BotLevelBrackets.Dynamic.RealPlayerWeight"       "10" "$BR_CONF"
  set_conf "BotLevelBrackets.Dynamic.SyncFactions"          "1"  "$BR_CONF"
fi


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
    echo "    AHBot ON (char GUIDs: ${AHBOT_GUIDS}) -> lists goods and buys fairly-priced player auctions."
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
  _CHATTER_SP_DEFAULT='You'"'"'re a real person playing WoW: Wrath of the Lich King (3.3.5a, level cap 80), chatting in-game with other players. Type like a normal friendly gamer: short, relaxed, light slang (lol, gg, lfg, lfm, ding, brb, gz, ty, wtb, wts, pst). Each message quietly tells you your current level just so you know what content you'"'"'ve reached — that note is background for you only: never announce, state, repeat, or tack your level (or "lvl N", "level N") onto what you say, since real players don'"'"'t sign their chat with their level. Only talk about content you would actually have reached by THAT level. If you'"'"'re low or mid level you'"'"'re still leveling: talk about your current zones, quests, dungeons your level, your class and spec, professions, gold, and saving for your first mounts — you have NOT been to Northrend, run heroics, or raided (Naxxramas/Ulduar/ToC/ICC) and you never talk as if you have. Only level-80 characters talk about heroics, raids, dailies, rep grinds, or endgame PvP. Only talk about real WoW things; never invent activities. You'"'"'re the person behind the keyboard, not the in-game character or an NPC — no fantasy roleplay voice. Vary how you start; never begin with '"'"'anyone'"'"'. You'"'"'re easygoing and mostly relaxed, but you'"'"'ve got a real personality and a sense of humor, not a chipper customer-service bot. Often enough to notice, though not every line, let some edge show: be dry or sarcastic, gripe about the usual WoW pain (bad RNG, repair bills, wipes, endless rep and daily grinds), rib another player good-naturedly, or crack a dumb joke. Keep it light: tease, don'"'"'t insult; never actually mean, hostile, or nasty toward the person you'"'"'re talking to, and still genuinely help if someone asks (a little sarcasm about it is fine). Vary how the humor lands so you don'"'"'t sound one-note. Never say you'"'"'re an AI, bot, or game master. No markdown, emojis, asterisk-actions, or quotation marks.'
  CHATTER_SYSTEM_PROMPT="${CHATTER_SYSTEM_PROMPT:-${_CHATTER_SP_DEFAULT}}"
  set_conf "PlayerbotChatter.SystemPrompt" "\"${CHATTER_SYSTEM_PROMPT}\"" "$PBCHAT_CONF"
  set_conf "PlayerbotChatter.MaxConcurrent" "${CHATTER_MAX_CONCURRENT:-3}"          "$PBCHAT_CONF"
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
  set_conf "PlayerbotChatter.AmbientWeightGeneric" "${CHATTER_AMBIENT_W_GENERIC:-35}"             "$PBCHAT_CONF"
  set_conf "PlayerbotChatter.AmbientWeightReact" "${CHATTER_AMBIENT_W_REACT:-45}"                 "$PBCHAT_CONF"
  set_conf "PlayerbotChatter.AmbientWeightFlavor" "${CHATTER_AMBIENT_W_FLAVOR:-12}"               "$PBCHAT_CONF"
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

# --- Registration website (ac-webreg) ----------------------------------------
# Opt-in: only wired up when WEBREG_ADMIN_PASS is set in .env. Adds a container
# to the generated override, a least-privilege MySQL user, and autogenerated
# secrets. The site is reached via the operator's Cloudflare tunnel.
if [[ -n "${WEBREG_ADMIN_PASS:-}" ]]; then
  echo "==> Configuring registration website (ac-webreg)"

  # Autogenerate secrets into .env if blank (idempotent).
  if [[ -z "${WEBREG_SESSION_SECRET:-}" ]]; then
    secret="$(openssl rand -hex 32)"
    if grep -q '^WEBREG_SESSION_SECRET=' "$AC_DIR/.env"; then
      sed -i "s|^WEBREG_SESSION_SECRET=.*|WEBREG_SESSION_SECRET=${secret}|" "$AC_DIR/.env"
    else
      echo "WEBREG_SESSION_SECRET=${secret}" >> "$AC_DIR/.env"
    fi
    WEBREG_SESSION_SECRET="$secret"
  fi
  if [[ -z "${WEBREG_DB_PASS:-}" ]]; then
    dbpw="$(openssl rand -hex 24)"
    if grep -q '^WEBREG_DB_PASS=' "$AC_DIR/.env"; then
      sed -i "s|^WEBREG_DB_PASS=.*|WEBREG_DB_PASS=${dbpw}|" "$AC_DIR/.env"
    else
      echo "WEBREG_DB_PASS=${dbpw}" >> "$AC_DIR/.env"
    fi
    WEBREG_DB_PASS="$dbpw"
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
      WEBREG_BOT_PREFIX: "\${WEBREG_BOT_PREFIX:-rndbot}"
    ports:
      - "\${WEBREG_LAN_PORT:-8090}:8090"
    volumes:
      - "\${CLIENT_ZIP_PATH:-/dev/null}:/data/client.zip:ro"
YAML
  # The main `docker compose up` (above) ran before this service was appended and
  # before its secrets existed, so it must be built + started now. Idempotent:
  # re-running reconciles the container with the regenerated override.
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

  # Autogenerate the sidecar DB password into .env if blank (idempotent).
  if [[ -z "${LORE_DB_PASS:-}" ]]; then
    lorepw="$(openssl rand -hex 24)"
    if grep -q '^LORE_DB_PASS=' "$AC_DIR/.env"; then
      sed -i "s|^LORE_DB_PASS=.*|LORE_DB_PASS=${lorepw}|" "$AC_DIR/.env"
    else
      echo "LORE_DB_PASS=${lorepw}" >> "$AC_DIR/.env"
    fi
    LORE_DB_PASS="$lorepw"
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
