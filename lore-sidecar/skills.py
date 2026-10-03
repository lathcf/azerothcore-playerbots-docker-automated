"""Skill handlers: classified intent + bot context + Db -> facts dict (or None)."""
from __future__ import annotations

from typing import Optional

import curated
import geo
from db import NPC_FLAGS

# services that map directly to an npcflag with no subname filter
_FLAG_SERVICES = {
    "vendor": NPC_FLAGS["vendor"],
    "repair": NPC_FLAGS["repair"],
    "flight_master": NPC_FLAGS["flight_master"],
    "innkeeper": NPC_FLAGS["innkeeper"],
    "banker": NPC_FLAGS["banker"],
    "auctioneer": NPC_FLAGS["auctioneer"],
}


_SERVICE_LABELS = {
    "vendor": "vendor",
    "repair": "repair vendor",
    "flight_master": "flight master",
    "innkeeper": "innkeeper",
    "banker": "banker",
    "auctioneer": "auctioneer",
    "mailbox": "mailbox",
}
_TEAM_BY_FACTION = {"Alliance": 0, "Horde": 1}
_TRY_LIMIT = 3


def _origin(bot: dict, asker: Optional[dict]) -> dict:
    """Where the question is measured from: the asking player when the payload carries its
    position (a whispered bot may be anywhere), else the bot (back-compat with an older
    worldserver that sends no `asker`). Faction/level/class come from the same unit."""
    if asker and all(asker.get(k) is not None for k in ("map", "x", "y")):
        return asker
    return bot


def _team_for(unit: dict) -> Optional[int]:
    return _TEAM_BY_FACTION.get(unit.get("faction") or "")


def service_label(entities: dict, origin: Optional[dict] = None) -> str:
    """Short human description of the asked-for service, e.g. 'paladin trainer'."""
    service = (entities.get("service") or "").lower()
    if service == "class_trainer":
        klass = (entities.get("class") or (origin or {}).get("class") or "").strip()
        return f"{klass} trainer" if klass else "class trainer"
    if service == "profession_trainer":
        prof = (entities.get("profession") or "").strip()
        return f"{prof} trainer" if prof else "profession trainer"
    return _SERVICE_LABELS.get(service, service.replace("_", " ") or "that")


def describe_ask(skill: str, entities: dict) -> str:
    """Short description of what was asked, for an honest not_found answer."""
    e = entities or {}
    if skill == "find_service_npc":
        return service_label(e)
    if skill == "item_info":
        return (e.get("item") or "").strip() or "that item"
    if skill == "quest_info":
        return (e.get("quest") or "").strip() or "that quest"
    if skill == "dungeon_info":
        return (e.get("dungeon") or "").strip() or "that dungeon"
    if skill == "where_is_npc":
        return (e.get("npc") or "").strip() or "that"
    if skill == "where_to_level":
        return "where to level"
    return "that"


def rank_try_places(rows: list[dict], faction: str, limit: int = _TRY_LIMIT) -> list[str]:
    """Rank world-wide (zone, area, n) rows into up to `limit` place names: the asker
    faction's capitals first (curated order), then the neutral hubs, then other zones by NPC
    count; the enemy faction's capitals go last (a filtered-in neutral NPC there is still a
    bad place to send someone)."""
    zones: dict[str, dict] = {}
    for r in rows:
        zone = (r.get("zone_name") or "").strip()
        area = (r.get("area_name") or "").strip()
        key = zone or area
        if not key:
            continue
        z = zones.setdefault(key, {"n": 0, "areas": {}})
        n = int(r.get("n") or 0)
        z["n"] += n
        if area:
            z["areas"][area] = z["areas"].get(area, 0) + n

    def matches(name: str, zone: str, z: dict) -> bool:
        low = name.lower()
        return zone.lower() == low or any(a.lower() == low for a in z["areas"])

    out: list[str] = []
    used: set[str] = set()

    def take_named(names: list[str]):
        for name in names:
            for zone, z in zones.items():
                if zone not in used and matches(name, zone, z):
                    used.add(zone)
                    if name not in out:
                        out.append(name)
                    break

    take_named(curated.capitals_for(faction))
    take_named(curated.NEUTRAL_HUBS)
    enemy = curated.enemy_capitals_for(faction)
    rest = sorted((zk for zk in zones if zk not in used
                   and not any(matches(c, zk, zones[zk]) for c in enemy)),
                  key=lambda zk: (-zones[zk]["n"], zk))
    for zk in rest:
        z = zones[zk]
        top = max(z["areas"].items(), key=lambda kv: (kv[1], kv[0]))[0] if z["areas"] else ""
        name = f"{top} ({zk})" if top and top.lower() != zk.lower() else zk
        if name not in out:
            out.append(name)
        used.add(zk)
    take_named(enemy)
    return out[:limit]


def _copper_to_gsc(copper: int) -> dict:
    copper = int(copper or 0)
    return {"gold": copper // 10000, "silver": (copper % 10000) // 100, "copper": copper % 100}


def _attach_area(facts: dict, entry, db) -> dict:
    """Add town/zone to a facts dict when the area table knows this NPC; else leave as-is."""
    if entry is None:
        return facts
    area = db.area_for_entry(int(entry))
    if area and (area.get("area_name") or area.get("zone_name")):
        if area.get("area_name"):
            facts["town"] = area["area_name"]
        if area.get("zone_name"):
            facts["zone"] = area["zone_name"]
    return facts


def _nearest_facts(rows: list[dict], origin: dict) -> Optional[dict]:
    row, dist = geo.nearest(rows, float(origin["x"]), float(origin["y"]))
    if row is None:
        return None
    # Cross-region artifact (see geo.SAME_REGION_MAX_YARDS): the closest match on this map is
    # actually in another region. Drop the meaningless 2D distance/direction but keep the name
    # (and the area, attached by the caller) so the bot can honestly say it's not close by.
    if dist > geo.SAME_REGION_MAX_YARDS:
        return {
            "name": row["name"],
            "subname": row.get("subname") or "",
            "not_nearby": True,
            "_entry": row.get("entry"),
        }
    return {
        "name": row["name"],
        "subname": row.get("subname") or "",
        "distance_yards": int(round(dist)),
        "direction": geo.direction(float(origin["x"]), float(origin["y"]),
                                   float(row["x"]), float(row["y"])),
        "_entry": row.get("entry"),
    }


def _turnin_facts(t: dict, origin: dict) -> dict:
    """Turn-in NPC facts: real distance/direction only when it's genuinely in this region;
    otherwise just the name (+ area, attached by the caller) — see geo.SAME_REGION_MAX_YARDS."""
    if int(t["map"]) == int(origin["map"]):
        d = geo.distance2d(float(origin["x"]), float(origin["y"]), float(t["x"]), float(t["y"]))
        if d <= geo.SAME_REGION_MAX_YARDS:
            return {"name": t["name"], "distance_yards": int(round(d)),
                    "direction": geo.direction(float(origin["x"]), float(origin["y"]),
                                               float(t["x"]), float(t["y"]))}
    return {"name": t["name"], "distance_yards": None, "direction": "another zone"}


def _world_fallback(db, flag: int, subname: Optional[str], team: Optional[int],
                    entities: dict, origin: dict) -> Optional[dict]:
    """Nothing usable near the asker: name up to _TRY_LIMIT places world-wide that have one,
    so the bot says "not around here, try X" instead of the model inventing a location."""
    rows = db.service_npc_places(flag, subname, team=team)
    places = rank_try_places(rows, origin.get("faction") or "")
    if not places:
        return None
    return {"not_nearby": True, "service": service_label(entities, origin), "try": places}


def find_service_npc(entities: dict, bot: dict, db, player_quests=None,
                     origin: Optional[dict] = None) -> Optional[dict]:
    origin = origin or bot
    service = (entities.get("service") or "").lower()
    map_id = int(origin["map"])
    team = _team_for(origin)
    flag = None
    subname = None
    if service == "mailbox":
        rows = db.mailboxes(map_id)  # gameobjects: no faction, no area table
    elif service in _FLAG_SERVICES:
        flag = _FLAG_SERVICES[service]
        rows = db.service_npcs(map_id, flag, None, team=team)
    elif service == "profession_trainer":
        flag = NPC_FLAGS["trainer"]
        subname = (entities.get("profession") or "").strip() or None
        rows = db.service_npcs(map_id, flag, subname, team=team)
    elif service == "class_trainer":
        flag = NPC_FLAGS["trainer"]
        subname = (entities.get("class") or origin.get("class") or "").strip() or None
        rows = db.service_npcs(map_id, flag, subname, team=team)
    else:
        return None
    facts = _nearest_facts(rows, origin)
    if flag is not None and (facts is None or facts.get("not_nearby")):
        fallback = _world_fallback(db, flag, subname, team, entities, origin)
        if fallback:
            return fallback
    if facts:
        entry = facts.pop("_entry", None)
        _attach_area(facts, entry, db)
    return facts


def item_info(entities: dict, bot: dict, db, player_quests=None,
              origin: Optional[dict] = None) -> Optional[dict]:
    name = (entities.get("item") or "").strip()
    if not name:
        return None
    item = db.item_by_name(name)
    if not item:
        return None
    drops = db.item_drop_sources(item["entry"])
    return {
        "name": item["name"],
        "quality": int(item.get("Quality", 0)),
        "item_level": int(item.get("ItemLevel", 0)),
        "required_level": int(item.get("RequiredLevel", 0)),
        "sell_price": _copper_to_gsc(item.get("SellPrice", 0)),
        "drops": [{"name": d["name"], "chance": round(float(d["chance"]), 1)} for d in drops],
        "aspect": entities.get("aspect", "general"),
    }


def _resolve_quest(entities: dict, bot: dict, db) -> Optional[dict]:
    name = (entities.get("quest") or "").strip().lower()
    # Prefer the bot's real active quest log.
    for q in bot.get("active_quests", []):
        title = (q.get("title") or "")
        if not name or name in title.lower():
            full = db.quest_by_id(int(q["id"]))
            if full:
                return full
    if name:
        return db.quest_by_name(name)
    return None


def _resolve_player_quest(entities: dict, player_quests: list) -> Optional[dict]:
    """Pick the player's quest the question is about: by name match, else the only active one."""
    name = (entities.get("quest") or "").strip().lower()
    if not player_quests:
        return None
    if name:
        for q in player_quests:
            if name in (q.get("title") or "").lower():
                return q
        return None
    if len(player_quests) == 1:
        return player_quests[0]
    return None


def quest_info(entities: dict, bot: dict, db, player_quests=None,
               origin: Optional[dict] = None) -> Optional[dict]:
    origin = origin or bot
    # Preferred path: the player's real quest log with live progress.
    pq = _resolve_player_quest(entities, player_quests or [])
    if pq is not None:
        objs = pq.get("objectives", [])
        incomplete = next((o for o in objs if not o.get("done")), None)
        if incomplete:  # can't turn in an unfinished quest — report what's left
            return {"title": pq.get("title", ""),
                    "current_objective": {"text": incomplete.get("text", ""),
                                          "have": incomplete.get("have", 0),
                                          "need": incomplete.get("need", 0)}}
        t = db.quest_turnin(int(pq["id"]))  # all objectives done -> turn-in
        if not t:
            return {"title": pq.get("title", ""), "ready_to_turn_in": True}
        turnin = _turnin_facts(t, origin)
        _attach_area(turnin, t.get("entry"), db)
        return {"title": pq.get("title", ""), "turnin": turnin}

    # Fallback: legacy name-based path (bot's quests / quest_by_name -> LogDescription).
    quest = _resolve_quest(entities, bot, db)
    if not quest:
        return None
    out = {"title": quest["title"], "objectives": quest.get("objectives") or "",
           "aspect": entities.get("aspect", "objective")}
    if out["aspect"] == "turnin":
        t = db.quest_turnin(int(quest["id"]))
        if not t:
            return None
        out["turnin"] = _turnin_facts(t, origin)
        _attach_area(out["turnin"], t.get("entry"), db)
    return out


def dungeon_info(entities: dict, bot: dict, db, player_quests=None,
                 origin: Optional[dict] = None) -> Optional[dict]:
    d = curated.dungeon_lookup(entities.get("dungeon") or "")
    if not d:
        return None
    return {**d, "aspect": entities.get("aspect", "general")}


def where_to_level(entities: dict, bot: dict, db, player_quests=None,
                   origin: Optional[dict] = None) -> Optional[dict]:
    origin = origin or bot
    level = int(origin.get("level") or bot["level"])
    zones = curated.leveling_zones(level, origin.get("faction") or "Alliance")
    return {"level": level, "zones": zones} if zones else None


def _place_facts(row: dict, asked: str, origin: dict) -> dict:
    """Location facts for a place (area/zone) from db.place_by_name. A heading is included only
    when the place is on the bot's map AND within range (geo.SAME_REGION_MAX_YARDS), so we never
    point across a discontiguous-map gulf."""
    area = row.get("area_name") or ""
    zone = row.get("zone_name") or ""
    # If the question named the zone itself, the place IS the zone; else it's the sub-area.
    # (area can be empty if the backfill row had a NULL area_name — degrade to zone-only.)
    if zone and asked.strip().lower() == zone.lower():
        place, show_zone = zone, ""
    else:
        place, show_zone = (area or zone), zone
    facts = {"place": place, "region": curated.region_for(zone or place, int(row["map"]))}
    if show_zone and show_zone != place:
        facts["zone"] = show_zone
    facts["same_map"] = int(row["map"]) == int(origin["map"])
    if facts["same_map"]:
        d = geo.distance2d(float(origin["x"]), float(origin["y"]), float(row["x"]), float(row["y"]))
        if d <= geo.SAME_REGION_MAX_YARDS:
            facts["direction"] = geo.direction(float(origin["x"]), float(origin["y"]),
                                               float(row["x"]), float(row["y"]))
        else:
            # Same map id but across the discontiguity gulf — symmetric with the creature path:
            # signal "far off" so the phrasing doesn't imply it's close.
            facts["not_nearby"] = True
    return facts


def where_is_npc(entities: dict, bot: dict, db, player_quests=None,
                 origin: Optional[dict] = None) -> Optional[dict]:
    origin = origin or bot
    name = (entities.get("npc") or "").strip()
    if not name:
        return None
    c = db.creature_by_name(name)
    spawn = db.spawn_for_entry(c["entry"]) if c else None
    if c and spawn:
        area = db.area_for_entry(c["entry"]) or {}
        facts = {"npc": c["name"],
                 "town": area.get("area_name") or "",
                 "zone": area.get("zone_name") or ""}
        d = (geo.distance2d(float(origin["x"]), float(origin["y"]), float(spawn["x"]), float(spawn["y"]))
             if int(spawn["map"]) == int(origin["map"]) else None)
        if d is not None and d <= geo.SAME_REGION_MAX_YARDS:
            facts["same_map"] = True
            facts["direction"] = geo.direction(float(origin["x"]), float(origin["y"]),
                                               float(spawn["x"]), float(spawn["y"]))
            facts["distance_yards"] = int(round(d))
        elif d is not None:
            # Same map id but another region (see geo.SAME_REGION_MAX_YARDS): the town/zone name
            # is the useful answer; a 2D direction across the gulf would just be wrong.
            facts["same_map"] = True
            facts["not_nearby"] = True
        else:
            facts["same_map"] = False
            facts["continent"] = curated.continent_for(int(spawn["map"]))
        return facts
    # Not a creature (or no spawn) — maybe the name is a place (zone/town).
    place = db.place_by_name(name)
    return _place_facts(place, name, origin) if place else None


_REGISTRY = {
    "find_service_npc": find_service_npc,
    "item_info": item_info,
    "quest_info": quest_info,
    "dungeon_info": dungeon_info,
    "where_to_level": where_to_level,
    "where_is_npc": where_is_npc,
}


def dispatch(skill: str, entities: dict, bot: dict, db, player_quests=None,
             asker: Optional[dict] = None) -> Optional[dict]:
    """`asker` (the whispering player, optional): location/faction/level-dependent answers are
    measured from it when it carries map/x/y; otherwise from the bot. The bot stays the voice."""
    handler = _REGISTRY.get(skill)
    if handler is None:
        return None
    return handler(entities or {}, bot, db, player_quests, origin=_origin(bot, asker))
