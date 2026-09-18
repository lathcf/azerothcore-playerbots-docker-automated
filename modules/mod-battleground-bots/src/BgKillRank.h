// modules/mod-battleground-bots/src/BgKillRank.h
// Pure tier ranking for the BG kill target. NO AzerothCore includes: compiled standalone by
// tests/test_killrank.cpp and included by the fork's BgKillTargetValue.cpp (patch 0019).
#ifndef MOD_BATTLEGROUND_BOTS_KILL_RANK_H
#define MOD_BATTLEGROUND_BOTS_KILL_RANK_H
#include <cstddef>
#include <vector>

namespace BgKillRank
{
    struct Cand
    {
        bool  isFc = false;                // enemy flag carrier
        bool  capturing = false;           // channelling the node/banner capture — any hit interrupts it
        bool  vehicle = false;             // hostile siege vehicle — demolisher/glaive/siege engine/catapult
        bool  attackingMe = false;         // this enemy's victim is the bot — it is already hitting us
        bool  healer = false;              // casting a heal now, or healer spec
        bool  unreachable = false;         // no LoS / >8y vertical / screened by >2 melee (melee bot)
        float hpPct = 100.f;
        bool  attackingProtectee = false;  // hitting our FC or one of our healers
        float dist = 0.f;
        bool  isHint = false;              // matches the director's team kill hint
        bool  isVictim = false;            // already the bot's current victim — commitment tie-break
    };

    // Ranking context. Everything that is about the BOT (not the candidate) lives here, so Tier()
    // stays a pure function of (candidate, my situation).
    struct RankCtx
    {
        bool  healerPriority = true;   // BattlegroundBots.HealerPriority
        float healerMaxDist  = 35.0f;  // HealerRange{Melee,Ranged} — how far a healer is worth walking
        bool  underPressure  = false;  // enough attackers on me / low enough HP that I must fight back
    };

    // 1 FC / capture channeller / hostile siege vehicle, 2 whoever is attacking ME, 3 a healer I can
    // actually reach, 4 low HP, 5 attacker of a protectee, 0 = not a priority.
    //
    // A capture cast dies to ANY damage, so it is worth the same interrupt-now urgency as the flag
    // carrier. A siege vehicle joins them because it is the only thing on the field that breaks a
    // keep gate, and nothing else will shoot it: the DRIVER is untargetable, so a ranking that sees
    // players only reads an empty gate.
    //
    // Tiers 2 and 3 are the 2026-09-15 realism rework. Live report: "bots are way too focused on
    // healers — they walk slowly toward a healer 30y away while 5 enemies beat on them". Healers
    // used to be tier 2 unconditionally, which made every bot abandon the fight it was IN for a
    // target it could not reach before dying. Now:
    //   * an enemy already swinging at us outranks the healer (a human fights back, and the enemy
    //     on top of you is the one you can actually kill);
    //   * the healer tier is bounded by REACH (healerMaxDist: melee ~12y, ranged ~35y) — a healer
    //     across the field is simply not a kill-target, it is a walk;
    //   * the healer tier is suppressed entirely while underPressure, for the same reason.
    // No tier was removed: an out-of-reach healer just falls through to the ordinary hp/peel tiers
    // (or 0), exactly like an unreachable one always did.
    inline int Tier(Cand const& c, RankCtx const& ctx)
    {
        if (c.isFc || c.capturing || c.vehicle) return 1;
        if (c.attackingMe) return 2;
        if (ctx.healerPriority && c.healer && !c.unreachable && !ctx.underPressure &&
            c.dist <= ctx.healerMaxDist)
            return 3;
        if (c.hpPct < 35.f) return 4;
        if (c.attackingProtectee) return 5;
        return 0;
    }

    // Index of a kill we are already most of the way through — the bot's CURRENT victim, below the
    // low-HP line — or -1. Switching off a target at 20% HP throws away every second of damage
    // already spent on it, so this beats the tier order outright (see Best()).
    inline int CommitIndex(std::vector<Cand> const& v)
    {
        for (std::size_t i = 0; i < v.size(); ++i)
            if (v[i].isVictim && v[i].hpPct < 35.f) return static_cast<int>(i);
        return -1;
    }

    // Index of the best candidate, or -1. Lowest non-zero tier wins; inside a tier the order is
    // hinted first, then the bot's CURRENT victim, then nearest. The isVictim step is the
    // commitment tie-break: without it two equal-tier picks swap every time the bots drift a
    // yard past each other, and each swap costs the bot its movement order.
    //
    // On top of that tie-break there is a hard commitment rule: once the current victim is under
    // the low-HP line, finish it — UNLESS something tier-1 (FC / capturer / siege vehicle) is on
    // the field, which is time-critical in a way a nearly-dead enemy is not.
    inline int Best(std::vector<Cand> const& v, RankCtx const& ctx)
    {
        int best = -1, bestTier = 0;
        for (std::size_t i = 0; i < v.size(); ++i)
        {
            int t = Tier(v[i], ctx);
            if (t == 0) continue;
            if (best < 0 || t < bestTier) { best = static_cast<int>(i); bestTier = t; continue; }
            if (t > bestTier) continue;

            Cand const& b = v[static_cast<std::size_t>(best)];
            if (v[i].isHint != b.isHint)     { if (v[i].isHint)   best = static_cast<int>(i); continue; }
            if (v[i].isVictim != b.isVictim) { if (v[i].isVictim) best = static_cast<int>(i); continue; }
            if (v[i].dist < b.dist) best = static_cast<int>(i);
        }
        if (best >= 0 && bestTier > 1)
            if (int commit = CommitIndex(v); commit >= 0) return commit;
        return best;
    }
}
#endif
