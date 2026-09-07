#include "EraBandClassifier.h"
#include "EraBandAllowlist.gen.h"
#include "EraTalentBots.h"
#include "EraTalentContent.h"
#include "EraTalents.h"
#include "Log.h"
#include "Player.h"
#include "SpellMgr.h"
#include "StringFormat.h"
#include <algorithm>
#include "Errors.h"
#include <unordered_map>

namespace
{
    struct Owner { EraId era; uint8 cls; uint32 nodeId; uint8 rank; };
    std::unordered_map<uint32, std::vector<Owner>> s_owners;   // band id -> every node grant of it
    bool s_built = false;

    char const* EraName(EraId e)
    {
        switch (e) { case ERA_VANILLA: return "Vanilla"; case ERA_TBC: return "TBC"; default: return "WotLK"; }
    }

    bool InBand(uint32 id) { return id >= EraTalents::ERA_CUSTOM_BAND_LOW && id < EraTalents::ERA_CUSTOM_BAND_HIGH; }

    // Rule 1. Returns true when legit; otherwise fills `reason` with the nearest owner's story and
    // returns false. `hadOwner` reports whether ANY owner exists (drives ORPHAN_NODE vs fall-through).
    bool NodeLegit(Player* p, EraId era, uint32 id, bool& hadOwner, std::string& reason)
    {
        auto it = s_owners.find(id);
        hadOwner = it != s_owners.end() && !it->second.empty();
        if (!hadOwner)
            return false;
        uint8 cls = p->getClass();
        Owner const* nearest = nullptr;   // same era+class > same class > anything
        for (Owner const& o : it->second)
        {
            if (o.era == era && o.cls == cls)
            {
                uint8 have = EraTalents::CurrentRank(p, era, o.nodeId);
                if (have == o.rank)
                    return true;
                nearest = &o;
                reason = Acore::StringFormat("{} node {} rank {} grant, node is at rank {}",
                                             EraName(era), o.nodeId, uint32(o.rank), uint32(have));
            }
            else if (!nearest || (o.cls == cls && nearest->cls != cls))
            {
                nearest = &o;
                reason = (o.cls == cls)
                    ? Acore::StringFormat("{} node {} grant held by a {} character", EraName(o.era), o.nodeId, EraName(era))
                    : Acore::StringFormat("class-{} node {} grant held by a class-{} character", uint32(o.cls), o.nodeId, uint32(cls));
            }
        }
        return false;
    }
}

namespace EraBandClassifier
{
    char const* VerdictName(Verdict v)
    {
        switch (v)
        {
            case Verdict::LEGIT_NODE:       return "LEGIT_NODE";
            case Verdict::LEGIT_CHAIN:      return "LEGIT_CHAIN";
            case Verdict::LEGIT_ALLOWLIST:  return "LEGIT_ALLOWLIST";
            case Verdict::ORPHAN_NODE:      return "ORPHAN_NODE";
            case Verdict::ORPHAN_CHAIN:     return "ORPHAN_CHAIN";
            case Verdict::ORPHAN_ALLOWLIST: return "ORPHAN_ALLOWLIST";
            default:                        return "UNCLASSIFIED";
        }
    }

    void BuildIndex()
    {
        s_owners.clear();
        static const uint8 kClasses[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 11 };
        for (uint8 e = 0; e <= uint8(ERA_WOTLK); ++e)
            for (uint8 cls : kClasses)
                for (const EraTalentNode* n : sEraTalentContent->NodesFor(e, cls))
                    for (size_t r = 0; r < n->rankSpell.size(); ++r)
                        if (n->rankSpell[r] != 0 && InBand(n->rankSpell[r]))
                            s_owners[n->rankSpell[r]].push_back({ EraId(e), cls, n->id, uint8(r + 1) });
        s_built = true;
        LOG_INFO("module", "[mod-era-talents] band classifier: {} node-grant ids indexed, {} allowlist entries (allowlist sha {})",
                 s_owners.size(), kEraBandAllowlistCount, kEraBandAllowlistHash);
    }

    size_t IndexedIds() { return s_owners.size(); }
    size_t AllowlistEntries() { return kEraBandAllowlistCount; }
    char const* AllowlistHash() { return kEraBandAllowlistHash; }

    Result Classify(Player* p, EraId era, uint32 id)
    {
        ASSERT(s_built, "EraBandClassifier::Classify before BuildIndex");   // core ASSERT: live in RelWithDebInfo (bare assert is compiled out by -DNDEBUG)
        if (!p)
            return { Verdict::UNCLASSIFIED, "no player" };
        if (!InBand(id))
            return { Verdict::UNCLASSIFIED, "not a band id" };

        // Rule 1 — node grant.
        bool hadOwner = false; std::string nodeReason;
        if (NodeLegit(p, era, id, hadOwner, nodeReason))
            return { Verdict::LEGIT_NODE, "current-era node grant" };

        // Rule 2 — spell_ranks member above a node-granted r1 (root must be a band id).
        bool rootHadOwner = false; std::string chainReason;
        uint32 root = sSpellMgr->GetFirstSpellInChain(id);
        if (root != id && InBand(root))
        {
            if (NodeLegit(p, era, root, rootHadOwner, chainReason))
                return { Verdict::LEGIT_CHAIN, Acore::StringFormat("rank above node-granted chain root {}", root) };
        }

        // Rule 3 — allowlist. First entry whose era/class/node all pass wins.
        bool matchedEntry = false; std::string allowReason;
        uint8 cls = p->getClass();
        for (size_t i = 0; i < kEraBandAllowlistCount; ++i)
        {
            EraBandAllowEntry const& e = kEraBandAllowlist[i];
            if (id < e.lo || id > e.hi)
                continue;
            matchedEntry = true;
            if (!(e.eraMask & (1u << uint8(era))))
            {
                if (allowReason.empty()) allowReason = Acore::StringFormat("allowlisted ({}) but not for {}", e.note, EraName(era));
                continue;
            }
            if (!(e.classMask & (1u << cls)))
            {
                if (allowReason.empty()) allowReason = Acore::StringFormat("allowlisted ({}) but not for class {}", e.note, uint32(cls));
                continue;
            }
            uint32 node = e.nodeByEra[uint8(era)];
            if (node != 0 && EraTalents::CurrentRank(p, era, node) == 0)
            {
                if (allowReason.empty()) allowReason = Acore::StringFormat("allowlisted ({}) but gated on {} node {}, at rank 0", e.note, EraName(era), node);
                continue;
            }
            return { Verdict::LEGIT_ALLOWLIST, Acore::StringFormat("allowlisted: {}", e.note) };
        }

        // Nothing legit. Orphan reason by priority.
        if (hadOwner)
            return { Verdict::ORPHAN_NODE, nodeReason };
        if (rootHadOwner)
            return { Verdict::ORPHAN_CHAIN, Acore::StringFormat("chain root {} not granted: {}", root, chainReason) };
        if (matchedEntry)
            return { Verdict::ORPHAN_ALLOWLIST, allowReason };
        return { Verdict::UNCLASSIFIED, "in no era's rank table, no spell_ranks chain, no allowlist entry" };
    }

    std::vector<uint32> HeldBandSpells(Player* p)
    {
        std::vector<uint32> out;
        if (!p)
            return out;
        for (auto const& kv : p->GetSpellMap())
            if (InBand(kv.first) && p->HasSpell(kv.first))
                out.push_back(kv.first);
        std::sort(out.begin(), out.end());
        return out;
    }

    uint32 Sweep(Player* p, EraId era, StripCallback const& onStrip)
    {
        if (!p)
            return 0;
        uint32 stripped = 0;
        for (uint32 id : HeldBandSpells(p))
        {
            if (!p->HasSpell(id))          // a previous strip may have cascaded this chain rank away
                continue;
            Result r = Classify(p, era, id);
            if (IsLegit(r.verdict))
                continue;
            p->removeSpell(id, SPEC_MASK_ALL, false);
            ++stripped;
            if (r.verdict == Verdict::UNCLASSIFIED)
                LOG_WARN("module", "[mod-era-talents] band sweep stripped {} from {} ({}, era {}): {} — {}",
                         id, p->GetName(), p->GetGUID().ToString(), EraName(era), VerdictName(r.verdict), r.reason);
            else if (EraTalentBots::IsBot(p))
                LOG_DEBUG("module", "[mod-era-talents] band sweep stripped {} from {} ({}, era {}): {} — {}",
                         id, p->GetName(), p->GetGUID().ToString(), EraName(era), VerdictName(r.verdict), r.reason);
            else
                LOG_INFO("module", "[mod-era-talents] band sweep stripped {} from {} ({}, era {}): {} — {}",
                         id, p->GetName(), p->GetGUID().ToString(), EraName(era), VerdictName(r.verdict), r.reason);
            if (onStrip)
                onStrip(id, r);
        }
        return stripped;
    }
}
