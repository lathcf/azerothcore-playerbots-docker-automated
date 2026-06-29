-- Persistent player raid roster: pins specific addclass-pool characters as one player's
-- fixed 40-bot roster, each slot carrying its intended role and forced talent tab.
CREATE TABLE IF NOT EXISTS `mod_raid_roster` (
  `owner_guid`  INT UNSIGNED NOT NULL,
  `bot_guid`    INT UNSIGNED NOT NULL,
  `class`       TINYINT UNSIGNED NOT NULL,
  `role`        TINYINT UNSIGNED NOT NULL,   -- 0=tank, 1=healer, 2=dps
  `spec_tab`    TINYINT UNSIGNED NOT NULL,   -- 0-based talent tab to force
  `slot_index`  TINYINT UNSIGNED NOT NULL,   -- 0..39, stable ordering
  PRIMARY KEY (`owner_guid`, `slot_index`),
  UNIQUE KEY `uk_bot` (`bot_guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
