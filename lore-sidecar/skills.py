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


def _nearest_facts(rows: list[dict], bot: dict) -> Optional[dict]:
    row, dist = geo.nearest(rows, float(bot["x"]), float(bot["y"]))
    if row is None:
        return None
    return {
        "name": row["name"],
        "subname": row.get("subname") or "",
        "distance_yards": int(round(dist)),
        "direction": geo.direction(float(bot["x"]), float(bot["y"]),
                                   float(row["x"]), float(row["y"])),
        "_entry": row.get("entry"),
    }


def find_service_npc(entities: dict, bot: dict, db, player_quests=None) -> Optional[dict]:
    service = (entities.get("service") or "").lower()
    map_id = int(bot["map"])
    if service == "mailbox":
        rows = db.mailboxes(map_id)
    elif service in _FLAG_SERVICES:
        rows = db.service_npcs(map_id, _FLAG_SERVICES[service], None)
    elif service == "profession_trainer":
        prof = (entities.get("profession") or "").strip()
        rows = db.service_npcs(map_id, NPC_FLAGS["trainer"], prof or None)
    elif service == "class_trainer":
        klass = (entities.get("class") or bot.get("class") or "").strip()
        rows = db.service_npcs(map_id, NPC_FLAGS["trainer"], klass or None)
    else:
        return None
    facts = _nearest_facts(rows, bot)
    if facts:
        entry = facts.pop("_entry", None)
        _attach_area(facts, entry, db)
    return facts


def item_info(entities: dict, bot: dict, db, player_quests=None) -> Optional[dict]:
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


def quest_info(entities: dict, bot: dict, db, player_quests=None) -> Optional[dict]:
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
        if int(t["map"]) == int(bot["map"]):
            turnin = {"name": t["name"],
                      "distance_yards": int(round(geo.distance2d(
                          float(bot["x"]), float(bot["y"]), float(t["x"]), float(t["y"])))),
                      "direction": geo.direction(float(bot["x"]), float(bot["y"]),
                                                 float(t["x"]), float(t["y"]))}
        else:
            turnin = {"name": t["name"], "distance_yards": None, "direction": "another zone"}
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
        if t and int(t["map"]) == int(bot["map"]):
            out["turnin"] = {
                "name": t["name"],
                "distance_yards": int(round(geo.distance2d(
                    float(bot["x"]), float(bot["y"]), float(t["x"]), float(t["y"])))),
                "direction": geo.direction(float(bot["x"]), float(bot["y"]),
                                           float(t["x"]), float(t["y"])),
            }
        elif t:
            out["turnin"] = {"name": t["name"], "distance_yards": None, "direction": "another zone"}
        else:
            return None
        _attach_area(out["turnin"], t.get("entry"), db)
    return out


def dungeon_info(entities: dict, bot: dict, db, player_quests=None) -> Optional[dict]:
    d = curated.dungeon_lookup(entities.get("dungeon") or "")
    if not d:
        return None
    return {**d, "aspect": entities.get("aspect", "general")}


def where_to_level(entities: dict, bot: dict, db, player_quests=None) -> Optional[dict]:
    zones = curated.leveling_zones(int(bot["level"]), bot.get("faction", "Alliance"))
    return {"level": int(bot["level"]), "zones": zones} if zones else None


def where_is_npc(entities: dict, bot: dict, db, player_quests=None) -> Optional[dict]:
    name = (entities.get("npc") or "").strip()
    if not name:
        return None
    c = db.creature_by_name(name)
    if not c:
        return None
    spawn = db.spawn_for_entry(c["entry"])
    if not spawn:
        return None
    area = db.area_for_entry(c["entry"]) or {}
    facts = {"npc": c["name"],
             "town": area.get("area_name") or "",
             "zone": area.get("zone_name") or ""}
    if int(spawn["map"]) == int(bot["map"]):
        facts["same_map"] = True
        facts["direction"] = geo.direction(float(bot["x"]), float(bot["y"]),
                                           float(spawn["x"]), float(spawn["y"]))
        facts["distance_yards"] = int(round(geo.distance2d(
            float(bot["x"]), float(bot["y"]), float(spawn["x"]), float(spawn["y"]))))
    else:
        facts["same_map"] = False
        facts["continent"] = curated.continent_for(int(spawn["map"]))
    return facts


_REGISTRY = {
    "find_service_npc": find_service_npc,
    "item_info": item_info,
    "quest_info": quest_info,
    "dungeon_info": dungeon_info,
    "where_to_level": where_to_level,
    "where_is_npc": where_is_npc,
}


def dispatch(skill: str, entities: dict, bot: dict, db, player_quests=None) -> Optional[dict]:
    handler = _REGISTRY.get(skill)
    if handler is None:
        return None
    return handler(entities or {}, bot, db, player_quests)
