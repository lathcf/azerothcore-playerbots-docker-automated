# Era Talents — Spell Authoring Framework

The canonical reference for authoring, shipping, and verifying era-talent spells.
Read this BEFORE adding or changing any spell in `era-data/`. Companion:
`docs/era-talents-cross-class-gotchas.md` (real-client checklist, still authoritative for
its items). Spec behind this doc:
`docs/superpowers/specs/2026-08-14-era-talents-spell-pipeline-framework-design.md`.

## Decision tree for a diverged spell

Walk top-down; use the FIRST mechanism that fits. Era safety rule: NEVER modify a stock
spell id (spell_dbc override or runtime SpellInfo mutation) — IP eras are per-character,
spells are global; a stock edit changes the spell for WotLK-stage characters too.

1. **SPELLMOD passive** (`mechanic: spellmod` / `multi`): the talent tweaks a stock spell's
   numbers (cast time, cost, damage%, radius...). Binds by family mask; the stock spell
   itself is untouched.
2. **Stat aura passive** (`mechanic: stat` / `multi`): flat/percent stat effects.
3. **Grant a stock id** (`grants:`): ONLY when the WotLK spell's behavior AND magnitude
   equal the era version (verify against Spell.dbc + 1.12 reference, not the tooltip).
4. **Custom castable/helper clone** (`helpers:` with `template:`): the era version differs
   mechanically (vanilla PI, VE). Clone the stock analog, override what differs, give the
   talent `grants: {1: <band id>}`. Requires a `client:` block (see client rules).
5. **Scripted proc** (`proc:` + `spell_script_names` + AuraScript in
   `EraTalentProcScripts.cpp`): behavior data can't express (VE's debuff-anchored heal).
6. **Core patch escape hatch** (LAST resort): behavior outside spell/proc/script reach
   (Shatter's crit-vs-frozen, patch 0018). Pattern: a SPELL_AURA_DUMMY marker with a
   reserved misc value, band-gated to [920000, 950000).

**BAND LEGITIMACY RULE (Phase 9.5, 2026-09-06 — applies to EVERY id you mint in the band).**
A band spell a character can **KNOW** that is *not* the current era's node grant, and *not* a
`spell_ranks` rank above a node-granted r1, **MUST** have an `era-data/band-allowlist.yaml`
entry (era / class / kind / optional gating node) — otherwise `EraBandClassifier::Sweep`, which
runs at the end of `ReconcileBaselineSpells` for **every** era including WotLK, strips it as
`UNCLASSIFIED` and logs a WARN. `tools/era_audit.py`'s `check_band_ownership` reports a
finding until the entry exists — and the audit must be clean to ship — so the miss is caught in the
pipeline, before it can reach a character. The allowlist is
the ONLY place non-node legitimacy is written down; `.eratalents doctor` and reconcile both call
the same `EraBandClassifier::Classify`, so they cannot disagree. Never list a node grant there
(audit check 4 refuses it), and never author a hand-written `removeSpell(<band id>)` inside
reconcile again — the sweep owns every `[920000, 950000)` strip. Ids that exist only as
SCRIPT-CAST payloads nobody learns (Misery debuffs, Improved Faerie Fire debuffs, Wyvern Sting
wake-DoT) are deliberately **not** allowlisted: a `UNCLASSIFIED` WARN if one ever lands in
`character_spell` is the desired signal. **Phase 9.5 minted no ids** — next free TBC helper stays
`949060`, next free DUMMY-marker misc stays `19`.

DUMMY-marker misc registry (band-gated, must stay distinct): 1 Shatter — Vanilla node 18045
(passives 920360-920364) AND TBC node 20857 (passives 942856-942860); patch 0018's band is
`[920000, 950000)` (widened 2026-09-05, TBC Phase 9, so the TBC passives arm it), which makes
misc 1 the first MECHANIC flag deliberately shared across eras (workflow lesson 25), 2 fire ward,
3 frost ward, 4 Improved VE, 5 Corruption era cast-time (marker 932930, patch 0019),
6-8 RETIRED (were Improved Voidwalker / Firebolt / Lash of Pain — reworked 2026-08-16 to plain
owner-side SPELLMODs; a pet's cast resolves the OWNER's spellmods via GetSpellModOwner, the stock
Demonic Power 18126 mechanism — do NOT re-claim 6-8 until every deployed DB has the rework),
9 Master Demonologist, 10 Improved Enslave Demon, 11 Improved Drain Mana, 12 Improved Firestone,
13 Improved Spellstone, 14 Improved Sap (Rogue — stay-stealth chance read by era_rog_sap_stealth),
15 Improved Berserker Rage (Warrior — rage-on-cast amount read by era_improved_berserker_rage, bound
to stock Berserker Rage 18499).
Next free: 19. (16 = Improved Scorpid Sting stamina-% — hunter node 18328, read by era_hunter_scorpid_sting;
17 = TBC Improved Seal of the Crusader crit-% — paladin node 20045, read by era_pal_jotc_crit off the
JotC clones' 0-base aura197 EFFECT_1; 18 = TBC Heart of the Wild bear-Stamina full-Intellect — druid
node 20135, marker spell **946280**, read BY SPELL ID (`HasAura(946280)`, the 0019 idiom) by **core
patch 0023** in `HandleShapeshiftBoosts` to skip the `amount / 2` halving for the BEAR spell 24899.)
Id bands: talent passives `920000 + (nodeId-18000)*8 + (rank-1)`; helpers hand-assigned in
the band; **932999 = generation sentinel (reserved, never author it)**.

**Reserved creature-entry band `[920000, 921000)`** (`TOTEM_CREATURE_BASE`/`TOTEM_CREATURE_END`
in `tools/gen_era_talents.py`) — era totem NPCs (`totems:` entries, see below) live here. This is
a **creature_template `entry` id space**, entirely INDEPENDENT of the `[920000, 950000)` **spell**
band above — the two ranges share their starting number by coincidence, not by design, and a
lookup in one table says nothing about the other. Verified free of any stock `creature_template`
row across the whole band as of 2026-08-23.

## Deleting a stock trainer row: the orphaned-higher-rank check (MANDATORY)

Era rank chains routinely `DELETE` a stock spell's `trainer_spell` row so an era character cannot
buy the stock version alongside its custom clone. **That deletion is global** — it removes the row
for WotLK-stage characters too — so before deleting any rank, check what else depends on it.

**The trap:** a higher rank's `ReqAbility1` may point at the rank you are deleting. Deleting it
does not just remove that rank; it makes **every rank above it unbuyable forever**. Found live on
2026-08-31 during TBC druid Phase 2: deleting stock Mangle r2/r3 (33982/33983/33986/33987) also
locked WotLK druids out of r4/r5, because `48563.ReqAbility1 = 33987` and
`48565.ReqAbility1 = 33983`. A native WotLK feral druid was left permanently at Mangle rank 1 —
a regression with nothing to do with eras.

**Run this before shipping any migration that deletes a trainer row.** It lists every trainer row
whose prerequisite your own migrations delete and which nothing else grants:

```bash
IDS=$(grep -ohE "DELETE FROM \`?trainer_spell\`? WHERE[^;]*" \
        modules/mod-era-talents/data/sql/world/base/*.sql \
      | grep -oE '[0-9]{4,6}' | sort -un | paste -sd,)
docker exec ac-database mysql -uroot -pchangeme_db_password acore_world -N -e "
SELECT ts.SpellId AS orphaned_higher_rank, ts.ReqAbility1 AS deleted_prereq, ts.ReqLevel, ts.TrainerId
FROM trainer_spell ts
WHERE ts.ReqAbility1 IN ($IDS) AND ts.ReqAbility1 NOT IN (SELECT SpellId FROM trainer_spell)
ORDER BY ts.ReqLevel;"
```

**A hit is not automatically a bug** — it is a bug only if nothing grants the deleted prerequisite
back. Every hit must be one of:

1. **Compensated by a `kBaselineSpellGates` row with `grantHigherEra=true`** — the gate force-grants
   rank 1 at `minLevel` to post-era characters, which satisfies the higher rank's `ReqAbility1`.
   This is why the four shipped Vanilla deletions are correct: priest Holy Nova 15237 (node 18121),
   priest Divine Spirit 14752 (18113), paladin Consecration 26573 (18606), warrior Shield Slam
   23922 (18552). Verify the row exists — do not assume it.
2. **Compensated by an explicit reconcile level-grant**, e.g. the druid `kWotlkDruidNG` arm that
   hands stock Nature's Grasp back to WotLK druids by level.
3. **Not needed at all, because the stock chain is already self-gating.** This is the preferred
   outcome and the one most often missed: if the stock higher rank's `ReqAbility1` is the *stock*
   rank 1, and the era character is granted a *clone* instead, it can never satisfy that
   requirement and can never buy the stock rank. **The DELETE was unnecessary — remove it rather
   than adding a compensating grant.** That was the resolution for Mangle.

Prefer option 3, then 1/2. Adding a compensating grant to paper over an unnecessary DELETE is the
wrong direction: it widens the blast radius instead of narrowing it.

**Not every `ReqAbility1 0` row is a leak.** The shaman weapon-skill rows **Two-Handed Axes 197**
and **Two-Handed Maces 199** carry `ReqAbility1 0` **BY DESIGN** — a WotLK shaman has those skills
as baseline, so the trainer row has no prerequisite to name. The era gate for them is the **equip
layer** (`era_talent_weapon_gate` + the proficiency clone **932406**), not the trainer row. Do not
"fix" them, and do not read them as an ungated era leak during a trainer sweep.

### Gotcha: `EffectMiscValue` is spelled `school:` on a single-effect `stat` node and `misc:` on a `multi` one

A `mechanic: stat` node passes `EffectMiscValue` through the key **`school:`** (`gen_era_talents.py:895`,
**default 127**), while a `stat` effect inside a `mechanic: multi` node passes the same field through
**`misc:`**. So a node granting `+Intellect` writes `school: 3` (= `STAT_INTELLECT`), which reads like
a school mask and is not one. Getting it wrong is silent: the generator emits whatever you give it and
`era_audit.py` has no check for it.

Worse, the `school:` **default of 127** means omitting it on a `stat` node produces an all-schools
value rather than an error — correct for a resistance aura, wrong for a stat one. Always write it
explicitly and decode it in a trailing comment, e.g. `school: 3  # EffectMiscValue = STAT_INTELLECT`.

Three TBC shaman Elemental nodes hit this split (20203, 20213, 20214/20217 on either side of it), and
it is the single most likely thing an author of the six remaining TBC classes gets wrong.

## TBC id layout (band raised to [920000, 950000))

The reserved custom-spell band's upper bound is now **950000** (was 933000, raised to make room
for TBC content — `tools/gen_era_talents.py`'s `CUSTOM_PASSIVE_END`). The 932999 generation
sentinel is unaffected — leave every reference to it alone; only the band's outer bound moved.
Layout inside the raised band, era-1 (TBC) only:

| Sub-range | Purpose |
|---|---|
| [920000, 933000) | Vanilla (era 0) — unchanged, see the registry above. |
| [933000, 936000) | (gap — reserved headroom between the eras, unclaimed) |
| [936000, 944000) | TBC auto-passives — `custom_passive_id(nodeId, rank)` off TBC node ids [20000, 21000), stride matching the Vanilla `920000 + (nodeId-18000)*8 + (rank-1)` formula but based at 936000. |
| [944000, 946000) | TBC pet-buff window (`ERA_PET_WINDOW[1] = (944000, 946000)` in `tools/gen_era_talents.py`). Default `petBuffBase` for era-1 datasets; a class with pet-buff nodes claims its own sub-slice via the dataset's `petBuffBase:` override — planned: hunter at 944000, warlock at 945000, each claimed when that class's TBC phase lands. **Hunter CLAIMED 944000 in TBC Phase 7 but currently emits NOTHING into it** (2026-09-05, gen `76709510`): every TBC hunter pet node ended up needing a HAND carrier via `petBuffIds` — a 119 `APPLY_AREA_AURA_PET` pair (Spirit Bond 948293/948294), per-rank proc carriers (Ferocious Inspiration 948295-948297), or a `mechanic: pet` DUMMY whose payload is a helper (Focused Fire 948302/948303) — so the generator emitted no auto pet-buff row and `[944000, 946000)` is EMPTY in the live DB. The `petBuffBase:` key stays declared on the dataset so a later hunter node that needs the auto path has the window reserved. **warlock CLAIMED 945000 in TBC Phase 8 (2026-09-05, gen `4ec433f1`) and likewise emits NOTHING into it** — spec 4c ("first real emission into the era-1 pet window") was not borne out: all 17 demon pet-passive carriers (Felguard 30147/30148/30149 included) are LIVE_MATCH, so every TBC warlock pet-stat talent is an **owner SPELLMOD** onto the carrier's family bit (Fel Stamina 0x8000000 + a self `MOD_INCREASE_HEALTH_PERCENT`; Fel Intellect 0x10000000 + a self `MOD_INCREASE_ENERGY_PERCENT` — **never grant 18731-18744, live is Fel Vitality and carries the other half**; Unholy Power / Demonic Tactics / Demonic Resilience likewise), and the two nodes that DO put an aura on the demon (Soul Link 948551, Master Demonologist 948560-948564) use hand carriers in the helper slice. `SELECT COUNT(*) FROM spell_dbc WHERE ID BETWEEN 945000 AND 945999` = **0**. The `petBuffBase:` key stays declared so a later warlock node needing the auto path has the window reserved. |
| [946000, 950000) | TBC hand-helper window (`ERA_HELPER_WINDOW[1] = (946000, 950000)`) — hand-assigned helper ids, same convention as the Vanilla 932900+ registry above. **Empty per-class slice table below, to be filled as each TBC class phase claims helpers:** |

| Range | Owner | Status |
|---|---|---|
| 946000-946255 | Paladin (TBC Phase 1) — **946000-946146 USED** (946115 skipped; 946147-946255 free-within-slice). Fresh wago-2.5.4 seal clones: SoR 946000-026, SoC 946027-033, SoL 946034-049, SoW 946050-062, SoJ 946063/946079, SotC/JotC 946064-077, **marker 946078** ("Era: TBC Paladin" seal-training, byte-copy of Vanilla 932749), SoB 946080-083, SoV 946084-086, Holy Shock 946087-089, Ardent Defender 946090-094, Holy Shield 946095-096, Redoubt procs 946097-101, Avenger's Shield 946102, flat Judgement of Command 946103-108, Vengeance 946109-113, Crusader Strike 946114. Later: 946116-946135 active-rank trainer chains (Holy Shock/Holy Shield/Avenger’s Shield r2+), 946136/946137 BoS r5 + block helper, 946138-946140 TBC Vindication all-stat debuffs. **FINAL REVIEW 2026-09-06 claims 946141-946146:** **946141-946145** Illumination clones of 20210/20212-215 with `EffectBasePoints_2` 59 (= the TBC 60% refund vs live's WotLK 30%), `scriptBindings`-bound to the CORE AuraScript `spell_pal_illumination` and each carrying its own clone `spell_proc` row mirroring the module's widened `-20210` row; **946146** Repentance clone of 20066 (`durationIndex` 32 = 6 s, `targetCreatureType` 64 = Humanoid-only). Nodes 20008/20060 grant them and the era-keyed `kEraPalStockSwaps` table strips stock 20210-20215 / 20066. Same review: SoJ 946063+946079 `manaCostPct: 10`, SoB 946080 flat 210 mana, SoV 946084 flat 250 mana, Crusader Strike 946114 `cooldown: 6000` (it had been inheriting WotLK 35395's 4 s), SoL 946034-038 `ppm: 10` / SoW 946050-053 `ppm: 12`. Sanctified Judgement uses auto-passives 936464-466 (no helper id). DUMMY-marker misc 17 consumed (Improved SotC crit, node 20045). Record: `docs/verification/era-talents-tbc-phase1-paladin.md`. | claimed |
| 946256-946511 | Druid (TBC Phase 2, class 11 / family 7 — the inversion trap) — **946256-946286 USED (946264 SKIPPED, see below); free-within-slice starts at 946287.** This row said "946277-946511 free" until 2026-09-06 — FALSE: **946277-946279** are the Improved Faerie Fire companion debuffs (node 20118's aura-109 ADD_TARGET_TRIGGER targets, fix round §13.2) and **946280** is the Heart of the Wild bear-Stamina DUMMY marker (misc 18) that arms core patch 0023. **946256/946257** Leader of the Pack TBC clone pair (+5% party crit): 946256 is the hidden `PASSIVE|DO_NOT_DISPLAY` form-passive, ShapeshiftMask 145 (cat/bear/direbear), Effect_1 TRIGGER_SPELL -> 946257; 946257 is the visible `APPLY_AREA_AURA_PARTY` aura 52 +5% at radiusIndex 11, **templated off STOCK 24932** so it inherits `SpellClassMask_2 = 2048` (Improved LotP node 20139's spellmod binds it) and `ProcTypeMask 4436`, with its own crit-only (`HitMask 2`) `spell_proc` row and a `scriptBindings` entry to the **core** script `spell_dru_leader_of_the_pack` — that script is bound BY ID to stock 24932, so the on-crit heal is simply dead on a clone without the bind (the same split-and-bind pattern as Vanilla 932503/932506). **946258-946260 / 946261-946263** Mangle (Bear) / Mangle (Cat) r1-r3 (templates 33878/33986/33987 and 33876/33982/33983): the ONLY Bear override is `DurationIndex 29` (12s vs live 3 = 60s) — the Bear clones author no `effects:` at all and inherit everything; Cat adds EFFECT_2 basePoints **159** (=160%, vs live 199). **Do NOT revert the bleed-amplify to TBC's literal aura 4 / misc 0** — a bare `SPELL_AURA_DUMMY` does nothing in 3.3.5a, so a literal copy ships Mangle with NO bleed amplification; keep the live aura 255 `MOD_MECHANIC_DAMAGE_TAKEN_PERCENT` / misc 15 carrier (+30% is identical in both eras, only the encoding changed). Node 20141 grants 946258 + 946261 directly and **never 33917** (a `SPELL_EFFECT_LEARN_SPELL` wrapper — the `trainer-rows-can-be-learn-wrappers` trap); r2/r3 are trainer-taught on TrainerId 33 at L58/68 behind a `ReqAbility1` chain + `spell_ranks` roots 946258/946261. **946264 was ALLOCATED (spec Amendment A.4) for an "Era: TBC Druid" version-swap marker and was NEVER USED** — the shipped reconcile arms gate on era + node rank + `EraReplacementSpellReady`/`EraNodeIsWired` (since 2026-09-03 `EraNodeIsWired` also requires `EraHasTalentTrees(era)`, so every arm keyed on it is inert while the era is outside the allowlist — affects druid 20133 / paladin arms too), and every trainer chain anchors on its talent-granted rank 1, so no marker anchor was needed (contrast paladin 946078, whose seals have no talent anchor). **946264 is FREE.** **946265/946266** Tree of Life (Route B, Amendment A.2, `data_only`, NO core patch): 946265 = hidden `PASSIVE|DO_NOT_DISPLAY` FORM_TREE form-passive (ShapeshiftMask 2) triggering 946266 = `APPLY_AREA_AURA_PARTY` aura **115 MOD_HEALING** misc 127 radiusIndex 11, amount = **25% of the caster's total Spirit** via the `era_dru_tree_of_life` `DoEffectCalcAmount` (basePoints 0, so a script that never runs ships nothing rather than a wrong magnitude; `canBeRecalculated=false` pins it at form entry). The core hardcasts WotLK's 34123 (+6% raid `MOD_HEALING_PCT`) on FORM_TREE unconditionally at `SpellAuraEffects.cpp:1359`, so the TBC aura must SUPPRESS it: `era_dru_tree_of_life_suppress` is bound to **stock 34123** (a script binding, not a `spell_dbc` override — same idiom as the shipped stock-Sap 18499/5138 binds) and returns false from `DoCheckAreaTarget` for an era-managed TBC druid (detected by `HasSpell(946265)` — ownership, never "is a bot"), leaving the aura owned but applied to nobody; `DoEffectCalcAmount -> 0` is the fallback. **946267/946268** Nature's Grace: stock 16880 procs at **33%** on this core (WotLK split it into a 3-rank 33/66/100% chain and left r1 at 33%) while TBC is 100% and node 20112 is maxRank 1 — so 946267 is a clone of 16880 at `ProcChance 100` triggering 946268, a clone of the -500ms `ADD_FLAT_MODIFIER` misc-10 shape at `DurationIndex 8` = 15s (TBC's real window; the Vanilla clone 932504 persists until consumed). **946269** Omen of Clarity castable TBC clone (30 min `DurationIndex 30`, `ProcTypeMask 20` melee-only, mana 120, `Attributes 0` + instant so it is a real spellbook entry) with its OWN hand `spell_proc` row (3.5 PPM + **10s Cooldown**) and a `spell_script_names` bind to `spell_dru_omen_of_clarity` — the Vanilla clone 932512 is NOT reused and its single shared proc row is untouched. **946270-946275** Nature's Grasp r1-r6 (35% / 1 charge / outdoor-only `Attributes 98304` / `DurationIndex 22` 45s / **powerCost 0** — TBC NG has NO `SpellPower` row on any rank, which is why the Vanilla 932505/932507-511 chain could not be reused); node 20101 grants r1, r2-r6 trainer-taught on TrainerId 33 at L18/28/38/48/58 behind `ReqAbility1` + `spell_ranks` root 946270. **946276 was OFFERED for a Faerie Fire (Feral) clone and DECLINED** (node 20133 keeps stock 16857) — and was later CLAIMED by the 2026-08-31 real-client fix round for the **Tree of Life FORM clone** (see below): `SpellInfoCorrections.cpp:1266` clears `SPELL_ATTR1_IMMUNITY_TO_HOSTILE_AND_FRIENDLY_EFFECTS` from 770/16857 BY ID and the generator has no `attributesEx` scalar to reproduce it; `Spell.cpp:8869` hardcodes `Id == 16857` to pre-cast 60089 in Bear/Dire Bear; and the shipped reconcile arm 5 + `kBaselineSpellGates` walker guard are keyed on this node granting stock 16857. **REUSED (only two, Amendment A.3 "fidelity to the letter"):** Vanilla **932500-932502** Predatory Strikes (node 20130) and **932513** Enrage (reconcile grant at L12). **946276** Tree of Life FORM clone of stock 33891 (fix round 2026-08-31, spec Amendment A.2 ROUTE A on top of the shipped Route B): node 20161 grants THIS, not stock 33891. Mechanically byte-identical — both effect tuples re-authored verbatim (aura 36 MOD_SHAPESHIFT misc 2 = FORM_TREE, aura 77 MECHANIC_IMMUNITY misc 17) and every other field inherited, StanceBarOrder 4 and ManaCostPct 28 included; `skillLine: 573` reproduces stock 33891's SkillLineAbility row 15089. ONLY the client Description/AuraDescription change: stock 33891 hardcodes `$34123s1%` / `$34123a1 yards`, which the client resolves against the WotLK 34123 row (+6%, 100 yd raid), so BOTH the talent panel (the addon hyperlinks a node's granted spell) and the spellbook promised WotLK behavior over a TBC mechanic. Safe because the form is keyed on the AURA's misc value, never the spell id — HandleShapeshiftBoosts's 34123 hardcast AND its PASSIVE|DO_NOT_DISPLAY Stances sweep (what fires 946265 -> 946266) both switch on GetMiscValue(), Unit::GetModelForForm hardcodes only 7090/35200, and 33891 appears nowhere by id in src/server. EraTalents.cpp CLASS_DRUID reconcile arm (7) strips stock 33891 from a managed-era druid (migration: characters the previous build granted it to). **946277-946280 CLAIMED by the 2026-08-31 tooltip-fidelity fix round (see below); 946281-946286 = Insect Swarm r1-r6** (node 20107, deferred-gaps round 2026-09-07, G-51 — wago 2.5.4 per-tick 18/32/50/72/99/132, -2% hit, mana 50/85/110/135/155/175; r2-r6 trainer-taught on TrainerId 33 behind a `ReqAbility1` chain + `spell_ranks` root 946281, and reconcile arm (9) strips stock 5570/24974-24977/27013/48468); **946287-946511 free.** Auto-passives 936800-937284 (141 rows). **Generator gained three purely-additive capabilities** (verified byte-neutral for already-shipped data): `op: bonusMultiplier` (SPELLMOD_BONUS_MULTIPLIER 24), a `tree` stance token (FORM_TREE mask bit 2), and a `procTypeMask:` `_HELPER_SCALARS` entry. **946277-946279** Improved Faerie Fire companion debuffs r1-r3 (node 20118, fix round 2026-08-31): TBC delivered that talent as two SPELLMODs onto Faerie Fire's own EFFECT_2/EFFECT_3 (aura184 melee hit / aura185 ranged hit at base 0), but stock 3.3.5a Faerie Fire has NO Effect_2 and its Effect_3 is aura**186** SPELL hit — so the shipped spellmod pair was half inert and half wrong-stat, promising melee+ranged and delivering neither. The node is now ONE `aura: 109` `SPELL_AURA_ADD_TARGET_TRIGGER` stat effect (chance 100, mask A=1024 — verified the ONLY two family-7 spells carrying that bit are 770 and 16857) that casts these three on the Faerie Fire target; they carry the REAL aura184+aura185 at +1/2/3%, read off the victim by `Unit::GetUnitMissChance`. **Cloning Faerie Fire was REJECTED** (the shape node 20118 originally wanted): a clone of 16857 loses two by-id core behaviours — `Spell.cpp:8869` pre-casts 60089 only for `Id == 16857`, which is what lets a BEAR cast Faerie Fire (Feral) without leaving form, and `SpellInfoCorrections.cpp:1266` clears `SPELL_ATTR1_IMMUNITY_TO_HOSTILE_AND_FRIENDLY_EFFECTS` by id — i.e. it trades a tooltip bug for a broken bear-form cast (same reasoning that keeps node 20133 on stock 16857). They `template: 770` to inherit its DispelType/school/duration/range, but MUST override `attributes` (stock 770 carries `NOT_SHAPESHIFTED`, fatal for the Feral path; 67108864 `AURA_IS_DEBUFF` is required because the amounts are POSITIVE numbers that are bad for the target) and **`attributesEx: 0`** — the clone-of-a-corrected-spell trap: 770's AttributesEx 98304 includes a bit the core clears BY ID, which a clone never receives. A hand `spell_ranks` chain (`2026_08_31_05_..._iff_ranks.sql`) roots them at 946277 so two druids of DIFFERENT talent rank cannot stack two unrelated ids that both carry aura184/185. **946280** "Era: TBC Heart of the Wild" marker (node 20135) — DUMMY misc **18**, arms **core patch 0023**; no `client:` block (never a trainer ReqAbility, so the 932939 trainer-UI nil-format crash cannot arise). **DUMMY-marker registry misc 18 consumed — next free 19.** **Generator gained three MORE additive capabilities in the fix round** (all TDD-covered, byte-neutral for shipped data): `trigger:` and `affects:`/`affectsMask:` on a `type: stat` effect, `SpellClassSet` now emitted whenever ANY effect carries a mask (not just spellmods — a family-0 masked effect matches EVERY spell because `SpellInfo::IsAffected` opens `if (!familyName) return true;`), a hard lint against a maskless or trigger-less `aura: 109`, and an `attributesEx` `_HELPER_SCALARS` entry. Record: `docs/verification/era-talents-tbc-phase2-druid.md`. | claimed |
| 946512-947511 | Shaman (TBC Phase 3, class 7 / family 11 — the inversion trap, mirror of druid's class 11 / family 7) — **CLOSED OUT at Plan 3b Task 8 (2026-09-02, gen `8e093eb4`). USED: 946512-946539 (28 tree helpers), 947000-947351 (**157** totem clone ids in per-family decade-aligned blocks — counted from the emitted `- id:` rows, not the 146 this row used to state — Earthbind 947000, Stoneskin 947010, Strength of Earth 947030, Stoneclaw 947050, Tremor 947070, Earth Elemental 947080, Searing 947090+947100, Magma 947109 (borrowed from Searing, ledger `lent_to`), Flametongue 947120, Frost Res 947140, Fire Elemental 947150, Healing Stream 947170, Mana Spring 947190, Fire Res 947200, Poison Cleansing 947210, Disease Cleansing 947220, Grounding 947240, Nature Res 947250, Sentry 947260, Grace of Air 947270, Windwall 947280, Tranquil Air 947290, Wrath of Air 947300, Fire Nova 947310, Windfury 947330, Mana Tide 947340, Totem of Wrath 947350), 947440 "Era: TBC Totems" marker, 947441 Windfury Weapon r5, **947442 Flametongue Attack / 947443 Nature's Guardian heal / 947444 Elemental Focus / 947445 Clearcasting** (the four fix-round clones — this row previously called 947442-947511 free, which is FALSE). FREE: 946540-946999 and **947446-947511**. Creatures: 51 fresh `creature_template` entries 920300-920580 (+ the reused Vanilla 920100-920290); auto-passives 937600-938073 (176 rows). NO DUMMY-marker misc consumed (next free stays 19). Record: `docs/verification/era-talents-tbc-phase3-shaman.md`.** Planned layout: **946512-946611** tree helpers (Plan 3a), **947000-947439** TBC totem summon/pulse clones — **the stride-20 formula this row used to give (`summonBase = 947000 + i*20`) is SUPERSEDED and must not be used to compute a base.** It overflows (26 families x 20 = 520 ids, running straight through the 947440-947511 Windfury/marker sub-range) and it collides with shipped data: the formula puts family index 6 (Searing) at 947120/947130, which are the ids Flametongue actually holds. **The authoritative allocation is the packed per-family table `id_allocation:` at the bottom of `era-data/_ref/tbc/shaman-totems-tbc.yaml`** — decade-aligned blocks sized to each family's real need. Read that table; never compute a base. **947440-947511** Windfury Weapon clones + the "Era: TBC Totems" marker (Plan 3b). Creature entries: reused families keep the Vanilla clones' 920100-920290; fresh TBC ranks take `920300 + i*10` in the reserved `[920000,921000)` creature band. **ELEMENTAL tab (Plan 3a Task 6, 2026-08-31) claims 946512 + 946513. ENHANCEMENT tab (Plan 3a Task 7, 2026-09-01) claims 946514-946526 — 946527-946611 free within the tree-helper sub-range.** **946512** "Focused Casting", the buff TBC's Eye of the Storm (node 20209) procs — a FROM-SCRATCH build (stock 29063 is WAGO_ONLY: absent from `Spell.dbc` AND `acore_world.spell_dbc`), authored at wago 29063's own values throughout: per-effect `maskA 2499` (word-A bits {0,1,6,7,8,11} = LB | CL | Healing Wave | LHW | Chain Heal | Ghost Wolf), `SchoolMask 2`, `DurationIndex 32` (= 6000 ms, matching the cached TBC tooltip's "lasts for 6 sec"). **Both of that spell's "unknowable" calls were retracted in the Task-6 fix round** — spec **Amendment A.12** establishes that per-effect class-mask **word 0** IS comparable between wago and live (higher words are not), which killed the whole-family-bind justification; and the `_ref`'s TBC `DurationIndex 21` reading was simply a misread (wago gives 32, which is 6000 ms in the TBC and 3.3.5a tables alike), so there was never a duration conflict and nothing here is on the real-client queue. **946513** Elemental Mastery — FRESH per Amendment A.9 (the shipped Vanilla clone 932413 matches on effects but carries `DurationIndex 9`/30 s vs TBC 21 and `ProcTypeMask 69632` vs TBC 87376): `template: 16166`, category neutralized + `RecoveryTime 180000`, `DurationIndex 21`, `ProcTypeMask 87376`, FLAT `SPELLMOD_CRITICAL_CHANCE` +100 (aura 107 misc 7) + PCT `SPELLMOD_COST` -100% (aura 108 misc 14), both per-effect-masked to `maskA 2416967683` = the five-spell fire/frost/nature damage set, **plus its own hand `spell_proc` row** (a clone inherits none). Neither helper needs a `scriptBindings:` entry (`_ref.script_bindings` has no row for 29063 or 16166). Two Elemental-tab masks are deliberately WIDER than wago's word 0 and say so in the dataset: **20204 Call of Flame** and **20212 Elemental Fury** each keep word-B bit18 (Fire Nova blast 8349), because the TBC tooltips name Fire Nova Totem and the era Fire Nova clone Plan 3b mints will carry its identity in word B on this build, not word A; 20212's live word-B bits 12/13 (WotLK-only spells) are dropped. Two Elemental-tab **accepted gaps** are recorded in `_ref.accepted_gaps`: **id 6** (Clearcasting 16246 granted stock, so its WotLK mask lets an already-earned charge be spent on a heal — wago 2416967683 vs live 2551185859) and **id 7** (Improved Fire Totems' Fire-Nova activation-delay half is not shipped; Plan 3b must extend `npc_era_fire_nova_totem::ResolveDetonateDelay`, which reads the Vanilla auto-passives 926464/926465, to this node's 937664/937665). Lightning Overload (node 20218) claims **no helper id**: it ships as `mechanic: proc-scripted`, so `spell_sha_lightning_overload` binds POSITIVELY to the five auto-passives 937744-937748 (spec §5's "bind each rank id individually" alternative — no `spell_ranks` root, no hand SQL). NO DUMMY-marker misc consumed by this tab (next free stays 19). **ENHANCEMENT (Plan 3a Task 7, 2026-09-01) claims 946514-946526:** 946514-946518 Flurry haste buffs r1-r5 (FROM-SCRATCH, deliberately NOT `template: 16257` — the generator has no `AttributesEx3` key, so a template clone would inherit live 16257's `SPELL_ATTR3_INSTANT_TARGET_PROCS` 524288 that TBC does not have; five per-rank POSITIVE `scriptBindings` to `spell_sha_flurry_proc` + five `spell_proc` rows at ProcFlags 4 / Cooldown 500), 946519 Spirit Weapons threat clone (template 36591, aura 10 MOD_THREAT −30% at TBC's school mask misc **1** = physical only vs live's 127), 946520 Stormstrike (template 17364, TBC's literal aura **87** MOD_DAMAGE_PERCENT_TAKEN misc 8 = +20% Nature from EVERYONE, ProcCharges 2, ProcTypeMask 139944, RecoveryTime 10000 from the cached TBC tooltip), 946521-946525 Unleashed Rage party AP buffs r1-r5 (FROM-SCRATCH, aura 166 at +2/4/6/8/10% with `ImplicitTargetA 20` TARGET_UNIT_CASTER_AREA_PARTY + EffectRadiusIndex 9 — five ranks, not three), 946526 Shamanistic Rage (template 30823, EFFECT_1 EFFECTIVE 30 vs live 15, RecoveryTime 120000, a `scriptBindings` entry to `spell_sha_shamanistic_rage` and a `spell_proc` row at **ProcsPerMinute 18**). 946526 also carries one ACCEPTED inherited-attribute gap — it inherits `AttributesEx5` `SPELL_ATTR5_ALLOW_WHILE_STUNNED` from its template, a WotLK addition the generator has no key for; the from-scratch escape the Flurry clones took is the wrong trade for a castable active (see `_ref.accepted_gaps` id 9, which is the NORMATIVE statement of both the gap and its one-line fix — do not restate it elsewhere). That last row needed a generator change: `spell_proc.ProcsPerMinute` was hard-coded 0, and a `ppm:` proc key was added (`tools/gen_era_talents.py`, test `test_proc_ppm_column`) — omitting it keeps the historical 0, so it is byte-neutral for shipped data. No DUMMY-marker misc consumed. **RESTORATION (Plan 3a Task 8, 2026-09-01) claims 946527-946539 — 946540-946611 free within the tree-helper sub-range:** 946527-946529 Ancestral Fortitude armor buffs r1-r3 (node 20244; FROM-SCRATCH per `_ref.reuse_ledger`, the shipped Vanilla clones 932408-410 use aura **142** `MOD_BASE_RESISTANCE_PCT` = a BASE_PCT modifier where TBC's aura **101** `MOD_RESISTANCE_PCT` is TOTAL_PCT — a different buff on a geared 70 — and templating live 16177 would inherit its `AttributesEx6` bit; carries `family: 11` + **`maskB: 67108864`**, which is LOAD-BEARING because node 20249's dispel-resist spellmod binds that bit, the same defect the Task-7 fix round found on Flurry's missing `maskB: 512`), 946530 Healing Way stacking buff (node 20252; `template: 29206` for its Healing-Wave mask A 64, aura 283 `MOD_HEALING_RECEIVED` +6%, `stackAmount: 3`, plus the ONE field that forced FRESH over the shipped 932411 — **`attributes: 327680`**), 946531 Nature's Swiftness (node 20253; `template: 16188`, Category/CategoryRecoveryTime neutralised + `RecoveryTime 180000` = the cached TBC tooltip's 3-min cooldown vs stock's 2-min *shared* Category 1202 — the swept column list contains neither RecoveryTime nor Category, so `_ref` could not settle it; FRESH rather than reusing the field-identical Vanilla clone 932412 because the reuse ledger deliberately closes at four assessed candidates), 946532-946536 Nature's Guardian passives r1-r5 (node 20257; HAND CLONES rather than auto-passives because the talent needs TWO slots the emitters cannot produce — e1 aura 42 -> 18350 at EFFECTIVE **10 on every rank** (the heal %, live is 3/6/9/12/15) and **an `effect: 0` slot 2 keeping LIVE's EFFECTIVE-30 carrier**, which `spell_sha_nature_guardian::CheckProc` reads as `Effects[EFFECT_1].CalcValue()` (0-based core enum = DBC slot 2) and TBC simply does not populate, so a literal clone yields threshold 0 and the talent NEVER fires; ProcChance 10/20/30/40/50 (live 100) and a per-rank `spell_proc` row at **Cooldown 5000** from the cached TBC tooltip's "5 second cooldown" vs the stock row's 30000; five POSITIVE `scriptBindings` to `spell_sha_nature_guardian`; no `client:` block, so `_grant_is_client_visible` keeps the panel on the authored prose), 946537-946539 Earth Shield r1-r3 (node 20260; `template:` 974/32593/32594, overriding `powerCost` 300/375/450 + **`manaCostPct: 0`** — TBC's flat cost vs live's 19/19/15% — and `procTypeMask: 139944`; a per-rank `spell_proc` row carrying the stock row's **AttributesMask 2**; r1 talent-granted, r2/r3 trainer-taught on TrainerId 14 at L60/70 behind a ReqAbility1 chain + `spell_ranks` root 946537, hand SQL `2026_08_31_12_era_talent_tbc_shaman_earth_shield_ranks.sql`. **The stock rows are left UNTOUCHED — neither deleted NOR re-gated** (the migration adds two rows and removes/edits none; do not go looking for re-gate SQL): the orphaned-higher-rank check found stock **974 has NO `trainer_spell` row at all**, so `32593.ReqAbility1 = 974` can never be satisfied by an era shaman and the stock chain is already **SELF**-gating — framework option 3 — while a DELETE would have orphaned 49283/49284 for native WotLK shamans. This is a DELIBERATE DEPARTURE from the plan's Step 2 item 2, which asked for a marker re-gate preserving `ReqAbility1` as `ReqAbility2`: that SQL would change no behaviour and only widen the blast radius on native shamans, so it was not written. SIX `scriptBindings`: `spell_sha_earth_shield` per rank, AND IP's `isAllowedToCastSpell` per rank, the Naxx40 heal blocker a clone would otherwise bypass, rebound from THIS module's generated SQL). Node 20255 Purification is the `school:`-not-`misc:` case the gotcha section warns about (single-effect `stat`, `school: 8` = TBC's Nature mask vs live's 127 — and the key's default is 127, i.e. exactly the wrong value). **Two accepted gaps opened:** `_ref.accepted_gaps` **id 10** (Nature's Guardian's heal payload 31616 stays stock, so the 10%-of-max-health arrives as a 10 s `MOD_INCREASE_HEALTH` bump instead of TBC's flat `SPELL_EFFECT_HEAL`; the core hardcodes 31616 so closing it needs an era AuraScript) and **id 11** (Restorative Totems' `SPELLMOD_DOT` half is NOT shipped. **The reason stated here originally — "code-verified inert, since Healing Stream is `aura 23 -> 52041 dummy -> 52042 direct heal`" — was WRONG and is retracted (Plan 3b Task 2 batch 3, 2026-09-01):** that describes LIVE 5672, i.e. WotLK's rework, whereas TBC's own 5672 (and the reused clone 931190-931194) is `aura 8 PERIODIC_HEAL @2000`, whose ticks DO reach `SpellHealingBonusDone` at `damagetype == DOT`. The DISPOSITION is unchanged on the right reason: shipping both halves would **double-apply**, and TBC's two effects carry the same magnitude, so the surviving `ALL_EFFECTS` half delivers the whole tooltip exactly once. Only the Mana Spring half of the old reasoning survives — a periodic ENERGIZE that no SPELLMOD_DOT path touches. Generalise it: **for an era clone, the spell that matters is the CLONE at the ERA's shape, never the live stock id it shares a name with.**) `_ref.accepted_gaps` id 4 gained a THIRD word-C consumer: node 20248 Totemic Mastery's `affectsMask {a: 100786176, b: 2097152, c: 98304}` named the Vanilla air-totem pulse clones 931330/931350 at C15/C16, so Plan 3b had to re-point or re-bit it alongside 20222/20226. **CLOSED 2026-09-01 by Plan 3b Task 2 batch 4a, by the RE-POINT branch — not by re-bitting.** The minted TBC air pulses carry TBC's OWN word-A bits (Grace of Air 947275 = 131072/bit17, Windwall 947285 = 32768/bit15, the latter SHARED with Stoneskin Totem Effect 8072 exactly as TBC's own Guardian Totems 16258/16293 assume), and the Vanilla-minted word-C bits are dropped. Nodes 20222 and 20226 needed NO edit (their `affects:` names re-resolve and the emitted masks are byte-identical to wago's own talent rows); node 20248's mask became `{a: 100917248, b: 2097152, c: 0}`. All three verified against the emitted SQL. Normative record: `era-data/_ref/tbc/shaman-spells.yaml` `accepted_gaps` id 4 `batch4a_closure`. Plan 3a Task 5's DEFERRED doctor predicate is now SHIPPED (`EraTalentsCommand.cpp`, ids 946538-946539 gated on `CurrentRank(ERA_TBC, 20260)`; 946537 stays on the generic `NodesFor()` fallback because it IS the node's rankSpell). HEADLESS COVERAGE, stated exactly: Restoration totals **65** points against a 61-point cap, so no single character can spend the tab — two level-70 shaman bots covered **18 of 19** authored nodes between them (15 nodes / 55 pts and 16 / 59), all `known=yes spellInfo=yes` with zero learn rejections and zero 946xxx orphans. **Node 20259 Improved Chain Heal was spent by neither and is headlessly unexercised** — it is on the real-client queue. NO generator change and NO DUMMY-marker misc consumed by this tab (next free stays 19). **946512-946539 USED; 946540-946611 free.** | claimed |
| 947512-947767 | Warrior (TBC Phase 4, class 1 / family 4) — **HEADLESS-COMPLETE at Task 9 (2026-09-03, gen `85d67983`); awaiting real-client sign-off. USED: 947512-947569 (58 ids). FREE within slice: 947570-947767.** 66 nodes 20300-20365; auto-passives **938400-938927**. Breakdown: **947512-947529** Bloodthirst r1-r6 — six damage clones (45% AP / 30 rage / 6 s `CategoryRecoveryTime`, all four families diverged from live) + six 8 s/5-charge heal buffs + six FLAT heal payloads 10/13/17/20/25/30 (live's `spell_warr_bloodthirst_heal` recomputes as a PERCENT of max health, so the core scripts are deliberately NOT rebound; `era_bloodthirst_vanilla` is). **947530-947532** Devastate r1-r3 (template 20243/30016/30022) — TBC's ONE-HANDED weapon gate (live requires a shield) and the per-Sunder bonus moved to **DBC slot 3**, because the core reads it as `CalculateSpellDamage(2, ...)` = the 0-based EFFECT_2; **there is no Devastate `spell_script_names` row at all** — the core implements it inside `Spell::EffectWeaponDamage` keyed on `SpellFamilyFlags[1] & 0x40` (`SpellEffects.cpp:3418-3434`), so a census-only search concludes "unscripted" and ships a clone that applies no Sunder. **947533-947540** Rampage — r1-r3 abilities + three AP stack buffs (+30/40/50, `CumulativeAura 5`) + the crit-watcher passive **947539** + the 5 s "Rampage Ready" aura **947540**. This is the phase's flagged RECONSTRUCTION and stays escalation-rung 1 (pure data + one generator key, no module script, no core patch): TBC gates the ability on `CasterAuraState 11`, which is **dead on this core** (AzerothCore's `AuraStateType` has no 11 and nothing in `src/server/game` ever sets it), so the window is reproduced as a real aura applied by a crit-only proc passive with `casterAuraSpell:` pointing at it. TBC's own indirection through the bare DUMMY 18350 is deliberately NOT copied (the Mangle-DUMMY lesson). 947539 is granted by the `CLASS_WARRIOR` reconcile arm (6) `learnSpell` because `era_talent_rank`'s PK allows a node exactly ONE granted spell. **947541-947546** Shield Slam r1-r6 at TBC's damage ranges 225-235/264-276/303-317/342-358/381-399/420-440 — **Category 1209 KEPT**, not neutralised to TBC's 971, because `SpellEffects.cpp:351` gates the shield-block-value bonus on `GetCategory() == 1209`. **947547** Last Stand (category neutralised + `RecoveryTime` 480000 = TBC's 8 min; `era_last_stand` rebound). **947548** Death Wish (`mechanic: 0` + `dispelType: 0` — TBC's is neither an Enrage effect nor Enrage-dispellable). **947549** Sweeping Strikes (10 charges / 10 s vs live 5 / 30 s; core `spell_warr_sweeping_strikes` REBOUND — its body is id-agnostic). **947550** Concussion Blow (stun-only, `RecoveryTime` 45000, spell-level Mechanic 12, **no script bind — deliberate**; `spell_warr_concussion_blow` must NOT be rebound). **947551** Deep Wound bleed carrier — FROM SCRATCH, 3000 ms x 12 s = 4 ticks (the Vanilla clone 932800 is 1000 ms x 12, inherited from the WotLK carrier it templated off), `family: 4` + `maskB: 0x10` so Blood Frenzy binds it, and WITHOUT live 12721's `AttributesEx4 0x100` / `AttributesEx6 0x20000000` ignore-modifier bits. **947552** Mace Specialization payload — FROM SCRATCH (live 5530 is a REPURPOSED id): 3 s stun PLUS TBC's added `ENERGIZE` 7 rage. **947553-947555** Blood Craze per-rank regen payloads (only override is `EffectAuraPeriod` 6000/3000/2000 ms vs live 3000/1500/1000; aura 20 heals 1% max health per tick, so the 1/2/3% ladder is carried by tick count inside a 6 s window). **947556-947560** Enrage buffs at TBC's +5/10/15/20/25% (live 12880/14201-4 are +2/4/6/8/10), `ProcChance 100` + `ProcTypeMask 0x14`, and `dispelType: 0` + `mechanic: 0` so the era buff is not soothable. **947561-947565** Shield Specialization per-rank passives carrying BOTH halves (e1 aura 51 `MOD_BLOCK_PERCENT` +1..5, e2 aura 42 -> 947566) with their own `spell_proc` rows at **HitMask 64 (PROC_HIT_BLOCK)** — live's row for -12298 carries HitMask 112, WotLK's widened dodge/parry/block rule. A generated node row could not carry both halves (a `proc` carrier is aura-42-only, a `multi` node is stat/spellmod-only), which is why these are helpers per rank. **947566** their 1-rage `ENERGIZE` payload (wago 23602 = 1 rage; live 23602 = 5). **947567** Shield Bash - Silenced (`preventionType: 1`, `attributesEx2: 0`, `attributesEx3: 0`; `family: 0` authored against 18498's SpellClassSet **8** = SPELLFAMILY_ROGUE on BOTH sides — inert either way, authored 0 to keep the band inside the `{0,4}` family invariant; and 18498's TBC `ShapeshiftMask 0x8000000` deliberately NOT copied — bit 27 is not a warrior stance, so a clone carrying it would fail `Spell::CheckShapeshift` in every form a warrior can be in). **Trainer chains (hand SQL `2026_09_03_02_..._fury_chains.sql` + `_03_..._prot_chains.sql`, 7 `trainer_spell` + 9 `spell_ranks` rows each, TrainerId 1): NOTHING DELETED AND NOTHING RE-GATED** — every stock chain is already SELF-gating (its `ReqAbility1` roots at a stock id a TBC warrior never holds; stock Bloodthirst has no `trainer_spell`/`spell_ranks` rows on this server at all), so the orphaned-higher-rank check had nothing to flag. **Reuse ledger: ALL FRESH or MOOT — zero of the fourteen Vanilla warrior helper families reused.** The exemplar is Flurry 932810-814, *"MOOT, AND reuse would have been WRONG"*: TBC 12966-12970 are LIVE_MATCH on all fourteen families so node 20338 triggers the STOCK buffs, while the Vanilla clones carry 1.12.1's +10/15/20/25/30% against TBC's +5/10/15/20/25% — reuse would have overpaid every rank by 5 pp. **Generator gained two purely-additive `_HELPER_SCALARS` entries** (both byte-neutral for shipped helpers, TDD-guarded): `casterAuraSpell` (the Rampage gate) and `dispelType` (Death Wish / Enrage). **NO DUMMY-marker misc consumed — next free stays 19**: node 20337 Improved Berserker Rage re-uses the existing misc **15** marker that `era_improved_berserker_rage` already reads, so the talent needs no new C++ and no new binding. **FINAL REVIEW R7 (2026-09-06)** claims **947568/947569** — Blood Frenzy debuff clones (`template:` 30069/30070 + `mechanic: 15` MECHANIC_BLEED, which live dropped), with node 20318's `trigger:` re-pointed at them; closes _ref accepted_gaps id 6, and the pair joins `spell_group` 1108 in hand SQL 2026_09_06_05 because a clone does not inherit its template's group membership. The same round adds the Enrage clones 947556-947560 to `spell_group` 1104 (Warrior Enrages, EXCLUSIVE) alongside the Vanilla clones 932815-932819 — each clone needs its OWN row, since `LoadSpellGroups` accepts only chain roots and these are in no `spell_ranks` chain. Record: `docs/verification/era-talents-tbc-phase4-warrior.md`. | claimed |
| 947768-948023 | Rogue (TBC Phase 5, class 4 / family 8) — **947768-947787 USED, 20 ids; 947788-948023 free-within-slice.** A grant-heavy tree: 67 nodes needed only 20 hand helpers because TBC == live on most of the tree. **947768-947772** Find Weakness r1-5 self-buffs (the talent's armor-ignore is delivered as a finisher-armed buff, reconstructed — no stock carrier survives; node 20419). **947773-947776** Mutilate visible-chain clones r1-4 (Amendment A.14 — the +20% -vs-poisoned rider is core-hardcoded on `SpellFamilyFlags[1] & 0x6` (the hand-halves) at WotLK's +20%, so TBC's +50% cannot be expressed in data on the stock id; the clones carry TBC tooltips, and `era_rog_mutilate_poison` + `era_rog_mutilate_behind` supply the +50% top-up and the behind-target requirement. r1 node-granted (20420), r2-r4 trainer-taught behind a `ReqAbility1` chain — hand SQL `2026_09_04_02_era_talent_tbc_rogue_mutilate_trainers.sql`, nothing deleted). **947777** Riposte TBC clone (node 20428) — live 14251 is WotLK's slow-attack + combo point; TBC is a 6s DISARM, so `EffectMechanic_2 = 3` DISARM must be authored or the clone inherits the template's MECHANIC_SLOW_ATTACK and lands in the wrong DR group. **947778** Adrenaline Rush clone (node 20441; TBC 5-min cd vs live 3-min — the TBC twin of the Vanilla clone 932770). **947779-947783** Mace Specialization per-rank proc passives + **947784** its 3s stun payload (authored — stock 5530 lost its stun entirely in WotLK; the TBC twin of the Vanilla 932988 conclusion). **947785** Blade Twisting daze payload (live 31125 is -70%/4s + SUPPRESS_TARGET_PROCS; TBC is -50%/8s). **947786** Preparation clone + **947787** Premeditation clone (node 20457/20463 — both readiness-gated, with the stock ids 14185/14183 stripped by `kEraRogTbcStockSwaps`; `era_rog_preparation_tbc` resets the TBC list). **NO DUMMY-marker misc consumed (next free stays 19.)** Generator gained `proc.affectsMask`, per-effect `mechanic:`, and node-effect `perLevel:` (all purely additive, TDD-covered, byte-neutral for shipped data). Record: `docs/verification/era-talents-tbc-phase5-rogue.md` | claimed |
| 948024-948279 | Priest (TBC Phase 6, class 5 / family 6) — **HEADLESS-COMPLETE at Task 9 (2026-09-04, gen `9ad9274e`); awaiting real-client sign-off. 948024-948080 USED, 57 ids; 948081-948279 free-within-slice.** 64 nodes 20500-20563 all wired (Discipline 22 / Holy 21 / Shadow 21); auto-passives **940000-940511** (131 rows). 28 of 64 nodes grant — **17 stock, 11 a band id**. Breakdown: **948024-948028** Divine Spirit r1-r5 + **948029-948030** Prayer of Spirit r1-r2 (node 20513) — live 14752 has ONE effect (aura 29 Spirit) where TBC has THREE (aura 174 `MOD_SPELL_DAMAGE_OF_STAT_PERCENT` + aura 175 `MOD_SPELL_HEALING_OF_STAT_PERCENT`, both at base 0), and those two hidden slots are exactly what Improved DS 33174/33182 spellmods via `SPELLMOD_EFFECT2/3` — **a stock grant can never carry Improved Divine Spirit**. Clones author word0 `0x20` so `SpellInfo.cpp:2141`'s `SPELL_SPECIFIC_PRIEST_DIVINE_SPIRIT` group exclusivity still applies, and the PoS clones author `attributesEx2 |= 0x4` (the by-id LOS `SpellInfoCorrections` fix a clone never inherits). **948031-948033** Vampiric Embrace trio — castable / hidden proc-passive / heal delivery, FRESH because TBC is **15%** where the shipped Vanilla trio (921152/932950/932951) is 20%; 948032 is a PERMANENT learned passive (a `TRIGGER_SPELL` temp buff did not proc) managed by the `CLASS_PRIEST` TBC reconcile arm (4). **948034-948038** Reflective Shield r1-r5 (node 20519) — the core reads the talent by `GetAuraEffectOfRankedSpell(33201, EFFECT_0)`, which a clone cannot satisfy, so `era_pri_reflective_shield_tbc` is instead bound to the TWELVE **STOCK** PW:Shield ranks (17/592/600/3747/6065/6066/10898/10899/10900/10901/25217/25218) and reads the caster's clone ownership; it uses `GetCaster()`, so a shield on a PARTY MEMBER reflects too. **948039** Power Infusion (cooldown-only divergence) + **948040** Pain Suppression (TBC 2-min cd). **948041-948042** Martyrdom "Focused Casting" payloads (aura 117) + **948043-948045** Focused Will buffs. **948046-948050** Circle of Healing r1-r5 (node 20542) — TBC heals the TARGET's party only with NO cooldown; **`spell_pri_circle_of_healing` is deliberately NOT rebound** (a type-31 target-cap hook against type-37 clones). **948051-948054** Lightwell r1-r4 (node 20539) — TBC's 6-min category cd + flat mana, but Amendment A.3 keeps LIVE's Effect-28 CREATURE summon so the click->heal chain survives (cost = accepted gap 9, the WotLK charge count and cancel rule). **948055** Surge of Light buff (ships with NO ICD — the live 6 s ICD exists only in the `-33150` proc row and no TBC column carries one; accepted gap 5) + **948056** Clearcasting buff (stock 34754 is repurposed live). **948057-948059** Inspiration buffs — TBC is **aura 101 armor**, not the Vanilla phase's deliberate school-1 "armor proxy". **948060-948063** Vampiric Touch r1-r3 + energize carrier (node 20563) — TBC returns MANA TO THE PARTY; new module script `era_pri_vampiric_touch_tbc` bound to all three ranks, and the core `spell_pri_vampiric_touch` is deliberately NOT rebound because its body is WotLK's dispel backlash (its absence is era-correct). **948064-948070** Mind Flay r1-r7 (node 20550) at TBC's aura-3 periodic encoding but keeping **LIVE's word2 `0x40` identity** (Amendment A.8 — TBC 15407's own word0 `0x800000` is a SHARED bit carried by 126 family-6 spells, so a literal copy would be caught by every spellmod aimed at any of them); the four talents whose TBC masks reach Mind Flay through that shared bit are re-pointed onto word2 `0x40` and therefore AUTHORED (20547/20552/20553/20559), while 20557 Focused Mind GRANTS STOCK because LIVE's own mask already reaches the clone. **948071** Silence (`attributesEx7: 0x800` + the `startRecoveryCategory`/`startRecoveryTime` GCD pair) + **948072** Shadowform (TBC's physical-only reduction). **948073-948077** Misery debuffs r1-r5 — 33196-33198 are WotLK's +hit repurposing and 33199/33200 are absent, and all five ship as ONE `spell_ranks` chain rooted at 948073 so `Aura::CanStackWith` -> `SpellInfo::IsRankOf` refuses to stack two priests' different ranks. Misery ships as **aura 109 `SPELL_AURA_ADD_TARGET_TRIGGER`** (Amendment A.7), which `Spell::DoTriggersOnSpellHit` casts on the spell's own target — so it needs NEITHER a `spell_proc` row NOR a rebind of the generic `spell_gen_proc_on_victim` the `_ref` census pre-registered. **948078** Shadow Vulnerability + **948079** the Blackout stun (from scratch, with `defenseType` authored so `Unit::SpellHitResult` can resist it) + **948080** the Spirit Tap buff. **Trainer chains (hand SQL `2026_09_05_02_..._divine_spirit_chain.sql` + `_03_..._holy_chains.sql` + `_04_..._shadow_chains.sql`: 21 `trainer_spell` rows on TrainerId 11 + 7 `spell_ranks` chains (6 trainer-taught + the Misery debuff chain 948073-948077, which exists only so `SpellInfo::IsRankOf` prevents cross-rank stacking)): NOTHING DELETED AND NOTHING RE-GATED** — every stock chain is already self-gating, so the orphaned-higher-rank check had nothing to flag and the WotLK ranks 48073/48074/48088/48089/48159/48160/48077/48078 stay reachable for native priests. **Two `kBaselineSpellGates` rows split three-way** (the 3rd and 4th of the seven): Holy Nova 18121 -> node-gated strip (druid shape; TBC still grants STOCK 15237), Divine Spirit + Prayer of Spirit 18113 -> UNCONDITIONAL strip (warrior Shield Slam shape), plus a respec arm (2b) + a cross-era arm that strip the trainer-taught **Prayer of Spirit** clone chain (948029->948030): the DS half cascades through `spell_ranks` and is seen by `StripOrphanedGrants`, but PoS roots at its own chain and is never node-granted, and the stock `spell_required(27681 -> 14752)` path is dead for a clone because `Player.cpp` walks `spell_required` only when `GetTalentSpellCost(firstRank) > 0` and `DBCStores` returns 0 for any id outside `Talent.dbc` (workflow lesson 21). **Reuse ledger: ALL EIGHT Vanilla priest helper groups FRESH** (921152/932950/932951 user-locked, 920920, 932960, 932970, 920987-989, and the near-miss 920976-978 Blessed Recovery — magnitudes agree, wiring does not); **FOUR STOCK payloads reused as-is**: 33143 Blessed Resilience buff, 27813/27817/27818 Blessed Recovery HoTs, 33619 Reflective Shield damage, 34919 VT energize. **Three module scripts:** `era_pri_vampiric_touch_tbc` (new), `era_pri_reflective_shield_tbc` (new), and `era_vampiric_embrace` made **table-driven** so one script serves the Vanilla trio (bound to 932950) and the TBC trio (bound to 948032); `era_blessed_recovery` gained the three TBC carriers 940224-940226 alongside its Vanilla 920976-920978. **Generator gained four purely-additive keys** (TDD-covered, byte-neutral for every already-shipped helper): `attributesEx7`, `defenseType`, `startRecoveryCategory` and `startRecoveryTime`; `era_audit.py`'s `_SPELLFIX_KEYABLE` gained the matching `AttributesEx7` row. **NO DUMMY-marker misc consumed — next free stays 19**: Improved Vampiric Embrace (20556) re-uses the existing misc **4** that `era_vampiric_embrace` already scans for, and the VT / Reflective Shield scripts key on clone OWNERSHIP. **ZERO core patches.** Record: `docs/verification/era-talents-tbc-phase6-priest.md` | claimed |
| 948280-948409 | Hunter (TBC Phase 7, class 3 / family 9) — **HEADLESS-COMPLETE at Task 9 (2026-09-05, gen `76709510`); real-client SIGNED OFF; final-review round R4 applied 2026-09-06. 948280-948385 USED, 54 ids (948382 consumed at R4); free within slice: 948282-948289, 948304-948329, 948352-948369, 948386-948409.** 64 nodes 20600-20663 all wired (Beast Mastery 21 / Marksmanship 20 / Survival 23); auto-passives **940800-941311** (118 rows). Node dispositions from the shipped dataset: **18 GRANT stock / 35 AUTHOR auto-passive (6 of them full RECONSTRUCTIONs) / 11 CLONE** (Scatter Shot moved GRANT -> CLONE at final-review R4). **Pre-partitioned by tab at spec time** (substrate 948280-948289 / BM 948290-948329 / SV 948330-948369 / MM 948370-948409) so the three tab tasks could not collide — no post-hoc id-correction amendment was needed (workflow lesson 22). Breakdown: **948280** "Era: TBC Hunter" marker (a `client:` block so the trainer UI renders the Requires line) — the THIRD arm of a three-way version-swap chain, Vanilla **932304** / TBC **948280** / WotLK **932305** (shaman three-marker precedent `9b69360`); the readiness conjunct sits on the `else if`, not inside it, so a build with TBC outside `EraHasTalentTrees` falls through to the WotLK arm instead of getting NO marker at all (`d90874f`). **948281** TBC Scorpid Sting clone — trainer-taught (TrainerId 7, L22, 6000c) behind 948280; TBC 3043 is **-5%** hit where live is -3% and `ManaCostPct` 9 vs 11 (user-locked baseline exception, Amendment A.1); its `dispel: 4` is LOAD-BEARING (`SpellInfo::GetSpellSpecific()` -> `SPELL_SPECIFIC_STING`, else two stings stack). **948290** Bestial Wrath (18 s, **`attributesEx5: 0` per TBC** where live and the Vanilla clone 932967 carry 0x60008 by template inheritance) — binds core `spell_hun_bestial_wrath` PLUS the new `era_hun_beast_within_tbc`. **948291/948292** The Beast Within passive + owner buff — **two** clones because live 34692 is not a bare marker (it carries `aura 79 MOD_DAMAGE_PERCENT_DONE +10` itself, so granting stock would give a permanent unconditional +10% damage) and because the core fires the owner buff BY ID at `SpellAuras.cpp:1841-1857` (`GetId()==19574` + `owner->HasAura(34692)`, all three ids stock) — dead on a clone, so the module script replicates BOTH the apply-side cast and the remove-side strip. **948293/948294** Spirit Bond 119 `APPLY_AREA_AURA_PET` carriers r1/r2. **948295-948297** Ferocious Inspiration pet proc carriers + **948298-948300** its party buffs — ONE CARRIER + ONE BUFF PER RANK, because a `mechanic: pet` passive is a bare DUMMY so the per-rank % must live on the buff (lesson 8); a full RECONSTRUCTION (Amendment A.11) since the stock `spell_pet_auras` rows point at LIVE_ONLY WotLK payloads while TBC's own 34457/34456 are WAGO_ONLY, and each carrier gets a generator `spell_proc` row (a clone never inherits one, lesson 18). **948301** Bestial Swiftness owner spellmod with `attributes` including 0x8000 `ONLY_OUTDOORS` — TBC's 19596 is an OWNER-held `SPELLMOD_ALL_EFFECTS +30` onto Tamed Pet Passive 04 where live "Boar's Speed" is a direct pet `aura 31` with the flag dropped, and because the passive is PLAYER-held the core's outdoor gate applies: this **CLOSES Vanilla accepted gap 2 for TBC** (Amendment A.2). **948302/948303** Focused Fire pet-damage carriers (accepted gap 12 — the Kill Command crit half cannot ride the same node). **948330-948333** Wyvern Sting sleep r1-r4 (word1 `0x1000` + `SpellIconID 1721` are load-bearing for `DIMINISHING_DISORIENT`) + **948334-948337** its wake-DoT r1-r4 (50/70/100/157 per tick), chained via `spell_ranks` with NO trainer rows, `targetA: 6` authored (the by-id `ApplySpellFix` a clone never receives). **948338-948341** Counterattack r1-r4 — **the first genuine reuse identity hit in any TBC class phase** (Vanilla 932979/932958/932959 are field-identical to 19306/20909/20910 on ALL FOURTEEN families) and still decided **FRESH** (Amendment A.10): TBC needs a 4th rank with no Vanilla counterpart, the strip tables are era-keyed, and a mixed-era chain would be the only one of its kind. **948342** Deterrence — **a TBC TALENT** (node 20650), so the stock 19263 trainer row stays gated on the WotLK marker 932305 and the TBC arm strips 19263. **948343** Entrapment root (4 s TBC vs 5 s Vanilla; templates off 19185 to inherit `SpellVisualID 7484` + `SpellIconID 20` for `DIMINISHING_ENTRAPMENT`) + **948344** Improved Wing Clip root. **948345** Expose Weakness payload — basePoints **0** plus the new `era_hun_expose_weakness_tbc` supplying 25% of the CASTER's Agility in `DoEffectCalcAmount`, because auras 127/165 take a FLAT amount on this core and the DBC literally says 25 (accepted gap 9; zero rather than a wrong +25 if the script never runs). **948346-948350** Master Tactician crit buffs — tooltip/data fidelity, NOT a behaviour fix (`MOD_WEAPON_CRIT_PERCENT` is summed by bare `GetTotalAuraModifier` calls and never mask-filtered); what IS load-bearing is the Aimed Shot bit on the TALENT's proc mask (Amendment A.17). **948351** Readiness — **clone + the new `era_hun_readiness_tbc`**: the core's `spell_hun_readiness` is a FAMILY WALK with a three-id exclusion list, and `Spell::SendSpellCooldown()` runs at `Spell.cpp:3933` BEFORE the effect handlers, so a clone rebound to it would clear its OWN 5-minute cooldown and be infinitely spammable; the module script replicates the walk with 948351 excluded and **deliberately does NOT exclude the era BW clone 948290** (TBC's tooltip has no Bestial Wrath exception — WotLK added it; Amendment A.17 correction 1 reversed A.6). **948370-948376** Aimed Shot r1-r7 — the identity decision: `SpellInfoCorrections.cpp:5311-5318` forces `NO_CATEGORY_COOLDOWN_MODS` + a 10 s `RecoveryTime` onto every family-9 spell carrying word0 `0x20000` (a PATTERN sweep, not an id list), so the clones ship **`family: 9` + `maskC: 2097152` (word2 bit 21)** with 0x20000 ABSENT — the free-bit census over 1151 family-9 records found word0 SATURATED, exactly ONE free word1 bit (6, consumed by TBC's own FI party buff) and bits 21-30 free in word2. The companion arm `SpellMgr.cpp:3492-3503` drives `FORCE_SEND_CATEGORY_COOLDOWNS` off the same bit, so the 6 s cooldown must live on `RecoveryTime`, never `CategoryRecoveryTime`. Talents authored so their masks reach the bit: Efficiency 20624, Mortal Shots 20630 (TBC's set WITHOUT 0x4000 Serpent Sting — the core ORs it into stock by id), **Hawk Eye 20643** (three auto-passives — this CLOSES accepted gap 5), the Rapid Killing buffs and the Master Tactician buffs. **948377-948380** Trueshot Aura r1-r4 — `APPLY_AREA_AURA_PARTY` flat +50/75/100/125 RAP with **NO mana cost** (TBC has no SpellPower row at all), r2-r4 WAGO_ONLY live. **948381** Improved Concussive Shot stun. **948382** Scatter Shot clone — MINTED AT R4 (2026-09-06) into what had been a deliberate hole (the reserved Concussive Barrage daze slot; the `-35100` census proved stock 35101 triggerable as-is, so nothing was minted there and the Task-5-pinned 948383 kept its id). Node 20632 moved GRANT -> CLONE for `AuraInterruptFlags` TBC 0x2 vs live 0x480002 (the 2 pp mana cost was never the reason), templating 19503 so word0 0x40000 + SpellIconID 132 keep it in `DIMINISHING_SCATTER_SHOT`; a `kEraHunTbcStockSwaps` row `{20632, 19503}` retires the stock spell. This CLOSES the Scatter Shot half of accepted gap 11 (the Intimidation half stays open). The generator gained one purely-additive key for it: **`auraInterruptFlags`** (TDD-covered, byte-neutral for shipped data). **948383** Silencing Shot (the divergence is the GCD; `speed` 60.0 acknowledged, not authored). **948384/948385** Rapid Killing buffs — and **the TALENT 20628 is AUTHORED, not granted** (Amendment A.18): a granted stock talent names its payload buffs BY ID and can never reach a clone, so "grant the talent, clone the buffs" was self-contradictory (workflow lesson 23). **Trainer chains (hand SQL `2026_09_06_02_..._scorpid_sting.sql` + `_03_..._survival_chains.sql` + `_04_..._marksman_chains.sql`: 16 `trainer_spell` rows on TrainerId 7 + 5 `spell_ranks` chains): NOTHING DELETED AND NOTHING RE-GATED** — every stock chain self-gates on an r1 a TBC hunter never holds, so WotLK 49049/49050, 48998/48999 and 49011/49012 stay reachable for native hunters. Costs are the authentic stock curve: Aimed Shot r2-7 L28/36/44/52/60/70 = 400/700/1300/2000/2500/10000; Counterattack r2-4 L42/54/66 = 1200/2100/2500; Wyvern Sting r2-4 L50/60/70 = 1800/2500/5000; Trueshot r2-4 L50/60/70 = 1800/2500/**5000** (a deliberate curve CHOICE — accepted gap 10). **No `kBaselineSpellGates` hunter row** — no TBC hunter talent became WotLK baseline. **`petBuffBase: 944000` is CLAIMED but UNUSED** — every pet node needed a hand `petBuffIds` carrier, so the generator emitted no auto pet-buff row and `[944000, 946000)` is empty in the live DB. **Three NEW module scripts** (`era_hun_beast_within_tbc`, `era_hun_expose_weakness_tbc`, `era_hun_readiness_tbc`) plus **`era_wyvern_sting` made TABLE-DRIVEN** so one script serves both eras ({932965->932966} Vanilla, {948330->948334, 948331->948335, 948332->948336, 948333->948337} TBC; an unlisted sleep id casts NOTHING, fail-closed; `Validate` runs PER BINDING so the Vanilla binding survives when the TBC DoTs are absent). **Generator gained ONE purely-additive key: `attributesEx4`** (TDD-covered, byte-neutral for shipped data) so the Aimed Shot chain can carry `FORCE_DISPLAY_CASTBAR` — deliberately NOT added to `era_audit.py`'s `_SPELLFIX_KEYABLE` set, which would newly fail the two shipped Bestial Wrath clones. **NO DUMMY-marker misc consumed — next free stays 19**: spec §4e's foreseen Improved Aspect of the Hawk "while Hawk" gate turned out not to exist (the Quick Shots proc lives on Aspect of the Hawk 13165's own e2 + its `-13165` proc row), so node 20600 GRANTS stock. **13 accepted gaps** (spec A.15 items 1-11 + A.16 item 12, plus **id 13** added at R4 — Entrapment ships the CORE-FIXED victim-centred AoE root rather than TBC's era-literal single-target root, deliberate parity with stock in every era, `closes_if: never`), of which **2 and 5 are CLOSED for TBC** and **11 is half-closed** (Scatter Shot closed at R4, Intimidation open). **ZERO core patches.** **Bundled Vanilla fixes:** 932966 `targetA: 6`; the Aimed Shot identity back-port (932952-957 to family 9 + word2 bit 21, ids unchanged so no `character_spell` migration, and Efficiency 18318's mask gains the bit) which **RETIRES Vanilla record gap 4**; and Trueshot 932962/932963 re-priced to 1800/2500. **R4 (2026-09-06) bundled four more Vanilla fixes:** Entrapment 932972 authored to the CORE-FIXED victim-centred AoE (targetA 53 / targetB 16 / radius 13 + `attributesEx5: 67108864`) and Improved Wing Clip 932978 to a plain single-target root (targetA 6 / targetB 0 / radius 0) — both had silently inherited 19185's raw 22/15/13, a 10 yd area root centred on the HUNTER (the same latent defect was fixed on TBC's 948343 in the same round); Ranged Weapon Specialization 922632-922636 gained stock 19507's ranged equip gate (`equipClass: 2` / `equipSubclass: 262156`) — without it `school: 127` was +1..5% to ALL damage; and Bestial Wrath 932967 gained both lost by-id corrections (`attributesEx2: 67108864`, `attributesEx4: 4`), as did TBC's 948290. Plus hand SQL `2026_09_06_02_era_talent_final_review_hunter.sql` putting TBC Aimed Shot 948370 in `spell_group` 1061 (mortal wounds) and the Scorpid Sting clones 932300-932303 / 948281 in 1060. Record: `docs/verification/era-talents-tbc-phase7-hunter.md` | claimed |
| 948410-948749 | Warlock (TBC Phase 8, class 9 / family 5 — the inversion trap with hunter) — **HEADLESS-COMPLETE at Task 6 (2026-09-05, gen `4ec433f1`); awaiting real-client sign-off. 65 ids USED.** 64 nodes 20700-20763 all wired (Affliction 21 / Demonology 22 / Destruction 21); auto-passives **941600-942100** (112 rows). Dispositions **24 GRANT / 33 AUTHOR / 6 CLONE / 1 RECONSTRUCT**; **zero core patches** (patch 0019 is REUSED unchanged — TBC Corruption is a 2 s cast at every rank). **EXACT USED IDS** — substrate: **948410** "Era: TBC Warlock" marker (trainer gate for the TBC stones), **948420-948428** the nine Create clones (Firestone Lesser/-/Greater/Major/**Master**, Spellstone -/Greater/Major/**Master**), **948430/948431** Master Firestone equip + melee proc, **948432/948433** Master Spellstone use + equip (+20 spell crit RATING). Affliction: **948450** Amplify Curse active, **948451-948456** Siphon Life r1-r6 (RECONSTRUCT — 18265 and all five higher ranks are WAGO_ONLY, i.e. the spell does not exist on 3.3.5a at all), **948457/948458** Improved Drain Soul threat/mana halves, **948460-948464** Shadow Embrace debuffs, **948470-948472** Unstable Affliction clone chain. Demonology: **948550/948551** the Soul Link visible buff + pet-cast split carrier (effect 143 radiates only to the OWNER, so the +5% is split across the pair — the shared `(buff, split)` table is `modules/mod-era-talents/src/EraSoulLink.h`), **948552-948557** Demonic Sacrifice clone + five per-demon buffs incl. the Felguard arm, **948560-948564** Master Demonologist 119 carriers incl. Felguard (entry 17252), **948570** Fel Domination clone (TBC 15-min cooldown; **a clone inherits NO `spell_proc` row**, so it carries its own), **948571-948573** Mana Feed helpers (templated on stock 30326 so they inherit **SpellIconID 1982**, which the core's Life Tap script keys on together with `aura 107 / op 12` at **effect index 0**). Destruction: **948650-948655** Conflagrate clone chain (template 17962 with e2/e3 left ZERO so the %-of-Immolate bonus computes 0 and the pure-DBC `TargetAuraState 14` Immolate consume survives with no script), **948660-948664** Improved Shadow Bolt debuffs (`procCharges: 4` + their own consume row; the five TBC debuff ids are WotLK's Shadow Mastery ranks live — a hard block on granting them), **948665** Aftermath daze, **948666** Pyroclasm stun, **948667** Nether Protection immunity, **948670-948672** Shadowfury clone chain. **HOLES (free within slice): 948411-948419, 948429, 948434-948449, 948459, 948465-948469, 948473-948549, 948558/948559, 948565-948569, 948574-948649, 948656-948659, 948668/948669, 948673-948749.** Four TBC trained CLONE chains on TrainerId 31 (Siphon Life 948452-456, Conflagrate 948651-655, UA 948471/472, Shadowfury 948671/672) chained off each talent-granted r1; **Shadowburn (17877) and Dark Pact (18220) are GRANTs** — their stock chains self-gate and **nothing is deleted**. **Four-marker chain** (see the 932900+ registry): 932930 cast-time (no longer a trainer gate), **932994** Vanilla stone gate, **948410** TBC stone gate, 932939 narrowed to WotLK. Scripts: **`era_spellstone_crit_rating` NEW** (EFFECT_0 `MOD_RATING` x marker 13 — `era_spellstone_absorb` hooks EFFECT_1 `SCHOOL_ABSORB` and could not scale 948433); `era_amplify_curse`, `era_improved_drain_soul`, `era_demonic_sacrifice`, `era_soul_link` / `era_soul_link_relink` and `era_master_demonologist` all became **era-table-driven** (they read `EraTalentBots::EraFor` + the node rank, never "is a bot"). **Two reconstructed items:** Master Firestone 22128 / Master Spellstone 22646, both `RequiredLevel 66`. **`petBuffBase: 945000` claimed, emits NOTHING** (owner spellmods — see the [944000, 946000) row). **No DUMMY-marker misc consumed** (next free stays 19). **`familyName: 5` guard:** the `_ref` shipped without it and the generator/audit default that key to **3 (mage)**, which would have given every warlock spellmod `SpellClassSet 3` and bound nothing — standing check for every future class `_ref`. **Bundled Vanilla fixes:** ISB `procCharges: 4` (932940-944), `manaCostPct: 0` on Conflagrate 932780-783 + Soul Link 932900 (their templates carry `ManaCostPct 16` and `Spell::CalculatePowerCost` ADDS it to the flat cost), and the historic orphan strip 921745/921754/921788. Record: `docs/verification/era-talents-tbc-phase8-warlock.md`. | claimed |
| 948750-949059 | Mage (TBC Phase 9, class 8 / family 3) — **47 ids USED; exact list recorded at Task 6 (2026-09-05).** **Substrate 948750-948759 is EMPTY by design** — the mage tree mints NO identity marker (every trained chain gates on its own r1 clone id, and `era_ward_reflect` is band-gated rather than era-gated). **Arcane 948760-948773 (14, no holes):** 948760 Arcane Power clone, 948761 Presence of Mind clone (neither authors the live `ExcludeCasterAuraSpell` cross-lock — TBC allowed AP+PoM together), 948762-948766 Magic Absorption r1-r5 (templated off 29441, per-rank POSITIVE `spell_mage_magic_absorption` bindings, own `spell_proc` row `typeMask 7 / hitMask 8 FULL_RESIST / cooldown 1000`), 948767-948769 Arcane Potency crit buffs 10/20/30 (`attrMask 0`, NOT the census row's 12 — see the field-semantics note on `PROC_ATTR_REQ_SPELLMOD`), 948770 Improved Counterspell silence, 948771-948772 Improved Blink buffs, 948773 Slow clone. **Fire 948860 + 948870-948887 + 948890-948893 (23):** 948860 Fire Vulnerability debuff, 948870-948879 Pyroblast r1-r10 (`castTimeIndex 171` = 6 s), 948880-948886 Blast Wave r1-r7 (effect 3 KNOCK_BACK zero-filled; live word-1 identity `flagsB 64` kept), 948887 Combustion (`RecoveryTime 180000`, bare DUMMY effect 1, `dispelType 0`), 948890-948893 Dragon's Breath r1-r4 (`durationIndex 27` = 3 s). **Frost 948960-948969 (10, no holes):** 948960 Winter's Chill debuff (`singleAuraStack: true` — `SpellMgr.cpp:3752` grants it to stock 12579 BY ID and a clone does not inherit it), 948961-948963 Blizzard chill r1-r3 (-30/-50/-65% on family-3 word-2 pseudo-bit 6), 948964-948969 Ice Barrier r1-r6 (+ `era_mag_ice_barrier_tbc`, TBC's 0.1 SP coefficient vs live's hardcoded 0.8068). **INTENTIONAL holes 948861-948869 and 948888-948889** — the Fire partition is laid out on decade boundaries (86x debuffs, 887x Pyroblast, 888x Blast Wave, 889x Dragon's Breath) so a reader can tell at a glance which chain an id belongs to; a deliberate readability exception to allocate-in-order (lesson 22). **Do not "fix" them.** **Free within slice:** 948750-948759, 948774-948859, 948861-948869, 948888-948889, 948894-948959, 948970-949059. Record: `docs/verification/era-talents-tbc-phase9-mage.md` §8. | claimed |
| 949060-949999 | *(unclaimed — Phase 9.5 / later fixes claim from here)* | free |

**Per-class TBC node-id slices** (importer defaults, `tools/import_tbc_talents.py` `ID_BASE`,
each 100 wide — a class's three tabs share one 100-wide slice, ids assigned within it by the
importer):

| Class | Base | Range |
|---|---|---|
| Paladin | 20000 | [20000, 20100) |
| Druid | 20100 | [20100, 20200) |
| Shaman | 20200 | [20200, 20300) |
| Warrior | 20300 | [20300, 20400) |
| Rogue | 20400 | [20400, 20500) |
| Priest | 20500 | [20500, 20600) |
| Hunter | 20600 | [20600, 20700) |
| Warlock | 20700 | [20700, 20800) |
| Mage | 20800 | [20800, 20900) |

20900-20999 spare (no class assigned — headroom for a tenth class slice or overflow, not
expected to be needed for the nine playable TBC classes).

**Node icon basenames — the `classic_` trap (guarded).** The Wowhead Classic-family tooltip
endpoints that `tools/import_tbc_talents.py` / `tools/import_daribon_talents.py` read serve
**WoW-Classic-2019 texture names** for spells whose art was re-cut, prefixed `classic_`. No such
texture exists in a 3.3.5a client, so the talent button renders **blank** — no error, no log line,
invisible headlessly. It has bitten twice (Vanilla shaman 18215/18240, hand-fixed; TBC druid 20122,
found by a real-client pass on 2026-08-31). The un-prefixed basename is the 3.3.5a one. Now guarded
on both ends: the importer strips a leading `classic_` (`_icon_name`, TDD-covered), and
`tools/era_audit.py` check 2b FAILS on any node icon carrying the prefix. It is a prefix check, not
an existence check — a real existence check would need `SpellIcon.dbc` plus the client MPQ texture
listing, neither of which is in this repo.

**Tab background textures.** TBC tab backgrounds come from the committed `TalentTab` CSV's
`BackgroundFile` column (`era-data/_ref/tbc/TalentTab-2.5.4.44833.csv`), taken verbatim as the
addon background token. Each token MUST be verified against a real 3.3.5a client
`Interface\TalentFrame\<token>` texture during that class's real-client pass — a miss renders
blank parchment silently (no error, no log line), so this can't be caught headless.

**FOLLOW-UP — DONE (TBC Phase 7, 2026-09-04):** the generator's `ERA_PET_WINDOW` reject/accept
path (era-1 `petBuffBase` default 944000, range-validated against `[944000,946000)`) now HAS a
direct unit test — `test_gen_era_talents.py::test_era1_pet_buff_base_emits_inside_tbc_pet_window`
(a fixture era-1 dataset with a pet node derives ids inside the window) and
`::test_era1_pet_buff_base_below_window_rejected` (943000 is rejected with the window message),
landed with the first TBC pet class. The pre-existing `petBuffBase` tests still cover era-0's
`PET_BUFF_BASE`/collision logic. Note the guard is what matters here rather than the emission:
the hunter dataset CLAIMS 944000 but emits nothing into it (every pet node needed a hand
`petBuffIds` carrier), so the test's fixture — not the shipped dataset — is what exercises the
accept path.

**Hand-assigned helper id registry (932900+).** Consult AND update this before claiming any
helper id — the YAML greps only catch ids already authored, not plan-reserved ones (Ferocity's
932973-977 were claimed collision-free but unrecorded until this table existed):

| Range | Owner | Status |
|---|---|---|
| 932300-932305 | Hunter Scorpid Sting reconstruction (2026-08-24) — **932300-303** Vanilla Scorpid Sting Str/Agi clones r1-4 (template 3043; E1/E2 MOD_STAT Str/Agi -21/-30/-46/-69 @ L22/32/42/52 mana 70/90/125/165 from the 1.12.1 client Spell.dbc; E3 MOD_STAT Stamina base 0, set to -pct% of the Str by `era_hunter_scorpid_sting` AuraScript where pct = the Improved Scorpid Sting DUMMY marker misc 16). Baseline VERSION-SWAP: trainer-taught (trainer.Id 7) behind marker **932304** "Vanilla Hunter"; stock 3043 (WotLK -hit-chance rework) re-gated behind marker **932305** "WotLK Hunter" (2026_08_24_12). Both markers carry a client: block (trainer-UI requirement name). spell_ranks 932300→303. This is a NEW hunter block (932952-979 was full); 932306-932399 FREE | claimed |
| 932310-932339 | Mage Vanilla clone chains (TBC Phase 9 re-audit, 2026-09-05; 932306-932309 left as hunter headroom). **932310-932317** Pyroblast r1-r8 Vanilla clones (6 s cast — live 11366-18809 are WotLK's 5 s; node 18024 grants 932310, r2-r8 trainer-taught behind it, stock chain swapped out in Vanilla), **932320-932324** Blast Wave r1-r5 (no WotLK knockback effect 3; node 18030), **932325** Combustion (3-min cooldown, bare DUMMY effect 1, `spell_mage_combustion` rebind; node 18032), **932326-932330** Magic Absorption r1-r5 Vanilla clones (the mana-on-full-resist DUMMY rider + helper-level proc row + `spell_mage_magic_absorption` rebind; node 18005 — the rider was never wired before this). **932331-932334** Ice Barrier r1-r4 Vanilla clones (base-only absorb — 1.12 had no spell-power scaling on absorbs; live's `spell_mage_ice_barrier` adds 80.68% SP to the stock chain; node 18049 grants 932331, r2-r4 trainer-taught; NO `scriptBindings` — a clone outside the `-11426` chain is never touched by that script, which IS the 1.12 behaviour). **932335** Cold Snap Vanilla clone (10-min cooldown — live 11958 is 8 min; bare DUMMY effect 1, module SpellScript `era_mag_cold_snap` bound by positive id because core `spell_mage_cold_snap` excludes only 11958 from its reset loop and would clear the clone's OWN cooldown; node 18041, added by the mage final review 2026-09-06). **SHIPPED and DB-verified 2026-09-05 (gen `1de3388d`)**: 27 ids used (932310-932317, 932320-932335); **free within slice: 932318-932319, 932336-932339**. Ten of the 37 custom TrainerId 16 rows and four of the nine `spell_ranks` chains belong to this slice. Record: `docs/verification/era-talents-tbc-phase9-mage.md` §4/§8/§10. | claimed |
| 932900-932949 | Warlock (all shipped helpers; internal gaps 911/912/915/916/939 are FREE only if this table says so — treat the whole range as warlock's) | claimed |
| 932994 | **Warlock "Era: Vanilla Warlock" marker** (TBC Phase 8, 2026-09-05) — server name `Era: Vanilla Warlock`, CLIENT label `Vanilla Warlock`. It took over the **trainer-gate** job that `932930` used to do: it is the `ReqAbility1` on the seven Vanilla Create-stone rows 932922-932928 (re-gated in place by `2026_09_07_02_era_talent_tbc_warlock_markers_stones.sql`; **nothing deleted**). Needed because TBC now also wants a stone gate and `932930` belongs to Vanilla AND TBC, so it can no longer separate them. **NB this id sits inside the block the rogue row below reserves (932992-932998) — it is claimed by WARLOCK; rogue must not take it.** | claimed |
| 932930 | Warlock era cast-time marker (DUMMY misc 5, arms core patch 0019). **No longer a trainer gate** as of TBC Phase 8. Server name deliberately left `Era: Vanilla Cast Times`; the CLIENT row was renamed to **"Pre-Wrath Warlock"** because a TBC warlock carries it too (TBC Corruption is a 2 s cast at every rank; WotLK 3.0 made it instant). Wanted for `era == ERA_VANILLA || (era == ERA_TBC && EraReplacementSpellReady(ERA_TBC, 948410))` — **the readiness fallback is on EVERY arm** (lesson 25). | claimed |
| 932939 | Warlock "Era: WotLK Warlock" marker — gate for the stock WotLK-item Create rows. **NARROWED** in TBC Phase 8 from `era != ERA_VANILLA` to WotLK-only *plus* the not-ready TBC fallback, so a TBC warlock with its own stone set never sees the WotLK enchant stones. (Listed explicitly because the 932900-932949 row above calls gap 939 "free" — it is NOT.) | claimed |
| 932950, 932951, 932960, 932970 | Priest (VE carriers, PI, Blackout stun) | claimed |
| 932952-932957 | Hunter Aimed Shot R1-R6 (Task 8) — R1 talent-granted clone (3s cast/6s cd, family+Category zeroed to escape the SpellInfoCorrections Aimed-Shot cooldown sweep), R2-R6 trainer-chained | claimed |
| 932958, 932959 | Hunter Counterattack R2/R3 trainer clones (Vanilla 70/110 dmg; WotLK 20909/20910 carry 84/132) — chain off talent-granted R1 932979 (Task 8) | claimed |
| 932961-932963 | Hunter Trueshot Aura R1-R3 (Task 8) — R1 talent-granted flat +50/75/100 RAP+melee-AP party clone (WotLK 19506 is +10% AP), R2-R3 trainer-chained | claimed |
| 932964 | Hunter Deterrence clone (Vanilla 25% dodge/parry, 10s, 5 min) | claimed |
| 932965, 932966 | Hunter Wyvern Sting sleep (12s) / DoT (300 over 12s) clones | claimed |
| 932967 | Hunter Bestial Wrath 18s/12% clone | claimed |
| 932968, 932969 | Hunter Spirit Bond 119 carriers R1/R2 | claimed |
| 932971 | Hunter Improved Concussive Shot 3s stun clone (Task 8 — reassigned from the never-used Quick Shots reservation; Vanilla stun id 19410 absent from 3.3.5a DBC) | claimed |
| 932972 | Hunter Entrapment 5s root clone (WotLK 19185 is 2s) | claimed |
| 932973-932977 | Hunter Ferocity dual-crit pet carriers R1-R5 | claimed |
| 932978 | Hunter Improved Wing Clip 5s root clone (Classic 19229 reconstruction; no 3.3.5a id exists) | claimed |
| 932979 | Hunter Counterattack Vanilla clone (40 dmg vs WotLK 19306's 48; Task 8 chains trainer R2/R3 off it) | claimed |
| 932980-932982, 932990, 932991 | Mage (Blizzard Chill ranks, ward helpers) | claimed |
| 932760-932769 | Mage grant-accuracy slice (mage's original 932980-991 allocation is nearly full). **932760** = Arcane Power Vanilla clone of 12042 (WotLK +20%/+20%/2-min-cd retune → Vanilla +30%/+30%/3-min cd; three aura-108 spellmods damage/cost/dot, per-effect masks A/B/C inherited from the template, category-neutralized like Last Stand 932840, SkillLine 237 Arcane, stale-12042 strip in EraTalents.cpp CLASS_MAGE block). **932762-932764 USED by the TBC Phase 9 Vanilla re-audit (2026-09-05):** 932762 Improved Counterspell silence clone (`aura 27` SILENCE, `durationIndex 35` = 4 s — node 18011's fix from 2 s; per-era identity, the TBC twin is 948770), 932763 Fire Vulnerability clone (node 18026: `aura 87 misc 4 val 3`, `durIdx 9`, `stackAmount 5`; no `spell_proc` row of its own — that is the TALENT's contract), 932764 Winter's Chill clone (node 18048: `aura 179 misc 16 val 2`, `durIdx 8`, `stackAmount 5`, `singleAuraStack: true`). **932765-932769 free.** Cold Snap 11958 + Combustion 11129 were evaluated in the same pass and LEFT AS STOCK grants at the time — both are script-bound (spell_mage_cold_snap resets frost cds; spell_mage_combustion stacks crit) so a clone would lose the behavior, and the cd divergences are minor (Cold Snap 8→10 min, Combustion 2→3 min), not worth a disproportionate era SpellScript. **(BOTH were subsequently CLONED: Combustion at 932325 in the Phase 9 re-audit — the 3-min/bare-DUMMY divergence turned out to be expressible with a `spell_mage_combustion` rebind — and Cold Snap at 932335 in the 2026-09-06 mage final review, with the module script `era_mag_cold_snap` because a core rebind would have let the clone reset itself. See the 932310-932339 row.)** | claimed |
| 932983-932989, 932992-932998 | Rogue (932983 = Preparation Vanilla clone of 14185, bound to era_rog_preparation; 932985-987 = Hemorrhage R1-R3; **932988 = the Mace-Specialization 3s stun payload** — authored from scratch, node 18428's proc trigger, because live 5530 "Mace Stun Effect" was repurposed into a pure +proficiency spell with no stun at all; **932989 = Riposte Vanilla clone of 14251** (claimed 2026-09-03 by the Vanilla rogue fix round: live 14251 is WotLK's slow-attack + combo-point rework, Vanilla/TBC is a 6s DISARM — node 18423 grants the clone and the stock grant is stripped); **932984 = Premeditation Vanilla clone of 14183** (claimed 2026-09-06 by the final-review fix round: `durationIndex` 1 = Vanilla's 10s combo-retain window vs live's 20s — node 18451 grants the clone, the stock grant is stripped, and it mirrors the TBC clone 947787 field for field); 932992-932998 reserved for the class) | claimed |
| 932770-932779 | Rogue grant-accuracy slice (rogue's 932983-998 allocation is nearly full). **932770** = Adrenaline Rush Vanilla clone of 13750 (node 18434) — WotLK 13750 shortened the cd to 3 min (RecoveryTime 180000); the +100% energy-regen aura (Effect_1 aura 110 MOD_POWER_REGEN_PERCENT, misc 3 POWER_ENERGY, DurationIndex 8=15s) is unchanged, so a pure data clone overriding ONLY cooldown:300000 = Vanilla 5-min cd is faithful (13750 is NOT script-bound; Category/CategoryRecoveryTime already 0 so no category neutralization needed, unlike Arcane Power 932760 / Last Stand 932840). SkillLine 38 (Combat), stale-13750 strip in EraTalents.cpp CLASS_ROGUE block. 932771-932779 free | claimed |
| 932780-932789 | Warlock grant-accuracy slice (warlock's 932900-949 allocation is nearly full). **932780-932783** = Conflagrate Vanilla 1.12 clone chain of 17962 (node 18250) — WotLK 17962 is the 3.0 rework (instant cast, damage = % of the target's Immolate, adds a periodic Fire DoT, consumes Immolate only absent Glyph 56235). Each rank clones 17962: castTimeIndex 1 (instant — 1.12; patch 1.7.0 changed Conflagrate from 1.5s to instant), Effect_1 SCHOOL_DAMAGE flat Fire, **Effect_2/3 zeroed** to drop the DoT. 1.12 per-rank level/mana/damage from classicdb.ch (stock rank ids 17962/18930/18931/18932, the last three absent from the 3.3.5a DBC): **932780** R1 L40 165m 240-307 (239+d68), **932781** R2 L48 200m 316-397 (315+d82), **932782** R3 L54 230m 383-480 (382+d98), **932783** R4 L60 255m 447-558 (446+d112). The consume + the "target must carry your Immolate" requirement are PURE DBC — no SpellScript: the clone inherits stock 17962's `TargetAuraState = 14` (AURA_STATE_CONFLAGRATE), and the core's generic SPELLFAMILY_WARLOCK SCHOOL_DAMAGE handler (SpellEffects.cpp:387) gates the cast on that state, adds a %-of-Immolate bonus from Effect_2/3 base values (→ 0 once zeroed, so flat damage stands + no DoT), and removes the caster's Immolate (line 427). Each rank inherits Category 672/CategoryRecoveryTime 10000 (10s cd), RangeIndex 4 (30yd), Fire, family 5 + flagB 8388608 (identity, so `affects: [Conflagrate]` fire-dmg spellmods still boost it), icon 12; SkillLine 593 (Destruction) + `client.rank`. Node 18250 grants R1 (932780); R2-R4 are trainer-taught (warlock trainer.Id 31) behind a ReqAbility1 chain off 932780 + a spell_ranks chain 932780->781->782->783 (SQL 2026_08_14_15_era_talent_warlock_conflagrate_trainer.sql), with a kWarlockTrainedRanks strip mirror + an era-gated stale-17962 strip in EraTalents.cpp CLASS_WARLOCK block (17962 is a legit WotLK talent). 932784-932789 free | claimed |
| 932400-932499 | Shaman (Phase 10, class 7, spell family 11 — the mirror-inversion of Druid's class 11 / family 7; do not confuse the two when cross-checking family-mask sweeps). Reserved by Task 1. ENHANCEMENT tab (Task 6) claims **932400-404** Flurry Vanilla haste-buff clones r1-5 (template 16257, family 0, aura138 MOD_MELEE_HASTE 10/15/20/25/30% + inherited ProcCharges 3 / ProcTypeMask 4 consume-on-swing; node 18824 procs them crit-only), **932405** Stormstrike Vanilla clone (template 17364, overrides RecoveryTime 20000 + flat ManaCost 319 + ManaCostPct 0; the aura-271 nature scope A=1049603/A_2=8192 + weapon-strike triggers 32175/32176 INHERIT because this helper authors no per-effect maskA/B/C — accepted gap: WotLK both-weapon-strike vs Vanilla single extra-attack), **932406** Two-Handed Axes and Maces proficiency (template 197, SPELL_EFFECT_PROFICIENCY 60 + equipClass 2 + equipSubclass 34 = axe2\|mace2). Other Enhancement nodes are stats (Ancestral Knowledge aura132 max-mana%, Thundering Strikes aura52 weapon-crit, Anticipation aura49 DODGE — not defense-skill, Toughness aura142 armor, Weapon Mastery aura79 physical-dmg%), a `multi` stat (Shield Spec aura51 block% + aura150 block-value%), spellmods (Improved Ghost Wolf op:castTime, Improved Lightning Shield op:damage on bit10 mirroring stock Improved Shields 16261, Enhancing Totems op:allEffects on SoE effect 8076 bit16, and `multi` spellmods Guardian Totems allEffects-on-Stoneskin-pulse-8072-bit15 + cooldown-on-Grounding-affectsMask{a:262144}, Improved Weapon Totems WF-pulse-8515-bit21 + FT-pulse-52109-bit25, Elemental Weapons Windfury-Attack-25504-bit23 + FT/FB-Attack-bit21/24), or grants (Parry stock 3127 = enables the parry mechanic shaman lacks baseline; Two-Handed/Stormstrike grant the clones). **ACCEPTED GAPS (Task-9 ledger):** Guardian Totems' Windwall Totem half + Enhancing Totems' Grace of Air half (both ABSENT from 3.3.5a — bind surviving totem only) + Stormstrike strike-count + **Elemental Weapons' Rockbiter half** (code-verified INERT — see below). Generator gained helper `equipClass`/`equipSubclass` `_HELPER_SCALARS` for 932406. NO DUMMY-marker misc consumed (next free stays 16). New _ref.spells entries added (Stoneskin Totem Effect 8072, Grounding Totem 8177, Strength of Earth 8076, Windfury Totem Buff 8515, Flametongue Totem Buff 52109, Windfury Attack 25504, Flametongue Attack 10444, Frostbrand Attack 8034). **932400-932406 USED by Enhancement.** **RESTORATION tab (Task 7) claims 932407-932412:** **932407** Mana Tide Totem Vanilla clone (template 16190 — summons the STOCK totem NPC 10467, whose creature_template_spell casts the WotLK %-max-mana pulse 16191->39610->spell_sha_mana_tide_totem; the clone's id!=16190 DODGES the hardcoded 10%-caster-HP override in SpellEffects.cpp:2433 so the totem gets the Vanilla 5 hp; effects: must author the SUMMON incl. `miscB:82` = the SummonProperties id or the templated-effects loop 0-fills it and breaks the summon). ACCEPTED BENEFICIAL DIVERGENCE: the Vanilla FLAT 170-mana/3s pulse is NOT built — the totem->pulse link is data (creature_template_spell) NOT hardcoded, but the %-mana FORMULA lives in a C++ SpellScript bound to 39610, so a flat rebuild needs a CUSTOM TOTEM CREATURE (new creature_template + creature_template_spell -> custom periodic aura -> custom flat SPELL_EFFECT_ENERGIZE area-party pulse), outside the era-talents generator + headless-untestable — FLAGGED as a per-case scope decision for the user, none built. **932408-410** Ancestral Healing armor buffs r1-3 (from-scratch aura 142 MOD_BASE_RESISTANCE_PCT misc 1 = +8/16/25% of the HEAL TARGET's item armor, 15s, targetA 21 — the LoH-rider precedent; NOT stock 16177's WotLK aura-87 dmg-taken rework). **932411** Healing Way stacking buff (template 29206 SOLELY to inherit its Effect_1 EffectSpellClassMaskA_1=64 = Healing Wave scope — this helper inherits the mask rather than authoring maskA/B/C; override attributes:0 to un-hide the passive, aura 283 MOD_HEALING_RECEIVED +6% caster-specific + IsAffectedOnSpell-scoped, `stackAmount:3` -> CumulativeAura 3, targetA 21, 15s). **932412** Nature's Swiftness Vanilla clone (template 16188 inheriting the nature-set effect mask 2499, category/categoryCooldown NEUTRALIZED to 0 + RecoveryTime 180000 = Vanilla 3-min cd vs stock 2-min). Other Restoration nodes are spellmods (Improved Healing Wave op:castTime, Tidal Focus op:cost, Improved Reincarnation op:cooldown — the +HP/mana-return half is an ACCEPTED GAP, return lives on 21169 SELF_RESURRECT with no family bit to bind, Totemic Focus op:cost on all-totem summon mask 537399320, Healing Focus op:notLoseCastingTime, Totemic Mastery op:radius on the friendly-totem PULSES (2026-08-24 fix round — REBOUND from the inert summon mask 537399320 to affectsMask{a:67231744,c:98304} = Stoneskin 8072 A15 + SoE 8076 A16 + Mana Spring/Healing Stream pulses A14/A13 + the 3 resistance pulses A26 | Grace of Air C15 + Windwall C16; buff radius lives on the pulse area-aura, not the summon — the pulse resolves the owner's SPELLMOD_RADIUS via CalcRadius→FillTargetMap. Windfury Totem deliberately EXCLUDED/FLAGGED — a friendly totem in authentic Vanilla but its era reconstruction ships no bindable friendly pulse yet), Healing Grace op:threat, Restorative Totems op:allEffects on the Mana Spring pulse bit14 + Healing Stream pulse bit13 = affectsMask{a:24576}, Tidal Mastery op:crit), a stat (Nature's Guidance `multi` aura54 melee-hit + aura55 spell-hit, Purification aura136 MOD_HEALING_DONE_PERCENT), grants (Nature's Swiftness/Mana Tide grant the clones), and two data PROCS (Ancestral Healing + Healing Way, ProcFlags 16384 DONE_SPELL_MAGIC_DMG_CLASS_POS, **`typeMask:2` PROC_SPELL_TYPE_HEAL is LOAD-BEARING** — a heal event carries typeMask 2 and the generator defaults SpellTypeMask to 1/DAMAGE, so 2&1==0 rejects the proc unconditionally; Ancestral Healing hitMask 2 crit-only, Healing Way chance 33/66/100 scoped to Healing Wave). New _ref.spells totem-summon entries added (Stoneclaw 5730, Searing 3599, Magma 8190, Mana Spring 5675, Healing Stream 5394, Strength of Earth Totem 8075). NO C++/hand-SQL/core-patch, NO DUMMY-marker misc consumed (next free stays 16). **932400-932412 USED; 932413-932499 FREE.** **2026-08-23 REDESIGN (Task-10-blocking review):** Elemental Weapons' original binding (Rockbiter identity bit22 + FT/FB shared imbue-passive bit11) was traced against the runtime (`Player::UpdateDamageDoneMods`, `spell_sha_flametongue_weapon`/`spell_sha_windfury_weapon`, `SpellEffectInfo::CalcValue`/`Unit::ApplyEffectModifiers`) and found to be a mis-bind on all three counts: Rockbiter's AP is a flat `SpellItemEnchantment.dbc` value with zero spellmod path (CODE-VERIFIED INERT, not reconstructable in data — flagged as a per-case core-patch decision for the user, none written); Flametongue/Frostbrand's real damage casts are the separate unique-identity spells Flametongue Attack (10444, word-A bit21) and Frostbrand Attack (8034, word-A bit24), not the shared-bit11 imbue passives (which are either non-caster CalcValue calls, for Flametongue, or simply the wrong spell); and the old FT/FB bit11 binding was LEAKING onto Windfury Weapon's own bit11-carrying CalcValue(player) call, stacking +5/10/15% onto Windfury's already-correct +13/27/40% (measured 18/37/55% total). Fixed: node 18827 now binds only Windfury Attack (unchanged, bit23) + Flametongue Attack/Frostbrand Attack (bit21/24) — see node 18827's own comment for the full per-imbue trace. RUNTIME-VERIFY at Task 10 (real client): Flurry proc-firing, and whether the FT/FB/WF-attack allEffects %s and totem allEffects %s actually move their magnitudes in the combat log. **ELEMENTAL COMBAT tab (Task 8) claims 932413 + 932415** (932414 RETIRED 2026-08-23 — see the EM single-buff note below; now FREE again): **932413** Elemental Mastery — ONE self-buff (template 16166 for instant/icon 117/family 11; category/categoryCooldown NEUTRALIZED to 0 + RecoveryTime 180000 = Vanilla 3-min cd; ProcCharges 1) carrying BOTH effects via the new per-effect helper masks: Effect_1 = FLAT SPELLMOD_CRITICAL_CHANCE +100 (aura 107, misc 7, basePoints 99, dieSides 1 = guaranteed crit), Effect_2 = PCT SPELLMOD_COST -100 (aura 108, misc 14), each authored `maskA:-1877999613 maskB:0 maskC:0` = the 5-spell fire/frost/nature damage set {0,1,20,28,31} (LB/CL/Earth+Flame+Frost Shock; maskB/maskC 0 zero out 16166 Effect_2's stray inherited 12288/4096 bits). One shared charge → a single matching damage cast consumes both (the COST mod registers the charge drop; the FLAT crit adds +100). **932415** Elemental Focus "Clearcasting" buff (template CONVECTION 16039, whose Effect_1 is a SPELLMOD_COST on the CLEAN 5-spell elemental DAMAGE set A {0,1,20,28,31}=LB/CL/Earth+Flame+Frost Shock, B0/C0, NO heal bits; override Attributes 327680 visible-buff + DurationIndex 8 + ProcCharges 1; Effect_2 0-filled — the shaman Omen-of-Clarity analog; node 18805's data proc fires it). **EM SINGLE-BUFF + ROOT-CAUSE FIX (2026-08-23):** the live bug — 932413's OLD crit effect used `aura 108` (ADD_PCT_MODIFIER) for SPELLMOD_CRITICAL_CHANCE, and `Player::ApplySpellMod` deliberately SKIPS a PCT crit mod unless the same aura already applied another mod to the spell (the "Surge of Light" exception), so the crit never applied and its charge only counted down. Fixed to a FLAT crit (aura 107, basePoints 99 + dieSides 1 = +100), matching the stock FLAT-crit precedent Divine Favor 20216 / Cold Blood 14177. EM was ALSO split into two helpers (932413 crit + 932414 cost) = TWO visible buffs; Vanilla EM is ONE buff. Collapsed to a single 932413 once **per-effect helper masks (maskA/maskB/maskC)** existed — both effects author the same damage-set mask on one spell, so 932414 was deleted and its id freed. **HEAL-LEAK note (2026-08-23):** 932415 (and formerly the cost buffs) must template 16039, NOT 16246 (WotLK Clearcasting), whose Effect_1 mask A -1743781437 = bits {0,1,6,7,8,20,27,28,31} INCLUDES Healing Wave(6)/LHW(7)/Chain Heal(8) — a -100% cost there leaks onto heals (IsAffected matches any nonzero-word intersection); Vanilla EM/Clearcasting are DAMAGE-only. 932413 now authors its OWN per-effect masks explicitly (no donor-inheritance dependency for its cost scope). Other Elemental nodes are spellmods (Convection op:cost, Concussion op:allEffects, Earth's Grasp `multi` allEffects-on-Stoneclaw-summon-bit3 + radius-on-Earthbind-slow-pulse-3600-Bbit0, Call of Flame op:allEffects affectsMask{a:1073741824,b:262144} on the fire-totem DAMAGE spells NOT the summons — the Elemental-Weapons redesign lesson, Reverberation op:cooldown, Call of Thunder op:crit, Improved Fire Totems op:threat affectsMask{a:4} on the Magma pulse, Eye of the Storm op:notLoseCastingTime, Storm Reach op:range, Lightning Mastery op:castTime), a stat (Elemental Warding aura 87 MOD_DAMAGE_PERCENT_TAKEN school-mask 28 = fire/frost/nature, ONE aura not multi), a data PROC (Elemental Focus flags 65536 DONE_SPELL_MAGIC_DMG_CLASS_NEG + school 28 + typeMask 1 DAMAGE + hitMask 0 + chance 10 -> 932415), and two GRANTS (Elemental Devastation 30160/29179/29180 = self-contained offensive-spell-crit procs +3/6/9%; Elemental Fury 60188 = self-contained +100%-crit-damage passive spellmod — BOTH grep-confirmed ungated, no HasTalent/core-script gate). **ACCEPTED GAPS (Task-9/10 ledger):** ~~Improved Fire Totems' "Fire Nova Totem activation delay" half~~ RESOLVED 2026-08-24 (fix round): the delay is a creature/AI property with no SPELLMOD op, so `npc_era_fire_nova_totem` (EraTalentTotemScripts.cpp) now reads the totem OWNER's Improved Fire Totems passive (HasAura 926464 r1 / 926465 r2) and subtracts 1000ms/rank from the 4000ms base (rank1→3000, rank2→2000; fail-safe to 4000 on no owner/not-player/disabled) — C++, no DBC/marker. The Magma-threat half stays the op:threat spellmod. + Eye of the Storm's mechanic (Vanilla proc-on-being-crit -> a 6s Focused Casting buff DIVERGED to a permanent per-cast notLoseCastingTime spellmod, matching WotLK stock 29062's own encoding of this talent — a headless-untestable taken-crit proc + a pushback-immunity buff avoided). New _ref.spells entries added (Earthbind slow pulse 3600 B bit0, Fire Nova Totem summon 27623). NO C++/hand-SQL/core-patch, NO DUMMY-marker misc consumed (next free stays 16). **932413 + 932415 USED; 932414 + 932416-932499 FREE.** RUNTIME-VERIFY at Task 10 (real client): Elemental Focus proc-firing (chance-on-cast); EM guaranteed-crit + free-cast on the next elemental spell; Call of Flame / Improved Fire Totems / Earth's Grasp-radius magnitudes actually moving in combat. **TASK 9 FULL-TREE VERIFICATION (2026-08-23, stamp d32d055c):** headless sweep confirmed 46/46 Shaman nodes wired (0 display_only, both YAML grep and live-DB cross-check), 9-class boot 432 nodes, era_audit 0, 124/124 host tests, zero C++ changed this phase (`git diff --stat 64ac4f5..HEAD -- modules/mod-era-talents/src/` empty). **932400-932413 + 932415 USED this phase; 932414 retired/FREE; 932416/932417 SINCE CLAIMED by the totem sub-project (era markers — see the 931000-931399 row); 932418-932499 FREE.** No DUMMY-marker registry misc consumed by Shaman (the misc registry above is authoritative — next free 17). Record: `docs/verification/era-talents-phase10-shaman.md` | claimed |
| 931000-931399 | Shaman totems (Plan 2, class 7) — summon/pulse clones, stride-20 per totem in _ref order (Earthbind..Windwall); creatures in [920100,920290] (920100-920279 Plan 2 Phase A totems, 920280-920290 sub-project 2's Fire Nova Totem extension); markers 932416 (Vanilla) / 932417 (post-Vanilla) | claimed |
| 932999 | generation sentinel | reserved forever |
| 932800-932899 | Warrior (deliberately BELOW the 932900 convention because 932900-998 is full; still well above Warrior's auto-passive ceiling (~924412), so no collision). ARMS (Task 6) claims **932800** Deep Wounds bleed (clone of 12721, 12s), **932801** Mace Specialization 3s stun (authored), **932802** Improved Hamstring 5s root (clone of Entrapment 19185), **932803-807** Tactical Mastery rage-retention clones r1-5 (clone of 12295, family4/icon139). FURY (Task 7) claims **932808** Death Wish Vanilla clone (of 12292 — WotLK dropped the fear immunity + -20% armor/resist), **932810-814** Flurry haste-buff clones r1-5 (of 12966, family0, 10/15/20/25/30%), **932815-819** Enrage melee-damage buff clones r1-5 (of 12880, +ProcCharges 12 + ProcFlags-4 consume-on-swing, 5/10/15/20/25%). PROTECTION (Task 8) claims **932821** Improved Revenge 3s stun (authored, mirrors Mace Spec 932801) + **932822** Concussion Blow Vanilla clone (of 12809 — WotLK is 30s cd + adds damage; Vanilla is 45s cd, stun-only). PROTECTION (Task 12) claims **932823-827** Shield Specialization clones r1-5 (of the stock chain 12298/12724/12725/12726/12727, family0) — each ONE granted passive carrying BOTH halves: Effect_1 aura 51 block% (+1/2/3/4/5%) + Effect_2 aura 42 PROC_TRIGGER_SPELL -> 12964 (+1 rage), plus a spell_proc row flags 680 / HitMask 64 (PROC_HIT_BLOCK) / Chance 20/40/60/80/100 — block-gated (Vanilla), unlike stock which rages on any taken hit. FURY (Bloodthirst rework) claims **932828-831** Bloodthirst damage clones r1-4 (of 23881; 45% AP via era_bloodthirst_vanilla SpellScript, 30 rage, 6s cd via neutralized category + RecoveryTime 6000, Effect_1 TRIGGER_SPELL -> the rank's heal buff), **932832-835** heal buffs r1-4 (of 23885; family0, ProcCharges 5, aura-42 -> the rank's flat heal, proc flags 20), **932836-839** flat heal spells r1-4 (of 23880; family0, SPELL_EFFECT_HEAL 10/13/17/20 self, no client row) — R1 talent-granted (node 18535), R2-R4 trainer-taught (L48/54/60) via spell_ranks 932828->829->830->831 + trainer.Id=1 rows (SQL 2026_08_18_20), strip mirror in EraTalents.cpp kWarriorTrainedRanks + stale-23881 strip. PROTECTION (Last Stand cd fix) claims **932840** Last Stand Vanilla clone (of 12975; category/categoryCooldown neutralized to 0 + RecoveryTime 600000 = Vanilla 10-min cd, vs stock 12975's 3-min category cd; the +30% max-HP is script-bound to stock 12975 so era_last_stand SpellScript — scriptBindings -> 932840 only — replicates spell_warr_last_stand::HandleDummy to cast the stock 12976 buff, SkillLine 256, stale-12975 strip in EraTalents.cpp). PROTECTION (final review R7, 2026-09-06) claims **932841-932844** Shield Slam Vanilla clone chain r1-r4 (templates 23922-23925; the era ranges 225-235/264-276/303-317/342-358 vs stock's WotLK 294-308/346-362/396-416/447-469 — rank-for-rank identical to the first four TBC clones 947541-544, since 2.0 only ADDED r5/r6; **Category 1209 KEPT**, because `Spell::EffectSchoolDMG` (SpellEffects.cpp:351) gates the shield-block-value bonus on `GetCategory() == 1209`; r1 talent-granted by node 18552, r2-r4 trainer-taught at L48/54/60 via spell_ranks 932841->842->843->844 + TrainerId 1 rows in hand SQL 2026_09_06_05; stock 23922-23925 stripped UNCONDITIONALLY from a Vanilla warrior by EraTalents.cpp arm (4)). BOTH ERAS (same round) claim **932845-932847** stance-CORRECTION passives (Defensive/Berserker/Battle) — hidden learned passives carrying `stances:` (ShapeshiftMask 131072/262144/65536) that ride ALONGSIDE the core's hardcoded stance passives 7376/7381/21156, which `AuraEffect::HandleShapeshiftBoosts` casts by id and which therefore cannot be cloned away. Amounts are authored as MULTIPLIERS onto the stock values (the core accumulates MOD_DAMAGE_PERCENT_DONE/TAKEN and MOD_THREAT multiplicatively, MOD_ARMOR_PENETRATION_PCT additively): Defensive aura 79 -5 + aura 10 -10 (=> -9.75% damage done, +30.5% threat); Berserker aura 87 +5 + aura 10 +25 (=> +10.25% damage taken, threat modifier exactly cancelled); Battle aura 10 +25 + aura 280 -10 (=> threat and the WotLK-only +10% armor penetration on stance spell 2457 both exactly cancelled). Granted to BOTH managed eras by EraTalents.cpp CLASS_WARRIOR arm (8) and recorded in `era-data/band-allowlist.yaml` with `eras: [vanilla, tbc]` (the paladin Lay on Hands 932668-670 dual-era-reuse precedent — TBC references the Vanilla-slice ids rather than minting twins). 932809, 932820, 932848-899 free. Defiance (18544) uses a plain `stat` MOD_THREAT aura, NOT the planned clone, so 932820 stays free | claimed |
| 932500-932599 | Druid (Phase 9, class 11). FERAL tab (Task 7) claims **932500-932502** Predatory Strikes % of level clones r1-3 (template 16972/16974/16975; inherit SpellIconID 1563 so the core feral-AP formula at StatSystem.cpp:405 reads the effect-0 DUMMY as the level mult; only effect-0 authored → drops the WotLK weapon-AP% + aura-109 Ravage proc; node 18725 grants them) + **932503** Leader of the Pack +3% party crit clone (template 17007 for passive attrs+icon 312; stances→ShapeshiftMask 145 so the shapeshift passive-loop applies it in cat/bear/direbear; effect 35 APPLY_AREA_AURA_PARTY aura 52; drops 24932's WotLK heal/mana — the core's stock LotP handler is HasTalent(17007)-gated, which era chars lack, so a clone is mandatory; client + skillLine 134). NO DUMMY-marker registry misc consumed (the Predatory DUMMY is icon-read, misc 0; next free stays 16). Blood Frenzy/Primal Fury are plain stock GRANTS (16952/16954 cat-combo, 16958/16961 bear-rage — cleanly separable, the _ref had the trigger labels swapped). BALANCE tab (Task 8) claims **932504** Nature's Grace flat -0.5s next-spell buff (node 18712 reconstruction — clone of stock 16886 reshaped from WotLK's %-cast-SPEED haste to Effect_1 aura107 ADD_FLAT_MODIFIER misc10 SPELLMOD_CASTING_TIME basePoints -500 + ProcCharges 1 + DurationIndex 21 = persists until the next spell consumes it, like Nature's Swiftness; **per-effect mask AUTHORED ALL-ONES** (maskA/B/C = 0xFFFFFFFF, final review I-19 2026-09-06) so it binds EVERY family-7 spell — 1.12 NG was unrestricted, and this row previously said it *inherits* 16886's nature-caster mask, which it no longer does. All-ones is safe because SpellInfo::IsAffected (SpellInfo.cpp:1331) rejects a family mismatch BEFORE reading the flags, and the clone's family is 7. (16886's own effect-1 words are (16777831, 58720288, 512); the often-quoted "B=1" is EffectSpellClassMaskB_1 = effect 2's word A.) Instants still never consume the charge — Player::ApplySpellMod (Player.cpp:10020-10023) skips a SPELLMOD_CASTING_TIME mod when the base cast time is already 0, before the charge is registered; triggered by node 18712's proc which fires on ANY magic-class spell CRIT — spell_proc flags 81920 = 0x4000 DONE_SPELL_MAGIC_DMG_CLASS_POS | 0x10000 DONE_SPELL_MAGIC_DMG_CLASS_NEG (this row read 327680 until 2026-09-06; both eras ship 81920 — verified in the generated SQL for 925696 and its TBC counterpart), typeMask 3 DAMAGE|HEAL, hitMask 2 crit-only, no family/school) + **932505** Nature's Grasp 35%-base clone (node 18701 — clone of stock 16689 overriding ONLY procChance 35 + procCharges 1 vs stock 100%/3; inherits Effect_2 aura42 PROC_TRIGGER -> 19975 Entangling Roots + ProcTypeMask 40 on-taken-hit + 45s/1-min-cd + the Nature's Grasp identity A 1048576/C 4096 so Improved Nature's Grasp 18702's op=procChance spellmod binds & raises it). No DUMMY-marker registry misc consumed (next free stays 16). Other Balance nodes are spellmods (Improved Wrath castTime / Improved Entangling Roots op=notLoseCastingTime / Natural Shapeshifter op=cost on Cat/Bear/DireBear/Moonkin forms / Improved Thorns op=allEffects on the Thorns damage-shield / Nature's Reach op=range / Moonglow op=cost mask=119 exact / Moonfury op=allEffects / Improved Moonfire multi allEffects+crit / Improved Starfire spellmod castTime + `procPassive:` 3-15% stun via STOCK Celestial Focus 16922 3s stun, client-visible, no clone), stats (Natural Weapons aura 79 MOD_DAMAGE_PERCENT_DONE school 1) or grants (Omen of Clarity 16864 / Vengeance 16909-13 — self-contained spellmod-carrier, no HasTalent gate / Moonkin Form stock 24858). **Moonkin Aura is an ACCEPTED baseline gap (Task 10):** the core HARDCASTS stock 24907 on FORM_MOONKIN entry (SpellAuraEffects.cpp:1577 "// Always cast Moonkin Aura", keyed on the form id, NOT the spell or a talent) = WotLK +5% RAID spell crit + haste rating vs Vanilla +3% PARTY spell crit — a form/aura clone CANNOT fix it (the core hardcasts 24907 for whatever entered form 31); only a core patch could, disproportionate for a minor beneficial divergence. Generator gained a `procChance` `_HELPER_SCALARS` entry (-> ProcChance column, mirrors `procCharges`) for 932505; safe for existing helpers (templated inherit, non-templated already default 0). RESTORATION tab (Task 9) claims **NO helpers** — every node is a grant (Furor 17056/17058-61 chain, icon-238 DUMMY read by the core's shapeshift handler, not HasTalent-gated / Insect Swarm 5570, an ACCEPTED baseline-magnitude gap, Task 10, WotLK ~144/12s+-3% hit vs Vanilla 66+-2% / Nature's Swiftness 17116 / Swiftmend 18562) or a spellmod (Improved MotW / Nature's Focus / Tranquil Spirit / Improved Rejuvenation / Subtlety / Improved Tranquility / Improved Regrowth / Improved Healing Touch / Improved Enrage op=effect2) or a stat (Reflection aura134 / Gift of Nature aura136). **TASK 10 FULL-TREE VERIFICATION (2026-08-20, stamp 46d8cbbf):** headless sweep confirmed 47/47 Druid nodes wired (0 display_only), family-7 guard 0 across both the auto-passive band and 932500-505, 8-class boot 386 nodes, era_audit 0, 121/121 host tests. **932500-932505 USED this phase; 932506-932599 FREE.** No DUMMY-marker registry misc consumed by Druid (Predatory Strikes reads by SpellIconID 1563, not a registry slot) — next free DUMMY marker stays 16. Record: `docs/verification/era-talents-phase9-druid.md`. **DRUID CLIENT FIX ROUND (2026-08-21, spec `2026-08-21-era-talents-druid-client-fix-round-design.md`, stamp `b8f05365`) claims 932506-932513:** **932506** Leader of the Pack VISIBLE party area-aura (issue 6 — 932503 stayed the hidden form-passive but its Effect_1 became SPELL_EFFECT_TRIGGER_SPELL -> 932506; 932506 is Attributes 0 / ShapeshiftMask 145 / effect 35 APPLY_AREA_AURA_PARTY aura 52 +3% / DurationIndex 21 / radius 12, no skillLine — the split works around HandleShapeshiftBoosts only reapplying a PASSIVE|DO_NOT_DISPLAY form-passive), **932507-932511** Nature's Grasp ranks 2-6 (issue 1 — clones of stock 16810/16811/16812/16813/17329 inheriting EffectTriggerSpell_2 = Entangling Roots 19974/19973/19972/19971/19970 + the NG identity A 1048576/C 4096, override procChance 35 + procCharges 1 + (FOLLOW-UP 2026-08-21) powerCost 50/65/80/95/110/125 (r1-6, 1.12 mana) + attributes 98304 = inherited 0x10000 | SPELL_ATTR0_ONLY_OUTDOORS 0x8000 (Vanilla NG cost mana + was outdoor-only; WotLK stock 16689 dropped both — applied to R1 932505 too); trainer-taught druid.Id 33 behind a ReqAbility1 chain off talent-granted R1 932505 + spell_ranks 932505->507->...->511, SQL 2026_08_21_00; stock NG chain 16689/.../53312 version-swapped in EraTalents.cpp CLASS_DRUID — stripped in Vanilla, auto-granted by level in WotLK, trainer rows deleted), **932512** Omen of Clarity CASTABLE Vanilla clone (issue 3 — stock 16864 is a hidden passive "does not give a spell"; clone overrides Attributes 0 + DurationIndex 6 = 600000ms/10min + ManaCost 120 + instant, keeps Effect_1 aura42 PROC_TRIGGER -> 16870 Clearcasting + inherited ProcTypeMask 81924; node 18708 grants 932512, stock 16864 stripped in Vanilla. **FOLLOW-UP (2026-08-21, hand SQL `2026_08_21_01_...omen_proc.sql`):** the clone inherited ProcChance 100 with NO spell_proc row → procced Clearcasting on EVERY hit. Fixed by a spell_proc row mirroring stock 16864 — **ProcsPerMinute 3.5** (weapon-speed-normalized; ProcFlags 0 ⇒ use the clone's DBC events, Chance 0) + a `spell_script_names` bind to `spell_dru_omen_of_clarity` (CheckProc filtering). spell_proc/spell_script_names for a clone are HAND SQL — the generator hardcodes ProcsPerMinute 0), **932513** Enrage CUSTOM Vanilla clone (issue 7 — version-swap, no talent gate: granted at L12 in Vanilla / stock 5229 at L12 in WotLK, 5229 trainer row deleted; Effect_1 periodic-energize basePoints 20 = 2 rage/tick x10s = 20 rage over time, Effect_2 ENERGIZE base 0 so Improved Enrage 18736 op=effect2 adds the ONLY instant 5/10 rage = 25/30 total, Effect_3 dropped — 1.12 aura-101 base -76 not confidently decodable as the Classic ~27%-armor drawback, omitted per spec = beneficial divergence). Issue 2 Faerie Fire (Feral) = a plain kBaselineSpellGates row `{ CLASS_DRUID, 18729, {16857}, 18, true }` + delete 16857's trainer row (no clone). Issue 4 Predatory Strikes / issue 5 Heart of the Wild = no mechanic change (HotW node 18730 tooltip rewritten to the shipped WotLK Int% + Bear-Sta Int%/2 + Cat-AP Int%/2 behavior). NO DUMMY-marker registry misc consumed — next free stays 16. **932500-932518 used; next free 932519** (932519-932599 FREE). **932514-932518** = Insect Swarm r1-r5 (node 18738, deferred-gaps round 2026-09-07, G-51) at the wowhead-Classic 1.12 values — per-tick 11/23/29/44/54, -2% hit, mana 45/85/100/140/160; r2-r5 trainer-taught on TrainerId 33 + `spell_ranks` root 932514, and reconcile arm (9) strips stock 5570/24974-24977/27013/48468, so the “Insect Swarm 5570 ACCEPTED baseline-magnitude gap” noted earlier in this row is CLOSED. DRUID COMPLETE & real-client verified 2026-08-21, final gen `8740b990` (fix round + 2 follow-ups: Omen proc 3.5 PPM, NG mana/outdoor). Record: `docs/verification/era-talents-phase9-druid.md` (fix-round addendum + close-out) | claimed |
| 932600-932699 | Paladin (Phase 8, class 2). RETRIBUTION tab (Task 6) claims: **932600/932601** were RESERVED for SotC/JotC but went UNUSED — the Task 6b reconstruction used the contiguous block **932634-932645** instead (see below); 932600/932601 are now FREE. **932602** Sanctity Aura +10% Holy party area-aura (template clone of Devotion Aura 465, effect 35 APPLY_AREA_AURA_PARTY), **932603-605** Vindication Str/Agi -5/10/15% debuff clones r1-3 (aura 80 MOD_PERCENT_STAT, 10s — WotLK 9452/26016 reduce AP flat, retuned), **932606** Seal of Command seal clone (template 20375; 70% weapon-dmg Holy proc via 932607, single-target — WotLK 20424 is 35%+cleave), **932607** Seal of Command proc damage (WEAPON_PERCENT_DAMAGE 70% Holy, hidden), **932608-609** Eye for an Eye clones r1/r2 (template 9799/25988, basePoints 14/29 → 15/30% — Vanilla; WotLK retuned in patch 3.0.2; the stock CORE script `spell_pal_eye_for_an_eye` is re-bound to the clones via scriptBindings), **932610-614** Vengeance Physical+Holy dmg buff clones r1-5 (template 20050, +3/6/9/12/15%, 8s — stock is 3 ranks 5/10/15%, retuned). No DUMMY markers claimed (all reads are self-amount or stock-script; next free stays 16). HOLY tab (Task 7, nodes 18601-18614) claims **NO helpers** — every node is a grant (Consecration 26573 / Divine Favor 20216 / Holy Shock 20473 / Illumination 20210-20215 chain), a spellmod (Spiritual Focus / Improved SoR via affectsMask B-bit29 / Healing Light / Improved BoW / Improved LoH cooldown / Lasting Judgement duration) or a stat/multi (Divine Strength/Intellect aura 137, Holy Power aura 71 misc 2, Unyielding Faith aura 117 misc 5/2). Consecration (18606) is the Holy baseline-leak gate (TRAINER-baseline, `2026_08_18_32...trainers.sql` + a CLASS_PALADIN kBaselineSpellGates row). PROTECTION tab (Task 8, nodes 18615-18629) claims **932615** Holy Shield block-buff castable (clone of 20925: +30% block + aura-42 -> 932616, 4 charges, 10s, 175 mana, category-neutralized 10s cd, block-gated proc hitMask 64; aura-189 rating dropped), **932616** Holy Shield block damage (hidden, SCHOOL_DAMAGE 50 Holy, targetA 6), **932617** Blessing of Sanctuary castable (clone of 20911: aura-14 MOD_DAMAGE_TAKEN -7 all-schools on the ally + aura-42 -> 932618, block-gated, 5 min, data-only — sidesteps stock 20911's core script), **932618** Blessing of Sanctuary block damage (hidden, SCHOOL_DAMAGE 14 Holy), **932619-623** Redoubt proc passives r1-5 (clone of 20127, aura-42 -> the rank buff, hitMask 2 crit-only, chance 100; stock aura-150 block-value passive dropped), **932624-628** Redoubt block-chance buffs r1-5 (clone of 20128, aura-51 +6/12/18/24/30% block, 10s, 5 charges via own hitMask-64 proc; VISIBLE client rows), **932629-633** Reckoning proc passives r1-5 (clone of 20177 -> STOCK 20178 extra-attack aura; only the proc chance retuned to Vanilla 20/40/60/80/100% vs stock 2/4/6/8/10%, hitMask 2 crit-only, flags 139944). No DUMMY markers claimed (all block/crit procs are pure spell_proc data; next free stays 16). Other authored Prot nodes are spellmods (Improved Devotion Aura via affectsMask A=64 / Improved Concentration Aura via affectsMask A=131072 / Improved Righteous Fury op-effect1 threat / Improved Hammer of Justice cooldown / Guardian's Favor multi cooldown+duration on Hand of Protection 1022 + Hand of Freedom 1044), stats (Precision aura 54 / Toughness aura 142 / Shield Spec aura 150 / Anticipation aura 30 misc 95 / One-Handed Weapon Spec aura 79 equipSubclass 145) or a grant+gate (Blessing of Kings 20217 = the Prot baseline-leak gate, extends `2026_08_18_32...trainers.sql` + a CLASS_PALADIN/18620 kBaselineSpellGates row). 932646-699 remain FREE. NOTE the seal-unleash is FULLY DATA-DRIVEN: core `spell_pal_judgement` casts each seal's EFFECT_2 DUMMY amount as the judgement spell, so a custom seal (family 10 + a seal family-flag bit → SPELL_SPECIFIC_SEAL) unleashes its judgement with zero core patch. **Task 6b — Seal of the Crusader FULL ranked reconstruction (node 18633 WIRED, no longer display_only):** the base SotC seal + Judgement of the Crusader are ABSENT from 3.3.5a (survival: reconstruct). Authored as **932634-932639** (seal r1-6, cloned off 20375 to inherit word-A bit25 → SPELL_SPECIFIC_SEAL: mutual exclusion + judge-unleash) + **932640-932645** (JotC r1-6, aura 14 MOD_DAMAGE_TAKEN misc 2 Holy). **6 ranks** (Vanilla max; the wowhead "rank 7" at L61 is a TBC addition, excluded). VALUES are the AUTHORITATIVE 1.12.1 client Spell.dbc (`era-wow/tools/vanilla-extract/Spell.dbc`, seal ids 21082/20162/20305-08, JotC 21183/20188/20300-03): seal AP 18/30/55/85/130/180 (base+1; the small per-level scaling 0.7-2.4 is dropped — generator authors flat values), attack speed +40%, mana 25/35/55/75/105/135, levels 6/12/22/32/42/52; JotC Holy-dmg-taken 20/30/50/80/110/140, 10s (magnitudes cross-check wowhead Classic exactly). NB wowhead Classic Era (1.15) lists HIGHER seal AP/mana — a later-vanilla rebalance; the 1.12.1 client is the project's canonical Vanilla source. OBTAIN = **reconcile-grant by level** (ReconcileBaselineSpells CLASS_PALADIN block grants seal ranks to Vanilla paladins at their learn level, strips ALL ranks in any higher era — the reverse of the baseline-LEAK gates, since SotC exists ONLY in Vanilla; no trainer rows, no marker); the JotC debuffs are cast triggered by the core, never known by the player. Improved SotC (18633) SCALING = a single plain SPELLMOD **op effect1** (SPELLMOD_EFFECT1), pct 5/10/15%, `affectsMask {c:512}` — both the seal AP (Effect_1 aura 99) and the JotC Holy-dmg (Effect_1 aura 14) live on Effect_1, so effect1 reaches BOTH while leaving the seal's attack speed (Effect_2) and DUMMY judgement-id (Effect_3) untouched; NO DUMMY marker, NO core patch, NO C++ scaling script (next free stays 16). The bind uses a FREE family-10 word-C bit9 (flagsC 512): every word-A bit is saturated across the 740 stock paladin spells, so both custom spells carry word-C bit9 (`maskC: 512`) — added to the generator's `_HELPER_SCALARS` as `maskB`/`maskC` → SpellClassMask_2/_3 (word A `mask` couldn't express it). No stock seal carries bit9, so no over-bind. **Real-client-pass fixes (2026-08-19) claim 932646-932672 (and correct 932615-618):** Holy Shock (node 18614) now grants a Vanilla CLONE instead of stock 20473 (WotLK ~314-340): **932646-648** dummy clones r1-3 (template 20473; inherit Effect_0 DUMMY + target-ANY(25) + 20yd + instant; bound to the `era_pal_holy_shock` SpellScript via scriptBindings) branch to **932649-651** hidden damage clones (SCHOOL_DAMAGE Holy 204-220/279-301/365-395) or **932652-654** hidden heal clones (SPELL_EFFECT_HEAL, SAME per-rank numbers) — Vanilla dual heal+damage at 1.12.1 magnitudes with NO EffectBonusMultiplier (core-default ~0.43 instant coefficient), owner-confirmed dual behavior. 3 ranks (R1 talent-granted, R2/R3 trainer-taught L48/56). **The heal clones carry `powerCost` 320/410/480 so ILLUMINATION refunds mana on a Holy Shock crit heal:** core `spell_pal_illumination` refunds base-cost*talent%; for a Holy Shock heal WITHOUT `SpellFamilyFlags[1] & 0x10000` it reads the heal spell's OWN ManaCost (the `else` branch), and the auto-generated Illumination proc (no `spell_proc` row → SpellFamilyMask 0 → SpellFamilyName forced to 0 = no family filter) fires on any crit heal. The heal is ALWAYS cast triggered (era_pal_holy_shock → CastSpell(...,true) = TRIGGERED_FULL_MASK ignores power cost), so the ManaCost is read but NEVER charged — Holy Shock still costs mana once via the dummy. HL/FoL Illumination unaffected. Holy Shield (node 18629) is 4 ranks (1.12.1 20169/20925/20927/20928): R1 = 932615/616 CORRECTED to 40 Holy / 140 mana, higher ranks **932655/657/659** castable r2-4 + **932656/658/660** block-damage triggers (65/95/130 Holy). Blessing of Sanctuary (node 18626) is **4 ranks** (owner-confirmed 2026-08-19: the 14/21/28/35 block sequence is ranks 1-4, there is no rank 5; the 1.12.1 DBC's lowest entry 20204 is not a real rank here) = 1.12.1 20911/20912/20913/20914, L30/40/50/60, mana 60/85/110/135, reduction **10/14/19/24**, block-damage **14/21/28/35 on ALL four ranks (r1=14)**. R1 = 932617 (reduction 10 + block 14 via 932618); higher ranks **932661/662/664** castable r2-4 + **932663/665/667** block-damage triggers 21/28/35 Holy. (The retired rank-5 clone **932666 is now FREE again**.) Holy Shock/Holy Shield/BoS higher ranks are trainer-taught (paladin trainers 3/4/5) via ReqAbility1 + spell_ranks chains off the talent-granted rank 1 (2026_08_19_30_era_talent_paladin_active_rank_trainers.sql); the band sweep (`EraBandClassifier`) strips them for a Vanilla paladin without the talent, and a `kEraPalStockSwaps`-adjacent arm strips a stale stock 20473 (the old per-class `kPalTrainedChains` table is retired). Lay on Hands (node 18607) **932668-670** Vanilla clones r1-3 (template 633; RecoveryTime 3,600,000 = Vanilla 1hr, category neutralized; HEAL_MAX_HEALTH + energize 250/550 mana on r2/r3; level-granted L10/30/50 by ReconcileBaselineSpells CLASS_PALADIN, stock 633/2800/10310/27154 stripped in Vanilla + kReconcileOnLearn strip-on-train — NOT a trainer-row delete, because LoH must keep working in WotLK; it is a version-swap, not a functionality leak) + **932671/672** armor-rider buffs (aura 142 MOD_BASE_RESISTANCE_PCT +15%/+30% of the LoH TARGET's item armor, 2 min, dur idx 4) fired by node 18607's new `procPassive:` block. **Generator feature added:** a `mechanic: spellmod` node may carry a `procPassive: {trigger[], flags, ...}` — the ONE granted passive gets Effect_2 aura-42 PROC_TRIGGER_SPELL + a matching spell_proc row, so a spellmod node can also hang a per-rank triggered aura on another unit without a second grant (used by Improved LoH: cooldown mod + armor-on-cast). Test: `test_spellmod_node_with_proc_passive`. Icon fixes: node 18629 `spell_holy_blessingofprotection` (was the only `classic_`-prefixed icon → blank), clone 932602 client.icon 502 (spell_holy_mindvision, matches node 18642; was 291 = Devotion Aura). **SEAL & JUDGEMENT RECONSTRUCTION (2026-08-20, spec `2026-08-20-paladin-vanilla-seal-reconstruction-design.md`) claims 932700-932748** — the four baseline seals reworked to Vanilla via a version-swap (grant Vanilla clones by level in Vanilla, strip stock seals `20154/20164/20165/20166` + the 3 stock judgement buttons `20271/53407/53408` in Vanilla, keep stock for WotLK) + a single custom `Judgement` with consume-on-judge. **932700-707** SoR seal ranks 1-8 (clone off 20375 → SPELL_SPECIFIC_SEAL, 30s, Effect_1 proc → on-hit, EFFECT_2 DUMMY → JoR), **932708-715** SoR on-hit Holy dmg r1-8 (family 10 + `maskC: 1024` so Improved SoR binds; base 3/7/12/19/28/39/51/65 × mainhand-speed via `era_pal_seal_of_righteousness` AuraScript bound to the seals), **932716-723** Judgement of Righteousness r1-8 (flat instant Holy 11/19/31/46/63/83/106/132, family 10 + maskC 1024, triggered payload NOT a button), **932724** Seal of Justice (reuses stock stun 20170 + stock JoJ 20184), **932725-728** SoL seal r1-4, **932729-732** SoL on-hit flat heal r1-4 (38/52/75/93), **932733-736** Judgement of Light payload clones r1-4 (template 20185 for bit19 → Lasting Judgement; flat heal 24/33/48/60 via `era_pal_judgement_of_light` AuraScript, WotLK stock is %-max-HP), **932737-739** SoW seal r1-3, **932740-742** SoW on-hit flat mana r1-3 (49/70/89), **932743-745** Judgement of Wisdom payload clones r1-3 (template 20186; flat mana 32/45/58 via `era_pal_judgement_of_wisdom` AuraScript, WotLK stock is %-base-mana), **932746** the single custom `Judgement` (clone off 20271 → family 10 word-A bit23 so Improved/Lasting Judgement + Benediction bind; bound to `era_pal_judgement` SpellScript = replicate core `spell_pal_judgement` seal EFFECT_2-DUMMY unleash + `RemoveAura(seal)` consume, omit WotLK button-debuff/base-dmg), **932747** JoL flat-heal carrier, **932748** JoW flat-mana carrier. Improved SoR rebind uses a NEW free family-10 word-C **bit10 = 1024** (SotC used bit9=512). Reconcile grant-by-level table extends the CLASS_PALADIN `kSotcSealRanks` shape; `kReconcileOnLearn` gains the 7 stock ids. Four era scripts (era_pal_judgement, era_pal_seal_of_righteousness, era_pal_judgement_of_light, era_pal_judgement_of_wisdom) — no DUMMY-marker registry misc consumed (all reads are self-amount/GetAmount; next free stays 16). Plan `docs/superpowers/plans/2026-08-20-paladin-vanilla-seal-reconstruction.md`. **SEAL TRAINER-TAUGHT REWORK v2 + fix rounds (2026-08-20) claim 932749-932753:** **932749** = the hidden Vanilla-only seal-training MARKER (Attributes 262608, no client; the `ReqAbility1` anchor every baseline seal/Judgement/LoH rank-1 trainer row requires — see the "Era-gating CUSTOM spell ranks that REPLACE a stock WotLK version" section above); **932750-932753** = Seal of Command ranks 2-5 (trainer-taught behind ReqAbility1 off the talent-granted R1 932606, mana 95/120/155/180 @ L30/40/50/60, all reusing the shared 70%-weapon-holy ON-SWING proc 932607; spell_ranks root 932606; the band sweep owns the strip — the old `kPalTrainedChains {18637,...}` row is retired). The four baseline seals + Judgement + LoH + SotC + SoC were all converted from auto-grant to trainer-taught; stock WotLK seals/judgement-buttons/LoH deleted from the Vanilla trainer + auto-granted to WotLK by level; `manaCostPct:0` added to every template-cloned seal (they inherited the template's %-cost); on-hit/proc damage helpers got `client:` blocks for combat-log visibility; a re-entrancy guard added to ReconcileBaselineSpells; an in-world re-strip (EraTalentPin OnPlayerUpdate) clears the core-auto-learned SoR 20154 client phantom. **FINAL REVIEW 2026-09-06 claims 932673-932683** (all in the 932600-932699 Retribution slice, whose previous high-water mark was 932672): **932673-677** flat *Judgement of Command* r1-5 (family 10 + word-A bit23, school 2, SCHOOL_DAMAGE targetA 6, per-level growth bounded by the wago SpellLevel/MaxLevel window — the SAME magnitudes as the TBC chain 946103-107, which TBC never retuned; each SoC rank's EFFECT_2 DUMMY now names its own JoC instead of re-firing the 70% on-swing proc 932607, and the "x2 if stunned" half stays an accepted gap); **932678-682** *Illumination* clones of 20210/20212-215 with `EffectBasePoints_2` 99 (= a 100% refund vs live's WotLK 30%), each `scriptBindings`-bound to the CORE AuraScript `spell_pal_illumination` (which reads the refund % off the aura's OWN Effect_2) and each carrying its own clone `spell_proc` row mirroring the module's widened `-20210` row (family 10 / mask0 0xC0200000 / mask1 0x10000 / SpellTypeMask 0 / phase 2 / HitMask 2 crit-only, `flags` spelled out as 20210's DBC ProcTypeMask 17408 because a 0 ProcFlags forbids authoring the phase/hit masks); **932683** *Repentance* clone of 20066 (`durationIndex` 32 = 6 s, `targetCreatureType` 64 = Humanoid-only, vs live's WotLK 60 s / mask 118). Nodes 18609 and 18644 grant the clones, and a single era-keyed `kEraPalStockSwaps` table in `ReconcileBaselineSpells` strips stock 20210/20212-20215 and 20066 in BOTH managed eras (readiness-gated per row via `EraNodeReplacesStock`). Same review: Sanctity Aura 932602 gains `mask: 0` (the Devotion Aura identity bit 64 the template leaked; maskC 32 kept), SoJ 932724 `manaCostPct: 13`, SoC 932606+932750-753 / SoJ 932724 / SoL 932725-728 / SoW 932737-739 swap their flat `chance` for real `ppm` rates (7 / 5 / 10 / 12), per-rank `spellLevel:` lands on the SoR, Holy Shock, Holy Shield and BoS chains, Deflection's stat effect pins `school: 0`, and hand SQL `2026_09_06_04_era_talent_final_review_paladin.sql` adds the five BoS clones (932617/932661/932662/932664 + TBC 946136) to stock `spell_group` 1007. **FREE: 932666 (retired BoS r5), 932684-932699; next free 932754 onward** | claimed |

**Shaman totem id bases (Plan 2 Phase A reservation, `931000-931399` summon/pulse + `[920100,920290]`
creature entries — 920100-920279 Phase A, 920280-920290 sub-project 2's Fire Nova Totem extension).** Deterministic allocation over the 18 content totems in `_ref` order: totem `i`
gets `summonBase = 931000 + i*20`, `pulseBase = summonBase + 10`, `creatureBase = 920100 + i*10`; rank
`r` uses `summonBase+(r-1)` / `pulseBase+(r-1)` / `creatureBase+(r-1)`. Era markers `932416` (Vanilla
Totems) and `932417` (post-Vanilla/WotLK Totems) are separately claimed FREE ids in shaman's
`932400-932499` slice (see that row above).

| Totem | summonBase | pulseBase | creatureBase |
|---|---|---|---|
| Earthbind | 931000 | 931010 | 920100 |
| Stoneskin | 931020 | 931030 | 920110 |
| StrengthOfEarth | 931040 | 931050 | 920120 |
| Stoneclaw | 931060 | 931070 | 920130 |
| Tremor | 931080 | 931090 | 920140 |
| Searing | 931100 | 931110 | 920150 |
| Magma | 931120 | 931130 | 920160 |
| Flametongue | 931140 | **931144** | 920170 |
| FrostResistance | 931160 | 931170 | 920180 |
| HealingStream | 931180 | 931190 | 920190 |
| ManaSpring | 931200 | 931210 | 920200 |
| FireResistance | 931220 | 931230 | 920210 |
| PoisonCleansing | 931240 | 931250 | 920220 |
| DiseaseCleansing | 931260 | 931270 | 920230 |
| Grounding | 931280 | 931290 | 920240 |
| NatureResistance | 931300 | 931310 | 920250 |
| GraceOfAir | 931320 | 931330 | 920260 |
| Windwall | 931340 | 931350 | 920270 |

**Flametongue is the one row the formula does NOT describe** (corrected 2026-09-06): its chain is
two spells per rank, not one, so the deterministic `pulseBase = summonBase + 10` (931150) is not what
shipped. The real allocation is **931144-931147 = the totem's PASSIVE** (`aura 23`
PERIODIC_TRIGGER_SPELL @5000 ms — this is the id the `totems:` block names as the creature's Index-0
pulse) and **931148-931151 = the EFFECT** it triggers (`SPELL_EFFECT_ENCHANT_HELD_ITEM` on the party
area). 931150 is therefore a live EFFECT id, not a free base. Read the dataset, not the formula.

Deep Wounds (Warrior, Arms) is proc-scripted and reads its own `aurEff->GetAmount()`, so it needs NO
cross-spell DUMMY-marker registry value — proc-scripted nodes never consume a registry misc. (Improved
Berserker Rage, by contrast, is `scripted` — its behavior binds to a DIFFERENT spell, stock Berserker
Rage 18499, so it DOES claim a cross-spell registry misc: #15.)

The generation stamp itself covers more than the datasets: it hashes the YAMLs AND the generator's own
inputs (`gen_era_talents.py`, `build_client_dbc.py`, plus the two DBC column/type files
`spell_dbc_columns.txt`/`spell_dbc_coltypes.txt`) — a tool-only change with no YAML edit is
still a new generation by design, so the stamp bumps and the canary fires even when no
spell content changed.

## Prerequisites: the _ref file and node schema

**`era-data/_ref/<class>-spells.yaml`** (e.g. `mage-spells.yaml`, `priest-spells.yaml`) is
the live-verified spell-family reference that `affects:`/`proc.affects`/family-mask binding
resolve against. **Never author its values from memory.** Read the methodology header
inside the _ref file itself: values come from a DBC read via `tools/dump_spell_family.py`
(SpellClassSet + SpellClassMask_1/2/3 straight out of the binary `Spell.dbc`) cross-checked
against a live `lookup spell` on the dev-box console via `tools/wgconsole.py` — never a
guess or a tooltip. Bootstrapping a NEW class starts by building its _ref file this way.

**Node schema:** nodes live under `tabs[].nodes[]` with `id`/`tab`/`tier`/`col`/`maxRank`/
`prereqTalentId`/`prereqPoints` (plus mechanic-specific keys per the decision tree above).
Node ids come from the Daribon-imported vanilla trees — `era-data/_skeletons/*.skeleton.yaml`
(currently `vanilla-mage.skeleton.yaml` and `vanilla-priest.skeleton.yaml`, produced by
`tools/import_daribon_talents.py`). `gen_era_talents.py`'s `lint()` rejects most structural
mistakes with actionable `ValueError`s — trust it and iterate rather than hand-verifying the
schema yourself.

**Icons and tree backgrounds: GRAB them, never guess — `tools/fetch_calc_icons.py <Class>`.**
Every node needs a real `icon:` (a client `Interface\Icons\` basename) and every tab needs a real
`background:` (a client `Interface\TalentFrame\` basename); the Daribon importer leaves both blank
and guessing from the tab/talent name ships "?" icons and identical tree backgrounds (the warlock
2026-08-16 bug). The tool pulls both authoritatively for the Vanilla era:
  * **Icons** come from the [maladr0it/classic-talent-calculator](https://github.com/maladr0it/classic-talent-calculator)
    repo (`src/trees/<Class>/data.ts`), whose Vanilla talent set matches ours 1:1 by name and was
    cross-validated 100% against wowhead + the local WotLK `Spell.dbc` during the warlock pass.
  * **Backgrounds** come from the tool's static `TalentTab.dbc` BackgroundFile table (all 9 classes)
    — the real basenames, which do NOT always match the tab name (Warlock Affliction→`WarlockCurses`,
    Demonology→`WarlockSummoning`; Paladin Retribution→`PaladinCombat`; Druid Feral→`DruidFeralCombat`).
Run `python3 tools/fetch_calc_icons.py <Class>` to print paste-ready `icon:`/`background:` values while
building the tree, and `--check era-data/vanilla/<class>.yaml` to diff an authored tree against the
source (matches by exact node name — a renamed node reads as "missing", an off icon as "mismatch").
Basenames are case-insensitive in the 3.3.5a client, so the lowercase the tool emits works verbatim.

## Totem NPCs (`totems:`)

A dataset-level `totems:` list (a sibling of `tabs:`/`helpers:`, not nested under a node) emits
**creature-only** rows for an era totem NPC: one `creature_template` row, one
`creature_template_model` row, and one or two `creature_template_spell` rows (Index 0, and Index 1
if a second pulse is given) — no `spell_dbc` rows come from a `totems:` entry itself. Keys per
entry:

* **`creature`** — the creature `entry`, must fall in the reserved band `[920000, 921000)`
  (`TOTEM_CREATURE_BASE`/`TOTEM_CREATURE_END`, see the id-band registry above — this is the
  creature-entry space, not the spell band).
* **`name`** — the totem's `creature_template` name (shown floating over the totem in-world).
* **`pulse`** — the Index-0 `creature_template_spell`: the id of an emitted helper/passive that
  becomes the totem's cast pulse, loaded into the totem creature's `m_spells[0]` and cast by the
  core's `Totem::InitSummon`/`TotemAI` (the same mechanism a stock totem NPC uses).
* **`pulse2`** (optional) — a second `creature_template_spell` at Index 1, for a totem that casts
  two pulses (e.g. a totem with both a periodic proc and a periodic damage tick).
* **`model`** (optional) — the `creature_template_model` `CreatureDisplayID`; defaults to 4588
  (the stock generic totem model) when omitted. For a PLAYER-owned totem this is cosmetic only —
  the core's `GetModelForTotem` overrides the displayed model by race/totem-type at summon time
  regardless of what ships here.

The SUMMON spell that plants the totem and the PULSE spell(s) it casts are **ordinary `helpers:`
clones**, not part of the `totems:` block itself:

* The summon helper's `Effect 28` (SUMMON) `misc` value must equal the totem's `creature` entry —
  that's the only link from the summon spell to the NPC row; `helpers:` needs no new keys for this.
* A pulse clone **must preserve the stock pulse spell's `SpellClassMask`** (inherit it from a
  `template:`, or author the matching `mask`/`maskB`/`maskC` by hand) so any shipped talent
  spellmod that targets the stock pulse (e.g. a totem-radius or totem-damage `%` mod) keeps
  binding to the clone. Dropping the mask silently strands those spellmods.

**A reused creature entry is REFERENCED, never REDECLARED.** When a later era's totem measures
identical to an earlier era's shipped clone, that era's summon points its `EffectMiscValue` straight
at the existing creature entry and ships **no `totems:` row of its own**. Copying the `totems:` row
into the second dataset emits **two `creature_template` rows for one entry from two SQL files**, and
load order then decides which pulse the NPC casts. This is how the TBC shaman reuses the Vanilla
substrate (`920100-920290`); every later era reusing it must do the same.

`tools/era_audit.py::check_totem_summons` is the automated guard for all of this: it validates the
full summon → creature → pulse chain — a `totems:` entry's `creature` must be the SUMMON target of
some emitted helper, and its `pulse`/`pulse2` must each resolve to an emitted spell id — and fails
the audit on any broken link (orphan creature, missing summon, or a pulse id that isn't shipped),
plus a **duplicate-declaration** finding for the redeclare mistake above.

**Its resolution is MANIFEST-WIDE, not per-dataset** (2026-09-01, Plan 3b Task 2 batch 1), precisely
because of that reuse rule: a run always loads every dataset in `era-data/datasets.txt`, so a
cross-era reference resolves. Three consequences worth knowing before you rely on it:

- **Findings are raised only for datasets named on the run.** A dataset loaded purely for resolution
  is never itself reported on — the bare sweep catches its problems.
- **Declaration order is `resolution_only + datasets`**, so a duplicate names the dataset you are
  EDITING as the offender. The reverse ordering pointed authors at merged, real-client-verified
  content on `master`.
- **The `pulse`/`pulse2` FK pool is manifest-wide too**, which matches runtime (all the SQL loads
  into one DB) but means a typo landing on *another* class's spell id will pass. `check_cross_dataset_ids`
  is the backstop, and it only runs when more than one dataset is named — so **a targeted single-file
  audit does not catch an id collision with a shipped era.** Run the bare sweep before committing.

Like any other generator/dataset change, adding or editing a `totems:` block only ships via
`tools/era-regen.sh` — it regenerates the SQL, the addon Lua, and the client MPQ patch together
under one generation stamp; hand-editing generated SQL for a totem row is not a legal ship path.

## The pipeline (the only legal ship path)

    era-data/<era>/<class>.yaml            <- the single source of truth
        |  tools/era-regen.sh              <- ALWAYS this; never regen one leg by hand
        +-> modules/mod-era-talents/data/sql/world/base/*.sql   (spell_dbc, spell_proc,
        |     spell_script_names, era_talent, era_talent_rank, era_talent_meta stamp)
        +-> client-addons-src/EraTalents/data/generated/*.lua   (addon tree + spellMods)
        +-> client-addons/_data-patches/patch-V.mpq             (client Spell.dbc +
              SkillLineAbility rows + sentinel 932999, merged into IP's patch)

    Ship server side: sync module into azerothcore-wotlk/modules/ (era-regen --sync-fork or
    setup.sh/update.sh on the server) -> db-import applies SQL -> worldserver restart.
    spell_dbc has no reload — always a worldserver restart; spell_proc CAN be
    `.reload spell_proc`'d live, but a new proc's custom spell_dbc row won't exist until
    restart anyway, so in practice: restart.
    Ship client side: copy patch-V.mpq to each client's Data/ AND the EraTalents addon
    (from client-addons/EraTalents/, restaged by fetch-client-addons.sh) to Interface/AddOns/,
    then RESTART the game (MPQs load only at launch). A stale addon copy silently lacks the
    canary check — restage after any addon change.

    Drift detection: the generation stamp (8-hex hash of the YAMLs + generator tool files) ships in
    era_talent_meta AND in sentinel spell 932999's client name. The addon compares both on
    every SYNC and warns in red chat on mismatch. `.eratalents doctor <char>` prints the
    server-side stamp and per-character diagnostics.

**Adding a class touches ONE manifest line plus FOUR hand-maintained client-side lists.**
`era-data/datasets.txt` is now the single source of truth for the server-side pipeline
(`tools/era-regen.sh` + `tools/era_audit.py` both read it — see below); everything else in the
dataset→artifact mapping is still duplicated by hand and every one of these must gain the new
class:

| Site | What to add |
|------|-------------|
| `era-data/datasets.txt` | one line: `<era>/<class>.yaml  <era_talent-data SQL filename>  <custom-spell SQL filename>`. This single line is picked up automatically by BOTH `tools/era-regen.sh` (which reads the manifest to build its `DATASETS` array and `DATA_SQL`/`CUSTOM_SQL` maps, keyed on the era-relative path so `tbc/mage.yaml` can never collide with `vanilla/mage.yaml`) and `tools/era_audit.py` (`DEFAULT_DATASETS` self-derives from the same file) — **neither of those two files needs any edit of its own anymore** |
| `client-patch/merge-into-patch.sh` | its own separate `DATASETS` array |
| `client-addons-src/EraTalents/build-addon.sh` | inline per-class `gen_era_talents.py` calls (`--sql` / `--lua` / `--custom-sql`) |
| `client-addons-src/EraTalents/EraTalents.toc` | the generated `data\generated\<Class>Vanilla.lua` line — miss it and the addon loads without that class, silently |
| `tools/gen_era_talents.py` | `CLASS_TOKENS` (line 12) — keyed by **class id**, NOT the spell-family id the audit's family set uses; the two disagree and even invert (Hunter = class 3 / family 9, Warlock = class 9 / family 5), so a bare `9` means Warlock here and Hunter there. **Fallback only**, and the one entry here that does not break anything if you forget it: `cls_name = d.get("className") or CLASS_TOKENS.get(cls, str(cls))`, and every dataset authors `className:`, so a missing token degrades the addon's displayed class name to a bare numeral rather than failing. Add it anyway; a numeric class name in the UI is a confusing way to learn this. Druid (Phase 9) registered as `11: "DRUID"` (class id 11, spell family 7). Shaman (Phase 10) registered as `7: "SHAMAN"` (class id 7, spell family 11) — the mirror-inversion of Druid's class-id/family-id pair (Druid = class 11 / family 7); a bare `7` means Shaman here but Druid's family set elsewhere, and a bare `11` means Druid here but Shaman's family set elsewhere |

`era-data/datasets.txt` (the manifest) is the **source of truth** for the server-side pipeline.
Two guards in `tools/test_gen_era_talents.py` now derive from it by parsing the manifest
(`_regen_datasets()`) instead of keeping yet another copy:
`test_committed_meta_sql_matches_datasets` (stamp) and
`test_era_audit_default_datasets_match_regen` (audit coverage). Neither needs editing when a
class is added, but each stays red until its OWN precondition is met, and they are different
preconditions — read the failure, don't fix the wrong file:

- **stamp test** — red until `tools/era-regen.sh` has actually been run, because the committed
  `era_talent_meta` SQL still carries the pre-class stamp. Editing the manifest alone will not
  clear it.
- **audit-coverage test** — red until `era-data/datasets.txt` gains the class's line.
  Running `era-regen.sh` will not clear it — `era_audit.py`'s `DEFAULT_DATASETS` reads the
  manifest directly, so the manifest line IS the fix here.

That stamp test previously hardcoded its own three-class list and spent a commit asserting the
pre-hunter stamp against a correctly-regenerated meta SQL, which is why it self-derives now.
Before the manifest existed, `tools/era-regen.sh`'s `DATASETS` array and `tools/era_audit.py`'s
`DEFAULT_DATASETS` were two independent hand-maintained copies of the same list, keyed on the
BARE basename. A class missing from `era_audit.py`'s copy silent-skipped it there while
`era_audit.py` still reported `0 finding(s)` — a false "clean," not a caught failure — and it
bit twice: warlock (its family-5 sweep coverage never actually ran) and rogue (shipped without
it 2026-08-17; caught by the guard test before merge, not by the audit itself — see
`docs/verification/era-talents-phase6-rogue.md`). Separately, a same-basename dataset under a
second era directory (e.g. a future `tbc/mage.yaml` alongside `vanilla/mage.yaml`) would have
silently collided in `era-regen.sh`'s `DATA_SQL`/`CUSTOM_SQL` maps and had one class's generated
SQL overwrite the other's. Both classes of bug are now structurally impossible: one manifest,
one path-keyed source, both readers deriving from it — and the manifest's own parsers (bash and
Python) validate field count and reject duplicate paths rather than silently truncating.

`fetch-client-addons.sh` does not currently maintain a dataset list of its own — it
delegates to `merge-into-patch.sh` — but re-verify that with a grep before assuming it
stays that way. A missed site is exactly the drift the canary (stamp mismatch) is
built to catch, just discovered later and more confusingly than a code review would
have caught it. Single-sourcing the remaining client-side lists (`merge-into-patch.sh`,
`build-addon.sh`, `EraTalents.toc`) is future work — deliberately out of scope for the manifest
(they're cheap enough to hand-maintain and touch client build/packaging concerns the manifest
doesn't model).

## Field semantics you WILL get wrong without this table

| Field | Trap |
|---|---|
| `Description_Lang_enUS` (client) | Spellbook/castable tooltip **effect prose ONLY** — server discards it, but the client renders the mechanical header (power cost / range / cast time / cooldown / "Requires …" / Reagents / Tools) as its own structured lines from the DBC numeric fields (`ManaCost`/`ManaCostPct`, `RangeIndex`, `CastingTimeIndex`, `RecoveryTime`/`CategoryRecoveryTime`, `EquippedItemClass`, reagent/totem columns — all inherited by a `template:` clone or set from `powerCost`/`rangeIndex`/`castTimeIndex`/`cooldown`). So a header baked into `client.description` renders **twice** (crammed). Author the effect sentence only. |
| `AuraDescription_Lang_enUS` (client) | THE BUFF/DEBUFF TOOLTIP. Template clones inherit the stock WotLK text — custom PI described itself as haste PI. Author `client.auraDescription` on every visible-aura clone (generator hard-errors if the template would leak). |
| `spell_dbc.Description_Lang_enUS` (SERVER row) vs `client.description` (YAML) | **Two different rows, and only one of them is ever rendered.** A `template:`d clone inherits the donor's `Description`/`AuraDescription` into its **server-side** `acore_world.spell_dbc` row, so `SELECT Description_Lang_enUS FROM spell_dbc WHERE ID=<clone>` shows the STOCK WotLK sentence — and that is harmless: the server discards spell text entirely, and the client reads its tooltip from the row the **MPQ** carries, which is built from `client.description`/`client.auraDescription`. Reviewers have twice flagged a clone as "shipping the wrong tooltip" off that server query. **The only authoritative check is the client-visible row**: read `client:` in the YAML, or the merged `Spell.dbc` the regen writes (`client-addons/_data-patches/patch-V.mpq`) — never the live `spell_dbc` text columns. A clone with NO `client:` block ships no client row at all (correct for a hidden marker/payload, wrong for anything the player can see). |
| `spell_proc.HitMask` | 0 = core default (normal+crit(+absorb)). 2 = crit-only. **8 = FULL_RESIST-only — on a DONE proc, a proc that never fires; on a TAKEN-only proc it is the legitimate 'fires only when I fully resist' contract (Magic Absorption -29441 and its era clones), and `era_audit.py` exempts that case.** The generator defaults 0; state it explicitly anyway. |
| `spell_proc.SpellTypeMask/SpellPhaseMask/HitMask` | Only meaningful for certain ProcFlags; the generator zeroes inapplicable ones (LoadSpellProcs logs errors otherwise). |
| `spell_proc` explicit-but-unusable fields | The generator hard-errors (`ValueError`) if you author `typeMask`/`phaseMask`/`hitMask` that the core would ignore for your `flags` — fix the flags or drop the key. |
| `spell_proc.AttributesMask` | 2 = PROC_ATTR_TRIGGERED_CAN_PROC — REQUIRED for any "all your <school> spells" proc (channels like Mind Flay and AoEs like Blizzard damage via triggered sub-spells). |
| `character_spell.specMask` | Row presence != knows the spell. removeSpell on a talent-cost spell RETAINS the row with specMask=0; `HasSpell` checks `IsInSpec`. Diagnose with `.eratalents doctor`, not raw row queries. |
| `EffectBasePoints`/`EffectDieSides` | Stock DBC rows store value-1 with DieSides=1 (roll adds 1..dieSides). The generator's authored effects use raw basePoints with DieSides=0 (value = basePoints exactly); the spellmod/stat builders use eff-1 with DieSides=1. Don't mix the conventions. |
| `EquippedItemClass` | 0 (zero-filled) = ITEM_CLASS_CONSUMABLE and KILLS every proc via the equip gate. Custom passives must carry -1 (the generator does). |
| `equipClass`/`equipSubclass` (YAML) | Weapon-equip gate (core `CheckAttackFitToAuraRequirement`) — a node's optional `equipClass:`/`equipSubclass:` now applies on `stat`, `proc`, AND `multi` rows (was `stat`-only, the Mage Wand Specialization precedent; Rogue Sword/Mace Spec need it on a proc row). |
| `affectsMask` (YAML, spellmod nodes/effects) | Overrides the emitted `EffectSpellClassMask` (A/B/C) verbatim (`{a,b,c}`) instead of ORing every `affects:` spell's FULL identity via `_class_mask`. **Required when a target's identity carries a SHARED family bit that would over-bind** — core `SpellInfo::IsAffected` ANDs across all three flag words, so a shared bit matches every spell that carries it. The paladin AURA-improvement talents are the case: Retribution/Devotion/Concentration Aura all share flagsC bit5 (=32, "paladin aura" category), so binding by full identity leaks the +% onto the other two auras — bind by the UNIQUE flagsA bit only (Ret=8, Devotion=64, Concentration=131072). `affects:` is still authored (drives the addon spellMods name-map + the not-in-_ref.spells lint); only the DBC mask changes. Lint rejects a non-dict or all-zero mask. |
| `CumulativeAura` | The spell_dbc stack column. `StackAmount` is only the C++ field name; a literal `StackAmount` key is silently dropped. |
| spell_dbc schema | Column count is ASSERTed against the DBC format at startup — adding/dropping a column crashes the worldserver. DB rows fully REPLACE the DBC record (no merge; empty strings inherit). |
| `op: resistMiss` (YAML, spellmod nodes) | SPELLMOD_RESIST_MISS_CHANCE (16) — the op for a HIT talent that raises the chance a specific spell (or spell family) lands. **Do NOT reach for aura 55 (MOD_SPELL_HIT_CHANCE) with a `school:`/family restriction: it is a NO-OP.** `StatSystem.cpp` sums `SPELL_AURA_MOD_SPELL_HIT_CHANCE` BARE — no school/family filter — so the aura either applies to *every* spell school or does nothing useful for a targeted talent; the mask you author is simply never read. Use `op: resistMiss` (aura 107/108, EffectMiscValue 16) with an `affects:`/`affectsMask:` binding instead — the fork consumes it in `MagicSpellHitResult` (Unit.cpp:3532, added to modHitChance) and in the melee/ranged spell miss calc (Unit.cpp:15325). Precedent: Classic Trap Mastery 19376 and Improved Feign Death 19287/37484. |
| `targetCreatureType` (YAML, helper scalar) | The DBC `TargetCreatureType` creature-type GATE — a BITMASK of `CREATURE_TYPE_*` as `1 << (type - 1)`, **not** a raw type id (64 = Humanoid, 1 = Beast). 0 = no gate. `SpellInfo::CheckTarget` refuses any unit outside the mask, so a clone whose era record only worked on a narrower victim set than the WotLK spell must author it — a `template:` clone silently inherits the stock (usually widened) value. |
| SpellInfoCorrections sweep | `SpellInfoCorrections.cpp:5245+` rewrites spells by family/icon/flag PATTERNS after our rows load. `tools/era_audit.py` checks our rows against the transcribed predicates — run it after authoring anything with a family mask or nonstandard icon fields. |

## Verification checklists (a spell is DONE when its checklist passes — not before)

**Every spell:** `tools/era-regen.sh` run (no partial regens); `uv run --with pyyaml
python tools/era_audit.py` clean (it self-enforces its own family-coverage assumption —
a new class's family outside {0,3,5,6,9} is itself a finding until SWEEP_PREDICATES is
extended); worldserver boots with no `spell_proc`/`sql.sql` errors mentioning the id;
`.eratalents doctor` clean on a test character that has the talent.

**Proc spell:** deal a NORMAL (non-crit, non-resist) triggering hit and show the proc fired
— combat log line, `Playerbots.log`, or the proc's visible effect. "The row looks like a
working spell's row" is NOT verification (VE's row looked perfect; HitMask killed it).

**Castable/buff/debuff:** server: aura present with expected amounts (doctor, or
`character_aura` after `.saveall`). Client: spellbook tooltip (Description) AND buff tooltip
(AuraDescription) both read the ERA text. Measured behavior: the stat/damage actually moves
(before/after numbers, not the character sheet alone — percent-done auras multiply at
damage time).

**SPELLMOD/stat passive:** before/after measurement of the modified quantity (cast time,
tooltip number via the addon's Tooltip.lua, damage).

**Client-visible anything:** canary green after installing the fresh patch-V.mpq (no red
generation warning at login).

## Process rules

- Never claim a spell works from data inspection alone; the checklist's runtime evidence is
  mandatory (this framework exists because "VE heal now procs" shipped unverified).
- One generation at a time: if you touched a YAML, run era-regen.sh before ANY testing —
  a stale leg invalidates whatever you observe.
- Stock-id edits are forbidden; if the decision tree bottoms out at a core patch, write a
  spec first (patch 0018 is the template) and keep the marker in the band.
- New DUMMY-marker misc values: claim the next free integer here and in the script that
  reads it.
- **Era transitions at 7→8 and 12→13 are player-initiated** (patch 0027 + `EraAdvanceGossip`): a test
  that needs a character to cross an expansion boundary uses `.ip set` (Force path) or the Anduin/Thrall
  gossip — a boss kill or the 10259 turn-in now clamps to 7 / 12 and logs `manual-advance hold`. Do not
  read that log line as a bug.

- **A retraction is not done until you have swept for EVERY copy — and plans and specs count.**
  One wrong claim about Healing Stream ("its pulse is `aura 23 → 52041 → 52042`, so `SPELLMOD_DOT`
  is provably inert") was written once and ended up in **five** places: a dataset node comment, a
  spec amendment, the `_ref` accepted-gap record, this registry, and the live plan. Three separate
  correction rounds each fixed some and missed others. The last one found was the **prescriptive**
  copy — the plan told the next executor to do the exact thing the correction had rejected, so a
  half-swept retraction was actively more dangerous than the original error.
  The rule:
  1. **Grep for the claim's FINGERPRINTS, not its conclusion** — the spell ids it names (`52041`),
     its distinctive phrases ("never a DOT", "provably inert"), the mechanism it asserts. A search
     for the conclusion finds only the copies that state it outright.
  2. **Mark every hit in place**, separating RETRACTED PREMISE from UNCHANGED CONCLUSION. Filing the
     correction *next to* the stale text instead of *on* it fails any reader who greps to one key or
     reads one field.
  3. **Sweep `docs/superpowers/plans/` and `specs/` too**, not just `_ref` and the datasets. A
     `_ref`-only sweep misses exactly the copy that gives instructions.

- **For an era clone, the spell that matters is the CLONE at the ERA's shape — never the live stock
  id it shares a name with.** This produced three separate confident-but-wrong claims in TBC Phase 3
  alone (the Healing Stream DOT reasoning; "the 8443 fuse series is absent" argued from
  `acore_world.spell_dbc`, which is the 6.8k-row CUSTOM-OVERRIDE table and not the 50k-row DBC; and
  Restorative Totems' inertness). Before writing "code-verified" or "provably inert", state which
  spell id you inspected and confirm it is the one the era character actually receives.
- **Persist the `era_character_talent` rank BEFORE `learnSpell` in `TryLearn`.** `learnSpell` fires
  `OnPlayerLearnSpell`, which for any id in `kReconcileOnLearn` (every gate-managed stock grant —
  Divine Spirit 14752, Shield Slam 23922, Ice Block 45438, Consecration/BoK, the seals/totems…)
  re-runs `ReconcileBaselineSpells`. That reconcile's baseline gate STRIPS the grant when it reads
  `CurrentRank(talentId) == 0`. If the rank row is written *after* `learnSpell`, the reconcile fired
  by that very learn sees rank 0 and strips the spell on the spot — the talent point stays spent but
  the spell vanishes until a relog's `ReapplyOnLogin` re-grants it (which then survives, because the
  row exists by then). Live-confirmed on Divine Spirit: the client showed the spell, casting hung
  because the server never actually knew it, and R2+ were untrainable (their trainer `ReqAbility1` =
  the missing rank 1). This bug is INVISIBLE on bots — `OnPlayerLearnSpell` early-returns on
  `IsBot`, so the reconcile-on-learn (and thus the strip) never fires for a bot. Only a real player
  reproduces it. **Diagnostic (watch for this on EVERY class as you test):** the symptom is "talent
  is learned but its granted spell doesn't work / casting hangs / higher ranks won't train," and
  `.eratalents doctor <char>` prints `node <talent> rank N: grant <spellId> known=NO  <-- PROBLEM`
  (a gate-managed stock grant absent from `character_spell`). The `TryLearn` reorder fixes it going
  forward; an already-broken character self-heals on the next login (`ReapplyOnLogin` re-grants once
  the rank row is present). `known=NO` on a NON-gate-managed grant is a different fault — trace the
  specific strip.

## Imported cross-class gotchas

Items below are carried over from `docs/era-talents-cross-class-gotchas.md` that aren't
already covered by the tables/sections above (the `attrMask: 2` proc rule, the
`CumulativeAura`/`StackAmount` field trap, and the DUMMY-marker registry are already
covered above and are not repeated here).

### Generator / clone traps (final review, 2026-09-06)

Record: `docs/verification/era-talents-final-review-tbc-vanilla.md`.

- **`affects:` ORs each named spell's FULL family mask** — so a spellmod also lands on every OTHER
  spell that shares a bit. Rogue word-1 bit 23 is "every combo strike"; mage Blizzard word-0 bit 19
  is also Frost Armor / Ice Armor. When the bit is shared, author **`affectsMask:`** with the unique
  bit instead of naming the spell.
- **A clone inherits NO `spell_linked_spell` and NO `spell_group` rows** — the stock spell's rows are
  keyed by its id and never follow. Mirror them in hand SQL: Combustion's stack cleanup, the
  Power-Infusion / Arcane-Power exclusivity group and the mortal-wounds group were each silently lost
  this way.
- **Aura 55 `MOD_SPELL_HIT_CHANCE` ignores `school:` entirely** — a school-scoped hit talent is a
  no-op. Hit talents must author **`op: resistMiss`** (with the spell mask), not a school.
- **Every Vanilla `_ref` file is a 3.3.5a id/mask REFERENCE with no 1.12 magnitudes** — its numbers
  are live values captured for identity work. Never quote one as an era value; the era magnitude
  comes from the 1.12 source, per the value-basis rules above.

### Procs

- **Match by damage SCHOOL, not the family-flag word list, for "all your `<school>` spells."**
  Some spells' family identity lives only in `SpellFamilyFlags` word 3 (e.g. Mind Flay `0,0,0x440`);
  the word-3 proc mask matches by every static check yet not in practice. Use
  `proc: { family: <n>, school: <mask> }` (generator supports both) — unambiguous AND more faithful.
  (Combined with the `attrMask: 2` above; the school switch alone was necessary-but-insufficient for
  Mind Flay — the real gate was the triggered-proc attr.)
- **A clone of a PPM-rated stock proc needs `proc: { ppm: <rate>, chance: 0 }` — and the rate is a
  FLOAT.** `spell_proc.ProcsPerMinute` scales the chance by the caster's weapon speed
  (`chance = value * attackSpeed / 60`, `Unit::GetPPMProcChance`), which a flat `Chance` cannot
  express, and **a clone inherits no `spell_proc` row at all**, so a PPM-rated stock proc reverts to
  a flat per-hit chance unless the clone authors its own. `SpellMgr::LoadSpellProcs` falls back to
  the spell's DBC `ProcChance` only when Chance **and** ProcsPerMinute are both 0, so the stock
  shape to reproduce is `ProcsPerMinute = <rate>, Chance = 0`.
  * **Fractional rates are the norm, not the exception** — the column is `float NOT NULL` and the
    core reads it as a float (`SpellMgr.h`, `float ProcsPerMinute`). Both PPM rows this repo ships
    by hand are **3.5** (Vanilla and TBC druid Omen of Clarity). The generator emits an int for an
    integral rate and a float otherwise, so `ppm: 3.5` survives; it did not always, and an `int()`
    cast there is a 14% rate error that raises nothing and fails no audit.
  * **Never author `chance:` alongside a non-zero `ppm`** — the generator hard-errors on it.
    `Aura::CalcProcChance` seeds `chance` from the row and then *overwrites it wholesale* with the
    PPM result whenever the event carries damage or heal info, so an authored chance is dead for
    every ordinary proc and resurrects only for a damage-less one. Omitting the key zeroes it
    silently (the stock shape); a negative `ppm` is also refused, because
    `SpellMgr::LoadSpellProcs` would clamp it to 0 at boot and the row would lose its rate.
  * **Prefer the key over hand SQL.** Before this existed, a PPM clone had to ship a hand-written
    `spell_proc` migration (`2026_08_21_01_era_talent_druid_omen_proc.sql` and its TBC twin
    `2026_08_31_04_era_talent_tbc_druid_omen.sql` are the two in tree, both annotated "the generator
    hardcodes ProcsPerMinute = 0" — no longer true). A dataset `proc:` block keeps the rate, the
    ICD and the family mask in one reviewed place and regenerates with everything else.
- **Trap-improvement procs ride `PROC_FLAG_DONE_TRAP_ACTIVATION` (0x200000) + a family mask over
  the trap EFFECT spells** (Hunter Entrapment 18334, Task 7). Spell.cpp:2288 raises the flag only
  for the four hunter trap-effect casts (family 9, `flags[0]&0x18` = Freezing/Frost Aura or
  `flags[2]&0x24000` = Explosive/Immolation); the proc event's spellInfo IS the trap effect spell,
  so a `spell_proc` family mask scopes WHICH traps qualify (something the stock DBC-only 19184
  proc can't do). Two non-defaults are load-bearing: `phaseMask: 4` — the event fires at
  PROC_SPELL_PHASE_FINISH and the generator's default HIT phase (2) never matches — and
  `attrMask: 2` (GO trap casts are triggered casts). The trap triggerer arrives as the proc
  victim (Spell.cpp:4329), so a trigger spell lands on the enemy that sprang the trap. FINISH
  events carry SPELL_TYPE_MASK_ALL, so the default `typeMask` 1 passes even for no-damage traps.
- **Watching a data-proc fire headlessly:** add a file appender + `Logger.spells.aura=5` to the
  live worldserver.conf (the conf log scale is 0-6 where 4 = Info and **5 = Debug** — a "4" logger
  silently drops LOG_DEBUG lines) and grep the file for `Triggering spell <id> from aura <passive>
  proc` (AuraEffect::HandleProcTriggerSpellAuraProc, SpellAuraEffects.cpp:6913). Combined with a
  grinding bot (BotActiveAlone=100 + `.playerbots rndbot reload/grind <bot>`) this proves a proc
  chain end-to-end without a client.
- **Verify proc/aura ENUM constants against the fork's headers, never memory.** Wrong values pass
  headless (the generator emits whatever you give it) and only fail in-game. Confirmed traps:
  `PROC_FLAG_KILL = 2` (not 4 = melee auto-attack), `MECHANIC_STUN = 12` (not 7 = root),
  `MOD_MECHANIC_RESISTANCE = 117` (the "chance to resist" aura — NOT the WotLK `MECHANIC_DURATION_MOD`
  = 232), `MOD_SPELL_DAMAGE_OF_STAT_PERCENT = 174` / `…HEALING… = 175`. When unsure, `dump_spell_effects.py`
  a stock analog and copy its exact encoding.

### Client visibility

- **Any custom spell the player must SEE needs a `client:` block + a patch rebuild.** A custom aura
  (debuff, stun, buff icon) or castable spell that the 3.3.5a client doesn't ship has NO row in the
  client `Spell.dbc`, so the client renders nothing — the effect still happens server-side but
  *invisibly*, which reads as "it never fired."
  - Priest **Blackout**'s authored stun helper (932970) worked server-side from day one but had no
    `client:` block → no stun icon; combined with its authentic 2–10% chance it looked completely dead
    until `client: { name, icon }` was added and the patch rebuilt.
  - Contrast: stock triggers (Ignite, silence, Winter's Chill, the stun/debuff a *stock* spell applies)
    are already in the client DBC — no `client:` block needed.
  - **Rule:** if you author a helper/debuff/stun/castable via `helpers:` and a player or its target
    should see an icon, add `client: { name, icon }` (a real `SpellIconID`) and rebuild the patch
    (`fetch-client-addons.sh` merges it into patch-V; or `client-patch/build-client-patch.sh`).
- **A spell used as a `trainer_spell.ReqAbility{1,2,3}` MUST have a client `Spell.dbc` name row**, even
  a hidden passive marker. When a trainer service is selected the WotLK trainer UI formats the
  required-ability's NAME (`GetTrainerServiceAbilityReq`), resolved from the client `Spell.dbc`; a
  reqAbility with no client row returns nil → `Blizzard_TrainerUI.lua ClassTrainer_SetSelection` crashes
  (`bad argument #2 to 'format'`) the instant the trainer opens on a learnable gated spell. Bit the
  shaman totems (2026-08-24): the era markers 932416/932417 gate the totem trainer rows but were authored
  `attributes:262608` with no `client:` block. Fix = add `client: { name, icon:1 }` (name only). Keep the
  marker's passive+hidden attr so the name does NOT add it to the spellbook, and give it **no
  `skillLine:`** so it gets no SkillLineAbility row/tab. Cost: the matching-era player sees a greyed/met
  "Requires &lt;name&gt;" line on each gated spell — accepted cosmetic of the reqAbility gate. **The old
  parenthetical here ("paladin/warlock markers never hit this — auto-learn, never trainer reqs") was
  STALE and caused a repeat:** the 2026-08-20 paladin seal rework made 932749 a trainer ReqAbility with
  no client row (latent — masked whenever Lua error display is off, since the C-side
  `SelectTrainerService` runs before the throw so training still worked), and TBC paladin byte-copied
  the shape into marker 946078 → the whole paladin trainer window Lua-errored empty on a real client
  (2026-08-30). Both markers now carry `client:` blocks ("Seal Training" / "TBC Seal Training").
  Sweep to run after ANY trainer-gating change: every `trainer_spell.ReqAbility{1,2,3}` ≥920000 must
  appear in the built client Spell.dbc.
- **Talent-panel icons: the Daribon importer leaves `icon: ''` → every node shows "?".** Populate them
  from `tools/fetch_calc_icons.py <Class>` (see *Prerequisites*), NOT from memory. Each `icon:` is a real
  `Interface\Icons\` basename the addon prepends the prefix to; basenames are case-insensitive so the
  tool's lowercase works verbatim. (Legacy resolver, if you ever lack the tool: bound nodes take the
  granted spell's `SpellIconID`, authored nodes the talent-name spell's, mapped via `SpellIcon.dbc`.)
- **Tree backgrounds: the `background:` token IS a real `Interface\TalentFrame\` file basename, and it
  does NOT always match the tab name.** The addon passes it straight through (`TalentUI.lua`
  `BackgroundBase`), and a failed `SetTexture` on a wrong (missing-file) token LEAVES THE LAST VALID ART
  in place — so one correct tab makes every tree look identical (the warlock 2026-08-16 bug: Affliction
  is `WarlockCurses`, Demonology `WarlockSummoning`, not `WarlockAffliction`/`WarlockDemonology`).
  `tools/fetch_calc_icons.py` prints the real basename per tab (static `TalentTab.dbc` table for all 9
  classes); never guess from the tab name.
- **The addon replaces the ENTIRE `PlayerTalentFrame` for era chars — anything else living inside that
  frame loses its access route.** `Hook.lua` guards `PlayerTalentFrame:OnShow` and, for an era char,
  `HideUIPanel`s the whole stock frame and shows the era panel instead. The hunter PET talent tree
  (Ferocity/Cunning/Tenacity) is a side spec-tab INSIDE that stock frame, so era hunters lost all access
  to it even though pet talents were deliberately kept on the stock WotLK system (the 2026-08-17 hunter
  real-client bug). The re-entry pattern (reusable for any future class with something inside the talent
  frame): a one-shot `ET._allowStock` flag that the `OnShow` guard honors, wrapped in `ET.OpenStockTalents`,
  exposed as an era-frame button + `/pettalents` slash. To land on and confine to a specific sub-tab:
  auto-`:Click()` the target spec tab (identify the pet tab by its `specIndex` being a `"petspec..."`
  string — let `GetPetTalentTree()` pick the active pet's tree, never hardcode), and suppress the other
  spec tabs with **`SetAlpha(0)` + `EnableMouse(false)` re-applied via each tab's `OnShow` hook, NOT
  `Hide()`** — `Hide()` flips `IsShown()`, which Blizzard's own frame layout reads and fights, and is the
  more taint-prone path (learning talents is a protected action; keep manipulations off `IsShown`).
  Restore the tabs on the frame's `OnHide` so a WotLK-era char is untouched. Addon-only — no
  generation-stamp/MPQ/server change.

### Totem authoring (the `totems:` feature — Vanilla shaman)

A totem = **summon** spell (Effect 28 SUMMON, `misc` = creature entry) + a **`totems:`** creature
(`creature_template`/`_model`/`_spell`) + a **pulse** at `creature_template_spell` Index 0 (the buff
the totem casts). Retunes clone a stock pulse (`template:`); the two air totems with no WotLK
equivalent (Grace of Air, Windwall) author the pulse **from scratch** with a minted family-11 mask.
Two invariants the from-scratch path made us learn the hard way (both real-client-verified 2026-08-24):

- **A from-scratch totem party-buff / persistent area aura MUST be PASSIVE** — set `attributes: 320`
  (SPELL_ATTR0_PASSIVE 0x40 + DO_NOT_LOG 0x100). A totem pulse is a persistent area aura with **no
  SpellDuration entry** (DurationIndex 0); `Aura::CalcMaxDuration` (SpellAuras.cpp:804) grants infinite
  duration ONLY `if (IsPassive() && !DurationEntry)`. A **non-passive** aura with no duration entry gets
  maxDuration **0** and expires the instant it is applied — the totem summons, the buff flashes for a
  split second and vanishes, and the totem looks broken. `template:` clones inherit 320 from the stock
  pulse for free; from-scratch pulses (`attributes` defaults to 0) must set it explicitly.
- **A cloned totem's periodic-refresh / second-hop pulse must `trigger:` the CLONE id, not the stock
  spell it cloned.** Grounding's Index-1 refresh pulse (clone of stock 8179) kept `trigger: 8178`
  (stock) while the Index-0 pulse applied our clone 931290 → the initial buff and the ~10s refresh were
  **two different spell ids**, so the player saw two magnet buffs (and potentially two redirect
  charges). Point the refresh at the custom clone. (Stock `trigger:` ids are fine when the triggered
  spell is an enemy debuff / threat / weapon-strike, not a player buff-bar aura — e.g. Stoneclaw
  threat+stun, Tremor dispel, Flametongue weapon strikes.)

### Server data tables this server actually reads

- **Trainers load from the modern `trainer` / `trainer_spell` tables, NOT legacy `npc_trainer`.**
  `ObjectMgr` reads `trainer_spell`; `npc_trainer` (and its templates like 200012) is dead weight the
  worldserver never loads. To gate a trainable spell, edit `trainer_spell` — find the `TrainerId` via
  `creature_default_trainer` (the priest trainer is `trainer.Id = 11`, "Hello, priest!"). Deleting a
  rank-1 row + chaining `ReqAbility1` on higher ranks gates the whole chain. `.reload trainer` applies
  it live. (Keep the `npc_trainer` edits too for consistency, but they're inert here.)
- **Vanilla-talent-that-became-baseline spells leak into the era.** A spell that was a Vanilla TALENT
  but a BASELINE spell in TBC/WotLK is free to Vanilla-era chars unless gated. IP's spell-block hook
  only fires for *WotLK-stage* players (returns early below level 70 / pre-WotLK), so it does NOT gate
  low eras. The module's `ReconcileBaselineSpells` (extensible `kBaselineSpellGates` table, now run for
  **ALL classes** — each row self-selects on its own `classId`, so it's no longer priest-only) is the
  fix: reconcile strips the spell from a Vanilla character who hasn't spent the gating talent, and the
  `era_talent_pin` `OnPlayerLearnSpell` hook strips it the instant such a character buys a rank from the
  trainer. Add a row to that table per such spell per class. **Two flavors — pick the right one:**
  - **AUTO-baseline** (`grantHigherEra = true`, e.g. **Holy Nova**): became a level-learned baseline in
    TBC/WotLK. Reconcile *force-grants* rank 1 to a TBC/WotLK character at `minLevel` (the core would
    have level-learned it) and strips it from Vanilla-without-talent.
  - **TRAINER-baseline** (bought FROM THE TRAINER in WotLK, ungated — **Shield Slam** (warrior node 18552,
    ranks 23922/23923/23924/23925/25258/30356/47487/47488), **Ice Block** (mage node 18046, 45438),
    **Divine Spirit** single-target (priest node 18113, ranks 14752/14818/14819/27841/25312/48073),
    **Consecration** (paladin Holy node 18606, ranks 26573/20116/20922/20923/20924/27173/48818/48819 —
    rank 1 26573 trained at L20, gate SQL `2026_08_18_32_era_talent_paladin_baseline_gate_trainers.sql`)):
    **a reconcile strip ALONE does NOT gate these — the trainer still LISTS the rank-1 spell, so a
    Vanilla char just trains it (this shipped broken once: `grantHigherEra=false` strip-only, user
    caught it in-client).** You MUST **delete the rank-1 `trainer_spell` row** in a module SQL migration
    (the Holy Nova pattern — see `2026_08_18_21_era_talent_baseline_gate_trainers.sql`); the higher ranks
    already chain `ReqAbility1` on rank 1, so removing rank 1 gates the whole chain behind the talent.
    Deleting rank 1 is GLOBAL (removes it for WotLK too), so set **`grantHigherEra = true`** to auto-grant
    rank 1 to a TBC/WotLK char at `minLevel` — they then train the higher ranks off it. Net: Vanilla-with-
    talent gets rank 1 from the talent grant; Vanilla-without can't obtain it; WotLK auto-learns rank 1 at
    level (a minor divergence from real-WotLK "train it", identical to Holy Nova). The reconcile strip
    stays as the safety net for pre-fix chars + era transitions, and `OnPlayerLearnSpell` covers a stray
    purchase, but **the trainer-row deletion is the actual gate.** NB the raid version of Divine Spirit
    (Prayer of Spirit) is a SEPARATE spell chain and is deliberately left ungated — gate only single-target
    ranks.

- **Era-gating CUSTOM spell ranks that REPLACE a stock WotLK version (the Paladin seal pattern, 2026-08-20).**
  When a spell exists in BOTH eras but the era version differs in values/ranks/mechanics (the Vanilla paladin
  seals: SoR/SoJ/SoL/SoW/SotC + Seal of Command + Lay on Hands + the single Judgement), you author custom era
  ranks and must make them obtainable ONLY in the era while the stock WotLK spell is obtainable ONLY in higher
  eras. This is the trainer-taught + version-swap discipline that took several real-client rounds to get right:

  1. **Obtain the custom ranks by TRAINER, not auto-grant.** Auto-granting all ranks by level floods the
     spellbook and isn't how Vanilla played (owner-rejected). Author each rank as a `helpers:` clone; add
     `trainer_spell` rows (paladin trainers 3/4/5); chain higher ranks' `ReqAbility1` off the previous rank;
     add a `spell_ranks` chain (first_spell_id = rank 1) so the spellbook COLLAPSES to one icon.
  2. **The era-gate is the rank-1 `ReqAbility1` anchor** — a spell only an era-appropriate character has, so a
     WotLK paladin (lacking it) can't train ANY rank of the chain. Two anchor flavors:
     - **TALENT spell** → the talent-granted rank 1 IS the anchor (Holy Shock/Holy Shield/BoS R2+, Seal of
       Command R2-5 chain off the talent-granted R1 932606, Bloodthirst, Conflagrate). The generic
       **band sweep** (`EraBandClassifier`, Phase 9.5) strips the trained higher ranks for a character
       who no longer holds the talent — rule (2), "a `spell_ranks` rank above a node-granted r1". The
       per-class `kPalTrainedChains` arm this bullet used to name is RETIRED; do not re-add one.
     - **BASELINE spell** (no talent — the four baseline seals + Judgement + LoH) → author a **hidden
       Vanilla-only MARKER** passive (paladin `932749`, Attributes 262608, no client block) that
       `ReconcileBaselineSpells` grants to every Vanilla char; EVERY baseline chain's rank 1 sets
       `ReqAbility1 = <marker>`. A WotLK char never has the marker → can't train the chain. This is the
       baseline analog of the talent anchor and is the key new mechanism.
  3. **Remove the stock WotLK version from the era — TWO cases, by how the core teaches it:**
     - **Trainer-taught stock** (SoJ/SoL/SoW 20164/20165/20166, judgement buttons 53407/53408, LoH ranks) →
       DELETE its `trainer_spell` rows globally + add a reconcile ELSE-branch (or `kBaselineSpellGates`
       `grantHigherEra`) that auto-grants the stock to TBC/WotLK chars BY LEVEL (multi-rank: grant every rank
       by its level). Vanilla char: stock stripped + can't train; WotLK char: auto-learns it (minor "learn at
       level" divergence, the Holy-Nova/Shield-Slam precedent).
     - **Core AUTO-LEARN stock** (`SkillLineAbility.AcquireMethod = 2`, e.g. Seal of Righteousness 20154 on
       skill 594, Judgement of Light 20271) → there is NO trainer row to delete; the core re-teaches it every
       login via `learnSkillRewardedSpells`. A login-time reconcile strip removes it SERVER-side but
       **`Player::removeSpell` suppresses the SMSG_REMOVED_SPELL client packet during `PlayerLoading`
       (Player.cpp:3388)**, so the client shows a PHANTOM icon (server correct, client stale — check
       `character_spell`, not the client, to confirm). Fix: an **IN-WORLD re-strip** of the stock ids in
       `EraTalentPin::OnPlayerUpdate`'s throttled block (packets flow in-world → the client clears it; fires
       once per login then no-ops), IN ADDITION to the login/level strip + the `kReconcileOnLearn` strip-on-learn.
  4. **Reconcile shape (CLASS block):** Vanilla → grant the marker + strip the stock ids; else (TBC/WotLK) →
     strip the marker + ALL custom ranks (so a transitioned char loses them) + auto-grant the stock by level.
     A WotLK char must end WITH stock and WITHOUT customs in BOTH the fresh-login and the era-transition path —
     verify both.
  5. **Four gotchas that each cost a real-client round:**
     - **Template clones inherit `ManaCostPct`** (Seal of Command 14% from 20375; BoS 7% from 20911) — the core
       charges flat cost PLUS the %, so actual mana >> the flat tooltip. Author `manaCostPct: 0` on every
       flat-cost clone (leave a genuinely %-cost spell like Judgement at its 5%).
     - **Hidden triggered damage/heal spells need a `client: {name,icon}` block** (no `skillLine`, so no
       spellbook row) or they are INVISIBLE in the combat log — the damage happens but "does nothing" visibly.
       Applies to every on-hit proc + judgement payload (SoR/SoL/SoW on-hit, Seal of Command 932607).
     - **`ReconcileBaselineSpells` MUST have a re-entrancy guard** (static bool + RAII): it calls `learnSpell`,
       which fires `OnPlayerLearnSpell` → re-enters reconcile. Without the guard, a WotLK auto-grant loop
       recurses; with it, `kReconcileOnLearn` can safely list the stock ids for instant re-strip.
     - ~~**`StripOrphanedGrants` never touches these**~~ — **SUPERSEDED by Phase 9.5 (2026-09-06).**
       `StripOrphanedGrants` is retired; `EraBandClassifier::Sweep` now classifies EVERY held band id,
       including `helpers:` markers and trainer-taught ranks that are in no node's `grants:` manifest.
       Those stay legit only because they carry an `era-data/band-allowlist.yaml` entry (rule 3) or sit
       above a node-granted r1 in a `spell_ranks` chain (rule 2). Verifying "not in `era_talent_rank`" is
       no longer sufficient — check the allowlist.
  Values come from the 1.12.1 client `Spell.dbc` (canonical); e.g. Seal of Command had **5 ranks** and Seal of
  Righteousness **8** — do not trust "it was 1 rank" memory.
  **Coverage limit (2026-09-07):** the extract at `era-wow/tools/vanilla-extract/Spell.dbc` holds 16 332 records
  and stops at id 21184 — e.g. Insect Swarm 5570/24974-24977 are absent (5570 reads `zzOLDSpell - Reuse`). For any
  id above 21184 use wowhead Classic tooltips and say so in the YAML comment. Spec/plan:
  `docs/superpowers/specs/2026-08-20-paladin-vanilla-seal-reconstruction-design.md` +
  `docs/verification/era-talents-phase8-paladin-seals.md` (records all the real-client rounds).

- **Grant-accuracy: `grant when Vanilla-exact` is UNSAFE for stock ACTIVE abilities — audit two dimensions.**
  A stock spell that "exists with the right name" can still be WotLK-diverged. (1) **Hidden C++ coefficient:**
  a script-driven active (Bloodthirst, Mortal Strike, Shield Slam, Last Stand, Cold Snap, Combustion) computes
  its damage/heal/reset in `src/server/scripts/Spells/spell_*.cpp`, NOT the DBC — grep the script for the id.
  Bloodthirst granted stock 23881 = WotLK 50% AP / no heal / 1 rank vs Vanilla 45% / heal-on-swings / 4 ranks;
  the fix is a custom clone + a SpellScript (`era_bloodthirst_vanilla`) + a trainer rank chain. A cd-only diff
  on a script-bound active (Cold Snap/Combustion) is best ACCEPTED — a bare clone would lose the scripted
  behavior. (2) **Baseline leak** (above). Warrior-Bloodthirst was the only hidden-coefficient case across the
  six classes, but always check both before granting a stock active.
- **A reconcile-managed spell must have reconcile invoked from EVERY state change that affects it.**
  A spell granted by `ReconcileBaselineSpells` (not by a talent rank) — e.g. the VE proc-passive
  932950, the Holy Nova baseline gate — only tracks talent state at reconcile call sites. `TryLearn`
  and `Reset` call it now (in addition to the login/level/era-transition hooks), so a new
  reconcile-managed spell activates/deactivates on learn/reset/login/level/era-transition
  automatically. Never rely on "relog to apply": a mid-session VE learn once left the priest with no
  proc-carrier (932950 absent) and a heal that could never fire until relog.

- **RETIRED 2026-09-06 (Phase 9.5) — read the next bullet as history only.** `StripOrphanedGrants` and
  **every hand-written per-class cross-era band strip list** it was complementary to — including
  (`kMageTrainedChainsVanilla` / `kMageTrainedChainsTbc` / `kMageTbcBandSpells` / `kMageVanillaBandSpells` / `kMageStrandedAutoPassives`, `kEraPalTrained` /
  `kVanillaPalSealsSuperseded` / `kTbcPalSeals946`, `kHunterTrainedChains{,Tbc}` /
  `kVanillaScorpidRanks`, `kWarlockTrainedRanks` / `kWarlockDarkPactRanks` / `kSiphonLifeTrainedRanks` /
  `kWarlockTrainedChainsTbc` / `kVanillaStoneCreates` / `kTbcStoneCreates`, `kRogueTrainedRanks`,
  `kEraWarBt{Vanilla,Tbc}` / `kEraWarTbcChains` / `kEraWarSwapPairs`, and the priest/druid strip blocks)
  are **deleted**. One rule replaces all of them: `EraBandClassifier` (`src/EraBandClassifier.{h,cpp}`),
  whose `Sweep` runs LAST inside `ReconcileBaselineSpells` for **every** era — WotLK included, which is
  the gap `StripOrphanedGrants` structurally could not close (it was scoped to `NodesFor(currentEra)`,
  empty for WotLK, so a stale Vanilla/TBC clone on an L80 bot lingered forever). Legitimacy comes from
  three sources in order — node grant (a reverse index over EVERY era/class), `spell_ranks` chain member
  above a node-granted r1, `era-data/band-allowlist.yaml` entry — and anything else is stripped with a
  logged verdict (`ORPHAN_NODE` / `ORPHAN_CHAIN` / `ORPHAN_ALLOWLIST` INFO, `UNCLASSIFIED` WARN).
  `.eratalents doctor`'s ORPHAN loop and the new `.eratalents bandsweep <char>` call the same `Classify`,
  so the doctor and reconcile can never disagree. **Do not add a `removeSpell(<band id>)` to reconcile
  again** — add an allowlist entry (or fix the node/chain) instead.

- **Generic orphaned-grant strip (2026-08-18, SYSTEMIC) — a plain `grants:` spell that is neither a
  `spell_ranks` chain nor gate-managed had NO strip on respec.** `Reset()` only `removeSpell`s the single
  recorded top-rank spell per node, and the only login-time strips were the gate table + the explicit
  per-class trained-rank lists. So a plain single grant (Divine Favor 20216, Holy Shock 20473) or a
  multi-rank grant (Illumination 20210/20212/20213/20214/20215) that got out of sync ORPHANED — real-client
  Paladin pass found a respecced Vanilla paladin holding ALL FIVE Illumination ranks at once (which also made
  its refund proc fire off the wrong rank) plus Divine Favor / Holy Shock / GBoK with **no** backing
  `era_character_talent` rows. Fix = `EraTalents::StripOrphanedGrants(p, era)`, called LAST inside
  `ReconcileBaselineSpells` (so it runs from every reconcile site: login / level / era-transition / TryLearn /
  Reset). It is **CLASS-SAFE by construction**: the only ids it may strip are the module's OWN grant manifest
  for the char's CURRENT (class, era) — `grantable` = every `era_talent_rank.grantedSpellId` for that
  class+era; `legit` = for each node the char has a rank in, ONLY that rank's spell (`rankSpell[rank-1]`);
  strip `s` iff `s ∈ grantable ∧ s ∉ legit ∧ HasSpell(s)`. It therefore can never touch a spell the module
  doesn't grant (a trained/quested/leveled stock spell is not in any node's `rankSpell[]`), never touches
  trainer-taught higher ranks (not in `era_talent_rank`), and never undoes reconcile's own force-grants (the
  VE proc 932950, warlock marker 932930, SotC seals 932634-639, WotLK gate grants — verified NONE are node
  grants). It is complementary to the gate/trained-rank strips, not a replacement (idempotent overlap via the
  `HasSpell` guard). **The one case it MUST NOT run on is a character that also uses the CORE talent system**
  (a playerbot with factory talents has stock Divine Favor/Holy Shock with no era row — running reconcile on
  it strips them): this is safe in production because bots are `IsBot`-guarded out of every reconcile hook and
  a human era char has core points pinned to 0, but it means a GM `.eratalents reset` on a *bot* will disable
  that bot's factory grantable spells until its next playerbots factory re-init. If you author a Vanilla talent
  that grants a stock spell ALSO obtainable another way for a Vanilla char, gate the other source (delete the
  trainer row, as the baseline-leak gates do) so the strip stays correct — err toward under-stripping.

- **Greater Blessing of Kings (25898) is a SEPARATE stock chain from Blessing of Kings 20217.** GBoK is a real
  WotLK baseline trainer spell (SpellLevel 60, aura 137 = +10% all stats) but its `trainer_spell` rows
  (paladin TrainerIds 3/4/5) are ungated (ReqAbility1=0), so a Vanilla paladin could train it — the same leak
  as 20217, which the first paladin `_32` gate missed. In Vanilla, GBoK did NOT exist as a trainable baseline
  (reagent/talent-gated raid version). Gated identically to 20217: delete the trainer rows
  (`2026_08_18_32_era_talent_paladin_baseline_gate_trainers.sql`) + a `kBaselineSpellGates` row. It needs its
  OWN row `{ CLASS_PALADIN, 18620, { 25898 }, 60, true }` (NOT merged into the L20 20217 row) because the gate
  force-grant path only grants `rankSpells[0]` at `minLevel`, and GBoK's real learn level is 60 — folding it
  into the L20 row would grant only 20217 and silently drop GBoK for WotLK paladins. 25898 is NOT a node grant,
  so the generic strip never touches it; the gate solely owns it. Also added to `kReconcileOnLearn`.

### Pet-affecting talents (warlock final pass, 2026-08-16 — read before ANY pet/summon talent)

All four items below were live-diagnosed on the dev stack (doctor instrumentation + a real-client
pass); each one failed SILENTLY with perfect-looking rows, so do not re-derive them from data
inspection.

- **A talent that modifies a PET'S OWN SPELL (cast time, cooldown, effect magnitude) is a plain
  owner-side SPELLMOD — never a script.** A pet's cast resolves the OWNER's spellmods:
  `GetSpellModOwner()` is consulted by `SpellEffectInfo::CalcValue`, `SpellInfo::CalcCastTime`
  and `Creature::AddSpellCooldown` (this also fixes the client display for free). Stock proof:
  Demonic Power 18126 = castTime mod masked to Firebolt (4096) + cooldown mod masked to Lash of
  Pain (8192). Pet abilities are family-masked like player spells (all four Voidwalker abilities
  share one bit, 33554432) — dump them and add `_ref` entries like any other spell.
- **A `mechanic: pet` (spell_pet_auras) talent passive MUST be `APPLY_AURA(SPELL_AURA_DUMMY)`
  with ImplicitTargetA=1 — the stock 23785 shape.** The generator emits this now; the original
  bare `SPELL_EFFECT_DUMMY` emission passed LoadPetAuras validation but NEVER registered the
  PetAura at learn-cast (`m_petAuras` stayed empty — the "all summon talents do nothing" bug).
  The dummy-AURA shape registers symmetrically in `AuraEffect::HandleAuraDummy` (AddPetAura on
  apply, RemovePetAura on remove/unlearn).
- **"While demon X is active, both the warlock and the demon get Y" = an area-aura row CAST BY
  THE PET ON ITSELF.** `SPELL_EFFECT_APPLY_AREA_AURA_PET` (119) fills targets = the aura holder
  + its owner; `..._OWNER` (143) = owner only; both need an `EffectRadiusIndex` (stock MD buffs
  use index 12 = 100 yd). Because the aura LIVES ON THE PET it auto-strips from the owner when
  the demon is dismissed/dies/is sacrificed — never hand-cast such buffs on the warlock (the
  original Master Demonologist design did, and the buff lingered on a petless warlock).
- **A pet cast targeted AT its owner (`pet->CastCustomSpell(owner, id, ...)` with a plain
  APPLY_AURA / targetA 21) silently never applies the aura.** Diagnosed on Soul Link: buff up,
  pet out, both script paths ran, carrier absent. Route pet→owner auras through a 143 area-aura
  self-cast instead (`pet->CastCustomSpell(pet, ...)` — the proven MD_Apply shape). This is also
  the ONLY way to give the owner an aura whose CASTER is the pet — which SPLIT_DAMAGE_PCT
  requires (Unit.cpp routes the victim's split damage to the aura's caster, skipping
  caster==victim).
- **A second pet-using class needs its OWN `petBuffBase:`.** `_pet_node_ordinals` restarts at 0
  per dataset, so `pet_buff_id`'s default `PET_BUFF_BASE` (928000) would derive the SAME auto
  pet-buff ids for two different classes' pet nodes (ordinal 0 -> 928000 either way) — a silent
  cross-class collision. Hunter uses the top-level dataset key `petBuffBase: 929000` to relocate
  its entire auto pet-buff band; Warlock is unchanged and keeps the 928000 default (pet-buff ids
  aren't persisted in character data — they're re-derived every login — so introducing the key is
  churn-free). Lint hard-errors if a dataset's `petBuffBase` is below `PET_BUFF_BASE` or if its
  worst-case demand (`n_pet_nodes * PET_BUFF_BLOCK`) would spill into the 932900+ hand-helper
  region — pick a fresh base with headroom for any future third pet class.
- **`petBuffIds:` is the escape hatch for a pet-buff shape the auto `_pet_buff_row` can't
  express** (e.g. a 119 `APPLY_AREA_AURA_PET` carrier, rather than a plain stat/spellmod aura on
  the pet itself). List one hand-authored `helpers:` id per rank on the `pet:` node instead of
  `petAura`/`petVariants` (mutually exclusive — lint rejects mixing them); the generator then
  skips the auto pet-buff emission entirely and points each rank's `spell_pet_auras` row straight
  at the listed helper id. Lint also verifies every listed id actually exists under the dataset's
  `helpers:` — an id that doesn't resolve there is a silent no-op at runtime, not a hard error, so
  the check catches the typo at generation time instead.

### Verification harness for pet/era talents (no real client needed)

- **`.eratalents doctor <char>` now prints the whole chain**: band DUMMY markers (id/misc/amount),
  every band aura ON the player (amount, caster, applied), `m_petAuras` registrations with the
  buff id mapped for the current pet entry, and the live pet's stats + band auras (owned AND
  applied — an owned-but-unapplied aura runs no handlers; note a 119/143 area row on the pet
  correctly shows applied=NO on the pet itself when its effect targets the owner).
- **Playerbots double as era test characters**: a bot's era comes from IP quest progression, and
  `.ip set <bot> N` (N>=1 — 0 is a silent no-op in `ForceUpdateProgressionState`) makes it
  Vanilla-era so `.eratalents learn` works on it from the console. Warlock bots keep demons
  summoned, giving a live pet to verify against. Bot quest/spell state does NOT survive a
  worldserver restart — redo the `.ip set` after every restart, and clean up
  `era_character_talent` rows for the bot when done.

### Talent-panel tooltip for spell-granting nodes (the addon renders the REAL spell)

- **A node with a `grants:` key shows the granted spell's own client tooltip in the talent panel,
  NOT its authored `tooltip:` prose.** The generator emits a per-rank `grant` map (`grant={ [rank]=
  spellId }` = the FIRST spell each rank grants) into the addon data, and `TalentUI.lua`'s
  `NodeTooltip` calls `GameTooltip:SetHyperlink("spell:"..id)` for those nodes — so the client
  renders the proper header (cost / range / cast / cooldown as structured lines) plus the effect
  description from the spell's own `Spell.dbc` row, and appends a `Rank X/Y` line. A clone ships in
  `patch-V.mpq`, a stock grant in the base DBC, so the hyperlink resolves either even before the
  talent is learned. This is why a grant node's `tooltip:` prose must NOT cram the mechanical header
  (`15 Rage 5 yd range Instant 45 sec cooldown Stuns…`) — the panel no longer shows that string; the
  granted spell's DBC row is the single source. Passive/modifier nodes (spellmod/stat/proc, no
  explicit `grants:`) keep the authored per-rank prose path. **Consequence:** for a grant node the
  granted spell IS the panel tooltip, so an accepted-divergence STOCK grant (e.g. Insect Swarm 5570)
  shows the stock WotLK numbers there — clone it if the panel must read era-exact.

### Granted STOCK spells: tooltips and spellbook tabs

- **A granted stock spell's CLIENT tooltip is global and immutable — if the WotLK text diverges
  from the era behavior, grant a custom clone instead.** Demonic Sacrifice (18788) showed the
  WotLK rework text over our Vanilla benefits; the node now grants clone 932929 with its own
  client row. Add a reconcile strip for the previously-granted stock id (characters keep known
  spells across a grant change; `ReapplyOnLogin` only adds).
- **A granted stock spell with no SkillLineAbility row lands in the client's GENERAL tab** (the
  stock game tabbed it via the talent frame, which our grants bypass). Dataset key
  `extraSkillLines: [{spell, skillLine}]` emits the client row — but prefer the custom-clone
  route above when the tooltip is wrong anyway (a clone's `client.skillLine` covers the tab).
- **An era-restored base cast time (core patch, e.g. Corruption/patch 0019) needs the dataset key
  `clientCastTimeBase: {SpellName: ms}`** — the client row still says Instant, and the addon's
  `Tooltip.lua` `RewriteCastBase` rewrites the cast line from this base + learned mods at every
  rank (including rank 0, which plain spellMods rewriting can't reach).

### Reconstructing removed Vanilla consumables (the Firestone/Spellstone pattern)

- 1.12 items often SURVIVE in `item_template` (flagged "(DEPRECATED)") while every spell they
  referenced was deleted from the 3.3.5 DBC — check `item_template` before scoping out an
  item-based talent. Rebuild: custom band spells for the item's use/equip effects (+`client:`
  rows for the Equip:/Use: tooltip lines), retarget the item's `spellid_N` columns by module SQL
  migration, and clone the stock create-spell with `itemType:` overriding `EffectItemType`.
- **Era-gate trainer teaching with `ReqAbility1 = 932930`** (the "Era: Vanilla Cast Times" marker
  every Vanilla-era warlock carries and nobody else does) — trainer_spell can't express eras, but
  a known-spell requirement can proxy one. Reconcile must strip BOTH ways on era transition (the
  custom creates when leaving the era, the stock WotLK creates while in it), plus an
  `OnPlayerLearnSpell` hook so a mistake purchase of the stock spell is stripped on the spot.
- Clients cache item names in `Cache/WDB` — after renaming a deprecated item, players must delete
  that folder once or they keep seeing the old name.

### Custom castable spells (stock actives whose WotLK form diverged)

- **A stock ACTIVE talent whose WotLK form diverged from Vanilla can't be fixed by overriding the
  stock id.** `spell_dbc` and the client `Spell.dbc` are **global** — overriding, say, Vampiric
  Embrace (15286) or Power Infusion (10060) to behave like their Vanilla form also changes the spell
  for WotLK-stage players, and a granted active's client tooltip is only ever correct via a
  per-spell client row, which is likewise global. There is no per-era override for an existing id.
  - Priest **Vampiric Embrace** became a self-buff in WotLK; Vanilla's was party-healing off the
    caster's Shadow damage. Priest **Power Infusion** became haste+mana in WotLK; Vanilla's was flat
    +20% spell damage/healing. Neither divergence is expressible by editing 15286/10060 in place.
  - **The era-safe fix: author a custom Vanilla-only castable in the reserved band and have the
    talent grant it instead of the stock id.** The stock id ships untouched for higher eras; the
    custom id only exists (and is only trainable/grantable) at the era that needs the old behavior.
- **The generator's `helpers:` form supports player-castable, multi-effect spells**, not just passive
  auras: `powerCost` / `rangeIndex` / `castTimeIndex` / `cooldown` plus an `effects:` list (per-effect
  `effect` / `aura` / `basePoints` / `dieSides` / `misc` / `miscB` / `trigger` / `targetA` / `radius` /
  `itemType`) and an optional `client:` block for the tooltip/icon. A helper may also carry a `proc:`
  block, which emits a `spell_proc` row for it just like a talent-node aura.
  - **Per-effect spell-class mask (`maskA` / `maskB` / `maskC`):** an effect entry may carry these three
    keys = the effect's family words 0/1/2 (the SAME meaning as a node's `affectsMask {a,b,c}`), i.e. the
    set of spells THIS effect's spellmod affects. Column convention (see `MASK_PREFIX` + the node emitter
    `_spellmod_row`): the LETTER is the effect index (A=Effect_1, B=Effect_2, C=Effect_3) and the
    `_1/_2/_3` suffix is the word index — so for effect `i` the words go to `{MASK_PREFIX[i]}_1/_2/_3`
    (Effect_1 → `EffectSpellClassMaskA_1/_2/_3`, Effect_2 → `...B_1/_2/_3`, Effect_3 → `...C_1/_2/_3`).
    Do NOT confuse the axes: an all-zero effect flag96 means "affects the WHOLE family" in
    `SpellInfo::IsAffected`, so a transposed write silently over-binds. They are written **only when
    present**, so a templated helper keeps INHERITING the cloned record's per-effect masks for any effect
    that omits them (purely additive). This is a DIFFERENT concept from the helper-level `mask`/`maskB`/
    `maskC` scalars, which set the spell's OWN family identity (`SpellClassMask_1/2/3` = SpellFamilyFlags,
    a whole-spell property that drives `affects:` binding), not which spells any effect's spellmod hits.
    Per-effect masks let ONE helper carry two spellmod effects each bound to a distinct spell set — which
    the whole-spell scalars cannot express. First use: the Vanilla Elemental Mastery 932413 (Shaman) —
    Effect_1 FLAT crit + Effect_2 PCT cost, both authored `maskA:-1877999613 maskB:0 maskC:0` = the
    fire/frost/nature damage set, collapsing what used to be two separate buffs (932413 + 932414) into
    one. Tests: `test_helper_effect_per_effect_class_mask` (keys land in the columns) +
    `test_helper_effect_without_mask_keys_does_not_emit_columns` (inheritance preserved). Both the server
    `spell_dbc` SQL and the client `Spell.dbc` pick these up from the single `helper_overrides` source
    (`build_client_dbc.py` applies column overrides generically), so one edit covers both paths.
  **Copy the stock analog's Range/CastTime/Duration indices** via
  `tools/dump_spell_effects.py` rather than guessing — these are DBC row-index lookups, not raw
  numbers, and a wrong index silently produces a spell with the wrong range or cast time. Its
  `DEFAULT_COLS` only includes `DurationIndex`; pass `--cols` explicitly to also pull
  `RangeIndex`/`CastingTimeIndex`.
- **Debuff-anchored heal pattern** (used for Vampiric Embrace): cast on the enemy →
  Effect_1 applies a **visible** DUMMY debuff on the mob (needs a `client:` block — it's the icon the
  target/raid sees), Effect_2 is `TRIGGER_SPELL` and puts a **hidden** proc-buff on the caster (no
  client row — it's never meant to be seen). The proc-buff's C++ `AuraScript`, in `CheckProc`, gates
  on `victim->HasAura(debuffId, caster->GetGUID())` so only Shadow damage landed on a mob carrying
  **this caster's own** debuff triggers the heal — not any Vampiric Embrace debuff from any caster.
- **Party vs raid scope: iterate with `group->SameSubGroup(caster, member)`, never bare
  `GetFirstMember()`/full-group iteration.** `GetFirstMember()` walks the entire `Group`, which spans
  up to a 40-man raid — a real bug caught in review turned a party-only heal into a raid-wide heal
  with up to 40 casts per channel/proc tick. Vanilla's Vampiric Embrace is party-scoped; scope every
  future group-heal AuraScript the same deliberate way, don't default to "whole group."

### Process

- **Headless proves STRUCTURE (rows exist, masks intersect, family = correct); it cannot prove
  RUNTIME behavior.** Procs firing, debuffs stacking, heals landing, tooltips rewriting, icons
  rendering, trainer gating — all are real-client-only checks. Budget a real-client pass per class and
  expect a round or two of these gotchas. When a fix can't be reproduced headlessly (no era char of
  that class on the dev box), say so and hand the live re-test to the user.

### Cleanup debt (non-blocking, revisit when next touching these files)

- **`modules/mod-era-talents/src/EraTalentProcScripts.cpp` has ~5 near-identical band-DUMMY-marker
  reader loops:** `era_ward_reflect`, `era_vampiric_embrace`, `era_improved_drain_mana`,
  `era_amplify_curse`, and `era_rog_sap_stealth` each hand-roll the same "walk the caster's auras,
  find the SPELL_AURA_DUMMY with this band's reserved misc value, read its amount" pattern. Extract
  a single `BandMarkerAmount(Unit*, int32 misc)` helper the next time any of these five is touched —
  not urgent enough to justify a standalone pass today.
- **Rogue's dataset (`era-data/vanilla/rogue.yaml`) uses a different authoring-comment style than
  mage/priest/warlock/hunter's** (placement/verbosity differ; generated SQL/Lua output is
  byte-identical either way — see the rogue verification record's cosmetic-items list). A
  cross-class comment-style consistency pass is optional, not a defect.
