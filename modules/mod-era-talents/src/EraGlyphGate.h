#ifndef MOD_ERA_GLYPH_GATE_H
#define MOD_ERA_GLYPH_GATE_H
class Player;

namespace EraGlyphGate
{
    // Glyphs are WotLK-era only. True when this character may hold/apply glyphs:
    // gate off, DK (always exempt), or era == WotLK (players: EraFromIP; bots: BotEra).
    bool Allowed(Player* p);

    // Strip all equipped glyphs (aura + slot) when !Allowed. Safe no-op otherwise.
    // Stripped glyphs are consumed — same behavior as a respec.
    void StripIfDisallowed(Player* p);
}

// C++-linkage bridge for fork patch 0020 (PlayerbotFactory::InitGlyphs): declared extern in
// the patched fork file with this EXACT signature — keep in sync or the fork fails to link.
bool EraGlyphGate_BotGlyphsAllowed(Player* bot);

#endif
