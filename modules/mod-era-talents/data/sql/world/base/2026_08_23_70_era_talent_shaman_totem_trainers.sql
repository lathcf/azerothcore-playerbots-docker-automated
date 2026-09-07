-- Vanilla Shaman totem trainer wiring (Plan 2 Phase G, 2026-08-23): custom Vanilla-totem
-- summon chains are TRAINER-TAUGHT behind the hidden presence-only marker 932416 ("Era:
-- Vanilla Totems", era-data/vanilla/shaman.yaml helpers:), mirroring the paladin seal-training
-- 932749 pattern (2026_08_20_40) and the same file's ReqAbility1 rank-chain convention. Rank 1
-- of each totem gates on the marker; rank r>1 gates on the previous rank's custom SpellId, so
-- the whole chain is reachable only in order and only for a Vanilla shaman (who alone has
-- 932416 -- granted by EraTalents.cpp's ReconcileBaselineSpells CLASS_SHAMAN block).
--
-- The SAME stock WotLK totem trainer rows are re-gated (not deleted) behind the OTHER marker,
-- 932417 ("Era: WotLK Totems") -- a post-Vanilla shaman (TBC/WotLK) alone has 932417, so a
-- Vanilla shaman can no longer buy the stock totem from the trainer even though the row still
-- exists (needed for 932417 to unlock it once the character transitions eras). Where a stock
-- row already carried a ReqAbility1 (the previous stock rank, e.g. 8154 required 8071), that
-- value is PRESERVED as ReqAbility2 -- the row now requires BOTH the marker AND the previous
-- stock rank, keeping the in-order-training chain intact for WotLK shamans exactly as before.
--
-- This server loads trainers from the modern `trainer`/`trainer_spell` tables (ObjectMgr.cpp),
-- NOT the legacy `npc_trainer`. Shaman GENERAL trainer is `trainer.Id` 14 (Requirement=7 =
-- shaman, 276 stock rows); Id 15 is a 4-spell SPECIALIZED trainer (verified: rows 332/2484/
-- 8017/8042) that happens to also teach stock Earthbind Totem (2484) -- so 2484's custom
-- Vanilla chain and stock re-gate are applied to BOTH 14 and 15; every other totem here only
-- has a stock row on 14 and is applied there only.
--
-- Some stock totem ranks (3599 Searing Totem r1, 5394 Healing Stream Totem r1, 8071 Stoneskin
-- Totem r1) have NO trainer_spell row at all on this core (confirmed by grep of the base
-- trainer_spell.sql dump) -- they are auto-learned, not trainer-taught, so there is nothing
-- to re-gate here; EraTalents.cpp's kStockShaSwap strip (ReconcileBaselineSpells) removes them
-- from a Vanilla shaman's spellbook directly instead. Idempotent: DELETE-before-INSERT
-- throughout (PRIMARY KEY is (TrainerId,SpellId), so a bare re-run would duplicate-key-fail).

-- (a) Custom Vanilla totem summon chains, rooted at marker 932416. MoneyCost mirrors each
--     totem's AUTHENTIC stock 3.3.5a trainer cost (from the core's own trainer_spell dump for
--     the stock counterpart at the same learn level -- e.g. stock Earthbind 2484 L6 = 100c, see
--     section (c) below). The stock trainer cost is a fixed level->cost curve, so where the
--     stock counterpart is auto-learned (no trainer row) or Vanilla-only and absent from 3.3.5a
--     (Grace of Air 8835/10627, Windwall 15107/15111/15112, the r1 auto-learn totems), the cost
--     is interpolated from that same curve at the row's learn level (comment gives the basis).
--     REPLACES the earlier ReqLevel*1000 curve, which over-priced every clone ~60x (bug: level-6
--     Earthbind cost 60s instead of the stock 1s).
DELETE FROM `trainer_spell` WHERE `TrainerId` IN (14,15) AND `SpellId` IN
  (931000,931020,931021,931022,931023,931024,931025,931040,931041,931042,931043,931060,931061,931062,931063,931064,931065,931080,931100,931101,931102,931103,931104,931105,931120,931121,931122,931123,931140,931141,931142,931143,931160,931161,931162,931180,931181,931182,931183,931184,931200,931201,931202,931203,931220,931221,931222,931240,931260,931280,931300,931301,931302,931320,931321,931340,931341,931342,931363,931364,931365,931353,931354,931355,931356,931357);
INSERT INTO `trainer_spell`
  (`TrainerId`,`SpellId`,`MoneyCost`,`ReqSkillLine`,`ReqSkillRank`,`ReqAbility1`,`ReqAbility2`,`ReqAbility3`,`ReqLevel`,`VerifiedBuild`) VALUES
  -- Earthbind (931000, single rank), rooted at marker 932416. Stock 2484 L6 = 100.
  (14, 931000,    100, 0, 0, 932416, 0, 0,  6, 0),
  (15, 931000,    100, 0, 0, 932416, 0, 0,  6, 0),
  -- Stoneskin r1-6 (931020-931025). Stock 8071(auto)/8154/8155/10406/10407/10408; r1 interp @L4.
  (14, 931020,    100, 0, 0, 932416, 0, 0,  4, 0),
  (14, 931021,    900, 0, 0, 931020, 0, 0, 14, 0),
  (14, 931022,   3500, 0, 0, 931021, 0, 0, 24, 0),
  (14, 931023,   9000, 0, 0, 931022, 0, 0, 34, 0),
  (14, 931024,  18000, 0, 0, 931023, 0, 0, 44, 0),
  (14, 931025,  29000, 0, 0, 931024, 0, 0, 54, 0),
  -- StrengthOfEarth r1-4 (931040-931043). Stock 8075/8160/8161/10442.
  (14, 931040,    400, 0, 0, 932416, 0, 0, 10, 0),
  (14, 931041,   3500, 0, 0, 931040, 0, 0, 24, 0),
  (14, 931042,  11000, 0, 0, 931041, 0, 0, 38, 0),
  (14, 931043,  27000, 0, 0, 931042, 0, 0, 52, 0),
  -- Stoneclaw r1-6 (931060-931065). Stock 5730/6390/6391/6392/10427/10428.
  (14, 931060,    100, 0, 0, 932416, 0, 0,  8, 0),
  (14, 931061,   2000, 0, 0, 931060, 0, 0, 18, 0),
  (14, 931062,   6000, 0, 0, 931061, 0, 0, 28, 0),
  (14, 931063,  11000, 0, 0, 931062, 0, 0, 38, 0),
  (14, 931064,  22000, 0, 0, 931063, 0, 0, 48, 0),
  (14, 931065,  32000, 0, 0, 931064, 0, 0, 58, 0),
  -- Tremor (931080, single rank). Stock 8143 L18 = 2000.
  (14, 931080,   2000, 0, 0, 932416, 0, 0, 18, 0),
  -- Searing r1-6 (931100-931105). Stock 3599(auto)/6363/6364/6365/10437/10438; r1 interp @L10.
  (14, 931100,    400, 0, 0, 932416, 0, 0, 10, 0),
  (14, 931101,   2200, 0, 0, 931100, 0, 0, 20, 0),
  (14, 931102,   7000, 0, 0, 931101, 0, 0, 30, 0),
  (14, 931103,  12000, 0, 0, 931102, 0, 0, 40, 0),
  (14, 931104,  24000, 0, 0, 931103, 0, 0, 50, 0),
  (14, 931105,  34000, 0, 0, 931104, 0, 0, 60, 0),
  -- Magma r1-4 (931120-931123). Stock 8190/10585/10586/10587.
  (14, 931120,   4000, 0, 0, 932416, 0, 0, 26, 0),
  (14, 931121,  10000, 0, 0, 931120, 0, 0, 36, 0),
  (14, 931122,  20000, 0, 0, 931121, 0, 0, 46, 0),
  (14, 931123,  30000, 0, 0, 931122, 0, 0, 56, 0),
  -- Flametongue r1-4 (931140-931143). Stock 8227/8249/10526/16387.
  (14, 931140,   6000, 0, 0, 932416, 0, 0, 28, 0),
  (14, 931141,  11000, 0, 0, 931140, 0, 0, 38, 0),
  (14, 931142,  22000, 0, 0, 931141, 0, 0, 48, 0),
  (14, 931143,  32000, 0, 0, 931142, 0, 0, 58, 0),
  -- FrostResistance r1-3 (931160-931162). Stock 8181/10478/10479.
  (14, 931160,   3500, 0, 0, 932416, 0, 0, 24, 0),
  (14, 931161,  11000, 0, 0, 931160, 0, 0, 38, 0),
  (14, 931162,  29000, 0, 0, 931161, 0, 0, 54, 0),
  -- HealingStream r1-5 (931180-931184). Stock 5394(auto)/6375/6377/10462/10463; r1 interp @L20.
  (14, 931180,   2200, 0, 0, 932416, 0, 0, 20, 0),
  (14, 931181,   7000, 0, 0, 931180, 0, 0, 30, 0),
  (14, 931182,  12000, 0, 0, 931181, 0, 0, 40, 0),
  (14, 931183,  24000, 0, 0, 931182, 0, 0, 50, 0),
  (14, 931184,  34000, 0, 0, 931183, 0, 0, 60, 0),
  -- ManaSpring r1-4 (931200-931203). Stock 5675/10495/10496/10497.
  (14, 931200,   4000, 0, 0, 932416, 0, 0, 26, 0),
  (14, 931201,  10000, 0, 0, 931200, 0, 0, 36, 0),
  (14, 931202,  20000, 0, 0, 931201, 0, 0, 46, 0),
  (14, 931203,  30000, 0, 0, 931202, 0, 0, 56, 0),
  -- FireResistance r1-3 (931220-931222). Stock 8184/10537/10538.
  (14, 931220,   6000, 0, 0, 932416, 0, 0, 28, 0),
  (14, 931221,  16000, 0, 0, 931220, 0, 0, 42, 0),
  (14, 931222,  32000, 0, 0, 931221, 0, 0, 58, 0),
  -- PoisonCleansing (931240, single rank). Stock 1.12 summon 8166 (absent 3.3.5a); interp @L22.
  (14, 931240,   3000, 0, 0, 932416, 0, 0, 22, 0),
  -- DiseaseCleansing (931260, single rank). Stock 8170 (Cleansing Totem) L38 = 11000.
  (14, 931260,  11000, 0, 0, 932416, 0, 0, 38, 0),
  -- Grounding (931280, single rank). Stock 8177 L30 = 7000.
  (14, 931280,   7000, 0, 0, 932416, 0, 0, 30, 0),
  -- NatureResistance r1-3 (931300-931302). Stock 10595/10600/10601.
  (14, 931300,   7000, 0, 0, 932416, 0, 0, 30, 0),
  (14, 931301,  18000, 0, 0, 931300, 0, 0, 44, 0),
  (14, 931302,  34000, 0, 0, 931301, 0, 0, 60, 0),
  -- GraceOfAir r1-2 (931320-931321). Stock 8835/10627 absent 3.3.5a; interp @L42/L56.
  (14, 931320,  16000, 0, 0, 932416, 0, 0, 42, 0),
  (14, 931321,  30000, 0, 0, 931320, 0, 0, 56, 0),
  -- Windwall r1-3 (931340-931342). Stock 15107/15111/15112 absent 3.3.5a; interp @L36/L46/L56.
  (14, 931340,  10000, 0, 0, 932416, 0, 0, 36, 0),
  (14, 931341,  20000, 0, 0, 931340, 0, 0, 46, 0),
  (14, 931342,  30000, 0, 0, 931341, 0, 0, 56, 0),
  -- Windfury r1-3 (931363-931365), Plan 2 SP2 Task 5. Stock 8512 L32 = 8000; r2/r3 interp @L42/L52.
  (14, 931363,   8000, 0, 0, 932416, 0, 0, 32, 0),
  (14, 931364,  16000, 0, 0, 931363, 0, 0, 42, 0),
  (14, 931365,  27000, 0, 0, 931364, 0, 0, 52, 0),
  -- Fire Nova r1-5 (931353-931357), Plan 2 SP2 Task 7. Stock 1535/8498/8499/11314/11315.
  (14, 931353,    800, 0, 0, 932416, 0, 0, 12, 0),
  (14, 931354,   3000, 0, 0, 931353, 0, 0, 22, 0),
  (14, 931355,   8000, 0, 0, 931354, 0, 0, 32, 0),
  (14, 931356,  16000, 0, 0, 931355, 0, 0, 42, 0),
  (14, 931357,  27000, 0, 0, 931356, 0, 0, 52, 0);

-- (b) spell_ranks chains for every multi-rank totem: collapses same-name ranks in the client
--     spellbook and makes Reset's removeSpell(rank1) CASCADE via GetNextSpellInChain.
DELETE FROM `spell_ranks` WHERE `first_spell_id` IN
  (931020,931040,931060,931100,931120,931140,931160,931180,931200,931220,931300,931320,931340,931363,931353);
INSERT INTO `spell_ranks` (`first_spell_id`,`spell_id`,`rank`) VALUES
  -- Stoneskin
  (931020, 931020, 1),
  (931020, 931021, 2),
  (931020, 931022, 3),
  (931020, 931023, 4),
  (931020, 931024, 5),
  (931020, 931025, 6),
  -- StrengthOfEarth
  (931040, 931040, 1),
  (931040, 931041, 2),
  (931040, 931042, 3),
  (931040, 931043, 4),
  -- Stoneclaw
  (931060, 931060, 1),
  (931060, 931061, 2),
  (931060, 931062, 3),
  (931060, 931063, 4),
  (931060, 931064, 5),
  (931060, 931065, 6),
  -- Searing
  (931100, 931100, 1),
  (931100, 931101, 2),
  (931100, 931102, 3),
  (931100, 931103, 4),
  (931100, 931104, 5),
  (931100, 931105, 6),
  -- Magma
  (931120, 931120, 1),
  (931120, 931121, 2),
  (931120, 931122, 3),
  (931120, 931123, 4),
  -- Flametongue
  (931140, 931140, 1),
  (931140, 931141, 2),
  (931140, 931142, 3),
  (931140, 931143, 4),
  -- FrostResistance
  (931160, 931160, 1),
  (931160, 931161, 2),
  (931160, 931162, 3),
  -- HealingStream
  (931180, 931180, 1),
  (931180, 931181, 2),
  (931180, 931182, 3),
  (931180, 931183, 4),
  (931180, 931184, 5),
  -- ManaSpring
  (931200, 931200, 1),
  (931200, 931201, 2),
  (931200, 931202, 3),
  (931200, 931203, 4),
  -- FireResistance
  (931220, 931220, 1),
  (931220, 931221, 2),
  (931220, 931222, 3),
  -- NatureResistance
  (931300, 931300, 1),
  (931300, 931301, 2),
  (931300, 931302, 3),
  -- GraceOfAir
  (931320, 931320, 1),
  (931320, 931321, 2),
  -- Windwall
  (931340, 931340, 1),
  (931340, 931341, 2),
  (931340, 931342, 3),
  -- Windfury
  (931363, 931363, 1),
  (931363, 931364, 2),
  (931363, 931365, 3),
  -- Fire Nova
  (931353, 931353, 1),
  (931353, 931354, 2),
  (931353, 931355, 3),
  (931353, 931356, 4),
  (931353, 931357, 5);

-- (b2) Mana Tide Totem ranks 2-3 (931369, 931370): HAND-WRITTEN, NOT rooted at marker 932416 --
--      mirrors the warlock CONFLAGRATE trainer-gate pattern instead
--      (2026_08_14_15_era_talent_warlock_conflagrate_trainer.sql). Rank 1 (custom spell 932407)
--      is granted by the Mana Tide talent (node 18845) alone -- a WotLK shaman never has 932407,
--      so requiring it (ReqAbility1) makes the R2->R3 chain reachable ONLY by a Vanilla shaman who
--      took the talent -- no 932416 marker needed on these two rows.
--        R2 931369  ReqAbility1=932407 (talent-granted R1)  L48  mana 40  (140 -> 155 mana/tick)
--        R3 931370  ReqAbility1=931369 (R2)                 L58  mana 60  (240 mana/tick)
--      1.12 per-rank level/mana from the sourced _ref (era-data/_ref/shaman-totems.yaml
--      ManaTideTotem.vanilla.ranksData: stock rank ids 17354/17359, absent from the 3.3.5a DBC).
--      MoneyCost is a gentle rising curve matching this file's other totem-trainer rows.
DELETE FROM `trainer_spell` WHERE `TrainerId`=14 AND `SpellId` IN (931369,931370);
INSERT INTO `trainer_spell`
  (`TrainerId`,`SpellId`,`MoneyCost`,`ReqSkillLine`,`ReqSkillRank`,`ReqAbility1`,`ReqAbility2`,`ReqAbility3`,`ReqLevel`,`VerifiedBuild`) VALUES
  (14, 931369,  22000, 0, 0, 932407, 0, 0, 48, 0),
  (14, 931370,  32000, 0, 0, 931369, 0, 0, 58, 0);

-- --- spell_ranks chain: 932407 (talent-granted R1) -> 931369 (R2) -> 931370 (R3), so the client
-- spellbook collapses same-name ranks to the highest known and Reset's removeSpell(932407)
-- CASCADES to strip the higher ranks on respec (the Conflagrate 932780-783 precedent). 932407 is
-- talent-granted (era_talent_rank), NOT managed by ReconcileBaselineSpells, so a chain here is safe.
DELETE FROM `spell_ranks` WHERE `first_spell_id`=932407;
INSERT INTO `spell_ranks` (`first_spell_id`,`spell_id`,`rank`) VALUES
  (932407, 932407, 1),
  (932407, 931369, 2),
  (932407, 931370, 3);

-- (c) Re-gate the STOCK totem trainer rows behind marker 932417 -- preserves original MoneyCost/
--     ReqLevel; a row that already had a ReqAbility1 (its own stock prev-rank prereq) keeps it
--     as ReqAbility2, so training order is still enforced for a post-Vanilla (WotLK) shaman.
--     Includes both kStockShaSwap (retuned Vanilla-diverged totems) and kEraShaHide (WrathOfAir/
--     Fire Elemental/Earth Elemental/Cleansing Totem/Fire Nova) from EraTalents.cpp -- Vanilla
--     never had these, so they are hidden from Vanilla shamans the same way.
--     Windfury Totem (kStockShaSwap): only rank 1 (8512, MoneyCost 8000/ReqLevel 32) has a
--     trainer_spell row on this core at all (confirmed by grep of the base trainer_spell.sql
--     dump) -- ranks 2-5 (10613,10614,25585,25587) have none (auto-learned/absent, same "nothing
--     to re-gate" case already noted above for Searing/Flametongue/Stoneskin rank 1), so only
--     8512 gets an INSERT row here.
DELETE FROM `trainer_spell` WHERE `TrainerId` IN (14,15) AND `SpellId` IN
  (2484,5675,5730,6363,6364,6365,6375,6377,6390,6391,6392,8075,8143,8154,8155,8160,8161,8177,8181,8184,8190,8227,8249,10406,10407,10408,10427,10428,10437,10438,10442,10462,10463,10478,10479,10495,10496,10497,10526,10537,10538,10585,10586,10587,10595,10600,10601,16387,3738,2894,2062,8170,1535,8512);
INSERT INTO `trainer_spell`
  (`TrainerId`,`SpellId`,`MoneyCost`,`ReqSkillLine`,`ReqSkillRank`,`ReqAbility1`,`ReqAbility2`,`ReqAbility3`,`ReqLevel`,`VerifiedBuild`) VALUES
  (14, 8512,   8000, 0, 0, 932417, 0, 0, 32, 0),
  (14, 2484,    100, 0, 0, 932417, 0, 0,  6, 0),
  (15, 2484,    100, 0, 0, 932417, 0, 0,  6, 0),
  (14, 5675,   4000, 0, 0, 932417, 0, 0, 26, 0),
  (14, 5730,    100, 0, 0, 932417, 0, 0,  8, 0),
  (14, 6363,   2200, 0, 0, 932417, 3599, 0, 20, 0),
  (14, 6364,   7000, 0, 0, 932417, 6363, 0, 30, 0),
  (14, 6365,  12000, 0, 0, 932417, 6364, 0, 40, 0),
  (14, 6375,   7000, 0, 0, 932417, 5394, 0, 30, 0),
  (14, 6377,  12000, 0, 0, 932417, 6375, 0, 40, 0),
  (14, 6390,   2000, 0, 0, 932417, 5730, 0, 18, 0),
  (14, 6391,   6000, 0, 0, 932417, 6390, 0, 28, 0),
  (14, 6392,  11000, 0, 0, 932417, 6391, 0, 38, 0),
  (14, 8075,    400, 0, 0, 932417, 0, 0, 10, 0),
  (14, 8143,   2000, 0, 0, 932417, 0, 0, 18, 0),
  (14, 8154,    900, 0, 0, 932417, 8071, 0, 14, 0),
  (14, 8155,   3500, 0, 0, 932417, 8154, 0, 24, 0),
  (14, 8160,   3500, 0, 0, 932417, 8075, 0, 24, 0),
  (14, 8161,  11000, 0, 0, 932417, 8160, 0, 38, 0),
  (14, 8177,   7000, 0, 0, 932417, 0, 0, 30, 0),
  (14, 8181,   3500, 0, 0, 932417, 0, 0, 24, 0),
  (14, 8184,   6000, 0, 0, 932417, 0, 0, 28, 0),
  (14, 8190,   4000, 0, 0, 932417, 0, 0, 26, 0),
  (14, 8227,   6000, 0, 0, 932417, 0, 0, 28, 0),
  (14, 8249,  11000, 0, 0, 932417, 8227, 0, 38, 0),
  (14, 10406,   9000, 0, 0, 932417, 8155, 0, 34, 0),
  (14, 10407,  18000, 0, 0, 932417, 10406, 0, 44, 0),
  (14, 10408,  29000, 0, 0, 932417, 10407, 0, 54, 0),
  (14, 10427,  22000, 0, 0, 932417, 6392, 0, 48, 0),
  (14, 10428,  32000, 0, 0, 932417, 10427, 0, 58, 0),
  (14, 10437,  24000, 0, 0, 932417, 6365, 0, 50, 0),
  (14, 10438,  34000, 0, 0, 932417, 10437, 0, 60, 0),
  (14, 10442,  27000, 0, 0, 932417, 8161, 0, 52, 0),
  (14, 10462,  24000, 0, 0, 932417, 6377, 0, 50, 0),
  (14, 10463,  34000, 0, 0, 932417, 10462, 0, 60, 0),
  (14, 10478,  11000, 0, 0, 932417, 8181, 0, 38, 0),
  (14, 10479,  29000, 0, 0, 932417, 10478, 0, 54, 0),
  (14, 10495,  10000, 0, 0, 932417, 5675, 0, 36, 0),
  (14, 10496,  20000, 0, 0, 932417, 10495, 0, 46, 0),
  (14, 10497,  30000, 0, 0, 932417, 10496, 0, 56, 0),
  (14, 10526,  22000, 0, 0, 932417, 8249, 0, 48, 0),
  (14, 10537,  16000, 0, 0, 932417, 8184, 0, 42, 0),
  (14, 10538,  32000, 0, 0, 932417, 10537, 0, 58, 0),
  (14, 10585,  10000, 0, 0, 932417, 8190, 0, 36, 0),
  (14, 10586,  20000, 0, 0, 932417, 10585, 0, 46, 0),
  (14, 10587,  30000, 0, 0, 932417, 10586, 0, 56, 0),
  (14, 10595,   7000, 0, 0, 932417, 0, 0, 30, 0),
  (14, 10600,  18000, 0, 0, 932417, 10595, 0, 44, 0),
  (14, 10601,  34000, 0, 0, 932417, 10600, 0, 60, 0),
  (14, 16387,  32000, 0, 0, 932417, 10526, 0, 58, 0),
  (14, 3738,  47000, 0, 0, 932417, 0, 0, 64, 0),
  (14, 2894,  71000, 0, 0, 932417, 0, 0, 68, 0),
  (14, 2062,  58000, 0, 0, 932417, 0, 0, 66, 0),
  (14, 8170,  11000, 0, 0, 932417, 0, 0, 38, 0),
  (14, 1535,    800, 0, 0, 932417, 0, 0, 12, 0);
