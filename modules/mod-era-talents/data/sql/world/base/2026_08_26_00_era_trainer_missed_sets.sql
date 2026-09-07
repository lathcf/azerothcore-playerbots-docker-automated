-- mod-era-talents Phase 3: close IP trainer-set holes.
-- IP gates era-introduced spells by bumping trainer_spell.ReqLevel past the era level cap,
-- but its UPDATEs are per-(TrainerId,SpellId) and MISSED trainer sets 4/5 — the very sets
-- IP repoints real paladin trainers onto, and the playerbots factory unions ALL sets.
-- Intent test (spec 2026-08-26 Phase 3, B.1): only spells IP itself gates somewhere.
-- Audit sweep 2026-08-26: exactly these two spells diverge; everything else is IP-legal.

-- 31789 Righteous Defense (TBC-introduced): IP gates TrainerId 3 to 61; sets 4/5 still 14.
UPDATE `trainer_spell` SET `ReqLevel` = 61 WHERE `SpellId` = 31789 AND `TrainerId` IN (4, 5);

-- 62124 Hand of Reckoning (WotLK-introduced): IP gates TrainerId 3 to 71; sets 4/5 still 16.
UPDATE `trainer_spell` SET `ReqLevel` = 71 WHERE `SpellId` = 62124 AND `TrainerId` IN (4, 5);
