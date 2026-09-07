# Era Talents — TBC Phase 3 (Shaman) verification record

> **Generation superseded (2026-09-06 final review):** the current stamp is the closing row of docs/verification/era-talents-final-review-tbc-vanilla.md §10 — reinstall patch-V.mpq + restage the addon before any real-client pass.

**Status: SIGNED OFF 2026-09-02 (final generation `19e3a621`) — PHASE 3 CLOSED, §11.** Covers
BOTH plans: Plan 3a (tree, 9/9 tasks) + Plan 3b (totems, Tasks 1–9). All 61 nodes authored and
wired, `era_audit.py` clean bare + targeted, 201/201 host tests green, three-era reconcile +
doctor runtime-verified on live bots at L48/60/70/80 (§7, §8), the tooltip-honesty fix round
applied (§10), and the user's real-client pass signed off (§11). Branch `feat/era-talents`,
**merged to master 2026-09-07** (TBC merges as a set after all nine classes + final review + Vanilla regression —
this phase's bundled Vanilla fixes ride that same push). The `EraTalentIP.cpp` ERA_TBC allowlist
flip is still an **uncommitted dev-box edit** (verified unstaged at close-out).

**Spec/plan:** `docs/superpowers/specs/2026-08-31-era-talents-tbc-phase3-shaman-content-design.md`
(**Amendments A.1–A.13 + B.1–B.8.1 supersede the body**) +
`docs/superpowers/plans/2026-08-31-era-talents-tbc-phase3a-shaman-tree.md` +
`...phase3b-shaman-totems.md`. Working state during the phase:
`docs/verification/era-talents-tbc-phase3-shaman-progress.md` (kept as the anti-spiral working-rules
record). **Findings authority:** `era-data/_ref/tbc/shaman-totems-tbc.yaml`
(`id_allocation:` / `accepted_gaps:` / `open_questions:`) and `era-data/_ref/tbc/shaman-spells.yaml`
— this record summarizes; the `_ref`s are normative.

**Phase 3b commits:** `bcbd127` Task 3 · `361ee63` Task 4 · `791b2a3` Task 5 · `d9f8abf` Task 6 ·
`a6d2b58` Task 7 (+ docs `2851954`/`f979e4c`/`67148e3`/`379e265`/`3ab66f9`).

---

## 1. What shipped

The complete **TBC 2.4.3 shaman talent tree** — **61 nodes** (Elemental 20 / Enhancement 21 /
Restoration 20, ids **20200–20260**, era 1) — plus the **full TBC totem substrate**: 21 retuned
totem families, a third era marker, trainer/rank SQL, and the Windfury Weapon imbue family.
Value basis: **wago 2.5.4.44833** end-to-end. Zero new core patches this phase.

- **Class 7, spell family 11** (the inversion trap — mirror of druid's 11/7). Family guard clean:
  0 rows in either band with `SpellClassSet` outside `{0, 11}`.
- **Auto-passives:** 176 rows, `937600–938073`.
- **Hand helpers:** 176 rows in the `946512–947511` slice — `946512–946539` tree (Plan 3a),
  `947000–947351` totems (packed per-family decade blocks, `id_allocation:` is authoritative),
  `947440` "Era: TBC Totems" marker, `947441` Windfury Weapon r5. **Free: 946540–946999,
  947442–947511.**
- **Creatures:** 51 fresh entries `920300–920580`; reused-family totems keep Vanilla's
  `920100–920290`.
- **SQL (hand):** `2026_08_31_12_..._earth_shield_ranks.sql`,
  `2026_08_31_20_era_totem_tbc_trainers.sql` (85 rows rooted at 947440 + 73 `spell_ranks`),
  `2026_08_31_21_era_totem_tbc_stock_regate.sql` (Searing r2–r6 un-gated to prev-rank-only; 14
  stock 61–70 rows re-gated behind 932417 with prev-rank kept as ReqAbility2),
  `2026_08_31_22_..._windfury_ranks.sql` (947441 gated 947440+16362; stock 25505 re-gated 932417).
- **C++:** three-way `CLASS_SHAMAN` reconcile split (`EraTalents.cpp` — `kEraShaTrainedTbc` 85 ids,
  `kStockShaSwapTbc` 70 ids, `kEraShaHideTbc` {8170, 1535}); doctor predicates + 13 CLASS_SHAMAN
  `kDiscriminators[]` rows (`EraTalentsCommand.cpp`); `ResolveDetonateDelay` extended to the TBC
  auto-passives 937664/937665 (`EraTalentTotemScripts.cpp`); the `OnPlayerLearnSpell` bot branch
  (§8) (`EraTalentPin.cpp`).

## 2. Per-tab node counts (Plan 3a) and the totem verdicts (Plan 3b)

All 61 wired, 0 display-only. Elemental 20 (20200–20219), Enhancement 21 (20220–20240),
Restoration 20 (20241–20260). Per-node dispositions: the framework registry's shaman row and the
two plan close-out sections carry the full tables; not repeated here.

**Totem survival sweep (Task 1):** 214 rank-roles across 29 families — family verdicts
IDENTICAL 0 / RETUNED 21 / ABSENT_BOTH 8; role verdicts IDENTICAL 5 / RETUNED 41 / ABSENT_BOTH 16.
The five identical roles (Earthbind pulse, Stoneclaw passives r1–r6, Tremor passive, Magma pulses
r1–r4, Grounding passive) are **reused in place** (931xxx ids referenced, never redeclared); every
family-level allocation is fresh.

**Grant-stock families (Amendment B.2's stock branch, all measured LIVE_MATCH):**

| Family | Granted stock | The one retuned rank |
|---|---|---|
| Searing Totem r1–r6 | 3599/6363/6364/6365/10437/10438 — stock rows un-gated to prev-rank-only chains | r7 clone 947090 (Vanilla can't reach: kStockShaSwap strips root 3599) |
| Windfury Weapon r1–r4 | 8232/8235/10486/16362 — stock chain already era-correct for everyone | r5 clone 947441 (cost model: TBC 190 flat vs live 7% base); rides stock enchant 2636 → stock passive 33757 → core script's TWO attacks |
| Mana Tide chain | clone 947340 → STOCK creature 10467 → stock 16191 → 39610 (both binds untouched) | summon-only clone (id ≠ 16190 dodges the WotLK 10%-HP hardcode) |
| Clearcasting 16246, Elemental Devastation, Elemental Fury, etc. | per Plan 3a's node table | — |

## 3. The three-marker resolution (triage rows 142/170/188 — all DECIDED)

`932416` (Vanilla) / **`947440` (TBC, minted Task 5)** / `932417` (WotLK). Each era's reconcile arm
grants its own marker and strips the other two, plus its era's swap/hide/trained lists. Runtime
proof in §7. Row 170 (2H axe/mace gate) confirmed no-change-permanent; details in
`docs/era-talents-tbc-reconcile-triage.md`.

## 4. Script bindings

23 `spell_script_names` rows on slice ids, all verified in the DB and matching the `_ref`'s
`script_bindings:` prescriptions row-for-row:

| Ids | Script |
|---|---|
| 946514–946518 (Flurry r1–5) | `spell_sha_flurry_proc` (+ per-rank `spell_proc`) |
| 946526 (Shamanistic Rage) | `spell_sha_shamanistic_rage` (+ `spell_proc` at 18 PPM) |
| 946532–946536 (Nature's Guardian r1–5) | `spell_sha_nature_guardian` |
| 946537–946539 (Earth Shield r1–3) | `spell_sha_earth_shield` + IP's `isAllowedToCastSpell` |
| 947260 (Sentry) | `spell_sha_sentry_totem` |
| 947335–947339 (Windfury Totem passives r1–5) | `spell_era_windfury_totem` (+ own `spell_proc` rows) |

The 156 unbound slice ids were cross-checked against the `_ref` — every one is a "no binding
wanted" case (fresh summon chains, dispel-shaped cleansing effects, the grant-stock Mana Tide/ToW
chains, the marker, and 947441 which keeps the whole stock proc mechanism).

## 5. Deliberate divergences kept (record-only)

- **Amendment B.8:** every minted pulse keeps `APPLY_AREA_AURA_RAID` + radius index 10 (30 yd) —
  the shipped-Vanilla shape — where TBC's literal is party/20 yd (index 9, and 35 yd on late
  WotLK rows). Scope and radius are the ONLY two fields carried forward from the Vanilla substrate
  by design; class masks, magnitudes, mana, durations, schools are TBC's own.
- **Vanilla is untouched** *as of Task 8's close* — datasets, clones 931xxx, nodes 188xx, markers —
  byte-identical (audit + the L48/L60 doctor runs; era-0 row counts and `1 2 64`/`1 11 62`
  unchanged). **Superseded by the §10 fix round**, which deliberately bundles user-approved Vanilla
  changes (FT proc restore + school, Stoneclaw trigger re-point, silence gating, yards prose,
  icons, Sentry clone 931390) for one prod push with TBC.

## 6. Accepted gaps (normative copies in the `_ref`s; headline list only)

**Most of this section's original list was CLOSED by the 2026-09-02 tooltip-honesty fix round —
see §10.** Still standing after it: totems ref **1** WF-Weapon hidden AP slot carried at live 444
(wago stores no Effect==0 rows — B.6's one warranted "not knowable"); **13** Windfury's literal
enchant chain not reconstructed (the era DUMMY+proc reconstruction ships instead — magnitudes
faithful, encoding not); OQ **6**'s duration half ("for 2 min" prose vs inherited 300 s — needs a
per-family 1.12 duration measurement, its own task); OQ **17**'s Fire-Nova half (attribute
divergences on merged Vanilla blast clones — undecided). Spells ref: **11** Restorative Totems'
SPELLMOD_DOT half not shipped (shipping it would double-apply; the tooltip amount arrives exactly
once). Gaps **4** and **8** were closed in batch 4a/4b (§7 adds gap 4's first live magnitude
proof); gap 5 (Fire Nova delay) closed by Task 4's `ResolveDetonateDelay` extension.

## 7. Headless sweep + three-era runtime proof (generation `8e093eb4`)

Step-1 sweep: **61** wired · family guard **0 rows** · nothing authored in 947512–949999 ·
every `creature_template_spell` pulse in 920300–920999 resolves · era table shows the nine era-0
rows, `1 2 64`, `1 11 62` unchanged + `1 7 61` · `era_talent_meta` = `8e093eb4` (matches the MPQ
generation). Audit 0 findings bare AND targeted; 199/199 tests.

Three-era doctor on one bot (Semga) moved through the bands, plus L48:

| Level | era | Marker (exactly one) | Orphans | AI resolve highlights |
|---|---|---|---|---|
| 48 | Vanilla | 932416 | 0 | — |
| 60 | Vanilla | 932416 | 0 | all four no-totem gates + strategy picks → 931xxx clones; 3738/8170 → 0 (correct: not Vanilla content) |
| 70 | TBC | **947440** | 0 | gates → 947xxx clones; **Searing 3599 → 3599 (grant-stock working)**; Wrath of Air 3738 → 947300 (TBC content restored); Cleansing 8170 → 0 (correct); Mana Tide 16190 → 947340 |
| 80 | WotLK | 932417 | 0 | every totem → stock id (full stock spellbook restored) |

**Stale-state self-heal:** four mid-session TBC-band bots built under pre-Task-6 code carried an
orphan 932417; one forced reconcile (level move) cleared it to 947440-only — the expected
migration path, no code needed.

**Bots drop era totems in combat (live sampling of six TBC-band bots):** Strength of Earth pulse
**947041 applied at amount 98** from era creature 920325 — authored 86 × Enhancing Totems' +15% =
98 — **the first live proof a totem-talent half moves a magnitude** (gap 4 receipt written);
Healing Stream 947180 at its authored 18 from 920420; Windfury passive 947335 from 920560.

## 8. Bot re-check — one defect found and fixed

The new CLASS_SHAMAN `kDiscriminators[]` rows (13: four no-totem gates, six strategy picks, three
identity walks) made shaman AI-resolve state observable for the first time — and immediately
caught: **`PlayerbotFactory::InitClassSpells` hardcodes `learnSpell(8071/3599/5394)`** after the
era reconcile, so every era-managed shaman bot re-acquired three stock WotLK rank-1 totems and the
HasSpell-first bridge cast stock over clone (measured pre-fix: L60 bot resolved 8071→8071).
A shipped-Vanilla bug too, not a TBC regression. **Fix:** `EraTalentPin.cpp OnPlayerLearnSpell`'s
blanket bot bail became a bot branch — gated on `EraTalents.BotTalents`, era via
`EraTalentBots::EraFor`, instant reconcile + resolve-cache invalidation on the ~3 gated ids per
build. Post-fix all three resolve to era clones (§7 table). Normative record: totems ref
`open_questions` id 18.

## 9. What remains UNVERIFIED (Plan 3b Task 9, user-run, real client)

The plan's Task 9 checklist is authoritative. Headless-invisible highlights: frame/tooltips/icons;
totem drop visuals + party-frame buffs; the five retuned pulse magnitudes; party-not-raid scope;
trainer flows in both directions; ToW +3%/+3%; **Windfury Weapon procs TWO attacks** (r5 clone on
the stock mechanism); Earth Shield charge consumption; the six headless-invisible procs;
Elemental Mastery; Lightning Overload; node **20259 Improved Chain Heal** (bot-unreachable);
Stormstrike-vs-Flurry charge (gap 3); Shamanistic Rage 18 PPM; Nature's Guardian's known
health-bump shape (gap 10 — NOT a new bug); gap 4's remaining halves (Grace of Air, Guardian's
Stoneskin/Windwall magnitudes); era transitions via `.ip set` in every direction.

## 10. Tooltip-honesty fix round (2026-09-02, generation `94fde52c`, user-directed)

User call: "leave the (no lie) items, fix everything else" — every gap where a tooltip promised
something that didn't happen, plus the cheap era-fidelity leaks, Vanilla halves bundled (one prod
push once TBC ships). Audit 0 findings bare + targeted, 201/201 tests. What closed:

| Gap | Fix |
|---|---|
| Totems **7** / OQ 7 — Flametongue imbue dealt NO damage, both eras | Enchants 124/285/543/1683/2637 re-pointed **server-side** (`spellitemenchantment_dbc` overlay, SQL `2026_09_02_00`; client rows untouched — glow/tooltip intact; live-DBC sweep: zero stock reachers) onto band proc DUMMYs **947135-947139** (wago magnitudes) bound to `era_sha_flametongue_totem_proc` — the core FT-Weapon speed math (bp/100×speed, clip [bp/77, bp/25]) dealt as fire via vehicle **947442** (family-zeroed 10444 clone, no Elemental Weapons leak). Attacker's own Improved Weapon Totems rank honored (+6/12%); the totem OWNER's rank is unreachable from an enchant proc — recorded. Also fixed in passing: Vanilla defect id 1 (931148-151 `school: 4`, was shadow) |
| Spells **10** — Nature's Guardian "heals" was a 10 s max-health bump | 946532-946536 rebound to `era_sha_nature_guardian` — same CheckProc/threshold/threat, heal delivered via band SPELL_EFFECT_HEAL vehicle **947443** at 10% max health |
| Spells **6** — Clearcasting spendable on heals | Node 20205 now grants **947444** (16164 clone, trigger→**947445**, `spell_sha_elemental_focus` rebound, proc row reproduced); 947445 = 16246 at TBC's damage-only maskA 2416967683 |
| OQ **17** Stoneclaw half — taunt added zero threat to unengaged mobs | Effects r1-r6 cloned (**947062-947067**, `attributesEx3: 0`, all else byte-inherited), Vanilla passives 931070-75 re-pointed (fixes both eras), minted r7 947061 authored ex3: 0. Fire-Nova half stays open |
| Spells **9** — Shamanistic Rage castable while stunned | `attributesEx5: 0` on 946526 (the key already existed) |
| Totems **14** — totems droppable while silenced | New `preventionType` generator key (test `test_helper_prevention_type_scalar`) + `preventionType: 1` on every real totem summon clone, both eras (85 TBC + 69 Vanilla; Sentry excluded — 0 both sides) |
| Totems **12** — Sentry buff lingered on early unsummon | Core **patch 0024** (`regen-sentry-unsummon-patch.sh`): `RemoveAurasDueToSpell(947260)` in the entry-gated block |
| OQ **6** yards half — Vanilla tooltips said "20 yards" vs shipped 30 | 53 description lines → "30 yards"; TBC ToW + Mana Tide node tooltips too. **Duration half left open** (per-family 1.12 measurement needed) |
| OQ **13** Vanilla half — Grace of Air/Windwall icons | 931320/321/330/331 → 337, 931340-342/350-352 → 174 (verified in the merged client Spell.dbc) |
| **Sentry double trainer row** (user's real-client find, gen `19e3a621`) | The stock 6495 row was deliberately ungated ("Vanilla OR WotLK" is inexpressible in one ReqAbility row), so a TBC shaman saw TWO Sentry rows and could buy the stock one, stripped at the next reconcile — and the promised instant strip-on-purchase had quietly never been wired (6495 wasn't in `kReconcileOnLearn`). Fixed by completing the three-marker pattern Sentry half-skipped: **Vanilla clone 931390** at 1.12's true values (80 FLAT mana + Nature, measured directly from the era-wow vanilla-extract Spell.dbc — the old "CONFIRMED UNCHANGED" verdict had missed live's 2%-of-base/Physical), stock row re-gated behind 932417, 6495 into both swap lists + `kReconcileOnLearn`, 931390 into `kEraShaTrained` + the `spell_sha_sentry_totem` bind + patch 0024's cleanup. One Sentry row per era. SQL `2026_09_02_01`; `_ref` receipts at the task5 disposition + the era-less ledger's no-op verdict |

**Generator hardening found by this round:** scriptBindings' per-(id,script) DELETE leaked the
previous script's row on any REBIND (both AuraScripts would have attached to Nature's Guardian on
a live DB — double heal/threat). Band ids now get one bare per-id DELETE (the module owns all of a
band id's bindings); stock ids keep the qualified delete. Test
`test_script_binding_band_ids_get_bare_delete`.

New Task 9 checks this round adds: exactly ONE Sentry Totem row at the trainer per era (and the
Vanilla one costs 80 mana flat); FT imbue procs visible fire damage per swing (speed-scaled,
main hand); Stoneclaw pulls an unengaged mob; NG heal appears as a real heal (no 10 s dropoff);
Clearcasting refuses heals; a silenced shaman cannot drop totems; Sentry buff drops instantly on
recall; SR refuses while stunned; GoA/Windwall icons render; totem tooltips read 30 yards.

## 11. Real-client sign-off (2026-09-02) — PHASE 3 CLOSED

**User-run Task 9 pass at generation `19e3a621`: SIGNED OFF** ("everything looks good"). Two
findings surfaced during the pass and were fixed + re-verified inside it, both already detailed
above: the Elemental Focus in-client tooltip (947444 client description — the window renders a
grant node's tooltip from the granted spell's client row, and the pre-fix stock grant showed
WotLK's "damage or healing" string) and the Sentry double trainer row (§10's last entry). Final
state: audit 0 findings bare + targeted, 201/201 host tests, three-era doctor zero-orphan at
L48/60/70/80 with exactly one marker per era, one Sentry row per era, generation `19e3a621` in
`era_talent_meta` and the shipped `patch-V.mpq`.

**Shaman TBC phase commits:** Tasks 3–8 + fix rounds — `bcbd127` `361ee63` `791b2a3` `d9f8abf`
`a6d2b58` `c9cd4aa` `388613a` `e77cf0a` `fab79bd` (+ docs commits). Branch `feat/era-talents`,
merged to master 2026-09-07; `EraTalentIP.cpp`'s ERA_TBC flip is committed.

**Next class: WARRIOR (Phase 4)** — start from `docs/era-talents-tbc-workflow.md` §3 (the
per-class phase process) + §4b's lessons, and read this record's §10 before the survival sweep:
the fix round's classes of finding (proc roles absent from ip-dbc, PreventionType, inherited
AttributesEx bits, grant-node client descriptions, per-era trainer-row OR-gating) are now
standing sweep items, not discoveries.
