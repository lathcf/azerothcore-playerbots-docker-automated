-- Dark Pact higher Vanilla ranks (R2-R3): trainable from the warlock trainer, era-gated via a
-- require-previous-rank chain. HAND-WRITTEN (not generated) — mirrors the Conflagrate trainer-gate
-- pattern (2026_08_14_15_era_talent_warlock_conflagrate_trainer.sql). Idempotent: DELETE-before-INSERT.
--
-- Context (final-review F-1, 2026-08-24): stock Dark Pact 18220 drains the WotLK 304 mana; the Vanilla
-- chain is 150/200/250 (1.12.1 client Spell.dbc, rank ids 18220/18937/18938). Node 18217 now grants the
-- Vanilla clone R1 (932784, 150); R2/R3 are the clones below.
--
-- This server loads trainers from the modern `trainer`/`trainer_spell` tables (ObjectMgr.cpp:
-- "SELECT ... FROM trainer_spell"), NOT the legacy `npc_trainer`. The MAIN warlock trainer is
-- `trainer.Id`=31 (greeting "Hello, warlock!"; the same trainer the Conflagrate/Siphon Life ranks use).
--
-- Era gate: rank 1 (custom spell 932784) exists ONLY via the Vanilla Dark Pact talent (node 18217 grants
-- it). A WotLK warlock never has 932784, so requiring it (ReqAbility1) makes the R2->R3 chain reachable
-- ONLY by a Vanilla-era character who took the talent — no extra era filter needed.
--   R2 932785  ReqAbility1=932784 (talent-granted R1)  L50  drains 200
--   R3 932786  ReqAbility1=932785 (R2)                 L60  drains 250
-- Vanilla per-rank drain from the 1.12.1 client Spell.dbc; ReqLevels follow the 3.3.5a Dark Pact chain
-- (18937 L50, 18938 L60). MoneyCost is a gentle rising curve to the learn levels (not a copy of any single
-- stock chain). Per-rank drain values are authored in era-data/vanilla/warlock.yaml (helpers 932785/786).

DELETE FROM `trainer_spell` WHERE `TrainerId`=31 AND `SpellId` IN (932785,932786);
INSERT INTO `trainer_spell`
  (`TrainerId`,`SpellId`,`MoneyCost`,`ReqSkillLine`,`ReqSkillRank`,`ReqAbility1`,`ReqAbility2`,`ReqAbility3`,`ReqLevel`,`VerifiedBuild`) VALUES
  (31, 932785, 18000, 0, 0, 932784, 0, 0, 50, 0),
  (31, 932786, 26000, 0, 0, 932785, 0, 0, 60, 0);

-- --- spell_ranks chain: make R1..R3 a real rank chain so the client spellbook collapses same-name ranks
-- to the highest known, and so Reset's removeSpell(932784) CASCADES to strip the higher ranks on the
-- respec path (the Conflagrate / Siphon Life precedent). first_spell_id is the talent-granted rank 1
-- (932784) for all rows; the core's LoadSpellRanks requires every id to have a spell_dbc record, which
-- these custom era rows do. NO interaction with the era module: 932784 is talent-granted (era_talent_rank),
-- NOT managed by ReconcileBaselineSpells, so a chain here is safe. `spell_id` is UNIQUE, so DELETE the
-- whole chain by first_spell_id before re-inserting (idempotent).
DELETE FROM `spell_ranks` WHERE `first_spell_id`=932784;
INSERT INTO `spell_ranks` (`first_spell_id`,`spell_id`,`rank`) VALUES
  (932784, 932784, 1),
  (932784, 932785, 2),
  (932784, 932786, 3);
