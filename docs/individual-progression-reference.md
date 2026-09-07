# Individual Progression — Quick Reference

A printable cheat-sheet for the `mod-individual-progression` era system on this server.

**Mental model:** a progression *state* means that content has been **completed**. So the
name tells you what you just cleared, and the payoff is the content that **opens up** as a
result. Progression is stored as hidden rewarded quests (`66000 + stage`), not a DB column,
and advances automatically on the boss kill / achievement that caps each tier.

Source of truth: `modules/.../src/IndividualProgression.h:227` (enum),
`IndividualProgression.cpp` (getters/setters/maps), `IndividualProgressionPlayer.cpp` (level caps).

---

## Progression states

| #  | State                       | Clearing it unlocks                                    |
|---:|-----------------------------|--------------------------------------------------------|
| 0  | `PROGRESSION_START`         | Fresh character, Vanilla leveling. **Level cap 60**    |
| 1  | `PROGRESSION_MOLTEN_CORE`   | Blackwing Lair                                         |
| 2  | `PROGRESSION_ONYXIA`        | (Onyxia down)                                          |
| 3  | `PROGRESSION_BLACKWING_LAIR`| Zul'Gurub, AQ War Effort, AQ quest line                |
| 4  | `PROGRESSION_PRE_AQ`        | AQ gates open, AQ outdoor war                          |
| 5  | `PROGRESSION_AQ_WAR`        | AQ raids, Field Duty quests, all Cenarion Hold NPCs    |
| 6  | `PROGRESSION_AQ`            | Naxxramas (40) + Scourge Invasion                      |
| 7  | `PROGRESSION_NAXX40`        | "Into the Breach" — Dark Portal opening quest          |
| 8  | `PROGRESSION_PRE_TBC`       | Karazhan, Gruul's Lair, Magtheridon. **TBC entry, cap 70** |
| 9  | `PROGRESSION_TBC_TIER_1`    | Serpentshrine Cavern, Tempest Keep                     |
| 10 | `PROGRESSION_TBC_TIER_2`    | Mount Hyjal, Black Temple                              |
| ~~11~~ | ~~`PROGRESSION_TBC_TIER_3`~~ | *Commented out (Zul'Aman) — value skipped*         |
| 12 | `PROGRESSION_TBC_TIER_4`    | Sunwell Plateau                                        |
| 13 | `PROGRESSION_TBC_TIER_5`    | WotLK Naxx / EoE / OS. **WotLK entry, cap 80**         |
| 14 | `PROGRESSION_WOTLK_TIER_1`  | Ulduar                                                 |
| 15 | `PROGRESSION_WOTLK_TIER_2`  | Trial of the Crusader                                  |
| 16 | `PROGRESSION_WOTLK_TIER_3`  | Icecrown Citadel                                       |
| 17 | `PROGRESSION_WOTLK_TIER_4`  | Ruby Sanctum                                           |
| 18 | `PROGRESSION_WOTLK_TIER_5`  | Final / max state                                      |

> **Value 11 is intentionally missing** (Zul'Aman tier commented out) — the sequence jumps
> 10 → 12. Internal loops still walk the raw integer range 1–18, so these numbers are authoritative.

---

## The three hard level-cap gates

XP is hard-capped into three eras (`IndividualProgressionPlayer.cpp:116`). Everything else
(phasing, NPC awareness, spell-rank blocking, damage/healing scaling) is soft.

| States | Era     | Level cap |
|--------|---------|-----------|
| 0–7    | Vanilla | 60        |
| 8–12   | TBC     | 70        |
| 13–18  | WotLK   | 80        |

---

## What actually advances progression (the boss map)

Each tier advances on **one specific kill** — the tier's flagship boss. Other bosses in the
same tier (e.g. Gruul, Magtheridon) grant no advancement; they're content the *previous* state
already unlocked. From `IndividualProgression.cpp` `bossMap` / `achievementMap`:

| Kill this boss        | Advances to                | Opens          |
|-----------------------|----------------------------|----------------|
| Ragnaros              | 1  `MOLTEN_CORE`           | BWL            |
| Onyxia                | 2  `ONYXIA`                | —              |
| Nefarian              | 3  `BLACKWING_LAIR`        | ZG, AQ line    |
| C'Thun                | 6  `AQ`                    | Naxx40         |
| Kel'Thuzad (Naxx40)   | 7  `NAXX40`                | Dark Portal    |
| **Prince Malchezaar** (Kara) | 9  `TBC_TIER_1`     | SSC, TK        |
| Kael'thas (TK)        | 10 `TBC_TIER_2`            | Hyjal, BT      |
| Illidan (BT)          | 12 `TBC_TIER_4`            | Sunwell        |
| Kil'jaeden (Sunwell)  | 13 `TBC_TIER_5`            | WotLK          |
| Kel'Thuzad (Naxx10/25)| 14 `WOTLK_TIER_1`          | Ulduar         |
| Yogg-Saron            | 15 `WOTLK_TIER_2`          | ToC            |
| Anub'arak (ToC)       | 16 `WOTLK_TIER_3`          | ICC            |
| Lich King             | 17 `WOTLK_TIER_4`          | Ruby Sanctum   |
| Halion                | 18 `WOTLK_TIER_5`          | (max)          |

> **Note:** Gruul, Magtheridon, and the pre-AQ war-effort steps do **not** appear in the
> boss map. Access to those is granted by state 8 (`PRE_TBC`); the jump to state 9 comes
> only from **Prince Malchezaar in Karazhan**.

---

## The "era" shorthand (this server)

For hand-setting characters and for bot roster sync, the 19 states collapse to three eras
(`docs/verification/ip-integration.md:57`, `modules/mod-raid-roster/src/RaidRosterEra.cpp:9`):

| Era     | Stage | Level band |
|---------|-------|------------|
| Vanilla | 0     | ≤ 60       |
| TBC     | 8     | 61–70      |
| WotLK   | 13    | 71–80      |

Bots fall back to 0 / 8 / 13 by their master's level band when the master's era reads as unset.

---

## Setting a character's era (GM commands)

`.ip` root command, all `SEC_GAMEMASTER`, console-usable
(`src/cs_individualProgression.cpp`):

- `.ip get [player]` — read current progression level
- `.ip set [player] <level>` — force-set to a stage (authoritative; can raise or lower)
- `.ip setbot` — set every RND bot in your group to your level
- also: `.ip setrep`, `.ip pvp`, `.ip tele`, `.ip attune`

**Gotcha:** `.ip set` **cannot set a character to stage 0** — the force-set early-returns on
`newState == 0`. To reset a char fully to Vanilla you must hand-clear its `66000+i` quests
(the raid-roster code does this for the Vanilla case).

### Worked example

A hunter at level 70 who has killed **Gruul and Magtheridon** but **not cleared Karazhan** →
set to **8** (`PRE_TBC`). Gruul/Mag kills don't advance progression; only downing **Prince
Malchezaar** moves them to 9 and opens SSC/TK. Stage 8 correctly reflects "TBC entry, cap 70,
working through T4, Kara not yet cleared."

```
.ip set Huntername 8
```
