-- =================================================================================================
-- mod-era-talents — HUNTER FINAL REVIEW (2026-09-06), hand SQL.
--
-- I-30 — put the era Aimed Shot and Scorpid Sting clones into the `spell_group` exclusivity groups
-- their STOCK counterparts already belong to. A clone has a different id, so it inherits nothing
-- from `spell_group`, and the generator never writes that table: these rows can only be hand SQL.
--
-- GROUP 1061 — the "mortal wounds" (healing-taken reduction) exclusivity group. Live rows:
--   1061: -1057 (a nested group whose sole member is 13218 Wound Poison), 12294 Mortal Strike,
--         19434 Aimed Shot, 56112 (verified live 2026-09-06).
-- Only the RANK-1 stock id is listed for a ranked chain (19434 is there; 20900-27065 are not), so
-- the era chain likewise needs only its r1. TBC Aimed Shot 948370 carries the -50% healing debuff
-- on its own effect, so without this row an era hunter's Aimed Shot would STACK with a warrior's
-- Mortal Strike / a rogue's Wound Poison instead of overriding-or-yielding like stock.
--
-- GROUP 1060 — the Scorpid Sting <-> Insect Swarm exclusivity group. Live rows: 1060: 3043 Scorpid
-- Sting, 5570 Insect Swarm. The group exists so the two attack-weakening debuffs cannot both sit on
-- one target; the era Scorpid Sting clones are the same spell in the same slot and belong in it,
-- whichever encoding their era uses:
--   Vanilla 932300-932303 (the four-rank chain) — `aura 29 MOD_STAT` reducing STRENGTH, the 1.12
--                          encoding, plus the module script `era_hunter_scorpid_sting`
--   TBC     948281        — `aura 54 MOD_HIT_CHANCE` -5%, TBC's own value (live 3043 is -3%)
-- Without these rows a druid's Insect Swarm and an era hunter's Scorpid Sting would both apply,
-- which no era ever allowed.
--
-- DELIBERATELY NOT ADDED: the VANILLA Aimed Shot clones 932952-932957. Vanilla's Aimed Shot carries
-- NO healing debuff at all (it is pure weapon damage + a cast time), so it has no business in a
-- mortal-wounds group; adding it would suppress an unrelated warrior/rogue debuff.
--
-- Idempotent: DELETE over exactly the band ids this file owns, then INSERT. AzerothCore re-applies a
-- CHANGED file by hash rather than by name, so a re-run is a no-op delete + re-insert. Nothing here
-- touches a stock row -- every spell_id below is inside the reserved era band [920000, 950000).
-- =================================================================================================
DELETE FROM `spell_group` WHERE `spell_id` IN (948370, 932300, 932301, 932302, 932303, 948281);
INSERT INTO `spell_group` (`id`, `spell_id`) VALUES
(1061, 948370),   -- TBC Aimed Shot r1 clone   -> mortal-wounds group (with 12294 / 19434 / 13218)
(1060, 932300),   -- Vanilla Scorpid Sting r1  -> Scorpid <-> Insect Swarm group (with 3043 / 5570)
(1060, 932301),   -- Vanilla Scorpid Sting r2
(1060, 932302),   -- Vanilla Scorpid Sting r3
(1060, 932303),   -- Vanilla Scorpid Sting r4
(1060, 948281);   -- TBC Scorpid Sting clone
