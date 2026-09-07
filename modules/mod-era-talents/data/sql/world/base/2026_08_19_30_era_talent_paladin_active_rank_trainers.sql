-- Paladin active-ability higher-rank trainer chains + spell_ranks (2026-08-19, Fixes 3/4/6 + 5).
-- Holy Shock (node 18614), Holy Shield (18629), and Blessing of Sanctuary (18626) are reconstructed as
-- Vanilla CLONE chains: the era talent grants clone RANK 1, and the higher ranks are trainer-taught
-- behind a ReqAbility1 chain off that talent-granted rank 1 — so ONLY a Vanilla-era paladin who took
-- the talent can train them (a WotLK paladin never has the clone rank 1). HAND-WRITTEN, mirrors the
-- warrior Bloodthirst (2026_08_18_20) / warlock Conflagrate (2026_08_14_15) trainer-gate pattern.
-- Idempotent: DELETE-before-INSERT.
--
-- This server loads trainers from the modern `trainer`/`trainer_spell` tables (ObjectMgr.cpp), NOT the
-- legacy `npc_trainer`. The GENERAL paladin trainers are `trainer.Id` 3, 4, 5 (Requirement=2 = paladin,
-- ~171 spells each; Id 6 is a 5-spell specialized trainer). Rows are added to all three so any paladin
-- can train regardless of faction/race trainer. Levels/mana/damage are the canonical 1.12.1 client
-- Spell.dbc (authored in era-data/vanilla/paladin.yaml helpers). MoneyCost follows the AUTHENTIC
-- paladin trainer level->cost curve (stock Blessing of Might/Wisdom + seal/LoH rows; ~1.4x the
-- shaman-totem curve), REPLACING an earlier steeper curve that over-priced the top ranks (L60 was
-- 80000 vs the ~46000 authentic).
--
-- Era gate: rank 1 exists ONLY as a Vanilla era-talent clone (Holy Shock 932646 / Holy Shield 932615 /
-- Blessing of Sanctuary 932617). A WotLK paladin never has those, so requiring rank 1 (ReqAbility1)
-- makes the whole chain reachable ONLY by a Vanilla-era character who took the talent — no extra era
-- filtering needed. EraBandClassifier::Sweep owns the strip of the trained ranks for a Vanilla
-- paladin who respecs away (rule 2: a `spell_ranks` rank above a node-granted r1); the per-class
-- EraTalents.cpp `kPalTrainedChains` arm this header used to name is RETIRED. A stale stock Holy
-- Shock 20473 is still stripped by its own era-gated arm in EraTalents.cpp (20473 is a STOCK id, so
-- no band rule can reach it).

DELETE FROM `trainer_spell` WHERE `TrainerId` IN (3,4,5) AND `SpellId` IN
  (932647,932648, 932655,932657,932659, 932661,932662,932664,932666);

-- Retired BoS rank-5 clone 932666 (the 2026-08-19 rework made BoS a 4-rank chain). Its trainer rows
-- are cleared above; its spell_dbc/spell_proc rows were emitted by the previous custom-spell SQL and the
-- generator no longer DELETEs an id it doesn't re-insert, so clean them here (idempotent). Nothing else
-- references 932666 (no era_talent_rank, no spell_ranks, no grant), so this leaves no dangling reference.
DELETE FROM `spell_dbc`  WHERE `ID`=932666;
DELETE FROM `spell_proc` WHERE `SpellId`=932666;
INSERT INTO `trainer_spell`
  (`TrainerId`,`SpellId`,`MoneyCost`,`ReqSkillLine`,`ReqSkillRank`,`ReqAbility1`,`ReqAbility2`,`ReqAbility3`,`ReqLevel`,`VerifiedBuild`) VALUES
  -- Holy Shock R2/R3 (req talent-granted R1 932646). L48/L56.
  (3, 932647, 26000, 0, 0, 932646, 0, 0, 48, 0),
  (3, 932648, 42000, 0, 0, 932647, 0, 0, 56, 0),
  (4, 932647, 26000, 0, 0, 932646, 0, 0, 48, 0),
  (4, 932648, 42000, 0, 0, 932647, 0, 0, 56, 0),
  (5, 932647, 26000, 0, 0, 932646, 0, 0, 48, 0),
  (5, 932648, 42000, 0, 0, 932647, 0, 0, 56, 0),
  -- Holy Shield R2/R3/R4 (req talent-granted R1 932615). L40/L50/L60.
  (3, 932655, 18000, 0, 0, 932615, 0, 0, 40, 0),
  (3, 932657, 28000, 0, 0, 932655, 0, 0, 50, 0),
  (3, 932659, 46000, 0, 0, 932657, 0, 0, 60, 0),
  (4, 932655, 18000, 0, 0, 932615, 0, 0, 40, 0),
  (4, 932657, 28000, 0, 0, 932655, 0, 0, 50, 0),
  (4, 932659, 46000, 0, 0, 932657, 0, 0, 60, 0),
  (5, 932655, 18000, 0, 0, 932615, 0, 0, 40, 0),
  (5, 932657, 28000, 0, 0, 932655, 0, 0, 50, 0),
  (5, 932659, 46000, 0, 0, 932657, 0, 0, 60, 0),
  -- Blessing of Sanctuary R2/R3/R4 (req talent-granted R1 932617). L40/L50/L60. R5 946136 is a
  -- TBC-Classic-only addition and ships its own trainer rows in 2026_08_30_10 (chained on R4 932664).
  (3, 932661, 18000, 0, 0, 932617, 0, 0, 40, 0),
  (3, 932662, 28000, 0, 0, 932661, 0, 0, 50, 0),
  (3, 932664, 46000, 0, 0, 932662, 0, 0, 60, 0),
  (4, 932661, 18000, 0, 0, 932617, 0, 0, 40, 0),
  (4, 932662, 28000, 0, 0, 932661, 0, 0, 50, 0),
  (4, 932664, 46000, 0, 0, 932662, 0, 0, 60, 0),
  (5, 932661, 18000, 0, 0, 932617, 0, 0, 40, 0),
  (5, 932662, 28000, 0, 0, 932661, 0, 0, 50, 0),
  (5, 932664, 46000, 0, 0, 932662, 0, 0, 60, 0);

-- spell_ranks chains: collapse same-name ranks in the client spellbook, let a higher rank overwrite
-- the lower aura cleanly, and make Reset's removeSpell(rank1) CASCADE via GetNextSpellInChain to strip
-- the trained ranks on respec. first_spell_id is the talent-granted (or level-granted) rank 1. Every id
-- has a spell_dbc record (the era clones), which LoadSpellRanks requires. `spell_id` is UNIQUE, so
-- DELETE by first_spell_id before re-inserting.
DELETE FROM `spell_ranks` WHERE `first_spell_id` IN (932646, 932615, 932617, 932668);
INSERT INTO `spell_ranks` (`first_spell_id`,`spell_id`,`rank`) VALUES
  -- Holy Shock
  (932646, 932646, 1),
  (932646, 932647, 2),
  (932646, 932648, 3),
  -- Holy Shield
  (932615, 932615, 1),
  (932615, 932655, 2),
  (932615, 932657, 3),
  (932615, 932659, 4),
  -- Blessing of Sanctuary (5 ranks: r1-r4 Vanilla clones, r5 946136 is the TBC-Classic addition).
  -- RE-APPLY HAZARD: AC re-applies an updated .sql when its CONTENT HASH changes, in filename order
  -- within that run -- NOT in original-ship order. This DELETE+INSERT and the one in
  -- 2026_08_30_10_era_tbc_paladin_bos_r5.sql must therefore emit the IDENTICAL r1-r5 chain, so the
  -- table converges no matter which of the two re-applies last (a 2026-09-04 re-apply of this file
  -- alone silently dropped the r5 row and orphaned 946136).
  (932617, 932617, 1),
  (932617, 932661, 2),
  (932617, 932662, 3),
  (932617, 932664, 4),
  (932617, 946136, 5),
  -- Lay on Hands Vanilla clones (level-granted by ReconcileBaselineSpells, node 18607 rework)
  (932668, 932668, 1),
  (932668, 932669, 2),
  (932668, 932670, 3);
