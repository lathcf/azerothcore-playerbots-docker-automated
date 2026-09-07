-- mod-era-talents TBC Phase 9 (2026-09-05): the mage talent-rooted CLONE chains, BOTH eras.
--
-- This phase clones seven trainer-taught chains — four in TBC and three in Vanilla — plus two
-- node-granted Magic Absorption chains that need only `spell_ranks`. Every chain's rank 1 is
-- NODE-granted; ranks 2+ are trainer-taught on TrainerId 16 (the mage trainer set), each row
-- `ReqAbility1`-chained behind the previous rank and therefore rooted at the node grant. That root
-- is what makes the rows era-safe on a shared TrainerId: only a TBC mage can ever hold 948870 /
-- 948880 / 948890 / 948964, and only a Vanilla mage 932310 / 932320 / 932331, so neither era can
-- see the other's rows and a WotLK mage sees neither. The mage tree mints NO era marker (spec §5),
-- so `ReqAbility2` is 0 on every row — unlike the warlock chains (2026_09_07_03), which gate on the
-- 948410 marker because their Create-stone rows are not rooted in a talent grant.
--
--   TBC (era-data/tbc/mage.yaml)
--     Pyroblast        node 20830 -> 948870 (r1 grant) + r2-r10 948871-948879  L24..70
--     Blast Wave       node 20837 -> 948880 (r1 grant) + r2-r7  948881-948886  L36..70
--     Dragon's Breath  node 20844 -> 948890 (r1 grant) + r2-r4  948891-948893  L56/64/70
--     Ice Barrier      node 20863 -> 948964 (r1 grant) + r2-r6  948965-948969  L46..70
--     Magic Absorption node 20804 -> 948762-948766, granted PER RANK by the node (no trainer rows;
--                                    `spell_ranks` only, so the clones behave as one rank chain)
--   Vanilla (era-data/vanilla/mage.yaml — the Amendment A.4 / A.7 re-audit FIX rows)
--     Pyroblast        node 18024 -> 932310 (r1 grant) + r2-r8  932311-932317  L24..60
--     Blast Wave       node 18030 -> 932320 (r1 grant) + r2-r5  932321-932324  L36..60
--     Ice Barrier      node 18049 -> 932331 (r1 grant) + r2-r4  932332-932334  L46/52/58
--     Magic Absorption node 18005 -> 932326-932330, granted PER RANK (as TBC above)
--     Combustion       node 18032 -> 932325 (single rank, node grant — nothing to write here)
--
-- COSTS AND LEVELS are the LIVE stock TrainerId-16 rows for the same rank of the same spell (dumped
-- 2026-09-05), never a `ReqLevel * 1000` formula:
--   Pyroblast       12505/12522/12523/12524/12525/12526/18809/27132/33938  L24/30/36/42/48/54/60/66/70
--                   cost 100/400/650/900/1400/1800/2100/3900/7500
--   Blast Wave      13018/13019/13020/13021/27133/33933                    L36/44/52/60/65/70
--                   cost 400/1148/1748/2100/3500/6000
--   Dragon's Breath 33041/33042/33043                                      L56/64/70
--                   cost 1900/2200/2500
--   Ice Barrier     13031/13032/13033/27134/33405                          L46/52/58/64/70
--                   cost 1700/1748/2000/2500/6000
-- The Vanilla chains stop where the era does (Pyroblast r8 = L60, Blast Wave r5 = L60, Ice Barrier
-- r4 = L58) and reuse the same stock costs for the ranks they keep.
--
-- NOTHING IS DELETED FROM THE STOCK ROWS. The only DELETEs here are the idempotent
-- DELETE-before-INSERT of the custom rows this file owns, plus the `spell_dbc` cleanup at the
-- bottom. Orphaned-higher-rank check (workflow lesson 4): every `ReqAbility1` written here is either
-- this file's own previous custom rank or a node-granted clone r1 — no stock row is removed, so no
-- stock higher rank is orphaned, and native WotLK mages keep the whole stock chain (including the
-- WotLK-only ranks 42890/42891, 42944/42945, 42949/42950, 43038/43039) reached through the stock r1.
--
-- This server loads `trainer`/`trainer_spell` (ObjectMgr.cpp), NOT legacy `npc_trainer`.
-- `.reload trainer` (or a restart) applies it live.

DELETE FROM `trainer_spell` WHERE `TrainerId`=16 AND `SpellId` IN
  (948871,948872,948873,948874,948875,948876,948877,948878,948879,
   948881,948882,948883,948884,948885,948886,
   948891,948892,948893,
   948965,948966,948967,948968,948969,
   932311,932312,932313,932314,932315,932316,932317,
   932321,932322,932323,932324,
   932332,932333,932334);
INSERT INTO `trainer_spell` (`TrainerId`,`SpellId`,`MoneyCost`,`ReqSkillLine`,`ReqSkillRank`,`ReqAbility1`,`ReqAbility2`,`ReqAbility3`,`ReqLevel`,`VerifiedBuild`) VALUES
-- TBC Pyroblast r2-r10 (node 20830; r1 948870 is the node grant)
(16, 948871,  100, 0, 0, 948870, 0, 0, 24, 0),
(16, 948872,  400, 0, 0, 948871, 0, 0, 30, 0),
(16, 948873,  650, 0, 0, 948872, 0, 0, 36, 0),
(16, 948874,  900, 0, 0, 948873, 0, 0, 42, 0),
(16, 948875, 1400, 0, 0, 948874, 0, 0, 48, 0),
(16, 948876, 1800, 0, 0, 948875, 0, 0, 54, 0),
(16, 948877, 2100, 0, 0, 948876, 0, 0, 60, 0),
(16, 948878, 3900, 0, 0, 948877, 0, 0, 66, 0),
(16, 948879, 7500, 0, 0, 948878, 0, 0, 70, 0),
-- TBC Blast Wave r2-r7 (node 20837; r1 948880 is the node grant)
(16, 948881,  400, 0, 0, 948880, 0, 0, 36, 0),
(16, 948882, 1148, 0, 0, 948881, 0, 0, 44, 0),
(16, 948883, 1748, 0, 0, 948882, 0, 0, 52, 0),
(16, 948884, 2100, 0, 0, 948883, 0, 0, 60, 0),
(16, 948885, 3500, 0, 0, 948884, 0, 0, 65, 0),
(16, 948886, 6000, 0, 0, 948885, 0, 0, 70, 0),
-- TBC Dragon's Breath r2-r4 (node 20844; r1 948890 is the node grant)
(16, 948891, 1900, 0, 0, 948890, 0, 0, 56, 0),
(16, 948892, 2200, 0, 0, 948891, 0, 0, 64, 0),
(16, 948893, 2500, 0, 0, 948892, 0, 0, 70, 0),
-- TBC Ice Barrier r2-r6 (node 20863; r1 948964 is the node grant)
(16, 948965, 1700, 0, 0, 948964, 0, 0, 46, 0),
(16, 948966, 1748, 0, 0, 948965, 0, 0, 52, 0),
(16, 948967, 2000, 0, 0, 948966, 0, 0, 58, 0),
(16, 948968, 2500, 0, 0, 948967, 0, 0, 64, 0),
(16, 948969, 6000, 0, 0, 948968, 0, 0, 70, 0),
-- Vanilla Pyroblast r2-r8 (node 18024; r1 932310 is the node grant)
(16, 932311,  100, 0, 0, 932310, 0, 0, 24, 0),
(16, 932312,  400, 0, 0, 932311, 0, 0, 30, 0),
(16, 932313,  650, 0, 0, 932312, 0, 0, 36, 0),
(16, 932314,  900, 0, 0, 932313, 0, 0, 42, 0),
(16, 932315, 1400, 0, 0, 932314, 0, 0, 48, 0),
(16, 932316, 1800, 0, 0, 932315, 0, 0, 54, 0),
(16, 932317, 2100, 0, 0, 932316, 0, 0, 60, 0),
-- Vanilla Blast Wave r2-r5 (node 18030; r1 932320 is the node grant)
(16, 932321,  400, 0, 0, 932320, 0, 0, 36, 0),
(16, 932322, 1148, 0, 0, 932321, 0, 0, 44, 0),
(16, 932323, 1748, 0, 0, 932322, 0, 0, 52, 0),
(16, 932324, 2100, 0, 0, 932323, 0, 0, 60, 0),
-- Vanilla Ice Barrier r2-r4 (node 18049; r1 932331 is the node grant)
(16, 932332, 1700, 0, 0, 932331, 0, 0, 46, 0),
(16, 932333, 1748, 0, 0, 932332, 0, 0, 52, 0),
(16, 932334, 2000, 0, 0, 932333, 0, 0, 58, 0);

-- spell_ranks: make each clone chain a real rank chain so (a) the client spellbook collapses
-- same-name ranks to the highest known, (b) `SpellInfo::IsRankOf` stops two ranks stacking, and
-- (c) `EraTalents::Reset`'s removeSpell(r1) CASCADES down the chain through GetNextSpellInChain on
-- the respec path (the Vanilla Siphon Life / Conflagrate and TBC warlock precedents).
-- first_spell_id is the node-granted r1 in every chain, INCLUDING the two Magic Absorption chains,
-- which have no trainer rows at all (the node grants each rank directly) but still need the chain
-- so two ranks never coexist. `spell_id` is UNIQUE, so DELETE the whole chain by first_spell_id.
DELETE FROM `spell_ranks` WHERE `first_spell_id` IN
  (948870, 948880, 948890, 948964, 948762, 932310, 932320, 932331, 932326);
INSERT INTO `spell_ranks` (`first_spell_id`,`spell_id`,`rank`) VALUES
-- TBC Pyroblast r1-r10
(948870,948870,1),(948870,948871,2),(948870,948872,3),(948870,948873,4),(948870,948874,5),
(948870,948875,6),(948870,948876,7),(948870,948877,8),(948870,948878,9),(948870,948879,10),
-- TBC Blast Wave r1-r7
(948880,948880,1),(948880,948881,2),(948880,948882,3),(948880,948883,4),(948880,948884,5),
(948880,948885,6),(948880,948886,7),
-- TBC Dragon's Breath r1-r4
(948890,948890,1),(948890,948891,2),(948890,948892,3),(948890,948893,4),
-- TBC Ice Barrier r1-r6
(948964,948964,1),(948964,948965,2),(948964,948966,3),(948964,948967,4),(948964,948968,5),
(948964,948969,6),
-- TBC Magic Absorption r1-r5 (node-granted per rank, node 20804)
(948762,948762,1),(948762,948763,2),(948762,948764,3),(948762,948765,4),(948762,948766,5),
-- Vanilla Pyroblast r1-r8
(932310,932310,1),(932310,932311,2),(932310,932312,3),(932310,932313,4),(932310,932314,5),
(932310,932315,6),(932310,932316,7),(932310,932317,8),
-- Vanilla Blast Wave r1-r5
(932320,932320,1),(932320,932321,2),(932320,932322,3),(932320,932323,4),(932320,932324,5),
-- Vanilla Ice Barrier r1-r4
(932331,932331,1),(932331,932332,2),(932331,932333,3),(932331,932334,4),
-- Vanilla Magic Absorption r1-r5 (node-granted per rank, node 18005)
(932326,932326,1),(932326,932327,2),(932326,932328,3),(932326,932329,4),(932326,932330,5);

-- Stranded Vanilla auto-passives 920040-920044 (spec Amendment B.4). Vanilla node 18005 changed
-- from a `mechanic: stat` node to a grant node in Task 2, so the generator no longer emits these
-- five rows in the Vanilla custom-spells SQL — but a DB that ran the PREVIOUS generation still
-- holds them in `spell_dbc`, where nothing would ever clean them up (the generated file only
-- rewrites the ids it still emits). They are legitimate for no era and no class, so drop them here.
-- The matching `character_spell` cleanup is Phase 9.5's generic band-orphan sweep
-- (`EraBandClassifier::Sweep`, run after every strip arm inside `ReconcileBaselineSpells` for every
-- era): 920040-920044 are node-granted by nothing and are listed under `stranded:` in
-- `era-data/band-allowlist.yaml`, so the classifier strips them on login/learn/reset. There is no
-- 920040-920044 arm in the `CLASS_MAGE` reconcile block — per-class band strips were all deleted in
-- favour of the classifier.
DELETE FROM `spell_dbc` WHERE `ID` BETWEEN 920040 AND 920044;
