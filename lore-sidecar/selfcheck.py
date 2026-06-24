"""Container self-check: real classify + one query per family. Exit non-zero on failure.

Run on the server after deploy:  docker compose exec ac-lore python selfcheck.py

Probes each DB query family ONCE so a schema/column mismatch in ANY of them surfaces
here (not silently at runtime, where it would just look like the feature never works).
A query that raises is reported [FAIL] and makes the check exit non-zero; an empty
result is still [OK] (the SQL executed against the real schema without error).
"""
import sys

from config import Settings
from db import Db, NPC_FLAGS
from llm import Llm


def _probe(name, fn) -> bool:
    """Run one query; [OK] (with row count) if it executes, [FAIL] if it raises."""
    try:
        res = fn()
        if isinstance(res, list):
            n = len(res)
        elif res is None:
            n = 0
        else:
            n = 1
        print(f"  [OK]   {name}: {n} row(s)")
        return True
    except Exception as e:  # pymysql errors on a bad column/table land here
        print(f"  [FAIL] {name}: {type(e).__name__}: {e}", file=sys.stderr)
        return False


def main() -> int:
    s = Settings.from_env()
    llm = Llm(s)
    db = Db(s)

    print("classify probe:")
    try:
        intent = llm.classify("where is the nearest vendor?")
        print("  classify ->", intent)
        if intent.get("skill") != "find_service_npc":
            print("  WARN: classify did not return find_service_npc", file=sys.stderr)
    except Exception as e:
        print(f"  [FAIL] classify: {type(e).__name__}: {e}", file=sys.stderr)
        return 1

    print("db query probes (one per family — schema check):")
    ok = True
    ok &= _probe("service_npcs(vendor, map 0)", lambda: db.service_npcs(0, NPC_FLAGS["vendor"], None))
    ok &= _probe("service_npcs(trainer subname)", lambda: db.service_npcs(0, NPC_FLAGS["trainer"], "Mining"))
    ok &= _probe("mailboxes(map 0)", lambda: db.mailboxes(0))
    ok &= _probe("item_by_name('Linen Cloth')", lambda: db.item_by_name("Linen Cloth"))
    ok &= _probe("item_drop_sources(2589)", lambda: db.item_drop_sources(2589))  # 2589 = Linen Cloth
    ok &= _probe("quest_by_name('the')", lambda: db.quest_by_name("the"))
    ok &= _probe("quest_by_id(2)", lambda: db.quest_by_id(2))
    ok &= _probe("quest_turnin(2)", lambda: db.quest_turnin(2))

    if not ok:
        print("SELF-CHECK FAILED: a query raised — likely a schema/column-name mismatch in db.py "
              "for this AzerothCore fork. Cross-check with SHOW COLUMNS and adjust db.py.",
              file=sys.stderr)
        return 1
    print("self-check OK: classify ran and every query family executed without schema errors.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
