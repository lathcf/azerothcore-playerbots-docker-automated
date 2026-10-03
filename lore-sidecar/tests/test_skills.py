import skills


class FakeDb:
    def __init__(self, **rows):
        self.rows = rows

    def service_npcs(self, map_id, flag, subname_like, team=None):
        return self.rows.get("service", [])

    def service_npc_places(self, flag, subname_like, team=None):
        return self.rows.get("places", [])

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

    def place_by_name(self, name):
        return self.rows.get("place")


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
        def service_npcs(self, map_id, flag, subname_like, team=None):
            captured["subname"] = subname_like
            return [{"name": "Brock", "subname": "Mining Trainer", "x": 10.0, "y": 0.0}]

    facts = skills.dispatch("find_service_npc",
                            {"service": "profession_trainer", "profession": "mining"},
                            BOT, Cap())
    assert captured["subname"].lower() == "mining"
    assert facts["name"] == "Brock"


def test_find_service_none_when_empty():
    assert skills.dispatch("find_service_npc", {"service": "vendor"}, BOT, FakeDb()) is None


def test_find_service_flags_cross_region_result_as_not_nearby():
    # Map 530 holds Outland AND the far-flung Blood Elf zones (Eversong/Silvermoon) on one
    # map id, so raw 2D "nearest" can land ~17000y away in another region. That precise
    # distance/direction is meaningless across the gap — report not-nearby, keep the name.
    db = FakeDb(service=[
        {"name": "Quaedl", "subname": "Paladin Trainer", "x": 15000.0, "y": 8000.0},
    ])
    bot = {**BOT, "map": 530, "x": 0.0, "y": 0.0, "class": "paladin"}
    facts = skills.dispatch("find_service_npc",
                            {"service": "class_trainer", "class": "paladin"}, bot, db)
    assert facts is not None
    assert facts.get("not_nearby") is True
    assert "distance_yards" not in facts
    assert "direction" not in facts
    assert facts["name"] == "Quaedl"


def test_find_service_prefers_in_region_over_cross_region():
    # A genuinely-close NPC must still win normally even when a cross-region one exists.
    db = FakeDb(service=[
        {"name": "Far Eversong Trainer", "subname": "Paladin Trainer", "x": 15000.0, "y": 8000.0},
        {"name": "Honor Hold Trainer", "subname": "Paladin Trainer", "x": 120.0, "y": 0.0},
    ])
    bot = {**BOT, "map": 530, "x": 0.0, "y": 0.0, "class": "paladin"}
    facts = skills.dispatch("find_service_npc",
                            {"service": "class_trainer", "class": "paladin"}, bot, db)
    assert facts["name"] == "Honor Hold Trainer"
    assert facts.get("not_nearby") is not True
    assert facts["distance_yards"] == 120


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


def test_where_is_place_area_resolves_zone_and_region():
    # Falconwing Square is a town in Eversong Woods; on map 530 it's across the gulf from
    # Outland, so no heading — just zone + correct region (NOT "Outland").
    db = NpcFakeDb(place={"area_name": "Falconwing Square", "zone_name": "Eversong Woods",
                          "map": 530, "x": 9000.0, "y": 9000.0})
    bot = {**BOT, "map": 530, "x": 0.0, "y": 0.0}
    facts = skills.dispatch("where_is_npc", {"npc": "Falconwing Square"}, bot, db)
    assert facts["place"] == "Falconwing Square"
    assert facts["zone"] == "Eversong Woods"
    assert facts["region"] == "the far north of the Eastern Kingdoms (Quel'Thalas)"
    assert facts["same_map"] is True
    assert "direction" not in facts
    assert facts["not_nearby"] is True   # same map, across the gulf -> far off, no heading


def test_where_is_place_cross_map_gives_region_no_heading():
    # Place on a different map entirely: region answers it; no heading, not flagged not_nearby.
    db = NpcFakeDb(place={"area_name": "Falconwing Square", "zone_name": "Eversong Woods",
                          "map": 530, "x": 0.0, "y": 0.0})
    bot = {**BOT, "map": 0, "x": 0.0, "y": 0.0}
    facts = skills.dispatch("where_is_npc", {"npc": "Falconwing Square"}, bot, db)
    assert facts["region"] == "the far north of the Eastern Kingdoms (Quel'Thalas)"
    assert facts["same_map"] is False
    assert "direction" not in facts and "not_nearby" not in facts


def test_where_is_place_zone_omits_zone_field_and_gives_heading_when_close():
    # Asking for the zone itself: place IS the zone, no redundant zone field; same map and
    # within range -> include a heading.
    db = NpcFakeDb(place={"area_name": "Sunstrider Isle", "zone_name": "Eversong Woods",
                          "map": 530, "x": 50.0, "y": 0.0})
    bot = {**BOT, "map": 530, "x": 0.0, "y": 0.0}
    facts = skills.dispatch("where_is_npc", {"npc": "Eversong Woods"}, bot, db)
    assert facts["place"] == "Eversong Woods"
    assert "zone" not in facts
    assert facts["region"] == "the far north of the Eastern Kingdoms (Quel'Thalas)"
    assert facts["same_map"] is True
    assert facts["direction"] == "north"


def test_where_is_creature_wins_over_place():
    db = NpcFakeDb(creature={"entry": 222, "name": "Brock Stoneseeker"},
                   spawn={"map": 0, "x": 100.0, "y": 0.0},
                   area={"area_name": "Lakeshire", "zone_name": "Redridge Mountains"},
                   place={"area_name": "Should Not Use", "zone_name": "Nope",
                          "map": 530, "x": 0.0, "y": 0.0})
    facts = skills.dispatch("where_is_npc", {"npc": "brock"}, BOT, db)
    assert facts["npc"] == "Brock Stoneseeker"
    assert "place" not in facts


def test_where_is_neither_creature_nor_place_returns_none():
    assert skills.dispatch("where_is_npc", {"npc": "nonexistent"}, BOT, NpcFakeDb()) is None


# --- asker-relative origin ------------------------------------------------------------------

ASKER = {"name": "Lewis", "level": 20, "class": "paladin", "faction": "Horde",
         "map": 1, "x": 1000.0, "y": 0.0, "z": 0.0, "zone": "The Barrens"}


class RecDb(AreaFakeDb):
    def __init__(self, **rows):
        super().__init__(**rows)
        self.calls = []

    def service_npcs(self, map_id, flag, subname_like, team=None):
        self.calls.append(("service", map_id, subname_like, team))
        return self.rows.get("service", [])

    def service_npc_places(self, flag, subname_like, team=None):
        self.calls.append(("places", subname_like, team))
        return self.rows.get("places", [])


def test_find_service_measures_from_asker_not_bot():
    db = RecDb(service=[{"name": "Near Asker", "subname": "General Goods", "x": 1050.0, "y": 0.0},
                        {"name": "Near Bot", "subname": "General Goods", "x": 10.0, "y": 0.0}])
    facts = skills.dispatch("find_service_npc", {"service": "vendor"}, BOT, db, asker=ASKER)
    assert facts["name"] == "Near Asker"
    assert facts["distance_yards"] == 50 and facts["direction"] == "north"
    # asker's map + team, and an unnamed class trainer would use the ASKER's class
    assert db.calls[0] == ("service", 1, None, 1)


def test_find_service_class_trainer_defaults_to_asker_class():
    db = RecDb(service=[{"name": "T", "subname": "Paladin Trainer", "x": 1000.0, "y": 0.0}])
    skills.dispatch("find_service_npc", {"service": "class_trainer"}, BOT, db, asker=ASKER)
    assert db.calls[0][2] == "paladin"


def test_find_service_asker_without_position_falls_back_to_bot():
    db = RecDb(service=[{"name": "V", "subname": "", "x": 10.0, "y": 0.0}])
    facts = skills.dispatch("find_service_npc", {"service": "vendor"}, BOT, db,
                            asker={"name": "Lewis", "faction": "Horde"})
    assert db.calls[0] == ("service", 0, None, 0)  # bot map 0, bot faction Alliance
    assert facts["distance_yards"] == 10


def test_where_is_npc_and_turnin_measure_from_asker():
    db = NpcFakeDb(creature={"entry": 1, "name": "Brock"},
                   spawn={"map": 1, "x": 1100.0, "y": 0.0}, area=None)
    facts = skills.dispatch("where_is_npc", {"npc": "brock"}, BOT, db, asker=ASKER)
    assert facts["same_map"] is True and facts["distance_yards"] == 100
    pq = [{"id": 26, "title": "Red Linen Goods",
           "objectives": [{"text": "x", "have": 1, "need": 1, "done": True}]}]
    db2 = AreaFakeDb(turnin={"name": "Sarah", "map": 1, "x": 1000.0, "y": 200.0}, area=None)
    facts2 = skills.dispatch("quest_info", {"quest": "red linen"}, BOT, db2, pq, asker=ASKER)
    assert facts2["turnin"]["distance_yards"] == 200 and facts2["turnin"]["direction"] == "west"


def test_where_to_level_uses_asker_level_and_faction():
    facts = skills.dispatch("where_to_level", {}, BOT, FakeDb(), asker=ASKER)
    assert facts["level"] == 20
    assert "The Barrens" in facts["zones"]  # Horde 11-20 band, not the bot's level-34 Alliance one


# --- world-wide 'try' fallback ---------------------------------------------------------------

def test_find_service_world_fallback_when_nothing_on_map():
    places = [
        {"zone_name": "Dustwallow Marsh", "area_name": "Theramore Isle", "n": 9},
        {"zone_name": "Ironforge", "area_name": "Ironforge", "n": 3},
        {"zone_name": "Stormwind City", "area_name": "Stormwind City", "n": 3},
        {"zone_name": "Elwynn Forest", "area_name": "Goldshire", "n": 1},
        {"zone_name": "Dalaran", "area_name": "Dalaran", "n": 1},
    ]
    alliance = {**ASKER, "faction": "Alliance", "map": 571}
    db = RecDb(service=[], places=places)
    facts = skills.dispatch("find_service_npc", {"service": "class_trainer", "class": "paladin"},
                            BOT, db, asker=alliance)
    assert facts == {"not_nearby": True, "service": "paladin trainer",
                     "try": ["Stormwind City", "Ironforge", "Dalaran"]}
    assert db.calls[1] == ("places", "paladin", 0)


def test_find_service_world_fallback_on_cross_region_nearest():
    db = RecDb(service=[{"name": "Far", "subname": "Paladin Trainer", "x": 20000.0, "y": 0.0}],
               places=[{"zone_name": "Orgrimmar", "area_name": "Orgrimmar", "n": 1}])
    facts = skills.dispatch("find_service_npc", {"service": "class_trainer", "class": "paladin"},
                            BOT, db, asker=ASKER)
    assert facts["try"] == ["Orgrimmar"] and facts["not_nearby"] is True


def test_find_service_none_only_when_nothing_anywhere():
    db = RecDb(service=[], places=[])
    assert skills.dispatch("find_service_npc", {"service": "banker"}, BOT, db, asker=ASKER) is None


def test_rank_try_places_capitals_hubs_then_count_enemy_last():
    rows = [
        {"zone_name": "Orgrimmar", "area_name": "Orgrimmar", "n": 50},  # enemy capital
        {"zone_name": "Wetlands", "area_name": "Menethil Harbor", "n": 2},
        {"zone_name": "Dustwallow Marsh", "area_name": "Theramore Isle", "n": 4},
        {"zone_name": "Shattrath City", "area_name": "Shattrath City", "n": 1},
        {"zone_name": "The Exodar", "area_name": "The Exodar", "n": 1},
    ]
    assert skills.rank_try_places(rows, "Alliance", limit=5) == [
        "The Exodar", "Shattrath City", "Theramore Isle (Dustwallow Marsh)",
        "Menethil Harbor (Wetlands)", "Orgrimmar"]


def test_mailbox_has_no_world_fallback():
    db = RecDb(mailboxes=[], places=[{"zone_name": "X", "area_name": "Y", "n": 1}])
    assert skills.dispatch("find_service_npc", {"service": "mailbox"}, BOT, db, asker=ASKER) is None


def test_describe_ask():
    assert skills.describe_ask("find_service_npc", {"service": "flight_master"}) == "flight master"
    assert skills.describe_ask("find_service_npc",
                               {"service": "profession_trainer", "profession": "mining"}) == "mining trainer"
    assert skills.describe_ask("where_is_npc", {"npc": "Thrall"}) == "Thrall"
    assert skills.describe_ask("quest_info", {}) == "that quest"
