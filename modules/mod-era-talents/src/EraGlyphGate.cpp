// EraGlyphGate — glyphs are a WotLK-era system (design doc 2026-08-26 Phase 3, D).
// Blocks glyph-item use and strips equipped glyphs for Vanilla/TBC-era characters,
// players AND bots. DKs are always exempt (WotLK class; matches the talent no-nodes guard).
#include "EraGlyphGate.h"
#include "EraTalentsConfig.h"
#include "EraTalentIP.h"
#include "EraTalentBots.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "DBCStores.h"
#include "Playerbots.h"   // GET_PLAYERBOT_AI — the bot test

namespace EraGlyphGate
{
    bool Allowed(Player* p)
    {
        if (!p)
            return true;
        if (!sEraTalentsConfig->Enabled() || !sEraTalentsConfig->GlyphGate())
            return true;
        if (p->getClass() == CLASS_DEATH_KNIGHT)
            return true;   // WotLK class — glyphs are era-legal at any level
        return EraTalentBots::EraFor(p) == ERA_WOTLK;   // bots by level band, players by IP
    }

    void StripIfDisallowed(Player* p)
    {
        if (!p || Allowed(p))
            return;

        bool stripped = false;
        for (uint32 slotIndex = 0; slotIndex < MAX_GLYPH_SLOT_INDEX; ++slotIndex)
        {
            uint32 glyph = p->GetGlyph(slotIndex);
            GlyphPropertiesEntry const* glyphEntry = sGlyphPropertiesStore.LookupEntry(glyph);
            if (!glyphEntry)
                continue;

            // Same wipe mechanics PlayerbotFactory::InitGlyphs uses: the glyph aura, any
            // aura it triggered, then the slot itself.
            p->RemoveAurasDueToSpell(glyphEntry->SpellId);
            Unit::AuraMap& ownedAuras = p->GetOwnedAuras();
            for (Unit::AuraMap::iterator iter = ownedAuras.begin(); iter != ownedAuras.end();)
            {
                Aura* aura = iter->second;
                if (SpellInfo const* trig = aura->GetTriggeredByAuraSpellInfo())
                {
                    if (trig->Id == glyphEntry->SpellId)
                    {
                        p->RemoveOwnedAura(iter);
                        continue;
                    }
                }
                ++iter;
            }
            p->SetGlyph(slotIndex, 0, true);
            stripped = true;
        }

        if (stripped)
            p->SendTalentsInfoData(false);
    }
}

bool EraGlyphGate_BotGlyphsAllowed(Player* bot)
{
    return EraGlyphGate::Allowed(bot);
}

class era_glyph_gate : public PlayerScript
{
public:
    era_glyph_gate() : PlayerScript("era_glyph_gate") {}

    // Blocks the glyph-apply path: applying a glyph is CMSG_USE_ITEM on the glyph item,
    // which runs Player::CanUseItem -> this hook. Guard-first: bail before any policy work.
    [[nodiscard]] bool OnPlayerCanUseItem(Player* p, ItemTemplate const* proto,
                                          InventoryResult& result) override
    {
        if (!p || !proto || proto->Class != ITEM_CLASS_GLYPH)
            return true;
        if (EraGlyphGate::Allowed(p))
            return true;
        result = EQUIP_ERR_CANT_DO_RIGHT_NOW;
        return false;
    }

    // Login covers players AND bots (random-bot sessions log in like players), including
    // the ~209 legacy glyphed Vanilla-band bots as they cycle online.
    void OnPlayerLogin(Player* p) override
    {
        EraGlyphGate::StripIfDisallowed(p);
    }
};

void AddSC_era_glyph_gate() { new era_glyph_gate(); }
