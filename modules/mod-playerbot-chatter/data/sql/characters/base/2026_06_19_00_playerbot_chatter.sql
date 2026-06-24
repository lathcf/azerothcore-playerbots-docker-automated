CREATE TABLE IF NOT EXISTS `mod_playerbot_chatter_history` (
    `id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
    `bot_guid` BIGINT UNSIGNED NOT NULL,
    `player_guid` BIGINT UNSIGNED NOT NULL,
    `ts` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    `player_message` TEXT NOT NULL,
    `bot_reply` TEXT NOT NULL,
    UNIQUE KEY `uniq_pair_msg` (`bot_guid`, `player_guid`, `player_message`(191), `bot_reply`(191)),
    KEY `idx_pair_ts` (`bot_guid`, `player_guid`, `ts`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
