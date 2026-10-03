/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 * Modified 2026-10 (in-house fork, azerothcore-playerbots-docker-automated contributors):
 * pricing table when the bot is disabled, .ahbot price. See ../NOTICE.
*/

#include "Chat.h"
#include "ScriptMgr.h"
#include "AuctionHouseBot.h"
#include "Log.h"
#include "Mail.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "StringFormat.h"
#include "WorldSession.h"

using namespace Acore::ChatCommands;

class AHBot_WorldScript : public WorldScript
{
private:
    bool HasPerformedStartup;

public:
    AHBot_WorldScript() : WorldScript("AHBot_WorldScript"), HasPerformedStartup(false) { }

    void OnAfterConfigLoad(bool /*reload*/) override
    {
        if (!auctionbot->IsModuleEnabled())
            return;

        auctionbot->InitializeConfiguration();
        if (HasPerformedStartup == true)
        {
            LOG_INFO("server.loading", "AuctionHouseBot: (Re)populating item candidate lists ...");
            auctionbot->PopulateItemCandidatesAndProportions();

            if (sConfigMgr->GetOption<bool>("AuctionHouseBot.AdvancedListingRules.UseDropRates.Enabled", false))
            {
                auctionbot->PopulateQuestRewardItemIDs();
                auctionbot->PopulateItemDropChances();
            }
        }
    }

    void OnStartup() override
    {
        if (!auctionbot->IsModuleEnabled())
        {
            // Bot off, but .ahprice and the Enchanter NPC still price from the table.
            auctionbot->InitializeConfiguration(false);
            auctionbot->BuildPricingTable();
            return;
        }

        LOG_INFO("server.loading", "AuctionHouseBot: (Re)populating item candidate lists ...");
        auctionbot->PopulateItemCandidatesAndProportions();
        if (sConfigMgr->GetOption<bool>("AuctionHouseBot.AdvancedListingRules.UseDropRates.Enabled", false))
        {
            auctionbot->PopulateQuestRewardItemIDs();
            auctionbot->PopulateItemDropChances();
        }
        HasPerformedStartup = true;
    }
};

class AHBot_AuctionHouseScript : public AuctionHouseScript
{
public:
    AHBot_AuctionHouseScript() : AuctionHouseScript("AHBot_AuctionHouseScript") { }

    void OnBeforeAuctionHouseMgrSendAuctionSuccessfulMail(AuctionHouseMgr* /*auctionHouseMgr*/, AuctionEntry* /*auction*/, Player* owner, uint32& /*owner_accId*/, uint32& /*profit*/, bool& sendNotification, bool& updateAchievementCriteria, bool& /*sendMail*/) override
    {
        if (owner)
        {
            bool isAHBot = false;
            for (AuctionHouseBotCharacter character : auctionbot->AHCharacters)
            {
                if (character.CharacterGUID == owner->GetGUID().GetCounter())
                {
                    isAHBot = true;
                    break;
                }
            }
            if (isAHBot == true)
            {
                sendNotification = false;
                updateAchievementCriteria = false;
            }
        }
    }

    void OnBeforeAuctionHouseMgrSendAuctionExpiredMail(AuctionHouseMgr* /*auctionHouseMgr*/, AuctionEntry* /*auction*/, Player* owner, uint32& /*owner_accId*/, bool& sendNotification, bool& sendMail) override
    {
        if (owner)
        {
            bool isAHBot = false;
            for (AuctionHouseBotCharacter character : auctionbot->AHCharacters)
            {
                if (character.CharacterGUID == owner->GetGUID().GetCounter())
                {
                    isAHBot = true;
                    break;
                }
            }
            if (isAHBot == true)
            {
                sendNotification = false;

                if (sConfigMgr->GetOption<bool>("AuctionHouseBot.ReturnExpiredAuctionItemsToBot", false))
                    sendMail = true;
                else
                    sendMail = false;
            }
        }   
    }

    void OnBeforeAuctionHouseMgrSendAuctionOutbiddedMail(AuctionHouseMgr* /*auctionHouseMgr*/, AuctionEntry* auction, Player* oldBidder, uint32& /*oldBidder_accId*/, Player* newBidder, uint32& newPrice, bool& /*sendNotification*/, bool& /*sendMail*/) override
    {
        if (oldBidder && !newBidder)
            oldBidder->GetSession()->SendAuctionBidderNotification((uint32)auction->GetHouseId(), auction->Id, ObjectGuid::Create<HighGuid::Player>(auctionbot->CurrentBotCharGUID), newPrice, auction->GetAuctionOutBid(), auction->item_template);
    }

    void OnBeforeAuctionHouseMgrSendAuctionWonMail(AuctionHouseMgr* /*auctionHouseMgr*/, AuctionEntry* /*auction*/, Player* bidder, uint32& /*bidder_accId*/, bool& sendNotification, bool& updateAchievementCriteria, bool& /*sendMail*/) override
    {
        // The bot buyer is a shell Player that never went through a full load (no map, no achievement data),
        // so suppress the paths that would touch that missing state when it wins an auction
        if (bidder)
        {
            bool isAHBot = false;
            for (AuctionHouseBotCharacter character : auctionbot->AHCharacters)
            {
                if (character.CharacterGUID == bidder->GetGUID().GetCounter())
                {
                    isAHBot = true;
                    break;
                }
            }
            if (isAHBot == true)
            {
                sendNotification = false;
                updateAchievementCriteria = false;
            }
        }
    }

    void OnBeforeAuctionHouseMgrUpdate() override
    {
        auctionbot->Update();
    }
};

class AHBot_MailScript : public MailScript
{
public:
    AHBot_MailScript() : MailScript("AHBot_MailScript") { }

    void OnBeforeMailDraftSendMailTo(MailDraft* /*mailDraft*/, MailReceiver const& receiver, MailSender const& sender, MailCheckMask& /*checked*/, uint32& /*deliver_delay*/, uint32& /*custom_expiration*/, bool& deleteMailItemsFromDB, bool& sendMail) override
    {
        bool isAHBot = false;
        for (AuctionHouseBotCharacter character : auctionbot->AHCharacters)
        {
            if (character.CharacterGUID == receiver.GetPlayerGUIDLow())
            {
                isAHBot = true;
                break;
            }
        }
        if (isAHBot == true)
        {
            if (sConfigMgr->GetOption<bool>("AuctionHouseBot.ReturnExpiredAuctionItemsToBot", false))
            {
                deleteMailItemsFromDB = false;
                sendMail = true;
            }
            else
            {
                if (sender.GetMailMessageType() == MAIL_AUCTION)        // auction mail with items
                    deleteMailItemsFromDB = true;
                sendMail = false;
            }
        }
    }
};

static std::string AHBotMoney(uint64 copper)
{
    return Acore::StringFormat("{}g {}s {}c", copper / 10000, (copper / 100) % 100, copper % 100);
}

class AHBot_CommandScript : public CommandScript
{
public:
    AHBot_CommandScript() : CommandScript("AHBot_CommandScript") { }

    Acore::ChatCommands::ChatCommandTable GetCommands() const override
    {
        static Acore::ChatCommands::ChatCommandTable AHBotCommandTable = {
            {"update", HandleAHBotUpdateCommand, SEC_GAMEMASTER, Acore::ChatCommands::Console::Yes},
            {"reload", HandleAHBotReloadCommand, SEC_GAMEMASTER, Acore::ChatCommands::Console::Yes},
            {"empty",  HandleAHBotEmptyCommand,  SEC_GAMEMASTER, Acore::ChatCommands::Console::Yes},
            {"help",  HandleAHBotHelpCommand,  SEC_GAMEMASTER, Acore::ChatCommands::Console::Yes},
            {"price", HandleAHBotPriceCommand, SEC_GAMEMASTER, Acore::ChatCommands::Console::Yes}
        };

        static Acore::ChatCommands::ChatCommandTable commandTable = {
            {"ahbot", AHBotCommandTable},
        };

        return commandTable;
    }

    static bool HandleAHBotUpdateCommand(ChatHandler* handler, const char* /*args*/)
    {
        LOG_INFO("module", "AuctionHouseBot: Updating Auction House...");
        handler->PSendSysMessage("AuctionHouseBot: Updating Auction House...");
        AuctionHouseBot::instance()->Update();
        LOG_INFO("module", "AuctionHouseBot: Auction House Updated.");
        handler->PSendSysMessage("AuctionHouseBot: Auction House Updated.");
        return true;
    }

    static bool HandleAHBotReloadCommand(ChatHandler* handler, char const* /*args*/)
    {
        LOG_INFO("module", "AuctionHouseBot: Reloading Config...");
        handler->PSendSysMessage("AuctionHouseBot: Reloading Config...");

        // Reload config file with isReload = true
        sConfigMgr->LoadModulesConfigs(true, false);
        AuctionHouseBot::instance()->InitializeConfiguration(auctionbot->IsModuleEnabled());
        AuctionHouseBot::instance()->PopulateItemCandidatesAndProportions();

        if (sConfigMgr->GetOption<bool>("AuctionHouseBot.AdvancedListingRules.UseDropRates.Enabled", true))
        {
            auctionbot->PopulateQuestRewardItemIDs();
            auctionbot->PopulateItemDropChances();
        }

        LOG_INFO("module", "AuctionHouseBot: Config reloaded.");
        handler->PSendSysMessage("AuctionHouseBot: Config reloaded.");        
        return true;
    }

    static bool HandleAHBotEmptyCommand(ChatHandler* handler, char const* /*args*/)
    {
        LOG_INFO("module", "AuctionHouseBot: Emptying Auction House...");
        handler->PSendSysMessage("AuctionHouseBot: Emptying Auction House...");
        AuctionHouseBot::instance()->EmptyAuctionHouses();
        AuctionHouseBot::instance()->CleanupExpiredAuctionItems(); // Must go after EmptyAuctionHouses()
        LOG_INFO("module", "AuctionHouseBot: Auction Houses Emptied.");
        handler->PSendSysMessage("AuctionHouseBot: Auction Houses Emptied.");
        return true;
    }

    static bool HandleAHBotHelpCommand(ChatHandler* handler, char const* /*args*/)
    {
        handler->PSendSysMessage("AuctionHouseBot commands:");
        handler->PSendSysMessage("  .ahbot reload - Reloads configuration");
        handler->PSendSysMessage("  .ahbot empty  - Removes all AuctionHouseBot auctions");
        handler->PSendSysMessage("  .ahbot update - Runs an update cycle");
        handler->PSendSysMessage("  .ahbot price <item> - Price breakdown (role, roll-up, gear K, sell/buy)");
        return true;
    }

    // .ahbot price <item link|id> — the full price breakdown (value source, sell range, buyer price).
    static bool HandleAHBotPriceCommand(ChatHandler* handler, Variant<Hyperlink<item>, uint32> itemArg)
    {
        uint32 itemId = itemArg.holds_alternative<Hyperlink<item>>()
            ? itemArg.get<Hyperlink<item>>()->Item->ItemId
            : itemArg.get<uint32>();
        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemId);
        if (!proto)
        {
            handler->SendSysMessage(Acore::StringFormat("AuctionHouseBot: no item {}", itemId));
            return true;
        }
        // Breakdown and sell/buy range come from the same snapshot (a reload can swap it).
        std::shared_ptr<AHBotPricing::Table const> table;
        AHBotItemPricing pricing;
        if (!auctionbot->GetItemPricing(itemId, pricing, &table) || !table)
        {
            handler->SendSysMessage("AuctionHouseBot: pricing table not built yet");
            return true;
        }
        auto priceIt = table->prices.find(itemId);
        if (priceIt == table->prices.end())
        {
            handler->SendSysMessage("AuctionHouseBot: pricing table not built yet");
            return true;
        }
        AHBotPricing::ItemPrice const& p = priceIt->second;
        handler->SendSysMessage(Acore::StringFormat("{} [{}]  role={}  source={}", proto->Name1, itemId,
            AHBotRoles::RoleName(p.role), AHBotPricing::SourceName(p.source)));
        handler->SendSysMessage(Acore::StringFormat("  formula {}   final {}   mats {}",
            AHBotMoney(p.formulaValue), AHBotMoney(p.finalValue), AHBotMoney(p.matsValue)));
        if (p.recipeIndex >= 0)
        {
            AHBotPricing::Recipe const& r = table->recipes[p.recipeIndex];
            handler->SendSysMessage(Acore::StringFormat("  recipe spell {}  yield {:.2f}  markup {:.2f}",
                r.spellId, r.yield, table->params.craftMarkup));
            for (AHBotPricing::Reagent const& rg : r.reagents)
            {
                ItemTemplate const* rp = sObjectMgr->GetItemTemplate(rg.itemId);
                auto v = table->reagentValue.find(rg.itemId);
                uint64 each = v != table->reagentValue.end() ? v->second : 0;
                handler->SendSysMessage(Acore::StringFormat("    {} x{} @ {} = {}", rp ? rp->Name1 : "?", rg.count,
                    AHBotMoney(each), AHBotMoney(each * rg.count)));
            }
        }
        if (p.k > 0.0)
        {
            std::string band = p.kBand == AHBotPricing::KBAND_QUALITY_WIDE
                ? std::string("quality-wide")
                : Acore::StringFormat("ilvl {}-{}", p.kBand > 0 ? (p.kBand - 1) * 5 : 0, (p.kBand + 1) * 5 + 4);   // 3-band window
            handler->SendSysMessage(Acore::StringFormat("  gear K {:.2f} from {} ({} crafted samples)", p.k, band, p.kSamples));
        }
        handler->SendSysMessage(Acore::StringFormat("  bot sells {} - {}   bot buys <= {}",
            AHBotMoney(pricing.sellMin), AHBotMoney(pricing.sellMax), AHBotMoney(pricing.buyMax)));
        return true;
    }
};

void AddAHBotScripts()
{
    new AHBot_WorldScript();
    new AHBot_AuctionHouseScript();
    new AHBot_MailScript();
    new AHBot_CommandScript();
}
