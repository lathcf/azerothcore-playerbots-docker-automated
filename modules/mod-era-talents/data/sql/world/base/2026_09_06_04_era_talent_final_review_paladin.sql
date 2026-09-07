-- =================================================================================================
-- mod-era-talents — PALADIN FINAL REVIEW (2026-09-06), hand SQL.
--
-- One shared-table contract a generated custom-spell row can never carry, because the table is not
-- part of the generator's output: `spell_group`. It is keyed on OUR band ids only, so nothing here
-- touches a stock row.
--
-- Idempotent: the block is DELETE-then-INSERT over exactly the band ids it owns, so AzerothCore
-- re-applying this file after a content edit (it re-runs a CHANGED file by hash, not by name) is a
-- no-op re-write rather than a duplicate insert.
-- =================================================================================================

-- -------------------------------------------------------------------------------------------------
-- I-30 (paladin) — SPELL_GROUP INHERITANCE for the Blessing of Sanctuary clones (BOTH eras).
--
-- `spell_group` membership is by SPELL ID, so a clone inherits none of it. Stock group 1007 is the
-- Blessing of Sanctuary exclusion group — measured on the live DB 2026-09-06, it holds EXACTLY the
-- stock pair:
--     SELECT id, spell_id FROM spell_group WHERE id = 1007;  ->  (1007, 20911), (1007, 25899)
-- i.e. Blessing of Sanctuary 20911 and Greater Blessing of Sanctuary 25899. Without the rows below,
-- an era paladin's BoS clone stacks alongside a real paladin's stock (or Greater) BoS instead of
-- replacing it, because SpellMgr's group lookup never sees the clone id.
--
-- Members added (the FIVE castable BoS clones a character can actually hold):
--   Vanilla ranks 1-4 : 932617 (r1, node-granted) / 932661 (r2) / 932662 (r3) / 932664 (r4)
--   TBC     rank  5   : 946136 (the TBC-Classic-only r5, trainer-taught off r4 932664)
-- The hidden block-damage payloads (932618/932663/932665/932667/946137) are NOT members: they are
-- triggered damage spells, never auras, so exclusion is meaningless for them.
--
-- Group id 1007 is a STOCK id and its two stock rows are deliberately left alone — this file only
-- ADDS band ids to it, and the DELETE below is scoped to those band ids so a re-apply can never
-- drop 20911/25899.
-- -------------------------------------------------------------------------------------------------
DELETE FROM `spell_group` WHERE `id`=1007 AND `spell_id` IN (932617, 932661, 932662, 932664, 946136);
INSERT INTO `spell_group` (`id`, `spell_id`) VALUES
  (1007, 932617),
  (1007, 932661),
  (1007, 932662),
  (1007, 932664),
  (1007, 946136);
