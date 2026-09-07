"""Tests for tools/import_tbc_talents.py — fixture CSVs, no network."""
import importlib.util, json, textwrap
import pytest, yaml

_spec = importlib.util.spec_from_file_location("import_tbc_talents", "tools/import_tbc_talents.py")
I = importlib.util.module_from_spec(_spec); _spec.loader.exec_module(I)

TALENT_CSV = textwrap.dedent("""\
    ID,Description_lang,TierID,Flags,ColumnIndex,TabID,ClassID,SpecID,SpellID,OverridesSpellID,RequiredSpellID,CategoryMask_0,CategoryMask_1,SpellRank_0,SpellRank_1,SpellRank_2,SpellRank_3,SpellRank_4,SpellRank_5,SpellRank_6,SpellRank_7,SpellRank_8,PrereqTalent_0,PrereqTalent_1,PrereqTalent_2,PrereqRank_0,PrereqRank_1,PrereqRank_2
    23,,2,0,3,41,0,0,0,0,0,0,0,11083,12351,0,0,0,0,0,0,0,0,0,0,0,0,0
    31,,0,0,0,41,0,0,0,0,0,0,0,11069,12338,12339,12340,12341,0,0,0,0,0,0,0,0,0,0
    32,,1,0,1,41,0,0,0,0,0,0,0,11119,11120,12846,12847,12848,0,0,0,0,31,0,0,5,0,0
    99,,0,0,1,61,0,0,0,0,0,0,0,11071,12496,12497,0,0,0,0,0,0,0,0,0,0,0,0
    """)

TAB_CSV = textwrap.dedent("""\
    ID,Name_lang,BackgroundFile,OrderIndex,RaceMask,ClassMask
    41,Fire,MageFire,1,2047,128
    61,Frost,MageFrost,2,2047,128
    81,Arcane,MageArcane,0,2047,128
    409,Tenacity,HunterPet,0,2047,1
    """)


@pytest.fixture
def csvs(tmp_path):
    t = tmp_path / "Talent.csv"; t.write_text(TALENT_CSV)
    tt = tmp_path / "TalentTab.csv"; tt.write_text(TAB_CSV)
    return str(t), str(tt)


def test_tabs_filtered_by_classmask_and_ordered(csvs):
    talent_csv, tab_csv = csvs
    d = I.build(talent_csv, tab_csv, class_id=8, id_base=20800, cache_dir=None)
    # mage mask = 1<<(8-1) = 128 -> tabs 81/41/61, ordered by OrderIndex 0,1,2
    assert [t["name"] for t in d["tabs"]] == ["Arcane", "Fire", "Frost"]
    assert [t["background"] for t in d["tabs"]] == ["MageArcane", "MageFire", "MageFrost"]
    assert [t["key"] for t in d["tabs"]] == [1, 2, 3]
    assert d["era"] == 1 and d["eraName"] == "TBC" and d["class"] == 8


def test_nodes_sorted_ids_ranks_prereqs(csvs):
    talent_csv, tab_csv = csvs
    d = I.build(talent_csv, tab_csv, class_id=8, id_base=20800, cache_dir=None)
    nodes = [n for t in d["tabs"] for n in t["nodes"]]
    by_id = {n["id"]: n for n in nodes}
    # sort key (tab-order, TierID, ColumnIndex): Arcane(tab1) empty; Fire(tab2): 31(t0),32(t1),23(t2); Frost(tab3): 99
    assert [n["id"] for n in nodes] == [20800, 20801, 20802, 20803]
    fire31, fire32, fire23, frost99 = by_id[20800], by_id[20801], by_id[20802], by_id[20803]
    assert fire31["tab"] == 2 and fire31["tier"] == 0 and fire31["col"] == 0
    assert fire31["maxRank"] == 5
    assert fire31["rankSpells"] == [11069, 12338, 12339, 12340, 12341]
    assert fire31["prereqTalentId"] == 0 and fire31["prereqPoints"] == 0
    assert fire32["prereqTalentId"] == fire31["id"]     # talent 32 prereqs talent 31
    assert fire23["tier"] == 2 and fire23["prereqPoints"] == 10   # 5 * tier
    assert frost99["tab"] == 3 and frost99["maxRank"] == 3


def test_offline_name_fallback(csvs):
    talent_csv, tab_csv = csvs
    d = I.build(talent_csv, tab_csv, class_id=8, id_base=20800, cache_dir=None)
    n = [x for t in d["tabs"] for x in t["nodes"]][0]
    assert n["name"] == "spell-11069" and n["icon"] == "" and n["tooltip"] == []


# talent 31 (Fire tier0, name-resolving spell 11069) has 5 ranks: 11069, 12338, 12339, 12340, 12341
_ALL_RANK_IDS = [11069, 12338, 12339, 12340, 12341]


def _write_tooltip(cache_dir, spell_id, name, icon, tooltip_html):
    (cache_dir / f"{spell_id}.json").write_text(json.dumps(
        {"name": name, "icon": icon, "tooltip": tooltip_html}))


def test_cached_tooltip_resolution(csvs, tmp_path):
    """Full per-rank cache -> tooltip[i] aligns 1:1 with rankSpells[i], and cleaning strips tags."""
    talent_csv, tab_csv = csvs
    cache = tmp_path / "tooltips"; cache.mkdir()
    _write_tooltip(cache, 11069, "Improved Fireball", "spell_fire_flamebolt",
                    "<b>x</b>Reduces the casting time of your Fireball spell by 0.1 sec.")
    for rank, sid in enumerate(_ALL_RANK_IDS[1:], start=2):
        _write_tooltip(cache, sid, "Improved Fireball", "spell_fire_flamebolt",
                        f"Rank {rank} tooltip text.")
    d = I.build(talent_csv, tab_csv, class_id=8, id_base=20800, cache_dir=str(cache))
    n = [x for t in d["tabs"] for x in t["nodes"] if x["rankSpells"][0] == 11069][0]
    assert n["name"] == "Improved Fireball"
    assert n["icon"] == "spell_fire_flamebolt"
    assert len(n["tooltip"]) == 5
    # _clean_tooltip strips HTML tags but keeps their inner text (matches import_daribon_talents.py's
    # convention), so the fixture's "<b>x</b>" prefix leaves a leading "x " before the sentence.
    assert n["tooltip"][0] == "x Reduces the casting time of your Fireball spell by 0.1 sec."
    assert n["tooltip"][4] == "Rank 5 tooltip text."


def test_partial_cache_tooltip_emits_empty(csvs, tmp_path):
    """Only SOME ranks cached -> tooltip must be [] (never misaligned per-rank text)."""
    talent_csv, tab_csv = csvs
    cache = tmp_path / "tooltips"; cache.mkdir()
    _write_tooltip(cache, 11069, "Improved Fireball", "spell_fire_flamebolt",
                    "Reduces the casting time of your Fireball spell by 0.1 sec.")
    d = I.build(talent_csv, tab_csv, class_id=8, id_base=20800, cache_dir=str(cache))
    n = [x for t in d["tabs"] for x in t["nodes"] if x["rankSpells"][0] == 11069][0]
    assert n["name"] == "Improved Fireball"   # name/icon still resolve from rank-0 alone
    assert n["tooltip"] == []


def test_clean_tooltip_extracts_effect_div_only(csvs, tmp_path):
    """A real Wowhead tooltip wraps the header (name, 'Talent', cost/cast/cooldown lines,
    'Requires Paladin') in a first <table> and the effect prose in <div class="q">, with any
    '(Proc chance: N%)' suffix OUTSIDE the q-div. Only the q-div text may reach the YAML —
    header junk baked into TBC paladin tooltips is the bug this guards against (2026-08-30)."""
    talent_csv, tab_csv = csvs
    cache = tmp_path / "tooltips"; cache.mkdir()
    wowhead_html = (
        '<table><tr><td><a class="whtt-name" href="/tbc/spell=11069/improved-fireball">'
        '<b class="whtt-name">Improved Fireball</b></a><div class="q0">Talent</div>'
        '3% of base mana<table width="100%"><tr><td>Instant</td><th>2 min cooldown</th></tr></table>'
        '<div class="wowhead-tooltip-requirements">Requires Mage</div></td></tr></table>'
        '<table><tr><td><div class="q">Reduces the casting time of your <b>Fireball</b> spell '
        'by 0.1 sec.</div> (Proc chance: 20%)</td></tr></table>')
    _write_tooltip(cache, 11069, "Improved Fireball", "spell_fire_flamebolt", wowhead_html)
    for rank, sid in enumerate(_ALL_RANK_IDS[1:], start=2):
        _write_tooltip(cache, sid, "Improved Fireball", "spell_fire_flamebolt",
                        f'<table>junk</table><table><tr><td><div class="q">Rank {rank} effect.</div></td></tr></table>')
    d = I.build(talent_csv, tab_csv, class_id=8, id_base=20800, cache_dir=str(cache))
    n = [x for t in d["tabs"] for x in t["nodes"] if x["rankSpells"][0] == 11069][0]
    assert n["tooltip"][0] == "Reduces the casting time of your Fireball spell by 0.1 sec."
    assert n["tooltip"][1] == "Rank 2 effect."


def test_unresolved_prereq_warns_and_defaults_to_zero(csvs, capsys):
    """PrereqTalent_0 pointing at a talent id outside this class's filtered tabs (here: 9999,
    which doesn't exist in the fixture at all) -> prereqTalentId defaults to 0 and a WARNING
    is printed to stderr, mirroring import_daribon_talents.py's identical branch."""
    talent_csv, tab_csv = csvs
    orphan_row = "500,,0,0,2,41,0,0,0,0,0,0,0,11119,0,0,0,0,0,0,0,0,9999,0,0,0,0,0\n"
    with open(talent_csv, "a") as f:
        f.write(orphan_row)
    d = I.build(talent_csv, tab_csv, class_id=8, id_base=20800, cache_dir=None)
    n = [x for t in d["tabs"] for x in t["nodes"] if x["rankSpells"] == [11119]][0]
    assert n["prereqTalentId"] == 0
    err = capsys.readouterr().err
    assert "WARNING" in err


def test_classic_icon_prefix_is_stripped(csvs, tmp_path):
    """Wowhead's TBC endpoint serves CLASSIC-era texture names for some spells
    ("classic_ability_druid_demoralizingroar"). No such texture exists in a 3.3.5a client, so the
    talent button renders BLANK — invisible headlessly, which is how it reached a real-client pass
    (TBC druid node 20122, fix round 2026-08-31). The importer must strip the `classic_` prefix;
    the remainder is the real 3.3.5a name (era-data/vanilla/druid.yaml ships
    `ability_druid_demoralizingroar` for the same talent)."""
    talent_csv, tab_csv = csvs
    cache = tmp_path / "tooltips"; cache.mkdir()
    _write_tooltip(cache, 11069, "Improved Fireball", "classic_spell_fire_flamebolt", "x")
    for sid in _ALL_RANK_IDS[1:]:
        _write_tooltip(cache, sid, "Improved Fireball", "classic_spell_fire_flamebolt", "x")
    d = I.build(talent_csv, tab_csv, class_id=8, id_base=20800, cache_dir=str(cache))
    n = [x for t in d["tabs"] for x in t["nodes"] if x["rankSpells"][0] == 11069][0]
    assert n["icon"] == "spell_fire_flamebolt"


def test_non_classic_icon_is_untouched(csvs, tmp_path):
    """Only a LEADING `classic_` is stripped — an icon that merely contains the word, or a name
    that legitimately starts with something else, must survive verbatim."""
    talent_csv, tab_csv = csvs
    cache = tmp_path / "tooltips"; cache.mkdir()
    _write_tooltip(cache, 11069, "Improved Fireball", "spell_fire_classic_flamebolt", "x")
    for sid in _ALL_RANK_IDS[1:]:
        _write_tooltip(cache, sid, "Improved Fireball", "spell_fire_classic_flamebolt", "x")
    d = I.build(talent_csv, tab_csv, class_id=8, id_base=20800, cache_dir=str(cache))
    n = [x for t in d["tabs"] for x in t["nodes"] if x["rankSpells"][0] == 11069][0]
    assert n["icon"] == "spell_fire_classic_flamebolt"
