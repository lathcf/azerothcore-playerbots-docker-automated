-- TBC Druid, Phase 2 Task 6 (Balance tab). Nature's Grasp ranks 2-6 for a TBC-era druid.
-- HAND-WRITTEN (not generated) — mirrors the Vanilla druid chain (2026_08_21_00_era_talent_druid_ng_trainer.sql)
-- exactly, which itself mirrors the warrior Bloodthirst / rogue Hemorrhage / paladin-seal trainer-gate
-- pattern. Idempotent: DELETE-before-INSERT.
--
-- This server loads trainers from the modern `trainer`/`trainer_spell` tables (ObjectMgr.cpp reads
-- `trainer_spell`), NOT the legacy `npc_trainer` table. The DRUID trainer is `trainer.Id`=33.
-- `.reload trainer` (or a restart) applies it live.
--
-- NOTE the STOCK Nature's Grasp chain (16689/16810/16811/16812/16813/17329/27009/53312) was ALREADY
-- deleted globally by the Vanilla phase (2026_08_21_00). This migration does NOT re-delete it.

-- =====================================================================================================
-- Nature's Grasp ranks 2-6 (TBC clones 946271-946275), trainer-taught, era-gated by a
-- require-previous-rank chain rooted at the talent-granted R1 (946270, TBC node 20101).
--
-- Era gate: rank 1 exists ONLY as the TBC era-talent clone 946270. Neither a Vanilla-era druid (whose
-- Nature's Grasp is the separate 932505/932507-511 chain) nor a WotLK druid (stock 16689+, auto-granted
-- by level) ever holds 946270, so requiring it via ReqAbility1 makes the R2->R3->...->R6 chain reachable
-- ONLY by a TBC-era druid who spent the talent — no extra era filtering needed. This is the same
-- talent-anchor flavor documented in docs/era-talents-framework.md ("Era-gating CUSTOM spell ranks that
-- REPLACE a stock WotLK version", anchor flavor 1).
--
-- Levels are the authentic TBC (wago 2.5.4.44833 SpellLevels) values for the ranks these clone —
-- 16810/16811/16812/16813/17329 = L18/28/38/48/58 — which match the stock 3.3.5a trainer rows the
-- Vanilla chain copied its MoneyCost from (1900/5000/12000/22000/32000).
--
-- The TBC clones are FREE to cast (TBC Nature's Grasp has no SpellPower row at any rank); that is a
-- property of the spell rows, not of these trainer rows, which only sell the ranks.
DELETE FROM `trainer_spell` WHERE `TrainerId`=33 AND `SpellId` IN (946271,946272,946273,946274,946275);
INSERT INTO `trainer_spell`
  (`TrainerId`,`SpellId`,`MoneyCost`,`ReqSkillLine`,`ReqSkillRank`,`ReqAbility1`,`ReqAbility2`,`ReqAbility3`,`ReqLevel`,`VerifiedBuild`) VALUES
  (33, 946271,  1900, 0, 0, 946270, 0, 0, 18, 0),   -- Rank 2 (-> Entangling Roots 19974): req talent-granted R1 946270
  (33, 946272,  5000, 0, 0, 946271, 0, 0, 28, 0),   -- Rank 3 (-> 19973): req R2 946271
  (33, 946273, 12000, 0, 0, 946272, 0, 0, 38, 0),   -- Rank 4 (-> 19972): req R3 946272
  (33, 946274, 22000, 0, 0, 946273, 0, 0, 48, 0),   -- Rank 5 (-> 19971): req R4 946273
  (33, 946275, 32000, 0, 0, 946274, 0, 0, 58, 0);   -- Rank 6 (-> 19970): req R5 946274

-- spell_ranks chain: make R1..R6 a real rank chain so the client spellbook collapses same-name ranks to
-- the highest known, higher-rank casts overwrite the lower buff cleanly, and Reset's removeSpell(946270)
-- CASCADES via GetNextSpellInChain to strip the trained ranks on respec. first_spell_id is the
-- talent-granted rank 1 (946270). 946270 is a plain node grant (era_talent_rank), NOT managed by
-- ReconcileBaselineSpells, so a chain here is safe (same as the Vanilla 932505 chain).
DELETE FROM `spell_ranks` WHERE `first_spell_id`=946270;
INSERT INTO `spell_ranks` (`first_spell_id`,`spell_id`,`rank`) VALUES
  (946270, 946270, 1),
  (946270, 946271, 2),
  (946270, 946272, 3),
  (946270, 946273, 4),
  (946270, 946274, 5),
  (946270, 946275, 6);
