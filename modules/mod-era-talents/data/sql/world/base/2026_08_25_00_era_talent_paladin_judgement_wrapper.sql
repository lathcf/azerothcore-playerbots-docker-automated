-- Paladin "Judgement" trainer WRAPPER leak (2026-08-25). Stock base data ships ungated
-- trainer_spell rows for spell 10321 on TrainerIds 3/4/5/6 (ReqLevel 4). 10321 is a Blizzard
-- LEARN-WRAPPER named "Judgement": Effect_1=36 teaches 20271 (WotLK Judgement of Light),
-- Effect_2=36 teaches 21084 (WotLK Seal of Righteousness). One purchase hands a Vanilla-era
-- paladin BOTH stock spells the 2026_08_20_40 rework version-swapped. That rework missed it
-- because (a) it searched trainer_spell for the TAUGHT ids (20271/20154) — a wrapper row is
-- keyed by the WRAPPER id — and (b) TrainerId 6 was excluded as a "5-spell specialized
-- trainer" when it is actually the STARTER-zone paladin trainer (creatures 925/926, Brother
-- Sammuel et al.). Two corrections to 2026_08_20_40's header while here: the legacy
-- `npc_trainer` table is loaded NOWHERE in this fork (dead data — its 200003/200004 template
-- rows are red herrings), and "20271 is not a trainer spell" was true only of DIRECT rows.
-- C++ side (same fix round): EraTalents.cpp adds 21084 to kStockPalSwap (login self-heal for
-- a Vanilla paladin who already bought the wrapper) and to kWotlkPalStock (level 4 — WotLK-
-- stage paladins keep the real WotLK SoR now that its only trainer source is gone; creation
-- grants only old-id 20154). era_audit.py's check_trainer_wrapper_leaks locks the invariant.
-- HAND-WRITTEN. Idempotent: DELETE-before-INSERT.

-- (a) Delete the wrapper everywhere (all four paladin trainer templates).
DELETE FROM `trainer_spell` WHERE `TrainerId` IN (3,4,5,6) AND `SpellId` = 10321;

-- (b) TrainerId 6 parity: mirror 2026_08_20_40's marker-gated custom seal/Judgement/LoH set
--     (its section (b)) onto the starter trainer, same costs/levels/ReqAbility1 chains —
--     rooted at the hidden marker 932749, so only a Vanilla-era paladin ever sees them; a
--     fresh Vanilla paladin can now train SoR R1 + Judgement in the starting zone, like real
--     Vanilla. WotLK paladins see nothing new.
DELETE FROM `trainer_spell` WHERE `TrainerId` = 6 AND `SpellId` IN
  (932700,932701,932702,932703,932704,932705,932706,932707,
   932724,
   932725,932726,932727,932728,
   932737,932738,932739,
   932634,932635,932636,932637,932638,932639,
   932746,
   932668,932669,932670);
INSERT INTO `trainer_spell`
  (`TrainerId`,`SpellId`,`MoneyCost`,`ReqSkillLine`,`ReqSkillRank`,`ReqAbility1`,`ReqAbility2`,`ReqAbility3`,`ReqLevel`,`VerifiedBuild`) VALUES
  -- Seal of Righteousness r1-8 (932700-707), rooted at the marker.
  (6, 932700,  1000, 0, 0, 932749, 0, 0,  1, 0),
  (6, 932701, 10000, 0, 0, 932700, 0, 0, 10, 0),
  (6, 932702, 18000, 0, 0, 932701, 0, 0, 18, 0),
  (6, 932703, 26000, 0, 0, 932702, 0, 0, 26, 0),
  (6, 932704, 34000, 0, 0, 932703, 0, 0, 34, 0),
  (6, 932705, 42000, 0, 0, 932704, 0, 0, 42, 0),
  (6, 932706, 50000, 0, 0, 932705, 0, 0, 50, 0),
  (6, 932707, 58000, 0, 0, 932706, 0, 0, 58, 0),
  -- Seal of Justice (932724, single rank), rooted at the marker.
  (6, 932724, 22000, 0, 0, 932749, 0, 0, 22, 0),
  -- Seal of Light r1-4 (932725-728), rooted at the marker.
  (6, 932725, 30000, 0, 0, 932749, 0, 0, 30, 0),
  (6, 932726, 40000, 0, 0, 932725, 0, 0, 40, 0),
  (6, 932727, 50000, 0, 0, 932726, 0, 0, 50, 0),
  (6, 932728, 60000, 0, 0, 932727, 0, 0, 60, 0),
  -- Seal of Wisdom r1-3 (932737-739), rooted at the marker.
  (6, 932737, 38000, 0, 0, 932749, 0, 0, 38, 0),
  (6, 932738, 48000, 0, 0, 932737, 0, 0, 48, 0),
  (6, 932739, 58000, 0, 0, 932738, 0, 0, 58, 0),
  -- Seal of the Crusader r1-6 (932634-639), rooted at the marker.
  (6, 932634,  6000, 0, 0, 932749, 0, 0,  6, 0),
  (6, 932635, 12000, 0, 0, 932634, 0, 0, 12, 0),
  (6, 932636, 22000, 0, 0, 932635, 0, 0, 22, 0),
  (6, 932637, 32000, 0, 0, 932636, 0, 0, 32, 0),
  (6, 932638, 42000, 0, 0, 932637, 0, 0, 42, 0),
  (6, 932639, 52000, 0, 0, 932638, 0, 0, 52, 0),
  -- Judgement (932746, single rank, the keystone button), rooted at the marker.
  (6, 932746,  4000, 0, 0, 932749, 0, 0,  4, 0),
  -- Lay on Hands r1-3 (932668-670), rooted at the marker.
  (6, 932668, 10000, 0, 0, 932749, 0, 0, 10, 0),
  (6, 932669, 30000, 0, 0, 932668, 0, 0, 30, 0),
  (6, 932670, 50000, 0, 0, 932669, 0, 0, 50, 0);

-- (b2) TrainerId 6 parity for Seal of Command R2-5 (932750-753) — mirrors 2026_08_20_40's
--      (b2): rooted at the TALENT-granted rank 1 (932606), NOT the 932749 marker (SoC is a
--      Ret talent; a paladin without it never has 932606, so the chain is unreachable).
DELETE FROM `trainer_spell` WHERE `TrainerId` = 6 AND `SpellId` IN
  (932750,932751,932752,932753);
INSERT INTO `trainer_spell`
  (`TrainerId`,`SpellId`,`MoneyCost`,`ReqSkillLine`,`ReqSkillRank`,`ReqAbility1`,`ReqAbility2`,`ReqAbility3`,`ReqLevel`,`VerifiedBuild`) VALUES
  (6, 932750, 30000, 0, 0, 932606, 0, 0, 30, 0),
  (6, 932751, 40000, 0, 0, 932750, 0, 0, 40, 0),
  (6, 932752, 50000, 0, 0, 932751, 0, 0, 50, 0),
  (6, 932753, 60000, 0, 0, 932752, 0, 0, 60, 0);
