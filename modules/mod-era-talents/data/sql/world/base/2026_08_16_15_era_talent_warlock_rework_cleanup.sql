-- mod-era-talents: 2026-08-16 warlock rework cleanup (hand-authored, applied once).
--
-- Improved Voidwalker (18222), Improved Firebolt (18239) and Improved Lash of Pain (18240) were
-- reworked from scripted markers to plain owner-side SPELLMOD passives (the stock Demonic Power
-- 18126 mechanism — an owner's spellmods bind to the pet's casts via GetSpellModOwner). The
-- regenerated dataset SQL no longer emits their scriptBindings / helper rows, but the generator's
-- per-id DELETE+INSERT pattern never removes rows a PREVIOUS generation inserted — and a
-- spell_script_names row naming a script that no longer exists in the binary errors on every boot.
-- This migration drops the stale rows.

-- Old per-rank pet-ability bindings (era_ivw_* on Torment/Suffering/Consume Shadows/Sacrifice,
-- era_ilop_cooldown on Lash of Pain). era_ifb_apply was a PetScript — no rows to clean.
DELETE FROM `spell_script_names` WHERE `ScriptName` IN
  ('era_ivw_threat', 'era_ivw_heal', 'era_ivw_absorb', 'era_ilop_cooldown');

-- Demonic Sacrifice moved from stock 18788 to custom clone 932929 (Vanilla client tooltip);
-- drop the old binding so the script only fires on the clone.
DELETE FROM `spell_script_names` WHERE `spell_id`=18788 AND `ScriptName`='era_demonic_sacrifice';

-- Retired custom spells: 932911/932912 = the old pet-side-only Master Demonologist halves (the
-- reshaped 932907-910 now radiate to both sides via SPELL_EFFECT_APPLY_AREA_AURA_PET), 932915 = the
-- old Improved Firebolt haste carrier. The module C++ still strips these ids off live characters.
DELETE FROM `spell_dbc` WHERE `ID` IN (932911, 932912, 932915);
