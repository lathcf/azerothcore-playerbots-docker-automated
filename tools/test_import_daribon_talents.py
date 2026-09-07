import importlib.util, pathlib, yaml

def _mod():
    spec = importlib.util.spec_from_file_location("imp", "tools/import_daribon_talents.py")
    m = importlib.util.module_from_spec(spec); spec.loader.exec_module(m); return m

DATA_JS = r'''
tree[i] = "Arcane"; i++;
tree[i] = "Fire"; i++;
tree[i] = "Frost"; i++;
talent[i] = [0, "Arcane Subtlety", 2, 1, 1]; i++;
talent[i] = [0, "Arcane Focus", 5, 2, 1]; i++;
talent[i] = [0, "Arcane Mind", 5, 3, 5, [getTalentID("Arcane Focus"),1]]; i++;
//Arcane Subtlety - Arcane
rank[i] = [ "Reduces resist by 5.", "Reduces resist by 10.", ]; i++;
//Arcane Focus
rank[i] = [ "r1","r2","r3","r4","r5", ]; i++;
//Arcane Mind
rank[i] = [ "a1","a2","a3","a4","a5", ]; i++;
'''

def test_parse_positions_and_prereqs(tmp_path):
    m = _mod()
    d = m.parse(DATA_JS, era=0, cls=8, era_name="Vanilla", class_name="MAGE",
                icons={}, id_base=18001)
    nodes = {n["name"]: n for tab in d["tabs"] for n in tab["nodes"]}
    af = nodes["Arcane Focus"]
    assert af["tab"] == 1 and af["tier"] == 0 and af["col"] == 1   # col2/row1 -> 0-based (1,0)
    assert af["maxRank"] == 5 and len(af["tooltip"]) == 5
    assert af["prereqPoints"] == 0                                  # tier-0 node -> 5*tier == 0
    am = nodes["Arcane Mind"]
    assert am["tier"] == 4 and am["col"] == 2                       # row5/col3 -> (4,2)
    assert am["prereqTalentId"] == af["id"] and am["prereqPoints"] == 20  # 5 * tier(4)
    assert "grants" not in af and "mechanic" not in af
    assert nodes["Arcane Subtlety"]["id"] == 18001


def test_clean_tooltip_strips_html_and_trainer_block():
    m = _mod()
    dirty = "Reduces cast by 0.5 sec.<br><br>&nbsp;Trainable Ranks Listed Below:<br>Rank 2: 150 Mana"
    assert m._clean_tooltip(dirty) == "Reduces cast by 0.5 sec."


def test_unresolved_prereq_warns_and_defaults_to_zero(capsys):
    m = _mod()
    data_js = r'''
tree[i] = "Arcane"; i++;
talent[i] = [0, "Orphan Talent", 1, 1, 1, [getTalentID("Nonexistent Prereq"),1]]; i++;
'''
    d = m.parse(data_js, era=0, cls=8, era_name="Vanilla", class_name="MAGE",
                icons={}, id_base=18001)
    node = d["tabs"][0]["nodes"][0]
    assert node["prereqTalentId"] == 0
    err = capsys.readouterr().err
    assert "WARNING" in err
