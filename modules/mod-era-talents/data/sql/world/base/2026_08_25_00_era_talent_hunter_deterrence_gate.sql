-- Deterrence trainer gate (2026-08-25). Deterrence became BASELINE in WotLK — trainer-taught at L60
-- (trainer.Id 7, spell 19263; verified NOT in talent_dbc, so it is not a WotLK talent) — but in Vanilla
-- it is a Survival TALENT (era node 18339 grants the Vanilla clone 932964, never the WotLK rework 19263).
-- The stock 19263 trainer row was UNGATED (ReqAbility1=0), so a Vanilla-era hunter could train the WotLK
-- deflect rework EVERY login even without the talent: ReconcileBaselineSpells strips 19263 from the
-- spellbook, but a strip can't stop the trainer OFFERING it (the user-reported leak).
--
-- Fix = re-gate 19263 behind the "WotLK Hunter" marker 932305 (only a TBC/WotLK hunter carries it;
-- ReconcileBaselineSpells CLASS_HUNTER grants 932305 to non-Vanilla hunters / 932304 to Vanilla hunters)
-- — the SAME two-marker trainer pattern the Scorpid Sting chains use (2026_08_24_12). 932305 has a
-- client: block so the Blizzard trainer UI can render the "Requires ..." line (an invisible marker
-- crashes it). A Vanilla hunter (no 932305) no longer sees 19263; a WotLK hunter trains it normally.
-- The existing EraTalents.cpp CLASS_HUNTER Vanilla-strip of 19263 stays (cleans up anyone who trained it
-- before this fix). MoneyCost 2200 / ReqLevel 60 preserved from the base row. Idempotent.
--
-- This server loads trainers from `trainer`/`trainer_spell` (ObjectMgr.cpp), NOT legacy `npc_trainer`.
-- `.reload trainer` (or a restart) applies it live.

DELETE FROM `trainer_spell` WHERE `TrainerId`=7 AND `SpellId`=19263;
INSERT INTO `trainer_spell` (`TrainerId`,`SpellId`,`MoneyCost`,`ReqSkillLine`,`ReqSkillRank`,`ReqAbility1`,`ReqAbility2`,`ReqAbility3`,`ReqLevel`,`VerifiedBuild`) VALUES
(7, 19263, 2200, 0, 0, 932305, 0, 0, 60, 0);

DELETE FROM `npc_trainer` WHERE `SpellID`=19263;   -- dead weight on this core, but keep it consistent
