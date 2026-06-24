"""MySQL world-data access. All reads go through _query so it is unit-testable."""
from __future__ import annotations

from typing import Optional

import pymysql

NPC_FLAGS = {
    "vendor": 0x80,
    "trainer": 0x10,
    "repair": 0x1000,
    "flight_master": 0x2000,
    "innkeeper": 0x10000,
    "banker": 0x20000,
    "auctioneer": 0x200000,
}
GO_TYPE_MAILBOX = 19


class Db:
    def __init__(self, settings=None):
        self._settings = settings
        self._conn = None

    # --- low-level seam (overridden in tests) ---------------------------------
    def _connect(self):
        s = self._settings
        return pymysql.connect(
            host=s.db_host, port=s.db_port, user=s.db_user,
            password=s.db_pass, database=s.db_world,
            cursorclass=pymysql.cursors.DictCursor, autocommit=True,
            connect_timeout=5, read_timeout=10,
        )

    def _query(self, sql: str, params: dict) -> list[dict]:
        # Reconnect on demand; world data is read-only so this stays simple.
        if self._conn is None or not self._conn.open:
            self._conn = self._connect()
        try:
            with self._conn.cursor() as cur:
                cur.execute(sql, params)
                return list(cur.fetchall())
        except pymysql.err.OperationalError:
            self._conn = self._connect()
            with self._conn.cursor() as cur:
                cur.execute(sql, params)
                return list(cur.fetchall())

    # --- service NPCs ---------------------------------------------------------
    def service_npcs(self, map_id: int, flag: int, subname_like: Optional[str]) -> list[dict]:
        sql = (
            "SELECT ct.entry AS entry, ct.name AS name, ct.subname AS subname, "
            "c.position_x AS x, c.position_y AS y "
            "FROM creature c JOIN creature_template ct ON c.id1 = ct.entry "
            "WHERE c.map = %(map)s AND (ct.npcflag & %(flag)s) <> 0"
        )
        params = {"map": map_id, "flag": flag}
        if subname_like:
            sql += " AND ct.subname LIKE %(subname)s"
            params["subname"] = f"%{subname_like}%"
        sql += " LIMIT 400"
        return self._query(sql, params)

    def mailboxes(self, map_id: int) -> list[dict]:
        sql = (
            "SELECT gt.name AS name, g.position_x AS x, g.position_y AS y "
            "FROM gameobject g JOIN gameobject_template gt ON g.id = gt.entry "
            "WHERE g.map = %(map)s AND gt.type = %(type)s LIMIT 400"
        )
        return self._query(sql, {"map": map_id, "type": GO_TYPE_MAILBOX})

    # --- items ----------------------------------------------------------------
    def item_by_name(self, name: str) -> Optional[dict]:
        sql = (
            "SELECT entry, name, Quality, SellPrice, RequiredLevel, ItemLevel "
            "FROM item_template WHERE name LIKE %(name)s "
            "ORDER BY (name = %(exact)s) DESC, CHAR_LENGTH(name) ASC LIMIT 1"
        )
        rows = self._query(sql, {"name": f"%{name}%", "exact": name})
        return rows[0] if rows else None

    def item_drop_sources(self, item_id: int) -> list[dict]:
        sql = (
            "SELECT ct.name AS name, clt.Chance AS chance "
            "FROM creature_loot_template clt "
            "JOIN creature_template ct ON clt.Entry = ct.entry "
            "WHERE clt.Item = %(item)s ORDER BY clt.Chance DESC LIMIT 3"
        )
        return self._query(sql, {"item": item_id})

    # --- quests ---------------------------------------------------------------
    def quest_by_name(self, name: str) -> Optional[dict]:
        sql = (
            "SELECT ID AS id, LogTitle AS title, LogDescription AS objectives "
            "FROM quest_template WHERE LogTitle LIKE %(name)s "
            "ORDER BY CHAR_LENGTH(LogTitle) ASC LIMIT 1"
        )
        rows = self._query(sql, {"name": f"%{name}%"})
        return rows[0] if rows else None

    def quest_by_id(self, quest_id: int) -> Optional[dict]:
        sql = ("SELECT ID AS id, LogTitle AS title, LogDescription AS objectives "
               "FROM quest_template WHERE ID = %(id)s LIMIT 1")
        rows = self._query(sql, {"id": quest_id})
        return rows[0] if rows else None

    def quest_turnin(self, quest_id: int) -> Optional[dict]:
        sql = (
            "SELECT ct.entry AS entry, ct.name AS name, c.map AS map, "
            "c.position_x AS x, c.position_y AS y "
            "FROM creature_questender qe "
            "JOIN creature_template ct ON qe.id = ct.entry "
            "JOIN creature c ON c.id1 = ct.entry "
            "WHERE qe.quest = %(quest)s LIMIT 1"
        )
        rows = self._query(sql, {"quest": quest_id})
        return rows[0] if rows else None

    def creature_by_name(self, name: str) -> Optional[dict]:
        sql = ("SELECT entry, name FROM creature_template WHERE name LIKE %(name)s "
               "ORDER BY (name = %(exact)s) DESC, CHAR_LENGTH(name) ASC LIMIT 1")
        rows = self._query(sql, {"name": f"%{name}%", "exact": name})
        return rows[0] if rows else None

    def spawn_for_entry(self, entry: int) -> Optional[dict]:
        sql = ("SELECT map, position_x AS x, position_y AS y FROM creature "
               "WHERE id1 = %(entry)s LIMIT 1")
        rows = self._query(sql, {"entry": entry})
        return rows[0] if rows else None

    def area_for_entry(self, entry: int) -> Optional[dict]:
        sql = ("SELECT area_name, zone_name FROM mod_chatter_npc_area "
               "WHERE creature_entry = %(entry)s LIMIT 1")
        rows = self._query(sql, {"entry": entry})
        if not rows:
            return None
        r = rows[0]
        return {"area_name": r.get("area_name") or "", "zone_name": r.get("zone_name") or ""}
