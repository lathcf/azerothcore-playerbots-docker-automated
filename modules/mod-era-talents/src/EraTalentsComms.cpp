#include "EraTalentsComms.h"
#include "EraTalents.h"
#include "EraTalentContent.h"
#include "EraTalentIP.h"
#include "EraTalentsConfig.h"
#include "ScriptMgr.h"
#include "Player.h"
#include "SharedDefines.h"

#include <sstream>
#include <vector>
#include <string>

// ERATAL addon protocol.
//
// Receive: OnPlayerBeforeSendChatMessage fires for EVERY chat send, including the bots'
// own addon traffic and every other addon prefix in play -- guard on Enabled() first
// (module hooks fire regardless of the enable flag) then on lang==LANG_ADDON and the
// "ERATAL" prefix before touching anything else. `type` is left alone; the addon
// channel keeps whatever chat type the client happened to send it as (observed as
// CHAT_MSG_WHISPER in the Task 1 spike), so no type check.
//
// Send: p->Whisper(text, LANG_ADDON, p) is the only delivery path that doesn't crash a
// 3.3.5a client -- CHAT_MSG_ADDON as a msgtype is fatal client-side. The client splits
// the received text on the first '\t' into (prefix, body), so every chunk is sent as
// "ERATAL\t" + body.

namespace
{
    // Leaves headroom under the 255-byte addon cap for the "ERATAL\t" prefix (7 bytes)
    // plus per-packet overhead elsewhere in the chat pipeline.
    constexpr size_t kMaxChunkBody = 230;
}

namespace EraTalentsComms
{
    std::vector<std::string> BuildSync(Player* p)
    {
        EraId era = EraFromIP(p);
        int availPts = EraTalents::AvailablePoints(p, era);

        // `managed` (1/0) is server-authoritative: it tells the addon whether to REPLACE the native
        // talent frame with our window. Only an era with authored trees is managed; a TBC character
        // (no trees yet) sends managed=0 and keeps Blizzard's WotLK frame. Header layout:
        //   "SYNC <era> <managed> <availPts> [<seq>/<total>] <id:rank,...>"
        int managed = EraHasTalentTrees(era) ? 1 : 0;
        std::string header = "SYNC " + std::to_string(uint32(era)) + " " + std::to_string(managed)
                           + " " + std::to_string(availPts) + " ";

        // Collect only SPENT talents -- an unspent node has nothing to sync.
        std::string list;
        for (const EraTalentNode* node : sEraTalentContent->NodesFor(uint8(era), p->getClass()))
        {
            uint8 rank = EraTalents::CurrentRank(p, era, node->id);
            if (rank == 0)
                continue;
            list += std::to_string(node->id) + ":" + std::to_string(uint32(rank)) + ",";
        }
        if (!list.empty())
            list.pop_back();   // trim trailing comma

        std::vector<std::string> chunks;

        // Fits in one chunk: the common case (PoC has at most a handful of spent talents).
        if (header.size() + list.size() <= kMaxChunkBody)
        {
            chunks.push_back(header + list);
            return chunks;
        }

        // Split the comma-separated talent list across multiple chunks. Each chunk gets a
        // "<seq>/<total>" sentinel inserted below so the addon's ParseSync ACCUMULATES across
        // chunks -- a sentinel-less SYNC resets the client rank table, so without it every chunk
        // but the last would be dropped. Budget leaves room for the sentinel ("NN/NN " ~= 8 bytes).
        size_t budget = (kMaxChunkBody > header.size() + 8) ? (kMaxChunkBody - header.size() - 8) : 1;
        std::string current;
        size_t start = 0;
        while (start < list.size())
        {
            size_t comma = list.find(',', start);
            std::string entry = (comma == std::string::npos) ? list.substr(start) : list.substr(start, comma - start);

            if (!current.empty() && current.size() + 1 + entry.size() > budget)
            {
                chunks.push_back(header + current);
                current.clear();
            }
            if (!current.empty())
                current += ",";
            current += entry;

            start = (comma == std::string::npos) ? list.size() : comma + 1;
        }
        if (!current.empty())
            chunks.push_back(header + current);

        if (chunks.empty())
            chunks.push_back(header);   // shouldn't happen (list empty already returned above), but stay safe

        // Insert the "<seq>/<total> " sentinel right after the header so the addon accumulates
        // instead of resetting on each chunk. Single-chunk SYNCs (the common case) never reach
        // here and stay sentinel-less (ParseSync treats a sentinel-less message as a full reset).
        for (size_t i = 0; i < chunks.size(); ++i)
        {
            std::string listPart = chunks[i].substr(header.size());
            chunks[i] = header + std::to_string(i + 1) + "/" + std::to_string(chunks.size()) + " " + listPart;
        }

        return chunks;
    }

    void SendSync(Player* p)
    {
        for (std::string const& body : BuildSync(p))
            p->Whisper(std::string("ERATAL\t") + body, LANG_ADDON, p);

        // Generation canary: the addon compares this stamp (from era_talent_meta, i.e. the
        // imported SQL generation) against the client MPQ's sentinel spell 932999 and warns
        // the player when the installed patch-V.mpq is a different generation.
        std::string const& stamp = sEraTalentContent->GenerationStamp();
        if (!stamp.empty())
            p->Whisper(std::string("ERATAL\tGEN ") + stamp, LANG_ADDON, p);
    }
}

class era_talents_comms : public PlayerScript
{
public:
    era_talents_comms() : PlayerScript("era_talents_comms") {}

    void OnPlayerBeforeSendChatMessage(Player* p, uint32& /*type*/, uint32& lang, std::string& msg) override
    {
        if (!sEraTalentsConfig->Enabled() || lang != LANG_ADDON)
            return;

        size_t tab = msg.find('\t');
        if (tab == std::string::npos)
            return;
        if (msg.compare(0, tab, "ERATAL") != 0)
            return;

        std::string body = msg.substr(tab + 1);
        std::istringstream iss(body);
        std::string verb;
        iss >> verb;

        if (verb == "LEARN")
        {
            uint32 talentId = 0;
            iss >> talentId;
            std::string err;
            EraTalents::TryLearn(p, talentId, err);   // engine re-validates authoritatively; client just gets fresh SYNC
        }
        else if (verb == "HELLO")
        {
            // no-op: just resync below -- the addon sends HELLO on login/reload to pull state.
        }
        else
        {
            return;
        }

        EraTalentsComms::SendSync(p);   // always answer with fresh state
    }
};

void AddSC_era_talents_comms()
{
    new era_talents_comms();
}
