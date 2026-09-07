# Era Talents — TBC Phase 8 (Warlock) verification record

> **Generation superseded (2026-09-06 final review):** the current stamp is the closing row of docs/verification/era-talents-final-review-tbc-vanilla.md §10 — reinstall patch-V.mpq + restage the addon before any real-client pass.

**Status: COMPLETE — real-client SIGNED OFF by the user 2026-09-05 (gen `4ec433f1`, zero findings reported).**
Generation stamp **`4ec433f1`** (`era_talent_meta` `generation` = `4ec433f1`; the same stamp is in
`client-addons/_data-patches/patch-V.mpq`). Branch `feat/era-talents`, merged to master 2026-09-07.

Everything below was measured on the dev-box stack (local Docker: `ac-db-import` + a
`--force-recreate`d `ac-worldserver` built from this branch) with the **TBC allowlist flipped on**
(`modules/mod-era-talents/src/EraTalentIP.cpp:21` reads `return era == ERA_VANILLA || era ==
ERA_TBC;`). That flip is a **deliberately UNCOMMITTED dev-box edit** — it is what makes a
level-61-70 character read `era=TBC` and get the custom window, and it is never staged. Every
readiness-gated arm in the module (`EraReplacementSpellReady`, `EraNodeIsWired`) is inert without
it, which is exactly the pre-Phase-8 behaviour a shipped build has.

Task commits: `9268b10` (Task 1 skeleton + `_ref`), `92dc999` (Affliction), `96ff310`
(Demonology), `84012e6` (Destruction + stone substrate), `b74cb50` (Task 5 wiring), plus this
task's fix and docs commits.

Spec: `docs/superpowers/specs/2026-09-05-era-talents-tbc-phase8-warlock-content-design.md`
(+ Amendments A.0, A, B). Authoring input: `era-data/_ref/tbc/warlock-spells.yaml`.

---

## 1. What shipped

- **64 TBC warlock talent nodes**, ids `20700-20763` (Affliction 21 / Demonology 22 /
  Destruction 21), all wired to ranks.
- **112 generated auto-passives** in `941600-942100` (the class's auto-passive window is
  `[941600, 942112)`).
- **65 hand helpers** in the claimed slice `948410-948749` (exact ids in §9).
- **Zero core patches.** Patch 0019 (Corruption era cast time) is REUSED unchanged — TBC
  Corruption is a 2 s cast at every rank, so the marker it keys on now covers Vanilla *and* TBC.
- **Two reconstructed TBC-only items** — Master Firestone `22128` and Master Spellstone `22646` —
  re-pointed onto band spells `948430-948433`.
- **Three module scripts changed / added:** `era_spellstone_crit_rating` (NEW),
  and `era_amplify_curse` / `era_improved_drain_soul` / `era_demonic_sacrifice` /
  `era_soul_link` + `era_soul_link_relink` / `era_master_demonologist` all made era-table-driven;
  the shared `(buff, split)` table lives in `modules/mod-era-talents/src/EraSoulLink.h`.
- **Three Vanilla fixes bundled** (§11).
- **No DUMMY-marker misc value consumed** (next free stays **19**).
- **`petBuffBase: 945000` claimed and EMPTY** — see §12.

---

## 2. Per-tab node counts and disposition tallies

Counted from `era-data/_ref/tbc/warlock-spells.yaml` `disposition_table:` (64 TBC rows + 50
Vanilla re-audit rows, which are excluded from the tallies below).

| Tab | Nodes | GRANT | AUTHOR | CLONE | RECONSTRUCT |
|---|---|---|---|---|---|
| Affliction (20700-20720) | 21 | 9 | 10 | 1 | 1 |
| Demonology (20721-20742) | 22 | 9 | 10 | 3 | 0 |
| Destruction (20743-20763) | 21 | 6 | 13 | 2 | 0 |
| **Total** | **64** | **24** | **33** | **6** | **1** |

Matches spec Amendment A exactly (24 GRANT / 33 AUTHOR / 6 CLONE / 1 RECONSTRUCT). The 50-row
Vanilla re-audit block resolved 49 `VANILLA_NO_CHANGE` + 1 actionable row (ISB, §11).

DB confirmation:

```
SELECT eraId, classId, COUNT(*) FROM era_talent GROUP BY eraId, classId;
  ...
  1   9   64
SELECT COUNT(*) AS wired FROM era_talent t WHERE t.eraId=1 AND t.classId=9
  AND EXISTS (SELECT 1 FROM era_talent_rank r WHERE r.talentId=t.id);
  64
```

---

## 3. Fourteen-family survival-map verdict distribution

403 ids swept. Of the **184 rank spells**:

| Verdict | Count |
|---|---|
| DIVERGED | 95 |
| LIVE_MATCH | 62 |
| WAGO_ONLY | 26 |
| LIVE_MATCH_UNVERIFIED_SLOT | 1 |

The single largest contributor to DIVERGED is the WotLK flat-`ManaCost` → `ManaCostPct`
conversion, which is an era-wide core data change and not a per-node finding (accepted gap 3).

---

## 4. The six talent-rooted chains — resolutions, levels and costs

`orphaned_higher_rank_check` result: **NOTHING IS DELETED, so nothing is orphaned.** Every stock
`trainer_spell` row this phase could have touched is either already `932939`-gated or self-gating
on its own talent id. The only trainer WRITES are ADDITIONS plus one UPDATE (re-gating the seven
Vanilla Create rows from `932930` to `932994`).

| Chain | Node | Resolution | Ids | Trainer (level / cost / `ReqAbility1`) |
|---|---|---|---|---|
| Siphon Life | 20713 | **RECONSTRUCT** — all six TBC ranks are WAGO_ONLY; 18265 does not exist live at all | r1 `948451` (node grant) + `948452-948456` | 38 / 10000c ← 948451; 48 / 14000c; 58 / 20000c; 63 / 2500c; 70 / 2500c |
| Conflagrate | 20760 | **CLONE ×6** of 17962 (e2/e3 left ZERO so the %-of-Immolate bonus computes 0 and the `TargetAuraState 14` consume survives with no script) | r1 `948650` + `948651-948655` | 48 / 16000c ← 948650; 54 / 22000c; 60 / 30000c; 65 / 2500c; 70 / 2500c |
| Unstable Affliction | 20720 | **CLONE ×3** (TBC 18 s / 660 total, 1.5 s cast, 270 flat mana) | r1 `948470` + `948471/948472` | 60 / 2500c ← 948470; 70 / 2500c |
| Shadowfury | 20763 | **CLONE ×3** (0.5 s cast, 2 s stun, 440 flat mana) | r1 `948670` + `948671/948672` | 60 / 2500c ← 948670; 70 / 2500c |
| Shadowburn | 20750 | **GRANT stock 17877** — the whole stock chain 18867-30546 self-gates on 17877 | stock | untouched (L24 148c … L70 3900c) |
| Dark Pact | 20717 | **GRANT stock 18220** — TBC's own drain IS 305, byte-identical to live. **This REVERSES the Vanilla clone decision** (1.12 drained 150) | stock | untouched (L50 748c, L60 1300c, L70 1300c) |

Live `trainer_spell` state for the new rows (TrainerId 31), verified post-import:

```
SpellId ReqLevel MoneyCost ReqAbility1
948420  28  5000  948410      948425  36   9000  948410
948421  36  9000  948420      948426  48  14000  948425
948422  46 13000  948421      948427  60  26000  948426
948423  56 22000  948422      948428  66  51000  948427
948424  66 51000  948423
948452  38 10000  948451      948651  48  16000  948650
948453  48 14000  948452      948652  54  22000  948651
948454  58 20000  948453      948653  60  30000  948652
948455  63  2500  948454      948654  65   2500  948653
948456  70  2500  948455      948655  70   2500  948654
948471  60  2500  948470      948671  60   2500  948670
948472  70  2500  948471      948672  70   2500  948671
```

The Vanilla Create rows now read `ReqAbility1 = 932994` and the TBC Create rows `948410`:

```
932922 28 932994 0 | 932923 36 932922 932994 | 932924 46 932923 932994 | 932925 56 932924 932994
932926 31 932994 0 | 932927 43 932926 932994 | 932928 55 932927 932994
948420 28 948410 0 | 948421 36 948420 948410 | 948422 46 948421 948410 | 948423 56 948422 948410
948424 66 948423 948410 | 948425 36 948410 0 | 948426 48 948425 948410 | 948427 60 948426 948410
948428 66 948427 948410
```

**Trainer-cost curve note:** the Create-stone and the r2-r4 clone rows are priced off the *general
spell* curve (5000/9000/13000/22000/51000 …) because that is what the stock stone rows and this
repo's shipped Vanilla clone rows use; the TBC-era top ranks (63/65/70) are priced off the
*talent-rooted* curve (2500c), matching the stock UA/Shadowfury rows they sit beside.

---

## 5. Censuses (from `era-data/_ref/tbc/warlock-spells.yaml`)

- **`spell_proc` census** (`proc_data_census:`): 17 relevant rows. Key findings that bound
  authoring: **a clone inherits NO `spell_proc` row** (so the Fel Domination clone `948570` needed
  the 18708-shaped row re-written, and every charged reconstruction needed a consume row); and for
  **Pyroclasm / Aftermath / Shadow Embrace the LIVE row's mask is the WRONG SPELL SET** (all three
  are the WotLK rework), so each AUTHORed node writes its own row. *NB Amendment B.4: the ref's
  `proc_data_census:` transcription is shifted one column for at least 18708 and 17941 — read
  `spell_proc` rows from the live DB, never from the transcription.*
- **Script census** (`script_census:`, 31 `spell_script_names` rows): the core scripts that matter
  are `spell_warl_unstable_affliction` (keys on **nothing** id-specific → clone works once bound),
  `spell_warl_shadowburn` (bound to the *carrier* 29341, not the talent → moot, Shadowburn is a
  GRANT), `spell_warl_nether_protection` (hardcodes the **WotLK** payloads 54370-54375 → granting
  the stock talent would deliver the WotLK mechanic, so the node is AUTHORed) and
  `spell_warl_improved_drain_soul` (keyed **by ranked id** on 18213 → the mana half needs a module
  script). No core script exists for Master Demonologist, Soul Link, Demonic Sacrifice, Fel
  Domination, Mana Feed, Backlash, ISB, Pyroclasm, Aftermath, Demonic Knowledge, Demonic Tactics
  or Dark Pact.
- **Pet-aura census** (`pet_aura_census:`): all 17 demon pet-passive carriers (Felguard 30147/
  30148/30149 included) are LIVE_MATCH — which is why the pet window stays empty (§12).

---

## 6. Core-hardcode table — the six questions (a)-(f)

| # | Question | Where | Keys on | Consequence |
|---|---|---|---|---|
| (a) | Conflagrate's Immolate consume | `SpellEffects.cpp:386-430` (SPELLFAMILY_WARLOCK arm of SCHOOL_DAMAGE) | `m_spellInfo->TargetAuraState == AURA_STATE_CONFLAGRATE` — a **DATA field**, not an id | Clone freely: template 17962 inherits the state, and leaving e2/e3 ZERO makes the %-bonus compute 0. Consume + requirement survive with no script. |
| (b) | Improved Healthstone lookup | `spell_warlock.cpp:601-669` | two-stage: finds the aura by family + **SpellIconID 284** + effect 0, then `switch`es on the **literal ids 18692 / 18693** | **Node 20721 MUST GRANT 18692/18693.** A band clone would be found by the icon and then produce the un-improved stone plus an error-log line per cast. |
| (c) | Unstable Affliction dispel backlash | `spell_warlock.cpp:1024-1049` | **nothing id-specific** — reads its own EFFECT_0 base × 9, casts the hardcoded 31117 | Clone freely; bind the script to the three clone ids. TBC 110×9 = 990 reproduces the tooltip exactly. |
| (d) | Shadowburn shard refund | `spell_warlock.cpp:1265-1283`, bound to the **positive** id 29341 | `SPELL_AURA_CHANNEL_DEATH_ITEM` at EFFECT_0 of the carrier — neither family nor the Shadowburn id | Moot — Shadowburn is a GRANT. |
| (e) | Proc payload reach (lesson 23) | `spell_proc` + talent effects | **Soul Leech / Nightfall / Aftermath / ISB**: MASK for the trigger set, **ID for the payload**. **Backlash**: neither (HitMask 1027 + 8 s ICD). **Pyroclasm / Shadow Embrace**: TBC used `OVERRIDE_CLASS_SCRIPTS` misc 2188/2189 and 4994, and **3.3.5a implements neither** | Soul Leech / Nightfall / Backlash GRANT safely. ISB / Pyroclasm / Aftermath / Shadow Embrace are AUTHOR + reconstruct + own proc row. |
| (f) | Does a spellbook-learned stock passive register its `spell_pet_auras` rows? | `Pet::CastPetAuras` (Pet.cpp:2354), `AuraEffect::HandleAuraDummy`, `Unit::AddPetAura` | nothing in the path consults `Talent.dbc` / `HasTalent` / the talent frame | **YES** — an era-granted stock passive registers exactly like a talented one and survives dismiss+resummon. The row is keyed `(spellId, effectId)` and fires only if that effect is a DUMMY. |

Two more that bound authoring: **Mana Feed** is found by family + **SpellIconID 1982** + `aura
107` at **effect index 0** (the one place in this class where an icon is load-bearing — helpers
`948571-948573` carry it), and **Soul Siphon** is implemented by its own `MiscValue` 4992/4993, so
an AUTHORed passive gets the exact core mechanism.

---

## 7. Reuse ledger — outcome: ALL FRESH / MOOT

Every Vanilla helper a TBC node overlaps was compared field-by-field across all fourteen families
plus mana and attributes. **Nothing is reused outright.** Two entries were recorded
REUSE-ELIGIBLE with the evidence (ISB debuffs 932940-944 → 948660-664, and Pyroclasm stun 932945 →
948666); Task 4 took the spec's default and shipped **FRESH** ids for both, which the live id list
in §9 confirms. One entry is FRESH-but-UNUSED (Vanilla Dark Pact clone chain — TBC GRANTs stock),
and one is "SHAPE reused, IDS FRESH" (the Create-stone clones, where the per-era trainer gate is
the entire point).

---

## 8. The four-marker chain, and the readiness fallback

The old two-marker Vanilla/non-Vanilla split became four markers, each a hidden passive **with a
`client:` block** (the Blizzard trainer UI formats its "Requires …" line off the `ReqAbility`
spell's NAME and throws a Lua error on a nameless one — the original stone-trainer crash):

| Marker | Name | Job | Wanted when |
|---|---|---|---|
| `932930` | server name kept **"Era: Vanilla Cast Times"**; CLIENT row renamed to **"Pre-Wrath Warlock"** (a TBC warlock carries it too) | arms **core patch 0019** (Corruption 2 s cast). **No longer a trainer gate.** | warlock AND (Vanilla OR (TBC AND TBC ready)) |
| `932994` | "Era: Vanilla Warlock" (client label **"Vanilla Warlock"**) | trainer gate for the Vanilla Create clones `932922-932928` | warlock AND Vanilla AND Vanilla ready |
| `948410` | "Era: TBC Warlock" (client label **"TBC Warlock"**) | trainer gate for the TBC Create clones `948420-948428` | warlock AND TBC AND TBC ready |
| `932939` | "Era: WotLK Warlock" | gate for the stock WotLK-item Create rows. **NARROWED** from `era != ERA_VANILLA` to WotLK-only | warlock AND (WotLK OR (TBC AND NOT TBC ready)) |

**The readiness fallback is load-bearing and EVERY arm takes it** (`EraTalents.cpp` ~:641,
`bool const tbcWarlockReady = EraReplacementSpellReady(ERA_TBC, 948410);`, which itself requires
`EraHasTalentTrees(ERA_TBC)`). This task's fix (commit below) changed the `932930` arm from
`era != ERA_WOTLK` to `era == ERA_VANILLA || (era == ERA_TBC && tbcWarlockReady)`.

*Why:* on a build **without** the TBC allowlist — the committed state — a TBC warlock must land in
the exact pre-Phase-8 state, which is `932939` + **no** `932930`. The old arm granted `932930` to
such a character, arming patch 0019's 2 s Corruption while the character is playing the *native
WotLK* tree, whose Improved Corruption has no cast-time reduction at all — i.e. a strictly slower
Corruption with no talent able to bring it back down. The stone arms already had this fallback;
the cast-time arm did not. Generalised as workflow **lesson 25**.

The `2026_09_07_02_era_talent_tbc_warlock_markers_stones.sql` migration also re-gates the seven
Vanilla Create rows from `932930` to `932994` in place — **nothing is deleted** (see the
"deleting a stock trainer row orphans higher ranks" memory).

---

## 9. Helper id table — 65 ids in `948410-948749`, with three holes

Verified against `SELECT ID FROM spell_dbc WHERE ID BETWEEN 948410 AND 948749 ORDER BY ID`
(count = 65, and the ranges below are that query's output, not the plan's pins):

| Partition | Used | Holes | What |
|---|---|---|---|
| Substrate 948410-948449 | `948410`; `948420-948428`; `948430-948433` | 948411-419, 948429, 948434-449 | "Era: TBC Warlock" marker; nine Create-stone clones (Firestone Lesser/-/Greater/Major/**Master**, Spellstone -/Greater/Major/**Master**); Master Firestone equip 948430 + melee proc 948431; Master Spellstone use 948432 + equip (+20 spell crit **rating**) 948433 |
| Affliction 948450-948549 | `948450-948458`; `948460-948464`; `948470-948472` | **948459**, 948465-469, 948473-948549 | Amplify Curse active 948450; Siphon Life r1-r6 948451-948456; Improved Drain Soul threat/mana 948457/948458; Shadow Embrace debuffs 948460-948464; UA clone chain 948470-948472 |
| Demonology 948550-948649 | `948550-948557`; `948560-948564`; `948570-948573` | **948558/948559**, 948565-569, 948574-948649 | Soul Link visible buff 948550 + pet-cast split carrier 948551; Demonic Sacrifice clone 948552 + five per-demon buffs 948553-948557 (incl. the **Felguard** arm); Master Demonologist carriers 948560-948564 (incl. Felguard 17252); Fel Domination clone 948570 (15 min CD + its own `spell_proc` row); Mana Feed helpers 948571-948573 |
| Destruction 948650-948749 | `948650-948655`; `948660-948667`; `948670-948672` | 948656-659, 948668/948669, 948673-948749 | Conflagrate clone chain 948650-948655; ISB debuffs 948660-948664 (`procCharges: 4`); Aftermath daze 948665; Pyroclasm stun 948666; Nether Protection immunity 948667; Shadowfury clone chain 948670-948672 |

Free within the slice: everything listed under "Holes" plus **948673-948749**. Guard queries both
returned zero rows:

```
-- family guard (nothing emitted with the wrong SpellClassSet)
SELECT ID, Name_Lang_enUS FROM spell_dbc
 WHERE (ID BETWEEN 941600 AND 942111 OR ID BETWEEN 945000 AND 945999
        OR ID BETWEEN 948410 AND 948749) AND SpellClassSet NOT IN (0, 5);      -- 0 rows
-- nothing emitted outside the claimed slice
SELECT ID FROM spell_dbc WHERE ID BETWEEN 948750 AND 949999;                    -- 0 rows
```

The family guard is the Amendment B.1 check: the `_ref` originally shipped without
`familyName: 5`, and the generator/audit default that key to **3 (mage)**, so every warlock
spellmod would have carried `SpellClassSet 3` and bound nothing. **Standing check for mage
(Phase 9): the `_ref` must carry `familyName:`.**

---

## 10. The stone substrate, and accepted gap 1 (both raw value sets)

Three Create-stone sets now exist, each gated by its own era marker, each era stripping the other
two (`kVanillaStoneCreates` / `kTbcStoneCreates` / `kStockStoneCreates` in `EraTalents.cpp`).
Reconcile only ever STRIPS — a Create spell is trainer-taught, never granted.

**Firestone: no gap.** The four shared tiers are value-IDENTICAL between 1.12 and 2.4.3
(25-35 / +10, 40-60 / +14, 60-90 / +17, 80-120 / +21).

**Spellstone: accepted gap 1.** The three shared tiers DIVERGE, and items are GLOBAL while eras are
per-character (spec Q2 = A), so a TBC warlock gets the 1.12 values:

| Tier | TBC 2.4.3 (items 5522 / 13602 / 13603) | Shipped Vanilla reconstruction |
|---|---|---|
| Spellstone | +8 spell crit **rating**, dispel-only Use, **Requires Level 36** | +1% spell crit, 400 absorb, **Requires Level 31** |
| Greater Spellstone | +11 spell crit rating, dispel-only Use, **L48** | +1% spell crit, 650 absorb, **L43** |
| Major Spellstone | +14 spell crit rating, dispel-only Use, **L60** | +1% spell crit, 900 absorb, **L55** |

The evidence that would close it is a per-era item mint; declined.

The **TBC-only Master tier is exact** and is new here — live `item_template` after import:

```
entry name             RequiredLevel spellid_1 trigger spellid_2 trigger
22128 Master Firestone       66        948430     1        0        0
22646 Master Spellstone      66        948432     0      948433     1
```

Master Firestone = 116-174 melee fire proc + up to 30 Fire spell damage; Master Spellstone = +20
spell crit rating, dispel-only. Both scale with Master Conjuror (node 20735) through DUMMY markers
12/13 — and 948433 needed a **new** script, `era_spellstone_crit_rating` (Amendment B.8): the
existing `era_spellstone_absorb` hooks `EFFECT_1 / SPELL_AURA_SCHOOL_ABSORB`, hard-wired to the
Vanilla use-spell's absorb slot, while 948433 is a single `EFFECT_0 / aura 189 MOD_RATING`, so the
old hook would have matched no effect (the core's "did not match dbc effect data" line at load)
and Master Conjuror would silently not scale the Master Spellstone at all.

---

## 11. Bundled Vanilla fixes (three)

1. **Improved Shadow Bolt — 4-charge cap** (both eras: Vanilla `932940-932944`, TBC
   `948660-948664`). The shipped Vanilla debuffs had no charge cap, so the debuff ran its full
   12 s; the Phase-4 record tracked this as a fidelity gap ("the generator has no ProcCharges").
   Fixed as `procCharges: 4` plus the **consume `spell_proc` block** — a charge with no proc row is
   never spent. The consume contract is wago 2.5.4's own 17794-17800 (ProcCharges 4, ProcTypeMask
   **139936** = the TAKEN classes with no TAKEN_PERIODIC, chance 100, **no phase mask** — a
   TAKEN-only row may not carry one), because live 17800 has no `spell_proc` row at all (WotLK
   Shadow Mastery has no charges).
2. **`manaCostPct: 0` on the Vanilla Conflagrate clones 932780-932783 and Soul Link 932900**
   (Amendment B.9). Their templates 17962 / 19028 carry `ManaCostPercentage 16`, and
   `Spell::CalculatePowerCost` **adds** the percentage cost on top of the authored flat cost, so
   both were overcharging. Every TBC clone authors `manaCostPct: 0` explicitly.
3. **Historic orphan strip — 921745 / 921754 / 921788.** Three Vanilla auto-passives that
   `StripOrphanedGrants` does not catch (it only resolves *node grants*), so they lingered on a
   WotLK-band warlock bot; the hunter record's broad sweep recorded them as "known pre-existing".
   All three are now in the cross-era leftover strip. **Verified on the same bot** — §13.3's L80
   rung, on the bot that carried them (Michanna, guid 9), prints exactly one band marker:

   ```
   == eratalents doctor: Michanna (class 9 level 80) ==
   era=WotLK storedEra=WotLK available=71 spent=0 generation=4ec433f1
     (no era talents spent this era)
     band DUMMY marker: spell 932939 misc 0 amount 0
     band aura on player: 932939 amount0 0 caster ... applied=yes
     m_petAuras registered: 0
   ```

   No `921745` / `921754` / `921788` anywhere in the WotLK rung, and zero ORPHAN lines. At the L60
   rung the same three ids are back — correctly, as *live node grants* of the Vanilla build
   (`node 18219 rank 3: grant 921754 known=yes`, `node 18223 rank 5: grant 921788 known=yes`).

---

## 12. Pet window `945000` — claimed, and EMPTY (Amendment A.2)

```
SELECT COUNT(*) FROM spell_dbc WHERE ID BETWEEN 945000 AND 945999;   -- 0
```

Spec §4c ("first real emission into the era-1 pet window") was **not borne out**. All 17 demon
pet-passive carriers are LIVE_MATCH, so every pet-stat talent is an **owner spellmod** onto the
carrier's family bit rather than a new pet buff: Fel Stamina `0x8000000` + a self
`MOD_INCREASE_HEALTH_PERCENT`; Fel Intellect `0x10000000` + a self `MOD_INCREASE_ENERGY_PERCENT`
(**do not grant 18731-18744 — live is Fel Vitality and carries the other half**); Unholy Power /
Demonic Tactics / Demonic Resilience likewise. `petBuffBase: 945000` stays declared on the dataset
so a later warlock node needing the auto path has the window reserved — the same outcome hunter
reached at 944000.

---

## 13. Headless gate (generation `4ec433f1`)

### 13.1 Boot and DB checks

`ac-db-import` reapplied `2026_09_07_02_era_talent_tbc_warlock_markers_stones.sql`
(`'DD2C5D8' -> '58FC585' (it changed)`) and the worldserver came up clean. Startup log, filtered
for era/`948xxx`/`941xxx`/drift/ERROR:

```
[mod-era-talents] startup (enable=true)
[mod-era-talents] loaded 944 talent nodes (generation 4ec433f1)
[mod-era-talents] bot build: Kaknal class 9 tab1 61pts
[mod-era-talents] bot build: Michanna class 9 tab1 61pts
... (26 bot-build lines this boot)
```

- **No `did not match dbc effect data` line** naming any `948xxx` / `941xxx` id — in particular
  none for **948433**, i.e. the new `era_spellstone_crit_rating` hook (EFFECT_0 / MOD_RATING)
  binds correctly.
- **No `kAuthoredOrders` drift WARN** for a warlock — the three TBC warlock orders validate, and
  the TBC-band bots spend the full **61**-point budget.
- One unrelated core ERROR appeared once, on a **level-13** (Vanilla-band) warlock bot's imp:
  `[ERROR]: [1062] Duplicate entry '6140-4511' for key 'pet_spell.PRIMARY'`. Diagnosed and
  dismissed as **not ours**: 4511 is a stock imp ability (91 other pets carry the same row), it is
  outside every era band, and the module teaches no pet spells at all
  (`grep -rn "learnSpell|addSpell" modules/mod-era-talents/src/ | grep -i pet` → no hits). It is a
  core `Pet::_SaveSpells` duplicate-insert, not an era defect.

### 13.2 The Michanna ladder — per-rung marker state

Bot: **Michanna**, guid 9, human warlock, the bot that historically carried 921745/921754/921788.
Driver: `tools/wgconsole.py` (first argument is the OUTPUT FILE), rungs L70 → L60 → L70 →
reset → L80 → L70.

> `.raidroster syncone <name>` answers **"Run this in-world as a player."** from the console, so
> the rebuilds in this ladder come from the **level-change hook** (`OnBotLevelChanged` →
> `FactoryReconcile`) rather than from the roster command. That is the same entry point and it
> exercises the same teardown-first path; the `[mod-era-talents] bot build: Michanna class 9 tab1
> 61pts` lines in the log are its receipt.

| Rung | era / points | Markers held | Pet | Orphans |
|---|---|---|---|---|
| L70 (start) | `era=TBC` spent **61** | `932930` (misc 5) + `948410` — **no** 932994, **no** 932939 | **Felguard, entry 17252** | 0 |
| L60 | `era=Vanilla` spent **51** | `932930` + `932994` — **no** 948410, **no** 932939 | none summoned | 0 |
| L70 | `era=TBC` spent **61** | `932930` + `948410` | Felguard 17252 | 0 |
| L70 after `.eratalents reset` | `era=TBC` available **61** spent **0** | `932930` + `948410` (markers are reconcile-owned, not build-owned) | Felguard, **`pet band aura: (none)`** | 0 |
| L80 | `era=WotLK` available **71** spent 0 | **`932939` only** — no 932930, i.e. Corruption is INSTANT for WotLK | Felguard, no band aura | 0 |
| L70 (return) | `era=TBC` spent **61** | `932930` + `948410` | (resummoning) | 0 |

```
grep -icE "ORPHAN|PROBLEM|WRONG-ERA|STALE" ladder.out   ->   0
```

**Zero at every rung, including the very first (pre-reconcile) doctor** — the login reconcile at
worldserver start had already stripped the three historic orphans, so they never even appeared.

The **live-demon evidence** (first rung, Demonology build, Felguard out):

```
  band DUMMY marker: spell 932930 misc 5 amount 0
  band DUMMY marker: spell 948410 misc 0 amount 0
  band aura on player: 948550 amount0 20 caster ... Type: Player ... applied=yes
  band aura APPLIED on player (owned by caster): 948551 amount0 20 caster ... Type: Pet ...
  pet: entry 17252 level 70 hp 4598/6941 mana 4581/4581
    pet band aura: 948551 amount0 20 applied=yes appliedEff0=NO
    pet band aura: 948564 amount0 5 applied=yes appliedEff0=yes
```

- **Soul Link pair works end to end**: the warlock holds `948550` (DUMMY 20), the *pet* holds the
  pet-cast carrier `948551`, and `948551`'s redirect reaches the warlock as an aura **owned by the
  pet** — exactly the Path-B split of Amendment B.3. `appliedEff0=NO` on the pet is CORRECT, not a
  defect: `948551`'s effect 0 is `SPELL_EFFECT_APPLY_AREA_AURA_OWNER (143)`, which radiates to the
  *owner* and never lands on the pet itself; the pet-side half is effect 1 (`aura 79`, +5%).
- **Master Demonologist's per-demon arm is exercised**: `948564` is live on the Felguard
  (entry 17252); an earlier rung with a **Felhunter** (entry 417) carried `948563` instead, i.e.
  the era table picks the arm from the live demon. **Demonology Felguard arm exercised: YES.**
- After `.eratalents reset` the pet's band auras drop to `(none)` and the trained clones are gone
  while the markers persist — lesson 21's tell, clean.

### 13.3 `AI resolve ->` lines (the by-name resolver contract)

Task 6 found that the `kDiscriminators` table in `EraTalentsCommand.cpp` had **no warlock rows at
all**, so spec §5's required `AI resolve` lines were missing. Ten rows were added (§14). The
resolver returns the id the bot ACTUALLY KNOWS for a stock id; a 0 means the check would be
permanently false. Because one build only spends one tab, the non-zero evidence is collected
across four builds of the same bot:

| Stock id | Build that took the node | Resolved to |
|---|---|---|
| 18288 Amplify Curse | Affliction | **948450** |
| 30108 Unstable Affliction | Affliction (roster) | **948472** (rank 3 — the resolver returns the TOP known rank, as the hunter phase measured) |
| 18220 Dark Pact | Affliction (roster) | **18220** (stock — TBC GRANTs) |
| 17962 Conflagrate | Destruction | **948650** |
| 30283 Shadowfury | Destruction | **948670** |
| 17877 Shadowburn | Destruction | **17877** (stock — GRANT, control row) |
| 19028 Soul Link | Demonology | **948550** (and **932900** on the Vanilla L60 rung) |
| 18708 Fel Domination | Demonology | **948570** (and **18708** stock on the Vanilla rung — Vanilla GRANTs) |
| 30146 Summon Felguard | Demonology | **30146** (stock GRANT) |
| 18265 Siphon Life | Affliction | **0 — and 0 is the only possible answer** (below) |

**Every TBC clone keeps its stock NAME** — verified in `spell_dbc`: 948450 "Amplify Curse", 948451
"Siphon Life", 948470 "Unstable Affliction", 948550/948551 "Soul Link", 948552 "Demonic
Sacrifice", 948560 "Master Demonologist", 948570 "Fel Domination", 948650 "Conflagrate", 948670
"Shadowfury" — so **no `kEraNameAliases` row is needed** for this class (contrast warrior's
"Improved Battle Shout" and rogue's "Sword Specialization").

**Siphon Life is a documented always-0**, and it is not a defect. Measured on a hand-built TBC
warlock holding the node:

```
  node 20713 rank 1: grant 948451 known=yes spellInfo=yes
  AI resolve: 18265 (Siphon Life r1 ...) -> 0 (NOT KNOWN — AI check is always false)
```

The character knows the clone under the right name; the resolver still returns 0 because it bails
at its own `if (SpellInfo const* stock = sSpellMgr->GetSpellInfo(stockSpellId))` guard — **18265
has no live SpellInfo at all** (WotLK deleted the spell when it folded Siphon Life into Corruption;
the `_ref` records all six TBC ranks as WAGO_ONLY). No by-id AI check can ever reach Siphon Life,
so a future patch-0021 arm must key on the era clone or on a name, never on 18265. The doctor row
is kept with that wording so the fact stays visible.

### 13.4 Broad all-class doctor sweep

Driver: every character with `online=1 AND level<=80` (500 bots, all ten classes), doctored in
batches of 40 through `tools/wgconsole.py`, `ORPHAN|PROBLEM` aggregated per class.

Driver: every character with `online=1 AND level<=80` (500 bots, all ten classes), doctored in
batches of 40 through `tools/wgconsole.py`, aggregated per class. Counting matches
**`ORPHAN ERA SPELL`** and **`<-- PROBLEM`** literally, not the loose
`grep -icE "ORPHAN|PROBLEM|WRONG-ERA|STALE"` — the hunter record's §12b warning applies: the SHAMAN
doctor's own `AI resolve:` label text contains the words "STALE STOCK", which is descriptive, not a
finding.

| Class | Bots doctored | Bands seen | ORPHAN ERA SPELL | PROBLEM / WRONG-ERA |
|---|---|---|---|---|
| Warrior | 50 | TBC 4 / Vanilla 24 / WotLK 22 | **0** | **0** |
| Paladin | 50 | TBC 4 / Vanilla 21 / WotLK 25 | **17** | **0** |
| Hunter | 50 | TBC 6 / Vanilla 26 / WotLK 18 | **11** | **0** |
| Rogue | 50 | TBC 5 / Vanilla 25 / WotLK 20 | **0** | **0** |
| Priest | 50 | TBC 7 / Vanilla 23 / WotLK 20 | **0** | **0** |
| Death Knight | 50 | TBC 6 / Vanilla 3 / WotLK 41 | **0** | **0** |
| Shaman | 50 | TBC 1 / Vanilla 29 / WotLK 20 | **0** | **0** |
| Mage | 50 | TBC 5 / Vanilla 27 / WotLK 18 | **9** | **0** |
| **Warlock** | **50** | **TBC 6 / Vanilla 26 / WotLK 18** | **0** | **0** |
| Druid | 50 | TBC 6 / Vanilla 23 / WotLK 21 | **0** | **0** |
| **Total** | **500** | | **37** | **0** |

**Warlock is clean across all 50 bots in all three bands** — including `Michanna`, the bot that
carried 921745 / 921754 / 921788 before this phase. That is this phase's own result.

**The 37 non-warlock lines are 4 bots, and all four are the pre-existing shape the hunter record
already diagnosed** (`docs/verification/era-talents-tbc-phase7-hunter.md` §12b and §18), NOT a
warlock regression — verified, not assumed:

| Bot | Class | State | Orphan ids |
|---|---|---|---|
| `Patrea` | Paladin | L80, `era=WotLK` **`storedEra=Vanilla`**, spent 0, **0 `era_character_talent` rows** | 932602, 946114, and 15 TBC paladin auto-passives 936004 / 936012 / 936340 / 936348 / 936353 / 936362 / 936388 / 936402 / 936426 / 936434 / 936449 / 936460 / 936466 / 936474 / 936500 |
| `Xonoth` | Hunter | L80, `era=WotLK`, spent 0, 0 rows | 932967 and 9 Vanilla hunter auto-passives 922412 / 922420 / 922442 / 922464 / 922476 / 922481 / 922492 / 922497 / 922513 |
| `Brudugs` | Hunter | L80, `era=WotLK`, spent 0, 0 rows | 922676 — **the exact bot and id the hunter record already recorded** |
| `Rozki` | Mage | L80, `era=WotLK`, spent 0, 0 rows | 932760 and 8 Vanilla mage auto-passives 920009 / 920020 / 920028 / 920044 / 920052 / 920072 / 920116 / 920122 |

Verbatim shape of every one of the 37 lines:

```
  ORPHAN ERA SPELL: <id> (known but no learned talent grants it)
```

**Diagnosis (unchanged from Phase 7):** `StripOrphanedGrants` is scoped to `NodesFor(currentEra)`,
which is **empty for WotLK**, so nothing in the current reconcile can see a leftover from a
*different* era's node list. All four bots hold **zero** `era_character_talent` rows while
`character_spell` still holds band ids — historic residue of band moves made under older code. The
current code does not produce them: this record's own ladder (§13.2) walks Michanna L60 → L70 →
L80 and the L80 rung holds **only** the marker, no band row at all.

**Why this task did not "fix" it.** The warlock fix for 921745/921754/921788 is a *hardcoded
strip-list entry* for three known ids. Extending that to 37 more paladin/hunter/mage ids is
whack-a-mole against an unbounded set; the real fix is a generic "strip any band spell that no
era's spent nodes grant" sweep, which the hunter record already called **a design change, not a
phase fix**. It is escalated to the orchestrator rather than improvised here — see §17.


---

## 14. Bot AI by-id check, and the doctor gap this task closed

```
grep -rnE "\b(17962|18265|18288|19028|25228|30108|30283|17877|18220|18788|30146|18708)\b" \
  azerothcore-wotlk/modules/mod-playerbots/src/Ai/Class/Warlock/ \
  azerothcore-wotlk/modules/mod-playerbots/src/Bot/ \
  azerothcore-wotlk/modules/mod-playerbots/src/Mgr/ | grep -v "\.md"
```

**Exactly one hit:**

```
Ai/Class/Warlock/WarlockTriggers.cpp:176:   {"felguard", 30146, 17252}};
```

`WrongPetTrigger`'s `WarlockPetDef pets[]` table. **Disposition: no follow-up needed.** 30146 is a
**GRANT** in the TBC tree (the ladder shows `node 20742 rank 1: grant 30146` and `AI resolve:
30146 -> 30146`), so the trigger's `HasSpell`-shaped check stays true for an era-managed bot with
no bridge routing. Nothing else in the warlock AI, the shared `Bot/` layer or `Mgr/` keys on any
spell this phase cloned — so **patch 0021 needs no warlock arm and was NOT regenerated.**

**Patch 0020's warlock stone-strategy suppression covers TBC-band bots.** Its gate is
`EraTalentBots_SpecTabs(player, eraTabs)` — i.e. *is this character era-managed*, not *which era* —
so a TBC-band warlock bot drops the `spellstone` / `firestone` bag-use strategies exactly like a
Vanilla-band one, which is right, because the TBC Create clones conjure the same relic-slot
**equippable** stones (plus the two Master tiers, 22128 equip-triggered) that the bag-use strategy
cannot operate.

**Doctor gap closed here (doctor-only, no data change):** ten `CLASS_WARLOCK` rows added to
`kDiscriminators` in `modules/mod-era-talents/src/EraTalentsCommand.cpp` (spec §5 required them;
Task 5 shipped none). They are **resolver-sanity rows, not patch-0021 sites** — the grep above is
the proof that nothing is broken today if they read 0 — but they are the only headless way to see
the by-name argument hold for six clone/reconstruct chains.

---

## 15. REAL-CLIENT CHECKLIST (user, level 70 warlock, fresh `patch-V.mpq`)

Everything above is headless. The following require a live 3.3.5a client with the generation
`4ec433f1` `patch-V.mpq` installed, and only the user can run them.

- Canary green (no red stale-generation warning in chat).
- Three tabs with real backgrounds, **64 icons present**.
- **Corruption cast bar 2 s** at 0/5 Improved Corruption and **instant** at 5/5.
- Siphon Life / Conflagrate / Shadowburn / UA / Shadowfury chains purchasable at the TBC levels,
  and the stock ranks not offered.
- **Master Firestone and Master Spellstone craftable**; the Vanilla tiers still craftable; the
  WotLK enchant stones NOT offered.
- **Master Conjuror scales an equipped stone.**
- **Soul Link 20% redirect with a Felguard out**, and it resumes after a resummon.
- **Demonic Sacrifice per demon including the Felguard**, correct buff icons.
- **Master Demonologist Felguard branch.**
- **Fel Stamina / Fel Intellect raise both the demon's and the warlock's pools**, and persist
  through dismiss/resummon.
- **Amplify Curse boosts Curse of Doom.**
- **ISB debuff drops after four Shadow Bolts.**
- **Nightfall Shadow Trance.**
- **Pyroclasm stun.**
- **Nether Protection immunity proc.**
- **Spellmod tooltips rewrite.**
- **Improved Succubus icon renders** (Amendment B.10 — the cached post-3.3.5a texture name
  `ability_warlock_randomizesuccubusincubus` would render blank and was changed to
  `spell_shadow_summonsuccubus`; the `classic_` guard does not catch post-3.3.5a names, so eyeball
  every icon).
- **Master Spellstone crit rating rises with Master Conjuror** (Amendment B.8 — the new
  `era_spellstone_crit_rating` script).

**Vanilla re-check** (the bundled fixes touch a shipped, signed-off era):

- Vanilla stones still trainable — now behind **932994**, and the trainer's "Requires …" line
  reads **"Vanilla Warlock"**.
- Corruption still 2 s for a Vanilla warlock.
- **ISB is now 4 charges.**
- Conflagrate and Soul Link cost their authored flat mana only (no hidden +16% of base mana).

---

## 16. Accepted gaps

| # | Gap | Detail |
|---|---|---|
| 1 | Shared Spellstone tier magnitudes | §10. Items are global, eras are per-character (Q2 = A). Firestone has no gap. |
| 2 | TBC-Classic renames in the cached tooltips | "Improved Sayaad", "Improved Subjugate Demon", "Succubus/Incubus", "Subjugate Demon", "Fel Guard". The dataset authors the 2.4.3 / live-3.3.5a names; the cache is evidence, not content. |
| 3 | WotLK flat-mana → `ManaCostPct` conversion (+ **3b** the WotLK global attribute bits `AttributesEx6 0x800000` / `AttributesEx4 0x100000`) | The largest DIVERGED contributor. Only matters where we CLONE (the clone carries the correct TBC flat cost); never on its own a reason to refuse a GRANT. |
| 4 | Nightfall's 6 s internal cooldown | `spell_proc` row -18094 carries `Cooldown 6000`; TBC had none. The row is stock AzerothCore balance and is global. Declined. |
| 5 | Dark Pact range | TBC 30 yd vs live 100 yd on the GRANTed stock spell. Harmless (the target is the caster's own demon). |
| 6 | `SPELL_ATTR0_CU_NO_POSITIVE_TAKEN_BONUS` never reaches a Conflagrate clone | `SpellMgr.cpp:3702` keys on the literal 17962. `AttributesCu` is core-computed with no DBC column and no generator key. Small impact (the era clone deals FLAT damage with `apply_direct_bonus = false`). **Framework follow-up, not this phase: `era_audit.check_spellfix_inheritance` reads only `ApplySpellFix({...})` in `SpellInfoCorrections.cpp` and is blind to `SpellMgr.cpp`'s by-id `AttributesCu` sweep.** |
| 7 | Shadow Embrace debuff duration | TBC's 32386-32391 are `DurationIndex 21` = PERMANENT because the TBC client managed their lifetime through class script 4994 (unimplemented on 3.3.5a). A permanent -5% physical debuff is not shippable, so Task 2 picked a finite duration; recorded so the deviation is not later read as an error. |
| 8 (new) | `18265` can never be resolved by id | §13.3. Not a defect and not fixable — the spell does not exist on 3.3.5a. Recorded so a future patch-0021 arm does not key on it. |

## 17. Known open items (not this phase's defects)

- ~~**ESCALATION — historic band residue on WotLK-band bots is wider than the three warlock ids this
  phase strip-listed.**~~ **CLOSED 2026-09-06 — shipped as Phase 9.5.** The 500-bot sweep had found
  the same shape on a paladin (17 ids), two hunters (11) and a mage (9) — §13.4; zero warlock. The
  generic fix landed as `EraBandClassifier` (`modules/mod-era-talents/src/EraBandClassifier.{h,cpp}`
  + `era-data/band-allowlist.yaml` → `src/EraBandAllowlist.gen.h`): one rule now decides whether a
  character may legitimately know any `[920000, 950000)` spell (current-era node grant at the spent
  rank / a `spell_ranks` rank above a node-granted r1 / an allowlist entry), `Sweep` runs after every
  strip arm in `ReconcileBaselineSpells` for **every** era including WotLK, and the per-class
  hardcoded strip lists were deleted as designed. `.eratalents doctor`'s ORPHAN loop and the new
  `.eratalents bandsweep <char>` call the same `Classify`, so reconcile and the doctor cannot
  disagree. Verified headless (residue bots clean, 0-orphan broad sweep, three ladders) —
  `docs/verification/era-talents-phase9-5-band-sweep.md`; design
  `docs/superpowers/specs/2026-09-05-era-talents-phase9-5-band-orphan-sweep-design.md`. The three
  warlock ids this phase strip-listed are covered by the generic rule and need no class-specific arm.
- `pet_spell` duplicate-insert core ERROR (§13.1) — a stock `Pet::_SaveSpells` issue, unrelated.
- Amendment B.4: the `_ref`'s `proc_data_census:` transcription is column-shifted for at least
  18708 and 17941. Read `spell_proc` from the live DB.
- Amendment B.11 (cosmetic): Wowhead rounds scaled damage maxima up where the core truncates
  (Conflagrate r1 reads 249-316, the server rolls 249-315); wago's TBC Conflagrate r1 `dieSides` is
  67 where the Vanilla clone used 68.
