# Era Talents — TBC Phase 9 (Mage) verification record

> **Generation superseded (2026-09-06 final review):** the current stamp is the closing row of docs/verification/era-talents-final-review-tbc-vanilla.md §10 — reinstall patch-V.mpq + restage the addon before any real-client pass.

**Status: HEADLESS-COMPLETE, AWAITING REAL-CLIENT SIGN-OFF.**
Generation stamp **`1de3388d`** (`acore_world.era_talent_meta` `generation` = `1de3388d`; the same
stamp is in `client-addons/_data-patches/patch-V.mpq`, rebuilt 2026-09-05 20:48 by Task 5's
`tools/era-regen.sh --sync-fork`). Branch `feat/era-talents`, **merged to master 2026-09-07**.

Everything below was measured on the dev-box stack (local Docker: `ac-db-import` then a
`--force-recreate`d `ac-worldserver` built from this branch) with the **TBC allowlist flipped on**
(`modules/mod-era-talents/src/EraTalentIP.cpp:21` reads
`return era == ERA_VANILLA || era == ERA_TBC;`). That flip is a **deliberately UNCOMMITTED dev-box
edit** and is never staged — every readiness-gated arm in the module (`EraReplacementSpellReady`,
`EraNodeIsWired`) is inert without it, which is exactly the behaviour a shipped build has today.

Task commits: `bc9430f` (Task 1 skeleton + `_ref` + Amendment A), `caced7f` + `713a452` (Task 2
Arcane + fix round), `324a916` (Task 3 Fire + fix round), `af55b5d` (Task 4 Frost), `ab8680a`
(Amendment D), `88c1603` (Task 5 id ledger), `e8bbadd` (Task 5 wiring), `8bc35f2` (Amendment E),
`6e6ca6c` (doctor Shatter PROBLEM text), plus this record's commit.

Spec: `docs/superpowers/specs/2026-09-05-era-talents-tbc-phase9-mage-content-design.md`
(+ Amendments A, B, C, D, E — cited by letter throughout; not restated here).
Plan: `docs/superpowers/plans/2026-09-05-era-talents-tbc-phase9-mage.md` (its
**"Task 5 id ledger (AUTHORITATIVE)"** is the id source).
Authoring input: `era-data/_ref/tbc/mage-spells.yaml`.

---

## 1. What shipped

- **67 TBC mage talent nodes**, ids `20800-20866` (Arcane 23 / Fire 22 / Frost 22), all wired to
  ranks. Class 8, `SpellFamilyName` 3 — **no class/family inversion this phase**.
- **Generated auto-passives** in the class window `942400-942935` (Shatter's five at
  `942856-942860`).
- **47 hand helpers** in the claimed slice `948750-949059` (exact ids in §8). The **substrate
  partition `948750-948759` is EMPTY by design** — the mage tree mints no identity marker; every
  trained chain gates on its own r1 clone id.
- **Nine CLONE nodes**, seven of them trained chains across BOTH eras (four TBC + three Vanilla),
  plus two node-granted Magic Absorption chains — 37 custom `trainer_spell` rows on TrainerId 16
  and nine `spell_ranks` chains, all in one hand SQL (`2026_09_08_02`). **Nothing deleted from any
  stock row.**
- **ONE core touch: patch 0018's band widened from `[920000, 933000)` to `[920000, 950000)`** so
  the TBC Shatter node's auto-passives arm it. No new marker, no new DUMMY misc value (§9).
- **One new module script**, `era_mag_ice_barrier_tbc` (TBC's 0.1 spell-power coefficient).
- **Fourteen Vanilla re-audit FIX rows** bundled into the same generation (§10), including four
  whole Vanilla clone chains and the stranded-id cleanup.
- **No DUMMY-marker misc consumed** (next free stays **19**). Reused: misc **1** (Shatter — now a
  cross-era MECHANIC flag), misc **2** (Molten Shields fire-ward reflect), misc **3** (Frost
  Warding frost-ward reflect).
- **No `petBuffBase:`** — mage declares none, and the Water Elemental needs none (§6 answer l).
- **No `kEraNameAliases` row and no patch-0021 mage arm** — the by-id grep is empty (§12).

---

## 2. Per-tab node counts and disposition tallies

Recounted programmatically from `disposition_table:` after the Ice Barrier GRANT → CLONE flip
(the ref's `disposition_tally:` block; never hand-maintained).

| Tab | Nodes | GRANT | AUTHOR | CLONE | RECONSTRUCT |
|---|---|---|---|---|---|
| Arcane | 23 | 4 | 15 | 4 | 0 |
| Fire | 22 | 8 | 10 | 4 | 0 |
| Frost | 22 | 8 | 13 | 1 | 0 |
| **Total** | **67** | **20** | **38** | **9** | **0** |

CLONE nodes: Arcane **20804** Magic Absorption / **20813** Presence of Mind / **20819** Arcane
Power / **20822** Slow; Fire **20830** Pyroblast / **20837** Blast Wave / **20841** Combustion /
**20844** Dragon's Breath; Frost **20863** Ice Barrier.

**Zero RECONSTRUCTs.** The two nodes whose every rank is WAGO_ONLY — **20803** Wand Specialization
and **20850** Improved Frost Nova — are single-effect spellmod/stat passives the generator emits
directly, so they are ordinary AUTHORs.

---

## 3. Fourteen-family survival-map verdict distribution

**291 ids swept.** Of the **193 rank spells**:

| Verdict | Count |
|---|---|
| DIVERGED | **101** |
| LIVE_MATCH | **63** |
| WAGO_ONLY | **23** |
| LIVE_MATCH_UNVERIFIED_SLOT | **6** |

The high DIVERGED share is why only 20 of 67 nodes could be GRANTs.

---

## 4. The seven talent-rooted trained chains — resolutions, levels and costs

Every custom row is on **TrainerId 16** at the **live stock counterpart's level and cost** (never
`ReqLevel × 1000`), chained on `ReqAbility1` behind the previous rank, rooted at the
**node-granted** r1 clone. **37 rows total. Nothing stock deleted** — each stock chain remains
self-gating on its stock r1 talent, which an era mage never holds, so the stock chain is
unreachable for era characters and intact for WotLK ones.

### TBC (four chains, 27 rows)

| Node | Chain | r1 → trained ranks | Deciding divergence | Levels | Costs |
|---|---|---|---|---|---|
| 20830 | Pyroblast r1-r10 | **948870** → 948871-948879 | TBC `CastingTimeIndex 171` = **6000 ms** vs live 6 = 5000 ms on all ten ranks | 24/30/36/42/48/54/60/66/70 | 100/400/650/900/1400/1800/2100/3900/7500 |
| 20837 | Blast Wave r1-r7 | **948880** → 948881-948886 | live carries an extra `e3 = KNOCK_BACK val 80 misc 100` TBC lacks (the -50% snare is identical) | 36/44/52/60/65/70 | 400/1148/1748/2100/3500/6000 |
| 20844 | Dragon's Breath r1-r4 | **948890** → 948891-948893 | disorient `DurationIndex 27` = **3000 ms** vs live 28 = 5000 ms | 56/64/70 | 1900/2200/2500 |
| 20863 | Ice Barrier r1-r6 | **948964** → 948965-948969 | absorb coefficient **0.1** vs live's hardcoded **0.8068** (§6 answer b) | 46/52/58/64/70 | 1700/1748/2000/2500/6000 |

Stock-swap rows (the era mage must not hold these):
`{20830, 11366, {12505,12522,12523,12524,12525,12526,18809,27132,33938,42890,42891}}`,
`{20837, 11113, {13018,13019,13020,13021,27133,33933,42944,42945}}`,
`{20844, 31661, {33041,33042,33043,42949,42950}}`,
`{20863, 11426, {13031,13032,13033,27134,33405,43038,43039}}`, plus the single-spell swaps
`{20841, 11129, {}}` (Combustion → 948887), `{20822, 31589, {}}` (Slow → 948773) and the
AP/PoM strips (stock **12042/12043** → 948760/948761).

### Vanilla (three chains, 10 rows — the re-audit's own clones)

| Node | Chain | r1 → trained ranks | Levels | Costs |
|---|---|---|---|---|
| 18024 | Pyroblast r1-r8 | **932310** → 932311-932317 | 24/30/36/42/48/54/60 | 100/400/650/900/1400/1800/2100 |
| 18030 | Blast Wave r1-r5 | **932320** → 932321-932324 | 36/44/52/60 | 400/1148/1748/2100 |
| 18049 | Ice Barrier r1-r4 | **932331** → 932332-932334 | 46/52/58 | 1700/1748/2000 |

### Node-granted chains (no trainer rows, `spell_ranks` only)

Magic Absorption **948762-948766** (TBC) and **932326-932330** (Vanilla) — five ranks each, granted
per talent rank.

### `spell_ranks` chain lengths, measured in the live DB

```
932310 8   932320 5   932326 5   932331 4   948762 5
948870 10  948880 7   948890 4   948964 6
```

Nine chains, exactly the authored lengths. **Explicit `spell_ranks` is mandatory for every era
clone chain** — a GRANTed stock talent gets its `ChainEntry` from Talent.dbc via
`LoadSpellTalentRanks`, but a band id is not in Talent.dbc (§6 answer g).

### Orphaned-higher-rank check

Run once over TrainerId 16 at Task 5 (Amendment E.2): the four rows it returns (roots **133 / 168 /
44425 / 44457**) are **pre-existing stock rows** whose roots are starting/book-taught spells with no
trainer row — not this phase's, and unchanged by it. The seven new chains root on node-granted
clones by construction.

---

## 5. Censuses (from `era-data/_ref/tbc/mage-spells.yaml`)

- **`proc_data_census:`** — every `spell_proc` row touching this tree, re-dumped **with the column
  header line** after a reviewer found nine rows column-shifted in the first pass (a row whose
  `ProcFlags` is 0 drops the zero in a headerless `mysql -N` dump and mislabels every later value).
  Each row is now an explicit `key: value` map so the shift cannot recur. Load-bearing corrections
  that came out of the re-dump: Improved Scorch `typeMask 0` (not 2), Impact `typeMask 1 /
  phaseMask 2`, Winter's Chill `typeMask 0 / phaseMask 2 / hitMask 3`, Arcane Concentration
  `procFlags 0 / typeMask 1`, Magic Absorption `hitMask 8 / phaseMask 0 / cooldown 1000`.
- **`script_census:`** — the thirteen core scripts binding this tree by id, each with what it reads
  and its disposition (rebind / era script / grant / untouched). Outcomes: two rebinds to era clones
  (`spell_mage_magic_absorption`, `spell_mage_combustion`), one parity rebind
  (`spell_mage_dragon_breath`), one **new** era script (`era_mag_ice_barrier_tbc`), and eight left
  stock.
- **`capstone_chains:`** / **`trainer_cost_curve:`** — the four TBC chains' per-rank TBC and live
  values plus the live TrainerId 16 level/cost rows that §4 quotes.
- **Family-3 bit census** (`fam3_bits.txt`) — every family-3 record in `Spell.dbc` bucketed per
  identity bit; the input for every authored spellmod mask and for the pseudo-bit-28 chill
  convention (§6 answer f).
- **`core_hardcodes:`** — §6.
- **`reuse_ledger:`** — §7. **`vanilla_fix_rows:`** — §10.

Script bindings verified in the live DB after boot:

```
932325 spell_mage_combustion        948887 spell_mage_combustion
932326 spell_mage_magic_absorption  948762 spell_mage_magic_absorption
948890 spell_mage_dragon_breath
948964..948969 era_mag_ice_barrier_tbc   (six rows)
```

---

## 6. Core-hardcode table — the twelve questions (a)-(l)

| # | Question | Answer | Consequence |
|---|---|---|---|
| **(a)** | Pyroblast `CastingTimeIndex` | TBC index **171** = 6000 ms; live index 6 = 5000 ms, on all ten ranks (Category 0→290 too) | **CLONE** chain 948870-948879 |
| **(b)** | Ice Barrier absorb coefficient | **0.1** — cmangos `mangos-tbc` `sql/base/mangos.sql:14318` `(11426, 0.1, …)`, chain-rooted at 11426 by `spell_chain` :14654-14659 — vs live's `0.8068f` hardcoded at `spell_mage.cpp:624/:660`. An 8× divergence at L70 spell power. wago has no coefficient column; wowsims/tbc does not model Ice Barrier | **CLONE** 948964-948969 + the new `era_mag_ice_barrier_tbc` (user decision, A.4 item 1) |
| **(c)** | Blast Wave knockback | live carries `e3 = eff 98 KNOCK_BACK val 80 misc 100`; TBC has only the -50% snare, which is identical | **CLONE** 948880-948886 (effect 3 zero-filled); `spell_mage_blast_wave` NOT rebound — it exists only to *prevent* the knockback the clones lack |
| **(d)** | Dragon's Breath disorient duration | TBC `DurationIndex 27` = 3000 ms vs live 28 = 5000 ms; Category 50→1215; live adds `AuraInterruptFlags 0x480000` | **CLONE** 948890-948893 + `spell_mage_dragon_breath` rebind (parity insurance) |
| **(e)** | Arcane Potency | the live script is a two-way rank switch (`GetRank()==1 ? 57529 : 57531`) that cannot serve three TBC ranks; TBC's own aura-107/misc-12 encoding is inert on 3.3.5a because a spellmod matches the spell being CAST | **AUTHOR** per-rank proc → fresh crit buffs **948767-948769** (10/20/30); no rebind |
| **(f)** | Improved Blizzard | TBC chill 12484/12485/12486 = **-30/-50/-65%** vs live -25/-40/-50; the live script switches on rank to those very stock ids | **AUTHOR** fresh chill helpers **948961-948963** on family-3 pseudo-bit 28 + an era `procPassive`; do NOT grant 11185 |
| **(g)** | Ignite | `8 * GetRank()` = 8/16/24/32/40 matches TBC exactly; the lone `ProcTypeMask` diff is neutralised by the `spell_proc -11119` row. Ranks come from Talent.dbc, not `spell_ranks` | **GRANT**. Standing consequence: every rank-driven era CLONE chain needs an explicit `spell_ranks` chain |
| **(h)** | Magic Absorption / Master of Elements / Clearcasting (all read `GetAmount()`) | Magic Absorption: TBC chain is **29441/29444/29445/29446/29447**, carries the mana rider IN DATA, and live's rank-5 id 29447 is **"Torment the Weak"** — a hard block on granting it. Master of Elements 29074-29076 val 10/20/30 in both eras. Clearcasting/Arcane Concentration LIVE_MATCH | Magic Absorption **CLONE** 948762-948766 + per-rank rebind + its own proc row (`typeMask 7`, `hitMask 8`, `cooldown 1000`); MoE and Clearcasting **GRANT** |
| **(i)** | Shatter | `Unit.cpp:9265-9279` hardcodes **17/34/50** off misc 849/910/911 and has no `case` for TBC's 912/913. Ranks 1-3 read LIVE_MATCH on all fourteen families and STILL pay the wrong numbers | **No stock Shatter rank may ever be granted.** Node 20857 grants nothing; auto-passives **942856-942860** (misc 1, amounts 10-50) arm patch 0018 (§9) |
| **(j)** | Molten Fury | the amount is data (`aura 112 misc 4919/4920`, TBC val 10/20); the **threshold** is `AURA_STATE_HEALTHLESS_35_PERCENT` in `Unit.cpp:8530-8536` with no data field | **AUTHOR** at 10/20% on a 35% window — strictly stronger than TBC's 20%. **Accepted gap 3** (user decision, A.4 item 2) → **CLOSED 2026-09-07, patch 0026** |
| **(k)** | Wand Specialization / patch 0025 | `grep -c ITEM_SUBCLASS_WEAPON_WAND` on the fork's `Unit.cpp` returns **3** (era band block `:8475-8487` + the stock guard `:12787`) — 0025 is applied. TBC 6057/6085 are byte-for-byte the shape the fixed Vanilla 18004 ships | straight **AUTHOR** under the same guard; no spell leak |
| **(l)** | Water Elemental | TBC 31687 is a real `SPELL_EFFECT_SUMMON` (`misc 510`, `durIdx 22` = **45 s**); live 31687 is a DUMMY whose script casts **70907**, itself `misc 510 durIdx 22` — same creature, same 45 s. Cooldown and `manaPct 16` identical. The permanent variant 70908 is glyph-gated, and `EraGlyphGate` blocks glyphs for era characters | **GRANT 31687**; no `petBuffBase:` needed |

Other `SPELLFAMILY_MAGE` arms checked and cleared, with the two that bound authoring:
`SpellMgr.cpp:3752 case 12579` gives Winter's Chill `SINGLE_AURA_STACK` **by id** → the clone
**948960** authors `singleAuraStack: true` (lesson 14); `SpellInfoCorrections.cpp:1055 {11129}
Dispel = DISPEL_NONE` → the Combustion clones author `dispelType: 0`.

---

## 7. Reuse ledger — outcome: 4 FRESH / 1 REUSE AS-IS

Five Vanilla-helper candidates examined. **No Vanilla helper's VALUES were found wrong.**

| Candidate | Values equal? | Decision | Reason |
|---|---|---|---|
| 932760 Arcane Power | yes (+30/+30/+30, cd 180000) | **FRESH 948760** | standing rule (memory `tbc-era-value-basis-wago-2540`) — TBC magnitudes are fresh clones at wago values |
| 932761 Presence of Mind | yes (cd 180000, ProcCharges 1) | **FRESH 948761** | same standing rule |
| 932980-932982 Blizzard chill | yes (-30/-50/-65) | **FRESH 948961-948963** | per-era identity — a Vanilla and a TBC mage must not share a helper (a Vanilla-only retune would move both); same pseudo-bit 28 so TBC Permafrost's mask reaches them |
| 932990 / 932991 ward-reflect helpers | n/a — they carry no era value; the % arrives as the marker's base point | **REUSE AS-IS** | `era_ward_reflect` is band-gated, not era-gated: it scans the caster's DUMMY auras for misc 2/3 inside `[920000, 950000)`, which already contains the TBC auto-passive window. A TBC Molten Shields (misc 2) / Frost Warding (misc 3) marker arms it with **zero C++ change and no new helper** |

Neither AP nor PoM clone authors the live `ExcludeCasterAuraSpell` cross-lock — **TBC allowed
Arcane Power and Presence of Mind together**.

---

## 8. Helper id table — 47 ids used in `948750-949059`, with documented holes

| Partition | Range | Used | Ids |
|---|---|---|---|
| Substrate | 948750-948759 | **0 — EMPTY by design** | — |
| Arcane | 948760-948859 | **14, no holes** | **948760** Arcane Power clone, **948761** Presence of Mind clone, **948762-948766** Magic Absorption r1-r5, **948767-948769** Arcane Potency crit buffs (10/20/30), **948770** Improved Counterspell silence, **948771-948772** Improved Blink buffs, **948773** Slow clone |
| Fire | 948860-948959 | **23** | **948860** Fire Vulnerability debuff, **948870-948879** Pyroblast r1-r10, **948880-948886** Blast Wave r1-r7, **948887** Combustion, **948890-948893** Dragon's Breath r1-r4 |
| Frost | 948960-949059 | **10, no holes** | **948960** Winter's Chill debuff, **948961-948963** Blizzard chill r1-r3, **948964-948969** Ice Barrier r1-r6 |

**Free within the slice:** 948750-948759 (the whole substrate), **948774-948859**,
**948861-948869**, **948888-948889**, **948894-948959**, **948970-949059**.

**948861-948869 and 948888-948889 are INTENTIONAL holes, not oversights.** The Fire partition is
laid out on decade boundaries (`8 6 x` debuffs, `887x` Pyroblast, `888x` Blast Wave, `889x`
Dragon's Breath) so a reader can tell at a glance which chain an id belongs to. This is a
deliberate readability exception to "allocate in order"; lesson 22 ("leave a HOLE rather than
renumber a pinned id") makes holes cheap, and the partition is 100 wide. **Do not "fix" them.**

**Next free TBC helper id: 949060.**

### Vanilla ids this phase added

| Slice | Used | Free within slice |
|---|---|---|
| 932310-932339 | **932310-932317** Pyroblast r1-r8, **932320-932325** Blast Wave r1-r5 + Combustion, **932326-932330** Magic Absorption r1-r5, **932331-932334** Ice Barrier r1-r4 | 932318-932319, 932335-932339 |
| 932760-932769 | **932762** Improved Counterspell silence (4 s), **932763** Fire Vulnerability, **932764** Winter's Chill — beside the pre-existing 932760 AP / 932761 PoM | 932765-932769 |

Pre-existing helpers this phase relies on but did not add: 932760/932761 (AP/PoM Vanilla clones),
932980-932982 (Vanilla Blizzard chill), 932990/932991 (ward reflect).

**Stranded ids retired:** node 18005 stopped being a `mechanic: stat` node, so its former
auto-passives **920040-920044** are in no era's rank table and `StripOrphanedGrants` can never see
them (lesson 17f). They are stripped by an unconditional `CLASS_MAGE` reconcile arm, checked for
every era and class by a dedicated doctor predicate, and deleted from `spell_dbc` by the hand SQL.
Verified GONE in the DB (§11.1) and on the one bot that carried 920044 (§11.4).

---

## 9. The one core touch — patch 0018's band, and its evidence

`patches/0018-core-shatter-crit-vs-frozen.patch` reads a caster's `SPELL_AURA_DUMMY` marker with
`EffectMiscValue 1` and adds its amount as crit% against `AURA_STATE_FROZEN` targets. Its band was
`[920000, 933000)` — which contains the **Vanilla** Shatter auto-passives (920360-920364) but not
the **TBC** ones (942856-942860). Task 5 widened it to **`[920000, 950000)`** — the same band
`era_ward_reflect` and patch 0025 already use. No new marker, no new misc value: **misc 1 is now a
cross-era MECHANIC flag** (lesson 25's design rule — a patch-arming marker may be shared across
eras; only a *trainer gate* must be per-era).

Regen and re-apply (Amendment E.1, done at Task 5):

- `tools/regen-shatter-patch.sh` is **baseline-aware for two contaminators now** — `Unit.cpp` is
  also edited by **0012** (bot aura batching) and **0025** (wand-spec era leak). The script reverses
  **0025 then 0012**, diffs, then re-applies **0012 then 0025** in numeric order to match
  `apply_patches`' lexicographic glob (memory `regen-baseline-contamination-trap`).
- **The pristine re-apply must reset `Unit.h` too** — patch 0012 spans `Unit.cpp` *and* `Unit.h`
  and `git apply` is atomic, so a `.cpp`-only checkout makes 0012 fail wholesale. Standing
  correction to the plan's Step 7.
- The hunk comment no longer contains the literal `933000`, so `grep -c 933000` on the patch is
  **0**; the history lives in the regen script header and the CLAUDE.md patch-inventory row.
- A **full ordered apply of every `patches/*.patch` on a clean fork** was verified at Task 5.

Evidence taken for this record, on the worktree the running image was built from:

```
$ grep -c "eraSpellId < 950000" azerothcore-wotlk/src/server/game/Entities/Unit/Unit.cpp
1
$ grep -c 950000 patches/0018-core-shatter-crit-vs-frozen.patch   -> 2
$ grep -c 933000 patches/0018-core-shatter-crit-vs-frozen.patch   -> 0
$ stat -c '%y' .../Unit.cpp                    2026-09-05 20:38:50 -0500
$ docker image inspect --format '{{.Created}}' acore/ac-wotlk-worldserver:master
                                               2026-09-05T20:54:46-05:00
```

**The image was built AFTER the widened source** — that is the headless proof the widened band is
in the running binary (the image ships no gdb, and a restart for the CLAUDE.md gdb one-off was not
worth it). The **behavioural** proof is the Frost-build doctor line in §11.3; the crit magnitude
itself is a real-client item (§14).

Container canary — the bind-mounted module the image compiled carries the final Task 6 source:

```
$ docker exec ac-worldserver sh -c 'grep -c "holds ONE misc-1 rank passive" \
    /azerothcore/modules/mod-era-talents/src/EraTalentsCommand.cpp'
1
```

---

## 10. Bundled Vanilla re-audit — 35 CLEAN / 14 FIX, all fourteen shipped

The full Vanilla mage tree (49 nodes) was re-audited under every standing check class as the
phase's Q2 scope. **Headline:** Vanilla mage was the ONLY dataset in the repo using
`op: crit, kind: pct`, and a PCT crit spellmod is effectively inert — `Player::ApplySpellMod` skips
PCT when the base value is 0 — so **four of the fourteen rows are that one defect**.

| Node | Name | Fix shipped | L60 real-client re-check item |
|---|---|---|---|
| 18005 | Magic Absorption | mana-on-full-resist DUMMY rider via Vanilla clone chain **932326-932330** + per-rank `spell_mage_magic_absorption` rebind + its own proc row (`typeMask 7`, `hitMask 8`, `cooldown 1000`); the rider was never wired before | mana is restored on a **full resist** |
| 18008 | Improved Arcane Explosion (TBC name: Arcane Impact) | `crit kind: pct → flat` | the crit bonus actually applies |
| 18011 | Improved Counterspell | silence 2 s → **4 s** via clone **932762** | the Counterspell silence lasts 4 s |
| 18022 | Incinerate | `crit kind: pct → flat` | the crit bonus actually applies |
| 18023 | Improved Flamestrike | `crit kind: pct → flat` | the crit bonus actually applies |
| 18024 | Pyroblast | clone chain **932310-932317** (`castTimeIndex` → 6 s; live values otherwise) | cast bar reads **6 s**; r2-r8 purchasable at L24-60; stock ranks not offered |
| 18026 | Improved Scorch | payload → Fire Vulnerability clone **932763** (+3%/stack, ×5, 30 s; `typeMask 0` aligned to the census) | Fire Vulnerability stacks to **5** with era text |
| 18029 | Critical Mass | `crit kind: pct → flat` (stock `aura 71` stat shape) | the crit bonus actually applies |
| 18030 | Blast Wave | clone chain **932320-932324** minus live's `e3` KNOCK_BACK | **no knockback**; r2-r5 purchasable at L36-60 |
| 18032 | Combustion | clone **932325**: `RecoveryTime 180000`, bare DUMMY effect 1, `dispelType: 0`, `spell_mage_combustion` rebind | tooltip/cooldown read **3 min**; the 3-charge crit-stack consume still works |
| 18036 | Ice Shards | `critDamage kind: flat → pct` | the crit-damage bonus actually applies |
| 18039 | Perma Frost → **Permafrost** | display name only (both measurable stores spell it one word); slug and node id unchanged | the talent reads **Permafrost** |
| 18048 | Winter's Chill | payload → clone **932764** (+2% **Frost** crit taken, ×5, `singleAuraStack: true` per `SpellMgr.cpp:3752`) | Winter's Chill stacks to **5** with era text and does **not** share a debuff with Improved Scorch |
| 18049 | Ice Barrier | clone chain **932331-932334** with **no** script binding — a clone outside the `-11426` chain is never touched by `spell_mage_ice_barrier`, which IS the 1.12 base-points-only behaviour (cmangos `mangos-classic` has no `spell_bonus_data` subsystem at all) | absorb tooltip = the **base value** (438 at r1) with spell power equipped; r2-r4 purchasable at L46-58 |

Improved Scorch and Winter's Chill **were never one debuff in either era** (22959 and 12579 are
separate in both) — same authoring outcome as the first pass, corrected reason.

Two "fidelity NOTE, not a fix" rows were recorded and deliberately **not** changed: 18031 Fire
Power and 18040 Piercing Ice omit the `misc 22 SPELLMOD_DOT` half TBC's own version carries; the
Vanilla tooltips say "damage done by your fire/frost spells" and no 1.12 evidence exists for the
DoT half.

---

## 11. Headless gate (generation `1de3388d`)

### 11.1 Boot and DB checks

`ac-db-import` applied `2026_09_08_00` (data) / `_01` (custom spells) / `_02` (hand SQL), then
`ac-worldserver` was `--force-recreate`d and came up clean:

```
docker compose ps ac-worldserver  ->  Up (0.0.0.0:8085->8085/tcp)
[mod-era-talents] startup (enable=true)
[mod-era-talents] loaded 1011 talent nodes (generation 1de3388d)
```

- **No `kAuthoredOrders` drift WARN** anywhere in the boot (`grep -iE "drift|WARN.*era"` → nothing)
  — the three TBC mage orders validate and TBC-band mage bots spend the full **61**-point budget.
- **No `did not match dbc effect data` line** naming any `942xxx` / `948xxx` / `9323xx` id — in
  particular none for `948964-948969`, i.e. the new `era_mag_ice_barrier_tbc` hook binds correctly.
- **No ERROR line at all in the boot window.** The only ERRORs seen during the whole session are
  core `[1062] Duplicate entry '<guid>-<spell>' for key 'pet_spell.PRIMARY'` lines that surfaced
  later, during the ladder, on unrelated **warlock** pets (`329-19647` Spell Lock on a felhunter,
  `940-27268` Firebolt r9 on an imp) — the same not-ours `Pet::_SaveSpells` duplicate-insert the
  warlock record diagnosed. The mage tree teaches no pet spells and declares no pet band.

`acore_world` checks, all as expected:

| Check | Result |
|---|---|
| `era_talent_meta` generation | **`1de3388d`** — the Task 5 regen stamp |
| `era_talent` era 1 / class 8 | **67** nodes (all nine TBC classes present: 61-67 each) |
| nodes with at least one `era_talent_rank` row | **67 wired** |
| family guard: `942400-942935` ∪ `948750-949059` with `SpellClassSet NOT IN (0,3)` | **no rows** |
| outside the claimed slice: `spell_dbc` in `949060-949999` | **no rows** |
| stranded `spell_dbc` in `920040-920044` | **no rows** — the `_02` DELETE landed |
| Shatter auto-passives `942856-942860` | misc **1**, base points **9/19/29/39/49** (= 10-50 with DieSides 1) |
| custom TrainerId 16 rows in `948750-949059` ∪ `932310-932339` | **37** |
| `spell_ranks` chains | nine, lengths **8/5/5/4/5/10/7/4/6** (§4) |
| `spell_script_names` bindings | `era_mag_ice_barrier_tbc` ×6, `spell_mage_combustion` ×2, `spell_mage_magic_absorption` ×2, `spell_mage_dragon_breath` (§5) |

### 11.2 The Gnablarizz ladder — five rungs, zero orphans

Bot **Gnablarizz** (guid 178, L80 mage), driven through `tools/wgconsole.py` (first argument is the
OUTPUT FILE): L60 → L70 → `reset` + re-sync → L80 → L70. As in the warlock phase,
`.raidroster syncone` answers **"Run this in-world as a player."** from the console, so each
rebuild comes from the **level-change hook** (`OnBotLevelChanged` → `FactoryReconcile`) — the same
entry point, exercising the same teardown-first path; the `[mod-era-talents] bot build: Gnablarizz
class 8 tabN 51/61pts` lines are its receipt.

| Rung | era / points | Tab built | Era spells held | Chains | Shatter | Orphans |
|---|---|---|---|---|---|---|
| **L60** | `era=Vanilla` spent **51** | tab 2 (Frost) | Vanilla auto-passives 920020-920388 + stock grants 11958 / 45438 + **Vanilla clone 932331**; **no 948xxx, no 942xxx** | Ice Barrier (18049) clone r1 known, **4 ranks** (highest r4); Pyroblast/Blast Wave r1 MISSING (nodes not taken) | Vanilla node **18045 rank 5** → `band DUMMY marker: spell 920364 misc 1 amount 50` (the doctor's `Shatter:` predicate is the TBC node 20857, so it prints no line here) | **0** |
| **L70** | `era=TBC` spent **61** | tab 1 (Fire) | TBC auto-passives 942412-942748 + stock grants 11213 / 12341 / 12848 / 29076 / 31640 / 11368 / 12400 + **clones 948870 / 948880 / 948887 / 948890**; **no 920xxx, no 9323xx** | Pyroblast **10 ranks**, Blast Wave **7**, Dragon's Breath **4**, Ice Barrier r1 MISSING (node not taken) | node 20857 rank **0**, misc-1 auras held **0** | **0** |
| **L70 after `.eratalents reset` + re-sync** | `era=TBC` available **61** spent **0** | — | "(no era talents spent this era)"; **all four chains clone r1 MISSING, 0 ranks known** | trained clone ranks **cascade-stripped** — lesson 21's tell, clean | rank 0, auras 0 | **0** |
| **L80** | `era=WotLK` available **71** spent 0 | native | **no 942xxx / 948xxx / 9323xx / 920xxx band spell and no band aura at all**; `11366 -> 11366` and `11129 -> 11129` show the native WotLK build holding the **stock** Pyroblast/Combustion, which is correct | — | — | **0** |
| **L70 (return)** | `era=TBC` spent **61** | tab 1 (Fire) | byte-identical to the first L70 rung | identical | rank 0, auras 0 | **0** |

```
grep -icE "ORPHAN|PROBLEM|WRONG-ERA|STALE" ladder.out   ->  0
grep -cE  "ORPHAN ERA SPELL|<-- PROBLEM"    ladder.out   ->  0
```

**The L80 rung is a real test, not a vacuous one.** The doctor's orphan predicate walks the
character's whole `GetSpellMap()` over `[920000, 950000)` and asks whether each known band spell is
granted by a learned talent — it is **not** scoped to `NodesFor(currentEra)` the way
`StripOrphanedGrants` is. A leftover TBC or Vanilla clone surviving the up-move to WotLK would
therefore have been reported here, and none was.

`AI resolve ->` lines, per rung (the by-name resolver contract; a 0 means the by-id AI check would
be permanently false, and for most of these rows 0 is the *correct* answer because the node is not
in that build):

| Stock id | L60 (Vanilla, Frost build) | L70 (TBC, Fire build) | L80 (WotLK) |
|---|---|---|---|
| 11426 Ice Barrier | **932334** (top known rank) | 0 — node not taken | 0 |
| 11366 Pyroblast | 0 — node not taken | **948879** | **11366** (stock, native build) |
| 11113 Blast Wave | 0 | **948886** | 0 |
| 31661 Dragon's Breath | 0 (correct always in Vanilla) | **948893** | 0 |
| 11129 Combustion | 0 | **948887** | **11129** (stock, native build) |
| 11958 Cold Snap | **11958** (stock GRANT — control row) | 0 — node not taken | 0 |
| 45438 Ice Block | **45438** (baseline) | **45438** | **45438** |
| 12042 / 12043 AP / PoM | 0 / 0 | 0 / 0 — nodes not in the Fire build | 0 / 0 |

**Stock 12042 / 12043 are ABSENT for both era mages at every rung** — the AP/PoM strip arms hold.

### 11.3 The Frost rung — Shatter's `M == (R ? 1 : 0)` invariant

Gnablarizz builds Fire at 70, so a second bot was levelled to 70 to land a **Frost** build.
`Ithesen` (guid 258) builds **tab 2, 61 pts**:

```
== eratalents doctor: Ithesen (class 8 level 70) ==
era=TBC storedEra=Vanilla available=0 spent=61 generation=1de3388d
  node 20853 rank 1: grant 12472 known=yes spellInfo=yes         (Icy Veins, stock GRANT)
  node 20857 rank 5: grant 942860 known=yes spellInfo=yes         (Shatter r5)
  node 20859 rank 1: grant 11958 known=yes spellInfo=yes          (Cold Snap, stock GRANT)
  node 20863 rank 1: grant 948964 known=yes spellInfo=yes
    script: era_mag_ice_barrier_tbc
  node 20866 rank 1: grant 31687 known=yes spellInfo=yes          (Water Elemental, stock GRANT)
  chain Ice Barrier (node 20863): talent rank 1, clone r1 known, ranks known 6 (highest r6)
  Shatter: node 20857 rank 5, misc-1 auras held = 1
  band DUMMY marker: spell 942860 misc 1 amount 50
  AI resolve: 11426 (Ice Barrier r1 ...) -> 948969
  AI resolve: 12472 (Icy Veins ...) -> 12472
  AI resolve: 11958 (Cold Snap ...) -> 11958
  AI resolve: 31687 (Summon Water Elemental ...) -> 31687
== doctor done ==      (0 ORPHAN, 0 PROBLEM)
```

- **`misc-1 auras held = 1` at rank 5 is CORRECT, and the doctor's invariant is `M == (R ? 1 : 0)`,
  not `M == R`** (Amendment E.1d). A `mechanic: stat` node holds exactly ONE rank passive:
  `EraTalents.cpp` grants `rankSpell[newRank-1]` and removes the previous rank on every rank-up and
  on reset. The marker line confirms the held passive is **942860, misc 1, amount 50** — Shatter's
  top rank, which is exactly what patch 0018 reads. (This is new lesson **26**.)
- The Ice Barrier clone chain is fully exercised: r1 granted by the node, **six ranks known**, the
  new era script bound, and the resolver returning the top rank **948969**.
- The three Frost stock GRANTs resolve to themselves — including **Icy Veins 12472** and
  **Water Elemental 31687**, whose spellbook placement is Amendment D.6's real-client item.

### 11.4 Broad all-class doctor sweep — 500 bots, all classes, all three bands

Every character with `online=1 AND level<=80` (**500 bots**), swept with `.eratalents doctor` in
four batches of 125. Driver: `/tmp/claude-1000/tbc-mage/sweep.py` — the `wgconsole.py` PTY
mechanism with a **per-command completion check on `== doctor done ==`**, so a short pump cannot
silently truncate a report (the rogue record §12b driver). **500 headers, 500 `doctor done`, 0
missed.** Counting matches `ORPHAN ERA SPELL` and `<-- PROBLEM` literally, not the loose
`grep -icE` (the shaman doctor's own `AI resolve:` label text contains "STALE STOCK", which is
descriptive, not a finding).

| Class | Bots | TBC band | Vanilla band | WotLK band | ORPHAN ERA SPELL | PROBLEM / WRONG-ERA |
|---|---|---|---|---|---|---|
| Warrior | 50 | 4 | 20 | 26 | **0** | **0** |
| Paladin | 50 | 5 | 23 | 22 | **0** | **0** |
| Hunter | 50 | 4 | 27 | 19 | 11 | **0** |
| Rogue | 50 | 6 | 22 | 22 | **0** | **0** |
| Priest | 50 | 9 | 23 | 18 | **0** | **0** |
| Death Knight | 50 | 7 | 5 | 38 | **0** | **0** |
| Shaman | 50 | 4 | 25 | 21 | **0** | **0** |
| **Mage** | **50** | **11** | **30** | **9** | **8** | **0** |
| Warlock | 50 | 4 | 27 | 19 | **0** | **0** |
| Druid | 50 | 5 | 22 | 23 | **0** | **0** |
| **Total** | **500** | **59** | **224** | **217** | **19** | **0** |

**Every mage bot in the Vanilla band (30) and the TBC band (11) is 0 ORPHAN / 0 PROBLEM** — that
is this phase's own result, and it covers eleven independent TBC builds across all three tabs.
**Zero `<-- PROBLEM` lines anywhere in the whole 500** — including the new Shatter predicate and
the new stranded-920040-920044 predicate, both of which are evaluated for every era and class.

The 19 remaining `ORPHAN ERA SPELL` lines are **three L80 WotLK-band bots**, all pre-existing
historic residue and all previously recorded:

| Bot | Class | State | Orphan ids | vs the warlock record |
|---|---|---|---|---|
| `Rozki` | Mage | L80 `era=WotLK`, spent 0 | 932760, 920009, 920020, 920028, 920052, 920072, 920116, 920122 — **8** | was **9**; **920044 is GONE** — this phase's stranded-id strip, observed live |
| `Xonoth` | Hunter | L80 `era=WotLK`, spent 0 | 932967 + nine Vanilla hunter auto-passives — **10** | unchanged |
| `Brudugs` | Hunter | L80 `era=WotLK`, spent 0 | 922676 — **1** | unchanged |

`Patrea` (paladin), the fourth bot the warlock record flagged with 17 lines, is now **clean** — and
the reason is in this record's own evidence, not a mystery: `mod-player-bot-level-brackets` has
since moved it to **L24**, so it reads `era=Vanilla` with 15 points spent and a live Vanilla build.
Coming back inside a managed band ran the teardown-first `FactoryReconcile`, which stripped the
residue. That is the band-move invariant working exactly as designed, and it is also the clearest
statement of the remaining gap: **the residue only persists while a bot sits in the UNMANAGED
WotLK band**, where `StripOrphanedGrants` (scoped to `NodesFor(currentEra)`, empty for WotLK) can
never see it.

**Not a Phase 9 defect** — it is **Phase 9.5**'s job (the scheduled generic band-orphan sweep:
"strip any band spell that no era's spent nodes grant", for every character in every band).

---

## 12. Bot AI by-id check — zero hits, no patch-0021 work

The by-id grep over `Ai/Class/Mage/`, `Bot/` and `Mgr/` for every stock id this phase cloned or
renamed returned **ZERO hits** (Amendment E.3). The mage AI resolves by **name**, and every clone
keeps its stock name, so:

- **no `kEraNameAliases` row** is needed (contrast warrior's "Improved Battle Shout" and rogue's
  "Hack and Slash");
- **patch 0021 needs no mage arm** and was NOT regenerated.

The 13 `CLASS_MAGE` rows in the doctor's `kDiscriminators` (§11.2/§11.3) are therefore
**resolver-sanity rows, not patch-0021 sites** — the grep is the proof that nothing is broken today
if one of them reads 0. They remain the only headless way to see the by-name argument hold for the
nine clone chains.

Patch **0020**'s existing arms are unaffected: `EraTalentBots_SpecTabs` gates on *is this character
era-managed*, so a TBC-band mage bot is spec-read from `era_character_talent` like any other
(observed: eleven TBC-band mage bots built into all three tabs, and `EraTalentBots::EraFor` giving
`era=TBC` on every one).

---

## 13. Accepted gaps

Gap **1** is **RETIRED** (Ice Barrier's coefficient was found: 0.1) and gap **8** is **REMOVED**
(all three scope-flagged Vanilla clone chains shipped). Both ids are kept rather than renumbered so
nothing citing them goes stale.

| # | Gap | Source |
|---|---|---|
| 2 | WotLK's flat-mana → `ManaCostPct` conversion on GRANTed L65-70 ranks. **No remaining instance in the mage tree** — all four TBC chains are cloned and every clone authors the TBC flat cost. Kept as a standing note for GRANTs elsewhere and for the Vanilla side | spec A.6 |
| 3 | **Molten Fury's health window is 35% on this core, 20% in TBC.** The amount is era-correct (10/20%); the threshold is `AURA_STATE_HEALTHLESS_35_PERCENT` in core code with no data field. Strictly stronger than TBC. User decision: accept, no core patch → **CLOSED 2026-09-07, patch 0026** | spec A.4 item 2 / §6 answer (j) |
| 4 | TBC Molten Shields' second half ("Molten Armor affects ranged and spell attacks") is not in the DBC. On 3.3.5a the `spell_proc -30482` row already fires on ranged and spell attacks unconditionally, so the baseline already grants it — strictly ≥ TBC | spec A.6 |
| 5 | Ice Block carries WotLK's Hypothermia lockout (`ExcludeCasterAuraSpell 41425`). 45438 is BASELINE in both eras; fixing it would mean cloning a baseline spell for two eras. Applies identically to Vanilla (node 18046) | spec A.6 |
| 6 | **Lesson-17d residual:** a GRANTed stock spell's SPELLBOOK tooltip is the stock client row. The talent PANEL shows era prose (the hunter phase's TalentUI fix); the spellbook does not. Applies to all **20** GRANT nodes | spec §7 item 1 / A.6 |
| 7 | Live-only extra halves dropped from three AUTHORed nodes: Magic Attunement's +3/6 yd range, Improved Blink's -25/-50% Blink cost, Prismatic Cloak's -1/-2 s Invisibility fade. All WotLK additions — a TBC mage correctly does not get them | spec A.6 |
| 9 | **Arcane Power reaches Blast Wave.** The AP clone 948760 inherits live 12042's per-effect masks (word1 `0x19048`), whose bit 6 is Blast Wave's identity in both eras; TBC's own AP word1 was `0x8`. Re-minting three per-effect masks by hand was declined as not worth the risk | spec B.6 |
| 10 | **Combustion's proc row includes Blast Wave.** The clones' proc row copies live's fire set, whose word1 bit 6 is Blast Wave — an era Blast Wave feeds Combustion where wago's `0xc00017` would not (same shape as gap 9) | spec C.5 |
| 11 | `spell_mage_combustion_proc` (bound to the global 28682) removes **stock** 11129 when the crit-stack aura is cancelled; that one cleanup path does not reach an era clone. The 3-charge consume path is unaffected | spec C.5 |
| 12 | **Era Ice Barrier can overwrite a stronger active barrier with a weaker one.** The stock `spell_mage_ice_barrier` `CheckCast` (which refuses that) is NOT rebound, because it compares using its own hardcoded 0.8068 and would refuse legitimate recasts against a 0.1 clone. Harmless for the fixed-value Vanilla chain | spec D.2 |

---

## 14. REAL-CLIENT CHECKLIST (user, fresh `patch-V.mpq` at generation `1de3388d`)

Headless green is not sign-off. Nothing headless can render a talent frame, read a tooltip, or
measure a crit rate, and the *player* era path is untestable from the console (`.ip set` is UB
there — it discards its player argument and acts on target-or-self).

### 14a. TBC mage at level 70 — spec §6 verbatim

1. **Canary green** (the addon does not warn in red chat that `patch-V.mpq` is a stale generation).
2. **Three tabs with real backgrounds, 67 icons present** (a blank button = the `classic_` icon
   trap, lesson 5).
3. **Shatter crit vs a Frost Nova'd target at 5/5 is visibly above 0/5** — this is the behavioural
   proof of the patch-0018 band widen.
4. **Pyroblast cast bar at the TBC length** (6 s).
5. **The four chains are purchasable at the TBC levels and the stock ranks are not offered**
   (Pyroblast L24-70, Blast Wave L36-70, Dragon's Breath L56-70, Ice Barrier L46-70).
6. **Arcane Power reads +30%/+30%/3 min and Presence of Mind 3 min** — and the two are usable
   **together** (TBC allowed it; the clones author no cross-lock).
7. **Improved Scorch and Winter's Chill each stack to 5 with era text and do not share a debuff.**
8. **Magic Absorption restores mana on a full resist.**
9. **Water Elemental lasts 45 s.**
10. **Ice Block trainable at 30, unchanged.**
11. **Spellmod tooltips rewrite** (the talent's effect shows in the affected spell's tooltip).

### 14b. Added by Amendment D.6 — granted stock ACTIVES land in the General tab

12. **Icy Veins (12472) and Summon Water Elemental (31687) appear in the spellbook's General tab
    and cast.** They are GRANTed stock actives with no `extraSkillLines:` row, exactly like the
    real-client-verified Vanilla Cold Snap 11958 precedent.

### 14c. Vanilla mage at level 60 — one line per FIX row (§10)

13. **Pyroblast cast bar = 6 s**, and r2-r8 purchasable at L24-60 with the stock ranks not offered.
14. **Blast Wave does not knock back.**
15. **Combustion reads a 3-minute cooldown** and its 3-charge crit-stack consume still works.
16. **Improved Counterspell silences for 4 s.**
17. **Improved Scorch's Fire Vulnerability stacks to 5 (+3% each)** with era text.
18. **Winter's Chill stacks to 5 (+2% Frost crit taken)** with era text, on its own debuff slot.
19. **Magic Absorption restores mana on a full resist** (never wired before this generation).
20. **Improved Arcane Explosion / Incinerate / Improved Flamestrike / Critical Mass crit bonuses
    actually apply** (the four `pct → flat` rows).
21. **Ice Shards' crit-damage bonus actually applies** (`flat → pct`).
22. **Permafrost is spelled as one word.**
23. **Ice Barrier's absorb tooltip shows the base value (438 at r1) with spell power equipped**, and
    r2-r4 are purchasable at L46-58.

Findings come back as a fix round: fix → regen under a new stamp → re-check the same list → append
a fix-round section here (workflow lesson 17's standing classes apply on every fix).

---

## 15. Sign-off

**SIGNED OFF 2026-09-05 — zero findings — Phase 9 CLOSED.** The user ran the §14 real-client
checklist at generation `1de3388d` (fresh `patch-V.mpq`, L70 TBC mage plus the Vanilla L60 re-check
for all fourteen FIX rows) and reported no findings. No fix round. The 16 nodes no bot build
exercised (§16) are therefore covered by this pass. TBC merges as a set after Phase 9.5, the TBC
all-class review and the Vanilla regression pass.

---

## 16. Known open items (not this phase's defects)

- **The generic band-orphan sweep — Phase 9.5, already scheduled.** Three L80 WotLK-band bots
  (`Rozki`, `Xonoth`, `Brudugs`) still carry 19 stale band passives between them, because
  `StripOrphanedGrants` is scoped to `NodesFor(currentEra)` and that is empty for WotLK. This phase
  removed one of them (920044) with a hardcoded arm and demonstrated (via `Patrea`) that a move
  back into a managed band self-heals; the general fix is a design change, not a phase fix.
- **Bot-build node coverage is partial by construction** (the standing headless blind spot the
  rogue record named). The eleven TBC-band mage bots plus the ladder covered all three tabs and
  granted **51 of the 67 nodes**; no single build spends every node, so the remaining **16** rest
  entirely on the §14 checklist: **20800** Arcane Subtlety, **20803** Wand Specialization,
  **20804** Magic Absorption, **20806** Magic Attunement, **20809** Improved Mana Shield,
  **20812** Improved Blink, **20815** Prismatic Cloak, **20829** Improved Flamestrike,
  **20833** Molten Shields, **20838** Blazing Speed, **20845** Frost Warding, **20851** Permafrost,
  **20854** Improved Blizzard, **20855** Arctic Reach, **20860** Improved Cone of Cold,
  **20861** Ice Floes. Consequences worth naming: **Magic Absorption 948762-948766 was never
  granted to a bot** (§14 items 8 and 19 are its only verification), and neither ward-reflect
  marker (misc 2 via 20833, misc 3 via 20845) was armed on a bot.
  Helpers that are **triggered payloads rather than node grants** — 948767-948769 (Arcane Potency
  crit buffs), 948770 (silence), 948771-948772 (Blink buffs), 948860 (Fire Vulnerability),
  948960 (Winter's Chill), 948961-948963 (Blizzard chill) — never appear on a `grant` line by
  design; their absence from the sweep is not a coverage gap, but their *behaviour* is likewise a
  real-client item (§14 items 7, 17, 18).
  Exercised by bots: the four TBC chains (948870 / 948880 / 948887 / 948890 on three bots each,
  948964 on three) and the Arcane clones **948760** / **948761** / **948773** (two bots each).
- **Accepted gaps 3, 9, 10, 12** are live behavioural deltas vs TBC that the user has accepted;
  they are not bugs to report from the real-client pass.
