-- Sentry Totem trainer rows: complete the three-marker pattern (fix round 2026-09-02).
-- Found in the user's real-client pass: a TBC shaman saw TWO Sentry rows — the era clone 947260
-- (947440-gated) AND the stock 6495 row, which was deliberately left UNGATED "for Vanilla's sake"
-- and was therefore buyable by a TBC shaman, then stripped at the next reconcile (money gone, row
-- still listed). The OR-gate a shared stock row would need (Vanilla 932416 OR WotLK 932417) is
-- inexpressible in trainer_spell (ReqAbility slots AND together), so Sentry now does what every
-- other totem family does: one row per era.
--   * stock 6495  -> re-gated behind 932417 (WotLK-only; original cost/level kept)
--   * clone 931390 -> the new VANILLA Sentry (1.12 values: 80 flat mana, Nature) behind 932416
--   * clone 947260 -> the TBC row, already gated 947440 (2026_08_31_20) — untouched here
-- Reconcile arms strip the other eras' versions (kStockShaSwap/kStockShaSwapTbc now both carry
-- 6495; 931390 joins kEraShaTrained), and 6495 joins kReconcileOnLearn for instant strip-on-stray.
-- Idempotent: UPDATE + DELETE-before-INSERT (PK is (TrainerId, SpellId)).
UPDATE `trainer_spell` SET `ReqAbility1`=932417 WHERE `TrainerId`=14 AND `SpellId`=6495;
DELETE FROM `trainer_spell` WHERE `TrainerId`=14 AND `SpellId`=931390;
INSERT INTO `trainer_spell`
  (`TrainerId`,`SpellId`,`MoneyCost`,`ReqSkillLine`,`ReqSkillRank`,`ReqAbility1`,`ReqAbility2`,`ReqAbility3`,`ReqLevel`,`VerifiedBuild`) VALUES
  (14, 931390, 9000, 0, 0, 932416, 0, 0, 34, 0);
