-- Aimed Shot R2-R6, Trueshot Aura R2-R3, and Counterattack R2-R3: trainable from the hunter
-- trainer, era-gated via a require-previous-rank chain. HAND-WRITTEN (not generated) — mirrors the
-- Siphon Life trainer-gate pattern (2026_08_14_14). Idempotent: DELETE-before-INSERT.
--
-- This server loads trainers from the modern `trainer`/`trainer_spell` tables (ObjectMgr.cpp:
-- "SELECT ... FROM trainer_spell"), NOT the legacy `npc_trainer` table. The MAIN hunter trainer is
-- `trainer.Id`=7 (greeting "Hello, hunter!  Ready for some training?"; 172 spells incl. Multi-Shot,
-- Arcane Shot, Serpent Sting, Concussive Shot); `trainer.Id`=8 is the 5-spell starter and is NOT used.
--
-- Era gate: rank 1 of each ability exists ONLY as a Vanilla era-talent CLONE (Aimed Shot 932952 via
-- node 18321, Trueshot Aura 932961 via 18330, Counterattack 932979 via 18344 — Task 7). A WotLK
-- hunter has the STOCK rank 1 (19434 / 19506 / 19306) instead, never the clone, so requiring the
-- clone (ReqAbility1) makes each R2+ chain reachable ONLY by a Vanilla-era character who took the
-- talent — no extra era filtering needed. Symmetric to the stock chains this server already ships
-- (stock Aimed 20900-20904 ReqAbility1 19434, Counterattack 20909/20910 ReqAbility1 19306), which a
-- Vanilla hunter can never train because they lack the stock rank 1 (baseline-leak sweep, Task 8
-- Step 1). Levels come from the stock rows' SpellLevel (Aimed 28/36/44/52/60, Counterattack 42/54)
-- and the Classic Trueshot ranks (50/60). MoneyCost reuses the stock rows' costs where they exist
-- (Aimed 20900-04, Counterattack 20909/10). Per-rank spell magnitudes are authored in
-- era-data/vanilla/hunter.yaml (the helpers).
--
-- TRUESHOT AURA R2/R3 RE-PRICED 2026-09-05 (TBC Phase 7, spec Amendment A.9): 8000/16000 -> 1800/2500.
-- No stock Trueshot trainer row exists in ANY expansion, so these two prices are interpolated. The
-- original pair came off TrainerId 7's BASELINE curve (L30 = 8000, L38 = 16000) and predates commits
-- 1286f34 / d1263c5, which established that a custom row prices at the AUTHENTIC STOCK cost rather
-- than a level-derived figure. TrainerId 7 is BIMODAL: a TALENT-ROOTED trained rank is far cheaper
-- than a baseline spell at the same level (at level 42 baseline ranks cost 24000-26000 while
-- Counterattack R2 costs 1200), and Trueshot is talent-rooted. The talent-chain sub-curve at these
-- two levels is unambiguous and doubly corroborated: L50 has exactly one stock talent-rooted row
-- (24132 Wyvern Sting R2 @ 1800) and L60 has two that AGREE (20904 Aimed Shot R6 and 24133 Wyvern
-- Sting R3, both @ 2500). Full table: era-data/_ref/tbc/hunter-spells.yaml `trainer_cost_curve`.
-- The TBC Trueshot clone chain (948378-948380, migration 2026_09_06_04) uses the same series
-- 1800/2500/5000 at L50/60/70, so the two eras now price identically.
--
-- The stock WotLK Deterrence 19263 is trainer-taught here at level 60 with no ReqAbility gate; it is
-- stripped for Vanilla-era hunters in EraTalents.cpp ReconcileBaselineSpells (the era Deterrence
-- talent grants clone 932964, never 19263). Left in trainer_spell for WotLK-stage hunters.

DELETE FROM `trainer_spell` WHERE `TrainerId`=7 AND `SpellId` IN
  (932953,932954,932955,932956,932957, 932962,932963, 932958,932959);
INSERT INTO `trainer_spell`
  (`TrainerId`,`SpellId`,`MoneyCost`,`ReqSkillLine`,`ReqSkillRank`,`ReqAbility1`,`ReqAbility2`,`ReqAbility3`,`ReqLevel`,`VerifiedBuild`) VALUES
  -- Aimed Shot R2-R6 (req: previous rank; R2 req talent-granted R1 932952)
  (7, 932953,  400, 0, 0, 932952, 0, 0, 28, 0),
  (7, 932954,  700, 0, 0, 932953, 0, 0, 36, 0),
  (7, 932955, 1300, 0, 0, 932954, 0, 0, 44, 0),
  (7, 932956, 2000, 0, 0, 932955, 0, 0, 52, 0),
  (7, 932957, 2500, 0, 0, 932956, 0, 0, 60, 0),
  -- Trueshot Aura R2-R3 (req: previous rank; R2 req talent-granted R1 932961)
  (7, 932962,  1800, 0, 0, 932961, 0, 0, 50, 0),   -- talent-chain sub-curve L50 (== stock 24132)
  (7, 932963,  2500, 0, 0, 932962, 0, 0, 60, 0),   -- talent-chain sub-curve L60 (== stock 20904/24133)
  -- Counterattack R2-R3 (req: previous rank; R2 req talent-granted R1 932979)
  (7, 932958, 1200, 0, 0, 932979, 0, 0, 42, 0),
  (7, 932959, 2100, 0, 0, 932958, 0, 0, 54, 0);

-- --- spell_ranks chains: make each ability a real rank chain so a higher rank OVERWRITES the lower
-- (Aimed Shot / Counterattack are single-cast damage — the chain lets the client spellbook collapse
-- same-name ranks to the highest known and lets Reset's removeSpell(rank1) CASCADE via
-- GetNextSpellInChain to strip the trained ranks on respec). first_spell_id is the talent-granted
-- rank 1 (932952 / 932961 / 932979). The core's LoadSpellRanks requires every id to have a spell_dbc
-- record, which these custom era rows do. `spell_id` is UNIQUE, so DELETE by first_spell_id before
-- re-inserting (idempotent). NB the rank-1 ids are talent-granted (era_talent_rank), NOT managed by
-- ReconcileBaselineSpells, so a chain here is safe (same as Siphon Life 932902).
DELETE FROM `spell_ranks` WHERE `first_spell_id` IN (932952, 932961, 932979);
INSERT INTO `spell_ranks` (`first_spell_id`,`spell_id`,`rank`) VALUES
  (932952, 932952, 1), (932952, 932953, 2), (932952, 932954, 3),
  (932952, 932955, 4), (932952, 932956, 5), (932952, 932957, 6),
  (932961, 932961, 1), (932961, 932962, 2), (932961, 932963, 3),
  (932979, 932979, 1), (932979, 932958, 2), (932979, 932959, 3);
