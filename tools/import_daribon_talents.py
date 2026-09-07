#!/usr/bin/env python3
"""Parse the authentic 1.12.1 Daribon talent calculator data into an era-data YAML skeleton.

Input: the text of a class's shared/global/talents/<class>/en/data.js (positions+tooltips) — fetch it
with: gh api "repos/Daribon/daribon.github.io/contents/shared/global/talents/<class>/en/data.js?ref=master" \
  --jq .content | base64 -d > /tmp/<class>.data.js
Schema: talent[i] = [treeIndex, "Name", maxRank, column(1-based), row(1-based), [getTalentID("Prereq"),rank]?]
        rank[i]   = parallel array of per-rank tooltip strings (same index order as talent[]).
Emits era-data shape: tab=treeIndex+1, tier=row-1, col=column-1. NO grants/mechanic (authoring adds those)."""
from __future__ import annotations
import argparse, ast, json, pathlib, re, sys, yaml

_TALENT = re.compile(r'talent\[i\]\s*=\s*(\[.*?\])\s*;', re.S)
_RANK   = re.compile(r'rank\[i\]\s*=\s*(\[.*?\])\s*;', re.S)
_TREE   = re.compile(r'tree\[i\]\s*=\s*"([^"]*)"')
_GETID  = re.compile(r'getTalentID\(\s*"([^"]*)"\s*\)')

def _py_array(js: str):
    """Turn a JS array literal into Python. getTalentID("X") -> the string "@X" sentinel; trailing commas ok."""
    js = _GETID.sub(lambda m: json.dumps("@" + m.group(1)), js)
    # Kill backslash line-continuations FIRST, or the later newline->space turns them into an
    # invalid "\ " escape (SyntaxWarning today, SyntaxError in future CPython).
    js = re.sub(r'\\\r?\n', ' ', js)
    js = js.replace("\r", " ").replace("\n", " ")
    return ast.literal_eval(js)

def _clean_tooltip(s: str) -> str:
    """Strip HTML markup and the trailing trainer block from a raw Daribon tooltip string."""
    s = s.split("Trainable Ranks", 1)[0]
    s = re.sub(r'<[^>]+>', ' ', s)
    s = s.replace("&nbsp;", " ")
    s = s.replace("\\", " ").replace("\t", " ")
    return re.sub(r'\s+', ' ', s).strip()

def parse(data_js: str, era: int, cls: int, era_name: str, class_name: str,
          icons: dict, id_base: int) -> dict:
    trees = _TREE.findall(data_js)
    raw_talents = [_py_array(m) for m in _TALENT.findall(data_js)]
    raw_ranks   = [_py_array(m) for m in _RANK.findall(data_js)]
    if len(raw_ranks) not in (0, len(raw_talents)):
        raise ValueError(f"rank[] count {len(raw_ranks)} != talent[] count {len(raw_talents)}")
    order = sorted(range(len(raw_talents)),
                   key=lambda k: (raw_talents[k][0], raw_talents[k][4], raw_talents[k][3]))
    name_to_id = {}
    for new_id, k in enumerate(order, start=id_base):
        name_to_id[raw_talents[k][1]] = new_id
    tabs = [{"key": t + 1, "name": trees[t], "background": f"{class_name.title()}{trees[t]}", "nodes": []}
            for t in range(len(trees))]
    for k in order:
        t = raw_talents[k]
        tree_i, name, max_rank, col, row = t[0], t[1], int(t[2]), int(t[3]), int(t[4])
        tier = row - 1
        pre_id = 0
        if len(t) > 5 and isinstance(t[5], list) and t[5] and isinstance(t[5][0], str) and t[5][0].startswith("@"):
            pre_name = t[5][0][1:]
            pre_id = name_to_id.get(pre_name, 0)
            if pre_id == 0:
                print(f"WARNING: prereq {pre_name!r} for talent {name!r} did not resolve to a parsed talent",
                      file=sys.stderr)
        tips = [_clean_tooltip(str(x)) for x in raw_ranks[k]] if raw_ranks else []
        node = {"id": name_to_id[name], "slug": re.sub(r'[^a-z0-9]+', '_', name.lower()).strip('_'),
                "name": name, "tab": tree_i + 1, "tier": tier, "col": col - 1,
                "icon": icons.get(name, ""), "maxRank": max_rank,
                "prereqTalentId": pre_id, "prereqPoints": 5 * tier, "tooltip": tips}
        tabs[tree_i]["nodes"].append(node)
    return {"era": era, "class": cls, "eraName": era_name, "className": class_name, "tabs": tabs}

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("data_js"); ap.add_argument("-o", "--out", required=True)
    ap.add_argument("--era", type=int, default=0); ap.add_argument("--class", dest="cls", type=int, required=True)
    ap.add_argument("--era-name", default="Vanilla"); ap.add_argument("--class-name", required=True)
    ap.add_argument("--id-base", type=int, required=True)
    a = ap.parse_args()
    d = parse(pathlib.Path(a.data_js).read_text(), a.era, a.cls, a.era_name, a.class_name, {}, a.id_base)
    pathlib.Path(a.out).write_text(yaml.safe_dump(d, sort_keys=False, width=120, allow_unicode=True))

if __name__ == "__main__":
    main()
