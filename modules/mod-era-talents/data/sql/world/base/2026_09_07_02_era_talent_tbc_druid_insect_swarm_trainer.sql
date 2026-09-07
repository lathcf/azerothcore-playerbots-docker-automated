-- Final review deferred-gaps round (2026-09-07, G-51 extended to TBC). TBC Insect Swarm ranks 2-6
-- (clones 946282-946286), trainer-taught behind a require-previous-rank chain rooted at the
-- talent-granted R1 946281 (node 20107). HAND-WRITTEN — mirrors 2026_08_31_02 (TBC Nature's Grasp)
-- and 2026_09_07_01 (Vanilla Insect Swarm). Idempotent.
--
-- Era gate: only a TBC druid who spent node 20107 holds 946281. NO stock rows deleted (the stock
-- chain self-gates on 5570). MoneyCost/ReqLevel copied from the stock 24974-24977/27013 rows.
DELETE FROM `trainer_spell` WHERE `TrainerId`=33 AND `SpellId` IN (946282,946283,946284,946285,946286);
INSERT INTO `trainer_spell`
  (`TrainerId`,`SpellId`,`MoneyCost`,`ReqSkillLine`,`ReqSkillRank`,`ReqAbility1`,`ReqAbility2`,`ReqAbility3`,`ReqLevel`,`VerifiedBuild`) VALUES
  (33, 946282,   6000, 0, 0, 946281, 0, 0, 30, 0),   -- Rank 2: req talent-granted R1
  (33, 946283,  14000, 0, 0, 946282, 0, 0, 40, 0),   -- Rank 3
  (33, 946284,  12000, 0, 0, 946283, 0, 0, 50, 0),   -- Rank 4
  (33, 946285,  34000, 0, 0, 946284, 0, 0, 60, 0),   -- Rank 5
  (33, 946286, 200000, 0, 0, 946285, 0, 0, 70, 0);   -- Rank 6

DELETE FROM `spell_ranks` WHERE `first_spell_id`=946281;
INSERT INTO `spell_ranks` (`first_spell_id`,`spell_id`,`rank`) VALUES
  (946281, 946281, 1),
  (946281, 946282, 2),
  (946281, 946283, 3),
  (946281, 946284, 4),
  (946281, 946285, 5),
  (946281, 946286, 6);
