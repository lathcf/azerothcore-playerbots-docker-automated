-- Siphon Life higher Vanilla ranks (R2-R4): trainable from the warlock trainer, era-gated via a
-- require-previous-rank chain. HAND-WRITTEN (not generated) — mirrors the Holy Nova trainer-gate
-- pattern. Idempotent: DELETE-before-INSERT.
--
-- This server loads trainers from the modern `trainer`/`trainer_spell` tables (ObjectMgr.cpp:
-- "SELECT ... FROM trainer_spell"), NOT the legacy `npc_trainer` table. The MAIN warlock trainer is
-- `trainer.Id`=31 (greeting "Hello, warlock!"; 232 spells incl. the full Corruption rank chain);
-- `trainer.Id`=32 is the starter trainer (5 spells) and is NOT used here.
--
-- Era gate: rank 1 (custom spell 932902) exists ONLY via the Vanilla Siphon Life talent (node 18213).
-- A WotLK warlock never has 932902, so requiring it (ReqAbility1) makes the entire R2->R3->R4 chain
-- reachable ONLY by a Vanilla-era character who took the talent — no extra era filtering needed.
--   R2 932919  ReqAbility1=932902 (talent-granted R1)  L38  mana 205
--   R3 932920  ReqAbility1=932919 (R2)                 L46  mana 285
--   R4 932921  ReqAbility1=932920 (R3)                 L54  mana 365
-- MoneyCost is a reasonable interpolation to the Siphon Life learn levels (L38=10000, L46=14000,
-- L54=20000) — a gentle rising curve in the spirit of the trainer's other mid-level spells, not a
-- copy of any single stock chain's values.
-- Per-tick health values (22/33/45) are authored in era-data/vanilla/warlock.yaml (helpers 932919-21).

DELETE FROM `trainer_spell` WHERE `TrainerId`=31 AND `SpellId` IN (932919,932920,932921);
INSERT INTO `trainer_spell`
  (`TrainerId`,`SpellId`,`MoneyCost`,`ReqSkillLine`,`ReqSkillRank`,`ReqAbility1`,`ReqAbility2`,`ReqAbility3`,`ReqLevel`,`VerifiedBuild`) VALUES
  (31, 932919, 10000, 0, 0, 932902, 0, 0, 38, 0),
  (31, 932920, 14000, 0, 0, 932919, 0, 0, 46, 0),
  (31, 932921, 20000, 0, 0, 932920, 0, 0, 54, 0);

-- --- spell_ranks chain (Fix 1): make R1..R4 a real rank chain so a higher rank OVERWRITES the
-- lower's SPELL_AURA_PERIODIC_LEECH on a target (instead of four unrelated aura-53 DoTs stacking),
-- and so the client spellbook collapses same-name ranks to the highest known. first_spell_id is the
-- talent-granted rank 1 (932902) for all rows; the core's LoadSpellRanks requires every id to have a
-- spell_dbc record, which these custom era rows do. NO interaction with the era module: 932902 is
-- talent-granted (era_talent_rank), NOT managed by ReconcileBaselineSpells, so a chain here is safe.
-- `spell_id` is UNIQUE, so DELETE the whole chain by first_spell_id before re-inserting (idempotent).
DELETE FROM `spell_ranks` WHERE `first_spell_id`=932902;
INSERT INTO `spell_ranks` (`first_spell_id`,`spell_id`,`rank`) VALUES
  (932902, 932902, 1),
  (932902, 932919, 2),
  (932902, 932920, 3),
  (932902, 932921, 4);
