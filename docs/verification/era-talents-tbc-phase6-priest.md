# Era Talents — TBC Phase 6 (Priest) verification record

> **Generation superseded (2026-09-06 final review):** the current stamp is the closing row of docs/verification/era-talents-final-review-tbc-vanilla.md §10 — reinstall patch-V.mpq + restage the addon before any real-client pass.

**Status: HEADLESS-COMPLETE, AWAITING REAL-CLIENT SIGN-OFF (generation `9ad9274e`).** All **64
nodes** authored and wired, `era_audit.py` clean (0 findings), **218/218** host tests green,
`gen_bot_builds.py` clean with three `OK TBC PRIEST` orders, the four-rung band-move doctor ladder
zero-orphan on a live bot, and a **275-bot** broad doctor sweep across all ten classes and both
managed bands with zero `ORPHAN` / `PROBLEM` / `known=no` / `spellInfo=no` rows. Branch
`feat/era-talents`, **merged to master 2026-09-07** (TBC merges as a set after all nine classes + final review + a
Vanilla regression pass). The `EraTalentIP.cpp` `ERA_TBC` allowlist flip remains an **uncommitted
dev-box edit** — verified ` M` and unstaged at close-out; no Phase-6 commit touches the file (the
last two commits to it are `312d69b` / `0fbd788`, both pre-phase).

**Zero new core patches this phase.** No `patches/*.patch` was added or regenerated.

**Spec/plan:**
`docs/superpowers/specs/2026-09-03-era-talents-tbc-phase6-priest-content-design.md`
(**Amendment A supersedes much of the body** — A.8 in particular decides the Mind Flay identity,
which in turn resolves the conditional dispositions the `_ref` recorded for nodes 20552/20557) +
`docs/superpowers/plans/2026-09-03-era-talents-tbc-phase6-priest.md`.
**Findings authority:** `era-data/_ref/tbc/priest-spells.yaml` (4349 lines — `families_compared:` /
`spells:` / `family6_bit_census:` / `survival_map:` / `triggers:` / `proc_data_census:` /
`script_census:` / `capstone_chains:` / `core_hardcodes:` / `reuse_ledger:` / `baseline_leaks:` /
`node_findings:` / `accepted_gaps:` / `open_questions:` / `spellfix_inheritance:`) — this record
summarizes; the `_ref` is normative. The dataset embeds a byte-equal copy of it under `_ref:`
(re-synced at Task 9; the re-sync was proved docs-only by regenerating `--custom-sql` and diffing
byte-for-byte against the committed `_01` SQL **before** the regen).

**Phase commits:** `3d2a90a` plan · `97d1048` T1 (slice reservation + guard test) · `8fa4d77` T3
(the ref) · `1ca5f55` Amendment A · `6de8761` A-deltas · `9476ee0` T3 review round · `5dbd499` T4
(wiring) · `3481936` T5 (substrate) · `019e4cf` T5 review round · `bd12f27` T6 (Discipline) ·
`637a151` T6 review round · `232f52e` T7 (Holy) · `bedb228` post-T7 corrections · `522598d` T8
(Shadow) · `8a4f9c5` + `1558d50` plan VE-resolve corrections · *this commit* T9.

---

## 1. What shipped

The complete **TBC 2.4.3 Priest talent tree** — **64 nodes** (Discipline 22 / Holy 21 / Shadow 21,
ids **20500–20563**, era 1) — plus six trainer-taught clone chains, three module scripts and four
new generator keys. Value basis: **wago 2.5.4** end-to-end (the standing TBC basis).

- **Class 5, spell family 6.** Family guard clean: **0 rows** in either band with `SpellClassSet`
  outside `{0, 6}`.
- **Auto-passives:** **131 rows in `940000–940511`** (`936000 + (nodeId−20000)*8 + (rank−1)`). No
  overlap with paladin 936000–936511 / druid 936800–937295 / shaman 937600–938087 / warrior
  938400–938927 / rogue 939200–939735.
- **Hand helpers: 57 ids, `948024–948080`** (§6). **Free within the priest slice: `948081–948279`.**
  Next class claims from **948280**.
- **NO DUMMY-marker misc consumed.** Improved Vampiric Embrace (20556) re-uses the existing misc
  **4** that `era_vampiric_embrace` already scans for; the VT and Reflective Shield scripts key on
  clone OWNERSHIP. Registry next-free stays **19**.
- **SQL (five files, `2026_09_05_00` … `_04`):** `_00_..._data.sql` + `_01_..._custom_spells.sql`
  (generated); `_02_..._divine_spirit_chain.sql`, `_03_..._holy_chains.sql`,
  `_04_..._shadow_chains.sql` (hand — 21 `trainer_spell` rows on TrainerId 11 + 7 `spell_ranks`
  chains (6 trainer-taught + the Misery debuff chain 948073-948077, which exists only so
  `SpellInfo::IsRankOf` prevents cross-rank stacking)). **Nothing deleted anywhere, nothing re-gated** — every stock chain this phase's clones
  replace is already self-gating, so the "deleting a stock trainer row orphans higher ranks" hazard
  never arose. Stock DS/PoS/CoH/VT/Lightwell/Mind Flay WotLK ranks (48073/48074/48088/48089/
  48159/48160/48077/48078) stay reachable for native priests.
- **C++:** `EraTalents.cpp` (two gate-walker skips + the `CLASS_PRIEST` TBC reconcile block, six
  readiness-gated arms — §8; and the `EraNodeIsWired` `EraHasTalentTrees` conjunct),
  `EraTalentProcScripts.cpp` (`era_vampiric_embrace` made table-driven so it serves both eras; new
  `era_pri_vampiric_touch_tbc`; new `era_pri_reflective_shield_tbc`),
  `EraTalentsCommand.cpp` (TBC priest doctor orphan predicates over 948024–948080 plus the
  stale-DS readiness mirror), `EraTalentBots.cpp` (`kAuthoredOrders` — three generated TBC priest
  orders), `tools/gen_bot_builds.py` (`BUILDS`).
- **Generator:** four purely-additive keys, TDD-covered and byte-neutral for already-shipped data —
  `attributesEx7` (Silence's `SpellInfoCorrections` bit), `defenseType` (the from-scratch Blackout
  stun's resistability), and the `startRecoveryCategory` / `startRecoveryTime` GCD pair (Silence).
  `era_audit.py`'s `_SPELLFIX_KEYABLE` gained the matching `AttributesEx7` row in the same commit.

## 2. Per-tab node counts and disposition tallies

All 64 wired, 0 display-only (live DB: `wired` = 64). Two views — the **shipped mechanic** (from
`era-data/tbc/priest.yaml` at generation `9ad9274e`) and the **`_ref` verdict** (`node_findings:`).

| Tab | Nodes | ids | grant | spellmod | stat | multi | proc | proc-scripted |
|---|---|---|---|---|---|---|---|---|
| Discipline | 22 | 20500–20521 | 8 | 3 | 3 | 6 | 2 | — |
| Holy | 21 | 20522–20542 | 13 | — | 1 | 2 | 4 | 1 |
| Shadow | 21 | 20543–20563 | 7 | 7 | 2 | 3 | 2 | — |
| **Total** | **64** | | **28** | **10** | **6** | **11** | **8** | **1** |

The 28 `mechanic: grant` nodes split **17 stock grants + 11 band grants**:

- **17 nodes grant STOCK** (TBC == live on all fourteen families, or on every reachable rank):
  **20504** Improved PW:Shield · **20506** Absolution · **20507** Inner Focus · **20511** Improved
  Mana Burn · **20522** Healing Focus · **20523** Improved Renew · **20524** Holy Specialization ·
  **20525** Spell Warding · **20526** Divine Fury · **20527** Holy Nova · **20530** Holy Reach ·
  **20531** Improved Healing · **20533** Healing Prayers · **20534** Spirit of Redemption ·
  **20535** Spiritual Guidance · **20548** Improved Psychic Scream · **20557** Focused Mind.
- **11 nodes grant a BAND id** (a clone or a chain root): **20513** Divine Spirit (948024) ·
  **20518** Power Infusion (948039) · **20519** Reflective Shield (948034–948038, one per rank) ·
  **20521** Pain Suppression (948040) · **20539** Lightwell (948051) · **20542** Circle of Healing
  (948046) · **20550** Mind Flay (948064) · **20554** Silence (948071) · **20555** Vampiric Embrace
  (948031) · **20560** Shadowform (948072) · **20563** Vampiric Touch (948060).
- The remaining **36** nodes are generated passives (`spellmod` 10 / `stat` 6 / `multi` 11 /
  `proc` 8 / `proc-scripted` 1), delivered by the 940000-band auto-passives.

**Two `_ref` node dispositions were CONDITIONALS, resolved in opposite directions by Amendment
A.8** (the Mind Flay identity decision), and a reader diffing the `_ref` against the dataset must
not read them as drift:

| Node | `_ref` disposition | Shipped | Why |
|---|---|---|---|
| 20552 Shadow Reach | "GRANT stock IF the era Mind Flay clone carries live's word2 0x40; CLONE otherwise" | **AUTHORED** (spellmod) | A.8 gives the era Mind Flay clone word2 `0x40` **plus** the TBC mask needs the Silence word0-bit26 re-point onto the era Silence clone's word1 `0x200000`, which a stock grant cannot carry |
| 20557 Focused Mind | "LIVE_MATCH but MASK-DEPENDENT — clone recommended" | **GRANT STOCK** (33213/33214/33215) | With the Mind Flay clone on live's word2 identity, LIVE's own mask is the one that reaches it |

## 3. Fourteen-family survival-map verdict distribution

`survival_map:` covers **325 ids** (the 64 nodes' rank spells plus the transitive triggered-chain
closure, the six trained chains, the twelve stock PW:Shield ranks and the WotLK VE/VT/Misery
payload seeds).

| Verdict | Count |
|---|---|
| `DIVERGED` | **171** |
| `LIVE_MATCH` | 83 |
| `WAGO_ONLY` | 30 |
| `LIVE_ONLY` | 24 |
| `LIVE_MATCH_UNVERIFIED_SLOT` | 15 |
| `OVERLAY_ONLY` | 2 |
| **Total** | **325** |

Per-NODE verdicts (64 rows in `node_findings:`) are dominated by `DIVERGED` (24 outright, plus 15
compound verdicts that diverge on some ranks) against 15 clean `LIVE_MATCH`. The four fixed points
the brainstorm predicted all landed as predicted: **14752 DIVERGED** (live lost effects 2/3),
**33174/33182 DIVERGED-or-inert** (their `SPELLMOD_EFFECT2/3` have no live target),
**33194/33195/33199/33200 WAGO_ONLY**, **33196–33198 DIVERGED** (WotLK's +hit repurposing).

Sweep-shape notes carried forward from earlier phases and re-confirmed here: the fourteen-family
column list (A effects · B proc options · C misc + all eight attribute words · D categories incl.
`PreventionType` · E power · F class-options word 0 · G cooldowns · H aura restrictions ·
I shapeshift · J levels · K equipped item · L interrupts · M targets · N identity/name) plus the
supplementary O per-effect class-mask pass; `SpellClassMask_1` is the live column name; the four
float columns are unpacked as floats.

## 4. The Mind Flay identity decision (Amendment A.8) and its per-talent outcomes

TBC 15407's own identity is word0 `0x00800000` — a **shared** bit carried by 126 family-6 spells —
and word2 0. LIVE 15407's identity is word2 `0x40` (unique to the Mind Flay chain) plus the shared
word2 `0x400`. **A.8 keeps LIVE's identity** for the era clone chain 948064–948070: a clone carrying
TBC's shared word0 bit would be caught by every spellmod aimed at any of those 126 spells.

Consequence — every talent whose TBC mask reaches Mind Flay through the shared word0 bit 23 must be
**re-pointed onto word2 `0x40`**, and A.8's grant-or-author rule then decides grant vs. author per
node:

| Node | TBC mask reaches MF via | Shipped |
|---|---|---|
| 20547 Shadow Focus | word0 `0x822000` | AUTHORED (spellmod) — re-pointed, plus the Silence word1 `0x200000` re-point |
| 20552 Shadow Reach | word0 `0x680a004` | AUTHORED (spellmod) — same two re-points |
| 20553 Shadow Weaving | word0 `0x288a010` | AUTHORED — word2 `0x40` added; **no** Silence re-point (TBC's own mask has no bit26, correct for a damage proc) |
| 20559 Darkness | e2 `0x2808000` | AUTHORED (multi, three spellmod effects) — e2's bit 23 re-pointed to word2 `0x40`; **no** Silence re-point (none of e1/e2/e3 carries bit26) |
| 20562 Misery | proc mask `0x908000` | AUTHORED as **aura 109** `SPELL_AURA_ADD_TARGET_TRIGGER` with the `-33191` proc row's contract transcribed onto the era identities (§7) |
| 20557 Focused Mind | word0 `0x22000` | **GRANT STOCK** — LIVE's mask already reaches the clone |

## 5. Reuse ledger — outcome

**Eight Vanilla priest helper groups examined: ALL EIGHT FRESH.** That continues the run — every
prior TBC class also ended all-FRESH/MOOT.

| Vanilla helper | Verdict | Deciding evidence |
|---|---|---|
| 921152 VE castable clone | **FRESH** (user-locked) | Vanilla ManaCost 40 flat / `ProcTypeMask 0x50000`; TBC's trio differs and is 15% not 20% |
| 932950 VE proc-passive | **FRESH** (user-locked) | basePoints **20** (1.12.1) vs TBC's **15**; `ProcTypeMask` 327680 vs `0x80050000` |
| 932951 VE heal delivery | **FRESH** (user-locked) | different school/attribute/target encoding from TBC 15290 |
| 920920 Power Infusion | **FRESH** | Vanilla clone's mana + duration differ; TBC is cooldown-only (A.2) |
| 932960 Shadow Vulnerability | **FRESH** | Vanilla 3%/stack encoding vs TBC 15258 |
| 932970 Blackout stun | **FRESH** | `DefenseType 0` on the Vanilla clone (see gap 12); TBC needs a resistable stun |
| 920987–920989 Inspiration | **FRESH** | Vanilla's deliberate "armor proxy" (aura 87, school 1) vs TBC's aura 101 armor (A.6) |
| 920976–920978 Blessed Recovery | **FRESH — the one near-miss** | magnitudes AGREE (8/16/25); the trigger/HoT wiring does not |

**Four STOCK payloads are reused as-is** (LIVE_MATCH on all fourteen families, so no clone is
warranted): **33143** Blessed Resilience buff · **27813/27817/27818** Blessed Recovery HoTs ·
**33619** Reflective Shield triggered damage (the era script casts it with a custom basepoint,
exactly as the core script does) · **34919** Vampiric Touch energize payload.

## 6. Fresh-helper id table — `948024–948080` (57 ids)

| Ids | What | Node | Why not a stock grant |
|---|---|---|---|
| **948024–948028** | Divine Spirit r1–r5 | 20513 | Live 14752 has ONE effect (aura 29); TBC has THREE (aura 29 Spirit + aura 174 `MOD_SPELL_DAMAGE_OF_STAT_PERCENT` + aura 175 `MOD_SPELL_HEALING_OF_STAT_PERCENT`, both at base 0) — the two hidden slots Improved DS spellmods via `SPELLMOD_EFFECT2/3`. A stock grant can never carry Improved DS. Clones carry word0 `0x20` (A.13) so `SpellInfo.cpp:2141`'s `SPELL_SPECIFIC_PRIEST_DIVINE_SPIRIT` group exclusivity still applies |
| **948029–948030** | Prayer of Spirit r1–r2 | 20513 | Same three-effect shape; `attributesEx2 |= 0x4` authored (A.14 — the `SpellInfoCorrections` LOS fix by-id never reaches a clone) |
| **948031–948033** | Vampiric Embrace trio (castable / hidden proc-passive / heal delivery) | 20555 | TBC is **15%** where the shipped Vanilla trio is 20%, and the proc contract differs. The passive **948032** is a PERMANENT learned passive (a `TRIGGER_SPELL` temp buff did not proc) managed by reconcile arm (4) |
| **948034–948038** | Reflective Shield r1–r5 | 20519 | The core reads the talent by `GetAuraEffectOfRankedSpell(33201, EFFECT_0)`; a clone breaks that lookup, so the module script supplies the reflect instead (§7) |
| **948039** | Power Infusion | 20518 | Cooldown-only divergence (A.2) — 3 min vs live's 2 min |
| **948040** | Pain Suppression | 20521 | TBC's 2-min cooldown vs live's 3-min |
| **948041–948042** | Martyrdom "Focused Casting" payloads r1–r2 | 20505 | aura 117 (A.6) |
| **948043–948045** | Focused Will buffs r1–r3 | 20517 | TBC values; live's are the WotLK retune |
| **948046–948050** | Circle of Healing r1–r5 | 20542 | TBC heals the **target's party only** and has **no cooldown**; live is raid-wide with a cooldown, and r5 converts flat mana to `ManaCostPct` |
| **948051–948054** | Lightwell r1–r4 | 20539 | TBC's 6-min **category** cooldown + flat mana; A.3 keeps LIVE's Effect-28 CREATURE summon so the click→heal chain survives (cost recorded as gap 9) |
| **948055** | Surge of Light buff | 20536 | ships with **NO ICD** (A.16 / gap 5 — the live 6 s ICD exists only in the `-33150` proc row and no TBC column carries it) |
| **948056** | Clearcasting buff (Holy Concentration) | 20538 | stock 34754 is repurposed live; a fresh buff is needed to carry `ProcCharges 1` |
| **948057–948059** | Inspiration buffs r1–r3 | 20529 | TBC is **aura 101 armor** (A.6); the Vanilla clones are the deliberate school-1 "armor proxy" |
| **948060–948063** | Vampiric Touch r1–r3 + energize carrier | 20563 | TBC returns **mana to the party**; live's VT is the WotLK dispel-backlash version. `era_pri_vampiric_touch_tbc` bound to all three ranks |
| **948064–948070** | Mind Flay r1–r7 | 20550 | TBC's aura-3 periodic encoding + the A.8 identity decision (word2 `0x40`) |
| **948071** | Silence | 20554 | `attributesEx7: 0x800` (`SPELL_ATTR7_CAN_CAUSE_INTERRUPT`, a `SpellInfoCorrections` by-id fix a clone never inherits) + the `startRecoveryCategory`/`startRecoveryTime` GCD pair |
| **948072** | Shadowform | 20560 | TBC's `attributes` / `attributesEx2` (physical-only damage reduction) |
| **948073–948077** | Misery debuffs r1–r5 | 20562 | 33196–33198 are repurposed live (+hit) and 33199/33200 absent; all five ship as ONE `spell_ranks` chain rooted at 948073 so `Aura::CanStackWith` → `SpellInfo::IsRankOf` refuses to stack two priests' different ranks |
| **948078** | Shadow Vulnerability | 20553 | TBC 15258's values; triggered by the node's aura-109 effect |
| **948079** | Blackout stun | 20544 | authored from scratch (no live carrier), with `defenseType` authored so `Unit::SpellHitResult` can resist it |
| **948080** | Spirit Tap buff | 20543 | fresh ×5 + buff (A.10) |

**948081–948279 remain free within the priest slice.** Live DB confirms exactly **57** rows in
`948024–948080` and **zero** rows in `948081–949999`.

## 7. Script census, rebinds and deliberate no-rebinds

`script_census:` recorded **46 rows** (12 core priest/generic scripts on ids this tree touches,
2 module scripts, 1 IP module row covering 34 ids).

**Bound this phase (live DB, `spell_script_names`):**

| Script | Bound to | Rows |
|---|---|---|
| `era_pri_reflective_shield_tbc` | the twelve **STOCK** PW:Shield ranks 17 / 592 / 600 / 3747 / 6065 / 6066 / 10898 / 10899 / 10900 / 10901 / 25217 / 25218 | 12 |
| `era_pri_vampiric_touch_tbc` | 948060 / 948061 / 948062 | 3 |
| `era_vampiric_embrace` (made table-driven) | 932950 (Vanilla) **+ 948032** (TBC) | 2 |
| `era_blessed_recovery` | 940224 / 940225 / 940226 (TBC per-rank DUMMY carriers) — alongside the pre-existing Vanilla 920976–920978 | 3 new (6 total) |

The Reflective Shield binding is the notable one: the reflect lives in the core's
`spell_pri_power_word_shield_aura::ReflectDamage`, which resolves the talent by
`GetAuraEffectOfRankedSpell(33201, EFFECT_0)` — a lookup an era clone cannot satisfy. The era script
is therefore **added to the stock PW:Shield ranks** (the generator's stock-id rebind semantics emit
an `(id, script)`-qualified DELETE, so the core's own binding is undisturbed) and reads the caster's
era clone ownership instead. It reads `GetCaster()`, so a shield cast on a **party member** reflects
too.

**Deliberate NO-rebinds, each with its reason:**

- **`spell_pri_circle_of_healing`** — a type-31 (`SPELL_EFFECT_HEAL`-target-cap) hook against
  clones that ship as type-37; rebinding it would re-impose the live raid-wide target rule the TBC
  clone exists to remove.
- **`spell_pri_vampiric_touch`** — its body is the **WotLK dispel backlash** (casts 64085 when VT is
  dispelled). TBC has no backlash, so its absence on the clones is era-correct.
- **`spell_gen_proc_on_victim`** (bound to `-33191`) — the `_ref` census pre-registered a REBIND
  disposition, and **A.7 superseded it**: Misery ships as **aura 109
  `SPELL_AURA_ADD_TARGET_TRIGGER`**, which `Spell::DoTriggersOnSpellHit` casts on the spell's own
  target, so the clones need neither a `spell_proc` row nor the generic victim-router. (The `_ref`
  line is left as-shipped so the sweep's reasoning stays auditable; the node 20562 comment in
  `era-data/tbc/priest.yaml` records the override.)
- **`spell_pri_blessed_recovery`** — **replaced**, not rebound. The core script computes the HoT id
  by rank arithmetic from `SPELL_PRIEST_BLESSED_RECOVERY_R1 = 27813`; the module's
  `era_blessed_recovery` carries the same mechanism on the node's own DUMMY carriers.
- **`spell_pri_lightwell` / `_renew`** — the era Lightwell keeps LIVE's creature and its stock Renew
  chain (A.3), so the core scripts already apply unchanged.
- **`spell_pri_improved_spirit_tap` / `_shadow_word_death` / `_mana_burn` / `_mind_control` /
  `_renew`** — baseline or WotLK-talent-owned; untouched.

## 8. Gate-row splits and reconcile arms

**Two of the seven `kBaselineSpellGates` rows were split three-way this phase** (the third and
fourth overall, after druid's Faerie Fire (Feral) and warrior's Shield Slam):

| Gate row | Split shape | Why |
|---|---|---|
| **Holy Nova 18121** | **node-gated strip** (druid shape) | Holy Nova is still a TALENT in TBC (node 20527), which grants **STOCK 15237**. The walker skips the WotLK force-grant for a TBC priest; arm (1) strips the stock ranks when 20527 is unspent |
| **Divine Spirit + Prayer of Spirit 18113** (both rows) | **UNCONDITIONAL strip** (warrior Shield Slam shape) | Node 20513 grants the CLONE chain, never the stock ranks — every stock rank lacks the two hidden effects — so a 20513-SPENT priest must not hold stock ranks either. The walker skip is gated on `EraNodeReplacesStock(ERA_TBC, CLASS_PRIEST, 20513, 14752)` (commit `019e4cf`), and the strip reads the rank list straight off **both** 18113 gate rows so the lists cannot drift. `EraNodeReplacesStock`, not the earlier `EraReplacementSpellReady`/`EraNodeIsWired` pair, because a half-authored state where 948024 exists but the node still grants stock 14752 must not skip the force-grant with nothing to replace it |

**`CLASS_PRIEST` TBC reconcile block — six readiness-gated arms** (`EraTalents.cpp` ~:2060–2170):

1. Holy Nova node-gated stock strip (15237 + the trained ranks + the WotLK 48077/48078 safety-net).
2. Divine Spirit / Prayer of Spirit unconditional stock strip, driven off `kBaselineSpellGates`.
3. **(2b)** the TBC **clone** DS/PoS chain strip (`kEraPriDsPosTbc`) when node 20513 is unspent —
   the respec path. The Divine Spirit half is already covered twice (`Player::removeSpell` cascades
   948024 → 948028 through `spell_ranks`, and `StripOrphanedGrants` sees the node grant), but
   **Prayer of Spirit roots at its own chain 948029 → 948030 and is TRAINER-taught, never
   node-granted**, so nothing removed it: the stock `spell_required(27681 → 14752)` path is dead for
   a clone because `Player.cpp` only walks `spell_required` when `GetTalentSpellCost(firstRank) > 0`
   and `DBCStores` returns 0 for any id outside `Talent.dbc` (workflow **lesson 21**). A matching
   **cross-era** leftover strip sits outside the TBC block (`era != ERA_TBC`) for the same reason —
   `StripOrphanedGrants` would not catch the PoS ranks on a band move either.
4. **(3)** grant-change stock swaps (`kEraPriTbcStockSwaps`) — each row readiness-gated by
   `EraNodeReplacesStock`, so the arm is dormant while a node is a seed or grants stock.
5. **(3b)** stock TRAINED ranks behind a swapped r1 — the login safety-net (rogue Mutilate shape),
   `kEraPriTbcStockChains`, all **FIVE** rows: Circle of Healing r2–r5 (34863/34864/34865/34866),
   Vampiric Touch r2–r3 (34916/34917), Reflective Shield r2–r5 (33202/33203/33204/33205), Mind Flay
   r2–r7 (17311/17312/17313/17314/18807/25387), and Lightwell r2–r4 (27870/27871/28275). The
   Reflective Shield row is the **only** strip path for 33202 — it is a WotLK **talent** rank, so the
   stock `spell_ranks` cascade from 33201 stops there and nobody trains it.
6. **(4)** the TBC VE hidden proc-passive **948032** — learned iff node 20555 is spent, and the
   VANILLA proc-passive 932950 stripped defensively (a TBC priest can never legitimately hold it,
   and the Vanilla arm only runs for `era < ERA_TBC`).

**`EraNodeIsWired` gained an `EraHasTalentTrees(era)` conjunct** (commit `019e4cf`), matching
`EraReplacementSpellReady` / `EraNodeReplacesStock`. This is a **cross-class** change — the druid
and paladin arms keyed on the same predicate inherit it — which is why §12's broad sweep covers
every class in both bands. With the era outside the implemented-era allowlist a character is on the
NATIVE talent frame and no era node can grant anything back, so every arm keyed on the predicate
must stay inert; without the conjunct, a committed Vanilla-only allowlist would strip stock spells
from a TBC player with nothing to replace them.

**Doctor:** TBC priest orphan predicates added over the whole helper range 948024–948080, each
keyed on `era == ERA_TBC && class == CLASS_PRIEST` plus the owning node's rank, with the
trainer-taught chains treated as "r1 node-granted, r2+ trainer-taught" (the framework's standing
shape); plus a **readiness** mirror of the two 18113 gate rows (`sSpellMgr->GetSpellInfo(948024)`,
not `HasSpell`) so a stale stock Divine Spirit is reported before the chain exists.

## 9. Accepted gaps

| # | Subject | Closes if |
|---|---|---|
| 1 | Holy Nova r7 (25331) mana — TBC's flat 875 vs live's `ManaCostPct 25`; node 20527 grants stock, so a L68+ era priest pays the WotLK percentage (~2× at 70) | node 20527 becomes a clone chain |
| 2 | The **baseline** PW:Shield chain (17…25218) diverges on every rank (`ImplicitTargetA`, the `WEAKENED_SOUL` re-encoding, r12's mana) | never, within the era model — baseline spells are global |
| 3 | Mass Dispel's WotLK-added third dispel trigger (72734) | never — baseline |
| 4 | Devouring Plague 2944's added ATTR5/ATTR6 bits | never — deliberate; IP-legal, CLAUDE.md forbids touching it |
| 5 | Surge of Light internal cooldown — live's `-33150` row carries a 6 s ICD; **no TBC column carries one**, so the era proc ships with none | a TBC-behaviour source (e.g. wowsims/tbc `sim/priest`) settles it |
| 6 | Pain Suppression's 5% threat reduction — promised by the TBC tooltip, present in **neither** side's spell data | a core `spell_pri_pain_suppression` implementation |
| 7 | Misery's aura-112 coefficient half (`OVERRIDE_CLASS_SCRIPTS` misc **5066**, no handler in this core) | a core patch implementing class script 5066 — judged not worth it: the TBC tooltip promises only the +N% spell damage taken |
| 8 | Wand Specialization r3–r5 read LIVE_MATCH inside a chain whose r1–r2 diverge (the live chain is non-monotone) | cloning all five ranks (the shipped disposition) |
| 9 | **Lightwell charge count and cancel rule** — A.3 keeps LIVE's creature summon so the click→heal chain survives, which necessarily keeps the two WotLK-side properties that live on the CREATURE (charges, cancel rule) | reproducing TBC's GAMEOBJECT summon instead — a much larger change |
| 10 | Four era-diverged attribute bits ride through on templated Shadow clones because the generator has no `attributesEx4` / `attributesEx6` key | adding those two keys; each bit was individually MEASURED INERT on this core first |
| 11 | Blackout's wago `ProcTypeMask` omits `DONE_PERIODIC` — a **deliberate** encoding departure so the stun can proc off a Mind Flay tick (which the tooltip promises) | n/a — deliberate, recorded so it is not "fixed" |
| 12 | Blackout 932970 DefenseType + Spirit Tap 18132 attrMask (CLOSED 2026-09-06, M-69 + I-22) — two VANILLA-era divergences this phase found and deliberately did NOT retro-fix: the Vanilla Blackout stun 932970 had `DefenseType 0` (so `Unit::SpellHitResult` never resisted it), and node 18132 Spirit Tap was missing `attrMask: 1` | **CLOSED 2026-09-06** by the priest final-review round (M-69 + I-22): 932970 now authors `defenseType: 1` + the missing `client.description`/`auraDescription`, and node 18132 Spirit Tap now authors `attrMask: 1` |
| 13 | **Inner Focus 14751 (node 20507) carries two WotLK-only attribute bits** — `AttributesEx3` `0x04000000` `CAN_PROC_FROM_PROCS` and `AttributesEx4` `0x00080000` `ALLOW_PROC_WHILE_SITTING`, neither of which TBC 2.5.4 has (dumped from `ip-dbc/Spell.dbc` 2026-09-06). Both merely widen when the buff may be consumed. **KEEP** — same disposition as the Vanilla twin 18107, and as every other inherited-attribute divergence (gap 10) | the generator gains an `attributesEx4` key AND node 20507/18107 is re-shaped from a stock grant into a clone authoring `attributesEx3: 0` / `attributesEx4: 64` |

## 10. Headless sweep (generation `9ad9274e`)

| Check | Result |
|---|---|
| `tools/era-regen.sh --sync-fork` | new stamp **`9ad9274e`**, +865 custom client spell rows merged into `patch-V.mpq`, module synced into the fork |
| `tools/era_audit.py` | **0 findings** |
| `pytest tools/ -q` | **218 passed** (93 s) |
| `tools/gen_bot_builds.py` | clean; three `OK TBC PRIEST` orders (tab0 61 pts / tab1 61 pts / tab2 61 pts); the emitted `kAuthoredOrders` rows are **byte-identical** to the three committed in `EraTalentBots.cpp` (no drift) |
| `docker compose build ac-worldserver` | exit 0 |
| readiness | `ac-worldserver` Up **and** `era_talent_meta` = `9ad9274e` |
| `era_talent` wired rows, era 1 class 5 | **64** |
| family guard (`SpellClassSet NOT IN (0,6)` over 940000–940511 ∪ 948024–948279) | **no rows** |
| outside-slice ids (948081–949999) | **no rows** |
| helper rows 948024–948080 | **57** |
| auto-passive rows 940000–940511 | **131** |
| `era_talent` group-by | all nine era-0 rows unchanged; era-1 `1 1 66` / `1 2 64` / `1 4 67` / `1 7 61` / `1 11 62` unchanged **plus the new `1 5 64`** |
| `spell_script_names` | 12 + 3 + 2 + 3 rows as §7 |
| `trainer_spell` rows in 948024–948279 | **21** (DS 4 + PoS 2 + CoH 4 + Lightwell 3 + VT 2 + Mind Flay 6), all TrainerId **11** |
| `spell_ranks` chains rooted in the slice | **7** — 948024 (5) · 948029 (2) · 948046 (5) · 948051 (4) · 948060 (3) · 948064 (7) · **948073 (5)** |

**Two expectation corrections worth recording** (both are correct-as-shipped, not defects):

1. The plan's Step-1 expectation listed **six** `spell_ranks` chains. There are **seven**: the
   Misery debuff chain **948073–948077** is deliberate — it is what makes
   `Aura::CanStackWith` → `SpellInfo::IsRankOf` refuse to stack two priests' different Misery ranks
   (the "no cross-rank stacking" real-client item in §15).
2. `era_blessed_recovery` shows **six** bindings, not three: the three new TBC carriers
   940224–940226 **plus** the three pre-existing Vanilla clones 920976–920978.

`git status` after the regen showed only `2026_08_14_11_era_talent_meta.sql` changed among the
generated artifacts — confirming the Task-9 `_ref` re-sync and the two comment edits are
**docs-only** and moved no shipped data.

## 11. Bot AI by-id check

Grep over `Ai/Class/Priest/`, `Bot/` and `Mgr/` for every stock id this phase cloned (10060, 15286,
14752, 27681, 33206, 34914/34916/34917, 34861/34863–34866, 33201, 33202–33205, 33191/33192–33195,
15407, 17311–17314, 18807, 25387, 724, 27870/27871, 28275, 15487, 15473) returned **three real
hits and one false positive**:

| Site | Verdict |
|---|---|
| `Ai/Class/Priest/PriestActions.h:152` (`CastVampiricEmbraceAction::GetTargetName`) | **already routed** through `EraTalentBots_ResolveSpellId(bot, 15286)` (patch 0021) |
| `Ai/Class/Priest/PriestTriggers.h:31` | **already routed**, same bridge |
| `Ai/Class/Priest/PriestActions.h:144` | a comment, not code |
| `Bot/PlayerbotAI.cpp:1774` `case 724:` | **FALSE POSITIVE** — 724 there is the Ruby Sanctum **map id**, not the Lightwell spell |

**No unrouted by-id site. No fork-patch change needed this phase.**

**Everything else resolves by NAME and is era-safe.** The priest AI's spell references are all
lowercase name strings (`"circle of healing"`, `"divine spirit"`, `"prayer of spirit"`,
`"lightwell"`, `"mind flay"`, `"pain suppression"`, `"power infusion"`, `"shadowform"`, `"silence"`,
`"vampiric embrace"`, `"vampiric touch"`, `"holy nova"`), and **every era clone keeps its stock
display name** — so **no `kEraNameAliases` entry is needed for priest**.

**The one subtlety, checked rather than assumed:** a TBC Shadow priest knows TWO spells named
"Vampiric Embrace" — the castable **948031** and the hidden proc-passive **948032**. That does not
break the cast path: `SpellIdValue::Calculate` skips passives
(`if (!spellInfo || spellInfo->IsPassive()) continue;`,
`Ai/Base/Value/SpellIdValue.cpp:55`), so the name resolver can only land on 948031.
`EraTalentBots_ResolveSpellId` — used only for **target selection** — returns the highest-id
same-name match, i.e. **948032**, and the AI only tests `!= 15286`, so it correctly picks
`"current target"` (TBC VE is an enemy debuff, unlike stock WotLK 15286's self-buff).

**Recorded doctor line, Shadow-built priest bot `Jaellylah` at L70 (era TBC):**

```
node 20555 rank 1: grant 948031 known=yes spellInfo=yes
band DUMMY marker: spell 948032 misc 0 amount 15
band aura on player: 948032 amount0 15 ... applied=yes
AI resolve: 15286 (Vampiric Embrace) -> 948032
```

(For contrast, the same bot at L60 — Vanilla band — reads `AI resolve: 15286 (Vampiric Embrace) ->
932950`, and on a **Holy**-built TBC bot it reads `-> 0 (NOT KNOWN — AI check is always false)`,
which is correct: no VE talent spent.)

## 12. Bot era fidelity

### 12a. Band-move ladder — priest bot `Jaellylah`

`.character level 60 → 70 → 80 → 70`, doctor at every rung:

| Rung | Era read | Points | Band rows held | ORPHAN/PROBLEM |
|---|---|---|---|---|
| L60 | Vanilla | 51 spent (tab 2) | Vanilla helpers only (920xxx/921xxx/932950) — **no 94xxxx row** | 0 |
| L70 | **TBC** | 61 spent (tab 1) | TBC only (940xxx auto-passives + 948046/948051) — **no 92xxxx/93xxxx row** | 0 |
| L80 | WotLK | 0 spent, 71 available | none — native talent frame | 0 |
| L70 (back) | **TBC** | 61 spent | identical to the first L70 rung | 0 |

The teardown-first invariant holds in both directions: no managed-era clone survives an out-of-band
move, which matters because the bot AI resolves spells **by name**.
(`.raidroster syncone` is console-refused — "Run this in-world as a player" — so the ladder's
factory reconcile is driven by the level change itself, which is the same `FactoryReconcile` entry
point.)

The Shadow build in §11 was produced by `.eratalents reset` + 31 `.eratalents learn` calls replaying
the generated tab-2 order (the factory's per-bot spec choice is sticky, so ten level re-rolls all
landed Holy). Every learn was accepted — **zero `Learn rejected:` lines** — which independently
re-verifies that the generated build order is skip-free against `TryLearn`'s rules.

### 12b. Broad doctor sweep — 275 bots, all classes, both managed bands

Driver: `/tmp/claude-1000/tbc-priest/sweep.py` (fresh, reusing `wgconsole.py`'s PTY mechanism) with
a **per-command completion check** on the doctor's own `== doctor done ==` trailer rather than a
fixed wait. **275 headers, 275 `doctor done`, 0 timeouts.**

| Class | Vanilla band | TBC band | Total | ORPHAN/PROBLEM |
|---|---|---|---|---|
| Warrior | 20 | 9 | 29 | **0** |
| Paladin | 24 | 1 | 25 | **0** |
| Hunter | 28 | 1 | 29 | **0** |
| Rogue | 24 | 12 | 36 | **0** |
| **Priest** | 27 | 4 | 31 | **0** |
| Death Knight | 4 | 3 | 7 | **0** |
| Shaman | 27 | 3 | 30 | **0** |
| Mage | 25 | 5 | 30 | **0** |
| Warlock | 27 | 2 | 29 | **0** |
| Druid | 24 | 5 | 29 | **0** |
| **Total** | **230** | **45** | **275** | **0** |

Also zero `known=no` and zero `spellInfo=no` rows across all 275. This is simultaneously the
**Vanilla regression check for all nine classes** and the blast-radius check for the
`EraNodeIsWired` `EraHasTalentTrees` conjunct (§8), which reaches the druid and paladin arms.
The hunter/warlock version-swap-chain false positives fixed in `9b69360` did not recur.

### 12c. Bot build orders

`kAuthoredOrders` gained three generated `(ERA_TBC, CLASS_PRIEST, tab)` rows, all validated
skip-free to the TBC 61-point budget by `gen_bot_builds.py` and confirmed byte-identical to the
generator's current output (§10). No greedy tier-fill fallback, no drift WARN.

## 13. Known open items (not this phase's defects)

- **Vanilla Improved Fade shared-bit over-binding** — surfaced while authoring the TBC twin;
  belongs to the merged, real-client-verified Vanilla tree. **Flag as a separate chip.**
- **Vanilla Wand Specialization school 1** — same provenance (`accepted_gaps` id 12, second half).
  **Flag as a separate chip.** *2026-09-04 follow-up (code-verified, see
  `era-talents-phase3-priest.md` Fix round 1): the Vanilla node is inert for two reasons (crossbow
  subclass gate + Physical misc), AND the TBC node 20501's own `school: 126` on a wand-gated aura 79
  **leaks onto every magic spell while a wand is equipped** — `Unit::SpellPctDamageModsDone`'s
  weapon-specific guard only skips `misc == NORMAL`, and `HasItemFitToSpellRequirements` scans the
  ranged slot. Stock 14524 has the same shape; no 3.3.5a talent teaches it, so upstream never saw it.
  20501 is NOT wand-correct as shipped: it is wand-correct plus +5…25% Mind Blast/SW:Pain/Smite/Holy
  Fire. **Implemented 2026-09-04 (band-gated core patch 0025 `core-wand-spec-era-no-spell-leak` +
  the two Vanilla YAML fixes, gen `bdad4294`), pending real-client sign-off** — the patch removes
  20501's spell-leak with NO TBC YAML change. Add "Mind Blast damage unchanged wand-on vs wand-off
  at 5/5 (TBC priest, 20501 at 5/5)" to the §15 real-client checklist.*
- **Vanilla Blackout stun 932970 `DefenseType 0`** — `accepted_gaps` id 12, first half; the TBC twin
  948079 authors `defenseType` correctly. Retro-fixing Vanilla is a separate, user-visible change.

None of these three is a Phase-6 regression; all three are pre-existing Vanilla content this
phase's sweep happened to illuminate.

## 14. Generator / tool changes this phase

| Change | Why | Guard |
|---|---|---|
| `attributesEx7:` key | Silence's `SpellInfoCorrections.cpp:3886` `SPELL_ATTR7_CAN_CAUSE_INTERRUPT` bit, which a clone never inherits | TDD test; `era_audit.py`'s `_SPELLFIX_KEYABLE` gained the matching `AttributesEx7` row in the same commit |
| `defenseType:` key | the from-scratch Blackout stun's resistability (`Unit::SpellHitResult`) | TDD test |
| `startRecoveryCategory:` / `startRecoveryTime:` pair | Silence's GCD | TDD test |
| `tools/test_gen_era_talents.py::test_priest_and_era1_windows_present` | asserts class 5 / era-1 windows and that family 6 is in `_TRANSCRIBED_SWEEP_FAMILIES`, so a later phase cannot silently drop the priest sweep | part of the 218 |

All four keys are **purely additive and byte-neutral for every already-shipped helper** (none of
which authors them) — re-confirmed by the regen diff in §10.

## 15. What remains UNVERIFIED — REAL-CLIENT CHECKLIST

Headless green is not sign-off. Bots never cast VE/VT on a mob in a way that proves the party
payloads, never shield a party member, never click a Lightwell, and the **player** era path (and
the managed→managed transition) is untestable headlessly. Run at **level 70, TBC era, with a fresh
`patch-V.mpq` at generation `9ad9274e`**.

**Frame and data**

1. All three tabs render real parchment (`PriestDiscipline` / `PriestHoly` / `PriestShadow`); tier 9
   is reachable in the scroll; the point pool reads **61**; **every talent icon renders** (no
   question marks).
2. The addon prints **no** stale-generation warning (the red chat line) — i.e. the installed MPQ is
   `9ad9274e`.

**Discipline**

3. **Divine Spirit** — the trained ranks and **Prayer of Spirit** are purchasable at the right
   levels (DS r2–r5 at 40/50/60/70; PoS r1/r2 at 60/70) and **only** with the previous rank known.
   **Stock DS is NOT purchasable and does not appear in the spellbook.**
4. **Respec out of Divine Spirit** (reset or unspend node 20513): **BOTH** the Divine Spirit ranks
   and Prayer of Spirit r1/r2 disappear from the spellbook (arm 2b — lesson 21); `.eratalents doctor`
   on the character afterwards shows **zero ORPHAN**.
5. **Improved Divine Spirit** — with the talent spent, the DS buff tooltip **and the character
   sheet** show the extra spell damage and healing from Spirit. This is the whole reason the chain
   is cloned; if the character sheet does not move, the aura-174/175 slots are not being modded.
6. **Prayer of Spirit** casts through line of sight on a raid group (the `attributesEx2 0x4` item).
7. **Reflective Shield** — PW:Shield on **SELF** reflects the clone's % of absorbed damage, **and**
   PW:Shield cast on a **PARTY MEMBER** also reflects (this is the `GetCaster()` reading that the
   module script exists to preserve).
8. **Power Infusion** — +20% damage/healing, **3 min** cooldown, TBC tooltip.
9. **Pain Suppression** — **2 min** cooldown. (Its 5% threat reduction is accepted gap 6 — the
   tooltip should not promise what is not shipped.)
10. **Focused Will**, **Martyrdom**, **Inspiration**, **Blessed Recovery** — each procs as tooltipped.
    Blessed Recovery in particular: take a physical crit, confirm the HoT lands.

**Holy**

11. **Circle of Healing** — heals up to 5 members of the **TARGET's party only** (in a raid, another
    group is **not** healed), has **NO cooldown**, and r2–r5 are trainable at 56/60/65/70.
12. **Circle of Healing clone tooltip**: the `$s1` heal value and `$a1` radius render as numbers
    (15 yd), not raw tokens.
13. **Lightwell** — summons, is clickable, heals, and shows **10 charges** (accepted gap 9 — the
    charge count is the WotLK creature's; the tooltip must not promise TBC's number).
14. **Holy Nova** — **not known until the talent is spent**; r2+ trainable afterwards; a **respec
    strips the whole chain** (the node-gated strip, arm 1).
15. **Surge of Light** — procs with **no internal cooldown** (accepted gap 5 — this is a deliberate
    divergence from live; confirm it does not feel degenerate).
16. **Holy Concentration** and **Blessed Resilience** — each procs as tooltipped.
17. **Spirit of Redemption**, **Spiritual Guidance**, **Searing Light** — tooltips match behavior.

**Shadow**

18. **Vampiric Embrace** — casting it applies the **debuff to the mob** (TBC shape), and the
    priest's Shadow damage heals the **party** (solo = self) at **15%**; **Improved VE** raises it
    to 20% / 25%. **Reset → re-learn mid-session → the heal works with NO relog** (the reconcile-on-
    learn invariant; this exact defect shipped once in Vanilla). A **Vanilla-band** character must
    never see 948031.
19. **Vampiric Touch** — the DoT ticks; the priest's Shadow damage to the target restores **mana to
    the party** (the combat log shows the energize); **dispelling VT causes NO backlash damage**;
    r2/r3 trainable at 60/70 with r1 known.
20. **Cast ONLY Vampiric Touch** on a target and cast nothing else — the party's mana must still
    rise from VT's own ticks (the dominant TBC case; proven by code reading only).
21. **Misery** — the debuff appears on SW:Pain / Mind Flay / Vampiric Touch and the target takes the
    tooltipped +% spell damage; **two priests' different Misery ranks do NOT stack** (the 948073
    rank chain).
22. **Shadow Weaving** — Shadow Vulnerability stacks to **5** and is **shared** with a second priest.
23. **Blackout** — stuns off a **Mind Flay tick** (accepted gap 11 is what makes this possible), for
    3 s, and is **resistable** (the `defenseType` item).
24. **Silence** — interrupts a cast, works at **20 yd**, and is **on the GCD** (the
    `startRecoveryCategory`/`startRecoveryTime` pair) — and shows as an interrupt in the combat log
    (the `attributesEx7 0x800` item).
25. **Shadowform** — the damage reduction is **physical only**.
26. **Mind Flay** — **20 yd** range, a **real periodic** channel (not a single hit), and r2–r7
    trainable at 28/36/44/52/60/68.
27. **Spirit Tap** — procs and restores mana as tooltipped.
28. **Darkness / Shadow Focus / Shadow Reach / Focused Mind** — each measurably affects the era Mind
    Flay clone (the A.8 re-point). Easiest check: Mind Flay tick damage moves when Darkness is
    spent.

**Cross-cutting**

29. **Accepted gaps read honestly** — every gap's tooltip describes shipped behavior, not TBC's
    promise (gaps 1, 5, 6, 9, 11 all have player-visible tooltips).
30. **Era transition BOTH ways** — in-game, **self-targeted** `.ip set` to a Vanilla stage and back
    to 8: the tree resets cleanly in both directions, no orphan spells, and **no 92xxxx/93xxxx
    priest helper survives on the TBC side** (nor any 948xxx on the Vanilla side).
31. **Glyph gate** — a TBC-era priest cannot use glyph items and has none equipped.

## 16. Real-client sign-off

**SIGNED OFF 2026-09-04** (user, real 3.3.5a client, `patch-V.mpq` gen `9ad9274e`).

| Item | Result | Notes |
|---|---|---|
| Checklist §15 run at gen `9ad9274e` | PASS | Full §15 pass on the dev realm with the fresh MPQ (addon canary green; `era_talent_meta` == client sentinel). |
| Findings | none | No fix round required. |
| Sign-off | ✅ | Phase 6 (priest) complete. Ships as part of the TBC set — merged to master 2026-09-07 (all nine TBC classes shipped, final all-class review + Vanilla regression passed; the `EraHasTalentTrees(ERA_TBC)` flip is committed). |
