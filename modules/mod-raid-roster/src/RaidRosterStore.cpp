#include "RaidRosterStore.h"
#include "DatabaseEnv.h"
#include "QueryResult.h"
#include "Field.h"

namespace RaidRosterStore
{
    bool Exists(uint32 ownerGuid)
    {
        QueryResult r = CharacterDatabase.Query(
            "SELECT 1 FROM mod_raid_roster WHERE owner_guid = {} LIMIT 1", ownerGuid);
        return r != nullptr;
    }

    std::vector<RaidRosterRow> Load(uint32 ownerGuid)
    {
        std::vector<RaidRosterRow> rows;
        QueryResult r = CharacterDatabase.Query(
            "SELECT bot_guid, class, role, spec_tab, slot_index FROM mod_raid_roster "
            "WHERE owner_guid = {} ORDER BY slot_index", ownerGuid);
        if (!r) return rows;
        do {
            Field* f = r->Fetch();
            RaidRosterRow row;
            row.botGuid = f[0].Get<uint32>();
            row.cls     = f[1].Get<uint8>();
            row.role    = f[2].Get<uint8>();
            row.specTab = f[3].Get<uint8>();
            row.slot    = f[4].Get<uint8>();
            rows.push_back(row);
        } while (r->NextRow());
        return rows;
    }

    void Replace(uint32 ownerGuid, std::vector<RaidRosterRow> const& rows)
    {
        CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
        trans->Append("DELETE FROM mod_raid_roster WHERE owner_guid = {}", ownerGuid);
        for (RaidRosterRow const& row : rows)
            trans->Append("INSERT INTO mod_raid_roster "
                "(owner_guid, bot_guid, class, role, spec_tab, slot_index) "
                "VALUES ({}, {}, {}, {}, {}, {})",
                ownerGuid, row.botGuid, row.cls, row.role, row.specTab, row.slot);
        CharacterDatabase.CommitTransaction(trans);
    }

    void Clear(uint32 ownerGuid)
    {
        CharacterDatabase.Execute("DELETE FROM mod_raid_roster WHERE owner_guid = {}", ownerGuid);
    }

    std::unordered_set<uint32> AllPinnedBots()
    {
        std::unordered_set<uint32> out;
        QueryResult r = CharacterDatabase.Query("SELECT bot_guid FROM mod_raid_roster");
        if (!r) return out;
        do { out.insert(r->Fetch()[0].Get<uint32>()); } while (r->NextRow());
        return out;
    }
}
