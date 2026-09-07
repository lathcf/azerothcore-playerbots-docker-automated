-- Conflagrate higher Vanilla ranks (R2-R4): trainable from the warlock trainer, era-gated via a
-- require-previous-rank chain. HAND-WRITTEN (not generated) — mirrors the Siphon Life trainer-gate
-- pattern (2026_08_14_14_era_talent_warlock_siphon_life_trainer.sql). Idempotent: DELETE-before-INSERT.
--
-- This server loads trainers from the modern `trainer`/`trainer_spell` tables (ObjectMgr.cpp:
-- "SELECT ... FROM trainer_spell"), NOT the legacy `npc_trainer`. The MAIN warlock trainer is
-- `trainer.Id`=31 (greeting "Hello, warlock!"; the same trainer the Siphon Life ranks use).
--
-- Era gate: rank 1 (custom spell 932780) exists ONLY via the Vanilla Conflagrate talent (node 18250 —
-- it grants 932780). A WotLK warlock never has 932780, so requiring it (ReqAbility1) makes the entire
-- R2->R3->R4 chain reachable ONLY by a Vanilla-era character who took the talent — no extra era filter.
--   R2 932781  ReqAbility1=932780 (talent-granted R1)  L48  mana 200  (240-307 -> 316-397)
--   R3 932782  ReqAbility1=932781 (R2)                 L54  mana 230  (383-480)
--   R4 932783  ReqAbility1=932782 (R3)                 L60  mana 255  (447-558)
-- 1.12 per-rank level/mana/damage from classicdb.ch (stock rank ids 18930/18931/18932, absent from the
-- 3.3.5a DBC). MoneyCost is a reasonable interpolation to the Conflagrate learn levels (L48/54/60), a
-- gentle rising curve in the spirit of the trainer's other high-level spells (not a copy of any single
-- stock chain's values). Per-rank flat damage + mana are authored in era-data/vanilla/warlock.yaml
-- (helpers 932781-783).

DELETE FROM `trainer_spell` WHERE `TrainerId`=31 AND `SpellId` IN (932781,932782,932783);
INSERT INTO `trainer_spell`
  (`TrainerId`,`SpellId`,`MoneyCost`,`ReqSkillLine`,`ReqSkillRank`,`ReqAbility1`,`ReqAbility2`,`ReqAbility3`,`ReqLevel`,`VerifiedBuild`) VALUES
  (31, 932781, 16000, 0, 0, 932780, 0, 0, 48, 0),
  (31, 932782, 22000, 0, 0, 932781, 0, 0, 54, 0),
  (31, 932783, 30000, 0, 0, 932782, 0, 0, 60, 0);

-- --- spell_ranks chain: make R1..R4 a real rank chain so the client spellbook collapses same-name
-- ranks to the highest known, and so Reset's removeSpell(932780) CASCADES to strip the higher ranks on
-- the respec path (the Siphon Life / Bloodthirst precedent). first_spell_id is the talent-granted rank 1
-- (932780) for all rows; the core's LoadSpellRanks requires every id to have a spell_dbc record, which
-- these custom era rows do. NO interaction with the era module: 932780 is talent-granted (era_talent_rank),
-- NOT managed by ReconcileBaselineSpells, so a chain here is safe. `spell_id` is UNIQUE, so DELETE the
-- whole chain by first_spell_id before re-inserting (idempotent).
DELETE FROM `spell_ranks` WHERE `first_spell_id`=932780;
INSERT INTO `spell_ranks` (`first_spell_id`,`spell_id`,`rank`) VALUES
  (932780, 932780, 1),
  (932780, 932781, 2),
  (932780, 932782, 3),
  (932780, 932783, 4);
