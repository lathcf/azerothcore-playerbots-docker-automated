-- mod-era-talents TBC Phase 8 (2026-09-05): the four TBC warlock talent-rooted CLONE chains.
--
-- Amendment A.1 makes four warlock talents band clones in TBC, each with trained ranks above the
-- node-granted rank 1:
--   Siphon Life   node 20713 -> 948451 (r1, node grant, L30) + r2-r6 948452-948456 at L38/48/58/63/70
--   Conflagrate   node 20760 -> 948650 (r1, node grant, L40) + r2-r6 948651-948655 at L48/54/60/65/70
--   Unstable Aff. node 20720 -> 948470 (r1, node grant, L50) + r2/r3 948471/948472 at L60/70
--   Shadowfury    node 20763 -> 948670 (r1, node grant, L50) + r2/r3 948671/948672 at L60/70
-- Shadowburn (20750) and Dark Pact (20717) are GRANTS of the stock ids, whose stock TrainerId-31
-- chains already self-gate on the talent-granted r1 — NOTHING is written for them here (A.1).
--
-- ROW SHAPE (the 2026_09_07_02 Create-clone shape): ReqAbility1 = the PREVIOUS rank (so the chain is
-- trained in order and r2 is unreachable without the node's r1), ReqAbility2 = 948410 "Era: TBC
-- Warlock" (so a Vanilla or WotLK warlock never sees the rows even if they somehow hold a rank).
--
-- COSTS: the <=60 ranks reuse the authentic Vanilla-curve costs already shipped for the same talents
-- (2026_08_14_14 Siphon Life 10000/14000/20000; 2026_08_14_15 Conflagrate 16000/22000/30000); the
-- 61-70 ranks price at 2500, which is what the LIVE stock TBC rows charge for exactly these two
-- chains' TBC ranks (trainer_spell 30404/30405 = 2500 each, 30413/30414 = 2500 each, dumped
-- 2026-09-05). UA/Shadowfury are all-TBC ranks, so both of their rows are 2500.
--
-- NOTHING IS DELETED except the idempotent DELETE-before-INSERT of the rows this file owns: the
-- stock UA/Shadowfury trained rows (30404/30405/47841/47843, 30413/30414/47846/47847) stay in place
-- for native WotLK warlocks and are reached through their own stock r1 (30108/30283), which a TBC
-- warlock no longer holds (reconcile's kEraWlTbcStockSwaps strips it). Orphaned-higher-rank check
-- (lesson 4): every ReqAbility1 written here is either the node-granted r1 of its own chain or the
-- previous custom rank, so no stock chain root is removed and none is orphaned.
--
-- This server loads `trainer`/`trainer_spell` (ObjectMgr.cpp), NOT legacy `npc_trainer`.
-- `.reload trainer` (or a restart) applies it live.

DELETE FROM `trainer_spell` WHERE `TrainerId`=31 AND `SpellId` IN
  (948452,948453,948454,948455,948456, 948651,948652,948653,948654,948655, 948471,948472, 948671,948672);
INSERT INTO `trainer_spell` (`TrainerId`,`SpellId`,`MoneyCost`,`ReqSkillLine`,`ReqSkillRank`,`ReqAbility1`,`ReqAbility2`,`ReqAbility3`,`ReqLevel`,`VerifiedBuild`) VALUES
-- Siphon Life r2-r6 (node 20713; r1 948451 is the node grant at L30)
(31, 948452, 10000, 0, 0, 948451, 948410, 0, 38, 0),
(31, 948453, 14000, 0, 0, 948452, 948410, 0, 48, 0),
(31, 948454, 20000, 0, 0, 948453, 948410, 0, 58, 0),
(31, 948455,  2500, 0, 0, 948454, 948410, 0, 63, 0),
(31, 948456,  2500, 0, 0, 948455, 948410, 0, 70, 0),
-- Conflagrate r2-r6 (node 20760; r1 948650 is the node grant at L40)
(31, 948651, 16000, 0, 0, 948650, 948410, 0, 48, 0),
(31, 948652, 22000, 0, 0, 948651, 948410, 0, 54, 0),
(31, 948653, 30000, 0, 0, 948652, 948410, 0, 60, 0),
(31, 948654,  2500, 0, 0, 948653, 948410, 0, 65, 0),
(31, 948655,  2500, 0, 0, 948654, 948410, 0, 70, 0),
-- Unstable Affliction r2/r3 (node 20720; r1 948470 is the node grant at L50)
(31, 948471,  2500, 0, 0, 948470, 948410, 0, 60, 0),
(31, 948472,  2500, 0, 0, 948471, 948410, 0, 70, 0),
-- Shadowfury r2/r3 (node 20763; r1 948670 is the node grant at L50)
(31, 948671,  2500, 0, 0, 948670, 948410, 0, 60, 0),
(31, 948672,  2500, 0, 0, 948671, 948410, 0, 70, 0);

-- spell_ranks: make each clone chain a real rank chain so the client spellbook collapses same-name
-- ranks to the highest known AND so EraTalents::Reset's removeSpell(r1) CASCADES down the chain on
-- the respec path (the Vanilla Siphon Life / Conflagrate precedent). first_spell_id is the
-- node-granted r1. `spell_id` is UNIQUE, so DELETE the whole chain by first_spell_id first.
DELETE FROM `spell_ranks` WHERE `first_spell_id` IN (948451, 948650, 948470, 948670);
INSERT INTO `spell_ranks` (`first_spell_id`,`spell_id`,`rank`) VALUES
(948451,948451,1),(948451,948452,2),(948451,948453,3),(948451,948454,4),(948451,948455,5),(948451,948456,6),
(948650,948650,1),(948650,948651,2),(948650,948652,3),(948650,948653,4),(948650,948654,5),(948650,948655,6),
(948470,948470,1),(948470,948471,2),(948470,948472,3),
(948670,948670,1),(948670,948671,2),(948670,948672,3);
