# Era Talents Phase 3 — Vanilla Priest: Headless Verification Record

**Date:** 2026-08-13
**Branch:** `feat/era-talents` (merged to master 2026-09-07)
**Scope:** Full 47-node Vanilla Priest tree (Discipline 15 / Holy 16 / Shadow 16) on the proven
`mod-era-talents` machinery. Plan: `docs/superpowers/plans/2026-08-13-era-talents-phase3-priest.md`.

## Build / boot

- Host: `pytest tools/test_gen_era_talents.py tools/test_import_daribon_talents.py` → **43 passed**.
- Lint: `gen_era_talents.py --sql` on **both** `priest.yaml` and `mage.yaml` → clean (no unknown
  `affects`/mechanic, no id-band collision).
- Dev-box boot: `[mod-era-talents] loaded 96 talent nodes` (49 Mage + 47 Priest), clean, no
  `malformed`/`ERROR` lines.

## DB integrity (Priest, classId 5)

| Check | Result | Expected |
|---|---|---|
| `era_talent` nodes | 47 | 47 |
| nodes with no rank row (unwired) | 0 | 0 (zero display-only) |
| `era_talent_rank` rows | 144 | 48+48+48 (sum of maxRanks/tab) |
| `spell_proc` rows in band | 20 | Spirit Tap 5 + Blackout 5 + Shadow Weaving 5 + Martyrdom 2 + Inspiration 3 |
| custom `spell_dbc` rows in band | 134 | authored passives + helpers |
| SPELLMOD passives with family≠6 | **0** | 0 (else silent no-op) |
| helper spells present (932970 Blackout stun, 920987-9 Inspiration) | 4 | 4 |
| orphan grants (granted custom id absent from spell_dbc) | 0 | 0 |
| both classes coexist | 5=47, 8=49 | Mage untouched by Priest import |

The `spellmod_wrong_family=0` check is the key structural guarantee: every authored SPELLMOD binds
`SpellClassSet=6` and its `EffectSpellClassMaskA_*` intersects the target stock spell (incl. the
word-3 Mind Flay case, verified `EffectSpellClassMaskA_3=64` on the Shadow Reach rows).

## Per-tab wiring summary

- **Shadow (16):** bound Mind Flay/Silence/Shadowform; spellmods Shadow Affinity/Imp SW:Pain/Imp
  Psychic Scream/Imp Mind Blast/Imp Fade/Shadow Reach; stat Shadow Focus/Darkness; procs Spirit
  Tap(→15271)/Blackout(→authored stun helper 932970)/Shadow Weaving(→15258); VE bound(15286) + Improved
  VE SPELLMOD(op effect1 raises core heal %).
- **Discipline (15):** bound Inner Focus/Divine Spirit/Power Infusion; spellmods Imp PW:Fortitude/Imp
  PW:Shield/Imp Inner Fire/Mental Agility/Imp Mana Burn; stat Wand Spec/Meditation/Mental Strength;
  multi Unbreakable Will(aura 117 resist-chance)/Silent Resolve(threat+resistDispel)/Force of Will;
  proc Martyrdom(→Focused Casting 14743/27828).
- **Holy (16):** bound Holy Nova/Spirit of Redemption/Lightwell/Blessed Recovery(stock talent);
  healing spellmods Healing Focus/Imp Renew/Divine Fury/Holy Reach/Imp Healing/Searing Light/Imp
  Prayer of Healing; stat Holy Spec(aura 71)/Spell Warding(aura 87)/Spiritual Healing(aura 136); multi
  Spiritual Guidance(aura 174+175, Spirit); proc Inspiration(authored buffs 920987-9).

## Generator/data deltas this phase

- `CLASS_TOKENS[5]="PRIEST"`; `OP["resistDispel"]=28` (Silent Resolve dispel-resist SPELLMOD).
- New `_ref/priest-spells.yaml` (live-verified family flags + reused ids).
- No new mechanic *forms*; no C++ (VE reuses the core script; Improved VE is a SPELLMOD).

## Deferred to Task 10 (real 3.3.5a client — needs a fresh Vanilla priest)

Headless covers structure; the following are runtime behaviors only observable in-client:
- Full learn-flow (learn a full spec, point accounting, prereqs, active-rank-only, reset,
  multi-chunk SYNC).
- Improved VE actually raising the core VE heal % (15→20→25 via the SPELLMOD).
- Blackout/Shadow Weaving procs firing off periodic SW:Pain/Mind Flay ticks (possible
  `PROC_ATTR_TRIGGERED_CAN_PROC` need); Blackout stun helper stunning 3s.
- Healing-spell tooltips rewriting via `Tooltip.lua` (Divine Fury cast time, Imp Healing cost).
- Fidelity decisions for the user: Shadow Weaving self-buff vs authentic target debuff; VE WotLK
  split; Blessed Recovery WotLK magnitude (5/10/15 vs Vanilla 8/16/25); Inspiration as
  damage-taken-reduction.
- Tab-background art (Priest* textures vs graceful fallback).

## Fix round 1 — Wand Specialization (18102) school binding (2026-09-04, code-verified, NOT shipped)

**Trigger:** TBC Phase 6 (`accepted_gaps` id 12, second half) flagged that Vanilla node 18102 authors
`school: 1` (Physical) where both TBC and live 3.3.5a 14524 carry `EffectMiscValue` 126. Verified
here against the generated rows, the live DBC, the fork's damage code, and the world DB.

### Verdict

The suspicion is **confirmed, and the defect is worse than a wrong school** — the shipped Vanilla
passive is inert for TWO independent reasons, and the proposed one-key fix (`school: 126`) would
**introduce a +5…25% bonus to every magic spell** the priest casts while a wand is equipped. Nothing was
regenerated. The fix needs a small band-gated core patch first (decision pending — see *Proposed fix*).

### Evidence

1. **Generated rows** `920816-920820` (`2026_08_13_11_era_talent_priest_custom_spells.sql`):
   `EffectAura_1=79`, `EffectMiscValue_1=1`, `EquippedItemClass=2`, `EquippedItemSubclass=262144`.
   The Vanilla mage twin 18004 (`920032/920033`, `2026_08_13_00_era_talent_custom_spells.sql`) is
   byte-identical in every one of those fields (misc 1, subclass 262144) — **same defect, same fix**.
2. **Wrong equip gate.** `262144 = 1<<18 = ITEM_SUBCLASS_WEAPON_CROSSBOW`; wands are
   `ITEM_SUBCLASS_WEAPON_WAND = 19` → `524288` (`ItemTemplate.h:362-363`; live 14524 carries 524288;
   `acore_world.item_template` has 339 wands, all `subclass=19`). `Item::IsFitToSpellRequirements`
   (`Item.cpp:923-926`) fails on the subclass bit, so BOTH gate paths — `Player::CheckAttackFitToAuraRequirement`
   (`Player.cpp:7303`) for the stat path and `Player::HasItemFitToSpellRequirements` (`Player.cpp:12781`)
   for the bonus paths — return false for an equipped wand. The aura currently applies to **nothing**.
   The mage YAML comment ("wand subclass 18") is the origin of the wrong constant.
3. **Wand shots are magic-school in this engine.** `Spell::Spell` (`Spell.cpp:604-618`): 5019 Shoot
   (`Effect_1=17 SPELL_EFFECT_WEAPON_DAMAGE_NOSCHOOL`, `DefenseType=1`, `AttributesEx2=0x20 AUTO_REPEAT`,
   `EquippedItemClass=2/524288`) is routed to `RANGED_ATTACK` via the AUTO_REPEAT default branch and
   `m_spellSchoolMask` is overridden to `1 << wand->Damage[0].DamageType` for `CLASSMASK_WAND_USERS`.
   World DB: every player-obtainable wand has a magic `dmg_type1` (Holy 46 / Fire 92 / Nature 45 /
   Frost 33 / Shadow 70 / Arcane 6); the 47 `dmg_type1=0` rows are `Monster -`/`NPC Equip`/`Deprecated`/
   `TEST` items plus four TBC/WotLK-level oddities (34138/34139 req 75, 36655 req 67, 38204 DB38) — none
   reachable by a Vanilla-era character.
4. **Why `school: 1` can never apply to a wand shot, even with the gate fixed.** `Spell::EffectWeaponDmg`
   (`SpellEffects.cpp:3633-3667`): `isPhysical = m_spellSchoolMask & SPELL_SCHOOL_MASK_NORMAL` is false
   for a magic wand, so `CalculateDamage(RANGED_ATTACK, normalized, addTotalPct=false)` → `Player::
   CalculateMinMaxDamage` (`StatSystem.cpp:582`) applies `totalPct = 1.0` — the `UNIT_MOD_DAMAGE_RANGED
   TOTAL_PCT` bucket, which is the ONLY place a Physical-misc aura 79 lands (`Unit::UpdateDamagePctDoneMods`,
   `Unit.cpp:12157-12192`, filters `misc & NORMAL`), is deliberately excluded. Then
   `Unit::MeleeDamageBonusDone(…, m_spellSchoolMask)` (`SpellEffects.cpp:3693` → `Unit.cpp:10193ff`) runs
   the aura-79 loop only when `!(damageSchoolMask & NORMAL)` and requires `misc & damageSchoolMask` — so
   only a mask containing the WAND's school (126 covers all six) is applied. The mage YAML rationale
   ("core feeds ranged (wand) TOTAL_PCT from aura 79 with the PHYSICAL school") describes the
   physical-weapon path and is **wrong for wands**.
5. **Character-sheet trap.** `Unit::UpdateDamagePhysical` (`StatSystem.cpp:60-71`) computes
   `UNIT_FIELD_MIN/MAXRANGEDDAMAGE` with `addTotalPct=true`, so with the subclass fixed and `school: 1`
   the paper-doll ranged damage would SHOW the +25% while the actual shot ignores it. "The sheet moved"
   is NOT verification for this talent — measure real hits.
6. **`school: 126` leaks onto spells (the reason this was not shipped).** `Unit::SpellPctDamageModsDone`
   (`Unit.cpp:8437-8490`) is the multiplier for every `SpellDamageBonusDone` call (direct AND DoT). Its
   weapon-specific guard skips a weapon-gated aura for a non-weapon spell **only when `misc ==
   SPELL_SCHOOL_MASK_NORMAL`**; a misc-126 wand-gated aura passes the guard, matches `misc & school` for
   any Shadow/Holy spell, falls to the `HasItemFitToSpellRequirements` branch, and that scan covers
   `EQUIPMENT_SLOT_MAINHAND..RANGED` (`Player.cpp:12792`) — an equipped wand satisfies it. Net: Mind Blast,
   SW:Pain ticks, Smite, Holy Fire… all +5…25% at 5/5 while wielding a wand. Stock 14524 has the same
   shape, but no 3.3.5a talent teaches it, which is why upstream never noticed.
   **This applies to the TBC node 20501 as shipped** (`era-data/tbc/priest.yaml`, `school: 126`,
   `equipSubclass: 524288`): the TBC clones 946xxx are wand-correct AND spell-leaking today. Not changed
   here (out of scope per the task), recorded in `era-talents-tbc-phase6-priest.md` §13.
7. **Not caught by any existing check.** `.eratalents doctor` reports rank/known state only;
   `era_audit.py` has no lint tying a wand-named node's `equipSubclass` to `1<<19`, nor one for
   aura-79 `school:` on a weapon-gated row. The headless pass above only asserted row presence.

### Proposed fix (needs a decision — core patch, so spec-first per `docs/era-talents-framework.md` Process rules)

- **Option A (recommended): band-gated core patch `0025-core-wand-spec-era-no-spell-leak`** in
  `Unit::SpellPctDamageModsDone`: inside the aura-79 lambda, `return false` when
  `spellProto->EquippedItemClass == -1` and the aura's spell id is in `[920000, 950000)` with
  `EquippedItemClass == ITEM_CLASS_WEAPON` and `EquippedItemSubClassMask & (1 << ITEM_SUBCLASS_WEAPON_WAND)`.
  Inert for every stock aura; wand shots are unaffected because they go through `MeleeDamageBonusDone`,
  not this function. **`Unit.cpp` is shared with patches 0012 and 0018 → the regen script MUST be
  baseline-aware** (same trap as 0018). Then author on BOTH Vanilla nodes (18102 priest, 18004 mage):
  `school: 126` and `equipSubclass: 524288`, fix the mage comment, `tools/era-regen.sh`, audit. TBC 20501
  becomes correct with no YAML change.
- **Option B: scripted proc** (`proc:` on `PROC_FLAG_DONE_RANGED_AUTO_ATTACK`, `HandleProc` deals
  `pct%` of `DamageInfo::GetDamage()` in the wand's school). No core patch, but a second combat-log hit
  per shot, a per-school damage helper problem, and a mechanic change on three nodes. Not recommended.
- **Either way, add an `era_audit.py` lint:** any node whose name contains "Wand" must author
  `equipSubclass: 524288`, and any weapon-gated aura-79 row must state `school:` explicitly.

### Real-client verification recipe (for whichever option ships)

Vanilla priest, wand equipped, target dummy: (1) wand hits with 0/5 vs 5/5 differ by ~25%; (2) Mind
Blast / Smite with 5/5 do NOT change between wand equipped and unequipped (the leak check — this is the
one that would have caught the TBC 20501 state); (3) the sheet's ranged damage is NOT accepted as
evidence (item 5). Repeat (2) on a TBC priest with 20501 5/5.

### Retractions swept (fingerprints: "school: 1", "physical, wand-equip gated", "wand subclass 18",
"TOTAL_PCT … PHYSICAL")

- `docs/verification/era-talents-priest-fidelity-audit.md` 18102 row — marked RETRACTED in place.
- `docs/verification/era-talents-tbc-phase6-priest.md` §13 — leak finding added against 20501.
- `era-data/vanilla/mage.yaml` 18004 comment and `era-data/tbc/priest.yaml` 20501 comment — **NOT
  edited yet**: the generation stamp is a content hash of the YAML files, so a comment-only edit would
  force a client MPQ round with no behaviour change. Fix them in the same regen as the real fix.

### Fix round 1 — SHIPPED (2026-09-04, headless-verified, gen `bdad4294`)

Option A implemented per `docs/superpowers/specs/2026-09-04-era-talents-wand-spec-leak-fix-design.md`.

**Files changed:**
- `patches/0025-core-wand-spec-era-no-spell-leak.patch` (new) + `tools/regen-wandspec-patch.sh`
  (new, baseline-aware: reverses BOTH 0012 and 0018 out of Unit.cpp, diffs, re-applies both). The
  single hunk sits in `Unit::SpellPctDamageModsDone` immediately after the stock 2H-spec weapon
  guard; it `return false`s for a non-weapon spell when the aura's id is in `[920000, 950000)` and
  it is `ITEM_CLASS_WEAPON` + wand-subclass gated. Band is `< 950000` (NOT 933000) so it covers the
  TBC clones at 940008-940012.
- `era-data/vanilla/priest.yaml` node 18102 and `era-data/vanilla/mage.yaml` node 18004:
  `school: 1`→`126`, `equipSubclass: 262144`→`524288`, stale comments rewritten. `era-data/tbc/
  priest.yaml` 20501 untouched (already correct; the patch removes its leak with no data change).
- `tools/gen_era_talents.py` (`_apply_equip_gate` comment corrected: 1<<19 wand, magic school 126).
- `tools/era_audit.py` new `check_wand_spec_gating` (L1 wand-name→524288, L2 equip-gated aura-79
  must author school, L3 wand-bit + school 1 → inert). Registered in `main()`.
- Doc retraction copies updated to "implemented, pending real-client sign-off":
  `era-talents-priest-fidelity-audit.md` 18102 row, `era-talents-tbc-phase6-priest.md` §13.

**Headless verification (this session):**
- `tools/era-regen.sh` → gen stamp **`bdad4294`** (+865 client spell rows into patch-V.mpq).
- `era_audit.py` → **0 findings** (the new lint included; confirmed it fires on the pre-fix
  262144/school-1 values and clears after the fix).
- Generated rows confirmed by parsing the custom-spell SQL: Vanilla priest 920816-920820 and mage
  920032-920033 (node 18004 is maxRank 2, so only two ranks exist — the spec's "920032-920036" over-
  counted), and TBC priest 940008-940012, all carry `EffectAura_1=79`, `EffectMiscValue_1=126`,
  `EquippedItemClass=2`, `EquippedItemSubclass=524288`, all in `[920000, 950000)`.
- Full ordered patch apply proven: from a pristine `Unit.cpp`+`Unit.h`, `git apply --check` +
  apply of 0012 → 0018 → 0025 all clean, no reject; worktree restored to the fully-applied state.
- `Unit::SpellPctHealingModsDone` consults only aura 135 (`MOD_HEALING_DONE_PERCENT`) +
  `OVERRIDE_CLASS_SCRIPTS`, NOT aura 79 — healing cannot leak, so no healing-side guard is needed.
- **Worldserver NOT rebuilt/restarted this session** (a core `Unit.cpp` change forces a full core
  recompile — too heavy to run against the live stack unprompted). The fork worktree source is
  whole (0012/0018/0025 all applied); the next `update.sh`/rebuild picks it up. `.eratalents doctor`
  on a test priest is therefore still pending the rebuild.

**Still pending — real-client sign-off (spec items a/b/c):** on a Vanilla priest with a wand
equipped at a target dummy — (a) wand hits differ ~25% at 5/5 vs 0/5; (b) **Mind Blast / Smite
damage UNCHANGED wand-on vs wand-off at 5/5** (the leak check — the one that must pass); (c) the
character-sheet ranged damage is NOT accepted as evidence (it shows the bonus regardless). Repeat
(b) on a TBC priest with 20501 at 5/5. Client MPQ patch-V (gen `bdad4294`) must be installed first.

## Fix round 2 — priest final review R5 (2026-09-06, headless-verified)

Eight Vanilla items and four TBC items from the all-class final review. Full per-item table:
`era-talents-priest-fidelity-audit.md` §1b (Vanilla) and `era-talents-tbc-phase6-priest.md` §9
(TBC accepted gaps 12/13). Data-only — **no C++ change**, so no rebuild; regen + db-import +
worldserver restart only.

**Vanilla (`era-data/vanilla/priest.yaml`)**

| Item | What changed |
|---|---|
| **F-7** *(critical)* | 18123 Inspiration `proc.flags` `1024` → `16384` + `typeMask: 2`. `1024` is `DONE_SPELL_NONE_DMG_CLASS_POS`, which a `DAMAGE_CLASS_MAGIC` heal never raises — **the talent could not fire at all**, which is also why the I-23 payload defect below had never been observed in play |
| **I-23 / M-70** | Inspiration helpers 920987-920989 re-authored from the aura-87 "armor proxy" (−8/−16/−25% physical damage taken) to the era encoding **aura 101 `MOD_RESISTANCE_PCT` misc 1 (armor) +8/+16/+25** (`bp` 7/15/24 / ds 1), matching the TBC clones 948057-948059; plus the `client:` block they never had (icon 1463, per-rank text) |
| **I-20** | 920920 Power Infusion `manaCostPct: 0` — the clone inherited stock 10060's `ManaCostPct 16` **on top of** the authored flat 182 |
| **I-21** | 18106 Martyrdom `proc.flags` `136` → `680` (was white-hits-only) |
| **I-22** | 18132 Spirit Tap `attrMask: 1` `PROC_ATTR_REQ_EXP_OR_HONOR` |
| **I-34** | 18136 Shadow Focus re-authored from `stat` aura 55 + `school: 32` (a **no-op** filter — `StatSystem.cpp` sums aura 55 bare) to `op: resistMiss` with an explicitly censused `affectsMask` |
| **M-69** | 932970 Blackout stun `defenseType: 1` (was unresistable) + `client.description`/`auraDescription` |
| **M-71** | 18107 Inner Focus — *no change*; the two WotLK-only proc-permission bits on granted stock 14751 are KEPT and recorded as `_ref/tbc` accepted_gaps id 13 |

**TBC (`era-data/tbc/priest.yaml`)** — M-22 Lightwell r4 948054 `maxLevel: 76` (`_ref`
`node_findings[28275]`: `J:Levels TBC(70, 76, 70)->LIVE(70, 74, 70)`) and the Lightwell banner's
"level windows identical between eras" claim corrected to r1-r3; M-23 Clearcasting 948056
`defenseType: 1` (a from-scratch helper zero-filled it; live 34754 carries 1); M-24 a banner
recording the fifteen templated clones that deliberately inherit their donor's `SpellClassMask_3`
= 1024, measured inert (`SELECT COUNT(*) FROM spell_proc WHERE SpellFamilyName = 6 AND
(SpellFamilyMask2 & 1024)` = 0); M-25 accepted_gaps id 13 + the §9 row.

**Both eras — hand SQL**
`modules/mod-era-talents/data/sql/world/base/2026_09_06_03_era_talent_final_review_priest.sql`
(I-30): `spell_group` membership for the band clones (Power Infusion 920920/948039 → 1122 + 1123;
Inspiration **r1** 920987/948057 → 1095, mirroring stock, which groups r1 only) and the
`spell_linked_spell` row `(948040, 44416, 2)` that gives the TBC Pain Suppression clone the −5%
threat drop its stock donor gets from `(33206, 44416, 2)`.
