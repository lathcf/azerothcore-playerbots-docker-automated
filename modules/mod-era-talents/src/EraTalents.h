#ifndef MOD_ERA_TALENTS_H
#define MOD_ERA_TALENTS_H
#include "EraTalentIP.h"
#include <string>
#include <unordered_map>
class Player;
namespace EraTalents
{
    // The reserved era custom-spell band. HIGH moved 933000 -> 950000 for the TBC era (TBC node
    // passives live at 936000-943999, TBC pets/helpers at 944000+; generator mirror:
    // gen_era_talents.py CUSTOM_PASSIVE_BASE/END). Nothing occupies [933000, 936000).
    constexpr uint32 ERA_CUSTOM_BAND_LOW  = 920000;
    constexpr uint32 ERA_CUSTOM_BAND_HIGH = 950000;

    int  SpentPoints(Player* p, EraId era);
    uint8 CurrentRank(Player* p, EraId era, uint32 talentId);
    int  AvailablePoints(Player* p, EraId era);
    bool TryLearn(Player* p, uint32 talentId, std::string& err);
    void Reset(Player* p, EraId era);           // strip passives + DELETE this era's rows
    void ReapplyOnLogin(Player* p, EraId era);  // idempotent re-grant from stored rows

    // The in-memory rank state backing CurrentRank/SpentPoints (talentId -> rank). Lazily
    // loaded with ONE SELECT per (character, era) and kept coherent by TryLearn/Reset — the
    // ONLY era_character_talent writers. Evicted on logout. Exists because per-call DB reads
    // stalled the world thread ~300ms per bot factory build (prod 2026-08-26). Returned BY
    // VALUE: readers run on map-update threads (AI spec bridge), so a reference into the
    // mutex-guarded cache would escape the lock (prod segfault 2026-08-26).
    std::unordered_map<uint32, uint8> Ranks(Player* p, EraId era);
    void EvictCache(Player* p);                 // logout: drop every era's entry for this guid

    // RAII deferred-write window for bot factory builds (world-thread only, non-reentrant —
    // a nested scope deactivates itself). TryLearn inside the scope updates only the rank
    // cache: no per-learn DB write, no per-learn ReconcileBaselineSpells. The destructor
    // persists the final (guid,era) rank state in ONE multi-row REPLACE, then runs a single
    // reconcile (+ Master Demonologist refresh for warlocks). Callers must NOT add their own
    // post-build reconcile.
    class BotBuildScope
    {
    public:
        BotBuildScope(Player* p, EraId era);
        ~BotBuildScope();
        BotBuildScope(BotBuildScope const&) = delete;
        BotBuildScope& operator=(BotBuildScope const&) = delete;
    private:
        Player* _p;
        EraId   _era;
        bool    _active;
    };

    // Reconcile spells that are baseline-obtainable in later eras but talent-only in Vanilla.
    // Runs for ALL classes (each gate row self-selects on classId). Two flavors:
    //   * AUTO-baseline (priest Holy Nova): force-grants rank 1 in TBC/WotLK.
    //   * TRAINER-baseline (warrior Shield Slam, mage Ice Block, priest Divine Spirit): strip-only
    //     — left untouched in TBC/WotLK (trained normally there).
    // In Vanilla both flavors strip all ranks unless the corresponding era talent has been learned.
    void ReconcileBaselineSpells(Player* p, EraId era);

    // TBC faction DPS seals (Task 5d): a paladin may hold ONLY the faction-appropriate seal —
    // Blood Elf (Horde) => Seal of Blood (946080); Alliance => Seal of Vengeance (946084). Strips the
    // wrong-faction seal by race if held. Must be callable from the BOT login path too: the factory
    // trainer-walk (InitAvailableSpells) is class-only-gated (Trainer::IsTrainerValidForPlayer checks
    // class, not faction) so it teaches a bot BOTH seals AFTER the mid-build reconcile has run — the
    // login-time strip is what removes the wrong one in steady state. Safe for any paladin (no-op when
    // the wrong seal isn't held); on a non-TBC character the band sweep (EraBandClassifier) strips every 946xxx seal as an allowlist era mismatch.
    void StripWrongFactionTbcPalSeal(Player* p);
}
#endif
