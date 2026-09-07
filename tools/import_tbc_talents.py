#!/usr/bin/env python3
"""Build era-data TBC skeleton YAMLs from the committed wago 2.5.4.44833 CSVs.

The TBC analog of import_daribon_talents.py, structurally richer: the wago Talent table carries
per-rank stock spell ids (SpellRank_0..8), so each node ships a `rankSpells:` list the survival
map (per-class Task 3) diffs against the live 3.3.5a store. Names/icons/per-rank tooltips resolve
from a local cache of Wowhead TBC tooltip JSON (era-data/_ref/tbc/tooltips/<spellid>.json,
endpoint https://nether.wowhead.com/tbc/tooltip/spell/<id>); --fetch fills the cache (browser
UA), offline runs emit `spell-<id>` placeholder names.

Usage:
  uv run --with pyyaml python tools/import_tbc_talents.py --class-id 2 \
      [--id-base 20000] [--fetch] [-o era-data/_skeletons/tbc-paladin.skeleton.yaml]

Per-class node-id slices (framework registry): paladin 20000, druid 20100, shaman 20200,
warrior 20300, rogue 20400, priest 20500, hunter 20600, warlock 20700, mage 20800."""
from __future__ import annotations
import argparse, csv, json, pathlib, re, sys, time, urllib.request
import yaml

ROOT = pathlib.Path(__file__).resolve().parent.parent
TALENT_CSV = ROOT / "era-data/_ref/tbc/Talent-2.5.4.44833.csv"
TAB_CSV = ROOT / "era-data/_ref/tbc/TalentTab-2.5.4.44833.csv"
CACHE_DIR = ROOT / "era-data/_ref/tbc/tooltips"
CLASS_TOKENS = {1: "WARRIOR", 2: "PALADIN", 3: "HUNTER", 4: "ROGUE", 5: "PRIEST",
                7: "SHAMAN", 8: "MAGE", 9: "WARLOCK", 11: "DRUID"}
ID_BASE = {2: 20000, 11: 20100, 7: 20200, 1: 20300, 4: 20400,
           5: 20500, 3: 20600, 9: 20700, 8: 20800}
UA = "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36"
WOWHEAD = "https://nether.wowhead.com/tbc/tooltip/spell/{}"


def _clean_tooltip(html: str) -> str:
    # Wowhead tooltip HTML = a header table (name, "Talent", cost/cast/cooldown lines,
    # "Requires <class>") followed by the effect prose in <div class="q">…</div>; any
    # "(Proc chance: N%)" suffix sits OUTSIDE the q-div. Only the q-div text is the talent
    # description — flattening the whole HTML baked the header junk into every TBC paladin
    # tooltip (found in real-client testing 2026-08-30). Fall back to whole-HTML flattening
    # only when no q-div exists (bare-text fixtures / non-wowhead sources).
    effect_divs = re.findall(r'<div class="q">(.*?)</div>', html or "", re.S)
    s = " ".join(effect_divs) if effect_divs else (html or "")
    s = re.sub(r"<[^>]+>", " ", s)
    s = s.replace("&nbsp;", " ").replace("&quot;", '"').replace("&amp;", "&")
    return re.sub(r"\s+", " ", s).strip()


def _tooltip_info(spell_id: int, cache_dir, fetch: bool):
    """{name, icon, tooltip} for a spell id from the cache (fetching if allowed), else None."""
    if cache_dir is None:
        return None
    p = pathlib.Path(cache_dir) / f"{spell_id}.json"
    if p.exists():
        return json.loads(p.read_text())
    if not fetch:
        return None
    # No try/except here (intentional simplification for a manually-run tool): a network error
    # aborts this --fetch batch, but every prior spell's JSON is already on disk, so re-running
    # the same command just resumes from where it left off (each cache write below is idempotent).
    req = urllib.request.Request(WOWHEAD.format(spell_id), headers={"User-Agent": UA})
    with urllib.request.urlopen(req, timeout=30) as r:
        data = json.loads(r.read().decode("utf-8"))
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(json.dumps(data))
    time.sleep(0.3)   # be a polite client — ~1400 total fetches across all classes
    return data


def _icon_name(icon: str) -> str:
    """Wowhead's TBC endpoint serves CLASSIC-era texture names for some spells (e.g.
    `classic_ability_druid_demoralizingroar`, `classic_spell_holy_blessingofprotection`). A 3.3.5a
    client has no such texture, so the talent button renders BLANK — with no error and no log line,
    which is why TBC druid node 20122 shipped broken and was only caught by a real-client pass
    (fix round 2026-08-31). Strip the prefix: the remainder is the real 3.3.5a name (the shipped
    Vanilla druid dataset uses `ability_druid_demoralizingroar` for the same talent).
    tools/era_audit.py has a matching guard so a reintroduction can never ship silently."""
    return icon[len("classic_"):] if icon.startswith("classic_") else icon


def _slug(name: str) -> str:
    return re.sub(r"[^a-z0-9]+", "_", name.lower()).strip("_")


def build(talent_csv, tab_csv, class_id: int, id_base: int,
          cache_dir=None, fetch: bool = False) -> dict:
    mask = 1 << (class_id - 1)
    with open(tab_csv, newline="") as f:
        tabs_raw = [r for r in csv.DictReader(f) if int(r["ClassMask"]) == mask]
    tabs_raw.sort(key=lambda r: int(r["OrderIndex"]))
    if len(tabs_raw) != 3:
        raise SystemExit(f"class {class_id}: expected 3 talent tabs, found {len(tabs_raw)}")
    tab_key = {int(r["ID"]): i + 1 for i, r in enumerate(tabs_raw)}   # TabID -> our 1-based key

    with open(talent_csv, newline="") as f:
        talents = [r for r in csv.DictReader(f) if int(r["TabID"]) in tab_key]
    talents.sort(key=lambda r: (tab_key[int(r["TabID"])], int(r["TierID"]), int(r["ColumnIndex"])))

    node_id_of = {int(r["ID"]): id_base + i for i, r in enumerate(talents)}
    tabs = [{"key": i + 1, "name": r["Name_lang"], "background": r["BackgroundFile"], "nodes": []}
            for i, r in enumerate(tabs_raw)]

    for r in talents:
        ranks = [int(r[f"SpellRank_{i}"]) for i in range(9) if int(r[f"SpellRank_{i}"])]
        info = _tooltip_info(ranks[0], cache_dir, fetch) if ranks else None
        name = (info or {}).get("name") or f"spell-{ranks[0] if ranks else 0}"
        icon = _icon_name((info or {}).get("icon") or "")
        tips = []
        for sid in ranks:
            ri = _tooltip_info(sid, cache_dir, fetch)
            if ri and ri.get("tooltip"):
                tips.append(_clean_tooltip(ri["tooltip"]))
        if len(tips) != len(ranks):
            tips = []   # partial cache -> emit none rather than misaligned rank text
        tier = int(r["TierID"])
        pre = int(r["PrereqTalent_0"])
        node = {
            "id": node_id_of[int(r["ID"])], "slug": _slug(name), "name": name,
            "tab": tab_key[int(r["TabID"])], "tier": tier, "col": int(r["ColumnIndex"]),
            "icon": icon, "maxRank": len(ranks),
            "prereqTalentId": node_id_of.get(pre, 0) if pre else 0,
            "prereqPoints": 5 * tier,
            "rankSpells": ranks,
            "tooltip": tips,
        }
        if pre and pre not in node_id_of:
            print(f"WARNING: talent {r['ID']}: prereq {pre} not in this class's tabs", file=sys.stderr)
        tabs[node["tab"] - 1]["nodes"].append(node)

    return {"era": 1, "class": class_id, "eraName": "TBC",
            "className": CLASS_TOKENS[class_id], "tabs": tabs}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--class-id", type=int, required=True, choices=sorted(CLASS_TOKENS))
    ap.add_argument("--id-base", type=int, default=None)
    ap.add_argument("--talent-csv", default=str(TALENT_CSV))
    ap.add_argument("--tab-csv", default=str(TAB_CSV))
    ap.add_argument("--cache-dir", default=str(CACHE_DIR))
    ap.add_argument("--fetch", action="store_true", help="fill the tooltip cache from Wowhead")
    ap.add_argument("-o", "--out", default=None)
    a = ap.parse_args()
    id_base = a.id_base if a.id_base is not None else ID_BASE[a.class_id]
    d = build(a.talent_csv, a.tab_csv, a.class_id, id_base, cache_dir=a.cache_dir, fetch=a.fetch)
    out = a.out or str(ROOT / "era-data/_skeletons" /
                       f"tbc-{CLASS_TOKENS[a.class_id].lower()}.skeleton.yaml")
    pathlib.Path(out).write_text(yaml.safe_dump(d, sort_keys=False, allow_unicode=True))
    n = sum(len(t["nodes"]) for t in d["tabs"])
    print(f"{out}: {n} nodes across {len(d['tabs'])} tabs (ids {id_base}..{id_base + n - 1})")


if __name__ == "__main__":
    main()
