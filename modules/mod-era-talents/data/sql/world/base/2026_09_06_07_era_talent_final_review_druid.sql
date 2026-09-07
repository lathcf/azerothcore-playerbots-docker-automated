-- Druid FINAL REVIEW round R9 (2026-09-06) — the one hand-written (non-generated) half it needs:
--   (I-30) the `spell_group` memberships the TBC druid era clones do not inherit from the stock
--          spells they replace (or, for Improved Faerie Fire, from the spell they ride along with).
-- Idempotent: DELETE-before-INSERT, and each DELETE names ONLY the ids this module owns (never a
-- whole spell_group id — the stock rows must survive). `spell_group` needs a worldserver restart to
-- apply (SpellMgr::LoadSpellGroups runs at load; there is no .reload for it).
--
-- WHY THIS FILE EXISTS AT ALL: a `template:` clone inherits DBC columns; it does NOT inherit rows in
-- the world DB's own tables. An era clone that REPLACES a grouped stock spell therefore silently
-- leaves that spell's stacking group and starts double-stacking with the very buffs/debuffs the
-- group exists to keep exclusive. Precedent: the warlock round's Unstable Affliction row
-- (2026_09_06_06_era_talent_final_review_warlock.sql) and the warrior round's Enrage/Blood Frenzy rows.
--
-- CHAIN-ROOT RULE (applied per chain below, and the reason some blocks list one id and others list
-- one id only *because* there is only one): `SpellMgr::LoadSpellGroups` REJECTS any spell_id whose
-- `SpellInfo::GetRank() > 1` ("is not first rank of spell") and ERASES the row, while
-- `GetSpellSpellGroupMapBounds` resolves every lookup through `GetFirstSpellInChain`. So for a clone
-- set that IS a `spell_ranks` chain, listing the root alone covers every rank and listing the higher
-- ranks would log an sql.sql error and change nothing. Verified per chain against `spell_ranks`:
--   946277 -> 946278 -> 946279   (chain, root 946277)  => list 946277 ONLY
--   946258 -> 946259 -> 946260   (chain, root 946258)  => list 946258 ONLY
--   946261 -> 946262 -> 946263   (chain, root 946261)  => list 946261 ONLY
--   946257                        (no spell_ranks row, single spell)  => list it
--   946266                        (no spell_ranks row, single spell)  => list it

-- ==================================================================================================
-- 1) Faerie Fire, group 1016 (stack_rule 3 EXCLUSIVE_SAME_EFFECT, "Faerie Fire"; stock members 770 +
--    16857; nested into 1019 "Minor Armor Debuffs" and 1051 "Spell Hit Debuffs", both rule 3).
--
-- Node 20118 (Improved Faerie Fire) deliberately keeps STOCK Faerie Fire 770/16857 (cloning it would
-- break the bear-form Feral cast — see the node comment), and delivers TBC's melee/ranged hit-taken
-- bonus as a COMPANION debuff 946277/278/279 cast on the Faerie Fire target via aura 109.
-- Those companions carry aura 184/185, which stock 770/16857 do not, so this row does NOT make them
-- exclusive with the Faerie Fire they accompany (rule 3 only conflicts on a shared effect) and it
-- does not touch the armor (1019) or spell-hit (1051) nests either. What it DOES fix is the case the
-- group exists for on the druid side: two druids with different Improved Faerie Fire ranks on the
-- same target would otherwise ADD their hit-taken bonuses (+1% and +3% = +4%), where TBC's bonus
-- rides the single, non-stacking Faerie Fire debuff. Rule 3 keeps only the strongest.
DELETE FROM `spell_group` WHERE `id`=1016 AND `spell_id` IN (946277);
INSERT INTO `spell_group` (`id`,`spell_id`) VALUES
  (1016, 946277);   -- TBC Improved Faerie Fire companion debuff r1 (r2/r3 = 946278/946279 resolve via spell_ranks)

-- ==================================================================================================
-- 2) Leader of the Pack, group 1023 (stack_rule 4 EXCLUSIVE_HIGHEST, "Leader of the Pack"; stock
--    member 24932; nested into 1025 "Physical Crit Buffs", rule 3).
--
-- The TBC pair is a hidden form-passive 946256 (PASSIVE|DO_NOT_DISPLAY, TRIGGER_SPELL) plus the
-- VISIBLE party aura 946257 (APPLY_AREA_AURA_PARTY, aura 52 MOD_WEAPON_CRIT_PERCENT, radiusIndex 11)
-- — confirmed off the shipped rows: 946256 is Effect_1=64 TRIGGER_SPELL with no aura, 946257 is
-- Effect_1=35 aura 52. 946257 is the one that REPLACES stock 24932 for a TBC-era druid, so it is the
-- one that must join the group; 946256 grants nothing to anybody and stays out (a passive trigger
-- shell in a crit-buff group would be meaningless).
DELETE FROM `spell_group` WHERE `id`=1023 AND `spell_id` IN (946257);
INSERT INTO `spell_group` (`id`,`spell_id`) VALUES
  (1023, 946257);   -- TBC Leader of the Pack VISIBLE party aura (not the hidden passive 946256)

-- ==================================================================================================
-- 3) Mangle (Bear), group 1030 (no stack rule of its own — it is NESTED into 1033 "Bleed Debuffs",
--    stack_rule 1 EXCLUSIVE; stock member 33878).
-- 4) Mangle (Cat),  group 1031 (same shape, nested into 1033; stock member 33876).
--
-- The exclusivity that matters here lives on the PARENT (1033): Mangle's bleed-damage-taken debuff
-- must not stack with Trauma/Stampede and friends. Membership of 1030/1031 is how a spell reaches
-- 1033 at all, so an era clone outside them is outside the Bleed Debuffs rule entirely.
DELETE FROM `spell_group` WHERE `id`=1030 AND `spell_id` IN (946258);
DELETE FROM `spell_group` WHERE `id`=1031 AND `spell_id` IN (946261);
INSERT INTO `spell_group` (`id`,`spell_id`) VALUES
  (1030, 946258),   -- TBC Mangle (Bear) r1 (r2/r3 = 946259/946260 resolve via spell_ranks)
  (1031, 946261);   -- TBC Mangle (Cat)  r1 (r2/r3 = 946262/946263 resolve via spell_ranks)

-- ==================================================================================================
-- 5) Healing Taken Buffs, group 1094 (stack_rule 3 EXCLUSIVE_SAME_EFFECT; stock members 34123 Tree of
--    Life aura and 63514).
--
-- Same hidden-passive/visible-aura split as Leader of the Pack: 946265 is the hidden FORM_TREE
-- passive (Effect_1=64 TRIGGER_SPELL, no aura) and 946266 is the VISIBLE party aura it fires
-- (Effect_1=35 aura 115 MOD_HEALING, amount computed by era_dru_tree_of_life). 946266 is what
-- replaces stock 34123 for a TBC-era druid, so 946266 joins the group and 946265 does not.
DELETE FROM `spell_group` WHERE `id`=1094 AND `spell_id` IN (946266);
INSERT INTO `spell_group` (`id`,`spell_id`) VALUES
  (1094, 946266);   -- TBC Tree of Life VISIBLE party aura (not the hidden form-passive 946265)
