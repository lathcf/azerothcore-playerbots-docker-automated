-- TBC druid Phase 2, Task 8: Omen of Clarity (node 20149) proc wiring for the fresh clone 946269.
--
-- HAND-WRITTEN, not generated: `tools/gen_era_talents.py` hardcodes `ProcsPerMinute = 0` and would
-- zero SpellPhaseMask for the ProcFlags=0 ("use the spell's own DBC events") shape, so the rate and
-- the internal cooldown cannot be expressed from the dataset. This mirrors the shipped Vanilla
-- precedent `2026_08_21_01_era_talent_druid_omen_proc.sql` line for line, with the TBC internal
-- cooldown added.
--
-- WHY IT IS NEEDED: 946269 clones stock 16864 and therefore inherits ProcChance 100. With no
-- `spell_proc` row the core falls back to that DBC chance and Clearcasting fires on EVERY qualifying
-- hit. Stock Omen is gated by a `spell_proc` row at ProcsPerMinute 3.5 (weapon-speed-normalized)
-- PLUS the core `spell_dru_omen_of_clarity` AuraScript, whose CheckProc rejects passives / energize
-- (Furor) / non-druid-non-damage spells and limits melee to on-next-swing. The Clearcasting (16870)
-- cast itself is the DBC default — the script does not PreventDefaultAction.
--
-- HOW THIS DIFFERS FROM THE VANILLA ROW (spec Amendment A.1/A.4, _ref.non_effect_divergences 16864):
--   * `Cooldown` = 10000. TBC 16864 carries SpellAuraOptions.ProcCategoryRecovery 10000, which
--     wowhead renders as the tooltip's "(10s cooldown)". The Vanilla clone 932512 has none.
--   * The MELEE-ONLY restriction is NOT here. TBC's ProcTypeMask 20 (DONE_MELEE_AUTO_ATTACK 0x4 |
--     DONE_SPELL_MELEE_DMG_CLASS 0x10) lives on 946269's own `spell_dbc` row (dataset key
--     `procTypeMask: 20`), precisely because `ProcFlags = 0` below means "use the spell's own DBC
--     events". Setting ProcFlags here instead would work but would put half of TBC's proc identity
--     in hand SQL and half in the dataset; the DBC row is the single source.
--
-- This file does NOT touch 932512's row — the Vanilla clone keeps its own (one row each, so a TBC
-- druid and a Vanilla druid each proc at 3.5 PPM; a duplicate row on either id would double the
-- Clearcasting rate).
--
-- Idempotent: DELETE-before-INSERT. `spell_proc` + `spell_script_names` load at worldserver startup
-- -> restart ac-worldserver to apply.

-- (1) Proc rate + the TBC 10s internal cooldown. ProcFlags 0 => the core uses 946269's own DBC
--     ProcTypeMask (20 = melee only) for the events; Chance 0 because ProcsPerMinute drives the rate.
DELETE FROM `spell_proc` WHERE `SpellId`=946269;
INSERT INTO `spell_proc`
  (`SpellId`,`SchoolMask`,`SpellFamilyName`,`SpellFamilyMask0`,`SpellFamilyMask1`,`SpellFamilyMask2`,
   `ProcFlags`,`SpellTypeMask`,`SpellPhaseMask`,`HitMask`,`AttributesMask`,`DisableEffectsMask`,
   `ProcsPerMinute`,`Chance`,`Cooldown`,`Charges`) VALUES
  (946269, 0, 0, 0, 0, 0,  0, 0, 2, 0, 0, 0,  3.5, 0, 10000, 0);

-- (2) Bind the core Omen AuraScript so CheckProc filtering matches stock (rejects passives/energize/
--     non-druid-non-damage; melee only on next-swing). The clone's EFFECT_0 is aura42
--     PROC_TRIGGER_SPELL (matches the script's OnEffectProc EFFECT_0 registration); the script's
--     Validate() only needs the stock T10-balance ids (present). The T10 HandleProc branch is inert
--     in TBC (no T10 set exists for an era character).
DELETE FROM `spell_script_names` WHERE `spell_id`=946269 AND `ScriptName`='spell_dru_omen_of_clarity';
INSERT INTO `spell_script_names` (`spell_id`,`ScriptName`) VALUES
  (946269, 'spell_dru_omen_of_clarity');
