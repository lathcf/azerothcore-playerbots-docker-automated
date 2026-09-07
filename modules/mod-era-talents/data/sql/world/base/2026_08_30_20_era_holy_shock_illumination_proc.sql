-- mod-era-talents: widen acore's Illumination proc row so the era Holy Shock HEAL executors match it
-- (sanity-audit fix 2026-08-30; docs/verification/era-talents-tbc-phase1-paladin.md §7d item 6).
--
-- acore ships `spell_proc` SpellId -20210 (all Illumination ranks): SpellFamilyName 10,
-- SpellFamilyMask0 0xC0000000 (Holy Light | Flash of Light), SpellFamilyMask1 0x00010000 (stock
-- Holy Shock heal 25914's bit), HitMask 2 (crit). The era Holy Shock heal executors (Vanilla
-- 932652-654, TBC 946089/946118/946121/946124/946127) deliberately carry family 10 + word-A bit21
-- (0x00200000, the stock Holy Shock identity bit) and NOT word-B bit16 — bit16 flips
-- spell_pal_illumination's refund onto the GetSpellWithRank(20473) branch, which reads STOCK WotLK
-- rank costs instead of the executors' authored per-rank powerCost. So mask0 gains bit21:
-- 0xC0000000 | 0x00200000 = 0xC0200000 (3223322624).
--
-- Global-but-inert-for-others: WotLK paladins' stock HS heal (25914, word-B bit16) still matches via
-- the unchanged mask1; the only stock spells carrying word-A bit21 are the stock Holy Shock
-- dummy/damage spells, whose (damage) crits now match the row but no-op inside
-- spell_pal_illumination (it bails without HealInfo, and the aura has no charges to waste).
-- Idempotent: DELETE first; re-applied after acore's base row on every db-import.

DELETE FROM `spell_proc` WHERE `SpellId`=-20210;
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`, `SpellFamilyMask2`, `ProcFlags`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`, `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
(-20210, 0, 10, 3223322624, 65536, 0, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0);
