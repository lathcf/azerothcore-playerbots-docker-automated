#include "PBChatterAmbient.h"
#include "PBChatterAmbientPrompt.h"
#include "PBChatterConfig.h"
#include "PBChatterEvents.h"
#include "PBChatterQueue.h"
#include "Player.h"
#include "Group.h"
#include "ObjectAccessor.h"
#include "ObjectGuid.h"
#include "Playerbots.h"
#include "Random.h"
#include <deque>
#include <iterator>   // std::next
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace
{
    struct Line { std::string speaker; std::string text; };

    struct Ctx
    {
        uint8_t  kind = AMB_ZONE;
        uint64_t ident = 0;        // zoneId / group raw guid / guildId
        uint64_t anchor = 0;       // a present real player's GUID counter
        uint32_t nextEmitMs = 0;
        uint32_t lastLineMs = 0;
        uint32_t cooldownUntilMs = 0;
        int      botStreak = 0;
        uint64_t lastAuthorBot = 0; // GUID counter of the last bot to speak here
        std::deque<Line> buffer;
        bool     eligible = true;
    };

    std::unordered_map<std::string, Ctx>      g_ctx;          // key -> context
    std::unordered_map<uint64_t, uint32_t>    g_botCooldown;  // bot counter -> until ms

    uint32_t g_nowMs      = 0;
    uint32_t g_sweepTimer = 0;
    uint32_t g_rateWindow = 0;
    uint32_t g_rateCount  = 0;

    std::string Key(uint8_t kind, uint64_t ident)
    {
        char c = kind == AMB_ZONE ? 'G' : (kind == AMB_GROUP ? 'P' : 'U');
        return std::string(1, c) + ":" + std::to_string(ident);
    }

    bool IsBot(Player* p)
    {
        PlayerbotAI* ai = GET_PLAYERBOT_AI(p);
        return ai && !ai->IsRealPlayer();
    }

    Player* FindByCounter(uint64_t counter)
    {
        return ObjectAccessor::FindPlayer(
            ObjectGuid::Create<HighGuid::Player>(static_cast<ObjectGuid::LowType>(counter)));
    }

    uint32_t RandSecMs(uint32_t lo, uint32_t hi)
    {
        if (hi < lo) hi = lo;
        return urand(lo, hi) * 1000u;
    }

    uint32_t NextInterval(bool active)
    {
        return active ? RandSecMs(g_PBChatAmbientFollowMin, g_PBChatAmbientFollowMax)
                      : RandSecMs(g_PBChatAmbientSeedMin, g_PBChatAmbientSeedMax);
    }

    bool Active(Ctx const& c)
    {
        return c.lastLineMs && (g_nowMs - c.lastLineMs) <= g_PBChatAmbientActiveWindow * 1000u;
    }

    Ctx& Ensure(uint8_t kind, uint64_t ident, uint64_t anchorCounter)
    {
        std::string key = Key(kind, ident);
        auto it = g_ctx.find(key);
        if (it == g_ctx.end())
        {
            Ctx c;
            c.kind = kind;
            c.ident = ident;
            c.anchor = anchorCounter;
            c.nextEmitMs = g_nowMs + RandSecMs(g_PBChatAmbientSeedMin, g_PBChatAmbientSeedMax);
            it = g_ctx.emplace(std::move(key), std::move(c)).first;
        }
        else
        {
            it->second.eligible = true;
            it->second.anchor = anchorCounter;
        }
        return it->second;
    }

    void PushLine(Ctx& c, std::string const& speaker, std::string const& text)
    {
        c.buffer.push_back(Line{ speaker, text });
        while (c.buffer.size() > g_PBChatAmbientBufferLen)
            c.buffer.pop_front();
    }

    void Sweep()
    {
        for (auto& [key, c] : g_ctx)
            c.eligible = false;

        for (auto const& pair : ObjectAccessor::GetPlayers())
        {
            Player* p = pair.second;
            if (!p || !p->IsInWorld() || IsBot(p))
                continue;
            uint64_t anchor = p->GetGUID().GetCounter();
            if (g_PBChatAmbientGeneral)
                Ensure(AMB_ZONE, p->GetZoneId(), anchor);
            if (g_PBChatAmbientGroup)
                if (Group* g = p->GetGroup())
                    Ensure(AMB_GROUP, g->GetGUID().GetRawValue(), anchor);
            if (g_PBChatAmbientGuild)
                if (uint32 gid = p->GetGuildId())
                    Ensure(AMB_GUILD, gid, anchor);
        }

        for (auto it = g_ctx.begin(); it != g_ctx.end();)
            it = it->second.eligible ? std::next(it) : g_ctx.erase(it);
    }

    void CollectCandidates(Ctx const& c, Player* anchor, std::vector<Player*>& out)
    {
        if (c.kind == AMB_GROUP)
        {
            Group* g = anchor->GetGroup();
            if (!g)
                return;
            for (GroupReference* r = g->GetFirstMember(); r; r = r->next())
            {
                Player* m = r->GetSource();
                if (m && m->IsInWorld() && m->IsAlive() && IsBot(m))
                    out.push_back(m);
            }
            return;
        }
        for (auto const& pair : ObjectAccessor::GetPlayers())
        {
            Player* p = pair.second;
            if (!p || !p->IsInWorld() || !p->IsAlive() || !IsBot(p))
                continue;
            if (c.kind == AMB_ZONE && p->GetZoneId() == static_cast<uint32>(c.ident))
                out.push_back(p);
            else if (c.kind == AMB_GUILD && p->GetGuildId() == static_cast<uint32>(c.ident))
                out.push_back(p);
        }
    }

    PBChatChannel ChannelForCtx(Ctx const& c, Player* anchor)
    {
        if (c.kind == AMB_ZONE)
            return PBChatChannel::General;
        if (c.kind == AMB_GUILD)
            return PBChatChannel::Guild;
        Group* g = anchor->GetGroup();
        return (g && g->isRaidGroup()) ? PBChatChannel::Raid : PBChatChannel::Party;
    }

    int PickMode(bool haveBuffer, bool haveEvent)
    {
        uint32_t wG = g_PBChatAmbientWGeneric;
        uint32_t wR = haveBuffer ? g_PBChatAmbientWReact  : 0;
        uint32_t wF = g_PBChatAmbientWFlavor;
        uint32_t wE = haveEvent  ? g_PBChatAmbientWEvent  : 0;
        uint32_t total = wG + wR + wF + wE;
        if (!total)
            return PBChatterAmbientPrompt::MODE_GENERIC;
        uint32_t roll = urand(0, total - 1);
        if (roll < wG) return PBChatterAmbientPrompt::MODE_GENERIC; roll -= wG;
        if (roll < wR) return PBChatterAmbientPrompt::MODE_REACT;   roll -= wR;
        if (roll < wF) return PBChatterAmbientPrompt::MODE_FLAVOR;
        return PBChatterAmbientPrompt::MODE_EVENT;
    }

    void TryEmit(Ctx& c)
    {
        if (g_PBChatAmbientMaxPerMin && g_rateCount >= g_PBChatAmbientMaxPerMin)
        {
            c.nextEmitMs = g_nowMs + 5000;
            return;
        }
        Player* anchor = FindByCounter(c.anchor);
        if (!anchor || !anchor->IsInWorld())
        {
            c.nextEmitMs = g_nowMs + 5000;
            return;
        }

        std::vector<Player*> cands;
        CollectCandidates(c, anchor, cands);

        std::vector<Player*> pool;
        for (Player* b : cands)
        {
            uint64_t counter = b->GetGUID().GetCounter();
            if (counter == c.lastAuthorBot)
                continue; // no immediate self-reply
            auto cit = g_botCooldown.find(counter);
            if (cit != g_botCooldown.end() && g_nowMs < cit->second)
                continue;
            pool.push_back(b);
        }
        if (pool.empty())
        {
            c.nextEmitMs = g_nowMs + NextInterval(Active(c));
            return;
        }

        Player* bot = pool[urand(0, pool.size() - 1)];

        bool haveBuffer = !c.buffer.empty();
        std::string eventHint;
        bool haveEvent = PBChatterEvents::Take(bot->GetGUID().GetCounter(), g_nowMs, eventHint);
        int mode = PickMode(haveBuffer, haveEvent);

        std::vector<std::pair<std::string, std::string>> recent;
        for (Line const& l : c.buffer)
            recent.emplace_back(l.speaker, l.text);

        PBChatJob job;
        job.botGuid          = bot->GetGUID().GetCounter();
        job.playerGuid       = 0;
        job.channel          = ChannelForCtx(c, anchor);
        job.systemPrompt     = g_PBChatSystemPrompt;
        job.prompt           = PBChatterAmbientPrompt::Build(mode, bot, c.kind, recent, eventHint);
        job.ambient          = true;
        job.ambientKind      = c.kind;
        job.ambientIdent     = c.ident;
        job.anchorPlayerGuid = c.anchor;

        bool active = Active(c);
        if (PBChatterQueue::TrySubmitAmbient(std::move(job)))
        {
            ++g_rateCount;
            g_botCooldown[bot->GetGUID().GetCounter()] =
                g_nowMs + g_PBChatAmbientPerBotCooldown * 1000u;
            c.nextEmitMs = g_nowMs + NextInterval(active);
        }
        else
        {
            c.nextEmitMs = g_nowMs + 3000; // queue busy with reactive work; retry soon
        }
    }
}

uint32_t PBChatterAmbient::NowMs()
{
    return g_nowMs;
}

void PBChatterAmbient::Tick(uint32_t diff)
{
    if (!g_PBChatEnable || !g_PBChatAmbientEnable)
        return;

    g_nowMs += diff;

    g_rateWindow += diff;
    if (g_rateWindow >= 60000)
    {
        g_rateWindow = 0;
        g_rateCount = 0;
    }

    g_sweepTimer += diff;
    if (g_sweepTimer >= 3000)
    {
        g_sweepTimer = 0;
        Sweep();
    }

    for (auto& [key, c] : g_ctx)
    {
        if (!c.eligible)
            continue;
        if (g_nowMs < c.cooldownUntilMs)
            continue;
        if (g_nowMs < c.nextEmitMs)
            continue;
        TryEmit(c);
    }
}

void PBChatterAmbient::OnPlayerLine(uint8_t kind, uint64_t ident, uint64_t anchorPlayerGuid,
                                    std::string const& speaker, std::string const& text)
{
    if (!g_PBChatEnable || !g_PBChatAmbientEnable)
        return;

    Ctx& c = Ensure(kind, ident, anchorPlayerGuid);
    PushLine(c, speaker, text);
    c.botStreak = 0;
    c.cooldownUntilMs = 0;
    c.lastAuthorBot = 0;
    c.lastLineMs = g_nowMs;

    uint32_t follow = g_nowMs + RandSecMs(g_PBChatAmbientFollowMin, g_PBChatAmbientFollowMax);
    if (c.nextEmitMs > follow)
        c.nextEmitMs = follow; // let a bot answer quickly
}

void PBChatterAmbient::OnBotLineDispatched(uint8_t kind, uint64_t ident, uint64_t botGuidCounter,
                                           std::string const& speaker, std::string const& text)
{
    auto it = g_ctx.find(Key(kind, ident));
    if (it == g_ctx.end())
        return;
    Ctx& c = it->second;
    PushLine(c, speaker, text);
    c.lastLineMs = g_nowMs;
    c.lastAuthorBot = botGuidCounter;
    ++c.botStreak;
    if (static_cast<uint32>(c.botStreak) >= g_PBChatAmbientBotStreakMax)
    {
        c.cooldownUntilMs = g_nowMs + g_PBChatAmbientCooldown * 1000u;
        c.botStreak = 0;
    }
}
