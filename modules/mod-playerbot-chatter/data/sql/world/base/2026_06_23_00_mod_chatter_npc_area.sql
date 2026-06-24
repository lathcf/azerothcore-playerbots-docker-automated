-- Cache of resolved area/zone names per creature entry, filled by the worldserver's
-- `.chatter backfillareas` console command (DBC names; not available to the sidecar otherwise).
CREATE TABLE IF NOT EXISTS `mod_chatter_npc_area` (
  `creature_entry` INT UNSIGNED NOT NULL,
  `area_name` VARCHAR(100) NOT NULL DEFAULT '',
  `zone_name` VARCHAR(100) NOT NULL DEFAULT '',
  PRIMARY KEY (`creature_entry`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
