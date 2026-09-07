#include "EraTalentContent.h"
#include "DatabaseEnv.h"
#include "QueryResult.h"
#include "Field.h"
#include "Log.h"

#include <utility>   // std::move

EraTalentContent* EraTalentContent::instance() { static EraTalentContent i; return &i; }

void EraTalentContent::Load()
{
    _nodes.clear();
    if (QueryResult r = WorldDatabase.Query(
            "SELECT id,eraId,classId,tab,tierRow,col,maxRank,prereqTalentId,prereqPoints FROM era_talent"))
    {
        do
        {
            Field* f = r->Fetch();
            EraTalentNode n;
            n.id = f[0].Get<uint32>();
            n.eraId = f[1].Get<uint8>();
            n.classId = f[2].Get<uint8>();
            n.tab = f[3].Get<uint8>();
            n.tierRow = f[4].Get<uint8>();
            n.col = f[5].Get<uint8>();
            n.maxRank = f[6].Get<uint8>();
            n.prereqTalentId = f[7].Get<uint32>();
            n.prereqPoints = f[8].Get<uint8>();
            n.rankSpell.assign(n.maxRank, 0);
            _nodes[n.id] = std::move(n);
        } while (r->NextRow());
    }
    if (QueryResult r = WorldDatabase.Query(
            "SELECT talentId,`rank`,grantedSpellId FROM era_talent_rank"))
    {
        do
        {
            Field* f = r->Fetch();
            uint32 tid = f[0].Get<uint32>();
            uint8  rank = f[1].Get<uint8>();
            uint32 spell = f[2].Get<uint32>();
            auto it = _nodes.find(tid);
            if (it != _nodes.end() && rank >= 1 && rank <= it->second.maxRank)
                it->second.rankSpell[rank - 1] = spell;
        } while (r->NextRow());
    }
    _generationStamp.clear();
    if (QueryResult r = WorldDatabase.Query(
            "SELECT v FROM era_talent_meta WHERE k='generation'"))
        _generationStamp = (*r)[0].Get<std::string>();
    LOG_INFO("module", "[mod-era-talents] loaded {} talent nodes (generation {})",
             _nodes.size(), _generationStamp.empty() ? "<unset>" : _generationStamp);
}

const EraTalentNode* EraTalentContent::Node(uint32 talentId) const
{
    auto it = _nodes.find(talentId);
    return it == _nodes.end() ? nullptr : &it->second;
}

std::vector<const EraTalentNode*> EraTalentContent::NodesFor(uint8 eraId, uint8 classId) const
{
    std::vector<const EraTalentNode*> out;
    for (auto const& kv : _nodes)
        if (kv.second.eraId == eraId && kv.second.classId == classId)
            out.push_back(&kv.second);
    return out;
}
