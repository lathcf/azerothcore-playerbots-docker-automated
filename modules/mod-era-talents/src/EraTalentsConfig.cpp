#include "EraTalentsConfig.h"
#include "Config.h"
EraTalentsConfig* EraTalentsConfig::instance() { static EraTalentsConfig i; return &i; }
void EraTalentsConfig::Load()
{
    _enabled = sConfigMgr->GetOption<bool>("EraTalents.Enable", true);
    _debug   = sConfigMgr->GetOption<bool>("EraTalents.Debug", false);
    _botTalents = sConfigMgr->GetOption<bool>("EraTalents.BotTalents", false);
    _glyphGate = sConfigMgr->GetOption<bool>("EraTalents.GlyphGate", true);
    _advanceGossip = sConfigMgr->GetOption<bool>("EraTalents.AdvanceGossip", true);
    _advanceTextTBC = sConfigMgr->GetOption<std::string>("EraTalents.AdvanceGossip.TextTBC",
        "Step through the Dark Portal? This will WIPE your current talents and move your character to The Burning Crusade: new talent trees and new spell ranks. Some spells will need to be re-learned at trainers. This cannot be undone.");
    _advanceTextWotLK = sConfigMgr->GetOption<std::string>("EraTalents.AdvanceGossip.TextWotLK",
        "Sail for Northrend? This will WIPE your current talents and move your character to Wrath of the Lich King: new talent trees and new spell ranks. Some spells will need to be re-learned at trainers. This cannot be undone.");
}
