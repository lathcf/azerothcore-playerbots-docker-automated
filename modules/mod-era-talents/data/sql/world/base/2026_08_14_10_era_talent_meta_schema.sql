-- Generation-stamp metadata for the era-talents pipeline (read at worldserver startup,
-- sent to the addon, compared against the client MPQ's sentinel spell 932999).
CREATE TABLE IF NOT EXISTS `era_talent_meta` (
  `k` VARCHAR(32) NOT NULL,
  `v` VARCHAR(64) NOT NULL,
  PRIMARY KEY (`k`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
