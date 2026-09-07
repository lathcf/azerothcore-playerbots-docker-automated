-- =================================================================================================
-- mod-era-talents — MAGE FINAL REVIEW (2026-09-06), hand SQL.
--
-- Two shared-table contracts a generated custom-spell row can never carry, because neither table is
-- part of the generator's output: `spell_linked_spell` and `spell_group`. Both are keyed on OUR band
-- ids only, so nothing here touches a stock row.
--
-- Idempotent: every block is DELETE-then-INSERT over exactly the band ids it owns, so AzerothCore
-- re-applying this file after a content edit (it re-runs a CHANGED file by hash, not by name) is a
-- no-op re-write rather than a duplicate insert.
-- =================================================================================================

-- -------------------------------------------------------------------------------------------------
-- F-1 — COMBUSTION: remove the crit stack when the buff ends (BOTH eras).
--
-- Core `spell_mage_combustion` builds the mage's crit bonus as stacks of 28682 while Combustion is
-- up; the STOCK spell sheds them through the `spell_linked_spell` row (-11129 -> -28682, type 0 =
-- "on aura remove, remove the linked aura"). A band clone has a DIFFERENT id, so that row never
-- reaches it (the same class of miss as SpellInfoCorrections) — the Vanilla clone 932325 and the
-- TBC clone 948887 each left a permanent 28682 stack behind when Combustion expired or its three
-- charges ran out. Column list mirrors the live -11129 row verbatim.
-- -------------------------------------------------------------------------------------------------
DELETE FROM `spell_linked_spell` WHERE `spell_trigger` IN (-932325, -948887);
INSERT INTO `spell_linked_spell` (`spell_trigger`, `spell_effect`, `type`, `comment`) VALUES
(-932325, -28682, 0, 'Era Vanilla Combustion — remove crit stack on end'),
(-948887, -28682, 0, 'Era TBC Combustion — remove crit stack on end');

-- -------------------------------------------------------------------------------------------------
-- I-17 — SPELL_GROUP INHERITANCE for the era clones.
--
-- `spell_group` membership is by SPELL ID, so a clone inherits none of it. Three clone pairs need it:
--
--   Arcane Power   932760 (Vanilla) / 948760 (TBC)  -> 1107, 1123
--       Stock 12042's own two memberships, and ONLY those: 1107 "Temporary Damage Increases"
--       (rule 4) and 1123 "Power Infusion and Arcane Power" (rule 4). Group 1122 "Temporary Haste
--       Buffs" (Bloodlust 2825 / Power Infusion 10060 / Heroism 32182) is deliberately NOT written
--       here — stock Arcane Power is not in it, and 1122 belongs to Power Infusion (the priest
--       slice owns it). The DELETE below is by spell_id across ALL groups, so re-applying this file
--       also removes the two 1122 rows an earlier revision of it inserted.
--   Winter's Chill 932764 (Vanilla) / 948960 (TBC)  -> 1037 "Spell Crit Debuffs" (rule 3), the
--       group that already holds stock 12579 + the Improved Scorch chain.
--   Fire Vulnerability 932763 (Vanilla) / 948860 (TBC) -> 1037, the group that already holds stock
--       22959.
--
-- Column list mirrors `SELECT * FROM spell_group WHERE spell_id = 12042` — the table is (id, spell_id)
-- with both columns in the primary key.
-- -------------------------------------------------------------------------------------------------
DELETE FROM `spell_group` WHERE `spell_id` IN (932760, 948760, 932764, 948960, 932763, 948860);
INSERT INTO `spell_group` (`id`, `spell_id`) VALUES
-- Arcane Power clones
(1107, 932760), (1123, 932760),
(1107, 948760), (1123, 948760),
-- Winter's Chill clones
(1037, 932764), (1037, 948960),
-- Fire Vulnerability clones
(1037, 932763), (1037, 948860);
