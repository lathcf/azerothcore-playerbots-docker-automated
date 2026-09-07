-- TBC Shaman, Plan 3a Task 8 (Restoration tab). Earth Shield ranks 2-3 for a TBC-era shaman.
-- HAND-WRITTEN (not generated) — mirrors the sibling TBC druid Mangle / Nature's Grasp chains
-- (2026_08_31_03 / 2026_08_31_02) and the TBC paladin active-rank chains (2026_08_27_02).
-- Idempotent: DELETE-before-INSERT.
--
-- This server loads trainers from the modern `trainer`/`trainer_spell` tables (ObjectMgr reads
-- `trainer_spell`), NOT the legacy `npc_trainer` table. The SHAMAN trainer is `trainer.Id` = 14.
-- `.reload trainer` (or a restart) applies it live.
--
-- WHY THE CLONES EXIST (era-data/tbc/shaman.yaml helpers 946537-946539, node 20260).
-- All three TBC ranks — 974 (L50, talent) / 32593 (L60) / 32594 (L70) — are DIVERGED, and on the
-- SAME two fields (era-data/_ref/tbc/shaman-spells.yaml survival_map):
--     ProcTypeMask  TBC 139944 -> LIVE 172712   (WotLK added bit15 TAKEN_SPELL_MAGIC_DMG_CLASS_POS,
--                                                i.e. let the shield proc off positive magic taken)
--     Cost          TBC flat ManaCost 300/375/450 -> LIVE ManaCost 0 + ManaCostPct 19/19/15
-- Everything else is inherited from the stock templates: the EFFECT_0 DUMMY heal (150/205/270), the
-- EFFECT_1 aura-149 REDUCE_PUSHBACK 30% carrier, ProcCharges 6, DurationIndex 6 (10 min),
-- RangeIndex 5 (40 yd), Category 1195 (the Elemental Shield exclusion group), SpellLevel 50/60/70.
--
-- This file adds ONLY the two clone trainer rows and one spell_ranks chain. It deletes nothing —
-- see section (a).

-- =====================================================================================================
-- (a) THE STOCK EARTH SHIELD TRAINER ROWS ARE LEFT IN PLACE. No DELETE is proposed anywhere in this
--     file, and that is a deliberate result of running the framework's MANDATORY orphaned-higher-rank
--     check ("Deleting a stock trainer row: the orphaned-higher-rank check", docs/era-talents-
--     framework.md) before writing it.
--
-- Measured on the live acore_world 2026-09-01:
--     TrainerId 14, SpellId 32593, ReqAbility1 = 974,   ReqLevel 60
--     TrainerId 14, SpellId 32594, ReqAbility1 = 32593, ReqLevel 70
--     TrainerId 14, SpellId 49283, ReqAbility1 = 32594, ReqLevel 75
--     TrainerId 14, SpellId 49284, ReqAbility1 = 49283, ReqLevel 80
--     SpellId 974 — NO trainer_spell row at all (Earth Shield rank 1 is talent-only in 3.3.5a;
--                   _ref.baseline_leaks `no_trainer_row` records the same for the other five
--                   talent-only shaman ids).
--
-- FRAMEWORK OPTION 3 APPLIES — "the stock chain is already self-gating", the preferred outcome and
-- the one most often missed. A TBC-era shaman is granted the CLONE 946537 and never stock 974, so it
-- can never satisfy 32593's ReqAbility1 and can never see or buy any rank of the stock chain. The
-- DELETE would have been unnecessary, and deleting 32593/32594 would have orphaned 49283/49284 for
-- native WotLK shamans — precisely the TBC-druid Mangle regression that put the check in the
-- framework in the first place. r4/r5 are ReqLevel 75/80, above the TBC cap of 70, so they are
-- unreachable for an era character regardless and need no suppression of their own.

-- =====================================================================================================
-- (b) Earth Shield clone ranks 2-3, trainer-taught, era-gated by a require-previous-rank chain rooted
--     at the TALENT-granted rank 1 (946537, TBC node 20260).
--
-- Era gate: rank 1 of this chain exists ONLY as the TBC era-talent clone. A Vanilla-era shaman (Earth
-- Shield does not exist in that tree at all) and a WotLK shaman (stock 974) never hold 946537, so
-- requiring it via ReqAbility1 makes r2->r3 reachable ONLY by a TBC-era shaman who spent the talent.
-- This is the talent-anchor flavor documented in docs/era-talents-framework.md ("Era-gating CUSTOM
-- spell ranks that REPLACE a stock WotLK version", anchor flavor 1) — no marker spell is needed, and
-- 946537 has a client Spell.dbc row (its `client:` block), which is what the WotLK trainer UI needs
-- to format the "Requires ..." line without the 932939-class nil-format Lua crash.
--
-- ReqLevel and MoneyCost are copied verbatim from the stock rows measured above (60/70; 1700/5000).
DELETE FROM `trainer_spell` WHERE `TrainerId`=14 AND `SpellId` IN (946538,946539);
INSERT INTO `trainer_spell`
  (`TrainerId`,`SpellId`,`MoneyCost`,`ReqSkillLine`,`ReqSkillRank`,`ReqAbility1`,`ReqAbility2`,`ReqAbility3`,`ReqLevel`,`VerifiedBuild`) VALUES
  (14, 946538, 1700, 0, 0, 946537, 0, 0, 60, 0),   -- Earth Shield Rank 2: req talent-granted R1 946537
  (14, 946539, 5000, 0, 0, 946538, 0, 0, 70, 0);   -- Earth Shield Rank 3: req R2 946538

-- =====================================================================================================
-- (c) spell_ranks chain. Makes R1..R3 a real rank chain so the client spellbook COLLAPSES the three
--     same-name ranks to the highest known, a higher-rank cast cleanly overwrites the lower shield,
--     and Reset's removeSpell(946537) CASCADES via GetNextSpellInChain to strip the trained ranks on
--     respec and on the TBC->WotLK era transition (EraTransition::Run calls EraTalents::Reset for the
--     era being left). first_spell_id is the talent-granted rank 1, exactly like the Vanilla
--     932505 / TBC 946270 Nature's Grasp and TBC 946258/946261 Mangle chains.
DELETE FROM `spell_ranks` WHERE `first_spell_id`=946537;
INSERT INTO `spell_ranks` (`first_spell_id`,`spell_id`,`rank`) VALUES
  (946537, 946537, 1),
  (946537, 946538, 2),
  (946537, 946539, 3);
