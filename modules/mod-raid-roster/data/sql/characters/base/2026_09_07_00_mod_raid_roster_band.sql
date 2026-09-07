-- Era band per roster slot (see RaidRosterComp.h RaidCompBand):
--   0 = eligible at every master level
--   1 = WotLK-only  (master level >= kWotlkBandMinLevel, 71) — the 4 Death Knight slots
--   2 = pre-WotLK-only (master level <  kWotlkBandMinLevel)  — the 4 DK substitute slots
-- DEFAULT 0 keeps pre-existing rows behaving exactly as before (DKs always eligible) until
-- `.raidroster create` tops the roster up and re-bands them.
--
-- IDEMPOTENT ON PURPOSE: the core updater re-applies a CHANGED file by hash (base/ is not an
-- archived dir), so a bare ALTER would re-run on any later edit, fail on the duplicate column,
-- and stop ac-db-import. The information_schema guard makes a re-apply a no-op.
SET @band_exists := (
  SELECT COUNT(1)
  FROM INFORMATION_SCHEMA.COLUMNS
  WHERE TABLE_SCHEMA = DATABASE()
    AND TABLE_NAME = 'mod_raid_roster'
    AND COLUMN_NAME = 'band'
);

SET @ddl := IF(@band_exists = 0,
  'ALTER TABLE `mod_raid_roster` ADD COLUMN `band` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT ''0=any 1=WotLK-only(71+) 2=pre-WotLK-only(<71)'';',
  'SELECT "mod_raid_roster.band already exists.";'
);

PREPARE stmt FROM @ddl;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;
