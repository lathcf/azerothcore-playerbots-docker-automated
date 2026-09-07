-- TBC Warrior, Phase 4 Task 8 (Protection tab): the two trainer-taught clone chains this tab
-- introduces — Devastate ranks 2-3 (947531/947532) and Shield Slam ranks 2-6 (947542-947546) —
-- plus their spell_ranks chains. HAND-WRITTEN (not generated), mirroring Task 7's Fury file
-- 2026_09_03_02_era_talent_tbc_warrior_fury_chains.sql and the TBC druid Mangle chain
-- 2026_08_31_03_era_talent_tbc_druid_mangle_trainers.sql. Idempotent: DELETE-before-INSERT.
--
-- This server loads trainers from the modern `trainer`/`trainer_spell` tables (ObjectMgr.cpp reads
-- `trainer_spell`), NOT the legacy `npc_trainer` table. The MAIN warrior trainer is `trainer.Id`=1
-- (Requirement=1 = CLASS_WARRIOR); `trainer.Id`=2 is a 6-spell starter and is not used. Verified
-- again this task: every stock Shield Slam and Devastate trained rank on this server sits on
-- TrainerId 1. `.reload trainer` (or a restart) applies it live.
--
-- ==================================================================================================
-- NOTHING IS DELETED OR RE-GATED HERE, and that is a VERIFIED PROPERTY, not an oversight. The
-- framework's orphaned-higher-rank check is MANDATORY before any DELETE; the correct resolution here
-- is framework OPTION 3 — "the stock chain is already self-gating, so the DELETE was unnecessary;
-- remove it rather than adding a compensating grant" — for BOTH chains:
--
--   * Devastate    stock 30016.ReqAbility1 = stock **20243**, and 30022.ReqAbility1 = 30016. Node
--                  20365 grants the CLONE 947530, never stock 20243, so a TBC warrior can never
--                  satisfy 30016's prerequisite and can never buy the stock ladder. Deleting the
--                  stock rows WOULD have been the druid-Mangle regression in miniature: stock
--                  47497.ReqAbility1 = 30022 and 47498.ReqAbility1 = 47497, so removing 30016/30022
--                  would lock NATIVE WotLK warriors out of Devastate ranks 4 and 5 permanently
--                  (_ref capstone_chains.devastate.trainer_sql, baseline_leaks → Devastate:
--                  "disposition: none. **Do not add a DELETE.**").
--
--   * Shield Slam  stock 23923.ReqAbility1 = stock **23922**, chained up to 30356, and then
--                  47487.ReqAbility1 = 30356 / 47488.ReqAbility1 = 47487 for the two WotLK ranks.
--                  Node 20362 grants the CLONE 947541, and the `kBaselineSpellGates` row at
--                  EraTalents.cpp:404 additionally STRIPS the whole stock chain from a TBC warrior
--                  (its TBC arm, EraTalents.cpp:907-915) precisely because that gate's
--                  `grantHigherEra=true` arm would otherwise force-grant stock r1 at level 40 — the
--                  one real baseline leak the warrior sweep found, and one created by our own
--                  substrate rather than by Blizzard data. Both mechanisms are era-scoped; neither
--                  touches a trainer row, so native WotLK warriors keep the full 8-rank ladder.
--
-- The check statement itself (docs/era-talents-framework.md, "Deleting a stock trainer row: the
-- orphaned-higher-rank check") was run for the record at Task 8 over every DELETE in this module's
-- migrations and returned ZERO rows — this file adds no id to its input set.
--
-- ERA GATE = the talent-anchor flavor (framework: "Era-gating CUSTOM spell ranks that REPLACE a
-- stock WotLK version", anchor flavor 1 — no marker spell needed). Rank 1 of each chain exists ONLY
-- as a TBC era-talent clone: 947530 (node 20365 Devastate) and 947541 (node 20362 Shield Slam). A
-- Vanilla-era warrior has no Devastate talent at all and gets stock 23922 for Shield Slam (Vanilla
-- node 18552 is a BOUND-GRANT of the stock spell); a WotLK warrior gets the stock chains. Neither
-- ever holds 947530/947541, so a `ReqAbility1` rooted there makes each ladder reachable ONLY by a
-- TBC-era warrior who spent the talent.
-- ==================================================================================================

-- --------------------------------------------------------------------------------------------------
-- (a) Devastate ranks 2-3, trainable at the TBC levels.
--
-- Levels are TBC's own SpellLevels for the chain (60 / 70), taken from wago 2.5.4 SpellLevels for
-- 30016/30022 and corroborated by the cached Wowhead TBC tooltips (era-data/_ref/tbc/tooltips/
-- {30016,30022}.json print "Requires level 60" / "Requires level 70"). MoneyCost mirrors the stock
-- rows verbatim (3100 / 3250) so an era warrior pays exactly what a native one does at that level.
-- Rank 1 is the talent grant at 50 and is not sold.
-- The per-rank Sunder bonus (15 / 25 / 35, on top of 50% weapon damage) is authored in
-- era-data/tbc/warrior.yaml (helpers 947530-947532).
DELETE FROM `trainer_spell` WHERE `TrainerId`=1 AND `SpellId` IN (947531,947532);
INSERT INTO `trainer_spell`
  (`TrainerId`,`SpellId`,`MoneyCost`,`ReqSkillLine`,`ReqSkillRank`,`ReqAbility1`,`ReqAbility2`,`ReqAbility3`,`ReqLevel`,`VerifiedBuild`) VALUES
  (1, 947531, 3100, 0, 0, 947530, 0, 0, 60, 0),   -- Rank 2 (+25 per Sunder): req talent-granted R1 947530
  (1, 947532, 3250, 0, 0, 947531, 0, 0, 70, 0);   -- Rank 3 (+35 per Sunder): req R2 947531

-- --------------------------------------------------------------------------------------------------
-- (b) Shield Slam ranks 2-6, trainable at the TBC levels.
--
-- Levels are TBC's own SpellLevels for the chain (48 / 54 / 60 / 66 / 70) — identical to the stock
-- rows' ReqLevel — and the cached Wowhead TBC tooltips for 23923/23924/23925/25258/30356 print the
-- same. MoneyCost mirrors the stock rows verbatim (40000 / 56000 / 62000 / 65000 / 71000).
-- Rank 1 is the talent grant at 40 and is not sold. Ranks 7-8 (stock 47487@75 / 47488@80) are above
-- the TBC level cap and have no era counterpart.
-- The per-rank damage (225-235 ... 420-440, vs live's 294-308 ... 549-577) is authored in
-- era-data/tbc/warrior.yaml (helpers 947541-947546).
DELETE FROM `trainer_spell` WHERE `TrainerId`=1 AND `SpellId` IN (947542,947543,947544,947545,947546);
INSERT INTO `trainer_spell`
  (`TrainerId`,`SpellId`,`MoneyCost`,`ReqSkillLine`,`ReqSkillRank`,`ReqAbility1`,`ReqAbility2`,`ReqAbility3`,`ReqLevel`,`VerifiedBuild`) VALUES
  (1, 947542, 40000, 0, 0, 947541, 0, 0, 48, 0),   -- Rank 2 (264-276): req talent-granted R1 947541
  (1, 947543, 56000, 0, 0, 947542, 0, 0, 54, 0),   -- Rank 3 (303-317): req R2 947542
  (1, 947544, 62000, 0, 0, 947543, 0, 0, 60, 0),   -- Rank 4 (342-358): req R3 947543
  (1, 947545, 65000, 0, 0, 947544, 0, 0, 66, 0),   -- Rank 5 (381-399): req R4 947544
  (1, 947546, 71000, 0, 0, 947545, 0, 0, 70, 0);   -- Rank 6 (420-440): req R5 947545

-- --------------------------------------------------------------------------------------------------
-- (c) spell_ranks chains, one per ability, each rooted at the TALENT-granted rank 1.
--
-- Three jobs, all load-bearing:
--   1. the client spellbook collapses same-name ranks to the highest known (which is why every rank's
--      `client:` block carries a `rank:` label);
--   2. a higher-rank cast cleanly supersedes the lower rank instead of coexisting as a second button;
--   3. `EraTalents::Reset`'s removeSpell(rank 1) CASCADES via GetNextSpellInChain, so a respec strips
--      the trained ranks too. The same cascade is what lets the CLASS_WARRIOR reconcile arm (5)'s
--      range strip clear a whole ladder on an era transition.
-- The core's LoadSpellRanks requires every id in a chain to have a spell_dbc record; these custom
-- rows do (generated into 2026_09_03_01_era_talent_tbc_warrior_custom_spells.sql, which db-import
-- applies before this file — the numeric filename order is the apply order).
-- `spell_id` is UNIQUE, so the chains are deleted by first_spell_id before re-inserting.
DELETE FROM `spell_ranks` WHERE `first_spell_id` IN (947530,947541);
INSERT INTO `spell_ranks` (`first_spell_id`,`spell_id`,`rank`) VALUES
  (947530, 947530, 1),
  (947530, 947531, 2),
  (947530, 947532, 3),
  (947541, 947541, 1),
  (947541, 947542, 2),
  (947541, 947543, 3),
  (947541, 947544, 4),
  (947541, 947545, 5),
  (947541, 947546, 6);
