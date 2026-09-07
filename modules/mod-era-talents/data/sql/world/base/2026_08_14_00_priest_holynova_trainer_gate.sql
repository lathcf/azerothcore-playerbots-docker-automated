-- Holy Nova era-gating: remove rank 1 from the priest trainer so it is NOT free in Vanilla
-- (the era talent grants it; the module reconcile grants it in TBC/WotLK). Idempotent.
--
-- NOTE: this server loads trainers from the modern `trainer`/`trainer_spell` tables
-- (ObjectMgr.cpp: "SELECT ... FROM trainer_spell"), NOT the legacy `npc_trainer` table. The
-- priest trainer is `trainer.Id`=11 (greeting "Hello, priest! ..."), linked to creatures via
-- `creature_default_trainer`. Deleting Holy Nova rank 1 (15237) here gates the whole rank chain
-- behind the talent, because the stock `trainer_spell` rows for ranks 2/3 already require the
-- previous rank via ReqAbility1 (15430->15237, 15431->15430) — so without rank 1 they're
-- untrainable too. (The higher-rank ReqAbility chain is asserted below for safety/idempotency.)
DELETE FROM `trainer_spell` WHERE `TrainerId`=11 AND `SpellId`=15237;
UPDATE `trainer_spell` SET `ReqAbility1`=15237 WHERE `TrainerId`=11 AND `SpellId`=15430;
UPDATE `trainer_spell` SET `ReqAbility1`=15430 WHERE `TrainerId`=11 AND `SpellId`=15431;

-- Also strip it from the legacy `npc_trainer` template (harmless if the core ever reads it, and
-- keeps the two tables consistent). Template 200012 is the priest set in that legacy layout.
DELETE FROM `npc_trainer` WHERE `ID`=200012 AND `SpellID`=15237;
UPDATE `npc_trainer` SET `ReqSpell`=15237 WHERE `ID`=200012 AND `SpellID`=15430;
UPDATE `npc_trainer` SET `ReqSpell`=15430 WHERE `ID`=200012 AND `SpellID`=15431;
