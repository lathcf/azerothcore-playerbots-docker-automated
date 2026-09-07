#ifndef MOD_ERA_TALENT_CONTENT_H
#define MOD_ERA_TALENT_CONTENT_H
#include "Define.h"
#include <string>
#include <vector>
#include <unordered_map>

struct EraTalentNode
{
    uint32 id = 0;
    uint8  eraId = 0;
    uint8  classId = 0;
    uint8  tab = 0;
    uint8  tierRow = 0;
    uint8  col = 0;
    uint8  maxRank = 0;
    uint32 prereqTalentId = 0;
    uint8  prereqPoints = 0;
    std::vector<uint32> rankSpell;   // index 0 = rank 1's grantedSpellId (0 if display-only)
};

class EraTalentContent
{
public:
    static EraTalentContent* instance();
    void Load();
    const EraTalentNode* Node(uint32 talentId) const;
    std::vector<const EraTalentNode*> NodesFor(uint8 eraId, uint8 classId) const;
    size_t Count() const { return _nodes.size(); }
    std::string const& GenerationStamp() const { return _generationStamp; }
private:
    std::unordered_map<uint32, EraTalentNode> _nodes;
    std::string _generationStamp;
};
#define sEraTalentContent EraTalentContent::instance()
#endif
