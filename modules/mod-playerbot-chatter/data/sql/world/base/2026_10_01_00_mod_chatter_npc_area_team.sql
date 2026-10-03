-- Which player faction can USE each creature entry (lore sidecar's service-NPC faction filter):
--   0 = Alliance only, 1 = Horde only, 2 = both / neutral, 3 = neither (hostile to both)
--   NULL = not yet computed (the sidecar fails open on NULL).
-- Filled by the worldserver's startup area backfill (PBChatterAreaBackfill) from
-- CreatureTemplate::faction via FactionTemplateEntry::IsHostileTo against the Human (1) and
-- Orc (2) player faction templates. Existing rows start NULL and are filled on the next boot.
--
-- IDEMPOTENT ON PURPOSE: the core updater re-applies a CHANGED file by hash (base/ is not an
-- archived dir), so a bare ALTER would re-run on any later edit, fail on the duplicate column,
-- and stop ac-db-import. The information_schema guard makes a re-apply a no-op.
SET @team_exists := (
  SELECT COUNT(1)
  FROM INFORMATION_SCHEMA.COLUMNS
  WHERE TABLE_SCHEMA = DATABASE()
    AND TABLE_NAME = 'mod_chatter_npc_area'
    AND COLUMN_NAME = 'team'
);

SET @ddl := IF(@team_exists = 0,
  'ALTER TABLE `mod_chatter_npc_area` ADD COLUMN `team` TINYINT UNSIGNED NULL DEFAULT NULL COMMENT ''0=Alliance-only 1=Horde-only 2=both 3=neither NULL=unknown'';',
  'SELECT "mod_chatter_npc_area.team already exists.";'
);

PREPARE stmt FROM @ddl;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;
