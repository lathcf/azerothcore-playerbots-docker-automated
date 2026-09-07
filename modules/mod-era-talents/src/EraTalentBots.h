#ifndef MOD_ERA_TALENT_BOTS_H
#define MOD_ERA_TALENT_BOTS_H
#include "EraTalentIP.h"
class Player;

namespace EraTalentBots
{
    // True for any playerbot (session flag OR attached AI — see the .cpp comment on login order).
    bool IsBot(Player* p);

    // THE bot era source: level band, NEVER EraFromIP (IP quest progression is 0 for every
    // bot, i.e. "Vanilla" even at 80). 1-60 -> Vanilla, 61-70 -> TBC, 71+ -> WotLK.
    EraId BotEra(Player* bot);

    // Era for ANY character: BotEra for bots, EraFromIP for real players. This is the
    // required era source everywhere a bot can reach (the CLAUDE.md invariant) — EraFromIP
    // reads every bot as Vanilla, which is exactly wrong for a TBC/WotLK-band bot the moment
    // those eras' trees ship. Deliberately UNCONDITIONAL on EraTalents.BotTalents (matches
    // EraGlyphGate::Allowed): a bot's era is a fact of its level band, not of the knob.
    EraId EraFor(Player* p);

    // Full reconcile at the bot's CURRENT level. Always tears down stale managed-era rows
    // first (bracket up-moves would otherwise leave era clone spells in the spellbook where
    // the AI's name-resolver can pick them). Then: managed band + class has nodes -> strip
    // native talents, spend an era build for specTab (0-based tab), return true (caller must
    // NOT run native talent init). Otherwise returns false (caller runs the native path).
    // Inert (returns false, touches nothing) unless EraTalents.Enable AND
    // EraTalents.BotTalents are on and the player is a bot.
    bool FactoryReconcile(Player* bot, int specTab);

    void OnBotLogin(Player* bot);          // re-grant persisted era passives (cheap, 1 query)
    void OnBotLevelChanged(Player* bot);   // spend new points; handle band crossings
    void OnLogout(Player* p);              // drop the spec-tab cache entry (players: no-op erase)
    void InvalidateSpecCache(Player* p);   // era rows changed outside this unit (player TryLearn/
                                           // Reset) — the spec-tab bridge serves players too

    // ONE WorldDatabase query at worldserver startup, building SpellId -> MIN(trainer_spell.
    // ReqLevel) for every stock spell with a levelled trainer row (band ids excluded — the Phase
    // 9.5 sweep owns those). Read-only afterwards, so map-thread readers need no lock. Feeds the
    // bracket DOWN-move residue strip in TeardownStale: trainer-taught STOCK spells above the
    // bot's new level had no owner before (measured: Frostfire Bolt 42985, ReqLevel 77, still on
    // TBC-band mage bots after an 80 -> 70 move). Call from WorldScript::OnStartup.
    void BuildTrainerLevelIndex();
}

// C++-linkage bridges for fork patch 0020. Each is declared extern in the patched fork file
// with this EXACT signature — keep in sync with the patch or the fork fails to link.
//
// PlayerbotFactory::InitTalentsTree: era build instead of the WotLK template (see .cpp).
bool EraTalentBots_FactoryReconcile(Player* bot, int specTab);
// AiFactory::GetPlayerSpecTabs: a managed-era character's talents live in era_character_talent,
// not the native talent map (stripped by the era system) — every spec consumer
// (GetPlayerSpecTab, IsTank/IsHeal, ResetStrategies, StatsWeightCalculator gear scoring, raid
// triggers) counts native talents and would read the character as untalented. This reports the
// era build's per-tab points instead — for BOTS and for era-managed real PLAYERS (the master's
// role auto-detect in .raidroster login reads IsTank/IsHeal(master); an era player otherwise
// reads as tab 0 — a Shadow-built priest master was detected as the group healer). tabs3 =
// uint32[3], written only on a true return. CACHED per guid (GetPlayerSpecTabs runs on
// per-second AI paths; era rows are a DB read — one query per character per session,
// invalidated on every era-row mutation: bot paths in EraTalentBots, player TryLearn/Reset via
// InvalidateSpecCache, and logout).
// Returns false (native path) for unmanaged bands, no rows, or (bots) knob off.
bool EraTalentBots_SpecTabs(Player* bot, uint32* tabs3);
// Era-transparent spell lookup for the bot AI's hardcoded stock-id tables (patch 0021).
// Semantics: the spell id this character KNOWS for stock spell `stockSpellId` —
//   * knows the stock id itself -> stockSpellId (native bots, WotLK band, knob off);
//   * managed-era bot that doesn't -> the highest-id KNOWN same-name spell in the era
//     custom band [920000, 950000) (era clones keep stock names; ids ascend with rank),
//     passives included (Thick Hide-style discriminators need them);
//   * otherwise 0 ("doesn't know it", exactly like a false HasSpell).
// So `if (Resolve(bot, id))` is a drop-in for `if (bot->HasSpell(id))`, and the result is
// the id to cast/compare (totem UNIT_CREATED_BY_SPELL identity checks included). CACHED
// per (guid, stockId) — hot AI paths never re-scan; invalidated with the spec-tab cache.
uint32 EraTalentBots_ResolveSpellId(Player* bot, uint32 stockSpellId);
// Post-trainer-walk hook (patch 0020): called right after each PlayerbotFactory
// InitAvailableSpells(). The walk is class-only gated, so it re-teaches BOTH TBC paladin faction
// seals (946080 Seal of Blood / 946084 Seal of Vengeance) after the mid-build reconcile has
// already dropped the wrong one — this is the only strip site that runs AFTER the walk. Inert
// unless EraTalents.BotTalents is on, the target is a bot, and its band era is managed.
void EraTalentBots_PostTrainerWalk(Player* bot);
#endif
