-- Druid fix round (2026-08-21), follow-up #2: Omen of Clarity proc rate.
-- Real-client pass found the castable Omen clone 932512 (issue 3) triggering Clearcasting on EVERY
-- hit. Root cause: the clone inherited stock 16864's DBC ProcFlags (proc events) + ProcChance 100,
-- but got NO `spell_proc` row and was NOT bound to the core AuraScript — so the proc system fell back
-- to the 100% DBC chance. Stock Omen 16864 is gated by a `spell_proc` row at ProcsPerMinute 3.5
-- (weapon-speed-normalized, ~5-6%/hit on a fast weapon) PLUS the core `spell_dru_omen_of_clarity`
-- AuraScript whose CheckProc rejects passives / energize (Furor) / non-druid-non-damage spells and
-- limits melee to on-next-swing. The Clearcasting (16870) cast itself is the DBC default (the script
-- does not PreventDefaultAction). HAND-WRITTEN (not generated): the generator hardcodes
-- ProcsPerMinute=0 and would zero SpellPhaseMask for the ProcFlags=0-rely-on-DBC shape, so this
-- mirrors stock 16864's exact `spell_proc` values. Idempotent: DELETE-before-INSERT. spell_proc +
-- spell_script_names load at worldserver startup -> restart ac-worldserver to apply (no rebuild, no
-- regen, no client MPQ change: Omen's client data is unchanged).

-- (1) Proc rate: mirror stock 16864 exactly (ProcsPerMinute 3.5, SpellPhaseMask 2, all else 0 =>
--     ProcFlags 0 makes the core use 932512's own DBC ProcFlags for the proc events; Chance 0 because
--     ProcsPerMinute drives the rate).
DELETE FROM `spell_proc` WHERE `SpellId`=932512;
INSERT INTO `spell_proc`
  (`SpellId`,`SchoolMask`,`SpellFamilyName`,`SpellFamilyMask0`,`SpellFamilyMask1`,`SpellFamilyMask2`,
   `ProcFlags`,`SpellTypeMask`,`SpellPhaseMask`,`HitMask`,`AttributesMask`,`DisableEffectsMask`,
   `ProcsPerMinute`,`Chance`,`Cooldown`,`Charges`) VALUES
  (932512, 0, 0, 0, 0, 0,  0, 0, 2, 0, 0, 0,  3.5, 0, 0, 0);

-- (2) Bind the core Omen AuraScript so CheckProc filtering matches stock (rejects passives/energize/
--     non-druid-non-damage; melee only on next-swing). The clone's EFFECT_0 is aura42
--     PROC_TRIGGER_SPELL (matches the script's OnEffectProc EFFECT_0 registration); the script's
--     Validate() only needs the stock T10-balance ids (present). T10 HandleProc is inert in Vanilla.
DELETE FROM `spell_script_names` WHERE `spell_id`=932512 AND `ScriptName`='spell_dru_omen_of_clarity';
INSERT INTO `spell_script_names` (`spell_id`,`ScriptName`) VALUES
  (932512, 'spell_dru_omen_of_clarity');
