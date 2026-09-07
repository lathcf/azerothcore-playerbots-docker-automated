-- Warlock FINAL REVIEW round R8 (2026-09-06) — the one hand-written (non-generated) half it needs:
--   (I-30) the `spell_group` membership the TBC Unstable Affliction era clone does not inherit from
--          the stock spell it replaces.
-- Idempotent: DELETE-before-INSERT, and the DELETE names only the id this module owns (never a whole
-- spell_group id — the stock rows in group 1099 must survive). `spell_group` needs a worldserver
-- restart to apply (SpellMgr::LoadSpellGroups runs at load; there is no .reload for it).

-- ==================================================================================================
-- Immolate / Unstable Affliction exclusivity, group 1099 (stack_rule 2, "Immolate and Unstable
-- Affliction" — verified in `spell_group_stack_rules`). Stock members: 348 (Immolate rank 1) and
-- 30108 (Unstable Affliction rank 1).
--
-- A `template:` clone inherits DBC columns; it does NOT inherit rows in the world DB's own tables, so
-- an era clone that REPLACES a grouped stock spell silently leaves its exclusivity group. TBC node
-- 20720 grants the clone 948470 instead of stock 30108, so without this row a TBC-era warlock could
-- hold Immolate and his era Unstable Affliction on the same target at once — the one thing the group
-- exists to prevent. Precedent: the warrior round's Enrage/Blood Frenzy rows
-- (2026_09_06_05_era_talent_final_review_warrior.sql) and group 1107's Arcane Power clones.
--
-- ONLY THE CHAIN ROOT IS LISTED, and that is deliberate. `SpellMgr::LoadSpellGroups` REJECTS any
-- spell_id whose `SpellInfo::GetRank() > 1` ("is not first rank of spell") and ERASES the row, while
-- `GetSpellSpellGroupMapBounds` resolves every lookup through `GetFirstSpellInChain`. 948471/948472
-- are ranks 2/3 of the `spell_ranks` chain rooted at 948470 (inserted by
-- 2026_09_07_03_era_talent_tbc_warlock_trained_chains.sql), exactly as stock 30404/30405/47841/47843
-- are ranks 2-5 under 30108 — so listing them would log an sql.sql error AND change nothing, and
-- listing 948470 alone already covers all three ranks. (This is the opposite of the warrior round's
-- Enrage case, where each era clone was its OWN chain root and every rank therefore needed a row.)
--
-- The Vanilla warlock has no counterpart here: Unstable Affliction is a TBC spell and the Vanilla
-- dataset clones neither it nor Immolate (TBC uses stock Immolate 348, which is already a member).
DELETE FROM `spell_group` WHERE `id`=1099 AND `spell_id` IN (948470);
INSERT INTO `spell_group` (`id`,`spell_id`) VALUES
  (1099, 948470);   -- TBC Unstable Affliction clone r1 (ranks 2/3 = 948471/948472 resolve via spell_ranks)
