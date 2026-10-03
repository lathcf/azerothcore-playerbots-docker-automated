from db import Db, NPC_FLAGS


class RecordingDb(Db):
    def __init__(self, rows):
        # bypass real connection
        self._rows = rows
        self.calls = []

    def _query(self, sql, params):
        self.calls.append((sql, params))
        return self._rows


def test_service_npcs_uses_flag_and_map():
    db = RecordingDb([{"name": "Brock Stoneseeker", "subname": "Mining Trainer",
                       "x": 1.0, "y": 2.0}])
    rows = db.service_npcs(map_id=0, flag=NPC_FLAGS["vendor"], subname_like=None)
    assert rows and rows[0]["name"] == "Brock Stoneseeker"
    sql, params = db.calls[0]
    assert "creature_template" in sql and "npcflag" in sql
    assert params["map"] == 0 and params["flag"] == NPC_FLAGS["vendor"]
    assert "subname" not in sql.lower().split("where")[1]  # no subname filter when None


def test_service_npcs_adds_subname_filter():
    db = RecordingDb([])
    db.service_npcs(map_id=1, flag=NPC_FLAGS["trainer"], subname_like="Mining")
    sql, params = db.calls[0]
    assert "subname LIKE" in sql
    assert params["subname"] == "%Mining%"


def test_item_by_name_fuzzy():
    db = RecordingDb([{"entry": 7073, "name": "Broken Fang", "Quality": 1,
                       "SellPrice": 17, "RequiredLevel": 10, "ItemLevel": 15}])
    item = db.item_by_name("broken fang")
    assert item["entry"] == 7073
    sql, params = db.calls[0]
    assert "item_template" in sql and "name LIKE" in sql
    assert params["name"] == "%broken fang%"


def test_item_by_name_miss_returns_none():
    assert RecordingDb([]).item_by_name("nope") is None


def test_mailboxes_query_uses_type_19():
    db = RecordingDb([])
    db.mailboxes(map_id=0)
    sql, params = db.calls[0]
    assert "gameobject_template" in sql and "type" in sql
    assert params["map"] == 0


def test_place_by_name_joins_area_and_creature():
    db = RecordingDb([{"area_name": "Falconwing Square", "zone_name": "Eversong Woods",
                       "map": 530, "x": 1.0, "y": 2.0}])
    row = db.place_by_name("Falconwing Square")
    assert row["zone_name"] == "Eversong Woods" and row["map"] == 530
    sql, params = db.calls[0]
    assert "mod_chatter_npc_area" in sql and "JOIN creature" in sql
    assert "c.id = a.creature_entry" in sql
    assert "a.area_name LIKE" in sql and "a.zone_name LIKE" in sql
    assert params["name"] == "%Falconwing Square%" and params["exact"] == "Falconwing Square"


def test_place_by_name_miss_returns_none():
    assert RecordingDb([]).place_by_name("nowhere") is None


def test_service_npcs_faction_filter_param():
    db = RecordingDb([])
    db.service_npcs(map_id=1, flag=NPC_FLAGS["trainer"], subname_like="Paladin", team=1)
    sql, params = db.calls[0]
    assert "LEFT JOIN mod_chatter_npc_area a ON a.creature_entry = ct.entry" in sql
    assert "(a.team IS NULL OR a.team = 2 OR a.team = %(team)s)" in sql
    assert params["team"] == 1


def test_service_npcs_no_team_no_filter():
    db = RecordingDb([])
    db.service_npcs(map_id=1, flag=NPC_FLAGS["vendor"], subname_like=None)
    sql, params = db.calls[0]
    assert "a.team" not in sql and "team" not in params


def test_service_npc_places_world_wide_grouped():
    db = RecordingDb([])
    db.service_npc_places(flag=NPC_FLAGS["trainer"], subname_like="Paladin", team=0)
    sql, params = db.calls[0]
    assert "c.map" not in sql  # every map
    assert "JOIN mod_chatter_npc_area a" in sql and "GROUP BY a.zone_name, a.area_name" in sql
    assert params == {"flag": NPC_FLAGS["trainer"], "subname": "%Paladin%", "team": 0}
