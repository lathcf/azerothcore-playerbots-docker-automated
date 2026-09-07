-- Flametongue Totem imbue: restore the enchant combat proc (tooltip-honesty fix round, 2026-09-02).
-- Closes era-data/_ref/tbc/shaman-totems-tbc.yaml accepted_gaps id 7 / open_questions id 7.
--
-- The era effect clones (Vanilla 931148-151, TBC 947130-134) apply the STOCK temp enchants
-- 124/285/543/1683/2637 ("Flametongue Totem 1..5", type 1 COMBAT_SPELL, chance 100), whose combat
-- spell is the 1.12/TBC 8253-series -- ABSENT from this build's Spell.dbc, so the imbue applied and
-- procced NOTHING, plus one Player::CastItemCombatSpell LOG_ERROR per qualifying swing per imbued
-- member. These overlay rows (DBCStores.cpp loads SpellItemEnchantment.dbc with this table exactly
-- like spell_dbc) are byte-copies of the client rows with ONLY EffectArg_1 re-pointed at the band
-- proc DUMMYs 947135-947139, each bound to `era_sha_flametongue_totem_proc` (weapon-speed
-- normalization, bp/100*speed clipped to [bp/77, bp/25], dealt as fire via vehicle 947442).
-- CLIENT rows are untouched -- glow, enchant name and tooltip render from the client's own DBC.
-- SAFE TO OVERRIDE GLOBALLY: a full live-Spell.dbc sweep (2026-09-02) found ZERO stock spells
-- applying any of these five enchant ids -- only the era effect clones reach them, so no
-- non-era character can observe the change.
-- Idempotent: DELETE-before-INSERT.
DELETE FROM `spellitemenchantment_dbc` WHERE `ID` IN (124, 285, 543, 1683, 2637);
INSERT INTO `spellitemenchantment_dbc`
  (`ID`, `Charges`, `Effect_1`, `Effect_2`, `Effect_3`,
   `EffectPointsMin_1`, `EffectPointsMin_2`, `EffectPointsMin_3`,
   `EffectPointsMax_1`, `EffectPointsMax_2`, `EffectPointsMax_3`,
   `EffectArg_1`, `EffectArg_2`, `EffectArg_3`,
   `Name_Lang_enUS`, `Name_Lang_Mask`, `ItemVisual`, `Flags`,
   `Src_ItemID`, `Condition_Id`, `RequiredSkillID`, `RequiredSkillRank`, `MinLevel`) VALUES
  (124,  0, 1, 0, 0, 100, 0, 0, 100, 0, 0, 947135, 0, 0, 'Flametongue Totem 1', 16712190, 32, 7, 0, 0, 0, 0, 0),
  (285,  0, 1, 0, 0, 100, 0, 0, 100, 0, 0, 947136, 0, 0, 'Flametongue Totem 2', 16712190, 32, 7, 0, 0, 0, 0, 0),
  (543,  0, 1, 0, 0, 100, 0, 0, 100, 0, 0, 947137, 0, 0, 'Flametongue Totem 3', 16712190, 32, 7, 0, 0, 0, 0, 0),
  (1683, 0, 1, 0, 0, 100, 0, 0, 100, 0, 0, 947138, 0, 0, 'Flametongue Totem 4', 16712190, 32, 7, 0, 0, 0, 0, 0),
  (2637, 0, 1, 0, 0, 100, 0, 0, 100, 0, 0, 947139, 0, 0, 'Flametongue Totem 5', 16712190, 32, 7, 0, 0, 0, 0, 0);
