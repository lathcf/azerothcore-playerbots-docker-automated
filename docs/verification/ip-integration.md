# mod-individual-progression (IP) integration — recon findings

Reconnaissance for integrating the community module **mod-individual-progression** (IP)
into this server's modset. This is Task 1 (pure source recon + pin); nothing is built here.

Source repo: https://github.com/ZhengPeiRu21/mod-individual-progression.git
Recon done against a clean clone of `HEAD` (the pin below).

## Recon Findings

### IP pin commit
`df1016444abcc21d025885282799ba76bebea627` (HEAD of `main` at recon time).
Added to `repo-pins.txt` as `mod-individual-progression df1016444abcc21d025885282799ba76bebea627`.

### IP auto-import world-SQL dir (relative path within the module)
`data/sql/world/base/` — every `.sql` here is re-applied at startup (AzerothCore module
base-SQL convention; files apply in alphabetical order). `data/sql/world/updates/` also
exists but ships empty (only `.gitignore`/`.gitkeep`). There is **no** `data/sql/db-world/`
dir — IP uses the older `data/sql/world/{base,updates}` module layout, **not** the newer
`db-world` naming. Auth/characters equivalents (`data/sql/auth/`, `data/sql/characters/`)
exist but are empty.

> Caveat: I confirmed the base-dir path from the module's own directory convention + the
> `zz_` sort-ordering prefix on the optional files (see below), not by reading the core's
> DBUpdater (the fork is gitignored and not in this scratch clone). IP is a widely-used
> module shipping SQL only in `data/sql/world/base/`, so that dir is definitively the
> auto-applied one.

### Progression setter API (exact header + call + full signature)
Header to include: **`IndividualProgression.h`** (the only public header, `src/IndividualProgression.h`).
Singleton accessor macro: **`sIndividualProgression`** (== `IndividualProgression::instance()`).

Two writers on the `IndividualProgression` class:

- `void UpdateProgressionState(Player* player, ProgressionState newState) const;`
  — **raise-only**. No-ops unless `newState > currentState`; also gated on `enabled`,
  `!newState` (stage 0 ignored), `IsInWorld()`, and `progressionLimit`. Adds the hidden
  quests from current..newState.
- `static void ForceUpdateProgressionState(Player* player, ProgressionState newState);`
  — **authoritative set** (can raise OR lower). Removes ALL hidden progression quests
  (66000+1 .. 66000+13), then re-grants 66000+1 .. 66000+newState. Gated only on
  `player && player->IsInWorld() && newState != 0`.

The GM `.ip set` command and the leader-sync path both call
`sIndividualProgression->ForceUpdateProgressionState(target, static_cast<ProgressionState>(level))`
— **this is the call a C++ module should use to set a bot's era** (cast the int stage to
`ProgressionState`). The bot must be `IsInWorld()` at the time.

### Progression getter API (exact call)
`uint8 sIndividualProgression->GetPlayerProgressionFromQuests(Player* player)` — returns the
highest hidden progression quest (66000+i) that is `QUEST_STATUS_REWARDED`; returns `0` if
the player is null / not in world. Progression is stored as rewarded hidden quests, **not** a
DB column, so it is per-character and travels with the character.
(Convenience predicates also exist: `hasPassedProgression(player, state)`,
`static isBeforeProgression(player, state)`.)

### Era → stage integer (`enum ProgressionState : uint8` in IndividualProgression.h)
The plan assumed **0 / 8 / 13** for Vanilla start / TBC / WotLK — **CONFIRMED correct**:

| Era (design intent)             | Stage int | Enum constant            |
|---------------------------------|-----------|--------------------------|
| Vanilla start (pre-TBC)         | **0**     | `PROGRESSION_START`      |
| TBC entry (char level 61-70)    | **8**     | `PROGRESSION_PRE_TBC` (Karazhan/Gruul/Magtheridon) |
| WotLK entry (char level 71-80)  | **13**    | `PROGRESSION_TBC_TIER_5` (WotLK Naxx, EoE, OS) |

Full enum (0-18): START=0, MOLTEN_CORE=1, ONYXIA=2, BLACKWING_LAIR=3, PRE_AQ=4, AQ_WAR=5,
AQ=6, NAXX40=7, PRE_TBC=8, TBC_TIER_1=9, TBC_TIER_2=10, (11 skipped — Zul'Aman),
TBC_TIER_4=12, TBC_TIER_5=13, WOTLK_TIER_1=14 (Ulduar), WOTLK_TIER_2=15 (TotC),
WOTLK_TIER_3=16 (ICC), WOTLK_TIER_4=17 (Ruby Sanctum), WOTLK_TIER_5=18.

Note value **11** (`PROGRESSION_TBC_TIER_3`, Zul'Aman) is commented out in the enum, but the
66000+i quest loops still iterate 1..13 inclusive, so the numeric values above hold.

### GM/console command to set progression
Root command **`.ip`** (subcommands, all `SEC_GAMEMASTER`, usable from console `Console::Yes`):
- `.ip get [player]` — prints a character's current progression level.
- **`.ip set [player] <level>`** — force-sets a character to `<level>` (calls
  `ForceUpdateProgressionState`). This is the command a human uses to hand-set each existing
  character's era. `[player]` is an optional `PlayerIdentifier`; the target must be online.
- `.ip setbot` — force-sets every RND bot in the caller's group to the caller's level.
- Also: `.ip setrep`, `.ip pvp`, `.ip tele <loc>`, `.ip attune <loc>`.

The same setter (`ForceUpdateProgressionState`) is what a C++ module should call directly to
set a bot's progression — no need to route through the chat command.

### Optional SQL filenames — count & confirmation
**22** files (matches the plan's "22 optional SQL"), all under `optional/sql/world/`, all
`zz_optional_*.sql`. To auto-apply, a file must be **copied into `data/sql/world/base/`**
(the `zz_` prefix makes it sort last, after the base SQL). The `optional/sql/world/` staging
dir is NOT itself auto-applied. Full list:

```
zz_optional_ammo_stack_size.sql
zz_optional_aq_quest_nerf.sql
zz_optional_av_landmines.sql
zz_optional_creature_stats.sql
zz_optional_item_stack_sizes.sql
zz_optional_limit_spells_to_expansion.sql
zz_optional_phasing.sql
zz_optional_remove_heirlooms.sql
zz_optional_restore_crafting_cd_timers.sql
zz_optional_restore_potion_cd.sql
zz_optional_restore_rogue_poisons.sql
zz_optional_small_group_adjustments.sql
zz_optional_spell_damage_and_healing.sql
zz_optional_stackable_buff_scrolls.sql
zz_optional_tbc_heroic_dungeon_keys_nerf.sql
zz_optional_tbc_pvp_prices.sql
zz_optional_unobtainable_items.sql
zz_optional_vanilla_crafting_requirements.sql
zz_optional_vanilla_models.sql
zz_optional_vanilla_regen_values.sql
zz_optional_vanilla_transports.sql
zz_optional_wotlk_hp_values_for_tbc_raids.sql
```

### Client-patch archive real names + repo paths
Under `optional/` (per `optional/patch-explanations.txt`):

- **`optional/patch-V.7z`** — the "patch-V" adjusted-**mana-cost** archive (Vanilla/TBC mana
  costs). Contains `patch-V.mpq` (client `Data/` folder) + `Spell.dbc` (server `data/dbc/`).
- **`optional/patch-S.7z`** — the alternative: keeps WotLK mana costs (`patch-S.mpq` +
  a different `Spell.dbc`). **Use patch-V OR patch-S, never both.**
- **`optional/dbc.7z`** — the "dbc"/**profession** archive. Server DBCs to restore
  Vanilla/TBC spells, crafting, recipes, reagents & profession leveling:
  `SkillLine.dbc`, `SkillLineAbility.dbc`, `SkillRaceClassInfo.dbc`,
  `SpellItemEnchantment.dbc` (into server `data/dbc/`). Also bundles visual-only client
  patches `patch-J.mpq` (Vanilla login screen) and `patch-U.mpq` (Vanilla loading screens).

### Header to include from a sibling C++ module + include/link note
`#include "IndividualProgression.h"`. IP ships **no** `.cmake` and **no** `CMakeLists.txt`
(only `include.sh`, which just sources `conf/conf.sh[.dist]` — it does NOT add an include
dir or export a target). The header lives at `modules/mod-individual-progression/src/`, which
is already on the compile include path for any module in the same `modules/` build (AzerothCore
adds every module's `src/` to the collect-sources include set). No extra `target_link_libraries`
is needed — IP is a header-visible singleton (`IndividualProgression::instance()`), so a sibling
module includes the header and calls `sIndividualProgression->...` directly, as long as IP is
present in the build. There is no separate `.so`/lib to link; module objects are compiled into
the worldserver.

## Notes for downstream tasks

- **Config prefix is `IndividualProgression.*`.** Main toggle `IndividualProgression.Enable = 1`;
  server-wide start stage `IndividualProgression.StartingProgression = 0`. Config wiring should
  set these from `.env`/`setup.sh` like other modules. There is also a `Bot*` family
  (`BotAccountsRegex`, `BotAccountsMaxLevel`, `BotOnlyAdjustments`, `BotAccountsEarnPvPTitles`)
  worth reviewing for playerbot interaction.
- **`ForceUpdateProgressionState` cannot set a character to stage 0.** It early-returns on
  `newState == 0` WITHOUT removing the hidden quests, so you can never "reset to Vanilla start"
  through this API — a character already past 0 stays where it is. For a C++ era-sync that must
  represent "Vanilla," either leave a fresh character untouched (default is 0) or handle the
  zero case out-of-band. `.ip set <player> 0` has the same limitation.
- **Both setters and the getter require `player->IsInWorld()`.** A C++ era-sync for bots must
  run while the bot is in world (getter returns 0 otherwise; setters no-op).
- **Progression is stored as rewarded hidden quests (IDs 66000+stage), not a DB column.** It is
  per-character and persists with the character. There is no bulk SQL "set everyone's era"; each
  character is set via the command or the API.
- **`UpdateProgressionState` is raise-only; `ForceUpdateProgressionState` is the authoritative
  set.** Use `ForceUpdateProgressionState` for era-sync so bots can be lowered as well as raised.
- **Optional SQL is copy-to-apply.** The optional-SQL copy task must `cp` chosen
  `optional/sql/world/zz_optional_*.sql` into `data/sql/world/base/` (or the equivalent staged
  path setup.sh assembles) — the module does not apply them from `optional/` automatically.
- **`data/sql/world/base/` (not `db-world`) is the auto-apply dir** — any code hardcoding the
  path must use the older `world` naming.

## Dev-box build verification (2026-08-12)

Full local build on the dev box (`azerothcore-wotlk` stack) with `IP_ENABLE=1`, 15 optionals ON, patch-V + dbc.7z. Three real bugs were found and fixed during this pass (see git history): `apply_ip_optional_sql` set-e return (`f566f76`), optional-copy called before `source .env` (`79445d3`), and a `GENERAL` enum clash between IP and playerbots headers fixed by isolating IP calls in `RaidRosterEra.{h,cpp}` (`6df669f`).

**Verified working:**
- IP module clones+pins (`df10164`), compiles, and loads (`individualProgression.conf` read at boot).
- All 15 enabled `zz_optional_*.sql` **applied** by db-import to the (pre-existing) DB — confirms the `data/sql/world/base` + tracked-updates mechanism re-applies new base files to an existing DB.
- 5 server DBCs overlaid into the worldserver via compose bind-mounts; `Spell.dbc` is byte-identical (53466210 B) host `ip-dbc/` ↔ container `data/dbc/`.
- Era-sync compiled + **linked** (worldserver links with no undefined reference to `RaidRosterEra::SyncBotToMaster`).
- worldserver boots stably (init → setup.sh config-restart → re-init), no crash/segfault. All 15 raid/other patches applied cleanly alongside IP.
- Side effect (expected on an overlay): some pre-existing characters had items mailed (`_LoadInventory ... reason 19`) because IP changed item templates — non-fatal, items preserved via mail.

**Raid-AI patch compatibility audit (grep of IP's applied world SQL for each patched encounter's creature entries):**
- **UNTOUCHED (safe): Heigan/Naxx (0002), Kologarn/Ulduar (0006), Lich King/ICC (0007)** — WotLK-baseline encounters; IP has zero creature-data references (Ulduar 33768's hits are an item/vendor number in `pvp_vendors.sql`/`tbc_item_changes.sql`, not the creature).
- **AQ40 Twins (0016) — NEEDS BOT TEST RAID:** IP's `vanilla_creature_immunities.sql` sets `CreatureImmunitiesId=-280` on Vek'lor (15276) and touches 15316/15317; `vanilla_creatures.sql` sets `detection_range=20`, `ArmorModifier=1` on both. Patch 0016 relies on the twins' taunt-immunity — confirm the strategy still holds under IP's restored immunity profile.
- **Sunwell (0014) — LOW RISK:** IP's *base* `tbc_raid_hp_restoration.sql` restores TBC boss HP regardless of the `IP_OPT_WOTLK_HP_FOR_TBC_RAIDS` optional (the design's rationale for keeping it OFF was inverted — the optional sets HP *back* to WotLK values). The strategy is framework-style (no HP margins), so HP only changes fight duration. Set `IP_OPT_WOTLK_HP_FOR_TBC_RAIDS=1` if you want Sunwell HP closest to what patch 0014 was tuned on.

**Remaining (interactive):** Task 10 manual per-character era-set; Task 12 era-sync in-game test (`.raidroster create/login/sync` → `.ip get` on a bot); AQ40 bot test raid per the flag above.

## Healer spell-weighting supplement (2026-08-12)

IP's `zz_optional_spell_damage_and_healing.sql` restores the vanilla/TBC "+damage and healing" split
via a hand-curated ~1777-entry list — which has gaps. Found via a synced level-70 TBC healer bot whose
T6 boots/belt still showed consolidated "Spell Power" while the rest of the set showed "damage and
healing". Verified against the binary `Spell.dbc` (parsed with `tools`-style reader): the stock
"spell power" on-equip spells grant *equal* heal/damage (e.g. 17493 = 44/44), whereas IP's `81xxx`
grant *heal-weighted* splits (heal ≈ 2.95×dmg, e.g. 81038 = 29/86). For a healer this is **not
cosmetic** — the balanced piece credits ~half the heal power the restored economy intends, and the
gear scorer routes `MOD_HEALING_DONE` to heal power (which healers weight) vs `MOD_DAMAGE_DONE` to
spell power.

**Fix:** `sql/ip-supplemental/zzz_ip_healer_spell_weighting.sql` — 51 missed TBC healer caster items
(Spirit present, spell-hit absent; PvP sets excluded) remapped balanced `S/S` → heal-weighted `81xxx`
with `dmg = round(0.669*S)` (budget-preserving; matches IP's own curation, e.g. covered ilvl-141 healer
boots use 81038). `setup.sh`'s `apply_ip_optional_sql` copies it into IP's `data/sql/world/base/` when
`IP_OPT_SPELL_DMG_HEALING=1`. Applied + verified on the dev DB (all 51 target spells resolve). DPS
caster items are intentionally left balanced (authentic for them). A worldserver restart (or next
rebuild) is needed for the running server to load the changed item templates.
