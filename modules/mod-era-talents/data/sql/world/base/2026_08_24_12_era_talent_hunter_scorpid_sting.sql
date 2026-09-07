-- Vanilla Scorpid Sting trainer wiring (2026-08-24). Stock Scorpid Sting 3043 (the only rank a WotLK
-- hunter trains) is the WotLK -hit-chance rework; the authentic Vanilla Str/Agi 4-rank chain
-- (14275-14277 were deleted from the 3.3.5a DBC) is reconstructed as custom clones 932300-303 (see
-- era-data/vanilla/hunter.yaml). Per-era gating on the shared hunter trainer (trainer.Id=7) via two
-- client-visible presence markers, EXACTLY like the warlock stone chains (932930/932939):
--   * custom Vanilla clones 932300-303 gated on 932304 ("Vanilla Hunter") — only a Vanilla hunter has it;
--   * stock 3043 RE-GATED on 932305 ("WotLK Hunter") — only a TBC/WotLK hunter has it (was ungated).
-- ReconcileBaselineSpells (EraTalents.cpp CLASS_HUNTER) grants the era-matching marker + strips the
-- wrong-era Scorpid Sting from the spellbook. A ReqAbility marker MUST have a client Spell.dbc row (both
-- do) or the Blizzard trainer UI crashes formatting the requirement name — the warlock-stone lesson.
--
-- This server loads trainers from `trainer`/`trainer_spell` (ObjectMgr.cpp), NOT legacy `npc_trainer`.
-- `.reload trainer` (or a restart) applies it live. Idempotent (DELETE before INSERT).

-- (a) Custom Vanilla Scorpid Sting chain, gated on the "Vanilla Hunter" marker 932304. Levels 22/32/42/52
--     (authentic 1.12.1); rank r>1 also requires the previous rank so the chain trains in order.
DELETE FROM `trainer_spell` WHERE `TrainerId`=7 AND `SpellId` IN (932300,932301,932302,932303);
INSERT INTO `trainer_spell` (`TrainerId`,`SpellId`,`MoneyCost`,`ReqSkillLine`,`ReqSkillRank`,`ReqAbility1`,`ReqAbility2`,`ReqAbility3`,`ReqLevel`,`VerifiedBuild`) VALUES
(7, 932300,  6000, 0, 0, 932304,      0, 0, 22, 0),   -- Scorpid Sting Rank 1
(7, 932301, 13000, 0, 0, 932300, 932304, 0, 32, 0),   -- Scorpid Sting Rank 2
(7, 932302, 26000, 0, 0, 932301, 932304, 0, 42, 0),   -- Scorpid Sting Rank 3
(7, 932303, 45000, 0, 0, 932302, 932304, 0, 52, 0);   -- Scorpid Sting Rank 4

-- (b) Stock Scorpid Sting 3043 RE-GATED behind the "WotLK Hunter" marker 932305 (was ungated). Original
--     MoneyCost 6000 / ReqLevel 22 preserved.
DELETE FROM `trainer_spell` WHERE `TrainerId`=7 AND `SpellId`=3043;
INSERT INTO `trainer_spell` (`TrainerId`,`SpellId`,`MoneyCost`,`ReqSkillLine`,`ReqSkillRank`,`ReqAbility1`,`ReqAbility2`,`ReqAbility3`,`ReqLevel`,`VerifiedBuild`) VALUES
(7, 3043, 6000, 0, 0, 932305, 0, 0, 22, 0);           -- stock WotLK Scorpid Sting, re-gated on 932305

-- (c) spell_ranks chain for the custom clones so the spellbook collapses same-name ranks to the highest
--     trained (Player::AddSpell supersede) and Reset's removeSpell(932300) cascades. Server-side only.
DELETE FROM `spell_ranks` WHERE `first_spell_id`=932300;
INSERT INTO `spell_ranks` (`first_spell_id`, `spell_id`, `rank`) VALUES
(932300, 932300, 1), (932300, 932301, 2), (932300, 932302, 3), (932300, 932303, 4);
