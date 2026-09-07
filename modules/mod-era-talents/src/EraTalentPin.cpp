#include "EraTalents.h"
#include "EraTransition.h"
#include "EraTalentsComms.h"
#include "EraTalentsConfig.h"
#include "EraTalentIP.h"
#include "ScriptMgr.h"
#include "Player.h"
#include "Pet.h"
#include "ObjectAccessor.h"
#include "World.h"           // sWorld / CONFIG_NO_RESET_TALENT_COST (trainer-reset hook)
#include "Playerbots.h"      // GET_PLAYERBOT_AI — era talents are a HUMAN-player feature
#include "EraTalentBots.h"   // bot-only era paths (players keep the pipeline below)
#include "EraSoulLink.h"     // EraSoulLink::kPairs — shared with era_soul_link/_relink (EraTalentProcScripts.cpp)

#include <mutex>
#include <unordered_map>
#include <unordered_set>

// Master Demonologist (EraTalentPetScripts.cpp): re-scale the level-dependent Felhunter resist branch
// on level-up, since the tracker otherwise only fires on demon summon.
void EraMasterDemonologist_OnLevelUp(Player* wl);

namespace
{
    // Real players only: bots are out of scope for era talents. Skipping them avoids the pin
    // touching a bot's factory talents and — critically — stops a bot that crosses an era band
    // (e.g. via mod-raid-roster's era-sync) from hitting resetTalents and losing its talents.
    // The SESSION flag is mandatory here: PlayerbotHolder attaches the bot AI only AFTER core
    // login completes, so GET_PLAYERBOT_AI is null for every bot inside OnPlayerLogin — the
    // player branch then ran for every bot login (EraFromIP reads bots as Vanilla), stripping
    // native talents from 61+ bots and reconciling them against the wrong era (91 bots
    // stripped in one boot, live 2026-08-26). mod-playerbots' own OnPlayerLogin script guards
    // with the same session test for exactly this reason.
    bool IsBot(Player* p) { return !p || p->GetSession()->IsBot() || GET_PLAYERBOT_AI(p) != nullptr; }

    // Per-player throttle for the mid-session era check (see OnPlayerUpdate). Holds real players
    // only (bots are skipped before this is touched); cleared on logout. Mutex-guarded:
    // OnPlayerUpdate runs inside Map::Update, and players on DIFFERENT maps tick on parallel
    // MapUpdater threads — an unlocked shared map here is the same rehash race that segfaulted
    // prod via EraTalentBots' resolve cache (2026-08-26); few players kept the odds low, not zero.
    std::unordered_map<ObjectGuid, uint32> g_checkTimer;
    std::mutex g_checkTimerMutex;
    constexpr uint32 kCheckIntervalMs = 2000;

    // Two-Handed Axes and Maces (node 18822) grants proficiency clone 932406; the weapon-gate hook
    // below reads it via HasSpell to decide whether a Vanilla shaman may equip a 2H axe/mace.
    constexpr uint32 SPELL_ERA_TWO_HANDED_AXES_MACES = 932406;
}

class era_talent_pin : public PlayerScript
{
public:
    era_talent_pin() : PlayerScript("era_talent_pin") {}

    void OnPlayerLogin(Player* p) override
    {
        if (!sEraTalentsConfig->Enabled())
            return;
        if (IsBot(p))
        {
            EraTalentBots::OnBotLogin(p);   // gated internally on EraTalents.BotTalents
            return;
        }
        EraId stored = EraTransition::StoredEra(p);
        EraId now = EraFromIP(p);
        EraTransition::Run(p, stored, now);          // resets ONLY on a crossing; always persists the stamp
        if (now == stored && EraHasTalentTrees(now))
        {
            EraTransition::StripNativeTalents(p, now); // pre-mod chars: first sighting stamped with no reset
            EraTalents::ReapplyOnLogin(p, now);      // re-grant persisted passives after a plain login
        }
        EraTalents::ReconcileBaselineSpells(p, now); // unconditional: must also run for WotLK priests
        pin(p, now);
    }

    // Mid-session era changes that are NOT level-ups (a GM `.ip set`, or a progression milestone
    // like a boss kill) aren't caught by the login/level hooks. Poll cheaply per real player and
    // reconcile within a couple seconds so the talents transition without a relog.
    void OnPlayerUpdate(Player* p, uint32 diff) override
    {
        if (!sEraTalentsConfig->Enabled() || IsBot(p) || !p->IsInWorld())
            return;
        {
            std::lock_guard<std::mutex> guard(g_checkTimerMutex);
            uint32& acc = g_checkTimer[p->GetGUID()];
            acc += diff;
            if (acc < kCheckIntervalMs)
                return;
            acc = 0;   // reference dies with this scope — nothing escapes the lock
        }
        EraTransition::Detect(p);                    // Run (in-combat: defers, flushed on leave-combat)

        // The core re-teaches skill-reward auto-learn seals (e.g. Seal of Righteousness 20154 on skill
        // 594) every login; the login-time reconcile strips them server-side, but removeSpell can't
        // notify the client during PlayerLoading (Player.cpp:3388), leaving a phantom spellbook icon.
        // Re-strip them here in-world (packets flow now) so the client updates. Fires once per login
        // (once stripped, the core doesn't re-teach until the next login), then no-ops.
        // A TBC paladin also runs the reconstructed seal substrate (reused Vanilla clones), so it too
        // must clear the core's re-taught stock-seal phantoms. Bots return above (IsBot), so EraFromIP
        // is the correct era source on this player-only path.
        if (p->getClass() == CLASS_PALADIN && (EraFromIP(p) == ERA_VANILLA || EraFromIP(p) == ERA_TBC))
            for (uint32 s : { 20154u, 20164u, 20165u, 20166u, 20271u, 53407u, 53408u, 633u, 2800u, 10310u, 27154u })
                if (p->HasSpell(s))
                    p->removeSpell(s, SPEC_MASK_ALL, false);

        // Soul Link authenticity + split-carrier self-heal (warlock). Soul Link "lasts as long as the
        // demon is active" in every era, and its redirect carrier (a permanent area-aura the demon
        // casts, radiating to the warlock) must never outlive the buff. Enforce both here every poll,
        // for EVERY era pair (EraSoulLink::kPairs — Vanilla 932900/932901, TBC 948550/948551; the same
        // table era_soul_link and era_soul_link_relink walk, so a new era needs no edit here):
        //   1. Buff up but NO LIVE demon (died / dismissed / replaced) -> drop the buff. era_soul_link::
        //      OnRemove then strips the carrier. Authentic: recast Soul Link after re-summoning. (A brief
        //      pet-null on a teleport also drops it — acceptable; you recast, as in Vanilla.)
        //   2. Carrier present but buff gone (the doctor-caught orphan: the carrier persists on the pet
        //      across logout while the buff does not restore, or any path era_soul_link::OnRemove
        //      missed) -> strip it from the demon (the owning area-aura SOURCE — removing only the
        //      warlock's radiated application just re-radiates next tick) and from the warlock.
        if (p->getClass() == CLASS_WARLOCK)
        {
            Pet* pet = p->GetPet();
            bool demonAlive = pet && pet->IsAlive();
            for (EraSoulLink::Pair const& sl : EraSoulLink::kPairs)
            {
                if (!demonAlive && p->HasAura(sl.buff))
                    p->RemoveAurasDueToSpell(sl.buff);     // buff drops with the demon; OnRemove strips the carrier
                if (!p->HasAura(sl.buff) && p->HasAura(sl.split))
                {
                    if (pet)
                        pet->RemoveAurasDueToSpell(sl.split);   // remove the source area-aura owned by the demon
                    p->RemoveAurasDueToSpell(sl.split);          // and the warlock's own radiated application
                }
            }
        }
    }

    void OnPlayerLevelChanged(Player* p, uint8 /*old*/) override
    {
        if (!sEraTalentsConfig->Enabled())
            return;
        if (IsBot(p))
        {
            EraTalentBots::OnBotLevelChanged(p);   // band crossings + new-point spend
            return;
        }
        EraTransition::Detect(p);                    // a level-up can cross an era boundary
        EraTalents::ReconcileBaselineSpells(p, EraFromIP(p)); // e.g. a priest dinging 20 gets rank 1
        EraMasterDemonologist_OnLevelUp(p);          // re-scale the level-dependent Felhunter resist branch
        // A same-era level-up changes the available point count (level-9-spent) but Detect no-ops
        // when the era is unchanged, so push a fresh SYNC here or the addon's point count goes stale.
        // Only a managed era has a live point count to refresh (a crossing into/out of one already
        // SYNCs via Detect->Run).
        if (EraHasTalentTrees(EraFromIP(p)))
            EraTalentsComms::SendSync(p);
    }

    // The warlock trainer still lists the STOCK Create Firestone/Spellstone ranks (they conjure the
    // WotLK weapon-enchant stones; trainer_spell can't express "only for non-Vanilla eras"). If a
    // Vanilla-era warlock buys one anyway, reconcile strips it on the spot instead of at the next
    // login/level. Keyed to the exact stock ids so this can't recurse (reconcile never learns them;
    // the 932930 marker learn inside reconcile re-enters here but doesn't match the id set).
    //
    // Same strip-on-purchase pattern for the baseline-leak gates: Shield Slam / Ice Block / Divine
    // Spirit are trainer-baseline in WotLK, so their WotLK-era trainer rows are still offered to a
    // Vanilla character. If a Vanilla-without-talent character buys any rank, reconcile strips it
    // instantly. Safe to list every rank id here: reconcile only strips them for a Vanilla char
    // lacking the gating talent (a Vanilla char WITH the talent, and any TBC/WotLK char, keep them),
    // and reconcile never LEARNS these ids (they're strip-only gates), so this can't recurse.
    void OnPlayerLearnSpell(Player* p, uint32 spellID) override
    {
        static const std::unordered_set<uint32> kReconcileOnLearn =
            { // stock Firestone/Spellstone creates
              6366, 17951, 17952, 17953, 27250, 60219, 60220, 2362, 17727, 17728, 47886, 47888,
              // Shield Slam ranks (warrior node 18552)
              23922, 23923, 23924, 23925, 25258, 30356, 47487, 47488,
              // Ice Block (mage node 18046)
              45438,
              // Divine Spirit single-target ranks + Prayer of Spirit raid ranks (priest node 18113):
              // both chains are gated on the Divine Spirit talent, so instant-reconcile a fresh train.
              14752, 14818, 14819, 27841, 25312, 48073, 27681, 32999, 48074,
              // Paladin (Consecration / Blessing of Kings baseline gates); 25898 = Greater Blessing
              // of Kings (separate stock chain, ungated L60 trainer rows deleted — strip on a mistaken
              // purchase from any lingering trainer for a Vanilla paladin without the BoK talent).
              26573, 20116, 20922, 20923, 20924, 27173, 48818, 48819, 20217, 25898,
              // Paladin stock Lay on Hands ranks + stock seals/WotLK Judgement buttons —
              // restored 2026-08-20 — the ReconcileBaselineSpells re-entrancy guard now makes this
              // safe; instant-strips a stock seal/judgement/LoH the moment a Vanilla paladin
              // acquires it, e.g. an auto-learn or a stray learn mid-session that the login/level
              // reconcile would otherwise not catch until next login.
              633, 2800, 10310, 27154, 20154, 20164, 20165, 20166, 20271, 53407, 53408,
              // Stock WotLK Seal of Vengeance / Seal of Corruption (version-swapped 2026-08-30:
              // trainer rows deleted, kStockPalSwap strips them in Vanilla AND TBC) — instant-strip
              // if anything re-teaches one to a Vanilla/TBC paladin mid-session.
              31801, 53736,
              // Shaman stock totems being version-swapped (kStockShaSwap) + post-Vanilla totems/spell
              // to hide (kEraShaHide) — strip the moment a Vanilla shaman trains one instead of
              // waiting for the next login/level reconcile.
              2484, 3599, 5394, 5675, 5730, 6363, 6364, 6365, 6375, 6377, 6390, 6391, 6392, 8071, 8075,
              8143, 8154, 8155, 8160, 8161, 8177, 8181, 8184, 8190, 8227, 8249, 10406, 10407, 10408,
              10427, 10428, 10437, 10438, 10442, 10462, 10463, 10478, 10479, 10495, 10496, 10497, 10526,
              10537, 10538, 10585, 10586, 10587, 10595, 10600, 10601, 16387,
              3738, 2894, 2062, 8170, 1535,
              // Stock WotLK Windfury Totem summons (kStockShaSwap) — mirrors the block above; the
              // custom Windfury chain (931363-365) is trainer-taught behind a ReqAbility1 chain, NOT
              // listed here (matches the established convention: only STOCK ids get instant-strip-
              // on-purchase, never a custom chain gated by ReqAbility).
              8512, 10613, 10614, 25585, 25587,
              // Stock Sentry Totem (fix round 2026-09-02): now WotLK-only (Vanilla trains clone
              // 931390, TBC 947260); instant-strip a stray acquisition — the WotLK arm strips
              // nothing here, so a legitimate WotLK purchase is untouched.
              6495,
              // Hunter version-swapped baselines (Phase 7, 2026-09-04): stock Scorpid Sting 3043 is
              // now WotLK-only (Vanilla trains clones 932300-303, TBC trains clone 948281) and stock
              // Deterrence 19263 is WotLK-only too (a TALENT in both Vanilla and TBC). Same shape as
              // Sentry Totem 6495 above: both are STOCK ids whose trainer rows are marker-gated, the
              // Vanilla/TBC reconcile arms strip them and the WotLK arm strips nothing, so a
              // legitimate WotLK purchase is untouched and reconcile never learns either id.
              3043, 19263 };
        if (!sEraTalentsConfig->Enabled() || !kReconcileOnLearn.count(spellID))
            return;
        if (IsBot(p))
        {
            if (!p || !sEraTalentsConfig->BotTalents())
                return;   // IsBot(null)==true; knob off = today's native-bot behavior byte-for-byte
            // BOTS reach here too (Plan 3b Task 8, 2026-09-02): PlayerbotFactory::InitClassSpells
            // HARDCODES learnSpell(8071/3599/5394) as shaman essentials, and it runs AFTER
            // InitTalentsTree's era reconcile — so an era-managed shaman bot re-acquired three
            // stock WotLK rank-1 totems and the by-id AI bridge (HasSpell-first) cast the stock
            // version over the era clone until the next login/level reconcile. The old blanket
            // IsBot bail existed for build-churn perf, but the set-membership check above already
            // makes this fire only on the handful of gated ids (~3 per shaman build). Era via
            // EraFor (level band), NEVER EraFromIP — every bot reads Vanilla there. The cache
            // invalidation keeps the resolve bridge from serving a pre-strip stock id.
            EraTalents::ReconcileBaselineSpells(p, EraTalentBots::EraFor(p));
            EraTalentBots::InvalidateSpecCache(p);
            return;
        }
        EraTalents::ReconcileBaselineSpells(p, EraFromIP(p));
    }

    void OnPlayerLeaveCombat(Player* p) override
    {
        if (!sEraTalentsConfig->Enabled() || IsBot(p))
            return;
        EraTransition::FlushCombat(p);               // apply a transition deferred during combat
    }

    void OnPlayerLogout(Player* p) override
    {
        {
            std::lock_guard<std::mutex> guard(g_checkTimerMutex);
            g_checkTimer.erase(p->GetGUID());        // don't leak timer entries for logged-out players
        }
        EraTalentBots::OnLogout(p);                  // drop the bot spec-tab cache entry (cheap erase)
        EraTalents::EvictCache(p);                   // drop the rank cache (re-loaded on next login)
    }

    // GM ".level"/normal level-up re-grants points AFTER this hook; defer the zero one tick so it wins.
    void OnPlayerFreeTalentPointsChanged(Player* p, uint32 points) override
    {
        if (!sEraTalentsConfig->Enabled() || IsBot(p) || points == 0 || !p->IsInWorld())
            return;
        if (!EraHasTalentTrees(EraFromIP(p)))
            return;                                  // WotLK, and any era whose trees aren't built yet (TBC): keep native points
        ObjectGuid guid = p->GetGUID();
        p->m_Events.AddEventAtOffset([guid]()
        {
            if (Player* pl = ObjectAccessor::FindPlayer(guid))
            {
                pl->SetFreeTalentPoints(0);
                pl->SendTalentsInfoData(false);
            }
        }, Milliseconds(1));
    }

    // Trainer-driven era respec (spec 2026-08-25-era-talents-trainer-reset-design.md).
    // Player::resetTalents() fires this hook as its FIRST statement — before the
    // m_usedTalentCount == 0 early-out — so it reaches us on the exact trainer path
    // (MSG_TALENT_WIPE_CONFIRM -> resetTalents(false)) even though an era character has zero
    // native talents. The core then early-outs: it never charges, never resets, never casts the
    // untalent visual — this hook does all three for a managed-era character. The charge is
    // exactly resetTalentsCost() — what the trainer's confirm dialog displayed (the core's
    // escalation counter is private and never advances for a character with 0 native talents,
    // so this is effectively a flat 1g — the dialog-truthful choice, user-approved).
    // Re-entrancy (EraTransition::Run's own resetTalents(true) calls) needs no flag:
    //   * entering a managed era: its rows were wiped when it was last left -> SpentPoints == 0;
    //   * leaving one: EraFromIP is already the new native era -> EraHasTalentTrees bails.
    // noCost callers (GM .reset talents, AT_LOGIN_RESET_TALENTS) reset era talents free — the
    // intended semantics of those paths.
    void OnPlayerTalentsReset(Player* p, bool noCost) override
    {
        if (!sEraTalentsConfig->Enabled() || IsBot(p))
            return;
        EraId era = EraFromIP(p);
        if (!EraHasTalentTrees(era))
            return;                              // native-era char: core handles everything
        if (EraTalents::SpentPoints(p, era) == 0)
            return;                              // nothing to reset
        if (!noCost)
        {
            uint32 cost = sWorld->getBoolConfig(CONFIG_NO_RESET_TALENT_COST) ? 0 : p->resetTalentsCost();
            if (cost && !p->HasEnoughMoney(cost))
            {
                p->SendBuyError(BUY_ERR_NOT_ENOUGHT_MONEY, 0, 0, 0);
                return;
            }
            p->ModifyMoney(-int32(cost));        // exactly what the trainer confirm dialog displayed
        }
        EraTalents::Reset(p, era);
        p->CastSpell(p, 14867, true);            // "Untalent Visual Effect" — the core's own cast never fires
        EraTalentsComms::SendSync(p);            // addon panel refreshes live
    }

private:
    void pin(Player* p, EraId era)
    {
        if (EraHasTalentTrees(era))
        {
            p->SetFreeTalentPoints(0);
            p->SendTalentsInfoData(false);
        }
    }
};

// Two-Handed Axes & Maces equip gate (node 18822). Core weapon proficiency is add-only
// (Player::m_WeaponProficiency has AddWeaponProficiency but NO remover), and a WotLK shaman carries
// the 2H-axe/2H-mace weapon SKILL baseline (SKILL_2H_AXES 172 / SKILL_2H_MACES 160), so once a
// character is a shaman, base Player::CanUseItem's skill-value check already allows 2H axes/maces —
// a Vanilla talent RESET can never revoke that access. This hook is the dynamic gate on top: for a
// Vanilla-era shaman WITHOUT the talent (its granted proficiency clone 932406), deny 2H axes/maces;
// every other class / era / item is returned unchanged (fail safe).
//
// BOTS ARE IN SCOPE (I-25, 2026-09-06) whenever EraTalents.BotTalents is on: an era-managed bot has
// the same era build as a player, and the factory gear engine happily equipped a 2H axe on an
// untalented Vanilla-band shaman bot. With the knob OFF the old `IsBot` bail is preserved
// byte-for-byte, so knob-off behaviour is unchanged. The era source is EraTalentBots::EraFor, NOT
// bare EraFromIP — IP quest progression is 0 for every bot, i.e. "Vanilla" even at 80, which would
// have made this gate fire on WotLK-band shaman bots (the standing EraFor-vs-EraFromIP invariant).
// 932406 carries NO SkillLineAbility
// row (verified 2026-08-24: no client-patch/data hit outside spell_dbc), so learning it adds no weapon
// SKILL line — there is nothing to strip on reset, and this equip gate is the complete fix.
class era_talent_weapon_gate : public PlayerScript
{
public:
    era_talent_weapon_gate() : PlayerScript("era_talent_weapon_gate") {}

    // Returning false makes Player::CanUseItem return `result` (PlayerStorage.cpp:2432, the equip
    // path via CanUseItem(Item*) -> CanUseItem(proto)). This is deliberately narrow and conservative:
    // it can ONLY block a Vanilla shaman's untalented 2H axe/mace — never any other class, era, or item.
    [[nodiscard]] bool OnPlayerCanUseItem(Player* p, ItemTemplate const* proto, InventoryResult& result) override
    {
        if (!sEraTalentsConfig->Enabled() || !p || !proto)
            return true;                              // module off / no player / no template -> allow
        if (IsBot(p) && !sEraTalentsConfig->BotTalents())
            return true;                              // bot with the bot knob off -> allow (unchanged)
        if (p->getClass() != CLASS_SHAMAN)
            return true;                              // not a shaman -> allow
        if (proto->Class != ITEM_CLASS_WEAPON ||
            (proto->SubClass != ITEM_SUBCLASS_WEAPON_AXE2 && proto->SubClass != ITEM_SUBCLASS_WEAPON_MACE2))
            return true;                              // not a 2H axe (subclass 1) / 2H mace (subclass 5) -> allow
        if (EraTalentBots::EraFor(p) != ERA_VANILLA)
            return true;                              // TBC/WotLK shaman -> 2H is baseline, allow

        // Vanilla shaman + a 2H axe/mace: allow ONLY with the Two-Handed Axes and Maces talent
        // (node 18822 grants proficiency clone 932406). HasSpell honors specMask, so a reset that
        // zeroes the talent-cost spell's specMask correctly reads as "not known" (framework field-
        // semantics: removeSpell retains the row at specMask 0; HasSpell checks IsInSpec).
        if (p->HasSpell(SPELL_ERA_TWO_HANDED_AXES_MACES))
            return true;

        result = EQUIP_ERR_NO_REQUIRED_PROFICIENCY;
        return false;                                 // deny: untalented Vanilla shaman cannot use 2H axe/mace
    }
};

void AddSC_era_talent_pin()
{
    new era_talent_pin();
    new era_talent_weapon_gate();
}
