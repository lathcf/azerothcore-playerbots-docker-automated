#include "EnchanterCommand.h"
#include "EnchanterCatalog.h"
#include "EnchanterConfig.h"
#include "EnchanterPrice.h"
#include "EnchanterRules.h"
#include "StringFormat.h"
#include <algorithm>
#include <cctype>

using namespace Acore::ChatCommands;

namespace
{
    std::string Lower(std::string s)
    {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return char(std::tolower(c)); });
        return s;
    }
}

ChatCommandTable EnchanterCommand::GetCommands() const
{
    static ChatCommandTable sub =
    {
        { "dump",         HandleDump,         SEC_GAMEMASTER, Console::Yes },
        { "reloadprices", HandleReloadPrices, SEC_GAMEMASTER, Console::Yes },
    };
    static ChatCommandTable root = { { "enchanter", sub } };
    return root;
}

// .enchanter dump <vanilla|tbc|wotlk> [name filter] — offers of EXACTLY that era, for review.
bool EnchanterCommand::HandleDump(ChatHandler* handler, std::string eraName, Tail filter)
{
    if (!g_EnchanterEnable)
    {
        handler->SendSysMessage("Enchanter is disabled (EnchanterNpc.Enable=0); no catalog was built.");
        return true;
    }
    std::string const e = Lower(eraName);
    int era = e == "vanilla" ? EnchanterRules::ERA_VANILLA : e == "tbc" ? EnchanterRules::ERA_TBC
            : e == "wotlk" ? EnchanterRules::ERA_WOTLK : -1;
    if (era < 0)
    {
        handler->SendSysMessage("Usage: .enchanter dump <vanilla|tbc|wotlk> [name filter]");
        return true;
    }
    std::string const needle = Lower(std::string(filter));

    std::vector<EnchantOffer> const& all = EnchanterCatalog::Offers();
    uint32 shown = 0;
    for (uint32 i = 0; i < all.size(); ++i)
    {
        EnchantOffer const& o = all[i];
        if (o.era != era || (!needle.empty() && Lower(o.name).find(needle) == std::string::npos))
            continue;
        // Recipe rank source: t = trainer, f = formula item, s = SkillLineAbility (suspect unless
        // learned on skill-up). Upgrade-item ranks come from RequiredLevel and carry no letter.
        char const* rankSrc = "";
        if (o.kind == EnchantSource::RECIPE)
            rankSrc = o.rankSource == EnchanterRules::RANK_TRAINER ? "t"
                    : o.rankSource == EnchanterRules::RANK_FORMULA ? "f" : "s";
        std::string mats;
        for (auto const& [entry, count] : o.reagents)
        {
            EnchanterPrice::Quote q = EnchanterPrice::UnitQuote(entry, TEAM_ALLIANCE);
            mats += Acore::StringFormat(" {}x{}@{}({})", entry, count, q.unitCopper, EnchanterPrice::SourceName(q.source));
        }
        handler->SendSysMessage(Acore::StringFormat(
            "[{}] {} spell={} item={} ench={} rank={}{} tier={} perk={}/{} | {} |{} | fee A={} H={}",
            i, o.kind == EnchantSource::RECIPE ? "recipe" : "item", o.spellId, o.itemId, o.enchantId, o.rank, rankSrc,
            o.ipTier, o.perkSkill, o.perkSkillValue, o.name, mats,
            EnchanterPrice::OfferFee(o, TEAM_ALLIANCE), EnchanterPrice::OfferFee(o, TEAM_HORDE)));
        ++shown;
    }
    handler->SendSysMessage(Acore::StringFormat("{} offer(s) shown.", shown));
    return true;
}

bool EnchanterCommand::HandleReloadPrices(ChatHandler* handler)
{
    if (!g_EnchanterEnable)
    {
        handler->SendSysMessage("Enchanter is disabled (EnchanterNpc.Enable=0).");
        return true;
    }
    EnchanterPrice::Rebuild();
    handler->SendSysMessage("Enchanter price cache rebuilt.");
    return true;
}
