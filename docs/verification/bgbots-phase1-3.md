# mod-battleground-bots — dev-box verification (Phases 1-3)

**Date:** 2026-09-13/14 · **Branch:** `feat/bgbots` · **Box:** local full Docker build (500 random bots, `MAX_RANDOM_BOTS=500`)
**Builds:** Phase 1 tree (a1f9bfc+7ab9842), Phase 2 fix round (7c1d301), Phase 3 (9619344+fe24500) — all compiled clean; module objects rebuilt each time (`grep 'mod-battleground-bots/src/.*\.cpp\.o'` in the build log), canaries `director v1` / `BgDirectorOwnsVictim` / `Resolve` found in the running binary.

## Setup notes (dev conf only, not committed)
- `.env`: `BGBOTS_ENABLE=1`, `BGBOTS_DEBUG=1`; conf instantiated by hand from `conf.dist` (`setup.sh` does this on a real run).
- Bots would not self-queue WS/AB with stock playerbots defaults: `AiPlayerbot.RandomBotAutoJoinWSBrackets = 7` / `ABBrackets = 6` are the level-80-ONLY brackets (bracket ids, not levels). Widened on the live `playerbots.conf` to `6,7` / `5,6`; `EYCount` set to 0 so WS/AB form (EY is directed since Phase 3 anyway). Consider carrying the bracket widening into `setup.sh` (separate change).

## Evidence

### Unknown BG is ignored (Phase 1 binary, before EY table existed)
An EY match ran with 29 bots on map 566 while `.bgbots status` reported `0 directed instance(s)` — the fail-open path for an unsupported BG.

### Arathi Basin (inst 1, Phase 3 binary), console `[BGBots]` debug lines
```
inst 1 A t=418s posture=BALANCED  held=3/2 score=440/310 bots=13 guard=12 assault=0 escort=0 chase=0 reinforce=0 roam=1 squads=0
inst 1 H t=418s posture=OFFENSIVE held=2/3 score=310/440 bots=11 guard=5  assault=6 ... squads=4
inst 1 A t=426s posture=OFFENSIVE held=2/3 score=450/320 bots=13 guard=5  assault=7 ... squads=2     <- posture flipped with the node loss
inst 1 H t=426s posture=BALANCED  held=3/2 score=320/450 bots=11 guard=9  assault=2 ... squads=0
inst 1 H t=430s posture=BALANCED  held=3/2 score=330/450 bots=11 guard=9  assault=0 reinforce=2      <- node under attack pulled reinforcements
```
- Posture follows the held-node margin (behind on nodes → OFFENSIVE, ahead → BALANCED/DEFENSIVE) with the 2-tick hysteresis.
- Every held node keeps its quota; a node with enemies present draws `reinforce`.
- Tuning observation: with GuardMin=2 and every AB node "contestable" (an enemy node is always within 300 y) the quota is 3/node, so a 3-node team spends 9 of 11-15 bots guarding. See the Tasks 10+12 review item 4 for the recommended adjustment.

### Warsong Gulch (inst 6, 5v5), `.bgbots status 6`
```
ALLIANCE posture=BALANCED bots=5 guard=2 assault=2 escort=0 chase=0 reinforce=0 roam=1 squads=2 [sq8->1 push] [sq10->1 stage]
  obj 0 Silverwing flag room owner=US    near F/E=3/0
  obj 1 Warsong flag room    owner=ENEMY near F/E=0/1
HORDE    posture=BALANCED bots=5 guard=2 assault=3 escort=0 chase=0 reinforce=0 roam=0 squads=3 [sq7->0 push] [sq9->0 push] [sq11->0 stage]
  obj 0 Silverwing flag room owner=ENEMY near F/E=0/3
  obj 1 Warsong flag room    owner=US    near F/E=1/0
```
- Flag rooms guarded (2 each), the 15 % roam share appears (`roam=1`), squads stage then push.
- **Defect found:** Horde has 3 squads for 3 attackers — a guard released by a shrinking quota founded a one-bot squad. Fixed in the engine (leftovers join the smallest existing squad for the focus) — see the commit `fix(bgbots): leftover bots join an existing squad ...`.

### Stability
No `segfault`/`ASSERT` in the worldserver log across ~2 h and three restarts; instances are created on start (`[BGBots] inst N type T start: posture A=.. H=..`) and disappear from `.bgbots status` after the match ends.


### Choke-slot probes (`.bgbots probe`, final build, bots as path owners)
Warsong (map 489, live 5v5 bot match, 2026-09-14): all 10 authored slots `REACHABLE calc=1 type=0x1` (PATHFIND_NORMAL), no ground-Z warning; path lengths 8-62 points (the two roof-ramp slots are the long ones). Flipped to `verified = true`.
```
probe: REACHABLE calc=1 type=0x1 points=8  end=(1517.5,1468.8,352.0)   Silverwing MELEE tunnel exit
probe: REACHABLE calc=1 type=0x1 points=10 end=(1508.3,1493.2,352.0)   Silverwing MELEE GY-ramp top
probe: REACHABLE calc=1 type=0x1 points=9  end=(1529.2,1456.5,352.1)   Silverwing RANGED overlook
probe: REACHABLE calc=1 type=0x1 points=62 end=(1500.6,1472.9,373.7)   Silverwing RANGED roof-ramp edge
probe: REACHABLE calc=1 type=0x1 points=10 end=(1533.9,1454.6,352.0)   Silverwing HEALER
probe: REACHABLE calc=1 type=0x1 points=8  end=(937.5,1451.1,345.6)    Warsong MELEE tunnel exit
probe: REACHABLE calc=1 type=0x1 points=9  end=(944.9,1423.1,345.4)    Warsong MELEE GY-ramp top
probe: REACHABLE calc=1 type=0x1 points=29 end=(925.2,1458.1,356.0)    Warsong RANGED balcony
probe: REACHABLE calc=1 type=0x1 points=48 end=(952.7,1445.0,367.6)    Warsong RANGED roof-ramp edge
probe: REACHABLE calc=1 type=0x1 points=30 end=(920.8,1459.9,355.8)    Warsong HEALER
```
Arathi (map 529, live bot match, 2026-09-14): all 16 authored slots `REACHABLE calc=1 type=0x1`, no ground-Z warning (3-8 path points each — every slot sits on the node's own approach). Flipped to `verified = true`.
```
Stables      MELEE 6pts (1169.3,1180.3,-56.4)  RANGED 4pts (1171.4,1190.6,-56.4)  HEALER 4pts (1164.4,1189.8,-56.3)
Blacksmith   MELEE 5pts (990.9,1039.3,-42.8)   MELEE 7pts (961.5,1030.8,-45.8)    RANGED 3pts (984.1,1042.9,-43.9)  HEALER 3pts (971.4,1040.9,-44.7)
Farm         MELEE 8pts (811.6,893.5,-58.1)    RANGED 4pts (803.9,884.6,-56.6)    HEALER 6pts (810.8,883.8,-58.0)
Lumber Mill  MELEE 6pts (864.7,1163.8,12.4)    RANGED 4pts (861.6,1158.4,12.3)    HEALER 4pts (853.5,1159.6,11.3)
Gold Mine    MELEE 7pts (1148.9,871.4,-112.0)  RANGED 5pts (1143.6,861.2,-111.4)  HEALER 5pts (1150.6,861.1,-110.9)
```

### Warsong 10v7 on the final build (all 26 slots verified), `.bgbots status 1` + debug line
```
ALLIANCE posture=BALANCED bots=9 guard=2 assault=4 escort=2 chase=0 reinforce=0 roam=1 squads=1 [sq6->1 push]
  obj 0 Silverwing flag room owner=US    near F/E=1/0
  obj 1 Warsong flag room    owner=ENEMY near F/E=1/0
HORDE    posture=BALANCED bots=7 guard=3 assault=1 escort=0 chase=3 reinforce=0 roam=0 squads=1 [sq11->0 push]
[BGBots] inst 1 A t=352s ... score=1/0 bots=10 guard=2 assault=4 escort=2 ... slots=2/10
[BGBots] inst 1 H t=352s ... score=0/1 bots=7  guard=3 assault=1 chase=3 ...  slots=2/10
```
- Alliance carries the flag: `escort=2` on our side, `chase=3` on the enemy side (flag jobs, spec §2 step 4); score 1-0 after a capture.
- `slots=2/10`: two guards per team seated on verified choke slots (HOLD_CHOKE), the remaining guard(s) ON_FLAG — the `ChokeMinGuards`/role-slot rule in action.
- One squad per team, both pushing (the one-bot-squad defect no longer appears).

### Phase 2 targeting — `Logger.playerbots = 5`, `AiPlayerbot.LogInGroupOnly = 0`, one WS match (~6 min)
```
A:attack bg priority target - OK   120      T:bg priority target near   594
A:bg peel for ally - OK             20      T:bg ally needs peel        341
A:bg healer follow squad - OK        0      T:bg healer squad far        90
A:dps assist - OK                  598
```
- Priority-target switches and peels execute (e.g. `Irni A:attack bg priority target - OK`, `Hognohgrogs A:bg peel for ally - OK`); the `dps assist` count is the stock chooser still running whenever no director pick stands (the `BgDirectorOwnsVictim` gate only holds the slot while the victim IS the pick).
- `bg healer follow squad` triggers but never wins a tick at relevance 5.0 (below every heal and dps action a healer in combat has). Accepted: the director assignment already moves a squad healer with its squad out of combat; the action is a mid-fight nudge only. Revisit only if healers are observed standing on the flag while their squad fights elsewhere.
- Note: `AiPlayerbot.LogInGroupOnly = 1` (the shipped default) suppresses the per-action trace for ungrouped BG bots — set it to 0 for this kind of check.

### Disabled = stock
`BattlegroundBots.Enable = 0` + restart: a 17-bot WS match formed and ran while `.bgbots status` reported `[BGBots] 0 directed instance(s)`; startup log `[BattlegroundBots] Enable=0`. Bots played on stock dice; no `[BGBots] inst` lines for that match. Conf restored to `Enable = 1` afterwards.

### Owner playtest feedback (2026-09-14) and dev-box population fixes
- Owner verdict after playing WS/AB at level 60: "bots are doing pretty good". Two tweaks requested and shipped in patch 0019 (`ad21d83`): dismount thrash on approach (rule now keyed on the bot + 12 s remount hold) and guards interrupting the node capper (capture cast = tier-1 kill target).
- A level-60 human could not fill a Warsong queue: this server's WS bracket for 60 is **60-60** (era split), only 3 Alliance bots sat at exactly 60 (min 5/team). `.debug bg` pops it immediately. To grow the 60/80 population the dev conf now uses a STATIC `LevelBrackets` distribution (10 ranges: 60-60 = 25 %, 80-80 = 35 %, others 5 %) — and the balancer only started moving bots once `LevelBrackets.IgnoreGuildBotsWithRealPlayers/IgnoreArenaTeamBots/IgnoreFriendListed` were set to 0: with them on it counted ZERO bots (300 of 500 online bots share a guild with a real character, 198 are in arena-roster teams). Dev-conf only; not in setup.sh.

### Isle of Conquest 40v40 (inst 7, endgame-release build), `.bgbots status 7` at t=816s
```
ALLIANCE posture=OFFENSIVE endgame=0 bots=40 guard=5 assault=28 roam=7 squads=8 [sq128->0 push] ... [sq142->0 push]
  obj 0 Refinery owner=ENEMY  obj 1 Quarry owner=US  obj 2 Docks owner=ENEMY  obj 3 Hangar owner=ENEMY  obj 4 Workshop owner=US
HORDE    posture=BALANCED  endgame=0 bots=40 guard=8 assault=22 reinforce=2 roam=8 squads=7 [sq171->1 push] ...
```
- IC node owners come from the live banner state; the two keeps' posture differs (2/3 vs 3/2 held).
- **Tuning follow-up (not blocking):** a single focus objective means all 7-8 squads of a 40-bot team converge on one node (Refinery / Quarry above). For 40-player BGs a second focus for the surplus squads would look more human. Recorded in the spec notes.

### Alterac Valley 40v40 (inst 21, endgame-release build), `.bgbots status 21`
```
ALLIANCE posture=BALANCED  endgame=0 bots=40 guard=13 assault=17 reinforce=2 roam=8 squads=3 [sq88->4 push] [sq90->4 push] [sq94->12 stage]
HORDE    posture=OFFENSIVE endgame=0 bots=40 guard=11 assault=19 reinforce=4 roam=6 squads=5 [sq89->2 push] [sq99->2 push] [sq101->3 push] ...
  15 objectives (7 graveyards, 8 towers) with live owners; e.g. Stonehearth Bunker owner=US near F/E=5/0, Iceblood Tower owner=US (assaulted) ...
```
- All 15 AV nodes are read and owned correctly (assaulted nodes count for the assaulting team, as in core); the GuardMaxPercent cap holds guards at 13/11 of 40; squads target graveyards/towers; reinforcements fire at contested nodes.
- Endgame (captain dead + 3 enemy towers destroyed → release to stock Drek'Thar/Vanndar logic) not yet observed in this match.

### IC and AV run to completion (build 9: siege release + keep gates + vehicle targets), 2026-09-15
| inst | BG | endgame flipped | ended at | notes |
|---|---|---|---|---|
| 1 | IC | H at t=128 s, A at t=650 s | t≈1462 s (24 min) | release on Hangar/Workshop/Docks control |
| 7 | IC | A at t=120 s | t≈1386 s (23 min) | |
| 11 | IC | A at t=122 s | t≈1338 s (22 min) | |
| 14 | IC | A at t=134 s | live at t=682 s | |
| 2 | AV | never | t≈2586 s (43 min) | ended without the release (captain never killed) |
| 10 | AV | never | t≈2058 s (34 min) | same; posture OFFENSIVE from t≈1200 s, held 5-6 / 7-8 |
- Three bot-only IC matches completed in 22-24 min each once the siege release existed (before it, `endgame` never flipped and the gate never fell). AV matches complete too, but the endgame release never fired because its captain-dead condition is never met by bots — the captain requirement is now a knob (`AV.EndgameNeedsCaptain`, default 0) so the tower count alone releases.

### AV on build 10 (captain requirement off)
Two more bot-only AVs (inst 2, inst 7) completed at ~36+ min without `endgame` flipping: neither side had 3 enemy towers DESTROYED before the match ended (towers need 4 uncontested minutes after the banner flips; bot matches end on reinforcements first). Default `AV.EndgameTowers` lowered 3 → 2 so the release is reachable in bot-paced games; the release path itself is exercised by IC every match. AV outcome with the director: complete, node-driven matches of 35-45 min; a general kill via the release remains unobserved.

### AV endgame release observed (build 11, `AV.EndgameTowers = 2`, captain not required)
```
inst 7 (AV): H endgame=1 from t=1484s (held 7/5) ... last tick t=1714s held=6/6 -> match ended ~4 min after the release
inst 1 (AV): no release, ended at t=2212s (Alliance held 10/4 throughout — reinforcement finish)
```
- With two enemy towers destroyed the Horde team was released to stock at 24.7 min and the match ended at 28.6 min — the stock general push works once the director lets go. AV without a release still ends on reinforcements (~37 min). AV is now verified end to end in bot-only matches.

## Still to verify (needs a client / GM character)
- Phase 2 fine behaviour only a human can judge: healer switch feel, unreachable-healer skip on a ledge, peel timing.
- Choke slots: observe HOLD_CHOKE seating and pull-back in a match once the verified slots are built in.
