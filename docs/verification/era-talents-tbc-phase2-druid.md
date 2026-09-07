# Era Talents — TBC Phase 2 (Druid) verification record

> **Generation superseded (2026-09-06 final review):** the current stamp is the closing row of docs/verification/era-talents-final-review-tbc-vanilla.md §10 — reinstall patch-V.mpq + restage the addon before any real-client pass.

**Status: HEADLESS-VERIFIED 2026-08-31 (generation `b52600c7`).** All 62 nodes authored and wired,
`era_audit.py` clean, 173/173 host tests green, three-era reconcile re-proven on live characters,
two live-found defects fixed (see §8). **Real-client sign-off (Task 10) NOT yet done** — three
behaviours are known-unobserved headlessly and are listed in §9. Branch `feat/era-talents` —
**merged to master 2026-09-07** (TBC merges as a set after its own final all-class review + a Vanilla-regression
check). The `EraTalentIP.cpp` ERA_TBC allowlist flip is still an **uncommitted dev-box edit**
(verified unstaged at close-out); the shipped allowlist stays Vanilla-only, so every TBC druid arm
committed here is dead code in production until the single final-enablement task.

**Spec/plan:** `docs/superpowers/specs/2026-08-30-era-talents-tbc-phase2-druid-content-design.md`
(read **Amendment A** — it overrides the spec body) +
`docs/superpowers/plans/2026-08-30-era-talents-tbc-phase2-druid.md`.

**Phase commits (feat/era-talents, on top of TBC Phase 1 close-out `d3ba4cf`):**
`108bd69`/`99da025` spec · `9891e27` plan · `80737e4` helper slice + class-11/era-1 guard test ·
`6da1d67` tooltip cache · `971df90` skeleton import · `3d3b390` survival map (`_ref`) ·
`ff17e0f`/`4b273cd` Amendment A · `6966394` wire dataset · `6c97db4`/`f78713e` reconcile arms +
FF(Feral) gate + doctor predicates · `49d752e` id-ledger reconcile to A.4 · `8dc4141` SQL slots ·
`a24a97d` Balance tab · `1177ffa` Feral tab + Mangle chain · `fe2f448` orphaned-higher-rank check ·
`aba1af0` Restoration tab + Tree of Life.

---

## 1. What shipped

The complete **TBC 2.4.3 druid talent tree** — **62 nodes** (Balance 21 / Feral Combat 21 /
Restoration 20, 9 tiers, ids **20100–20161**, era 1). **Zero core patches.** Value basis: **wago
2.5.4.44833** end-to-end (see memory `tbc-era-value-basis-wago-2540`), live-verified against the
running server's `Spell.dbc` by Task 3.

- **Class 11, spell family 7** (the inversion trap). `era_audit.py`'s family guard is clean, and the
  DB sweep found **0** rows in the druid auto-passive band or helper slice with `SpellClassSet`
  outside `{0, 7}`.
- **Auto-passives:** 141 generated rows in `936800–937284` (band `936000 + (nodeId−20000)*8 +
  (rank−1)`; the slice reserved for druid is `936800–937292`).
- **Hand helpers:** 19 ids used — see §4.
- **SQL:** `2026_08_31_00_..._data.sql` / `_01_..._custom_spells.sql` (generated),
  `_02_..._ng_trainers.sql` / `_03_..._mangle_trainers.sql` / `_04_..._omen.sql` (hand).
- **Addon Lua:** `client-addons-src/EraTalents/data/generated/DruidTBC.lua`; wired into
  `datasets.txt`, `build-addon.sh`, `EraTalents.toc`, `merge-into-patch.sh`.
- **C++:** four three-way `CLASS_DRUID` reconcile arms + the Faerie Fire (Feral)
  `kBaselineSpellGates` three-way split (`EraTalents.cpp`); doctor predicates
  (`EraTalentsCommand.cpp`); `era_dru_tree_of_life` + `era_dru_tree_of_life_suppress`
  (`EraTalentProcScripts.cpp`).
- **Generator (purely additive, byte-neutral for shipped data):** `op: bonusMultiplier`
  (SPELLMOD_BONUS_MULTIPLIER 24), a `tree` stance token (`FORM_TREE` mask bit 2), and a
  `procTypeMask:` helper scalar.

## 2. Per-tab node counts and dispositions

All 62 nodes are wired; **0 display-only**.

| Tab | Nodes | Ids | spellmod | multi | stat | grant / clone |
|---|---|---|---|---|---|---|
| Balance | 21 | 20100–20120 | 7 | 6 | 1 | 7 |
| Feral Combat | 21 | 20121–20141 | 2 | 9 | 3 | 7 |
| Restoration | 20 | 20142–20161 | 9 | 4 | 1 | 6 |

**Grant/clone nodes (what each hands the player):**

| Node | Name | Grants |
|---|---|---|
| 20101 | Nature's Grasp | **fresh clone** 946270 (r2–r6 trainer-taught 946271–946275) |
| 20107 | Insect Swarm | stock 5570 (accepted magnitude gap, §6) |
| 20109 | Vengeance | stock 16909–16913 |
| 20112 | Nature's Grace | **fresh clone** 946267 → buff 946268 |
| 20117 | Moonkin Form | stock 24858 (accepted aura-scope gap, §6) |
| 20119 | Wrath of Cenarius | stock 33603–33607 |
| 20120 | Force of Nature | stock 33831 |
| 20127 | Feral Charge | stock 16979 |
| 20130 | Predatory Strikes | **reused Vanilla clones** 932500–932502 |
| 20131 | Primal Fury | stock 16958 / 16961 |
| 20133 | Faerie Fire (Feral) | stock 16857 (accepted substrate gap, §6) |
| 20135 | Heart of the Wild | stock 17003–17006, 24894 |
| 20138 | Leader of the Pack | **fresh clone pair** 946256 (hidden form-passive) → 946257 (visible party aura) |
| 20141 | Mangle | **fresh clones** 946258 (Bear r1) + 946261 (Cat r1); r2–r3 trainer-taught |
| 20143 | Furor | stock 17056–17061 |
| 20149 | Omen of Clarity | **fresh clone** 946269 |
| 20152 | Nature's Swiftness | stock 17116 |
| 20158 | Swiftmend | stock 18562 |
| 20159 | Natural Perfection | stock 33881–33883 |
| 20161 | Tree of Life | stock form 33891 + **fresh pair** 946265 → 946266 (Route B, §5) |

## 3. Reuse ledger (Amendment A.3 — "fidelity to the letter")

Only **value-identical** helpers were reused. Everything the survival map flagged as
`REUSE_WITH_GAP` became a fresh TBC clone instead.

| Vanilla clone | Reused? | Node | Evidence |
|---|---|---|---|
| **932500–932502** Predatory Strikes r1–r3 | **YES** | 20130 | Byte-identical: TBC and live EFFECT_0 agree (+50/100/150% of level as AP); TBC has no EFFECT_1/2 and the Vanilla clones 0-fill them. WotLK's added weapon-AP% + Ravage proc are absent in TBC. |
| **932513** Enrage | **YES** | — (reconcile grant at L12) | Task 3's survival map found TBC 2.4.3 Enrage byte-identical to the Vanilla clone. The armor drawback stays an accepted gap (§6). |
| 932504 Nature's Grace buff | no → **946268** | 20112 | Mechanic/magnitude match, but TBC `DurationIndex 8` = 15 s vs the clone's `21` (persists until consumed). |
| 932505 + 932507–932511 Nature's Grasp r1–r6 | no → **946270–946275** | 20101 | TBC has **no `SpellPower` row** on any of the six ranks — Nature's Grasp is **free** in 2.4.3; the Vanilla clones charge 50–125 mana. |
| 932512 castable Omen of Clarity | no → **946269** | 20149 | TBC is 30 min (`DurationIndex 30`), melee-only (`ProcTypeMask 20` vs 81924), 10 s ICD. Only the 120 mana matched. |

Consequence for the reconcile arms (Amendment A.5): only Enrage is a shared arm; Nature's Grasp and
Omen split **three ways**, each era stripping the other two eras' spells. Verified in §7.

## 4. Fresh-clone id table — the TRUE used sub-range

Slice claimed at Task 1: **946256–946511**. **Actually used: 19 ids, `946256–946275` with `946264`
NOT used.** `946276–946511` is free within the slice; `946512–949999` is unclaimed (DB sweep: 0 rows
authored there).

| Id | What | Node |
|---|---|---|
| 946256 | Leader of the Pack — hidden form-passive (`PASSIVE\|DO_NOT_DISPLAY`, ShapeshiftMask 145 = cat/bear/direbear, `TRIGGER_SPELL` → 946257) | 20138 |
| 946257 | Leader of the Pack — visible `APPLY_AREA_AURA_PARTY` aura 52 **+5%** crit, radiusIndex 11. Templates **stock 24932** (to inherit `SpellClassMask_2 = 2048` so Improved LotP's spellmod binds, and `ProcTypeMask 4436`), carries its own crit-only (`HitMask 2`) `spell_proc` row, and is bound to the **core** script `spell_dru_leader_of_the_pack` — that script is bound by id to stock 24932, so the on-crit heal is dead on a clone without the bind. | 20138 |
| 946258–946260 | Mangle (Bear) r1–r3 — `DurationIndex 29` (12 s) is the only override; effects inherited wholesale (EFFECT_2 is 115% in **both** eras) | 20141 |
| 946261–946263 | Mangle (Cat) r1–r3 — `DurationIndex 29` **and** EFFECT_2 basePoints **159** (= 160%, vs live 199) | 20141 |
| **946264** | **NOT USED.** Amendment A.4 allocated it for an "Era: TBC Druid" version-swap marker; the shipped reconcile arms gate on era + node rank + `EraReplacementSpellReady`/`EraNodeIsWired`, and the trainer chains anchor on the talent-granted rank 1, so no marker was needed. Confirmed: no `Era: TBC Druid` row exists in `spell_dbc`. **Free.** | — |
| 946265 | Tree of Life — hidden `FORM_TREE` form-passive (`PASSIVE\|DO_NOT_DISPLAY`, ShapeshiftMask 2, `TRIGGER_SPELL` → 946266) | 20161 |
| 946266 | Tree of Life — `APPLY_AREA_AURA_PARTY`, aura **115** `MOD_HEALING`, misc 127, radiusIndex 11 (45 yd); amount = **25% of the caster's total Spirit** via `era_dru_tree_of_life` `DoEffectCalcAmount` | 20161 |
| 946267 | Nature's Grace talent carrier — clone of 16880 at **ProcChance 100** (stock procs at **33%** on this core; WotLK split it into a 33/66/100% chain and left r1 at 33%) | 20112 |
| 946268 | Nature's Grace buff — the −500 ms `ADD_FLAT_MODIFIER` misc-10 shape at `DurationIndex 8` = 15 s | 20112 |
| 946269 | Omen of Clarity — castable clone: 30 min, `ProcTypeMask 20` (melee-only), own `spell_proc` row at **3.5 PPM + 10 s cooldown**, `spell_script_names` bind to `spell_dru_omen_of_clarity`, `Attributes 0` + instant so it appears in the spellbook (stock 16864 is a hidden passive with no spellbook entry) | 20149 |
| 946270–946275 | Nature's Grasp r1–r6 — 35% chance / 1 charge / outdoor-only (`Attributes 98304`) / `DurationIndex 22` (45 s) / **powerCost 0**; inherited per-rank Entangling Roots triggers. r1 node-granted, r2–r6 trainer-taught (TrainerId 33, ReqLevel 18/28/38/48/58) behind a `ReqAbility1` chain + `spell_ranks`. | 20101 |

**946276 was evaluated and DECLINED** (Task 7) for a Faerie Fire (Feral) clone — see the gap table
in §6 and `_ref.fresh_clones._946276_DECLINED` for the three code facts. It stays free.

**DUMMY-marker registry: none claimed this phase.** Next-free stays **18** (paladin consumed 17).

## 5. Tree of Life — Route B (`data_only`, no core patch)

**Decision (Amendment A.2):** stock 34123 (WotLK's +6% raid `MOD_HEALING_PCT`) is hardcast on
`FORM_TREE` unconditionally at `SpellAuraEffects.cpp:1359` with no `HasTalent` gate and no script
anywhere in the fork. TBC's aura is **115 `MOD_HEALING`** (a flat healing-received bonus, party
scope). The obstacle is a **double-dip**, not a missing mechanism, so the TBC aura must *suppress*
the WotLK one:

1. `era_dru_tree_of_life_suppress` is an AuraScript bound to **stock 34123** (a script binding, not
   a `spell_dbc` override — the same idiom as the shipped stock-Sap 18499/5138 binds, so the
   "never override a stock spell" rule is intact). Primary suppression is
   `DoCheckAreaTarget → false` for an era-managed TBC druid (`Aura::CanBeAppliedOn →
   CheckAreaTarget` runs for every candidate including the caster), leaving the aura owned but
   applied to nobody — no +6%, no handler, and no duplicate "Tree of Life" buff icon.
   `DoEffectCalcAmount → 0` is the belt-and-braces fallback. The era test is
   `druid->HasSpell(ERA_TOL_FORM_PASSIVE)` — **ownership of 946265**, never "is a bot".
2. The real TBC effect is delivered by 946265 → 946266 (§4), amount = 25% of total Spirit.
   `GetStat(STAT_SPIRIT)` is the fully-buffed value, so Living Spirit (node 20157) folds in.
   `canBeRecalculated = false` pins the amount at form entry — the core never recalculates
   `SPELL_AURA_MOD_HEALING` on a stat change (documented limitation: re-enter the form after a
   Spirit swap).

**Verified headlessly:** the two clones exist with the right shape; both script binds are present in
`spell_script_names` (34123 → `era_dru_tree_of_life_suppress`, 946266 → `era_dru_tree_of_life`);
node 20161 grants stock 33891 and 946265 is the reconcile-granted partner; the doctor recognises
946265 as a paired grant. `basePoints` on 946266 is 0, so a script that failed to run would ship
*nothing* rather than a wrong magnitude.

**NOT verified:** the suppression itself, and the Spirit-derived amount, have **never been observed
on a character in Tree of Life form.** No bot on this realm shapeshifts and there is no game client
here. Amendment A.2's own confidence label —
**HIGH_ON_MECHANISM / UNVERIFIED_AT_RUNTIME** — still stands. This is a Task-10 item; the fallback
if it fails at runtime is `accepted_gap` (ship WotLK's aura, rewrite the tooltip), **never** a core
patch.

## 6. Accepted gaps (from `era-data/tbc/druid.yaml` `_ref.accepted_gaps`)

That embedded `_ref:` block is the **single origin** for this ledger — `gen_era_talents.py` makes the
inline copy win over the standalone `era-data/_ref/tbc/druid-spells.yaml`. The last three gaps that
lived only in the standalone file (`tree_of_life_form_properties`, `natural_perfection_periodic_bit`,
`furor_wotlk_extra_effect`) were merged into it on 2026-09-06 (final review M-6) and appear below.


| Gap | What ships instead | Why accepted |
|---|---|---|
| **Moonkin Aura scope** (§6.1) | Node 20117 grants stock 24858; the core hardcasts 24907 on `FORM_MOONKIN` (`SpellAuraEffects.cpp:1577`), giving WotLK's **raid**-scope aura instead of TBC's **party** scope. Magnitude (+5% spell crit) is already correct in both eras. | Core hardcast keyed on the form id, not on a talent or spell — unreachable without a core patch. Node tooltip rewritten to the shipped behaviour. Precedented by the Vanilla phase (which accepted a larger miss). |
| **CLOSED 2026-09-07** (clones 946281-946286) — **Insect Swarm line** (§6.2) | The whole six-rank line stays at WotLK values, rank 1 included (per-tick 24/39/62/90/124/172 vs TBC 18/32/50/72/99/132; hit debuff −3% vs −2%). | r2–r6 are trainer-taught at WotLK values; authoring r1 alone would make the chain non-monotonic (TBC r1 18 → WotLK r2 39), which is worse for a player than a uniform offset. |
| **Enrage armor drawback** (§6.3) | TBC 5229's EFFECT_1 (`DUMMY`, basePoints −76) armor reduction is omitted — inherited from the reused Vanilla clone 932513. | Beneficial divergence; the Vanilla omission was itself reasoned (the 1.12 aura-101 base was not confidently decodable). |
| **Force of Nature treant stats** (§6.4) | Treants come from WotLK `creature_template`. | **MEASURED at Task 6 — no era divergence.** Entry 1964 is the TBC-flagged row (`exp=1`), no AI/script, no addon row; the summon is a Guardian so `InitStatsForLevel(owner->GetLevel())` applies. A stock WotLK druid on this core summons identical treants. **Caveat:** absolute parity with retail 2.4.3 tuning is not verifiable here (no TBC world-DB snapshot in this repo), so this closes as "no measurable era divergence", not "proven identical to retail TBC". |
| **Predatory Strikes in Moonkin** | No attack power in Moonkin Form (the core gates the icon-1563 DUMMY on `IsInFeralForm()`, `StatSystem.cpp:408`). | **NOT an era divergence** — the WotLK tooltip makes the same promise and stock WotLK druids behave identically. Recorded only so it is not mistaken for a regression during the real-client pass. |
| ~~**Improved Faerie Fire substrate**~~ | **CLOSED by 946277-279** (aura-109 `ADD_TARGET_TRIGGER`, fix round §13.2). Node 20118 no longer uses the spellmod pair at all: it carries one aura 109 masked to A=1024 (the only two family-7 holders are Faerie Fire 770 and Faerie Fire (Feral) 16857) at chance 100, casting companion debuffs 946277/946278/946279 on the target. Those carry the real aura184 (melee) + aura185 (ranged) at +1/2/3%, read by the victim's own `GetUnitMissChance` (`Unit.cpp:3839`). | Historical text (the spellmod halves being inert / mis-landing on spell hit) applied only to the pre-§13.2 shape. **Residual = baseline Faerie Fire 770 only** — its own stock 3.3.5a substrate, a baseline-spell divergence rather than a talent one, and 770 is not in this tree (the same story for 16857 is the next row). |
| **Faerie Fire (Feral) substrate** | Node 20133 grants **stock 16857**: −5% armor for 5 min instead of TBC's flat −175 armor for 40 s. | A 946276 clone was evaluated and declined on three code facts: (1) `SpellInfoCorrections.cpp:1266` clears `SPELL_ATTR1_IMMUNITY_TO_HOSTILE_AND_FRIENDLY_EFFECTS` from 770/16857 **by id** and the generator has no `attributesEx` scalar to reproduce it; (2) `Spell.cpp:8869` hardcodes `Id == 16857` to pre-cast 60089 in Bear/Dire Bear; (3) the shipped substrate (reconcile arm 5 + the `kBaselineSpellGates` walker guard) is keyed on this node granting stock 16857. Tooltip rewritten to the shipped behaviour. |
| **Feral Swiftness dodge scope** | Node 20126's dodge applies in **Cat Form only**; TBC's tooltip promises Cat/Bear/Dire Bear. | The core's `spell_dru_feral_swiftness` delivers the bear/dire-bear dodge only `if (player->HasTalent(...))`, which era characters never satisfy, and 24867/24864 lack `SPELL_ATTR0_PASSIVE` so `HandleShapeshiftBoosts` will not self-apply them. One passive carries one ShapeshiftMask and the speed half must stay Cat-only; closing it needs a second helper clone per rank, which A.4 does not allocate. Tooltip rewritten. |
| **Feral Instinct threat encoding** | The +5/10/15% threat is delivered as `SPELL_AURA_MOD_THREAT` gated to ShapeshiftMask 145, so it also applies in **Cat** Form (TBC's tooltip says Bear / Dire Bear). | Same rule as the Mangle bleed carrier: author the era **behaviour** in this core's encoding rather than a literal copy of an encoding this core reads differently. TBC's own `EffectSpellClassMask` is not recoverable from any source in this repo. The shipped Vanilla node 18718 uses this exact aura with no stance gate at all, so the TBC node is strictly closer. The Cat leak is unavoidable inside one passive because the other half (Prowl stealth) is Cat-only. |
| **Nurturing Instinct — Cat healing half** | Node 20134 ships only the Agility-to-healing half (at TBC values 50/100%, vs WotLK 35/70%); the "+10/20% healing done to you in Cat form" half does not ship. | **NOT an era divergence** — it is not in the spell row in either era; on this core it comes from 47179/47180, added by `spell_dru_nurturing_instinct` bound **by id** to 33872/33873, and those carry `Attributes 400` (no `SPELL_ATTR0_PASSIVE`) so `HandleShapeshiftBoosts` never applies them. A stock WotLK druid gets the same nothing. Tooltip rewritten. |
| **CLOSED 2026-09-07** (era_dru_leader_of_the_pack) — **Improved LotP mana return** | Node 20139 also returns mana to the druid on a LotP heal proc — a WotLK 3.0 addition TBC did not have. | The heal and the mana return live in one core script (`spell_druid.cpp:1428-1433`), and 946257 must be bound to that script to get the heal at all. Splitting them needs an era copy of the script for a small beneficial divergence. Recorded, not fixed. |
| **Tree of Life form properties** | TBC's Tree of Life tooltip also promises −20% movement speed and −20% mana cost on the castable-in-form set. Neither is a spell EFFECT (TBC 33891 has exactly two effects, aura 36 `MOD_SHAPESHIFT` misc 2 and aura 77 `MECHANIC_IMMUNITY` misc 17 — byte-identical to live), so both are core-side `FORM_TREE` properties. Node 20161's tooltip omits them rather than promising them. | Out of reach without a core patch, and the divergence is against retail TBC rather than against anything this project introduced — a stock WotLK druid on this core gets the same form behaviour. |
| **Natural Perfection periodic bit** | Node 20159 grants the stock chain 33881/33882/33883, whose `ProcTypeMask` is 664232 where TBC's is 139944 — one bit, `0x00080000 PROC_FLAG_TAKEN_PERIODIC`. WotLK also arms the absorb off periodic damage taken; TBC arms it only off direct crits. | A one-bit beneficial widening on a talent that is otherwise byte-identical (all three ranks and all three absorb spells 45281/45282/45283 match). `_ref` graded it COSMETIC and reclassified the node into the "clean, plain grants" bucket (Amendment A.1); three fresh clone ids to remove a proc trigger is disproportionate. |
| **Furor WotLK extra effect** | Node 20143 grants the stock Furor chain 17056/17058-17061. Its EFFECT_0 (the icon-238 DUMMY the core reads) is byte-identical to TBC, but live adds an EFFECT_1 TBC does not have — aura 107 `ADD_FLAT_MODIFIER`, misc 12 `SPELLMOD_EFFECT2`, +1/3/5/7/9, all-zero effect class mask — so a stock grant ships it. | **INHERITED, not introduced:** the shipped Vanilla node 18733 grants the same five ids with the same extra effect. Keeping the two managed eras' grant manifests identical is load-bearing for the band orphan sweep across an era transition (see node 20109's comment), so diverging costs more than the effect is worth. |
| ~~Mangle stock rank chain~~ | **WITHDRAWN — not a gap.** | The first revision of `2026_08_31_03` DELETEd the four stock Mangle r2/r3 trainer rows and was **reverted**: it was a live regression for native WotLK druids (the stock chain continues past r3 — 48563/48565 at L75 require 33987/33983 — with no compensating grant, unlike the Nature's Grasp precedent which *is* compensated by `kWotlkDruidNG`), and it was unnecessary (stock r2 requires stock r1, which a TBC druid never holds). The migration now **restores** the four rows idempotently. Recorded so it is not re-introduced. |

## 7. Three-era reconcile — the check Task 5 could not run

Task 5 shipped the four `CLASS_DRUID` arms and the FF(Feral) gate behind readiness guards
(`EraReplacementSpellReady`, `EraNodeIsWired`) and could only verify their **dormant** state,
because every TBC druid node was a `display_only` seed at the time. With the content shipped the
guards flip. Re-run on live characters at generation `b52600c7` (`.eratalents reset` forces a
reconcile; `.eratalents doctor` reads `Player::HasSpell`, the authority):

| Character | Level | Era read | Holds | Correct? |
|---|---|---|---|---|
| Shummep (bot) | 58 | Vanilla | **932513 only** (talents reset) | yes — unchanged from before this phase; no TBC ids |
| Nyteana (bot) | 65 | **TBC** | **932513** (reused Enrage) + **946269** (TBC Omen, node 20149 spent) | yes — **no** stock 16689/16864, **no** Vanilla clones 932505/932512 |
| Ollesithe (bot) | 77 | WotLK | stock **5229**, the stock NG chain **16689/16810/16811/16812/16813/17329/27009**, stock **16857** | yes — no era clones. (53312 absent is correct: `kWotlkDruidNG` grants it at L78.) |

**`ORPHAN ERA SPELL` count across every doctor run in this task: 0** (after the §8 fix; the
pre-fix run surfaced one real gap — see §8).

**Positive side of the two gates Task 5 could not exercise** (both on Terenmon, a level-63 TBC-band
druid bot, using `.eratalents learn` — each learn runs a fresh reconcile, so both survived ~22
subsequent reconcile passes):

- **Node 20101 spent → 946270 retained.** `node 20101 rank 1: grant 946270 known=yes`. The
  `keepTbcNG` boolean guards a single loop over all six ids `946270–946275`, so proving the guard
  proves the chain.
  **What was NOT observed:** the trained ranks **946271–946275** specifically — they are
  trainer-taught and no character on this realm has trained them (bots only reach a trainer through
  a factory randomize). The guard is shared; the ranks were not individually seen.
- **Node 20133 spent → stock 16857 retained.** `node 20133 rank 1: grant 16857 known=yes`, and it
  survived a further reconcile from learning node 20134. The negative side (unspent → stripped) is
  the Nyteana/Kylarya rows above.

**Clone trainer chains are live.** Nyteana, after `.character level Nyteana 65` followed by
`.playerbots rndbot levelup Nyteana` (which runs `PlayerbotFactory::Randomize`, whose second
`InitAvailableSpells` pass walks the class trainers *after* the era build lands), came out with a
56-point Feral build including node 20141 **and had learned 946259 (Mangle Bear r2) and 946262
(Mangle Cat r2)** from the trainer chain —
`Trainer::CanTeachSpell` honouring `ReqAbility1` off the talent-granted rank 1. (r3 needs level 68.)
This is the same chain shape as Nature's Grasp r2–r6, on TrainerId 33 (class trainer, class 11, 17
druid-trainer creatures).

**Bracket teardown (the load-bearing invariant) confirmed twice.** `rndbot init` re-rolls a bot's
level; two TBC-band druids were rolled down into the Vanilla band mid-task
(Kylarya 68 → 51, Terenmon 63 → 38). After each move, `character_spell` held **zero** ids in
`936000–950000` and `era_character_talent` held **zero** era-1 rows — no 946xxx clone lingered to be
picked up by the AI's name-based resolution.

## 8. Bot re-check (spec §9) — two defects found and fixed

### 8a. `EraTalentBots_ResolveSpellId` never matched a generated auto-passive — **FIXED**

Patch 0021 routes the AI's hardcoded stock-id checks through
`EraTalentBots_ResolveSpellId(bot, stockId)`, which scans the bot's spellbook for a **known
same-name spell** in the era band. The scan required an **exact** name match
(`strlen(name) != nameLen → continue`).

Two naming conventions live in the band:
- hand-authored `helpers:` clones keep the stock name verbatim — "Omen of Clarity", "Blessing of
  Sanctuary", the shaman totems, "Vampiric Embrace";
- **generated auto-passives are named `"<node name> (Rank N)"`** (`tools/gen_era_talents.py`).

So every patch-0021 discriminator that targets a talent **passive** resolved to **0** — i.e. was
permanently false, silently (`HasAura(0)` is just false). Confirmed from the live stores: stock
16931 is `"Thick Hide"` in `Spell.dbc`; the clone an era druid actually knows is
`"Thick Hide (Rank 3)"` (937002 TBC / 925762–925764 Vanilla). Affected, in both eras:

- **druid Thick Hide 16931** — the bear discriminator behind `LfgJoinAction::GetRoles`,
  `AiFactory` `IsTank`/spec pick, and `WorldBuffAction`'s bear-vs-cat branch;
- **paladin Improved Blessing of Might / Wisdom** (20042/20244, `PaladinGreaterBlessingAction`).

This is a **pre-existing bug from the Vanilla bot phase**, not a TBC-druid regression.

**Fix (`EraTalentBots.cpp`):** trim a trailing `" (Rank N)"` before the compare, and require the
parsed `N >= sSpellMgr->GetSpellRank(stockSpellId)` — the by-id checks mean "this rank or better"
(16931 *is* Thick Hide r3), era nodes can carry a different rank count than the stock chain (Vanilla
Thick Hide is a 5-rank node), and only the bot's **current** rank is in its spellbook. A stock spell
with no rank chain (`GetSpellRank` 0) accepts any rank, preserving the old behaviour for those.
`"Thick Hide (Pet, Rank 1)"` does not match (no `" (Rank "` substring), so hunter pet passives are
unaffected.

**Runtime evidence** (new doctor line, §8c), one rank at a time on a TBC-band druid bot:

```
node 20125 rank 1: grant 937000   AI resolve: 16931 -> 0
node 20125 rank 2: grant 937001   AI resolve: 16931 -> 0
node 20125 rank 3: grant 937002   AI resolve: 16931 -> 937002
```

and across bands: Vanilla Shummep (Thick Hide 5/5) → **925764**; TBC Nyteana → **937002**; WotLK
Ollesithe → 0 (correct — it holds neither).

**Omen of Clarity resolves correctly and to the FRESH TBC clone:** on a TBC druid with node 20149
spent, `AI resolve: 16864 (Omen of Clarity) -> 946269` — **not** the Vanilla 932512. That path was
already era-safe (946269 keeps the stock name verbatim); it was re-proven rather than assumed.

**Still unverified:** the *behavioural* consequence of the Thick Hide fix — an era feral druid bot
actually being assigned the tank role / tanking in bear — was not observed. The bridge now returns
the right id; what the AI does with it is a live-play observation.

### 8b. Doctor false-orphaned the trainer-taught Mangle **Bear** ranks — **FIXED**

A live TBC druid bot that had trained Mangle (Bear) r2 reported
`ORPHAN ERA SPELL: 946259 (known but no learned talent grants it)`. Task 7 added a predicate for the
**Cat** chain (`946261–946263`) because Cat r1 is the reconcile-granted partner, but node 20141's own
`rankSpell` is Bear r1 (946258), so **946259/946260** — trainer-taught, never node-granted — fell
through to the generic `NodesFor()` check. Predicate range widened to **946258–946263**, gated on
node 20141 exactly as before, so the Bear and Cat chains now read identically. Re-run: **0 orphans.**

### 8c. New permanent doctor diagnostic (added while fixing 8a)

`.eratalents doctor` now prints, for the target's class, the patch-0021 AI discriminators and what
they resolve to:

```
AI resolve: 16931 (Thick Hide r3 (bear discriminator)) -> 937002
AI resolve: 16864 (Omen of Clarity) -> 946269
```

A `0 (NOT KNOWN — AI check is always false)` line is the tell for exactly the failure in 8a, which
was otherwise invisible for two phases. Covers druid (16931, 16864), paladin (20042, 20244, 20911)
and priest (15286).

### 8d. `tools/gen_bot_builds.py` is Vanilla-only — **RECORDED, not fixed**

`tools/gen_bot_builds.py` reads only `era-data/vanilla/*.yaml` (line 17) and its `BUILDS` table is
hand-authored with **Vanilla node ids**. The emitted `kAuthoredOrders` in `EraTalentBots.cpp` is
keyed `(class, specTab)` with **no era component**, so a TBC-band bot gets the Vanilla order, every
`TryLearn` in it is refused (`TryLearn` hard-guards `n->eraId != era`), and the skip canary fires
once per entry. Confirmed in `Server.log`: 63 `order/tree drift` warnings, all for TBC-band bots
(`Drimdinn tab0`, `Nyteana tab1`, `Umlohda tab2`); Vanilla-band builds in the same window produced
none.

**Impact:** builds still complete — the greedy fill takes over — but the authored **ordering** never
applies to a TBC bot, so keystones are reached last instead of as early as the gates allow. That is
the exact regression class the authored orders exist to prevent (the level-48 shadow priest stall,
2026-08-26). There is **no** cross-era leakage: `TryLearn`'s era guard makes every wrong-era entry a
no-op.

**Predates druid** — TBC paladin has the same hole.

**Why not fixed here:** the fix is not small. It needs (1) `gen_bot_builds.py` to read the
`era-data/datasets.txt` manifest instead of the `vanilla/` glob and key `BUILDS` by
`(era, class, specTab)`, (2) `kAuthoredOrders` re-keyed to include the era with a matching lookup in
`SpendBuild`, and (3) **seven newly hand-authored TBC build orders** (paladin ×3, druid ×3 + the
druid `specTab 3` "cat" pseudo-spec), each validated skip-free by the tool. Authoring build orders
is content work outside this task's scope, and it should land once as a TBC-wide pass rather than
per class phase. **Follow-up recommended before TBC enablement**, alongside the remaining seven
classes.

## 9. What remains UNVERIFIED

Nothing headless can reach these; they are Task-10 real-client items. **None of them passed — none
of them were observed at all.**

1. **Leader of the Pack applying and being VISIBLE in form.** The hidden-passive (946256) → visible
   party-aura (946257) split exists precisely because `HandleShapeshiftBoosts` only reapplies a
   `PASSIVE|DO_NOT_DISPLAY` form passive. No character on this realm shapeshifted during this task,
   so neither the application on form entry, the reapply on form re-entry, nor the party-frame
   visibility was seen. The core-script bind that carries Improved LotP's heal is present in
   `spell_script_names`; the heal firing was not observed.
2. **Tree of Life double-dip suppression** (§5) — mechanism code-verified end to end, never run.
3. **Proc firing generally** — Nature's Grace on crit (946267 → 946268), Celestial Focus's stun,
   Omen of Clarity's Clearcasting at ~3.5 PPM, the Mangle bleed amplification, the LotP crit heal.
   The `spell_proc` rows and script binds are present and correctly shaped in the DB; **no proc was
   observed firing.**

Also unobserved, lower risk: Nature's Grasp ranks 2–6 being trainable (the analogous Mangle chain
*was* observed working, §7); the client frame rendering all three tabs at tier 9 with a 61-point
pool; and the behavioural effect of the §8a fix.

## 10. Headless sweep numbers (generation `b52600c7`)

| Check | Result |
|---|---|
| `tools/era-regen.sh --sync-fork` | clean; **byte-neutral** (no artifact diff — the committed generated files already match) |
| `tools/era_audit.py` | **0 findings** |
| `pytest tools/ -q` | **173 passed** |
| Druid nodes wired (`era_talent` ⋈ `era_talent_rank`) | **62 / 62**, 0 display-only |
| Family guard: `spell_dbc` rows in `936800–937292` ∪ `946256–946511` with `SpellClassSet ∉ {0,7}` | **0 rows** |
| Anything authored in `946512–949999` | **0 rows** |
| Druid helper ids used in `946256–946511` | **19** (min 946256, max 946275; 946264 unused) |
| Druid auto-passives | 141 rows, `936800–937284` |
| `era_talent` census | era 0: `1:52 2:44 3:46 4:51 5:47 7:46 8:49 9:50 11:47` (unchanged) · era 1: `2:64` (paladin, unchanged) + **`11:62`** (druid) |
| Worldserver boot | `[mod-era-talents] loaded 558 talent nodes (generation b52600c7)` = 432 + 64 + 62 ✓, no era-talents errors |
| `era_talent_meta` | `generation = b52600c7` (matches the regen stamp and the client MPQ) |
| `spell_proc` for 932512 (shared Vanilla Omen row) | exactly **1** (946269 has its own row: 3.5 PPM, 10 s cooldown) |
| Clone trainer chains | 946259/946260 + 946262/946263 @ L58/68, 946271–946275 @ L18/28/38/48/58, all `ReqAbility1`-chained on TrainerId 33; `spell_ranks` roots 946258 / 946261 / 946270 |
| `EraTalentIP.cpp` allowlist flip | ` M` (unstaged), **not** in the index — verified at close-out |

## 11. Next

- **Task 10 (user):** real-client pass at level 70, era stage 8, with the freshly built
  `client-addons/_data-patches/patch-V.mpq` (stamp `b52600c7`). The checklist is in the plan; §9
  above is the priority list.
- **Follow-up:** §8d (`gen_bot_builds.py` era awareness + TBC build orders), best done once for all
  TBC classes.
- **Next class in the TBC workflow order:** shaman.

---

## 12. Real-client fix round (2026-08-31, generation `05b215b4`)

The Task-10 real-client pass on a level-70 TBC druid found **three** defects. Everything else
passed — including the Tree of Life double-dip suppression, which the user confirmed works ("party
members only show the 1 buff for tree of life and it is the correct TBC one"), closing §9's
highest-priority unknown and validating Amendment A.2's Route B end to end.

### 12.1 Defect 1 — Savage Fury (node 20132) applied TWICE to Mangle (Cat)

**Symptom (user):** Mangle (Cat) rank 3 read `231% damage plus 455` with Savage Fury 2/2.
Mangle (Bear) rank 3 read `115% damage plus 155` and was **correct**.

**Root cause.** The node authored `Mangle (Cat)` into **both** the `op: damage`
(`SPELLMOD_DAMAGE`, misc 0) and the `op: effect3` (`SPELLMOD_EFFECT3`, misc 23) spellmods. Mangle
(Cat)'s damage lives entirely in `Effect_3` (`SPELL_EFFECT_WEAPON_PERCENT_DAMAGE`), and **both**
mods reach it on this core:

| Mod | Path |
|---|---|
| `SPELLMOD_EFFECT3` | `SpellEffectInfo::CalcValue` → `Unit::ApplyEffectModifiers` applies `SPELLMOD_EFFECT3` for `effect_index == 2` (`Unit.cpp:11603`) — this scales the weapon-percent value itself |
| `SPELLMOD_DAMAGE` | `Spell::EffectWeaponDmg` ends with `MeleeDamageBonusDone(...)` (`SpellEffects.cpp:3693`), which applies `SPELLMOD_DAMAGE` to the finished number (`Unit.cpp:10377`) |

So the talent multiplied twice: `160 × 1.2 × 1.2 = 230.4` and `165 × 1.2 = 198`,
`198 × 230/100 = 455.4` → exactly the reported `231% … plus 455`.

**The masks are the authority, and live already answers the question.** `Spell.dbc` fields 122-130
are **effect-major** (each effect's `(A,B,C)` triple is contiguous, despite the `A_1/A_2/A_3`
column *names*). Stock 16998/16999 read:

| Effect | Mod | Mask (A,B,C) | Spells |
|---|---|---|---|
| 1 | `SPELLMOD_DAMAGE` | (6144, 0, 262144) | Maul (A 2048), Rake (A 4096), Claw (C 262144) |
| 2 | `SPELLMOD_DOT` | (4096, 0, 0) | Rake |
| 3 | `SPELLMOD_EFFECT3` | (0, 1088, 0) | Mangle (Bear) (B 64), Mangle (Cat) (B 1024) |

Blizzard puts Mangle in the **EFFECT3 mod only** and never in the DAMAGE mod. The defect was ours,
not a core quirk.

**Fix.** Drop `Mangle (Cat)` from the `op: damage` `affects:` list; keep the `op: effect3` mod (the
live-faithful carrier). This is the *opposite* of removing `effect3`: both orderings happen to
produce identical server damage (`(wpn+165) × 1.6 × 1.2 == (wpn+165) × 1.92`), but only this one
reproduces live's tooltip, where the flat `$m1` term is **not** scaled by Savage Fury.

**Computed tooltips** (946263, `EffectBasePoints_1 = 164` → `$m1 = 165`, `EffectBasePoints_3 = 159`
→ `$s3 = $m3 = 160`; format `$s3% normal damage plus ${$m1*$m3/100}`):

| Savage Fury | BEFORE | AFTER |
|---|---|---|
| 0/2 | `160% normal damage plus 264` | `160% normal damage plus 264` (unchanged) |
| 1/2 | double-applied (160×1.1×1.1 = 193.6, and `$m1` scaled to 181.5) — exact client rounding not pinned down at this rank | `176% normal damage plus 290` (165×176/100 = 290.4) |
| 2/2 | **`231% … plus 455`** (the reported bug) | **`192% normal damage plus 316`** |

Mangle (Bear) is untouched — it is in neither mask, before or after — and stays
`115% damage plus 155` (946260: `$m1 = 135`, `$s3 = 115`, `135×115/100 = 155`).

**Proof (server, post-`db-import`):**

```
spell_dbc 937056/937057 EffectMiscValue_1..3 = 0, 22, 23
  masks (effect-major) = (4096,0,262144) | (4096,0,0) | (0,1024,0)
                        Rake+Claw          Rake         Mangle (Cat)
```
The generated-SQL diff is a single value: e1's word B `1024 → 0`.

**Swept for the same pattern:** node 20132 was the only place in the dataset where a `damage`
spellmod and an `effectN` spellmod name the same spell (`op: effect2/effect3` appears elsewhere
only on nodes 20118 Faerie Fire, 20139 Improved LotP and 20154 Improved Tranquility, none of which
carry a `damage` mod). Rake's `damage` + `dot` pair is not a double-dip: `Unit.cpp:8934` /
`Unit.cpp:9716` select `SPELLMOD_DOT` **or** `SPELLMOD_DAMAGE` by `damagetype`, never both.

### 12.2 Defect 2 — Feral Aggression (node 20122) rendered a BLANK icon

**Root cause.** The icon was `classic_ability_druid_demoralizingroar`. The `classic_`-prefixed
names are WoW-Classic-2019 re-cuts that **do not exist in a 3.3.5a client**, so the talent button
renders blank — with no error and no log line, which is why it is invisible headlessly and only a
real-client pass can catch it. `tools/import_tbc_talents.py` copies Wowhead's icon field verbatim,
and the Wowhead TBC endpoint serves the Classic name for these spells.

This is the **second** time it has happened: `era-data/vanilla/shaman.yaml` nodes 18215/18240 carry
the same hand-written correction ("`classic_`-prefixed names are WoW-Classic-2019 renames absent
from the 3.3.5a client (blank button)"), but no guard was added then.

**Three-part fix:**

1. **Data** — node 20122's icon is now `ability_druid_demoralizingroar`, the basename the shipped,
   real-client-verified Vanilla druid node 18717 already uses.
2. **Importer** — `tools/import_tbc_talents.py` gained `_icon_name()`, which strips a **leading**
   `classic_`. Shipped TDD: `test_classic_icon_prefix_is_stripped` (written failing first, then
   passing) plus `test_non_classic_icon_is_untouched` (an icon that merely *contains* the word
   survives verbatim). This matters for the seven unshipped TBC classes — the tooltip cache already
   holds `classic_spell_holy_blessingofprotection` for paladin Holy Shield (41021/20925/41026), so a
   re-import of paladin today would have reintroduced the bug.
3. **Guard** — `tools/era_audit.py` gained `check_icon_names` (check 2b), which FAILS on any node
   icon starting with `classic_`. Verified by running the audit against a deliberately re-broken
   copy of the dataset: exit 1, one finding naming node 20122 and the corrected basename.

   *Why only a prefix check:* a true existence check would need `SpellIcon.dbc` plus the client MPQ
   texture listing, and this repo carries neither (`ip-dbc/` holds only `Spell.dbc`,
   `SkillLine*.dbc` and `SpellItemEnchantment.dbc`). The `classic_` prefix is the only mechanism
   that has actually produced a blank button here, twice, and it is free to check.

Scope confirmed: exactly one shipped node was affected. The two `era-data/_skeletons/` files were
corrected in place too (they are importer output, and the fixed importer would now emit them that
way) — `tbc-druid.skeleton.yaml` and the two paladin Holy Shield rows in `tbc-paladin.skeleton.yaml`.

### 12.3 Defect 3 — Tree of Life (node 20161) showed the WotLK tooltip — `tree_of_life_client_tooltip` CLOSED

**Symptom (user):** the *mechanism* is right (one buff, the TBC one) but "the tooltip for the druid
shows the WOTLK version talking about 6% healing".

**Root cause.** Node 20161 granted **stock 33891**, whose client Description hardcodes cross-spell
references:

> `... you increase healing received by $34123s1% for all party and raid members within $34123a1 yards ...`

The client resolves `$34123s1` / `$34123a1` against the **WotLK 34123 row** (`EffectBasePoints_1 =
5` → 6%, `EffectRadiusIndex_1 = 12` → 100 yd, `APPLY_AREA_AURA_RAID`) — not against the suppressed
value and not against this dataset's prose. The addon hyperlinks a node's granted spell
(`gen_era_talents.py::_grant_is_client_visible`), so the talent panel showed it too. Stock DBC rows
are global and may not be edited, and the `$34123…` references cannot be re-pointed.

**Fix — spec Amendment A.2's ROUTE A, taken on top of the shipped Route B.** New helper **946276**,
a clone of 33891 with its own authored prose; node 20161 grants the clone.

*Mechanically byte-identical.* A field-by-field diff of the two rows in the freshly built client
`Spell.dbc` shows **only** the `ID` and the localized name/description strings differ — every
numeric field matches, `Attributes 327696` / `AttributesEx 98304` / `AttributesEx4 2097152`,
`ManaCostPct 28`, `DurationIndex 21`, `SpellVisualID 8598`, `SpellIconID 2257`, `ActiveIconID 122`,
`NameSubtext "Shapeshift"`, `SpellClassSet 7` / mask B 65536 and — the one that matters for the
stance bar — **`StanceBarOrder 4`** included. The two effects are re-authored verbatim (aura 36
`MOD_SHAPESHIFT` misc 2 = `FORM_TREE`; aura 77 `MECHANIC_IMMUNITY` misc 17 = Polymorph), because the
generator lints that a `template:` helper must author `effects:`. `skillLine: 573` reproduces stock
33891's `SkillLineAbility` row 15089 (SkillLine 573 Restoration, ClassMask 1024) so the spellbook
tab is unchanged. *(The non-enUS localized strings round-trip through a latin-1/UTF-8 re-encode —
pre-existing behavior shared by every shipped clone, e.g. 946258 vs 33878; the enUS strings, which
are what an enUS client reads, are exact.)*

*Why the form mechanic and the suppression survive — code-verified:*

- **`33891` appears nowhere by id in `src/server`** (grep).
- `AuraEffect::HandleShapeshiftBoosts` switches on `GetMiscValue()` — the **aura's** form, not the
  spell id — both to hardcast 34123 (`SpellAuraEffects.cpp:1358`) and, crucially, in its
  `PASSIVE|DO_NOT_DISPLAY` sweep over the player's whole spell map
  (`SpellAuraEffects.cpp:1443-1457`), which is what applies 946265 → 946266. The sweep is entirely
  agnostic to what caused the form.
- `Unit::GetModelForForm` (`Unit.cpp:15477`) hardcodes only 7090 and 35200, then resolves by FORM.
- The suppression gate is unchanged: `era_dru_tree_of_life_suppress` is still bound to stock 34123
  and still keys on `HasSpell(946265)`, which the `kEraDruidTbcPaired` arm still supplies.
- The doctor's 946265 predicate and `Reset()` / `StripOrphanedGrants` all key on the *node's*
  granted spell, which is now the clone — so a respec or a TBC→Vanilla / TBC→WotLK transition strips
  946276 exactly the way it stripped 33891 (`EraTransition::Run` calls `EraTalents::Reset(p, from)`
  on every crossing out of a managed era).

**Migration — new reconcile arm (7)** in `EraTalents.cpp`'s `CLASS_DRUID` block (line 1350, a
sibling of arms 1-6, not nested):

```cpp
if (era < ERA_WOTLK && p->HasSpell(33891))
    p->removeSpell(33891, SPEC_MASK_ALL, false);
```

Every druid the **previous** build granted 33891 to still holds it, and nothing else would remove
it: `StripOrphanedGrants` only considers spells present in some current-era node's `rankSpell`, and
33891 is in none any more. `era < ERA_WOTLK` is load-bearing — 33891 is an ordinary native talent
spell for a WotLK druid.

**Computed tooltip:**

| | Text the client renders |
|---|---|
| BEFORE (stock 33891) | `Shapeshift into the Tree of Life. While in this form you increase healing received by 6% for all party and raid members within 100 yards, …` |
| AFTER (clone 946276) | `Shapeshift into the Tree of Life.  While in this form you increase healing received by all party members within 45 yards by an amount equal to 25% of your total Spirit, …` |
| Buff tooltip BEFORE | `Immune to Polymorph effects. Increases healing received by 6% for all party and raid members within 100 yards.` |
| Buff tooltip AFTER | `Immune to Polymorph effects.  Increases healing received by all party members within 45 yards by an amount equal to 25% of your total Spirit.` |

`_ref.accepted_gaps.tree_of_life_client_tooltip` is recorded as **WITHDRAWN — CLOSED**.

### 12.4 Runtime verification actually run

| Check | Result |
|---|---|
| `tools/era-regen.sh --sync-fork` | clean, generation **`05b215b4`**; client MPQ re-merged (+1 `SkillLineAbility` row = 946276) |
| `tools/era_audit.py` | **0 findings** |
| `tools/era_audit.py` on a deliberately re-broken copy (icon put back to `classic_…`) | exit **1**, 1 finding — the new guard fires |
| `pytest tools/ -q` | **175 passed** (173 + the 2 new importer tests) |
| `docker compose up -d --build ac-worldserver` | built; `db-import` reapplied the 3 changed era SQL files |
| Worldserver boot | `[mod-era-talents] loaded 558 talent nodes (generation 05b215b4)`, no era-talents errors |
| `era_talent_meta` | `generation = 05b215b4` |
| `era_talent_rank` node 20161 | `rank 1 → 946276` |
| `spell_dbc` 946276 | `Effect_1 = 6 / aura 36 / misc 2`, `Effect_2 = 6 / aura 77 / misc 17`, `Attributes 327696`, `ManaCostPct 28`, `DurationIndex 21`, `SpellIconID 2257`, `ActiveIconID 122` |
| `spell_script_names` | `34123 → era_dru_tree_of_life_suppress`, `946266 → era_dru_tree_of_life` (both unchanged) |
| Live TBC druid bot (`Thelanas`, L70) rebuilt to node 20161 via `.eratalents learn` | doctor: `node 20161 rank 1: grant 946276 known=yes spellInfo=yes`, **no ORPHAN lines**; `character_spell` holds **946276 + 946265** and never 33891 |
| Migration arm (7) | proven live: 33891 was injected into `character_spell` for the TBC-band druid bots, the worldserver restarted, then a reconcile (`.eratalents learn`) **removed the row** — 33891 gone, 946276 kept. Test rows cleaned up afterwards |
| `EraTalentIP.cpp` allowlist flip | ` M` (unstaged), **not** in the index |

**One nuance found while proving arm (7):** `EraTalentBots::OnBotLogin` calls `ReapplyOnLogin` but
**not** `ReconcileBaselineSpells` (that is why `StripWrongFactionTbcPalSeal` is invoked separately
there), so a *bot* login alone does not run the strip; a bot's factory build
(`SpendBuild` → `BotBuildScope`), a level change, or any `TryLearn`/`Reset` does. This is the
established shape for every version-swap arm in the file and is not a regression. The **player**
path is unaffected: `era_talent_pin::OnPlayerLogin` calls `EraTalents::ReconcileBaselineSpells`
unconditionally, so a real druid's next login strips 33891.

### 12.5 What still needs the user's eyes

1. **Mangle (Cat) tooltip** — confirm rank 3 with Savage Fury 2/2 now reads **`192% normal damage
   plus 316`** (and Bear still reads `115% damage plus 155`). Computed from the built client DBC,
   not observed.
2. **Feral Aggression icon** — confirm the button is no longer blank. `ability_druid_demoralizingroar`
   is inferred from the shipped, real-client-verified Vanilla druid node 18717; there is no client
   icon listing in this repo to check against.
3. **Tree of Life, the one genuinely unproven thing.** The form is now a custom spell id. Everything
   server-side is proven (see 12.4) and every client field except the strings is byte-identical to
   stock 33891, `StanceBarOrder 4` included — but **no client exists here and bots never
   shapeshift**, so the stance-bar button, the shift itself, and the suppression under the clone
   have not been observed. Please check, in this order: (a) Tree of Life appears in the Restoration
   spellbook tab and on the shapeshift bar; (b) shifting in and out works; (c) party members still
   see exactly one Tree of Life buff and it is the TBC one; (d) the druid's own tooltip now reads
   the 25%-of-Spirit / 45-yard / party text in both the talent panel and the spellbook.
   **If (a) or (b) fails, the revert is one line** — set node 20161's `grants: {1: [33891]}` back in
   `era-data/tbc/druid.yaml`, drop reconcile arm (7), re-run `tools/era-regen.sh --sync-fork`; the
   Route B mechanic never depended on the clone. A working form with a wrong tooltip is strictly
   better than a broken capstone.
4. **The user's own character `Druidtest`** still holds stock 33891 in `character_spell` (granted by
   the previous build). It was deliberately left as-is so their next login exercises arm (7) for
   real — after logging in, `.eratalents doctor Druidtest` should show `node 20161 rank 1: grant
   946276` and there should be exactly one Tree of Life in the spellbook.

---

## 13. Real-client sign-off (2026-08-31) — PHASE 2 CLOSED

The user ran the full Task-10 real-client pass at level 70 / IP stage 8 on generation `b52600c7`,
reported four findings, and signed off on generation **`05b215b4`** after the §12 fix round.

**Confirmed working in a real client** — every item §9 listed as unverified is now observed except
where noted:

- **Tree of Life double-dip suppression WORKS.** Party members see exactly ONE Tree of Life buff
  and it is the TBC one (946266). This was the phase's highest-risk item — Amendment A.2 shipped
  it `HIGH_ON_MECHANISM / UNVERIFIED_AT_RUNTIME`, and the mechanism is now proven in play. **Route
  B is validated; the `accepted_gap` fallback was not needed and no core patch was written.**
- Talent frame, all three tabs, tier 9, the 61-point pool.
- Leader of the Pack, forms, procs, era transitions — all passed with no findings.

**Findings, all fixed in §12:**

| # | Finding | Verdict |
|---|---|---|
| 1 | Mangle (Cat) r3 read `203-231% plus 455` | **Real bug.** Savage Fury listed Mangle (Cat) under BOTH `damage` and `effect3`, applying +20% twice. Fixed by removing Mangle from the `damage` mod — matching stock 16998/16999's own masks, where Blizzard puts Mangle in the EFFECT3 mod only. |
| 2 | Mangle (Bear) r3 read `115% plus 155` | **Not a bug.** The description formula is `$s3% plus ${$m1*$m3/100}`; 135×115/100 = 155 is correct. Recorded so it is not "fixed" later. |
| 3 | Feral Aggression had no icon | **Real bug.** `classic_ability_druid_demoralizingroar` is a Classic-2019 texture absent from 3.3.5a. Fixed at three levels: the node, the importer, and a new `era_audit.py` guard. |
| 4 | Tree of Life tooltip showed WotLK's 6% | **Real bug, mechanism unaffected.** Stock 33891's Description hardcodes `$34123s1`. Closed by cloning the form as 946276 with authored TBC text (Amendment A's Route A, taken only once the tooltip gap was confirmed in play). |

**True final id usage:** `946256-946276` (21 ids) of the druid slice `946256-946511`. **946264 was
allocated by Amendment A.4 and never used** — it is FREE. `946277-946511` free. No DUMMY marker
claimed; registry next-free stays 18.

> **SUPERSEDED by §13** (fix round 2026-08-31): `946277-946280` are now CLAIMED — 946277-946279 the
> Improved Faerie Fire companion debuffs, 946280 the "Era: TBC Heart of the Wild" marker.
> `946281-946511` free. **DUMMY-marker misc 18 IS now consumed; registry next-free is 19.**
> 946264 remains free.

**Residual, accepted as-is:** the accepted-gaps table in §6 stands unchanged (Moonkin Aura scope,
Insect Swarm line, Enrage armor drawback, Force of Nature treants, Predatory Strikes in Moonkin).
The `tree_of_life_client_tooltip` gap is **WITHDRAWN** — closed by §12.

**Status: TBC Phase 2 (Druid) COMPLETE.** Do NOT merge — TBC ships as a set after all nine classes
plus a final all-class review and a Vanilla regression pass.

---

## 13. Tooltip-fidelity audit + fix round (2026-08-31, generation `f7c7b6fd`)

A full **"does the tooltip match what actually happens"** sweep over all 62 nodes, all 141
auto-passives and all helper spells, run after the §12 real-client round. Method: four independent
audits (Balance / Feral / Restoration / the custom-spell layer) cross-checking each authored tooltip
against the generated `spell_dbc` + `spell_proc` rows, the aura/misc semantics **in the fork's own
source**, the mask coverage against the running server's `Spell.dbc`, the wago 2.5.4 reference, the
addon Lua, the live DB, and the built `patch-V.mpq`.

**Result: 59 of 62 nodes clean. Three tooltip-vs-behaviour defects, all fixed here.** The custom
spell layer (helpers, proc wiring, core-script binds on clones, trainer chains, `spell_ranks`,
reconcile arms, generation stamp) came back clean with **zero** defects, as did the whole
Restoration tab.

Every fix below changes **behaviour to match the tooltip**, never the tooltip to match behaviour —
the opposite of the §12.3 / node-20126 / node-20140 treatment, which rewrote prose where the
substrate genuinely could not carry the era's mechanic.

### 13.1 Defect 1 — Nature's Grace (node 20112) never procced on HEAL crits

**Tooltip:** "All spell criticals grace you with a blessing of nature…" (wago 2.5.4 confirms TBC's
text has no "non-periodic" qualifier).

**Root cause.** The `spell_proc` row for carrier 946267 had `ProcFlags 327680`, which is
`DONE_SPELL_MAGIC_DMG_CLASS_NEG (0x10000)` **| `DONE_PERIODIC` (0x40000)** — not what the author
intended. Both this dataset and the Vanilla one document the intent as "DONE_SPELL_MAGIC_DMG_CLASS
**pos+neg** = damage + heal spells", and the Vanilla YAML even spells the arithmetic out as
`POS(262144)`. **262144 is `DONE_PERIODIC`; the real `..._POS` bit is `0x4000` = 16384**
(`SpellMgr.h:131`). So a resto druid's Healing Touch/Regrowth crits — half of what the talent is for
— could never fire it, while periodic ticks (which TBC could not crit at all) could.

**Fix.** `flags: 81920` (= 65536 | 16384) in **both** `era-data/tbc/druid.yaml` (helper 946267) and
`era-data/vanilla/druid.yaml` (node 18712). The bogus periodic bit is **dropped, not kept**: it was
never intended, and periodic crits are a WotLK-ism. `SpellTypeMask 3` and `HitMask 2` survive the
generator's own usability gate under the new flags.

**Scope — this shipped in VANILLA too.** Node 18712's row (925696) carried the identical value and
is **on `master`**, so this fix repairs a live Vanilla bug as well. Verified the same 327680 value
elsewhere is NOT this bug: priest Blackout and Shadow Weaving genuinely want damage+periodic (no
heal component), and their comments say so.

**Proof (regenerated rows, only the flags field moves):**

```
- (925696, …, 327680, 3, 2, 2, …)   +  (925696, …, 81920, 3, 2, 2, …)   [Vanilla]
- (946267, …, 327680, 3, 2, 2, …)   +  (946267, …, 81920, 3, 2, 2, …)   [TBC]
```

### 13.2 Defect 2 — Improved Faerie Fire (node 20118) gave SPELL hit, not melee/ranged

**Tooltip:** "…increases the chance the target will be hit by melee and ranged attacks by 1/2/3%."

**Root cause.** TBC implements this as two SPELLMODs onto Faerie Fire's **own** EFFECT_2/EFFECT_3,
which in TBC are aura184 `MOD_ATTACKER_MELEE_HIT_CHANCE` and aura185
`MOD_ATTACKER_RANGED_HIT_CHANCE` at base 0. On 3.3.5a **that substrate does not exist**: stock 770
has no Effect_2 at all, and its Effect_3 is aura**186** `MOD_ATTACKER_SPELL_HIT_CHANCE`. The shipped
node authored both spellmods anyway, so the `effect2` half was wholly inert and the `effect3` half
silently granted **+spell hit** — the tooltip promised two things and runtime delivered neither.
(This was a known, documented `_ref.accepted_gaps` entry; what made it a defect rather than a gap is
that node 20118 is a non-grant node, so the authored prose IS what the player reads — unlike the
grant nodes, where the client hyperlinks the real spell's tooltip.)

**Fix — deliver the hit chance as a companion debuff.** The node becomes ONE effect: `aura: 109`
`SPELL_AURA_ADD_TARGET_TRIGGER`, chance 100, masked to A=1024, triggering new clones
**946277/946278/946279**, which carry the real aura184 + aura185 at +1/2/3% and are read off the
victim by `Unit::GetUnitMissChance` (`Unit.cpp:3839`, separate melee and ranged reads, positive
amount = easier to hit). Verified `ADD_TARGET_TRIGGER` fires on **any landed** affected spell — no
damage or dmg-class requirement — so a pure debuff application like Faerie Fire triggers it
(`Spell::DoTriggersOnSpellHit`), and that mask bit 1024 is carried by **exactly** 770 and 16857 in
the whole DBC (no over-match).

**Why NOT the obvious fix of cloning Faerie Fire with TBC's effect layout:** a clone of 16857 loses
two by-id core behaviours — `Spell.cpp:8869` pre-casts 60089 only when `Id == 16857`, which is
**what lets a bear cast Faerie Fire (Feral) without leaving form**, and `SpellInfoCorrections.cpp`
clears an attribute by id. That trades a tooltip bug for a broken bear-form cast. It is the same
reasoning that already keeps node 20133 on stock 16857.

Two traps found and closed while authoring the clones:

* **`attributes` must be overridden, not inherited** — stock 770 carries 65536
  `SPELL_ATTR0_NOT_SHAPESHIFTED` (the very reason Faerie Fire (Feral) exists as a separate spell).
  Set to 67108864 `SPELL_ATTR0_AURA_IS_DEBUFF`, needed because these amounts are POSITIVE numbers
  that are BAD for the target.
* **`attributesEx: 0` — the clone-of-a-corrected-spell trap.** 770's AttributesEx 98304 includes
  `SPELL_ATTR1_IMMUNITY_TO_HOSTILE_AND_FRIENDLY_EFFECTS`, which
  `SpellInfoCorrections.cpp:1266` **clears from 770/16857 BY ID** — a correction no clone receives.
  Left inherited, the companion debuff could land on a target whose school immunity blocks the
  Faerie Fire it accompanies (`Spell.cpp:6967` reads the bit on the aura being applied). Closing
  this needed a new `attributesEx` generator scalar.

**New divergence introduced by the fix, and closed:** because the cast id now varies with talent
rank, two druids with DIFFERENT ranks would land two unrelated spell ids that both carry
aura184/185 and **stack** (+4% from a rank-3 plus a rank-1). Stock Faerie Fire cannot do this — every
druid casts one id. Fixed with a hand `spell_ranks` chain rooted at 946277
(`2026_08_31_05_era_talent_tbc_druid_iff_ranks.sql`), so `RemoveNoStackAurasDueToAura` replaces
rather than adds.

### 13.3 Defect 3 — Heart of the Wild (node 20135) gave HALF the promised bear Stamina

**Tooltip:** Intellect +4/8/12/16/20%, Bear/Dire Bear **Stamina +4/8/12/16/20%**, Cat attack power
+2/4/6/8/10%.

**Root cause — a magnitude that lives in CORE CODE, not spell data.** The granted stock chain
(17003-17006/24894) is byte-identical between TBC and live, so the effect-diff pipeline every era
dataset is built from could not see anything wrong. But the per-form halves are not in the spell at
all: `AuraEffect::HandleShapeshiftBoosts` scans for the Intellect aura by `SpellIconID == 240` and
casts serverside 24899 (bear) / 24900 (cat) with **`HotWMod = amount / 2`**
(`SpellAuraEffects.cpp:1519`) — WotLK's retune. So bear Stamina came out **2/4/6/8/10%**, half the
tooltip at every rank. Cat attack power is *also* halved and is *correct* — TBC's cat value really
is 2/4/6/8/10%.

**Why this could not be fixed in data.** The halved value derives from the **stock, global**
Intellect aura, and eras are per-character, so the source cannot be edited. A second stance-gated
Stamina top-up cannot reach the numbers either: `MOD_TOTAL_STAT_PERCENTAGE` auras combine
**MULTIPLICATIVELY** (`Unit::GetTotalAuraMultiplier` → `AddPct`, `Unit.cpp:6252`), so the exact
top-ups are 1.96/3.85/5.66/7.41/**9.09**% — non-integer, and the nearest integers land **19.90%** at
rank 5 instead of 20%.

**Fix — core patch 0023** (framework decision-tree step 6, the escape hatch), the same marker idiom
as 0018/0019: the module grants hidden marker **946280** (`SPELL_AURA_DUMMY`, misc **18**) via a new
`EraTalents.cpp` CLASS_DRUID reconcile arm (8), gated on era TBC **and** node 20135 rank ≠ 0 (so a
respec drops it); the patch skips the halving for the **BEAR** spell 24899 only when that marker is
present. Node 20135's YAML is unchanged, so no `grants:`+`mechanic:` combination was needed (the
`era_talent_rank` PK is one grant per rank).

**Deliberately scoped:** Cat (24900) is untouched, and the arm never arms outside TBC — **Vanilla's
own node 18730 tooltip states the halved 2/4/6/8/10% Stamina and is therefore already correct.**
`tools/regen-hotw-patch.sh` gates against a future diff that adds code touching 24900.

`Auras/SpellAuraEffects.cpp` is touched by **no other patch**, so the regen is a pristine single-file
diff and is NOT baseline-aware. NB a loose `grep -l SpellAuraEffects patches/*.patch` matches 0019
and 0021 — but only on their `#include "SpellAuraEffects.h"` context lines. Match the full path.

### 13.4 Generator capabilities added (all TDD-covered, byte-neutral for shipped data)

Verified byte-neutral by re-running `era-regen.sh` and confirming only the three intended files move.

| Capability | Why |
|---|---|
| `trigger:` on a `type: stat` effect (scalar or per-rank list) | emit `EffectTriggerSpell_i` + `ImplicitTargetA_i=1` for aura 109 |
| `affects:`/`affectsMask:` on a `type: stat` effect | aura 109 is scoped BY mask |
| `SpellClassSet` emitted when **any** effect carries a mask (was: spellmods only) | **R1, the severe one** — `SpellInfo::IsAffected` opens `if (!familyName) return true;`, so a family-0 masked effect matches EVERY spell and the companion debuff would land on every cast |
| hard lint: `aura: 109` without a mask, without a trigger, or with a wrong-length trigger list | makes the above unauthorable rather than silently wrong |
| `attributesEx` `_HELPER_SCALARS` entry | override an ATTR1 bit the core clears from the template BY ID (§13.2) |

### 13.5 Verification run

| Check | Result |
|---|---|
| `tools/era_audit.py` | **0 findings** |
| `pytest tools/ -q` | **182 passed** (was 173; +9 new) |
| `tools/era-regen.sh` | clean; only the 3 intended SQL files changed |
| Generation stamp (DB SQL / `era_talent_meta` / MPQ sentinel) | **`f7c7b6fd`**, no drift |
| Client MPQ | +562 custom client spell rows (+3: the 946277-279 debuffs; 946280 has no client row) |
| Patch 0023 round-trip | `git apply -R` → `--check` → re-apply all clean; 1 file, 17 insertions |
| Vanilla druid SQL diff | exactly ONE line (the 925696 proc flags) |
| Addon Lua | unchanged, correctly — node 20118's tooltip and rank count did not move, and `effect2`/`effect3` are not `DISCRETE_OPS` so its spellMods map was always empty |

### 13.6 What is NOT verified — real-client items for the next pass

None of the three fixes has been observed in play; all are code/data-verified only. Priority list:

1. **Nature's Grace on a HEAL crit** — the actual defect. Crit a Regrowth/Healing Touch and confirm
   the Clearcasting-style buff appears (and still appears on a Wrath/Starfire crit).
2. **Improved Faerie Fire** — cast Faerie Fire and confirm the companion debuff lands and reads
   "+N% chance to be hit by melee and ranged"; then cast **Faerie Fire (Feral) from BEAR form** and
   confirm it still lands (the bear-form path is the reason cloning was rejected). Confirm it is
   dispellable with the Faerie Fire it accompanies.
3. **Heart of the Wild** — in Bear Form with 5/5, confirm Stamina is +20% (not +10%), and that Cat
   Form attack power stays +10%. Confirm a respec to 0/5 drops marker 946280 and the bonus.
4. Regression: a **Vanilla** druid's Nature's Grace still procs, and a **native WotLK** druid's
   Heart of the Wild is unchanged (the marker must never arm outside TBC).

**Status: fix round authored and headless-verified; real-client pass signed off; merged to master 2026-09-07.**
