-- TBC Warrior, Phase 4 Task 7 (Fury tab): the two trainer-taught clone chains this tab introduces —
-- Bloodthirst ranks 2-6 (947513-947517) and Rampage ranks 2-3 (947534/947535) — plus their
-- spell_ranks chains. HAND-WRITTEN (not generated), mirroring the shipped Vanilla Bloodthirst file
-- 2026_08_18_20_era_talent_warrior_bloodthirst_trainer.sql and the TBC druid Mangle chain
-- 2026_08_31_03_era_talent_tbc_druid_mangle_trainers.sql. Idempotent: DELETE-before-INSERT.
--
-- This server loads trainers from the modern `trainer`/`trainer_spell` tables (ObjectMgr.cpp reads
-- `trainer_spell`), NOT the legacy `npc_trainer` table. The MAIN warrior trainer is `trainer.Id`=1
-- (Requirement=1 = CLASS_WARRIOR, 133 spells; `trainer.Id`=2 is a 6-spell starter and is not used).
-- Verified 2026-09-03 by querying the stock warrior talent-ability chains: every Mortal Strike,
-- Shield Slam and Devastate trained rank on this server sits on TrainerId 1.
-- `.reload trainer` (or a restart) applies it live.
--
-- ==================================================================================================
-- NOTHING IS DELETED OR RE-GATED HERE, and that is a verified property rather than an oversight.
-- The framework's orphaned-higher-rank check (docs/era-talents-framework.md) is MANDATORY before any
-- DELETE; it does not apply because there is no stock row to delete:
--   * Bloodthirst  — stock 23881/23892/23893/23894/25251/30335 have NO `trainer_spell` rows and NO
--     `spell_ranks` rows on this server at all (queried at plan Task 3; the shipped Vanilla
--     migration's own header records the same finding for the ranks Vanilla reaches). Stock
--     Bloodthirst is talent-granted in the stock game, so there is nothing to buy and nothing to
--     strand.
--   * Rampage      — stock 29801 is a single-rank WotLK PASSIVE; 30030/30031/30032/30033 do not
--     exist in the live store at all (WAGO_ONLY), so there is likewise no trainer row and no rank
--     chain to disturb.
-- Both are therefore pure INSERTs with zero exposure for native WotLK warriors.
--
-- ERA GATE = the talent-anchor flavor (framework: "Era-gating CUSTOM spell ranks that REPLACE a
-- stock WotLK version", anchor flavor 1 — no marker spell needed). Rank 1 of each chain exists ONLY
-- as a TBC era-talent clone: 947512 (node 20340 Bloodthirst) and 947533 (node 20343 Rampage). A
-- Vanilla-era warrior gets 932828 for Bloodthirst and has no Rampage talent at all; a WotLK warrior
-- gets stock 23881 / the stock 29801 passive. Neither ever holds 947512/947533, so a `ReqAbility1`
-- rooted there makes the whole ladder reachable ONLY by a TBC-era warrior who spent the talent.
-- ==================================================================================================

-- --------------------------------------------------------------------------------------------------
-- (a) Bloodthirst ranks 2-6, trainable at the TBC levels.
--
-- Levels are TBC's own SpellLevels for the chain (48 / 54 / 60 / 66 / 70), taken from wago 2.5.4
-- SpellLevels for 23892/23893/23894/25251/30335 and corroborated by the cached Wowhead TBC tooltips
-- (era-data/_ref/tbc/tooltips/30335.json prints "Requires level 70"). Ranks 2-4 reuse the Vanilla
-- file's levels and MoneyCost verbatim (48/54/60 at 4000/6000/8000) because TBC did not move them;
-- ranks 5-6 are new to TBC and continue the same gentle curve.
-- Per-rank heal (10/13/17/20/25/30) and the 45%-AP / 30-rage / 6-second damage half are authored in
-- era-data/tbc/warrior.yaml (helpers 947512-947529).
DELETE FROM `trainer_spell` WHERE `TrainerId`=1 AND `SpellId` IN (947513,947514,947515,947516,947517);
INSERT INTO `trainer_spell`
  (`TrainerId`,`SpellId`,`MoneyCost`,`ReqSkillLine`,`ReqSkillRank`,`ReqAbility1`,`ReqAbility2`,`ReqAbility3`,`ReqLevel`,`VerifiedBuild`) VALUES
  (1, 947513,  4000, 0, 0, 947512, 0, 0, 48, 0),   -- Rank 2 (heal 13): req talent-granted R1 947512
  (1, 947514,  6000, 0, 0, 947513, 0, 0, 54, 0),   -- Rank 3 (heal 17): req R2 947513
  (1, 947515,  8000, 0, 0, 947514, 0, 0, 60, 0),   -- Rank 4 (heal 20): req R3 947514
  (1, 947516, 10000, 0, 0, 947515, 0, 0, 66, 0),   -- Rank 5 (heal 25): req R4 947515  [TBC-only rank]
  (1, 947517, 12000, 0, 0, 947516, 0, 0, 70, 0);   -- Rank 6 (heal 30): req R5 947516  [TBC-only rank]

-- --------------------------------------------------------------------------------------------------
-- (b) Rampage ranks 2-3, trainable at 60 / 70.
--
-- Levels are TBC's own (wago 2.5.4 SpellLevels: 29801 = 50, 30030 = 60, 30033 = 70) and the cached
-- Wowhead TBC tooltips for 30030/30033 print "Requires level 60" / "Requires level 70" verbatim.
-- Rank 1 is the talent grant at 50 and is not sold. MoneyCost follows the same curve as the
-- Bloodthirst ranks at the same levels.
-- The per-rank attack power (30 / 40 / 50 per stack, 5 stacks) is authored in
-- era-data/tbc/warrior.yaml (helpers 947533-947540).
DELETE FROM `trainer_spell` WHERE `TrainerId`=1 AND `SpellId` IN (947534,947535);
INSERT INTO `trainer_spell`
  (`TrainerId`,`SpellId`,`MoneyCost`,`ReqSkillLine`,`ReqSkillRank`,`ReqAbility1`,`ReqAbility2`,`ReqAbility3`,`ReqLevel`,`VerifiedBuild`) VALUES
  (1, 947534,  8000, 0, 0, 947533, 0, 0, 60, 0),   -- Rank 2 (+40 AP/stack): req talent-granted R1 947533
  (1, 947535, 12000, 0, 0, 947534, 0, 0, 70, 0);   -- Rank 3 (+50 AP/stack): req R2 947534

-- --------------------------------------------------------------------------------------------------
-- (c) spell_ranks chains, one per ability, each rooted at the TALENT-granted rank 1.
--
-- Three jobs, all of them load-bearing:
--   1. the client spellbook collapses same-name ranks to the highest known (which is why every rank's
--      `client:` block carries a `rank:` label);
--   2. a higher-rank cast cleanly overwrites the lower rank's buff instead of stacking with it —
--      Bloodthirst's 5-charge heal buff and Rampage's 5-stack attack-power buff both need this;
--   3. `EraTalents::Reset`'s removeSpell(rank 1) CASCADES via GetNextSpellInChain, so a respec strips
--      the trained ranks too. The same cascade is what makes the CLASS_WARRIOR reconcile arms' single
--      removeSpell(947512) / removeSpell(947533) clear a whole ladder on an era transition.
-- The core's LoadSpellRanks requires every id in a chain to have a spell_dbc record; these custom
-- rows do (generated into 2026_09_03_01_era_talent_tbc_warrior_custom_spells.sql, which db-import
-- applies before this file — the numeric filename order is the apply order).
-- `spell_id` is UNIQUE, so the chains are deleted by first_spell_id before re-inserting.
DELETE FROM `spell_ranks` WHERE `first_spell_id` IN (947512,947533);
INSERT INTO `spell_ranks` (`first_spell_id`,`spell_id`,`rank`) VALUES
  (947512, 947512, 1),
  (947512, 947513, 2),
  (947512, 947514, 3),
  (947512, 947515, 4),
  (947512, 947516, 5),
  (947512, 947517, 6),
  (947533, 947533, 1),
  (947533, 947534, 2),
  (947533, 947535, 3);
