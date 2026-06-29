# AzerothCore + Playerbots — Automated LAN WoW Server

**One-command setup and day-to-day management for a personal WotLK 3.3.5a private server packed
with player-like bots.** This repo is *not* the game server — it's an **automation overlay**: a
set of scripts and docs that clone, build, configure, tune, and operate
[AzerothCore](https://github.com/azerothcore/azerothcore-wotlk) (via its
[playerbots fork](https://github.com/mod-playerbots/azerothcore-wotlk)) and a curated set of
community modules in Docker. On top of that orchestration it adds **a couple of custom mods of its
own** (AI bot chat, a lore Q&A sidecar, and a web registration site).

The goal is to take you from a bare Linux or Windows host to a populated, self-running world with a
single `./setup.sh`, then keep it running with plain `start` / `stop` / `update` / `backup`
scripts — without hand-wiring `docker-compose`, module SQL, or dozens of `.conf` files yourself.
**Almost everything you actually play is other people's work** (the core, the bot engine, the
modules); this repo's job is to assemble and manage it. See
[Credits & acknowledgements](#credits--acknowledgements) — please support the upstream projects.

**What lives in *this* repo:** the orchestration scripts (`setup.sh`, `update.sh`, `start.sh` /
`stop.sh`, `backup.sh`, `fetch-client-addons.sh`), the Windows/WSL2 wrapper layer (`windows/`), the
three custom components (`modules/mod-playerbot-chatter/`, `lore-sidecar/`, `webreg/`), and this
documentation. The core, the bot engine, and every community module are fetched from their own
upstream repos at build time (and are gitignored here) — none of their code is redistributed in
this repo.

> **Note on version:** this is **Wrath of the Lich King (3.3.5a)** — clients must be 3.3.5a.

## What you get
- AzerothCore (playerbots fork) in Docker: database + authserver + worldserver.
- ~500-3000 **roaming bots** that quest/grind to populate the world, plus **party bots**
  you invite into your group. (Only a fraction run full AI at once unless near a real
  player; `SmartScale` auto-throttles under load. Set the count with `MAX_RANDOM_BOTS` in
  `.env` — roughly `MAX_RANDOM_BOTS / 20` bots per faction per level bracket.)
- **Boosted XP** (3×) for relaxed solo/co-op progression.
- Optional **AI bot chat** (`mod-playerbot-chatter`): bots hold natural conversations through a
  local Ollama LLM — both **reactive** (they reply when you whisper, `/say`, or talk in
  party/raid) and **ambient** (bots self-initiate WoW small-talk and banter in General / party /
  guild, replacing playerbots' canned broadcasts). Off by default (`CHATTER_ENABLE`). See
  [AI bot chat](#ai-bot-chat-mod-playerbot-chatter).
- Optional **lore sidecar**: on top of the chat module, bots can answer *whispered factual
  questions* ("where's the nearest trainer?", "what drops X?") from your server's real game data.
  Off by default (`LORE_ENABLE`). See [Lore sidecar](#lore-sidecar--factual-qa-optional).
- Optional **raid roster** (`mod-raid-roster`): pin a personal 40-bot roster (same bots, same
  roles, every time), then bulk-log a 5/10/25/40-man subset that auto-forms a raid — geared to
  match you, with a one-click **minimap addon** to drive it. Off by default (`RAIDROSTER_ENABLE`).
  See [Raid roster](#raid-roster-mod-raid-roster).

## Requirements
- **A host to run the server on**, with Docker + Docker Compose. Either:
  - **Linux** with Docker Engine — the primary target, or
  - **Windows 10/11** with [Docker Desktop](https://www.docker.com/products/docker-desktop/) on its
    WSL2 backend — the containers are identical; a thin PowerShell layer handles the Windows-only
    bits (firewall + host-IP detection). See
    [Windows (WSL2 + Docker Desktop)](#windows-wsl2--docker-desktop).

  (Tested target: 4 cores / 8 GB RAM, or 4 cores / 16 GB for a large bot count.)
- A legitimate WoW **3.3.5a** client on each player's machine.
- *(Optional, for the AI bot chat / lore sidecar only)* an [Ollama](https://ollama.com/) instance —
  ideally GPU-backed (~8 GB VRAM for the default 8B model). It can run on the same host or any
  reachable machine; point `OLLAMA_IP` at it. Not needed unless you enable `CHATTER_ENABLE`.

## Linux Deploy (on the server)
```bash
# On the server, clone the repo, then:
git clone https://github.com/lathcf/azerothcore-playerbots-docker-automated.git AzerothCore
cd AzerothCore
# Set your options (DB password, bot count, optional features) in .env.example FIRST:
nano .env.example
./setup.sh
```
On the **first run**, `setup.sh` copies `.env.example` to `azerothcore-wotlk/.env` (the file the
stack actually reads) — so put your settings in `.env.example` *before* that first run. It then
clones the fork + modules, compiles the core, imports the database, downloads client data (several
GB, incl. the **mmaps** bots need to navigate), tunes config, and starts everything. Subsequent
runs are fast and idempotent.

> **Changing settings later:** once installed, edit `azerothcore-wotlk/.env` directly and re-run
> `./setup.sh` — it does **not** re-copy `.env.example` over an existing `azerothcore-wotlk/.env`,
> so further edits to `.env.example` are ignored.

`setup.sh` prints the server's LAN IP at the end. Note it.

## Windows (WSL2 + Docker Desktop)

You can run the exact same stack on Windows 10/11 using Docker Desktop's WSL2 backend. The
containers are identical; a small set of PowerShell scripts in `windows/` handle the only
Windows-specific bits (a firewall rule and telling the realm your *Windows host* LAN IP).

### Prerequisites (install these once)

1. **Install WSL2** — the lightweight Linux layer Docker runs on.
   - Right-click the **Start** button → **Terminal (Admin)** (or **Windows PowerShell (Admin)**),
     and click **Yes** on the security prompt.
   - Run `wsl --install`, then **reboot if Windows asks you to**. This installs WSL2 and the
     **Ubuntu** Linux distribution.
   - After the reboot, an **Ubuntu** window opens on its own and asks you to **create a UNIX
     username and password**. This is a brand-new login *just for Linux* — it is **not** your
     Windows account. Pick a lowercase username with no spaces and a password you'll remember
     (you'll type it for `sudo` later). **Write the username down** — it's the `<you>` in the paths
     below. (If no window appears, just open Ubuntu yourself per *Opening your Ubuntu terminal*
     below, and it'll prompt you on first launch.)
2. **Install Docker Desktop.**
   - Download and install it from https://www.docker.com/products/docker-desktop/ (accept the
     defaults — it uses the WSL2 backend automatically).
   - Launch Docker Desktop and wait until it's fully started (the whale icon in the system tray
     stops animating).
   - Open **Settings → Resources → WSL Integration**, turn the **Ubuntu** toggle **ON**, and click
     **Apply & restart**. Leave Docker Desktop running.

**Opening your Ubuntu terminal** — you'll need this in the next section. Any one of these works:
- Click **Start**, type **Ubuntu**, press **Enter**. *(Easiest.)*
- …or open **Windows Terminal**, click the dropdown arrow (˅) next to the tabs, and choose **Ubuntu**.
- …or in any PowerShell / Command Prompt window, type `wsl` and press **Enter**.

A Linux terminal opens — that's where the `git`/`bash` commands below go. It is **not** the same
window as PowerShell; keep both handy, since the install uses each for different steps.

### Install

1. **Download the repo into your Linux home folder.** In your **Ubuntu terminal**, install Git if
   it isn't already there (enter your Linux password when asked), then clone. **Do not** clone it
   under `/mnt/c/...` (the Windows drive) — it's far slower there and breaks Docker's file
   permissions.
   ```bash
   sudo apt update && sudo apt install -y git          # skip if 'git' is already installed
   git clone https://github.com/lathcf/azerothcore-playerbots-docker-automated.git ~/AzerothCore
   ```
   This creates `~/AzerothCore` (i.e. `/home/<you>/AzerothCore`). *(Forked it? Swap in your
   fork's HTTPS URL instead.)*
2. **Set your options.** Still in the Ubuntu terminal, open the settings file in the simple `nano`
   editor:
   ```bash
   nano ~/AzerothCore/.env.example
   ```
   Set a strong `DOCKER_DB_ROOT_PASSWORD`, and tweak bot count / optional features to taste (same
   keys as the Linux flow). **Leave `LAN_IP` blank** — the installer fills it in. Save and quit
   nano with **Ctrl-O**, **Enter**, then **Ctrl-X**.
3. **Run the Windows installer from an *elevated* PowerShell.** Adding the firewall rule needs
   Admin, so this part runs in **PowerShell**, *not* the Ubuntu terminal. Right-click **Start** →
   **Terminal (Admin)** (or **Windows PowerShell (Admin)**) → **Yes**, then change into the repo's
   `windows` folder. Your files live inside WSL, reachable from Windows at
   `\\wsl$\Ubuntu\home\<you>\AzerothCore` — replace `<you>` with your Linux username:
   ```powershell
   cd \\wsl$\Ubuntu\home\<you>\AzerothCore\windows
   .\Setup-AzerothCore.ps1
   ```
   (Not sure of the path? In your **Ubuntu** terminal run `explorer.exe ~/AzerothCore/windows` —
   File Explorer opens that folder and its address bar shows the exact `\\wsl$\…` path to copy.)

   **If PowerShell says "running scripts is disabled on this system,"** it's blocking the unsigned
   script. Allow it for this one window, then re-run:
   ```powershell
   Set-ExecutionPolicy -Scope Process Bypass -Force
   .\Setup-AzerothCore.ps1
   ```

   The installer then verifies WSL + Docker Desktop, detects your **Windows host** LAN IP, opens
   the firewall for the auth/world ports, writes `LAN_IP` into the WSL-side `.env`, and runs
   `./setup.sh` inside WSL. **The first run is long** — it compiles the core and downloads several
   GB of game data. When it finishes it prints the `set realmlist <ip>` line to give your players.

   *Defaults just work* when you cloned to `~/AzerothCore` and have a single distro. Otherwise add
   `-WslPath <path>` (cloned elsewhere), `-Distro <name>` (more than one distro), or
   `-LanIp <your.host.ip>` (if IP autodetection fails — `ipconfig` shows your address).

**Daily ops** (no admin needed), from the `windows/` folder:
```powershell
.\Start-AzerothCore.ps1     # starts Docker Desktop if needed, then resumes the server
.\Stop-AzerothCore.ps1      # graceful stop (waits up to 120s; -Grace to change)
.\Update-AzerothCore.ps1    # pull latest fork + modules and rebuild (config + DB preserved)
```

**Why no "mirrored networking"?** WSL2's mirrored mode conflicts with Docker Desktop's port
forwarder (both bind the same host ports, failing silently), so this uses default NAT mode plus
Docker Desktop's normal port publishing to the Windows host. LAN reach works because the installer
opens the firewall and the realm advertises the Windows host IP. To remove the firewall rules
later: `Remove-NetFirewallRule -Group AzerothCore` (elevated).

**Connecting clients** is identical to the Linux flow below — point `realmlist.wtf` at the
Windows host IP the installer printed.

## Connect a client
1. Install/locate a WoW **3.3.5a** client.
2. Edit `WoW/Data/enUS/realmlist.wtf` (or `WoW/realmlist.wtf`) to contain exactly:
   ```
   set realmlist <SERVER_LAN_IP>
   ```
3. Launch `Wow.exe`, log in with an account (below), pick the realm, play.

## Create accounts (one per person)
The first account must be made from the worldserver console:
```bash
docker attach ac-worldserver
```
At the `AC>` prompt:
```
account create <username> <password>
account set gmlevel <username> 3 -1   # give yourself full GM (optional)
```
Detach **without stopping the server**: press `Ctrl-P` then `Ctrl-Q`.

## Registration website (ac-webreg)

A small Go service so friends can self-register accounts and download the client.

- **Enable it:** set `WEBREG_ADMIN_PASS` in `.env` (blank = off). `setup.sh` autogenerates
  `WEBREG_SESSION_SECRET` and `WEBREG_DB_PASS`, creates a least-privilege `webreg` MySQL user
  (rights only on `acore_auth.account`), and adds the `ac-webreg` container.
- **Expose it:** the container publishes `WEBREG_LAN_PORT` (default 8090) on the LAN only. For
  LAN-only use, just browse to `http://<this-host-LAN-ip>:8090`. To let friends reach it from
  outside, put it behind a reverse proxy or tunnel (e.g. a **Cloudflare Tunnel** fronted by
  Cloudflare Access, or nginx with TLS) pointed at `http://<this-host-LAN-ip>:8090` — don't
  WAN-forward the raw HTTP port. Remote access then flows only through the proxy, gated by
  whatever auth it enforces.
- **Client download:** put your prepared client zip where `CLIENT_ZIP_PATH` points; it is
  bind-mounted read-only and served with resume support. Bake `realmlist.wtf` into the zip so
  friends just unzip and play.
- **Admin console:** `/admin` (HTTP Basic Auth: `WEBREG_ADMIN_USER` / `WEBREG_ADMIN_PASS`)
  lists your real accounts (playerbot accounts matching `WEBREG_BOT_PREFIX`, default `rndbot`,
  are hidden; there's a name search). Per account you can **reset the password** or **ban/unban**
  (block/restore login). Ban is reversible and writes to `acore_auth.account_banned`; it is not a
  hard delete — to fully purge an account and its characters, use the worldserver console's
  `account delete <name>`. There is no email recovery by design.
- **Pages:** `/register`, `/login`, `/account` (change password), `/download`, `/admin`.

## Using bots in-game
- **Roaming bots** log in automatically — you'll see them questing around the world.
  They're spread across level brackets (via `mod-player-bot-level-brackets`) so low and
  high zones both feel populated.
- **Party bots:** whisper or use chat commands to add bots to your group, e.g.
  `.playerbots bot add <name>` / invite then accept; full command list:
  https://github.com/mod-playerbots/mod-playerbots
- Bots follow, fight, heal, and can fill a 5-man or raid.

## How the bots behave
The bots genuinely play the game — they level, quest, grind, use the economy, and form
guilds — rather than just running around. All of this is tunable in
`azerothcore-wotlk/env/dist/etc/modules/playerbots.conf`.

**Leveling.** Random bots span levels 1–80 and gain XP from questing/grinding. The
`mod-player-bot-level-brackets` module spreads the population across level ranges so every
zone stays populated (the system also balances the distribution, so you won't end up with
1000 max-level bots). Relevant keys: `AiPlayerbot.RandomBotMinLevel` / `RandomBotMaxLevel`,
`AiPlayerbot.RandomBotXPRate` (multiplies your server XP rate for bots).

**Auction house.** Bots travel to auctioneers and post looted items for sale with real
bid/buyout prices, so the AH fills with listings you can buy. The playerbots themselves only
*sell* (no buy/bid code path), so they won't purchase auctions you post — if you want your
listings bought, enable the optional **AHBot** economy (see "Auction house economy" below).

**Guilds.** Bots form their own guilds (`AiPlayerbot.RandomBotGuildCount`, default 20;
`RandomBotGuildSizeMax`, default 15), chat in guild about events, and will invite real
players to groups/raids/guilds (`AiPlayerbot.AllowGuildBots = 1`).

**Daily activity mix** (`AiPlayerbot.EnableNewRpgStrategy = 1`). When not near a real player,
each bot picks an activity by weight — raise/lower these to taste:

| Activity (`AiPlayerbot.RpgStatusProbWeight.*`) | Default | Behavior |
|---|---|---|
| `DoQuest` | 60 | Pick a quest, travel to it, complete it |
| `WanderNpc` | 20 | Interact with NPCs (trainer, vendor, repair, auctioneer, innkeeper) |
| `GoGrind` | 15 | Travel to level-appropriate spots and farm mobs |
| `WanderRandom` | 15 | Roam nearby hunting mobs |
| `TravelFlight` | 15 | Use a flight master to fly somewhere new |
| `GoCamp` | 10 | Return to an inn / flight point |
| `OutdoorPvp` | 10 | Contest points in outdoor PvP zones |
| `Rest` | 5 | Idle briefly |

They also repair gear, learn trainer spells, spend talent points, pick professions and roll
on recipes, and equip class-appropriate gear. **Near you or in your group**, a bot switches
to full combat AI (tank/heal/DPS, follow, assist).

**PvP.** Bots contest outdoor PvP objectives (e.g. Wintergrasp, Hellfire towers) when in
those zones, and they don't randomly gank in the open world — PvP is objective/queue driven.

Battlegrounds work at **all level brackets, on demand**: with `AiPlayerbot.RandomBotJoinBG = 1`,
whenever you queue *any* BG/arena at *any* level, bots at your bracket fill it (the level-bracket
module keeps bots at every level). So you can queue WSG, AB, EotS, AV, IoC, or rated arena at
whatever level you are and it pops. `setup.sh` additionally keeps light always-on level-80
WSG/AB/EotS running for ambiance (`RandomBotAutoJoinBG`); AV/IoC are on-demand only to avoid
tying up bots. **Rated arena** (2v2/3v3/5v5) is enabled at level 80 — make your own arena team
and queue; bot teams provide the opposition. (Lower-level arena needs core code changes, per the
module.) Tune via `AiPlayerbot.RandomBotAutoJoinBG*` and `RandomBotArenaTeam*Count`.

Example tweaks: more questing → raise `DoQuest`; a grindier world → raise `GoGrind` and lower
`DoQuest`; more bot guilds → raise `RandomBotGuildCount`. Apply changes with
`./stop.sh && ./start.sh`. What this *isn't*: a perfect human-driven economy or flawless raid
mechanics — hard encounters rely on scripted bot strategies.

## Bot-management addons (recommended)
Typing chat commands gets tedious with many bots. Two client UI addons make it far easier.

Stage them from this folder, then deploy to each player's client:
```bash
./fetch-client-addons.sh        # downloads into ./client-addons/
```
- **MultiBot** — in-game panel to control bots: roster, gear, talents/specs, outfits,
  loot-master distribution, and combat strategies. Its server half
  (`mod-multibot-bridge`) is already built into the server by `setup.sh`.
- **PlayerBotManager** — gear/GearScore tracking and raid-composition planning across your
  whole roster. Opens with `/pmb`.

Install on **each player's machine** (these are client addons, not server software):
1. Copy the addon folder into `World of Warcraft/Interface/AddOns/`
   (ensure a `.toc` sits at the folder's top level — `find client-addons -name '*.toc'`).
2. Enable them in the "AddOns" menu at character select.

Other community addons you may want (not auto-installed): **DBM** (boss-fight warnings)
and **CompactRaidFrame-3.3.5** (raid frames) for raiding with bot groups.

## Auction house economy (AHBot) — optional
By default the playerbots only *sell* on the AH. The `mod-ah-bot-plus` module adds a dedicated
auction-house agent that both **lists goods** and **buys fairly-priced player auctions** — so
items you post actually get bought. It's a market-maker, not the adventuring bots themselves.

**Setup:**
1. Create a **normal account + character** to act as the AH bot (NOT a playerbot account), and
   don't play it in-game. Create the account from the worldserver console
   (`docker attach ac-worldserver` → `account create ahbot <pass>`), then make a character on it.
2. Find that character's GUID:
   ```bash
   docker compose exec -T ac-database mysql -uroot -p"$DOCKER_DB_ROOT_PASSWORD" \
     -e "SELECT guid,name FROM acore_characters.characters WHERE name='YourAHChar';"
   ```
3. Set it in `azerothcore-wotlk/.env` and re-run `./setup.sh`:
   ```dotenv
   AHBOT_GUIDS=5        # comma-separate for multiple, e.g. 5,6
   ```
   `setup.sh` then enables the seller + buyer and assigns that character (no rebuild needed — it
   only re-patches the config and restarts). Leaving `AHBOT_GUIDS` blank keeps AHBot off even
   though the module is built in.

**How it prices items (important):** it does **not** look at other listings or supply/demand.
Each item's value is computed from a per-category base price (`PriceMinimumCenterBase.*`) and/or
its vendor sell price, scaled by quality/item level, with a random ±25%. The buyer pays that
calculated value × `AuctionHouseBot.Buyer.AcceptablePriceModifier` (default 1.0). So **list at
or below an item's calculated value and the bot buys it out**; overprice it and it won't.

**Useful knobs** in `azerothcore-wotlk/env/dist/etc/modules/mod_ahbot.conf`:
- `Buyer.AcceptablePriceModifier` — raise above 1.0 (e.g. 1.25) so more of your listings sell.
- `Buyer.BidAgainstPlayers` (default `false`) — leave off so the bot doesn't outbid your family
  on auctions they're trying to win; buyouts of your listings still work with it off.
- `Buyer.PreventOverpayingForVendorItems` (default `true`) — stops vendor-flip exploits.

Note: with both this and playerbots running, the AH gets listings from both — that's fine.

## Start / stop / manage
From this folder:
```bash
./start.sh          # start or resume the server
./stop.sh           # graceful stop (waits up to 120s for a clean save)
./stop.sh 60        # ...or pass your own grace period in seconds
```

`stop.sh` matters because, with hundreds of bots, the worldserver needs time to save on
shutdown — Docker's default 10s timeout can force-kill it mid-save. `stop.sh` gives it 120s.
For the cleanest shutdown, you can also trigger a save from the console first:
`docker attach ac-worldserver` → `server shutdown 30` (detach with `Ctrl-P Ctrl-Q`).

Raw Docker Compose equivalents (run in the `azerothcore-wotlk/` dir):
```bash
docker compose ps                      # status
docker compose logs -f ac-worldserver  # world logs (watch bots log in)
docker compose stop -t 120             # graceful stop (what stop.sh does)
docker compose up -d                   # start / resume (what start.sh does)
docker compose down                    # remove containers (volumes/data kept)
# NEVER use `down -v` — the -v deletes the volumes and wipes your database.
```

## Tuning
Edit `azerothcore-wotlk/env/dist/etc/worldserver.conf` (rates) or
`azerothcore-wotlk/env/dist/etc/modules/playerbots.conf` (bot counts), then:
```bash
docker compose restart ac-worldserver
```
Key knobs: `Rate.XP.Kill/Quest/Explore`, `AiPlayerbot.MinRandomBots`,
`AiPlayerbot.MaxRandomBots`. Re-running `./setup.sh` re-applies the documented defaults.

## Updating to the latest from upstream
To pull the newest code from the AzerothCore fork and all cloned modules (and re-sync the
in-repo `mod-playerbot-chatter`), then rebuild:
```bash
cd AzerothCore
./update.sh
```
This fetches the latest commits, recompiles only what changed, and re-runs the database
migration step automatically. Your tuned config (`env/dist/etc/*.conf`) and your database
(named Docker volume) are **preserved**.

Tips:
- Updates to this fork are community-driven and occasionally introduce breaking changes.
  Before a big update, back up the database (run from the `azerothcore-wotlk/` dir):
  ```bash
  docker compose exec -T ac-database \
    mysqldump -uroot -p"$DOCKER_DB_ROOT_PASSWORD" --all-databases > backup-$(date +%F).sql
  ```
- **Pin to a known-good commit if an update breaks.** `update.sh` resets the fork *and every
  cloned module* to its branch tip. To freeze one at a specific commit instead, add a line to
  `repo-pins.txt` — `<repo-basename> <commit>`, e.g. `azerothcore-wotlk <sha>` or
  `mod-aoe-loot <sha>`; both `setup.sh` and `update.sh` honor it. Remove the line to resume
  tracking. Caveat: rolling *code* back does **not** undo database migrations a newer build
  already applied, so for a clean rollback pair the pin with a pre-update backup restore.
- New config options added by an update are not auto-merged into your existing `.conf`
  files; they fall back to compiled defaults. Diff against the `.conf.dist` files in
  `env/dist/etc/` if you want to adopt new settings.
- If a community module ever fails to compile (they're third-party and can lag behind core
  changes), remove its folder from `azerothcore-wotlk/modules/` and rebuild, or keep it but
  exclude it from the build by adding `-DDISABLED_AC_MODULES="mod-name"` to the CMake args.
  The server runs fine without the optional modules.

## AI bot chat (mod-playerbot-chatter)

A custom local module (in `modules/mod-playerbot-chatter/`, compiled into the build by
`setup.sh`) that routes bot chat through an Ollama LLM so bots talk like actual people playing
the game — without touching playerbot commands or MultiBot control. It has two layers:
**reactive** (bots reply to you) and **ambient** (bots talk on their own).

**Reactive — bots reply to you:**
- **Whispers** to a bot → that bot replies.
- **Proximity `/say`** → up to `CHATTER_SAY_MAX_BOTS` nearby bots reply (chance-gated; a bot
  named in the message always answers).
- **Party/raid chat** → up to `CHATTER_GROUP_MAX_BOTS` grouped bots reply (same rules).
- **Memory:** each `(bot, player)` pair keeps a rolling history (last `CHATTER_HISTORY_LEN`
  exchanges) in `mod_playerbot_chatter_history` (`acore_characters`), loaded on startup and
  flushed every 5 min + on shutdown — so a bot remembers you across restarts.

**Ambient — bots talk on their own** (`CHATTER_AMBIENT_ENABLE`, follows `CHATTER_ENABLE`):
- Bots self-initiate WoW chatter in **General / party-raid / guild**, and answer each other —
  fully replacing playerbots' canned broadcasts ("ding", "Just turned in Quest"). It only ever
  speaks in a channel that has a **real player** present, so empty channels cost zero GPU.
- **Stays in character:** topics are picked by the bot's **level** (a level-30 talks
  dungeons/mounts/leveling, not raids), and the prompts are grounded in real WotLK content so the
  model talks about actual zones/dungeons/raids/professions instead of inventing things. Two-speed
  cadence (a slow line to open a topic, fast follow-ups to carry a conversation) with cooldowns
  and a global messages/min ceiling so it stays lively but not spammy. Reactive replies always
  get priority — ambient is dropped whenever a whisper/`/say` is waiting.

**Talks like a player, not an NPC:** all output comes from "the person behind the keyboard" —
casual and friendly, aware of the character's class/level/zone/current activity, with no
fantasy-accent roleplay. The personality + game grounding is one config line
(`PlayerbotChatter.SystemPrompt`).

**Stays out of the command system (by design):**
- The chat hook only *observes* — it always returns `true`, never blocking or editing a message.
- Command-looking chat is left for playerbots/MultiBot and gets no AI reply: control prefixes
  (`.`, `+`, `-`, `!`, `#`, `@`), known command words (`follow`, `stay`, `attack`, `do attack`,
  …, tunable via `CHATTER_COMMAND_KEYWORDS`), and addon/bot/system traffic are all dropped.
- When enabled, `setup.sh` pins `AiPlayerbot.RandomBotTalk=0`, `EnableBroadcasts=0`, and
  `RandomBotSayWithoutMaster=0` to silence playerbots' built-in canned chatter (the reply engine,
  the ding/quest/loot/kill broadcaster, and unsolicited masterless talk) so this mod owns the
  whole bot voice and bots don't double-answer. This is separate from the command path — playerbot
  commands and MultiBot whisper control keep working untouched.

**Enable it:**
1. Set `CHATTER_ENABLE=1` in `.env` — this turns on **both** layers (ambient follows it unless
   you set `CHATTER_AMBIENT_ENABLE` separately).
2. Point `CHATTER_URL` (or just `OLLAMA_IP`) at your Ollama instance, and `ollama pull` your model
   (default `llama3.1:8b`).
3. Run `./setup.sh` (first install) or `./update.sh` (rebuild).

**Picking a model — this matters most for quality.** The chat is only as good as the model's WoW
knowledge. A ~4B model invents non-WoW nonsense ("grind squirrels", "bounty board"); an **8B**
model knows the actual game and is the sweet spot. `llama3.1:8b` (the default — it also does tool
calling, useful later) uses ~5.5 GB VRAM at Q4 and runs well on an 8 GB card at
`CHATTER_MAX_CONCURRENT=3`. Set `CHATTER_MODEL` in `.env` so `setup.sh` keeps it; to A/B a model
live, edit `PlayerbotChatter.Model` in the conf + `docker compose restart ac-worldserver`.

**Tuning & debugging:** every `CHATTER_*` knob is a config value — edit
`env/dist/etc/modules/mod_playerbot_chatter.conf` and `docker compose restart ac-worldserver`
to apply (no rebuild; only C++ changes need `./update.sh`). Set `PlayerbotChatter.Debug = 1`
there to log each Ollama request/reply, then watch with
`docker compose logs -f ac-worldserver | grep -i playerbotchatter`.

**Key `.env` knobs** (see `.env.example` for the full list):

| Knob | Default | Description |
|---|---|---|
| `CHATTER_ENABLE` | `0` | Master on/off switch |
| `CHATTER_URL` | (from `OLLAMA_IP`) | Full Ollama `/api/generate` URL |
| `CHATTER_MODEL` | `llama3.1:8b` | Ollama model — 8B+ recommended for real WoW knowledge |
| `CHATTER_THINK` | `0` | `0` = send `"think":false` (only matters for reasoning models) |
| `CHATTER_MAX_CONCURRENT` | `3` | Max simultaneous Ollama requests (GPU gate) |
| `CHATTER_SAY_RANGE` | `40` | Yards within which bots hear `/say` |
| `CHATTER_SAY_MAX_BOTS` | `2` | Max bots that reply to `/say` |
| `CHATTER_SAY_CHANCE` | `35` | Percent chance each nearby bot replies |
| `CHATTER_GROUP_MAX_BOTS` | `2` | Max grouped bots that reply to party/raid chat |
| `CHATTER_GROUP_CHANCE` | `50` | Percent chance each grouped bot replies |
| `CHATTER_WHISPER_CHANCE` | `100` | Percent chance a whispered bot replies |
| `CHATTER_HISTORY_LEN` | `10` | Conversation turns remembered per (bot, player) |
| `CHATTER_REPLY_MAXLEN` | `200` | Max reply length (characters) |

Ambient chatter has its own `CHATTER_AMBIENT_*` family (per-channel toggles, two-speed cadence,
cooldowns, content-mode weights) — all default sensibly; see `.env.example` for the annotated
list. The two you'll reach for: `CHATTER_AMBIENT_MAX_PER_MIN` (overall volume; lower it if the GPU
is the bottleneck) and `CHATTER_AMBIENT_W_REACT` vs `CHATTER_AMBIENT_W_GENERIC` (bots answering
each other vs opening new topics).

## Lore sidecar — factual Q&A (optional)

An optional companion to the chatter module: a separate Python container (`ac-lore`) that answers
*whispered factual questions* — "where's the nearest mining trainer?", "what drops X?", "where do
I turn in this quest?" — from your server's **real game data**, phrased in the bot's own voice.
Off by default; it builds on the reactive chatter layer, so it needs `CHATTER_ENABLE=1` too.

**How it works:** when you whisper a bot something that looks like a factual question, the
worldserver ships the bot's live state (position, real quest log) to the sidecar, which
**classifies** the question (Ollama) → **looks it up** in the world database (a read-only `lore`
MySQL user) → **phrases** a natural reply (Ollama). On any miss, timeout, or non-question it
silently falls back to a normal chatter reply, so a disabled or unreachable sidecar is invisible.
Ordinary chatter is grounded in the bot's real active quest the same way.

**Enable it:**
1. Set `CHATTER_ENABLE=1` and `LORE_ENABLE=1` in your `.env`, and make sure `OLLAMA_IP` points at
   your Ollama host (the sidecar uses the same Ollama as the chatter module).
2. Run `./setup.sh`. It creates the least-privilege read-only `lore` MySQL user and adds the
   `ac-lore` container (autogenerating `LORE_DB_PASS`).

**Iterating:** the sidecar is pure Python under `lore-sidecar/` — edit it and
`docker compose up -d --build ac-lore`, no core rebuild. Knobs: `LORE_MODEL` (default
`llama3.1:8b`), `LORE_TIMEOUT` / `LORE_GEN_TIMEOUT` (raise these if your GPU is slow under chatter
load), and `LORE_DEBUG=1` for verbose request/reply logs.

## Raid roster (mod-raid-roster)

A small local module (authored in this repo) that gives you a **personal, persistent raid team
you fully control** — the same named bots, in the same roles, every time you log in.

**Why it exists.** The roaming random bots are great for populating the world and filling a quick
group, but they're a shared, shifting pool: you don't get the *same* bots twice, and running
maintenance on them tends to leave them wildly over-geared. This module instead pins a fixed
roster to *your character* and keeps the bots geared to *you*, so you can sit down and instantly
run a 5/10/25/40 with a stable, level-appropriate group.

**How it works.** It's a thin layer over mod-playerbots' built-in **addclass** bots (pre-made,
fully controllable class bots) — no core patches. `.raidroster create` reserves 40 of them as
your roster (4 tanks / 9 healers / 27 DPS), each pinned to a fixed talent spec, and records them
in the DB so they survive restarts. `.raidroster login <size>` brings a role-balanced subset
online and lets the bot engine auto-form the raid. `.raidroster sync` levels and gears every
online bot to match your character **and** re-applies each one's tank/heal/dps spec — so the same
bots tank and heal every time, and never out-gear you. Each player's roster is independent (keyed
to their character) and no two players ever share a bot, so a few friends can each keep their own
team from the same pool.

Off by default; enable with `RAIDROSTER_ENABLE=1` in `.env` and re-run `./setup.sh` (a worldserver
restart also generates the addclass pool the first time).

**Typical session:** `.raidroster create` once (ever), then each play session
`.raidroster login 25` → `.raidroster sync` → raid. Re-run `sync` after you level or upgrade gear.

**Commands** (in-game as a player, or from the worldserver console):

| Command | What it does |
|---|---|
| `.raidroster create` | Pin 40 addclass-pool bots as your roster (4 tank / 9 heal / 27 dps). Errors if a roster already exists. |
| `.raidroster login [5\|10\|25\|40] [tank\|heal\|dps]` | Log in that many bots (role-balanced subset, you + N bots = raid size). Omit size for 40. Picks a **random assortment** of your reserved bots for each role — keeping any already online, so re-running `login` doesn't reshuffle your current group, while a fresh session brings different faces. **Fills your own slot from your spec** — if you're tank/heal-specced it drops a bot of that role so you're not redundant; add an explicit `tank`/`heal`/`dps` to override. Auto-forms the raid; trims extras already online. |
| `.raidroster sync` | Level + gear every online roster bot to match you, then force each slot's pinned tank/heal/dps spec. Fixes any overgear. |
| `.raidroster logout` | Log out all online roster bots (roster stays pinned). |
| `.raidroster reset` | Clear all saved instance lockouts (raids + heroic dungeons) for you and every roster bot at once — online **or** offline. A multi-character, non-GM stand-in for `.instance unbind all` (skips whatever instance you're standing in). |
| `.raidroster remove confirm` | Delete your roster (characters return to the shared addclass pool). |
| `.raidroster status` | Show roster size, role counts, and how many bots are currently online. |

**Notes:**
- No GM rank required — the commands run on any normal character (they use `SEC_PLAYER`,
  like mod-playerbots' own `.playerbot` command), so your regular main can manage the roster.
- "40-man" means you + 39 bots (the WoW cap is 40, one slot is you). Smaller sizes are
  proportionally role-balanced: 5-man = 1 tank / 1 heal / 2 dps + you; 10-man = 2/2/5 + you.
- **You count as a role, not just a slot.** `login` reads your spec: a tank fills the tank slot
  (so a 5-man fields 0 tank / 1 heal / 3 dps bots), a healer fills a heal slot, anyone else is
  DPS. Override with `login <size> tank|heal|dps` (e.g. a Prot warrior who wants to DPS a run).
  At 40-man the bot pool is DPS-maxed, so taking tank/heal just shifts the freed slot to a spare
  healer — you always get a full group.
- Bots are drawn from mod-playerbots' *addclass pool* (`AiPlayerbot.AddClassAccountPoolSize`,
  default 50 via `ADDCLASS_POOL_SIZE`). The pool must exist (needs a worldserver restart after
  first setup before `create` will find characters).
- `.raidroster sync` caps bot gear to `AiPlayerbot.AutoInitEquipLevelLimitRatio` × your gear
  score (tunable via `GEAR_MATCH_RATIO`, default 1.0). Run it after leveling or a gear upgrade.
- `MAX_ADDED_BOTS=60` (default; overrides the core's 40-bot limit) gives headroom for a full
  40-man plus any manually-added bots.

**Minimap addon (optional).** So you don't have to type the commands, a small companion **client**
addon (`RaidRoster`) puts a draggable button on the minimap; clicking it opens a dropdown that
fires each command for you — **Create**, **Login 5 / 10 / 25 / 40** (auto-detects your role), a
**Login as ▸ Tank / Healer / DPS ▸ size** submenu for the override, **Sync**, **Logoff**,
**Reset locks**, and **Status**. It's self-contained (no libraries), remembers its position per character, and just
sends the same `.raidroster` commands (so the server stays the single source of truth — there's no
auto-sync; click **Sync** yourself once the bots are in).

It's authored in `client-addons-src/RaidRoster/` (version-controlled); `./fetch-client-addons.sh`
stages it into `client-addons/RaidRoster/` alongside the downloaded addons. Like every client
addon here, copy that folder into each player's `World of Warcraft/Interface/AddOns/`.

## Backups & data safety
Everything (characters, gear, gold, guilds, AH) lives in MySQL in a persistent Docker volume,
so restarts and host reboots are safe. Characters auto-save every **5 minutes**
(`PlayerSaveInterval`, lowered from the 15-min default) plus on logout/level-up — so a power
cut costs at most a few minutes of progress, never the whole world. MySQL/InnoDB is crash-safe
and recovers cleanly on the next boot.

The remaining risks are disk failure and accidental `docker compose down -v`. Guard against
both with backups:
```bash
./backup.sh        # bundles all DBs + .env into ./backups/acore-<timestamp>.tar, keeps last 14
```
**`setup.sh` installs a nightly cron job for this automatically** (4 AM by default). It's
idempotent — re-running `setup.sh` won't duplicate it. To change or remove it:
```bash
crontab -e         # edit/delete the line tagged "# azerothcore-backup"
```
Options:
- Skip installing the cron: run `BACKUP_CRON=0 ./setup.sh`.
- Change the time: run with e.g. `BACKUP_SCHEDULE="30 3 * * *" ./setup.sh`.
- Tune retention with `BACKUP_KEEP` (default 14).

The dump uses `--single-transaction`, so it won't lock the live server.

Each bundle holds both the databases **and** `.env` (which carries the DB root
password and the secrets `setup.sh` autogenerates), so one file is a complete
snapshot.

**Restore** a backup with one command (overwrites current data — it stops the
server, reloads the databases, restores `.env`, and brings everything back up):
```bash
./restore.sh backups/acore-<timestamp>.tar       # add --yes to skip the prompt
```
This works on a brand-new machine too: `git clone` the repo, run `./setup.sh`
once to build the server, then `./restore.sh <bundle>`. It re-aligns the live
MySQL root password to the one in your backed-up `.env`, so future backups keep
working. (Restore targets same-version recovery — restoring an older dump onto
newer binaries can mismatch schema, since migrations don't auto-revert.)

## Security note
This is built for a **trusted LAN**. By default the realm only advertises your private IP and
only the LAN can reach it. Always set a strong, unique `DOCKER_DB_ROOT_PASSWORD` in
`azerothcore-wotlk/.env`.

What `setup.sh` already locks down:
- **MySQL (3306) is bound to `127.0.0.1`** — never reachable off the host.
- **SOAP (7878) is not published** and is disabled + loopback-bound in config (it's a remote
  GM-command console).
- **Auth brute-force lockout is on** (`WrongPass.MaxCount=5`, 10-min account lock) — stock
  AzerothCore ships this *off*.

## Exposing to a friend (security)
Opening a game server to the internet is inherently risky; the worldserver/authserver C++ stack
isn't hardened for hostile traffic. The safe way to let **one** friend in is to expose the
minimum and restrict *who* can reach it at the firewall. Steps:

1. **Strong DB password.** Set `DOCKER_DB_ROOT_PASSWORD` to a long unique value. `setup.sh`
   refuses to run with `PUBLIC_REALM_ADDRESS` set while it's still the default.
2. **Set the public realm address.** In `.env`, set `PUBLIC_REALM_ADDRESS` to your WAN public IP,
   or a **DDNS hostname** if your home IP is dynamic (the authserver resolves it at startup —
   restart the stack if the IP behind it changes). Re-run `./setup.sh`. LAN players keep using
   the private IP automatically (no NAT-hairpin breakage).
3. **On your router/firewall — forward only two ports, scoped to your friend's IP** (terms below
   are TP-Link Omada; adapt to your gateway):
   - Create a **port forward / Virtual Server**: `TCP 3724` and `TCP 8085` → the server's LAN IP
     (same internal port numbers).
   - Add a **firewall/ACL rule** above the forward: **permit source = your friend's static public
     IP** to those ports, and a following rule that **drops all other WAN sources** to them. This
     is the control that keeps the internet at large off your auth/world ports.
   - **Never forward 3306 (MySQL) or 7878 (SOAP).**
4. **Keep the forwarded external world port = 8085.** The realm advertises a single world port to
   both LAN and remote clients, so don't remap it to a different external number.
5. **One account per person.** Create it from the console (see "Create accounts"), or enable
   the self-service **Registration website (ac-webreg)** (see that section) to let friends
   register themselves. Give your friend their own account, not a GM one (`.account create`
   then set GM level only for yourself).
6. **Caveats of the static-IP ACL:** if your friend's IP changes, update the ACL. Watch
   `azerothcore-wotlk/env/dist/logs/Auth.log` for failed logins. Keep nightly backups (already
   configured) and keep an off-box copy.

## Credits & acknowledgements

**This project is mostly automation.** The server core, the bots, and nearly every feature
described above are the work of the AzerothCore community and the module authors below. This repo
only clones, builds, configures, and manages their code — all of it is fetched from its own
upstream at build time and **none of it is redistributed here**. If you find this useful, please
star and support the original projects; they did the hard part.

**Core**
- **[AzerothCore](https://github.com/azerothcore/azerothcore-wotlk)** — the open-source WotLK
  3.3.5a server core that everything is built on (AGPL-3.0).
- **[mod-playerbots fork of AzerothCore](https://github.com/mod-playerbots/azerothcore-wotlk)**
  (branch `Playerbot`) — the AzerothCore fork carrying the playerbots patches; the core this repo
  actually checks out and builds.

**Server modules** (cloned and compiled into the build by `setup.sh`)
- **[mod-playerbots](https://github.com/mod-playerbots/mod-playerbots)** — the bot engine itself:
  the player-like bots that quest, grind, group, trade, and fight. The heart of the experience.
- **[mod-player-bot-level-brackets](https://github.com/DustinHendrickson/mod-player-bot-level-brackets)**
  (Dustin Hendrickson) — spreads random bots across level ranges so every zone stays populated.
- **[mod-junk-to-gold](https://github.com/noisiver/mod-junk-to-gold)** (noisiver) — auto-sells
  gray trash to cut bag clutter.
- **[mod-multibot-bridge](https://github.com/Wishmaster117/mod-multibot-bridge)** (Wishmaster117)
  — the server half of the in-game *MultiBot* control addon.
- **[mod-ah-bot-plus](https://github.com/NathanHandley/mod-ah-bot-plus)** (Nathan Handley) — the
  optional auction-house economy agent (the AHBot in [Auction house economy](#auction-house-economy-ahbot--optional)).
- **[mod-aoe-loot](https://github.com/azerothcore/mod-aoe-loot)** — currently disabled in
  `setup.sh` (it broke group-loot rolls), credited because it ships in the module list.

**Client addons** (staged by `fetch-client-addons.sh`, installed on each player's PC)
- **[MultiBot (MultiBot-Chatless)](https://github.com/Wishmaster117/MultiBot-Chatless)**
  (Wishmaster117) — in-game bot-control panel (roster, gear, specs, strategies, loot).
- **[PlayerBotManager](https://github.com/Lichborne-AC/PlayerbotManager)** (Lichborne-AC) —
  gear/GearScore tracking and raid-composition planning across your roster.

**Platform & tooling**
- **[Docker](https://www.docker.com/) & Docker Compose** — the containerization the whole stack
  runs on.
- **[Ollama](https://ollama.com/)** — the local LLM runtime that powers the optional AI bot chat
  and lore sidecar.
- **MySQL** — the game database (run as a container).

**Custom to this repo** (authored here and version-controlled with this overlay — *not* upstream)
- **`modules/mod-playerbot-chatter/`** — AI bot chat (reactive + ambient) routed through Ollama.
- **`modules/mod-raid-roster/`** — persistent 40-bot raid roster over the addclass pool, with
  role-balanced subset login and gear-to-master sync. Paired with the **`RaidRoster`** client
  addon (`client-addons-src/RaidRoster/`) — a minimap menu that runs its commands.
- **`lore-sidecar/`** — a Python sidecar that answers whispered factual questions from real game
  data.
- **`webreg/`** — a Go self-service account-registration and client-download site.

*World of Warcraft* and *Wrath of the Lich King* are trademarks of Blizzard Entertainment. This is
a non-commercial, private-use project; it ships no Blizzard game data or client files.
