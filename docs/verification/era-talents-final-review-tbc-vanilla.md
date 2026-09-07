# Era Talents — Final All-Class Review (TBC + Vanilla + bots) — findings report

**Branch/HEAD:** audit @ `0a89029` (spec `9d224a7`, plan `8138576`); fix rounds b309dd6 … bbf0a6b; closing gate 2026-09-07 · **Generation:** audited `1de3388d` → **closing `70418133`** (install this `patch-V.mpq` + restage the addon before Phase G) · **Dates:** audit 2026-09-06, fixes 2026-09-06/07
**Spec:** `docs/superpowers/specs/2026-09-06-era-talents-final-all-class-review-design.md` · **Plan:** `docs/superpowers/plans/2026-09-06-era-talents-final-all-class-review.md`
**Method:** 22 read-only agents (Phase A gate; 9 TBC + 9 Vanilla per-class audits on four dimensions — fidelity, leak/grant, headless behaviour probe, gap re-open; bot path; cross-era; Vanilla regression). No file was changed by the audit; every finding below is a proposal for Phase F.

**Headline:** 0 Critical in TBC. **7 Critical in Vanilla** (all in classes that shipped before the TBC discipline: mage Combustion stack never expires; rogue `affects:` shared-bit leaks six spellmods + one proc onto every combo strike; shaman Earthbind/Magma totems pulse once; hunter Entrapment/Improved Wing Clip root is a caster-centred AoE; priest Inspiration never procs). **34 Important** (26 per-class/bot + 8 cross-cutting), ~90 Minor. Every headless probe (55 doctors across 18 datasets + ladders) returned 0 ORPHAN / 0 PROBLEM / 0 UNCLASSIFIED and `bandsweep 0 stripped`.

---

## 0. Headless integrity gate (Phase A) — GREEN

| Check | Result |
|---|---|
| `python3 tools/era_audit.py` | 0 finding(s) |
| `tools/` pytest (venv) | 245 passed |
| Regen determinism | tree clean before/after `tools/era-regen.sh`; stamp reproduces `1de3388d`; live `era_talent_meta` k=generation v=`1de3388d` |
| Node counts (YAML = generated SQL = live) | Vanilla 52/44/46/51/47/46/49/50/47 = **432**; TBC 66/64/64/67/64/61/67/64/62 = **579**; class 6 absent |
| `display_only` | 0 key hits (4 prose mentions); no such column |
| Id registry | 3548 custom `spell_dbc` ids in 920008-948969; 0 cross-dataset collisions; 0 out-of-window own ids; 932999 absent; 949060 absent; misc 19 unused; 16 dual-era reuse ids listed for D1 |
| Dangling references | 1291 YAML refs, 0 dangling; 0 SQL target problems |
| `spell_ranks` authored vs live | 445 = 445, 0 missing / 0 extra (BoS r5 `932617,946136,5` present) |
| `spell_proc` sanity | 564 module rows; the 10 HitMask=8-only rows are Magic Absorption (crit-meaningful); 17 Chance=0&PPM=0 rows all inherit DBC ProcChance 100; 0 heal rows lacking SpellTypeMask&2 |
| Totem creatures | 69 in [920100,920290] (120 in the widened band), identical to module SQL, none stock |
| Boot canary | `band classifier: 2562 node-grant ids indexed, 83 allowlist entries (allowlist sha 8b13166aba08)`; 0 build-order drift WARNs |
| Broad doctor sweep | 500 online bots ≤ 80 → 0 ORPHAN / 0 PROBLEM / 0 UNCLASSIFIED |

Recipe notes: `era_talent_meta` is key/value (`SELECT v … WHERE k='generation'`); backtick `rank` in `spell_ranks` queries (MySQL 8 reserved word — a bare `rank` silently returns nothing when stderr is discarded).

---

## 1. Per-class verdicts

| Era | Class | Verdict | C / I / M | Gaps (cheap / costly / keep / closed) | Probe |
|---|---|---|---|---|---|
| TBC | paladin | FINDINGS(9) | 0 / 4 / 5 | 2 / 1 / 7 / 1 | 3 tabs, 0/0/0 |
| TBC | druid | FINDINGS(4) | 0 / 0 / 4 | 0 (+4 doc) / 2 / 11 / 4 | 3 tabs, 0/0/0 |
| TBC | shaman | FINDINGS(7) | 0 / 1 / 6 | 1 / 2 / 9 / 11 | 3 tabs, 0/0/0 |
| TBC | warrior | FINDINGS(4) | 0 / 0 / 4 | 1 (marginal) / 3 / 4 / 1 | 3 tabs (54/66 live), 0/0/0 |
| TBC | rogue | FINDINGS(4) | 0 / 0 / 4 | 0 / 1 / 7 / 3 | 3 tabs, 0/0/0 |
| TBC | priest | FINDINGS(5) | 0 / 0 / 5 | 2 / 4 / 7 / 1 | 3 tabs, 0/0/0 |
| TBC | hunter | FINDINGS(5) | 0 / 1 / 4 | 2 / 2 / 7 / 4 | 3 tabs (MM partial), 0/0/0 |
| TBC | warlock | FINDINGS(6) | 0 / 0 / 6 | 1 (rec. keep) / 3 / 9 / 0 | 3 tabs, 0/0/0 |
| TBC | mage | FINDINGS(2) | 0 / 0 / 2 | 2 / 4 / 5 / 0 | 3 tabs, 0/0/0 |
| Van | warrior | FINDINGS(4) | 0 / 1 / 3 | 0 / 3 / 5 / 1 | 3 tabs, 0/0/0 |
| Van | paladin | FINDINGS(8) | 0 / 5 / 3 | 8 / 0 / 3 / 2 | 3 tabs, 0/0/0 |
| Van | hunter | FINDINGS(6) | **1** / 0 / 5 | 2 / 0 / 8 / 1 | 3 tabs (37/46 live), 0/0/0 |
| Van | rogue | FINDINGS(6) | **2** / 1 / 3 | 2 / 0 / 3 / 2 | 3 tabs, 0/0/0 |
| Van | priest | FINDINGS(9) | **1** / 4 / 4 | 2 / 0 / 4 / 2 | 3 tabs, 0/0/0 |
| Van | shaman | FINDINGS(9) | **2** / 2 / 5 | 4 / 0 / 7 / 3 | 2 tabs live + Elemental headless, 0/0/0 |
| Van | mage | FINDINGS(7) | **1** / 2 / 4 | 0 / 1 / 4 / 3 | 3 tabs, 0/0/0 |
| Van | warlock | FINDINGS(4) | 0 / 1 / 3 | 1 / 1 / 6 / 3 | 2 tabs live + Destruction static, 0/0/0 |
| Van | druid | FINDINGS(6) | 0 / 1 / 5 | 2 / 1 / 7 / 1 | 3 tabs, 0/0/0 |
| — | bot path | FINDINGS(7) | 0 / 3 / 3 (+1 info) | — | 2 ladders L60→70→reset→71→80→70 clean; §9.6 CLOSED-AS-EXPECTED |
| — | cross-era (D1) | FINDINGS(24) | 2 (dupes) / 13 / 9 | — | 13 sweeps; X10-X13 clean |
| — | Vanilla regression (D2) | GREEN — FINDINGS(2) | 0 / 0 / 2 | — | 9 classes 0/0/0; 9 L80 bots only allowlisted markers; 63 sweep strips all ORPHAN_ALLOWLIST, 0 UNCLASSIFIED |

---

## 2. Merge-gate findings — Critical (fix before merge)

### F-1 · mage Combustion clone (Vanilla 932325 **and** TBC 948887): stacked +crit buff never removed — **Critical, both eras**
`spell_linked_spell` holds only `-11129 → -28682` (remove the +10%/stack crit aura when stock Combustion ends). No row exists for either clone and no module SQL creates one. 28682 is `CumulativeAura 10, DurationIndex 21` (infinite): after the third crit the stack is permanent → up to +100% Fire crit for the rest of the session. `spell_mage_combustion_proc` only removes 11129 (the reverse direction). Proven by DB + core-code inspection; in-combat confirmation is real-client.
**Fix:** `INSERT INTO spell_linked_spell VALUES (-932325,-28682,0,'Era Vanilla Combustion'),(-948887,-28682,0,'Era TBC Combustion')` in the mage hand SQL. Real-client re-test: yes (3 crits, watch 28682 fall off).

### F-2 · Vanilla rogue: `affects:` shared family bit 23 binds six spellmods to every combo strike — **Critical**
`_resolve_affects_mask` ORs each named spell's FULL live `SpellClassMask_1`; family-8 word1 bit 23 (8388608) is shared by Sinister Strike 1752, Backstab 53, Ambush 8676, Eviscerate 2098, Hemorrhage 16511 (+ clones 932985-7); `IsAffected` is ANY-bit. Emitted masks for 18401 Improved Eviscerate (923208), 18409 Lethality (923272/276), 18419 Improved Backstab (923352/354), 18433 Aggression (923464/466), 18436 Opportunity e1 (923488/490), 18442 Improved Ambush (923536/538) all contain bit 23; stock analogs do not. Net: Improved Ambush 3/3 = +45% crit on SS/Backstab/Eviscerate/Hemorrhage, etc.
**Fix:** author `affectsMask:` with the unique bits — 18401 `{a:131072}`, 18409 `{a:1174405134}`, 18419 `{a:4}`, 18433 `{a:131078}`, 18436 e1 `{a:516}`, 18442 `{a:512}`. Real-client: yes (crit/damage on SS vs Ambush).

### F-3 · Vanilla rogue 18440 Initiative: proc mask leaks the same bit 23 — **Critical**
`spell_proc` 923520-522 `SpellFamilyMask0 = 8390400` (bits 8,9,10,23) → the bonus combo point fires on SS/Backstab/Eviscerate/Hemorrhage, not just Ambush/Garrote/Cheap Shot. **Fix:** `proc.affectsMask: {a: 1792}`.

### F-4 · Vanilla shaman Earthbind Totem (creature 920100): pulse cast once, no driver — **Critical**
`creature_template_spell` Index0 = the slow FIELD 931010 (DurationIndex 28 = 5 s); stock 2630's Index0 is the DRIVER 6474 (aura 23 periodic trigger @3 s). `Totem::InitSummon` casts m_spells[0] once → the snare dies ~40 s into a 45 s totem. Banner-documented in `shaman.yaml:2813` as "SHIPPED, NOT FIXED". **Fix:** mint a Vanilla driver clone (aura 23 PERIODIC_TRIGGER_SPELL @3000 → 931010) in a free 931011-931019 slot and make it Index0, mirroring TBC 947001.

### F-5 · Vanilla shaman Magma Totem (creatures 920160-163): same defect — **Critical**
Index0 = bare SCHOOL_DAMAGE pulses 931130-133; stock 5929 uses DRIVER 8188 @2 s → ~1 of 10 ticks lands. Banner `shaman.yaml:3542`. **Fix:** 4 drivers from 931134-931139; fix together with F-4 (one defect class). Real-client: yes for both (watch the pulse repeat).

### F-6 · Vanilla hunter Entrapment 932972 / Improved Wing Clip 932978: root is a caster-centred AoE — **Critical**
Both clone 19185 authoring only `targetA: 22`, so they inherit `TargetB 15` + `EffectRadiusIndex 13` (10 yd) → the root is an AoE centred on the HUNTER: it roots unrelated mobs within 10 yd and misses the trapped/clipped victim when the hunter is farther than 10 yd (the kiting case). The core's by-id `SpellInfoCorrections.cpp:691-697` fix never reaches clones; the TBC dataset already ships the fix (948343/948344) and its comment names 932972 as the outstanding one. **Fix:** 932972 `targetB: 16, radius: 13, attributesEx5: 67108864`; 932978 `targetB: 0, radius: 0, attributesEx5: 0`. Real-client: yes.

### F-7 · Vanilla priest 18123 Inspiration: never procs — **Critical**
`spell_proc` 920984-6 `ProcFlags 1024` (DONE_SPELL_NONE_DMG_CLASS_POS); every affected heal is DmgClass MAGIC and the core emits 0x4000 for a positive magic spell → the mask never matches. Stock 14892 = 87376; the TBC twin 940304-6 and every other era heal-crit proc use 16384. Live doctor shows `flags=1024`. **Fix:** `proc.flags: 16384`. Also the helpers 920987-9 are wrong (see I-23). Real-client: yes.

---

## 3. Merge-gate findings — Important (fix or explicitly accept)

### TBC
- **I-1 paladin** seals SoJ 946063/946079, SoB 946080, SoV 946084 ship `ManaCost 0 / ManaCostPct 0`; every other TBC seal has a cost. Fix: `manaCostPct` from wago 2.5.4 SpellPower (SoJ 10; SoB/SoV to be fetched). *wago SpellPower CSV is not committed — fetch before fixing.*
- **I-2 paladin** 20063 Crusader Strike 946114 `RecoveryTime 4000` inherited from WotLK 35395; YAML asserts 6 s; no `cooldown:` authored. Fix: fetch wago cooldown, add explicit `cooldown:`.
- **I-3 paladin / bots** Alliance bot holds BOTH 946080 SoB and 946084 SoV after level-change or randomize: `StripWrongFactionTbcPalSeal` runs only from OnBotLogin (EraTalentBots.cpp:377) and mid-reconcile (EraTalents.cpp:1252), both before the factory trainer re-walk; `OnBotLevelChanged` never calls it, and in the fork `PlayerbotFactory.cpp:722 InitTalentsTree` precedes `:736 InitAvailableSpells`. Fix: call it at every `OnBotLevelChanged` exit when era==TBC AND in patch 0020 after `PlayerbotFactory.cpp:736` (not at the end of FactoryReconcile).
- **I-4 paladin** SoL 946034-038 / SoW 946050-053 `spell_proc Chance 100` vs stock 20165 PPM 10 / 20166 PPM 12. Fix: `proc.ppm` (drop `chance`).
- **I-5 shaman** Totemic Call 36936 live `ReqLevel 61`: the `_ref` retraction (gap id 6, "already TBC-correct at 30, IP line commented out") read `class_trainers.sql:114` but IP ALSO ships the active `zz_optional_limit_spells_to_expansion.sql:26` UPDATE → 61. Fix: correct `_ref`/gap 6; a real L30 re-gate needs a module UPDATE ordered after IP's `zz_` file.
- **I-6 hunter** 20645 Entrapment root 948343 ships the core-fixed AoE shape (targetA 22/B 15/radius 10 yd) where the number of record is single-target (targetA 6); sibling 948344 zeroes them; justified only in a comment, no gap entry. **Design call:** the core makes stock 19185 AoE in every era, so an era-literal root is strictly weaker than stock — either author `targetA 6/targetB 0/radius 0` or add gap 13 recording the deliberate choice.

### Vanilla
- **I-7 warrior** 18552 Shield Slam grants stock 23922 (294-308) vs tooltip 225-235; era ranks already recorded at EraTalents.cpp:1023; TBC clones 947541-546. Fix: Vanilla clone (bp 224 d15, family 4 + word2 512), regrant, stale-23922 strip arm.
- **I-8 paladin** 18642 Sanctity Aura 932602 inherits Devotion Aura's unique identity bit 64 → Improved Devotion Aura (affectsMask a:64) scales Sanctity's Holy% at 5/5. Fix: `mask: 0` (keep maskC 32). *Also the reuse id TBC paladin references — cross-era.*
- **I-9 paladin** 18637 Seal of Command: Effect_3 DUMMY → 932607 (the 70% weapon proc) so Judgement of Command is a 70% weapon hit, not the tooltip's 68-74 Holy (×2 stunned). Fix: 5 flat JoC clones (mirror TBC 946103-108).
- **I-10 paladin** 18609 Illumination grants stock 20210-215 (Effect_2 29 → 30% refund; core reads the aura's own Effect_2); tooltip promises 100%. Fix: 5 clones Effect_2 = 100 + `scriptBindings` to `spell_pal_illumination` + proc rows. **Same shape in TBC (60%) — see ledger G-TBC-pal-1.**
- **I-11 paladin** Seal of Justice 932724 free (`ManaCost 0 / Pct 0`), only free seal; YAML rationale wrong (stock 20164 = Pct 14). Fix: cost from 1.12.1 client Spell.dbc.
- **I-12 paladin** SoL 932725-728 / SoW 932737-739 `Chance 100` vs stock PPM 10/12. Fix: `proc.ppm` (owner call on the 1.12 rate — server-side column, not in the client DBC).
- **I-13 rogue** 18430 Sword Specialization era proc rows `Cooldown 0` vs stock `-12281 Cooldown 6000` → chain-procs every swing. Fix: `proc.cooldown: 6000`.
- **I-14 shaman** every friendly buff/heal totem pulse ships `EffectRadiusIndex 10` = **30 yd** (WotLK) where `_ref/shaman-totems.yaml:64` records index 9 = 20 yd; YAML comments call idx 10 "the 20yd index". Fix: `radius: 9` on the friendly pulses + 53 "within 30 yards" strings → 20.
- **I-15 shaman** 18830 Stormstrike 932405 `ProcCharges 4` (WotLK) vs its own "next 2 sources" text and 1.12. Fix: `procCharges: 2`.
- **I-16 mage** 18028 Master of Elements 920224-226 ship `AttributesMask 0` and no CAN_PROC_FROM_PROCS, so triggered Blizzard damage (42208) never refunds (siblings 18037/18042/18048 author `attrMask: 2`). Fix: `attrMask: 2`.
- **I-17 mage** `spell_group` never swept: Arcane Power clone 932760 lost groups 1107 + 1123 (stacks with Power Infusion/Unholy Frenzy/ToT); Winter's Chill 932764 lost group 1037 (stacks with native 12579). Fix: `spell_group` rows in the mage hand SQL. **Cross-cutting — D1 X2.**
- **I-18 warlock** 18201 Suppression is `stat` aura 55 + `school: 32`, but aura 55 ignores misc (UpdateSpellHitChances is a single scalar) → +10% hit on Fire spells too. Fix: re-author as spellmod `op: resistMiss` (SPELLMOD_RESIST_MISS_CHANCE 16, one new OP entry in gen_era_talents.py:90) over the Affliction set. **Same encoding ships for priest Shadow Focus — D1 X3.**
- **I-19 druid** 18712 Nature's Grace 932504 inherits WotLK's nature-caster masks (A_1 16777831 / B_1 1); tooltip says "your next spell"; Vanilla NG was unrestricted → never reaches GotW/Rebirth/Revive/Hibernate and an unmasked cast does not consume the charge. Fix: per-effect `maskA/B/C: 0xFFFFFFFF` on 932504, or narrow both tooltips.
- **I-20 priest** 18115 Power Infusion 920920 sets `ManaCost 182` but inherits `ManaCostPct 16` → ~500 mana at 60. Fix: `manaCostPct: 0`.
- **I-21 priest** 18106 Martyrdom 920848/9 `ProcFlags 136` misses TAKEN_SPELL_MELEE/RANGED → boss ability crits never proc; stock 680 (sibling Blessed Recovery uses 680). Fix: `proc.flags: 680`.
- **I-22 priest** 18132 Spirit Tap 921056-60 `AttributesMask 0`; tooltip "yields experience"; TBC authors `attrMask: 1`. Fix: `proc.attrMask: 1`.
- **I-23 priest** Inspiration helpers 920987-9 ship aura 87 MOD_DAMAGE_PERCENT_TAKEN −8/16/25% (flat physical cut) vs tooltip "armor +8/16/25%"; era records (TBC `_ref` reuse_ledger) are aura 101 MOD_RESISTANCE_PCT misc 1 +8/16/25; the 2026-08-14 audit cleared it against WotLK 15359 (wrong era). Fix: aura 101 misc 1 vals 8/16/25.

### Cross-cutting (D1)
- **I-27 mage, both eras** every node whose `affects:` names Blizzard binds Frost Armor + Ice Armor through shared family bit 19 (van 18028 MoE, 18036, 18037, 18040, 18042, 18043, 18044, 18048; tbc 20854, 20856). Fix: `affectsMask` using Blizzard's unique bit 128 — 18042/20854 `{a:128}`, 18036/18037/18040/18048 `{a:672}`, 18043 `{a:160}`, 18044 `{a:736}`, 20856 `{a:736,b:16385}`, 18028 `{a:4195063,b:64}`.
- **I-28 mage, both eras** Frost Warding (van 18033 / tbc 20845) ORs Frost+Ice Armor identities incl. bit 19 → boosts Blizzard. Fix: `affectsMask: {a: 33554432}`.
- **I-29 mage, both eras** Permafrost (van 18039 / tbc 20851) binds Mage Armor 6117 via bit 268435456 carried by the Chill/Chilled helpers. Fix: van `{a:544}`, tbc `{a:1049120}`.
- **I-30 spell_group losses (both eras)** Winter's Chill/Fire Vulnerability → 1037 (stack with ISB and each other); PI/AP → 1107/1122/1123 (PI+AP+Bloodlust stack); TBC Aimed Shot 948370 → 1061 (mortal-wounds double-applies with MS/Wound Poison); BoS clones → 1007; plus the 12 listed in §6 X2; TBC Pain Suppression 948040 lost the 33206→44416 threat-drop link. Fix: mirror every clone into its template's `spell_group` rows (+ `spell_linked_spell (948040, 44416, 400000)`) in hand SQL.
- **I-31 Vanilla shaman** 42 totem summons `DurationIndex 5` = 5 min vs the tooltips' 2 min; TBC uses index 4 (120 s) for the same totems. Fix: `durationIndex: 4` (resolves M-47).
- **I-32 Vanilla hunter** 18329 Ranged Weapon Specialization 922632-636: `school: 127` with no equip gate → +1-5% to ALL damage of ALL schools (stock 19507 / TBC 941080-084 gate on ranged). Fix: add the equip gate.
- **I-33 framework registry** shaman TBC row "947442-947511 free within slice" is false — 947442-945 are live; totem count 157 not 146. Fix: correct the row (a re-mint there would collide).
- **I-34 aura 55 `school:` no-op** also on van priest 18136 Shadow Focus, van mage 18002 Arcane Focus + 18035 Elemental Precision (I-18 covers warlock). Fix: same `resistMiss` spellmod re-authoring, one new OP entry shared by all four.
- **Provenance (X9):** all nine Vanilla `_ref` files are 3.3.5a id/mask references with no 1.12 magnitudes — every Vanilla value rests on authoring comments. Not a fix item; a standing caveat for the ledger and for any future Vanilla re-verification.

### Bot path
- **I-24** (= I-3) dual-faction TBC seals — fix location above.
- **I-25** `era_talent_weapon_gate::OnPlayerCanUseItem` bails `IsBot` (EraTalentPin.cpp:343) → Vanilla shaman bots bypass the 2H axe/mace talent gate: 7 of 11 Vanilla-band shaman bots wield a 2H mace/axe without 932406. Fix: drop the IsBot bail (keep a `BotTalents()` guard), `EraFromIP`→`EraFor` at :350.
- **I-26** bracket DOWN-move keeps post-era TRAINER-TAUGHT stock spells and gear (D1 X13 confirms node-granted stock IS reset; this is the trainer-taught class): teardown/Sweep cover only the band; 42985 (TrainerId 16 ReqLevel 77) known by 4/5 TBC-band mages; 47280 ×10, 51419/55268 ×5 on ≤70 bots; 6 sub-71 bots hold req≥71 gear (Mana Sapphire 33312 req77 ×4). Fix: down-move teardown strips `character_spell` rows whose min `trainer_spell.ReqLevel` > level. **Related: D1 X13 (stock-grant strip on era exit for players).**

---

## 4. Minor findings (fix in the class round when the class is touched, else next pass)

| # | era/class | node / scope | evidence | fix |
|---|---|---|---|---|
| M-1 | TBC paladin | 20048 Conviction | 5 tooltips begin "Weapon " (importer junk) | drop |
| M-2 | TBC paladin | 20049 SoC tooltip | "(1s cooldown)" trailer | strip |
| M-3 | TBC paladin | 946136 BoS r5 | description "5 min" vs DurationIndex 6 = 10 min | "10 min" |
| M-4 | TBC paladin | 20016 Holy Shock yaml:451 | "ranks 2-5 not yet cloned" stale | delete |
| M-5 | TBC paladin | record §9 | misc 17 consumed / helpers to 946140 not recorded | fix record + registry |
| M-6 | TBC druid | embedded vs standalone `_ref.accepted_gaps` | 3 gaps only in standalone (tree_of_life_form_properties, natural_perfection_periodic_bit, furor_wotlk_extra_effect) | merge; embedded = origin |
| M-7 | TBC druid | 20118 Imp Faerie Fire gap + record §6 | says "shipped inert"; fix round replaced with 946277-279 | rewrite CLOSED |
| M-8 | TBC druid | 20112/20101/20149 gaps | closed by 946268/946270-275/946269, still open in text | mark WITHDRAWN |
| M-9 | TBC druid | 20110 Celestial Focus 936880-2 | spellmod e1 / proc e2 reversed vs stock; inert (nothing binds SPELLMOD_EFFECT2/3) — same in Vanilla 925688-692 | comment note |
| M-10 | TBC shaman | Fire Nova summons 947310-316 | BaseLevel=SpellLevel 10 from template 3599 (Vanilla 931353-357 too) | per-rank `spellLevel`/`baseLevel` |
| M-11 | TBC shaman | 20248 Totemic Mastery tooltip | says 30 yd, delivers 40 (pulses idx 10 + flat +10) | reword (or fix with I-14's radius change — then 30 is right) |
| M-12 | TBC shaman | `_ref` open_questions 16246 / 39610 | superseded decisions (clone 947444; clone 947340) unrecorded | add lines |
| M-13 | TBC shaman | `_ref` ungated_trainer_rows[8232] | Windfury Weapon r1-r4 stock by design (gap 1); reads as open leak | add Task-7 disposition |
| M-14 | TBC shaman | 20208 yaml comment | "Plan 3b must extend reader" — extended (EraTalentTotemScripts.cpp:32-33/152-155) | update |
| M-15 | TBC warrior | EraTalentProcScripts.cpp:1870-1871 | header "bails … on bots" (forbidden wording; code correct) | delete phrase |
| M-16 | TBC warrior | :1859, :1986/:1999 | Vanilla-only scope claims; also serve TBC 20337 / 947547 | widen |
| M-17 | TBC warrior | record §12 | kAuthoredOrders era-blind / drift wall — stale since 2026-09-03 | strike |
| M-18 | TBC warrior | 947561-565 proc rows | SpellPhaseMask 0 (taken-block) undocumented | comment |
| M-19 | TBC rogue | record §14 b1/b2 | stale (orders exist; Sword Spec alias fixed §17) | rewrite / delete |
| M-20 | TBC rogue | `_ref` 20419/20444 cross-ref | Find Weakness mask ≠ Surprise Attacks (SnD excluded) — data right, sentence wrong | correct |
| M-21 | TBC rogue | `_ref` 20428 cross_era_note | "Vanilla 18423 NOT fixed" — fixed (932989) | CLOSED |
| M-22 | TBC priest | 948054 Lightwell r4 | maxLevel 74 vs wago 76; banner claims windows identical | `maxLevel: 76` |
| M-23 | TBC priest | 948056 Holy Concentration | DefenseType 0 vs wago 1 | `defenseType: 1` |
| M-24 | TBC priest | 16 templated clones | SpellClassMask_3 1024 inherited (measured inert) | `maskC: 0` or banner |
| M-25 | TBC priest | 20507 Inner Focus | stock 14751 WotLK-only ATTR3/ATTR4 not in ledger | gap 13 + §9 row |
| M-26 | TBC priest | CLAUDE.md Phase-9.5 para | "priest tail grant-only" — strips STOCK ids (invariant intact) | reword |
| M-27 | TBC hunter | 20602 Focused Fire tooltip | promises unshipped KC crit half (gap 12) | trim |
| M-28 | TBC hunter | 20631 Concussive Barrage comment | "no TBC family mask" contradicts _ref (mask 0x1000; proc row supersedes) | reword |
| M-29 | TBC hunter | record stamp | 76709510 ×4 vs live 1de3388d (incl. §19 MPQ instruction) | restamp (D1 lists all stale records) |
| M-30 | TBC hunter | gap 11 | split: Scatter Shot half CLOSE-cheap (948382 reserved-free), Intimidation KEEP | split |
| M-31 | TBC warlock | 20757 Nether Protection 942056-058 | spell_proc SpellTypeMask 1 (DAMAGE default) vs "after being hit" → non-damaging spells can't arm it | `typeMask: 0` (**data fix**) |
| M-32 | TBC warlock | 948425-427 comments | trained-level comments say 31/43/55; rows 36/48/60 (correct) | fix comments |
| M-33 | TBC warlock | record "Known open items" | band sweep listed as scheduled — landed | close → phase9-5 record |
| M-34 | TBC warlock | spec A.1 Amplify Curse / Fel Domination rows | shipped shape differs (DUMMY + script; hitMask 0) | amend |
| M-35 | TBC warlock | 948650-655 Conflagrate | EffectAuraPeriod_2 2000 residue with Effect_2 0 | note / zero in 0-fill |
| M-36 | TBC mage | 20834 Master of Elements | stock 29074-076 carry CAN_PROC_FROM_PROCS (TBC 0) — unadjudicated | gap 13 or clone with attributesEx3 0 |
| M-37 | TBC mage | 20822 Slow 948773 | EffectMechanic_1 11 kept alongside spell Mechanic 11 (no behavioural diff) | `mechanic: 0` on e1 or drop |
| M-38 | Van hunter | 18325/18327 comments | "clone is family 0" retired; gap survives because stock 19485 maskA_3 lacks bit 21 | correct; optional authored critDamage spellmod |
| M-39 | Van hunter | 18326 Scatter Shot comment | claims stock free; live ManaCostPct 8 (~138 vs 1.12 ~123) | correct |
| M-40 | Van hunter | 932967 Bestial Wrath spellFixAck | "no key for AttributesEx4" — `attributesEx4`/`attributesEx2` exist | author both |
| M-41 | Van hunter | 18321/18330 comments | name retired kHunterTrainedChains | reword (band sweep) |
| M-42 | Van hunter | record | stamp 41c1d3b0; describes retired IAotH/Quick Shots model; gap 3 stale | refresh |
| M-43 | Van rogue | 18443 Setup 923544-546 | Cooldown 0 vs stock 1000 (undocumented) | `proc.cooldown: 1000` or document |
| M-44 | Van rogue | 18444 Imp Sap | stale spell_proc rows 923552-554 from the retired proc design survive migrated DBs (generator only DELETEs re-inserted ids) | hand DELETE or generator per-passive DELETE |
| M-45 | Van rogue | record :203/:220, TBC `_ref`:1385 | stale designs / "NOT fixed" | refresh |
| M-46 | Van shaman | summons FN/WF/GoA/Windwall/Mana Tide/PC | SpellLevel/BaseLevel from wrong-rank donor | per-rank keys |
| M-47 | Van shaman | 12 summon families DurationIndex 5 | idx 5 = 300 s; 41 tooltips say "2 min"; 4 comments assert idx 5 == 2 min; 1.12 duration not in _ref | determine 1.12 → idx 4 or fix tooltips |
| M-48 | Van shaman | 18845 Mana Tide | DurationIndex 564 = 13 s vs "12 sec" | fix |
| M-49 | Van shaman | 18826 yaml:3645 | "cannot boost — do not chase" stale (era_sha_flametongue_totem_proc reads 926608/926609) | replace |
| M-50 | Van shaman | FT 931144-147 / Stoneclaw 931070-075 | trigger TBC-band 947135-138 / 947062-067 (deliberate cross-era coupling) | record it |
| M-51 | Van shaman | framework:54 | FT pulseBase 931150 vs file 931144-151 | amend |
| M-52 | Van warrior | record :249-251 | Defiance bullet stale (closed 2026-08-24); "unmerged" ×2 | fix |
| M-53 | Van warrior | EraTalentProcScripts.cpp:1870 + warrior.yaml:470 | "bails on bots" wording | rewrite |
| M-54 | Van warrior | dev-box spell_dbc 924288-292 | orphan pre-Task-12 passives (no repo reference) | drop on dev box / range DELETE |
| M-55 | Van paladin | framework:245 | kPalTrainedChains retired | drop |
| M-56 | Van paladin | seal/HS/HShield/BoS chains | SpellLevel identical across ranks | per-rank `spellLevel` |
| M-57 | Van paladin | 18634 Deflection 925072-076 | EffectMiscValue 127 (default when `school:` omitted) — harmless for aura 47, latent | `school: 0` / default 0 |
| M-58 | Van warlock | 932940-944 comment | "DurationIndex 9 = 12s" wrong twice (rows idx 29 = 12 s; idx 9 = 30 s) | fix comment |
| M-59 | Van warlock | yaml:699 | ReqAbility1 932930 → live 932994 | update |
| M-60 | Van warlock | 932913 Imp Drain Soul buff | no `client:` block → no icon | add |
| M-61 | Van mage | doctor heuristic EraTalentsCommand.cpp:203 | "hitMask=8 never fires" false positive (Magic Absorption; TBC 948762-766 too) | narrow |
| M-62 | Van mage | doctor Vanilla arm | no Shatter M==(R?1:0) predicate / no STALE STOCK 12042/12043 line | mirror TBC arm |
| M-63 | Van mage | 932990/932991, 932980-982 | no `client:` / no descriptions | add (generation bump) |
| M-64 | Van mage | trained_chains.sql:142 + record:104 | name a non-existent "920040-920044 strip arm" | reword (band sweep + `stranded:`) |
| M-65 | Van druid | record gap #9 / allocation | Imp Enrage stale; "next free 932506" (actually 932514) | fix |
| M-66 | Van druid | framework:244 | NG proc flags 327680 vs shipped 81920 | correct |
| M-67 | Van druid | embedded `_ref` | PRE-CORRECTION snapshot; generator makes inline copy WIN (gen:1760-1764) | re-sync (**D1 X4 for all datasets**) |
| M-68 | Van druid | CLAUDE.md 0023 row + regen-hotw-patch.sh:27 | cite our own tooltip rewrite as 1.12 evidence (circular) | restate |
| M-69 | Van priest | 932970 Blackout | DefenseType 0 (unresistable); no descriptions | `defenseType: 1` + text |
| M-70 | Van priest | 920987-9 | no `client:` block | add |
| M-71 | Van priest | 18107 Inner Focus | ATTR3/ATTR4 unrecorded | note |
| M-72 | Van priest | VE resolver | scanForName picks highest id → hidden passive 932950 (consumers only test != 15286; latent; TBC same) | prefer non-passive |
| M-73 | bots | doctor `storedEra` | dead on bot path (only EraTransition writes it; every entry IsBot-bailed) | label "(player-path only)" |
| M-74 | bots | record phase9-5 §9.6 | syncone is a console no-op; W1 re-spend = background randomizer | CLOSED-AS-EXPECTED + randomizer note |
| M-75 | bots | regen-botera-patch.sh:7,57,62 + CLAUDE.md 0020 row | claim 0014 touches AiFactory.cpp (0 hunks) | drop line / "0003/0004" |
| M-76 | bots | patches 0003/0014 | fail `apply --reverse --check` (adjacent-context edits by 0004/0016); content intact; apply_pin resets first | none (latent if pins removed) |

---

## 5. Consolidated gap ledger (ONE owner decision)

Classification: **CLOSE-cheap** = YAML/data only with existing generator keys, at most new ids from the class's own slice; **CLOSE-costly** = new script / core patch / new misc / generator feature / owner value call; **KEEP** = still unachievable or deliberately declined. Findings F/I above are NOT repeated here except where they double as a gap. OWNER DECISION column is blank for you.

| # | Era | Class | Node | Gap | New class | Proposed fix / keep reason | OWNER |
|---|---|---|---|---|---|---|---|
| G-1 | TBC | pal | 20008 Illumination | 30% refund vs TBC 60% | CLOSE-cheap | 5 helper ids 946141+ templated on 20210-215, Effect_2 59, scriptBindings spell_pal_illumination, proc row, re-point grants | |
| G-2 | TBC | pal | 20060 Repentance | 1 min / broad types vs 6 s Humanoid | CLOSE-cheap | one helper clone (6 s, TargetCreatureType humanoid) — needs new `_HELPER_SCALARS` `targetCreatureType` (shared with G-31) | |
| G-3 | TBC | pal | 20049 JoC 946103-108 | stun-doubling | CLOSE-costly | new SpellScript | |
| G-4 | TBC | pal | 20029/20031/20056/20062/20047/20003/20044-58 | Stoicism dispel half, Imp Conc rider, Imp Sanctity 932602 shared bit, Fanaticism RF exception, Vindication 30%, Imp SoR flat, attributesEx3 | KEEP | as recorded (Imp Sanctity's shared bit is I-8's fix) | |
| G-5 | TBC | dru | 20126 Feral Swiftness | dodge scope one-form | CLOSE-costly | 2 helpers + kEraDruidTbcPaired arm | |
| G-6 | TBC | dru | 20139 Imp LotP | mana half in core script | CLOSE-costly | era copy of spell_dru_leader_of_the_pack | |
| G-7 | TBC | dru | 11 others | moonkin aura, insect swarm line, enrage drawback, FoN, PS moonkin, FF feral substrate, feral instinct, nurturing instinct, natural perfection bit, furor e2, ToL form props, spell_group/required inheritance (moot) | KEEP | | |
| G-8 | TBC | sha | weapon imbues (spells id 1) | FT/Frostbrand/Rockbiter stock WotLK | CLOSE-costly | 3 chains × 5-9 clones + spellitemenchantment re-points + recreated script binds — **user decision** | |
| G-9 | TBC | sha | Totemic Call (totems id 6) | doc wrong (I-5) | CLOSE-cheap (doc) / costly (re-gate) | | |
| G-10 | TBC | sha | Rockbiter flat, Restorative Totems double-apply, WF AP slot, raid/30yd pulses (owner B.8), WF Totem chain, off-hand procs | KEEP | | | |
| G-11 | TBC | war | stance passives 7376/7381 + Battle Stance arpen | core hard-casts global ids | CLOSE-costly | module shapeshift hook + band-gated correction auras, EraFor-gated (shared with G-33) | |
| G-12 | TBC | war | 20343 Rampage ready-aura 947540 | not cleared after cast | CLOSE-costly | ~15-line SpellScript AfterCast | |
| G-13 | TBC | war | 20318 Blood Frenzy | stock 30069/30070 lack Mechanic 15 | CLOSE-cheap (marginal) | 2 clones 947568/947569 with `mechanic:` | |
| G-14 | TBC | war | Devastate Sunder id, Mace Spec chances, Unbridled Wrath ladder, 12328 kept in WotLK | KEEP | | | |
| G-15 | TBC | rog | 20433 Mace Spec aura 163 | not weapon-gated | CLOSE-costly | band-gated core guard modelled on 0025 at Unit.cpp:1553/1835/9456 (identical to stock 13709) | |
| G-16 | TBC | rog | poison payloads, Wound Poison, Cold Blood Shiv, baseline Stealth/Vanish, Mutilate residual, DefenseType, PREVENTS_ANIM | KEEP | | | |
| G-17 | TBC | pri | 948054 maxLevel 76; 948056 defenseType 1 | data nits | CLOSE-cheap | one scalar each | |
| G-18 | TBC | pri | Holy Nova r7 25331; Lightwell charges/cancel; attributesEx4/6 key; Inner Focus bits | | CLOSE-costly | | |
| G-19 | TBC | pri | baseline PW:S/Mass Dispel, Devouring Plague, Surge of Light ICD, Pain Suppression threat, Misery aura-112, Blackout ProcTypeMask | KEEP | | | |
| G-20 | TBC | hun | Scatter Shot (gap 11 half) | 8% vs 6% mana | CLOSE-cheap | clone at reserved 948382, manaCostPct 6, auraInterruptFlags 0x2 | |
| G-21 | TBC | hun | 948290 Bestial Wrath | attributesEx4 0x4 | CLOSE-cheap | one line | |
| G-22 | TBC | hun | Frost Trap Aura 13810 −60%; Focused Fire KC half | | CLOSE-costly | GO indirection + baseline exception; paired arm + damaging KC | |
| G-23 | TBC | hun | Imp FD, Frost Trap Slow 67035, aura 52/79 scope, Aimed Shot debuff duration, Trueshot curve, Intimidation half | KEEP | | | |
| G-24 | TBC | wlk | Dark Pact range 30 vs 100 yd | | CLOSE-cheap (rec. KEEP) | 4 clones + chain; unobservable (own demon) | |
| G-25 | TBC | wlk | Conflagrate CU attr (SpellMgr.cpp:3702 by-id — **framework: era_audit blind to SpellMgr.cpp**); auraInterruptFlags 0x80000; UA AttributesEx6 | | CLOSE-costly | | |
| G-26 | TBC | wlk | stones 1.12 values (+ record sentence: marker 13 scales ABSORB not 932949 crit), renames, flat-mana bits, Nightfall ICD, Shadow Embrace duration, Siphon Life 18265, Master Firestone shape, procCharges inert, Soul Siphon misc | KEEP | | | |
| G-27 | TBC | mag | 948760 Arcane Power word1 0x19048 (bit 6 Blast Wave; TBC 0x8); 948887 Combustion proc mask incl. Blast Wave | | CLOSE-cheap | per-effect maskA/B/C + proc.affectsMask in LIVE encoding (remint judgement) | |
| G-28 | TBC | mag | Molten Fury 35→20% (Unit.cpp hardcode; owner said no core patch); Ice Block Hypothermia; Combustion cancel script; Ice Barrier CheckCast companion | | CLOSE-costly | | |
| G-29 | TBC | mag | flat-mana bits, Molten Shields, spellbook tooltips, dropped live halves, retired ids, unbuilt nodes | KEEP | | | |
| G-30 | Van | war | stance passives (7376 −10/−5/+45 vs Vanilla −10/−10/+30; 7381 +3/+5/−20 vs +3/+10/none) — UNRECORDED | | CLOSE-costly | same hook as G-11; at minimum record | |
| G-31 | Van | war | rage-cost tooltip rewrite (addon RewriteCost Mana-only) | | CLOSE-costly | addon feature, class-wide | |
| G-32 | Van | war | Imp Taunt, Imp Shield Block (procCharges would nerf), Imp Bloodrage flat, Imp Hamstring spellFixAck, Imp Charge verify | KEEP | add Charge +3/+6 to real-client checklist | | |
| G-33 | Van | pal | 18644 Repentance | 60 s / 118 types vs 6 s / Humanoid | CLOSE-cheap | clone 20066 (free ids 932600/601/666/673-699) + new `targetCreatureType` scalar | |
| G-34 | Van | pal | 18637 SoC `chance: 40` "≈7 PPM"; SoJ stun `chance: 25` placeholder | premise stale — `proc.ppm` exists | CLOSE-cheap | `ppm: 7` / anchor 5 (owner value call) | |
| G-35 | Van | pal | Imp Conc rider; SotC flat AP (`perLevel:` per-effect EXISTS per rogue agent — re-check); Vindication 30% | KEEP | | | |
| G-36 | Van | hun | Entrapment/Imp Wing Clip shape (F-6); Bestial Wrath attributesEx4/Ex2 | | CLOSE-cheap | | |
| G-37 | Van | hun | Imp FD, Bestial Swiftness outdoor, Lethal Shots/RWS scope, Clever Traps/Trap Mastery 67035, Trueshot caster-extra, Imp Scorpid player-only, Intimidation 8% (=137.6), Scatter Shot (no 1.12 cost in _ref) | KEEP | | | |
| G-38 | Van | rog | 18445 Serrated Blades | flat aura 280 vs per-level armor | CLOSE-cheap | `stat` aura 123 school 1 basePoints 0 `perLevel: [-1.67,-3.34,-5]` (Vanilla exact) | |
| G-39 | Van | rog | 18451 Premeditation | 20 s vs 10 s | CLOSE-cheap | helper 932984 (reserved-free) template 14183 durationIndex 1, mirror TBC 947787, strip arm | |
| G-40 | Van | rog | Mace Spec +skill, Weapon Expertise, Riposte word2 bit 8 | KEEP | | | |
| G-41 | Van | pri | 932970 defenseType + text; 18132 attrMask 1 (I-22) | | CLOSE-cheap | | |
| G-42 | Van | pri | Mental Agility (caveat: PI 920920 inherited Fear Ward identity bit); Lightwell (record arithmetic wrong — 801 HP/6 s vs 800/10 s, near-moot, tooltip edit); Imp Psychic Scream (now SETTLED correct); PI cost component / VE split | KEEP | | | |
| G-43 | Van | sha | Earthbind/Magma drivers (F-4/F-5); Stormstrike charges (I-15); Imp Weapon Totems doc; FT pulseBase registry | | CLOSE-cheap | | |
| G-44 | Van | sha | Rockbiter flat enchant, Imp Reincarnation mask-0 21169, Eye of the Storm encoding, Stormstrike strike count, Convection/Totemic Focus cache, WF Totem 20%, Fire Nova ~4 s | KEEP | | | |
| G-45 | Van | mag | Cold Snap (rationale REPLACED: a frost clone would reset its own cd → infinite Cold Snap; needs module script) | | CLOSE-costly | era_mag_cold_snap | |
| G-46 | Van | mag | Ice Barrier CheckCast (base-only clone makes overwrite deliberate); Ice Block Hypothermia; Combustion proc mask (authored, era-correct); AP masks inherited but era-correct (doc line) | KEEP | | | |
| G-47 | Van | wlk | Imp Firestone `chance: 20` | ~6 PPM | CLOSE-cheap | `proc.ppm` (rate must be sourced) | |
| G-48 | Van | wlk | MD per-rank auraDescriptions (20 ids) | | CLOSE-costly | | |
| G-49 | Van | wlk | stone equip-aura shape, Imp Enslave resist, WotLK omissions (correct), Emberstorm/Shadow Mastery aura 79 (genuinely school-filtered), Conflagrate CU (single application correct) | KEEP | | | |
| G-50 | Van | dru | Natural Shapeshifter Travel/Aquatic bits; NG scope (I-19) | | CLOSE-cheap | | |
| G-51 | Van | dru | Insect Swarm era values | NG clone-chain precedent; no 1.12 per-rank values | CLOSE-costly | | |
| G-52 | Van | dru | Moonkin 24907 hardcast (stated "zero-core-patch" reason stale, still not worth it), Feral Instinct form split, Feline Swiftness outdoors, FF (Feral) 16857 by-id, HotW, Furor icon read, Enrage armor drawback | KEEP | | | |
| G-53 | bots | — | I-25 weapon gate IsBot bail; I-26 down-move stock residue; storedEra label; §9.6 doc; regen-botera 0014 line | | see §3/§4 | | |

Already-CLOSED rows that only need doc text (no decision): TBC druid ×4, TBC shaman ×11, TBC warrior Death Wish, TBC rogue ×3, TBC hunter ×4, Vanilla warrior Defiance, Vanilla paladin Imp LoH + rank chains, Vanilla rogue Preparation/Imp Sap, Vanilla priest Holy Reach/SoR tooltip, Vanilla shaman Windwall/GoA/Mana Tide/Fire Nova delay/Totemic Mastery, Vanilla mage Combustion cd/PoM/Magic Absorption, Vanilla druid Imp Enrage, Vanilla warlock Dark Pact/Demonic Sacrifice/ISB.

---

## 6. Cross-era + consistency (D1) — FINDINGS(24): 2 Critical (= F-1, F-2/F-3), 13 Important, 9 Minor

Thirteen sweeps over all 18 datasets. The two Criticals are the per-class ones already filed (mage Combustion linked-removal; rogue bit-23 leak). New Important findings are I-27…I-33 in §3. Sweep summary:

| Sweep | Result |
|---|---|
| X1 `affects:` shared-bit over-application | 408 specs without `affectsMask` swept; 109 over-bind a player spell; **4 diverge from retail**: rogue bit 23 (F-2/F-3); **mage Blizzard identity 524416 carries bit 19 shared with Frost Armor 168 + Ice Armor 7302** → every Blizzard-naming node (van 18028/18036/18037/18040/18042/18043/18044/18048, tbc 20854/20856) also modifies both armors (retail uses Blizzard's unique bit 128); **Frost Warding** (van 18033 / tbc 20845) boosts Blizzard; **Permafrost** (van 18039 / tbc 20851) binds Mage Armor 6117. Verified NOT findings (stock shares the bit): Feral Aggression→Rip, Sacred Duty, Imp BoM/BoW→Greater, Imp Fortitude→Prayer, warlock demon passives, Totemic Focus, Imp Judgement/Fanaticism |
| X2 `spell_linked_spell` / `spell_group` inheritance | 422 clone templates: 4 template linked rows + 26 group memberships exist; **0 band ids appear in either table**. Combustion (F-1); **TBC Pain Suppression 948040 lost the 33206→44416 threat-drop link**; Winter's Chill / Fire Vulnerability clones lost group 1037; PI + AP clones lost 1107/1122/1123; **TBC Aimed Shot 948370 missing from mortal-wounds group 1061**; BoS clones lost 1007; 12 more clones lost rule-bearing groups (1016 FF 946277-279; 1086 Stoneskin 931030/947010; 1099 UA 948470; 1104 Enrage 932815-819/947556; 1105 Mana Spring 931210/947195; 1095 Inspiration 948057; 1060 Scorpid 932300-303/948281; 1096 Slow 948773; 1094 ToL 946266; 1058 ToW 947351; 1023 LotP 946257; 1030/1031 Mangle). Deterrence clones cleared. Minor: 932513 Enrage lost (-5229,-51185) (WotLK-only); groups 1027/1050/1064/1076/1077 have no stack rule (inert) |
| X3 aura 55 `school:` no-op | 4 hits: van priest 18136 Shadow Focus, van mage 18002 Arcane Focus + 18035 Elemental Precision, van warlock 18201 Suppression (I-18) — `StatSystem.cpp:885` sums aura 55 bare, so each adds hit to ALL schools. Fix: `SPELLMOD_RESIST_MISS_CHANCE` (op 16, one new OP entry) bound by family mask. (18836 / 20189 / TBC Precision are misc 0 = correctly all-school) |
| X4 embedded `_ref` vs standalone | 8 datasets differ (van druid/shaman; tbc druid/shaman/warrior/hunter/paladin) — **downgraded to Minor**: `spells` flagsA/B/C identical everywhere, drift confined to audit/doc keys the generator never reads. `gen_era_talents.py:1758-1765` makes the inline copy win outright. Fix: one canonical copy |
| X5 index-semantic claims | **Vanilla shaman 42 totem summons ship DurationIndex 5 = 300 s while every description says "2 min"** — TBC corrected the same totems to index 4 = 120 s (tbc/shaman.yaml:3299/:3482) and it was never back-ported → **I-31 resolves M-47 in favour of `durationIndex: 4`**. Five Vanilla shaman comments annotate index 10 as "20 yd"/"10yd" (it is 30 yd; 20 yd = index 9); tbc/shaman.yaml:1863 Totemic Range comment contradicts :3303/:5496 |
| X6 generator defaults / orphans | **van hunter 18329 Ranged Weapon Specialization 922632-636: `school: 127` with NO equip gate (EquippedItemClass −1) → +1-5% to ALL damage of ALL schools** (stock 19507 and TBC 941080-084 carry eqCls 2 / eqSub 262156) → I-32. Other misc-127 rows verified inert or retail-faithful. Orphan `spell_proc` = exactly 923552-554 (rogue Imp Sap, M-44); orphan `spell_dbc` = 924288-292 (M-54), 928131-132 Fel Intellect pet r4/r5, 932414 Elemental Mastery — dev-box residue, a re-mint would collide |
| X7 script headers | bot-bail wording CLEAN (all 13 headers state never-bail); `EraTalentTotemScripts.cpp:1` says "Vanilla totems" but `spell_era_windfury_totem` also binds TBC 947335-339 (Minor) |
| X8 registry / doc drift | **framework shaman row "947442-947511 free" is FALSE — 947442-947445 are live** (FT Attack, Nature's Guardian, Elemental Focus, Clearcasting); totem count 157 not 146 → I-33. Other seven "free" ranges confirmed 0 live ids. NG proc flags 327680→81920 (M-66); kPalTrainedChains ×3 (M-55); FT pulseBase 931150→931144 (M-51); CLAUDE.md 0020 row 0014 (M-75); 0023 row circular (M-68); "unmerged" in phase10-shaman:389, phase8-paladin-seals:4, vanilla-mage:78 |
| X9 number-of-record provenance | **all nine `era-data/_ref/<class>-spells.yaml` carry only `{id, flagsA, flagsB, flagsC}` — zero magnitudes/cooldowns/durations.** Vanilla has no machine-checkable 1.12 ground truth (TBC has the wago CSVs + tooltip cache); Vanilla magnitudes live only in per-node authoring comments. This is the soil the 5-min-totem and Shield Slam defects grew in |
| X10 cross-era pairs | clean: Phase A reuse ids resolve to live Vanilla owners; reconcile arms era-paired (Deterrence 19263 Vanilla arm EraTalents.cpp:714 / TBC arm :758); allowlist per-class era census symmetric; the one functional Vanilla→TBC coupling (BoS r5 row) is documented and handled |
| X11 conventions | clean: 18 datasets wired everywhere; 28 + 28 `kAuthoredOrders` keys; the 11 passive-flagged band rows with non-zero DurationIndex are all index 21 (permanent totem passives, intended); `attributesMask` never on a node |
| X12 regen | `tools/era-regen.sh` once → `git diff --stat` empty, stamp `1de3388d` |
| X13 stock-grant strip on era exit | **0 unstripped** — premise corrected: `EraTalents::Reset` (EraTalents.cpp:311-320) removes every spent node's `rankSpell` with NO band filter, on every player crossing (EraTransition.cpp:112-137) and every bot bracket move (EraTalentBots.cpp:63-78, :292); 203 node→stock grants verified. Residual gaps named: (a) `TeardownStale` skips an era whose `SpentPoints == 0` (lost rows → spells linger); (b) `EraTransition::Run` defers while `IsInCombat`. **I-26's 42985 is TRAINER-taught (TrainerId 16, ReqLevel 77), not a node grant, so Reset cannot reach it — I-26 stands for trainer-taught residue on bracket down-moves** |

## 7. Vanilla regression (D2) — GREEN (2 Minor)

| Check | Result |
|---|---|
| V1 per-class doctors (33 captures, 9 classes) | every header `era=Vanilla … generation=1de3388d`; 0 ORPHAN / 0 PROBLEM / 0 UNCLASSIFIED; `bandsweep 0 stripped`; max granted band id **932994** — no id ≥ 933000 on any Vanilla character |
| V2 scripts | 0 `script … not found`; all 48 `era_%` ScriptNames defined + `RegisterSpellScript`ed; `spell_era_windfury_totem` bound outside the LIKE pattern; 0 band bindings without a `spell_dbc` row |
| V3 trainer gating | the only `ReqAbility1=0` rows in the whole set are shaman 2H Axes 197 / 2H Maces 199 — a deliberate EQUIP-layer gate (`era_talent_weapon_gate`, 932406; a WotLK shaman has the skills baseline, so the table cannot gate them) → record in the framework so a future sweep does not read them as a hole. Every other item: r1 absent from `trainer_spell` (pal Consecration/Holy Shield/Holy Shock/BoK/GBoK/BoS; pri Divine Spirit/Holy Nova/Inner Focus; mage Ice Block/Cold Snap/AP/PoM; rog Hemorrhage/Prep/Premed; war Shield Slam/MS/BT/Last Stand; dru Insect Swarm/NG/Omen/FF-Feral) with higher ranks chained on the previous rank; warlock stones marker-gated (932939 stock / 932994 era); hunter Deterrence/Scorpid on 932305 |
| V4 totems | 69 creatures byte-identical to module SQL; Index-0 classification: 12 DRIVER (Stoneclaw, Tremor, Flametongue, Poison/Disease Cleansing), 47 DIRECT-correct (persistent area auras), 10 AI-driven (Searing, Fire Nova), **5 DIRECT-defective = Earthbind 920100 + Magma 920160-163 (confirms F-4/F-5; no new ones)** |
| V5 three-way logic | 9 L80 bots (one per class) `era=WotLK`, 0/0/0; the only band ids held are the allowlisted WotLK markers 932305 (hunter) and 932939 (warlock), matching `band-allowlist.yaml` + `EraBandAllowlist.gen.h` era mask 0x4. Uptime tally: 63 `band sweep stripped`, **all `ORPHAN_ALLOWLIST`** (rule-3 gate at rank 0 after randomize/respec churn — 7 Vanilla 932950 VE, 56 TBC 946261/946265/946280/947539/948029/948032; timestamps minutes apart, no loop), **0 `UNCLASSIFIED`** |
| Minor findings | (1) 197/199 ungated-by-design needs a framework note; (2) the same gate bails `IsBot` = I-25 |

Phase G residue from the 2026-08-24 §5 list is folded into §8b. Its header names gen `71272694`; the MPQ must be re-staged at the post-fix generation.

---

## 8. Real-client checklists (Phase G — filtered to classes Phase F touches)

### 8a Player path (Phase 9.5 §6) — one human character
`.ip set <char> 1` → `.eratalents doctor` → `.ip set <char> 8` → doctor → `.ip set <char> 13` → doctor → `.ip set <char> 1` → doctor: zero `ORPHAN ERA SPELL` at every hop; panel + spellbook show only the current era. `.eratalents doctor` on every existing human character: zero orphans. Addon canary green (no red stale-generation warning) after installing the post-fix `patch-V.mpq`.

### 8b Vanilla (one block per class; run only the classes Phase F touched)
- **Mage:** Combustion — three Fire crits, watch 28682 stacks fall off when Combustion ends (F-1); Blizzard crit refunds mana with Master of Elements (I-16); Arcane Power does NOT stack with Power Infusion (I-17); PoM 3-min cd; ward reflects; Ice Block untrainable.
- **Rogue:** Improved Ambush/Backstab/Eviscerate crit and damage land ONLY on their named strike (F-2); Initiative combo point only on Ambush/Garrote/Cheap Shot (F-3); Sword Spec extra attack rate-limited (I-13); Riposte disarms; Preparation 10-min incl. Blind.
- **Shaman:** Earthbind snare repeats for the totem's whole life; Magma ticks every 2 s (F-4/F-5); party buff totems reach 20 yd, not 30 (I-14); Stormstrike consumes 2 charges (I-15); every totem summons; Windfury Totem does not stack with WF Weapon; 2H axe/mace gated.
- **Hunter:** Entrapment / Improved Wing Clip root the kited target at any range and nothing else (F-6); Aspect of the Hawk value; trap procs; pet talents; Deterrence not dual-held.
- **Priest:** Inspiration procs on heal crits and shows +armor (F-7, I-23); Power Infusion costs ~182 mana (I-20); Martyrdom procs on boss ability crits (I-21); Spirit Tap only on XP kills (I-22); Blackout stun resistible + tooltip; VE party heal; Divine Spirit/Holy Nova untrainable.
- **Paladin:** Judgement of Command is a flat Holy hit (I-9); Illumination refunds 100% (I-10); Seal of Justice costs mana (I-11); SoL/SoW proc at the chosen PPM (I-12); Improved Devotion Aura no longer scales Sanctity (I-8); Repentance 6 s Humanoid (G-33 if closed); each seal + Judgement; Consecration/BoK/GBoK untrainable.
- **Warrior:** Shield Slam hits 225-235 (I-7); Deep Wounds bleed; Imp Berserker Rage energize; Shield Spec rage on block; Defiance only in Defensive Stance.
- **Warlock:** Suppression adds hit ONLY to Affliction spells (I-18); Dark Pact 150; Conflagrate instant; pet passives; Firestone/Spellstone trainer gating; Corruption cast time.
- **Druid:** Nature's Grace haste applies to Gift of the Wild / Rebirth too (I-19); Omen procs; LotP/Moonkin party auras; Enrage 25/30; form gating.

### 8c TBC (one block per class; run only the classes Phase F touched)
- **Paladin:** SoJ/SoB/SoV cost mana (I-1); Crusader Strike cooldown = the wago value (I-2); SoL/SoW proc at PPM (I-4); an Alliance paladin bot/char holds SoV only (I-3); Conviction tooltips clean.
- **Shaman:** Totemic Call trainable at the recorded level (I-5); Totemic Mastery tooltip matches radius; totems summon + pulse.
- **Hunter:** Entrapment root shape per the design call (I-6); Scatter Shot mana if G-20 closed.
- **Warlock:** Nether Protection arms off a non-damaging Fire/Shadow hit (M-31).
- **Mage:** Combustion stacks fall off (F-1); AP/Combustion masks if G-27 closed; Shatter/Ice Barrier as per Phase 9 record §15.
- **Druid / Warrior / Rogue / Priest:** untouched unless a Minor is folded in — no re-test required beyond the doc fixes.

---

## 9. What was NOT run
- Any real-client check (tooltips, icons, panel, proc firing in the combat log, MPQ rows) — Phase G.
- 1.12.1 MAGNITUDE re-verification for Vanilla classes whose `_ref` is a 3.3.5a id/mask reference (warrior, mage, druid, priest, hunter report no 1.12 magnitude arbiter in-repo; paladin/rogue/shaman/warlock partially) — see D1 X9 for the per-file provenance.
- Live probes of the nodes no authored bot order reaches (listed per class under DEVIATIONS in the agent reports; all static-verified).
- wago 2.5.4 SpellPower/SpellCooldowns fetches for I-1/I-2/I-4 magnitudes (not committed).

## 10. Owner decision (2026-09-06) and fix rounds (Phase F)

**Owner decision ("go with your recommendations"):**
- FIX every Critical (F-1…F-7) and every Important (I-1…I-34).
- CLOSE every CLOSE-cheap ledger row; KEEP every KEEP row.
- CLOSE-costly: close **G-11/G-30** (stance passives, both eras, one band-gated shapeshift hook) and **G-45** (era Cold Snap script) this pass; **defer** G-3, G-5, G-6, G-8, G-12, G-15, G-18, G-22, G-25, G-28, G-31, G-48, G-51 (the final "still pending" list).
- I-6 TBC Entrapment: ACCEPT the core-fixed AoE shape (strictly better than era-literal; consistent with stock) and record it as TBC hunter accepted gap 13. Vanilla F-6 mirrors the same core-fixed shape (root centred on the VICTIM, not the hunter) for Entrapment and single-target for Improved Wing Clip.
- PPM anchors where no 1.12 source exists: SoL 10, SoW 12, SoJ 5 (stock WotLK), SoC 7 (the known Vanilla rate), Firestone 6.

**Round order (serial — every data round regenerates the shared artifacts):** R0 generator prerequisites (`resistMiss` OP, `targetCreatureType` scalar) → R1 mage → R2 rogue → R3 shaman → R4 hunter → R5 priest → R6 paladin → R7 warrior → R8 warlock → R9 druid → R10 bots/C++ → R11 docs/registry → R12 closing gate. Each round: implementer → one review → one fix pass → record row below.

| Round | Status | Generation | Notes |
|---|---|---|---|
| R0 generator | DONE b309dd6 | 1de3388d → ed6c167d | `resistMiss` already existed (gen:109); `targetCreatureType` scalar + test added; framework rows; stamp bumps on any generator edit by design → clients need the new MPQ |
| R1 mage (both eras) | **DONE** 50f7c82 + review + fix pass cfd9fa7 | ed6c167d → da77a9b8 | Fix pass: Blizzard-Chill pseudo-identity moved from word-0 bit 28 (= Mage Armor 6117, never recycle) to word-2 bit 6 (64) on 932980-982/948961-963; Permafrost {a:544/1049120, c:64} both eras; AP group rows 1107+1123 only (1122 is PI's). F-1 (2 linked rows), I-16, I-17 (10 group rows — 1122 questioned: stock AP is not in 1122), I-27/28/29 (Permafrost masks dropped the Chill pseudo-bit 28 which collides with Mage Armor's identity — resolution pending), G-27 (word-1 bit 6 removed per effect), M-36/37/63/64, G-45 (932335 + era_mag_cold_snap; `AI resolve 11958 -> 932335` live). Verify: audit 0, 246 pass, db-import applied, doctors Lerie/Andarion/Putsur 0/0/0. Nit for R10: doctor label "Cold Snap stock GRANT in BOTH eras" now stale for Vanilla. Lerie left at spent=11 |
| R2 rogue | **DONE** 40329c9 + review + fix pass 4d9454c (Premeditation cooldown 120000; Opportunity comment) | da77a9b8 → db12c9e6 | F-2 six unique-bit masks (18436 e1 only), F-3 Initiative 1792, I-13 ICD 6000, M-43 ICD 1000, M-44 DELETE 923552-554, G-38 Serrated Blades aura 123 perLevel −1.67/−3.34/−5, G-39 Premeditation 932984 (DurationIndex 1; cooldown 120000 pending fix pass — inherited stock 20 s), docs M-19/20/21/45 + gaps #4/#6 CLOSED. Verify: audit 0, 246 pass, db-import applied, Syllastor/Klirlic 0/0/0, `13964 -> 923444` |
| R3 shaman (both eras) | **DONE** 42c06f8 + review + fix pass da3c547 (Sentry 931390 stays 5 min; TBC _ref oq 6 + B.8 rationale) | db12c9e6 → bae41229 | F-4 driver 931011 (clone 6474 @3000 → 931010), F-5 drivers 931134-137 (@2000 → 931130-133), I-14 radius 9 on 39 pulses + FT effect clones 931148-151 (accepted extension) + 53 strings, I-15 procCharges 2, I-31 41 summons durationIndex 4, M-48 Mana Tide idx 29, M-46/M-10 per-rank levels, M-11 "40 yards", I-5/I-33/M-12/13/14/49/50/51/X7 docs. Verify: audit 0, 246 pass, db-import applied, Kespo/Rukk 0/0/0, all totem resolves on clones. I-31 final set = 40 summons |
| R4 hunter (both eras) | **DONE** d47e23c, review APPROVE (2 doc nits → R11: framework hunter row 18/35/11; tbc/hunter.yaml:~634 "receives the WHOLE fix" wording) | bae41229 → f0fc70d0 | F-6: 932972 AND TBC 948343 → core-fixed 53/16/r13/attrEx5 (948343 had shipped caster-centred 22/15 too — the TBC audit misread it; accepted), 932978/948344 single-target; I-32 equip gate 2/262156; G-36/G-21 attribute bits; I-6 gap 13 (describes the shipped 53/16 shape); G-20 948382 + new generator key `auraInterruptFlags` + swap; I-30 groups 1061/1060; M-27/28/38/39/41/42. Verify: audit 0, 247 pass, db-import, Lorneas/Kavat 0/0/0, `20632 → 948382`. Leftovers → R11: TBC hunter `_ref` gap id 12 missing though the record cites it; tbc-workflow:200/411 still call 948382 a hole |
| R5 priest (both eras) | **DONE** f332adc, review APPROVE (1 doc nit → R11: TBC priest record §9 row 12 description should pair Blackout with Spirit Tap, not Wand Spec) | f0fc70d0 → 98c056de | F-7 flags 16384 + typeMask 2 (heal); I-23 aura 101 misc 1 +8/16/25 + client (M-70); I-20/21/22; I-34 Shadow Focus resistMiss 2-10 masks (33792004,0,64) — DP/Mind Control via affectsMask (not in Vanilla _ref); M-69 Blackout defenseType 1 + text; I-30 PI 920920/948039 → 1122+1123, Inspiration r1 920987/948057 → 1095 (only 14893 is grouped in stock), PS 948040 linked 44416; M-22 (banner at :743) / M-23 / M-24 (15 clones, not 16 — 948057-059 excluded) / M-25 gap 13 + gap 12 CLOSED; M-71. Verify: audit 0, 247 pass, db-import, Karanie/Nurgrah/Donham 0/0/0 |
| R6 paladin (both eras) | **DONE** 35f7a16 + review + fix pass 996b99a (5 stale comments; 3 seal SQL headers name the band sweep) | 98c056de → 078fc482 | TBC I-1 (SoB 210 / SoV 250 / SoJ 10%), I-2 CS 6 s, I-4 PPM 10/12, G-1 Illumination 60% 946141-145, G-2 Repentance 946146, M-1..5; Vanilla I-8 mask 0, I-9 JoC 932673-677, I-10 Illumination 100% 932678-682, I-11 SoJ 13% (Classic anchor), I-12/G-34 PPM, G-33 Repentance 932683, M-55/56/57, I-30 BoS → 1007. Judgment calls accepted: SoC ppm 7 on all 5 ranks; seal→judgement pointer is EffectBasePoints_3 (not TriggerSpell). Verify: audit 0, 247 pass, db-import, Airaani/Jiora/Sathelon 0/0/0, no 20210/20066 residue; TBC clones 946141-146 DB-verified only (no probe bot spends 20008/20060). Fix pass done. Leftover → R11: tbc/paladin.yaml:3376 historical kPalTrainedChains mention |
| R7 warrior (both eras) | **DONE** b318dec + review + fix pass 06f7fa2 (TBC _ref gap 6 CLOSED + ledger; PASSIVE in the attribute breakdown) | 078fc482 → c8a840c1 | I-7 Shield Slam clones 932841-844, G-11/G-30 stance-correction passives 932845-847 (data-only, stances: gate, both eras), G-13 Blood Frenzy 947568/569, I-30 Enrage → 1104 (TBC 947556-560 all listed — each its own chain root), M-15/16/17/18/52/53. Stance corrections authored from the dump: 932845 Def = aura 79 −5 + aura 10 −10 (→ −9.75% done, +30.5% threat); 932846 Ber = aura 87 +5 + aura 10 +25 (→ +10.25% taken, threat ×1.00); 932847 Bat = aura 10 +25 + aura 280 −10 (cancels 21156's −20% threat and 2457's +10% arpen exactly); allowlist 83→86 (sha c388d2c0bad6). Additions: Blood Frenzy clones in group 1108 + client rows. Verify: audit 0, 247 pass, db-import, Dilkiz/Mehotryt 0/0/0, stance passives applied per stance on both bots, no 23922 residue. Fix pass done |
| R8 warlock (both eras) | **DONE** e4e39a1, review APPROVE (nit → R11: "Atrocity" wording) | c8a840c1 → 2d5782d0 | I-18 Suppression resistMiss, M-31 Nether Protection typeMask 0, G-47 Firestone ppm 6, I-30 UA 948470 → 1099, M-32/33/34/35/58/59/60. Suppression masks (4768794, 1547, 0) = 10 Affliction spells + Siphon Life/CoDoom/HoT bits; Curse of Tongues (bit 31, shared with Dark Pact) deliberately EXCLUDED — residual Minor. UA group 1099 lists 948470 only (948471/472 are chain ranks). Verify: audit 0, 247 pass, db-import, Remgich/Thenata 0/0/0, 932930 marker on both; Vanilla 18201 line not captured (Remgich rolled Demonology) — TBC twin 941603 applied=yes as the same code path |
| R9 druid (both eras) | **DONE** 60d6a5f + review + fix pass 0d32713 (framework druid rows; NG comment — instants never consume the charge on this core) | 2d5782d0 → 152e6991 | I-19 NG masks, I-30 groups 1016/1023/1030/1031/1094, G-50 Natural Shapeshifter, M-6/7/8/9 TBC docs, M-65/66/67/68 Vanilla docs + embedded _ref resync. Groups: 1016←946277, 1030←946258, 1031←946261 (chain r1-only); 1023←946257, 1094←946266 (singles). NG 932504 all-ones masks. Travel 783 (B bit14) / Aquatic 1066 (A bit29) added to both _ref copies; only other holder = NPC 27546. Verify: audit 0, 247 pass, db-import, Araelar/Kichme 0/0/0 |
| R10 bots/C++ | **DONE** bcc5fa3 + review + fix pass 822a2c9 (trainer strip skips the current era's node-granted stock ids by construction — 504 (era,class,id) tuples; resolver comment) | (no regen; gen 152e6991) | I-3/I-24 faction seal strip (OnBotLevelChanged + patch 0020 post-walk hook), I-25 weapon gate IsBot bail, I-26 down-move trainer-taught strip, M-61/62/72/73/75 + R1 Cold Snap label. Findings while fixing: a THIRD trainer walk (AutoMaintenanceOnLevelupAction::LearnTrainerSpells, AI tick after the factory pass) re-taught both seals and the above-level spells → patch 0020 now 4 files (26→29 hunks) and the post-walk bridge runs BOTH the faction-seal strip and the trainer-level strip. New startup index: `trainer level map: 2138 stock spells`. Verify: build OK, 0020 reverse-check OK, Drimdinn holds 946084 only / Sathelon 946080 only (DB), Eragon 80→70 lost 42985 (+57 trainer-taught), VE resolves to castable 921152, storedEra labelled, 0 orphans. Weapon-gate baseline: 5 Vanilla shaman bots still wield 2H (until their next gear pass) |
| R11 docs/registry/cleanup | **DONE** bbf0a6b (16 records stamped; CLAUDE.md rows; tbc-workflow final-review row; framework notes + generator traps; hunter gap 12; orphans 924288-292/928131-132/932414 deleted, 0 holders) | 152e6991 → 70418133 | record stamp headers, CLAUDE.md rows (0020 4 files, 0023 rationale, Phase-9.5 wording, generator traps, db-import recipe), tbc-workflow 948382 + final-review row, framework notes, hunter/priest/paladin/warlock leftovers, orphan spell_dbc DELETE, phase9-5 §9.6 closed |
| **R12 closing gate** | **CLEAN 2026-09-07** | **CLOSING GENERATION `70418133`** | regen idempotent (tree clean); audit 0; 247 pass; gen_bot_builds exit 0; canary `band classifier: 2578 / 86 / c388d2c0bad6` + `trainer level map: 2138 stock, 504 node-granted excluded`; 0 drift; 1011 nodes (432 + 579, per-class counts unchanged); band rows: spell_dbc 3582, spell_ranks 449, spell_group 43, spell_linked_spell 3; 500-bot doctor sweep 0 ORPHAN / 0 PROBLEM / 0 UNCLASSIFIED (1 log strip = ORPHAN_ALLOWLIST rule-3, correct); patch stack all OK except the documented 0003/0014 context fails; binary canary present (`grep -ac` — `strings` is not in the image) |

## 11. Still pending after Phase F (the owner's "final close-costly list")

**Deferred CLOSE-costly ledger rows (owner decision 2026-09-06 — not fixed, still accepted gaps):**
| Row | Era/class | Item | Why costly |
|---|---|---|---|
| G-3 | TBC pal | **CLOSED 2026-09-07** (deferred-gaps round, gen b35a304d) — `era_pal_judgement_of_command` SpellScript bound to TBC 946103-946108 and Vanilla 932673-932677. Was: 20049 JoC ×2-if-stunned (also the Vanilla JoC clones 932673-677 inherit the same half) | new SpellScript on the JoC clones |
| G-5 | TBC dru | 20126 Feral Swiftness dodge scope | 2 helper ids + `kEraDruidTbcPaired` C++ arm |
| G-6 | TBC dru | **CLOSED 2026-09-07** (deferred-gaps round, gen b35a304d) — `era_dru_leader_of_the_pack` AuraScript (the core script minus the 68285 mana cast) bound to 946257. Was: 20139 Improved LotP mana half | era copy of `spell_dru_leader_of_the_pack` |
| G-8 | TBC sha | Flametongue/Frostbrand/Rockbiter Weapon imbues at TBC values | ~20 clones + `spellitemenchantment_dbc` re-points + recreated weapon-script binds (user call) |
| G-9 (re-gate half) | TBC sha | Totemic Call 36936 L30 re-gate | module UPDATE must be ordered after IP's `zz_optional_limit_spells_to_expansion.sql` |
| G-12 | TBC war | **CLOSED 2026-09-07** (deferred-gaps round, gen b35a304d) — `era_rampage_consume_ready` SpellScript (AfterCast removes 947540) bound to 947533/947534/947535. Was: 20343 Rampage ready-aura 947540 not cleared after cast | ~15-line SpellScript AfterCast |
| G-15 | TBC rog | 20433 Mace Spec aura-163 crit-damage half not weapon-gated | band-gated core guard modelled on patch 0025 (identical to stock 13709) |
| G-18 | TBC pri | Holy Nova r7 25331 flat 875; Lightwell 10 charges / 30% cancel; `attributesEx4/6` generator key; Inner Focus WotLK attribute bits | clones + C++ swap row; GO rebuild or AuraScript; generator feature |
| G-22 | TBC hun | Frost Trap Aura 13810 −60%; Focused Fire Kill Command crit half | GO indirection + baseline-clone exception; paired arm + damaging Kill Command |
| G-25 | TBC wlk | Conflagrate `SPELL_ATTR0_CU_NO_POSITIVE_TAKEN_BONUS` (SpellMgr.cpp by-id); `auraInterruptFlags 0x80000` on 948460-464; UA `AttributesEx6` TAPS_IMMEDIATELY | core patch or `AttributesCu` key; generator keys |
| G-28 | TBC mag | **CLOSED 2026-09-07** (deferred-gaps round, gen b35a304d) — **Molten Fury only**, via core patch **0026** (`Unit::SpellPctDamageModsDone` selects `AURA_STATE_HEALTHLESS_20_PERCENT` for a band aura) — this reverses the 2026-09-05 "no core patch" call (owner, 2026-09-07). The other three items in this row REMAIN OPEN: Ice Block Hypothermia (baseline clone); Combustion stack-cancel script on 28682; Ice Barrier CheckCast companion | core patch / baseline fork / two small scripts |
| G-31 | Van war | rage-cost tooltip rewrite (addon `RewriteCost` handles Mana only) | addon feature, class-wide |
| G-48 | Van wlk | Master Demonologist per-rank auraDescriptions (20 helper ids) | id + MPQ churn for tooltip precision |
| G-51 | Van dru | **CLOSED 2026-09-07** (deferred-gaps round, gen b35a304d) — Vanilla clone chain 932514-932518 (node 18738) at the wowhead-Classic 1.12 tooltip values, plus trainer SQL + `spell_ranks` and `ReconcileBaselineSpells` CLASS_DRUID arm (9) stripping stock 5570/24974-24977/27013/48468; **extended to TBC** (clones 946281-946286, node 20107, wago 2.5.4 values). Was: Insect Swarm era values (66 dmg / −2% hit) | no 1.12 per-rank values in `_ref`; NG clone-chain precedent exists |
| G-24 | TBC wlk | Dark Pact range 30 vs 100 yd | CLOSE-cheap but recommended KEEP (target is the caster's own demon — unobservable) |

**Residuals recorded during the fix rounds (Minor, accepted):**
- Vanilla warlock Suppression excludes Curse of Tongues (its only family bit is shared with Dark Pact / Curse of Idiocy / Shadow Embrace).
- Stance-correction rounding: Defensive −9.75% done (target −10), +30.5% threat (target +30); Berserker +10.25% taken (target +10). Berserker/Battle threat and Battle armor-pen are exact.
- Nature's Grace all-ones mask: instants never consume the charge on this core (`Player::ApplySpellMod` skips a CASTING_TIME mod at base 0 before the charge is registered) — 1.12 parity unreachable here.
- Blood Frenzy TBC clones 947568/569 keep the inherited `AttributesEx2 0x4` (inert on an aura-109 payload).
- 5 Vanilla-band shaman bots still wield 2H weapons until their next gear pass (the gate now applies; nothing re-equips them retroactively).
- Bracket down-move GEAR residue on bots is accepted (only trainer-taught SPELLS are stripped).

**Recommended follow-ups (not scheduled):**
- `tools/era_audit.py` guards for the two generator traps: (X1) an `affects:`-derived mask whose intersecting player-spell set is a superset of the node's `affects:` list; (X2) a clone whose template appears in `spell_linked_spell` / `spell_group` without mirrored band rows.
- A machine-checkable 1.12 number-of-record for Vanilla (every `era-data/_ref/<class>-spells.yaml` is a 3.3.5a id/mask reference).
- **1.12.1 extract coverage (found 2026-09-07):** the era-wow `tools/vanilla-extract/Spell.dbc` the framework calls the canonical 1.12.1 source is **truncated at id 21184** (16 332 records) — Insect Swarm is absent entirely from it (5570 reads `zzOLDSpell - Reuse`). It is canonical only for the ids it contains; above 21184 an authored Vanilla value has to come from wowhead Classic tooltips. A complete 1.12.1 `Spell.dbc` (or a second extract source) would remove that hole and is a prerequisite for the machine-checkable number-of-record above.
- Phase G real-client pass (§8, filtered) after installing the closing generation's `patch-V.mpq`, then the owner's merge decision.

## 12. Deferred-gaps round (2026-09-07) — CLOSED, closing generation `b35a304d`

Round: **final review deferred-gaps round (2026-09-07)**. Spec
`docs/superpowers/specs/2026-09-07-era-talents-deferred-gaps-round-design.md`, plan
`docs/superpowers/plans/2026-09-07-era-talents-deferred-gaps-round.md`.
Generation `70418133` → **`b35a304d`** (the client `patch-V.mpq` must be reinstalled).
**Stamp note.** Tasks 1-7 shipped and were verified at `149ea262`; the docs/records task (Task 8)
then edited YAML **comments only** (mage node 20842, TBC druid LotP/Insect-Swarm prose, one
duplicate `description:` key in TBC paladin whose two strings were byte-identical). Because
`generation_stamp` hashes the raw dataset bytes, that bumped the stamp to **`b35a304d`** while
**every generated artifact except the `era_talent_meta` row is byte-identical** (`tools/era-regen.sh
--sync-fork` re-emitted only `2026_08_14_11_era_talent_meta.sql`). So the Task-7 evidence below was
captured at `149ea262` and applies unchanged; the stamp that must be in the DB and in the installed
`patch-V.mpq` is **`b35a304d`**.

| Row | Mechanism | Commit |
|---|---|---|
| G-3 (TBC + Van pal) | `era_pal_judgement_of_command` SpellScript — `OnEffectHitTarget` EFFECT_0 doubles the damage if the hit unit carries a stun-mechanic aura; bound to TBC JoC 946103-946108 and Vanilla JoC 932673-932677. Seal descriptions gained "or double that against a stunned target" | 2cfd61c (bindings), 4e2fa2b (scripts) |
| G-6 (TBC dru) | `era_dru_leader_of_the_pack` AuraScript — the core `spell_dru_leader_of_the_pack` `HandleProc` **minus** the WotLK 68285 mana cast; bound to 946257 in place of the core script | 2cfd61c, 4e2fa2b |
| G-12 (TBC war) | `era_rampage_consume_ready` SpellScript — `AfterCast` removes the ready-aura 947540; bound to 947533/947534/947535 | 2cfd61c, 4e2fa2b |
| G-28 (TBC mag, Molten Fury only) | core patch **0026 `core-molten-fury-era-window`** — in `Unit::SpellPctDamageModsDone`, the `case 4920: case 4919:` arm selects `AURA_STATE_HEALTHLESS_20_PERCENT` when the owning aura's id is in `[920000, 950000)`, else the stock 35%. Regen `tools/regen-moltenfury-patch.sh` (baseline-aware: reverses 0025→0018→0012, diffs, re-applies 0012→0018→0025); `regen-shatter-patch.sh` and `regen-wandspec-patch.sh` gained 0026 as a contaminator. **Reverses the 2026-09-05 "no core patch" decision (owner, 2026-09-07).** The row's three other items stay open | 0528c73 |
| G-51 (Van **and** TBC dru) | Vanilla clones **932514-932518** (node 18738; 11/23/29/44/54 per tick ×6 every 2 s, −2% hit, mana 45/85/100/140/160; hand SQL `2026_09_07_01_era_talent_druid_insect_swarm_trainer.sql`, TrainerId 33 chain + `spell_ranks`) and TBC clones **946281-946286** (node 20107; 18/32/50/72/99/132, −2%, mana 50/85/110/135/155/175; `2026_09_07_02_era_talent_tbc_druid_insect_swarm_trainer.sql`). **No stock trainer deletion** — the stock chain self-gates on 5570. `ReconcileBaselineSpells` CLASS_DRUID **arm (9)** strips stock 5570/24974-24977/27013/48468, readiness-gated by `EraNodeReplacesStock` | 5ee7709 (Van), 7337850 (TBC), c25453d (arm 9) |

**Value-source finding (G-51 Vanilla).** The era-wow `tools/vanilla-extract/Spell.dbc` — the
framework's "canonical 1.12.1" source — is **truncated at id 21184** (16 332 records) and contains
no Insect Swarm at all (5570 reads `zzOLDSpell - Reuse`). The Vanilla values were therefore taken
from **wowhead Classic** tooltips (66/138/174/264/324 total, −2% hit) with a YAML comment saying
so. TBC values are the wago 2.5.4 survival lines already recorded in the TBC `_ref`.

**Headless verification (Task 7, 2026-09-07).**
- `db-import` exit 0, then `--force-recreate`; live `era_talent_meta` generation = **`149ea262`** (the round's code/data stamp; re-imported as `b35a304d` after the Task-8 comment edits).
- `spell_script_names`: **15** rows (6 TBC JoC + 5 Vanilla JoC + 3 Rampage + the 946257 core→era swap).
- `spell_ranks`: 5 rows (Vanilla chain) + 6 rows (TBC chain); **9** trainer rows across the two hand SQL files.
- `spell_dbc` spot checks: 932514 = `11 / 0 / 2000 / -2 / 45 / 0 / 1771`, 932518 = `54 / … / 160`,
  946281 = `18 / … / 50`, 946286 = `132 / … / 175`.
- Stock trainer rows for 24974-48468 **intact** (6) — nothing global was deleted.
- Boot canaries: `band classifier: 2580 node-grant ids indexed, 86 allowlist entries (allowlist sha c388d2c0bad6)`
  (+2 vs R12 = 932514 / 946281), and
  `trainer level map: 2138 stock spell(s) … 502 node-granted stock id(s) excluded across 18 (era,class) set(s)`.
- `.eratalents doctor` on Airaani 60 (Van pal), Drimdinn 70 (TBC pal), Flapionua 60 (Van dru),
  Feonuke 65 (TBC dru), Eragon 70 (TBC mage): **0 ORPHAN / 0 PROBLEM / 0 UNCLASSIFIED**, with
  `node 18738 rank 1: grant 932514 known=yes` and `node 20107 rank 1: grant 946281 known=yes`.
- **Not doctor-sampled: TBC-band warrior** — no warrior was online in the 61-70 band (the bot
  brackets jump 60→71), so Rampage (G-12) has no headless doctor evidence; it rests on the
  binding row count plus the real-client check below.
- Stock-strip check: **0** live stock Insect Swarm rows on online managed-era druids. 193 residual
  `character_spell` rows carry `specMask 0` — unknown to the character (`HasSpell` is false via
  `IsInSpec`); this is pre-existing dual-spec residue, **not** a defect of this round.
- Pre-existing upstream boot warning, untouched by this round:
  `SpellId 5570 listed in 'spell_group' with stack rule 3 does not share aura assigned for group 1060`
  (stock `spell_group.sql:408`).
- Patch 0026 build canary: worldserver binary 2026-09-07 10:04, `0026 in tree`.

**Real-client checklist (owner, after installing the `b35a304d` `patch-V.mpq`):**
- **Paladin, both eras** — Hammer of Justice → Judgement of Command lands ≈ 2× the unstunned hit.
- **TBC warrior** — Rampage cannot be recast until the next crit re-arms the ready aura.
- **TBC druid** — Leader of the Pack heal shows **no mana line**; Insect Swarm reads 108 over 12 s
  and −2% hit; rank 2 is buyable at the trainer at level 30; **no stock 5570** in the spellbook.
- **TBC mage** — Molten Fury's bonus applies only below **20%** target health.
- **Vanilla druid** — Insect Swarm 11 per tick; ranks 2-5 at the trainer at 30/40/50/60.
