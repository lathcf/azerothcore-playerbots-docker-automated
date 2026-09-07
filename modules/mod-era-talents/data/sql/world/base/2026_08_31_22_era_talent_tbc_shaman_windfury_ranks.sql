-- TBC Windfury Weapon rank wiring (Plan 3b Task 7, 2026-09-01). Much smaller than the plan
-- expected, because the measurement shrank the family to GRANT-STOCK r1-r4 + ONE clone at r5:
--
--   * Ranks 1-4 (8232/8235/10486/16362, L30/40/50/60) are LIVE_MATCH on every comparable field,
--     and their stock rows are already correct for every era: 8232 ungated at L30 (1.12 content —
--     a Vanilla shaman legitimately buys it), r2-r4 chained off the previous rank, WotLK's r6-r8
--     level-gated at 71+. Amendment A.6's "one genuine leak" therefore CLOSES as no-leak: with
--     r1-r4 granted stock, the only WotLK-shaped rank reachable under any managed cap was r5 —
--     which this file marker-gates. NOTHING here touches the r1-r4 rows (and nothing is deleted:
--     a DELETE would orphan the higher ranks, the Mangle regression).
--   * Rank 5 25505 (L68) is DIVERGED on cost model (TBC 190 flat vs live 0 + 7% of base), so a
--     TBC shaman trains the clone 947441 instead — gated on marker 947440 AND stock r4 16362, the
--     exact shape of Searing's custom r7 on top of its grant-stock chain. The stock 25505 row is
--     re-gated behind the WotLK marker 932417 with its prev-rank prerequisite preserved as
--     ReqAbility2 (original MoneyCost/ReqLevel kept). A Vanilla shaman reaches neither (L68).
--
-- NO spell_ranks rows: the stock chain rooted at 8232 already owns ranks 1-8, so chaining 947441
-- would double-book rank 5 — the clone stays chainless, the accepted Searing-r7 cosmetic (a second
-- "Windfury Weapon" spellbook entry above the collapsed stock chain).
-- Idempotent: DELETE-before-INSERT (PK is (TrainerId,SpellId)).
DELETE FROM `trainer_spell` WHERE `TrainerId`=14 AND `SpellId` IN (947441, 25505);
INSERT INTO `trainer_spell`
  (`TrainerId`,`SpellId`,`MoneyCost`,`ReqSkillLine`,`ReqSkillRank`,`ReqAbility1`,`ReqAbility2`,`ReqAbility3`,`ReqLevel`,`VerifiedBuild`) VALUES
  (14, 947441, 68000, 0, 0, 947440, 16362, 0, 68, 0),
  (14, 25505,  71000, 0, 0, 932417, 16362, 0, 68, 0);
