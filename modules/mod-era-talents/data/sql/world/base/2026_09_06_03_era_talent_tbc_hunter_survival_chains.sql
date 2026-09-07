-- TBC Hunter, Phase 7 Task 7 (Survival tab). Wyvern Sting sleep ranks 2-4 and Counterattack ranks
-- 2-4 for a TBC-era hunter, plus the three `spell_ranks` chains the Survival clones need.
-- HAND-WRITTEN (not generated) — mirrors the TBC rogue Mutilate chain (2026_09_04_02) and the TBC
-- priest chains (2026_09_05_02/03/04). Idempotent: DELETE-before-INSERT on every table.
--
-- This server loads trainers from the modern `trainer`/`trainer_spell` tables (ObjectMgr.cpp reads
-- "SELECT ... FROM trainer_spell"), NOT the legacy `npc_trainer` table. The MAIN hunter trainer is
-- `trainer.Id`=7 (Requirement=3 = CLASS_HUNTER) — the same id the stock Wyvern Sting / Counterattack
-- rows and this module's Vanilla + TBC Scorpid Sting rows use. `trainer.Id`=8 is the five-spell
-- starter trainer (levels 2-6: 1494/1978/13163/1130/3044) and is untouched.
-- `.reload trainer` (or a restart) applies it live.
--
-- WHY THE CLONES EXIST (era-data/tbc/hunter.yaml helpers 948330-948341, nodes 20660 / 20656):
--   * Wyvern Sting — TBC's sleep is 12 s on a 2-minute category cooldown; live is 30 s on 1 minute.
--     Its wake-DoT runs 12 s at half the tick (same total). All eight ids DIVERGED.
--   * Counterattack — TBC deals 40/70/110/165; live deals 48/84/132/196. All four ranks DIVERGED.
--     Spec Amendment A.10 records that the shipped VANILLA clones 932979/932958/932959 are
--     field-identical to TBC r1-r3 on all fourteen families and DECLINES the reuse (r4 has no Vanilla
--     counterpart, the strip tables are era-keyed, and a mixed-era chain would be unique here).
--
-- This file adds ONLY the six clone trainer rows and three `spell_ranks` chains. It deletes nothing.

-- =====================================================================================================
-- (a) THE STOCK TRAINER ROWS ARE LEFT COMPLETELY ALONE — and here is the orphaned-higher-rank check
--     that says they must be (docs/era-talents-framework.md "Deleting a stock trainer row: the
--     orphaned-higher-rank check (MANDATORY)"), plus the intent test that says no deletion is even
--     warranted. See also the standing lesson `deleting-a-stock-trainer-row-orphans-higher-ranks`.
--
-- The stock TrainerId-7 Wyvern Sting chain is:
--     24132 ReqLevel 50 ReqAbility1 19386  MoneyCost 1800
--     24133 ReqLevel 60 ReqAbility1 24132  MoneyCost 2500
--     27068 ReqLevel 70 ReqAbility1 24133  MoneyCost 5000
--     49011 ReqLevel 75 ReqAbility1 27068  MoneyCost 100000   <-- WotLK rank 5
--     49012 ReqLevel 80 ReqAbility1 49011  MoneyCost 100000   <-- WotLK rank 6
--
-- The stock TrainerId-7 Counterattack chain is:
--     20909 ReqLevel 42 ReqAbility1 19306  MoneyCost 1200
--     20910 ReqLevel 54 ReqAbility1 20909  MoneyCost 2100
--     27067 ReqLevel 66 ReqAbility1 20910  MoneyCost 2500
--     48998 ReqLevel 72 ReqAbility1 27067  MoneyCost 15000    <-- WotLK rank 5
--     48999 ReqLevel 78 ReqAbility1 48998  MoneyCost 15000    <-- WotLK rank 6
--
--   1. ORPHANED-HIGHER-RANK CHECK: deleting 24132/24133/27068 would strand 49011 (ReqAbility1 27068)
--      and 49012 (ReqAbility1 49011); deleting 20909/20910/27067 would strand 48998 (ReqAbility1
--      27067) and 48999 (ReqAbility1 48998). There is NO compensating grant for any of those four ids
--      anywhere in this module, so a deletion would leave every NATIVE WotLK hunter — who never
--      touches the era system — permanently stuck at rank 1 of both abilities. That is exactly the
--      regression the first revision of the druid Mangle migration shipped and had to reverse.
--   2. THE DELETION WOULD ALSO BE POINTLESS: both stock chains ALREADY self-gate against a TBC-era
--      hunter. Each roots at rank 1 (`24132.ReqAbility1 = 19386`, `20909.ReqAbility1 = 19306`), and
--      those rank-1 ids are TALENT-granted, never trainer-taught — node 20660 grants clone 948330 and
--      node 20656 grants clone 948338 instead, while EraTalents.cpp's readiness-gated
--      `kEraHunTbcStockSwaps` strips a lingering 19386/19306 (and its stock higher ranks) from a TBC
--      hunter. Verified there is no other path to stock 19386/19306 for such a character: neither has
--      a `trainer_spell` row of its own (rank 1 is talent-granted in both eras), neither is taught by
--      an Effect-36 learn-wrapper on this trainer (the `trainer-rows-can-be-learn-wrappers` trap;
--      era_audit's wrapper sweep covers it), and their SkillLineAbility rows carry AcquireMethod 0 so
--      `learnSkillRewardedSpells` never auto-teaches them. A TBC hunter therefore cannot satisfy
--      ReqAbility1 and cannot see or buy the stock ranks.
--   3. TRAINER-GATING INTENT TEST (the Devouring Plague rule): a stock `trainer_spell` row is only a
--      "hole" if the module gates that spell somewhere and a missed set leaks it.
--      mod-individual-progression gates neither ability, and the WotLK-only ranks sit at ReqLevel
--      72-80 — unreachable at the TBC level cap of 70 (_ref baseline_leaks records ZERO leaks in this
--      tree). So there is nothing to close.
-- Net: no DELETE, no UPDATE, no re-gate on any stock row. Do not "tidy" this by adding one.

-- =====================================================================================================
-- (b) Wyvern Sting sleep ranks 2-4 and Counterattack ranks 2-4, trainer-taught, era-gated by a
--     require-previous-rank chain rooted at the TALENT-granted rank 1 (948330 / 948338).
--
-- Era gate: rank 1 of each chain exists ONLY as the TBC era-talent clone. A Vanilla-era hunter holds
-- the Vanilla clones (932965 / 932979) and a WotLK hunter holds the stock ids, so neither ever holds
-- 948330 or 948338 — requiring it via ReqAbility1 makes each ladder reachable ONLY by a TBC-era hunter
-- who spent the talent. This is the talent-anchor flavour of the trainer gate documented in
-- docs/era-talents-framework.md ("Era-gating CUSTOM spell ranks that REPLACE a stock WotLK version",
-- anchor flavour 1) — no marker spell is needed, exactly as for the TBC rogue Mutilate chain and the
-- Vanilla hunter Counterattack chain (2026_08_16_22).
--
-- ReqLevel and MoneyCost are copied VERBATIM from the stock rows above — the AUTHENTIC costs, never
-- ReqLevel x 1000 (commits 1286f34 / d1263c5). Those levels are independently corroborated three ways:
-- the wago 2.5.4 SpellLevels for 24132/24133/27068 and 20909/20910/27067, the cached Wowhead TBC
-- tooltips, and the live stock trainer rows themselves (_ref trainer_cost_curve.per_clone_row_costs).
-- NOTE the costs sit on the trainer's TALENT-CHAIN sub-curve, which is far cheaper than the baseline
-- curve at the same level (at level 42 a baseline hunter rank costs 24000-26000 while Counterattack
-- rank 2 costs 1200) — read the sub-curve, not the headline curve.
--
-- The Wyvern wake-DoT clones 948334-948337 get NO trainer rows: they are never learned, only cast by
-- the `era_wyvern_sting` module script when the matching sleep is removed.
DELETE FROM `trainer_spell` WHERE `TrainerId`=7 AND `SpellId` IN (948331,948332,948333,948339,948340,948341);
INSERT INTO `trainer_spell`
  (`TrainerId`,`SpellId`,`MoneyCost`,`ReqSkillLine`,`ReqSkillRank`,`ReqAbility1`,`ReqAbility2`,`ReqAbility3`,`ReqLevel`,`VerifiedBuild`) VALUES
  (7, 948331, 1800, 0, 0, 948330, 0, 0, 50, 0),   -- Wyvern Sting Rank 2 (155 mana, 420 DoT): req talent-granted R1 948330
  (7, 948332, 2500, 0, 0, 948331, 0, 0, 60, 0),   -- Wyvern Sting Rank 3 (205 mana, 600 DoT): req R2 948331
  (7, 948333, 5000, 0, 0, 948332, 0, 0, 70, 0),   -- Wyvern Sting Rank 4 (255 mana, 942 DoT): req R3 948332
  (7, 948339, 1200, 0, 0, 948338, 0, 0, 42, 0),   -- Counterattack Rank 2 (70 damage): req talent-granted R1 948338
  (7, 948340, 2100, 0, 0, 948339, 0, 0, 54, 0),   -- Counterattack Rank 3 (110 damage): req R2 948339
  (7, 948341, 2500, 0, 0, 948340, 0, 0, 66, 0);   -- Counterattack Rank 4 (165 damage): req R3 948340

-- =====================================================================================================
-- (c) The three `spell_ranks` chains. Two of them do the usual four jobs (spellbook collapse, respec
--     cascade, supersede-for-rank-walks, trainer "next rank"); the DoT chain exists for one reason only.
--
--   * 948330 -> 948331 -> 948332 -> 948333 (Wyvern Sting SLEEP) and
--     948338 -> 948339 -> 948340 -> 948341 (Counterattack):
--       - the client spellbook collapses the four same-name / same-skill-line (51 Survival) ranks to
--         the highest known instead of showing four separate buttons;
--       - `Player::removeSpell` walks GetNextSpellInChain, so Reset()'s removeSpell(948330 / 948338)
--         on respec CASCADES and strips every trained rank — the same respec-clean property the TBC
--         rogue Mutilate (947773-776), TBC warrior Bloodthirst (947512-517) and Vanilla Hemorrhage
--         (932985-987) chains rely on. Both chains are rooted at a NODE GRANT, not at a trainer-taught
--         id, so the lesson-21 arm-2b problem (a chain nothing strips on respec) does not arise here.
--       - a higher rank cleanly supersedes the lower for the bot AI's rank walks and the client's
--         "next rank available" logic.
--   * 948334 -> 948335 -> 948336 -> 948337 (Wyvern Sting wake-DoT) is NEVER learned and NEVER trained,
--     so none of the above applies. It exists so `SpellInfo::IsRankOf` sees the four DoTs as one rank
--     chain and `Aura::CanStackWith` REPLACES rather than stacks when a hunter re-applies Wyvern Sting
--     with a different rank on the same target. Exactly the TBC priest Misery-debuff chain
--     (948073-948077) precedent: a chain whose only job is cross-rank non-stacking.
--
-- The core's SpellMgr::LoadSpellRanks requires every id in a chain to have a spell_dbc record, which
-- these custom era rows do (2026_09_06_01_era_talent_tbc_hunter_custom_spells.sql, generated).
-- `spell_ranks.spell_id` is UNIQUE, so delete by first_spell_id before re-inserting.
DELETE FROM `spell_ranks` WHERE `first_spell_id` IN (948330,948334,948338);
INSERT INTO `spell_ranks` (`first_spell_id`,`spell_id`,`rank`) VALUES
  (948330, 948330, 1),
  (948330, 948331, 2),
  (948330, 948332, 3),
  (948330, 948333, 4),
  (948334, 948334, 1),
  (948334, 948335, 2),
  (948334, 948336, 3),
  (948334, 948337, 4),
  (948338, 948338, 1),
  (948338, 948339, 2),
  (948338, 948340, 3),
  (948338, 948341, 4);
