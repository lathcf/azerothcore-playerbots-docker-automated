-- Vanilla Paladin seal/Judgement/Lay-on-Hands rework v2 (2026-08-20): TRAINER-TAUGHT, not
-- auto-granted. A real-client test of the 2026-08-20 seal reconstruction (2026_08_19_30's
-- companion C++ Task 9) found the wrong obtain-model: every custom seal/Judgement/LoH rank was
-- AUTO-GRANTED by level (ReconcileBaselineSpells) while the stock WotLK seals/judgement-buttons/
-- LoH stayed trainable, so a Vanilla paladin ended up with BOTH sets cluttering the spellbook and
-- trainer window. Fix: every custom rank is now trainer-taught behind a ReqAbility1 chain rooted at
-- the hidden marker 932749 ("Seal Training", era-data/vanilla/paladin.yaml helpers:) — a WotLK
-- paladin never has 932749, so the whole chain is unreachable for them. The stock WotLK trainer
-- rows for the SAME abilities are deleted below (so a Vanilla paladin can no longer buy the WotLK
-- version by mistake); EraTalents.cpp's CLASS_PALADIN reconcile block auto-grants those stock ids
-- by level to WotLK paladins instead (their trainer rows are gone, so a straight trainer buy is no
-- longer possible for them either — this is a version SWAP of the obtain method, not a removal).
-- Mirrors the 2026_08_19_30 file's header/style exactly. HAND-WRITTEN. Idempotent: DELETE-before-INSERT.
--
-- This server loads trainers from the modern `trainer`/`trainer_spell` tables (ObjectMgr.cpp), NOT
-- the legacy `npc_trainer`. The GENERAL paladin trainers are `trainer.Id` 3, 4, 5 (Requirement=2 =
-- paladin, ~171 spells each; Id 6 is a 5-spell specialized trainer). Rows are added to/removed from
-- all three so any paladin can train regardless of faction/race trainer.
--
-- NOT touched here: stock 20154 (Seal of Righteousness) and 20271 (Judgement, née "Judgement of
-- Light") are NOT trainer_spell rows at all on this core (confirmed: no row for either id on
-- TrainerId 3/4/5, nor anywhere else) — WotLK auto-learns both directly, so there is nothing to
-- delete; EraTalents.cpp still re-grants them by level on an era transition for safety.
--
-- (b2) below is a LATER ADDITION (Seal of Command 5-rank Vanilla reconstruction) appended to this
-- same file: SoC is a Ret TALENT (node 18637), not one of the marker-rooted baseline seals above,
-- so it deliberately does NOT gate on 932749 — see (b2)'s own comment.

-- (a) Strip the stock WotLK trainer rows for every ability this rework version-swaps. Stock
--     20154/20271 are deliberately excluded (see note above -- they are not trainer rows).
DELETE FROM `trainer_spell` WHERE `TrainerId` IN (3,4,5) AND `SpellId` IN
  (20164,20165,20166,53407,53408,633,2800,10310,27154);

-- (b) Custom seal/Judgement/LoH rank-1 rows gate on the marker 932749; every higher rank gates on
--     the previous rank (ReqAbility1), so the whole chain is reachable only by training in order.
--     MoneyCost follows the AUTHENTIC paladin trainer level->cost curve (anchored on the stock
--     3.3.5a trainer_spell rows for Blessing of Might/Wisdom, Seal of Justice/Light/Wisdom, and
--     Lay on Hands -- paladin costs run ~1.4x the shaman-totem curve). REPLACES the earlier
--     `level * 1000` curve, which over-priced low ranks ~10x (Seal of Command r1 @L6 cost 60s
--     instead of the ~2s authentic). DELETE first (PRIMARY KEY is (TrainerId,SpellId),
--     so a bare re-run of this file would otherwise duplicate-key-fail on db-import).
DELETE FROM `trainer_spell` WHERE `TrainerId` IN (3,4,5) AND `SpellId` IN
  (932700,932701,932702,932703,932704,932705,932706,932707,
   932724,
   932725,932726,932727,932728,
   932737,932738,932739,
   932634,932635,932636,932637,932638,932639,
   932746,
   932668,932669,932670);
INSERT INTO `trainer_spell`
  (`TrainerId`,`SpellId`,`MoneyCost`,`ReqSkillLine`,`ReqSkillRank`,`ReqAbility1`,`ReqAbility2`,`ReqAbility3`,`ReqLevel`,`VerifiedBuild`) VALUES
  -- Seal of Righteousness r1-8 (932700-707), rooted at the marker.
  (3, 932700,  10, 0, 0, 932749, 0, 0,  1, 0),
  (3, 932701, 600, 0, 0, 932700, 0, 0, 10, 0),
  (3, 932702, 3000, 0, 0, 932701, 0, 0, 18, 0),
  (3, 932703, 6000, 0, 0, 932702, 0, 0, 26, 0),
  (3, 932704, 13000, 0, 0, 932703, 0, 0, 34, 0),
  (3, 932705, 21000, 0, 0, 932704, 0, 0, 42, 0),
  (3, 932706, 28000, 0, 0, 932705, 0, 0, 50, 0),
  (3, 932707, 44000, 0, 0, 932706, 0, 0, 58, 0),
  (4, 932700,  10, 0, 0, 932749, 0, 0,  1, 0),
  (4, 932701, 600, 0, 0, 932700, 0, 0, 10, 0),
  (4, 932702, 3000, 0, 0, 932701, 0, 0, 18, 0),
  (4, 932703, 6000, 0, 0, 932702, 0, 0, 26, 0),
  (4, 932704, 13000, 0, 0, 932703, 0, 0, 34, 0),
  (4, 932705, 21000, 0, 0, 932704, 0, 0, 42, 0),
  (4, 932706, 28000, 0, 0, 932705, 0, 0, 50, 0),
  (4, 932707, 44000, 0, 0, 932706, 0, 0, 58, 0),
  (5, 932700,  10, 0, 0, 932749, 0, 0,  1, 0),
  (5, 932701, 600, 0, 0, 932700, 0, 0, 10, 0),
  (5, 932702, 3000, 0, 0, 932701, 0, 0, 18, 0),
  (5, 932703, 6000, 0, 0, 932702, 0, 0, 26, 0),
  (5, 932704, 13000, 0, 0, 932703, 0, 0, 34, 0),
  (5, 932705, 21000, 0, 0, 932704, 0, 0, 42, 0),
  (5, 932706, 28000, 0, 0, 932705, 0, 0, 50, 0),
  (5, 932707, 44000, 0, 0, 932706, 0, 0, 58, 0),
  -- Seal of Justice (932724, single rank), rooted at the marker.
  (3, 932724, 4000, 0, 0, 932749, 0, 0, 22, 0),
  (4, 932724, 4000, 0, 0, 932749, 0, 0, 22, 0),
  (5, 932724, 4000, 0, 0, 932749, 0, 0, 22, 0),
  -- Seal of Light r1-4 (932725-728), rooted at the marker.
  (3, 932725, 11000, 0, 0, 932749, 0, 0, 30, 0),
  (3, 932726, 18000, 0, 0, 932725, 0, 0, 40, 0),
  (3, 932727, 28000, 0, 0, 932726, 0, 0, 50, 0),
  (3, 932728, 46000, 0, 0, 932727, 0, 0, 60, 0),
  (4, 932725, 11000, 0, 0, 932749, 0, 0, 30, 0),
  (4, 932726, 18000, 0, 0, 932725, 0, 0, 40, 0),
  (4, 932727, 28000, 0, 0, 932726, 0, 0, 50, 0),
  (4, 932728, 46000, 0, 0, 932727, 0, 0, 60, 0),
  (5, 932725, 11000, 0, 0, 932749, 0, 0, 30, 0),
  (5, 932726, 18000, 0, 0, 932725, 0, 0, 40, 0),
  (5, 932727, 28000, 0, 0, 932726, 0, 0, 50, 0),
  (5, 932728, 46000, 0, 0, 932727, 0, 0, 60, 0),
  -- Seal of Wisdom r1-3 (932737-739), rooted at the marker.
  (3, 932737, 16000, 0, 0, 932749, 0, 0, 38, 0),
  (3, 932738, 26000, 0, 0, 932737, 0, 0, 48, 0),
  (3, 932739, 44000, 0, 0, 932738, 0, 0, 58, 0),
  (4, 932737, 16000, 0, 0, 932749, 0, 0, 38, 0),
  (4, 932738, 26000, 0, 0, 932737, 0, 0, 48, 0),
  (4, 932739, 44000, 0, 0, 932738, 0, 0, 58, 0),
  (5, 932737, 16000, 0, 0, 932749, 0, 0, 38, 0),
  (5, 932738, 26000, 0, 0, 932737, 0, 0, 48, 0),
  (5, 932739, 44000, 0, 0, 932738, 0, 0, 58, 0),
  -- Seal of the Crusader r1-6 (932634-639) — was grant-by-level (Task 9's kSotcSealRanks), now
  -- trainer-taught like every other seal, rooted at the marker.
  (3, 932634,  200, 0, 0, 932749, 0, 0,  6, 0),
  (3, 932635, 1000, 0, 0, 932634, 0, 0, 12, 0),
  (3, 932636, 4000, 0, 0, 932635, 0, 0, 22, 0),
  (3, 932637, 12000, 0, 0, 932636, 0, 0, 32, 0),
  (3, 932638, 21000, 0, 0, 932637, 0, 0, 42, 0),
  (3, 932639, 34000, 0, 0, 932638, 0, 0, 52, 0),
  (4, 932634,  200, 0, 0, 932749, 0, 0,  6, 0),
  (4, 932635, 1000, 0, 0, 932634, 0, 0, 12, 0),
  (4, 932636, 4000, 0, 0, 932635, 0, 0, 22, 0),
  (4, 932637, 12000, 0, 0, 932636, 0, 0, 32, 0),
  (4, 932638, 21000, 0, 0, 932637, 0, 0, 42, 0),
  (4, 932639, 34000, 0, 0, 932638, 0, 0, 52, 0),
  (5, 932634,  200, 0, 0, 932749, 0, 0,  6, 0),
  (5, 932635, 1000, 0, 0, 932634, 0, 0, 12, 0),
  (5, 932636, 4000, 0, 0, 932635, 0, 0, 22, 0),
  (5, 932637, 12000, 0, 0, 932636, 0, 0, 32, 0),
  (5, 932638, 21000, 0, 0, 932637, 0, 0, 42, 0),
  (5, 932639, 34000, 0, 0, 932638, 0, 0, 52, 0),
  -- Judgement (932746, single rank, the keystone button), rooted at the marker.
  (3, 932746,  100, 0, 0, 932749, 0, 0,  4, 0),
  (4, 932746,  100, 0, 0, 932749, 0, 0,  4, 0),
  (5, 932746,  100, 0, 0, 932749, 0, 0,  4, 0),
  -- Lay on Hands r1-3 (932668-670), rooted at the marker.
  (3, 932668, 600, 0, 0, 932749, 0, 0, 10, 0),
  (3, 932669, 11000, 0, 0, 932668, 0, 0, 30, 0),
  (3, 932670, 28000, 0, 0, 932669, 0, 0, 50, 0),
  (4, 932668, 600, 0, 0, 932749, 0, 0, 10, 0),
  (4, 932669, 11000, 0, 0, 932668, 0, 0, 30, 0),
  (4, 932670, 28000, 0, 0, 932669, 0, 0, 50, 0),
  (5, 932668, 600, 0, 0, 932749, 0, 0, 10, 0),
  (5, 932669, 11000, 0, 0, 932668, 0, 0, 30, 0),
  (5, 932670, 28000, 0, 0, 932669, 0, 0, 50, 0);

-- (b2) Seal of Command R2-5 (932750-753) — UNLIKE the marker-rooted seals in (b) above, SoC is
--     a Ret TALENT (node 18637) that grants clone RANK 1 (932606) directly; it is NOT gated on the
--     932749 marker. Higher ranks are trainer-taught behind a ReqAbility1 chain off the
--     talent-granted rank 1, mirroring the Holy Shock R2/R3 / Holy Shield R2-4 / Blessing of
--     Sanctuary R2-4 pattern in 2026_08_19_30 (that file's pattern, not this file's marker pattern)
--     — so a WotLK paladin, or a Vanilla paladin who never took the SoC talent, never has 932606
--     and the whole chain is unreachable. 1.12.1 levels 30/40/50/60; MoneyCost curve like the
--     other seals in this file. EraBandClassifier::Sweep owns the strip (rule 2: a `spell_ranks`
--     rank above a node-granted r1), so R2-5 go when a Vanilla paladin no longer holds the talent.
--     The per-class EraTalents.cpp `kPalTrainedChains` arm this header used to name is RETIRED.
DELETE FROM `trainer_spell` WHERE `TrainerId` IN (3,4,5) AND `SpellId` IN
  (932750,932751,932752,932753);
INSERT INTO `trainer_spell`
  (`TrainerId`,`SpellId`,`MoneyCost`,`ReqSkillLine`,`ReqSkillRank`,`ReqAbility1`,`ReqAbility2`,`ReqAbility3`,`ReqLevel`,`VerifiedBuild`) VALUES
  (3, 932750, 11000, 0, 0, 932606, 0, 0, 30, 0),
  (3, 932751, 18000, 0, 0, 932750, 0, 0, 40, 0),
  (3, 932752, 28000, 0, 0, 932751, 0, 0, 50, 0),
  (3, 932753, 46000, 0, 0, 932752, 0, 0, 60, 0),
  (4, 932750, 11000, 0, 0, 932606, 0, 0, 30, 0),
  (4, 932751, 18000, 0, 0, 932750, 0, 0, 40, 0),
  (4, 932752, 28000, 0, 0, 932751, 0, 0, 50, 0),
  (4, 932753, 46000, 0, 0, 932752, 0, 0, 60, 0),
  (5, 932750, 11000, 0, 0, 932606, 0, 0, 30, 0),
  (5, 932751, 18000, 0, 0, 932750, 0, 0, 40, 0),
  (5, 932752, 28000, 0, 0, 932751, 0, 0, 50, 0),
  (5, 932753, 46000, 0, 0, 932752, 0, 0, 60, 0);

-- (c) spell_ranks chains: collapse same-name ranks in the client spellbook and make Reset's
--     removeSpell(rank1) CASCADE via GetNextSpellInChain (matters once a rank is trainer-taught
--     rather than talent-granted: the marker strip below removes rank 1, and the chain removes the
--     rest). SoJ (932724) and Judgement (932746) are single-rank -> no spell_ranks row. LoH
--     (932668) already has a chain from 2026_08_19_30 -- re-asserted here (same rows) since this
--     file re-derives the full trainer-taught set; harmless idempotent overlap. SoC (932606) is
--     rooted at the talent-granted rank 1 per (b2) above, NOT the marker.
DELETE FROM `spell_ranks` WHERE `first_spell_id` IN (932700, 932725, 932737, 932634, 932668, 932606);
INSERT INTO `spell_ranks` (`first_spell_id`,`spell_id`,`rank`) VALUES
  -- Seal of Righteousness
  (932700, 932700, 1),
  (932700, 932701, 2),
  (932700, 932702, 3),
  (932700, 932703, 4),
  (932700, 932704, 5),
  (932700, 932705, 6),
  (932700, 932706, 7),
  (932700, 932707, 8),
  -- Seal of Light
  (932725, 932725, 1),
  (932725, 932726, 2),
  (932725, 932727, 3),
  (932725, 932728, 4),
  -- Seal of Wisdom
  (932737, 932737, 1),
  (932737, 932738, 2),
  (932737, 932739, 3),
  -- Seal of the Crusader
  (932634, 932634, 1),
  (932634, 932635, 2),
  (932634, 932636, 3),
  (932634, 932637, 4),
  (932634, 932638, 5),
  (932634, 932639, 6),
  -- Lay on Hands
  (932668, 932668, 1),
  (932668, 932669, 2),
  (932668, 932670, 3),
  -- Seal of Command (talent-granted R1, trainer-taught R2-5)
  (932606, 932606, 1),
  (932606, 932750, 2),
  (932606, 932751, 3),
  (932606, 932752, 4),
  (932606, 932753, 5);
