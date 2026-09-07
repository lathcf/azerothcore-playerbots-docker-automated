-- =================================================================================================
-- mod-era-talents — PRIEST FINAL REVIEW (2026-09-06), hand SQL.
--
-- Two shared-table contracts a generated custom-spell row can never carry, because neither table is
-- part of the generator's output: `spell_group` and `spell_linked_spell`. Both are keyed on OUR band
-- ids only, so nothing here touches a stock row.
--
-- Idempotent: every block is DELETE-then-INSERT over exactly the band ids it owns, so AzerothCore
-- re-applying this file after a content edit (it re-runs a CHANGED file by hash, not by name) is a
-- no-op re-write rather than a duplicate insert.
-- =================================================================================================

-- -------------------------------------------------------------------------------------------------
-- I-30a — SPELL_GROUP INHERITANCE for the era clones (BOTH eras).
--
-- `spell_group` membership is by SPELL ID, so a clone inherits none of it. Two clone families need
-- it, and each mirrors its stock donor's memberships EXACTLY — measured on the live DB 2026-09-06,
-- not assumed:
--
--   Power Infusion  920920 (Vanilla) / 948039 (TBC)  -> 1122, 1123
--       `SELECT id FROM spell_group WHERE spell_id = 10060` returns exactly {1122, 1123}:
--       1122 "Temporary Haste Buffs" (with Bloodlust 2825 / Heroism 32182) and 1123 "Power Infusion
--       and Arcane Power" (with Arcane Power 12042 and the mage era clones 932760 / 948760, written
--       by 2026_09_06_00_era_talent_final_review_mage.sql). Both era PIs are the era's stand-in for
--       10060, so both must share its exclusivity — otherwise an era priest could stack PI with
--       Bloodlust/Heroism or with a mage's Arcane Power, which stock forbids.
--       NB the VANILLA clone 920920 is a spell-damage/healing buff, not a haste buff, so 1122 is
--       arguably generous for it — it is written anyway because the group is what the SERVER uses to
--       decide "these two are the same cooldown", and the Vanilla clone occupies exactly stock PI's
--       slot in that decision for a Vanilla-era priest.
--
--   Inspiration r1  920987 (Vanilla) / 948057 (TBC)  -> 1095
--       `SELECT * FROM spell_group WHERE spell_id IN (14893, 15357, 15359)` returns ONE row —
--       (1095, 14893) — and group 1095 holds exactly {14893, 16177} (Inspiration r1 + the shaman
--       Ancestral Fortitude r1). Stock therefore groups the RANK-1 ids only; ranks 2/3 (15357/15359)
--       are in no group at all. The clones mirror that per rank rather than "improving" on it, so
--       920988/920989 and 948058/948059 are deliberately absent below.
--
-- Column list mirrors `SELECT * FROM spell_group WHERE spell_id = 10060` — the table is
-- (id, spell_id) with both columns in the primary key.
-- -------------------------------------------------------------------------------------------------
DELETE FROM `spell_group` WHERE `spell_id` IN (920920, 948039, 920987, 948057);
INSERT INTO `spell_group` (`id`, `spell_id`) VALUES
-- Power Infusion clones
(1122, 920920), (1123, 920920),
(1122, 948039), (1123, 948039),
-- Inspiration rank-1 clones
(1095, 920987),
(1095, 948057);

-- -------------------------------------------------------------------------------------------------
-- I-30b — PAIN SUPPRESSION THREAT LINK for the TBC clone 948040.
--
-- Stock Pain Suppression's "reduces a friendly target's threat by 5%" has no backing spell EFFECT on
-- either side (`_ref` accepted_gaps id 6); the core delivers it through a `spell_linked_spell` row
-- instead — measured live 2026-09-06: (33206, 44416, 2, 'Pain Suppression (threat)'), where type 2
-- is SPELL_LINK_AURA (`SpellMgr.h:101`, "+: aura") — the linked spell fires when the trigger's aura
-- is APPLIED. 44416 is the -5% threat spell (Effect_1 = 125 SPELL_EFFECT_REDUCE_THREAT_PERCENT,
-- basePoints -6 / ds1 = -5%, ImplicitTargetA 22, instant). That row is keyed on the STOCK id, so the
-- band clone 948040 — which is what
-- a TBC-era priest actually casts — silently lost the threat drop, exactly the same class of miss as
-- SpellInfoCorrections-by-id.
--
-- 948040 is confirmed to be the TBC Pain Suppression clone (`era-data/tbc/priest.yaml`: `template:
-- 33206`, node 20521's grant), and its two effects are 33206's own verbatim (aura 87 -40% damage
-- taken misc 127; aura 235 +65 dispel resist) — the only authored deltas are `cooldown: 120000`
-- (TBC 2 min) and `attributesEx5: 0` (drop live-only ALLOW_WHILE_STUNNED), neither of which touches
-- the threat contract. Column list mirrors the live 33206 row verbatim.
--
-- The VANILLA era has no Pain Suppression (a TBC-added spell), so there is no second row here.
-- -------------------------------------------------------------------------------------------------
DELETE FROM `spell_linked_spell` WHERE `spell_trigger` = 948040;
INSERT INTO `spell_linked_spell` (`spell_trigger`, `spell_effect`, `type`, `comment`) VALUES
(948040, 44416, 2, 'Era TBC Pain Suppression (threat)');
