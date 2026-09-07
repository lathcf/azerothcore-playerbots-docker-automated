-- Baseline-leak gate (2026-08-18): Shield Slam (23922, warrior), Ice Block (45438, mage) and
-- Divine Spirit (14752, priest) are talent-only in Vanilla but became TRAINER-baseline in WotLK.
-- Their stock rank-1 `trainer_spell` rows carry ReqAbility1=0 (ungated), so a Vanilla-era character
-- could TRAIN them from the class trainer WITHOUT ever spending the era talent that is supposed to
-- grant them (the reported leak). The reconcile strip alone can't fix this — the trainer still
-- OFFERS the spell (trainer_spell is global and can't be era-conditional).
--
-- Fix = the proven Holy Nova pattern (see 2026_08_14_00_priest_holynova_trainer_gate.sql): delete the
-- rank-1 trainer rows so the trainer no longer offers them to ANYONE. The higher ranks already chain
-- ReqAbility1 on the previous rank (23923->23922, 14818->14752, etc. — verified in trainer_spell), so
-- removing rank 1 gates each whole chain behind the talent. WotLK-stage characters get rank 1 back via
-- the module reconcile (ReconcileBaselineSpells, these gates flipped to grantHigherEra=true) at the
-- spell's level, then train the higher ranks off it — exactly how Holy Nova is handled. A Vanilla
-- character gets rank 1 ONLY from the era talent grant (node 18552 / 18046 / 18113). Idempotent.
--
-- This server loads trainers from the modern `trainer`/`trainer_spell` tables (ObjectMgr.cpp), NOT
-- legacy `npc_trainer`; deletes are by SpellId (across all trainer sets) so no other trainer can offer
-- rank 1 either. `.reload trainer` (or a restart) applies it live.

DELETE FROM `trainer_spell` WHERE `SpellId`=23922;   -- Shield Slam rank 1 (warrior trainer Id 1, L40)
DELETE FROM `trainer_spell` WHERE `SpellId`=45438;   -- Ice Block (mage trainer Id 16, L30, single rank)
DELETE FROM `trainer_spell` WHERE `SpellId`=14752;   -- Divine Spirit rank 1 (priest trainer Id 11, L30)

-- Keep the legacy `npc_trainer` table consistent (dead weight on this server, but mirror the delete
-- as the Holy Nova gate does). Harmless if the core never reads it.
DELETE FROM `npc_trainer` WHERE `SpellID`=23922;
DELETE FROM `npc_trainer` WHERE `SpellID`=45438;
DELETE FROM `npc_trainer` WHERE `SpellID`=14752;
