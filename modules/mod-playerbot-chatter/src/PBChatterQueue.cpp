#include "PBChatterQueue.h"
#include "PBChatterConfig.h"
#include "PBChatterOllama.h"
#include "PBChatterLore.h"
#include "PBChatterMemory.h"
#include "Log.h"
#include <mutex>
#include <queue>
#include <thread>

namespace
{
    std::mutex g_mutex;
    int g_running = 0;
    std::queue<PBChatJob> g_pending;

    std::mutex g_resultMutex;
    std::vector<PBChatResult> g_results;

    void RunJob(PBChatJob job);

    void StartNext()
    {
        // assumes g_mutex held
        if (g_pending.empty())
            return;
        if (g_PBChatMaxConcurrent != 0 && g_running >= (int)g_PBChatMaxConcurrent)
            return;
        PBChatJob job = std::move(g_pending.front());
        g_pending.pop();
        ++g_running;
        std::thread(RunJob, std::move(job)).detach();
    }

    void RunJob(PBChatJob job)
    {
        std::string reply;
        if (job.lore)
            reply = PBChatterLore::Ask(job.lorePayload);   // sidecar first
        if (reply.empty())                                  // disabled/miss/timeout -> reactive fallback
            reply = PBChatterOllama::Ask(job.systemPrompt, job.prompt);
        if (!reply.empty())
        {
            if (!job.ambient)
                PBChatterMemory::Append(job.botGuid, job.playerGuid, job.playerMessage, reply);
            std::lock_guard<std::mutex> lock(g_resultMutex);
            PBChatResult r;
            r.botGuid           = job.botGuid;
            r.playerGuid        = job.playerGuid;
            r.playerName        = job.playerName;
            r.channel           = job.channel;
            r.reply             = reply;
            r.ambient           = job.ambient;
            r.ambientKind       = job.ambientKind;
            r.ambientIdent      = job.ambientIdent;
            r.anchorPlayerGuid  = job.anchorPlayerGuid;
            g_results.push_back(std::move(r));
        }
        else if (g_PBChatDebug)
        {
            // Empty after a successful POST usually means the model returned no text.
            // Drop silently in normal operation; surface it when debugging.
            LOG_INFO("server.loading", "[PlayerbotChatter] Empty reply for bot {} -> no message sent.", job.botGuid);
        }
        std::lock_guard<std::mutex> lock(g_mutex);
        --g_running;
        StartNext();
    }
}

void PBChatterQueue::Submit(PBChatJob job)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    g_pending.push(std::move(job));
    StartNext();
}

std::vector<PBChatResult> PBChatterQueue::DrainResults()
{
    std::lock_guard<std::mutex> lock(g_resultMutex);
    std::vector<PBChatResult> out;
    out.swap(g_results);
    return out;
}

bool PBChatterQueue::TrySubmitAmbient(PBChatJob job)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    // Reactive-first: only run ambient when there is idle headroom and no backlog.
    if (!g_pending.empty())
        return false;
    if (g_PBChatMaxConcurrent != 0 && g_running >= (int)g_PBChatMaxConcurrent)
        return false;
    g_pending.push(std::move(job));
    StartNext();
    return true;
}
