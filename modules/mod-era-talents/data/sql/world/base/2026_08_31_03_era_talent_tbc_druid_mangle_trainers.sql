-- TBC Druid, Phase 2 Task 7 (Feral Combat tab). Mangle ranks 2-3, for BOTH forms, for a TBC-era druid.
-- HAND-WRITTEN (not generated) — mirrors the TBC paladin active-rank chains
-- (2026_08_27_02_era_talent_tbc_paladin_seal_substrate.sql) and the sibling TBC druid Nature's Grasp
-- chain (2026_08_31_02). Idempotent: DELETE-before-INSERT.
--
-- This server loads trainers from the modern `trainer`/`trainer_spell` tables (ObjectMgr.cpp reads
-- `trainer_spell`), NOT the legacy `npc_trainer` table. The DRUID trainer is `trainer.Id`=33.
-- `.reload trainer` (or a restart) applies it live.
--
-- WHY THE CLONES EXIST (era-data/tbc/druid.yaml helpers 946258-946263, node 20141):
--   * all six ranks: DurationIndex 29 (12000 ms) — TBC's bleed-amplify window; live uses index 3
--     (60000 ms). This is the real TBC divergence (_ref.non_effect_divergences 33876).
--   * Cat only: EFFECT_2 SPELL_EFFECT_WEAPON_PERCENT_DAMAGE basePoints 159 (=160%) vs live 199
--     (=200%). Mangle (Bear) is 115% in BOTH eras and is untouched.
-- Everything else (EFFECT_0 weapon damage, the aura-255/MECHANIC_BLEED +30% carrier, the 200-rage /
-- 45-energy costs, ShapeshiftMask, the Bear 6 s category cooldown, the family identity) is inherited
-- from the stock templates.
--
-- This file adds ONLY the four clone trainer rows and their two spell_ranks chains. It deletes nothing:
-- see section (a) for why the stock rank-2/3 rows must stay.

-- =====================================================================================================
-- (a) The four STOCK Mangle rank-2/3 trainer rows are LEFT IN PLACE — and RESTORED here if an earlier
--     revision of this file removed them.
--
-- HISTORY (read before "re-simplifying" this): the first revision of this migration DELETEd
-- 33982/33983/33986/33987 from TrainerId 33, following _ref.baseline_leaks' `delete_for_era`
-- disposition and the Nature's Grasp / paladin precedents. That was WRONG on two counts:
--
--   1. It was a live regression for NATIVE WotLK druids, who do not use the era system at all. The
--      stock chain continues past rank 3: 48563 (Mangle (Bear) r4, L75) has ReqAbility1 = 33987 and
--      48565 (Mangle (Cat) r4, L75) has ReqAbility1 = 33983. Deleting r2/r3 therefore stranded every
--      native druid at Mangle rank 1 — r4/r5 became unbuyable too — and unlike the Nature's Grasp
--      deletion there is NO compensating reconcile grant for these ids anywhere in the module.
--   2. It was unnecessary. The stock chain is ALREADY self-gating against an era character: stock r2
--      requires stock r1 as ReqAbility1 (33986 <- 33878, 33982 <- 33876), and a TBC druid is granted
--      946258 / 946261 and never 33878 / 33876. Verified there is no other path to the stock rank 1:
--      it has no trainer_spell row of its own (it is taught only by the 33917 SPELL_EFFECT_LEARN_SPELL
--      wrapper, which only the NATIVE talent path _addTalentAurasAndSpells can process), and its
--      SkillLineAbility AcquireMethod is 0, so learnSkillRewardedSpells never auto-teaches it either.
--      An era druid therefore cannot satisfy the requirement and cannot see or buy the stock ranks —
--      exactly the property the deletion was reaching for.
--
-- The restore below is idempotent and correct in both directions: on a fresh install the base
-- data/sql/base/db_world/trainer_spell.sql already carries these four rows and this is a no-op
-- rewrite of identical values; on a database that applied the earlier revision it puts them back.
-- Values are copied verbatim from that base file.
DELETE FROM `trainer_spell` WHERE `TrainerId`=33 AND `SpellId` IN (33982,33983,33986,33987);
INSERT INTO `trainer_spell`
  (`TrainerId`,`SpellId`,`MoneyCost`,`ReqSkillLine`,`ReqSkillRank`,`ReqAbility1`,`ReqAbility2`,`ReqAbility3`,`ReqLevel`,`VerifiedBuild`) VALUES
  (33, 33986, 1700, 0, 0, 33878, 0, 0, 58, 0),   -- Mangle (Bear) Rank 2: req stock r1 33878
  (33, 33987, 1900, 0, 0, 33986, 0, 0, 68, 0),   -- Mangle (Bear) Rank 3: req stock r2 33986 (48563 r4 chains off this)
  (33, 33982, 1700, 0, 0, 33876, 0, 0, 58, 0),   -- Mangle (Cat)  Rank 2: req stock r1 33876
  (33, 33983, 1700, 0, 0, 33982, 0, 0, 68, 0);   -- Mangle (Cat)  Rank 3: req stock r2 33982 (48565 r4 chains off this)

-- =====================================================================================================
-- (b) Mangle clone ranks 2-3 for both forms, trainer-taught, era-gated by a require-previous-rank chain
--     rooted at the TALENT-granted rank 1 (946258 Bear / 946261 Cat, TBC node 20141).
--
-- Era gate: rank 1 of each chain exists ONLY as the TBC era-talent clone. Neither a Vanilla-era druid
-- (no Mangle at all — the talent does not exist in that tree) nor a WotLK druid (stock 33878/33876)
-- ever holds 946258/946261, so requiring it via ReqAbility1 makes the r2->r3 chain reachable ONLY by a
-- TBC-era druid who spent the talent. This is the talent-anchor flavor documented in
-- docs/era-talents-framework.md ("Era-gating CUSTOM spell ranks that REPLACE a stock WotLK version",
-- anchor flavor 1) — no marker spell is needed.
--
-- HOW THE TWO RANK-1 ANCHORS ARE OBTAINED: era_talent_rank stores ONE grantedSpellId per
-- (talentId, rank), so node 20141 grants only Mangle (Bear) r1 946258; Mangle (Cat) r1 946261 is
-- handed out (and stripped) by EraTalents.cpp's CLASS_DRUID reconcile arm (6) `kEraDruidTbcPaired`.
-- Both anchors therefore exist exactly when the talent is spent, which is all these rows require.
--
-- ReqLevel and MoneyCost are copied verbatim from the stock rows deleted above (58/68; 1700/1700/1700/1900).
DELETE FROM `trainer_spell` WHERE `TrainerId`=33 AND `SpellId` IN (946259,946260,946262,946263);
INSERT INTO `trainer_spell`
  (`TrainerId`,`SpellId`,`MoneyCost`,`ReqSkillLine`,`ReqSkillRank`,`ReqAbility1`,`ReqAbility2`,`ReqAbility3`,`ReqLevel`,`VerifiedBuild`) VALUES
  (33, 946259, 1700, 0, 0, 946258, 0, 0, 58, 0),   -- Mangle (Bear) Rank 2: req talent-granted R1 946258
  (33, 946260, 1900, 0, 0, 946259, 0, 0, 68, 0),   -- Mangle (Bear) Rank 3: req R2 946259
  (33, 946262, 1700, 0, 0, 946261, 0, 0, 58, 0),   -- Mangle (Cat)  Rank 2: req talent-granted R1 946261
  (33, 946263, 1700, 0, 0, 946262, 0, 0, 68, 0);   -- Mangle (Cat)  Rank 3: req R2 946262

-- =====================================================================================================
-- (c) spell_ranks chains, one per form. Makes R1..R3 a real rank chain so the client spellbook collapses
--     the same-name ranks to the highest known, higher-rank casts overwrite the lower debuff cleanly,
--     and Reset's removeSpell(rank 1) CASCADES via GetNextSpellInChain to strip the trained ranks on
--     respec. first_spell_id is the talent-granted rank 1. 946258 is a plain node grant
--     (era_talent_rank), exactly like the Vanilla 932505 / TBC 946270 Nature's Grasp chains. 946261 is
--     reconcile-managed (arm 6 above), which is also safe here: that arm's grant is rank-exact and its
--     removeSpell(946261) is what CASCADES 946262/946263, so the chain is the strip mechanism rather
--     than something the reconcile can fight.
DELETE FROM `spell_ranks` WHERE `first_spell_id` IN (946258,946261);
INSERT INTO `spell_ranks` (`first_spell_id`,`spell_id`,`rank`) VALUES
  (946258, 946258, 1),
  (946258, 946259, 2),
  (946258, 946260, 3),
  (946261, 946261, 1),
  (946261, 946262, 2),
  (946261, 946263, 3);
