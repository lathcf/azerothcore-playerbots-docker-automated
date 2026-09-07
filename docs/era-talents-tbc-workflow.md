# Era Talents — TBC Era Workflow (kickoff + canonical per-class process)

**Status:** COMPLETE — all nine TBC classes shipped and merged to master 2026-09-07 (prepared 2026-08-25, immediately after the Vanilla merge to master).
**Branch:** `feat/era-talents` (TBC authored here, merged to master 2026-09-07; Vanilla shipped in master `8f15d1d`).
**Prerequisite reading, in order:** `docs/era-talents-framework.md` (canonical — decision tree,
id registry, wiring sites, verification checklists; era-agnostic and still authoritative),
this doc, then the Shaman phase pair as the workflow template
(`docs/superpowers/specs/2026-08-21-era-talents-phase10-shaman-content-design.md` +
`docs/superpowers/plans/2026-08-21-era-talents-phase10-shaman.md` +
`docs/verification/era-talents-phase10-shaman.md`).

This doc is the TBC analog of the Vanilla project's institutional knowledge: what the era is,
where the trustworthy data lives, what infrastructure must change BEFORE the first class, and
the per-class workflow (adapted from the Vanilla phases 2-10 pattern). A fresh session starting
TBC work reads this and enters via `/superpowers:brainstorming` for **Phase 0 (enablement)** —
do not start a class before Phase 0 ships.

---

## 1. What the TBC era is

- `era: 1` / `eraName: TBC` (`EraId::ERA_TBC`), datasets in **`era-data/tbc/<class>.yaml`**
  (the generator already reads era from the YAML; the stamp already salts by parent dir, so
  `vanilla/mage.yaml` and `tbc/mage.yaml` cannot alias).
- IP stages **8-12** (`PROGRESSION_PRE_TBC` .. `PROGRESSION_TBC_TIER_4`); WotLK starts at 13.
  Dev-box era-setting: `.ip set <char> 8`.
- Level cap **70**, talent budget **61 points** (`level - 9` — the existing `AvailablePoints`
  formula is numerically correct for TBC, but see Phase 0 item 6 for the per-era cap gap).
- 9 classes, same as Vanilla (Draenei/Blood Elf unlock shaman/paladin cross-faction but adds
  no class — race gating is out of scope for the talent window).
- Trees are 3 tabs as before but **up to 9 tiers deep** (rows 1-9, capstone at tier 9),
  ~60-70 nodes/class, **579 talents / 27 tabs total** (vs Vanilla's 432).
- Divergence baseline is **TBC 2.4.3 vs stock WotLK 3.3.5a** — a much smaller gap than
  Vanilla's (one expansion instead of two). Expect far more clean `grants:` and far fewer
  clones/reconstructions than Vanilla needed. The Vanilla pass deliberately EXCLUDED
  TBC-added ranks (e.g. rank 7s learned at 61+) — those now get authored.

## 2. Sources of truth (verified 2026-08-25 — each was actually fetched and cross-checked)

**Decisive fidelity check:** the wago.tools 2.5.4.44833 `Talent` table and the Wowhead TBC
talent-calc blob were diffed talent-by-talent: **identical across all 579 talents** (ids, tabs,
positions, per-rank spell ids, prereqs — 0 mismatches). TBC Classic (2.5.x) uses the 2.4.3
talent trees verbatim; all documented TBCC deviations from original 2.4.3 are outside the
trees (cross-faction seals as NEW spells, Drums nerf, raid tuning). **Treat 2.5.x data as
2.4.3-faithful.** Independent pre-TBCC cross-check: `Vampyr7878/WoW-Talent-Caluclator-TBC`
(2019, private-server-era XML) matches TBCC rank values exactly where sampled.

| Rank | Source | Provides | How |
|---|---|---|---|
| **1 — committed snapshot** | `era-data/_ref/tbc/Talent-2.5.4.44833.csv` + `TalentTab-2.5.4.44833.csv` (this repo, from wago.tools) | THE structural source of record: talent id, `TierID` (row), `ColumnIndex`, `TabID`, `SpellRank_0..8` (per-rank spell ids), `PrereqTalent_0..2`+`PrereqRank`; TalentTab gives tab `Name_lang`, `BackgroundFile` (the addon background token!), `OrderIndex`, `ClassMask` | already in-repo; refresh via `https://wago.tools/db2/<Table>/csv?build=2.5.4.44833` |
| **2 — spell mechanics** | wago.tools `Spell` / `SpellName` / `SpellEffect` / `SpellMisc` CSVs, build 2.5.4.44833 | effect/aura types, base points, misc values, `$s1`-formula `Description_lang`/`AuraDescription_lang`, keyed by the exact spell ids the Talent table references. **This is the TBC analog of the 1.12.1 Spell.dbc that was Vanilla's number-of-record.** Classic clients split the spell record into DB2 component tables — "Spell.dbc" = join of these | `https://wago.tools/db2/Spell/csv?build=2.5.4.44833` etc. Pull rows on demand; committing all four whole is optional (they're large) |
| **3 — rendered tooltips** | Wowhead TBC: blob `https://nether.wowhead.com/tbc/data/talents-classic?dv=17&db=1785232218` (strip the `WH.setPageData(...)` wrapper → JSON) + per-spell `https://nether.wowhead.com/tbc/tooltip/spell/<id>` → `{name, icon, tooltip}` | per-rank human-readable talent text + **icon names** (the TBC replacement for `fetch_calc_icons.py`'s Vanilla-only source) | curl with a browser UA; ~1400 tooltip requests for every rank of every talent — scriptable, cache locally under `era-data/_ref/tbc/` |
| **4 — behavior reference** | `github.com/wowsims/tbc`: `ui/core/talents/<class>.ts` (structure cross-check) and `sim/<class>/talents.go` (Go implementations of what each talent actually DOES per rank) | the best "what is the real mechanic" reference when translating a TBC talent into spellmod/aura/proc terms | raw.githubusercontent.com fetches |
| 5 — cross-checks | `oppahansi/nltc` `assets/db/ltc_data.db.sql` (SQLite: vanilla/tbc/wotlk trees + multi-language per-rank tooltips, no Blizzard spell ids); `Vampyr7878/WoW-Talent-Caluclator-TBC` `Data/<Class>.xml` (2019 = original-2.4.3-era text); warcraft.wiki.gg for prose history (TBC effect + what Wrath changed) | independent verification when a value looks suspicious | download once if needed |

**Precedence rule (the TBC analog of "1.12 Spell.dbc beats wowhead classic 1.15"):** the
committed wago 2.5.4 CSVs (structure) + wago Spell tables (magnitudes) are the numbers of
record. Wowhead TBC tooltips are presentation. wowsims is the behavior tiebreaker. When a
wiki disagrees with the wago tables, the tables win. **Daribon has no TBC data** — the
Vanilla importer's source does not extend; the CSV importer below replaces it.

**Note the structural upgrade over Vanilla:** the Vanilla skeleton (Daribon) had no spell ids —
every node's stock analog had to be hand-researched. The TBC `Talent` CSV carries
`SpellRank_0..8` directly, so the skeleton importer can pre-fill each node's per-rank stock
spell ids AND the survival map (Task 3) can be mostly mechanized: for each referenced spell
id, diff wago-2.5.4 `SpellEffect` rows against the live 3.3.5a store (`dump_spell_effects.py`)
— identical rows = `grants:` candidate, diverged = clone/spellmod candidate, absent from
3.3.5a = reconstruction.

## 3. Phase 0 — TBC enablement (its own spec+plan, BEFORE any class)

Infrastructure findings from the 2026-08-25 pipeline audit. Items 1-3 are hard blockers;
the rest are known landmines Phase 0 must disposition. Entry: `/superpowers:brainstorming`.

1. **`era-regen.sh` keying bug (HARD, silent):** `DATA_SQL`/`CUSTOM_SQL` are `declare -A`
   maps keyed by **bare basename** (`mage.yaml`) — `era-data/tbc/mage.yaml` collides with
   `era-data/vanilla/mage.yaml` and one silently overwrites the other's SQL output. The
   keying must include the era dir. Same-shape lists also live in
   `client-patch/merge-into-patch.sh`, `client-addons-src/EraTalents/build-addon.sh` (27
   inline invocations), `EraTalents.toc`, and `era_audit.py DEFAULT_DATASETS` — all double
   to 18 datasets. `tools/test_gen_era_talents.py` parses the bash array, so verify the two
   guard tests still work after the keying change. (Phase 0 is also the natural time to
   single-source these five lists if cheap.)
2. **Spell-id band capacity (HARD):** the auto-passive formula
   `920000 + (nodeId-18000)*8` leaves only node ids 18846-18999 (154 nodes) of clean
   headroom — TBC needs ~550-620. Free space above the band is verified (only three
   non-spell constants ≥933000 exist anywhere: WG MotionMaster point ids). **Recommended
   layout, to be confirmed in the Phase-0 brainstorm:** raise `CUSTOM_PASSIVE_END`
   (933000 → e.g. 950000) and reserve TBC node ids at base **20000** (20000-20999 →
   auto-passives 936000-943999, cleanly above every Vanilla claim and the 932999
   sentinel); TBC hand-helper region 944000+ with per-class slices registered in the
   framework table; TBC pet bands (hunter/warlock `petBuffBase:`) at 945000/946000-ish.
   Do NOT make `custom_passive_id()` era-aware instead — that renumbers every shipped
   Vanilla passive and players' `character_spell` rows store those ids (breaking
   migration). Also note `era_talent PRIMARY KEY (id)` is global across eras — the 20000+
   node base satisfies that too. Cross-era passive/pet-band collisions are NOT linted
   (lint is per-dataset) — Phase 0 should add a cross-dataset collision check to
   `era_audit.py`.
3. **Addon tier depth (HARD):** `client-addons-src/EraTalents/TalentUI.lua` `CHILD_H = 455`
   fits 7 tiers; TBC trees have 9 (`rowY(8) = 524` renders outside the scroll child,
   unreachable, no error). Fix the layout (and derive from the tree data, not a constant).
   Tab backgrounds: take the token straight from the committed `TalentTab` CSV
   `BackgroundFile` column, and verify each token has a 3.3.5a client texture
   (`Interface\TalentFrame\<token>`) — a miss renders blank parchment silently.
4. **`EraHasTalentTrees` allowlist + first-ever managed→managed transition:** the enable
   is one line (`EraTalentIP.cpp:21` → `|| era == ERA_TBC`), but flipping it makes the
   `EraTransition.cpp` managed→managed path (Vanilla↔TBC: `Reset(from)` + `ResetAllSpecs`
   + `Pin`) live for the first time ever, and retires the "native→native TBC↔WotLK no-op"
   branch. Test both directions on the dev box with a real character before any class
   ships. **Deployment note: keep the allowlist Vanilla-only until ALL nine TBC classes
   are authored + verified** (same implemented-era policy as before — TBC chars keep
   native WotLK talents until their trees ship as a set; see
   memory `era-talents-implemented-era-allowlist`).
5. **`ReconcileBaselineSpells` re-triage (~28 branches):** every `era == ERA_VANILLA`
   predicate in `EraTalents.cpp` evaluates false for TBC, and the `era >= ERA_TBC` branch
   currently FORCE-GRANTS baseline rank-1s — correct while TBC is native, wrong once TBC
   owns trees (a TBC char whose TBC *talent* is the gate would get the spell free). Each
   gate row needs a three-way decision (Vanilla-managed / TBC-managed / WotLK-native).
   Same audit for `EraTalentPin.cpp` hardcoded `ERA_VANILLA` gates (line 74 paladin BoK —
   needs a decision; line 277 shaman 2H — stays Vanilla-only, TBC shamans have 2H
   baseline), the `.eratalents doctor` orphan predicates (~15, all "on a non-(Vanilla X)"
   — they'd false-positive on every legitimate TBC era spell), version-swap marker chains
   (a third "TBC <Class>" `ReqAbility1` marker wherever a spell has three versions), and
   the band-gated core patches 0018/0019 (their DUMMY markers are Vanilla-named; decide
   whether the behavior extends to TBC — e.g. ~~Corruption IS instant in TBC, so 0019 stays
   Vanilla-only~~ (**HISTORICAL, WRONG — corrected 2026-09-05, TBC Phase 8: TBC Corruption is a
   2 s cast, so 932930 is granted to every non-WotLK warlock; see the Phase 8 spec §1**), and
   TBC-era Shatter exists as a real TBC talent needing its own look).
6. **Per-era point cap:** `AvailablePoints = level - 9` with no era cap — a level-70 char
   pinned to the Vanilla era would get 61 points into a 51-point tree (IP normally caps
   level per era, but `.ip set` + GM levels can produce the state). Phase 0 should cap
   points per era (Vanilla 51 / TBC 61).
7. **New importer:** `tools/import_tbc_talents.py` (or a `--format csv` mode) reading the
   committed wago CSVs → `era-data/_skeletons/tbc-<class>.skeleton.yaml`, same node-shape
   output as the Daribon importer (tab/tier/col/maxRank/prereq), PLUS per-rank stock spell
   ids from `SpellRank_0..8`, names+icons via the Wowhead tooltip endpoint (cache the
   fetches under `era-data/_ref/tbc/tooltips/`). `prereqPoints = 5 * tier` still holds.
   TDD like every generator change.
8. **`_ref` era-scoping:** `era-data/_ref/<class>-spells.yaml` is era-less and
   Vanilla-verified; TBC datasets must not inherit Vanilla-era family masks for spells
   reworked in 2.x. Decide: extend in place with era-tagged sections, or
   `era-data/_ref/tbc/<class>-spells.yaml` (the `ref:` key resolves relative to the
   dataset dir, so per-era ref files fall out naturally). Also fix the stale id-counter
   comments in `era-data/vanilla/shaman.yaml` (lines ~1353/1355/1895/1960: 932416/17 are
   claimed, DUMMY next-free is 17) — deferred from the 2026-08-25 tidy because YAML
   comments hash into the generation stamp; Phase 0's first regen bumps it anyway.

Phase 0 DoD: 18-dataset regen produces distinct SQL for both eras of one pilot class
(structure only, e.g. a 2-node stub TBC mage), audit covers both eras + cross-era collision
check, addon renders a 9-tier stub tree, both transition directions verified on a dev-box
character, reconcile/doctor/marker triage documented as a decision table, importer merged
with tests. Then delete the stub before the first real class (or grow it into Phase 1 mage).

## 4. Per-class workflow (Phases 1-9, one class per phase — the Vanilla pattern, adapted)

The Vanilla per-class process carries over intact; only the era-specific inputs change.
Full detail lives in the framework doc + the Shaman phase pair; deltas for TBC:

1. **Entry:** `/superpowers:brainstorming` → design spec
   `docs/superpowers/specs/<date>-era-talents-tbc-phase<N>-<class>-content-design.md`.
   Lock the same two decisions (fidelity bar; core-patch policy — target zero). §3 risk
   clusters become three buckets: **new-in-2.0 nodes** (author from scratch — e.g.
   Lightning Overload, Mangle, Shadowstep, The Beast Within, Pain Suppression, Devastate,
   Avenging Wrath, Unstable Affliction, Ice Barrier reshuffles), **retuned nodes**
   (re-derive magnitudes vs 3.3.5a), **removed/moved nodes**. Include the class's
   "TBC-talent-that-became-WotLK-baseline" leak sweep (smaller than Vanilla's but real).
2. **Plan:** `superpowers:writing-plans` → same 10-task backbone. Task 2 uses the new CSV
   importer; Task 3's survival map starts from the mechanized wago-vs-3.3.5a SpellEffect
   diff, then live-verifies (`wgconsole.py` `lookup spell`, `dump_spell_effects.py`) —
   the live 3.3.5a store remains the final word on what the server actually has.
3. **Wiring:** same six sites (now with era-aware keys from Phase 0). SQL migrations named
   `..._era_talent_tbc_<class>_*.sql` to never collide with the Vanilla pair. Plus the **bot
   build orders**: add the class's `("TBC","<CLASS>",0..2)` entries to `BUILDS` in
   `tools/gen_bot_builds.py`, run it, paste the emitted `kAuthoredOrders` table into
   `EraTalentBots.cpp` (§4b lesson 10 — the tool fails on a manifest dataset with no orders,
   so a forgotten class cannot ship silently).
4. **Testing:** `.ip set <char> 8` for era; everything else identical (regen → audit →
   restart → `.eratalents doctor` → headless gate → real-client pass at level 70 with the
   fresh `patch-V.mpq`). Real-client checklists per spell type are unchanged
   (framework doc). Trainer-taught rank chains now extend to 61-70 trained ranks.
5. **Close-out:** verification record `docs/verification/era-talents-tbc-phase<N>-<class>.md`,
   framework registry rows (TBC helper slice + DUMMY markers), memory update, commits on
   `feat/era-talents`. **TBC merged to master as a set on 2026-09-07**, after its own final
   all-class review (the Vanilla review plan
   `docs/superpowers/plans/2026-08-24-era-talents-final-all-class-review-plan.md` is the
   template) AND a regression check that the 9 Vanilla classes still pass their minimal
   re-test checklists (the module's era logic went three-way in Phase 0).

## 4a. Phase status (update as each class closes)

| Phase | Class | Status |
|---|---|---|
| 0 | *(enablement)* | shipped |
| 1 | Paladin | **COMPLETE** — real-client signed off, gen `f606ab37`. 64 nodes 20000-20063; helpers 946000-946140. |
| 2 | Druid | **COMPLETE** — real-client signed off 2026-08-31, gen `05b215b4`. 62 nodes 20100-20161; helpers **946256-946276** (946264 allocated-but-unused, free). Record: `docs/verification/era-talents-tbc-phase2-druid.md`. |
| 3 | Shaman | **COMPLETE** — real-client signed off 2026-09-02, gen `19e3a621` (incl. the tooltip-honesty fix round — see lesson 17 and the record §10-§11). 61 nodes 20200-20260 all wired; helpers **946512-946539** (tree) + **947000-947351** (totems, packed per `id_allocation:` in `era-data/_ref/tbc/shaman-totems-tbc.yaml`) + **947440** marker + **947441** Windfury Weapon r5 — 947442-947511 and 946540-946999 free within the slice. Creatures **920300-920580** (51 fresh entries). Three-marker totem chain (932416/947440/932417) runtime-verified at L48/60/70/80, zero orphans all eras. Record: `docs/verification/era-talents-tbc-phase3-shaman.md`. |
| **4** | **Warrior** | **COMPLETE** — real-client SIGNED OFF 2026-09-03, gen `85d67983`. 66 nodes 20300-20365 all wired; helpers **947512-947567** (56 ids: Bloodthirst 947512-529, Devastate 947530-532, Rampage 947533-540, Shield Slam 947541-546, Last Stand 947547, Death Wish 947548, Sweeping Strikes 947549, Concussion Blow 947550, Deep Wound bleed 947551, Mace Spec payload 947552, Blood Craze 947553-555, Enrage 947556-560, Shield Spec 947561-565 + its rage payload 947566, Shield Bash silence 947567) — **947568-947767 free within slice**. Auto-passives 938400-938927. Four trainer-taught clone chains (14 `trainer_spell` + 18 `spell_ranks` rows, TrainerId 1); **nothing deleted, nothing re-gated** — every stock chain was already self-gating. Reuse ledger ALL FRESH/MOOT. Generator gained `casterAuraSpell` + `dispelType`. **No DUMMY-marker misc consumed** (node 20337 re-uses the existing misc 15). Band-move ladder zero-orphan at L60/70/80/70; all 66 nodes exercised by real bot builds. Record: `docs/verification/era-talents-tbc-phase4-warrior.md`. |
| **5** | **Rogue** | **COMPLETE** — real-client SIGNED OFF 2026-09-03, gen `f5c228df`, zero findings (record §16). 67 nodes 20400-20466 all wired (Assassination 21 / Combat 24 / Subtlety 22); helpers **947768-947787** (20 ids only — a grant-heavy tree: Find Weakness 947768-772, Mutilate visible chain 947773-776, Riposte 947777, Adrenaline Rush 947778, Mace Spec 947779-783 + stun payload 947784, Blade Twisting daze 947785, Preparation 947786, Premeditation 947787) — **947788-948023 free within slice**. Auto-passives 939200-939724 (113 rows). One hand SQL (Mutilate trainer chain, TrainerId 9, L50/60/70; **nothing deleted**), zero core patches. Reuse ledger 0/5. Three module scripts (`era_rog_preparation_tbc`, `era_rog_mutilate_poison`, `era_rog_mutilate_behind`). Generator gained `proc.affectsMask`, per-effect `mechanic:`, node-effect `perLevel:`. **No DUMMY-marker misc consumed.** Band-move ladder zero-orphan at L60/70/80/70. Bundles a cross-era fix: Vanilla Riposte clone **932989**. Record: `docs/verification/era-talents-tbc-phase5-rogue.md`. Real-client checklist = plan Task 10 Step 2 + the record's §15. |
| **6** | **Priest** | **COMPLETE — real-client SIGNED OFF 2026-09-04**, gen `9ad9274e`. 64 nodes 20500-20563 all wired (Discipline 22 / Holy 21 / Shadow 21); helpers **948024-948080** (57 ids: Divine Spirit 948024-028 + Prayer of Spirit 948029-030, VE trio 948031-033, Reflective Shield 948034-038, Power Infusion 948039, Pain Suppression 948040, Martyrdom payloads 948041-042, Focused Will 948043-045, Circle of Healing 948046-050, Lightwell 948051-054, Surge of Light buff 948055, Clearcasting 948056, Inspiration 948057-059, Vampiric Touch 948060-063, Mind Flay 948064-070, Silence 948071, Shadowform 948072, Misery debuffs 948073-077, Shadow Vulnerability 948078, Blackout stun 948079, Spirit Tap buff 948080) — **948081-948279 free within slice**. Auto-passives 940000-940511 (131 rows). 28 of 64 nodes grant: **17 stock, 11 a band id**. Three hand SQL files (21 `trainer_spell` rows on TrainerId 11 + 7 `spell_ranks` chains (6 trainer-taught + the Misery debuff chain 948073-948077, which exists only so `SpellInfo::IsRankOf` prevents cross-rank stacking); **nothing deleted, nothing re-gated**), **zero core patches**. **Two `kBaselineSpellGates` rows split three-way** — Holy Nova 18121 (node-gated strip, druid shape) and Divine Spirit + Prayer of Spirit 18113 (unconditional strip, warrior shape) — plus a respec arm (2b) for the trainer-taught PoS clone chain (lesson 21). Reuse ledger **0/8** (all FRESH); four STOCK payloads reused as-is (33143, 27813/27817/27818, 33619, 34919). Three module scripts (`era_pri_vampiric_touch_tbc`, `era_pri_reflective_shield_tbc`, and `era_vampiric_embrace` made table-driven for both eras). Generator gained `attributesEx7`, `defenseType`, `startRecoveryCategory`, `startRecoveryTime`. **No DUMMY-marker misc consumed** (Improved VE re-uses misc 4). Band-move ladder zero-orphan at L60/70/80/70; 275-bot broad doctor sweep across all ten classes and both bands, zero ORPHAN/PROBLEM. Record: `docs/verification/era-talents-tbc-phase6-priest.md`. Real-client checklist = the record's §15. |
| **7** | **Hunter** | **COMPLETE — real-client SIGNED OFF 2026-09-05**, gen `76709510` (one finding: stock-grant panel tooltips rendered WotLK text — addon fix `10dd211`, cross-class, record §21). 64 nodes 20600-20663 all wired (Beast Mastery 21 / Marksmanship 20 / Survival 23); helpers **948280-948385** (53 ids: TBC Hunter marker 948280 + Scorpid Sting clone 948281; Bestial Wrath 948290, The Beast Within passive/owner-buff 948291/948292, Spirit Bond 119 carriers 948293/948294, Ferocious Inspiration carriers 948295-297 + party buffs 948298-300, Bestial Swiftness owner spellmod 948301, Focused Fire carriers 948302/948303; Wyvern sleep 948330-333 + wake-DoT 948334-337, Counterattack 948338-341, Deterrence 948342, Entrapment root 948343, Imp. Wing Clip root 948344, Expose Weakness payload 948345, Master Tactician buffs 948346-350, Readiness 948351; Aimed Shot 948370-376, Trueshot Aura 948377-380, Imp. Concussive stun 948381, Silencing Shot 948383, Rapid Killing buffs 948384/948385) — **948382 was a deliberate hole, now consumed by the TBC Scatter Shot clone (final review R4)** (it had been the reserved Concussive Barrage daze slot: stock 35101 can be triggered as-is, so nothing was minted then and the Task-5-pinned 948383 kept its id). **Free within slice: 948282-948289, 948304-948329, 948352-948369, 948386-948409.** Auto-passives 940800-941311 (118 rows). Node dispositions from the shipped dataset: **18 GRANT stock / 35 AUTHOR auto-passive (6 of them full RECONSTRUCTIONs) / 11 CLONE** (Scatter Shot moved GRANT -> CLONE at final-review R4). Three hand SQL files (16 `trainer_spell` rows on TrainerId 7 + 5 `spell_ranks` chains; **nothing deleted, nothing re-gated**), **zero core patches**. **No `kBaselineSpellGates` hunter row.** The class's version-swap marker became a real THREE-way chain (Vanilla 932304 / **TBC 948280** / WotLK 932305), and Deterrence turned out to be a TBC TALENT (node 20650, clone 948342). Reuse ledger **0/22** (all FRESH or MOOT) — including the first genuine identity hit in any TBC phase (Counterattack 932979/932958/932959 are field-identical to 19306/20909/20910 on all fourteen families, and were still declined). Three NEW module scripts (`era_hun_beast_within_tbc`, `era_hun_expose_weakness_tbc`, `era_hun_readiness_tbc`) plus `era_wyvern_sting` made table-driven for both eras. Generator gained `attributesEx4` (deliberately NOT added to the audit's `_SPELLFIX_KEYABLE` set). **`petBuffBase: 944000` claimed but UNUSED** (every pet node needed a hand `petBuffIds` carrier). **No DUMMY-marker misc consumed** (spec §4e's foreseen Improved-Aspect-of-the-Hawk gate turned out not to exist). 12 accepted gaps, of which 2 and 5 are CLOSED for TBC. **Bundled Vanilla fixes:** 932966 `targetA: 6`; the Aimed Shot identity back-port (932952-957 -> family 9 + word2 bit 21, Efficiency 18318's mask gains the bit) which **RETIRES Vanilla record gap 4**; Trueshot 932962/932963 re-priced 1800/2500. **Cross-class:** bots now reconcile in UNMANAGED bands too (`e1e7308`). Band-move ladder zero-orphan at L60/70/80/70 with a live pet; 277-bot broad doctor sweep across all ten classes and both bands, zero ORPHAN/PROBLEM. Record: `docs/verification/era-talents-tbc-phase7-hunter.md`. Real-client checklist = the record's §19. |
| **8** | **Warlock** | **COMPLETE — real-client SIGNED OFF 2026-09-05**, gen `4ec433f1`, zero findings. 64 nodes **20700-20763** all wired (Affliction 21 / Demonology 22 / Destruction 21); helpers **948410-948749** partitioned by tab, **65 ids used** — substrate `948410`, `948420-948428`, `948430-948433`; Affliction `948450-948458`, `948460-948464`, `948470-948472`; Demonology `948550-948557`, `948560-948564`, `948570-948573`; Destruction `948650-948655`, `948660-948667`, `948670-948672` (holes 948459 / 948558-948559 / 948668-948669 plus the tail of each partition — full list in the framework registry). Auto-passives **941600-942100** (112 rows). Dispositions **24 GRANT / 33 AUTHOR / 6 CLONE / 1 RECONSTRUCT**; **zero core patches** (patch 0019 reused unchanged). **Four-marker chain** — 932930 cast-time (client label "Pre-Wrath Warlock"; **no longer a trainer gate**), **932994** Vanilla stone gate (NEW), **948410** TBC stone gate, 932939 narrowed to WotLK — and **every arm rides the same `EraReplacementSpellReady` fallback** (Task 6 fix: the 932930 arm did not, which would have armed a 2 s Corruption for a TBC warlock on a build without the TBC allowlist; lesson 25). Three Create-stone sets, each era stripping the other two; Master Firestone 22128 + Master Spellstone 22646 reconstructed onto 948430-948433. Four TBC trained CLONE chains + a 7-row stock-swap list; Shadowburn and Dark Pact GRANT stock; **nothing deleted** (the Vanilla stone rows are UPDATEd in place). New module script `era_spellstone_crit_rating`; five existing scripts made era-table-driven. **Bundled Vanilla fixes:** ISB `procCharges: 4`, `manaCostPct: 0` on Conflagrate 932780-783 + Soul Link 932900, historic orphan strip 921745/921754/921788 (verified GONE at the L80 rung on the very bot that carried them). **`petBuffBase: 945000` claimed and EMPTY.** **No DUMMY-marker misc consumed** (next free 19). Band-move ladder zero-orphan at L70/60/70/reset/80/70 with a live **Felguard** out (Soul Link pair + the Master Demonologist Felguard carrier 948564 both live on the demon); 500-bot broad doctor sweep across all ten classes and all three bands — **warlock 0 ORPHAN / 0 PROBLEM on all 50 warlock bots**; the sweep's other 37 `ORPHAN ERA SPELL` lines are 4 non-warlock L80 bots (paladin `Patrea` 17, hunter `Xonoth` 10 / `Brudugs` 1, mage `Rozki` 9), all with ZERO `era_character_talent` rows — the **pre-existing** historic-residue shape the hunter record already diagnosed (`StripOrphanedGrants` is scoped to `NodesFor(currentEra)`, empty for WotLK), now ESCALATED because a generic "strip any band spell no era's spent nodes grant" sweep is a design change, not a phase fix. Ten `CLASS_WARLOCK` resolver-sanity rows added to the doctor at Task 6 (spec 5 required them; Task 5 shipped none) — **no `kEraNameAliases` row needed, every clone keeps its stock name**, and the single warlock by-id AI site (`WarlockTriggers.cpp:176` `{"felguard", 30146, 17252}`) keys on a GRANTed stock id, so **patch 0021 needs no warlock arm**. Record: `docs/verification/era-talents-tbc-phase8-warlock.md`. Real-client checklist = the record's 15. Spec: `docs/superpowers/specs/2026-09-05-era-talents-tbc-phase8-warlock-content-design.md`. |
| **9** | **Mage** | **COMPLETE — real-client SIGNED OFF 2026-09-05 (zero findings)**, gen `1de3388d`. 67 nodes **20800-20866** all wired (Arcane 23 / Fire 22 / Frost 22); dispositions **20 GRANT / 38 AUTHOR / 9 CLONE / 0 RECONSTRUCT** (291 ids swept; the 193 rank spells 101 DIVERGED / 63 LIVE_MATCH / 23 WAGO_ONLY / 6 LIVE_MATCH_UNVERIFIED_SLOT). Helpers **948750-949059**, **47 ids used**: substrate **948750-948759 EMPTY by design** (the tree mints no marker — every trained chain gates on its own r1 clone id); Arcane **948760-948773** (AP 948760 / PoM 948761 — no cross-lock, TBC allowed both; Magic Absorption r1-r5 948762-948766; Arcane Potency crit buffs 948767-948769; silence 948770; Blink buffs 948771-948772; Slow 948773); Fire **948860** + **948870-948879** Pyroblast r1-r10 + **948880-948886** Blast Wave r1-r7 + **948887** Combustion + **948890-948893** Dragon's Breath r1-r4 (**holes 948861-948869 / 948888-948889 are INTENTIONAL** decade-boundary readability, do not renumber); Frost **948960-948969** (Winter's Chill 948960, Blizzard chill 948961-948963, Ice Barrier r1-r6 948964-948969). Auto-passives **942400-942935** (Shatter 942856-942860). **Seven trainer-taught CLONE chains across BOTH eras** + two node-granted Magic Absorption chains = **37 TrainerId 16 rows and nine `spell_ranks` chains** in one hand SQL (`2026_09_08_02`), **nothing deleted from stock rows**; the orphaned-higher-rank check's four hits (roots 133/168/44425/44457) are pre-existing stock rows. **ONE core touch: patch 0018's band widened to `[920000, 950000)`** so the TBC Shatter node's auto-passives arm it — no new marker, **misc 1 is now the first cross-era MECHANIC flag**; `tools/regen-shatter-patch.sh` gained **0025 as a second contaminator** (reverse 0025 then 0012; re-apply 0012 then 0025) and the pristine reset must name **`Unit.h`** too (0012 spans both files, `git apply` is atomic). One new module script `era_mag_ice_barrier_tbc` (TBC's 0.1 SP coefficient vs live's hardcoded 0.8068 — cmangos `mangos-tbc` `sql/base/mangos.sql:14318`). **Bundled Vanilla re-audit: 35 CLEAN / 14 FIX, all fourteen shipped** (18005/18008/18011/18022/18023/18024/18026/18029/18030/18032/18036/18039/18048/18049) — incl. four whole Vanilla clone chains (932310-932317 Pyroblast 6 s, 932320-932324 Blast Wave no-knockback, 932325 Combustion 3-min, 932331-932334 Ice Barrier base-only), the Magic Absorption rider (932326-932330), payload clones 932762-932764, and the stranded 920040-920044 cleanup. **Headline of the re-audit:** Vanilla mage was the only dataset in the repo using `op: crit, kind: pct`, which is inert (`ApplySpellMod` skips PCT at base 0) — four of the fourteen rows are that one defect. **No DUMMY-marker misc consumed** (next free 19). Headless gate: 5-rung ladder L60/70/reset/80/70 **zero orphans**, plus a **Frost** rung proving the Shatter invariant `M == (R ? 1 : 0)` (rank 5 → misc-1 auras held **1**, marker 942860 amount 50) and the Ice Barrier clone chain at 6 ranks with its era script bound; **500-bot broad doctor sweep across all ten classes and all three bands — 0 PROBLEM anywhere, and every one of the 41 Vanilla-band + TBC-band mage bots 0 ORPHAN**. The sweep's 19 remaining orphan lines are three L80 WotLK-band bots (`Rozki` 8, `Xonoth` 10, `Brudugs` 1) — pre-existing residue, Phase 9.5's job; **`Rozki` lost 920044**, this phase's stranded-id strip observed live, and `Patrea` is now clean because the level-bracket module moved it back into a managed band (the teardown-first reconcile self-heals). **Zero bot-AI by-id hits** — the mage AI resolves by name and every clone keeps its stock name, so **no `kEraNameAliases` row and no patch-0021 mage arm**. Record: `docs/verification/era-talents-tbc-phase9-mage.md`. Real-client checklist = the record's §14 (TBC L70 items 1-12 + Vanilla L60 items 13-23). Spec: `docs/superpowers/specs/2026-09-05-era-talents-tbc-phase9-mage-content-design.md` (+ Amendments A-E). |
| **9.5** | *(generic band-orphan sweep)* | **COMPLETE (headless) 2026-09-06** — generation `1de3388d` unchanged (Phase 9.5 mints NO ids; next free helper stays 949060, next free misc stays 19). `StripOrphanedGrants` and **every** per-class cross-era strip list are retired in favour of `EraBandClassifier` (`src/EraBandClassifier.{h,cpp}`): three legitimacy sources in order — (1) node grant at the character's spent rank via a reverse index over EVERY era/class, (2) `spell_ranks` rank above a node-granted r1, (3) an `era-data/band-allowlist.yaml` entry matching era + class + optional gating node — with `Sweep` running after every strip arm in `ReconcileBaselineSpells` (the priest tail after it is grant-only) for **every** era, WotLK included (the gap `StripOrphanedGrants` structurally could not close). `.eratalents doctor`'s ORPHAN loop and the new `.eratalents bandsweep <char>` call the same `Classify`. Allowlist = **83 entries**, sha `8b13166aba08`, generated to `src/EraBandAllowlist.gen.h` by `era-regen.sh` and guarded by `era_audit.py check_band_ownership`; boot canary `band classifier: 2562 node-grant ids indexed, 83 allowlist entries (allowlist sha 8b13166aba08)`. **Residue bots before → after:** `Patrea` 17 → **0** (no band rows), `Xonoth` 10 → **0** (keeps only the allowlisted WotLK hunter marker 932305), `Brudugs` 1 → **0** (same marker), `Rozki` 8 → **0**; `.eratalents bandsweep` on all four returns `0 stripped` (idempotent). **Broad sweep: 500 online bots ≤ 80, all ten classes, all three bands (TBC 48 / Vanilla 222 / WotLK 230) — 0 `ORPHAN ERA SPELL`, 0 `<-- PROBLEM`, 0 `UNCLASSIFIED`** (the warlock record's 37 and the mage record's 19 residual lines are gone and nothing new appeared). **Three ladders** L60/70/reset/80/70: **mage** `Caeuun` (Frost) 0 orphans — TBC Pyroblast r10 / Blast Wave r7 / Dragon's Breath r4 all held above their node-granted r1 (rule 2), no 948xxx at L60 or L80; **druid** `Kichme` 0 orphans — 946261/946265/946280 stripped exactly when nodes 20141/20161/20135 sit at rank 0 (rule 3), Enrage 932513 legit at L60+L70 and stripped at L80; **paladin** `Sathelon` clean on every dual-era / seal / LoH / Judgement check (932617 known iff the era's node is spent, the Vanilla and TBC seal chains each stripped on the hop out of their era, exactly one faction seal 946080, LoH 932668-670 + Judgement 932746 gone at L80) **but 2 lines on `946136`** — see the escalation. Second boot (500 bots re-logged, window anchored on the container's `StartedAt` — a `--since 6m` window silently reaches back into the ladders and over-counts): **2** strip lines total, one a correct rule-3 gate (`948032` on a TBC priest whose node 20555 is at rank 0) and one the same 946136. **ESCALATED (not a Phase 9.5 defect, own fix round): TBC paladin Blessing of Sanctuary r5 `946136`** — (a) its `trainer_spell` rows gate on the era marker 946078 instead of the previous rank 932664, so `PlayerbotFactory` teaches r5 to any TBC paladin ≥70 without node 20033; (b) its `spell_ranks` row `(932617, 946136, 5)` is missing from the live world DB because `2026_08_19_30_..._active_rank_trainers.sql` (whose `DELETE` covers `first_spell_id=932617`) was **re-applied 2026-09-04, after** `2026_08_30_10_..._bos_r5.sql` was applied 2026-08-30 — a filename-order-vs-application-order trap that only bites a DB where the earlier-dated file is edited again (a fresh import is correct); a full authored-vs-live diff of all 445 module `spell_ranks` tuples found **exactly this one row missing**. Record: `docs/verification/era-talents-phase9-5-band-sweep.md` (real-client checklist = its §6). Spec: `docs/superpowers/specs/2026-09-05-era-talents-phase9-5-band-orphan-sweep-design.md`. **NEXT = the TBC final all-class review**, then the Vanilla regression, then merge. |
| **Final review** | *(TBC + Vanilla + bots)* | **FIX ROUNDS COMPLETE 2026-09-06** — 7 Critical / 34 Important fixed across 11 rounds; record `docs/verification/era-talents-final-review-tbc-vanilla.md`; closing generation: see its §10 closing row |

Next free TBC helper id: **949060** — the MAGE slice `948750-949059` is claimed and **CLOSED at 47 used ids** (Phase 9, verified 2026-09-05: substrate 948750-948759 **EMPTY by design**, Arcane 948760-948773, Fire 948860 + 948870-948887 + 948890-948893, Frost 948960-948969; free holes *within* it: 948750-948759, 948774-948859, 948861-948869, 948888-948889, 948894-948959, 948970-949059 — the two Fire holes are INTENTIONAL decade boundaries, do not renumber). The warlock slice `948410-948749` is claimed (partitioned by
tab: substrate 948410-948449, Affliction 948450-948549, Demonology 948550-948649, Destruction
948650-948749; free holes *within* it are listed in the Phase 8 verification record at close-out).
The hunter slice `948280-948409` is claimed, with `948282-948289`, `948304-948329`, `948352-948369`
and `948386-948409` free *within* it (do not take ids from another class's slice; the next class
phase, **MAGE**, claims its own sub-range from **948750**). Priest reserved 948024-948279
(948081-948279 free within it); rogue reserved
947768-948023 (947788-948023 free within it); warrior reserved 947512-947767 (947568-947767 free
within it); shaman reserved 946512-947511 (947442-947511 + 946540-946999 free within it); druid's
free-within-slice remainder is 946281-946511.
Era-1 pet window `[944000, 946000)`: BOTH 1000-wide bases are now claimed — hunter 944000 (Phase 7)
and warlock 945000 (Phase 8) — and **neither emits a single row**. Every pet node in both classes
either needed a hand `petBuffIds` carrier or turned out to be an owner spellmod onto a stock
LIVE_MATCH carrier. A future era-1 pet class therefore needs a base allocated in the framework
registry rather than taken from this window; mage (Phase 9) is not a pet class in the relevant sense
and is expected to declare none.
Next free DUMMY-marker misc: **19** (Phase 9 consumed none — the TBC Shatter node re-uses the existing misc **1**, now a cross-era MECHANIC flag; **Phase 9.5 consumed none either — it mints no ids at all**). Next phase: **the TBC final all-class review** (Phase 9.5 closed headless 2026-09-06), then the Vanilla regression, then merge.

## 4b. Hard-won lessons — apply these from Phase 3 onward

Each of these cost real debugging in Phase 1 or 2. They are cheap to honor up front.

1. **The survival map MUST include a non-effect field sweep.** An `Effect`/`EffectAura`/
   `EffectBasePoints`/`EffectMiscValue` diff is NOT sufficient. Phase 2's effect-only first pass
   called Nature's Grace "clean" while stock 16880 procs at **33%** on this core versus TBC's
   **100%** — the node is maxRank 1, so granting stock would have shipped one third of the effect.
   Also sweep **`ProcChance`, `ProcCharges`, `ProcTypeMask`, `DurationIndex`, and `ManaCost`**
   (wago `SpellAuraOptions` / `SpellMisc` / `SpellPower`). Phase 2 found **nine** real divergences
   this way, including TBC Nature's Grasp being free to cast (no `SpellPower` row at all) and TBC
   Omen of Clarity being a 30-minute melee-only buff.
2. **Chase triggered and form-implied spells, not just the talent row.** The `Talent` CSV's
   `SpellRank_*` ids are only the entry points. Nature's Grace 16880 reads clean while its buff
   16886 is the divergence; Leader of the Pack 17007 reads clean while its crit aura 24932 is the
   divergence; both druid form auras (24907, 34123) are core hardcasts no talent row mentions.
3. **Never copy a TBC effect encoding literally without checking it still works in 3.3.5a.** TBC
   stored Mangle's bleed amplification as `SPELL_AURA_DUMMY`; a bare DUMMY does nothing on this
   core, so a literal copy would have shipped Mangle with no bleed amplification. The +30%
   magnitude was identical in both eras — only the *encoding* changed. Keep the live carrier and
   author the values.
4. **Before any `DELETE FROM trainer_spell`, run the orphaned-higher-rank check** in
   `docs/era-talents-framework.md`. Phase 2 shipped a deletion that permanently locked **native
   WotLK** druids out of Mangle r4/r5. Prefer removing an unnecessary DELETE over adding a
   compensating grant — the stock chain is usually already self-gating.
5. **Icons: the Wowhead TBC endpoint serves Classic-2019 texture names for some spells.** Anything
   prefixed `classic_` renders BLANK in a 3.3.5a client with no error. The importer now strips the
   prefix and `era_audit.py` fails on it, but check the class's icons in the real-client pass —
   a blank button is invisible headlessly. (This has now happened twice: Vanilla shaman, TBC druid.)
6. **A core script bound BY ID to a stock spell is dead on a clone.** The on-crit heal script
   `spell_dru_leader_of_the_pack` is bound to stock 24932, so the clone needed an explicit
   `scriptBindings:` entry. Whenever you clone a spell the core scripts, check
   `spell_script_names` and re-bind.
7. **A stock spell's Description may hardcode another spell's id** (`$34123s1`). The client then
   renders the WotLK value no matter what you author. If the tooltip text matters, the form/spell
   itself must be cloned — you cannot fix it from the talent side.
8. **`era_talent_rank`'s PK is `(talentId, rank)` — one spell per rank.** A talent that grants two
   spells at one rank (TBC druid Mangle: Bear + Cat; Blood Frenzy/Primal Fury) cannot be expressed
   in the dataset. Phase 2 handled it with a table-driven reconcile arm (`kEraDruidTbcPaired` in
   `EraTalents.cpp`, `custom:` flag controls cross-era stripping). **Reuse that arm's shape**; it
   grows per class, and the alternative is migrating a shipped table's primary key.
9. **Verification surfaces that do NOT work on the dev box** — plan around them rather than
   rediscovering them: bots never shapeshift (so no form-passive check), bot spellbooks bypass
   trainers entirely (so they cannot answer a trainer-gate question), and reconcile is
   event-driven (an online bot will not pick up new content until a login/level/learn/reset).
   Verify on a bot that *actually reconciled*, and hand form/proc/tooltip checks to the
   real-client pass.
10. **RESOLVED 2026-09-03 — bot build orders are now per-class work.** `tools/gen_bot_builds.py`
    used to read only `era-data/vanilla`, so `kAuthoredOrders` (`EraTalentBots.cpp`) was keyed
    `(class, specTab)` with Vanilla node ids and every TBC-band bot build missed the table, fell
    through to the greedy tier-fill (keystone-starving) and logged one drift WARN per entry. The
    tool now reads the `era-data/datasets.txt` manifest, the table is keyed `(era, class,
    specTab)` (`std::tuple<uint8, uint8, int>`, `ERA_*` symbols), each order must cover the era's
    full `AvailablePoints` budget (Vanilla 51 / TBC 61) with the requested tab as the max tab,
    and **coverage is enforced: a dataset in the manifest with no authored order for specTabs
    0-2 (druid 0-3) fails the tool.** So step 3 (wiring) of every remaining TBC phase now
    includes: author the class's three `("TBC","<CLASS>",n)` orders in `BUILDS`, run the tool
    until it validates, paste the emitted table over `kAuthoredOrders` verbatim (never hand-edit
    entries). The five shipped TBC classes (paladin, druid, shaman, warrior, rogue) were authored
    in the fix commit.

11. **Five field families have now each been found OUTSIDE a survival sweep, one per review round**
    (TBC shaman, 2026-08/09): non-effect fields; **triggered chains**; **attributes** (all eight
    words); **class-mask word 0**; and **`SpellPower`/mana**. Each was a category nobody had looked
    at rather than an error inside what had been. Two of them inverted a headline conclusion — the
    attribute sweep flipped all four reuse candidates to FRESH, and the mana sweep inverted an
    entire 28-family reuse ledger. **Sweep all five from the first pass, and when a sweep reports
    "everything matches", suspect its column list before believing it.**
12. **Read the PRIOR ERA's `_ref` file before sweeping.** Two of the TBC shaman phase's four reuse
    candidates were sitting in the Vanilla phase's own reference file, already diagnosed, in a file
    the TBC pass never opened. A fresh sweep cannot find a rework whose talent row is byte-identical;
    the prior era's notes can.
13. **Never write "not knowable" about a value that is in the wago CSVs.** Done three times in the
    TBC shaman phase. It is worse than a wrong value, because it tells the next reader not to check.
    If something genuinely cannot be determined, name the evidence that would settle it.

**Suggested class order:** lead with the classes whose 2.0 tree changes were largest so the
hardest reconstruction problems surface early — a reasonable order is
**paladin (Crusader Strike/Avenging Wrath + seal landscape), druid (Mangle/Lifebloom),
shaman (Lightning Overload/totem retunes — totem TBC magnitudes need a reuse-vs-retune
decision against the Vanilla `931000-931399` reconstructions), warrior (Devastate/Rampage),
rogue (Mutilate/Shadowstep), priest (Pain Suppression/CoH), hunter (TBW/Readiness),
warlock (UA), mage** — confirm in each phase's brainstorm; nothing depends on the order
beyond morale.

14. **A clone never receives `SpellInfoCorrections.cpp`'s by-id fixes** — the core patches stock
    `SpellInfo` objects by spell id, and a clone has a different id. Repo-wide this silently degraded
    ~49 clones across 7 datasets. `era_audit.py::check_spellfix_inheritance` now enforces it; clear a
    finding by authoring the post-fix value (`attributes`, `attributesEx`, `attributesEx2/3/5/7`) or by
    `spellFixAck:`. **Direction is a property of the FIX, not of the key** — some corrections `|=` a
    bit and others `&= ~` one, and every key authors the FINAL value.

15. **Correct a wrong RATIONALE as rigorously as a wrong value.** Phase 3 shipped correct data with
    wrong reasons three times, and each survived review because the *decision* looked right. A wrong
    reason gets quoted into later decisions and nothing fails when it is wrong. Never write
    "code-verified" or "provably inert" without naming the spell id you inspected — and confirm it is
    the one the ERA character receives, not the live stock id sharing its name.

16. **A retraction is not done until you sweep for the claim's FINGERPRINTS**, not its conclusion —
    spell ids, distinctive phrases — and the sweep must include `docs/superpowers/plans/` and
    `specs/`, because the copy that gives an *instruction* is the dangerous one. **Sweep for the
    claim, not for the commit that repeated it:** one claim was corrected in three places while the
    ORIGIN copy, in an earlier batch's file, stayed standing. Corollary: **when a finding says two
    sources disagree, print BOTH raw values** — a fabricated wago-vs-live disagreement cost a false
    open question and a false precedent before a second reviewer measured both sides.

17. **The shaman fix-round finding CLASSES are standing sweep items, not discoveries** (Phase 3
    §10-§11, 2026-09-02). Check each of these DURING authoring, per class: (a) proc-role spells
    ABSENT from ip-dbc behind an enchant/trigger chain — the applier works, the payload silently
    doesn't (Flametongue's 8253-series; the fix shape is a server-side enchant re-point via the
    `spellitemenchantment_dbc` overlay + band DUMMYs + an era script); (b) `PreventionType` —
    never in any sweep column list, and every totem-style summon diverged (`preventionType`
    generator key exists now); (c) inherited `AttributesEx3/5` bits a template rides in (stun/LoS/
    aggro semantics — the keys exist, author the era value); (d) a GRANT node's in-client tooltip
    is the granted spell's CLIENT-DBC description, so a clone that replaces a stock grant MUST
    author `client.description` or the panel goes blank (and a stock grant shows WotLK's prose);
    (e) a stock trainer row two eras share cannot OR-gate — complete the per-era clone pattern
    instead of leaving one era buy-then-strip (Sentry); (f) swapping a node's grant strands the OLD
    grant id on built characters — `StripOrphanedGrants` only sees CURRENT node-table ids, so add
    the retired stock id to the era swap lists; (g) REBINDING a band spell's script needs the
    generator's bare per-id DELETE semantics (in place since 2026-09-02) — before it, the old
    script's row leaked and both scripts attached.

18. **Sweep the `spell_proc` / `spell_proc_event` DB rows, not just the DBC** (Phase 5, spec
    Amendment **A.9**). A NEGATIVE-id row in `acore_world.spell_proc` / `spell_proc_event`
    (`-13960`, `-31244`, `-14186`, …) OVERRIDES the DBC's `ProcTypeMask`/`ProcChance` for that
    spell, so a fourteen-family DBC sweep can call a proc talent LIVE_MATCH while the server
    actually runs a different proc contract — or, in the other direction, can call it DIVERGED
    when a `spell_proc_event` row has already neutralised the difference. The rogue phase built a
    **`proc_data_census:`** (13 entries) alongside the script census and it changed three verdicts:
    Combat Potency's single `ProcTypeMask` divergence is already neutralised by its own `-35541`
    row (so grant stock, script kept), while Sword Specialization's `-13960` row is **all zeros**,
    which a CLONE does not inherit — an authored/cloned proc talent must have its proc row written
    by the generator or it silently never fires. Do this census per class, in the same task as the
    `spell_script_names` census; a granted-stock proc keeps its row for free, a clone never does.
19. **`EffectSpellClassMask{A,B,C}_{1,2,3}` is `{effect}_{word}`, not `{word}_{effect}`** (Phase 5,
    spec Amendment **A.12**). The LETTER is the effect index (A = Effect_1, B = Effect_2,
    C = Effect_3) and the numeric suffix is the family mask WORD (1 = word0, 2 = word1,
    3 = word2). Reading it the other way round transposes every multi-effect spellmod comparison
    and manufactures divergences that are not there (and hides real ones). The same convention
    governs the generator's per-effect `maskA`/`maskB`/`maskC` keys and `affectsMask {a,b,c}`.
    Note this is DISTINCT from the helper-level `mask`/`maskB`/`maskC` scalars, which set the
    spell's OWN identity flags (`SpellClassMask_1/2/3`).
20. **A capstone that reads LIVE_MATCH on all fourteen families can still be era-wrong in CORE
    CODE** (Phase 5, Amendment **A.14**; the same class as core patch 0023's Heart of the Wild).
    Mutilate's "+20% damage vs a poisoned target" is not in any DBC column — it is hardcoded in
    `SpellEffects.cpp` behind `m_spellInfo->SpellFamilyFlags[1] & 0x6` (the hidden hand-halves
    5374/27576/34414-34419's own family bits, NOT the visible cast's id), along with the *absence*
    of TBC's behind-the-target requirement — so an effect-diff pipeline can never see that TBC's
    number is **+50%**. Standing check, per class: before calling a capstone or signature ability
    "identical", **LEAD with a family-flag grep** — grep `src/server/game/Spells/SpellEffects.cpp`,
    `Entities/Unit/Unit.cpp` and `Auras/SpellAuraEffects.cpp` for the class's `SPELLFAMILY_<CLASS>`
    arms and `SpellFamilyFlags[n] & <identity bits of the capstone and its triggered spells>` —
    THEN make `Id == <stockId>` the secondary check. **An id grep alone MISSED Mutilate — the
    branch keys on the hand-halves' family bits, not `Id == 1329`.** A hit on either means the
    magnitude lives in code, and the disposition is a clone + an era script (or a core patch),
    never a grant.

21. **`spell_required` is a NO-OP for a clone chain, and a chain rooted at a TRAINER-taught id is
    invisible to every automatic strip** (Phase 6, priest node 20513, arm 2b). The Divine Spirit
    clone chain is node-granted at r1 (948024), so a respec is already covered twice over:
    `Player::removeSpell` cascades r1 -> r5 through `spell_ranks` (`GetNextSpellInChain`), and
    `StripOrphanedGrants` sees the node grant. **Prayer of Spirit is not.** It roots at its own
    `spell_ranks` chain (948029 -> 948030), is TRAINER-taught, and is never a node grant — so
    nothing removes it on respec. The stock analog would be handled by `spell_required`
    (27681 -> 14752), but that path is dead for a clone: `Player.cpp` only walks `spell_required`
    when `GetTalentSpellCost(firstRank) > 0`, and `DBCStores` returns **0 for any id outside
    `Talent.dbc`** — which every era band id is, by construction. The result was a priest with a
    full Prayer of Spirit sitting in the spellbook and no Divine Spirit talent, and nothing in the
    audit, the boot log or the band-move ladder sees it, because the rows are legitimate
    `character_spell` entries for legitimate era spells.
    **Standing rule, per CHAIN not per class: any era clone chain whose ROOT is trainer-taught
    rather than node-granted needs an explicit strip arm keyed on `CurrentRank(node) == 0`** —
    and, separately, a cross-era leftover strip outside the era block, because
    `StripOrphanedGrants` will not catch it on a band move either (the priest block carries both:
    the `era != ERA_TBC` strip above the class blocks, and arm 2b inside). List the WHOLE chain in
    the strip array, not just the un-cascaded tail — it is idempotent and it also cleans an
    era-transition leftover. The tell is a `.eratalents doctor` run **after a respec**, not after a
    band move: the band move strips the entire managed band and passes either way.

22. **Pre-partition a class's helper slice BY TAB at spec time** (Phase 7). The hunter spec split
    `948280-948409` into substrate `948280-948289` / Beast Mastery `948290-948329` / Survival
    `948330-948369` / Marksmanship `948370-948409` **before any tab task ran**, so the three tab
    tasks could allocate independently and could not collide. The priest phase, which allocated
    strictly in order across tasks, needed a post-hoc id-correction amendment when a later task's
    count grew; the hunter phase needed none. The cost is a few unused ids per partition, which is
    free — the slice is 130 wide. **Corollary: leave a HOLE rather than renumber a pinned id.**
    When Marksmanship's Concussive Barrage daze slot turned out not to be needed (stock 35101 can
    be triggered as-is), 948382 was simply left empty because 948383 had already been pinned to
    Silencing Shot in an earlier task's plan and in the doctor predicates. Renumbering would have
    touched the dataset, the SQL, the reconcile tables, the doctor and the record for zero gain.
    Record the hole explicitly in the id table and the registry so a later reader does not "fix" it.
    (948382 is no longer a hole — the final review's R4 hunter round **consumed it for the TBC
    Scatter Shot clone**; the lesson stands, the example is now historical.)

23. **A talent whose stock rank names its payload BY ID cannot be "grant the talent, clone the
    payload."** (Phase 7, Rapid Killing, spec A.18.) Amendment A.8 originally decided to GRANT
    stock Rapid Killing 34948/34949 and CLONE its triggered buffs 35098/35099 — the buffs being
    what actually carries the Aimed Shot mask. That is self-contradictory: the granted stock talent
    carries `EffectTriggerSpell -> 35098/35099`, so it triggers the STOCK buffs forever and can
    never reach the clones. The talent has to be AUTHORED (a `procPassive` spellmod node whose own
    trigger names the clone) or the clone has to be dropped. **The check, before writing any
    "grant the talent, clone its payload" line: does the STOCK talent rank reference the payload by
    id?** If yes, the pair is inseparable — either both stock or both authored. (The inverse case
    is fine and common: a talent whose payload is reached by MASK can grant stock while the payload
    is cloned, because the mask follows the identity, not the id.)

24. **Bots in an UNMANAGED band never reconciled at all** (Phase 7, commit `e1e7308`). Three
    independent gates all bailed on `!EraHasTalentTrees(era)` — `EraTalentBots::OnBotLogin`,
    `FactoryReconcile`, and `BotBuildScope`'s end-of-build reconcile — and `TeardownStale` resets
    `era_character_talent` **ranks** only, touching neither markers nor trainer-taught era spells.
    So **any marker or trainer-taught era spell survived a bot band move indefinitely**: a hunter
    bot moved 70 -> 80 kept the TBC marker 948280 and the TBC Scorpid clone 948281 forever
    (measured). Bots now reconcile like players in **every** band — `OnBotLogin` unconditionally,
    `OnBotLevelChanged` when the band is unmanaged (managed bands are already covered by
    `SpendBuild`), placed before the early return so both exits are covered. **Two consequences to
    expect when you make this kind of change:** (a) WotLK-band bots begin receiving things they
    never got — the `kBaselineSpellGates` `era >= ERA_TBC` force-grants and the WotLK presence
    markers — which is the CORRECT state and is what a real WotLK player already gets, but it is a
    real behaviour change and belongs in the record; (b) routing the level hook into reconcile put
    the re-entrancy guard and the build-scope state on **map-update threads**, so both had to become
    `thread_local`. The tell for the original defect is a band-move ladder whose L80 rung still
    prints a band row — which the doctor shows and nothing else does.


25. **A marker that arms a core patch must not double as a trainer gate — and every arm of a
    marker table takes the same readiness fallback** (Phase 8, warlock). `932930` did two jobs at once:
    it armed core patch 0019 (Corruption's 2 s cast) AND it was the `ReqAbility1` gate on the Vanilla
    Create-stone trainer rows. That was fine while exactly one era wanted the 2 s cast. The moment TBC
    needed the FIRST job too — TBC Corruption is also a 2 s cast — the marker could no longer separate
    the two eras for the SECOND job, and the trainer gate had to move out into two new markers
    (`932994` Vanilla / `948410` TBC) while `932930` widened to "non-WotLK". **Design rule: a
    patch-arming marker is a MECHANIC flag and may be shared across eras; a trainer gate is an
    IDENTITY flag and must be per-era. Never let one id be both.**
    The second half is the trap that survived into Task 5: with three era arms in one marker table,
    the two NEW ones were readiness-gated (`EraReplacementSpellReady`, the hunter-948280 precedent) and
    the OLD one was left as a bare `era != ERA_WOTLK`. On a build where TBC is outside the
    `EraHasTalentTrees` allowlist — the COMMITTED state — that lands a TBC warlock in a half-armed
    branch: it keeps `932939` and the stock stones (correct) but ALSO gets `932930`, arming a 2 s
    Corruption while it is playing the NATIVE WotLK tree, whose Improved Corruption has no cast-time
    reduction at all — strictly worse, with no talent able to undo it. **When you add a readiness
    fallback to a marker table, apply it to every arm and then read the truth table for the
    not-ready era end to end: the answer must be the byte-for-byte pre-phase state, not "most of it".**

26. **A `mechanic: stat` node holds exactly ONE rank passive, so any "one marker aura per rank"
    invariant is wrong — the count is `(rank ? 1 : 0)`** (Phase 9, mage). The TBC Shatter node
    20857 authors five DUMMY auto-passives 942856-942860 at 10/20/30/40/50, and the obvious doctor
    predicate — "a mage at Shatter rank R holds R misc-1 auras" — is false at every rank above 1.
    `EraTalents.cpp` grants `rankSpell[newRank-1]` and REMOVES the previous rank on every rank-up
    and on reset, so a rank-5 Shatter holds the single 50% passive, not five stacking ones. The
    correct invariant is `M == (R ? 1 : 0)`, and the doctor's PROBLEM text must say so, or the
    phase's own headless gate reports a false defect on every Frost build. **Generalisation: before
    writing a doctor predicate over auto-passives, read how the node's `mechanic:` grants them —
    `stat` is one-of-N, a per-rank passive node is N-of-N.**

27. **Copy a census row's masks/flags/phase onto a clone; NEVER its `attributesMask` onto a
    non-spellmod buff** (Phase 9, mage — Arcane Potency). The Arcane Potency crit buffs
    948767-948769 take Clearcasting 12536's live consumption mask (mask0 `0x20c21af7` /
    mask1 `0x29040`, phase 1, flags 69904) so the same cast drops both charges — but their
    `attrMask` is deliberately **0**, not the census row's **12**. `PROC_ATTR_REQ_SPELLMOD` tests
    `m_appliedMods`, which only aura-107/108 spellmods populate (`SpellAuras.cpp:2188`; the core's
    own auto-gen at `SpellMgr.cpp:2258` ORs it only for ADD_FLAT/ADD_PCT auras), so copying 12 onto
    an aura-57 buff would have frozen the charge forever. **The census row tells you WHEN a proc
    fires; its attributes tell you what the ORIGINAL spell was, and a clone that changed the aura
    type must re-derive them.**

28. **Non-node band legitimacy is written down in exactly ONE place — `era-data/band-allowlist.yaml`**
    (Phase 9.5). Reconcile and the doctor share `EraBandClassifier::Classify`, so they cannot
    disagree, and an allowlist miss is a WARN plus a doctor PROBLEM, never a silent lingering
    orphan. The corollary for every future phase: when you mint a band id a character can KNOW
    that is neither the era's node grant nor a `spell_ranks` rank above a node-granted r1 — a
    marker, a trainer-rooted chain root, a partner buff, a level grant, a proc passive — add its
    allowlist entry in the same commit. `tools/era_audit.py check_band_ownership` fails until you
    do. Ids that are only ever SCRIPT-CAST payloads stay OUT of the allowlist on purpose (listing
    one asserts "a character may know this" and would mask a real orphan).

## 5. Open questions for the Phase-0 brainstorm (decide these first)

1. Confirm the id layout in §3.2 (TBC node base 20000, band end, helper/pet regions).
2. `_ref` era-scoping shape (§3.8).
3. Whether to commit the four wago Spell-table CSVs wholesale (~large) or fetch rows
   on demand into per-class `_ref` files (recommended: on-demand, like Vanilla).
4. Single-source the five dataset lists now, or extend all five by hand again.
5. The reconcile/marker three-way decision table — walk every gate row with the user.
6. Pilot class choice (stub vs starting Phase 1 directly inside Phase 0's verification).
