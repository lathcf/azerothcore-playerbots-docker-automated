-- Baseline-leak gate (2026-08-18, Paladin Holy Task 7): Consecration (26573) is a talent-only spell
-- in Vanilla (Holy node 18606) but became a TRAINER-baseline spell in WotLK. Its stock rank-1
-- `trainer_spell` row carries ReqAbility1=0 (ungated) at ReqLevel 20, so a Vanilla-era paladin could
-- TRAIN Consecration from the class trainer WITHOUT ever spending the era talent that grants it — the
-- same baseline leak Shield Slam / Ice Block / Divine Spirit have (2026_08_18_21). A reconcile strip
-- alone can't fix this: the trainer still OFFERS the spell (trainer_spell is global, not era-conditional).
--
-- Fix = the proven Holy Nova / Shield Slam pattern: delete the rank-1 trainer rows so the trainer no
-- longer offers Consecration to ANYONE. The higher ranks already chain ReqAbility1 on the previous rank
-- (20116->26573, 20922->20116, 20923->20922, 20924->20923, 27173->20924, 48818->27173, 48819->48818 —
-- verified in trainer_spell), so removing rank 1 gates the whole chain behind the talent. WotLK-stage
-- paladins get rank 1 back via the module reconcile (ReconcileBaselineSpells, kBaselineSpellGates row
-- CLASS_PALADIN/18606/grantHigherEra=true) at level 20, then train the higher ranks off it — exactly how
-- Holy Nova is handled. A Vanilla paladin gets rank 1 ONLY from the era talent grant (node 18606). Idempotent.
--
-- This server loads trainers from the modern `trainer`/`trainer_spell` tables (ObjectMgr.cpp), NOT legacy
-- `npc_trainer`; the delete is by SpellId (across all trainer sets — 26573 appears under several paladin
-- trainer Ids 3/4/5/...), so no trainer can offer rank 1 either. `.reload trainer` (or a restart) applies it live.

DELETE FROM `trainer_spell` WHERE `SpellId`=26573;   -- Consecration rank 1 (paladin trainers, L20, ungated)

-- Keep the legacy `npc_trainer` table consistent (dead weight on this server, but mirror the delete as
-- the Holy Nova / Shield Slam gates do). Harmless if the core never reads it.
DELETE FROM `npc_trainer` WHERE `SpellID`=26573;

-- Baseline-leak gate (2026-08-18, Paladin Protection Task 8): Blessing of Kings (20217) is a talent-only
-- spell in Vanilla (Protection node 18620) but became a TRAINER-baseline spell in WotLK. Its stock
-- `trainer_spell` rows (TrainerIds 3/4/5) carry ReqAbility1=0 (ungated) at ReqLevel 20, so a Vanilla-era
-- paladin could TRAIN it without spending the era talent that grants it — the same leak as Consecration
-- above. BoK is SINGLE-RANK (no higher-rank chain), so deleting the row fully gates it. WotLK-stage
-- paladins get it back via the module reconcile (kBaselineSpellGates row CLASS_PALADIN/18620/
-- grantHigherEra=true) at level 20; a Vanilla paladin gets it ONLY from node 18620's grant. Idempotent.
DELETE FROM `trainer_spell` WHERE `SpellId`=20217;   -- Blessing of Kings (paladin trainers, L20, ungated, single rank)
DELETE FROM `npc_trainer`   WHERE `SpellID`=20217;

-- Baseline-leak gate (2026-08-18, Paladin Protection): Greater Blessing of Kings (25898) is a SEPARATE
-- stock chain from BoK 20217 (aura 137 = +10% all stats, SpellLevel 60). Its stock `trainer_spell` rows
-- (paladin TrainerIds 3/4/5) carry ReqAbility1=0 (ungated) at ReqLevel 60, so a Vanilla-era paladin could
-- TRAIN it without the BoK talent — the same leak as 20217, which the `_32` gate above missed. In Vanilla,
-- Greater Blessing of Kings did NOT exist as a trainable baseline (it was the reagent/talent-gated raid
-- version), so a Vanilla paladin must not train it. Delete the ungated rows so no trainer offers it; WotLK-
-- stage paladins get it back via the module reconcile (its own kBaselineSpellGates row CLASS_PALADIN/18620/
-- {25898}/minLevel 60/grantHigherEra=true) at level 60. Single-rank (no higher-rank chain). Idempotent.
DELETE FROM `trainer_spell` WHERE `SpellId`=25898;   -- Greater Blessing of Kings (paladin trainers, L60, ungated)
DELETE FROM `npc_trainer`   WHERE `SpellID`=25898;
