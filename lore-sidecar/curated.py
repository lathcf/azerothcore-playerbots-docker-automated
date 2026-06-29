"""Static WotLK reference data the game DB encodes awkwardly. Extend freely."""
from __future__ import annotations

import re
from typing import Optional

_DUNGEONS = [
    {"name": "The Deadmines", "min_level": 15, "max_level": 21,
     "location": "Westfall (Alliance)",
     "bosses": ["Rhahk'Zor", "Sneed", "Mr. Smite", "Captain Greenskin", "Edwin VanCleef"]},
    {"name": "Wailing Caverns", "min_level": 15, "max_level": 25,
     "location": "Northern Barrens (Horde)",
     "bosses": ["Lady Anacondra", "Lord Cobrahn", "Mutanus the Devourer"]},
    {"name": "Shadowfang Keep", "min_level": 16, "max_level": 26,
     "location": "Silverpine Forest",
     "bosses": ["Baron Ashbury", "Commander Springvale", "Lord Godfrey"]},
    {"name": "Scarlet Monastery", "min_level": 26, "max_level": 45,
     "location": "Tirisfal Glades",
     "bosses": ["Herod", "Arcanist Doan", "Scarlet Commander Mograine", "High Inquisitor Whitemane"]},
    {"name": "Utgarde Keep", "min_level": 70, "max_level": 72,
     "location": "Howling Fjord (Northrend)",
     "bosses": ["Prince Keleseth", "Skarvald and Dalronn", "Ingvar the Plunderer"]},
    {"name": "The Nexus", "min_level": 71, "max_level": 73,
     "location": "Coldarra, Borean Tundra (Northrend)",
     "bosses": ["Grand Magus Telestra", "Anomalus", "Ormorok", "Keristrasza"]},
    {"name": "Gundrak", "min_level": 76, "max_level": 78,
     "location": "Zul'Drak (Northrend)",
     "bosses": ["Slad'ran", "Drakkari Colossus", "Moorabi", "Gal'darah"]},
]

# Level → leveling zones. Each band is (max_level, {faction: [zones]}). 'Both' merges in.
_BANDS = [
    (10, {"Both": ["your starting zone"]}),
    (20, {"Alliance": ["Westfall", "Loch Modan", "Darkshore"],
          "Horde": ["The Barrens", "Silverpine Forest", "Ghostlands"]}),
    (30, {"Alliance": ["Redridge Mountains", "Duskwood", "Wetlands"],
          "Horde": ["Stonetalon Mountains", "Hillsbrad Foothills", "Thousand Needles"]}),
    (40, {"Both": ["Arathi Highlands", "Desolace", "Stranglethorn Vale", "Dustwallow Marsh"]}),
    (50, {"Both": ["Tanaris", "Feralas", "The Hinterlands", "Searing Gorge"]}),
    (60, {"Both": ["Un'Goro Crater", "Silithus", "Winterspring", "Eastern/Western Plaguelands"]}),
    (68, {"Both": ["Hellfire Peninsula", "Zangarmarsh", "Terokkar Forest", "Nagrand"]}),
    (78, {"Both": ["Borean Tundra", "Howling Fjord", "Dragonblight", "Grizzly Hills", "Zul'Drak"]}),
    (80, {"Both": ["Sholazar Basin", "The Storm Peaks", "Icecrown"]}),
]


def _norm(s: str) -> str:
    return re.sub(r"\s+", " ", s.strip().lower())


def dungeon_lookup(name: str) -> Optional[dict]:
    n = _norm(name)
    if not n:
        return None
    for d in _DUNGEONS:
        dn = _norm(d["name"])
        if n == dn or n in dn or dn in n:
            return d
    return None


def leveling_zones(level: int, faction: str) -> list[str]:
    for max_level, zones in _BANDS:
        if level <= max_level:
            out = list(zones.get("Both", []))
            out += zones.get(faction, [])
            return out
    # Above the top band: return the top band's zones (never empty).
    top = _BANDS[-1][1]
    return list(top.get("Both", [])) + top.get(faction, [])


_CONTINENTS = {0: "Eastern Kingdoms", 1: "Kalimdor", 530: "Outland", 571: "Northrend"}


def continent_for(map_id: int) -> str:
    return _CONTINENTS.get(int(map_id), "")


# Zones on a discontiguous map id that continent_for would mislabel. Map 530 ("Outland")
# also carries Quel'Thalas (Blood Elf) and the Draenei isles; name their true lore region.
# Keys are the DB zone_name (from mod_chatter_npc_area), lowercased via _norm — match that
# exact string when adding entries, not a DBC display variant.
_REGION_OVERRIDES = {
    "eversong woods": "the far north of the Eastern Kingdoms (Quel'Thalas)",
    "ghostlands": "the far north of the Eastern Kingdoms (Quel'Thalas)",
    "silvermoon city": "the far north of the Eastern Kingdoms (Quel'Thalas)",
    "isle of quel'danas": "the far north of the Eastern Kingdoms (Quel'Thalas)",
    "sunstrider isle": "the far north of the Eastern Kingdoms (Quel'Thalas)",
    "azuremyst isle": "the islands off the coast of Kalimdor",
    "bloodmyst isle": "the islands off the coast of Kalimdor",
    "the exodar": "the islands off the coast of Kalimdor",
    "ammen vale": "the islands off the coast of Kalimdor",
}


def region_for(zone_name: str, map_id: int) -> str:
    """Broad lore region for a zone. Overrides the discontiguous map-530 zones (Quel'Thalas,
    Draenei isles) that continent_for would mislabel; otherwise falls back to the continent."""
    override = _REGION_OVERRIDES.get(_norm(zone_name or ""))
    return override if override else continent_for(map_id)
