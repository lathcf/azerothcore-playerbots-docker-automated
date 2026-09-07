-- TBC Rogue, Phase 5 Task 6b (Assassination tab). Mutilate ranks 2-4 for a TBC-era rogue.
-- HAND-WRITTEN (not generated) — mirrors the TBC druid Mangle chain (2026_08_31_03) and the Vanilla
-- rogue Hemorrhage chain (2026_08_17_12). Idempotent: DELETE-before-INSERT on every table.
--
-- This server loads trainers from the modern `trainer`/`trainer_spell` tables (ObjectMgr.cpp reads
-- "SELECT ... FROM trainer_spell"), NOT the legacy `npc_trainer` table. The MAIN rogue trainer is
-- `trainer.Id`=9 (Requirement=4 = CLASS_ROGUE, 146 spells) — the same id the stock Mutilate rows
-- and the Vanilla Hemorrhage clone chain use. `trainer.Id`=10 is the 5-spell starter and is unused.
-- `.reload trainer` (or a restart) applies it live.
--
-- WHY THE CLONES EXIST (era-data/tbc/rogue.yaml helpers 947773-947776, node 20420, spec Amendment
-- A.14 — user-locked 2026-09-03, superseding A.5's grant-stock disposition):
-- the fourteen-family DBC sweep was RIGHT that TBC's whole 12-spell Mutilate chain is data-identical
-- to 3.3.5a's apart from behaviour-equivalent attribute re-encodings. It could not see the two
-- properties that actually moved, because neither is spell data:
--   * "+50% damage against Poisoned targets" is HARDCODED in
--     src/server/game/Spells/SpellEffects.cpp:3454-3475 (SPELLFAMILY_ROGUE arm of
--     Spell::EffectWeaponDmg), keyed on `SpellFamilyFlags[1] & 0x6` — the hidden hand-halves'
--     identity bits — and applying `AddPct(totalDamagePercentMod, 20.0f)`: WotLK's 3.0.2 retune.
--   * "Must be behind the target" is the CU attribute SPELL_ATTR0_CU_REQ_CASTER_BEHIND_TARGET,
--     handed out by a hardcoded by-id switch in SpellMgr.cpp:3590 that never lists Mutilate.
-- Both are restored by module scripts (`era_rog_mutilate_poison` on the eight STOCK hidden halves,
-- `era_rog_mutilate_behind` on the four clones); the CLONES exist so the panel tooltip AND the
-- spellbook entry read TBC's text instead of WotLK's "+20%, no positional line" (framework lesson
-- 17d: a grant node's in-client tooltip IS the granted spell's client-DBC Description).
--
-- Only the FOUR VISIBLE casts are cloned (1329 -> 947773 @L40 node-granted, 34411 -> 947774 @L50,
-- 34412 -> 947775 @L60, 34413 -> 947776 @L70). The EIGHT hidden hand-halves (5374/27576 and
-- 34414-34419) are NOT cloned: each clone triggers the stock halves exactly as stock does, they are
-- invisible to players, and the +%-vs-poisoned core branch keys on THEIR family bits — which is
-- precisely the hook the era script attaches to.
--
-- This file adds ONLY the three clone trainer rows and one spell_ranks chain. It deletes nothing.

-- =====================================================================================================
-- (a) THE STOCK MUTILATE TRAINER ROWS ARE LEFT COMPLETELY ALONE — and here is the orphaned-higher-rank
--     check that says they must be (docs/era-talents-framework.md "Deleting a stock trainer row: the
--     orphaned-higher-rank check (MANDATORY)"), plus the intent test that says no deletion is even
--     warranted.
--
-- The stock TrainerId-9 chain is:
--     34411 ReqLevel 50 ReqAbility1 1329   MoneyCost 5500
--     34412 ReqLevel 60 ReqAbility1 34411  MoneyCost 6500
--     34413 ReqLevel 70 ReqAbility1 34412  MoneyCost 7500
--     48663 ReqLevel 75 ReqAbility1 34413  MoneyCost 15000   <-- WotLK rank 5
--     48666 ReqLevel 80 ReqAbility1 48663  MoneyCost 15000   <-- WotLK rank 6
--
--   1. ORPHANED-HIGHER-RANK CHECK: deleting 34411/34412/34413 would strand the WotLK ranks. 48663's
--      ReqAbility1 is 34413 and 48666's is 48663, and there is NO compensating grant for either id
--      anywhere in this module — so a deletion would leave every NATIVE WotLK rogue (who never
--      touches the era system) stuck at Mutilate rank 1, ranks 5 and 6 included. This is exactly the
--      regression the first revision of the druid Mangle migration shipped and had to reverse
--      (2026_08_31_03 section (a)), and the standing lesson
--      `deleting-a-stock-trainer-row-orphans-higher-ranks`.
--   2. THE DELETION WOULD ALSO BE POINTLESS: the stock chain ALREADY self-gates against a TBC-era
--      rogue. Its root requirement is stock rank 1 (1329), which node 20420 no longer grants — it
--      grants clone 947773 — and EraTalents.cpp's readiness-gated `kEraRogTbcStockSwaps` strips a
--      lingering 1329 from a TBC rogue. Verified there is no other path to stock 1329 for such a
--      character: it has NO `trainer_spell` row of its own (r1 is talent-granted in both eras — see
--      _ref capstone_chains.mutilate.live_trainer_rows_note), it is not taught by any Effect-36
--      learn-wrapper on this trainer (the `trainer-rows-can-be-learn-wrappers` trap; era_audit's
--      wrapper sweep covers it), and its SkillLineAbility (row 14895, SkillLine 253) has
--      AcquireMethod 0, so `learnSkillRewardedSpells` never auto-teaches it. A TBC rogue therefore
--      cannot satisfy ReqAbility1 and cannot see or buy the stock ranks.
--   3. TRAINER-GATING INTENT TEST (the Devouring Plague rule): a stock `trainer_spell` row is only a
--      "hole" if the module gates that spell somewhere and a missed set leaks it. Mutilate is gated
--      NOWHERE by mod-individual-progression, and the WotLK-only ranks sit at ReqLevel 75/80 —
--      unreachable at the TBC level cap of 70 (_ref baseline_leaks: ZERO leaks in this tree). So
--      there is nothing to close.
-- Net: no DELETE, no UPDATE, no re-gate on any stock row. Do not "tidy" this by adding one.

-- =====================================================================================================
-- (b) Mutilate clone ranks 2-4, trainer-taught, era-gated by a require-previous-rank chain rooted at
--     the TALENT-granted rank 1 (947773, TBC node 20420).
--
-- Era gate: rank 1 of this chain exists ONLY as the TBC era-talent clone. A Vanilla-era rogue has no
-- Mutilate at all (the talent does not exist in that tree) and a WotLK rogue holds stock 1329, so
-- neither ever holds 947773 — requiring it via ReqAbility1 makes the r2->r3->r4 ladder reachable ONLY
-- by a TBC-era rogue who spent the talent. This is the talent-anchor flavour of the trainer gate
-- documented in docs/era-talents-framework.md ("Era-gating CUSTOM spell ranks that REPLACE a stock
-- WotLK version", anchor flavour 1) — no marker spell is needed, exactly as for the TBC druid Mangle
-- and Nature's Grasp chains and the Vanilla rogue Hemorrhage chain.
--
-- ReqLevel 50/60/70 and MoneyCost 5500/6500/7500 are copied VERBATIM from the stock rows above. Those
-- levels are independently corroborated three ways: the wago 2.5.4 SpellLevels for 34411/34412/34413,
-- the cached Wowhead TBC tooltips ("Requires level 50/60/70"), and the live trainer rows themselves
-- (_ref capstone_chains.mutilate).
DELETE FROM `trainer_spell` WHERE `TrainerId`=9 AND `SpellId` IN (947774,947775,947776);
INSERT INTO `trainer_spell`
  (`TrainerId`,`SpellId`,`MoneyCost`,`ReqSkillLine`,`ReqSkillRank`,`ReqAbility1`,`ReqAbility2`,`ReqAbility3`,`ReqLevel`,`VerifiedBuild`) VALUES
  (9, 947774, 5500, 0, 0, 947773, 0, 0, 50, 0),   -- Mutilate Rank 2 (+63/hand): req talent-granted R1 947773
  (9, 947775, 6500, 0, 0, 947774, 0, 0, 60, 0),   -- Mutilate Rank 3 (+88/hand): req R2 947774
  (9, 947776, 7500, 0, 0, 947775, 0, 0, 70, 0);   -- Mutilate Rank 4 (+101/hand): req R3 947775

-- =====================================================================================================
-- (c) spell_ranks chain 947773 -> 947774 -> 947775 -> 947776. Three jobs, all load-bearing here:
--   * the client spellbook collapses the four same-name / same-skill-line (253 Assassination) ranks
--     to the highest known, instead of showing four separate Mutilate buttons;
--   * `Player::removeSpell` walks GetNextSpellInChain, so Reset()'s removeSpell(947773) on respec
--     CASCADES and strips every trained rank — the same respec-clean property the Hemorrhage
--     (932985-987), Bloodthirst (947512-517) and Mangle (946258-260) chains rely on;
--   * a higher rank cleanly SUPERSEDES the lower for the AI's rank walks and for the client's
--     "next rank available" logic.
-- first_spell_id is the talent-granted rank 1. 947773 is a plain node grant (era_talent_rank), NOT a
-- ReconcileBaselineSpells-managed row, so a rank chain here cannot fight the reconcile — identical to
-- Hemorrhage 932985 / TBC Bloodthirst 947512 / TBC Nature's Grasp 946270. The core's
-- SpellMgr::LoadSpellRanks requires every id in the chain to have a spell_dbc record, which these
-- custom era rows do (2026_09_04_01_era_talent_tbc_rogue_custom_spells.sql, generated).
-- `spell_ranks.spell_id` is UNIQUE, so delete by first_spell_id before re-inserting.
DELETE FROM `spell_ranks` WHERE `first_spell_id`=947773;
INSERT INTO `spell_ranks` (`first_spell_id`,`spell_id`,`rank`) VALUES
  (947773, 947773, 1),
  (947773, 947774, 2),
  (947773, 947775, 3),
  (947773, 947776, 4);
