-- TBC Hunter, Phase 7 Task 8 (Marksmanship tab). Aimed Shot ranks 2-7 and Trueshot Aura ranks 2-4
-- for a TBC-era hunter, plus the two `spell_ranks` chains the Marksmanship clones need.
-- HAND-WRITTEN (not generated) — same shape as 2026_09_06_03 (Survival). Idempotent:
-- DELETE-before-INSERT on every table.
--
-- This server loads trainers from the modern `trainer`/`trainer_spell` tables (ObjectMgr.cpp reads
-- "SELECT ... FROM trainer_spell"), NOT the legacy `npc_trainer` table. The MAIN hunter trainer is
-- `trainer.Id`=7 (Requirement=3 = CLASS_HUNTER) — the same id the stock Aimed Shot rows and this
-- module's Vanilla + TBC clone rows use. `trainer.Id`=8 is the five-spell starter trainer
-- (levels 2-6: 1494/1978/13163/1130/3044) and is untouched. `.reload trainer` (or a restart)
-- applies it live.
--
-- WHY THE CLONES EXIST (era-data/tbc/hunter.yaml helpers 948370-948380, nodes 20627 / 20637):
--   * Aimed Shot — TBC is a 2.5 s CAST on a 6 s cooldown dealing +70/125/200/330/460/600/870, with
--     a -50% healing debuff on the target for 10 s. Live is INSTANT on 10 s for +5/35/55/90/110/
--     150/205 — and that 10 s is not even in the DBC: it is forced at load by the family-9 +
--     word0-0x20000 sweep at SpellInfoCorrections.cpp:5311-5318. The clone chain carries the FREE
--     family-9 word2 bit 21 instead of that swept bit, so the sweep never sees it. All seven
--     ranks DIVERGED.
--   * Trueshot Aura — TBC is a FLAT +50/75/100/125 attack power to the PARTY within 45 yd over FOUR
--     ranks, free to cast; live is a single-rank +10% to the RAID. r1 DIVERGED, r2-r4 (20905/20906/
--     27066) are WAGO_ONLY — absent from 3.3.5a entirely.
--
-- This file adds ONLY the nine clone trainer rows and two `spell_ranks` chains. It deletes nothing.

-- =====================================================================================================
-- (a) THE STOCK TRAINER ROWS ARE LEFT COMPLETELY ALONE — and here is the orphaned-higher-rank check
--     that says they must be (docs/era-talents-framework.md "Deleting a stock trainer row: the
--     orphaned-higher-rank check (MANDATORY)"), plus the intent test that says no deletion is even
--     warranted. See also the standing lesson `deleting-a-stock-trainer-row-orphans-higher-ranks`.
--
-- The stock TrainerId-7 Aimed Shot chain is:
--     20900 ReqLevel 28 ReqAbility1 19434  MoneyCost 400
--     20901 ReqLevel 36 ReqAbility1 20900  MoneyCost 700
--     20902 ReqLevel 44 ReqAbility1 20901  MoneyCost 1300
--     20903 ReqLevel 52 ReqAbility1 20902  MoneyCost 2000
--     20904 ReqLevel 60 ReqAbility1 20903  MoneyCost 2500
--     27065 ReqLevel 70 ReqAbility1 20904  MoneyCost 10000
--     49049 ReqLevel 75 ReqAbility1 27065  MoneyCost 10000    <-- WotLK rank 8
--     49050 ReqLevel 80 ReqAbility1 49049  MoneyCost 10000    <-- WotLK rank 9
--
-- Trueshot Aura has NO stock trainer rows AT ALL: `SELECT * FROM trainer_spell WHERE SpellId IN
-- (19506,20905,20906,27066)` returns zero rows on TrainerId 7 or anywhere else, because live has a
-- single talent-granted rank. So there is nothing there to orphan, delete or re-gate.
--
--   1. ORPHANED-HIGHER-RANK CHECK (Aimed Shot): deleting 20900..27065 would strand 49049
--      (ReqAbility1 27065) and 49050 (ReqAbility1 49049). There is NO compensating grant for either
--      id anywhere in this module, so a deletion would leave every NATIVE WotLK hunter — who never
--      touches the era system — permanently stuck at Aimed Shot rank 1. That is exactly the
--      regression the first revision of the druid Mangle migration shipped and had to reverse.
--   2. THE DELETION WOULD ALSO BE POINTLESS: the stock chain ALREADY self-gates against a TBC-era
--      hunter. It roots at `20900.ReqAbility1 = 19434`, and stock 19434 is TALENT-granted, never
--      trainer-taught — node 20627 grants clone 948370 instead, while EraTalents.cpp's
--      readiness-gated `kEraHunTbcStockSwaps` strips a lingering 19434 (and its stock higher ranks)
--      from a TBC hunter. Verified there is no other path to stock 19434 for such a character: it
--      has no `trainer_spell` row of its own, it is not taught by an Effect-36 learn-wrapper on this
--      trainer (the `trainer-rows-can-be-learn-wrappers` trap; era_audit's wrapper sweep covers it),
--      and its SkillLineAbility row (10956, skill 163) carries AcquireMethod 0 so
--      `learnSkillRewardedSpells` never auto-teaches it. A TBC hunter therefore cannot satisfy
--      ReqAbility1 and cannot see or buy the stock ranks.
--   3. TRAINER-GATING INTENT TEST (the Devouring Plague rule): a stock `trainer_spell` row is only a
--      "hole" if the module gates that spell somewhere and a missed set leaks it.
--      mod-individual-progression gates neither ability, and the WotLK-only Aimed Shot ranks sit at
--      ReqLevel 75-80 — unreachable at the TBC level cap of 70 (_ref baseline_leaks records ZERO
--      leaks in this tree). So there is nothing to close.
-- Net: no DELETE, no UPDATE, no re-gate on any stock row. Do not "tidy" this by adding one.

-- =====================================================================================================
-- (b) Aimed Shot ranks 2-7 and Trueshot Aura ranks 2-4, trainer-taught, era-gated by a
--     require-previous-rank chain rooted at the TALENT-granted rank 1 (948370 / 948377).
--
-- Era gate: rank 1 of each chain exists ONLY as the TBC era-talent clone. A Vanilla-era hunter holds
-- the Vanilla clones (932952 / 932961) and a WotLK hunter holds the stock ids, so neither ever holds
-- 948370 or 948377 — requiring it via ReqAbility1 makes each ladder reachable ONLY by a TBC-era
-- hunter who spent the talent. This is the talent-anchor flavour of the trainer gate documented in
-- docs/era-talents-framework.md ("Era-gating CUSTOM spell ranks that REPLACE a stock WotLK version",
-- anchor flavour 1) — no marker spell is needed, exactly as for the Survival chains in _03.
--
-- AIMED SHOT ReqLevel and MoneyCost are copied VERBATIM from the stock rows above — the AUTHENTIC
-- costs, never ReqLevel x 1000 (commits 1286f34 / d1263c5). Those levels are independently
-- corroborated three ways: the wago 2.5.4 SpellLevels for 20900-20904/27065, the cached Wowhead TBC
-- tooltip for the chain, and the live stock trainer rows themselves.
--
-- TRUESHOT AURA IS THE ONE INTERPOLATED PRICE IN THE PHASE, and the reasoning is on the record
-- (spec Amendment A.9 / accepted gap 10). There are ZERO stock Trueshot trainer rows in any
-- expansion, so nothing can be copied. The levels are read from wago `SpellLevels` (BaseLevel ==
-- SpellLevel on every rank: 40/50/60/70; the cached TBC tooltip independently confirms r1's 45-yard
-- +50 AP). The costs come off TrainerId 7's TALENT-CHAIN sub-curve — which is far cheaper than the
-- baseline curve at the same level (at level 42 a baseline hunter rank costs 24000-26000 while
-- Counterattack rank 2 costs 1200), and Trueshot is talent-rooted:
--     L50 -> 1800: exactly one stock talent-rooted row sits at level 50 (24132 Wyvern Sting r2).
--     L60 -> 2500: TWO independent stock rows agree (20904 Aimed Shot r6 and 24133 Wyvern Sting r3).
--                  (19263 Deterrence @ 2200 is a flat talent-taught row, not a chain rank.)
--     L70 -> 5000: the two stock rows at level 70 DISAGREE — 27068 Wyvern Sting r4 @ 5000 versus
--                  27065 Aimed Shot r7 @ 10000. **5000 is a deliberate CHOICE**, taken because the
--                  Wyvern Sting chain is the only stock talent-rooted chain whose rank levels are
--                  exactly 50/60/70, so it is the single self-consistent series to copy. Aimed
--                  Shot's 10000 is the rejected alternative; recorded so it is not re-litigated.
-- The SAME series was back-ported to the shipped VANILLA rows in this commit
-- (2026_08_16_22_..._hunter_trainer_ranks.sql: 932962 8000 -> 1800, 932963 16000 -> 2500), because
-- those two figures came off the BASELINE curve and predate commits 1286f34 / d1263c5.
DELETE FROM `trainer_spell` WHERE `TrainerId`=7 AND `SpellId` IN
  (948371,948372,948373,948374,948375,948376,948378,948379,948380);
INSERT INTO `trainer_spell`
  (`TrainerId`,`SpellId`,`MoneyCost`,`ReqSkillLine`,`ReqSkillRank`,`ReqAbility1`,`ReqAbility2`,`ReqAbility3`,`ReqLevel`,`VerifiedBuild`) VALUES
  (7, 948371,   400, 0, 0, 948370, 0, 0, 28, 0),   -- Aimed Shot Rank 2 (115 mana, +125): req talent-granted R1 948370
  (7, 948372,   700, 0, 0, 948371, 0, 0, 36, 0),   -- Aimed Shot Rank 3 (160 mana, +200): req R2 948371
  (7, 948373,  1300, 0, 0, 948372, 0, 0, 44, 0),   -- Aimed Shot Rank 4 (210 mana, +330): req R3 948372
  (7, 948374,  2000, 0, 0, 948373, 0, 0, 52, 0),   -- Aimed Shot Rank 5 (260 mana, +460): req R4 948373
  (7, 948375,  2500, 0, 0, 948374, 0, 0, 60, 0),   -- Aimed Shot Rank 6 (310 mana, +600): req R5 948374
  (7, 948376, 10000, 0, 0, 948375, 0, 0, 70, 0),   -- Aimed Shot Rank 7 (370 mana, +870): req R6 948375
  (7, 948378,  1800, 0, 0, 948377, 0, 0, 50, 0),   -- Trueshot Aura Rank 2 (+75 AP): req talent-granted R1 948377
  (7, 948379,  2500, 0, 0, 948378, 0, 0, 60, 0),   -- Trueshot Aura Rank 3 (+100 AP): req R2 948378
  (7, 948380,  5000, 0, 0, 948379, 0, 0, 70, 0);   -- Trueshot Aura Rank 4 (+125 AP): req R3 948379

-- =====================================================================================================
-- (c) The two `spell_ranks` chains, both rooted at the NODE-GRANTED rank 1. Each does the usual four
--     jobs:
--       - the client spellbook collapses the same-name / same-skill-line (163 Marksmanship) ranks to
--         the highest known instead of showing seven (or four) separate buttons;
--       - `Player::removeSpell` walks GetNextSpellInChain, so Reset()'s removeSpell(948370 / 948377)
--         on respec CASCADES and strips every trained rank — the same respec-clean property the
--         Survival chains (948330/948338), the TBC rogue Mutilate chain (947773-776) and the Vanilla
--         Aimed Shot chain (932952-957) rely on. Both chains are rooted at a NODE GRANT, not at a
--         trainer-taught id, so lesson 21's arm-2b problem (a chain nothing strips on respec) does
--         not arise here;
--       - a higher rank cleanly supersedes the lower for the bot AI's rank walks and for the
--         client's "next rank available" logic;
--       - `SpellInfo::IsRankOf` sees them as one chain, so two ranks of the Aimed Shot healing
--         debuff cannot stack on one target.
--
-- The core's SpellMgr::LoadSpellRanks requires every id in a chain to have a spell_dbc record, which
-- these custom era rows do (2026_09_06_01_era_talent_tbc_hunter_custom_spells.sql, generated).
-- `spell_ranks.spell_id` is UNIQUE, so delete by first_spell_id before re-inserting.
DELETE FROM `spell_ranks` WHERE `first_spell_id` IN (948370,948377);
INSERT INTO `spell_ranks` (`first_spell_id`,`spell_id`,`rank`) VALUES
  (948370, 948370, 1),
  (948370, 948371, 2),
  (948370, 948372, 3),
  (948370, 948373, 4),
  (948370, 948374, 5),
  (948370, 948375, 6),
  (948370, 948376, 7),
  (948377, 948377, 1),
  (948377, 948378, 2),
  (948377, 948379, 3),
  (948377, 948380, 4);
