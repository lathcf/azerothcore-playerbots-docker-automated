# Era Talents — Phase 9.5 verification record (generic band-orphan sweep)

> **Generation superseded (2026-09-06 final review):** the current stamp is the closing row of docs/verification/era-talents-final-review-tbc-vanilla.md §10 — reinstall patch-V.mpq + restage the addon before any real-client pass.

**Branch:** `feat/era-talents` (merged to master 2026-09-07). **Date:** 2026-09-06. **Status: HEADLESS COMPLETE with one
escalation** — the sweep itself is clean everywhere (500-bot broad sweep 0/0/0, three ladders), and it
surfaced **two pre-existing TBC-paladin content defects** in Blessing of Sanctuary rank 5 that are
NOT Phase 9.5 regressions. See §7.

- **Generation stamp:** `1de3388d` (unchanged — Phase 9.5 mints no ids and the allowlist is
  deliberately outside the stamp, spec Amendment A.3).
- **Allowlist:** `era-data/band-allowlist.yaml`, **83 entries**, sha `8b13166aba08`
  (`kEraBandAllowlistCount` / `kEraBandAllowlistHash` in `modules/mod-era-talents/src/EraBandAllowlist.gen.h`).
- **Build under test:** `docker compose up -d --build ac-worldserver` at HEAD `6e079f9` — exit 0,
  `grep -c " error"` = **0** (`/tmp/claude-1000/p95/build6.log`; `EraBandClassifier.cpp.o` compiled).
  NOTE: the running container had to be `--force-recreate`d — the plain `up -d --build` left it on the
  previous (now dangling) image.
- **Spec:** `docs/superpowers/specs/2026-09-05-era-talents-phase9-5-band-orphan-sweep-design.md`
  (§6 verification list, Amendments A.1-A.5 and B.1-B.5).

---

## 0. Boot canary

```
[mod-era-talents] band classifier: 2562 node-grant ids indexed, 83 allowlist entries (allowlist sha 8b13166aba08)
```

Matches the spec's expected `N = 2562` and the generated header's own count/sha — proof the generated
header is the one that compiled in.

First boot after the rebuild:

| Metric | Count |
|---|---|
| `band sweep stripped` lines | **1** |
| `UNCLASSIFIED` lines | **0** |

The single line is a correct rule-3 gating strip:

```
[mod-era-talents] band sweep stripped 948029 from Narlanne (… era TBC): ORPHAN_ALLOWLIST —
allowlisted (Prayer of Spirit TBC r1-2 — own spell_ranks root (948029), trainer-taught off the
node-granted Divine Spirit r1 948024) but gated on TBC node 20513, at rank 0
```

## 1. The four residue bots — before / after

**Before** is not a doctor: every bot that logs in during boot is already swept by its login
reconcile, so the "before" evidence is the boot log. All four had in fact already been cleaned by the
Task-4 build's own boot (that container has since been recreated, so its log is not retained) — the
boot under test therefore emitted **no** strip line for any of them, and none of them ever produced an
`UNCLASSIFIED`.

| Bot | Class | Level / era | `era_character_talent` rows | Band ids in `character_spell` after | Verdict |
|---|---|---|---|---|---|
| `Patrea` | Paladin (2) | 80, WotLK | 0 | **none** | clean (was 17 orphan lines: 932602, 946114 + 15 TBC auto-passives 936004/936012/936340/936348/936353/936362/936388/936402/936426/936434/936449/936460/936466/936474/936500) |
| `Xonoth` | Hunter (3) | 80, WotLK | 0 | `932305` only (= "Era WotLK Hunter marker", allowlisted for wotlk/hunter → `LEGIT_ALLOWLIST`) | clean (was 10 lines: 932967 + 922412/922420/922442/922464/922476/922481/922492/922497/922513) |
| `Brudugs` | Hunter (3) | 80, WotLK | 0 | `932305` only (same marker) | clean (was 1 line: 922676) |
| `Rozki` | Mage (8) | 80, WotLK | 0 | **none** | clean (was 8 lines: 932760 + 920009/920020/920028/920052/920072/920116/920122; 920044 had already gone in Phase 9) |

Reference "before" id lists are the warlock record §13.4 / the mage record's re-measurement.

SQL used for the "after" column:

```sql
SELECT c.name, s.spell FROM character_spell s JOIN characters c ON c.guid = s.guid
WHERE c.name IN ('Patrea','Xonoth','Brudugs','Rozki') AND s.spell BETWEEN 920000 AND 949999;
-- → Brudugs 932305 / Xonoth 932305 only
```

## 2. Doctor + `bandsweep` idempotence

```
python3 tools/wgconsole.py … ".eratalents doctor {Patrea,Xonoth,Brudugs,Rozki}" \
                             ".eratalents bandsweep {Patrea,Rozki,Xonoth,Brudugs}"
```

| Check | Result |
|---|---|
| doctor headers / `== doctor done ==` | 4 / 4 |
| `ORPHAN ERA SPELL` | **0** |
| `PROBLEM` | **0** |
| `== bandsweep done: 0 stripped ==` | **4 / 4** (`Patrea` class 2 L80 WotLK, `Rozki` class 8 L80 WotLK, `Xonoth` class 3 L80 WotLK, `Brudugs` class 3 L80 WotLK) |

The sweep is idempotent: a second explicit pass over an already-reconciled character strips nothing.

## 3. Broad sweep — every online bot ≤ 80

Driver: the rogue/mage-record `sweep.py` (batches of 40, waits on each doctor's own
`== doctor done ==` sentinel), name list = `SELECT name FROM characters WHERE online=1 AND level<=80`
(**500 bots**, all ten classes, all three bands).

```
grep -c "== eratalents doctor" sweep.out   → 500
grep -c "doctor done"          sweep.out   → 500
grep -c "ORPHAN ERA SPELL"     sweep.out   → 0
grep -c "<-- PROBLEM"          sweep.out   → 0
docker compose logs … | grep -c "UNCLASSIFIED"  → 0
```

| Class | Bots doctored | Bands seen | ORPHAN ERA SPELL | `<-- PROBLEM` |
|---|---|---|---|---|
| Warrior | 50 | TBC 3 / Vanilla 20 / WotLK 27 | **0** | **0** |
| Paladin | 50 | TBC 4 / Vanilla 25 / WotLK 21 | **0** | **0** |
| Hunter | 50 | TBC 3 / Vanilla 27 / WotLK 20 | **0** | **0** |
| Rogue | 50 | TBC 8 / Vanilla 21 / WotLK 21 | **0** | **0** |
| Priest | 50 | TBC 4 / Vanilla 21 / WotLK 25 | **0** | **0** |
| Death Knight | 50 | TBC 6 / Vanilla 3 / WotLK 41 | **0** | **0** |
| Shaman | 50 | TBC 5 / Vanilla 24 / WotLK 21 | **0** | **0** |
| Mage | 50 | TBC 6 / Vanilla 30 / WotLK 14 | **0** | **0** |
| Warlock | 50 | TBC 3 / Vanilla 27 / WotLK 20 | **0** | **0** |
| Druid | 50 | TBC 6 / Vanilla 24 / WotLK 20 | **0** | **0** |
| **Total** | **500** | TBC 48 / Vanilla 222 / WotLK 230 | **0** | **0** |

This is the headline result. The warlock record's 37 residual orphan lines and the mage record's 19
are both **gone**, and no new ones appeared in any class or band. The three strips the worldserver
logged during the sweep window were all correct rule-3 gating decisions:

```
946261 from Kichme  (era TBC): ORPHAN_ALLOWLIST — … but gated on TBC node 20141, at rank 0
946280 from Kichme  (era TBC): ORPHAN_ALLOWLIST — … but gated on TBC node 20135, at rank 0
948029 from Narlanne(era TBC): ORPHAN_ALLOWLIST — … but gated on TBC node 20513, at rank 0
```

WARN (`UNCLASSIFIED`) count across the whole window: **0**.

Caveat on the §7 finding's absence here: `946136` had already been stripped from the only paladin then
holding it (`Larenvedon`, on the boot before this build's), and `Sathelon` only re-acquired it from the
factory during the §4b ladder, which ran *after* this sweep. The sweep therefore genuinely saw no
orphan — it is not that the sweep missed one.

## 4. Three ladders (L60 → L70 → reset → L80 → L70)

Each rung: `.character level <Bot> <L>` → `.raidroster syncone <Bot>` → `.eratalents doctor <Bot>`.

### 4a. Mage — `Caeuun` (class 8, **Frost**: 51 pts in tab 3 pre-ladder) — rule 2

`grep -c "ORPHAN ERA SPELL\|<-- PROBLEM"` → **0** over 6 doctors. Zero band strips logged.

| Rung | Doctor line |
|---|---|
| L60 | `era=Vanilla … spent=51`; `node 18049 rank 1: grant 932331 known=yes`; `chain Ice Barrier (node 18049): talent rank 1, clone r1 known, ranks known 4 (highest r4)`; `chain Pyroblast (node 18024): talent rank 0, clone r1 MISSING, ranks known 0`; `chain Blast Wave (node 18030): talent rank 0, clone r1 MISSING, ranks known 0`. **No 948xxx anywhere.** |
| L70 | `era=TBC … spent=61`; `chain Pyroblast (node 20830): talent rank 1, clone r1 known, ranks known 10 (highest r10)`; `chain Blast Wave (node 20837): talent rank 1, clone r1 known, ranks known 7 (highest r7)`; `chain Dragon's Breath (node 20844): talent rank 1, clone r1 known, ranks known 4 (highest r4)`. **Trained ranks far above the node-granted r1, zero orphans** — rule 2 exactly as designed. |
| reset | `spent=0`; all four chains `clone r1 MISSING, ranks known 0`; `Shatter: node 20857 rank 0, misc-1 auras held = 0` |
| L80 | `era=WotLK … spent=0`; no band lines at all; `AI resolve: 11366 (Pyroblast r1 …) -> 11366` (stock). **No 948xxx known.** |
| L70 (return) | `era=TBC`, clean |

Post-ladder re-check after the bot re-randomized: `chain Ice Barrier (node 20863): talent rank 1,
clone r1 known, ranks known 6 (highest r6)` and `== bandsweep done: 0 stripped ==`.

### 4b. Paladin — `Sathelon` (class 2, Retribution) — dual-era ids, trainer-rooted seals, reuse

`grep -c "ORPHAN ERA SPELL\|<-- PROBLEM"` → **2**, both the same id `946136`, both at the two TBC (L70)
rungs. Diagnosis in §7 — it is a content defect the sweep *found*, not a sweep defect. Everything else
on this ladder is exactly the expected behaviour:

| Check | Evidence |
|---|---|
| dual-era 932617 known iff the matching era's node is spent | L60 Vanilla: `node 18626 rank 1: grant 932617 known=yes`. L70 TBC (Ret build, node 20033 **not** spent): 932617 absent from `character_spell`. |
| dual-era 932602 | L70 TBC: `node 20055 rank 1: grant 932602 known=yes` |
| Vanilla seal clones gone on the Vanilla→TBC hop | `932634 / 932700 / 932724 / 932725 / 932737 / 932749` all `ORPHAN_ALLOWLIST — … but not for TBC` (chain roots; ranks cascade via `spell_ranks`) |
| TBC seal chains gone on both hops out of TBC | `946000 / 946034 / 946050 / 946063 / 946064 / 946078` stripped `… but not for Vanilla` **and** `… but not for WotLK` |
| exactly one faction seal at L70 | only `946080` (Seal of Blood, Horde) was held and stripped at L80; the Alliance counterpart never appears |
| trainer-rooted seal ranks at L70 with no orphan | L70 rung holds `946000-946008` (SoR r1-9), `946034-946038` (SoL), `946050-946053` (SoW), `946063-946070` (SoJ/SotC), `946079/946080/946084` — all rule-2 legit off their node-granted / marker-gated roots, **zero orphan lines for any of them** |
| LoH 932668-932670 / Judgement 932746 known at L60 **and** L70, gone at L80 | held through both managed rungs; at L80 `932668 … ORPHAN_ALLOWLIST — allowlisted (Lay on Hands clones — Vanilla trainer-taught, TBC reconcile-granted (reused substrate)) but not for WotLK` and `932746 … (the single Judgement button …) but not for WotLK` |
| L80 rung | `era=WotLK … spent=0`, no band grants, `AI resolve: 20911 (Blessing of Sanctuary) -> 20911` (stock) |

### 4c. Druid — `Kichme` (class 11, Feral) — rule 3 partners and gating nodes

`grep -c "ORPHAN ERA SPELL\|<-- PROBLEM"` → **0** over 6 doctors.

| Check | Evidence |
|---|---|
| 946261 / 946265 known iff nodes 20141 / 20161 are spent | L70 rung: `node 20161 rank 1: grant 946276 known=yes` with 946265 held; after `reset`: `band sweep stripped 946265 … ORPHAN_ALLOWLIST — allowlisted (hidden Tree of Life form passive, partner of node 20161's form grant) but gated on TBC node 20161, at rank 0` and the matching `946261 … but gated on TBC node 20141, at rank 0`. **Doctor shows no orphan at any rung** — the partner strip happens inside the same reconcile. |
| 946280 (HotW bear-stamina marker, patch 0023) | `… but gated on TBC node 20135, at rank 0` — same rule-3 gate, stripped at rank 0 |
| 932513 (Enrage) known at L60 and L70, gone at L80 | `band sweep stripped 932513 … ORPHAN_ALLOWLIST — allowlisted (Enrage clone, reconcile-granted at L12 (version swap of stock 5229)) but not for WotLK` at the L80 rung |
| Vanilla Nature's Grasp chain absent when 18701 is at rank 0 | L60 rung spends 18700/18704/18706/18732/18734/18735/18737/18738/18740/18741/18742/18743/18745/18746 — **not** 18701 — and 932505/932507-932511 are absent from `character_spell` throughout |

## 5. Second-boot noise

`docker compose restart ac-worldserver`, then count over a window anchored on the container's own
`State.StartedAt` (a `--since 6m` window silently reaches back past the restart into the §4 ladders —
that is how a first draft of this section over-counted; anchor on `StartedAt`).

Settled state: `Up 5 minutes`, **500** bots online again.

| Metric | Count |
|---|---|
| `band sweep stripped` | **2** |
| of which `UNCLASSIFIED` | **1** |

```
948032 from Zalthos  (era TBC): ORPHAN_ALLOWLIST — allowlisted (Vampiric Embrace heal proc-passive (TBC)) but gated on TBC node 20555, at rank 0
946136 from Sathelon (era TBC): UNCLASSIFIED — in no era's rank table, no spell_ranks chain, no allowlist entry
```

The first is a correct rule-3 gating strip on a bot whose build changed across the relog — the same
shape as the first boot's single line, not residue. The second is the §7 finding, and it recurs on
every boot for every L70 TBC paladin until the content defect is fixed. **Every other one of the 500
bots re-logged clean**, which is the property this step exists to prove: the first boot's sweep is not
re-doing work.

## 6. REAL-CLIENT CHECKLIST (for the user — not run headlessly)

`.ip set` discards its player argument from the console and bots never fire `EraTransition::Detect`
(spec Amendment A.4), so the player path cannot be driven headlessly. On a real client:

1. On a human character, **self-targeted**: `.ip set <char> 1` → `.eratalents doctor` →
   `.ip set <char> 8` → `.eratalents doctor` → `.ip set <char> 13` → `.eratalents doctor` →
   `.ip set <char> 1` → `.eratalents doctor`. Expect **zero `ORPHAN ERA SPELL`** at every hop, and the
   talent panel + spellbook showing only the current era's spells.
2. `.eratalents doctor` on each of your existing characters — **zero orphans**.

## 7. ESCALATION — Blessing of Sanctuary rank 5 (946136): two pre-existing content defects

The sweep did its job and found real content problems. **Neither is a Phase 9.5 regression**, and
neither should be fixed by touching the classifier or the allowlist.

**Symptom.** Every TBC-band (L70) paladin ends up KNOWING `946136` (BoS r5) and the sweep strips it as
`UNCLASSIFIED` with a WARN, which the doctor renders as:

```
ORPHAN ERA SPELL: 946136 (in no era's rank table, no spell_ranks chain, no allowlist entry)  <-- PROBLEM (unclassified = allowlist miss)
```

Observed on `Sathelon` (paladin, L70, TBC) at both L70 ladder rungs and again on the second boot, and
on `Larenvedon` (paladin, L70, TBC) — the two paladins that happened to be sitting in the TBC band.
It is the ONLY `UNCLASSIFIED` this whole verification produced.

**Defect 1 — the r5 trainer row gates on the era marker, not the previous rank.**
`modules/mod-era-talents/data/sql/world/base/2026_08_30_10_era_tbc_paladin_bos_r5.sql` writes
`ReqAbility1 = 946078` (the "Era: TBC Paladin" marker) where every other rank in the chain gates on its
predecessor:

```
TrainerId SpellId ReqAbility1 ReqLevel
3/4/5     932661  932617      40     <- r2 requires r1
3/4/5     932662  932661      50     <- r3 requires r2
3/4/5     932664  932662      60     <- r4 requires r3
3/4/5     946136  946078      70     <- r5 requires ONLY the era marker   *** defect ***
```

`PlayerbotFactory` learns every trainer spell a bot qualifies for, so a **Retribution** paladin with
node 20033 (Blessing of Sanctuary) at rank 0 — and therefore without 932617/932661/932662/932664 —
learns rank 5 anyway. Verified: `Sathelon`'s `character_spell` holds `946136` and **none** of
932617/932661/932662/932664. Likely fix: `ReqAbility1 = 932664`.

**Defect 2 — the r5 `spell_ranks` row is missing from the live world DB (SQL re-application order).**

```sql
SELECT * FROM spell_ranks WHERE first_spell_id = 932617;
-- 932617 932617 1 / 932617 932661 2 / 932617 932662 3 / 932617 932664 4    ← (932617, 946136, 5) ABSENT
```

Cause, from `acore_world.updates`:

| File | Applied |
|---|---|
| `2026_08_30_10_era_tbc_paladin_bos_r5.sql` (inserts `(932617, 946136, 5)`) | 2026-08-30 16:07 |
| `2026_08_19_30_era_talent_paladin_active_rank_trainers.sql` (`DELETE FROM spell_ranks WHERE first_spell_id IN (932646, 932615, **932617**, 932668)`, re-inserts only r1-r4) | **2026-09-04 19:26** |

The AzerothCore module updater re-runs a file whose content hash changed **at that moment**, not in
filename order, so the earlier-dated `_19_30` file ran *after* `_30_10` and silently deleted its row.
A **fresh** import applies them in filename order and is correct — this is drift that bites any live
server (dev box today, production tomorrow) whenever `_19_30` is edited again. Scope was bounded by
diffing all **445** authored `spell_ranks` tuples in the module SQL against the live table: **exactly
this one row is missing, nothing else.** Likely fix: move the r5 row into `_19_30` (or narrow its
`DELETE`), plus a one-off re-insert on any DB that already drifted.

**Why the sweep is not at fault.** With defect 2 fixed, `GetFirstSpellInChain(946136)` would return
932617, rule 2 would consult rule 1 on 932617, find node 20033 at rank 0, and return `ORPHAN_CHAIN` —
still a strip and still an `ORPHAN ERA SPELL` doctor line, just an INFO instead of a WARN. The line
only disappears once defect 1 stops handing rank 5 to paladins who never took the talent. The
allowlist must **not** grow an entry for 946136: it is a genuine chain member, and listing it would
assert "any TBC paladin may know BoS r5", which is the bug.

---

## 8. What was NOT run

- The `era_audit.py` negative test was run in Task 3 and reproduced by the final review (2026-09-06):
  deleting the 932749 (Vanilla seal-training marker) entry from `era-data/band-allowlist.yaml` →
  `era_audit: 28 finding(s)`, every line of the shape
  `band-ownership: trainer_spell ReqAbility1 932749 (teaching 9327xx) is a band id that is neither a node grant, a chain member, nor allowlisted (a bare marker needs a `marker` entry)`
  (check 2 fires, not check 1 — 932749 is reconcile-granted, not trainer-taught, so spec §6.1's "check 1"
  expectation was slightly off); entry restored → `era_audit: 0 finding(s)`. Not repeated in the gate run.
- Any real-client verification — §6 is the checklist handed to the user.
- Any fix for §7 — a separate dispatch owns it.

---

## 9. Fix round 1 (2026-09-06)

Closes the §7 escalation. Two SQL files + four comment/include nits in `EraTalents.cpp`; no YAML,
no header, no Python tool, no allowlist entry for 946136 (it is a genuine chain member — listing it
would assert "any TBC paladin may know BoS r5", which is the bug).

**The chain, read from the SQL (not assumed):** r1 `932617` → r2 `932661` → r3 `932662` →
r4 `932664` → r5 `946136`.

### 9.1 Defect 1 — r5 trainer row gated on a marker instead of the previous rank

`2026_08_30_10_era_tbc_paladin_bos_r5.sql` shipped all three rows (TrainerId 3/4/5) with
`ReqAbility1 = 946078` (the Era:TBC-Paladin marker). Every other rank ≥ 2 row in this chain gates on
the previous rank. The bot factory's trainer walk learns any row whose requirements it meets, so a
TBC paladin who never took node 20033 — and therefore holds no r1 932617 — still satisfied 946078
and was handed r5. Fixed: `ReqAbility1 = 932664` on all three rows. Era exclusivity does not need
the marker here — r4 exists only as an era clone trained off the talent-granted r1, and `ReqLevel 70`
is out of a Vanilla-era character's reach. The file's header prose (which claimed the marker gate)
was corrected with it.

### 9.2 Defect 2 — r5 `spell_ranks` row lost to re-apply order

`2026_08_19_30_…` owns the 932617 chain with a `DELETE … WHERE first_spell_id IN (… 932617 …)`
followed by an r1-r4 `INSERT`; `2026_08_30_10_…` appended only the r5 tuple. AC re-applies a file
when its **content hash** changes, in filename order within that run — not in original-ship order —
so the 2026-09-04 re-apply of `_19_30` ran after `_30_10` and deleted its row.

Fixed so the table converges regardless of which file re-applies last: **both** files now emit the
**identical full r1-r5 chain** — `_19_30` gained the `(932617, 946136, 5)` tuple, and `_30_10`'s
statement became `DELETE FROM spell_ranks WHERE first_spell_id=932617` + an r1-r5 `INSERT`
(previously a `DELETE … WHERE spell_id=946136` + the single r5 tuple). Each file carries a
`RE-APPLY HAZARD` comment naming the mechanism. Editing both files changes both hashes, so the
updater re-applies both at the next boot — that is what repaired the live table (verified in 9.5).

### 9.3 Same-shape check across all module SQL — one hit, the one already known

Throwaway script `/tmp/claude-1000/p95/chaincheck.py` (not committed): imports `era_audit.py` via
`spec_from_file_location`, takes the post-replay trainer rows from `_effective_trainer_rows()`
(6731 rows, 0 replay findings), replays every module `spell_ranks` `DELETE`/`INSERT` in filename
order (445 rows), collects the node-granted spell set from `era_talent_rank` (2979 ids), and flags
any trainer row teaching **rank ≥ 2 of a chain whose root is node-granted** where `ReqAbility1` is
not the previous rank.

- Against the **pre-fix** files (negative control, files restored from HEAD for the run):
  **3 hits** — TrainerId 3/4/5 × SpellId 946136, `ReqAbility1=946078`, "should be 932664". Nothing else.
- Against the **fixed** files: `CLEAN`.

So the shape exists nowhere else in `modules/mod-era-talents/data/sql/world/base`.

### 9.4 Comment/include nits from the Task 5 review (`src/EraTalents.cpp`, no behaviour change)

- Druid Enrage WotLK arm: the comment claimed it strips custom 932513; the generic band sweep owns
  that strip now — reworded.
- `#include <unordered_set>` removed (unused after Task 5; no `unordered_set` token remains in the file).
- The `EraBandClassifier::Sweep` call-site comment no longer says the sweep runs "LAST" — it runs
  "after every strip arm above (the priest tail below is grant-only)".
- The warlock 932930/stone marker fallback and the hunter `else` (WotLK marker 932305) each gained
  one line: if TBC is ever removed from `EraHasTalentTrees`, the fallback grant is an allowlist era
  mismatch for a TBC-band character and the band sweep strips it each reconcile — the allowlist
  eras, not the arm, decide.

### 9.5 Verification (dev box, gen `1de3388d`)

| # | Command | Result |
|---|---|---|
| 1 | `uv run --with pyyaml python tools/era_audit.py` | `era_audit: 0 finding(s)` |
| 2 | module sync + `docker compose up -d --build ac-worldserver` | `exit=0`, `grep -c " error"` → **0**; then `up -d ac-db-import` + `up -d --force-recreate ac-worldserver`; readiness after ~130 s = `ac-worldserver running Up`, `era_talent_meta` → `generation 1de3388d`; boot log clean (`[mod-era-talents] loaded 1011 talent nodes`, `band classifier: 2562 node-grant ids indexed, 83 allowlist entries (allowlist sha 8b13166aba08)`) |
| 3 | `SELECT * FROM spell_ranks WHERE first_spell_id=932617` / `SELECT SpellId, ReqAbility1 FROM trainer_spell WHERE SpellId=946136` | five rows `932617/932661/932662/932664/946136` ranks 1-5; three trainer rows all `946136 → 932664` |
| 4 | `wgconsole.py … .eratalents doctor Sathelon / Larenvedon` | both online at L70 (class 2); `grep -c "ORPHAN ERA SPELL\|<-- PROBLEM"` → **0**. Sathelon's band section no longer lists 946136 and `AI resolve: 20911 (Blessing of Sanctuary) -> 0` (correct — node 20033 is rank 0). `SELECT COUNT(*) FROM character_spell WHERE spell=946136` → **0** holders server-wide. `logs --since 5m … grep -c UNCLASSIFIED` → **0** |

### 9.6 Open item carried forward — **CLOSED-AS-EXPECTED 2026-09-06**

`.raidroster syncone` issued immediately after `.eratalents reset` left the bot at `spent=0` — the
sync did not re-spend an era build. **Not a defect:** `syncone` is a console **no-op**
(`RaidRosterCommand.cpp:374-375` requires an in-world player session), so `spent=0` right after it
was the expected outcome; the immediate re-spend observed during the 2026-09-06 final review was
the background randomizer's own factory pass, not `syncone`. Verified 2026-09-06 — no
roster/factory-sequencing defect, and nothing in the band sweep is implicated.
