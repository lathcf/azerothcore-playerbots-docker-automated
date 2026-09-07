-- Final review deferred-gaps round (2026-09-07, G-51). Vanilla Insect Swarm ranks 2-5 (clones
-- 932515-932518), trainer-taught behind a require-previous-rank chain rooted at the talent-granted
-- R1 932514 (node 18738). HAND-WRITTEN — mirrors 2026_08_21_00 (Nature's Grasp). Idempotent.
--
-- Era gate: only a Vanilla druid who spent node 18738 holds 932514, so ReqAbility1 makes the chain
-- reachable only for them. NO stock rows are deleted: the stock chain (24974..48468) already
-- self-gates on 5570, which a managed-era druid no longer receives — deleting it would orphan WotLK
-- druids' higher ranks. MoneyCost/ReqLevel copied from the stock 24974-24977 rows (TrainerId 33).
DELETE FROM `trainer_spell` WHERE `TrainerId`=33 AND `SpellId` IN (932515,932516,932517,932518);
INSERT INTO `trainer_spell`
  (`TrainerId`,`SpellId`,`MoneyCost`,`ReqSkillLine`,`ReqSkillRank`,`ReqAbility1`,`ReqAbility2`,`ReqAbility3`,`ReqLevel`,`VerifiedBuild`) VALUES
  (33, 932515,  6000, 0, 0, 932514, 0, 0, 30, 0),   -- Rank 2: req talent-granted R1
  (33, 932516, 14000, 0, 0, 932515, 0, 0, 40, 0),   -- Rank 3
  (33, 932517, 12000, 0, 0, 932516, 0, 0, 50, 0),   -- Rank 4
  (33, 932518, 34000, 0, 0, 932517, 0, 0, 60, 0);   -- Rank 5

-- spell_ranks chain rooted at the node-granted R1 (spellbook collapse, overwrite, Reset cascade;
-- the band sweep's rule 2 owns r2-r5 as ranks above a node-granted r1).
DELETE FROM `spell_ranks` WHERE `first_spell_id`=932514;
INSERT INTO `spell_ranks` (`first_spell_id`,`spell_id`,`rank`) VALUES
  (932514, 932514, 1),
  (932514, 932515, 2),
  (932514, 932516, 3),
  (932514, 932517, 4),
  (932514, 932518, 5);
