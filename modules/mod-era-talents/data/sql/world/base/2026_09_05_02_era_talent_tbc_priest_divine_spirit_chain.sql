-- TBC Priest, Phase 6 Task 6 (Discipline tab). Divine Spirit ranks 2-5 and Prayer of Spirit ranks
-- 1-2 for a TBC-era priest, as trainer-taught era clones.
-- HAND-WRITTEN (not generated) — mirrors the TBC rogue Mutilate chain (2026_09_04_02) and the TBC
-- druid Mangle chain (2026_08_31_03). Idempotent: DELETE-before-INSERT on every table.
--
-- This server loads trainers from the modern `trainer`/`trainer_spell` tables (ObjectMgr.cpp reads
-- "SELECT ... FROM trainer_spell"), NOT the legacy `npc_trainer` table. THE PRIEST TRAINER SET IS
-- `trainer.Id` = 11 (Type 0, Requirement 5 = CLASS_PRIEST, "Hello, priest!  Ready for some
-- training?"), and every stock Divine Spirit / Prayer of Spirit row lives on it. `trainer.Id` = 12
-- is a SECOND priest set with the identical greeting, but it is a five-row STARTER set — measured
-- 2026-09-05, its complete contents are Power Word: Shield r1 (17, L6), Shadow Word: Pain (589,
-- L4), Lesser Heal r2 (591, L6), Power Word: Fortitude (1243, L1) and Lesser Heal r1 (2052, L4).
-- It carries NO Divine Spirit or Prayer of Spirit row at all, so naming only TrainerId 11 below
-- leaves nothing behind. `.reload trainer` (or a restart) applies this file live.
--
-- WHY THE CLONES EXIST (era-data/tbc/priest.yaml helpers 948024-948030, node 20513, spec section 4a
-- and era-data/_ref/tbc/priest-spells.yaml capstone_chains.divine_spirit_and_prayer_of_spirit):
-- live 14752 carries ONE effect (aura 29 MOD_STAT, Spirit). TBC's record carries THREE — the same
-- Spirit buff PLUS two ZERO-VALUED carriers, aura 174 MOD_SPELL_DAMAGE_OF_STAT_PERCENT (misc 126
-- all schools, miscB 4 = Spirit) and aura 175 MOD_SPELL_HEALING_OF_STAT_PERCENT (misc 4 = Spirit).
-- Those two carriers ARE the mechanism of Improved Divine Spirit (node 20514, wago 33174/33182 =
-- SPELLMOD_EFFECT2/SPELLMOD_EFFECT3 +5/+10 masked to family-6 word0 0x20): WotLK deleted them when
-- it retired the talent, so a STOCK GRANT of 14752 can never carry Improved Divine Spirit no matter
-- what the talent authors. The per-rank mana also differs on every rank (TBC flat 250/350/450/555/
-- 680 and 1500/1800 vs live 285/420/785/970 + a ManaCostPct 26 on r5, and 1940 + ManaCostPct 69).
--
-- Node 20513 grants clone rank 1 (948024). This file teaches the rest behind a ReqAbility1 chain
-- rooted at that grant. It deletes nothing and re-gates nothing.

-- =====================================================================================================
-- (a) THE STOCK DIVINE SPIRIT / PRAYER OF SPIRIT TRAINER ROWS ARE LEFT COMPLETELY ALONE — and here is
--     the orphaned-higher-rank check that says they must be (docs/era-talents-framework.md
--     "Deleting a stock trainer row: the orphaned-higher-rank check (MANDATORY)"), plus the intent
--     test that says no deletion is warranted in the first place.
--
-- The stock TrainerId-11 chains, read off the live table 2026-09-05 (they match
-- _ref capstone_chains.divine_spirit_and_prayer_of_spirit.live_trainer_rows exactly):
--     14818 ReqLevel 40 ReqAbility1 14752  MoneyCost  900     <-- Divine Spirit r2
--     14819 ReqLevel 50 ReqAbility1 14818  MoneyCost 1500     <-- r3
--     27841 ReqLevel 60 ReqAbility1 14819  MoneyCost 2300     <-- r4
--     25312 ReqLevel 70 ReqAbility1 27841  MoneyCost 2300     <-- r5
--     48073 ReqLevel 80 ReqAbility1 25312  MoneyCost 9000     <-- r6, WotLK-only
--     27681 ReqLevel 60 ReqAbility1 14752  MoneyCost 2300     <-- Prayer of Spirit r1
--     32999 ReqLevel 70 ReqAbility1 27681  MoneyCost 3400     <-- r2
--     48074 ReqLevel 80 ReqAbility1 32999  MoneyCost 9000     <-- r3, WotLK-only
--   (14752 itself has NO `trainer_spell` row on ANY TrainerId — verified by direct query.)
--
--   1. ORPHANED-HIGHER-RANK CHECK: the WHOLE chain hangs off ReqAbility1 = 14752. Deleting or
--      re-pointing 14818 would strand 14819 -> 27841 -> 25312 -> 48073, and deleting/re-pointing
--      27681 would strand 32999 -> 48074, for every NATIVE WotLK priest (who never touches the era
--      system). There is no compensating grant for any of those ids in this module. That is exactly
--      the regression the first revision of the druid Mangle migration shipped and had to reverse
--      (2026_08_31_03 section (a)), and the standing lesson
--      `deleting-a-stock-trainer-row-orphans-higher-ranks`.
--   2. THE DELETION WOULD ALSO BE POINTLESS, because the stock chain ALREADY self-gates against a
--      TBC-era priest — framework option 3, the preferred outcome. Its root requirement is stock
--      rank 1 (14752), which node 20513 no longer grants (it grants clone 948024), and which has no
--      `trainer_spell` row of its own on any trainer, so a TBC priest can never buy it either.
--      EraTalents.cpp's CLASS_PRIEST TBC arm (2) additionally strips every stock DS/PoS rank from a
--      TBC priest (readiness-gated on EraNodeReplacesStock(ERA_TBC, CLASS_PRIEST, 20513, 14752),
--      which node 20513's clone grant is what arms), covering a character migrated from an older
--      build. Unable to satisfy ReqAbility1, a TBC priest cannot see or buy any stock rank.
--   3. TRAINER-GATING INTENT TEST (the Devouring Plague rule): a stock `trainer_spell` row is a
--      "hole" only if the module gates that spell somewhere and a missed set leaks it. The
--      `kBaselineSpellGates` rows for talent 18113 in EraTalents.cpp are the gate, and they are
--      VANILLA-scoped strip-only rows (grantHigherEra = false on the Prayer of Spirit row, and the
--      Divine Spirit row's WotLK force-grant is explicitly SKIPPED for a TBC priest by the gate
--      walker at EraTalents.cpp:1980). The TBC leak they were protecting against is closed by the
--      unconditional strip in arm (2), not by a trainer edit — see
--      _ref capstone_chains.divine_spirit_and_prayer_of_spirit.orphan_check, which reaches the same
--      "PREFERRED SHAPE: change nothing in trainer_spell" conclusion independently.
-- Net: no DELETE, no UPDATE, no re-gate on any stock row. Do not "tidy" this by adding one.

-- =====================================================================================================
-- (b) Divine Spirit clone ranks 2-5 and Prayer of Spirit clone ranks 1-2, trainer-taught, era-gated
--     by a require-previous-rank chain rooted at the TALENT-granted rank 1 (948024, TBC node 20513).
--
-- Era gate: rank 1 of this chain exists ONLY as the TBC era-talent clone. A Vanilla-era priest's own
-- Divine Spirit node (18113) grants STOCK 14752, and a WotLK priest trains the stock chain, so
-- neither ever holds 948024 — requiring it via ReqAbility1 makes this ladder reachable ONLY by a
-- TBC-era priest who spent the talent. This is the talent-anchor flavour of the trainer gate
-- documented in docs/era-talents-framework.md ("Era-gating CUSTOM spell ranks that REPLACE a stock
-- WotLK version", anchor flavour 1) — no marker spell is needed, exactly as for the TBC rogue
-- Mutilate and TBC druid Mangle / Nature's Grasp chains.
--
-- PRAYER OF SPIRIT RANK 1 REQUIRES DIVINE SPIRIT RANK 1, NOT A PRAYER-OF-SPIRIT PREDECESSOR — this
-- mirrors the stock shape exactly (stock 27681.ReqAbility1 = 14752), so the era chain reproduces
-- TBC's own "you must know Divine Spirit before you can buy the raid version" requirement.
--
-- ReqLevel 40/50/60/70 and 60/70 are corroborated three ways: the wago 2.5.4 `SpellLevels` for
-- 14818/14819/27841/25312/27681/32999 (family J of the survival sweep, which found ZERO
-- TBC-vs-WotLK trainer-level drift anywhere in this class), the live stock rows above, and
-- _ref capstone_chains.divine_spirit_and_prayer_of_spirit.tbc_levels. MoneyCost is copied VERBATIM
-- from the stock rows above.
--
-- A spell used as a `trainer_spell.ReqAbility1` MUST have a client `Spell.dbc` name row or the
-- WotLK trainer UI Lua-errors the whole window empty (framework "Client visibility"; the paladin
-- 946078 bug). Every id referenced here — 948024 through 948030 — carries a `client:` block in
-- era-data/tbc/priest.yaml, so they all ship a client row in patch-V.mpq.
DELETE FROM `trainer_spell` WHERE `TrainerId`=11 AND `SpellId` IN (948025,948026,948027,948028,948029,948030);
INSERT INTO `trainer_spell`
  (`TrainerId`,`SpellId`,`MoneyCost`,`ReqSkillLine`,`ReqSkillRank`,`ReqAbility1`,`ReqAbility2`,`ReqAbility3`,`ReqLevel`,`VerifiedBuild`) VALUES
  (11, 948025,  900, 0, 0, 948024, 0, 0, 40, 0),   -- Divine Spirit   Rank 2 (+23 Spirit): req talent-granted R1 948024
  (11, 948026, 1500, 0, 0, 948025, 0, 0, 50, 0),   -- Divine Spirit   Rank 3 (+33 Spirit): req R2 948025
  (11, 948027, 2300, 0, 0, 948026, 0, 0, 60, 0),   -- Divine Spirit   Rank 4 (+40 Spirit): req R3 948026
  (11, 948028, 2300, 0, 0, 948027, 0, 0, 70, 0),   -- Divine Spirit   Rank 5 (+50 Spirit): req R4 948027
  (11, 948029, 2300, 0, 0, 948024, 0, 0, 60, 0),   -- Prayer of Spirit Rank 1 (+40 Spirit, party): req DS R1 948024 (stock shape)
  (11, 948030, 3400, 0, 0, 948029, 0, 0, 70, 0);   -- Prayer of Spirit Rank 2 (+50 Spirit, party): req PoS R1 948029

-- =====================================================================================================
-- (c) TWO `spell_ranks` chains, mirroring the stock pair: 948024 -> 948025 -> 948026 -> 948027 ->
--     948028 (Divine Spirit r1-r5) and 948029 -> 948030 (Prayer of Spirit r1-r2). Three jobs, all
--     load-bearing here:
--   * the client spellbook collapses same-name / same-skill-line ranks (SkillLine 613 Discipline,
--     from each clone's `client.skillLine`) to the highest known, instead of showing five separate
--     Divine Spirit buttons;
--   * `Player::removeSpell` walks GetNextSpellInChain, so Reset()'s removeSpell(948024) on respec
--     CASCADES and strips every trained rank — the same respec-clean property the Mutilate
--     (947773-776), Bloodthirst (947512-517) and Mangle (946258-260) chains rely on;
--   * a higher rank cleanly SUPERSEDES the lower for the bot AI's rank walks and for the client's
--     "next rank available" logic.
--
-- The Prayer of Spirit chain is SEPARATE and roots at 948029, exactly as the stock chain roots at
-- 27681 rather than folding into 14752's — Divine Spirit and Prayer of Spirit are two spells that
-- merely share a spell-specific group, and merging them would make the client collapse the raid
-- version into the single-target one.
--
-- NOTE the interaction with `first_spell_id` = 948024, which IS a plain node grant (era_talent_rank)
-- and NOT a ReconcileBaselineSpells-managed row — so this rank chain cannot fight the reconcile.
-- (Contrast the STOCK 14752, which IS gate-managed: had the era ranks been hung off the stock root,
-- the gate's strip and the chain's cascade would have raced.) Identical situation to the TBC rogue
-- Mutilate 947773 and TBC druid Nature's Grasp 946270 chains.
-- SpellMgr::LoadSpellRanks requires every id in a chain to have a `spell_dbc` record; these do
-- (2026_09_05_01_era_talent_tbc_priest_custom_spells.sql, generated).
--
-- RESPEC/BAND-MOVE STRIP IS MODULE-SIDE, NOT `spell_required`: the stock shape (27681.ReqAbility1 =
-- 14752) would normally rely on core `Player.cpp`'s `spell_required` walk to strip a dependent
-- spell when its prerequisite goes away, but that walk only fires when
-- `GetTalentSpellCost(firstRankSpellId) > 0` — true for a REAL Talent.dbc entry, and DBCStores
-- returns 0 for any id outside it. Every id in this chain (948024-948030) is a CUSTOM spell, so
-- `GetTalentSpellCost` is always 0 for them and the core-side removal never runs. DS r1 948024 is
-- at least a node grant, so a respec still clears 948024-948028 via the `spell_ranks` cascade above
-- (`Player::removeSpell` walks `GetNextSpellInChain`) plus `StripOrphanedGrants`, which sees any
-- node grant. Prayer of Spirit 948029/948030 is trainer-taught and never a node grant, so neither
-- mechanism reaches it — `EraTalents.cpp`'s CLASS_PRIEST TBC arm (2b), plus its cross-era mirror
-- just above the TBC block, are the ONLY things that strip 948029/948030 on a respec or an
-- era-band move. This file's `ReqAbility1` chain is authoring-time gating only (who can BUY the
-- next rank); it has no runtime removal role.
-- `spell_ranks.spell_id` is UNIQUE, so delete by first_spell_id before re-inserting.
DELETE FROM `spell_ranks` WHERE `first_spell_id` IN (948024, 948029);
INSERT INTO `spell_ranks` (`first_spell_id`,`spell_id`,`rank`) VALUES
  (948024, 948024, 1),
  (948024, 948025, 2),
  (948024, 948026, 3),
  (948024, 948027, 4),
  (948024, 948028, 5),
  (948029, 948029, 1),
  (948029, 948030, 2);
