# Era Talents — TBC Phase 7 (Hunter) verification record

> **Generation superseded (2026-09-06 final review):** the current stamp is the closing row of docs/verification/era-talents-final-review-tbc-vanilla.md §10 — reinstall patch-V.mpq + restage the addon before any real-client pass.

**Status: HEADLESS-COMPLETE, AWAITING REAL-CLIENT SIGN-OFF (generation `76709510`).** All **64
nodes** authored and wired, `era_audit.py` clean (0 findings), **223/223** host tests green,
`gen_bot_builds.py` clean with three `OK TBC HUNTER` orders, the four-rung band-move doctor ladder
zero-orphan on a live hunter bot **with a live pet**, and a **277-bot** broad doctor sweep across
all ten classes and both managed bands with zero `ORPHAN` / `PROBLEM` / `known=no` / `spellInfo=no`
rows. (A separate seven-bot **level-80** spot-check of the newly-reached WotLK reconcile arms found
four orphan lines across two bots — three known-pre-existing warlock ones and one hunter one — all
`character_spell` residue from band moves made under older code, structurally invisible to
`StripOrphanedGrants`, and **not reproducible with the current build**; see §12b and §18.)
Branch `feat/era-talents`, **merged to master 2026-09-07** (TBC merges as a set after all nine classes + final
review + a Vanilla regression pass). The `EraTalentIP.cpp` `ERA_TBC` allowlist flip remains an
**uncommitted dev-box edit** — every headless run in this record was made with that edit live, and
the file was verified ` M` and unstaged at close-out; no Phase-7 commit touches it.

**Zero new core patches this phase.** No `patches/*.patch` was added or regenerated.

**Spec/plan:**
`docs/superpowers/specs/2026-09-04-era-talents-tbc-phase7-hunter-content-design.md`
(**Amendment A.1–A.18 supersedes much of the body** — A.16/A.17/A.18 are the per-tab outcomes and
carry the corrections that this record is the roll-up of) + the matching plan.
**Findings authority:** `era-data/_ref/tbc/hunter-spells.yaml` (4166 lines — `method:` /
`id_repurposing:` / `spells:` / `family9_bit_census:` / `proc_data_census:` / `script_census:` /
`core_hardcodes:` / `reuse_ledger:` / `spec_bucket_contradictions:` / `accepted_gaps:` /
`open_questions:` / `survival_map:`) — this record summarizes; the `_ref` is normative.

**Phase commits:** `67c940eb` plan · `179ee5c` T3 (the ref) · `a3bc6b9` Amendment A · `6e90d31`
A-deltas · `8f9dfa6` T3 review round · `cc9d06e` ref recounts · `19141be` T4 (wiring) · `1f90abc`
T5 (substrate) · `e1e7308` + `d90874f` T5 bot-reconcile fixes · `c628ca9` T6 (Beast Mastery) ·
`20f3376` A.16 · `3a07ba6` T7 (Survival) · `46a699e` T7 Readiness correction + A.17 · `2a414c4`
T8 (Marksmanship + the Vanilla back-port) · `e38baa4` A.18 · *this commit* + the doctor-predicate
commit, T9.

---

## 1. What shipped

The complete **TBC 2.4.3 Hunter talent tree** — **64 nodes** (Beast Mastery 21 / Marksmanship 20 /
Survival 23, ids **20600–20663**, era 1) — plus five trainer-taught clone chains, three new module
scripts, one existing script made table-driven, and one new generator key. Value basis:
**wago 2.5.4** end-to-end (the standing TBC basis). Hunter is the **first TBC pet class**; TBC-era
pets keep the stock WotLK pet-talent system by user decision (spec §1), so there is no pet-tree
sub-project — every pet-touching talent rides `mechanic: pet` / `petBuffIds` / owner spellmods onto
the stock Tamed Pet Passive carriers.

- **Class 3, spell family 9.** Family guard clean: **0 rows** with `SpellClassSet` outside `{0, 9}`
  across `940800–941311` ∪ `944000–945999` ∪ `948280–948409`.
- **Auto-passives:** **118 rows in `940800–941311`** (`936000 + (nodeId−20000)*8 + (rank−1)`). No
  overlap with paladin 936000–936511 / druid 936800–937295 / shaman 937600–938087 / warrior
  938400–938927 / rogue 939200–939735 / priest 940000–940511.
- **Hand helpers: 53 ids in `948280–948385`**, with **`948382` a deliberate hole** (§9).
  **Free within the hunter slice: `948282–948289`, `948304–948329`, `948352–948369`,
  `948386–948409`, and `948410–949999` beyond it.** Next class (WARLOCK, Phase 8) claims from
  **948410**.
- **`petBuffBase: 944000` is CLAIMED but UNUSED.** The key is declared on the dataset (first claim
  of the era-1 pet window `[944000, 946000)`), but **every** pet node ended up needing a hand
  carrier via `petBuffIds` — a 119 `APPLY_AREA_AURA_PET` pair (Spirit Bond), per-rank proc carriers
  (Ferocious Inspiration), or a `mechanic: pet` DUMMY whose payload is a helper (Focused Fire) — so
  the generator emitted **no auto pet-buff row** and the 944000 band is empty in the live DB. The
  key stays declared so a later hunter node that needs the auto path has the window reserved; the
  generator unit test that guards the era-1 window ships regardless.
- **NO DUMMY-marker misc consumed.** Spec §4e foresaw one for Improved Aspect of the Hawk's
  "only while Hawk is active" gate; A.5 proved **there is no gate to express** (the Quick Shots proc
  lives on Aspect of the Hawk 13165's own e2 plus its `-13165` proc row, so the talent is a pure
  spellmod pair and grants stock). Registry next-free misc stays **19**.
- **SQL (five files, `2026_09_06_00` … `_04`):** `_00_..._data.sql` + `_01_..._custom_spells.sql`
  (generated); `_02_..._scorpid_sting.sql` (the TBC Hunter marker 948280 + the Scorpid clone 948281
  trainer row), `_03_..._survival_chains.sql` (Wyvern Sting + Counterattack), `_04_..._marksman_
  chains.sql` (Aimed Shot + Trueshot Aura). **Nothing deleted anywhere, nothing re-gated** — every
  stock chain this phase's clones replace already self-gates on an r1 a TBC hunter never holds
  (A.13), so the "deleting a stock trainer row orphans higher ranks" hazard never arose. WotLK
  49049/49050 (Aimed Shot r8/r9), 48998/48999 (Counterattack), 49011/49012 (Wyvern Sting) stay
  reachable for native hunters. TrainerId 8 (the 5-spell starter trainer) is untouched.
- **C++:** `EraTalents.cpp` (the three-way `CLASS_HUNTER` marker arm + `kHunterTrainedChainsTbc` +
  `kEraHunTbcStockSwaps`; `thread_local` on `g_botBuild` and `s_inReconcile`),
  `EraTalentBots.cpp` (`kAuthoredOrders` — three generated TBC hunter orders; the unconditional
  `OnBotLogin` reconcile and the `!EraHasTalentTrees` `OnBotLevelChanged` reconcile — §11),
  `EraTalentProcScripts.cpp` (`era_wyvern_sting` made table-driven for both eras; new
  `era_hun_beast_within_tbc`, `era_hun_expose_weakness_tbc`, `era_hun_readiness_tbc`),
  `EraTalentsCommand.cpp` (TBC hunter doctor orphan predicates over the whole helper slice, the
  three-way marker predicates, the `AI resolve` hunter rows, **and the Vanilla trained-chain
  predicates added at Task 9** — §14), `tools/gen_bot_builds.py` (`BUILDS`).
- **Generator:** one purely-additive key — **`attributesEx4`** (TDD-covered, byte-neutral for every
  already-shipped helper), so the Aimed Shot chain can carry `SPELL_ATTR4_FORCE_DISPLAY_CASTBAR`.
  It was **deliberately NOT added** to `era_audit.py`'s `_SPELLFIX_KEYABLE` set: doing so would
  newly fail the two already-shipped Bestial Wrath clones (A.18).

## 2. Per-tab node counts and disposition tallies

All 64 wired, 0 display-only (live DB: `wired` = 64). Counted **from the shipped dataset**
(`era-data/tbc/hunter.yaml` at generation `76709510`), classifying each node by what its `grants:`
map contains: no `grants:` → **AUTHOR**; only stock ids (< 920000) → **GRANT**; only band ids
(≥ 920000) → **CLONE**.

| Tab | Nodes | ids | GRANT stock | AUTHOR (auto-passive) | CLONE (band grant) |
|---|---|---|---|---|---|
| Beast Mastery | 21 | 20600–20620 | 10 | 8 | 3 |
| Marksmanship | 20 | 20621–20640 | 5 | 12 | 3 |
| Survival | 23 | 20641–20663 | 4 | 15 | 4 |
| **Total** | **64** | 20600–20663 | **19** | **35** | **10** |

**The `_ref`'s `method.node_dispositions` roll-up reads GRANT 20 / AUTHOR 29 / CLONE 9 /
RECONSTRUCT 6.** Both are correct; the two views differ in exactly three understood ways, recorded
so a reader diffing them does not read drift:

1. **RECONSTRUCT is a sub-flavour of AUTHOR in the shipped data.** The six RECONSTRUCT nodes
   (20616 Ferocious Inspiration, 20621 Improved Concussive Shot, 20641 Monster Slaying, 20642
   Humanoid Slaying, 20647 Improved Wing Clip, 20648 Clever Traps) ship as auto-passives with no
   `grants:` map, so the classifier counts them as AUTHOR. 29 + 6 = **35** ✓.
2. **Node 20628 Rapid Killing moved GRANT → AUTHOR** (A.18). A granted stock talent names its
   payload buffs **by id** (`EffectTriggerSpell` → stock 35098/35099) and can therefore never reach
   the era clones that carry the Aimed Shot identity bit — "grant the talent, clone the buffs" was
   self-contradictory. Shipped as a `procPassive` spellmod node triggering 948384/948385. 20 − 1 =
   **19** ✓. (This is workflow **lesson 23**.)
3. **Node 20607 Bestial Swiftness** is AUTHORED in mechanism (A.2 — an owner-held
   `SPELLMOD_ALL_EFFECTS +30` onto Tamed Pet Passive 04 carrying `SPELL_ATTR0_ONLY_OUTDOORS`) but
   its delivery is a **hand helper id** (948301), so the dataset classifier reads it as a band
   grant. 9 + 1 = **10** ✓.

**The 19 GRANT-stock nodes:** 20600 Improved Aspect of the Hawk · 20601 Endurance Training ·
20604 Thick Hide · 20605 Improved Revive Pet · 20609 Improved Mend Pet · 20610 Ferocity ·
20612 Intimidation · 20613 Bestial Discipline · 20615 Frenzy · 20619 Serpent's Swiftness ·
20622 Lethal Shots · 20625 Go for the Throat · 20632 Scatter Shot · 20633 Barrage ·
20638 Improved Barrage · 20655 Killer Instinct · 20657 Resourcefulness · 20658 Lightning
Reflexes · 20659 Thrill of the Hunt.

**The 10 CLONE nodes:** 20607 Bestial Swiftness (948301) · 20617 Bestial Wrath (948290) ·
20620 The Beast Within (948291) · 20627 Aimed Shot (948370) · 20637 Trueshot Aura (948377) ·
20640 Silencing Shot (948383) · 20650 Deterrence (948342) · 20656 Counterattack (948338) ·
20660 Wyvern Sting (948330) · 20663 Readiness (948351).

**Three grants are load-bearing "the era-literal encoding is INERT on this core" calls** and are
recorded so nobody "fixes" them into clones: **Improved Mend Pet 20609** (TBC's own e1 is
`aura 112 OVERRIDE_CLASS_SCRIPTS`; the core's `spell_hun_improved_mend_pet` binds to
`EFFECT_0, SPELL_AURA_DUMMY`, so a literal clone would be dead — live's DUMMY is the working
re-implementation at the same 25/50%, lesson 3), **Go for the Throat 20625** and **Thrill of the
Hunt 20659** (both DIVERGED on the DBC but NEUTRALISED by their negative-id `spell_proc` rows
`-34950` / `-34497` forcing crit-only — a clone would lose the gate, lesson 18).

## 3. Fourteen-family survival-map verdict distribution

`survival_map:` covers **301 ids** — the 191 distinct rank spells across the 64 nodes plus the
transitive `EffectTriggerSpell` closure on both sides, extended four times by edges the closure
alone cannot see (`spell_pet_auras` rows, the nine family-9 pet passive carriers, core-script-
reached ids from `spell_hunter.cpp`'s enum, and the 32 baseline spells the tree's masks target).

| Verdict | All 301 ids | The 191 rank spells |
|---|---|---|
| `DIVERGED` | **162** | **88** |
| `LIVE_MATCH` | 80 | 62 |
| `WAGO_ONLY` | 34 | 26 |
| `LIVE_ONLY` | 10 | — |
| `LIVE_MATCH_UNVERIFIED_SLOT` | 15 | 15 |
| **Total** | **301** | **191** |

Per-tab, counted per `(node, rank)` slot:

| Tab | LIVE_MATCH | DIVERGED | WAGO_ONLY | UNVERIFIED_SLOT | slots |
|---|---|---|---|---|---|
| Beast Mastery | 32 | 21 | 0 | 7 | 60 |
| Marksmanship | 14 | 36 | 12 | 5 | 67 |
| Survival | 16 | 31 | 14 | 3 | 64 |

Sweep shape unchanged from earlier phases: the fourteen-family column list (A effects · B proc
options · C misc + all eight attribute words · D categories incl. `PreventionType` · E power ·
F class-options word 0 · G cooldowns · H aura restrictions · I shapeshift · J levels · K equipped
item · L interrupts · M targets · N identity/name) plus the supplementary O per-effect class-mask
pass (43 differences, all recorded).

**Two id-repurposing hits the N-family + O-mask pass caught, and only those two passes could:**

| ids | TBC name | Live name | Consequence |
|---|---|---|---|
| 19286/19287 | Improved Feign Death | **Survival Tactics** | The effect tuple still reads as +2/+4 resist-miss (so family A did NOT flag 19286), but live's mask is (0x11c, 0x2000, 0) — Feign Death **plus** three traps **plus** Snake Trap Effect — and live adds a `SPELLMOD_COOLDOWN` half. Granting stock ships the **wrong talent**; node 20653 is AUTHORED |
| 19596 | Bestial Swiftness | **Boar's Speed** | Same id, completely different encoding — TBC is an owner spellmod onto Tamed Pet Passive 04 with `ONLY_OUTDOORS`; live is a direct pet `aura 31` with the flag dropped. This is what closes Vanilla gap 2 for TBC (A.2) |

## 4. The five trained clone chains — levels, costs, and the "nothing deleted" verdict

All five are live in `trainer_spell` on **TrainerId 7** (the hunter trainer), each with the correct
`ReqAbility1` chaining. Costs are the **authentic stock curve**, never `ReqLevel × 1000` (the
`1286f34` / `d1263c5` rule).

| Chain | Node | Ids | Levels | Costs (copper) |
|---|---|---|---|---|
| Aimed Shot r2–r7 | 20627 | 948371–948376 | 28 / 36 / 44 / 52 / 60 / 70 | 400 / 700 / 1300 / 2000 / 2500 / 10000 |
| Trueshot Aura r2–r4 | 20637 | 948378–948380 | 50 / 60 / 70 | 1800 / 2500 / **5000** |
| Counterattack r2–r4 | 20656 | 948339–948341 | 42 / 54 / 66 | 1200 / 2100 / 2500 |
| Wyvern Sting r2–r4 | 20660 | 948331–948333 | 50 / 60 / 70 | 1800 / 2500 / 5000 |
| Scorpid Sting (single rank) | — (baseline swap) | 948281 | 22 | 6000 |

The Trueshot r4 price is a **deliberate choice** (A.9 / accepted gap 10): there are **no stock
Trueshot trainer rows at all** (20905/20906/27066 are WAGO_ONLY), so the prices are interpolated —
L50/L60 are unambiguous, and at L70 the two stock talent-rooted chains disagree (Wyvern r4 = 5000,
Aimed r7 = 10000). Wyvern Sting is the only stock talent-rooted chain whose rank levels are exactly
50/60/70, so it is the self-consistent series to copy.

**`spell_ranks` chains rooted in the slice: 5** — 948330 (4, Wyvern sleep) · 948334 (4, Wyvern
wake-DoT, **no trainer rows** — it chains only so `era_wyvern_sting` and `IsRankOf` resolve) ·
948338 (4, Counterattack) · 948370 (7, Aimed Shot) · 948377 (4, Trueshot Aura).

## 5. The Aimed Shot identity decision (A.7) and the free-bit census

`SpellInfoCorrections.cpp:5311-5318` forces `SPELL_ATTR6_NO_CATEGORY_COOLDOWN_MODS` + a 10 s
`RecoveryTime` onto **every** `SPELLFAMILY_HUNTER` spell carrying word0 `0x20000` — a **pattern
sweep, not an id list**, so it runs over our `spell_dbc` rows too. The Vanilla phase escaped it by
zeroing family **and** mask, which cost Efficiency's reach (Vanilla gap 4).

**Free-bit census over 1151 family-9 records (both stores):**

| Word | Free bits | Verdict |
|---|---|---|
| word0 | **none** | fully saturated — bit 17 is Aimed Shot's own, and it is the correction key |
| word1 | **[6]** only | and bit 6 is what TBC's own Ferocious Inspiration party buff 34456 carries, so authoring that WAGO_ONLY spell **consumes** it |
| word2 | **[21–30]** | ten free in both stores; **bit 21 (`0x200000`) is the chosen era Aimed Shot identity** |

**Decision: `family: 9` + `maskC: 2097152`, `0x20000` absent.** That satisfies the correction
sweep's `& 0x20000` test as FALSE while keeping the spell family-reachable — so Readiness's family
walk, `SpellInfo::IsRangedWeaponSpell()` (family 9 without word1 bit 28 → ammo + ranged-weapon
scaling) and every era spellmod all still see it. The **companion arm**
(`SpellMgr.cpp:3492-3503`) drives `FORCE_SEND_CATEGORY_COOLDOWNS` off the **same** bit, so dropping
it means the cooldown must live on `RecoveryTime`, never `CategoryRecoveryTime`. Live DB confirms:
948370–948376 carry `SpellClassSet 9`, mask `(0, 0, 2097152)`, `CastingTimeIndex 19` (2.5 s),
`RecoveryTime 6000`, `Category 0`, `CategoryRecoveryTime 0`.

**Talents authored (rather than granted) so their masks reach the bit:** Efficiency 20624, Mortal
Shots 20630 (**TBC's set WITHOUT 0x4000 Serpent Sting** — the core ORs it into stock by id, so the
era version is deliberately era-literal), **Hawk Eye 20643** (three auto-passives rather than
accepting a gap — this is what CLOSES `_ref` accepted gap 5), plus the Rapid Killing buffs
948384/948385 and the Master Tactician buffs 948346–948350. Careful Aim / Barrage / Improved
Barrage do **not** reach Aimed Shot in TBC — and granting stock Barrage/Improved Barrage is
era-correct **by construction**, because live ADDS Aimed Shot's `0x20000` to their masks and the
era clone drops the bit.

**Master Tactician correction (A.17):** the five buff clones are **tooltip/data fidelity, not a
behaviour fix** — `MOD_WEAPON_CRIT_PERCENT` is summed by bare `GetTotalAuraModifier` calls
(`Player.cpp:7244`, `Unit.cpp:3906/9209`) and never mask-filtered, so stock 34833–34837 would
already have raised the era Aimed Shot's crit. What IS load-bearing is the Aimed Shot bit on the
**talent's proc mask** (node 20662 `proc.affectsMask c: 2097152`) — without it the era Aimed Shot
could not trigger Master Tactician at all.

**Live confirmation of the reach (both eras):**

```
922544–922548 Efficiency (Vanilla)   mask = (64000, 128, 2097152)
940992–940996 Efficiency (TBC)       mask = (522752, 4225, 2097152)
941044        Mortal Shots (TBC r5)  mask = (408065, 1, 2097152)
```

## 6. Core-hardcode table — the six questions (a)–(f)

| # | Subject | file:line | Keys on | Answer |
|---|---|---|---|---|
| (a) | **The Beast Within owner buff** | `SpellAuras.cpp:1841-1857` | `GetId() == 19574` (BW aura on the PET) **and** `owner->HasAura(34692)` — all three ids stock | **DEAD on a clone → module script REQUIRED.** `era_hun_beast_within_tbc` replicates both halves. Second, independent reason for the clone: live 34692 is not a bare marker — it carries `aura 79 MOD_DAMAGE_PERCENT_DONE +10` itself, where TBC's is a pure DUMMY, so granting stock would give a TBC hunter a permanent unconditional +10% damage |
| (b) | **Improved Aspect of the Hawk / Quick Shots** | `spell_proc` row `-13165`; Spell.dbc 13165 e2 | **nothing** — the proc lives ON Aspect of the Hawk itself | **There is no gate to reproduce.** "While Aspect of the Hawk is active" is structural. IAotH 19552–19556 is a pure two-effect spellmod pair, byte-identical wago↔live on all five ranks → GRANT stock. Spec §4e is **void** and DUMMY misc 19 is NOT consumed |
| (c) | **Readiness** | `spell_hunter.cpp:668-712` (`spell_hun_readiness`), bound POSITIVELY to 23989 | a **family walk** plus a three-entry id EXCLUSION list (23989 / 19574 / 59543) | **CLONE + module script.** `Spell::SendSpellCooldown()` runs at `Spell.cpp:3933`, BEFORE the effect handlers — so a clone rebound to the core script would clear its **own** 5-minute cooldown and be infinitely spammable. `era_hun_readiness_tbc` replicates the walk with **948351** in the exclusion set. Also: 23989 differs from live on exactly one field, `RecoveryTime 300000 → 180000` — a grant would ship a 3-minute capstone where TBC is 5 |
| (d) | **Focused Fire / Intimidation / Scatter Shot / Improved Mend Pet** | `spell_pet_auras` data; `spell_hunter.cpp:1285` / `:1717` / `:533` | data (`GetPetAura`), `Id == 19577`, `Id == 37506` (the **NPC copy**, not 19503), `EFFECT_0 + SPELL_AURA_DUMMY` | Focused Fire AUTHORED (no core dependency). Intimidation + Scatter Shot **GRANT stock** (accepted gap 11 — the only divergence is `ManaCostPct` 6 vs 8, and a clone would lose `spell_hun_intimidation` + its proc-event row). **19503 has NO script row** — a Scatter Shot clone would need no rebind, recorded so nobody adds a spurious one. Improved Mend Pet GRANTS stock (lesson 3) |
| (e) | **Pet-aura registration** | `SpellAuraEffects.cpp:5186-5193` → `Unit::AddPetAura` → `Pet::CastPetAuras` | `sSpellMgr->GetPetAura(spellId, effectIndex)` at aura-apply time | **A spellbook-learned stock passive DOES register its `spell_pet_auras` row — CONDITIONAL on the effect at that index being a DUMMY.** That condition is why Ferocious Inspiration and Animal Handler need the module pet path (A.11) |
| (f) | **The Aimed Shot correction arm** | `SpellInfoCorrections.cpp:5311-5318` | family 9 **and** word0 `0x20000` — a pattern sweep over every loaded SpellInfo | **The clone must NOT carry word0 0x20000** → §5's word2 bit-21 identity. `era_audit.py::check_spellfix_inheritance` transcribes the predicate; the audit is clean, and the live query `SpellClassSet=9 AND (SpellClassMask_1 & 131072) AND ID >= 920000` returns **no rows** |

**Diminishing-returns consequence, recorded because it is easy to lose in a clone:** four hunter DR
groups key on a **family-flag + SpellIconID pair** (`SpellMgr.cpp:215-232`), two of them also on a
`SpellVisual`. The Scatter Shot path is untouched (grant), the **Wyvern Sting sleep clones must
carry word1 `0x1000` + `SpellIconID 1721`** for `DIMINISHING_DISORIENT`, and the **Entrapment root
clone templates off 19185** so it inherits `SpellVisualID 7484` + `SpellIconID 20` for
`DIMINISHING_ENTRAPMENT`. The Wyvern sleep clone also inherits the family-9 word1-0x1000 PvP
duration cap (6 s, `SpellMgr.cpp:339-347`) for free.

## 7. Reuse ledger — outcome: ALL FRESH, including the first genuine identity hit

**22 candidate Vanilla-helper pairs assessed.** For the first time in any TBC class phase, three
pairs came back **field-identical on all fourteen families** — and the decision was still FRESH.

| Vanilla helper | Verdict | Deciding evidence |
|---|---|---|
| **932979 / 932958 / 932959 Counterattack r1–r3** | **FRESH (A.10) — the identity is RECORDED, not acted on** | **ZERO diffs on any of the three pairs.** Vanilla 1.12 and TBC 2.4.3 Counterattack are the same spell. Declined for three reasons: (1) TBC needs a **fourth** rank (27067, 165 dmg @ L66) with no Vanilla counterpart, so reuse gives a chain half-Vanilla-band and half-TBC-band that the `spell_ranks` root, `kHunterTrainedChainsTbc` and the doctor predicates would all have to straddle; (2) the strip tables are **era-keyed**, so a shared id must stay alive for both eras — which breaks the `era != ERA_TBC` cross-era strip that is the safety net after a band move; (3) every prior TBC phase closed all-FRESH/MOOT and the one druid reuse was for node-granted single passives, not a trainer-taught rank chain. **Do not reopen without new evidence.** |
| 932966 Wyvern DoT r1 | **FRESH** | one diff, and it is the `ProcChance 0/101` noise sentinel — but TBC needs four DoT ranks and the chain must root in the TBC band so `era_wyvern_sting`'s per-era table resolves cleanly |
| 932968 / 932969 Spirit Bond carriers | **FRESH** | magnitudes identical; `AttributesEx2 0x400000` vs `0x400004` (the Vanilla clone inherited `IGNORE_LINE_OF_SIGHT` from its template). Attribute-word inequality is the ledger's stated bar |
| 932964 Deterrence | **FRESH** | the spec's flagged "tooltips read alike" candidate — the tooltips do, the rows do not: 7 diffs incl. the WotLK ranged-weapon `EquippedItemClass/Subclass` requirement the Vanilla clone inherited |
| 932965 Wyvern sleep r1 | **FRESH** | 7 diffs incl. the RecoveryTime/CategoryRecoveryTime swap and `AttributesEx4/Ex6 0x800000` |
| 932967 Bestial Wrath | **FRESH** | 4 diffs incl. `AttributesEx5 0 → 0x60008` (inherited) — a real design question, see §13 |
| 932952–932957 Aimed Shot r1–r6 | **FRESH** | 11–13 diffs per rank, headed by the whole point of §5 (`family 9→0`, `word0 0x20000→0`), plus cast time and the missing -50% healing rider |
| 932961–932963 Trueshot Aura r1–r3 | **FRESH** | 6–8 diffs, headed by `APPLY_AREA_AURA_PARTY → APPLY_AREA_AURA_RAID` (the Vanilla clone took live's raid scope) and **TBC Trueshot has NO SpellPower row at all — it is free to cast** |
| 932971 Improved Concussive stun | **FRESH** | 4 diffs; TBC's own 19410 also has real art where the Vanilla clone was built from scratch |
| 932972 Entrapment root | **FRESH** | the era-defining diff: `DurationIndex 35 (4 s)` TBC vs `28 (5 s)` Vanilla |
| 932978 Improved Wing Clip root | **FRESH** | 8 diffs; TBC's own 19229 is the cleaner source and is WAGO_ONLY — a reconstruction, not a re-clone |
| 932973–932977 Ferocity carriers | **MOOT** | TBC Ferocity is LIVE_MATCH on all fourteen; node 20610 grants stock and no carrier is minted |
| 932300–932303 Scorpid Sting r1–r4 | **MOOT, and reuse would have been WRONG** | the Vanilla reconstruction is a 4-rank Str/Agi/Sta drain; TBC 3043 is a single-rank `aura 54 MOD_HIT_CHANCE`. Different spell, mechanic and rank count |

## 8. The three-way hunter marker chain

Phase 7 split what used to be a two-arm (Vanilla / post-Vanilla) version-swap into **three**, the
shaman three-marker precedent (`9b69360`):

| Era | Marker | Grants | Strips |
|---|---|---|---|
| Vanilla | **932304** | the Vanilla Scorpid Sting clone chain 932300–932303 on TrainerId 7 | 932305, 948280, 948281, stock 3043 |
| **TBC** | **948280** | the TBC Scorpid clone **948281** on TrainerId 7 (L22, 6000c) | 932304, 932305, 932300–932303, stock 3043 (only once 948281 is trainable), **stock Deterrence 19263** |
| WotLK | **932305** | stock 3043 + the stock Deterrence 19263 trainer row | 932304, 948280, 948281, 932300–932303 |

**The readiness conjunct is on the `else if`, not inside it, and that is load-bearing.** With the
conjunct inside the arm, a TBC hunter on a build where TBC is outside the `EraHasTalentTrees`
allowlist (which is the **committed** state of `EraTalentIP.cpp`) fell into an empty branch and got
**no marker at all** — losing the 932305 the pre-Phase-7 `else` used to grant, i.e. losing access to
stock Scorpid Sting *and* stock Deterrence with no era replacement. Falling through to the WotLK
arm instead makes "not ready" byte-identical to pre-Phase-7 behaviour (commit `d90874f`).

**Why the TBC Scorpid clone ships at all (A.1, user-locked option A):** TBC 3043's
`EffectBasePoints_1 −6` + die 1 = **−5%** hit; live is `−4` + 1 = **−3%**, and `ManaCostPct` is 9 vs
11. The base-spell exception was already crossed for this exact spell in the Vanilla phase; Phase 7
completes the per-era pattern (lesson 17e).

**Deterrence is a TBC TALENT** (node 20650, clone 948342), which is why the stock 19263 trainer row
must stay on 932305 and the TBC arm strips 19263 unconditionally (belt-and-braces: the
`kEraHunTbcStockSwaps` row for 20650 covers a hunter whose marker arm has not run yet).

**Ladder evidence** (§12a) — the era marker printed at each rung: `932304` → `948280` → `932305` →
`948280`.

## 9. Fresh-helper id table — 53 ids in `948280–948385`, with one hole

| Ids | What | Node | Why not a stock grant |
|---|---|---|---|
| **948280** | "Era: TBC Hunter" marker (a `client:` block so the trainer UI can render the "Requires…" line) | — | §8 |
| **948281** | Scorpid Sting (TBC single rank) | — (baseline swap) | −5% vs live's −3%, `ManaCostPct` 9 vs 11 (A.1). `dispel: 4` is **load-bearing**, not cosmetic — `SpellInfo::GetSpellSpecific()` puts family-9 + DISPEL_POISON into `SPELL_SPECIFIC_STING`, and without it two stings would stack |
| **948290** | Bestial Wrath | 20617 | 18 s; **`attributesEx5: 0` per TBC** (live and the Vanilla clone carry `0x60008` by template inheritance). Binds core `spell_hun_bestial_wrath` (CheckCast) **plus** `era_hun_beast_within_tbc` |
| **948291 / 948292** | The Beast Within passive / owner buff | 20620 | live 34692 carries `aura 79 +10%` where TBC's is a bare DUMMY (§6a); the owner buff is a 34471 clone because the core fires the stock one by id |
| **948293 / 948294** | Spirit Bond 119 `APPLY_AREA_AURA_PET` carriers r1/r2 | 20611 | TBC has no healing-received half; FRESH per §7 |
| **948295–948297** | Ferocious Inspiration pet proc carriers r1–r3 | 20616 | full RECONSTRUCTION (A.11): the stock `spell_pet_auras` rows point at LIVE_ONLY WotLK payloads and TBC's own two payload ids are WAGO_ONLY. **One carrier + one buff PER RANK**, because a `mechanic: pet` passive is a bare DUMMY so the per-rank % must live on the buff (lesson 8). Each carrier gets a generator `spell_proc` row (a clone never inherits a proc row — lesson 18) |
| **948298–948300** | Ferocious Inspiration party buffs r1–r3 | 20616 | as above; TBC's word1 bit 6 identity is fine on a payload |
| **948301** | Bestial Swiftness owner spellmod | 20607 | A.2 — `SPELLMOD_ALL_EFFECTS +30` onto Tamed Pet Passive 04 with `attributes` including `0x8000` (`ONLY_OUTDOORS`). Because the passive is **player-held**, the core's outdoor gate applies — this CLOSES Vanilla accepted gap 2 for TBC |
| **948302 / 948303** | Focused Fire pet-damage carriers r1/r2 | 20602 | see accepted gap 12 — the Kill Command crit half cannot ride the same node |
| **948330–948333** | Wyvern Sting sleep r1–r4 | 20660 | word1 `0x1000` + `SpellIconID 1721` for DR; each binds `era_wyvern_sting` |
| **948334–948337** | Wyvern Sting wake-DoT r1–r4 (50/70/100/157 per tick) | 20660 | chains via `spell_ranks` with **no trainer rows**; `targetA: 6` authored (the by-id `ApplySpellFix` a clone never receives — A.14, and the same defect back-ported to Vanilla 932966) |
| **948338–948341** | Counterattack r1–r4 | 20656 | FRESH despite the identity — §7 |
| **948342** | Deterrence | 20650 | 7 diffs incl. live's ranged-weapon requirement |
| **948343** | Entrapment root (4 s) | 20645 | TBC is 4 s, Vanilla 5 s; templates off 19185 to keep the DR visual/icon |
| **948344** | Improved Wing Clip root (5 s) | 20647 | reconstruction from TBC's own WAGO_ONLY 19229 |
| **948345** | Expose Weakness payload | 20661 | basePoints **0** + `era_hun_expose_weakness_tbc` supplying 25% of the caster's Agility in `DoEffectCalcAmount` — auras 127/165 take a FLAT amount on this core, and the DBC literally says 25 (accepted gap 9). Zero rather than a wrong flat 25 if the script never runs |
| **948346–948350** | Master Tactician crit buffs r1–r5 | 20662 | cloned for the Aimed Shot bit; tooltip/data fidelity, not a behaviour fix (A.17) |
| **948351** | Readiness | 20663 | §6c — 5 min not 3, and it needs its own script |
| **948370–948376** | Aimed Shot r1–r7 | 20627 | §5. `attributesEx4` (`FORCE_DISPLAY_CASTBAR`) is the new generator key |
| **948377–948380** | Trueshot Aura r1–r4 | 20637 | `APPLY_AREA_AURA_PARTY` flat +50/75/100/125 RAP, **no mana cost**; r2–r4 are WAGO_ONLY live |
| **948381** | Improved Concussive Shot stun | 20621 | reconstruction |
| ~~948382~~ | **DELIBERATE HOLE** | — | the reserved Concussive Barrage daze slot. The `-35100` proc census proved stock 35101 can be triggered as-is, so nothing was minted — and the Task-5-pinned **948383** kept its id rather than renumber (workflow **lesson 22**) |
| **948383** | Silencing Shot | 20640 | the divergence is the GCD, expressible via `startRecoveryCategory`/`startRecoveryTime`; `speed` 60.0 is acknowledged, not authored (no generator key) |
| **948384 / 948385** | Rapid Killing buffs r1/r2 | 20628 | cloned for the Aimed Shot bit; the **talent** is authored as a `procPassive` spellmod node triggering them (A.18) |

Live DB confirms exactly these **53** ids in `948280–948409` (948382 absent) and **zero** rows in
`948410–949999`.

## 10. Script census, bindings and deliberate no-rebinds

Only **three** POSITIVE single-id hunter script bindings in the whole `spell_script_names` table are
touched by a TBC era node: 23989 (Readiness), 19574 (Bestial Wrath), 19577 (Intimidation).

**Bound this phase (live DB):**

| Script | Bound to | Rows |
|---|---|---|
| `spell_hun_bestial_wrath` (core) | **948290** — alongside the pre-existing Vanilla 932967 | 1 new |
| `era_hun_beast_within_tbc` (new) | 948290 (the BW clone; the aura lives on the PET, so `GetTarget()->GetOwner()` is the hunter) | 1 |
| `era_hun_expose_weakness_tbc` (new) | 948345 | 1 |
| `era_hun_readiness_tbc` (new) | 948351 | 1 |
| `era_wyvern_sting` (made **table-driven**) | 932965 (Vanilla) **+ 948330–948333** (TBC r1–r4) | 4 new (5 total) |

`era_wyvern_sting`'s table is `{sleep clone → wake-DoT}`: `932965→932966`, `948330→948334`,
`948331→948335`, `948332→948336`, `948333→948337`. An unlisted sleep id casts **nothing**
(fail-closed) rather than falling back to a wrong rank. **Vanilla behaviour is byte-identical** to
the retired single-constant version. `Validate` runs **per binding** (the `era_vampiric_embrace`
precedent) so the live Vanilla binding does not fail when the TBC DoTs are absent, and vice versa.

`era_hun_beast_within_tbc` replicates **both** halves of the core arm — the apply-side cast **and**
the remove-side strip. Without the mirror, an early Bestial Wrath removal (dispel, pet death, pet
dismiss, respec mid-BW) would strand the owner buff for its full 18 s. The remove is unconditional
(the core's own `else` is inside the `HasAura(34692)` test, but a hunter can only ever hold the
buff by way of the passive, and stripping a buff nobody has is free).

`era_hun_readiness_tbc` excludes **948351** (itself), 23989, 19574 and 59543 — and **deliberately
does NOT exclude the era Bestial Wrath clone 948290**. **A.17 correction 1:** TBC's 23989 tooltip
has no Bestial Wrath exception (WotLK added it), so a TBC Readiness *should* reset the era BW clone.
A.6 was wrong; the `_ref`'s `core_hardcodes.readiness` was right. Fixed at `46a699e`.

All three new scripts gate on `!Enabled()` + null checks only, **never on "is a bot"** (the CLAUDE.md
era-script rule; the bot-Judgement no-op precedent).

**Deliberate NO-rebinds, each with its reason:**

- **`spell_hun_readiness`** — must NOT be bound to 948351 (§6c: `SendSpellCooldown` runs first).
- **`spell_hun_wyvern_sting`** — resolves the DoT by `GetSpellWithRank(24131, rank)`, i.e. against
  the **stock** chain; that is exactly why `era_wyvern_sting` exists.
- **`spell_hun_scatter_shot`** — bound to the NPC copy **37506**, not to the player spell 19503
  (which has no script row at all). Node 20632 grants stock; nothing to rebind either way.
- **`spell_hun_intimidation`** / **`spell_hun_improved_mend_pet`** / **`spell_hun_thrill_of_the_hunt`**
  — all three nodes GRANT stock, so their positive/negative bindings apply unchanged. Cloning any of
  them would have cost the binding (that is half the argument in accepted gap 11).
- **`spell_hun_animal_handler`** (bound to the WotLK **pet payload** 68361, not the talent),
  **`spell_hun_kill_command`** (baseline in TBC, trained at 66) and the whole WotLK-only set
  (Chimera / Explosive / Lock and Load / Misdirection / Rapid Recuperation / glyph scripts) — none
  is reachable by a TBC hunter talent.

**Proc/pet-aura data census (live DB):** 24 generator `spell_proc` rows in the auto-passive band
`940800–941311` + **3** on the Ferocious Inspiration carriers 948295–948297; **7** `spell_pet_auras`
rows (940888/940889 → Spirit Bond 948293/948294; 940928/940929/940930 → FI 948295/948296/948297;
940816/940817 → Focused Fire 948302/948303).

## 11. The bot-reconcile change (`e1e7308` / `d90874f`) and its cross-class side effect

**The defect:** bots in an **unmanaged** band never reconciled at all. `OnBotLogin` bailed on
`!EraHasTalentTrees(era)`, `FactoryReconcile` returned early on the same predicate, `BotBuildScope`'s
end-of-build reconcile only runs for managed bands, and `TeardownStale` resets `era_character_talent`
**ranks** only — it never touches markers or trainer-taught era spells. Measured 2026-09-04: a hunter
bot moved 70 → 80 kept the TBC marker **948280** and the TBC Scorpid clone **948281 indefinitely**.

**The fix:** `OnBotLogin` now calls `EraTalents::ReconcileBaselineSpells(bot, era)` **unconditionally**,
and `OnBotLevelChanged` calls it when `!EraHasTalentTrees(era)` (managed bands are deliberately not
double-reconciled — `SpendBuild`'s `BotBuildScope` already does it), placed **before** the early
return so both exits are covered. This mirrors the player path (`EraTalentPin.cpp:71`).

**SIDE EFFECT, stated explicitly because it is a real behaviour change for WotLK-band bots:**
reaching reconcile in an unmanaged band also hands them things they previously never got — the
`kBaselineSpellGates` `era >= ERA_TBC` force-grants (Holy Nova r1, Shield Slam r1, Ice Block, Divine
Spirit r1, Consecration r1) and the WotLK-era presence markers (**932939** warlock stones / **932417**
shaman totems / **932305** hunter). That is the **correct** WotLK state and exactly what a real WotLK
player already gets; bots were simply never reaching it. Cost is bounded: one `learnSpell` per missing
spell per bot, **once**, persisted to `character_spell`; steady-state logins re-check with plain
`HasSpell` probes and grant nothing. §12b's L80 spot-check is the blast-radius measurement.

**Thread-safety:** routing every WotLK-band bot through reconcile put `s_inReconcile` (the
re-entrancy guard) and `g_botBuild` (the build-scope state) on **map-update threads** via the level
hooks. Both are now `thread_local` — correct and race-free because at most one scope is ever live per
thread, and both are per-character state, not shared caches. (`EraTalentBots`'s spec-tab cache is
genuinely shared and stays mutex-guarded.)

**Known, expected consequence for existing characters:** a TBC-band hunter bot that already exists
loses stock Scorpid Sting at its next login (the TBC arm strips 3043) and gains the clone **948281
only at its next factory randomize** (the clone is trainer-taught, never reconcile-granted, and the
console cannot drive a bot's trainer walk). This is the designed behaviour, not a defect.

**Doctor gating** was corrected in the same commit (`d90874f`): the TBC-hunter marker PROBLEM line is
suppressed unless `EraHasTalentTrees(ERA_TBC) && sSpellMgr->GetSpellInfo(948280)`, so a build with
the allowlist off does not report every TBC hunter as broken.

## 12. Bot era fidelity

### 12a. Band-move ladder — hunter bot `Izelzenn`, with a live pet

`.character level 60 → 70 → 80 → 70`, `.eratalents doctor` at every rung. **`grep -icE
"ORPHAN|PROBLEM|WRONG-ERA|STALE"` over the whole transcript = 0.**

| Rung | Era read | Points | Era marker | Band rows held | Pet |
|---|---|---|---|---|---|
| L60 | Vanilla | 51 spent (Survival) | **932304** | Vanilla helpers only (922xxx auto-passives, 932952/932957 Aimed Shot, 932964/932965/932979) — **no 94xxxx row** | entry 17217, no band pet aura (no Vanilla pet node in the drawn build) |
| L70 | **TBC** | 61 spent (Beast Mastery) | **948280** | TBC only (940xxx auto-passives + 948290/948291) — **no 92xxxx/93xxxx row** | entry 17217; **`m_petAuras registered: 3`**, pet holds **948294 / 948297 / 948303** all `applied=yes appliedEff0=yes`; the player side shows 948294 + 948303 as `band aura APPLIED on player (owned by caster)` with a **Pet** caster GUID |
| L80 | WotLK | 0 spent, 71 available | **932305** | none — native talent frame | entry 17217, no band aura |
| L70 (back) | **TBC** | 61 spent (Marksmanship) | **948280** | TBC only (940xxx + 948370/948377/948383) | entry 22044, no band aura (Marksmanship has no pet node) |

The teardown-first invariant holds in both directions: no managed-era clone survives an out-of-band
move — which matters because the bot AI resolves spells **by name**.

**Four extra `band DUMMY marker` lines appear at the L70 Beast Mastery rung** (940817 Focused Fire,
940889 Catlike Reflexes, 940930 Ferocious Inspiration, 948291 The Beast Within). These are **not**
era markers: the doctor's `band DUMMY marker` line prints every band-owned `SPELL_AURA_DUMMY`, and
those four are DUMMY-shaped node grants (three of them the pet-aura registration carriers, whose
DUMMY-ness is exactly the `GetPetAura` registration condition of §6e). The **era** marker is the one
of 932304 / 948280 / 932305 present at each rung, and it is unique per rung.

**Trained-chain evidence, and the reason the Vanilla predicates were needed.** The `AI resolve` rows
show the bot's factory trainer walk really does buy trained ranks in both bands:

```
L60 (Vanilla):  19434 -> 932957   (Aimed Shot rank 6 — the top Vanilla rank)
L70 (TBC, MM):  19434 -> 948376   (Aimed Shot rank 7)
                19506 -> 948380   (Trueshot Aura rank 4)
                34490 -> 948383   (Silencing Shot)
L80 (WotLK):    19434 -> 19434    (stock — correct)
```

The L60 rung is the direct evidence for the Task-9 doctor fix (§14): `19434 -> 932957` proves the
bot held **trained** Vanilla ranks up to r6, and with no predicate the doctor's generic `NodesFor()`
fallback cannot resolve 932953–932957 (only r1 932952 is node-granted), so each would be reported as
an orphan — the gap A.18 recorded. The rung is now silent, and so is the whole ladder.

### 12b. Broad doctor sweep — 277 bots, all classes, both managed bands

Every bot online at level ≤ 70, driven through `.eratalents doctor` in batches of 40 via
`tools/wgconsole.py` (one output file per batch; each doctor's own `== doctor done ==` trailer is the
completion marker).

**277 doctor headers, 277 `== doctor done ==` trailers, 0 timeouts.** Bots were selected as
`online=1 AND level<=70`, so every row lands in a MANAGED band — no WotLK-band rows appear here (the
L80 spot-check below covers that side).

| Class | Vanilla band | TBC band | Total | ORPHAN/PROBLEM |
|---|---|---|---|---|
| Warrior | 24 | 4 | 28 | **0** |
| Paladin | 22 | 6 | 28 | **0** |
| **Hunter** | 28 | 4 | 32 | **0** |
| Rogue | 23 | 9 | 32 | **0** |
| Priest | 26 | 7 | 33 | **0** |
| Death Knight | 1 | 4 | 5 | **0** |
| Shaman | 26 | 1 | 27 | **0** |
| Mage | 28 | 5 | 33 | **0** |
| Warlock | 24 | 4 | 28 | **0** |
| Druid | 23 | 8 | 31 | **0** |
| **Total** | **225** | **52** | **277** | **0** |

Also zero `known=no` and zero `spellInfo=no` rows across all 277 — and in fact the strings `ORPHAN`
and `PROBLEM` do not occur **anywhere** in the seven batch transcripts. This is simultaneously the
**Vanilla regression check for all nine classes** (in particular the Aimed Shot identity back-port,
which touched shipped Vanilla data) and the blast-radius check for the unmanaged-band reconcile
change of §11.

**One aggregation trap worth recording**, because the plan's own suggested command has it: a naive
`grep -icE "ORPHAN|PROBLEM|WRONG-ERA|STALE"` reports **27 false hits** on this sweep — the shaman
doctor's own `AI resolve:` LABEL text reads `Elemental Focus (16164 = STALE STOCK; 947444 = clean)`,
which is descriptive, not a finding. Match `ORPHAN ERA SPELL` / `<-- PROBLEM` instead.

Level-80 spot-check of the newly-reached WotLK reconcile arms (§11) — three hunters, two priests,
two warlocks:

| Bot | Class | Era read | Era marker granted | ORPHAN |
|---|---|---|---|---|
| `Idrus` | Hunter | WotLK | **932305** | 0 |
| `Brudugs` | Hunter | WotLK | **932305** | **1 — 922676** (see below) |
| `Notaan` | Hunter | WotLK | **932305** | 0 |
| `Telanae` | Priest | WotLK | (priest has no era presence marker) | 0 |
| `Satrus` | Priest | WotLK | (none) | 0 |
| `Michanna` | Warlock | WotLK | **932939** | 3 — 921745 / 921754 / 921788 (**known pre-existing**) |
| `Klymkin` | Warlock | WotLK | **932939** | 0 |

The marker grants are the §11 side effect working as designed: all three hunters now hold 932305 and
both warlocks hold 932939, which before `e1e7308` they would never have received.

**`Brudugs`'s single orphan is the SAME pre-existing shape as `Michanna`'s three, not a regression** —
verified rather than assumed. Both bots have **zero `era_character_talent` rows**, yet
`character_spell` still holds band ids (`Brudugs`: 922676 = the Vanilla hunter Entrapment rank-5
auto-passive, node 18334; `Michanna`: three Vanilla warlock auto-passives). These are historic
residue from band moves made under older code: `StripOrphanedGrants` is scoped to
`NodesFor(currentEra)`, which is **empty** for WotLK, so nothing in the current reconcile can see a
leftover from a *different* era's node list. The current code does not produce them — the §12a
ladder is the proof: `Izelzenn` arrived at L80 straight from a 51-point Vanilla build and held
**only** the marker 932305, no 92xxxx row at all. Recorded as an open item in §18; a retro-cleanup
would need a generic "strip any band spell no era's spent nodes grant" sweep, which is a design
change, not a Phase-7 fix.

### 12c. Bot build orders

`kAuthoredOrders` gained three generated `(ERA_TBC, CLASS_HUNTER, tab)` rows, all validated
skip-free to the TBC budget by `gen_bot_builds.py`: **tab0 64 pts / tab1 61 pts / tab2 64 pts**
(tabs 0 and 2 overflow past 61 because the generator validates each order to the era's full budget
and the Beast Mastery / Survival trees have more than 61 points of in-tab nodes — the extra entries
are the cross-tab spillover the validator requires). Three `OK TBC HUNTER` lines, **no `FAIL`**, and
**zero drift WARNs** in the worldserver log.

## 13. Bundled Vanilla fixes (shipped in this phase's commits)

Four Vanilla-era changes ride the TBC hunter phase, each with its own justification:

| Fix | Commit | What | Why bundled |
|---|---|---|---|
| **932966 `targetA: 6`** | `3a07ba6` | the Vanilla Wyvern wake-DoT clone carried `ImplicitTargetA_1 = 25` (TARGET_UNIT_TARGET_ANY); `ApplySpellFix({24131,24134,24135})` rewrites the stock DoT's TargetA to **6**, and a clone never receives a by-id fix (A.14 item 1) | found by the TBC sweep while authoring the identical TBC clones; a one-field data fix |
| **Aimed Shot identity back-port** | `2a414c4` | 932952–932957 adopt `family: 9` + word2 bit 21 (ids unchanged, so **no `character_spell` migration**), and Vanilla Efficiency 18318's mask gains the bit | the rogue-phase Riposte precedent for a bundled cross-era fix; **retires Vanilla record gap 4** |
| **Vanilla Trueshot re-price** | `2a414c4` | 932962 → **1800**, 932963 → **2500** (from 8000/16000) | the shipped rows predate the authentic-cost rule (`1286f34` / `d1263c5`) and were on the baseline `ReqLevel × 1000`-adjacent curve |
| — | — | *(not actioned)* the Vanilla BW clone 932967 inherits live's `AttributesEx5 0x60008`; and BW clone 948290 could now author `attributesEx4 \|= 0x4` since the key exists | both deferred to a Vanilla fix round (A.14 item 2 / A.18) |

Live confirmation: `932952`/`932957` carry `SpellClassSet 9`, mask `(0,0,2097152)`,
`CastingTimeIndex 14` (3 s — Vanilla's own), `RecoveryTime 6000`; Efficiency 922544–922548 mask
`(64000, 128, 2097152)`; `932966 ImplicitTargetA_1 = 6`; trainer rows 932962 = 1800, 932963 = 2500.
**The 6 s cooldown survives the identity change** because the `SpellInfoCorrections` sweep keys on
word0 `0x20000`, which the clones still do not carry.

## 14. Doctor gap closed at Task 9 (master-era, doctor-only)

The Vanilla hunter **trained-rank** chains had no orphan predicate, so every Vanilla-band hunter
reported spurious `ORPHAN ERA SPELL` lines. This is a **pre-existing master-era gap** — the same
shape as the Vanilla Bloodthirst / Hemorrhage / warlock rows that earlier phases fixed — surfaced by
this phase's ladder run. Three node-gated predicates added to `EraTalentsCommand.cpp`, legitimate
iff `era == ERA_VANILLA && CLASS_HUNTER && CurrentRank(t, ERA_VANILLA, <node>) != 0`:

| Ids | Chain | Node |
|---|---|---|
| 932953–932957 | Aimed Shot r2–r6 (r1 = 932952) | 18321 |
| 932962–932963 | Trueshot Aura r2–r3 (r1 = 932961) | 18330 |
| 932958–932959 | Counterattack r2–r3 (r1 = **932979**) | 18344 |

**Rank 1 is deliberately OUTSIDE each range** — the Vanilla convention (unlike the TBC slices, where
r1 is inside so the chain reads as one unit): r1 is a plain node grant the generic `NodesFor()`
fallback resolves correctly on its own, and Counterattack's r1 is not even contiguous with its r2–r3
pair. Doctor-only; no behaviour change; no generation-stamp bump.

## 15. Headless sweep (generation `76709510`)

| Check | Result |
|---|---|
| `tools/era-regen.sh --sync-fork` | stamp **`76709510`**, +913 custom client spell rows merged into `patch-V.mpq`, module synced into the fork |
| `tools/era_audit.py` | **0 findings** |
| `pytest tools/ -q` | **223 passed** (102 s) |
| `tools/gen_bot_builds.py` | clean; three `OK TBC HUNTER` orders; **no `FAIL`** |
| `docker compose build ac-worldserver` | exit 0 |
| readiness | `ac-worldserver` Up **and** `era_talent_meta` = `76709510` |
| boot line | `[mod-era-talents] loaded 880 talent nodes (generation 76709510)` |
| `era_talent` wired rows, era 1 class 3 | **64** |
| family guard (`SpellClassSet NOT IN (0,9)` over 940800–941311 ∪ 944000–945999 ∪ 948280–948409) | **no rows** |
| outside-slice ids (948410–949999) | **no rows** |
| swept-bit guard (`SpellClassSet=9 AND SpellClassMask_1 & 0x20000 AND ID ≥ 920000`) | **no rows** — no era spell can be caught by the Aimed Shot correction sweep |
| helper rows 948280–948409 | **53**, and **948382 absent** as designed |
| auto-passive rows 940800–941311 | **118** |
| pet-window rows 944000–945999 | **0** (`petBuffBase` claimed, unused — §1) |
| `era_talent` group-by | all nine era-0 rows unchanged; era-1 `1 1 66` / `1 2 64` / `1 4 67` / `1 5 64` / `1 7 61` / `1 11 62` unchanged **plus the new `1 3 64`** |
| `trainer_spell` rows in 948280–948409 | **16** (Scorpid 1 + Wyvern 3 + Counterattack 3 + Aimed Shot 6 + Trueshot 3), all TrainerId **7** |
| `spell_ranks` chains rooted in the slice | **5** — 948330 (4) · 948334 (4) · 948338 (4) · 948370 (7) · 948377 (4) |
| `spell_script_names` rows on band ids 948280–948409 | **8** — 948290 x2 (`spell_hun_bestial_wrath` + `era_hun_beast_within_tbc`), 948345, 948351, and 948330–948333 (`era_wyvern_sting`) |
| `spell_proc` rows | 24 auto-passive + 3 helper (948295–948297) |
| `spell_pet_auras` rows | 7 |
| worldserver log | **0 `ERROR`**, **0 `drift`** WARNs, 0 crash/assertion indicators. `Errors.log` holds only three pre-existing unrelated lines (a MySQL deadlock retry, `spell_gen_submerge_visual` unassigned, an SAI text id) |
| `git status` after the regen | **no generated-artifact diff** — the Task-9 C++ change is doctor-only and moves no shipped data, so the stamp did not move |

## 16. Bot AI by-id check

Grep over `Ai/Class/Hunter/`, `Bot/`, `Mgr/` and `Ai/` for every stock id this phase cloned or
swapped (19434, 20900–20904, 27065, 19506, 19306, 20909, 20910, 27067, 19386, 24132, 24133, 27068,
19263, 19574, 34692, 23989, 34490, 3043) returned **exactly one hit**:

| Site | Verdict |
|---|---|
| `Ai/Base/Value/ArenaCoordValues.cpp:31` — `19263, // Deterrence` in `TURTLE_AURAS[]` | **NO ACTION.** This is our own patch-0005 arena code, and it is a **target-side aura presence** test (a target under a turtle defensive is a wasted kill-target swap), not a spell the bot casts. Rated arena is **level 80**, i.e. the WotLK band, where a hunter holds stock 19263 — so the list is correct for every character that can reach it. If rated arena ever became reachable below 80, this would need the patch-0021 `EraTalentBots_ResolveSpellId` idiom |

**No unrouted by-id site in the hunter AI itself. No fork-patch change needed this phase** — spec §5
predicted this and it held.

**Everything else resolves by NAME and is era-safe.** The hunter AI's references are lowercase name
strings (`"bestial wrath"`, `"trueshot aura"`, `"wyvern sting"`, `"deterrence"`, `"readiness"`,
`"silencing shot"`, `"intimidation"`, `"aimed shot"`, `"scatter shot"`), and **every era clone keeps
its stock display name** — so **no `kEraNameAliases` entry is needed for hunter**. The doctor's
`AI resolve →` rows in §12a are the live proof for the three ids the doctor tracks.

## 17. Accepted gaps

Spec A.15 items 1–11 plus A.16 item 12. **Gaps 2 and 5 are CLOSED for TBC** and are kept in the table
so the reasoning survives.

| # | Subject | Closes if |
|---|---|---|
| 1 | **Improved Feign Death is behaviorally inert on this core** — `AuraEffect::HandleFeignDeath` computes no resist roll (the resisted branch is commented out, as retail WotLK ships), so node 20653's `SPELLMOD_RESIST_MISS_CHANCE` buys nothing. Rows are authored faithfully at TBC's +2/+4 | the core restores the FD resist roll (a core patch; out of scope under the zero-core-patch policy) |
| 2 | ~~Bestial Swiftness has no outdoor gate~~ | **CLOSED for TBC by A.2** — TBC's own 19596 is an OWNER-held spellmod carrying `ONLY_OUTDOORS`, and a player-held passive IS area-gated by the core. Vanilla node 18308 stays as-is (a back-port candidate) |
| 3 | **Improved Scorpid Sting is not a TBC node** — informational; the Vanilla DUMMY misc 16 + `era_hunter_scorpid_sting` stay Vanilla-only | n/a — recorded so nobody wires misc 16 into the TBC dataset |
| 4 | **Frost Trap Aura 13810's slow is a BASELINE divergence** (TBC −60% vs live −50%) | a user decision to extend the per-era baseline-clone exception to Frost Trap; the wago values are recorded and nothing is unknown |
| 5 | ~~Hawk Eye does not extend the era Aimed Shot's range~~ | **CLOSED by A.7** — node 20643 is AUTHORED as three auto-passives whose `affectsMask` is TBC's word-0 set **plus** `c: 2097152`. Cost: three auto-passive rows |
| 6 | **Frost Trap Slow 67035 is unreachable** by any spellmod (all-zero family mask). **Narrower in TBC than in Vanilla**: 67035 and its trigger 63487 are both LIVE_ONLY WotLK re-plumbing; TBC's frost trap casts 13810 directly and 13810 IS masked, so the ground-cloud duration/resist ARE extended | n/a — a WotLK addition an era hunter simply also gets |
| 7 | **Lethal Shots / Ranged Weapon Specialization have all-weapon / all-school scope** — aura 52 recomputes ALL weapon crit; aura 79 misc 127 is all-school. No ranged-only equivalent exists in 3.3.5a, and TBC's own rows use the same auras | n/a — the stock chains ship the same scope in both eras |
| 8 | **Aimed Shot's −50% healing rider inherits the SPELL's `DurationIndex 1` (10 s)** rather than a separate value | real-client verification that the debuff reads 10 s (checklist item) |
| 9 | **Expose Weakness's "25% of your Agility" needs a script, not data** — auras 127/165 take a FLAT amount here and the DBC literally says 25. basePoints 0 so a non-running script ships **nothing** rather than a wrong flat +25 | n/a — closed by design at escalation rung 5 (module script, not a core patch) |
| 10 | **Trueshot r4's 5000c is a curve CHOICE** between two equally authentic stock prices (Wyvern 5000 vs Aimed 10000) | n/a — deliberate, recorded so it is not re-litigated |
| 11 | **Intimidation and Scatter Shot cost 8% base mana instead of TBC's 6%** (both GRANT stock). Cloning either was the fidelity-maximal alternative and was declined — an Intimidation clone would lose `spell_hun_intimidation`, its positive proc-event row and live's e2 `SCRIPT_EFFECT` that sends the pet | a later decision to clone either; the wago values are recorded |
| 12 | **Focused Fire (20602) ships the pet-damage half only** — TBC's e2 (+10/20% Kill Command crit, `aura 107` misc 7 on word1 `0x800`) cannot ride the same node (a `mechanic: pet` passive is a fixed single DUMMY; one spell per rank, lesson 8). It is **inert on this core regardless**: the Kill Command a TBC hunter trains at 66 is live 34026, a self-DUMMY with no damage (TBC's damage half 34027 is WAGO-only) | a `kEraHunTbcPaired` arm or an `ownerEffects:` generator key exists **and** Kill Command deals damage |

## 18. Known open items (not this phase's defects)

- **Vanilla BW clone 932967 inherits live's `AttributesEx5 0x60008`** (`ALLOW_WHILE_STUNNED |
  FLEEING | CONFUSED`) — a Vanilla fix-round candidate (A.14 item 2). The TBC clone 948290 authors
  `attributesEx5: 0` correctly.
- **TBC BW clone 948290 could now author `attributesEx4 |= 0x4`** since the key exists (A.18,
  deferred deliberately — same family as the item above).
- **Vanilla node 18308 Bestial Swiftness** could adopt the A.2 owner-spellmod shape and close its
  outdoor gate too (gap 2's Vanilla half).
- **Stale band spells on bots that crossed a band under OLDER code have no retro-cleanup.**
  Level-80 warlock `Michanna` reports three Vanilla auto-passive orphans (921745 / 921754 / 921788)
  and level-80 hunter `Brudugs` reports one (922676). Both bots have **zero `era_character_talent`
  rows** — the residue is in `character_spell` only. The cause is structural, not a Phase-7 defect:
  `StripOrphanedGrants` walks `NodesFor(currentEra)`, which is **empty** for a WotLK-band character,
  so no reconcile pass can see a leftover belonging to a *different* era's node list. Current code
  does not create them (§12a's ladder lands at L80 with nothing but the marker). Closing this would
  need a generic "strip any band id that no era's spent nodes grant" sweep — a design change worth
  raising, since it also affects real players who moved eras before a given fix landed.

## 19. What remains UNVERIFIED — REAL-CLIENT CHECKLIST

Headless green is not sign-off. Bots never buy a trained rank interactively, never watch a cast bar,
never see a tooltip, and the **player** era path (and the managed→managed transition) is untestable
headlessly. Run at **level 70, TBC era, with a fresh `patch-V.mpq` at generation `76709510`**.

**Frame and data**

1. All three tabs render real parchment (`HunterBeastMastery` / `HunterMarksmanship` /
   `HunterSurvival`); tier 9 is reachable in the scroll; the point pool reads **61**; **all 64
   talent icons render** (no question marks).
2. The addon prints **no** stale-generation warning (the red chat line) — the installed MPQ is
   `76709510`.
3. **The hunter-only "Pet Talents" button / `/pettalents` still opens the STOCK pet tree**
   (Ferocity / Cunning / Tenacity) — TBC pets deliberately keep the WotLK pet-talent system.

**Marksmanship**

4. **Aimed Shot** — cast bar at the **TBC cast time (2.5 s)**, **6 s cooldown**, the **−50% healing
   debuff** lands on the target (and reads **10 s** — accepted gap 8), and the **mana cost drops
   with Efficiency spent** (the §5 identity decision).
5. **Aimed Shot r2–r7 are purchasable at 28/36/44/52/60/70** at the authentic costs, and the
   **stock** ranks are **not offered**.
6. **Trueshot Aura** gives a party member the **flat RAP** (not live's +10% AP), costs **no mana**,
   and **r4 is trainable at 70**.
7. **Silencing Shot** silences and its **GCD** matches TBC (the `startRecoveryCategory`/
   `startRecoveryTime` pair).
8. **Rapid Killing** — after a killing blow, the next **Aimed Shot** hits harder (the buff clone
   reaches the era Aimed Shot).
9. **Improved Concussive Shot** stuns off Concussive Shot for 3 s.
10. **Hawk Eye** measurably extends the **era Aimed Shot's** range (gap 5's closure).

**Beast Mastery**

11. **Bestial Wrath** — **18 s**, and **The Beast Within** puts the **owner buff** on the hunter
    (−20% mana cost, +10% damage) **and grants CC immunity**; the buff **disappears** if Bestial
    Wrath is removed early (dispel / pet death / dismiss).
12. **Ferocious Inspiration** — a **pet crit** puts the party +damage buff up.
13. **Spirit Bond** — ticks on **both** the hunter and the pet, and **survives re-summoning the
    pet** (dismiss + call, and a different pet).
14. **Focused Fire** — pet damage rises (the Kill Command crit half is accepted gap 12).
15. **Bestial Swiftness** — pet speed +30% **outdoors** and **not indoors** (gap 2's TBC closure).
16. **Go for the Throat** — the hunter's ranged crit gives the pet focus.

**Survival**

17. **Wyvern Sting** — sleeps, and on waking applies the **era wake-DoT at the right rank**;
    **r2–r4 trainable at 50/60/70**, and a **respec out of the node cascades the trained ranks away**.
18. **Counterattack** — **r2–r4 trainable at 42/54/66**, same respec cascade.
19. **Deterrence** — the **talent** version gives 25% dodge/parry for 10 s, and the **stock 19263 is
    NOT purchasable** from the trainer.
20. **Readiness** — resets the hunter's cooldowns, **including the era Bestial Wrath clone**
    (TBC-authentic, A.17 correction 1), and **does NOT reset its own** 5-minute cooldown.
21. **Expose Weakness** — a ranged crit puts the debuff up and it is worth roughly **25% of the
    hunter's Agility** in AP to attackers (not a flat +25 — gap 9).
22. **Master Tactician** — procs off ranged hits and raises crit.
23. **Entrapment** roots for **4 s** (not Vanilla's 5) and is subject to DR; **Improved Wing Clip**
    roots for 5 s.
24. **Monster Slaying / Humanoid Slaying** measurably move damage against a Beast vs a Humanoid.
25. **Clever Traps / Trap Mastery** — trap duration/damage and resist behave as tooltipped.

**Baseline swap**

26. **Scorpid Sting** at the **TBC −5%** value, and **stock 3043 is not purchasable**; two stings do
    **not** stack on one target (the `dispel: 4` item).

**Vanilla re-check (the bundled back-port)**

27. On a **Vanilla** hunter: **Aimed Shot's mana drops with Efficiency spent**, its **cooldown is
    still 6 s**, and its **cast time is still 3 s**.
28. On a **Vanilla** hunter: **Trueshot Aura r2 costs 1800c and r3 costs 2500c** at the trainer.

**Cross-cutting**

29. **Accepted gaps read honestly** — every gap's tooltip describes shipped behaviour, not TBC's
    promise (gaps 1, 3, 4, 6, 7, 8, 9, 11, 12 all have player-visible tooltips).
30. **Era transition BOTH ways** — in-game, **self-targeted** `.ip set` to a Vanilla stage and back:
    the tree resets cleanly in both directions, no orphan spells, and **no 92xxxx/93xxxx hunter
    helper survives on the TBC side** (nor any 948xxx on the Vanilla side). The marker is exactly one
    of 932304 / 948280 / 932305 at every point.
31. **Glyph gate** — a TBC-era hunter cannot use glyph items and has none equipped.

## 20. Real-client sign-off

**SIGNED OFF by the user 2026-09-05** ("looks good") on a level-70 TBC hunter at gen `76709510` with the
fresh `patch-V.mpq` and the restaged addon.

| Item | Result | Notes |
|---|---|---|
| Checklist §19 run at gen `76709510` | PASS | one finding, fixed the same session (§21) |
| Findings | 1 | Improved Aspect of the Hawk panel tooltip showed WotLK text — addon fix `10dd211` (stock-grant nodes now show the authored era prose; affects every class/era, no regen) |
| Sign-off | **COMPLETE** | Phase 7 (hunter) ships as part of the TBC set — merged to master 2026-09-07 (all nine TBC classes shipped, final all-class review + Vanilla regression passed; the `EraHasTalentTrees(ERA_TBC)` flip is committed). Next class: **WARLOCK (Phase 8)**, helpers from 948410, pet band 945000. |

## 21. Real-client fix round

### Finding 1 (2026-09-05) — Improved Aspect of the Hawk panel tooltip named "Aspect of the Dragonhawk"
**Root cause:** `TalentUI.lua NodeTooltip` renders any grant node via `SetHyperlink("spell:<id>")`. For a
BAND clone the client row comes from patch-V.mpq (era text); for a STOCK grant it is the 3.3.5a row, i.e.
WotLK prose. Every stock-grant node in every class/era was affected in the talent PANEL (the datasets'
`tooltip:` text was always era-correct — it was simply not shown). **Fix (`10dd211`):** the hyperlink path
is taken only for ids >= 920000; stock grants show the authored per-rank era prose. Addon-only; no regen,
no stamp change; restage `client-addons/EraTalents`. **Residual (framework lesson 17d):** a granted STOCK
spell's SPELLBOOK tooltip is still the stock client row (global) — visible for the passive itself in the
spellbook's passive list; era-correcting that would require cloning each stock grant for text alone.
