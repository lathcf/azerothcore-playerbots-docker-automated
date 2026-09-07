-- TBC Paladin seal-training SUBSTRATE (Task 5c, 2026-08-27): the 946xxx standard seals become
-- TRAINER-TAUGHT, mirroring the Vanilla seal model (2026_08_20_40). 5b (commit ee6b646) authored the
-- six standard seals (SoR/SoJ/SoL/SoW/SotC + SoC) as fresh 946xxx clones but AUTO-GRANTED them by level
-- as an interim; the user wants them trainer-learnable instead. This file adds:
--   (a) a NEW hidden "Era: TBC Paladin" seal-training marker (946078) — the TBC analog of the Vanilla
--       932749 marker. Attributes-only (PASSIVE|hidden 262608), no effect but its presence: every TBC
--       custom seal/Judgement rank-1 trainer_spell row has ReqAbility1 = 946078, so a non-TBC paladin
--       (who never holds the marker) can never train the chains. Reconcile grants it to every TBC
--       paladin (EraTalents.cpp CLASS_PALADIN TBC arm); the row is a byte-copy of the generated 932749
--       marker with only the ID + name changed (same hidden-passive shape).
--   (b) trainer_spell rows on the GENERAL paladin trainers (TrainerId 3/4/5, mirroring 2026_08_20_40):
--       SoR/SoJ/SoL/SoW/SotC rank-1 rooted at the marker 946078, higher ranks chained via ReqAbility1
--       off the previous rank. Seal of Command r1 (946027) is TALENT-granted (node 20049, a later task),
--       so its r2-6 (946028-032) root at the talent-granted r1, NOT the marker — mirroring the Vanilla
--       SoC (932606 + 932750-753) precedent. MoneyCost is the level*1000 copper curve of 2026_08_20_40.
--   (c) spell_ranks chains so each multi-rank seal collapses to one spellbook icon and Reset's
--       removeSpell(rank1) CASCADES on respec/strip (SoR/SoJ/SoL/SoW/SotC/SoC).
-- The reused single Judgement (932746) + Lay on Hands (932668-670) stay grant-by-level in the reconcile
-- arm (not seals / not rebalanced) — no trainer rows here. SoB/SoV (faction seals) are Task 5d and will
-- add rows on THIS marker (946078). HAND-WRITTEN. Idempotent: DELETE-before-INSERT.
--
-- Trainer model: this server loads trainers from the modern `trainer`/`trainer_spell` tables; the GENERAL
-- paladin trainers are TrainerId 3/4/5 (Requirement=2 = paladin). Stock WotLK seal/judgement trainer rows
-- were already globally deleted by the Vanilla rework (2026_08_20_40), so a TBC paladin can ONLY train the
-- 946xxx marker-gated seals.

-- (a) The "Era: TBC Paladin" marker (946078) is NO LONGER authored here (2026-08-30): it moved
--     into era-data/tbc/paladin.yaml as a generator-owned helper WITH a client: block, because a
--     trainer ReqAbility spell must exist in the CLIENT Spell.dbc too — Blizzard_TrainerUI.lua:319
--     formats the requirement name unconditionally and a client-unknown id Lua-errors the whole
--     trainer window (real-client repro 2026-08-30). The server row now ships in the generated
--     2026_08_27_01 custom-spells file; the client row rides patch-V.mpq.

-- (b) Custom seal trainer rows. rank-1 rows gate on the marker 946078; higher ranks gate on the
--     previous rank (ReqAbility1). DELETE first (PRIMARY KEY (TrainerId,SpellId)).
DELETE FROM `trainer_spell` WHERE `TrainerId` IN (3,4,5) AND `SpellId` IN
  (946000,946001,946002,946003,946004,946005,946006,946007,946008,946063,946079,946034,946035,946036,946037,946038,946050,946051,946052,946053,946064,946065,946066,946067,946068,946069,946070,946028,946029,946030,946031,946032,946080,946084);
INSERT INTO `trainer_spell`
  (`TrainerId`,`SpellId`,`MoneyCost`,`ReqSkillLine`,`ReqSkillRank`,`ReqAbility1`,`ReqAbility2`,`ReqAbility3`,`ReqLevel`,`VerifiedBuild`) VALUES
  -- TrainerId 3
  -- Seal of Righteousness r1-9 (946000-008), rooted at the marker.
  (3, 946000,   1000, 0, 0, 946078, 0, 0,  1, 0),
  (3, 946001,  10000, 0, 0, 946000, 0, 0, 10, 0),
  (3, 946002,  18000, 0, 0, 946001, 0, 0, 18, 0),
  (3, 946003,  26000, 0, 0, 946002, 0, 0, 26, 0),
  (3, 946004,  34000, 0, 0, 946003, 0, 0, 34, 0),
  (3, 946005,  42000, 0, 0, 946004, 0, 0, 42, 0),
  (3, 946006,  50000, 0, 0, 946005, 0, 0, 50, 0),
  (3, 946007,  58000, 0, 0, 946006, 0, 0, 58, 0),
  (3, 946008,  66000, 0, 0, 946007, 0, 0, 66, 0),
  -- Seal of Justice r1-2 (946063/946079), rooted at the marker.
  (3, 946063,  22000, 0, 0, 946078, 0, 0, 22, 0),
  (3, 946079,  48000, 0, 0, 946063, 0, 0, 48, 0),
  -- Seal of Light r1-5 (946034-038), rooted at the marker.
  (3, 946034,  30000, 0, 0, 946078, 0, 0, 30, 0),
  (3, 946035,  40000, 0, 0, 946034, 0, 0, 40, 0),
  (3, 946036,  50000, 0, 0, 946035, 0, 0, 50, 0),
  (3, 946037,  60000, 0, 0, 946036, 0, 0, 60, 0),
  (3, 946038,  69000, 0, 0, 946037, 0, 0, 69, 0),
  -- Seal of Wisdom r1-4 (946050-053), rooted at the marker.
  (3, 946050,  38000, 0, 0, 946078, 0, 0, 38, 0),
  (3, 946051,  48000, 0, 0, 946050, 0, 0, 48, 0),
  (3, 946052,  58000, 0, 0, 946051, 0, 0, 58, 0),
  (3, 946053,  67000, 0, 0, 946052, 0, 0, 67, 0),
  -- Seal of the Crusader r1-7 (946064-070), rooted at the marker.
  (3, 946064,   6000, 0, 0, 946078, 0, 0,  6, 0),
  (3, 946065,  12000, 0, 0, 946064, 0, 0, 12, 0),
  (3, 946066,  22000, 0, 0, 946065, 0, 0, 22, 0),
  (3, 946067,  32000, 0, 0, 946066, 0, 0, 32, 0),
  (3, 946068,  42000, 0, 0, 946067, 0, 0, 42, 0),
  (3, 946069,  52000, 0, 0, 946068, 0, 0, 52, 0),
  (3, 946070,  61000, 0, 0, 946069, 0, 0, 61, 0),
  -- Seal of Command r2-6 (946028-032), rooted at the TALENT-granted r1 946027 (node 20049), NOT the marker.
  (3, 946028,  30000, 0, 0, 946027, 0, 0, 30, 0),
  (3, 946029,  40000, 0, 0, 946028, 0, 0, 40, 0),
  (3, 946030,  50000, 0, 0, 946029, 0, 0, 50, 0),
  (3, 946031,  60000, 0, 0, 946030, 0, 0, 60, 0),
  (3, 946032,  70000, 0, 0, 946031, 0, 0, 70, 0),
  -- TrainerId 4
  -- Seal of Righteousness r1-9 (946000-008), rooted at the marker.
  (4, 946000,   1000, 0, 0, 946078, 0, 0,  1, 0),
  (4, 946001,  10000, 0, 0, 946000, 0, 0, 10, 0),
  (4, 946002,  18000, 0, 0, 946001, 0, 0, 18, 0),
  (4, 946003,  26000, 0, 0, 946002, 0, 0, 26, 0),
  (4, 946004,  34000, 0, 0, 946003, 0, 0, 34, 0),
  (4, 946005,  42000, 0, 0, 946004, 0, 0, 42, 0),
  (4, 946006,  50000, 0, 0, 946005, 0, 0, 50, 0),
  (4, 946007,  58000, 0, 0, 946006, 0, 0, 58, 0),
  (4, 946008,  66000, 0, 0, 946007, 0, 0, 66, 0),
  -- Seal of Justice r1-2 (946063/946079), rooted at the marker.
  (4, 946063,  22000, 0, 0, 946078, 0, 0, 22, 0),
  (4, 946079,  48000, 0, 0, 946063, 0, 0, 48, 0),
  -- Seal of Light r1-5 (946034-038), rooted at the marker.
  (4, 946034,  30000, 0, 0, 946078, 0, 0, 30, 0),
  (4, 946035,  40000, 0, 0, 946034, 0, 0, 40, 0),
  (4, 946036,  50000, 0, 0, 946035, 0, 0, 50, 0),
  (4, 946037,  60000, 0, 0, 946036, 0, 0, 60, 0),
  (4, 946038,  69000, 0, 0, 946037, 0, 0, 69, 0),
  -- Seal of Wisdom r1-4 (946050-053), rooted at the marker.
  (4, 946050,  38000, 0, 0, 946078, 0, 0, 38, 0),
  (4, 946051,  48000, 0, 0, 946050, 0, 0, 48, 0),
  (4, 946052,  58000, 0, 0, 946051, 0, 0, 58, 0),
  (4, 946053,  67000, 0, 0, 946052, 0, 0, 67, 0),
  -- Seal of the Crusader r1-7 (946064-070), rooted at the marker.
  (4, 946064,   6000, 0, 0, 946078, 0, 0,  6, 0),
  (4, 946065,  12000, 0, 0, 946064, 0, 0, 12, 0),
  (4, 946066,  22000, 0, 0, 946065, 0, 0, 22, 0),
  (4, 946067,  32000, 0, 0, 946066, 0, 0, 32, 0),
  (4, 946068,  42000, 0, 0, 946067, 0, 0, 42, 0),
  (4, 946069,  52000, 0, 0, 946068, 0, 0, 52, 0),
  (4, 946070,  61000, 0, 0, 946069, 0, 0, 61, 0),
  -- Seal of Command r2-6 (946028-032), rooted at the TALENT-granted r1 946027 (node 20049), NOT the marker.
  (4, 946028,  30000, 0, 0, 946027, 0, 0, 30, 0),
  (4, 946029,  40000, 0, 0, 946028, 0, 0, 40, 0),
  (4, 946030,  50000, 0, 0, 946029, 0, 0, 50, 0),
  (4, 946031,  60000, 0, 0, 946030, 0, 0, 60, 0),
  (4, 946032,  70000, 0, 0, 946031, 0, 0, 70, 0),
  -- TrainerId 5
  -- Seal of Righteousness r1-9 (946000-008), rooted at the marker.
  (5, 946000,   1000, 0, 0, 946078, 0, 0,  1, 0),
  (5, 946001,  10000, 0, 0, 946000, 0, 0, 10, 0),
  (5, 946002,  18000, 0, 0, 946001, 0, 0, 18, 0),
  (5, 946003,  26000, 0, 0, 946002, 0, 0, 26, 0),
  (5, 946004,  34000, 0, 0, 946003, 0, 0, 34, 0),
  (5, 946005,  42000, 0, 0, 946004, 0, 0, 42, 0),
  (5, 946006,  50000, 0, 0, 946005, 0, 0, 50, 0),
  (5, 946007,  58000, 0, 0, 946006, 0, 0, 58, 0),
  (5, 946008,  66000, 0, 0, 946007, 0, 0, 66, 0),
  -- Seal of Justice r1-2 (946063/946079), rooted at the marker.
  (5, 946063,  22000, 0, 0, 946078, 0, 0, 22, 0),
  (5, 946079,  48000, 0, 0, 946063, 0, 0, 48, 0),
  -- Seal of Light r1-5 (946034-038), rooted at the marker.
  (5, 946034,  30000, 0, 0, 946078, 0, 0, 30, 0),
  (5, 946035,  40000, 0, 0, 946034, 0, 0, 40, 0),
  (5, 946036,  50000, 0, 0, 946035, 0, 0, 50, 0),
  (5, 946037,  60000, 0, 0, 946036, 0, 0, 60, 0),
  (5, 946038,  69000, 0, 0, 946037, 0, 0, 69, 0),
  -- Seal of Wisdom r1-4 (946050-053), rooted at the marker.
  (5, 946050,  38000, 0, 0, 946078, 0, 0, 38, 0),
  (5, 946051,  48000, 0, 0, 946050, 0, 0, 48, 0),
  (5, 946052,  58000, 0, 0, 946051, 0, 0, 58, 0),
  (5, 946053,  67000, 0, 0, 946052, 0, 0, 67, 0),
  -- Seal of the Crusader r1-7 (946064-070), rooted at the marker.
  (5, 946064,   6000, 0, 0, 946078, 0, 0,  6, 0),
  (5, 946065,  12000, 0, 0, 946064, 0, 0, 12, 0),
  (5, 946066,  22000, 0, 0, 946065, 0, 0, 22, 0),
  (5, 946067,  32000, 0, 0, 946066, 0, 0, 32, 0),
  (5, 946068,  42000, 0, 0, 946067, 0, 0, 42, 0),
  (5, 946069,  52000, 0, 0, 946068, 0, 0, 52, 0),
  (5, 946070,  61000, 0, 0, 946069, 0, 0, 61, 0),
  -- Seal of Command r2-6 (946028-032), rooted at the TALENT-granted r1 946027 (node 20049), NOT the marker.
  (5, 946028,  30000, 0, 0, 946027, 0, 0, 30, 0),
  (5, 946029,  40000, 0, 0, 946028, 0, 0, 40, 0),
  (5, 946030,  50000, 0, 0, 946029, 0, 0, 50, 0),
  (5, 946031,  60000, 0, 0, 946030, 0, 0, 60, 0),
  (5, 946032,  70000, 0, 0, 946031, 0, 0, 70, 0),
  -- TBC FACTION DPS SEALS (Task 5d): single rank each, rooted at the marker 946078, learned at L66.
  -- FACTION-SPLIT by trainer so a HUMAN sees only the faction-appropriate seal (trainer reachability):
  -- TrainerId 3 = Alliance paladin trainers (Stormwind/Ironforge/Exodar) => Seal of Vengeance ONLY;
  -- TrainerId 4 = Horde/Blood Elf paladin trainers (Silvermoon) => Seal of Blood ONLY.
  -- (Neither on TrainerId 5 = the neutral faction-35 "Paladin Trainer", so no both-seals leak there.)
  -- BOTS bypass physical trainers (the factory trainer-walk is class-only-gated) so a bot learns BOTH
  -- regardless of this split; EraTalents::StripWrongFactionTbcPalSeal removes the wrong one on login.
  (4, 946080,  66000, 0, 0, 946078, 0, 0, 66, 0),   -- Seal of Blood (Horde/BE), Silvermoon trainers
  (3, 946084,  66000, 0, 0, 946078, 0, 0, 66, 0);   -- Seal of Vengeance (Alliance), Alliance trainers

-- (c) spell_ranks chains: collapse same-name seal ranks in the spellbook + CASCADE removeSpell(rank1).
DELETE FROM `spell_ranks` WHERE `first_spell_id` IN (946000,946063,946034,946050,946064,946027);
INSERT INTO `spell_ranks` (`first_spell_id`,`spell_id`,`rank`) VALUES
  -- Seal of Righteousness
  (946000, 946000, 1),
  (946000, 946001, 2),
  (946000, 946002, 3),
  (946000, 946003, 4),
  (946000, 946004, 5),
  (946000, 946005, 6),
  (946000, 946006, 7),
  (946000, 946007, 8),
  (946000, 946008, 9),
  -- Seal of Justice
  (946063, 946063, 1),
  (946063, 946079, 2),
  -- Seal of Light
  (946034, 946034, 1),
  (946034, 946035, 2),
  (946034, 946036, 3),
  (946034, 946037, 4),
  (946034, 946038, 5),
  -- Seal of Wisdom
  (946050, 946050, 1),
  (946050, 946051, 2),
  (946050, 946052, 3),
  (946050, 946053, 4),
  -- Seal of the Crusader
  (946064, 946064, 1),
  (946064, 946065, 2),
  (946064, 946066, 3),
  (946064, 946067, 4),
  (946064, 946068, 5),
  (946064, 946069, 6),
  (946064, 946070, 7),
  -- Seal of Command (talent r1, trainer r2-6)
  (946027, 946027, 1),
  (946027, 946028, 2),
  (946027, 946029, 3),
  (946027, 946030, 4),
  (946027, 946031, 5),
  (946027, 946032, 6);

-- ===================================================================================================
-- ACTIVE-RANK TRAINER CHAINS (2026-08-30): higher ranks (r2+) of the four active abilities whose talent
-- node grants ONLY rank 1. Mirrors the Vanilla precedent (2026_08_19_30). UNLIKE the seals above, these
-- root on the TALENT-GRANTED rank 1 (ReqAbility1 = the r1 clone), NOT the marker 946078 — the r1 clone
-- exists ONLY for a TBC paladin who took the talent, so requiring it makes the whole chain reachable
-- ONLY by that paladin (no extra era filtering needed). EraBandClassifier::Sweep owns the strip on
-- respec/era-transition (rule 2: a `spell_ranks` rank above a node-granted r1); the per-class
-- EraTalents.cpp `kPalTrainedChains` arm this header used to name is RETIRED. ReqLevel = wago-2.5.4.44833 SpellLevel. Ids: Holy Shock r2-5
-- (946116/119/122/125, root 946087), Holy Shield r2-4 (946128/130/132, root 946095), Avenger's Shield
-- r2-3 (946134/135, root 946102). Blessing of Sanctuary is NOT here: it REUSES the Vanilla clones
-- 932661/932662/932664, whose trainer rows already ship in 2026_08_19_30 gated on 932617 — the TBC BoS
-- node (20033) also grants 932617, so those rows already serve a TBC paladin. Idempotent: DELETE first.
DELETE FROM `trainer_spell` WHERE `TrainerId` IN (3,4,5) AND `SpellId` IN
  (946116,946119,946122,946125, 946128,946130,946132, 946134,946135);
INSERT INTO `trainer_spell`
  (`TrainerId`,`SpellId`,`MoneyCost`,`ReqSkillLine`,`ReqSkillRank`,`ReqAbility1`,`ReqAbility2`,`ReqAbility3`,`ReqLevel`,`VerifiedBuild`) VALUES
  -- Holy Shock R2/R3/R4/R5 (root talent-granted R1 946087). wago L48/56/64/70.
  (3, 946116,  40000, 0, 0, 946087, 0, 0, 48, 0),
  (3, 946119,  60000, 0, 0, 946116, 0, 0, 56, 0),
  (3, 946122,  90000, 0, 0, 946119, 0, 0, 64, 0),
  (3, 946125, 130000, 0, 0, 946122, 0, 0, 70, 0),
  (4, 946116,  40000, 0, 0, 946087, 0, 0, 48, 0),
  (4, 946119,  60000, 0, 0, 946116, 0, 0, 56, 0),
  (4, 946122,  90000, 0, 0, 946119, 0, 0, 64, 0),
  (4, 946125, 130000, 0, 0, 946122, 0, 0, 70, 0),
  (5, 946116,  40000, 0, 0, 946087, 0, 0, 48, 0),
  (5, 946119,  60000, 0, 0, 946116, 0, 0, 56, 0),
  (5, 946122,  90000, 0, 0, 946119, 0, 0, 64, 0),
  (5, 946125, 130000, 0, 0, 946122, 0, 0, 70, 0),
  -- Holy Shield R2/R3/R4 (root talent-granted R1 946095). wago L50/60/70.
  (3, 946128,  40000, 0, 0, 946095, 0, 0, 50, 0),
  (3, 946130,  80000, 0, 0, 946128, 0, 0, 60, 0),
  (3, 946132, 120000, 0, 0, 946130, 0, 0, 70, 0),
  (4, 946128,  40000, 0, 0, 946095, 0, 0, 50, 0),
  (4, 946130,  80000, 0, 0, 946128, 0, 0, 60, 0),
  (4, 946132, 120000, 0, 0, 946130, 0, 0, 70, 0),
  (5, 946128,  40000, 0, 0, 946095, 0, 0, 50, 0),
  (5, 946130,  80000, 0, 0, 946128, 0, 0, 60, 0),
  (5, 946132, 120000, 0, 0, 946130, 0, 0, 70, 0),
  -- Avenger's Shield R2/R3 (root talent-granted R1 946102). wago L60/70.
  (3, 946134,  80000, 0, 0, 946102, 0, 0, 60, 0),
  (3, 946135, 130000, 0, 0, 946134, 0, 0, 70, 0),
  (4, 946134,  80000, 0, 0, 946102, 0, 0, 60, 0),
  (4, 946135, 130000, 0, 0, 946134, 0, 0, 70, 0),
  (5, 946134,  80000, 0, 0, 946102, 0, 0, 60, 0),
  (5, 946135, 130000, 0, 0, 946134, 0, 0, 70, 0);

-- spell_ranks chains: collapse same-name ranks to one spellbook icon + CASCADE removeSpell(rank1) on
-- respec (GetNextSpellInChain). first_spell_id = the talent-granted r1 clone. Every id has a spell_dbc
-- row (LoadSpellRanks requires it). spell_id is UNIQUE -> DELETE by first_spell_id before re-inserting.
-- (Blessing of Sanctuary's chain (932617 -> 932661/932662/932664) already ships in 2026_08_19_30.)
DELETE FROM `spell_ranks` WHERE `first_spell_id` IN (946087, 946095, 946102);
INSERT INTO `spell_ranks` (`first_spell_id`,`spell_id`,`rank`) VALUES
  -- Holy Shock (r1 talent-granted 946087, r2-5 trainer)
  (946087, 946087, 1),
  (946087, 946116, 2),
  (946087, 946119, 3),
  (946087, 946122, 4),
  (946087, 946125, 5),
  -- Holy Shield (r1 talent-granted 946095, r2-4 trainer)
  (946095, 946095, 1),
  (946095, 946128, 2),
  (946095, 946130, 3),
  (946095, 946132, 4),
  -- Avenger's Shield (r1 talent-granted 946102, r2-3 trainer)
  (946102, 946102, 1),
  (946102, 946134, 2),
  (946102, 946135, 3);
