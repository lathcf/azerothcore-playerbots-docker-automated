import skills


class FakeDb:
    def __init__(self, **rows):
        self.rows = rows

    def service_npcs(self, map_id, flag, subname_like):
        return self.rows.get("service", [])

    def mailboxes(self, map_id):
        return self.rows.get("mailboxes", [])

    def item_by_name(self, name):
        return self.rows.get("item")

    def item_drop_sources(self, item_id):
        return self.rows.get("drops", [])

    def quest_by_name(self, name):
        return self.rows.get("quest")

    def quest_by_id(self, qid):
        return self.rows.get("quest")

    def quest_turnin(self, qid):
        return self.rows.get("turnin")


BOT = {"name": "Wizard", "level": 34, "class": "mage", "faction": "Alliance",
       "map": 0, "x": 0.0, "y": 0.0, "z": 0.0, "zone": "Elwynn",
       "active_quests": [{"id": 26, "title": "Red Linen Goods"}]}


def test_find_service_vendor_returns_nearest_with_direction():
    db = FakeDb(service=[
        {"name": "Far Vendor", "subname": "General Goods", "x": 500.0, "y": 0.0},
        {"name": "Near Vendor", "subname": "General Goods", "x": 50.0, "y": 0.0},
    ])
    facts = skills.dispatch("find_service_npc", {"service": "vendor"}, BOT, db)
    assert facts["name"] == "Near Vendor"
    assert facts["direction"] == "north"
    assert facts["distance_yards"] == 50


def test_find_profession_trainer_passes_subname():
    captured = {}

    class Cap(FakeDb):
        def service_npcs(self, map_id, flag, subname_like):
            captured["subname"] = subname_like
            return [{"name": "Brock", "subname": "Mining Trainer", "x": 10.0, "y": 0.0}]

    facts = skills.dispatch("find_service_npc",
                            {"service": "profession_trainer", "profession": "mining"},
                            BOT, Cap())
    assert captured["subname"].lower() == "mining"
    assert facts["name"] == "Brock"


def test_find_service_none_when_empty():
    assert skills.dispatch("find_service_npc", {"service": "vendor"}, BOT, FakeDb()) is None


def test_find_mailbox_uses_mailboxes():
    db = FakeDb(mailboxes=[{"name": "Mailbox", "x": 5.0, "y": 0.0}])
    facts = skills.dispatch("find_service_npc", {"service": "mailbox"}, BOT, db)
    assert facts["name"] == "Mailbox"


def test_item_info_value_and_drops():
    db = FakeDb(item={"entry": 1, "name": "Broken Fang", "Quality": 1,
                      "SellPrice": 10017, "RequiredLevel": 10, "ItemLevel": 15},
                drops=[{"name": "Fang Beast", "chance": 12.5}])
    facts = skills.dispatch("item_info", {"item": "broken fang", "aspect": "general"}, BOT, db)
    assert facts["name"] == "Broken Fang"
    assert facts["sell_price"] == {"gold": 1, "silver": 0, "copper": 17}
    assert facts["drops"][0]["name"] == "Fang Beast"


def test_item_info_miss():
    assert skills.dispatch("item_info", {"item": "nope", "aspect": "general"}, BOT, FakeDb()) is None


def test_quest_info_turnin_from_active_log():
    db = FakeDb(quest={"id": 26, "title": "Red Linen Goods", "objectives": "Bring 10 linen"},
                turnin={"name": "Tailor Sarah", "map": 0, "x": 100.0, "y": 0.0})
    facts = skills.dispatch("quest_info", {"quest": "red linen", "aspect": "turnin"}, BOT, db)
    assert facts["turnin"]["name"] == "Tailor Sarah"
    assert facts["turnin"]["direction"] == "north"


def test_quest_info_objective():
    db = FakeDb(quest={"id": 26, "title": "Red Linen Goods", "objectives": "Bring 10 linen"})
    facts = skills.dispatch("quest_info", {"quest": "red linen", "aspect": "objective"}, BOT, db)
    assert "linen" in facts["objectives"].lower()


def test_dungeon_info():
    facts = skills.dispatch("dungeon_info", {"dungeon": "deadmines", "aspect": "general"}, BOT, FakeDb())
    assert facts["name"] == "The Deadmines"
    assert "Edwin VanCleef" in facts["bosses"]


def test_where_to_level():
    facts = skills.dispatch("where_to_level", {}, BOT, FakeDb())
    assert facts["level"] == 34 and facts["zones"]


def test_chitchat_and_unknown_return_none():
    assert skills.dispatch("chitchat", {}, BOT, FakeDb()) is None
    assert skills.dispatch("bogus", {}, BOT, FakeDb()) is None


def test_quest_info_player_quests_reports_incomplete_objective():
    pq = [{"id": 26, "title": "Red Linen Goods",
           "objectives": [{"text": "Bolt of Linen Cloth", "have": 3, "need": 10, "done": False}]}]
    facts = skills.dispatch("quest_info", {"quest": "red linen", "aspect": "objective"}, BOT, FakeDb(), pq)
    assert facts["title"] == "Red Linen Goods"
    assert facts["current_objective"] == {"text": "Bolt of Linen Cloth", "have": 3, "need": 10}


def test_quest_info_player_quests_all_done_gives_turnin():
    pq = [{"id": 26, "title": "Red Linen Goods",
           "objectives": [{"text": "Bolt of Linen Cloth", "have": 10, "need": 10, "done": True}]}]
    db = FakeDb(turnin={"name": "Tailor Sarah", "map": 0, "x": 100.0, "y": 0.0})
    facts = skills.dispatch("quest_info", {"quest": "red linen", "aspect": "turnin"}, BOT, db, pq)
    assert facts["turnin"]["name"] == "Tailor Sarah"
    assert facts["turnin"]["direction"] == "north"


def test_quest_info_player_quests_resolves_exact_chain_link():
    pq = [{"id": 1, "title": "The Hunt Begins",
           "objectives": [{"text": "Wolf", "have": 5, "need": 5, "done": True}]},
          {"id": 2, "title": "The Hunt Continues",
           "objectives": [{"text": "Bear", "have": 1, "need": 8, "done": False}]}]
    facts = skills.dispatch("quest_info", {"quest": "hunt continues", "aspect": "objective"}, BOT, FakeDb(), pq)
    assert facts["title"] == "The Hunt Continues"
    assert facts["current_objective"]["text"] == "Bear"


def test_quest_info_falls_back_to_legacy_without_player_quests():
    db = FakeDb(quest={"id": 26, "title": "Red Linen Goods", "objectives": "Bring 10 linen"})
    facts = skills.dispatch("quest_info", {"quest": "red linen", "aspect": "objective"}, BOT, db)
    assert "linen" in facts["objectives"].lower()


class AreaFakeDb(FakeDb):
    def area_for_entry(self, entry):
        return self.rows.get("area")  # {"area_name":..,"zone_name":..} or None


def test_find_service_includes_town_when_known():
    db = AreaFakeDb(service=[{"name": "Brock", "subname": "Mining Trainer", "x": 50.0, "y": 0.0, "entry": 777}],
                    area={"area_name": "Lakeshire", "zone_name": "Redridge Mountains"})
    facts = skills.dispatch("find_service_npc", {"service": "profession_trainer", "profession": "mining"}, BOT, db)
    assert facts["town"] == "Lakeshire"
    assert facts["zone"] == "Redridge Mountains"
    assert "_entry" not in facts


def test_find_service_omits_town_when_unknown():
    db = AreaFakeDb(service=[{"name": "Brock", "subname": "Mining Trainer", "x": 50.0, "y": 0.0, "entry": 777}],
                    area=None)
    facts = skills.dispatch("find_service_npc", {"service": "profession_trainer", "profession": "mining"}, BOT, db)
    assert facts["name"] == "Brock"
    assert "town" not in facts


def test_quest_turnin_includes_town_when_known():
    pq = [{"id": 26, "title": "Red Linen Goods",
           "objectives": [{"text": "Bolt of Linen Cloth", "have": 10, "need": 10, "done": True}]}]
    db = AreaFakeDb(turnin={"name": "Tailor Sarah", "map": 0, "x": 100.0, "y": 0.0, "entry": 888},
                    area={"area_name": "Lakeshire", "zone_name": "Redridge Mountains"})
    facts = skills.dispatch("quest_info", {"quest": "red linen"}, BOT, db, pq)
    assert facts["turnin"]["town"] == "Lakeshire"


class NpcFakeDb(FakeDb):
    def creature_by_name(self, name):
        return self.rows.get("creature")

    def spawn_for_entry(self, entry):
        return self.rows.get("spawn")

    def area_for_entry(self, entry):
        return self.rows.get("area")


def test_where_is_npc_same_map_gives_direction():
    db = NpcFakeDb(creature={"entry": 222, "name": "Brock Stoneseeker"},
                   spawn={"map": 0, "x": 100.0, "y": 0.0},
                   area={"area_name": "Lakeshire", "zone_name": "Redridge Mountains"})
    facts = skills.dispatch("where_is_npc", {"npc": "brock"}, BOT, db)
    assert facts["npc"] == "Brock Stoneseeker"
    assert facts["town"] == "Lakeshire"
    assert facts["same_map"] is True
    assert facts["direction"] == "north"


def test_where_is_npc_cross_map_gives_continent():
    db = NpcFakeDb(creature={"entry": 4949, "name": "Thrall"},
                   spawn={"map": 1, "x": 0.0, "y": 0.0},
                   area={"area_name": "Orgrimmar", "zone_name": "Durotar"})
    facts = skills.dispatch("where_is_npc", {"npc": "thrall"}, BOT, db)  # BOT is on map 0
    assert facts["same_map"] is False
    assert facts["continent"] == "Kalimdor"
    assert facts["town"] == "Orgrimmar"


def test_where_is_npc_not_found_returns_none():
    assert skills.dispatch("where_is_npc", {"npc": "nobody"}, BOT, NpcFakeDb()) is None
