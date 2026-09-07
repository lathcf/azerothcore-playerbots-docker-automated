-- TBC Paladin Blessing of Sanctuary RANK 5 (946136) — a TBC-Classic addition (L70) that the
-- Vanilla BoS chain (r1-r4, clones 932617/661/662/664) stops short of. Authored fresh at wago-
-- 2.5.4.44833 values (user-confirmed 2026-08-30): reduce damage taken by up to 80, block-damage
-- 46 Holy (trigger 946137), 180 mana. TBC-ONLY: the trainer row chains on r4 932664 (an era clone
-- only a paladin who took the BoS node ever holds) at ReqLevel 70, out of a Vanilla character's
-- reach, and the reconcile
-- TBC BoS chain (node 20033; the band sweep in EraBandClassifier strips it when the node is unspent or the era is not TBC). The r1-r4
-- clones + their trainer/spell_ranks rows live in the Vanilla 2026_08_19_30 file (reused for TBC).
-- HAND-WRITTEN. Idempotent: DELETE-before-INSERT.

-- spell_ranks: r5 supersedes r4 (932664) and cascades removeSpell on respec (root 932617).
-- RE-APPLY HAZARD: AC re-applies an updated .sql when its CONTENT HASH changes, in filename order
-- within that run -- NOT in original-ship order. 2026_08_19_30 also owns this chain and re-emits it
-- with a DELETE by first_spell_id, so this file must emit the IDENTICAL full r1-r5 chain (not just
-- the r5 tuple) or a re-apply of that file alone drops r5 and orphans 946136 -- exactly what happened
-- on 2026-09-04, leaving TBC paladin bots holding an unchained 946136 the doctor reported as ORPHAN.
DELETE FROM `spell_ranks` WHERE `first_spell_id`=932617;
INSERT INTO `spell_ranks` (`first_spell_id`, `spell_id`, `rank`) VALUES
  (932617, 932617, 1),
  (932617, 932661, 2),
  (932617, 932662, 3),
  (932617, 932664, 4),
  (932617, 946136, 5);

-- trainer_spell: BoS r5 on every paladin trainer (TrainerId 3/4/5), ReqLevel 70, 80s money cost
-- (matching the r4 tier). ReqAbility1 is the PREVIOUS RANK 932664 (r4), like every other rank>=2 row
-- in this chain -- NOT the Era:TBC-Paladin marker 946078. Gating a rank>=2 row on a marker breaks the
-- chain the whole design rests on: the bot factory's trainer walk learns any row whose requirements it
-- meets, so a TBC paladin who never took node 20033 (and therefore has no r1 932617) still satisfied
-- 946078 and was handed r5 as a band orphan (found on Sathelon/Larenvedon, Phase 9.5 band sweep).
-- Era exclusivity does not need the marker here: r4 only exists as an era clone trained off the
-- talent-granted r1, and ReqLevel 70 is out of a Vanilla-era character's reach.
DELETE FROM `trainer_spell` WHERE `SpellId`=946136;
INSERT INTO `trainer_spell` (`TrainerId`, `SpellId`, `MoneyCost`, `ReqSkillLine`, `ReqSkillRank`, `ReqAbility1`, `ReqAbility2`, `ReqAbility3`, `ReqLevel`, `VerifiedBuild`) VALUES
  (3, 946136, 80000, 0, 0, 932664, 0, 0, 70, 0),
  (4, 946136, 80000, 0, 0, 932664, 0, 0, 70, 0),
  (5, 946136, 80000, 0, 0, 932664, 0, 0, 70, 0);
