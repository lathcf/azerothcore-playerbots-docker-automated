-- Improved Faerie Fire (TBC druid node 20118) — rank chain for the companion debuffs 946277-946279.
--
-- The talent's aura109 ADD_TARGET_TRIGGER carrier casts one of three DIFFERENT spell ids depending
-- on the caster's talent rank. Stock Faerie Fire has no such problem: every druid casts the same
-- spell id, so a second druid's application refreshes rather than adds. Without a rank chain here,
-- two druids with DIFFERENT ranks of this talent (or one druid who respecs while an old debuff is
-- still ticking on a mob) would land two unrelated spell ids that BOTH carry aura184/aura185, and
-- the hit bonuses would STACK — e.g. a rank-3 druid plus a rank-1 druid giving +4% instead of +3%.
--
-- Declaring them a chain makes SpellMgr::IsRankSpellDueToSpell treat them as ranks of one spell, so
-- Unit::_ApplyAura's RemoveNoStackAurasDueToAura replaces the lower rank instead of adding to it —
-- the same "higher-rank casts overwrite the lower buff cleanly" reason the Nature's Grasp chain in
-- 2026_08_31_02 gives. first_spell_id is rank 1 (946277).
--
-- These three are pure triggered debuffs: never learned, never trained, never in a spellbook, and
-- NOT managed by ReconcileBaselineSpells — so unlike the Nature's Grasp chain there is no
-- learn/reset cascade riding on this, it exists only for aura no-stack resolution.
DELETE FROM `spell_ranks` WHERE `first_spell_id`=946277;
INSERT INTO `spell_ranks` (`first_spell_id`,`spell_id`,`rank`) VALUES
  (946277, 946277, 1),
  (946277, 946278, 2),
  (946277, 946279, 3);
