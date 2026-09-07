-- Hemorrhage higher Vanilla ranks (R2 932986, R3 932987): trainable from the rogue trainer, era-gated
-- via a require-previous-rank chain. HAND-WRITTEN (not generated) — mirrors the hunter Aimed Shot /
-- warlock Siphon Life trainer-gate pattern (2026_08_16_22 / 2026_08_14_14). Idempotent: DELETE-before-INSERT.
--
-- This server loads trainers from the modern `trainer`/`trainer_spell` tables (ObjectMgr.cpp:
-- "SELECT ... FROM trainer_spell"), NOT the legacy `npc_trainer` table. The MAIN rogue trainer is
-- `trainer.Id`=9 (Requirement=4 = CLASS_ROGUE; greeting "Hello, rogue!  Ready for some training?";
-- 146 spells); `trainer.Id`=10 is the 5-spell starter and is NOT used.
--
-- Era gate: rank 1 exists ONLY as a Vanilla era-talent CLONE (Hemorrhage 932985 via node 18449). A
-- WotLK rogue never has 932985, so requiring it (ReqAbility1) makes the R2->R3 chain reachable ONLY by
-- a Vanilla-era character who took the talent — no extra era filtering needed. This is exactly why the
-- STOCK Hemorrhage ranks already on this trainer (17347 ReqAbility1=16511 L46, 17348 ReqAbility1=17347
-- L58) can NEVER be trained by a Vanilla rogue: they require stock rank 1 (16511), which the era talent
-- no longer grants (it grants the custom clone 932985 instead). So the stock chain self-gates and needs
-- no strip — the identical baseline-leak precedent as the hunter stock Aimed Shot 20900-04 (ReqAbility1
-- = stock 19434, which a Vanilla hunter never has). A WotLK-stage rogue still trains the stock ranks.
--
-- Levels come from the stock Hemorrhage rows' trainer ReqLevel (17347 = 46, 17348 = 58 — the authentic
-- Vanilla trainer levels; classic references agree R3 requires ~58). MoneyCost is a gentle rising curve
-- in the spirit of the trainer's other mid-level spells (not a copy of any single stock chain).
-- Per-rank debuff magnitudes (+5 / +7 flat physical) are authored in era-data/vanilla/rogue.yaml
-- (helpers 932986 / 932987).

DELETE FROM `trainer_spell` WHERE `TrainerId`=9 AND `SpellId` IN (932986,932987);
INSERT INTO `trainer_spell`
  (`TrainerId`,`SpellId`,`MoneyCost`,`ReqSkillLine`,`ReqSkillRank`,`ReqAbility1`,`ReqAbility2`,`ReqAbility3`,`ReqLevel`,`VerifiedBuild`) VALUES
  (9, 932986, 4000, 0, 0, 932985, 0, 0, 46, 0),   -- Rank 2 (+5): req talent-granted R1 932985
  (9, 932987, 8000, 0, 0, 932986, 0, 0, 58, 0);   -- Rank 3 (+7): req R2 932986

-- --- spell_ranks chain: make R1..R3 a real rank chain so a higher rank OVERWRITES the lower's
-- MOD_DAMAGE_TAKEN debuff on a target (instead of three unrelated aura-14 debuffs), the client
-- spellbook collapses same-name ranks to the highest known, and Reset's removeSpell(932985) CASCADES
-- via GetNextSpellInChain to strip the trained ranks on respec. first_spell_id is the talent-granted
-- rank 1 (932985). The core's LoadSpellRanks requires every id to have a spell_dbc record, which these
-- custom era rows do. NO interaction with the era module: 932985 is talent-granted (era_talent_rank),
-- NOT managed by ReconcileBaselineSpells, so a chain here is safe (same as Siphon Life 932902 / Aimed
-- Shot 932952). `spell_id` is UNIQUE, so DELETE the chain by first_spell_id before re-inserting.
DELETE FROM `spell_ranks` WHERE `first_spell_id`=932985;
INSERT INTO `spell_ranks` (`first_spell_id`,`spell_id`,`rank`) VALUES
  (932985, 932985, 1),
  (932985, 932986, 2),
  (932985, 932987, 3);
