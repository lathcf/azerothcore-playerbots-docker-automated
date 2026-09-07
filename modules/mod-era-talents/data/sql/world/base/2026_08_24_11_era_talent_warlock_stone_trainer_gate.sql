-- Firestone/Spellstone trainer gate (2026-08-24, rev 3 — the shaman two-marker pattern).
--
-- The Vanilla warlock stones are era-diverged: the custom clones (932922-928) conjure the Vanilla
-- off-hand items; the stock Create Firestone/Spellstone ids conjure the WotLK weapon-enchant items.
-- The trainer is global, so both must coexist on warlock trainer TrainerId 31, each gated so only its
-- era's warlock sees it — EXACTLY like the shaman totem trainer (2026_08_23_70), which gates the custom
-- chain on marker 932416 and re-gates the stock chain on 932417.
--
-- WHY A CLIENT-VISIBLE MARKER: a trainer_spell ReqAbility spell must have a client Spell.dbc row or the
-- Blizzard trainer UI (ClassTrainer_SetSelection -> format) gets a nil requirement NAME and throws a Lua
-- error ("bad argument #2 to 'format'") the instant the row is selected. The first attempts here gated
-- the custom rows on 932930 ("Era: Vanilla Cast Times"), which had NO client row -> that crash. Fix:
-- 932930 and the new mirror 932939 ("Era: WotLK Warlock") now BOTH carry a client: block (name only, no
-- skillLine -> still hidden from the spellbook) — see era-data/vanilla/warlock.yaml.
--
--   * Custom Vanilla clones (932922-928): gated on 932930 (only a Vanilla warlock carries it).
--   * Stock chains (6366.. / 2362..): RE-GATED (not deleted) on 932939 (only a TBC/WotLK warlock carries
--     it). A row that already had a prev-rank ReqAbility1 keeps it and gets 932939 as ReqAbility2, so the
--     in-order training chain is intact for WotLK warlocks and a Vanilla warlock (no 932939) sees none.
--
-- ReconcileBaselineSpells (EraTalents.cpp CLASS_WARLOCK) grants 932930 to Vanilla warlocks / 932939 to
-- post-Vanilla warlocks (strips the other), and strips the wrong era's create spells from the spellbook.
-- Trainer-taught, never auto-granted. This server loads `trainer`/`trainer_spell` (ObjectMgr.cpp), NOT
-- legacy `npc_trainer`. `.reload trainer` (or a restart) applies it live. Idempotent (DELETE before INSERT).

-- (a) Custom Vanilla create clones, gated on the Vanilla marker 932930 (re-inserted here so it is robust
--     to an earlier rev of this file having deleted them; values match 2026_08_16_16_..._vanilla_stones.sql).
DELETE FROM `trainer_spell` WHERE `TrainerId`=31 AND `SpellId` IN (932922,932923,932924,932925,932926,932927,932928);
INSERT INTO `trainer_spell` (`TrainerId`,`SpellId`,`MoneyCost`,`ReqSkillLine`,`ReqSkillRank`,`ReqAbility1`,`ReqAbility2`,`ReqAbility3`,`ReqLevel`,`VerifiedBuild`) VALUES
(31, 932922,  5000, 0, 0, 932930,      0, 0, 28, 0),   -- Create Firestone (Lesser)  -> item 1254
(31, 932923,  9000, 0, 0, 932922, 932930, 0, 36, 0),   -- Create Firestone           -> item 13699
(31, 932924, 13000, 0, 0, 932923, 932930, 0, 46, 0),   -- Create Firestone (Greater) -> item 13700
(31, 932925, 22000, 0, 0, 932924, 932930, 0, 56, 0),   -- Create Firestone (Major)   -> item 13701
(31, 932926,  9000, 0, 0, 932930,      0, 0, 31, 0),   -- Create Spellstone          -> item 5522
(31, 932927, 14000, 0, 0, 932926, 932930, 0, 43, 0),   -- Create Spellstone (Greater)-> item 13602
(31, 932928, 26000, 0, 0, 932927, 932930, 0, 55, 0);   -- Create Spellstone (Major)  -> item 13603

-- (b) Stock Create Firestone/Spellstone chains, RE-GATED on the WotLK marker 932939. Original MoneyCost/
--     ReqLevel preserved (base trainer_spell.sql); a row with a prev-rank ReqAbility1 keeps it and gets
--     932939 as ReqAbility2, rank-1 (6366/2362) takes 932939 as ReqAbility1.
DELETE FROM `trainer_spell` WHERE `TrainerId`=31 AND `SpellId` IN (6366,17951,17952,17953,27250,60219,60220,2362,17727,17728,28172,47886,47888);
INSERT INTO `trainer_spell` (`TrainerId`,`SpellId`,`MoneyCost`,`ReqSkillLine`,`ReqSkillRank`,`ReqAbility1`,`ReqAbility2`,`ReqAbility3`,`ReqLevel`,`VerifiedBuild`) VALUES
(31,  6366,   5000, 0, 0, 932939,      0, 0, 28, 0),   -- Create Firestone (rank 1)
(31, 17951,   9000, 0, 0,   6366, 932939, 0, 36, 0),   -- Create Firestone (rank 2)
(31, 17952,  13000, 0, 0,  17951, 932939, 0, 46, 0),   -- Create Firestone (rank 3)
(31, 17953,  22000, 0, 0,  17952, 932939, 0, 56, 0),   -- Create Firestone (rank 4)
(31, 27250,  51000, 0, 0,  17953, 932939, 0, 66, 0),   -- Create Firestone (rank 5, WotLK)
(31, 60219, 160000, 0, 0,  27250, 932939, 0, 74, 0),   -- Create Firestone (rank 6, WotLK)
(31, 60220, 160000, 0, 0,  60219, 932939, 0, 80, 0),   -- Create Firestone (rank 7, WotLK)
(31,  2362,   9000, 0, 0, 932939,      0, 0, 36, 0),   -- Create Spellstone (rank 1)
(31, 17727,  14000, 0, 0,   2362, 932939, 0, 48, 0),   -- Create Spellstone (rank 2)
(31, 17728,  26000, 0, 0,  17727, 932939, 0, 60, 0),   -- Create Spellstone (rank 3)
(31, 28172,  51000, 0, 0,  17728, 932939, 0, 66, 0),   -- Create Spellstone (rank 4, WotLK)
(31, 47886, 160000, 0, 0,  28172, 932939, 0, 72, 0),   -- Create Spellstone (rank 5, WotLK)
(31, 47888, 160000, 0, 0,  47886, 932939, 0, 78, 0);   -- Create Spellstone (rank 6, WotLK)

-- (c) spell_ranks chain for the custom Vanilla creates (the stock 6366/2362 chains already exist). Without
--     this the same-name custom ranks show as SEPARATE spellbook entries; with it Player::AddSpell
--     supersedes the lower rank (SMSG_SUPERCEDED_SPELL) so only the highest trained rank shows — exactly
--     like the stock firestone/spellstone chains and the shaman custom-totem spell_ranks. Server-side only.
DELETE FROM `spell_ranks` WHERE `first_spell_id` IN (932922,932926);
INSERT INTO `spell_ranks` (`first_spell_id`, `spell_id`, `rank`) VALUES
(932922, 932922, 1), (932922, 932923, 2), (932922, 932924, 3), (932922, 932925, 4),   -- Create Firestone
(932926, 932926, 1), (932926, 932927, 2), (932926, 932928, 3);                         -- Create Spellstone
