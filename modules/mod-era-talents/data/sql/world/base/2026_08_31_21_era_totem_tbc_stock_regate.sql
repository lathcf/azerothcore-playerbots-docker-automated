-- TBC stock-totem trainer re-gate (Plan 3b Task 5, 2026-09-01). Two jobs, both measured against
-- the live trainer_spell rows before writing (base dump + the Vanilla-phase re-gate file
-- 2026_08_23_70, which this file deliberately loads AFTER and partially overrides):
--
-- (a) SEARING r2-r6 are UN-marker-gated. The Vanilla phase re-gated 6363/6364/6365/10437/10438
--     behind 932417 ("Era: WotLK Totems") + prev-rank -- correct then, but TBC's disposition is
--     "leave stock alone" (the first grant-stock family; Task 2 batch 2), and a TBC shaman holds
--     947440, not 932417, so those rows would lock TBC out of its own totem. The fix drops the
--     marker and keeps ONLY the prev-rank chain, which is era-correct three ways:
--       * VANILLA stays excluded byte-identically -- the chain roots at stock 3599, which
--         kStockShaSwap strips from every Vanilla shaman's spellbook (EraTalents.cpp:1406), so a
--         Vanilla shaman can never satisfy ReqAbility1=3599. Before: blocked by missing 932417;
--         after: blocked by missing 3599. Same outcome, no behavior change.
--       * WotLK reachability is unchanged -- any WotLK shaman who knows 3599 (auto-learned) also
--         held 932417, so dropping the marker widens nothing.
--       * TBC becomes able to train the chain, which is the point. r1 3599 is auto-learned; the
--         retuned r7 is the CUSTOM 947090 in the companion trainer file, gated 947440 + 10438.
--     MoneyCost/ReqLevel preserved from the Vanilla file's rows.
DELETE FROM `trainer_spell` WHERE `TrainerId`=14 AND `SpellId` IN (6363,6364,6365,10437,10438);
INSERT INTO `trainer_spell`
  (`TrainerId`,`SpellId`,`MoneyCost`,`ReqSkillLine`,`ReqSkillRank`,`ReqAbility1`,`ReqAbility2`,`ReqAbility3`,`ReqLevel`,`VerifiedBuild`) VALUES
  (14, 6363,   2200, 0, 0, 3599,  0, 0, 20, 0),
  (14, 6364,   7000, 0, 0, 6363,  0, 0, 30, 0),
  (14, 6365,  12000, 0, 0, 6364,  0, 0, 40, 0),
  (14, 10437, 24000, 0, 0, 6365,  0, 0, 50, 0),
  (14, 10438, 34000, 0, 0, 10437, 0, 0, 60, 0);

-- (b) The 61-70 STOCK ranks (the ref's suppression `the_25xxx_ranks_are_ALREADY_reachable` list)
--     are re-gated behind the WotLK marker 932417, preserving each row's original MoneyCost/
--     ReqLevel and its prev-rank prerequisite as ReqAbility2 -- exactly how the Vanilla file
--     re-gated the sub-61 stock roots. For most families this is defense-in-depth (their chain
--     roots are already 932417-gated, so a TBC shaman can't climb to them anyway); for **25533
--     Searing r7 it is LOAD-BEARING**, because (a) above just made its chain root TBC-reachable.
--     A WotLK shaman is unaffected: they hold 932417 and the prev rank both.
--
--     25361 (Strength of Earth r5) is DELIBERATELY SKIPPED: mod-individual-progression's
--     class_trainers.sql DELETE+INSERTs that exact row (ReqLevel 60 -> 61) and loads after this
--     module, so a row here would be silently clobbered. It stays chain-gated through 10442,
--     which the Vanilla file already gates on 932417, so a TBC shaman cannot reach it. 25528
--     below keeps 25361 as its ReqAbility2, preserving the WotLK in-order chain across the gap.
--
--     25359 / 25577 / 25585 / 25587 (Grace of Air r3, Windwall r4, Windfury r4/r5) have NO
--     trainer_spell row on this core at all (absent-from-live families / the Windfury "nothing to
--     re-gate" case the Vanilla file documents) -- nothing to do.
--
--     57720/57721 (Totem of Wrath r2/r3) are NOT re-gated, per the settled disposition
--     (_ref suppression.ungated_trainer_rows + node 20219): they self-gate through
--     ReqAbility1=30706, which no TBC shaman has (node 20219 grants clone 947350 instead).
--
--     36936 (Totemic Recall) needs NO row: the base row is (14,36936,...,ReqLevel 30, ungated) --
--     TBC-correct as-is. The ref's "sells it at 61" claim read mod-individual-progression's
--     COMMENTED-OUT re-point as active; corrected at the origin entry.
--
--     6495 (Sentry, ungated for every era) is left alone: gating it on 932417 would take the
--     stock Sentry away from VANILLA shamans, who buy it today on purpose. The TBC-side stock
--     suppression belongs to Task 6's kStockShaSwap TBC arm (reconcile-on-learn strips a bought
--     6495 immediately); recorded as an accepted gap in the ref.
DELETE FROM `trainer_spell` WHERE `TrainerId`=14 AND `SpellId` IN
  (25508,25509,25525,25528,25533,25546,25547,25552,25557,25560,25563,25567,25570,25574);
INSERT INTO `trainer_spell`
  (`TrainerId`,`SpellId`,`MoneyCost`,`ReqSkillLine`,`ReqSkillRank`,`ReqAbility1`,`ReqAbility2`,`ReqAbility3`,`ReqLevel`,`VerifiedBuild`) VALUES
  (14, 25508, 42000, 0, 0, 932417, 10408, 0, 63, 0),
  (14, 25509, 88000, 0, 0, 932417, 25508, 0, 70, 0),
  (14, 25525, 64000, 0, 0, 932417, 10428, 0, 67, 0),
  (14, 25528, 52000, 0, 0, 932417, 25361, 0, 65, 0),
  (14, 25533, 79000, 0, 0, 932417, 10438, 0, 69, 0),
  (14, 25546, 34000, 0, 0, 932417, 11315, 0, 61, 0),
  (14, 25547, 88000, 0, 0, 932417, 25546, 0, 70, 0),
  (14, 25552, 52000, 0, 0, 932417, 10587, 0, 65, 0),
  (14, 25557, 64000, 0, 0, 932417, 16387, 0, 67, 0),
  (14, 25560, 64000, 0, 0, 932417, 10479, 0, 67, 0),
  (14, 25563, 71000, 0, 0, 932417, 10538, 0, 68, 0),
  (14, 25567, 79000, 0, 0, 932417, 10463, 0, 69, 0),
  (14, 25570, 52000, 0, 0, 932417, 10497, 0, 65, 0),
  (14, 25574, 79000, 0, 0, 932417, 10601, 0, 69, 0);
