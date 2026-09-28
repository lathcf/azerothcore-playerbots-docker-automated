#include "Chat.h"
#include "Creature.h"
#include "DBCStores.h"
#include "EnchanterCatalog.h"
#include "EnchanterConfig.h"
#include "EnchanterEra.h"
#include "EnchanterPrice.h"
#include "EnchanterRules.h"
#include "GossipDef.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "ScriptedGossip.h"
#include "StringFormat.h"
#include "WorldSession.h"
#include <array>
#include <utility>

using namespace EnchanterRules;

namespace
{
    constexpr uint32 kTextGreeting = 601020;
    constexpr uint32 kTextNothing  = 601021;
    constexpr uint32 kTextClosed   = 601022;
    constexpr uint32 kVisualSpell  = 12512;   // the enchant flash upstream mod-npc-enchanter casts

    std::array<std::pair<uint8, char const*>, 13> const kSlots = {{
        { EQUIPMENT_SLOT_HEAD,      "Head"      }, { EQUIPMENT_SLOT_SHOULDERS, "Shoulders" },
        { EQUIPMENT_SLOT_BACK,      "Back"      }, { EQUIPMENT_SLOT_CHEST,     "Chest"     },
        { EQUIPMENT_SLOT_WRISTS,    "Wrists"    }, { EQUIPMENT_SLOT_HANDS,     "Hands"     },
        { EQUIPMENT_SLOT_LEGS,      "Legs"      }, { EQUIPMENT_SLOT_FEET,      "Feet"      },
        { EQUIPMENT_SLOT_FINGER1,   "Ring 1"    }, { EQUIPMENT_SLOT_FINGER2,   "Ring 2"    },
        { EQUIPMENT_SLOT_MAINHAND,  "Main Hand" }, { EQUIPMENT_SLOT_OFFHAND,   "Off Hand"  },
        { EQUIPMENT_SLOT_RANGED,    "Ranged"    },
    }};

    bool IsOfferedSlot(uint8 slot)
    {
        for (auto const& s : kSlots)
            if (s.first == slot)
                return true;
        return false;
    }

    std::string CurrentEnchantName(Item* item)
    {
        uint32 id = item->GetEnchantmentId(PERM_ENCHANTMENT_SLOT);
        if (!id)
            return "none";
        SpellItemEnchantmentEntry const* e = sSpellItemEnchantmentStore.LookupEntry(id);
        return (e && e->description[0]) ? e->description[0] : "unknown";
    }

    Item* EquippedAt(Player* player, uint8 slot)
    {
        return player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
    }

    // "4x Arcane Dust, 1x Large Prismatic Shard"
    std::string OwnMatsList(EnchanterPrice::MatsPlan const& plan)
    {
        std::string out;
        for (auto const& [entry, count] : plan.useOwn)
        {
            ItemTemplate const* proto = sObjectMgr->GetItemTemplate(entry);
            out += Acore::StringFormat("{}{}x {}", out.empty() ? "" : ", ", count,
                proto ? proto->Name1 : std::to_string(entry));
        }
        return out;
    }

    void ShowTop(Player* player, Creature* creature, bool useMats)
    {
        ClearGossipMenuFor(player);
        AddGossipItemFor(player, GOSSIP_ICON_INTERACT_1,
            useMats ? "Use my materials: ON" : "Use my materials: OFF",
            EncodeSender(kSenderTop, useMats), kActionToggleMats);
        bool any = false;
        for (auto const& [slot, label] : kSlots)
        {
            Item* item = EquippedAt(player, slot);
            if (!item || EnchanterCatalog::EligibleFor(player, item).empty())
                continue;
            any = true;
            AddGossipItemFor(player, GOSSIP_ICON_TRAINER,
                Acore::StringFormat("{}: {} [{}]", label, item->GetTemplate()->Name1, CurrentEnchantName(item)),
                EncodeSender(kSenderSlotBase + slot, useMats), kActionPageBase + 0);
        }
        SendGossipMenuFor(player, any ? kTextGreeting : kTextNothing, creature);
    }

    void ShowSlot(Player* player, Creature* creature, uint8 slot, uint32 page, bool useMats)
    {
        Item* item = EquippedAt(player, slot);
        std::vector<uint32> const offers = item ? EnchanterCatalog::EligibleFor(player, item) : std::vector<uint32>();
        if (offers.empty())
        {
            ShowTop(player, creature, useMats);
            return;
        }
        ClearGossipMenuFor(player);
        uint32 const sender = EncodeSender(kSenderSlotBase + slot, useMats);
        uint32 const n = uint32(offers.size());
        std::string const current = CurrentEnchantName(item);
        for (uint32 i = PageBegin(page, n); i < PageEnd(page, n); ++i)
        {
            EnchantOffer const& o = EnchanterCatalog::Offers()[offers[i]];
            EnchanterPrice::MatsPlan const plan = EnchanterPrice::PlanMats(o, player->GetTeamId(), useMats ? player : nullptr);
            uint32 const fee = EnchanterPrice::FeeForPlan(o, plan);
            std::string popup = Acore::StringFormat("Replace {} with {}?", current, o.name);
            if (!plan.useOwn.empty())
                popup += " Uses your " + OwnMatsList(plan) + ".";
            AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG,
                Acore::StringFormat("{} - {}", o.name, MoneyString(fee)),
                sender, offers[i], popup, fee, false);
        }
        if (HasNextPage(page, n))
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Next page", sender, kActionPageBase + page + 1);
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Back", EncodeSender(kSenderTop, useMats), kActionBack);
        SendGossipMenuFor(player, kTextGreeting, creature);
    }

    // `confirmedFee` is the BoxMoney of the menu item the player confirmed: never charge more.
    void Buy(Player* player, Creature* creature, uint8 slot, uint32 offerIndex, uint32 confirmedFee, bool useMats)
    {
        CloseGossipMenuFor(player);
        ChatHandler chat(player->GetSession());
        std::vector<EnchantOffer> const& all = EnchanterCatalog::Offers();
        Item* item = EquippedAt(player, slot);
        if (offerIndex >= all.size() || !item)
        {
            chat.SendSysMessage("Equip the item you want enchanted first.");
            return;
        }
        EnchantOffer const& o = all[offerIndex];
        if (!EnchanterCatalog::Eligible(player, EnchanterEra::PlayerEra(player), item, o))
        {
            chat.SendSysMessage("That enchant isn't available for this item any more.");
            return;
        }
        TeamId const team = player->GetTeamId();
        // Re-plan from the live bags: mats sold or moved since the menu was built raise the fee,
        // which the confirmed-fee guard below catches.
        EnchanterPrice::MatsPlan const plan = EnchanterPrice::PlanMats(o, team, useMats ? player : nullptr);
        uint32 const fee = EnchanterPrice::FeeForPlan(o, plan);
        // With own mats, ANY fee change means the bags changed and the popup's "Uses your ..." list
        // the player consented to is stale: re-show, take nothing.
        if (useMats && fee != confirmedFee)
        {
            chat.SendSysMessage("Your materials changed. Here are the updated prices.");
            ShowSlot(player, creature, slot, 0, useMats);
            return;
        }
        if (fee > confirmedFee)
        {
            chat.SendSysMessage(Acore::StringFormat("Market prices moved: {} now costs {}.", o.name, MoneyString(fee)));
            ShowSlot(player, creature, slot, 0, useMats);
            return;
        }
        if (!player->HasEnoughMoney(fee))
        {
            player->SendBuyError(BUY_ERR_NOT_ENOUGHT_MONEY, creature, 0, 0);
            return;
        }
        if (!plan.useOwn.empty() && player->GetTradeData())
        {
            chat.SendSysMessage("Finish your trade first.");
            return;
        }
        // Verify every used entry before anything is taken.
        for (auto const& [entry, used] : plan.useOwn)
        {
            if (entry == item->GetEntry() || EnchanterPrice::BagItemCount(player, entry) < used)
            {
                chat.SendSysMessage("Your materials changed. Talk to me again.");
                return;
            }
        }
        // DestroyItemCount scans the backpack, then keyring, then bags, before equipment/bank.
        // Reagents never sit in the keyring and nothing is in a trade (refused above), so the
        // bag-only count verified here is always satisfied before it reaches equipment or bank.
        for (auto const& [entry, used] : plan.useOwn)
            player->DestroyItemCount(entry, used, true);

        player->ModifyMoney(-int32(fee));
        // Canonical sequence from Spell::EffectEnchantItemPerm.
        player->ApplyEnchantment(item, PERM_ENCHANTMENT_SLOT, false);
        item->SetEnchantment(PERM_ENCHANTMENT_SLOT, o.enchantId, 0, 0, creature->GetGUID());
        player->ApplyEnchantment(item, PERM_ENCHANTMENT_SLOT, true);
        player->RemoveTradeableItem(item);
        item->ClearSoulboundTradeable(player);

        creature->CastSpell(player, kVisualSpell, true);
        std::string const ownList = OwnMatsList(plan);
        chat.SendSysMessage(Acore::StringFormat("{} applied to {} for {}{}.", o.name, item->GetTemplate()->Name1, MoneyString(fee),
            ownList.empty() ? "" : " (used your " + ownList + ")"));

        if (g_EnchanterDebug)
        {
            std::string parts;
            for (auto const& [entry, count] : o.reagents)
            {
                EnchanterPrice::Quote q = EnchanterPrice::UnitQuote(entry, team);
                parts += Acore::StringFormat(" {}x{}@{}({})", entry, count, q.unitCopper, EnchanterPrice::SourceName(q.source));
            }
            std::string used;
            for (auto const& [entry, count] : plan.useOwn)
                used += Acore::StringFormat(" {}x{}", entry, count);
            LOG_INFO("module", "[Enchanter] {} bought '{}' (offer {}, enchant {}) on {} for {}c: mats {}c ->{}; own mats:{}",
                player->GetName(), o.name, offerIndex, o.enchantId, item->GetEntry(), fee,
                EnchanterPrice::MatsCopper(o, team), parts, used.empty() ? " none" : used);
        }
    }
}

class npc_enchanter_era : public CreatureScript
{
public:
    npc_enchanter_era() : CreatureScript("npc_enchanter_era") { }

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        if (!g_EnchanterEnable)
        {
            ClearGossipMenuFor(player);
            SendGossipMenuFor(player, kTextClosed, creature);
            return true;
        }
        if (player->GetSession()->IsBot())
            return true;
        ShowTop(player, creature, true);
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 sender, uint32 action) override
    {
        if (!g_EnchanterEnable || player->GetSession()->IsBot())
        {
            CloseGossipMenuFor(player);
            return true;
        }
        bool const useMats = SenderUsesMats(sender);
        uint32 const base = SenderBase(sender);
        if (action == kActionToggleMats)
        {
            ShowTop(player, creature, !useMats);
            return true;
        }
        if (base == kSenderTop || action == kActionBack)
        {
            ShowTop(player, creature, useMats);
            return true;
        }
        if (base < kSenderSlotBase || !IsOfferedSlot(uint8(base - kSenderSlotBase)))
        {
            CloseGossipMenuFor(player);
            return true;
        }
        uint8 const slot = uint8(base - kSenderSlotBase);
        if (IsPageAction(action))
        {
            ShowSlot(player, creature, slot, action - kActionPageBase, useMats);
            return true;
        }
        // The fee the player saw and confirmed lives on the still-open server-side menu item
        // (GossipMenuItem::OptionType holds the action).
        bool found = false;
        uint32 confirmedFee = 0;
        for (auto const& [id, mi] : player->PlayerTalkClass->GetGossipMenu().GetMenuItems())
        {
            if (mi.Sender == sender && mi.OptionType == action)
            {
                confirmedFee = mi.BoxMoney;
                found = true;
                break;
            }
        }
        if (!found)
        {
            CloseGossipMenuFor(player);
            return true;
        }
        Buy(player, creature, slot, action, confirmedFee, useMats);
        return true;
    }
};

void AddEnchanterNpcScript()
{
    new npc_enchanter_era();
}
