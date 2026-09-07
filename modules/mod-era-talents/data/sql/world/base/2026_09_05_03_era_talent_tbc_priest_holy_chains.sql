-- TBC Priest, Phase 6 Task 7 (Holy tab). Circle of Healing ranks 2-5 and Lightwell ranks 2-4 for a
-- TBC-era priest, as trainer-taught era clones.
-- HAND-WRITTEN (not generated) — same shape as the Discipline chain in this phase
-- (2026_09_05_02_era_talent_tbc_priest_divine_spirit_chain.sql), the TBC rogue Mutilate chain
-- (2026_09_04_02) and the TBC druid Mangle chain (2026_08_31_03). Idempotent: DELETE-before-INSERT
-- on every table.
--
-- This server loads trainers from the modern `trainer`/`trainer_spell` tables (ObjectMgr.cpp reads
-- "SELECT ... FROM trainer_spell"), NOT the legacy `npc_trainer` table. THE PRIEST TRAINER SET IS
-- `trainer.Id` = 11 (Type 0, Requirement 5 = CLASS_PRIEST). `trainer.Id` = 12 is a SECOND priest
-- set with the identical greeting, but it is a five-row STARTER set (Power Word: Shield r1 17,
-- Shadow Word: Pain 589, Lesser Heal r2 591, Power Word: Fortitude 1243, Lesser Heal r1 2052) and
-- carries NO Circle of Healing or Lightwell row at all, so naming only TrainerId 11 below leaves
-- nothing behind. `.reload trainer` (or a restart) applies this file live.
--
-- WHY THE CLONES EXIST (era-data/tbc/priest.yaml helpers 948046-948054, nodes 20542 and 20539,
-- spec section 4d + Amendment A.3, era-data/_ref/tbc/priest-spells.yaml capstone_chains):
--   * CIRCLE OF HEALING (node 20542, the Holy tier-9 capstone) diverges on five families. TBC heals
--     the friendly target and THAT TARGET'S PARTY (ImplicitTargetA 21 / B 37) for 246-270 .. 409-451
--     at a FLAT 300/337/375/412/450 mana and NO cooldown; live is raid-smart (A 63 / B 31), heals
--     ~40% more, costs ManaCostPct 24/24/24/24/21 and carries Category 1204 with a 6-second
--     CategoryRecoveryTime.
--   * LIGHTWELL (node 20539) diverges on four. TBC's is a SIX-minute category cooldown with a 1.5 s
--     cast and flat 225/295/365/445 mana; live is three minutes, a 0.5 s cast, and ManaCostPct 17 on
--     rank 4. (TBC also summoned a GAMEOBJECT where live summons a creature; spec Amendment A.3
--     deliberately keeps the LIVE creature shape so the working click->heal chain survives — that is
--     a dataset decision, not a trainer one.)
--
-- Node 20542 grants Circle of Healing clone rank 1 (948046) and node 20539 grants Lightwell clone
-- rank 1 (948051). This file teaches the rest behind ReqAbility1 chains rooted at those grants.
-- IT DELETES NOTHING AND RE-GATES NOTHING.

-- =====================================================================================================
-- (a) THE STOCK CIRCLE OF HEALING AND LIGHTWELL TRAINER ROWS ARE LEFT COMPLETELY ALONE — and here is
--     the orphaned-higher-rank check that says they must be (docs/era-talents-framework.md
--     "Deleting a stock trainer row: the orphaned-higher-rank check (MANDATORY)"), plus the intent
--     test that says no deletion is warranted in the first place.
--
-- The stock TrainerId-11 chains, read off the live table 2026-09-05 (they match
-- _ref capstone_chains.circle_of_healing.live_trainer_rows / .lightwell.live_trainer_rows exactly):
--     34863 ReqLevel 56 ReqAbility1 34861  MoneyCost 2100     <-- Circle of Healing r2
--     34864 ReqLevel 60 ReqAbility1 34863  MoneyCost 2300     <-- r3
--     34865 ReqLevel 65 ReqAbility1 34864  MoneyCost 4000     <-- r4
--     34866 ReqLevel 70 ReqAbility1 34865  MoneyCost 7000     <-- r5
--     48088 ReqLevel 75 ReqAbility1 34866  MoneyCost 9000     <-- r6, WotLK-only
--     48089 ReqLevel 80 ReqAbility1 48088  MoneyCost 9000     <-- r7, WotLK-only
--   (34861 itself has NO `trainer_spell` row on ANY TrainerId.)
--     27870 ReqLevel 50 ReqAbility1   724  MoneyCost 1200     <-- Lightwell r2
--     27871 ReqLevel 60 ReqAbility1 27870  MoneyCost 1500     <-- r3
--     28275 ReqLevel 70 ReqAbility1 27871  MoneyCost 1500     <-- r4
--     48086 ReqLevel 75 ReqAbility1 28275  MoneyCost 9000     <-- r5, WotLK-only
--     48087 ReqLevel 80 ReqAbility1 48086  MoneyCost 9000     <-- r6, WotLK-only
--   (724 itself has NO `trainer_spell` row on ANY TrainerId.)
--
--   1. ORPHANED-HIGHER-RANK CHECK: each chain hangs off ReqAbility1 = its own rank 1. Deleting or
--      re-pointing 34863 would strand 34864 -> 34865 -> 34866 -> 48088 -> 48089, and deleting or
--      re-pointing 27870 would strand 27871 -> 28275 -> 48086 -> 48087, for every NATIVE WotLK
--      priest (who never touches the era system). There is no compensating grant for any of those
--      ids in this module. That is exactly the regression the first revision of the druid Mangle
--      migration shipped and had to reverse (2026_08_31_03 section (a)), and the standing lesson
--      `deleting-a-stock-trainer-row-orphans-higher-ranks`.
--   2. THE DELETION WOULD ALSO BE POINTLESS, because both stock chains ALREADY self-gate against a
--      TBC-era priest — framework option 3, the preferred outcome. Each chain's root requirement is
--      its stock rank 1 (34861 / 724), which the era node no longer grants (it grants clone 948046 /
--      948051) and which has no `trainer_spell` row of its own on any trainer, so a TBC priest can
--      never buy it either. `kBaselineSpellGates` does not force-grant either id (the only two
--      priest gate rows are Holy Nova 18121 and Divine Spirit 18113). EraTalents.cpp's CLASS_PRIEST
--      TBC arms (3) and (3b) additionally strip stock 34861/724 and their trained ranks from a TBC
--      priest, readiness-gated on EraNodeReplacesStock, covering a character migrated from an older
--      build. Unable to satisfy ReqAbility1, a TBC priest cannot see or buy any stock rank.
--   3. TRAINER-GATING INTENT TEST (the Devouring Plague rule): a stock `trainer_spell` row is a
--      "hole" only if the module gates that spell somewhere and a missed set leaks it. Neither
--      Circle of Healing nor Lightwell is gated anywhere in this module other than by the node grant
--      itself, so there is no gate for a trainer row to leak past.
--      _ref capstone_chains.circle_of_healing.orphan_check and .lightwell.orphan_check reach the
--      same "self-gating already; add clone rows and touch nothing stock" conclusion independently.
-- Net: no DELETE, no UPDATE, no re-gate on any stock row. Do not "tidy" this by adding one.

-- =====================================================================================================
-- (b) Circle of Healing clone ranks 2-5 and Lightwell clone ranks 2-4, trainer-taught, era-gated by
--     a require-previous-rank chain rooted at the TALENT-granted rank 1 (948046 / 948051).
--
-- Era gate: rank 1 of each chain exists ONLY as the TBC era-talent clone. A Vanilla-era priest has
-- neither talent (the Vanilla tree has no Circle of Healing and its Lightwell node grants stock 724),
-- and a WotLK priest trains the stock chains, so neither ever holds 948046 or 948051 — requiring
-- them via ReqAbility1 makes these ladders reachable ONLY by a TBC-era priest who spent the talent.
-- This is the talent-anchor flavour of the trainer gate documented in
-- docs/era-talents-framework.md ("Era-gating CUSTOM spell ranks that REPLACE a stock WotLK version",
-- anchor flavour 1) — no marker spell is needed, exactly as for the TBC Divine Spirit, rogue
-- Mutilate and druid Mangle / Nature's Grasp chains.
--
-- ReqLevel 56/60/65/70 and 50/60/70 are corroborated three ways: the wago 2.5.4 `SpellLevels` for
-- 34863/34864/34865/34866 and 27870/27871/28275 (family J of the survival sweep, which found ZERO
-- TBC-vs-WotLK trainer-level drift anywhere in this class), the live stock rows above, and
-- _ref capstone_chains.circle_of_healing.tbc_levels / .lightwell.tbc_levels. MoneyCost is copied
-- VERBATIM from the stock rows above.
--
-- A spell used as a `trainer_spell.ReqAbility1` MUST have a client `Spell.dbc` name row or the
-- WotLK trainer UI Lua-errors the whole window empty (framework "Client visibility"; the paladin
-- 946078 bug). Every id referenced here — 948046 through 948054 — carries a `client:` block in
-- era-data/tbc/priest.yaml, so they all ship a client row in patch-V.mpq.
DELETE FROM `trainer_spell` WHERE `TrainerId`=11 AND `SpellId` IN (948047,948048,948049,948050,948052,948053,948054);
INSERT INTO `trainer_spell`
  (`TrainerId`,`SpellId`,`MoneyCost`,`ReqSkillLine`,`ReqSkillRank`,`ReqAbility1`,`ReqAbility2`,`ReqAbility3`,`ReqLevel`,`VerifiedBuild`) VALUES
  (11, 948047, 2100, 0, 0, 948046, 0, 0, 56, 0),   -- Circle of Healing Rank 2 (287-318): req talent-granted R1 948046
  (11, 948048, 2300, 0, 0, 948047, 0, 0, 60, 0),   -- Circle of Healing Rank 3 (327-361): req R2 948047
  (11, 948049, 4000, 0, 0, 948048, 0, 0, 65, 0),   -- Circle of Healing Rank 4 (370-408): req R3 948048
  (11, 948050, 7000, 0, 0, 948049, 0, 0, 70, 0),   -- Circle of Healing Rank 5 (409-451): req R4 948049
  (11, 948052, 1200, 0, 0, 948051, 0, 0, 50, 0),   -- Lightwell          Rank 2 (1164 hp): req talent-granted R1 948051
  (11, 948053, 1500, 0, 0, 948052, 0, 0, 60, 0),   -- Lightwell          Rank 3 (1599 hp): req R2 948052
  (11, 948054, 1500, 0, 0, 948053, 0, 0, 70, 0);   -- Lightwell          Rank 4 (2361 hp): req R3 948053

-- =====================================================================================================
-- (c) TWO `spell_ranks` chains, mirroring the stock pair: 948046 -> 948047 -> 948048 -> 948049 ->
--     948050 (Circle of Healing r1-r5) and 948051 -> 948052 -> 948053 -> 948054 (Lightwell r1-r4).
--     Three jobs, all load-bearing here:
--   * the client spellbook collapses same-name / same-skill-line ranks (SkillLine 56 Holy, from each
--     clone's `client.skillLine`) to the highest known, instead of showing five separate Circle of
--     Healing buttons;
--   * `Player::removeSpell` walks GetNextSpellInChain, so Reset()'s removeSpell(948046) /
--     removeSpell(948051) on respec CASCADES and strips every trained rank — the same respec-clean
--     property the Divine Spirit (948024-028), Mutilate (947773-776) and Mangle (946258-260) chains
--     rely on;
--   * a higher rank cleanly SUPERSEDES the lower for the bot AI's rank walks and for the client's
--     "next rank available" logic.
--
-- RESPEC / BAND-MOVE STRIP: unlike the Prayer of Spirit half of the Discipline chain, BOTH chains
-- here root at a NODE GRANT, so no extra reconcile arm is needed and none is added. A respec clears
-- the whole chain through the `spell_ranks` cascade above plus `StripOrphanedGrants` (which sees any
-- node grant); an era-band move out of TBC is covered by the same cascade for players (EraTransition
-- runs Reset(from)) and by EraTalentBots::FactoryReconcile's teardown-first managed-row strip for
-- bots. The `ReqAbility1` chain in section (b) is authoring-time gating only (who can BUY the next
-- rank); it has no runtime removal role — core `Player.cpp`'s `spell_required` walk only fires when
-- `GetTalentSpellCost(firstRankSpellId) > 0`, and DBCStores returns 0 for any id outside Talent.dbc,
-- which every id in these chains is.
--
-- NOTE the interaction with `first_spell_id` = 948046 / 948051: both ARE plain node grants
-- (era_talent_rank) and NOT ReconcileBaselineSpells-managed rows, so these rank chains cannot fight
-- the reconcile. (Contrast the STOCK 34861/724: had the era ranks been hung off a gate-managed stock
-- root, the gate's strip and the chain's cascade would have raced.) Identical situation to the TBC
-- Divine Spirit 948024, rogue Mutilate 947773 and druid Nature's Grasp 946270 chains.
-- SpellMgr::LoadSpellRanks requires every id in a chain to have a `spell_dbc` record; these do
-- (2026_09_05_01_era_talent_tbc_priest_custom_spells.sql, generated).
-- `spell_ranks.spell_id` is UNIQUE, so delete by first_spell_id before re-inserting.
DELETE FROM `spell_ranks` WHERE `first_spell_id` IN (948046, 948051);
INSERT INTO `spell_ranks` (`first_spell_id`,`spell_id`,`rank`) VALUES
  (948046, 948046, 1),
  (948046, 948047, 2),
  (948046, 948048, 3),
  (948046, 948049, 4),
  (948046, 948050, 5),
  (948051, 948051, 1),
  (948051, 948052, 2),
  (948051, 948053, 3),
  (948051, 948054, 4);

-- =====================================================================================================
-- (d) NO `spell_script_names` ROWS ARE ADDED BY THIS FILE, AND THAT IS A DELIBERATE, MEASURED CALL
--     ON BOTH CHAINS. (The dataset's own `scriptBindings:` is likewise untouched by this task.)
--
--   * CIRCLE OF HEALING — the core script `spell_pri_circle_of_healing` (bound to `-34861`, i.e. the
--     whole stock rank chain) is NOT rebound onto 948046-948050. Its only hook is
--     `OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(..., EFFECT_0,
--     TARGET_UNIT_DEST_AREA_ALLY)` (spell_priest.cpp:197) — target type 31, which is live CoH's
--     ImplicitTargetB. The era clones carry TBC's own ImplicitTargetB 37
--     (TARGET_UNIT_LASTTARGET_AREA_PARTY), so `SpellScript::TargetHook::CheckEffect`
--     (SpellScript.cpp:242) would reject the hook and `SpellScript::_Validate` (SpellScript.cpp:334)
--     would LOG_ERROR "Spell `948046` Effect ... did not match dbc effect data" once per rank per
--     boot while the handler never ran. And the handler is not needed: its body is a RaidCheck
--     filter plus `targets.resize(5)`, a cap WotLK requires because target type 31 selects the whole
--     raid around the destination, whereas TBC's type 37 is `TARGET_CHECK_PARTY` referenced off the
--     healed ally and is structurally limited to that ally's party. Its other branch (Glyph of
--     Circle of Healing widening 5 -> 6) is inert for an era priest under `EraGlyphGate`.
--   * LIGHTWELL — neither `spell_pri_lightwell` nor `spell_pri_lightwell_renew` is bound to the
--     SUMMON spell on either side, so there is nothing to rebind. Measured on the live DB
--     2026-09-05: `spell_script_names` carries 60123 -> spell_pri_lightwell (the summoned creature's
--     own click spell) and -7001 -> spell_pri_lightwell_renew (the Renew rank chain), and NO row at
--     all for 724/27870/27871/28275. Because the clones keep live's Effect 28 creature summon
--     (spec Amendment A.3), the summoned NPC is the same 31897/31896/31895/31894 and both scripts
--     keep working untouched.
--     ACCEPTED GAP (recorded as _ref accepted_gaps id 9): keeping the creature shape also keeps the
--     WotLK charge count and cancel rule — `SPELL_PRIEST_LIGHTWELL_CHARGES` 59907 carries
--     ProcCharges 10 where TBC's tooltip promises 5, and `spell_pri_lightwell_renew` breaks the HoT
--     at 30% of the clicker's health where TBC broke it on any damage. The clones' client
--     descriptions state what actually ships rather than what TBC promised. The per-rank heal is NOT
--     part of the gap: the Lightwell Renew chain 7001/27873/27874/28276 is magnitude-identical
--     between TBC and live (267/388/533/787 per 2 s tick x 3 ticks = 801/1164/1599/2361).
