-- mod-enchanter-npc: the Enchanter NPC, its gossip texts, and one spawn per major city.
-- Three templates share one script and differ only in race/model (the `creature` table has no
-- per-spawn model column here): 601020 Night Elf (Alliance capitals), 601023 Undead (Horde
-- capitals), 601024 Human (Shattrath, Dalaran). Models borrowed from enchanters that stand
-- outside those cities, so no trainer gets a look-alike next to it: Alanna Raveneye (Auberdine),
-- Vance Undergloom (Brill), Lucan Cordell (Stormwind).
-- Idempotent (DELETE-then-INSERT on fixed ids): the core updater re-applies a CHANGED base file by hash.
-- NB: this server's `creature` table is single-spawn-id (`id`, not `id1`).

DELETE FROM `creature_template` WHERE `entry` IN (601020, 601023, 601024);
INSERT INTO `creature_template`
  (`entry`, `name`, `subname`, `gossip_menu_id`, `minlevel`, `maxlevel`, `exp`, `faction`, `npcflag`,
   `unit_class`, `type`, `flags_extra`, `ScriptName`)
VALUES
  (601020, 'Selvyn Brightthread', 'Enchantments', 0, 80, 80, 2, 35, 1, 8, 7, 2, 'npc_enchanter_era'),
  (601023, 'Selvyn Brightthread', 'Enchantments', 0, 80, 80, 2, 35, 1, 8, 7, 2, 'npc_enchanter_era'),
  (601024, 'Selvyn Brightthread', 'Enchantments', 0, 80, 80, 2, 35, 1, 8, 7, 2, 'npc_enchanter_era');

DELETE FROM `creature_template_model` WHERE `CreatureID` IN (601020, 601023, 601024);
INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`)
VALUES
  (601020, 0, 1717, 1, 1, 0),   -- Night Elf (Alanna Raveneye's model)
  (601023, 0, 4039, 1, 1, 0),   -- Undead (Vance Undergloom's model)
  (601024, 0, 1492, 1, 1, 0);   -- Human (Lucan Cordell's model)

DELETE FROM `npc_text` WHERE `ID` IN (601020, 601021, 601022);
INSERT INTO `npc_text` (`ID`, `text0_0`, `Probability0`) VALUES
  (601020, 'I can lay any enchantment you could get in your time on your gear, for the price of the materials and a little for my trouble. Which piece?', 1),
  (601021, 'I have nothing for your gear right now. Come back when you have something worth enchanting.', 1),
  (601022, 'The enchanter is closed for the day.', 1);

DELETE FROM `creature` WHERE `guid` BETWEEN 9601000 AND 9601007;
INSERT INTO `creature`
  (`guid`, `id`, `map`, `zoneId`, `areaId`, `spawnMask`, `phaseMask`, `position_x`, `position_y`, `position_z`,
   `orientation`, `spawntimesecs`, `wander_distance`, `MovementType`, `Comment`)
VALUES
  (9601000, 601020, 0,   1519, 0, 1, 1, -8856.50,   801.33,   96.52, 2.216, 300, 0, 0, 'mod-enchanter-npc: Stormwind'),
  (9601001, 601020, 0,   1537, 0, 1, 1, -4806.87, -1193.25,  505.81, 0.904, 300, 0, 0, 'mod-enchanter-npc: Ironforge'),
  (9601002, 601020, 1,   1657, 0, 1, 1, 10141.34,  2320.20, 1333.00, 5.753, 300, 0, 0, 'mod-enchanter-npc: Darnassus'),
  (9601003, 601023, 1,   1637, 0, 1, 1,  1913.38, -4433.65,   24.80, 0.399, 300, 0, 0, 'mod-enchanter-npc: Orgrimmar'),
  (9601004, 601023, 0,   1497, 0, 1, 1,  1484.73,   280.04,  -62.16, 0.958, 300, 0, 0, 'mod-enchanter-npc: Undercity'),
  (9601005, 601023, 1,   1638, 0, 1, 1, -1116.23,    42.38,  140.98, 3.091, 300, 0, 0, 'mod-enchanter-npc: Thunder Bluff'),
  (9601006, 601024, 530, 3703, 0, 1, 1, -2276.52,  5573.12,   79.89, 0.408, 300, 0, 0, 'mod-enchanter-npc: Shattrath'),
  (9601007, 601024, 571, 4395, 0, 1, 1,  5843.82,   726.89,  642.08, 0.523, 300, 0, 0, 'mod-enchanter-npc: Dalaran');
