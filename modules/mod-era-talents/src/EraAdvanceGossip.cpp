// EraAdvanceGossip — player-initiated expansion advance at the faction leader.
// Spec: docs/superpowers/specs/2026-09-07-era-manual-expansion-advance-design.md
//
// IP's automatic advance is held at stages 8 and 13 by patch 0027 (ManualAdvanceStates), so a
// character eligible to cross an expansion boundary must come here and confirm. On Accept we
// only force the IP state; the existing 2s era poll (EraTransition::Detect/Run) performs the
// actual talent wipe / tree switch / reconcile / glyph strip / addon SYNC exactly as for any
// other IP change. No transition logic lives in this file.
//
// Hook mechanics: ScriptMgr::OnGossipHello/OnGossipSelect dispatch to every AllCreatureScript
// BEFORE the stock menu; returning true replaces the stock PrepareGossipMenu/SendPreparedGossip
// (so we rebuild the stock menu ourselves and append one item), returning false leaves stock
// flow untouched. Thrall's npc_thrall_warchief ScriptedAI has no gossip of its own.
#include "EraTalentsConfig.h"
#include "EraTalentIP.h"
#include "EraTalentBots.h"
#include "Player.h"
#include "Creature.h"
#include "ScriptMgr.h"
#include "ScriptedGossip.h"
#include "Log.h"
#include <optional>

namespace
{
    constexpr uint32 kAnduinWrynn   = 1747;   // Stormwind Keep (gossip_menu_id 11874, npcflag 1)
    constexpr uint32 kThrall        = 4949;   // Orgrimmar, Valley of Wisdom (gossip_menu_id 3664, npcflag 3)
    constexpr uint32 kSender        = 0xE7A0; // module-unique gossip sender — never collides with a stock menu
    constexpr uint32 kActionAdvance = 1;

    bool IsLeaderForTeam(Creature* c, Player* p)
    {
        switch (c->GetEntry())
        {
            case kAnduinWrynn: return p->GetTeamId() == TEAM_ALLIANCE;
            case kThrall:      return p->GetTeamId() == TEAM_HORDE;
            default:           return false;
        }
    }

    // THE eligibility rule (spec §2 table). Empty = not eligible; stock gossip untouched.
    std::optional<uint8> PendingAdvance(Player* p)
    {
        if (!p || !p->IsInWorld() || !p->IsAlive() || p->IsInCombat())
            return std::nullopt;
        if (EraTalentBots::IsBot(p))
            return std::nullopt;   // bots derive era from level band, never from IP
        uint8 state = EraIP::State(p);
        if (state == EraIP::kStateNaxx40)
            return EraIP::kStatePreTBC;
        if (state == EraIP::kStateTbcTier4 && p->HasAchieved(EraIP::kAchievementKilJaeden))
            return EraIP::kStateTbcTier5;
        return std::nullopt;
    }

    std::string const& PopupFor(uint8 target)
    {
        return target == EraIP::kStatePreTBC ? sEraTalentsConfig->AdvanceTextTBC()
                                             : sEraTalentsConfig->AdvanceTextWotLK();
    }
}

class era_advance_gossip : public AllCreatureScript
{
public:
    era_advance_gossip() : AllCreatureScript("era_advance_gossip") {}

    bool CanCreatureGossipHello(Player* player, Creature* creature) override
    {
        // Guard before ANY deref (module-hooks gotcha).
        if (!sEraTalentsConfig->Enabled() || !sEraTalentsConfig->AdvanceGossip())
            return false;
        if (!player || !creature || !IsLeaderForTeam(creature, player))
            return false;
        std::optional<uint8> target = PendingAdvance(player);
        if (!target)
            return false;

        // Rebuild the stock menu (quest-giver entries + stock gossip text), append ours, send.
        player->PrepareGossipMenu(creature, creature->GetGossipMenuId(), true);
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Progress to the next expansion",
                         kSender, kActionAdvance, PopupFor(*target), 0, false);
        player->SendPreparedGossip(creature);
        return true;
    }

    bool CanCreatureGossipSelect(Player* player, Creature* creature, uint32 sender, uint32 action) override
    {
        if (!sEraTalentsConfig->Enabled() || !sEraTalentsConfig->AdvanceGossip())
            return false;
        if (sender != kSender || action != kActionAdvance)
            return false;   // a stock quest/gossip selection — let Player::OnGossipSelect handle it
        if (!player || !creature)
            return false;

        CloseGossipMenuFor(player);
        if (!IsLeaderForTeam(creature, player))
            return true;
        std::optional<uint8> target = PendingAdvance(player);   // re-validate: a stale menu cannot fire
        if (!target)
            return true;

        uint8 from = EraIP::State(player);
        EraIP::ForceState(player, *target);
        LOG_INFO("module", "[EraAdvance] {} {}->{} via {} (player-confirmed expansion advance)",
                 player->GetName(), uint32(from), uint32(*target), creature->GetName());
        return true;
    }
};

void AddSC_era_advance_gossip() { new era_advance_gossip(); }
