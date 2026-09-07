-- Bloodthirst higher Vanilla ranks (R2 932829, R3 932830, R4 932831): trainable from the warrior
-- trainer, era-gated via a require-previous-rank chain. HAND-WRITTEN (not generated) — mirrors the
-- rogue Hemorrhage / hunter Aimed Shot / warlock Siphon Life trainer-gate pattern
-- (2026_08_17_12 / 2026_08_16_22 / 2026_08_14_14). Idempotent: DELETE-before-INSERT.
--
-- This server loads trainers from the modern `trainer`/`trainer_spell` tables (ObjectMgr.cpp:
-- "SELECT ... FROM trainer_spell"), NOT the legacy `npc_trainer` table. The MAIN warrior trainer is
-- `trainer.Id`=1 (Requirement=1 = CLASS_WARRIOR; greeting "Hello, warrior!  Ready for some training?";
-- 133 spells); `trainer.Id`=2 is a 6-spell starter and is NOT used.
--
-- Era gate: rank 1 exists ONLY as a Vanilla era-talent CLONE (Bloodthirst 932828 via node 18535). A
-- WotLK warrior never has 932828, so requiring it (ReqAbility1) makes the R2->R3->R4 chain reachable
-- ONLY by a Vanilla-era character who took the talent — no extra era filtering needed. Stock Bloodthirst
-- (23881 and its higher ranks 23892/23893/23894) is NOT on this trainer at all (it is talent-granted in
-- stock), so there is nothing for a Vanilla warrior to accidentally train and no strip is needed there;
-- the node no longer grants 23881 (it grants clone 932828), and EraTalents.cpp strips a stale 23881 from
-- a warrior who learned Bloodthirst before this rework.
--
-- Levels are the authentic Vanilla Bloodthirst trainer levels (R2 L48, R3 L54, R4 L60; classic
-- references / wowclassicdb 23892/23893/23894). MoneyCost is a gentle rising curve in the spirit of the
-- trainer's other level-40+ spells. Per-rank damage (45% AP, all ranks) and heal (10/13/17/20) are
-- authored in era-data/vanilla/warrior.yaml (helpers 932828-839).

DELETE FROM `trainer_spell` WHERE `TrainerId`=1 AND `SpellId` IN (932829,932830,932831);
INSERT INTO `trainer_spell`
  (`TrainerId`,`SpellId`,`MoneyCost`,`ReqSkillLine`,`ReqSkillRank`,`ReqAbility1`,`ReqAbility2`,`ReqAbility3`,`ReqLevel`,`VerifiedBuild`) VALUES
  (1, 932829, 4000, 0, 0, 932828, 0, 0, 48, 0),   -- Rank 2 (heal 13): req talent-granted R1 932828
  (1, 932830, 6000, 0, 0, 932829, 0, 0, 54, 0),   -- Rank 3 (heal 17): req R2 932829
  (1, 932831, 8000, 0, 0, 932830, 0, 0, 60, 0);   -- Rank 4 (heal 20): req R3 932830

-- --- spell_ranks chain: make R1..R4 a real rank chain so the client spellbook collapses same-name
-- ranks to the highest known, higher-rank casts overwrite the lower heal buff cleanly, and Reset's
-- removeSpell(932828) CASCADES via GetNextSpellInChain to strip the trained ranks on respec.
-- first_spell_id is the talent-granted rank 1 (932828). The core's LoadSpellRanks requires every id to
-- have a spell_dbc record, which these custom era rows do. NO interaction with the era module: 932828 is
-- talent-granted (era_talent_rank), NOT managed by ReconcileBaselineSpells, so a chain here is safe
-- (same as Hemorrhage 932985 / Siphon Life 932902 / Aimed Shot 932952). `spell_id` is UNIQUE, so DELETE
-- the chain by first_spell_id before re-inserting.
DELETE FROM `spell_ranks` WHERE `first_spell_id`=932828;
INSERT INTO `spell_ranks` (`first_spell_id`,`spell_id`,`rank`) VALUES
  (932828, 932828, 1),
  (932828, 932829, 2),
  (932828, 932830, 3),
  (932828, 932831, 4);
