-- mod-era-talents TBC Phase 8 (2026-09-05): the four-marker warlock chain + the TBC stone set.
-- 932930 is now the SHARED pre-Wrath Corruption cast-time marker (TBC Corruption is a 2 s cast), so it can no
-- longer gate the Vanilla stone recipes. Gates move to per-era markers: 932994 (Vanilla) / 948410 (TBC);
-- 932939 stays the WotLK gate (its rows are unchanged from 2026_08_24_11). Nothing deleted; the Vanilla rows
-- are UPDATEd in place (orphan check: every ReqAbility chain root stays reachable for its own era).
--
-- PRE-STATE of the two Master items, printed live 2026-09-05 before writing this file:
--   entry   name                            RequiredLevel  spellid_1  spelltrigger_1  spellid_2  spelltrigger_2
--   22128   Master Firestone (DEPRECATED)   0              27252      1               27256      1
--   22646   Master Spellstone (DEPRECATED)  66             28170      0               32789      1
-- Both rows already match the reconstructed Vanilla tiers on every other equip-relevant column
-- (InventoryType 28, Flags 2097154, Quality 1, class 4 / subclass 0 — same as 13701 / 13603), so this file
-- touches only name / RequiredLevel / spellid_N / spelltrigger_N, exactly like 2026_08_16_16.
-- 27252/27256/28170/32789 are the dead TBC behaviour spells (absent from the 3.3.5 DBC), which is why the
-- items were shells; the band spells 948430-948433 (era-data/tbc/warlock.yaml) replace them.
-- Same accepted consequence as the Vanilla reconstruction: the STOCK Create rows 27250 / 28172 (WotLK
-- warlocks, gated on 932939) conjure these same global items and therefore also get the band behaviour —
-- items are global, per 2026_08_16_16's precedent.

-- (a) Vanilla Create clones: re-gate 932930 -> 932994.
UPDATE `trainer_spell` SET `ReqAbility1`=932994 WHERE `TrainerId`=31 AND `SpellId` IN (932922, 932926) AND `ReqAbility1`=932930;
UPDATE `trainer_spell` SET `ReqAbility2`=932994 WHERE `TrainerId`=31 AND `SpellId` IN (932923,932924,932925,932927,932928) AND `ReqAbility2`=932930;

-- (b) TBC Create clones behind 948410, stock costs/levels.
DELETE FROM `trainer_spell` WHERE `TrainerId`=31 AND `SpellId` IN (948420,948421,948422,948423,948424,948425,948426,948427,948428);
INSERT INTO `trainer_spell` (`TrainerId`,`SpellId`,`MoneyCost`,`ReqSkillLine`,`ReqSkillRank`,`ReqAbility1`,`ReqAbility2`,`ReqAbility3`,`ReqLevel`,`VerifiedBuild`) VALUES
(31, 948420,  5000, 0, 0, 948410,      0, 0, 28, 0),   -- Create Firestone (Lesser)  -> 1254
(31, 948421,  9000, 0, 0, 948420, 948410, 0, 36, 0),   -- Create Firestone           -> 13699
(31, 948422, 13000, 0, 0, 948421, 948410, 0, 46, 0),   -- Create Firestone (Greater) -> 13700
(31, 948423, 22000, 0, 0, 948422, 948410, 0, 56, 0),   -- Create Firestone (Major)   -> 13701
(31, 948424, 51000, 0, 0, 948423, 948410, 0, 66, 0),   -- Create Firestone (Master)  -> 22128
(31, 948425,  9000, 0, 0, 948410,      0, 0, 36, 0),   -- Create Spellstone          -> 5522
(31, 948426, 14000, 0, 0, 948425, 948410, 0, 48, 0),   -- Create Spellstone (Greater)-> 13602
(31, 948427, 26000, 0, 0, 948426, 948410, 0, 60, 0),   -- Create Spellstone (Major)  -> 13603
(31, 948428, 51000, 0, 0, 948427, 948410, 0, 66, 0);   -- Create Spellstone (Master) -> 22646

-- spell_ranks for the TBC Create chains: same reason as 2026_08_24_11 (c) — without it the same-name
-- ranks show as separate spellbook entries instead of superseding.
DELETE FROM `spell_ranks` WHERE `first_spell_id` IN (948420, 948425);
INSERT INTO `spell_ranks` (`first_spell_id`,`spell_id`,`rank`) VALUES
(948420,948420,1),(948420,948421,2),(948420,948422,3),(948420,948423,4),(948420,948424,5),
(948425,948425,1),(948425,948426,2),(948425,948427,3),(948425,948428,4);

-- (c) Master-tier items: reconstruct exactly like 2026_08_16_16 (deprecated stock rows re-pointed at band spells).
UPDATE `item_template` SET `name`='Master Firestone',  `RequiredLevel`=66, `spellid_1`=948430, `spelltrigger_1`=1, `spellid_2`=0,      `spelltrigger_2`=0 WHERE `entry`=22128;
UPDATE `item_template` SET `name`='Master Spellstone', `RequiredLevel`=66, `spellid_1`=948432, `spelltrigger_1`=0, `spellid_2`=948433, `spelltrigger_2`=1 WHERE `entry`=22646;
