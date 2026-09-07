-- TBC Priest, Phase 6 Task 8 (Shadow tab). Vampiric Touch ranks 2-3 and Mind Flay ranks 2-7 for a
-- TBC-era priest as trainer-taught era clones, plus a `spell_ranks` chain over the five Misery
-- debuff payloads.
-- HAND-WRITTEN (not generated) — same shape as the Discipline chain
-- (2026_09_05_02_era_talent_tbc_priest_divine_spirit_chain.sql) and the Holy chain
-- (2026_09_05_03_era_talent_tbc_priest_holy_chains.sql) in this phase, the TBC rogue Mutilate chain
-- (2026_09_04_02) and the TBC druid Mangle chain (2026_08_31_03). Idempotent: DELETE-before-INSERT
-- on every table.
--
-- This server loads trainers from the modern `trainer`/`trainer_spell` tables (ObjectMgr.cpp reads
-- "SELECT ... FROM trainer_spell"), NOT the legacy `npc_trainer` table. THE PRIEST TRAINER SET IS
-- `trainer.Id` = 11 (Type 0, Requirement 5 = CLASS_PRIEST). `trainer.Id` = 12 is a SECOND priest
-- set with the identical greeting, but it is a five-row STARTER set (Power Word: Shield r1 17,
-- Shadow Word: Pain 589, Lesser Heal r2 591, Power Word: Fortitude 1243, Lesser Heal r1 2052) and
-- carries NO Vampiric Touch or Mind Flay row at all, so naming only TrainerId 11 below leaves
-- nothing behind. `.reload trainer` (or a restart) applies this file live.
--
-- WHY THE CLONES EXIST (era-data/tbc/priest.yaml helpers 948060-948070, nodes 20563 and 20550, spec
-- section 4c + Amendment A.8, era-data/_ref/tbc/priest-spells.yaml capstone_chains):
--   * VAMPIRIC TOUCH (node 20563, the Shadow tier-9 capstone) diverges on five families. TBC's e1
--     DUMMY value is 5 — the percentage of the priest's Shadow damage returned to the PARTY as mana
--     — and the spell costs a FLAT 325/400/425. WotLK cut the DUMMY to 2, added a third DUMMY effect
--     and a `-34914` `spell_proc` row keyed to MIND BLAST (that pair IS the Replenishment design),
--     converted the cost to ManaCostPct 18/18/16, and bound `spell_pri_vampiric_touch` to the chain
--     for a dispel backlash TBC never had. The era clones carry TBC's numbers and this module's
--     `era_pri_vampiric_touch_tbc` AuraScript; the core backlash script is deliberately not rebound.
--   * MIND FLAY (node 20550) diverges on four, and the DAMAGE ENCODING ITSELF moved. TBC deals it
--     directly as `aura 3 SPELL_AURA_PERIODIC_DAMAGE` (25/42/62/87/110/142/176 per second for 3 s)
--     at RangeIndex 3 (20 yd) for flat mana; WotLK made e1 an empty DUMMY and moved the damage into
--     the triggered spell 58381, widened the range to 30 yd and converted rank 7 to ManaCostPct 9.
--
-- Node 20563 grants Vampiric Touch clone rank 1 (948060) and node 20550 grants Mind Flay clone rank 1
-- (948064). This file teaches the rest behind ReqAbility1 chains rooted at those grants.
-- IT DELETES NOTHING AND RE-GATES NOTHING.

-- =====================================================================================================
-- (a) THE STOCK VAMPIRIC TOUCH AND MIND FLAY TRAINER ROWS ARE LEFT COMPLETELY ALONE — and here is the
--     orphaned-higher-rank check that says they must be (docs/era-talents-framework.md "Deleting a
--     stock trainer row: the orphaned-higher-rank check (MANDATORY)"), plus the intent test that says
--     no deletion is warranted in the first place.
--
-- The stock TrainerId-11 chains, read off the live table 2026-09-05 (they match
-- _ref capstone_chains.vampiric_touch.live_trainer_rows / .mind_flay.live_trainer_rows exactly):
--     34916 ReqLevel 60 ReqAbility1 34914  MoneyCost 2300     <-- Vampiric Touch r2
--     34917 ReqLevel 70 ReqAbility1 34916  MoneyCost 2300     <-- r3
--     48159 ReqLevel 75 ReqAbility1 34917  MoneyCost 9000     <-- r4, WotLK-only
--     48160 ReqLevel 80 ReqAbility1 48159  MoneyCost 9000     <-- r5, WotLK-only
--   (34914 itself has NO `trainer_spell` row on ANY TrainerId.)
--     17311 ReqLevel 28 ReqAbility1 15407  MoneyCost  400     <-- Mind Flay r2
--     17312 ReqLevel 36 ReqAbility1 17311  MoneyCost  700     <-- r3
--     17313 ReqLevel 44 ReqAbility1 17312  MoneyCost 1200     <-- r4
--     17314 ReqLevel 52 ReqAbility1 17313  MoneyCost 1900     <-- r5
--     18807 ReqLevel 60 ReqAbility1 17314  MoneyCost 2300     <-- r6
--     25387 ReqLevel 68 ReqAbility1 18807  MoneyCost 6500     <-- r7
--     48155 ReqLevel 74 ReqAbility1 25387  MoneyCost 9000     <-- r8, WotLK-only
--     48156 ReqLevel 80 ReqAbility1 48155  MoneyCost 9000     <-- r9, WotLK-only
--   (15407 itself has NO `trainer_spell` row on ANY TrainerId.)
--
--   1. ORPHANED-HIGHER-RANK CHECK: each chain hangs off ReqAbility1 = its own rank 1. Deleting or
--      re-pointing 34916 would strand 34917 -> 48159 -> 48160, and deleting or re-pointing 17311
--      would strand 17312 -> 17313 -> 17314 -> 18807 -> 25387 -> 48155 -> 48156, for every NATIVE
--      WotLK priest (who never touches the era system). There is no compensating grant for any of
--      those ids in this module. That is exactly the regression the first revision of the druid
--      Mangle migration shipped and had to reverse (2026_08_31_03 section (a)), and the standing
--      lesson `deleting-a-stock-trainer-row-orphans-higher-ranks`.
--   2. THE DELETION WOULD ALSO BE POINTLESS, because both stock chains ALREADY self-gate against a
--      TBC-era priest — framework option 3, the preferred outcome. Each chain's root requirement is
--      its stock rank 1 (34914 / 15407), which the era node no longer grants (it grants clone 948060
--      / 948064) and which has no `trainer_spell` row of its own on any trainer, so a TBC priest can
--      never buy it either. `kBaselineSpellGates` does not force-grant either id (the only two priest
--      gate rows are Holy Nova 18121 and Divine Spirit 18113). EraTalents.cpp's CLASS_PRIEST TBC arms
--      (3) and (3b) additionally strip stock 34914/15407 and their trained ranks from a TBC priest,
--      readiness-gated on EraNodeReplacesStock, covering a character migrated from an older build.
--      Unable to satisfy ReqAbility1, a TBC priest cannot see or buy any stock rank.
--   3. TRAINER-GATING INTENT TEST (the Devouring Plague rule): a stock `trainer_spell` row is a
--      "hole" only if the module gates that spell somewhere and a missed set leaks it. Neither
--      Vampiric Touch nor Mind Flay is gated anywhere in this module other than by the node grant
--      itself, so there is no gate for a trainer row to leak past.
--      _ref capstone_chains.vampiric_touch.orphan_check and .mind_flay.orphan_check reach the same
--      "self-gating already; add clone rows and touch nothing stock" conclusion independently.
-- Net: no DELETE, no UPDATE, no re-gate on any stock row. Do not "tidy" this by adding one.

-- =====================================================================================================
-- (b) Vampiric Touch clone ranks 2-3 and Mind Flay clone ranks 2-7, trainer-taught, era-gated by a
--     require-previous-rank chain rooted at the TALENT-granted rank 1 (948060 / 948064).
--
-- Era gate: rank 1 of each chain exists ONLY as the TBC era-talent clone. A Vanilla-era priest has
-- neither talent (the Vanilla tree has no Vampiric Touch at all, and its Mind Flay node 18138 grants
-- stock 15407), and a WotLK priest trains the stock chains, so neither ever holds 948060 or 948064 —
-- requiring them via ReqAbility1 makes these ladders reachable ONLY by a TBC-era priest who spent the
-- talent. This is the talent-anchor flavour of the trainer gate documented in
-- docs/era-talents-framework.md ("Era-gating CUSTOM spell ranks that REPLACE a stock WotLK version",
-- anchor flavour 1) — no marker spell is needed, exactly as for the TBC Divine Spirit, Circle of
-- Healing, Lightwell, rogue Mutilate and druid Mangle / Nature's Grasp chains.
--
-- ReqLevel 60/70 and 28/36/44/52/60/68 are corroborated three ways: the wago 2.5.4 `SpellLevels` for
-- 34916/34917 and 17311/17312/17313/17314/18807/25387 (family J of the survival sweep, which found
-- ZERO TBC-vs-WotLK trainer-level drift anywhere in this class), the live stock rows above, and
-- _ref capstone_chains.vampiric_touch.tbc_levels / .mind_flay.tbc_levels. MoneyCost is copied
-- VERBATIM from the stock rows above.
--
-- A spell used as a `trainer_spell.ReqAbility1` MUST have a client `Spell.dbc` name row or the WotLK
-- trainer UI Lua-errors the whole window empty (framework "Client visibility"; the paladin 946078
-- bug). Every id referenced here — 948060 through 948062 and 948064 through 948070 — carries a
-- `client:` block in era-data/tbc/priest.yaml, so they all ship a client row in patch-V.mpq.
DELETE FROM `trainer_spell` WHERE `TrainerId`=11 AND `SpellId` IN (948061,948062,948065,948066,948067,948068,948069,948070);
INSERT INTO `trainer_spell`
  (`TrainerId`,`SpellId`,`MoneyCost`,`ReqSkillLine`,`ReqSkillRank`,`ReqAbility1`,`ReqAbility2`,`ReqAbility3`,`ReqLevel`,`VerifiedBuild`) VALUES
  (11, 948061, 2300, 0, 0, 948060, 0, 0, 60, 0),   -- Vampiric Touch Rank 2 (600 dmg): req talent-granted R1 948060
  (11, 948062, 2300, 0, 0, 948061, 0, 0, 70, 0),   -- Vampiric Touch Rank 3 (650 dmg): req R2 948061
  (11, 948065,  400, 0, 0, 948064, 0, 0, 28, 0),   -- Mind Flay      Rank 2 (126 dmg): req talent-granted R1 948064
  (11, 948066,  700, 0, 0, 948065, 0, 0, 36, 0),   -- Mind Flay      Rank 3 (186 dmg): req R2 948065
  (11, 948067, 1200, 0, 0, 948066, 0, 0, 44, 0),   -- Mind Flay      Rank 4 (261 dmg): req R3 948066
  (11, 948068, 1900, 0, 0, 948067, 0, 0, 52, 0),   -- Mind Flay      Rank 5 (330 dmg): req R4 948067
  (11, 948069, 2300, 0, 0, 948068, 0, 0, 60, 0),   -- Mind Flay      Rank 6 (426 dmg): req R5 948068
  (11, 948070, 6500, 0, 0, 948069, 0, 0, 68, 0);   -- Mind Flay      Rank 7 (528 dmg): req R6 948069

-- =====================================================================================================
-- (c) THREE `spell_ranks` chains: 948060 -> 948062 (Vampiric Touch r1-r3), 948064 -> 948070 (Mind Flay
--     r1-r7) and 948073 -> 948077 (the five Misery DEBUFF payloads).
--
-- The first two mirror the stock pair and do three jobs, all load-bearing here:
--   * the client spellbook collapses same-name / same-skill-line ranks (SkillLine 78 Shadow, from each
--     clone's `client.skillLine`) to the highest known, instead of showing three Vampiric Touch and
--     seven Mind Flay buttons;
--   * `Player::removeSpell` walks GetNextSpellInChain, so Reset()'s removeSpell(948060) /
--     removeSpell(948064) on respec CASCADES and strips every trained rank — the same respec-clean
--     property the Divine Spirit (948024-028), Circle of Healing (948046-050), Lightwell (948051-054),
--     Mutilate (947773-776) and Mangle (946258-260) chains rely on;
--   * a higher rank cleanly SUPERSEDES the lower for the bot AI's rank walks and for the client's
--     "next rank available" logic.
--
-- THE MISERY CHAIN IS A DIFFERENT KIND OF CHAIN AND IS THE ONE NEW THING IN THIS FILE. 948073-948077
-- are never learned, never trained and never in a spellbook — they are the debuff node 20562's
-- aura-109 effect puts on the priest's target. The chain exists solely so two priests of DIFFERENT
-- Misery rank cannot stack two unrelated ids that both carry `aura 87 MOD_DAMAGE_PERCENT_TAKEN`:
-- `Aura::CanStackWith` (SpellAuras.cpp:2071) tests `m_spellInfo->IsRankOf(existingSpellInfo)`, which
-- is `GetFirstRankSpell() == ...` — i.e. it reads `spell_ranks` — and refuses to stack two ranks of
-- one chain. Without it a 5-priest raid could pile +1%..+5% on one target simultaneously. This is the
-- TBC druid Improved Faerie Fire precedent verbatim (946277-946279,
-- 2026_08_31_05_era_talent_tbc_druid_iff_ranks.sql).
-- The Shadow Weaving payload needs no such chain: all five talent ranks trigger the SAME debuff id
-- (948078), and its cross-caster sharing is handled by `singleAuraStack: true` in the dataset
-- (SPELL_ATTR0_CU_SINGLE_AURA_STACK — one shared 5-stack per target).
--
-- RESPEC / BAND-MOVE STRIP: the Vampiric Touch and Mind Flay chains both root at a NODE GRANT, so no
-- extra reconcile arm is needed and none is added. A respec clears each chain through the
-- `spell_ranks` cascade above plus `StripOrphanedGrants` (which sees any node grant); an era-band move
-- out of TBC is covered by the same cascade for players (EraTransition runs Reset(from)) and by
-- EraTalentBots::FactoryReconcile's teardown-first managed-row strip for bots. The `ReqAbility1` chain
-- in section (b) is authoring-time gating only (who can BUY the next rank); it has no runtime removal
-- role — core `Player.cpp`'s `spell_required` walk only fires when
-- `GetTalentSpellCost(firstRankSpellId) > 0`, and DBCStores returns 0 for any id outside Talent.dbc,
-- which every id in these chains is. The Misery debuffs are never learned at all, so nothing strips
-- them and nothing needs to.
--
-- NOTE the interaction with `first_spell_id` = 948060 / 948064: both ARE plain node grants
-- (era_talent_rank) and NOT ReconcileBaselineSpells-managed rows, so these rank chains cannot fight
-- the reconcile. (Contrast the STOCK 34914/15407: had the era ranks been hung off a gate-managed
-- stock root, the gate's strip and the chain's cascade would have raced.) Identical situation to the
-- TBC Divine Spirit 948024, Circle of Healing 948046, rogue Mutilate 947773 and druid Nature's Grasp
-- 946270 chains.
-- SpellMgr::LoadSpellRanks requires every id in a chain to have a `spell_dbc` record; these do
-- (2026_09_05_01_era_talent_tbc_priest_custom_spells.sql, generated).
-- `spell_ranks.spell_id` is UNIQUE, so delete by first_spell_id before re-inserting.
DELETE FROM `spell_ranks` WHERE `first_spell_id` IN (948060, 948064, 948073);
INSERT INTO `spell_ranks` (`first_spell_id`,`spell_id`,`rank`) VALUES
  (948060, 948060, 1),
  (948060, 948061, 2),
  (948060, 948062, 3),
  (948064, 948064, 1),
  (948064, 948065, 2),
  (948064, 948066, 3),
  (948064, 948067, 4),
  (948064, 948068, 5),
  (948064, 948069, 6),
  (948064, 948070, 7),
  (948073, 948073, 1),
  (948073, 948074, 2),
  (948073, 948075, 3),
  (948073, 948076, 4),
  (948073, 948077, 5);

-- =====================================================================================================
-- (d) NO `spell_script_names` ROWS ARE ADDED BY THIS FILE, AND ONE OMISSION IS A DELIBERATE, MEASURED
--     CALL. (The dataset's own `scriptBindings:` DOES add three — `era_pri_vampiric_touch_tbc` on
--     948060/948061/948062 — but those ride the generated custom-spell SQL, not this migration.)
--
--   * VAMPIRIC TOUCH — the core script `spell_pri_vampiric_touch` (bound to `-34914`, i.e. the whole
--     stock rank chain) is NOT rebound onto the clones. Its entire body is the WotLK dispel backlash:
--     on AURA_REMOVE_BY_ENEMY_SPELL it casts 64085 on the dispeller. TBC's Vampiric Touch has no such
--     rider anywhere in its record, and 64085 is LIVE_ONLY (`_ref` triggers), so the ABSENCE of the
--     rebind is the era behaviour, not an oversight (spec section 4c, `_ref` script_census).
--   * MIND FLAY — `spell_script_names` has NO row for 15407 or any of 17311/17312/17313/17314/18807/
--     25387 on either side (verified on the live DB 2026-09-05), so there is nothing to rebind. The
--     WotLK damage carrier 58381 is likewise unscripted; it is simply not used by the clones.
--   * MISERY — the core's `spell_gen_proc_on_victim` is bound to `-33191` and is NOT rebound either.
--     That generic script exists to move an aura-42 PROC_TRIGGER_SPELL's cast from the caster onto the
--     victim; the era node ships as `aura 109 SPELL_AURA_ADD_TARGET_TRIGGER`, which
--     `Spell::DoTriggersOnSpellHit` (Spell.cpp:3260) already casts on the triggering spell's own
--     target. Rebinding it would attach a script to spells that never raise the proc it handles.
