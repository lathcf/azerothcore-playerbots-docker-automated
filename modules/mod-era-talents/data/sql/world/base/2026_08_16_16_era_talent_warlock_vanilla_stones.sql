-- mod-era-talents: reconstruct the Vanilla Firestone/Spellstone items (2026-08-16).
--
-- The 1.12 stone items survive in item_template flagged "(DEPRECATED)", but every behavior spell
-- they referenced (758/17945/17947/17949, 23480-23483, 128/17729/17730, 32794/32795) is gone from
-- the 3.3.5 DBC — the items were dead shells. The era-talents warlock dataset authors replacement
-- band spells (equip passives 932931-934 + proc damage 932935-938, use spells 932946-948, shared
-- crit passive 932949 — see era-data/vanilla/warlock.yaml); this migration re-points the items at
-- them, restores the authentic 1.12 names/levels, and era-gates the conjuring spells on the warlock
-- trainer. Items are only obtainable via the custom Vanilla-only Create spells (932922-928), so the
-- item edits cannot leak to WotLK-stage characters.
-- NB: clients that already inspected a stone cache the OLD name in Cache/WDB — delete that folder
-- once if a "(DEPRECATED)" name lingers client-side.

-- Firestones: spellid_1 = the combined equip passive (+fire spellpower + melee fire proc),
-- trigger 1 = ON_EQUIP. The old second spell slot is cleared (both halves live on one spell now).
UPDATE `item_template` SET `name`='Lesser Firestone',  `spellid_1`=932931, `spelltrigger_1`=1, `spellid_2`=0, `spelltrigger_2`=0 WHERE `entry`=1254;
UPDATE `item_template` SET `name`='Firestone',         `spellid_1`=932932, `spelltrigger_1`=1, `spellid_2`=0, `spelltrigger_2`=0 WHERE `entry`=13699;
UPDATE `item_template` SET `name`='Greater Firestone', `spellid_1`=932933, `spelltrigger_1`=1, `spellid_2`=0, `spelltrigger_2`=0 WHERE `entry`=13700;
UPDATE `item_template` SET `name`='Major Firestone',   `spellid_1`=932934, `spelltrigger_1`=1, `spellid_2`=0, `spelltrigger_2`=0 WHERE `entry`=13701;

-- Spellstones: spellid_1 = the use spell (dispel + absorb, trigger 0 = ON_USE), spellid_2 = the
-- shared +1% spell-crit equip passive (trigger 1). RequiredLevel restored to the 1.12 values
-- (31/43/55 — the WotLK rows carried 36/48/60).
UPDATE `item_template` SET `name`='Spellstone',         `RequiredLevel`=31, `spellid_1`=932946, `spelltrigger_1`=0, `spellid_2`=932949, `spelltrigger_2`=1 WHERE `entry`=5522;
UPDATE `item_template` SET `name`='Greater Spellstone', `RequiredLevel`=43, `spellid_1`=932947, `spelltrigger_1`=0, `spellid_2`=932949, `spelltrigger_2`=1 WHERE `entry`=13602;
UPDATE `item_template` SET `name`='Major Spellstone',   `RequiredLevel`=55, `spellid_1`=932948, `spelltrigger_1`=0, `spellid_2`=932949, `spelltrigger_2`=1 WHERE `entry`=13603;

-- Trainer rows (warlock class trainer list = TrainerId 31, where the stock Create Firestone/
-- Spellstone ranks live). ERA GATE: ReqAbility 932930 = the hidden "Era: Vanilla Cast Times"
-- marker every Vanilla-era warlock carries (ReconcileBaselineSpells grants/strips it), so only
-- Vanilla-era warlocks ever see these; higher ranks also chain on the previous custom rank like
-- the stock rows do. Costs copied from the stock rank rows. `.reload trainer` applies live.
-- (Legacy `npc_trainer` is dead weight on this core — ObjectMgr loads `trainer_spell` — so no
-- npc_trainer rows are authored; see docs/era-talents-framework.md.)
DELETE FROM `trainer_spell` WHERE `TrainerId`=31 AND `SpellId` IN (932922,932923,932924,932925,932926,932927,932928);
INSERT INTO `trainer_spell` (`TrainerId`, `SpellId`, `MoneyCost`, `ReqSkillLine`, `ReqSkillRank`, `ReqAbility1`, `ReqAbility2`, `ReqAbility3`, `ReqLevel`, `VerifiedBuild`) VALUES
(31, 932922,  5000, 0, 0, 932930,      0, 0, 28, 0),   -- Create Firestone (Lesser)  -> item 1254
(31, 932923,  9000, 0, 0, 932922, 932930, 0, 36, 0),   -- Create Firestone           -> item 13699
(31, 932924, 13000, 0, 0, 932923, 932930, 0, 46, 0),   -- Create Firestone (Greater) -> item 13700
(31, 932925, 22000, 0, 0, 932924, 932930, 0, 56, 0),   -- Create Firestone (Major)   -> item 13701
(31, 932926,  9000, 0, 0, 932930,      0, 0, 31, 0),   -- Create Spellstone          -> item 5522
(31, 932927, 14000, 0, 0, 932926, 932930, 0, 43, 0),   -- Create Spellstone (Greater)-> item 13602
(31, 932928, 26000, 0, 0, 932927, 932930, 0, 55, 0);   -- Create Spellstone (Major)  -> item 13603
