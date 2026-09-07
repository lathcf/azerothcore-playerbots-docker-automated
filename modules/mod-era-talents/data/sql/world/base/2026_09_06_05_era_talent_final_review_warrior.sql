-- Warrior FINAL REVIEW round R7 (2026-09-06) — the two hand-written (non-generated) halves it needs:
--   (a) the VANILLA Shield Slam clone chain's trainer_spell + spell_ranks rows (item I-7);
--   (b) spell_group memberships the era CLONES do not inherit from the stock spells they replace
--       (item I-30: Enrage; plus the Blood Frenzy debuffs this same round clones, item G-13).
-- Idempotent: every statement is DELETE-before-INSERT, and every DELETE names only ids this module
-- owns (never a whole spell_group id — stock rows in the same group must survive).
--
-- This server loads trainers from the modern `trainer`/`trainer_spell` tables (ObjectMgr.cpp reads
-- `trainer_spell`), NOT the legacy `npc_trainer` table. The MAIN warrior trainer is `trainer.Id`=1
-- (Requirement=1 = CLASS_WARRIOR); `trainer.Id`=2 is a 6-spell starter and is not used.
-- `.reload trainer` (or a restart) applies (a) live; `spell_group` needs a restart.

-- ==================================================================================================
-- (a) Vanilla Shield Slam ranks 2-4 (932842/932843/932844), trainable at the Vanilla levels.
--
-- Node 18552 now grants the era clone 932841 instead of stock 23922 (era-data/vanilla/warrior.yaml
-- helpers 932841-932844: the stock chain ships the WotLK damage 294-308 ... 447-469 where Vanilla is
-- 225-235 ... 342-358). This mirrors the shipped Vanilla Bloodthirst chain
-- (2026_08_18_20_era_talent_warrior_bloodthirst_trainer.sql) and the TBC Shield Slam chain
-- (2026_09_03_03_era_talent_tbc_warrior_prot_chains.sql) exactly.
--
-- NOTHING IS DELETED OR RE-GATED, and that is a VERIFIED PROPERTY, not an oversight. The framework's
-- orphaned-higher-rank check is MANDATORY before any DELETE; the correct resolution here is framework
-- OPTION 3 — "the stock chain is already self-gating, so the DELETE was unnecessary" — because stock
-- 23923.ReqAbility1 = stock 23922, chained up through 30356 and then 47487/47488. Node 18552 grants
-- the CLONE, never stock 23922, so a Vanilla warrior can never satisfy 23923's prerequisite and can
-- never buy the stock ladder; deleting those rows would instead strand native WotLK warriors at
-- Shield Slam rank 1. (Stock rank 1's OWN trainer row was already deleted, globally, by
-- 2026_08_18_21_era_talent_baseline_gate_trainers.sql — that file still owns it, and the
-- kBaselineSpellGates walker still force-grants stock r1 at L40 to a WotLK-stage warrior.)
--
-- ERA GATE = the talent-anchor flavor (framework: "Era-gating CUSTOM spell ranks that REPLACE a stock
-- WotLK version", anchor flavor 1 — no marker spell needed). Rank 1 exists ONLY as a Vanilla
-- era-talent clone (932841). A TBC warrior gets its own clone chain 947541-947546 and a WotLK warrior
-- gets stock 23922, so neither ever holds 932841 and a `ReqAbility1` rooted there makes the ladder
-- reachable ONLY by a Vanilla-era warrior who spent the talent.
--
-- Levels are Vanilla's own SpellLevels for the chain (48 / 54 / 60), identical to the stock rows'
-- ReqLevel; MoneyCost mirrors the stock rows verbatim (40000 / 56000 / 62000) so an era warrior pays
-- exactly what a native one does at that level. Rank 1 is the talent grant at 40 and is not sold.
-- Ranks 5-8 (stock 25258@66 / 30356@70 / 47487@75 / 47488@80) are above the Vanilla level cap and
-- have no era counterpart.
DELETE FROM `trainer_spell` WHERE `TrainerId`=1 AND `SpellId` IN (932842,932843,932844);
INSERT INTO `trainer_spell`
  (`TrainerId`,`SpellId`,`MoneyCost`,`ReqSkillLine`,`ReqSkillRank`,`ReqAbility1`,`ReqAbility2`,`ReqAbility3`,`ReqLevel`,`VerifiedBuild`) VALUES
  (1, 932842, 40000, 0, 0, 932841, 0, 0, 48, 0),   -- Rank 2 (264-276): req talent-granted R1 932841
  (1, 932843, 56000, 0, 0, 932842, 0, 0, 54, 0),   -- Rank 3 (303-317): req R2 932842
  (1, 932844, 62000, 0, 0, 932843, 0, 0, 60, 0);   -- Rank 4 (342-358): req R3 932843

-- spell_ranks chain, rooted at the TALENT-granted rank 1. Three jobs, all load-bearing:
--   1. the client spellbook collapses same-name ranks to the highest known (which is why every rank's
--      `client:` block carries a `rank:` label);
--   2. a higher-rank cast cleanly supersedes the lower rank instead of coexisting as a second button;
--   3. `EraTalents::Reset`'s removeSpell(rank 1) CASCADES via GetNextSpellInChain, so a respec strips
--      the trained ranks too — and so does the CLASS_WARRIOR arm's era-transition strip.
-- The core's LoadSpellRanks requires every id in a chain to have a spell_dbc record; these custom rows
-- do (generated into 2026_08_18_11_era_talent_warrior_custom_spells.sql, which db-import applies
-- before this file — the numeric filename order is the apply order).
-- `spell_id` is UNIQUE, so the chain is deleted by first_spell_id before re-inserting.
DELETE FROM `spell_ranks` WHERE `first_spell_id`=932841;
INSERT INTO `spell_ranks` (`first_spell_id`,`spell_id`,`rank`) VALUES
  (932841, 932841, 1),
  (932841, 932842, 2),
  (932841, 932843, 3),
  (932841, 932844, 4);

-- ==================================================================================================
-- (b) spell_group memberships for era clones.
--
-- A `template:` clone inherits DBC columns; it does NOT inherit rows in the world DB's own tables, so
-- an era clone that REPLACES a grouped stock spell silently leaves its exclusivity group. Both cases
-- below are that bug. Precedent: group 1107 already carries the era Arcane Power clones 932760/948760
-- alongside stock 12042.
--
-- `SpellMgr::LoadSpellGroups` REJECTS any spell_id whose `SpellInfo::GetRank() > 1` ("is not first
-- rank of spell"), and `GetSpellSpellGroupMapBounds` resolves a lookup through `GetFirstSpellInChain`.
-- That is why stock 14201-14204 / 30070 are absent here and still covered (they are ranks 2+ of a
-- `spell_ranks` chain rooted at a listed id) and why EVERY era clone below needs its OWN row: none of
-- them is in a `spell_ranks` chain, so each is its own first rank.

-- (b1) Warrior Enrages, group 1104 (stack_rule 1 = EXCLUSIVE; stock members 12880 + 57514 + 57518).
-- Vanilla Enrage buff clones 932815-932819 (node 18534, +5/10/15/20/25%) and the TBC clones
-- 947556-947560 (node 20328, TBC's own +5/10/15/20/25%) both replace stock 12880's chain for their
-- era, so without these rows an era warrior's Enrage would stack with Wrecking Crew's 57514/57518.
-- NB the round's own task text named only 947556 on the TBC side; all five TBC ranks are listed here
-- for the same reason all five Vanilla ranks are — each clone is its own chain root, so listing only
-- rank 1 would leave ranks 2-5 stacking.
DELETE FROM `spell_group` WHERE `id`=1104 AND `spell_id` IN
  (932815,932816,932817,932818,932819,947556,947557,947558,947559,947560);
INSERT INTO `spell_group` (`id`,`spell_id`) VALUES
  (1104, 932815),   -- Vanilla Enrage r1  (+5%)
  (1104, 932816),   -- Vanilla Enrage r2  (+10%)
  (1104, 932817),   -- Vanilla Enrage r3  (+15%)
  (1104, 932818),   -- Vanilla Enrage r4  (+20%)
  (1104, 932819),   -- Vanilla Enrage r5  (+25%)
  (1104, 947556),   -- TBC Enrage r1      (+5%)
  (1104, 947557),   -- TBC Enrage r2      (+10%)
  (1104, 947558),   -- TBC Enrage r3      (+15%)
  (1104, 947559),   -- TBC Enrage r4      (+20%)
  (1104, 947560);   -- TBC Enrage r5      (+25%)

-- (b2) Blood Frenzy / Savage Combat physical-damage-taken exclusivity, group 1108 (stock members
-- 30069 + rogue Savage Combat 58684). The TBC clones 947568/947569 (final review R7 item G-13) are
-- what node 20318 now triggers, so they must join the group the stock debuffs belong to.
DELETE FROM `spell_group` WHERE `id`=1108 AND `spell_id` IN (947568,947569);
INSERT INTO `spell_group` (`id`,`spell_id`) VALUES
  (1108, 947568),   -- TBC Blood Frenzy debuff r1 (+2% physical damage taken)
  (1108, 947569);   -- TBC Blood Frenzy debuff r2 (+4% physical damage taken)
