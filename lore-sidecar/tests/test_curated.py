from curated import continent_for, dungeon_lookup, leveling_zones


def test_dungeon_lookup_known():
    d = dungeon_lookup("deadmines")
    assert d is not None
    assert d["min_level"] <= 18 <= d["max_level"]
    assert "Edwin VanCleef" in d["bosses"]


def test_dungeon_lookup_fuzzy_and_miss():
    assert dungeon_lookup("THE  deadmines")["name"] == "The Deadmines"
    assert dungeon_lookup("not a real dungeon") is None


def test_leveling_zones_band_and_faction():
    z = leveling_zones(34, "Alliance")
    assert isinstance(z, list) and z
    assert any("Stranglethorn" in name or "Desolace" in name or "Arathi" in name for name in z)
    # Out-of-range high level still returns the top band, never empty.
    assert leveling_zones(80, "Horde")


def test_continent_for_known_and_unknown():
    assert continent_for(0) == "Eastern Kingdoms"
    assert continent_for(1) == "Kalimdor"
    assert continent_for(571) == "Northrend"
    assert continent_for(530) == "Outland"
    assert continent_for(99999) == ""
