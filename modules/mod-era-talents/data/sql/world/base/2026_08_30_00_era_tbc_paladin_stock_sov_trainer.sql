-- Stock WotLK Seal of Vengeance (31801, Alliance, TrainerId 3) / Seal of Corruption (53736,
-- Horde, TrainerId 4) were still UNGATED trainer_spell rows (ReqLevel 64) — a TBC-era paladin
-- could train the WotLK-reworked seal alongside the era substrate's own SoV/SoB clones
-- (946084/946080). Found in real-client testing 2026-08-30 (Paladintest had stock 31801 in
-- character_spell next to the TBC substrate). Same version-swap model as the Vanilla seal
-- rework (2026_08_20_40): delete the stock rows here; ReconcileBaselineSpells strips the ids
-- for Vanilla/TBC paladins (kStockPalSwap) and auto-grants the faction-appropriate seal by
-- level (64) for WotLK paladins, so nothing is lost at WotLK stage. HAND-WRITTEN. Idempotent.
DELETE FROM `trainer_spell` WHERE `SpellId` IN (31801, 53736);
