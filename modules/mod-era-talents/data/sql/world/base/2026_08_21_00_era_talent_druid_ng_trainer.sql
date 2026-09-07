-- Druid fix round (2026-08-21). Nature's Grasp full 6-rank Vanilla reconstruction + baseline-leak /
-- version-swap trainer-row deletions (Faerie Fire (Feral) + Enrage). HAND-WRITTEN (not generated) —
-- mirrors the warrior Bloodthirst / rogue Hemorrhage / paladin-seal trainer-gate pattern
-- (2026_08_18_20 / 2026_08_17_12 / 2026_08_20_40). Idempotent: DELETE-before-INSERT.
--
-- This server loads trainers from the modern `trainer`/`trainer_spell` tables (ObjectMgr.cpp reads
-- `trainer_spell`), NOT the legacy `npc_trainer` table. The DRUID trainer is `trainer.Id`=33.
-- `.reload trainer` (or a restart) applies it live.

-- =====================================================================================================
-- (1) Nature's Grasp ranks 2-6 (custom clones 932507-932511), trainer-taught, era-gated by a
--     require-previous-rank chain rooted at the talent-granted R1 (932505, node 18701).
-- Era gate: rank 1 exists ONLY as the Vanilla era-talent clone 932505. A WotLK druid never has 932505,
-- so requiring it (ReqAbility1) makes the R2->R3->...->R6 chain reachable ONLY by a Vanilla-era druid who
-- took the talent — no extra era filtering needed. Levels/costs are the authentic stock NG trainer
-- values (16810/16811/16812/16813/17329 = L18/28/38/48/58, MoneyCost 1900/5000/12000/22000/32000).
DELETE FROM `trainer_spell` WHERE `TrainerId`=33 AND `SpellId` IN (932507,932508,932509,932510,932511);
INSERT INTO `trainer_spell`
  (`TrainerId`,`SpellId`,`MoneyCost`,`ReqSkillLine`,`ReqSkillRank`,`ReqAbility1`,`ReqAbility2`,`ReqAbility3`,`ReqLevel`,`VerifiedBuild`) VALUES
  (33, 932507,  1900, 0, 0, 932505, 0, 0, 18, 0),   -- Rank 2 (-> Entangling Roots 19974): req talent-granted R1 932505
  (33, 932508,  5000, 0, 0, 932507, 0, 0, 28, 0),   -- Rank 3 (-> 19973): req R2 932507
  (33, 932509, 12000, 0, 0, 932508, 0, 0, 38, 0),   -- Rank 4 (-> 19972): req R3 932508
  (33, 932510, 22000, 0, 0, 932509, 0, 0, 48, 0),   -- Rank 5 (-> 19971): req R4 932509
  (33, 932511, 32000, 0, 0, 932510, 0, 0, 58, 0);   -- Rank 6 (-> 19970): req R5 932510

-- spell_ranks chain: make R1..R6 a real rank chain so the client spellbook collapses same-name ranks to
-- the highest known, higher-rank casts overwrite the lower buff cleanly, and Reset's removeSpell(932505)
-- CASCADES via GetNextSpellInChain to strip the trained ranks on respec. first_spell_id is the
-- talent-granted rank 1 (932505). 932505 is a plain node grant (era_talent_rank), NOT managed by
-- ReconcileBaselineSpells, so a chain here is safe (same as Bloodthirst 932828 / Hemorrhage 932985).
DELETE FROM `spell_ranks` WHERE `first_spell_id`=932505;
INSERT INTO `spell_ranks` (`first_spell_id`,`spell_id`,`rank`) VALUES
  (932505, 932505, 1),
  (932505, 932507, 2),
  (932505, 932508, 3),
  (932505, 932509, 4),
  (932505, 932510, 5),
  (932505, 932511, 6);

-- =====================================================================================================
-- (2) Close the WotLK baseline leaks by DELETING stock trainer rows globally (across all trainer sets,
--     by SpellId). Deletes are GLOBAL; the module reconcile (EraTalents.cpp CLASS_DRUID + kBaselineSpellGates)
--     restores each to WotLK-stage druids by level (grantHigherEra / version-swap), and grants/strips
--     the era version in Vanilla. This is the proven Holy Nova / Shield Slam / Consecration pattern.

-- Nature's Grasp stock chain (16689 L10 was ungated via ReqAbility1=339 Entangling Roots = the leak;
-- higher ranks chained off it). All ranks removed; a Vanilla druid gets R1 only from the talent (932505)
-- and trains R2-R6 (932507-511) above; a WotLK druid auto-learns stock NG by level (CLASS_DRUID block).
DELETE FROM `trainer_spell` WHERE `SpellId` IN (16689,16810,16811,16812,16813,17329,27009,53312);

-- Faerie Fire (Feral) 16857 (druid trainer, L18, ReqAbility1=0 = ungated leak). Talent-only in Vanilla
-- (node 18729 grants it). WotLK druids auto-learn it at L18 via the kBaselineSpellGates row
-- { CLASS_DRUID, 18729, {16857}, 18, true }.
DELETE FROM `trainer_spell` WHERE `SpellId`=16857;

-- Enrage 5229 (druid trainer Id 33, L12, ReqAbility1=0 = ungated leak). Version-swapped: a Vanilla druid
-- gets the custom clone 932513 by level, a WotLK druid gets stock 5229 by level (CLASS_DRUID block).
DELETE FROM `trainer_spell` WHERE `SpellId`=5229;

-- Keep the legacy `npc_trainer` table consistent (dead weight on this server, but mirror the deletes as
-- the Holy Nova / baseline-gate migrations do). Harmless if the core never reads it.
DELETE FROM `npc_trainer` WHERE `SpellID` IN (16689,16810,16811,16812,16813,17329,27009,53312,16857,5229);
