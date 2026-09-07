-- TBC Shaman totem trainer wiring (Plan 3b Task 5, 2026-09-01): the TBC totem summon chains are
-- TRAINER-TAUGHT behind the hidden presence-only marker 947440 ("Era: TBC Totems",
-- era-data/tbc/shaman.yaml helpers:), mirroring the Vanilla file this one is modelled on
-- (2026_08_23_70_era_talent_shaman_totem_trainers.sql -- read its header for the trainer-id and
-- ReqAbility conventions). Rank 1 of each family gates on the marker; rank r>1 gates on the
-- previous rank's custom SpellId, so each chain is reachable only in order and only for a TBC
-- shaman (who alone holds 947440 -- granted by Task 6's three-way CLASS_SHAMAN reconcile arm).
--
-- Unlike the plan's expectation, NO reused 931xxx id appears here: Task 2/3 minted a fresh 947xxx
-- SUMMON for every family (reuse covered pulses/passives/creatures, which are never trainer-taught),
-- so the Vanilla 932416-gated rows are untouched and there is no (TrainerId,SpellId) PK collision.
--
-- SEARING is the exception on purpose (the standing "leave stock alone" obligation): ranks 1-6 are
-- GRANT-STOCK (r1 3599 auto-learned; r2-r6 stock trainer rows un-gated to prev-rank-only in the
-- companion re-gate file), and only the retuned r7 clone 947090 appears here -- gated on the marker
-- AND stock r6 10438 so it slots on top of the stock chain for TBC shamans only.
--
-- MANA TIDE (947340) and TOTEM OF WRATH (947350) are talent-granted (nodes 20256/20219) and have
-- NO trainer rows. The two ELEMENTALS and WRATH OF AIR are TBC-new trainer content and DO.
-- MoneyCost follows the AUTHENTIC totem trainer level->cost curve (the stock 3.3.5a totem
-- trainer_spell costs, extended above L60 for TBC ranks) -- e.g. an Earthbind clone @L6 costs
-- 100c, not 6000c. REPLACES the earlier ReqLevel*1000 curve, which over-priced low ranks ~60x.
-- Idempotent: DELETE-before-INSERT throughout (PK is (TrainerId,SpellId)).

-- (a) Custom TBC totem summon chains, rooted at marker 947440.
DELETE FROM `trainer_spell` WHERE `TrainerId` IN (14,15) AND `SpellId` IN
  (947000,947020,947021,947022,947023,947024,947025,947026,947027,947030,947031,947032,947033,947034,947035,947050,947051,947052,947053,947054,947055,947056,947070,947080,947090,947115,947116,947117,947118,947119,947120,947121,947122,947123,947124,947140,947141,947142,947143,947150,947170,947171,947172,947173,947174,947175,947190,947191,947192,947193,947194,947200,947201,947202,947203,947210,947220,947240,947250,947251,947252,947253,947260,947270,947271,947272,947280,947281,947282,947283,947290,947300,947310,947311,947312,947313,947314,947315,947316,947330,947331,947332,947333,947334);
INSERT INTO `trainer_spell`
  (`TrainerId`,`SpellId`,`MoneyCost`,`ReqSkillLine`,`ReqSkillRank`,`ReqAbility1`,`ReqAbility2`,`ReqAbility3`,`ReqLevel`,`VerifiedBuild`) VALUES
  -- Earthbind (947000, single rank; trainers 14 AND 15 -- Id 15 also teaches stock 2484).
  (14, 947000,   100, 0, 0, 947440, 0, 0,  6, 0),
  (15, 947000,   100, 0, 0, 947440, 0, 0,  6, 0),
  -- Stoneskin r1-8 (947020-947027).
  (14, 947020,   100, 0, 0, 947440, 0, 0,  4, 0),
  (14, 947021,  900, 0, 0, 947020, 0, 0, 14, 0),
  (14, 947022,  3500, 0, 0, 947021, 0, 0, 24, 0),
  (14, 947023,  9000, 0, 0, 947022, 0, 0, 34, 0),
  (14, 947024,  18000, 0, 0, 947023, 0, 0, 44, 0),
  (14, 947025,  29000, 0, 0, 947024, 0, 0, 54, 0),
  (14, 947026,  43000, 0, 0, 947025, 0, 0, 63, 0),
  (14, 947027,  66000, 0, 0, 947026, 0, 0, 70, 0),
  -- Strength of Earth r1-6 (947030-947035).
  (14, 947030,  400, 0, 0, 947440, 0, 0, 10, 0),
  (14, 947031,  3500, 0, 0, 947030, 0, 0, 24, 0),
  (14, 947032,  11000, 0, 0, 947031, 0, 0, 38, 0),
  (14, 947033,  27000, 0, 0, 947032, 0, 0, 52, 0),
  (14, 947034,  34000, 0, 0, 947033, 0, 0, 60, 0),
  (14, 947035,  49000, 0, 0, 947034, 0, 0, 65, 0),
  -- Stoneclaw r1-7 (947050-947056).
  (14, 947050,   100, 0, 0, 947440, 0, 0,  8, 0),
  (14, 947051,  2000, 0, 0, 947050, 0, 0, 18, 0),
  (14, 947052,  6000, 0, 0, 947051, 0, 0, 28, 0),
  (14, 947053,  11000, 0, 0, 947052, 0, 0, 38, 0),
  (14, 947054,  22000, 0, 0, 947053, 0, 0, 48, 0),
  (14, 947055,  32000, 0, 0, 947054, 0, 0, 58, 0),
  (14, 947056,  55000, 0, 0, 947055, 0, 0, 67, 0),
  -- Tremor (947070, single rank).
  (14, 947070,  2000, 0, 0, 947440, 0, 0, 18, 0),
  -- Earth Elemental Totem (947080, single rank, TBC-new).
  (14, 947080,  52000, 0, 0, 947440, 0, 0, 66, 0),
  -- Searing r7 ONLY (947090) -- r1-r6 are GRANT-STOCK (see the re-gate file); the custom top rank
  -- gates on the marker AND the stock r6, so only a TBC shaman who trained the stock chain gets it.
  (14, 947090,  62000, 0, 0, 947440, 10438, 0, 69, 0),
  -- Magma r1-5 (947115-947119).
  (14, 947115,  4000, 0, 0, 947440, 0, 0, 26, 0),
  (14, 947116,  10000, 0, 0, 947115, 0, 0, 36, 0),
  (14, 947117,  20000, 0, 0, 947116, 0, 0, 46, 0),
  (14, 947118,  30000, 0, 0, 947117, 0, 0, 56, 0),
  (14, 947119,  49000, 0, 0, 947118, 0, 0, 65, 0),
  -- Flametongue r1-5 (947120-947124).
  (14, 947120,  6000, 0, 0, 947440, 0, 0, 28, 0),
  (14, 947121,  11000, 0, 0, 947120, 0, 0, 38, 0),
  (14, 947122,  22000, 0, 0, 947121, 0, 0, 48, 0),
  (14, 947123,  32000, 0, 0, 947122, 0, 0, 58, 0),
  (14, 947124,  55000, 0, 0, 947123, 0, 0, 67, 0),
  -- Frost Resistance r1-4 (947140-947143).
  (14, 947140,  3500, 0, 0, 947440, 0, 0, 24, 0),
  (14, 947141,  11000, 0, 0, 947140, 0, 0, 38, 0),
  (14, 947142,  29000, 0, 0, 947141, 0, 0, 54, 0),
  (14, 947143,  55000, 0, 0, 947142, 0, 0, 67, 0),
  -- Fire Elemental Totem (947150, single rank, TBC-new).
  (14, 947150,  58000, 0, 0, 947440, 0, 0, 68, 0),
  -- Healing Stream r1-6 (947170-947175).
  (14, 947170,  2200, 0, 0, 947440, 0, 0, 20, 0),
  (14, 947171,  7000, 0, 0, 947170, 0, 0, 30, 0),
  (14, 947172,  12000, 0, 0, 947171, 0, 0, 40, 0),
  (14, 947173,  24000, 0, 0, 947172, 0, 0, 50, 0),
  (14, 947174,  34000, 0, 0, 947173, 0, 0, 60, 0),
  (14, 947175,  62000, 0, 0, 947174, 0, 0, 69, 0),
  -- Mana Spring r1-5 (947190-947194).
  (14, 947190,  4000, 0, 0, 947440, 0, 0, 26, 0),
  (14, 947191,  10000, 0, 0, 947190, 0, 0, 36, 0),
  (14, 947192,  20000, 0, 0, 947191, 0, 0, 46, 0),
  (14, 947193,  30000, 0, 0, 947192, 0, 0, 56, 0),
  (14, 947194,  49000, 0, 0, 947193, 0, 0, 65, 0),
  -- Fire Resistance r1-4 (947200-947203).
  (14, 947200,  6000, 0, 0, 947440, 0, 0, 28, 0),
  (14, 947201,  16000, 0, 0, 947200, 0, 0, 42, 0),
  (14, 947202,  32000, 0, 0, 947201, 0, 0, 58, 0),
  (14, 947203,  58000, 0, 0, 947202, 0, 0, 68, 0),
  -- Poison Cleansing (947210, single rank).
  (14, 947210,  3000, 0, 0, 947440, 0, 0, 22, 0),
  -- Disease Cleansing (947220, single rank).
  (14, 947220,  11000, 0, 0, 947440, 0, 0, 38, 0),
  -- Grounding (947240, single rank).
  (14, 947240,  7000, 0, 0, 947440, 0, 0, 30, 0),
  -- Nature Resistance r1-4 (947250-947253).
  (14, 947250,  7000, 0, 0, 947440, 0, 0, 30, 0),
  (14, 947251,  18000, 0, 0, 947250, 0, 0, 44, 0),
  (14, 947252,  34000, 0, 0, 947251, 0, 0, 60, 0),
  (14, 947253,  62000, 0, 0, 947252, 0, 0, 69, 0),
  -- Sentry (947260, single rank).
  (14, 947260,  9000, 0, 0, 947440, 0, 0, 34, 0),
  -- Grace of Air r1-3 (947270-947272).
  (14, 947270,  16000, 0, 0, 947440, 0, 0, 42, 0),
  (14, 947271,  30000, 0, 0, 947270, 0, 0, 56, 0),
  (14, 947272,  34000, 0, 0, 947271, 0, 0, 60, 0),
  -- Windwall r1-4 (947280-947283).
  (14, 947280,  10000, 0, 0, 947440, 0, 0, 36, 0),
  (14, 947281,  20000, 0, 0, 947280, 0, 0, 46, 0),
  (14, 947282,  30000, 0, 0, 947281, 0, 0, 56, 0),
  (14, 947283,  49000, 0, 0, 947282, 0, 0, 65, 0),
  -- Tranquil Air (947290, single rank, TBC-only).
  (14, 947290,  24000, 0, 0, 947440, 0, 0, 50, 0),
  -- Wrath of Air (947300, single rank, TBC-new).
  (14, 947300,  46000, 0, 0, 947440, 0, 0, 64, 0),
  -- Fire Nova r1-7 (947310-947316).
  (14, 947310,  800, 0, 0, 947440, 0, 0, 12, 0),
  (14, 947311,  3000, 0, 0, 947310, 0, 0, 22, 0),
  (14, 947312,  8000, 0, 0, 947311, 0, 0, 32, 0),
  (14, 947313,  16000, 0, 0, 947312, 0, 0, 42, 0),
  (14, 947314,  27000, 0, 0, 947313, 0, 0, 52, 0),
  (14, 947315,  37000, 0, 0, 947314, 0, 0, 61, 0),
  (14, 947316,  66000, 0, 0, 947315, 0, 0, 70, 0),
  -- Windfury r1-5 (947330-947334).
  (14, 947330,  8000, 0, 0, 947440, 0, 0, 32, 0),
  (14, 947331,  16000, 0, 0, 947330, 0, 0, 42, 0),
  (14, 947332,  27000, 0, 0, 947331, 0, 0, 52, 0),
  (14, 947333,  37000, 0, 0, 947332, 0, 0, 61, 0),
  (14, 947334,  66000, 0, 0, 947333, 0, 0, 70, 0);

-- (b) spell_ranks chains for every multi-rank TBC totem family (deferred here by Task 3): collapses
--     same-name ranks in the client spellbook and makes removeSpell(rank1) CASCADE via
--     GetNextSpellInChain. Each mixed chain roots at its RANK-1 id per the plan.
--     SEARING deliberately has NO chain row: its ranks 1-7 live on the STOCK chain rooted at 3599
--     (base spell_ranks), whose rank-7 slot stock 25533 already occupies -- adding 947090 there
--     would double-book rank 7, so the custom r7 stays chainless (it shows as a second "Searing
--     Totem" spellbook entry above the collapsed stock chain; cosmetic, correct).
--     MANA TIDE and TOTEM OF WRATH are single-rank in TBC: no chain (unlike Vanilla's 3-rank
--     932407 chain, which is 932416-era content and untouched).
DELETE FROM `spell_ranks` WHERE `first_spell_id` IN
  (947020,947030,947050,947115,947120,947140,947170,947190,947200,947250,947270,947280,947310,947330);
INSERT INTO `spell_ranks` (`first_spell_id`,`spell_id`,`rank`) VALUES
  -- Stoneskin
  (947020, 947020, 1),
  (947020, 947021, 2),
  (947020, 947022, 3),
  (947020, 947023, 4),
  (947020, 947024, 5),
  (947020, 947025, 6),
  (947020, 947026, 7),
  (947020, 947027, 8),
  -- StrengthOfEarth
  (947030, 947030, 1),
  (947030, 947031, 2),
  (947030, 947032, 3),
  (947030, 947033, 4),
  (947030, 947034, 5),
  (947030, 947035, 6),
  -- Stoneclaw
  (947050, 947050, 1),
  (947050, 947051, 2),
  (947050, 947052, 3),
  (947050, 947053, 4),
  (947050, 947054, 5),
  (947050, 947055, 6),
  (947050, 947056, 7),
  -- Magma
  (947115, 947115, 1),
  (947115, 947116, 2),
  (947115, 947117, 3),
  (947115, 947118, 4),
  (947115, 947119, 5),
  -- Flametongue
  (947120, 947120, 1),
  (947120, 947121, 2),
  (947120, 947122, 3),
  (947120, 947123, 4),
  (947120, 947124, 5),
  -- FrostResistance
  (947140, 947140, 1),
  (947140, 947141, 2),
  (947140, 947142, 3),
  (947140, 947143, 4),
  -- HealingStream
  (947170, 947170, 1),
  (947170, 947171, 2),
  (947170, 947172, 3),
  (947170, 947173, 4),
  (947170, 947174, 5),
  (947170, 947175, 6),
  -- ManaSpring
  (947190, 947190, 1),
  (947190, 947191, 2),
  (947190, 947192, 3),
  (947190, 947193, 4),
  (947190, 947194, 5),
  -- FireResistance
  (947200, 947200, 1),
  (947200, 947201, 2),
  (947200, 947202, 3),
  (947200, 947203, 4),
  -- NatureResistance
  (947250, 947250, 1),
  (947250, 947251, 2),
  (947250, 947252, 3),
  (947250, 947253, 4),
  -- GraceOfAir
  (947270, 947270, 1),
  (947270, 947271, 2),
  (947270, 947272, 3),
  -- Windwall
  (947280, 947280, 1),
  (947280, 947281, 2),
  (947280, 947282, 3),
  (947280, 947283, 4),
  -- FireNova
  (947310, 947310, 1),
  (947310, 947311, 2),
  (947310, 947312, 3),
  (947310, 947313, 4),
  (947310, 947314, 5),
  (947310, 947315, 6),
  (947310, 947316, 7),
  -- Windfury
  (947330, 947330, 1),
  (947330, 947331, 2),
  (947330, 947332, 3),
  (947330, 947333, 4),
  (947330, 947334, 5);
