import subprocess, sys, textwrap, pathlib, os, re
import pytest, yaml as _yaml
import importlib.util as _ilu

_COLS = [ln.split()[1] for ln in open("tools/spell_dbc_columns.txt").read().splitlines() if ln.strip()]

_G_spec = _ilu.spec_from_file_location("gen_era_talents", "tools/gen_era_talents.py")
G = _ilu.module_from_spec(_G_spec); _G_spec.loader.exec_module(G)


def _parse_values(row: str):
    """Split a '(v1, v2, ...)' VALUES tuple string into fields, respecting single-quoted strings
    (so a quoted string containing a comma isn't split)."""
    body = row.strip()
    assert body.startswith("(") and body.rstrip(";").endswith(")")
    body = body.rstrip(";")[1:-1]
    vals, cur, in_str, i = [], "", False, 0
    while i < len(body):
        ch = body[i]
        if in_str:
            if ch == "'" and body[i:i + 2] == "''":
                cur += "'"; i += 2; continue
            if ch == "'":
                in_str = False; cur += ch; i += 1; continue
            cur += ch
        else:
            if ch == "'":
                in_str = True; cur += ch
            elif ch == ",":
                vals.append(cur.strip()); cur = ""
            else:
                cur += ch
        i += 1
    vals.append(cur.strip())
    return vals


FIXTURE = textwrap.dedent(r"""
era: 0
class: 8
eraName: Vanilla
className: MAGE
tabs:
  - key: 1
    name: Arcane
    background: MageArcane
    nodes: []
  - key: 2
    name: Fire
    background: MageFire
    nodes:
      - id: 18001
        slug: imp_fireball
        name: "Improved Fireball"
        tab: 2
        tier: 0
        col: 0
        icon: "iconA"
        maxRank: 5
        prereqTalentId: 0
        prereqPoints: 0
        tooltip:
          - "Reduces the casting time of your Fireball spell by 0.1 sec."
          - "r2"
          - "r3"
          - "r4"
          - "r5"
        grants: {1: [11069], 2: [11069], 3: [12338], 4: [12338], 5: [12339]}
      - id: 18002
        slug: flame_throwing
        name: "Flame Throwing"
        tab: 2
        tier: 0
        col: 2
        icon: "iconB"
        maxRank: 2
        prereqTalentId: 0
        prereqPoints: 0
        display_only: true
        tooltip:
          - "t1"
          - "t2"
  - key: 3
    name: Frost
    background: MageFrost
    nodes: []
""")


def _run(tmp, *args):
    return subprocess.run([sys.executable, "tools/gen_era_talents.py", *args],
                          capture_output=True, text=True, cwd=os.getcwd())


def test_sql_has_node_and_rank_rows(tmp_path):
    y = tmp_path / "mage.yaml"; y.write_text(FIXTURE)
    out = tmp_path / "out.sql"
    r = _run(tmp_path, "--sql", str(y), "-o", str(out))
    assert r.returncode == 0, r.stderr
    sql = out.read_text()
    assert "INTO `era_talent`" in sql
    node_part, _, rank_part = sql.partition("INSERT INTO `era_talent_rank`")
    assert node_part.count("(18001,") == 1
    assert node_part.count("(18002,") == 1
    assert "era_talent_rank" in sql
    assert rank_part.count("(18001,1,11069)") == 1
    assert rank_part.count("(18001,5,12339)") == 1
    assert "(18002," not in rank_part  # display-only has no rank rows


def test_lua_matches_addon_schema(tmp_path):
    y = tmp_path / "mage.yaml"; y.write_text(FIXTURE)
    out = tmp_path / "MageVanilla.lua"
    r = _run(tmp_path, "--lua", str(y), "-o", str(out))
    assert r.returncode == 0, r.stderr
    lua = out.read_text()
    assert "EraTalentsData" in lua
    assert "[0]" in lua and "[8]" in lua
    assert 'eraName = "Vanilla"' in lua
    assert "tabs = {" in lua and "talents = {" in lua
    assert "18001" in lua and "18002" in lua        # all nodes incl. display-only
    assert "tier=0" in lua and "col=2" in lua
    assert "prereqTalent=0" in lua                    # NOT prereqId
    assert "icon=" in lua
    assert "tooltip={" in lua
    assert 'wired=true' in lua and 'wired=false' in lua
    # A grant node emits a per-rank `grant` map (rank -> first granted spell) for the addon's
    # SetHyperlink spell-tooltip render; the fixture's grants span two distinct spells across ranks.
    assert "grant={ [1]=11069, [2]=11069, [3]=12338, [4]=12338, [5]=12339 }" in lua
    assert "Improved Fireball" in lua
    # loadable by 3.3.5a lua
    import shutil
    if shutil.which("luac5.1"):
        assert subprocess.run(["luac5.1", "-p", str(out)]).returncode == 0


def test_lua_grant_absent_for_nongrant_nodes(tmp_path):
    """Only nodes with `grants:` emit a `grant` map. A display-only / passive-modifier node
    (no explicit grants) must NOT carry one, so the addon keeps its authored-prose tooltip path."""
    y = tmp_path / "mage.yaml"; y.write_text(FIXTURE)
    out = tmp_path / "MageVanilla.lua"
    r = _run(tmp_path, "--lua", str(y), "-o", str(out))
    assert r.returncode == 0, r.stderr
    lua = out.read_text()
    # Node 18002 is display_only with no grants -> its emitted line has no `grant=`.
    line_18002 = next(ln for ln in lua.splitlines() if "id=18002," in ln)
    assert "grant=" not in line_18002
    # Exactly one node in the fixture grants, so exactly one `grant={` appears.
    assert lua.count("grant={") == 1


def _lint(d):
    import importlib.util
    spec = importlib.util.spec_from_file_location("g", "tools/gen_era_talents.py")
    m = importlib.util.module_from_spec(spec); spec.loader.exec_module(m)
    return m.lint(d)


def test_lint_rejects_missing_prereq():
    d = _yaml.safe_load(FIXTURE)
    d["tabs"][1]["nodes"][0]["prereqTalentId"] = 99999
    with pytest.raises(ValueError, match="prereq"):
        _lint(d)


def test_lint_rejects_grid_collision():
    d = _yaml.safe_load(FIXTURE)
    d["tabs"][1]["nodes"][1]["tier"] = 0
    d["tabs"][1]["nodes"][1]["col"] = 0   # collide with 18001 at (tab2,tier0,col0)
    with pytest.raises(ValueError, match="collision"):
        _lint(d)


def test_lint_rejects_noncontiguous_ranks():
    d = _yaml.safe_load(FIXTURE)
    d["tabs"][1]["nodes"][0]["grants"] = {1: [11069], 3: [12338]}
    with pytest.raises(ValueError, match="contiguous"):
        _lint(d)


SPELLMOD_FIXTURE = textwrap.dedent(r"""
era: 0
class: 8
eraName: Vanilla
className: MAGE
_ref:
  familyName: 3
  spells: { Frostbolt: { id: 116, flagsA: 32, flagsB: 0, flagsC: 0 } }
tabs:
  - key: 3
    name: Frost
    background: MageFrost
    nodes:
      - id: 18034
        slug: imp_frostbolt
        name: "Improved Frostbolt"
        tab: 3
        tier: 0
        col: 1
        icon: "iconF"
        maxRank: 5
        mechanic: spellmod
        affects: [Frostbolt]
        op: castTime
        kind: flat
        vals: [-100,-200,-300,-400,-500]
        tooltip: ["-0.1s","-0.2s","-0.3s","-0.4s","-0.5s"]
""")


def test_custom_passive_spell_dbc(tmp_path):
    y = tmp_path/"m.yaml"; y.write_text(SPELLMOD_FIXTURE)
    out = tmp_path/"custom.sql"
    r = _run(tmp_path, "--custom-sql", str(y), "-o", str(out))
    assert r.returncode == 0, r.stderr
    sql = out.read_text()
    assert "DELETE FROM `spell_dbc` WHERE `ID`=920272;" in sql   # 920000+(18034-18000)*8+0
    row = [l for l in sql.splitlines() if l.startswith("(920272,")][0]
    col = {name: v for name, v in zip(_COLS, _parse_values(row))}
    assert len(_parse_values(row)) == len(_COLS) == 234
    assert col["Effect_1"] == "6"
    assert col["EffectAura_1"] == "107"
    assert col["EffectMiscValue_1"] == "10"
    assert col["EffectBasePoints_1"] == "-101" and col["EffectDieSides_1"] == "1"
    assert col["SpellClassSet"] == "3"
    assert col["EffectSpellClassMaskA_1"] == "32"


# A spellmod node may additionally carry a `procPassive:` — the SAME granted passive gets an Effect_2
# aura-42 PROC_TRIGGER_SPELL plus a spell_proc row, so a spellmod node can hang a per-rank triggered
# aura on another unit without a second granted spell (Vanilla Improved Lay on Hands: cooldown mod +
# armor-buff-on-cast). Reuses the SPELLMOD_FIXTURE mage node with an added procPassive block.
SPELLMOD_PROC_FIXTURE = SPELLMOD_FIXTURE.replace(
    '        tooltip: ["-0.1s","-0.2s","-0.3s","-0.4s","-0.5s"]\n',
    '        tooltip: ["-0.1s","-0.2s","-0.3s","-0.4s","-0.5s"]\n'
    "        procPassive:\n"
    "          trigger: [800001, 800002, 800003, 800004, 800005]\n"
    "          flags: 65536\n"
    "          chance: 100\n")


def test_spellmod_node_with_proc_passive(tmp_path):
    y = tmp_path/"m.yaml"; y.write_text(SPELLMOD_PROC_FIXTURE)
    out = tmp_path/"custom.sql"
    r = _run(tmp_path, "--custom-sql", str(y), "-o", str(out))
    assert r.returncode == 0, r.stderr
    sql = out.read_text()
    pid = 920272  # 920000+(18034-18000)*8+0
    rows = [l for l in sql.splitlines() if l.startswith(f"({pid},")]
    assert len(rows) == 2, "expected a spell_dbc row AND a spell_proc row for the passive"
    col = {name: v for name, v in zip(_COLS, _parse_values(rows[0]))}
    # Effect_1 stays the cooldown spellmod; Effect_2 is the added proc-trigger.
    assert col["Effect_1"] == "6" and col["EffectAura_1"] == "107"
    assert col["Effect_2"] == "6" and col["EffectAura_2"] == "42"
    assert col["EffectTriggerSpell_2"] == "800001"      # rank 1 trigger
    assert "INSERT INTO `spell_proc`" in sql
    assert f"DELETE FROM `spell_proc` WHERE `SpellId`={pid};" in sql
    proc_cols = ["SpellId", "SchoolMask", "SpellFamilyName", "SpellFamilyMask0",
                 "SpellFamilyMask1", "SpellFamilyMask2", "ProcFlags", "SpellTypeMask",
                 "SpellPhaseMask", "HitMask", "AttributesMask", "DisableEffectsMask",
                 "ProcsPerMinute", "Chance", "Cooldown", "Charges"]
    proc = dict(zip(proc_cols, _parse_values(rows[1])))
    assert proc["ProcFlags"] == "65536" and proc["Chance"] == "100"
    # rank 3 picks its own per-rank trigger id
    row3 = [l for l in sql.splitlines() if l.startswith(f"({pid+2},")][0]
    col3 = {name: v for name, v in zip(_COLS, _parse_values(row3))}
    assert col3["EffectTriggerSpell_2"] == "800003"


def test_lint_rejects_proc_passive_bad_trigger_length():
    import tools.gen_era_talents as g
    d = _yaml.safe_load(SPELLMOD_PROC_FIXTURE.replace(
        "          trigger: [800001, 800002, 800003, 800004, 800005]\n",
        "          trigger: [800001, 800002]\n"))
    with pytest.raises(ValueError, match="procPassive.trigger length"):
        g.lint(d)


# affectsMask override: bind by an explicit (a,b,c) mask instead of the target's full identity, so a
# SHARED family bit (e.g. the paladin flagsC bit5 "aura" category on Ret/Devotion/Concentration Aura)
# does not over-bind onto the other spells that share it. The target here carries A=8 + a shared C=32;
# the override binds A=8 / C=0 so only the unique bit is emitted.
AFFECTSMASK_FIXTURE = textwrap.dedent(r"""
era: 0
class: 2
eraName: Vanilla
className: PALADIN
_ref:
  familyName: 10
  spells:
    Retribution Aura: { id: 7294, flagsA: 8, flagsB: 0, flagsC: 32 }
    Devotion Aura:    { id: 465,  flagsA: 64, flagsB: 0, flagsC: 32 }
tabs:
  - key: 3
    name: Retribution
    background: PaladinCombat
    nodes:
      - id: 18640
        slug: imp_ret_aura
        name: "Improved Retribution Aura"
        tab: 3
        tier: 3
        col: 2
        icon: "iconR"
        maxRank: 2
        mechanic: spellmod
        affects: [Retribution Aura]
        affectsMask: { a: 8, b: 0, c: 0 }
        op: effect1
        kind: pct
        vals: [25, 50]
        tooltip: ["25%", "50%"]
""")


def test_affects_mask_override_emits_only_specified_words(tmp_path):
    y = tmp_path/"p.yaml"; y.write_text(AFFECTSMASK_FIXTURE)
    out = tmp_path/"custom.sql"
    r = _run(tmp_path, "--custom-sql", str(y), "-o", str(out))
    assert r.returncode == 0, r.stderr
    sql = out.read_text()
    row = [l for l in sql.splitlines() if l.startswith("(925120,")][0]  # 920000+(18640-18000)*8+0
    col = {name: v for name, v in zip(_COLS, _parse_values(row))}
    # UNIQUE bit only — the shared C=32 "paladin aura" category bit is dropped, so Devotion/
    # Concentration Aura (which also carry C=32) are NOT matched by core SpellInfo::IsAffected.
    assert col["EffectSpellClassMaskA_1"] == "8"    # word A
    assert col["EffectSpellClassMaskA_2"] == "0"    # word B
    assert col["EffectSpellClassMaskA_3"] == "0"    # word C (NOT 32)


def test_affects_mask_default_uses_full_identity(tmp_path):
    """Without affectsMask, the emitted mask is the target's FULL identity (A=8 AND C=32) — the
    pre-fix behavior, kept for spells whose identity carries no shared bit."""
    d = _yaml.safe_load(AFFECTSMASK_FIXTURE)
    del d["tabs"][0]["nodes"][0]["affectsMask"]
    y = tmp_path/"p.yaml"; y.write_text(_yaml.safe_dump(d))
    out = tmp_path/"custom.sql"
    r = _run(tmp_path, "--custom-sql", str(y), "-o", str(out))
    assert r.returncode == 0, r.stderr
    row = [l for l in out.read_text().splitlines() if l.startswith("(925120,")][0]
    col = {name: v for name, v in zip(_COLS, _parse_values(row))}
    assert col["EffectSpellClassMaskA_1"] == "8"
    assert col["EffectSpellClassMaskA_3"] == "32"   # the shared category bit leaks in (over-binds)


def test_lint_rejects_all_zero_affects_mask():
    d = _yaml.safe_load(AFFECTSMASK_FIXTURE)
    d["tabs"][0]["nodes"][0]["affectsMask"] = {"a": 0, "b": 0, "c": 0}
    with pytest.raises(ValueError, match="affectsMask is all-zero"):
        _lint(d)


def test_lint_rejects_non_dict_affects_mask():
    d = _yaml.safe_load(AFFECTSMASK_FIXTURE)
    d["tabs"][0]["nodes"][0]["affectsMask"] = 8
    with pytest.raises(ValueError, match="affectsMask must be a dict"):
        _lint(d)


def test_sql_auto_grants_mechanic_node_to_custom_passive(tmp_path):
    """A mechanic node with no explicit `grants:` must still populate era_talent_rank, wired
    to the SAME deterministic custom_passive_id --custom-sql creates the spell_dbc row at —
    otherwise TryLearn's rankSpell lookup stays 0 and every rank is rejected as malformed."""
    y = tmp_path / "m.yaml"; y.write_text(SPELLMOD_FIXTURE)
    out = tmp_path / "out.sql"
    r = _run(tmp_path, "--sql", str(y), "-o", str(out))
    assert r.returncode == 0, r.stderr
    sql = out.read_text()
    _, _, rank_part = sql.partition("INSERT INTO `era_talent_rank`")
    # node 18034, maxRank 5 -> pids 920272..920276 (920000+(18034-18000)*8+(rank-1))
    for rank, pid in enumerate(range(920272, 920277), start=1):
        assert f"({18034},{rank},{pid})" in rank_part


def test_custom_passive_rank2(tmp_path):
    y = tmp_path/"m.yaml"; y.write_text(SPELLMOD_FIXTURE)
    out = tmp_path/"custom.sql"
    r = _run(tmp_path, "--custom-sql", str(y), "-o", str(out))
    assert r.returncode == 0, r.stderr
    sql = out.read_text()
    assert "DELETE FROM `spell_dbc` WHERE `ID`=920273;" in sql   # rank 2
    row = [l for l in sql.splitlines() if l.startswith("(920273,")][0]
    col = {name: v for name, v in zip(_COLS, _parse_values(row))}
    assert col["EffectBasePoints_1"] == "-201"


PROC_FIXTURE = textwrap.dedent(r"""
era: 0
class: 8
eraName: Vanilla
className: MAGE
tabs:
  - key: 1
    name: Arcane
    background: MageArcane
    nodes:
      - id: 18006
        slug: arcane_concentration
        name: "Arcane Concentration"
        tab: 1
        tier: 0
        col: 0
        icon: "iconC"
        maxRank: 5
        mechanic: proc
        proc: { flags: 65536, chance: [10, 20, 30, 40, 50], trigger: 12536 }
        tooltip: ["r1", "r2", "r3", "r4", "r5"]
""")

PROC_SCRIPTED_FIXTURE = textwrap.dedent(r"""
era: 0
class: 8
eraName: Vanilla
className: MAGE
tabs:
  - key: 2
    name: Fire
    background: MageFire
    nodes:
      - id: 18010
        slug: ignite
        name: "Ignite"
        tab: 2
        tier: 0
        col: 0
        icon: "iconD"
        maxRank: 5
        mechanic: proc-scripted
        proc: { flags: 262144, chance: 100, cooldown: 4000, hitMask: 2 }
        script: era_ignite
        vals: [8, 16, 24, 32, 40]
        tooltip: ["r1", "r2", "r3", "r4", "r5"]
""")


def test_custom_proc_passive_and_spell_proc(tmp_path):
    y = tmp_path / "m.yaml"; y.write_text(PROC_FIXTURE)
    out = tmp_path / "custom.sql"
    r = _run(tmp_path, "--custom-sql", str(y), "-o", str(out))
    assert r.returncode == 0, r.stderr
    sql = out.read_text()
    # rank-1 passive id = 920000 + (18006-18000)*8 + 0 = 920048
    assert "DELETE FROM `spell_dbc` WHERE `ID`=920048;" in sql
    matches = [l for l in sql.splitlines() if l.startswith("(920048,")]
    assert len(matches) == 2, "expected exactly a spell_dbc row and a spell_proc row"
    dbc_col = {name: v for name, v in zip(_COLS, _parse_values(matches[0]))}
    assert dbc_col["EffectAura_1"] == "42"           # SPELL_AURA_PROC_TRIGGER_SPELL
    assert dbc_col["EffectTriggerSpell_1"] == "12536"
    assert "INSERT INTO `spell_proc`" in sql
    # idempotency: spell_proc must be DELETE-before-INSERT, or a re-apply fails db-import
    assert "DELETE FROM `spell_proc` WHERE `SpellId`=920048;" in sql
    proc_vals = _parse_values(matches[1])
    proc_cols = ["SpellId", "SchoolMask", "SpellFamilyName", "SpellFamilyMask0",
                 "SpellFamilyMask1", "SpellFamilyMask2", "ProcFlags", "SpellTypeMask",
                 "SpellPhaseMask", "HitMask", "AttributesMask", "DisableEffectsMask",
                 "ProcsPerMinute", "Chance", "Cooldown", "Charges"]
    proc = dict(zip(proc_cols, proc_vals))
    assert proc["ProcFlags"] == "65536"
    assert proc["Chance"] == "10"                     # rank 1 -> chance[0]

    # rank 3 -> per-rank chance
    pid3 = 920048 + 2
    row3 = [l for l in sql.splitlines() if l.startswith(f"({pid3},")][1]
    proc3 = dict(zip(proc_cols, _parse_values(row3)))
    assert proc3["Chance"] == "30"
    assert "INSERT INTO `spell_script_names`" not in sql   # plain proc, not scripted


def test_custom_proc_scripted_emits_script_names(tmp_path):
    y = tmp_path / "m.yaml"; y.write_text(PROC_SCRIPTED_FIXTURE)
    out = tmp_path / "custom.sql"
    r = _run(tmp_path, "--custom-sql", str(y), "-o", str(out))
    assert r.returncode == 0, r.stderr
    sql = out.read_text()
    assert "INSERT INTO `spell_script_names`" in sql
    for rank in range(1, 6):
        pid = 920000 + (18010 - 18000) * 8 + (rank - 1)
        assert f"({pid}, 'era_ignite');" in sql
        assert f"DELETE FROM `spell_script_names` WHERE `spell_id`={pid};" in sql
    # the scripted passive must be a DUMMY effect carrying the rank's pct (script reads GetAmount),
    # NOT a PROC_TRIGGER_SPELL — the script casts the real trigger itself.
    pid1 = 920000 + (18010 - 18000) * 8
    row = [l for l in sql.splitlines() if l.startswith(f"({pid1},")][0]
    col = {name: v for name, v in zip(_COLS, _parse_values(row))}
    assert col["EffectAura_1"] == "4"                 # SPELL_AURA_DUMMY
    assert col["EffectBasePoints_1"] == "7"           # 8 - 1 (rank 1 pct)
    assert col["EffectDieSides_1"] == "1"
    assert col["EffectTriggerSpell_1"] == "0"         # no auto-trigger


SCRIPTED_FIXTURE = textwrap.dedent(r"""
era: 0
class: 9
eraName: Vanilla
className: WARLOCK
_ref: { familyName: 5, spells: {} }
tabs:
  - key: 2
    name: Demonology
    background: WarlockDemonology
    nodes:
      - id: 18222
        slug: improved_voidwalker
        name: "Improved Voidwalker"
        tab: 2
        tier: 1
        col: 1
        icon: "iconIVW"
        maxRank: 3
        mechanic: scripted
        marker: 6
        vals: [10, 20, 30]
        tooltip: ["10%","20%","30%"]
""")


def test_scripted_mechanic_emits_marker_passive_no_proc(tmp_path):
    import tools.gen_era_talents as g
    y = tmp_path/"w.yaml"; y.write_text(SCRIPTED_FIXTURE)
    out = tmp_path/"custom.sql"; sqlout = tmp_path/"data.sql"
    r = _run(tmp_path, "--custom-sql", str(y), "-o", str(out))
    assert r.returncode == 0, r.stderr
    sql = out.read_text()
    p1 = 920000 + (18222-18000)*8 + 0                 # 921776
    row = [l for l in sql.splitlines() if l.startswith(f"({p1},")][0]
    col = {name:v for name,v in zip(_COLS, _parse_values(row))}
    assert col["EffectAura_1"] == "4"                  # SPELL_AURA_DUMMY
    assert col["EffectMiscValue_1"] == "6"             # the marker
    assert col["EffectBasePoints_1"] == "9"            # eff-1 (proc-scripted base-point convention)
    assert col["EquippedItemClass"] == "-1"            # never the consumable-class proc-killer
    assert "INSERT INTO `spell_proc`" not in sql, "scripted mechanic must NOT emit spell_proc"
    # no top-level scriptBindings in the fixture -> a scripted node emits NEITHER extra row
    # (behavior binds separately via scriptBindings, not off the marker passive).
    assert "INSERT INTO `spell_script_names`" not in sql, \
        "scripted mechanic must NOT emit spell_script_names"
    d = _run(tmp_path, "--sql", str(y), "-o", str(sqlout)); assert d.returncode == 0, d.stderr
    dsql = sqlout.read_text()
    for rank in range(1, 4):
        assert f"({18222},{rank},{920000+(18222-18000)*8+(rank-1)})" in dsql.replace(" ", "")


@pytest.mark.parametrize("bad", [5, 256])
def test_lint_rejects_scripted_marker_out_of_range(bad):
    d = _yaml.safe_load(SCRIPTED_FIXTURE)
    d["tabs"][0]["nodes"][0]["marker"] = bad
    with pytest.raises(ValueError, match="marker must be an int"):
        _lint(d)


def test_lint_rejects_scripted_marker_bool():
    # a YAML `true`/`false` is an int subclass in Python — it must NOT satisfy the 6..255 check.
    d = _yaml.safe_load(SCRIPTED_FIXTURE)
    d["tabs"][0]["nodes"][0]["marker"] = True
    with pytest.raises(ValueError, match="marker must be an int"):
        _lint(d)


def test_lint_rejects_scripted_missing_vals():
    d = _yaml.safe_load(SCRIPTED_FIXTURE)
    del d["tabs"][0]["nodes"][0]["vals"]
    with pytest.raises(ValueError, match="missing required key|vals"):
        _lint(d)


def test_lint_rejects_scripted_wrong_length_vals():
    d = _yaml.safe_load(SCRIPTED_FIXTURE)
    d["tabs"][0]["nodes"][0]["vals"] = [10, 20]     # maxRank is 3
    with pytest.raises(ValueError, match="vals length must equal maxRank"):
        _lint(d)


def test_lint_rejects_scripted_duplicate_marker():
    """Two scripted nodes in one dataset sharing a marker emit indistinguishable DUMMY carriers."""
    d = _yaml.safe_load(SCRIPTED_FIXTURE)
    n0 = d["tabs"][0]["nodes"][0]
    dup = dict(n0)
    dup.update(id=18223, slug="dup", name="Dup", tier=1, col=2)   # same marker (6), distinct cell
    d["tabs"][0]["nodes"].append(dup)
    with pytest.raises(ValueError, match="marker 6 collides"):
        _lint(d)


def test_lint_rejects_proc_scripted_missing_vals():
    d = _yaml.safe_load(PROC_SCRIPTED_FIXTURE)
    del d["tabs"][0]["nodes"][0]["vals"]
    with pytest.raises(ValueError, match="missing required key|vals"):
        _lint(d)


def test_lint_rejects_data_proc_missing_trigger():
    d = _yaml.safe_load(PROC_FIXTURE)
    del d["tabs"][0]["nodes"][0]["proc"]["trigger"]
    with pytest.raises(ValueError, match="trigger"):
        _lint(d)


def test_lua_spellmods_map(tmp_path):
    y = tmp_path / "m.yaml"; y.write_text(SPELLMOD_FIXTURE)
    out = tmp_path / "m.lua"
    r = _run(tmp_path, "--lua", str(y), "-o", str(out))
    assert r.returncode == 0, r.stderr
    lua = out.read_text()
    assert "spellMods = {" in lua
    assert '["Frostbolt"]' in lua
    assert 'attr="castTime"' in lua
    assert 'kind="flat"' in lua
    assert "vals={ -100, -200, -300, -400, -500 }" in lua
    assert "talent=18034" in lua
    import shutil
    if shutil.which("luac5.1"):
        assert subprocess.run(["luac5.1", "-p", str(out)]).returncode == 0


def test_lua_wired_true_for_mechanic_node_without_explicit_grants(tmp_path):
    """wired must reflect actual server binding: a mechanic node is auto-bound by emit_sql even
    with no explicit `grants:`, so the addon's wired flag must say true, not false."""
    y = tmp_path / "m.yaml"; y.write_text(SPELLMOD_FIXTURE)
    out = tmp_path / "m.lua"
    r = _run(tmp_path, "--lua", str(y), "-o", str(out))
    assert r.returncode == 0, r.stderr
    lua = out.read_text()
    assert "id=18034" in lua and "wired=true" in lua
    assert "wired=false" not in lua


def test_lua_spellmods_empty_for_grants_only(tmp_path):
    """The plain FIXTURE has no mechanic:spellmod nodes -> spellMods still emits, empty."""
    y = tmp_path / "mage.yaml"; y.write_text(FIXTURE)
    out = tmp_path / "MageVanilla.lua"
    r = _run(tmp_path, "--lua", str(y), "-o", str(out))
    assert r.returncode == 0, r.stderr
    lua = out.read_text()
    assert "spellMods = {" in lua


def test_lint_rejects_spellmod_missing_op():
    d = _yaml.safe_load(SPELLMOD_FIXTURE)
    del d["tabs"][0]["nodes"][0]["op"]
    with pytest.raises(ValueError, match="missing required key"):
        _lint(d)


def test_lint_rejects_vals_length_mismatch():
    d = _yaml.safe_load(SPELLMOD_FIXTURE)
    d["tabs"][0]["nodes"][0]["vals"] = [-100, -200]  # maxRank is 5
    with pytest.raises(ValueError, match="vals length"):
        _lint(d)


def test_lint_rejects_unknown_affects_spell():
    d = _yaml.safe_load(SPELLMOD_FIXTURE)
    d["tabs"][0]["nodes"][0]["affects"] = ["NotARealSpell"]
    with pytest.raises(ValueError, match="not in _ref.spells"):
        _lint(d)


def test_lint_rejects_proc_missing_flags():
    d = _yaml.safe_load(PROC_FIXTURE)
    del d["tabs"][0]["nodes"][0]["proc"]["flags"]
    with pytest.raises(ValueError, match="proc"):
        _lint(d)


def test_lint_rejects_proc_scripted_missing_script():
    d = _yaml.safe_load(PROC_SCRIPTED_FIXTURE)
    del d["tabs"][0]["nodes"][0]["script"]
    with pytest.raises(ValueError, match="missing required key"):
        _lint(d)


def test_lint_rejects_custom_passive_id_collision():
    d = _yaml.safe_load(SPELLMOD_FIXTURE)
    # A second node whose (id, rank) formula lands on the same custom-passive id band slot
    # as node 18034's ranks (maxRank 5 uses slots 0..4 of its 8-wide block) — force a
    # collision by cloning the node under a different id that maps into the same slot.
    clone = dict(d["tabs"][0]["nodes"][0])
    clone["id"] = 18034  # identical id -> guaranteed identical pids across all ranks
    clone["slug"] = "imp_frostbolt_dup"
    clone["col"] = 2
    d["tabs"][0]["nodes"].append(clone)
    with pytest.raises(ValueError, match="collision"):
        _lint(d)


# ---------------------------------------------------------------------------
# B1: new SpellModOps + mechanic:multi (heterogeneous multi-effect passives)
# ---------------------------------------------------------------------------

NEWOP_FIXTURE = textwrap.dedent(r"""
era: 0
class: 8
eraName: Vanilla
className: MAGE
_ref:
  familyName: 3
  spells: { Frostbolt: { id: 116, flagsA: 32, flagsB: 0, flagsC: 0 } }
tabs:
  - key: 3
    name: Frost
    background: MageFrost
    nodes:
      - id: 18036
        slug: ice_shards
        name: "Ice Shards"
        tab: 3
        tier: 1
        col: 0
        icon: "iconIS"
        maxRank: 5
        mechanic: spellmod
        affects: [Frostbolt]
        op: critDamage
        kind: flat
        vals: [20, 40, 60, 80, 100]
        tooltip: ["r1","r2","r3","r4","r5"]
""")

MULTI_FIXTURE = textwrap.dedent(r"""
era: 0
class: 8
eraName: Vanilla
className: MAGE
_ref:
  familyName: 3
  spells:
    Frostbolt:      { id: 116, flagsA: 32,     flagsB: 0, flagsC: 0 }
    Blizzard:       { id: 10,  flagsA: 524416, flagsB: 0, flagsC: 0 }
    "Cone of Cold": { id: 120, flagsA: 512,    flagsB: 0, flagsC: 0 }
tabs:
  - key: 3
    name: Frost
    background: MageFrost
    nodes:
      - id: 18044
        slug: frost_channeling
        name: "Frost Channeling"
        tab: 3
        tier: 3
        col: 1
        icon: "iconFC"
        maxRank: 3
        mechanic: multi
        effects:
          - type: spellmod
            affects: [Frostbolt, Blizzard, "Cone of Cold"]
            op: cost
            kind: pct
            vals: [-5, -10, -15]
          - type: spellmod
            affects: [Frostbolt, Blizzard, "Cone of Cold"]
            op: threat
            kind: pct
            vals: [-10, -20, -30]
        tooltip: ["r1","r2","r3"]
""")

MULTI_HETERO_FIXTURE = textwrap.dedent(r"""
era: 0
class: 8
eraName: Vanilla
className: MAGE
_ref:
  familyName: 3
  spells: { "Arcane Missiles": { id: 5143, flagsA: 2048, flagsB: 0, flagsC: 0 } }
tabs:
  - key: 1
    name: Arcane
    background: MageArcane
    nodes:
      - id: 18001
        slug: arcane_subtlety
        name: "Arcane Subtlety"
        tab: 1
        tier: 0
        col: 0
        icon: "iconAS"
        maxRank: 2
        mechanic: multi
        effects:
          - type: stat
            aura: 123
            misc: 127
            vals: [-5, -10]
          - type: spellmod
            affects: ["Arcane Missiles"]
            op: threat
            kind: pct
            vals: [-20, -40]
        tooltip: ["r1","r2"]
""")


def test_new_op_crit_damage(tmp_path):
    y = tmp_path / "m.yaml"; y.write_text(NEWOP_FIXTURE)
    out = tmp_path / "custom.sql"
    r = _run(tmp_path, "--custom-sql", str(y), "-o", str(out))
    assert r.returncode == 0, r.stderr
    sql = out.read_text()
    pid = 920000 + (18036 - 18000) * 8  # 920288
    row = [l for l in sql.splitlines() if l.startswith(f"({pid},")][0]
    col = {name: v for name, v in zip(_COLS, _parse_values(row))}
    assert col["EffectMiscValue_1"] == "15"       # SPELLMOD_CRIT_DAMAGE_BONUS
    assert col["EffectAura_1"] == "107"            # flat
    assert col["EffectBasePoints_1"] == "19"       # 20 - 1


CHARGES_FIXTURE = textwrap.dedent(r"""
era: 0
class: 8
eraName: Vanilla
className: MAGE
_ref:
  familyName: 3
  spells: { Frostbolt: { id: 116, flagsA: 32, flagsB: 0, flagsC: 0 } }
tabs:
  - key: 3
    name: Frost
    background: MageFrost
    nodes:
      - id: 18037
        slug: extra_charges
        name: "Extra Charges"
        tab: 3
        tier: 1
        col: 1
        icon: "iconEC"
        maxRank: 2
        mechanic: spellmod
        affects: [Frostbolt]
        op: charges
        kind: flat
        vals: [2, 4]
        tooltip: ["r1","r2"]
""")


def test_op_charges(tmp_path):
    # SPELLMOD_CHARGES = 4 (added for TBC paladin Improved Holy Shield's +2/+4 charges spellmod).
    y = tmp_path / "m.yaml"; y.write_text(CHARGES_FIXTURE)
    out = tmp_path / "custom.sql"
    r = _run(tmp_path, "--custom-sql", str(y), "-o", str(out))
    assert r.returncode == 0, r.stderr
    sql = out.read_text()
    pid = 920000 + (18037 - 18000) * 8  # 920296
    row = [l for l in sql.splitlines() if l.startswith(f"({pid},")][0]
    col = {name: v for name, v in zip(_COLS, _parse_values(row))}
    assert col["EffectMiscValue_1"] == "4"        # SPELLMOD_CHARGES
    assert col["EffectAura_1"] == "107"           # flat
    assert col["EffectBasePoints_1"] == "1"       # 2 - 1


def test_multi_two_spellmod_effects(tmp_path):
    y = tmp_path / "m.yaml"; y.write_text(MULTI_FIXTURE)
    out = tmp_path / "custom.sql"
    r = _run(tmp_path, "--custom-sql", str(y), "-o", str(out))
    assert r.returncode == 0, r.stderr
    sql = out.read_text()
    pid = 920000 + (18044 - 18000) * 8  # 920352
    row = [l for l in sql.splitlines() if l.startswith(f"({pid},")][0]
    col = {name: v for name, v in zip(_COLS, _parse_values(row))}
    mask = str(32 | 524416 | 512)  # 524960
    # effect 1: cost pct
    assert col["Effect_1"] == "6" and col["EffectAura_1"] == "108"
    assert col["EffectMiscValue_1"] == "14"                 # SPELLMOD_COST
    assert col["EffectBasePoints_1"] == "-6"                # -5 - 1
    assert col["EffectSpellClassMaskA_1"] == mask
    # effect 2: threat pct -> mask goes in the B columns
    assert col["Effect_2"] == "6" and col["EffectAura_2"] == "108"
    assert col["EffectMiscValue_2"] == "2"                  # SPELLMOD_THREAT
    assert col["EffectBasePoints_2"] == "-11"               # -10 - 1
    assert col["EffectSpellClassMaskB_1"] == mask
    assert col["SpellClassSet"] == "3"


def test_multi_heterogeneous_stat_plus_spellmod(tmp_path):
    y = tmp_path / "m.yaml"; y.write_text(MULTI_HETERO_FIXTURE)
    out = tmp_path / "custom.sql"
    r = _run(tmp_path, "--custom-sql", str(y), "-o", str(out))
    assert r.returncode == 0, r.stderr
    sql = out.read_text()
    pid = 920000 + (18001 - 18000) * 8  # 920008
    row = [l for l in sql.splitlines() if l.startswith(f"({pid},")][0]
    col = {name: v for name, v in zip(_COLS, _parse_values(row))}
    # effect 1: plain stat aura (MOD_TARGET_RESISTANCE), misc = school mask, NO class mask
    assert col["Effect_1"] == "6" and col["EffectAura_1"] == "123"
    assert col["EffectMiscValue_1"] == "127"
    assert col["EffectBasePoints_1"] == "-6"                # -5 - 1
    assert col["EffectSpellClassMaskA_1"] == "0"
    # effect 2: spellmod threat on Arcane Missiles
    assert col["EffectAura_2"] == "108" and col["EffectMiscValue_2"] == "2"
    assert col["EffectSpellClassMaskB_1"] == "2048"
    # a spellmod effect is present -> family tag required
    assert col["SpellClassSet"] == "3"


def test_multi_auto_grants_and_wired(tmp_path):
    y = tmp_path / "m.yaml"; y.write_text(MULTI_FIXTURE)
    sql_out = tmp_path / "out.sql"; lua_out = tmp_path / "m.lua"
    assert _run(tmp_path, "--sql", str(y), "-o", str(sql_out)).returncode == 0
    assert _run(tmp_path, "--lua", str(y), "-o", str(lua_out)).returncode == 0
    _, _, rank_part = sql_out.read_text().partition("INSERT INTO `era_talent_rank`")
    for rank, pid in enumerate(range(920352, 920355), start=1):
        assert f"(18044,{rank},{pid})" in rank_part
    assert "id=18044" in lua_out.read_text() and "wired=true" in lua_out.read_text()


def test_multi_spellmods_map_includes_discrete_effect(tmp_path):
    """A mechanic:multi node with a discrete-op spellmod effect (cost) must still feed the
    addon spellMods tooltip map; the non-discrete threat effect must not."""
    y = tmp_path / "m.yaml"; y.write_text(MULTI_FIXTURE)
    out = tmp_path / "m.lua"
    assert _run(tmp_path, "--lua", str(y), "-o", str(out)).returncode == 0
    lua = out.read_text()
    assert 'attr="cost"' in lua
    assert 'attr="threat"' not in lua
    assert "talent=18044" in lua


# ---------------------------------------------------------------------------
# ADD_TARGET_TRIGGER (aura 109) stat effects — "when you land spell X, also apply Y to the
# target". Motivating case: TBC druid Improved Faerie Fire (node 20118), where the TBC talent's
# effect2/effect3 SPELLMODs have no substrate on 3.3.5a (stock Faerie Fire has no melee/ranged
# hit effects to modify), so the melee+ranged hit the tooltip promises is delivered by attaching
# a companion debuff instead. Needs a `trigger:` and a class mask on a `type: stat` effect.
# ---------------------------------------------------------------------------

TARGET_TRIGGER_FIXTURE = textwrap.dedent(r"""
era: 1
class: 11
eraName: TBC
className: DRUID
_ref:
  familyName: 7
  spells:
    "Faerie Fire":          { id: 770,   flagsA: 1024, flagsB: 0, flagsC: 0 }
    "Faerie Fire (Feral)":  { id: 16857, flagsA: 1024, flagsB: 0, flagsC: 0 }
tabs:
  - key: 1
    name: Balance
    background: DruidBalance
    nodes:
      - id: 20118
        slug: improved_faerie_fire
        name: "Improved Faerie Fire"
        tab: 1
        tier: 6
        col: 2
        icon: "iconIFF"
        maxRank: 3
        mechanic: multi
        effects:
          - type: stat
            aura: 109
            affects: ["Faerie Fire", "Faerie Fire (Feral)"]
            trigger: [946277, 946278, 946279]
            vals: [100, 100, 100]
        tooltip: ["r1","r2","r3"]
""")


def test_stat_effect_emits_target_trigger_and_mask(tmp_path):
    """A `type: stat` effect may carry `trigger:` (per-rank) + `affects:`; the emitted row must
    carry EffectTriggerSpell_i, the class mask, ImplicitTargetA_i=1 (the aura sits on the caster),
    and — critically — SpellClassSet, without which SpellInfo::IsAffected short-circuits
    `if (!familyName) return true;` and the trigger fires on EVERY spell the player lands."""
    y = tmp_path / "m.yaml"; y.write_text(TARGET_TRIGGER_FIXTURE)
    out = tmp_path / "custom.sql"
    r = _run(tmp_path, "--custom-sql", str(y), "-o", str(out))
    assert r.returncode == 0, r.stderr
    sql = out.read_text()
    for rank, trig in enumerate((946277, 946278, 946279), start=1):
        pid = 936000 + (20118 - 20000) * 8 + (rank - 1)
        row = [l for l in sql.splitlines() if l.startswith(f"({pid},")][0]
        col = {name: v for name, v in zip(_COLS, _parse_values(row))}
        assert col["Effect_1"] == "6" and col["EffectAura_1"] == "109"
        assert col["EffectTriggerSpell_1"] == str(trig)
        assert col["EffectBasePoints_1"] == "99"        # 100 - 1, dieSides 1 -> 100% chance
        assert col["EffectDieSides_1"] == "1"
        assert col["ImplicitTargetA_1"] == "1"
        assert col["EffectSpellClassMaskA_1"] == "1024"
        assert col["SpellClassSet"] == "7"


def test_stat_effect_trigger_accepts_scalar(tmp_path):
    """`trigger:` may be a single id shared by every rank, matching the `procPassive:` idiom."""
    d = _yaml.safe_load(TARGET_TRIGGER_FIXTURE)
    d["tabs"][0]["nodes"][0]["effects"][0]["trigger"] = 946277
    y = tmp_path / "m.yaml"; y.write_text(_yaml.safe_dump(d))
    out = tmp_path / "custom.sql"
    assert _run(tmp_path, "--custom-sql", str(y), "-o", str(out)).returncode == 0
    sql = out.read_text()
    for rank in (1, 2, 3):
        pid = 936000 + (20118 - 20000) * 8 + (rank - 1)
        row = [l for l in sql.splitlines() if l.startswith(f"({pid},")][0]
        col = {name: v for name, v in zip(_COLS, _parse_values(row))}
        assert col["EffectTriggerSpell_1"] == "946277"


def test_helper_attributes_ex_scalar(tmp_path):
    """A templated helper can author `attributesEx:` to override — not inherit — the template's
    AttributesEx. Needed by the Improved Faerie Fire debuffs: stock Faerie Fire 770 carries
    AttributesEx 98304, and SpellInfoCorrections.cpp clears
    SPELL_ATTR1_IMMUNITY_TO_HOSTILE_AND_FRIENDLY_EFFECTS from 770/16857 BY ID — a correction a
    clone does not inherit, so an inheriting clone would bypass school immunity that blocks the
    Faerie Fire it accompanies."""
    d = _yaml.safe_load(TARGET_TRIGGER_FIXTURE)
    d["helpers"] = [{
        "id": 946277, "name": "Improved Faerie Fire", "template": 770,
        "attributes": 67108864, "attributesEx": 0, "durationIndex": 5,
        "effects": [{"effect": 6, "aura": 184, "basePoints": 1, "dieSides": 0, "targetA": 6}],
    }]
    y = tmp_path / "m.yaml"; y.write_text(_yaml.safe_dump(d))
    out = tmp_path / "custom.sql"
    r = _run(tmp_path, "--custom-sql", str(y), "-o", str(out))
    assert r.returncode == 0, r.stderr
    row = [l for l in out.read_text().splitlines() if l.startswith("(946277,")][0]
    col = {name: v for name, v in zip(_COLS, _parse_values(row))}
    assert col["AttributesEx"] == "0", "authored attributesEx must override the template's 98304"
    assert col["Attributes"] == "67108864"


def test_lint_rejects_maskless_target_trigger():
    """A maskless aura 109 is the R1 footgun: family 0 makes IsAffected return true for every
    spell, so the companion debuff would land on every cast. Must be a hard error, not a warning."""
    d = _yaml.safe_load(TARGET_TRIGGER_FIXTURE)
    del d["tabs"][0]["nodes"][0]["effects"][0]["affects"]
    with pytest.raises(ValueError, match="109"):
        _lint(d)


def test_lint_rejects_target_trigger_without_trigger_id():
    """aura 109 with no trigger spell is a silent no-op."""
    d = _yaml.safe_load(TARGET_TRIGGER_FIXTURE)
    del d["tabs"][0]["nodes"][0]["effects"][0]["trigger"]
    with pytest.raises(ValueError, match="trigger"):
        _lint(d)


def test_lint_rejects_trigger_list_wrong_length():
    d = _yaml.safe_load(TARGET_TRIGGER_FIXTURE)
    d["tabs"][0]["nodes"][0]["effects"][0]["trigger"] = [946277, 946278]
    with pytest.raises(ValueError, match="trigger"):
        _lint(d)


def test_pure_stat_effect_stays_family_less(tmp_path):
    """Guard the existing contract: a stat effect with NO mask must keep SpellClassSet 0 so it
    cannot accidentally match a spell (the hetero fixture's aura 123 has no affects)."""
    y = tmp_path / "m.yaml"; y.write_text(MULTI_HETERO_FIXTURE)
    out = tmp_path / "custom.sql"
    assert _run(tmp_path, "--custom-sql", str(y), "-o", str(out)).returncode == 0
    sql = out.read_text()
    pid = 920000 + (18001 - 18000) * 8
    row = [l for l in sql.splitlines() if l.startswith(f"({pid},")][0]
    col = {name: v for name, v in zip(_COLS, _parse_values(row))}
    assert col["EffectSpellClassMaskA_1"] == "0"


def test_lint_rejects_multi_missing_effects():
    d = _yaml.safe_load(MULTI_FIXTURE)
    del d["tabs"][0]["nodes"][0]["effects"]
    with pytest.raises(ValueError, match="missing required key"):
        _lint(d)


def test_lint_rejects_multi_effect_missing_type():
    d = _yaml.safe_load(MULTI_FIXTURE)
    del d["tabs"][0]["nodes"][0]["effects"][0]["type"]
    with pytest.raises(ValueError, match="type"):
        _lint(d)


def test_lint_rejects_multi_effect_bad_op():
    d = _yaml.safe_load(MULTI_FIXTURE)
    d["tabs"][0]["nodes"][0]["effects"][0]["op"] = "notAnOp"
    with pytest.raises(ValueError, match="SpellModOp"):
        _lint(d)


def test_lint_rejects_multi_effect_vals_mismatch():
    d = _yaml.safe_load(MULTI_FIXTURE)
    d["tabs"][0]["nodes"][0]["effects"][0]["vals"] = [-5]   # maxRank 3
    with pytest.raises(ValueError, match="vals length"):
        _lint(d)


# ---------------------------------------------------------------------------
# B4: family-restricted data procs (proc.affects -> spell_proc family + mask)
# ---------------------------------------------------------------------------

PROC_FAMILY_FIXTURE = textwrap.dedent(r"""
era: 0
class: 8
eraName: Vanilla
className: MAGE
_ref:
  familyName: 3
  spells:
    Fireball:     { id: 133,   flagsA: 1, flagsB: 0, flagsC: 0 }
    "Fire Blast": { id: 2136,  flagsA: 2, flagsB: 0, flagsC: 0 }
    "Blast Wave": { id: 11113, flagsA: 0, flagsB: 64, flagsC: 0 }
tabs:
  - key: 2
    name: Fire
    background: MageFire
    nodes:
      - id: 18018
        slug: impact
        name: "Impact"
        tab: 2
        tier: 0
        col: 2
        icon: "iconImp"
        maxRank: 5
        mechanic: proc
        proc:
          flags: 65536
          chance: [2, 4, 6, 8, 10]
          trigger: 12355
          hitMask: 0
          affects: [Fireball, "Fire Blast", "Blast Wave"]
        tooltip: ["r1", "r2", "r3", "r4", "r5"]
""")


def test_proc_family_restricted_mask(tmp_path):
    y = tmp_path / "m.yaml"; y.write_text(PROC_FAMILY_FIXTURE)
    out = tmp_path / "custom.sql"
    r = _run(tmp_path, "--custom-sql", str(y), "-o", str(out))
    assert r.returncode == 0, r.stderr
    sql = out.read_text()
    pid = 920000 + (18018 - 18000) * 8  # 920144
    proc_cols = ["SpellId", "SchoolMask", "SpellFamilyName", "SpellFamilyMask0",
                 "SpellFamilyMask1", "SpellFamilyMask2", "ProcFlags", "SpellTypeMask",
                 "SpellPhaseMask", "HitMask", "AttributesMask", "DisableEffectsMask",
                 "ProcsPerMinute", "Chance", "Cooldown", "Charges"]
    # the proc row is the 2nd row that starts with (pid,
    rows = [l for l in sql.splitlines() if l.startswith(f"({pid},")]
    proc = dict(zip(proc_cols, _parse_values(rows[1])))
    assert proc["SpellFamilyName"] == "3"                 # mage
    assert proc["SpellFamilyMask0"] == str(1 | 2)         # Fireball | Fire Blast
    assert proc["SpellFamilyMask1"] == "64"               # Blast Wave (word 2)
    assert proc["HitMask"] == "0"
    assert proc["Chance"] == "2"


def test_lint_rejects_proc_affects_unknown_spell():
    d = _yaml.safe_load(PROC_FAMILY_FIXTURE)
    d["tabs"][0]["nodes"][0]["proc"]["affects"] = ["NotARealSpell"]
    with pytest.raises(ValueError, match="not in _ref.spells"):
        _lint(d)


def test_proc_per_rank_trigger_list(tmp_path):
    """proc.trigger as a per-rank list (Improved Blizzard's Chilled 12484/12485/12486)."""
    d = _yaml.safe_load(PROC_FAMILY_FIXTURE)
    n = d["tabs"][0]["nodes"][0]
    n["maxRank"] = 3
    n["proc"]["chance"] = [100, 100, 100]
    n["proc"]["trigger"] = [12484, 12485, 12486]
    n["tooltip"] = ["r1", "r2", "r3"]
    y = tmp_path / "m.yaml"; y.write_text(_yaml.safe_dump(d))
    out = tmp_path / "custom.sql"
    assert _run(tmp_path, "--custom-sql", str(y), "-o", str(out)).returncode == 0
    sql = out.read_text()
    for rank, trig in enumerate((12484, 12485, 12486), start=1):
        pid = 920000 + (18018 - 18000) * 8 + (rank - 1)
        row = [l for l in sql.splitlines() if l.startswith(f"({pid},") and len(_parse_values(l)) == len(_COLS)][0]
        col = {name: v for name, v in zip(_COLS, _parse_values(row))}
        assert col["EffectTriggerSpell_1"] == str(trig)


def test_spellmod_value_multiplier_op(tmp_path):
    d = _yaml.safe_load(SPELLMOD_FIXTURE)
    n = d["tabs"][0]["nodes"][0]
    n["op"] = "valueMultiplier"; n["kind"] = "pct"
    y = tmp_path / "m.yaml"; y.write_text(_yaml.safe_dump(d))
    out = tmp_path / "custom.sql"
    assert _run(tmp_path, "--custom-sql", str(y), "-o", str(out)).returncode == 0
    row = [l for l in out.read_text().splitlines() if l.startswith("(920272,")][0]
    col = {name: v for name, v in zip(_COLS, _parse_values(row))}
    assert col["EffectMiscValue_1"] == "27" and col["EffectAura_1"] == "108"


def test_helper_spells_emitted(tmp_path):
    d = _yaml.safe_load(PROC_FIXTURE)
    d["helpers"] = [{"id": 932990, "name": "Fire Ward Reflect", "aura": 75, "misc": 4,
                     "durationIndex": 9}]
    y = tmp_path / "m.yaml"; y.write_text(_yaml.safe_dump(d))
    out = tmp_path / "custom.sql"
    assert _run(tmp_path, "--custom-sql", str(y), "-o", str(out)).returncode == 0
    sql = out.read_text()
    assert "DELETE FROM `spell_dbc` WHERE `ID`=932990;" in sql
    row = [l for l in sql.splitlines() if l.startswith("(932990,")][0]
    col = {name: v for name, v in zip(_COLS, _parse_values(row))}
    assert col["EffectAura_1"] == "75" and col["EffectMiscValue_1"] == "4"
    assert col["DurationIndex"] == "9" and col["ImplicitTargetA_1"] == "1"


def test_helper_effect_per_level_scaling(tmp_path):
    """A helper effect's `perLevel` lands in EffectRealPointsPerLevel_i (float column) and the
    helper-level spellLevel/baseLevel/maxLevel scalars land in their DBC columns — the growth
    window core CalcValue uses (RPPL * (min(casterLevel, MaxLevel) - SpellLevel)). First user:
    the TBC Judgement of Command chain 946103-108 (halved wago-2.5.4 per-level values)."""
    d = _yaml.safe_load(PROC_FIXTURE)
    d["helpers"] = [{"id": 932991, "name": "JoC Scaling Probe",
                     "spellLevel": 20, "baseLevel": 20, "maxLevel": 28,
                     "effects": [{"effect": 2, "basePoints": 46, "dieSides": 5,
                                  "perLevel": 2.8, "targetA": 6}]}]
    y = tmp_path / "m.yaml"; y.write_text(_yaml.safe_dump(d))
    out = tmp_path / "custom.sql"
    assert _run(tmp_path, "--custom-sql", str(y), "-o", str(out)).returncode == 0
    row = [l for l in out.read_text().splitlines() if l.startswith("(932991,")][0]
    col = {name: v for name, v in zip(_COLS, _parse_values(row))}
    assert col["SpellLevel"] == "20" and col["BaseLevel"] == "20" and col["MaxLevel"] == "28"
    assert float(col["EffectRealPointsPerLevel_1"]) == 2.8
    assert col["EffectBasePoints_1"] == "46" and col["EffectDieSides_1"] == "5"


def test_custom_passives_have_no_equip_requirement(tmp_path):
    """Every custom passive must set EquippedItemClass=-1 (no requirement). Left at the zero-fill
    default (0=CONSUMABLE), the proc equip gate in Aura::GetProcEffectMask silently kills every
    proc on a player. Spellmod (920272), stat (from PROC-less), proc (920048), dummy passives."""
    y = tmp_path / "m.yaml"; y.write_text(SPELLMOD_FIXTURE)
    out = tmp_path / "custom.sql"
    assert _run(tmp_path, "--custom-sql", str(y), "-o", str(out)).returncode == 0
    row = [l for l in out.read_text().splitlines() if l.startswith("(920272,")][0]
    col = {name: v for name, v in zip(_COLS, _parse_values(row))}
    assert col["EquippedItemClass"] == "-1"

    y2 = tmp_path / "p.yaml"; y2.write_text(PROC_FAMILY_FIXTURE)
    out2 = tmp_path / "p.sql"
    assert _run(tmp_path, "--custom-sql", str(y2), "-o", str(out2)).returncode == 0
    prow = [l for l in out2.read_text().splitlines()
            if l.startswith("(920144,") and len(_parse_values(l)) == len(_COLS)][0]
    pcol = {name: v for name, v in zip(_COLS, _parse_values(prow))}
    assert pcol["EquippedItemClass"] == "-1"


def test_proc_attr_mask(tmp_path):
    """proc.attrMask flows to spell_proc.AttributesMask (2 = PROC_ATTR_TRIGGERED_CAN_PROC, needed
    to proc off triggered ticks like Blizzard's)."""
    d = _yaml.safe_load(PROC_FAMILY_FIXTURE)
    d["tabs"][0]["nodes"][0]["proc"]["attrMask"] = 2
    y = tmp_path / "m.yaml"; y.write_text(_yaml.safe_dump(d))
    out = tmp_path / "custom.sql"
    assert _run(tmp_path, "--custom-sql", str(y), "-o", str(out)).returncode == 0
    proc_cols = ["SpellId", "SchoolMask", "SpellFamilyName", "SpellFamilyMask0", "SpellFamilyMask1",
                 "SpellFamilyMask2", "ProcFlags", "SpellTypeMask", "SpellPhaseMask", "HitMask",
                 "AttributesMask", "DisableEffectsMask", "ProcsPerMinute", "Chance", "Cooldown",
                 "Charges"]
    rows = [l for l in out.read_text().splitlines() if l.startswith("(920144,")]
    proc = dict(zip(proc_cols, _parse_values(rows[1])))
    assert proc["AttributesMask"] == "2"


def test_proc_affects_mask_override(tmp_path):
    """`proc.affectsMask: {a,b,c}` overrides spell_proc's SpellFamilyMask VERBATIM, exactly as the
    node/effect-level key overrides an emitted EffectSpellClassMask (_resolve_affects_mask).

    Needed whenever the OR of the `affects:` spells' FULL identities would OVER-bind because a
    target carries a SHARED family bit. Motivating case: TBC rogue Find Weakness (node 20419)
    procs on FINISHING MOVES only — TBC's own proc mask is (0x3e0000, 0x9, 0), but Eviscerate /
    Envenom / Deadly Throw all also carry family-8 word-0 bit 23, the shared "offensive ability"
    bit that 88 rogue spells have, so the identity OR is (0xbe0000, 0x9, 0) and
    SpellInfo::IsAffected (which returns true on ANY nonzero word intersection) would fire the
    proc off Sinister Strike/Backstab/Ambush/Hemorrhage too. `affects:` is still authored (it
    drives the lint + documents intent); only the emitted mask changes."""
    d = _yaml.safe_load(PROC_FAMILY_FIXTURE)
    n = d["tabs"][0]["nodes"][0]
    n["proc"]["affectsMask"] = {"a": 0x3E0000, "b": 9}
    y = tmp_path / "m.yaml"; y.write_text(_yaml.safe_dump(d))
    out = tmp_path / "custom.sql"
    r = _run(tmp_path, "--custom-sql", str(y), "-o", str(out))
    assert r.returncode == 0, r.stderr
    proc_cols = ["SpellId", "SchoolMask", "SpellFamilyName", "SpellFamilyMask0", "SpellFamilyMask1",
                 "SpellFamilyMask2", "ProcFlags", "SpellTypeMask", "SpellPhaseMask", "HitMask",
                 "AttributesMask", "DisableEffectsMask", "ProcsPerMinute", "Chance", "Cooldown",
                 "Charges"]
    rows = [l for l in out.read_text().splitlines() if l.startswith("(920144,")]
    proc = dict(zip(proc_cols, _parse_values(rows[1])))
    assert proc["SpellFamilyName"] == "3"                    # still the _ref family
    assert proc["SpellFamilyMask0"] == str(0x3E0000)         # verbatim, NOT the identity OR (1|2)
    assert proc["SpellFamilyMask1"] == "9"
    assert proc["SpellFamilyMask2"] == "0"


def test_proc_affects_mask_alone_sets_the_family(tmp_path):
    """`affectsMask:` works WITHOUT an `affects:` list — the family still comes from _ref, so the
    row is family-scoped rather than family-0 (which IsAffected treats as 'matches everything')."""
    d = _yaml.safe_load(PROC_FAMILY_FIXTURE)
    proc = d["tabs"][0]["nodes"][0]["proc"]
    del proc["affects"]
    proc["affectsMask"] = {"a": 0x20000}
    y = tmp_path / "m.yaml"; y.write_text(_yaml.safe_dump(d))
    out = tmp_path / "custom.sql"
    r = _run(tmp_path, "--custom-sql", str(y), "-o", str(out))
    assert r.returncode == 0, r.stderr
    rows = [l for l in out.read_text().splitlines() if l.startswith("(920144,")]
    vals = _parse_values(rows[1])
    assert vals[2] == "3" and vals[3] == str(0x20000) and vals[4] == "0"


def test_lint_rejects_all_zero_proc_affects_mask():
    d = _yaml.safe_load(PROC_FAMILY_FIXTURE)
    d["tabs"][0]["nodes"][0]["proc"]["affectsMask"] = {"a": 0, "b": 0, "c": 0}
    with pytest.raises(ValueError, match="affectsMask is all-zero"):
        _lint(d)


def test_lint_rejects_helper_out_of_band():
    d = _yaml.safe_load(PROC_FIXTURE)
    d["helpers"] = [{"id": 5, "name": "bad", "aura": 75}]
    with pytest.raises(ValueError, match="out of reserved era band"):
        _lint(d)


def test_lint_rejects_templated_helper_without_effects():
    d = _yaml.safe_load(PROC_FIXTURE)
    d["helpers"] = [{"id": 920921, "name": "X", "template": 10060, "aura": 79}]
    with pytest.raises(ValueError, match="requires an authored 'effects:' list"):
        _lint(d)


PRIEST_MULTI_FIXTURE = textwrap.dedent(r"""
era: 0
class: 5
eraName: Vanilla
className: PRIEST
_ref: { familyName: 6, spells: {} }
tabs:
  - key: 2
    name: Holy
    background: PriestHoly
    nodes:
      - id: 18130
        slug: spiritual_guidance
        name: "Spiritual Guidance"
        tab: 2
        tier: 4
        col: 2
        icon: "iconSG"
        maxRank: 5
        mechanic: multi
        effects:
          - { type: stat, aura: 175, misc: 127, miscB: 4, vals: [5,10,15,20,25] }   # MOD_SPELL_DAMAGE_OF_STAT_PERCENT, stat 4 = Spirit
          - { type: stat, aura: 176, misc: 127, miscB: 4, vals: [5,10,15,20,25] }   # MOD_SPELL_HEALING_OF_STAT_PERCENT
        tooltip: ["5%","10%","15%","20%","25%"]
""")


def test_priest_class_token_and_stat_of_stat(tmp_path):
    import importlib.util
    spec = importlib.util.spec_from_file_location("g", "tools/gen_era_talents.py")
    m = importlib.util.module_from_spec(spec); spec.loader.exec_module(m)
    assert m.CLASS_TOKENS[5] == "PRIEST"

    y = tmp_path / "priest.yaml"; y.write_text(PRIEST_MULTI_FIXTURE)
    out = tmp_path / "custom.sql"
    r = _run(tmp_path, "--custom-sql", str(y), "-o", str(out))
    assert r.returncode == 0, r.stderr
    sql = out.read_text()
    pid = 920000 + (18130 - 18000) * 8  # 921040
    assert f"DELETE FROM `spell_dbc` WHERE `ID`={pid};" in sql
    row = [l for l in sql.splitlines() if l.startswith(f"({pid},")][0]
    col = {name: v for name, v in zip(_COLS, _parse_values(row))}
    assert col["EffectAura_1"] == "175" and col["EffectMiscValueB_1"] == "4"
    assert col["EffectAura_2"] == "176" and col["EffectMiscValueB_2"] == "4"
    assert col["SpellClassSet"] == "0"   # pure-stat multi stays family-less


WARLOCK_PET_FIXTURE = textwrap.dedent(r"""
era: 0
class: 9
eraName: Vanilla
className: WARLOCK
_ref: { familyName: 5, spells: {} }
tabs:
  - key: 2
    name: Demonology
    background: WarlockDemonology
    nodes:
      - id: 18205
        slug: fel_stamina
        name: "Fel Stamina"
        tab: 2
        tier: 2
        col: 0
        icon: "iconFS"
        maxRank: 3
        mechanic: pet
        pet: 0                     # 0 = all demons
        petAura: 137              # SPELL_AURA_MOD_TOTAL_STAT_PERCENTAGE (verify enum in authoring)
        petMisc: 2                # STAT_STAMINA
        vals: [3, 6, 9]           # +% pet stamina per rank
        tooltip: ["3%","6%","9%"]
""")


def test_pet_mechanic_emits_passive_petbuff_and_petaura_rows(tmp_path):
    import importlib.util
    spec = importlib.util.spec_from_file_location("g", "tools/gen_era_talents.py")
    g = importlib.util.module_from_spec(spec); spec.loader.exec_module(g)
    assert g.CLASS_TOKENS[9] == "WARLOCK"
    y = tmp_path/"w.yaml"; y.write_text(WARLOCK_PET_FIXTURE)
    out = tmp_path/"custom.sql"
    r = _run(tmp_path, "--custom-sql", str(y), "-o", str(out))
    assert r.returncode == 0, r.stderr
    sql = out.read_text()
    passive_r1 = 920000 + (18205-18000)*8 + 0        # 921640
    # pet-buff ids are keyed by pet-node ORDINAL (not the node-id multiplier): node 18205 is the
    # only pet node here -> ordinal 0 -> block base 928000, variant 0, rank 1 -> 928000.
    petbuff_r1 = 928000                              # PET_BUFF_BASE + 0*64 + 0*maxRank + 0
    # (a) talent passive exists and carries the STOCK pet-aura talent shape at index 0:
    # APPLY_AURA(SPELL_AURA_DUMMY) targeting the caster (WotLK MD 23785's encoding). A bare
    # SPELL_EFFECT_DUMMY never registered the PetAura at learn-cast (live-verified 2026-08-16:
    # m_petAuras stayed empty); the dummy AURA registers via HandleAuraDummy apply/remove.
    row = [l for l in sql.splitlines() if l.startswith(f"({passive_r1},")][0]
    col = {name:v for name,v in zip(_COLS, _parse_values(row))}
    assert col["Effect_1"] == "6" and col["EffectAura_1"] == "4", \
        "pet talent passive must carry APPLY_AURA(SPELL_AURA_DUMMY) at eff0"
    assert col["ImplicitTargetA_1"] == "1", "pet talent passive dummy aura must target the caster"
    # (b) the pet-buff spell_dbc row exists with the stat aura + self target
    prow = [l for l in sql.splitlines() if l.startswith(f"({petbuff_r1},")][0]
    pcol = {name:v for name,v in zip(_COLS, _parse_values(prow))}
    assert pcol["EffectAura_1"] == "137" and pcol["EffectMiscValue_1"] == "2"
    # NB: the real spell_dbc column is ImplicitTargetA_1 (there is no "EffectImplicitTargetA_1");
    # 1 == TARGET_UNIT_CASTER, i.e. the pet casts the buff on itself (verified in Unit::CastPetAura).
    assert pcol["ImplicitTargetA_1"] == "1"
    assert pcol["EffectBasePoints_1"] == "2"          # eff-1 convention (val 3 -> 2), same as _stat_row
    # (c) the spell_pet_auras mapping row exists for rank 1
    assert f"DELETE FROM `spell_pet_auras` WHERE `spell`={passive_r1}" in sql
    assert f"({passive_r1}, 0, 0, {petbuff_r1})" in sql or f"({passive_r1},0,0,{petbuff_r1})" in sql
    # (d) three ranks => three passive/petbuff/petaura triples
    assert sql.count("INSERT INTO `spell_pet_auras`") >= 1
    for rank in range(3):
        assert f"({920000+(18205-18000)*8+rank}," in sql   # talent passive (node-id keyed)
        assert f"({928000+rank}," in sql                   # pet-buff (ordinal 0 block, rank-indexed)


WARLOCK_PET_VARIANTS_FIXTURE = textwrap.dedent(r"""
era: 0
class: 9
eraName: Vanilla
className: WARLOCK
_ref: { familyName: 5, spells: {} }
tabs:
  - key: 2
    name: Demonology
    background: WarlockDemonology
    nodes:
      - id: 18210
        slug: master_demonologist
        name: "Master Demonologist"
        tab: 2
        tier: 4
        col: 1
        icon: "iconMD"
        maxRank: 2
        mechanic: pet
        petVariants:                       # per-demon: one pet-buff + spell_pet_auras row each
          - { pet: 416, petAura: 79,  vals: [4, 8] }   # Imp
          - { pet: 417, petAura: 158, vals: [4, 8] }   # Voidwalker
        tooltip: ["r1","r2"]
""")


def test_pet_variants_emit_distinct_buffs_and_two_petaura_rows(tmp_path):
    y = tmp_path/"w.yaml"; y.write_text(WARLOCK_PET_VARIANTS_FIXTURE)
    out = tmp_path/"custom.sql"
    r = _run(tmp_path, "--custom-sql", str(y), "-o", str(out))
    assert r.returncode == 0, r.stderr
    sql = out.read_text()
    # node 18210 is the only pet node -> ordinal 0 -> block base 928000. Within the block:
    # id = 928000 + variant*maxRank(2) + (rank-1). Talent passives stay node-id keyed (921680/1).
    passive_r1 = 920000 + (18210-18000)*8 + 0        # 921680
    passive_r2 = passive_r1 + 1                       # 921681
    # (a) distinct pet-buff ids per (variant, rank): v0 -> 928000/928001, v1 -> 928002/928003
    v0r1, v0r2, v1r1, v1r2 = 928000, 928001, 928002, 928003
    for bid, aura in ((v0r1, "79"), (v1r1, "158")):
        brow = [l for l in sql.splitlines() if l.startswith(f"({bid},")][0]
        bcol = {name:v for name,v in zip(_COLS, _parse_values(brow))}
        assert bcol["EffectAura_1"] == aura
        assert bcol["ImplicitTargetA_1"] == "1"
        assert bcol["EffectBasePoints_1"] == "3"     # val 4 -> 3 (eff-1)
    # (b) TWO spell_pet_auras rows per rank, one per demon, sharing the passive at effectId 0
    assert f"({passive_r1}, 0, 416, {v0r1})" in sql
    assert f"({passive_r1}, 0, 417, {v1r1})" in sql
    assert f"({passive_r2}, 0, 416, {v0r2})" in sql
    assert f"({passive_r2}, 0, 417, {v1r2})" in sql
    # each rank's INSERT carries both demon rows (comma-joined, one statement per passive)
    assert f"DELETE FROM `spell_pet_auras` WHERE `spell`={passive_r1}" in sql


def test_lint_rejects_pet_variants_with_bare_petaura():
    d = _yaml.safe_load(WARLOCK_PET_VARIANTS_FIXTURE)
    d["tabs"][0]["nodes"][0]["petAura"] = 137        # bare node-level petAura alongside petVariants
    with pytest.raises(ValueError, match="petAura|petVariants"):
        _lint(d)


def test_lint_rejects_pet_variant_block_overflow():
    """The guard that would have caught the old too-small block: a node whose variants*maxRank
    exceeds the per-node PET_BUFF_BLOCK (64) must hard-error, regardless of neighbors."""
    d = _yaml.safe_load(WARLOCK_PET_VARIANTS_FIXTURE)   # maxRank 2 -> need >32 variants to bust 64
    d["tabs"][0]["nodes"][0]["petVariants"] = [
        {"pet": 400 + i, "petAura": 79, "vals": [4, 8]} for i in range(33)]   # 33*2 = 66 > 64
    with pytest.raises(ValueError, match="block|variants|pet-buff"):
        _lint(d)


def test_stat_equip_requirement_fields(tmp_path):
    """A stat node can carry a weapon equip-requirement (Wand Specialization)."""
    d = _yaml.safe_load(PROC_FIXTURE)  # reuse arcane tab shell
    d["tabs"][0]["nodes"][0] = {
        "id": 18004, "slug": "wand_spec", "name": "Wand Specialization", "tab": 1,
        "tier": 1, "col": 0, "icon": "", "maxRank": 2, "mechanic": "stat", "aura": 79,
        "school": 1, "vals": [13, 25], "equipClass": 2, "equipSubclass": 262144,
        "tooltip": ["r1", "r2"],
    }
    y = tmp_path / "m.yaml"; y.write_text(_yaml.safe_dump(d))
    out = tmp_path / "custom.sql"
    assert _run(tmp_path, "--custom-sql", str(y), "-o", str(out)).returncode == 0
    row = [l for l in out.read_text().splitlines() if l.startswith("(920032,")][0]  # 18004 r1
    col = {name: v for name, v in zip(_COLS, _parse_values(row))}
    assert col["EffectAura_1"] == "79" and col["EffectMiscValue_1"] == "1"
    assert col["EquippedItemClass"] == "2"
    assert col["EquippedItemSubclass"] == "262144"


def test_castable_helper_multi_effect():
    h = {"id": 920920, "name": "Power Infusion", "powerCost": 182, "rangeIndex": 4,
         "castTimeIndex": 1, "cooldown": 180000, "durationIndex": 8,
         "effects": [{"effect": 6, "aura": 79, "basePoints": 20, "misc": 126, "targetA": 21},
                     {"effect": 6, "aura": 136, "basePoints": 20, "targetA": 21}]}
    o = G.helper_overrides(h)
    assert o["ManaCost"] == 182 and o["RangeIndex"] == 4 and o["RecoveryTime"] == 180000
    assert o["CastingTimeIndex"] == 1 and o["DurationIndex"] == 8
    assert (o["Effect_1"], o["EffectAura_1"], o["EffectBasePoints_1"],
            o["EffectMiscValue_1"], o["ImplicitTargetA_1"]) == (6, 79, 20, 126, 21)
    assert (o["Effect_2"], o["EffectAura_2"], o["EffectBasePoints_2"],
            o["ImplicitTargetA_2"]) == (6, 136, 20, 21)


def test_castable_helper_trigger_effect():
    h = {"id": 921152, "name": "Vampiric Embrace", "durationIndex": 3,
         "effects": [{"effect": 6, "aura": 4, "targetA": 6},
                     {"effect": 64, "trigger": 932950, "targetA": 1}]}
    o = G.helper_overrides(h)
    assert (o["Effect_1"], o["EffectAura_1"], o["ImplicitTargetA_1"]) == (6, 4, 6)
    assert (o["Effect_2"], o["EffectTriggerSpell_2"], o["ImplicitTargetA_2"]) == (64, 932950, 1)


def test_single_effect_helper_unchanged():
    h = {"id": 932960, "name": "Shadow Vulnerability", "aura": 87, "basePoints": 3, "misc": 32,
         "school": 32, "targetA": 6, "durationIndex": 8, "stackAmount": 5}
    o = G.helper_overrides(h)
    assert (o["Effect_1"], o["EffectAura_1"], o["EffectBasePoints_1"], o["EffectMiscValue_1"],
            o["ImplicitTargetA_1"], o["CumulativeAura"]) == (6, 87, 3, 32, 6, 5)


def _effect_flag96(row_values, effect_idx):
    """Decode one effect's EffectSpellClassMask flag96 (3x uint32 words) from a full generated
    234-value row, honoring the real column convention: the LETTER = effect index (A=Effect_1,
    B=Effect_2, C=Effect_3) and the _1/_2/_3 suffix = family word0/1/2. Returns [word0, word1, word2]."""
    letter = "ABC"[effect_idx - 1]
    idx = {c: i for i, c in enumerate(G.COLS)}
    return [row_values[idx[f"EffectSpellClassMask{letter}_{w}"]] for w in (1, 2, 3)]


def test_helper_effect_per_effect_class_mask():
    """A helper effect's maskA/maskB/maskC (word0/1/2) land in the flag96 of THAT effect (Part A) —
    lets one helper carry two spellmod effects each bound to a distinct spell set (Elemental
    Mastery 932413: Effect_1 crit + Effect_2 cost, both on the fire/frost/nature damage set).

    Decodes the generated row against the flag96[3] layout (letter=effect, _1/_2/_3=word) rather
    than trusting column names — so a loop that transposes the effect/word axes FAILS here (it would
    put Effect_2's words in EffectSpellClassMaskA_2/B_2/C_2, and leave Effect_2's own flag96
    EffectSpellClassMaskB_1/2/3 all-zero, which the core treats as 'affects the whole family')."""
    DAMAGE_SET = -1877999613
    h = {"id": 932413, "name": "Elemental Mastery", "template": 16166,
         "effects": [{"effect": 6, "aura": 107, "misc": 7, "basePoints": 99, "dieSides": 1,
                      "targetA": 1, "maskA": DAMAGE_SET, "maskB": 0, "maskC": 0},
                     {"effect": 6, "aura": 108, "misc": 14, "basePoints": -100, "dieSides": 0,
                      "targetA": 1, "maskA": DAMAGE_SET, "maskB": 0, "maskC": 0}]}
    o = G.helper_overrides(h, templated=True)
    # Correct cells: Effect_1 -> A_1/A_2/A_3, Effect_2 -> B_1/B_2/B_3.
    assert (o["EffectSpellClassMaskA_1"], o["EffectSpellClassMaskA_2"],
            o["EffectSpellClassMaskA_3"]) == (DAMAGE_SET, 0, 0)
    assert (o["EffectSpellClassMaskB_1"], o["EffectSpellClassMaskB_2"],
            o["EffectSpellClassMaskB_3"]) == (DAMAGE_SET, 0, 0)
    # Decode a full generated row (template = zero-filled 234 values) against flag96[3].
    row = G._row_values(o, _template_row(16166, ""))
    assert _effect_flag96(row, 1) == [DAMAGE_SET, 0, 0]          # Effect_1 crit
    eff2 = _effect_flag96(row, 2)
    assert eff2 == [DAMAGE_SET, 0, 0]                            # Effect_2 cost
    assert any(eff2), "Effect_2 mask must be NON-zero (all-zero = affects the whole family)"


def test_helper_effect_without_mask_keys_does_not_emit_columns():
    """An effect that omits maskA/maskB/maskC emits NO EffectSpellClassMask column, so a templated
    helper keeps INHERITING the cloned record's per-effect masks (purely additive — Part A)."""
    h = {"id": 932412, "name": "Nature's Swiftness", "template": 16188,
         "effects": [{"effect": 6, "aura": 108, "basePoints": -101, "dieSides": 1,
                      "misc": 10, "targetA": 1}]}
    o = G.helper_overrides(h, templated=True)
    for axis in ("A", "B", "C"):
        for i in (1, 2, 3):
            assert f"EffectSpellClassMask{axis}_{i}" not in o


def test_helper_effect_mechanic_column():
    """A helper effect's `mechanic` key writes EffectMechanic_N — the per-effect MECHANIC_* the
    core reads for mechanic immunity (Unit::IsImmunedToSpellEffect) and for the diminishing-returns
    group (SpellMgr::GetDiminishingReturnsGroupForSpell, e.g. DIMINISHING_DISARM off
    MECHANIC_DISARM). Motivating case: the TBC rogue Riposte clone (node 20428) re-authors stock
    14251's Effect_2 from live's aura 138 MOD_MELEE_HASTE (EffectMechanic 8 = SLOW_ATTACK) to TBC's
    aura 67 MOD_DISARM, which needs EffectMechanic 3 = DISARM. Without the key the templated clone
    silently INHERITS 8, so a disarm-immune target would be checked for slow-attack immunity."""
    h = {"id": 947777, "name": "Riposte", "template": 14251,
         "effects": [{"effect": 31, "basePoints": 149, "dieSides": 1, "targetA": 6},
                     {"effect": 6, "aura": 67, "basePoints": 0, "dieSides": 0,
                      "mechanic": 3, "targetA": 6}]}
    o = G.helper_overrides(h, templated=True)
    assert o["EffectMechanic_2"] == 3
    assert "EffectMechanic_1" not in o
    assert "EffectMechanic_3" not in o


def test_helper_effect_without_mechanic_key_does_not_emit_column():
    """An effect that omits `mechanic` emits NO EffectMechanic column, so a templated helper keeps
    INHERITING the cloned record's per-effect mechanic (purely additive, like radius/amplitude —
    byte-neutral for every already-shipped helper)."""
    h = {"id": 931190, "name": "Healing Stream Totem Pulse", "template": 25567,
         "effects": [{"effect": 6, "aura": 24, "basePoints": 4, "targetA": 1}]}
    o = G.helper_overrides(h, templated=True)
    for i in (1, 2, 3):
        assert f"EffectMechanic_{i}" not in o


def test_helper_effect_amplitude_sets_aura_period():
    """A periodic-aura effect's `amplitude` key sets EffectAuraPeriod_N (the aura tick period in
    ms) — needed because core's AuraEffect::CalculatePeriodic broken-dbc guard defaults an
    unset/zero period to 1000ms, which silently doubles the tick rate of any custom periodic
    aura (e.g. PERIODIC_ENERGIZE) authored without it."""
    h = {"id": 931210, "name": "Mana Spring Totem Pulse", "template": 24379,
         "effects": [{"effect": 6, "aura": 24, "basePoints": 4, "amplitude": 2000}]}
    o = G.helper_overrides(h, templated=True)
    assert o["EffectAuraPeriod_1"] == 2000


def test_helper_effect_without_amplitude_key_does_not_emit_column():
    """An effect that omits `amplitude` emits NO EffectAuraPeriod column, so a templated helper
    keeps INHERITING the cloned record's period (purely additive, like radius/itemType)."""
    h = {"id": 931190, "name": "Healing Stream Totem Pulse", "template": 25567,
         "effects": [{"effect": 6, "aura": 24, "basePoints": 4}]}
    o = G.helper_overrides(h, templated=True)
    for i in (1, 2, 3):
        assert f"EffectAuraPeriod_{i}" not in o


def test_helper_effect_target_b_column():
    """A helper effect's `targetB` writes ImplicitTargetB_N (e.g. TARGET_UNIT_SRC_AREA_ENEMY=15
    paired with a TARGET_SRC_CASTER=22 source-setter — the proven-working Magma Totem Pulse 8187
    shape) — needed for a src-area AoE, since targetA=22 only sets the source POINT and a
    template-inherited ImplicitTargetB reading a destination the source-setter never fills (e.g.
    TARGET_UNIT_DEST_AREA_ENEMY=16) hits nothing."""
    h = {"id": 931358, "name": "Fire Nova", "template": 8349,
         "effects": [{"effect": 2, "basePoints": 47, "dieSides": 9, "targetA": 22,
                      "targetB": 15, "radius": 13}]}
    o = G.helper_overrides(h, templated=True)
    assert o["ImplicitTargetA_1"] == 22
    assert o["ImplicitTargetB_1"] == 15


def test_helper_effect_without_target_b_key_does_not_emit_column():
    """An effect that omits `targetB` emits NO ImplicitTargetB column, so a templated helper keeps
    INHERITING the cloned record's ImplicitTargetB (purely additive, like radius/amplitude)."""
    h = {"id": 931190, "name": "Healing Stream Totem Pulse", "template": 25567,
         "effects": [{"effect": 6, "aura": 24, "basePoints": 4, "targetA": 1}]}
    o = G.helper_overrides(h, templated=True)
    for i in (1, 2, 3):
        assert f"ImplicitTargetB_{i}" not in o


def test_helper_proc_emits_spell_proc():
    d = {"tabs": [], "helpers": [{"id": 932950, "name": "Vampiric Embrace", "aura": 4,
         "basePoints": 20, "durationIndex": 3,
         "proc": {"flags": 327680, "school": 32, "family": 6, "attrMask": 2, "chance": 100}}]}
    sql = G.emit_custom_sql(d)
    assert "INSERT INTO `spell_proc`" in sql
    assert "DELETE FROM `spell_proc` WHERE `SpellId`=932950;" in sql
    # [-1]: the spell_proc row is emitted AFTER the spell_dbc row, and both happen to start with
    # "(932950," (SpellId == the helper's own ID) — [0] would silently match the wrong row.
    proc_line = [l for l in sql.splitlines() if l.startswith("(932950,")][-1]
    proc_cols = ["SpellId", "SchoolMask", "SpellFamilyName", "SpellFamilyMask0",
                 "SpellFamilyMask1", "SpellFamilyMask2", "ProcFlags", "SpellTypeMask",
                 "SpellPhaseMask", "HitMask", "AttributesMask", "DisableEffectsMask",
                 "ProcsPerMinute", "Chance", "Cooldown", "Charges"]
    proc = dict(zip(proc_cols, _parse_values(proc_line)))
    assert proc["SchoolMask"] == "32"
    assert proc["SpellFamilyName"] == "6"
    assert proc["ProcFlags"] == "327680"
    assert proc["AttributesMask"] == "2"
    assert proc["Chance"] == "100"
    assert proc["Charges"] == "0"


def test_lint_accepts_helper_effects_without_aura():
    d = {"tabs": [{"key": 1, "name": "t", "nodes": []}],
         "helpers": [{"id": 932951, "name": "VE Heal",
                      "effects": [{"effect": 10, "basePoints": 0, "targetA": 1}]}]}
    assert G.lint(d) is True


def test_lint_rejects_helper_more_than_3_effects():
    d = {"tabs": [{"key": 1, "name": "t", "nodes": []}],
         "helpers": [{"id": 932952, "name": "Too Many Effects",
                      "effects": [{"effect": 6, "aura": 4, "targetA": 1},
                                  {"effect": 6, "aura": 5, "targetA": 1},
                                  {"effect": 6, "aura": 6, "targetA": 1},
                                  {"effect": 6, "aura": 7, "targetA": 1}]}]}
    with pytest.raises(ValueError, match="3 effects"):
        G.lint(d)


def test_lint_rejects_helper_proc_without_flags():
    d = {"tabs": [{"key": 1, "name": "t", "nodes": []}],
         "helpers": [{"id": 932953, "name": "Bad Proc", "aura": 4, "proc": {"chance": 100}}]}
    with pytest.raises(ValueError, match="proc needs flags"):
        G.lint(d)


def _proc_row_vals(proc, spell_id=932950):
    """Emit one spell_proc row via the shared helper-proc path and return {col: value-string}."""
    cols, row = G._spell_proc_row_for({}, spell_id, proc, proc.get("chance", 100))
    return dict(zip(cols, _parse_values(row)))


def test_proc_hitmask_defaults_to_zero():
    # Omitting hitMask must yield the core default 0 (NORMAL|CRIT|ABSORB for done procs),
    # NOT 8 (PROC_HIT_FULL_RESIST) — HitMask=8 made Vampiric Embrace proc only on full resists.
    vals = _proc_row_vals({"flags": 327680, "school": 32, "family": 6})
    assert vals["HitMask"] == "0"


def test_proc_ppm_column():
    # `ppm` -> spell_proc.ProcsPerMinute. The core only falls back to the DBC ProcChance when BOTH
    # Chance and ProcsPerMinute are 0 (SpellMgr::LoadSpellProcs), so a clone of a PPM-rated stock
    # proc (TBC shaman Shamanistic Rage: stock 30823 is ProcsPerMinute 18 / Chance 0) must be able
    # to author both. Omitting `ppm` keeps the historical 0 — byte-neutral for shipped data.
    vals = _proc_row_vals({"flags": 20, "typeMask": 1, "phaseMask": 2, "ppm": 18, "chance": 0})
    assert vals["ProcsPerMinute"] == "18"
    assert vals["Chance"] == "0"
    assert _proc_row_vals({"flags": 20})["ProcsPerMinute"] == "0"


def test_proc_ppm_keeps_a_fractional_rate():
    # THE CASE THE KEY EXISTS FOR. `spell_proc.ProcsPerMinute` is a FLOAT column the core reads as
    # a float, and both PPM rows this repo already ships by hand are 3.5 (the Vanilla and TBC druid
    # Omen of Clarity procs — the most likely next user of this key). An int() cast would emit 3, a
    # 14% rate error with no exception and no audit finding.
    assert _proc_row_vals({"flags": 20, "ppm": 3.5, "chance": 0})["ProcsPerMinute"] == "3.5"
    # ...while an integral rate still emits as an int, so every already-committed row stays
    # byte-identical (str(18.0) would be "18.0" and would move all 13 datasets).
    assert _proc_row_vals({"flags": 20, "ppm": 18.0, "chance": 0})["ProcsPerMinute"] == "18"
    assert _proc_row_vals({"flags": 20, "ppm": 0})["ProcsPerMinute"] == "0"


def test_proc_ppm_rejects_a_live_chance():
    # Aura::CalcProcChance (SpellAuras.cpp:2270) overwrites Chance with the PPM result whenever
    # ProcsPerMinute != 0 and the event carries damage or heal info, so an authored non-zero chance
    # is dead for every ordinary event and half-live for a damage-less one. Same "explicitly
    # authored value the core would ignore is a hard error" rule the ProcFlags-driven fields follow.
    with pytest.raises(ValueError, match="unusable alongside"):
        _proc_row_vals({"flags": 20, "ppm": 18, "chance": 50})
    # an explicit 0 is the stock PPM-row shape and stays legal
    assert _proc_row_vals({"flags": 20, "ppm": 18, "chance": 0})["Chance"] == "0"
    # ...and so does omitting it: the default 100 is zeroed silently rather than shipped half-live
    assert _proc_row_vals({"flags": 20, "ppm": 18})["Chance"] == "0"


def test_proc_ppm_rejects_a_negative_rate():
    # SpellMgr.cpp:2096 clamps a negative ProcsPerMinute to 0 at boot and logs an sql.sql error —
    # i.e. the row silently loses its rate. Refuse at generation time instead.
    with pytest.raises(ValueError, match="is negative"):
        _proc_row_vals({"flags": 20, "ppm": -1, "chance": 0})


def test_proc_hitmask_explicit_value_kept():
    vals = _proc_row_vals({"flags": 680, "hitMask": 2})
    assert vals["HitMask"] == "2"
    # 680 is a TAKEN proc: it has no spell-phase (phase is a DONE-side concept), so the
    # phase field zeros from its default; typeMask is a usable field and keeps its default.
    assert vals["SpellPhaseMask"] == "0"
    assert vals["SpellTypeMask"] == "1"


def test_proc_fields_zeroed_when_inapplicable():
    # PROC_FLAG_KILL (0x2) is neither a spell nor a hit proc: SpellTypeMask, SpellPhaseMask
    # and HitMask are all meaningless and the core logs "won't be used" errors — emit 0s.
    # No explicit typeMask/phaseMask/hitMask keys here, so all three zero silently (defaults).
    vals = _proc_row_vals({"flags": 2})
    assert vals["SpellTypeMask"] == "0"
    assert vals["SpellPhaseMask"] == "0"
    assert vals["HitMask"] == "0"


def test_proc_explicit_unusable_field_rejected():
    # Same flags as above (0x2 = PROC_FLAG_KILL), but hitMask is now EXPLICITLY authored in the
    # YAML — the core would silently ignore it, so the generator must hard-error instead of
    # discarding the author's intent.
    with pytest.raises(ValueError):
        _proc_row_vals({"flags": 2, "hitMask": 2})


def test_proc_fields_kept_when_applicable():
    # 65536 = PROC_FLAG_DONE_SPELL_MAGIC_DMG_CLASS_NEG: in SPELL_PROC_FLAG_MASK,
    # REQ_SPELL_PHASE_PROC_FLAG_MASK and DONE_HIT_PROC_FLAG_MASK — all three fields apply.
    vals = _proc_row_vals({"flags": 65536, "typeMask": 1, "phaseMask": 2, "hitMask": 2})
    assert vals["SpellTypeMask"] == "1"
    assert vals["SpellPhaseMask"] == "2"
    assert vals["HitMask"] == "2"


def _bcd():
    """Import tools/build_client_dbc.py as a module (it self-inserts tools/ into sys.path)."""
    spec = _ilu.spec_from_file_location("build_client_dbc", "tools/build_client_dbc.py")
    m = _ilu.module_from_spec(spec)
    spec.loader.exec_module(m)
    return m


def _template_row(spell_id, aura_desc):
    """A 234-value template row: zero-filled except ID and AuraDescription."""
    vals = G._row_values({"ID": spell_id})
    vals[G.COLS.index("AuraDescription_Lang_enUS")] = aura_desc
    return vals


def _write_dataset(tmp_path, helper_yaml):
    p = tmp_path / "ds.yaml"
    p.write_text("era: 0\nclass: 5\ntabs: []\nhelpers:\n" + helper_yaml)
    return str(p)


def test_client_aura_description_written(tmp_path):
    ds = _write_dataset(tmp_path, textwrap.dedent("""\
        - id: 920920
          name: Power Infusion
          template: 10060
          effects:
          - {effect: 6, aura: 79, basePoints: 20, targetA: 21}
          client:
            name: Power Infusion
            icon: 1872
            description: "Spellbook text."
            auraDescription: "Spell damage and healing increased by 20%."
    """))
    bcd = _bcd()
    rows = bcd._client_rows([ds], {10060: _template_row(10060, "WotLK haste text $s1%.")})
    vals = rows[920920]
    assert vals[G.COLS.index("AuraDescription_Lang_enUS")] == \
        "Spell damage and healing increased by 20%."


def test_client_aura_description_required_when_template_leaks(tmp_path):
    # Template carries a non-empty AuraDescription and the clone applies a visible aura:
    # omitting client.auraDescription would show the STOCK WotLK buff text — hard error.
    ds = _write_dataset(tmp_path, textwrap.dedent("""\
        - id: 920920
          name: Power Infusion
          template: 10060
          effects:
          - {effect: 6, aura: 79, basePoints: 20, targetA: 21}
          client:
            name: Power Infusion
            icon: 1872
            description: "Spellbook text."
    """))
    bcd = _bcd()
    with pytest.raises(SystemExit, match="auraDescription"):
        bcd._client_rows([ds], {10060: _template_row(10060, "WotLK haste text $s1%.")})


def test_client_aura_description_not_required_without_leak(tmp_path):
    # Template AuraDescription empty -> nothing can leak -> no requirement.
    ds = _write_dataset(tmp_path, textwrap.dedent("""\
        - id: 920921
          name: Some Buff
          template: 10060
          effects:
          - {effect: 6, aura: 79, basePoints: 20, targetA: 21}
          client: {name: Some Buff, icon: 1}
    """))
    bcd = _bcd()
    rows = bcd._client_rows([ds], {10060: _template_row(10060, "")})
    assert 920921 in rows


def test_client_aura_description_not_required_for_non_aura_effects(tmp_path):
    # Template leaks a non-empty AuraDescription, but the authored effects don't apply an aura
    # (no effect: 6) -- nothing would show it, so no auraDescription requirement.
    ds = _write_dataset(tmp_path, textwrap.dedent("""\
        - id: 920921
          name: Some Proc
          template: 10060
          effects:
          - {effect: 10, basePoints: 0, targetA: 21}
          client: {name: Some Proc, icon: 1}
    """))
    bcd = _bcd()
    rows = bcd._client_rows([ds], {10060: _template_row(10060, "WotLK haste text.")})
    assert 920921 in rows


def test_client_aura_description_leak_check_uses_template_effects(tmp_path):
    # A standalone client-patch build (client-patch/build-client-patch.sh invokes
    # build_client_dbc.py directly) never runs the generator's own lint() -- so a helper with a
    # `template:` and NO authored `effects:` at all (lint would reject that shape) can still reach
    # _client_rows. With no authored effects, the clone ships the TEMPLATE's own Effect_i/aura
    # slots wholesale, so the leak check must inspect THOSE, not the (empty) authored list.
    ds = _write_dataset(tmp_path, textwrap.dedent("""\
        - id: 920922
          name: Some Templated Helper
          template: 10060
          client: {name: Some Templated Helper, icon: 1}
    """))
    tmpl = _template_row(10060, "WotLK haste text.")
    tmpl[G.COLS.index("Effect_1")] = 6
    bcd = _bcd()
    with pytest.raises(SystemExit, match="auraDescription"):
        bcd._client_rows([ds], {10060: tmpl})


def test_client_rank_label_written(tmp_path):
    # A `rank:` int on a helper's client block -> NameSubtext_Lang ("Rank N"), the 3.3.5a client's
    # spellbook rank field. Drives the same-name rank collapse + the visible "Rank N" label.
    ds = _write_dataset(tmp_path, textwrap.dedent("""\
        - id: 932902
          name: Siphon Life
          aura: 53
          basePoints: 15
          client:
            name: Siphon Life
            icon: 152
            rank: 1
    """))
    bcd = _bcd()
    rows = bcd._client_rows([ds], {})
    vals = rows[932902]
    assert vals[G.COLS.index("NameSubtext_Lang_enUS")] == "Rank 1"
    assert vals[G.COLS.index("NameSubtext_Lang_Mask")] == 16712190


def test_client_rank_label_absent_without_key(tmp_path):
    # No `rank:` -> the rank field stays at its default (no "Rank N"), unchanged from prior behavior.
    ds = _write_dataset(tmp_path, textwrap.dedent("""\
        - id: 932902
          name: Siphon Life
          aura: 53
          basePoints: 15
          client: {name: Siphon Life, icon: 152}
    """))
    bcd = _bcd()
    rows = bcd._client_rows([ds], {})
    vals = rows[932902]
    assert vals[G.COLS.index("NameSubtext_Lang_enUS")] == 0   # template-less default (no override)


def test_client_rank_label_overrides_template(tmp_path):
    # With a template, the "Rank N" label must OVERRIDE whatever rank text the cloned stock row had.
    ds = _write_dataset(tmp_path, textwrap.dedent("""\
        - id: 932919
          name: Siphon Life
          template: 35195
          effects:
          - {effect: 6, aura: 53, basePoints: 22, targetA: 6}
          client:
            name: Siphon Life
            icon: 152
            rank: 2
    """))
    tmpl = _template_row(35195, "")
    tmpl[G.COLS.index("NameSubtext_Lang_enUS")] = "Stock Rank"
    bcd = _bcd()
    rows = bcd._client_rows([ds], {35195: tmpl})
    assert rows[932919][G.COLS.index("NameSubtext_Lang_enUS")] == "Rank 2"


def test_generation_stamp_deterministic_and_content_sensitive(tmp_path):
    a = tmp_path / "a.yaml"; b = tmp_path / "b.yaml"
    a.write_text("era: 0\n"); b.write_text("class: 5\n")
    s1 = G.generation_stamp([str(a), str(b)])
    s2 = G.generation_stamp([str(b), str(a)])       # order-insensitive (sorted by basename)
    assert s1 == s2 and len(s1) == 8
    int(s1, 16)   # must parse as hex
    a.write_text("era: 1\n")
    assert G.generation_stamp([str(a), str(b)]) != s1


def test_generation_stamp_era_dir_aware_no_aliasing(tmp_path):
    """Two datasets sharing a basename under DIFFERENT era dirs (era-data/tbc/mage.yaml vs
    era-data/vanilla/mage.yaml) must not alias: the sort/salt key is (parent dir, basename), not
    basename alone, so order-insensitivity and content-sensitivity both hold even in that case."""
    vanilla_dir = tmp_path / "vanilla"; tbc_dir = tmp_path / "tbc"
    vanilla_dir.mkdir(parents=True); tbc_dir.mkdir(parents=True)
    vanilla_mage = vanilla_dir / "mage.yaml"; tbc_mage = tbc_dir / "mage.yaml"
    vanilla_mage.write_text("era: 0\n"); tbc_mage.write_text("era: 1\n")

    s1 = G.generation_stamp([str(vanilla_mage), str(tbc_mage)])
    s2 = G.generation_stamp([str(tbc_mage), str(vanilla_mage)])
    assert s1 == s2   # order-insensitive across the two argument orders

    # Swapping the two files' CONTENTS must change the stamp (no aliasing on basename alone).
    vanilla_mage.write_text("era: 1\n"); tbc_mage.write_text("era: 0\n")
    s3 = G.generation_stamp([str(vanilla_mage), str(tbc_mage)])
    assert s3 != s1


def test_meta_sql_emits_stamp_row(tmp_path):
    a = tmp_path / "a.yaml"; a.write_text("era: 0\n")
    out = G.emit_meta_sql([str(a)])
    stamp = G.generation_stamp([str(a)])
    assert "DELETE FROM `era_talent_meta` WHERE `k`='generation';" in out
    assert f"INSERT INTO `era_talent_meta` (`k`, `v`) VALUES ('generation', '{stamp}');" in out


def test_sentinel_client_row(tmp_path):
    bcd = _bcd()
    row = bcd._sentinel_row("a1b2c3d4")
    assert row[G.COLS.index("ID")] == 932999
    assert row[G.COLS.index("Name_Lang_enUS")] == "EraTalents Gen a1b2c3d4"


def test_lint_rejects_node_passive_at_sentinel_id():
    """custom_passive_id(19624, 8) == SENTINEL_SPELL_ID (932999) — a node whose id/maxRank land
    on that formula must be rejected as a collision with the reserved generation-canary sentinel,
    not silently claim it."""
    d = {
        "era": 0, "class": 8, "eraName": "Vanilla", "className": "MAGE",
        "tabs": [{
            "key": 1, "name": "Arcane", "background": "MageArcane",
            "nodes": [{
                "id": 19624, "slug": "sentinel_collider", "name": "Sentinel Collider",
                "tab": 1, "tier": 0, "col": 0, "icon": "iconX", "maxRank": 8,
                "mechanic": "stat", "aura": 117,
                "vals": [1, 2, 3, 4, 5, 6, 7, 8],
                "tooltip": ["r1", "r2", "r3", "r4", "r5", "r6", "r7", "r8"],
            }],
        }],
    }
    assert G.custom_passive_id(19624, 8) == G.SENTINEL_SPELL_ID
    with pytest.raises(ValueError, match="collision|sentinel"):
        G.lint(d)


def _regen_datasets():
    """The canonical dataset list, PARSED from era-data/datasets.txt (the manifest era-regen.sh
    and era_audit.py read). Parse failures assert loudly — never silently yield a short list."""
    lines = [l for l in pathlib.Path("era-data/datasets.txt").read_text().splitlines()
             if l.strip() and not l.lstrip().startswith("#")]
    assert len(lines) >= 3, f"era-data/datasets.txt: implausibly few datasets: {lines}"
    paths = []
    for l in lines:
        parts = l.split()
        assert len(parts) == 3, f"era-data/datasets.txt: bad line (need 3 fields): {l!r}"
        paths.append("era-data/" + parts[0])
    assert len(paths) == len({p for p in paths}), "era-data/datasets.txt: duplicate dataset"
    return paths


def test_regen_script_reads_manifest():
    """era-regen.sh must read the manifest, not carry its own list — grep for the read."""
    sh = pathlib.Path("tools/era-regen.sh").read_text()
    assert "era-data/datasets.txt" in sh
    assert "declare -A DATA_SQL CUSTOM_SQL" in sh


def test_committed_meta_sql_matches_datasets():
    """The committed meta SQL must always be regenerated after any change that moves the stamp
    (dataset OR generator-input change) — this test makes stale committed meta SQL a CI failure.
    The dataset list self-derives from era-data/datasets.txt (the manifest), so adding a class
    needs no edit here."""
    sql = pathlib.Path(
        "modules/mod-era-talents/data/sql/world/base/2026_08_14_11_era_talent_meta.sql"
    ).read_text()
    m = re.search(r"'generation', '([0-9a-f]{8})'", sql)
    assert m, "no generation stamp found in committed meta SQL"
    assert m.group(1) == G.generation_stamp(_regen_datasets())


def _dataset_key(p):
    """Normalize a dataset path to 'era/basename' so absolute and repo-relative forms compare."""
    return "/".join(pathlib.Path(p).parts[-2:])


def test_era_audit_default_datasets_match_regen():
    """era_audit.py is invoked BARE everywhere in the plans/docs, so a dataset missing from its
    DEFAULT_DATASETS is silently never audited — warlock shipped that way, and its family-5
    sweep coverage never actually executed. Pin the list to the manifest's."""
    spec = _ilu.spec_from_file_location("era_audit", "tools/era_audit.py")
    A = _ilu.module_from_spec(spec); spec.loader.exec_module(A)
    assert [_dataset_key(p) for p in A.DEFAULT_DATASETS] == \
           [_dataset_key(p) for p in _regen_datasets()]


def _load_era_audit_module():
    spec = _ilu.spec_from_file_location("era_audit", "tools/era_audit.py")
    A = _ilu.module_from_spec(spec); spec.loader.exec_module(A)
    return A


def test_era_audit_manifest_validation_rejects_malformed_line(tmp_path, monkeypatch):
    """era_audit.py is the "invoked BARE everywhere" backstop, so its manifest parse must fail
    loudly on a bad line rather than silently truncating the dataset list."""
    A = _load_era_audit_module()
    (tmp_path / "era-data").mkdir()
    (tmp_path / "era-data" / "datasets.txt").write_text("vanilla/mage.yaml only_two_fields.sql\n")
    monkeypatch.setattr(A, "_TOOLS", tmp_path / "tools")
    with pytest.raises(ValueError, match="need 3 fields"):
        A._load_manifest_datasets()


def test_era_audit_manifest_validation_rejects_duplicate(tmp_path, monkeypatch):
    A = _load_era_audit_module()
    (tmp_path / "era-data").mkdir()
    (tmp_path / "era-data" / "datasets.txt").write_text(
        "vanilla/mage.yaml a.sql b.sql\nvanilla/mage.yaml c.sql d.sql\n")
    monkeypatch.setattr(A, "_TOOLS", tmp_path / "tools")
    with pytest.raises(ValueError, match="duplicate dataset"):
        A._load_manifest_datasets()


def test_era_audit_flags_damage_typemask_on_no_damage_proc():
    """Check 1b — the Improved Wing Clip bug: a hit-class proc row with the default
    SpellTypeMask 1 (DAMAGE) whose only affected spell deals no damage can never fire (the hit
    event carries NO_DMG_HEAL). The audit must flag it, and `typeMask: 5` must silence it."""
    spec = _ilu.spec_from_file_location("era_audit", "tools/era_audit.py")
    A = _ilu.module_from_spec(spec); spec.loader.exec_module(A)
    node = {"id": 18336, "name": "Improved Wing Clip", "tab": 3, "tier": 1, "col": 2,
            "maxRank": 1, "mechanic": "proc",
            "proc": {"flags": 16, "affects": ["Wing Clip"], "chance": [4], "trigger": 932978}}
    d = {"tabs": [{"key": 3, "nodes": [node]}],
         "_ref": {"familyName": 9, "spells": {"Wing Clip": {"id": 2974, "flagsA": 64}}}}
    # Synthetic base-DBC row: Wing Clip's real 3.3.5a shape — one APPLY_AURA(33) effect, no damage.
    row = [0] * len(A.G.COLS)
    row[A.IDX["Effect_1"]] = 6; row[A.IDX["EffectAura_1"]] = 33
    findings = []
    A.check_proc_damage_reachability("t", d, {2974: row}, findings)
    assert len(findings) == 1 and "never" in findings[0] and "18336" in findings[0]
    node["proc"]["typeMask"] = 5                      # the fix: DAMAGE | NO_DMG_HEAL
    findings = []
    A.check_proc_damage_reachability("t", d, {2974: row}, findings)
    assert findings == []
    # attrMask 2 (TRIGGERED_CAN_PROC) must also silence it: the event spell is then a triggered
    # family-mask sibling, not the named base row (the Improved Blizzard 18042 shape).
    del node["proc"]["typeMask"]
    node["proc"]["attrMask"] = 2
    findings = []
    A.check_proc_damage_reachability("t", d, {2974: row}, findings)
    assert findings == []
    # A damage-capable affected spell must not be flagged even at the default typeMask.
    del node["proc"]["attrMask"]
    row[A.IDX["Effect_1"]] = 2                        # SPELL_EFFECT_SCHOOL_DAMAGE
    findings = []
    A.check_proc_damage_reachability("t", d, {2974: row}, findings)
    assert findings == []


# ---------------------------------------------------------------------------
# Phase 5 Task 1: HUNTER class token + per-dataset petBuffBase + petBuffIds override
# ---------------------------------------------------------------------------

HUNTER_PET_FIXTURE = textwrap.dedent(r"""
era: 0
class: 3
eraName: Vanilla
className: HUNTER
petBuffBase: 929000
_ref: { familyName: 9, spells: {} }
tabs:
  - key: 1
    name: Beast Mastery
    background: HunterBeastMastery
    nodes:
      - id: 18309
        slug: unleashed_fury
        name: "Unleashed Fury"
        tab: 1
        tier: 2
        col: 2
        icon: "ability_bullrush"
        maxRank: 5
        mechanic: pet
        pet: 0                      # 0 = ALL pets (tamed beasts are arbitrary creature entries)
        petAura: 79                 # SPELL_AURA_MOD_DAMAGE_PERCENT_DONE
        petMisc: 1                  # physical
        vals: [4, 8, 12, 16, 20]
        tooltip: ["4%","8%","12%","16%","20%"]
      - id: 18312
        slug: spirit_bond
        name: "Spirit Bond"
        tab: 1
        tier: 4
        col: 0
        icon: "ability_druid_demoralizingroar"
        maxRank: 2
        mechanic: pet
        pet: 0
        petBuffIds: [932968, 932969]   # hand-authored 119 area-aura helpers, one per rank
        tooltip: ["1%","2%"]
helpers:
  - id: 932968
    name: "Spirit Bond"
    effects:
      - { effect: 119, aura: 20, basePoints: 1, targetA: 1, radius: 12 }
    durationIndex: 21
  - id: 932969
    name: "Spirit Bond"
    effects:
      - { effect: 119, aura: 20, basePoints: 2, targetA: 1, radius: 12 }
    durationIndex: 21
""")


def test_hunter_class_token_registered():
    import tools.gen_era_talents as g
    assert g.CLASS_TOKENS[3] == "HUNTER"


def test_pet_buff_base_key_offsets_petbuff_band(tmp_path):
    y = tmp_path/"h.yaml"; y.write_text(HUNTER_PET_FIXTURE)
    out = tmp_path/"custom.sql"
    r = _run(tmp_path, "--custom-sql", str(y), "-o", str(out))
    assert r.returncode == 0, r.stderr
    sql = out.read_text()
    # Unleashed Fury is the FIRST pet node (ordinal 0) -> its rank-1 pet-buff sits AT petBuffBase.
    assert "(929000," in sql, "petBuffBase must relocate the auto pet-buff band"
    assert "(928000," not in sql, "hunter must NOT emit into the warlock default band"
    # the spell_pet_auras row maps the passive to the relocated buff
    passive_r1 = 920000 + (18309-18000)*8       # 922472
    assert f"({passive_r1}, 0, 0, 929000)" in sql or f"({passive_r1},0,0,929000)" in sql


def test_pet_buff_ids_maps_hand_authored_helpers(tmp_path):
    y = tmp_path/"h.yaml"; y.write_text(HUNTER_PET_FIXTURE)
    out = tmp_path/"custom.sql"
    r = _run(tmp_path, "--custom-sql", str(y), "-o", str(out))
    assert r.returncode == 0, r.stderr
    sql = out.read_text()
    sb_r1 = 920000 + (18312-18000)*8            # 922496
    sb_r2 = sb_r1 + 1
    # spell_pet_auras rows point at the HAND helpers, per rank
    assert f"({sb_r1}, 0, 0, 932968)" in sql or f"({sb_r1},0,0,932968)" in sql
    assert f"({sb_r2}, 0, 0, 932969)" in sql or f"({sb_r2},0,0,932969)" in sql
    # NO auto pet-buff was emitted for the petBuffIds node (its block stays empty):
    assert "(929064," not in sql, "petBuffIds must SUPPRESS the auto _pet_buff_row emission"
    # the helpers themselves exist (helpers: leg) with the area-aura effect
    row = [l for l in sql.splitlines() if l.startswith("(932968,")][0]
    col = {name: v for name, v in zip(_COLS, _parse_values(row))}
    assert col["Effect_1"] == "119"             # SPELL_EFFECT_APPLY_AREA_AURA_PET


def test_pet_buff_ids_lint_rejects_wrong_length(tmp_path):
    bad = HUNTER_PET_FIXTURE.replace("petBuffIds: [932968, 932969]", "petBuffIds: [932968]")
    y = tmp_path/"h.yaml"; y.write_text(bad)
    r = _run(tmp_path, "--custom-sql", str(y), "-o", str(tmp_path/"o.sql"))
    assert r.returncode != 0 and "petBuffIds" in (r.stderr or "")


def test_lint_rejects_pet_buff_base_too_low(tmp_path):
    bad = HUNTER_PET_FIXTURE.replace("petBuffBase: 929000", "petBuffBase: 927000")
    y = tmp_path/"h.yaml"; y.write_text(bad)
    r = _run(tmp_path, "--custom-sql", str(y), "-o", str(tmp_path/"o.sql"))
    assert r.returncode != 0 and "petBuffBase" in (r.stderr or "")


def test_lint_rejects_pet_buff_base_spill_into_hand_helper_region(tmp_path):
    # 2 pet nodes * PET_BUFF_BLOCK(64) = 128 -> worst-case end 932800+128=932928 > 932900.
    bad = HUNTER_PET_FIXTURE.replace("petBuffBase: 929000", "petBuffBase: 932800")
    y = tmp_path/"h.yaml"; y.write_text(bad)
    r = _run(tmp_path, "--custom-sql", str(y), "-o", str(tmp_path/"o.sql"))
    assert r.returncode != 0 and "petBuffBase" in (r.stderr or "")


def test_lint_rejects_pet_buff_ids_combined_with_pet_aura(tmp_path):
    bad = HUNTER_PET_FIXTURE.replace(
        "petBuffIds: [932968, 932969]   # hand-authored 119 area-aura helpers, one per rank",
        "petAura: 79\n        petBuffIds: [932968, 932969]   # hand-authored 119 area-aura helpers, one per rank")
    y = tmp_path/"h.yaml"; y.write_text(bad)
    r = _run(tmp_path, "--custom-sql", str(y), "-o", str(tmp_path/"o.sql"))
    assert r.returncode != 0 and "petBuffIds" in (r.stderr or "")


def test_lint_rejects_pet_buff_ids_not_in_helpers(tmp_path):
    bad = HUNTER_PET_FIXTURE.replace("petBuffIds: [932968, 932969]", "petBuffIds: [932968, 932970]")
    y = tmp_path/"h.yaml"; y.write_text(bad)
    r = _run(tmp_path, "--custom-sql", str(y), "-o", str(tmp_path/"o.sql"))
    assert r.returncode != 0 and "helper" in (r.stderr or "")


def test_spellmod_op_proc_chance(tmp_path):
    """op: procChance = SPELLMOD_CHANCE_OF_SUCCESS (18) — added for Hunter Improved Aspect of the
    Hawk (node 18301), whose first effect raises Aspect of the Hawk 13165's aura-42 ProcChance
    (the stock 19552-19556 encoding: aura 107 flat, EffectMiscValue 18)."""
    d = _yaml.safe_load(SPELLMOD_FIXTURE)
    n = d["tabs"][0]["nodes"][0]
    n["op"] = "procChance"; n["kind"] = "flat"
    y = tmp_path / "m.yaml"; y.write_text(_yaml.safe_dump(d))
    out = tmp_path / "custom.sql"
    assert _run(tmp_path, "--custom-sql", str(y), "-o", str(out)).returncode == 0
    row = [l for l in out.read_text().splitlines() if l.startswith("(920272,")][0]
    col = {name: v for name, v in zip(_COLS, _parse_values(row))}
    assert col["EffectMiscValue_1"] == "18" and col["EffectAura_1"] == "107"


def test_spellmod_op_bonus_multiplier(tmp_path):
    """op: bonusMultiplier = SPELLMOD_BONUS_MULTIPLIER (24) — added for the TBC Druid Empowered
    Touch (node 20155, flat +10/+20 on Healing Touch) and Empowered Rejuvenation (node 20160, pct
    +4..+20 on the HoTs), whose wago 2.5.4 rows both carry EffectMiscValue 24. The core applies it
    where the spell-power coefficient is resolved (Unit.cpp:8924 / :9689 / :9808)."""
    d = _yaml.safe_load(SPELLMOD_FIXTURE)
    n = d["tabs"][0]["nodes"][0]
    n["op"] = "bonusMultiplier"; n["kind"] = "flat"
    y = tmp_path / "m.yaml"; y.write_text(_yaml.safe_dump(d))
    out = tmp_path / "custom.sql"
    assert _run(tmp_path, "--custom-sql", str(y), "-o", str(out)).returncode == 0
    row = [l for l in out.read_text().splitlines() if l.startswith("(920272,")][0]
    col = {name: v for name, v in zip(_COLS, _parse_values(row))}
    assert col["EffectMiscValue_1"] == "24" and col["EffectAura_1"] == "107"


def test_stance_token_tree(tmp_path):
    """STANCE_TOKENS['tree'] = 2 — FORM_TREE is 0x02 (UnitDefines.h:74) and
    HandleShapeshiftBoosts tests `Stances & (1 << (form - 1))`, so the mask bit is 1<<1 = 2 (the
    same ShapeshiftMask stock Tree of Life Aura 34123 carries). Added for the TBC Druid Tree of
    Life form-passive 946265."""
    import tools.gen_era_talents as g
    assert g.STANCE_TOKENS["tree"] == 2


def test_helper_proc_type_mask_column(tmp_path):
    """`procTypeMask:` -> spell_dbc.ProcTypeMask — added for the TBC Druid Omen of Clarity clone
    946269. TBC 16864 procs on MELEE ONLY (mask 20 = DONE_MELEE_AUTO_ATTACK 0x4 |
    DONE_SPELL_MELEE_DMG_CLASS 0x10); live 16864 is 81924 (also magic spell hits), so a clone that
    could not override the column would proc off spellcasts."""
    d = _yaml.safe_load(PROC_FIXTURE)
    d["helpers"] = [{"id": 932505, "name": "Proc Mask Helper", "aura": 42, "misc": 0,
                     "procTypeMask": 20}]
    y = tmp_path / "m.yaml"; y.write_text(_yaml.safe_dump(d))
    out = tmp_path / "custom.sql"
    assert _run(tmp_path, "--custom-sql", str(y), "-o", str(out)).returncode == 0
    row = [l for l in out.read_text().splitlines() if l.startswith("(932505,")][0]
    col = {name: v for name, v in zip(_COLS, _parse_values(row))}
    assert col["ProcTypeMask"] == "20"


def test_spellmod_op_resist_miss(tmp_path):
    """op: resistMiss = SPELLMOD_RESIST_MISS_CHANCE (16) — added for Hunter Trap Mastery
    (node 18340) / Improved Feign Death (18342), mirroring Classic Trap Mastery 19376 and the
    Classic/WotLK Improved Feign Death shape 19287/37484 (aura 107 flat, EffectMiscValue 16)."""
    d = _yaml.safe_load(SPELLMOD_FIXTURE)
    n = d["tabs"][0]["nodes"][0]
    n["op"] = "resistMiss"; n["kind"] = "flat"
    y = tmp_path / "m.yaml"; y.write_text(_yaml.safe_dump(d))
    out = tmp_path / "custom.sql"
    assert _run(tmp_path, "--custom-sql", str(y), "-o", str(out)).returncode == 0
    row = [l for l in out.read_text().splitlines() if l.startswith("(920272,")][0]
    col = {name: v for name, v in zip(_COLS, _parse_values(row))}
    assert col["EffectMiscValue_1"] == "16" and col["EffectAura_1"] == "107"


def test_helper_mana_cost_pct_column(tmp_path):
    """`manaCostPct:` -> spell_dbc.ManaCostPct — added for the Vanilla Bestial Wrath clone 932967
    (12% of base mana vs stock 19574's 10%; the flat powerCost/ManaCost column can't express a
    percentage-of-base cost)."""
    d = _yaml.safe_load(PROC_FIXTURE)
    d["helpers"] = [{"id": 932990, "name": "Pct Cost Helper", "aura": 75, "misc": 4,
                     "manaCostPct": 12}]
    y = tmp_path / "m.yaml"; y.write_text(_yaml.safe_dump(d))
    out = tmp_path / "custom.sql"
    assert _run(tmp_path, "--custom-sql", str(y), "-o", str(out)).returncode == 0
    row = [l for l in out.read_text().splitlines() if l.startswith("(932990,")][0]
    col = {name: v for name, v in zip(_COLS, _parse_values(row))}
    assert col["ManaCostPct"] == "12"


def test_helper_target_creature_type_column(tmp_path):
    """`targetCreatureType:` -> spell_dbc.TargetCreatureType — the creature-type GATE bitmask
    (64 = Humanoid). A templated clone inherits the stock (usually widened) value, so an era
    record that only works on a narrower victim set has to author the column."""
    d = _yaml.safe_load(PROC_FIXTURE)
    d["helpers"] = [{"id": 932991, "name": "Creature Type Helper", "aura": 7, "misc": 0,
                     "targetCreatureType": 64}]
    y = tmp_path / "m.yaml"; y.write_text(_yaml.safe_dump(d))
    out = tmp_path / "custom.sql"
    assert _run(tmp_path, "--custom-sql", str(y), "-o", str(out)).returncode == 0
    row = [l for l in out.read_text().splitlines() if l.startswith("(932991,")][0]
    col = {name: v for name, v in zip(_COLS, _parse_values(row))}
    assert col["TargetCreatureType"] == "64"


def test_helper_proc_charges_column(tmp_path):
    """`procCharges:` -> spell_dbc.ProcCharges — added for the Vanilla Hemorrhage clone 932985
    (the debuff carries 30 charges vs stock 16511's 10; the flat scalar can't otherwise be
    expressed on a cloned/authored castable helper)."""
    d = _yaml.safe_load(PROC_FIXTURE)
    d["helpers"] = [{"id": 932985, "name": "Proc Charges Helper", "aura": 14, "misc": 1,
                     "procCharges": 30}]
    y = tmp_path / "m.yaml"; y.write_text(_yaml.safe_dump(d))
    out = tmp_path / "custom.sql"
    assert _run(tmp_path, "--custom-sql", str(y), "-o", str(out)).returncode == 0
    row = [l for l in out.read_text().splitlines() if l.startswith("(932985,")][0]
    col = {name: v for name, v in zip(_COLS, _parse_values(row))}
    assert col["ProcCharges"] == "30"


def test_helper_proc_chance_column(tmp_path):
    """`procChance:` -> spell_dbc.ProcChance — added for the Vanilla Druid Nature's Grasp clone
    932505 (a 35%-base self-buff vs stock 16689's 100%, so Improved Nature's Grasp's op=procChance
    spellmod has headroom to raise it; the flat scalar can't otherwise be expressed on a
    cloned/authored castable helper)."""
    d = _yaml.safe_load(PROC_FIXTURE)
    d["helpers"] = [{"id": 932505, "name": "Proc Chance Helper", "aura": 42, "misc": 0,
                     "procChance": 35}]
    y = tmp_path / "m.yaml"; y.write_text(_yaml.safe_dump(d))
    out = tmp_path / "custom.sql"
    assert _run(tmp_path, "--custom-sql", str(y), "-o", str(out)).returncode == 0
    row = [l for l in out.read_text().splitlines() if l.startswith("(932505,")][0]
    col = {name: v for name, v in zip(_COLS, _parse_values(row))}
    assert col["ProcChance"] == "35"


def test_helper_caster_aura_spell_column(tmp_path):
    """`casterAuraSpell:` -> spell_dbc.CasterAuraSpell — added for the TBC Warrior Rampage
    reconstruction (947533-947535, node 20343). TBC Rampage gates on `CasterAuraState 11`
    ("scored a critical hit recently"), a state AzerothCore's AuraStateType does not define and
    nothing in src/server/game ever sets, so copying it literally makes the clone permanently
    uncastable. The era route reproduces the 5 s crit window as a real aura (947540, applied by a
    crit-only proc passive) and gates the ability on HOLDING it: `Spell::CheckCast` fails with
    SPELL_FAILED_CASTER_AURASTATE when `m_spellInfo->CasterAuraSpell && !m_caster->HasAura(...)`.
    Byte-neutral for every shipped helper — an unlisted key keeps the template's value / 0."""
    d = _yaml.safe_load(PROC_FIXTURE)
    d["helpers"] = [{"id": 932993, "name": "Caster Aura Gate Helper", "aura": 4, "misc": 0,
                     "casterAuraSpell": 947540}]
    y = tmp_path / "m.yaml"; y.write_text(_yaml.safe_dump(d))
    out = tmp_path / "custom.sql"
    assert _run(tmp_path, "--custom-sql", str(y), "-o", str(out)).returncode == 0
    row = [l for l in out.read_text().splitlines() if l.startswith("(932993,")][0]
    col = {name: v for name, v in zip(_COLS, _parse_values(row))}
    assert col["CasterAuraSpell"] == "947540"


def test_helper_dispel_type_column(tmp_path):
    """`dispelType:` -> spell_dbc.DispelType — added for the TBC Warrior Death Wish clone 947548
    (_ref accepted_gaps id 5). BOTH RAW VALUES: wago 2.5.4 12292 DispelType 0; live 12292
    DispelType 9 (DISPEL_ENRAGE), because WotLK made Death Wish an Enrage effect. Without this key
    a templated clone inherits 9 and stays removable by an Enrage dispel that TBC's own record does
    not allow. Byte-neutral for every shipped helper — an unlisted key keeps the template's
    value / 0.

    NOTE the fixture authors a NON-ZERO value on purpose: a from-scratch helper zero-fills every
    unlisted scalar, so asserting the real shipping value (0) would pass with or without the key."""
    d = _yaml.safe_load(PROC_FIXTURE)
    d["helpers"] = [{"id": 932994, "name": "Dispel Type Helper", "aura": 79, "misc": 1,
                     "dispelType": 9}]
    y = tmp_path / "m.yaml"; y.write_text(_yaml.safe_dump(d))
    out = tmp_path / "custom.sql"
    assert _run(tmp_path, "--custom-sql", str(y), "-o", str(out)).returncode == 0
    row = [l for l in out.read_text().splitlines() if l.startswith("(932994,")][0]
    col = {name: v for name, v in zip(_COLS, _parse_values(row))}
    assert col["DispelType"] == "9"


def test_helper_aura_interrupt_flags_column(tmp_path):
    """`auraInterruptFlags:` -> spell_dbc.AuraInterruptFlags — added for the TBC hunter Scatter Shot
    clone 948382. BOTH RAW VALUES: wago 2.5.4 19503 AuraInterruptFlags 0x2
    (AURA_INTERRUPT_FLAG_DAMAGE — "any damage caused will remove the effect", the TBC tooltip);
    live 19503 AuraInterruptFlags 0x480002 (read from ip-dbc/Spell.dbc, 2026-09-06), WotLK having
    added TAKE_DAMAGE-family bits TBC's own record does not list. Without the key a templated clone
    inherits live's wider set and the disorient breaks on events TBC never breaks it on.
    Byte-neutral for every shipped helper — an unlisted key keeps the template's value / 0.

    NOTE the fixture authors a NON-ZERO value on purpose: a from-scratch helper zero-fills every
    unlisted scalar, so asserting 0 would pass with or without the key."""
    d = _yaml.safe_load(PROC_FIXTURE)
    d["helpers"] = [{"id": 932996, "name": "Interrupt Flags Helper", "aura": 5, "misc": 0,
                     "auraInterruptFlags": 0x2}]
    y = tmp_path / "m.yaml"; y.write_text(_yaml.safe_dump(d))
    out = tmp_path / "custom.sql"
    assert _run(tmp_path, "--custom-sql", str(y), "-o", str(out)).returncode == 0
    row = [l for l in out.read_text().splitlines() if l.startswith("(932996,")][0]
    col = {name: v for name, v in zip(_COLS, _parse_values(row))}
    assert col["AuraInterruptFlags"] == "2"


def test_helper_attributes_ex7_column(tmp_path):
    """`attributesEx7:` -> spell_dbc.AttributesEx7 — added for the TBC Priest Silence clone 948071
    (spec Amendment A.14). `SpellInfoCorrections.cpp:3886` ORs SPELL_ATTR7_CAN_CAUSE_INTERRUPT
    (0x800) onto {47476, 15487 Priest Silence, 5211/6798/8983 Druid Bash} BY ID, and a clone has a
    different id, so it never receives it. BOTH RAW VALUES: wago 2.5.4 15487 AttributesEx7 0; live
    15487 AttributesEx7 0 in the DBC and 0x800 only AFTER the correction runs. Without the key the
    era Silence applies the silence aura but does not interrupt the target's in-progress cast.
    Byte-neutral for every shipped helper — an unlisted key keeps the template's value / 0."""
    d = _yaml.safe_load(PROC_FIXTURE)
    d["helpers"] = [{"id": 932995, "name": "Ex7 Helper", "aura": 27, "misc": 0,
                     "attributesEx7": 0x800}]
    y = tmp_path / "m.yaml"; y.write_text(_yaml.safe_dump(d))
    out = tmp_path / "custom.sql"
    assert _run(tmp_path, "--custom-sql", str(y), "-o", str(out)).returncode == 0
    row = [l for l in out.read_text().splitlines() if l.startswith("(932995,")][0]
    col = {name: v for name, v in zip(_COLS, _parse_values(row))}
    assert col["AttributesEx7"] == "2048"


def test_helper_attributes_ex4_column(tmp_path):
    """`attributesEx4:` -> spell_dbc.AttributesEx4 — added for the TBC hunter Aimed Shot clone chain
    948370-948376. BOTH RAW VALUES: wago 2.5.4 19434 AttributesEx4 0x18000000
    (SPELL_ATTR4_FORCE_DISPLAY_CASTBAR 0x08000000 | SPELL_ATTR4_IGNORE_COMBAT_TIMERS 0x10000000);
    live 19434 AttributesEx4 0x10000000 (read from ip-dbc/Spell.dbc, 2026-09-05). WotLK made Aimed
    Shot instant and dropped the cast-bar bit; the era clone restores TBC's 2.5 s cast and needs the
    bit back or the cast has no bar. The bit is CLIENT-only (no src/server reader), and
    tools/build_client_dbc.py builds the client row through helper_overrides, so this key is how it
    reaches the patch DBC. Byte-neutral for every shipped helper — an unlisted key keeps the
    template's value / 0."""
    d = _yaml.safe_load(PROC_FIXTURE)
    d["helpers"] = [{"id": 932997, "name": "Ex4 Helper", "aura": 4, "misc": 0,
                     "attributesEx4": 0x18000000}]
    y = tmp_path / "m.yaml"; y.write_text(_yaml.safe_dump(d))
    out = tmp_path / "custom.sql"
    assert _run(tmp_path, "--custom-sql", str(y), "-o", str(out)).returncode == 0
    row = [l for l in out.read_text().splitlines() if l.startswith("(932997,")][0]
    col = {name: v for name, v in zip(_COLS, _parse_values(row))}
    assert col["AttributesEx4"] == "402653184"


def test_helper_defense_type_column(tmp_path):
    """`defenseType:` -> spell_dbc.DefenseType (the DmgClass column) — added for the TBC Priest
    Blackout stun 948079, a FROM-SCRATCH reconstruction (wago 15269 is absent from 3.3.5a). wago's
    own record carries DefenseType 1 (MAGIC); a from-scratch helper zero-fills every unlisted
    scalar, and `Unit::SpellHitResult` returns SPELL_MISS_NONE unconditionally for DmgClass 0 —
    i.e. an UNRESISTABLE stun. Byte-neutral for every shipped helper."""
    d = _yaml.safe_load(PROC_FIXTURE)
    d["helpers"] = [{"id": 932996, "name": "Defense Type Helper", "aura": 12, "misc": 0,
                     "defenseType": 1}]
    y = tmp_path / "m.yaml"; y.write_text(_yaml.safe_dump(d))
    out = tmp_path / "custom.sql"
    assert _run(tmp_path, "--custom-sql", str(y), "-o", str(out)).returncode == 0
    row = [l for l in out.read_text().splitlines() if l.startswith("(932996,")][0]
    col = {name: v for name, v in zip(_COLS, _parse_values(row))}
    assert col["DefenseType"] == "1"


def test_helper_start_recovery_columns(tmp_path):
    """`startRecoveryCategory:`/`startRecoveryTime:` -> spell_dbc.StartRecoveryCategory /
    StartRecoveryTime — the GLOBAL COOLDOWN pair, added for the TBC Priest Silence clone 948071.
    BOTH RAW VALUES: wago 2.5.4 15487 is 133/1500 (ON the GCD); live 15487 is 0/0 (WotLK took
    Silence off the GCD). `Spell::TriggerGlobalCooldown` (Spell.cpp:8922) returns early on
    StartRecoveryTime 0, so a template clone of live's 0/0 starts no GCD at all; the category is what
    the haste-scaling branch at :8948 requires. Byte-neutral for shipped helpers."""
    d = _yaml.safe_load(PROC_FIXTURE)
    d["helpers"] = [{"id": 932997, "name": "GCD Helper", "aura": 27, "misc": 0,
                     "startRecoveryCategory": 133, "startRecoveryTime": 1500}]
    y = tmp_path / "m.yaml"; y.write_text(_yaml.safe_dump(d))
    out = tmp_path / "custom.sql"
    assert _run(tmp_path, "--custom-sql", str(y), "-o", str(out)).returncode == 0
    row = [l for l in out.read_text().splitlines() if l.startswith("(932997,")][0]
    col = {name: v for name, v in zip(_COLS, _parse_values(row))}
    assert col["StartRecoveryCategory"] == "133"
    assert col["StartRecoveryTime"] == "1500"


def test_helper_category_cooldown_columns(tmp_path):
    """`category:`/`categoryCooldown:` -> spell_dbc.Category/CategoryRecoveryTime — added for the
    Vanilla Aimed Shot clones (932952-957), which must set both to 0 to neutralize stock Aimed
    Shot's Category-85 10s cooldown so their 6s RecoveryTime is the real cooldown."""
    d = _yaml.safe_load(PROC_FIXTURE)
    d["helpers"] = [{"id": 932991, "name": "Category Helper", "aura": 3, "misc": 0,
                     "category": 0, "categoryCooldown": 0, "cooldown": 6000}]
    y = tmp_path / "m.yaml"; y.write_text(_yaml.safe_dump(d))
    out = tmp_path / "custom.sql"
    assert _run(tmp_path, "--custom-sql", str(y), "-o", str(out)).returncode == 0
    row = [l for l in out.read_text().splitlines() if l.startswith("(932991,")][0]
    col = {name: v for name, v in zip(_COLS, _parse_values(row))}
    assert col["Category"] == "0" and col["CategoryRecoveryTime"] == "0" and col["RecoveryTime"] == "6000"


def test_helper_mask_b_and_c_columns(tmp_path):
    """`maskB:`/`maskC:` -> spell_dbc.SpellClassMask_2/_3 — the helper's OWN family-flag words B/C
    (added for the Paladin Seal of the Crusader reconstruction, node 18633: every family-10 word-A
    bit was already used by a stock paladin spell, so SotC/JotC carry a free word-C bit instead).
    `mask` (word A, the legacy key) still flows to SpellClassMask_1 alongside them."""
    d = _yaml.safe_load(PROC_FIXTURE)
    d["helpers"] = [{"id": 932992, "name": "Mask BC Helper", "aura": 4, "misc": 0,
                     "family": 10, "mask": 4, "maskB": 256, "maskC": 512}]
    y = tmp_path / "m.yaml"; y.write_text(_yaml.safe_dump(d))
    out = tmp_path / "custom.sql"
    assert _run(tmp_path, "--custom-sql", str(y), "-o", str(out)).returncode == 0
    row = [l for l in out.read_text().splitlines() if l.startswith("(932992,")][0]
    col = {name: v for name, v in zip(_COLS, _parse_values(row))}
    assert col["SpellClassSet"] == "10"
    assert col["SpellClassMask_1"] == "4"
    assert col["SpellClassMask_2"] == "256"
    assert col["SpellClassMask_3"] == "512"


def test_templated_helper_equip_class_and_subclass_passthrough(tmp_path):
    """`equipClass:`/`equipSubclass:` on a TEMPLATED helper (a `template:` clone-and-override, the
    `helpers:`-block path a module C++ script casts by fixed id) -> the spell_dbc
    EquippedItemClass/EquippedItemSubclass override columns — added for the Shaman Two-Handed Axes
    and Maces proficiency clone 932406 (Task 6), which clones stock 197 "Two-Handed Axes"
    (EquippedItemClass=2, EquippedItemSubclass=2 = axe2 only, confirmed via
    tools/dump_spell_effects.py against azerothcore-wotlk/ip-dbc/Spell.dbc) and widens the inherited
    subclass to axe2|mace2 = 34 so SPELL_EFFECT_PROFICIENCY (SpellEffects.cpp:2319) grants both. Task
    6 added the two `_HELPER_SCALARS` entries but shipped with NO test — TDD-confirmed: this test
    fails (AssertionError, EquippedItemSubclass stays the template's inherited "2") with the
    `equipClass`/`equipSubclass` tuples commented out of `_HELPER_SCALARS`, and passes with them
    present. Asserting on EquippedItemSubclass specifically (not just EquippedItemClass, which
    happens to match the template's own inherited value) is what proves the OVERRIDE fired rather
    than merely inheriting the template row untouched."""
    d = _yaml.safe_load(PROC_FIXTURE)
    d["helpers"] = [{"id": 932406, "name": "Two-Handed Axes and Maces", "template": 197,
                     "family": 0, "equipClass": 2, "equipSubclass": 34,
                     "effects": [{"effect": 60, "aura": 0, "basePoints": 0, "dieSides": 0, "targetA": 1}]}]
    y = tmp_path / "m.yaml"; y.write_text(_yaml.safe_dump(d))
    out = tmp_path / "custom.sql"
    r = _run(tmp_path, "--custom-sql", str(y), "-o", str(out))
    assert r.returncode == 0, r.stderr
    row = [l for l in out.read_text().splitlines() if l.startswith("(932406,")][0]
    col = {name: v for name, v in zip(_COLS, _parse_values(row))}
    assert col["EquippedItemClass"] == "2"
    assert col["EquippedItemSubclass"] == "34"


# ---------------------------------------------------------------------------
# Phase 6 Task 1: ROGUE class token + weapon-equip gating on proc/multi rows
# ---------------------------------------------------------------------------

ROGUE_EQUIP_FIXTURE = textwrap.dedent(r"""
era: 0
class: 4
eraName: Vanilla
className: ROGUE
_ref: { familyName: 8, spells: { sinister_strike: { flagsA: 4 } } }
tabs:
  - key: 1
    name: Combat
    background: RogueCombat
    nodes:
      - id: 18420
        slug: sword_specialization
        name: "Sword Specialization"
        tab: 1
        tier: 5
        col: 1
        icon: "inv_sword_27"
        maxRank: 5
        mechanic: proc
        equipClass: 2
        equipSubclass: 132
        proc: { flags: 4, chance: [1, 2, 3, 4, 5], trigger: 932989 }
        tooltip: ["1%","2%","3%","4%","5%"]
      - id: 18421
        slug: dagger_specialization
        name: "Dagger Specialization"
        tab: 1
        tier: 4
        col: 1
        icon: "inv_weapon_shortblade_05"
        maxRank: 5
        mechanic: stat
        aura: 52
        equipClass: 2
        equipSubclass: 2048
        vals: [1, 2, 3, 4, 5]
        tooltip: ["1%","2%","3%","4%","5%"]
helpers:
  - id: 932989
    name: "Sword Specialization"
    effects:
      - { effect: 3 }
""")


def test_rogue_class_token_registered():
    import tools.gen_era_talents as g
    assert g.CLASS_TOKENS[4] == "ROGUE"


def test_proc_row_honors_equip_gating(tmp_path):
    """Sword Spec's extra-attack PROC passive must carry the weapon equip-gate so core's
    CheckAttackFitToAuraRequirement restricts it to sword-wielding attacks. Uses --custom-sql (not
    --sql) because the weapon-gated spell_dbc row only appears in the custom-passive SQL — --sql
    only emits era_talent/era_talent_rank rows (see test_stat_equip_requirement_fields for the same
    pattern with mechanic: stat)."""
    y = tmp_path/"r.yaml"; y.write_text(ROGUE_EQUIP_FIXTURE)
    out = tmp_path/"custom.sql"
    r = _run(tmp_path, "--custom-sql", str(y), "-o", str(out))
    assert r.returncode == 0, r.stderr
    sql = out.read_text()
    proc_pid = 920000 + (18420-18000)*8       # 923360 (Sword Spec rank 1 proc passive)
    row = [l for l in sql.splitlines() if l.startswith(f"({proc_pid},")][0]
    col = {name: v for name, v in zip(_COLS, _parse_values(row))}
    assert col["EquippedItemClass"] == "2", "proc passive must carry the weapon class gate"
    assert col["EquippedItemSubclass"] == "132", "proc passive must carry the weapon subclass mask"


def test_stat_row_equip_gating_still_works(tmp_path):
    """Regression guard: mechanic: stat equip-gating (the pre-existing Wand Spec mechanism) must
    keep working unchanged once the gate is factored into a shared helper."""
    y = tmp_path/"r.yaml"; y.write_text(ROGUE_EQUIP_FIXTURE)
    out = tmp_path/"custom.sql"
    r = _run(tmp_path, "--custom-sql", str(y), "-o", str(out))
    assert r.returncode == 0, r.stderr
    sql = out.read_text()
    stat_pid = 920000 + (18421-18000)*8       # 923368 (Dagger Spec rank 1)
    row = [l for l in sql.splitlines() if l.startswith(f"({stat_pid},")][0]
    col = {name: v for name, v in zip(_COLS, _parse_values(row))}
    assert col["EquippedItemClass"] == "2" and col["EquippedItemSubclass"] == "2048"


def test_warrior_class_token_registered():
    import tools.gen_era_talents as g
    assert g.CLASS_TOKENS[1] == "WARRIOR"


def test_equip_gate_helper_exists_for_warrior_weapon_specs():
    # The proc/multi equip-gate (added in the Rogue pass) is a hard dependency of Warrior's
    # weapon specs; assert it is present so this task never silently ships without it.
    import tools.gen_era_talents as g
    assert hasattr(g, "_apply_equip_gate"), "weapon-equip gate helper missing (Rogue Task 1)"


def test_paladin_class_token_registered():
    import tools.gen_era_talents as g
    assert g.CLASS_TOKENS[2] == "PALADIN"


def test_equip_gate_and_helper_effects_exist_for_paladin():
    # Two-Handed Weapon Spec needs the proc/stat/multi equip-gate (Rogue Task 1);
    # Sanctity Aura's party area-aura needs the helpers: effects list (warlock pass).
    # Assert both so this task never silently ships without its dependencies.
    import tools.gen_era_talents as g
    assert hasattr(g, "_apply_equip_gate"), "weapon-equip gate helper missing (Rogue Task 1)"
    assert hasattr(g, "_helper_row"), "helpers: emission path missing (warlock pass)"


def test_druid_class_token_registered():
    import tools.gen_era_talents as g
    assert g.CLASS_TOKENS[11] == "DRUID"


def test_reused_paths_present_for_druid():
    # Druid reuses the equip-gate (if any weapon-gated passive appears) and the helpers: clone path
    # (Moonkin/LotP/etc reconstructions). Assert both so this task can't ship without its deps.
    import tools.gen_era_talents as g
    assert hasattr(g, "_apply_equip_gate"), "weapon-equip gate helper missing"
    assert hasattr(g, "_helper_row"), "helpers: emission path missing"


def test_shaman_class_token_registered():
    import tools.gen_era_talents as g
    assert g.CLASS_TOKENS[7] == "SHAMAN"


def test_reused_paths_present_for_shaman():
    # Shaman reuses the equip-gate (weapon-skill/imbue-gated passives), the helpers: clone path
    # (Mana Tide / Lightning Overload / Flurry reconstructions), and stances: is already present
    # (not expected to be used, but assert it so nobody re-adds it).
    import tools.gen_era_talents as g
    assert hasattr(g, "_apply_equip_gate"), "weapon-equip gate helper missing"
    assert hasattr(g, "_helper_row"), "helpers: emission path missing"
    assert hasattr(g, "_apply_stances"), "stances: path missing (should already exist from druid)"


def test_stances_helper_sets_shapeshift_mask_raw_int():
    import tools.gen_era_talents as g
    over = {}
    g._apply_stances(over, {"stances": 16})   # raw mask
    assert over["ShapeshiftMask"] == 16


def test_stances_helper_resolves_form_tokens():
    import tools.gen_era_talents as g
    over = {}
    g._apply_stances(over, {"stances": ["cat", "bear"]})
    assert over["ShapeshiftMask"] == (g.STANCE_TOKENS["cat"] | g.STANCE_TOKENS["bear"])
    # Also pin the literal value — the assertion above pulls from the same dict the code uses, so
    # it can't catch an accidental change to the token VALUES themselves (cat=1, bear=16 -> 17).
    assert over["ShapeshiftMask"] == 17


def test_stances_absent_leaves_column_unset():
    import tools.gen_era_talents as g
    over = {}
    g._apply_stances(over, {})
    assert "ShapeshiftMask" not in over


def test_stances_rejects_bad_value():
    import tools.gen_era_talents as g
    import pytest
    with pytest.raises(ValueError):
        g._apply_stances({}, {"stances": 0})
    with pytest.raises(ValueError):
        g._apply_stances({}, {"stances": ["notaform"]})


def test_stances_rejects_empty_list():
    import tools.gen_era_talents as g
    import pytest
    with pytest.raises(ValueError):
        g._apply_stances({}, {"stances": []})


def test_stances_rejects_bool():
    """isinstance(True, int) is True in Python — without an explicit bool guard, `stances: true`
    would silently resolve to ShapeshiftMask=1 (Cat-only) with no error."""
    import tools.gen_era_talents as g
    import pytest
    with pytest.raises(ValueError):
        g._resolve_stance_mask(True)
    with pytest.raises(ValueError):
        g._resolve_stance_mask(False)


def test_lint_rejects_stances_on_non_honoring_mechanic():
    """A `stances:` on a mechanic whose row builder never calls _apply_stances (spellmod, pet, or
    a mechanic-less grant/display node) would silently no-op at emit time — lint must catch it."""
    import tools.gen_era_talents as g
    import pytest
    d = {
        "era": 0, "class": 11, "eraName": "Vanilla", "className": "DRUID",
        "_ref": {"familyName": 7, "spells": {"Mangle (Bear)": {"flagsA": 1}}},
        "tabs": [{"key": 1, "name": "Feral", "background": "DruidFeralCombat", "nodes": [
            {"id": 18700, "name": "Bad Stances Spellmod", "tab": 1, "tier": 0, "col": 0,
             "maxRank": 1, "mechanic": "spellmod", "affects": ["Mangle (Bear)"],
             "op": "flat", "kind": "flat", "vals": [1], "stances": ["cat"],
             "tooltip": ["1"]},
        ]}],
    }
    with pytest.raises(ValueError, match=r"node 18700 stances: not honored by mechanic 'spellmod'"):
        g.lint(d)


DRUID_STANCES_FIXTURE = textwrap.dedent(r"""
era: 0
class: 11
eraName: Vanilla
className: DRUID
_ref: { familyName: 7, spells: {} }
tabs:
  - key: 1
    name: Feral
    background: DruidFeralCombat
    nodes:
      - id: 18725
        name: "Sharpened Claws Test"
        tab: 1
        tier: 0
        col: 0
        icon: "ability_racial_bearform"
        maxRank: 1
        mechanic: stat
        aura: 118
        stances: ["cat", "bear"]
        vals: [1]
        tooltip: ["1"]
""")


def test_stat_row_honors_stances_end_to_end(tmp_path):
    """End-to-end regression guard mirroring test_stat_row_equip_gating_still_works: runs a real
    dataset through the --custom-sql emit path (the same path a future edit could silently break
    by dropping the _apply_stances call from _stat_row) and asserts the emitted row's
    ShapeshiftMask column. 17 = STANCE_TOKENS['cat'] (1) | STANCE_TOKENS['bear'] (16)."""
    import tools.gen_era_talents as g
    y = tmp_path/"d.yaml"; y.write_text(DRUID_STANCES_FIXTURE)
    out = tmp_path/"custom.sql"
    r = _run(tmp_path, "--custom-sql", str(y), "-o", str(out))
    assert r.returncode == 0, r.stderr
    sql = out.read_text()
    pid = g.custom_passive_id(18725, 1)
    row = [l for l in sql.splitlines() if l.startswith(f"({pid},")][0]
    col = {name: v for name, v in zip(_COLS, _parse_values(row))}
    assert col["ShapeshiftMask"] == "17", "stat passive must carry the ShapeshiftMask form gate"


def test_totem_creature_band_constants():
    assert G.TOTEM_CREATURE_BASE == 920000
    assert G.TOTEM_CREATURE_END == 921000
    assert G.TOTEM_CREATURE_END > G.TOTEM_CREATURE_BASE


def test_totem_creature_template_row(tmp_path):
    d = _yaml.safe_load(PROC_FIXTURE)
    d["totems"] = [{"creature": 920010, "name": "Stoneskin Totem", "pulse": 932450}]
    y = tmp_path / "m.yaml"; y.write_text(_yaml.safe_dump(d))
    out = tmp_path / "custom.sql"
    assert _run(tmp_path, "--custom-sql", str(y), "-o", str(out)).returncode == 0
    sql = out.read_text()
    assert "DELETE FROM `creature_template` WHERE `entry`=920010;" in sql
    row = [l for l in sql.splitlines() if l.startswith("(920010,")][0]
    assert "'Stoneskin Totem'" in row
    assert row.endswith(",12340);")
    vals = _parse_values(row)
    assert vals[0] == "920010"
    assert vals[32] == "11"    # type = CREATURE_TYPE_TOTEM


def test_totem_creature_template_column_count(tmp_path):
    # Guard for the ONE positional (no-column-list) INSERT in the generator: _TOTEM_CT_ROW is a
    # verbatim clone of stock 5873's creature_template row, which relies on column ORDER. If the
    # base schema gains/reorders a column this count drifts and the row silently mis-maps — this
    # test fails loudly instead. 55 = creature_template's column count on the pinned WotLK core.
    d = _yaml.safe_load(PROC_FIXTURE)
    d["totems"] = [{"creature": 920015, "name": "Stoneskin Totem", "pulse": 932450}]
    y = tmp_path / "m.yaml"; y.write_text(_yaml.safe_dump(d))
    out = tmp_path / "custom.sql"
    assert _run(tmp_path, "--custom-sql", str(y), "-o", str(out)).returncode == 0
    row = [l for l in out.read_text().splitlines() if l.startswith("(920015,")][0]
    assert len(_parse_values(row)) == 55


def test_totem_creature_name_sql_escaped(tmp_path):
    # A name with an apostrophe must be SQL-escaped ('' ) so the INSERT stays valid.
    d = _yaml.safe_load(PROC_FIXTURE)
    d["totems"] = [{"creature": 920016, "name": "Al'Akir Totem", "pulse": 932450}]
    y = tmp_path / "m.yaml"; y.write_text(_yaml.safe_dump(d))
    out = tmp_path / "custom.sql"
    assert _run(tmp_path, "--custom-sql", str(y), "-o", str(out)).returncode == 0
    row = [l for l in out.read_text().splitlines() if l.startswith("(920016,")][0]
    assert "'Al''Akir Totem'" in row
    vals = _parse_values(row)
    assert vals[0] == "920016"
    assert len(vals) == 55       # escaping must not split the name into two values


def test_unsigned_mask_column_emits_uint32_not_negative(tmp_path):
    # EffectSpellClassMaskA_1 is `int unsigned`; a high-bit mask authored as a signed literal
    # (e.g. Elemental Mastery's -1877999613 = bits {0,1,20,28,31}) must emit as its uint32 form
    # (2416967683), or MySQL strict mode rejects it (ERROR 1264). A SIGNED column (EffectBasePoints_1)
    # must keep its negative (e.g. Stoneskin's -3 reduction).
    d = _yaml.safe_load(PROC_FIXTURE)
    d["helpers"] = [{"id": 932990, "name": "Neg Mask Test",
                     "effects": [{"effect": 6, "aura": 108, "basePoints": -3, "maskA": -1877999613}]}]
    y = tmp_path / "m.yaml"; y.write_text(_yaml.safe_dump(d))
    out = tmp_path / "custom.sql"
    assert _run(tmp_path, "--custom-sql", str(y), "-o", str(out)).returncode == 0
    row = [l for l in out.read_text().splitlines() if l.startswith("(932990,")][0]
    col = {name: v for name, v in zip(_COLS, _parse_values(row))}
    assert col["EffectSpellClassMaskA_1"] == "2416967683"   # -1877999613 & 0xFFFFFFFF
    assert col["EffectBasePoints_1"] == "-3"                 # signed column keeps the negative


def test_totem_creature_model_row(tmp_path):
    d = _yaml.safe_load(PROC_FIXTURE)
    d["totems"] = [{"creature": 920011, "name": "Grace of Air Totem", "pulse": 932451, "model": 4590}]
    y = tmp_path / "m.yaml"; y.write_text(_yaml.safe_dump(d))
    out = tmp_path / "custom.sql"
    assert _run(tmp_path, "--custom-sql", str(y), "-o", str(out)).returncode == 0
    sql = out.read_text()
    assert "DELETE FROM `creature_template_model` WHERE `CreatureID`=920011;" in sql
    assert ("INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, "
            "`DisplayScale`, `Probability`, `VerifiedBuild`) VALUES\n(920011, 0, 4590, 1, 1, 12340);") in sql


def test_totem_creature_model_defaults_displayid(tmp_path):
    d = _yaml.safe_load(PROC_FIXTURE)
    d["totems"] = [{"creature": 920012, "name": "Tranquil Air Totem", "pulse": 932452}]
    y = tmp_path / "m.yaml"; y.write_text(_yaml.safe_dump(d))
    out = tmp_path / "custom.sql"
    assert _run(tmp_path, "--custom-sql", str(y), "-o", str(out)).returncode == 0
    assert "(920012, 0, 4588, 1, 1, 12340);" in out.read_text()


def test_totem_creature_spell_row(tmp_path):
    d = _yaml.safe_load(PROC_FIXTURE)
    d["totems"] = [{"creature": 920013, "name": "Searing Totem", "pulse": 932453}]
    y = tmp_path / "m.yaml"; y.write_text(_yaml.safe_dump(d))
    out = tmp_path / "custom.sql"
    assert _run(tmp_path, "--custom-sql", str(y), "-o", str(out)).returncode == 0
    sql = out.read_text()
    assert "DELETE FROM `creature_template_spell` WHERE `CreatureID`=920013;" in sql
    assert ("INSERT INTO `creature_template_spell` (`CreatureID`, `Index`, `Spell`, "
            "`VerifiedBuild`) VALUES\n(920013, 0, 932453, 12340);") in sql


def test_totem_second_pulse_index1(tmp_path):
    d = _yaml.safe_load(PROC_FIXTURE)
    d["totems"] = [{"creature": 920014, "name": "Magma Totem", "pulse": 932454, "pulse2": 932455}]
    y = tmp_path / "m.yaml"; y.write_text(_yaml.safe_dump(d))
    out = tmp_path / "custom.sql"
    assert _run(tmp_path, "--custom-sql", str(y), "-o", str(out)).returncode == 0
    sql = out.read_text()
    assert "(920014, 0, 932454, 12340)" in sql
    assert "(920014, 1, 932455, 12340)" in sql


def test_totem_script_name_emitted():
    d = {"totems": [{"creature": 920280, "name": "Fire Nova Totem",
                     "pulse": 931358, "script": "npc_era_fire_nova_totem"}]}
    sql = "\n".join(G._totem_rows(d))
    assert "npc_era_fire_nova_totem" in sql


def test_totem_without_script_is_empty_scriptname():
    d = {"totems": [{"creature": 920281, "name": "X", "pulse": 931359}]}
    sql = "\n".join(G._totem_rows(d))
    assert "npc_era_fire_nova_totem" not in sql


def _totem_dataset(totems):
    d = _yaml.safe_load(PROC_FIXTURE)
    d["totems"] = totems
    return d


def test_lint_totem_creature_out_of_band():
    # NOTE: this project's lint(d) raises ValueError on every validation failure (see
    # test_lint_rejects_missing_prereq et al. above and the _lint() helper at line ~134) — there
    # is no SystemExit path in lint() itself. Matching that existing convention here rather than
    # the plan's literal `pytest.raises(SystemExit)`.
    with pytest.raises(ValueError):
        _lint(_totem_dataset([{"creature": 5, "name": "X", "pulse": 932450}]))


def test_lint_totem_missing_pulse():
    with pytest.raises(ValueError):
        _lint(_totem_dataset([{"creature": 920020, "name": "X"}]))


def test_lint_totem_duplicate_creature():
    with pytest.raises(ValueError):
        _lint(_totem_dataset([{"creature": 920021, "name": "A", "pulse": 932450},
                              {"creature": 920021, "name": "B", "pulse": 932451}]))


def test_lint_totem_valid_passes():
    _lint(_totem_dataset([{"creature": 920022, "name": "A", "pulse": 932450}]))


# ---------------------------------------------------------------------------
# era_audit.py: totem summon->creature->pulse linkage check
# ---------------------------------------------------------------------------

def _audit_run(dataset_path):
    return subprocess.run([sys.executable, "tools/era_audit.py", str(dataset_path)],
                          capture_output=True, text=True, cwd=os.getcwd())


def test_audit_flags_totem_creature_without_summon(tmp_path):
    d = _yaml.safe_load(PROC_FIXTURE)
    d["helpers"] = [{"id": 932460, "name": "Orphan Pulse", "aura": 8, "misc": 0}]
    d["totems"] = [{"creature": 920030, "name": "Orphan Totem", "pulse": 932460}]
    y = tmp_path / "m.yaml"; y.write_text(_yaml.safe_dump(d))
    r = _audit_run(y)
    assert r.returncode != 0
    assert "920030" in (r.stdout + r.stderr)


def test_audit_passes_linked_totem(tmp_path):
    d = _yaml.safe_load(PROC_FIXTURE)
    d["helpers"] = [
        {"id": 932461, "name": "Test Pulse", "aura": 8, "misc": 0},
        {"id": 932462, "name": "Test Totem",
         "effects": [{"effect": 28, "misc": 920031, "targetA": 1}]},
    ]
    d["totems"] = [{"creature": 920031, "name": "Test Totem", "pulse": 932461}]
    y = tmp_path / "m.yaml"; y.write_text(_yaml.safe_dump(d))
    r = _audit_run(y)
    assert "920031" not in (r.stdout + r.stderr)   # no totem-linkage finding for the linked totem


def test_audit_flags_broken_pulse_link(tmp_path):
    d = _yaml.safe_load(PROC_FIXTURE)
    d["helpers"] = [{"id": 932463, "name": "Summon", "effects": [{"effect": 28, "misc": 920032, "targetA": 1}]}]
    d["totems"] = [{"creature": 920032, "name": "Broken Totem", "pulse": 999999}]  # 999999 not emitted
    y = tmp_path / "m.yaml"; y.write_text(_yaml.safe_dump(d))
    r = _audit_run(y)
    assert r.returncode != 0
    assert "999999" in (r.stdout + r.stderr)


def test_totem_summon_helper_emits_client_row(tmp_path):
    # tools/build_client_dbc.py::_client_rows already emits a client Spell.dbc row for ANY
    # `helpers:` entry that carries a `client:` block, regardless of what its effects do --
    # a totem SUMMON helper (Effect 28, misc = the totem's creature entry) needs no new
    # client-side code. This is a regression test proving that existing behavior covers it.
    d = _yaml.safe_load(PROC_FIXTURE)
    d["helpers"] = [{
        "id": 932470, "name": "Grace of Air Totem",
        "effects": [{"effect": 28, "misc": 920040, "targetA": 1}],
        "client": {"name": "Grace of Air Totem", "icon": 5000,
                   "description": "Summons a Grace of Air Totem."},
    }]
    d["totems"] = [{"creature": 920040, "name": "Grace of Air Totem", "pulse": 932471}]
    y = tmp_path / "m.yaml"; y.write_text(_yaml.safe_dump(d))
    bcd = _bcd()
    rows = bcd._client_rows([str(y)], {})
    assert 932470 in rows
    vals = rows[932470]
    assert vals[G.COLS.index("Name_Lang_enUS")] == "Grace of Air Totem"
    assert vals[G.COLS.index("SpellIconID")] == 5000


def _minimal_ds(era, node_id, helpers=None):
    """Smallest lint-passing dataset with one stat node at node_id."""
    d = {
        "era": era, "class": 8,
        "eraName": "Vanilla" if era == 0 else "TBC", "className": "MAGE",
        "tabs": [{
            "key": 1, "name": "Stub", "background": "MageFire",
            "nodes": [{
                "id": node_id, "slug": "stub_node", "name": "Stub Node",
                "tab": 1, "tier": 0, "col": 0, "icon": "iconX", "maxRank": 1,
                "mechanic": "stat", "aura": 55, "school": 64, "vals": [1],
                "tooltip": ["t1"],
            }],
        }],
    }
    if helpers:
        d["helpers"] = helpers
    return d


def test_custom_passive_end_raised_tbc_nodes_in_band():
    """TBC node base 20000 -> auto-passives 936000-943999, inside the raised band end 950000."""
    assert G.CUSTOM_PASSIVE_END == 950000
    assert G.custom_passive_id(20000, 1) == 936000
    assert G.custom_passive_id(20999, 8) == 943999


def test_lint_per_era_node_windows():
    """era 0 nodes live in [18000,19625), era 1 in [20000,21000) — cross-window ids are rejected
    (an era-1 node at 18001 would silently claim a VANILLA passive id)."""
    G.lint(_minimal_ds(0, 18001))                      # vanilla in-window: ok
    G.lint(_minimal_ds(1, 20800))                      # tbc in-window: ok
    with pytest.raises(ValueError, match="node window"):
        G.lint(_minimal_ds(1, 18001))                  # tbc node in the vanilla window
    with pytest.raises(ValueError, match="node window"):
        G.lint(_minimal_ds(0, 20000))                  # vanilla node in the tbc window


def test_lint_era1_helper_window():
    """TBC hand-helpers must live in [946000,950000); a vanilla-region helper id on an era-1
    dataset is rejected. Era-0 helpers keep the historical loose whole-band rule."""
    helper = [{"id": 946000, "name": "Tbc Helper", "aura": 55, "misc": 64}]
    G.lint(_minimal_ds(1, 20800, helpers=helper))      # ok
    bad = [{"id": 932990, "name": "Bad Helper", "aura": 55, "misc": 64}]
    with pytest.raises(ValueError, match="helper.*window|window.*helper"):
        G.lint(_minimal_ds(1, 20800, helpers=bad))


def _load_audit():
    spec = _ilu.spec_from_file_location("era_audit", "tools/era_audit.py")
    A = _ilu.module_from_spec(spec); spec.loader.exec_module(A)
    return A


def test_era_audit_cross_dataset_collision(tmp_path):
    """Two datasets claiming the same node id (=> same passive slots) must be flagged; the same
    pair with distinct ids must not be. Lint is per-dataset, so only the audit can see this."""
    import textwrap
    A = _load_audit()
    def _write(name, era, node_id):
        p = tmp_path / name
        p.write_text(textwrap.dedent(f"""\
            era: {era}
            class: 8
            eraName: X
            className: MAGE
            tabs:
            - key: 1
              name: Stub
              background: MageFire
              nodes:
              - id: {node_id}
                slug: stub_{node_id}
                name: Stub {node_id}
                tab: 1
                tier: 0
                col: 0
                icon: iconX
                maxRank: 1
                mechanic: stat
                aura: 55
                school: 64
                vals: [1]
                tooltip: [t1]
            """))
        return str(p)

    a = _write("a.yaml", 0, 18001)
    b = _write("b.yaml", 0, 18001)          # SAME node id, different dataset
    c = _write("c.yaml", 1, 20800)          # distinct
    findings = []
    A.check_cross_dataset_ids([a, b], findings)
    assert findings and any("18001" in f for f in findings)
    findings = []
    A.check_cross_dataset_ids([a, c], findings)
    assert findings == []


def test_era_audit_cross_dataset_helper_collision(tmp_path):
    """Two datasets each declaring a `helpers:` entry with the same id must be flagged; distinct
    helper ids must not be."""
    import textwrap
    A = _load_audit()
    def _write(name, helper_id):
        p = tmp_path / name
        p.write_text(textwrap.dedent(f"""\
            era: 0
            class: 8
            eraName: X
            className: MAGE
            tabs: []
            helpers:
            - id: {helper_id}
              name: Helper {helper_id}
              aura: 55
              misc: 64
            """))
        return str(p)

    a = _write("a.yaml", 932500)
    b = _write("b.yaml", 932500)            # SAME helper id, different dataset
    c = _write("c.yaml", 932501)            # distinct
    findings = []
    A.check_cross_dataset_ids([a, b], findings)
    assert findings and any("932500" in f for f in findings)
    findings = []
    A.check_cross_dataset_ids([a, c], findings)
    assert findings == []


def test_era_audit_cross_dataset_pet_window_overlap(tmp_path):
    """Pet-buff window overlap must be detected using the REAL pet-node shape (`mechanic: pet`
    with the legit-falsy `pet: 0` = "all pets" sentinel) — this is the regression guard for the
    truthiness bug (`n.get("pet")` reads 0 for a `pet: 0` node and silently never registers the
    window, so hunter/warlock-shaped datasets never got checked at all)."""
    import textwrap
    A = _load_audit()
    def _write(name, node_id, pet_buff_base):
        p = tmp_path / name
        p.write_text(textwrap.dedent(f"""\
            era: 0
            class: 8
            eraName: X
            className: HUNTER
            petBuffBase: {pet_buff_base}
            tabs:
            - key: 1
              name: Stub
              background: MageFire
              nodes:
              - id: {node_id}
                slug: endurance_training
                name: Endurance Training
                tab: 1
                tier: 0
                col: 2
                icon: spell_nature_reincarnation
                maxRank: 1
                mechanic: pet
                pet: 0
                petAura: 133
                vals: [3]
                tooltip: [t1]
            """))
        return str(p)

    a = _write("a.yaml", 18300, 930000)      # window [930000, 930064)
    b = _write("b.yaml", 18301, 930040)      # window [930040, 930104) — overlaps a
    c = _write("c.yaml", 18302, 931000)      # window [931000, 931064) — disjoint from a
    findings = []
    A.check_cross_dataset_ids([a, b], findings)
    assert findings and any("pet-buff window" in f for f in findings)
    findings = []
    A.check_cross_dataset_ids([a, c], findings)
    assert findings == []


def test_paladin_and_era1_windows_present():
    import tools.gen_era_talents as g
    # Paladin token exists (class 2); era-1 id windows are Phase-0 infrastructure.
    assert g.CLASS_TOKENS[2] == "PALADIN"
    assert g.ERA_NODE_WINDOW[1] == (20000, 21000)
    assert g.ERA_HELPER_WINDOW[1] == (946000, 950000)
    assert g.ERA_PET_WINDOW[1] == (944000, 946000)
    assert g.CUSTOM_PASSIVE_END == 950000
    # Reused authoring paths a paladin TBC dataset needs (assert so nobody re-adds them).
    assert hasattr(g, "_helper_row"), "helpers: emission path missing"
    assert hasattr(g, "_apply_equip_gate"), "weapon-equip gate helper missing"


def test_druid_and_era1_windows_present():
    import tools.gen_era_talents as g
    # Druid token exists (class 11 — NOTE the inversion: class 11, spell family 7).
    assert g.CLASS_TOKENS[11] == "DRUID"
    # Era-1 id windows are Phase-0 infrastructure.
    assert g.ERA_NODE_WINDOW[1] == (20000, 21000)
    assert g.ERA_HELPER_WINDOW[1] == (946000, 950000)
    assert g.CUSTOM_PASSIVE_END == 950000
    # Reused authoring path the druid dataset needs (assert so nobody re-adds it).
    assert hasattr(g, "_helper_row"), "helpers: emission path missing"
    # The transcribed SpellInfoCorrections family-sweep set lives in era_audit.py, not the
    # generator (era_audit.py is what actually walks SPELLFAMILY_* patterns) — family 7 (druid)
    # must be swept.
    a = _load_era_audit_module()
    assert 7 in a._TRANSCRIBED_SWEEP_FAMILIES, "family 7 (druid) must be swept"


def test_shaman_and_era1_windows_present():
    import tools.gen_era_talents as g
    # Shaman token exists (class 7 — NOTE the inversion: class 7, spell family 11).
    assert g.CLASS_TOKENS[7] == "SHAMAN"
    # Era-1 id windows are Phase-0 infrastructure.
    assert g.ERA_NODE_WINDOW[1] == (20000, 21000)
    assert g.ERA_HELPER_WINDOW[1] == (946000, 950000)
    assert g.CUSTOM_PASSIVE_END == 950000
    # The shaman node slice the importer assigns must fall inside the era-1 node window.
    import tools.import_tbc_talents as imp
    assert imp.ID_BASE[7] == 20200
    lo, hi = g.ERA_NODE_WINDOW[1]
    assert lo <= imp.ID_BASE[7] < imp.ID_BASE[7] + 61 <= hi   # TBC shaman tree = 61 nodes
    # Reused authoring paths this dataset needs (assert so nobody re-adds them).
    assert hasattr(g, "_helper_row"), "helpers: emission path missing"
    assert hasattr(g, "_totem_rows"), "totems: emission path missing (Plan 3b depends on it)"


def test_audit_totem_reuse_across_datasets(tmp_path):
    """A later era REUSES the previous era's totem creature + pulse: its summon points straight at
    the shipped creature entry instead of duplicating the creature/pulse rows (Plan 3b: the TBC
    Strength of Earth / Stoneclaw / Tremor / Earthbind summons reuse 920100-920140). era_audit must
    resolve the linkage across every dataset on the run, not per-file, or every such reuse reports
    as a broken link."""
    base = _yaml.safe_load(PROC_FIXTURE)
    base["helpers"] = [
        {"id": 932480, "name": "Era Pulse", "aura": 8, "misc": 0},
        {"id": 932481, "name": "Era Totem",
         "effects": [{"effect": 28, "misc": 920050, "targetA": 1}]},
    ]
    base["totems"] = [{"creature": 920050, "name": "Era Totem", "pulse": 932480}]
    a = tmp_path / "a.yaml"; a.write_text(_yaml.safe_dump(base))

    # The reusing dataset declares NO totems: block and no pulse -- only a summon pointing at the
    # creature the first dataset owns.
    reuse = _yaml.safe_load(PROC_FIXTURE)
    reuse["era"] = 1
    reuse["helpers"] = [{"id": 932482, "name": "Era Totem II",
                         "effects": [{"effect": 28, "misc": 920050, "targetA": 1}]}]
    b = tmp_path / "b.yaml"; b.write_text(_yaml.safe_dump(reuse))

    both = subprocess.run([sys.executable, "tools/era_audit.py", str(a), str(b)],
                          capture_output=True, text=True, cwd=os.getcwd())
    assert "920050" not in (both.stdout + both.stderr), both.stdout + both.stderr

    # ...but the reusing dataset ALONE still fails, so a genuinely dangling reference is caught.
    alone = _audit_run(b)
    assert alone.returncode != 0
    assert "920050" in (alone.stdout + alone.stderr)


def test_audit_flags_duplicate_totem_creature_across_datasets(tmp_path):
    """Two datasets must never DECLARE the same creature entry -- that emits two creature_template
    rows for one entry from two generated SQL files. Reuse means referencing, not redeclaring."""
    d = _yaml.safe_load(PROC_FIXTURE)
    d["helpers"] = [
        {"id": 932483, "name": "Dup Pulse", "aura": 8, "misc": 0},
        {"id": 932484, "name": "Dup Totem",
         "effects": [{"effect": 28, "misc": 920051, "targetA": 1}]},
    ]
    d["totems"] = [{"creature": 920051, "name": "Dup Totem", "pulse": 932483}]
    a = tmp_path / "a.yaml"; a.write_text(_yaml.safe_dump(d))
    d2 = _yaml.safe_load(_yaml.safe_dump(d))
    d2["era"] = 1
    d2["helpers"][0]["id"] = 932485
    d2["helpers"][1]["id"] = 932486
    d2["totems"][0]["pulse"] = 932485
    b = tmp_path / "b.yaml"; b.write_text(_yaml.safe_dump(d2))
    r = subprocess.run([sys.executable, "tools/era_audit.py", str(a), str(b)],
                       capture_output=True, text=True, cwd=os.getcwd())
    assert r.returncode != 0
    assert "already declared by" in (r.stdout + r.stderr)


def test_audit_single_manifest_dataset_resolves_cross_era_totem_reuse():
    """EVERY manifest dataset must audit clean ALONE, not merely as part of the full sweep.

    `era-data/tbc/shaman.yaml` deliberately points several summons at Vanilla creature entries
    (920100 / 920120-3 / 920130-5 / 920140) rather than redeclaring them -- Amendment B.8.1's reuse
    rule. When the linkage check resolved only against the datasets ON THE RUN, auditing the TBC
    file alone produced twelve "references totem creature ... with no totems: entry" findings
    against a by-design arrangement. A false finding on correct data is worse than no check: it
    sends the next reader chasing a bug that is not there.

    Loops DEFAULT_DATASETS rather than a hardcoded pair so every future era/class is covered the
    day it lands -- the same self-deriving pattern as test_era_audit_default_datasets_match_regen.
    Asserts only the two totem-linkage substrings, NOT the exit code: an unrelated future finding
    in some other check should fail its own test, not this one with a message about totem reuse."""
    import era_audit as A
    assert A.DEFAULT_DATASETS, "manifest produced no datasets"
    for ds in A.DEFAULT_DATASETS:
        r = subprocess.run([sys.executable, "tools/era_audit.py", ds],
                           capture_output=True, text=True, cwd=os.getcwd())
        out = r.stdout + r.stderr
        assert "no totems: entry" not in out, f"{ds} alone: {out}"
        assert "already declared by" not in out, f"{ds} alone: {out}"


def test_audit_duplicate_totem_creature_blames_the_dataset_under_edit():
    """A duplicate creature declaration must name the dataset the author is EDITING as the
    offender, in both run modes.

    Creature declarations are walked `resolution_only + datasets` so the file NOT under edit takes
    the incumbent slot. With the run walked first, a targeted audit made the newly-broken file the
    incumbent and reported the shipped file as already declaring the entry -- pointing the author at
    `era-data/vanilla/shaman.yaml`, which is merged, real-client-verified content on master. The
    defect was still detected; the blame was inverted, which is worse than a bare miss because it
    is actionable in the wrong direction."""
    import era_audit as A
    van = ("era-data/vanilla/shaman.yaml", A.G._load("era-data/vanilla/shaman.yaml"))
    tbc = ("era-data/tbc/shaman.yaml", A.G._load("era-data/tbc/shaman.yaml"))
    # The mistake the check exists to catch: a reusing era REDECLARES an entry it should reference.
    tbc[1].setdefault("totems", []).append(
        {"creature": 920140, "name": "Tremor Totem", "pulse": 931090})
    for label, (ds, ro) in {"targeted": ([tbc], [van]), "bare": ([van, tbc], [])}.items():
        findings = []
        A.check_totem_summons(ds, findings, ro)
        dupes = [f for f in findings if "920140" in f and "already declared by" in f]
        assert len(dupes) == 1, f"{label}: expected one collision finding, got {findings}"
        assert dupes[0].startswith("era-data/tbc/shaman.yaml:"), f"{label}: blamed wrong file: {dupes[0]}"
        assert "already declared by era-data/vanilla/shaman.yaml" in dupes[0], dupes[0]


def test_audit_checks_pulse_fk_of_a_duplicate_declaration_loser():
    """The LOSER of a duplicate-entry collision is absent from the creatures dict, so walking that
    dict for the pulse/pulse2 FK check would leave the broken row's own pulse unvalidated -- the
    row most likely to be wrong is the one that escapes. The FK loop walks all totems instead."""
    import era_audit as A
    van = ("era-data/vanilla/shaman.yaml", A.G._load("era-data/vanilla/shaman.yaml"))
    tbc = ("era-data/tbc/shaman.yaml", A.G._load("era-data/tbc/shaman.yaml"))
    tbc[1].setdefault("totems", []).append(
        {"creature": 920140, "name": "Bogus Totem", "pulse": 946000000})
    findings = []
    A.check_totem_summons([tbc], findings, [van])
    assert any("946000000" in f and "not an emitted custom spell id" in f for f in findings), findings


# ---------------------------------------------------------------------------------------------
# `attributesEx2/3/5` helper scalars + the audit check they exist to satisfy.
#
# Commit aaa4086 added the three keys with NO test, breaking the precedent that every
# _HELPER_SCALARS addition carries one (test_helper_attributes_ex_scalar above is the direct
# sibling; test_proc_ppm_column is the other). The keys exist because
# SpellInfoCorrections.cpp ORs attribute bits onto a stock spell BY ID at load and a clone -- a
# different id -- never receives them, so the AUTHOR has to bake the post-fix value into the
# cloned row. Both halves are pinned here: the emission (right column, overwrite not OR) and
# era_audit.check_spellfix_inheritance, which is what finds the missing ones.
# ---------------------------------------------------------------------------------------------

def _attr_ex_helper(hid, template=20185, **extra):
    """A minimal templated helper — the Judgement-of-Light debuff shape (946044's)."""
    h = {"id": hid, "name": "Judgement of Light", "template": template, "durationIndex": 1,
         "effects": [{"effect": 6, "aura": 4, "basePoints": 25, "dieSides": 0, "targetA": 6}]}
    h.update(extra)
    return h


def test_helper_attributes_ex2_ex3_ex5_scalars(tmp_path):
    """Each of `attributesEx2` / `attributesEx3` / `attributesEx5` must reach its OWN spell_dbc
    column, and must OVERWRITE the template's inherited value rather than OR into it — the author
    computes the post-fix value by hand (live DBC value | the bit the correction sets), so an
    implicit OR in the generator would make an intentional CLEAR (`attributesEx3: 0`, or the
    `&= ~` corrections at SpellInfoCorrections.cpp:4009) impossible to express."""
    d = _yaml.safe_load(TARGET_TRIGGER_FIXTURE)
    d["helpers"] = [
        # 1. control: no keys authored -> the template's own AttributesEx3 is inherited unchanged.
        _attr_ex_helper(946044),
        # 2. the real 946044 authoring: live 20185's 1074004480 (0x40040200) | ATTR3_SUPPRESS_
        #    CASTER_PROCS 0x10000 = 1074070016 (SpellInfoCorrections.cpp:561).
        _attr_ex_helper(946045, attributesEx3=1074070016),
        # 3. all three keys at once, with attributesEx3 CLEARED — only overwrite semantics can
        #    produce 0 from a template whose own value is 1074004480.
        _attr_ex_helper(946046, attributesEx2=268435460, attributesEx3=0, attributesEx5=512),
    ]
    y = tmp_path / "m.yaml"; y.write_text(_yaml.safe_dump(d))
    out = tmp_path / "custom.sql"
    r = _run(tmp_path, "--custom-sql", str(y), "-o", str(out))
    assert r.returncode == 0, r.stderr
    sql = out.read_text()

    def col(hid):
        # A cloned row is NOT one physical line: 20185's Description carries a real newline, and
        # its localized strings carry a bare \x85 that str.splitlines() also treats as a break.
        # Split on "\n" only, then accumulate until the tuple actually parses to full width.
        lines = sql.split("\n")
        i = next(k for k, l in enumerate(lines) if l.startswith(f"({hid},"))
        row = lines[i]
        while True:
            try:
                vals = _parse_values(row)
            except AssertionError:
                vals = []
            if len(vals) == len(_COLS):
                return dict(zip(_COLS, vals))
            i += 1
            assert i < len(lines), f"row for {hid} never closed"
            row += "\n" + lines[i]

    inherited = col(946044)["AttributesEx3"]
    assert inherited == "1074004480", \
        f"template 20185's live AttributesEx3 changed ({inherited}); recompute the post-fix values"
    assert col(946045)["AttributesEx3"] == "1074070016", "authored attributesEx3 must land verbatim"
    c = col(946046)
    assert c["AttributesEx2"] == "268435460", "attributesEx2 -> AttributesEx2"
    assert c["AttributesEx5"] == "512", "attributesEx5 -> AttributesEx5"
    assert c["AttributesEx3"] == "0", \
        "attributesEx3 must OVERWRITE the template's 1074004480, not OR into it"
    # The keys are independent: authoring Ex3 alone must not disturb Ex2/Ex5 (they still inherit).
    assert col(946045)["AttributesEx2"] == col(946044)["AttributesEx2"]
    assert col(946045)["AttributesEx5"] == col(946044)["AttributesEx5"]


def test_helper_prevention_type_scalar(tmp_path):
    """`preventionType` must reach the PreventionType column and OVERWRITE the template's value
    (both eras' totem summons author 1 where live is 0 — totems-tbc accepted_gaps id 14; an
    explicit 0 must also be expressible for a template whose live value is nonzero)."""
    d = _yaml.safe_load(TARGET_TRIGGER_FIXTURE)
    d["helpers"] = [
        _attr_ex_helper(946044),                          # control: inherit template's value
        _attr_ex_helper(946045, preventionType=1),        # authored 1 lands verbatim
        _attr_ex_helper(946046, preventionType=0),        # explicit 0 overwrites, never ORs
    ]
    y = tmp_path / "m.yaml"; y.write_text(_yaml.safe_dump(d))
    out = tmp_path / "custom.sql"
    r = _run(tmp_path, "--custom-sql", str(y), "-o", str(out))
    assert r.returncode == 0, r.stderr
    sql = out.read_text()

    def col(hid):
        lines = sql.split("\n")
        i = next(k for k, l in enumerate(lines) if l.startswith(f"({hid},"))
        row = lines[i]
        while True:
            try:
                vals = _parse_values(row)
            except AssertionError:
                vals = []
            if len(vals) == len(_COLS):
                return dict(zip(_COLS, vals))
            i += 1
            assert i < len(lines), f"row for {hid} never closed"
            row += "\n" + lines[i]

    inherited = col(946044)["PreventionType"]
    assert col(946045)["PreventionType"] == "1", "authored preventionType must land verbatim"
    assert col(946046)["PreventionType"] == "0", "explicit 0 must overwrite the template's value"
    # Unlisted key inherits: the control row must carry the template's own value untouched.
    assert inherited == col(946044)["PreventionType"]


def test_script_binding_band_ids_get_bare_delete(tmp_path):
    """A BAND-id scriptBindings entry must emit ONE bare per-id DELETE (the module owns all of a
    band id's bindings, so a REBIND converges on a live DB — the old (id,script)-qualified delete
    leaked the previous script's row and both AuraScripts attached). Multi-script band ids delete
    once then insert every script. STOCK ids keep the qualified delete — a bare delete would nuke
    the core's own bindings."""
    d = _yaml.safe_load(TARGET_TRIGGER_FIXTURE)
    d["helpers"] = [_attr_ex_helper(946044)]
    d["scriptBindings"] = [
        {"spell": 946044, "script": "era_script_a"},
        {"spell": 946044, "script": "era_script_b"},   # second script, same band id
        {"spell": 16257,  "script": "spell_sha_flurry_proc"},
    ]
    y = tmp_path / "m.yaml"; y.write_text(_yaml.safe_dump(d))
    out = tmp_path / "custom.sql"
    r = _run(tmp_path, "--custom-sql", str(y), "-o", str(out))
    assert r.returncode == 0, r.stderr
    sql = out.read_text()
    assert sql.count("DELETE FROM `spell_script_names` WHERE `spell_id`=946044;") == 1, \
        "band id must get exactly ONE bare per-id delete"
    assert "(946044, 'era_script_a');" in sql and "(946044, 'era_script_b');" in sql
    assert "DELETE FROM `spell_script_names` WHERE `spell_id`=16257 AND `ScriptName`='spell_sha_flurry_proc';" in sql, \
        "stock id must keep the (id,script)-qualified delete"
    assert "WHERE `spell_id`=16257;" not in sql, "stock id must never get a bare delete"


_FAKE_CORRECTIONS = """
void SpellMgr::LoadSpellInfoCorrections()
{
    // Judgement of Light / Judgement of Wisdom
    ApplySpellFix({ 20185, 20186 }, [](SpellInfo* spellInfo)
    {
        // Two brace-matcher traps, both deliberate: a NESTED block below (a naive scan to the
        // first '}' would stop there), and the stray '}' in THIS comment (an unbalanced brace
        // inside a comment closed the body early until _blank_comments_and_strings was added --
        // and the fields after it were then silently dropped, i.e. a false NEGATIVE).
        if (spellInfo->Speed > 0.0f) { spellInfo->Speed = 0.0f; }
        spellInfo->AttributesEx3 |= SPELL_ATTR3_SUPPRESS_CASTER_PROCS;
    });

    // Wrath of Air Totem rank 1 (Aura) — a correction with NO generator key
    ApplySpellFix({ 2895 }, [](SpellInfo* spellInfo)
    {
        spellInfo->Effects[EFFECT_0].TargetA = SpellImplicitTargetInfo(TARGET_UNIT_CASTER);
    });
}
"""


def _spellfix_findings(monkeypatch, tmp_path, helper):
    A = _load_era_audit_module()
    src = tmp_path / "SpellInfoCorrections.cpp"
    src.write_text(_FAKE_CORRECTIONS)
    monkeypatch.setattr(A, "_SPELLFIX_SRC", src)
    findings = []
    A.check_spellfix_inheritance([("t.yaml", {"helpers": [helper]})], findings)
    return findings


def test_spellfix_inheritance_fires_on_a_corrected_template(monkeypatch, tmp_path):
    """The check must FIRE on a helper cloning an id SpellInfoCorrections.cpp fixes by id — the
    whole defect class is silent otherwise (no generator error, no runtime error). Pinned against
    a SYNTHETIC corrections file, not the gitignored fork checkout, because `_spellfix_sets()`
    returns [] when the fork is absent and a fork-dependent test would pass vacuously."""
    f = _spellfix_findings(monkeypatch, tmp_path, _attr_ex_helper(946044))
    assert len(f) == 1, f
    assert "946044" in f[0] and "20185" in f[0] and "attributesEx3" in f[0]
    assert "AttributesEx3" in f[0], "the finding must name the core field it read"


def test_spellfix_inheritance_quiet_when_the_key_is_authored(monkeypatch, tmp_path):
    """Escape hatch 1: author the post-fix value and the finding goes away."""
    assert _spellfix_findings(
        monkeypatch, tmp_path, _attr_ex_helper(946044, attributesEx3=1074070016)) == []
    # ...and the VALUE is not inspected — the check can only see whether the key is present.
    assert _spellfix_findings(monkeypatch, tmp_path, _attr_ex_helper(946044, attributesEx3=0)) == []


def test_spellfix_inheritance_quiet_when_acked(monkeypatch, tmp_path):
    """Escape hatch 2: `spellFixAck:` silences the helper — the only option for a correction with
    no generator key (an `Effects[...]`/target rewrite) or one the clone does not need."""
    assert _spellfix_findings(
        monkeypatch, tmp_path, _attr_ex_helper(946044, spellFixAck="reproduced by X")) == []


def test_spellfix_inheritance_reports_keyless_corrections_separately(monkeypatch, tmp_path):
    """A correction touching only `Effects[...]` has no attribute key, so the check must say so
    and demand an ack rather than name a key the author cannot author."""
    f = _spellfix_findings(monkeypatch, tmp_path, _attr_ex_helper(947301, template=2895))
    assert len(f) == 1, f
    assert "no generator key" in f[0] and "spellFixAck" in f[0] and "Effects" in f[0]
    assert _spellfix_findings(
        monkeypatch, tmp_path, _attr_ex_helper(947301, template=2895, spellFixAck="reproduced")) == []


def test_spellfix_inheritance_ignores_uncorrected_and_untemplated_helpers(monkeypatch, tmp_path):
    """No false positives: a clone of an id the core never fixes, and a from-scratch helper with
    no `template:` at all, must both stay silent."""
    assert _spellfix_findings(monkeypatch, tmp_path, _attr_ex_helper(946044, template=17364)) == []
    h = _attr_ex_helper(946044)
    del h["template"]
    assert _spellfix_findings(monkeypatch, tmp_path, h) == []


def test_spellfix_ack_is_lint_only_and_emits_no_column(monkeypatch, tmp_path):
    """`spellFixAck` must never reach the emitted row: it is prose for the audit, and the
    generator has no unknown-key validation, so this pins the "no column" half of the contract
    (see _HELPER_LINT_ONLY_KEYS in gen_era_talents.py). A clone with and without the ack must
    emit BYTE-IDENTICAL SQL — this is what makes acking 25 shipped, real-client-verified Vanilla
    helpers a no-op on their generated rows."""
    assert "spellFixAck" in G._HELPER_LINT_ONLY_KEYS
    assert "spellFixAck" not in dict(G._HELPER_SCALARS)
    d = _yaml.safe_load(TARGET_TRIGGER_FIXTURE)
    d["helpers"] = [_attr_ex_helper(946044)]
    plain = tmp_path / "plain.yaml"; plain.write_text(_yaml.safe_dump(d))
    d["helpers"] = [_attr_ex_helper(946044, spellFixAck="the core's 20185 fix, acknowledged")]
    acked = tmp_path / "acked.yaml"; acked.write_text(_yaml.safe_dump(d))
    a, b = tmp_path / "a.sql", tmp_path / "b.sql"
    assert _run(tmp_path, "--custom-sql", str(plain), "-o", str(a)).returncode == 0
    assert _run(tmp_path, "--custom-sql", str(acked), "-o", str(b)).returncode == 0
    assert a.read_text() == b.read_text(), "spellFixAck changed the emitted SQL"


def test_warrior_and_era1_windows_present():
    import tools.gen_era_talents as g
    assert g.CLASS_TOKENS[1] == "WARRIOR"
    assert g.ERA_NODE_WINDOW[1] == (20000, 21000)
    assert g.ERA_HELPER_WINDOW[1] == (946000, 950000)
    assert g.CUSTOM_PASSIVE_END == 950000
    assert hasattr(g, "_helper_row"), "helpers: emission path missing"
    a = _load_era_audit_module()
    assert 4 in a._TRANSCRIBED_SWEEP_FAMILIES, "family 4 (warrior) must be swept"


def test_rogue_and_era1_windows_present():
    import tools.gen_era_talents as g
    assert g.CLASS_TOKENS[4] == "ROGUE"
    assert g.ERA_NODE_WINDOW[1] == (20000, 21000)
    assert g.ERA_HELPER_WINDOW[1] == (946000, 950000)
    assert g.CUSTOM_PASSIVE_END == 950000
    a = _load_era_audit_module()
    assert 8 in a._TRANSCRIBED_SWEEP_FAMILIES, "family 8 (rogue) must be swept"


# run via `python -m pytest` from the repo root (imports `tools.gen_era_talents`); plain
# `pytest tools/` lacks the package path
def test_priest_and_era1_windows_present():
    import tools.gen_era_talents as g
    assert g.CLASS_TOKENS[5] == "PRIEST"
    assert g.ERA_NODE_WINDOW[1] == (20000, 21000)
    assert g.ERA_HELPER_WINDOW[1] == (946000, 950000)
    assert g.CUSTOM_PASSIVE_END == 950000
    a = _load_era_audit_module()
    assert 6 in a._TRANSCRIBED_SWEEP_FAMILIES, "family 6 (priest) must be swept"


def test_hunter_and_era1_windows_present():
    import tools.gen_era_talents as g
    assert g.CLASS_TOKENS[3] == "HUNTER"                 # class id 3 (family 9 is the audit's key)
    assert g.ERA_NODE_WINDOW[1] == (20000, 21000)
    assert g.ERA_HELPER_WINDOW[1] == (946000, 950000)
    assert g.ERA_PET_WINDOW[1] == (944000, 946000)
    assert g.CUSTOM_PASSIVE_END == 950000
    a = _load_era_audit_module()
    assert 9 in a._TRANSCRIBED_SWEEP_FAMILIES, "family 9 (hunter) must be swept"


def _era1_pet_fixture(pet_buff_base: int) -> str:
    # HUNTER_PET_FIXTURE re-homed to era 1 (TBC): node ids in the era-1 node window, pet-buff base
    # in the era-1 pet window. Same two pet nodes (auto petAura + hand petBuffIds). Each needle is
    # asserted present before its replacement, so a fixture drift fails loudly instead of silently
    # no-opping one of the substitutions.
    s = HUNTER_PET_FIXTURE
    for old, new in (
        ("era: 0", "era: 1"),
        ("eraName: Vanilla", "eraName: TBC"),
        ("petBuffBase: 929000", f"petBuffBase: {pet_buff_base}"),
        ("id: 18309", "id: 20608"),
        ("id: 18312", "id: 20611"),
        ("id: 932968", "id: 948292"),
        ("id: 932969", "id: 948293"),
        ("petBuffIds: [932968, 932969]", "petBuffIds: [948292, 948293]"),
    ):
        assert old in s, f"fixture drift: {old!r} not found"
        s = s.replace(old, new)
    return s


def test_era1_pet_buff_base_emits_inside_tbc_pet_window(tmp_path):
    y = tmp_path / "h.yaml"; y.write_text(_era1_pet_fixture(944000))
    out = tmp_path / "custom.sql"
    r = _run(tmp_path, "--custom-sql", str(y), "-o", str(out))
    assert r.returncode == 0, r.stderr
    sql = out.read_text()
    # Unleashed Fury (20608) is the FIRST pet node (ordinal 0) -> its rank-1 auto pet-buff sits AT the base.
    assert "(944000," in sql, "era-1 petBuffBase must place the auto pet-buff band at 944000"
    assert "(928000," not in sql and "(929000," not in sql, "era-1 dataset must not emit into an era-0 pet band"
    passive_r1 = 936000 + (20608 - 20000) * 8          # 940864 — era-1 auto-passive formula
    assert f"({passive_r1}, 0, 0, 944000)" in sql or f"({passive_r1},0,0,944000)" in sql


def test_era1_pet_buff_base_below_window_rejected(tmp_path):
    y = tmp_path / "h.yaml"; y.write_text(_era1_pet_fixture(943000))   # below ERA_PET_WINDOW[1]
    r = _run(tmp_path, "--custom-sql", str(y), "-o", str(tmp_path / "x.sql"))
    # gen_era_talents.py:377 -- f"petBuffBase {pet_base} must be >= {plo} (era-{era} pet window)"
    assert r.returncode != 0
    assert "petBuffBase 943000 must be >= 944000 (era-1 pet window)" in (r.stderr or "")


def test_era1_pet_buff_base_spilling_window_rejected(tmp_path):
    y = tmp_path / "h.yaml"; y.write_text(_era1_pet_fixture(945950))   # 2 pet nodes * 64 spills past 946000
    r = _run(tmp_path, "--custom-sql", str(y), "-o", str(tmp_path / "x.sql"))
    # gen_era_talents.py:382-385 -- f"petBuffBase {pet_base} + {n_pet_nodes} pet node(s) * "
    # f"{PET_BUFF_BLOCK} = {worst_case_end} would spill into the hand-helper region (>= {phi}) "
    # f"— pick a lower petBuffBase or reduce pet nodes"
    assert r.returncode != 0
    assert ("petBuffBase 945950 + 2 pet node(s) * 64 = 946078 would spill into the "
            "hand-helper region (>= 946000)") in (r.stderr or "")


def test_node_effect_per_level_column():
    """A `mechanic: multi` node effect's `perLevel` key writes EffectRealPointsPerLevel_N — the
    FLOAT column core CalcValue reads as
    `RealPointsPerLevel * (min(casterLevel, MaxLevel) - max(BaseLevel, SpellLevel))`, i.e. a talent
    whose magnitude grows with the character's level. Motivating case: TBC rogue Serrated Blades
    (node 20455), whose armor-ignore half is aura 123 MOD_TARGET_RESISTANCE at basePoints 0 with
    EffectRealPointsPerLevel -2.67/-5.34/-8.00 per rank. Per-rank LIST form, same convention as
    `trigger:`."""
    d = {"_ref": {"familyName": 8, "spells": {"Rupture": {"id": 1943, "flagsA": 1048576,
                                                          "flagsB": 0, "flagsC": 0}}}}
    node = {"id": 20455, "name": "Serrated Blades", "maxRank": 3, "mechanic": "multi",
            "effects": [
                {"type": "spellmod", "affects": ["Rupture"], "op": "dot", "kind": "pct",
                 "vals": [10, 20, 30]},
                {"type": "stat", "aura": 123, "misc": 1, "vals": [0, 0, 0],
                 "perLevel": [-2.67, -5.34, -8.00]},
            ]}
    for rank, expected in ((1, -2.67), (2, -5.34), (3, -8.00)):
        f = G._effect_fields(d, node["effects"][1], rank, 2)
        assert f["EffectRealPointsPerLevel_2"] == pytest.approx(expected)
        # the base value still follows the (val-1, dieSides 1) node convention => effective 0
        assert f["EffectBasePoints_2"] == -1 and f["EffectDieSides_2"] == 1
    # a scalar float applies to every rank
    scalar = {"type": "stat", "aura": 123, "misc": 1, "vals": [0, 0, 0], "perLevel": -1.5}
    assert G._effect_fields(d, scalar, 2, 1)["EffectRealPointsPerLevel_1"] == pytest.approx(-1.5)


def test_node_effect_without_per_level_key_does_not_emit_column():
    """An effect that omits `perLevel` emits NO EffectRealPointsPerLevel column, so every
    already-shipped node passive row is byte-identical (purely additive key)."""
    d = {"_ref": {"familyName": 3, "spells": {}}}
    for spec in ({"type": "stat", "aura": 117, "misc": 11, "vals": [5, 10]},
                 {"type": "spellmod", "affectsMask": {"a": 32, "b": 0, "c": 0},
                  "affects": [], "op": "damage", "kind": "pct", "vals": [5, 10]}):
        f = G._effect_fields(d, spec, 1, 1)
        assert not any(k.startswith("EffectRealPointsPerLevel") for k in f)


def test_lint_rejects_short_per_level_list():
    """A per-rank `perLevel` list shorter than maxRank would silently scale only the low ranks."""
    d = _yaml.safe_load(MULTI_HETERO_FIXTURE)
    d["tabs"][0]["nodes"][0]["effects"][0]["perLevel"] = [-1.0]      # maxRank is 2
    with pytest.raises(ValueError, match="perLevel list length"):
        G.lint(d)


def test_node_effect_per_level_reaches_the_custom_spell_row(tmp_path):
    """`perLevel:` must survive the whole `--custom-sql` pipeline into the emitted VALUES tuple,
    not just `_effect_fields`.

    The three tests above all stop at `_effect_fields`/`lint`, so they would still pass if the row
    writer dropped the column or emitted it as an int. `EffectRealPointsPerLevel_N` is a FLOAT
    column, and `_row` formats floats with a bare `f"{v}"` while ints go through `int(v)` — an
    int-typed path would silently ship `-2` for Serrated Blades' `-2.67` (a 25% magnitude loss).
    Asserts the literal text of both per-rank rows."""
    d = _yaml.safe_load(MULTI_HETERO_FIXTURE)
    # effect 1 is the `type: stat` aura-123 half — the Serrated Blades (node 20455) shape.
    d["tabs"][0]["nodes"][0]["effects"][0]["perLevel"] = [-2.67, -5.34]
    y = tmp_path / "m.yaml"; y.write_text(_yaml.safe_dump(d, sort_keys=False))
    out = tmp_path / "custom.sql"
    r = _run(tmp_path, "--custom-sql", str(y), "-o", str(out))
    assert r.returncode == 0, r.stderr
    sql = out.read_text()
    base = 920000 + (18001 - 18000) * 8          # 920008 = rank 1's auto-passive id
    for rank, expected in ((1, "-2.67"), (2, "-5.34")):
        pid = base + rank - 1
        row = [l for l in sql.splitlines() if l.startswith(f"({pid},")][0]
        col = {name: v for name, v in zip(_COLS, _parse_values(row))}
        assert col["EffectRealPointsPerLevel_1"] == expected, \
            f"rank {rank} row {pid}: got {col['EffectRealPointsPerLevel_1']!r}"
        # the OTHER effect (the spellmod half) must not pick the column up, and the base value
        # still follows the (val-1, dieSides 1) node convention.
        assert col["EffectRealPointsPerLevel_2"] == "0.0"
        assert col["EffectBasePoints_1"] == str(-5 * rank - 1)
