#!/usr/bin/env python3
"""fetch_calc_icons.py — pull authentic Vanilla talent ICONS (and the real tree BACKGROUNDS)
for a class, so authoring a new era-talents tree never leaves `icon: ''` or a guessed
`background:` token again.

Two authoritative sources, combined:
  * ICONS  — the maladr0it/classic-talent-calculator repo (`src/trees/<Class>/data.ts`),
             which lists `icon: icons["<basename>"]` per talent node. It is Vanilla-era and its
             talent set matches ours 1:1 (names + order). Cross-validated 100% against wowhead +
             the local WotLK Spell.dbc during the warlock icon pass (2026-08-16).
  * BACKGROUNDS — the static `TalentTab.dbc` BackgroundFile table below (real
             `Interface\\TalentFrame\\<basename>` file basenames). NOT taken from the calculator,
             whose background assets are its own screenshots keyed by lowercase tab name. The DBC
             basenames DO NOT always match the tab name — e.g. Warlock kept its vanilla-beta names
             (Affliction->WarlockCurses, Demonology->WarlockSummoning), Paladin Retribution is
             PaladinCombat, Druid Feral is DruidFeralCombat. Never guess these from the tab name.

Icon basenames are case-insensitive in the 3.3.5a client (the addon prepends `Interface\\Icons\\`),
so the lowercase the calculator returns works verbatim in the class YAML.

Usage:
    python3 tools/fetch_calc_icons.py Warlock            # print backgrounds + per-node icons
    python3 tools/fetch_calc_icons.py Warlock --check era-data/vanilla/warlock.yaml
                                                         # diff the calc against a class YAML

Stdlib + curl only (no network libs). Requires `curl` on PATH.
"""
import argparse
import re
import subprocess
import sys

RAW = "https://raw.githubusercontent.com/maladr0it/classic-talent-calculator/master/src/trees/{cls}/data.ts"

# Real Interface\TalentFrame\<BackgroundFile> basenames from 3.3.5a TalentTab.dbc, in tab order
# (spec 1/2/3). These are STATIC WotLK client data — verify against TalentTab.dbc if ever unsure.
BACKGROUNDS = {
    "Warrior":     ["WarriorArms", "WarriorFury", "WarriorProtection"],
    "Paladin":     ["PaladinHoly", "PaladinProtection", "PaladinCombat"],   # Ret = "PaladinCombat"
    "Hunter":      ["HunterBeastMastery", "HunterMarksmanship", "HunterSurvival"],
    "Rogue":       ["RogueAssassination", "RogueCombat", "RogueSubtlety"],
    "Priest":      ["PriestDiscipline", "PriestHoly", "PriestShadow"],
    "DeathKnight": ["DeathKnightBlood", "DeathKnightFrost", "DeathKnightUnholy"],
    "Shaman":      ["ShamanElementalCombat", "ShamanEnhancement", "ShamanRestoration"],
    "Mage":        ["MageArcane", "MageFire", "MageFrost"],
    "Warlock":     ["WarlockCurses", "WarlockSummoning", "WarlockDestruction"],  # Aff/Demo keep beta names
    "Druid":       ["DruidBalance", "DruidFeralCombat", "DruidRestoration"],
}


def fetch(cls):
    url = RAW.format(cls=cls)
    try:
        out = subprocess.run(["curl", "-fsS", "-A", "Mozilla/5.0", url],
                             capture_output=True, text=True, check=True)
    except subprocess.CalledProcessError as e:
        sys.exit(f"fetch failed for {cls}: {e.stderr.strip() or e}")
    return out.stdout


def parse(txt):
    """Return [(tree_name, [(node_name, icon), ...]), ...] in file order.

    A tree header block has `name: "X"` + `background:` + `icon:` (no `pos:`). A talent node
    block has `name` + `pos:` + `icon: icons["<basename>"]`.
    """
    trees = []
    # tree headers: `Xxx: {\n name: "Tree", background: ...` — capture the tree display name
    for m in re.finditer(r'name:\s*"([^"]+)",\s*\n\s*background:', txt):
        trees.append((m.group(1), []))
    # per-node (name, icon) where the block contains pos: (i.e. a real talent, not a tree header)
    nodes = [(n, i) for n, mid, i in
             re.findall(r'name:\s*"([^"]+)"(.*?)icon:\s*icons\["([^"]+)"\]', txt, re.S)
             if 'pos:' in mid]
    # the calculator emits trees in order and all of a tree's nodes before the next tree header;
    # re-walk the text to assign each node to its preceding tree header.
    order = []
    for m in re.finditer(r'name:\s*"([^"]+)",\s*\n\s*background:'
                         r'|name:\s*"([^"]+)"(?=(?:(?!name:).)*?pos:)', txt, re.S):
        if m.group(1) is not None:
            order.append(("TREE", m.group(1)))
        else:
            order.append(("NODE", m.group(2)))
    icon_by_node = dict(nodes)
    result, cur = [], None
    for kind, name in order:
        if kind == "TREE":
            cur = (name, [])
            result.append(cur)
        elif cur is not None and name in icon_by_node:
            cur[1].append((name, icon_by_node[name]))
    return result


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("cls", help="class name, e.g. Warlock (matches src/trees/<Class>)")
    ap.add_argument("--check", metavar="YAML", help="diff the calc icons against a class YAML")
    a = ap.parse_args()
    cls = a.cls.capitalize() if a.cls.islower() else a.cls
    trees = parse(fetch(cls))
    if not trees:
        sys.exit(f"no talent data parsed for {cls}")
    bgs = BACKGROUNDS.get(cls, ["<UNKNOWN — fill from TalentTab.dbc>"] * len(trees))

    if a.check:
        import yaml
        d = yaml.safe_load(open(a.check))
        want = {}
        for t in d["tabs"]:
            for n in t["nodes"]:
                want[n["name"]] = n.get("icon", "")
        calc = {name: icon for _, nodes in trees for name, icon in nodes}
        bad = 0
        for name, icon in calc.items():
            got = want.get(name)
            if got is None:
                print(f"  [yaml missing node] {name}")
                bad += 1
            elif str(got).lower() != icon.lower():
                print(f"  [icon mismatch]  {name}: yaml={got!r} calc={icon!r}")
                bad += 1
        # backgrounds
        for i, t in enumerate(d["tabs"]):
            exp = bgs[i] if i < len(bgs) else "?"
            if str(t.get("background", "")).lower() != exp.lower():
                print(f"  [bg mismatch]    tab {t.get('key')} {t.get('name')}: "
                      f"yaml={t.get('background')!r} expected={exp!r}")
                bad += 1
        print(f"{'OK — icons+backgrounds match' if not bad else f'{bad} difference(s)'} "
              f"({len(calc)} nodes)")
        sys.exit(1 if bad else 0)

    # print mode: ready to paste into the class YAML
    for i, (tree, nodes) in enumerate(trees):
        bg = bgs[i] if i < len(bgs) else "?"
        print(f"\n# ── {tree}  (tab {i+1})   background: {bg}")
        for name, icon in nodes:
            print(f"  {name:32} icon: {icon}")
    total = sum(len(n) for _, n in trees)
    print(f"\n# {len(trees)} trees, {total} nodes. Backgrounds are real Interface\\TalentFrame basenames.")


if __name__ == "__main__":
    main()
