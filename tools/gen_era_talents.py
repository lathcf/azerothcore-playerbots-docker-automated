#!/usr/bin/env python3
"""Emit EraTalents addon Lua and/or server binding SQL from one era dataset YAML.

Single source of truth: era-data/<era>/<class>.yaml. --sql writes DELETE+INSERT for
`era_talent` (one row/node, incl. display-only) and `era_talent_rank` (one row per
(node,rank,grantedSpellId); NONE for display-only nodes). --lua writes the addon tree.
"""
from __future__ import annotations
import functools
import argparse, pathlib, yaml

ERA_NAMES = {0: "Vanilla", 1: "TBC", 2: "WotLK"}
CLASS_TOKENS = {1: "WARRIOR", 2: "PALADIN", 3: "HUNTER", 4: "ROGUE", 5: "PRIEST", 7: "SHAMAN", 8: "MAGE", 9: "WARLOCK", 11: "DRUID"}

# `spell_dbc` column names, in table order. Loaded relative to THIS script (not CWD) so the
# generator works regardless of the caller's working directory. Column NAME = the 2nd token
# of each "<index>\t<Name>" line in the file.
COLS = [ln.split()[1] for ln in
        (pathlib.Path(__file__).parent / "spell_dbc_columns.txt").read_text().splitlines()
        if ln.strip()]

# Per-column binary type (float / string / signed / unsigned), same order as COLS. Drives BOTH the
# client-DBC encode (tools/build_client_dbc.py) and the template DECODE below.
COLTYPES = [ln.split()[1] for ln in
            (pathlib.Path(__file__).parent / "spell_dbc_coltypes.txt").read_text().splitlines()
            if ln.strip()]
assert len(COLTYPES) == len(COLS), (len(COLTYPES), len(COLS))

# Default base client Spell.dbc used to CLONE template rows (era-wow's proven approach: a custom
# spell inherits a REAL record's ~219 non-authored fields — SpellVisual, cast/interrupt flags,
# attributes — instead of shipping a hollow zero-filled row that has no cast animation and a
# malformed SpellInfo). Resolved relative to this script; overridable via --base-dbc.
DEFAULT_BASE_DBC = pathlib.Path(__file__).resolve().parent.parent / "azerothcore-wotlk" / "ip-dbc" / "Spell.dbc"


@functools.lru_cache(maxsize=4)
def _read_dbc_templates_cached(base_dbc_path_str: str) -> dict:
    """Memoized inner for read_dbc_templates. Decoding the 50 011-row Spell.dbc costs ~1 s, and
    `emit_custom_sql` calls it on EVERY invocation — so a single audit run decoded the same file
    once per dataset (twice for datasets on the run, since check_sweep_collisions emits too). At 12
    datasets that was ~20 s of pure repeat work, growing linearly with each era/class added. Keyed
    on the path STRING because pathlib.Path is hashable but two Paths for one file need not compare
    equal; callers pass a resolved path. maxsize 4 covers the base DBC plus any --base-dbc override
    without pinning many multi-MB dicts."""
    return _read_dbc_templates_uncached(base_dbc_path_str)


def read_dbc_templates(base_dbc_path) -> dict:
    """Cached front door — see _read_dbc_templates_cached. Returns the SHARED dict, so callers must
    treat it as read-only; every current caller only reads (clone-and-override copies the row list
    before mutating)."""
    return _read_dbc_templates_cached(str(pathlib.Path(base_dbc_path).resolve()))


def _read_dbc_templates_uncached(base_dbc_path) -> dict:
    """{spell_id: [234 typed values]} decoded from a base Spell.dbc, for clone-and-override. String
    columns resolve to their actual text (offset 0 -> ""), so a cloned row re-encodes/re-emits
    identically on both the server-SQL and client-DBC paths."""
    import struct
    data = pathlib.Path(base_dbc_path).read_bytes()
    magic, rc, fc, rs, _ss = struct.unpack_from("<4siiii", data, 0)
    if magic != b"WDBC" or fc != len(COLS):
        raise SystemExit(f"unexpected base DBC {base_dbc_path}: magic={magic} fields={fc}")
    base = 20
    strbase = base + rc * rs
    def _str(off):
        if off == 0:
            return ""
        end = data.index(b"\x00", strbase + off)
        return data[strbase + off:end].decode("latin1")
    out: dict = {}
    for i in range(rc):
        rec = data[base + i * rs: base + (i + 1) * rs]
        vals = []
        for j, t in enumerate(COLTYPES):
            if t == "float":
                vals.append(struct.unpack_from("<f", rec, 4 * j)[0])
            elif t == "string":
                vals.append(_str(struct.unpack_from("<I", rec, 4 * j)[0]))
            elif t == "signed":
                vals.append(struct.unpack_from("<i", rec, 4 * j)[0])
            else:
                vals.append(struct.unpack_from("<I", rec, 4 * j)[0])
        out[vals[0]] = vals
    return out

# SpellModOp values (SpellDefines.h). castTime/cooldown/range/cost are proven against era-wow's
# real Improved Fireball row; the rest are transcribed verbatim from the fork's SpellModOp enum
# (src/server/game/Spells/SpellDefines.h) — confirm any new op there before authoring a node.
OP = {"damage": 0, "duration": 1, "threat": 2, "effect1": 3, "charges": 4, "range": 5, "radius": 6,
      "crit": 7, "allEffects": 8, "notLoseCastingTime": 9, "castTime": 10, "cooldown": 11,
      "effect2": 12, "cost": 14, "critDamage": 15, "dot": 22, "effect3": 23,
      "bonusMultiplier": 24,   # SPELLMOD_BONUS_MULTIPLIER — scales the spell-power/bonus-healing
                               # COEFFICIENT, not the base value. Applied in Unit.cpp:8924 (damage
                               # done), :9689 (healing done) and :9808 (healing taken) right where
                               # the coefficient is resolved. Used by the TBC druid Empowered Touch
                               # (20155, flat +10/+20) and Empowered Rejuvenation (20160, pct
                               # +4..+20) nodes, whose wago 2.5.4 rows carry EffectMiscValue 24.
      "valueMultiplier": 27,   # SPELLMOD_VALUE_MULTIPLIER — the ONLY hook on Mana Shield's ratio
      "procChance": 18,        # SPELLMOD_CHANCE_OF_SUCCESS — verified against stock Hunter
                               # Improved Aspect of the Hawk (19552-19556) Effect_1: aura 107 flat,
                               # EffectMiscValue_1=18, masked to Aspect of the Hawk; the core applies
                               # it to an aura-42 proc's chance in Aura::CalcProcChance
                               # (SpellAuras.cpp:2299, via GetSpellModOwner) — used for the Vanilla
                               # Hunter IAotH node (18301)
      "resistDispel": 28,      # SPELLMOD_RESIST_DISPEL_CHANCE — verified against stock Priest
                               # Silent Resolve (14523/14784/14785) Effect_2: aura 107 flat,
                               # EffectMiscValue_2=28, used for Discipline's Silent Resolve node
      "resistMiss": 16}        # SPELLMOD_RESIST_MISS_CHANCE — the "reduce enemies' resist chance"
                               # op. Verified against Classic Trap Mastery 19376 (wowhead classic:
                               # aura 107 flat, "Modifies Hit Chance (16)", masked to the trap
                               # EFFECT spells) and the FD-resist shape shared by Classic Improved
                               # Feign Death 19287 and WotLK 37484 (aura 107 flat, misc 16, masked
                               # 256 = Feign Death). Consumed by the fork in MagicSpellHitResult
                               # (Unit.cpp:3532, adds to modHitChance — the trap-effect path) and
                               # the melee/ranged spell miss calc (Unit.cpp:15325). Used by the
                               # Vanilla Hunter Trap Mastery (18340) + Improved Feign Death (18342)
AURA = {"flat": 107, "pct": 108}          # ADD_FLAT_MODIFIER / ADD_PCT_MODIFIER
# Per-effect SpellClassMask column prefix: effect 1 -> A, effect 2 -> B, effect 3 -> C
# (each with word suffixes _1/_2/_3). Used to place a spellmod effect's family mask.
MASK_PREFIX = {1: "EffectSpellClassMaskA", 2: "EffectSpellClassMaskB", 3: "EffectSpellClassMaskC"}
# SPELL_AURA_ADD_TARGET_TRIGGER (SpellAuraDefines.h) — "when you land a spell matching this
# effect's class mask, also cast EffectTriggerSpell on that spell's target". The chance is the
# effect's own amount (Spell.cpp, DoTriggersOnSpellHit).
AURA_ADD_TARGET_TRIGGER = 109
PASSIVE_ATTR = 262608                     # era-wow proven passive-spellmod Attributes template
FLOAT_COLS = {c for c in COLS if "PerLevel" in c or "Multiple" in c or "Amplitude" in c
              or "BonusMultiplier" in c}

CUSTOM_PASSIVE_BASE = 920000
CUSTOM_PASSIVE_END = 950000                # exclusive upper bound of the reserved id band
# Per-era id regions (TBC Phase 0 spec, 2026-08-27). Node windows bound which formula slots an
# era's datasets may claim; the era-1 pet/helper windows keep TBC hand ids inside the reserved
# TBC slices (era 0 keeps its historical loose whole-band helper rule — those ids shipped).
# TBC: nodes 20000-20999 -> passives 936000-943999; pets [944000,946000); helpers [946000,950000).
ERA_NODE_WINDOW   = {0: (18000, 19625), 1: (20000, 21000)}
ERA_PET_WINDOW    = {1: (944000, 946000)}   # era 0: PET_BUFF_BASE..HAND_HELPER_BASE (below)
ERA_HELPER_WINDOW = {1: (946000, 950000)}   # era 0: whole band (grandfathered)
# Parallel sub-band for `pet:` pet-buff spells (the aura the pet casts on itself), offset into a
# distinct band so pet-buffs never collide with 920000 talent passives or the 932999 sentinel. Each
# pet node gets a PET_BUFF_BLOCK-wide reserved block keyed by its pet-node ordinal (see
# _pet_node_ordinals / pet_buff_id). 64 comfortably fits the realistic max (Master Demonologist:
# 4 demons × up to 8 ranks = 32); the reserved band above 928000 holds ~78 such blocks, far more
# than the ~50 Warlock nodes could ever be.
#
# This is a DEFAULT, not a global allocator: `_pet_node_ordinals` restarts at 0 per dataset, so a
# SECOND pet-using class (e.g. Hunter) that left this default would derive the SAME ids as the
# Warlock's own pet nodes. A dataset overrides it with the top-level `petBuffBase:` key (lint
# validates it's >= PET_BUFF_BASE and that the dataset's worst-case demand stays under the
# 932900 hand-helper region) — pick a fresh multiple-of-some-headroom base per pet class.
PET_BUFF_BASE = 928000
PET_BUFF_BLOCK = 64
# Exclusive upper bound a dataset's pet-buff band (default or `petBuffBase:`-relocated) must stay
# under: everything from here to CUSTOM_PASSIVE_END is the hand-authored `helpers:` region (e.g.
# the Warlock Soul Link/Siphon Life carriers), and an auto pet-buff id spilling into it would
# silently collide with a hand-picked helper id.
HAND_HELPER_BASE = 932900
DEFAULT_PROC_CHANCE = 100                  # spell_proc.Chance when a proc block omits `chance`

# Reserved creature_template.entry band for era totem NPCs. NOTE: this is the CREATURE-entry id
# space, INDEPENDENT of the [920000,933000) spell band that shares these numbers by coincidence.
# Verified 2026-08-23: zero base creature_template rows in [920000,921000).
TOTEM_CREATURE_BASE = 920000
TOTEM_CREATURE_END = 921000                # exclusive

# spellmod ops that are discrete client tooltip lines the addon can recompute from talent rank;
# damage/crit/duration are not standalone numeric lines so they don't contribute to spellMods.
DISCRETE_OPS = {"castTime", "cooldown", "range", "cost"}

_MECHANIC_REQUIRED = {
    "spellmod": ["affects", "op", "kind", "vals"],
    "stat": ["aura", "vals"],
    "proc": ["proc"],
    "proc-scripted": ["proc", "script", "vals"],
    # `scripted` = proc-scripted minus the spell_proc emission: a per-rank hidden band-gated
    # DUMMY marker passive (EffectMiscValue=marker, EffectBasePoints=vals[rank]-1). The behavior
    # binds separately via a top-level `scriptBindings:` on some OTHER spell (a pet ability, a
    # stock channel), so the node itself carries no proc/script — just the learnable marker.
    "scripted": ["marker", "vals"],
    "multi": ["effects"],
    # A `pet:` node emits a talent passive (DUMMY marker) + a pet-buff spell the pet casts on
    # itself + a spell_pet_auras row mapping passive->pet-buff. The single/demon-agnostic shape
    # carries node-level petAura+vals; the per-demon shape carries `petVariants:` instead (each
    # variant a {pet, petAura, vals, ...} dict) — lint swaps the required list in that case.
    "pet": ["petAura", "vals"],
}


def _nodes(d):
    for tab in d["tabs"]:
        for n in tab.get("nodes", []):
            yield tab, n


def _lua_str(s: str) -> str:
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


def emit_sql(d) -> str:
    era, cls = int(d["era"]), int(d["class"])
    # Clear rank rows via a subquery against era_talent BEFORE era_talent itself
    # is cleared (order matters: era_talent must still hold this era/class's rows
    # when the subquery runs, or the second DELETE matches nothing).
    lines = ["-- generated by tools/gen_era_talents.py — DO NOT EDIT",
             f"DELETE FROM `era_talent_rank` WHERE talentId IN "
             f"(SELECT id FROM `era_talent` WHERE eraId={era} AND classId={cls});",
             f"DELETE FROM `era_talent` WHERE eraId={era} AND classId={cls};"]
    node_vals, rank_vals = [], []
    for tab, n in _nodes(d):
        node_vals.append(
            f"({int(n['id'])},{era},{cls},{int(n['tab'])},{int(n['tier'])},"
            f"{int(n['col'])},{int(n['maxRank'])},{int(n.get('prereqTalentId',0))},"
            f"{int(n.get('prereqPoints',0))})")
        if n.get("display_only"):
            continue
        grants = n.get("grants") or {}
        if grants:
            for rank in sorted(int(k) for k in grants):
                for spell in grants[rank] or []:
                    rank_vals.append(f"({int(n['id'])},{rank},{int(spell)})")
        elif n.get("mechanic"):
            # A mechanic node (spellmod/stat/proc/proc-scripted) with no explicit `grants:`
            # is wired to the CUSTOM passive `emit_custom_sql` creates for it — the engine's
            # rankSpell is read straight from era_talent_rank, so without this row the node
            # would look content-complete (custom_sql emits the spell_dbc row) but TryLearn
            # would reject every rank with "malformed talent data" (rankSpell stays 0).
            for rank in range(1, int(n["maxRank"]) + 1):
                rank_vals.append(f"({int(n['id'])},{rank},{custom_passive_id(n['id'], rank)})")
    if node_vals:
        lines.append("INSERT INTO `era_talent` "
                     "(id,eraId,classId,tab,tierRow,col,maxRank,prereqTalentId,prereqPoints) VALUES")
        lines.append(",\n".join(node_vals) + ";")
    if rank_vals:
        lines.append("INSERT INTO `era_talent_rank` (talentId,`rank`,grantedSpellId) VALUES")
        lines.append(",\n".join(rank_vals) + ";")
    return "\n".join(lines) + "\n"


def emit_lua(d) -> str:
    era, cls = int(d["era"]), int(d["class"])
    era_name = d.get("eraName") or ERA_NAMES.get(era, str(era))
    cls_name = d.get("className") or CLASS_TOKENS.get(cls, str(cls))
    L = ["-- generated by tools/gen_era_talents.py — DO NOT EDIT",
         "EraTalentsData = EraTalentsData or {}",
         f"EraTalentsData[{era}] = EraTalentsData[{era}] or {{}}",
         f"EraTalentsData[{era}][{cls}] = {{",
         f"  eraName = {_lua_str(era_name)}, className = {_lua_str(cls_name)},"]
    tab_parts = []
    for tab in d["tabs"]:
        bg = tab.get("background", "")
        tab_parts.append(f"[{int(tab['key'])}]={{name={_lua_str(tab['name'])}, background={_lua_str(bg)}}}")
    L.append("  tabs = { " + ", ".join(tab_parts) + " },")
    # A granted spell only has a `grant` (SetHyperlink) tooltip in the addon if the CLIENT will
    # actually have a tooltip for it: a stock spell (base Spell.dbc) always does; a custom-band id
    # only does if it ships a `client:` block (build_client_dbc emits a client Spell.dbc row for a
    # helper ONLY when it carries `client:`). A grant to a client-LESS custom passive (a hidden proc
    # carrier — Redoubt/Reckoning/Eye for an Eye grant these) is not in the client DBC at all, so a
    # SetHyperlink to it renders a BLANK tooltip. Those nodes must keep their authored prose instead.
    _client_helper_ids = {int(h["id"]) for h in (d.get("helpers") or []) if h.get("client")}

    def _grant_is_client_visible(sid):
        return sid < 920000 or sid in _client_helper_ids

    L.append("  talents = {")
    for tab, n in _nodes(d):
        # A node is server-wired if it isn't display_only AND either grants an explicit spell
        # OR carries a mechanic (spellmod/stat/proc/proc-scripted) — the latter is auto-bound to
        # its custom passive by emit_sql even with no explicit `grants:` (see emit_sql).
        wired = "true" if (not n.get("display_only") and (n.get("grants") or n.get("mechanic"))) else "false"
        tips = n.get("tooltip") or []
        tip_lua = "{ " + ", ".join(_lua_str(t) for t in tips) + " }"
        # A spell-granting node emits a per-rank `grant` map (rank -> the FIRST spell it grants,
        # the primary castable). The addon's NodeTooltip renders that spell's REAL client tooltip
        # (native cost/range/cast/cooldown header + effect description) instead of the authored
        # prose — so the granted active's own DBC row is the single source of the header lines.
        grants = n.get("grants") or {}
        grant_lua = ""
        grant_items = []
        for rank in sorted(int(k) for k in grants):
            spells = grants[rank] or []
            if spells and _grant_is_client_visible(int(spells[0])):
                grant_items.append(f"[{rank}]={int(spells[0])}")
        # Only emit `grant` when rank 1 resolves (a uniform hidden-passive grant yields no items and
        # falls back to prose); a rank-1 gap with later ranks present would misalign node.grant[1].
        if grant_items and grant_items[0].startswith("[1]="):
            grant_lua = ", grant={ " + ", ".join(grant_items) + " }"
        L.append(
            f"    {{ id={int(n['id'])}, tab={int(n['tab'])}, tier={int(n['tier'])}, "
            f"col={int(n['col'])}, name={_lua_str(n['name'])}, icon={_lua_str(n.get('icon',''))}, "
            f"maxRank={int(n['maxRank'])}, prereqTalent={int(n.get('prereqTalentId',0))}, "
            f"prereqPoints={int(n.get('prereqPoints',0))}, wired={wired}, tooltip={tip_lua}{grant_lua} }},")
    L.append("  },")
    spell_mods = build_spell_mods(d)
    L.append("  spellMods = {")
    for name in sorted(spell_mods):
        parts = []
        for e in spell_mods[name]:
            vals = ", ".join(str(v) for v in e["vals"])
            parts.append(
                f"{{talent={e['talent']}, attr={_lua_str(e['attr'])}, "
                f"kind={_lua_str(e['kind'])}, vals={{ {vals} }} }}")
        L.append(f"    [{_lua_str(name)}] = {{ " + ", ".join(parts) + " },")
    L.append("  },")
    # Era-wide client base-cast overrides (dataset key `clientCastTimeBase`, name -> base ms): a
    # spell whose SERVER cast time is era-restored by a core patch (Corruption 2s, patch 0019) while
    # the client Spell.dbc still says Instant. Tooltip.lua rewrites the cast line from this base so
    # the tooltip matches the real cast time at every talent rank (including rank 0).
    ctb = d.get("clientCastTimeBase") or {}
    if ctb:
        L.append("  castTimeBase = {")
        for name in sorted(ctb):
            L.append(f"    [{_lua_str(name)}] = {int(ctb[name])},")
        L.append("  },")
    L.append("}")
    return "\n".join(L) + "\n"


def build_spell_mods(d) -> dict:
    """Map affected spell NAME -> [{talent, attr, kind, vals[]}] for the addon's client-side
    tooltip rewrite. Only `mechanic: spellmod` nodes whose `op` is a discrete tooltip line
    (DISCRETE_OPS) contribute — damage/crit/duration mods aren't standalone numeric lines."""
    spell_mods: dict = {}
    for _tab, n in _nodes(d):
        mech = n.get("mechanic")
        # A single spellmod node contributes its own op; a multi node contributes each of its
        # spellmod-typed effects. Non-spellmod mechanics (stat/proc/…) never contribute.
        if mech == "spellmod":
            specs = [n]
        elif mech == "multi":
            specs = [e for e in n["effects"] if e["type"] == "spellmod"]
        else:
            continue
        for spec in specs:
            op = spec.get("op")
            if op not in DISCRETE_OPS:
                continue
            entry = {"talent": int(n["id"]), "attr": op, "kind": spec.get("kind", "flat"),
                     "vals": [int(v) for v in spec["vals"]]}
            for name in spec.get("affects", []):
                spell_mods.setdefault(name, []).append(entry)
    return spell_mods


def lint(d):
    ids = {int(n["id"]) for _, n in _nodes(d)}
    era = int(d.get("era", 0))
    nlo, nhi = ERA_NODE_WINDOW.get(era, ERA_NODE_WINDOW[0])
    for _tab, n in _nodes(d):
        nid = int(n["id"])
        if not (nlo <= nid < nhi):
            raise ValueError(f"node {nid}: id outside the era-{era} node window [{nlo},{nhi})")
    seen_cell = {}
    # Pre-seed the sentinel id so BOTH the node-passive collision check (below, in the node loop)
    # and the helper collision check (further down) reject any authored id landing on it —
    # custom_passive_id(19624, 8) == 932999, so a node passive can otherwise claim it silently.
    seen_pid = {SENTINEL_SPELL_ID: ("sentinel", SENTINEL_SPELL_ID)}
    # Same-dataset `scripted` marker registry: two scripted nodes sharing a marker would emit
    # indistinguishable DUMMY carriers, so the C++ side couldn't tell them apart. Cross-dataset
    # collisions can't be caught here (markers are a global namespace) — the framework-doc registry
    # stays the authority; this is only a within-file guardrail.
    seen_marker = {}
    ref = d.get("_ref")
    ref_spells = (ref or {}).get("spells") if ref else None
    pet_ordinals = _pet_node_ordinals(d)
    helper_ids = {int(h["id"]) for h in (d.get("helpers") or [])}
    # `petBuffBase:` relocates a dataset's ENTIRE auto pet-buff band (see pet_buff_id) — needed
    # because `_pet_node_ordinals` restarts at 0 per dataset, so a second pet-class sharing the
    # default PET_BUFF_BASE would collide with the warlock's own auto ids. Validate it once, up
    # front: it must not undercut the reserved band, and the dataset's worst-case demand (every
    # pet node claiming a full PET_BUFF_BLOCK) must not spill into the hand-helper region.
    if era in ERA_PET_WINDOW:
        plo, phi = ERA_PET_WINDOW[era]
    else:
        plo, phi = PET_BUFF_BASE, HAND_HELPER_BASE
    pet_base = int(d.get("petBuffBase", plo))
    if pet_base < plo:
        raise ValueError(f"petBuffBase {pet_base} must be >= {plo} (era-{era} pet window)")
    n_pet_nodes = len(pet_ordinals)
    if n_pet_nodes:
        worst_case_end = pet_base + n_pet_nodes * PET_BUFF_BLOCK
        if worst_case_end > phi:
            raise ValueError(
                f"petBuffBase {pet_base} + {n_pet_nodes} pet node(s) * {PET_BUFF_BLOCK} = "
                f"{worst_case_end} would spill into the hand-helper region (>= {phi}) "
                f"— pick a lower petBuffBase or reduce pet nodes")
    for tab, n in _nodes(d):
        if int(n["tab"]) != int(tab["key"]):
            raise ValueError(
                f"node {n['id']} has tab {n['tab']} but sits under tab key {tab['key']}")
        pre = int(n.get("prereqTalentId", 0))
        if pre and pre not in ids:
            raise ValueError(f"node {n['id']} prereq {pre} is not a node in this file")
        cell = (int(n["tab"]), int(n["tier"]), int(n["col"]))
        if cell in seen_cell:
            raise ValueError(f"grid collision at {cell}: nodes {seen_cell[cell]} and {n['id']}")
        seen_cell[cell] = int(n["id"])
        if not n.get("display_only") and not n.get("mechanic"):
            grants = n.get("grants") or {}
            ranks = sorted(int(k) for k in grants)
            if ranks != list(range(1, int(n["maxRank"]) + 1)):
                raise ValueError(f"node {n['id']} ranks not contiguous 1..maxRank: {ranks}")
        mech = n.get("mechanic")
        if n.get("stances") is not None:
            if mech not in _STANCES_HONORED_MECHANICS:
                raise ValueError(
                    f"node {n['id']} stances: not honored by mechanic {mech!r} "
                    f"(only stat/multi/proc gate by form)")
            try:
                _resolve_stance_mask(n["stances"])
            except ValueError as e:
                raise ValueError(f"node {n['id']} stances: {e}") from e
        if not mech:
            continue
        required = _MECHANIC_REQUIRED.get(mech)
        if required is None:
            raise ValueError(f"node {n['id']} has unknown mechanic {mech!r}")
        # A `pet:` node in the per-demon shape carries petAura/vals INSIDE each petVariants entry,
        # not at the node level — swap the required list so the generic check doesn't false-fail.
        # A `petBuffIds:` node (hand-authored carrier helpers, one per rank) is a third shape.
        if mech == "pet" and n.get("petVariants") is not None:
            required = ["petVariants"]
        elif mech == "pet" and n.get("petBuffIds") is not None:
            required = ["petBuffIds"]
        missing = [k for k in required if n.get(k) is None]
        if missing:
            raise ValueError(
                f"node {n['id']} mechanic {mech!r} missing required key(s): {missing}")
        max_rank = int(n["maxRank"])
        if mech == "spellmod":
            if not isinstance(n["affects"], list):
                raise ValueError(f"node {n['id']} affects must be a list")
            if n["op"] not in OP:
                raise ValueError(f"node {n['id']} op {n['op']!r} is not a known SpellModOp")
            if n["kind"] not in ("flat", "pct"):
                raise ValueError(f"node {n['id']} kind {n['kind']!r} must be 'flat' or 'pct'")
            if not isinstance(n["vals"], list) or len(n["vals"]) != max_rank:
                raise ValueError(
                    f"node {n['id']} vals length must equal maxRank {max_rank}: {n.get('vals')}")
            if ref_spells is not None:
                for name in n["affects"]:
                    if name not in ref_spells:
                        raise ValueError(
                            f"node {n['id']} affects spell {name!r} not in _ref.spells")
            _lint_affects_mask(n["id"], n)
            # Optional `procPassive:` — the spellmod passive also carries a per-rank proc-trigger.
            pp = n.get("procPassive")
            if pp is not None:
                if "trigger" not in pp or "flags" not in pp:
                    raise ValueError(
                        f"node {n['id']} procPassive requires 'trigger' and 'flags'")
                trig = pp["trigger"]
                if isinstance(trig, list) and len(trig) != max_rank:
                    raise ValueError(
                        f"node {n['id']} procPassive.trigger length must equal maxRank "
                        f"{max_rank}: {trig}")
        elif mech == "stat":
            if not isinstance(n["aura"], int):
                raise ValueError(f"node {n['id']} aura must be an int")
            if not isinstance(n["vals"], list) or len(n["vals"]) != max_rank:
                raise ValueError(
                    f"node {n['id']} vals length must equal maxRank {max_rank}: {n.get('vals')}")
        elif mech == "multi":
            effects = n["effects"]
            if not isinstance(effects, list) or not effects:
                raise ValueError(f"node {n['id']} multi.effects must be a non-empty list")
            if len(effects) > 3:
                raise ValueError(f"node {n['id']} multi has >3 effects (DBC has 3 effect slots)")
            for e in effects:
                etype = e.get("type")
                if etype not in ("spellmod", "stat"):
                    raise ValueError(
                        f"node {n['id']} effect type {etype!r} must be 'spellmod' or 'stat'")
                if not isinstance(e.get("vals"), list) or len(e["vals"]) != max_rank:
                    raise ValueError(
                        f"node {n['id']} effect vals length must equal maxRank {max_rank}: "
                        f"{e.get('vals')}")
                # `perLevel:` (EffectRealPointsPerLevel) may be one float for every rank or a
                # per-rank list — a short list would silently scale only the low ranks.
                pl = e.get("perLevel")
                if isinstance(pl, list) and len(pl) != max_rank:
                    raise ValueError(
                        f"node {n['id']} effect perLevel list length must equal "
                        f"maxRank {max_rank}: {pl}")
                if etype == "spellmod":
                    if e.get("op") not in OP:
                        raise ValueError(
                            f"node {n['id']} effect op {e.get('op')!r} is not a known SpellModOp")
                    if e.get("kind") not in ("flat", "pct"):
                        raise ValueError(
                            f"node {n['id']} effect kind {e.get('kind')!r} must be 'flat'/'pct'")
                    if not isinstance(e.get("affects"), list):
                        raise ValueError(f"node {n['id']} spellmod effect needs an affects list")
                    if ref_spells is not None:
                        for name in e["affects"]:
                            if name not in ref_spells:
                                raise ValueError(
                                    f"node {n['id']} affects spell {name!r} not in _ref.spells")
                    _lint_affects_mask(n["id"], e)
                else:  # stat
                    if not isinstance(e.get("aura"), int):
                        raise ValueError(f"node {n['id']} stat effect aura must be an int")
                    trig = e.get("trigger")
                    if isinstance(trig, list) and len(trig) != max_rank:
                        raise ValueError(
                            f"node {n['id']} stat effect trigger list length must equal "
                            f"maxRank {max_rank}: {trig}")
                    masked = e.get("affects") is not None or e.get("affectsMask") is not None
                    if masked:
                        if e.get("affects") is not None:
                            if not isinstance(e["affects"], list):
                                raise ValueError(
                                    f"node {n['id']} stat effect affects must be a list")
                            if ref_spells is not None:
                                for name in e["affects"]:
                                    if name not in ref_spells:
                                        raise ValueError(
                                            f"node {n['id']} affects spell {name!r} "
                                            f"not in _ref.spells")
                        _lint_affects_mask(n["id"], e)
                    # SPELL_AURA_ADD_TARGET_TRIGGER is only meaningful with BOTH a mask and a
                    # trigger. Maskless is the dangerous case: SpellInfo::IsAffected returns true
                    # for every spell when familyName is 0, so the companion spell would be cast
                    # on the target of literally every spell the character lands.
                    if int(e["aura"]) == AURA_ADD_TARGET_TRIGGER:
                        if not masked:
                            raise ValueError(
                                f"node {n['id']} aura 109 (ADD_TARGET_TRIGGER) needs "
                                f"affects:/affectsMask: — a maskless one fires on EVERY spell")
                        if trig is None:
                            raise ValueError(
                                f"node {n['id']} aura 109 (ADD_TARGET_TRIGGER) needs a "
                                f"trigger: spell id, otherwise it is a silent no-op")
        elif mech in ("proc", "proc-scripted"):
            proc = n["proc"]
            if not isinstance(proc, dict) or "flags" not in proc:
                raise ValueError(f"node {n['id']} proc must be a dict with 'flags'")
            chance = proc.get("chance")
            if isinstance(chance, list) and len(chance) != max_rank:
                raise ValueError(
                    f"node {n['id']} proc.chance length must equal maxRank {max_rank}: {chance}")
            if proc.get("affects") is not None:
                if not isinstance(proc["affects"], list):
                    raise ValueError(f"node {n['id']} proc.affects must be a list")
                if ref_spells is not None:
                    for name in proc["affects"]:
                        if name not in ref_spells:
                            raise ValueError(
                                f"node {n['id']} proc affects spell {name!r} not in _ref.spells")
            # An optional verbatim `proc.affectsMask` override (see _spell_proc_row_for).
            _lint_affects_mask(n["id"], proc)
            if mech == "proc":
                if proc.get("trigger") is None:
                    raise ValueError(f"node {n['id']} data proc requires proc.trigger")
            else:  # proc-scripted
                if not isinstance(n.get("script"), str):
                    raise ValueError(f"node {n['id']} proc-scripted requires a 'script' string")
                if not isinstance(n.get("vals"), list) or len(n["vals"]) != max_rank:
                    raise ValueError(
                        f"node {n['id']} proc-scripted vals length must equal maxRank {max_rank}: "
                        f"{n.get('vals')}")
        elif mech == "scripted":
            marker = n.get("marker")
            if not isinstance(marker, int) or isinstance(marker, bool) or not (6 <= marker <= 255):
                raise ValueError(
                    f"node {n['id']} scripted marker must be an int in 6..255: {marker!r}")
            if marker in seen_marker:
                raise ValueError(
                    f"node {n['id']} scripted marker {marker} collides with node "
                    f"{seen_marker[marker]} in the same dataset")
            seen_marker[marker] = int(n["id"])
            if not isinstance(n.get("vals"), list) or len(n["vals"]) != max_rank:
                raise ValueError(
                    f"node {n['id']} scripted vals length must equal maxRank {max_rank}: "
                    f"{n.get('vals')}")
        elif mech == "pet":
            # Three mutually-exclusive shapes: single/demon-agnostic (node-level petAura+vals,
            # optional `pet:` creature entry, 0 = all demons), per-demon (`petVariants:` list, each
            # entry a {pet, petAura, vals} dict), or `petBuffIds:` (hand-authored carrier helpers,
            # one per rank, for shapes the auto pet-buff row can't express — e.g. a 119 area-aura
            # with a nonstandard effect). Mixing any two of the three is ambiguous.
            variants = n.get("petVariants")
            buff_ids = n.get("petBuffIds")
            if buff_ids is not None:
                if variants is not None or n.get("petAura") is not None:
                    raise ValueError(
                        f"node {n['id']} pet: petBuffIds is mutually exclusive with "
                        f"petAura/petVariants")
                if not isinstance(buff_ids, list) or len(buff_ids) != max_rank:
                    raise ValueError(
                        f"node {n['id']} petBuffIds length must equal maxRank {max_rank}: "
                        f"{buff_ids}")
                for bid in buff_ids:
                    if int(bid) not in helper_ids:
                        raise ValueError(
                            f"node {n['id']} petBuffIds entry {bid} is not a helper id in this "
                            f"dataset's helpers: (a listed id not in helpers: is a silent no-op "
                            f"at runtime)")
            elif variants is not None:
                if n.get("petAura") is not None:
                    raise ValueError(
                        f"node {n['id']} pet: cannot combine a node-level petAura with petVariants "
                        f"(put petAura inside each variant)")
                if not isinstance(variants, list) or not variants:
                    raise ValueError(f"node {n['id']} pet.petVariants must be a non-empty list")
                for v in variants:
                    if not isinstance(v, dict) or v.get("petAura") is None:
                        raise ValueError(f"node {n['id']} each petVariants entry needs a petAura")
                    if not isinstance(v.get("vals"), list) or len(v["vals"]) != max_rank:
                        raise ValueError(
                            f"node {n['id']} petVariants vals length must equal maxRank {max_rank}: "
                            f"{v.get('vals')}")
            else:
                if not isinstance(n["vals"], list) or len(n["vals"]) != max_rank:
                    raise ValueError(
                        f"node {n['id']} vals length must equal maxRank {max_rank}: {n.get('vals')}")
        for rank in range(1, max_rank + 1):
            pid = custom_passive_id(n["id"], rank)
            if pid in seen_pid:
                raise ValueError(
                    f"custom passive id {pid} collision: nodes {seen_pid[pid]} and "
                    f"({n['id']}, rank {rank})")
            seen_pid[pid] = (n["id"], rank)
        # A `pet:` node's demand is (variants * maxRank) pet-buff ids; each node owns a
        # PET_BUFF_BLOCK-wide reserved block. HARD-ERROR when the demand exceeds the block — this
        # guard fires on the node alone (no reliance on the collision map or neighbor layout), so a
        # variant id can never spill into the next pet node's block. Then register each emitted
        # pet-buff id (distinct petBuffBase-based band, so it can't cross-collide with 920000
        # passives). A `petBuffIds:` node emits NO auto pet-buff ids at all — its hand-authored
        # helper ids are registered by the `helpers:` loop below, not here.
        if mech == "pet" and n.get("petBuffIds") is None:
            ordinal = pet_ordinals[int(n["id"])]
            n_variants = len(_pet_bindings(n))
            if n_variants * max_rank > PET_BUFF_BLOCK:
                raise ValueError(
                    f"node {n['id']} pet: demands {n_variants}*{max_rank}={n_variants * max_rank} "
                    f"pet-buff ids but the per-node block is {PET_BUFF_BLOCK} — too many variants "
                    f"and/or ranks")
            for rank in range(1, max_rank + 1):
                for vi in range(n_variants):
                    bid = pet_buff_id(pet_base, ordinal, rank, vi, max_rank)
                    if bid in seen_pid:
                        raise ValueError(
                            f"pet buff id {bid} collision: {seen_pid[bid]} and "
                            f"({n['id']}, rank {rank}, variant {vi})")
                    seen_pid[bid] = ("pet-buff", n["id"], rank, vi)
    for h in d.get("helpers", []) or []:
        hid = int(h["id"])
        if not (CUSTOM_PASSIVE_BASE <= hid < CUSTOM_PASSIVE_END):
            raise ValueError(f"helper id {hid} out of reserved era band")
        if era in ERA_HELPER_WINDOW:
            hlo, hhi = ERA_HELPER_WINDOW[era]
            if not (hlo <= hid < hhi):
                raise ValueError(f"helper {hid}: outside the era-{era} helper window [{hlo},{hhi})")
        if hid in seen_pid:
            raise ValueError(f"helper id {hid} collides with a talent passive id")
        if "name" not in h or ("aura" not in h and "effects" not in h):
            raise ValueError(f"helper {hid} needs id/name and either aura or effects")
        if h.get("template") is not None and not h.get("effects"):
            raise ValueError(
                f"helper {hid}: 'template:' requires an authored 'effects:' list (template "
                f"effects would pass through wholesale and a top-level 'aura:' is silently "
                f"ignored)")
        if h.get("effects") and len(h["effects"]) > 3:
            raise ValueError(
                f"helper {hid} has >3 effects (spell_dbc only has Effect_1..3, max 3 allowed)")
        if h.get("proc") is not None:
            proc = h["proc"]
            if not isinstance(proc, dict) or "flags" not in proc:
                raise ValueError(f"helper {hid} proc needs flags (must be a dict with 'flags')")
            _lint_affects_mask(hid, proc)   # optional verbatim mask override, same as a node proc
        if h.get("stances") is not None:
            try:
                _resolve_stance_mask(h["stances"])
            except ValueError as e:
                raise ValueError(f"helper {hid} stances: {e}") from e
        seen_pid[hid] = ("helper", hid)
    seen_creatures = set()
    for t in d.get("totems", []) or []:
        entry = t.get("creature")
        if entry is None or not (TOTEM_CREATURE_BASE <= int(entry) < TOTEM_CREATURE_END):
            raise ValueError(
                f"totem {t.get('name','?')}: creature {entry} outside reserved band "
                f"[{TOTEM_CREATURE_BASE},{TOTEM_CREATURE_END})")
        if int(entry) in seen_creatures:
            raise ValueError(f"totem {t.get('name','?')}: duplicate creature entry {entry}")
        seen_creatures.add(int(entry))
        if not t.get("name"):
            raise ValueError(f"totem creature {entry}: missing name")
        if t.get("pulse") is None:
            raise ValueError(f"totem {t.get('name','?')}: missing pulse spell id")
    return True


def _lint_affects_mask(node_id, spec):
    """Validate an optional `affectsMask:` override on a spellmod node/effect (see
    _resolve_affects_mask). Must be a dict of int a/b/c (any subset; unset words default 0) and at
    least one word must be nonzero (an all-zero mask binds NOTHING — surely a mistake)."""
    am = spec.get("affectsMask")
    if am is None:
        return
    if not isinstance(am, dict):
        raise ValueError(f"node {node_id} affectsMask must be a dict with int a/b/c")
    for k in ("a", "b", "c"):
        if k in am and (not isinstance(am[k], int) or isinstance(am[k], bool)):
            raise ValueError(f"node {node_id} affectsMask.{k} must be an int")
    if not any(int(am.get(k, 0)) for k in ("a", "b", "c")):
        raise ValueError(f"node {node_id} affectsMask is all-zero (binds nothing) — set at least one word")


def custom_passive_id(node_id: int, rank: int) -> int:
    """Deterministic custom spell_dbc id for (node, rank), stable across regenerations."""
    pid = CUSTOM_PASSIVE_BASE + (int(node_id) - 18000) * 8 + (rank - 1)
    if not (CUSTOM_PASSIVE_BASE <= pid < CUSTOM_PASSIVE_END):
        raise ValueError(f"custom passive id {pid} for node {node_id} rank {rank} out of band")
    return pid


def _pet_node_ordinals(d) -> dict:
    """{node_id: ordinal} for the pet nodes in this dataset, counting ONLY `pet:` nodes in the
    deterministic `_nodes` iteration order (tabs in file order, nodes within). The ordinal — not the
    global (node_id-18000) multiplier — keys each pet node's pet-buff block, so a handful of pet
    nodes stay comfortably in-band even with a wide (64) block; `PET_BUFF_BASE+(node_id-18000)*64`
    would bust the band for high Warlock ids (e.g. 18251). Ordinals shift on regen if pet nodes are
    added/removed, which is harmless: pet-buff ids aren't persisted in character data (they're
    server-cast auras, re-derived every login), unlike the talent-passive ids the player learns."""
    ords, i = {}, 0
    for _tab, n in _nodes(d):
        if n.get("mechanic") == "pet":
            ords[int(n["id"])] = i
            i += 1
    return ords


def pet_buff_id(base: int, ordinal: int, rank: int, variant: int, max_rank: int) -> int:
    """Deterministic custom spell_dbc id for a `pet:` node's pet-buff (the aura the pet casts on
    itself), stable across regenerations for a fixed set of pet nodes. `base` is the dataset's
    pet-buff band origin (PET_BUFF_BASE by default, or the dataset's `petBuffBase:` override —
    see the docstring on PET_BUFF_BASE for why a second pet-using class needs its own base). Each
    pet node owns a PET_BUFF_BLOCK-wide (64) reserved block keyed by its pet-node `ordinal`; within
    the block, the per-demon `variant` steps past the node's ranks by `max_rank` and `rank` indexes
    the demon's ranks. variant 0 = the single/demon-agnostic buff. A node's demand is
    `variants*max_rank`; lint HARD-ERRORS when that exceeds PET_BUFF_BLOCK, so a variant id can
    never spill into the next node's block (that guard, not the collision map, is what makes
    overflow impossible)."""
    pid = base + int(ordinal) * PET_BUFF_BLOCK + int(variant) * int(max_rank) + (rank - 1)
    if not (CUSTOM_PASSIVE_BASE <= pid < CUSTOM_PASSIVE_END):
        raise ValueError(
            f"pet buff id {pid} (base {base}, ordinal {ordinal}, rank {rank}, variant {variant}) "
            f"out of band")
    return pid


def _row_values(overrides: dict, template_values: list | None = None) -> list:
    """The 234 raw column values (ints/floats/strs) for a spell_dbc row, in table order. Shared by
    the SQL emitter (_row) and the client-DBC builder (tools/build_client_dbc.py) so a custom spell's
    fields are identical on both paths — a divergence rubber-bands the client on cast.

    With `template_values` (a real spell's cloned 234-value row), seed from it and apply only the
    authored `overrides` on top — the era-wow clone-and-override path. Without it, the legacy
    zero-filled path (every non-overridden column defaults to 0 / 0.0)."""
    if template_values is not None:
        vals = list(template_values)
        for i, c in enumerate(COLS):
            if c in overrides:
                vals[i] = overrides[c]
        return vals
    return [overrides.get(c, 0.0 if c in FLOAT_COLS else 0) for c in COLS]


def _row(overrides: dict, template_values: list | None = None) -> str:
    vals = []
    for v, t in zip(_row_values(overrides, template_values), COLTYPES):
        if isinstance(v, str):
            vals.append("'" + v.replace("'", "''") + "'")
        elif isinstance(v, float):
            vals.append(f"{v}")
        else:
            iv = int(v)
            # UNSIGNED columns (e.g. SpellClassMask_*/EffectSpellClassMask* are `int unsigned`) reject a
            # negative in MySQL strict mode (ERROR 1264). A high-bit family mask authored as a signed
            # literal (e.g. Elemental Mastery's -1877999613 = bits {0,1,20,28,31}) or inherited from a
            # DBC template read as signed must be emitted as its uint32 two's-complement form. SIGNED
            # columns (EffectBasePoints_* etc.) keep their negatives — e.g. Stoneskin's -3 reduction.
            if t == "unsigned" and iv < 0:
                iv &= 0xFFFFFFFF
            vals.append(str(iv))
    return "(" + ", ".join(vals) + ")"


def _class_mask(d, affects):
    ref = d.get("_ref", {}) or {}
    spells = ref.get("spells", {}) or {}
    a = b = c = 0
    for name in affects:
        f = spells.get(name)
        if not f:
            raise ValueError(f"affects spell {name!r} not in _ref.spells")
        a |= int(f.get("flagsA", 0)); b |= int(f.get("flagsB", 0)); c |= int(f.get("flagsC", 0))
    return a, b, c


def _resolve_affects_mask(d, spec):
    """The (A,B,C) family class-mask a spellmod effect binds by. Defaults to the OR of every
    `affects:` spell's FULL identity (via _class_mask). An optional `affectsMask: {a,b,c}` on the
    node/effect overrides it verbatim — required when a target's full identity includes a SHARED
    family bit that would OVER-bind. The paladin AURA-improvement talents are the motivating case:
    Retribution/Devotion/Concentration Aura all carry the shared flagsC bit5 (=32) 'paladin aura'
    category, and core SpellInfo::IsAffected ANDs across all three flag words, so binding by the
    full identity leaks the +% onto the OTHER two auras. Each aura's UNIQUE flagsA bit is
    sufficient and correct (Retribution=8, Devotion=64, Concentration=131072). `affects:` is still
    authored (it drives the addon spellMods name mapping + lint); only the emitted DBC mask changes.
    Reusable by the Prot Improved Devotion/Concentration Aura nodes (Tasks 7/8)."""
    am = spec.get("affectsMask")
    if am is not None:
        return int(am.get("a", 0)), int(am.get("b", 0)), int(am.get("c", 0))
    return _class_mask(d, spec["affects"])


def _spellmod_row(d, node, rank, eff, tip):
    fam = int((d.get("_ref") or {}).get("familyName", 3))
    a, b, c = _resolve_affects_mask(d, node)
    aura = AURA[node.get("kind", "flat")]
    op = OP[node["op"]]
    over = {
        "ID": custom_passive_id(node["id"], rank), "Attributes": PASSIVE_ATTR,
        "CastingTimeIndex": 1, "RangeIndex": 1, "Effect_1": 6, "EffectDieSides_1": 1,
        "EffectBasePoints_1": eff - 1, "EffectAura_1": aura, "EffectMiscValue_1": op,
        "EffectSpellClassMaskA_1": a, "EffectSpellClassMaskA_2": b, "EffectSpellClassMaskA_3": c,
        "SpellClassSet": fam, "SchoolMask": 0, "SpellIconID": 1, "SpellPriority": 50, "EquippedItemClass": -1,
        "Name_Lang_enUS": f"{node['name']} (Rank {rank})", "Name_Lang_Mask": 16712190,
        "Description_Lang_enUS": tip, "Description_Lang_Mask": 16712190,
    }
    # Optional `procPassive:` — the ONE passive this node grants ALSO carries a proc-trigger on
    # Effect_2 (aura 42 PROC_TRIGGER_SPELL), so a spellmod node can additionally hang a per-rank
    # triggered aura on another unit without a second granted spell (the engine's one-spell-per-node-
    # rank model can't grant two). Motivating case: Vanilla Improved Lay on Hands (node 18607) reduces
    # LoH's cooldown (the spellmod) AND, on LoH cast, buffs the target's armor (the proc). emit_custom_sql
    # emits a matching spell_proc row keyed on this passive id. The trigger is per-rank (a list) or shared.
    pp = node.get("procPassive")
    if pp:
        t = pp["trigger"]
        trig = int(t[rank - 1]) if isinstance(t, list) else int(t)
        over.update({"Effect_2": 6, "EffectAura_2": 42, "EffectDieSides_2": 0,
                     "EffectBasePoints_2": 0, "EffectTriggerSpell_2": trig, "ImplicitTargetA_2": 1})
    # NOTE: stances: is intentionally NOT applied here — a spellmod tweaks a stock spell's
    # numbers globally (cast time/cost/damage%/radius...), not per-form; form-gating a spellmod
    # would be meaningless since it doesn't itself apply an effect while shapeshifted.
    return _row(over)


# Druid shapeshift-form gating -> spell_dbc ShapeshiftMask (col 12). Mask bit for a form is
# 1 << (formId - 1); formId per ShapeshiftForm in
# azerothcore-wotlk/src/server/game/Entities/Unit/UnitDefines.h:73-101 (FORM_CAT=0x01,
# FORM_TRAVEL=0x03, FORM_AQUA=0x04, FORM_BEAR=0x05, FORM_DIREBEAR=0x08, FORM_MOONKIN=0x1F).
# Two-way verified 2026-08-20: the core itself computes this identically
# (SpellInfo::CheckShapeshift, SpellInfo.cpp:1453, `stanceMask = 1 << (form - 1)`), and the
# formula matches the empirical Spell.dbc ShapeshiftMask of stock Druid abilities dumped via
# tools/dump_spell_effects.py — Shred/Rip/Ferocious Bite (Cat-only) = 1 = 1<<(1-1); Maul/
# Swipe(Bear) (Bear+Dire Bear) = 144 = (1<<(5-1)) | (1<<(8-1)).
STANCE_TOKENS = {
    "cat":      1 << (1 - 1),
    "travel":   1 << (3 - 1),
    "aquatic":  1 << (4 - 1),
    "bear":     1 << (5 - 1),
    "direbear": 1 << (8 - 1),
    "moonkin":  1 << (31 - 1),
    # FORM_TREE = 0x02 (UnitDefines.h:74) -> HandleShapeshiftBoosts tests
    # `spellInfo->Stances & (1 << (GetMiscValue() - 1))` = bit 1 = 2. Stock Tree of Life Aura 34123
    # carries exactly this ShapeshiftMask; the TBC druid Tree of Life form-passive 946265 mirrors it.
    "tree":     1 << (2 - 1),
}

# Node-level mechanics whose row builder actually calls _apply_stances (grep the call sites to
# keep this in sync: _stat_row/_multi_row/_proc_passive_row — the latter handles proc AND
# proc-scripted AND scripted, since emit_custom_sql dispatches all three to it). `spellmod` and
# `pet` nodes route through _spellmod_row / _pet_passive_row+_pet_buff_row, neither of which
# applies stances, and a mechanic-less (grant-only/display_only) node never emits a custom row at
# all — a `stances:` key on any of those would be silently dropped at emit time, so lint rejects it.
_STANCES_HONORED_MECHANICS = frozenset({"stat", "multi", "proc", "proc-scripted", "scripted"})


def _resolve_stance_mask(stances):
    if isinstance(stances, bool):
        raise ValueError(f"stances must be an int mask or token list, not a bool: {stances!r}")
    if isinstance(stances, int):
        if stances <= 0:
            raise ValueError(f"stances mask must be a positive int, got {stances!r}")
        return stances
    if isinstance(stances, list) and stances:
        mask = 0
        for tok in stances:
            key = str(tok).lower()
            if key not in STANCE_TOKENS:
                raise ValueError(f"unknown stance token {tok!r}; known: {sorted(STANCE_TOKENS)}")
            mask |= STANCE_TOKENS[key]
        return mask
    raise ValueError(f"stances must be a positive int or non-empty token list, got {stances!r}")


def _apply_stances(over, node):
    """Copy a node's optional stances: form-restriction onto a spell row (core honors
    ShapeshiftMask). Mirrors _apply_equip_gate; used by Druid form-conditional passives
    (Sharpened Claws, Heart of the Wild, Predatory Strikes, Savage Fury, ...)."""
    if node.get("stances") is not None:
        over["ShapeshiftMask"] = _resolve_stance_mask(node["stances"])
    return over


def _apply_equip_gate(over, node):
    """Copy a node's optional weapon-equip requirement onto a spell row so an aura/proc only
    applies while a matching weapon is held (core CheckAttackFitToAuraRequirement). Mirrors the
    _stat_row gate; used by rogue weapon-spec procs (Sword/Mace) and any multi variant."""
    if node.get("equipClass") is not None:
        over["EquippedItemClass"] = int(node["equipClass"])
    if node.get("equipSubclass") is not None:
        over["EquippedItemSubclass"] = int(node["equipSubclass"])
    return over


def _stat_row(d, node, rank, eff, tip):
    over = {
        "ID": custom_passive_id(node["id"], rank), "Attributes": PASSIVE_ATTR,
        "CastingTimeIndex": 1, "RangeIndex": 1, "Effect_1": 6, "EffectDieSides_1": 1,
        "EffectBasePoints_1": eff - 1, "EffectAura_1": int(node["aura"]),
        "EffectMiscValue_1": int(node.get("school", 127)),
        "SpellClassSet": 0, "SchoolMask": 0, "SpellIconID": 1, "SpellPriority": 50, "EquippedItemClass": -1,
        "Name_Lang_enUS": f"{node['name']} (Rank {rank})", "Name_Lang_Mask": 16712190,
        "Description_Lang_enUS": tip, "Description_Lang_Mask": 16712190,
    }
    # Optional weapon equip-requirement, so an aura only applies while a matching weapon is held
    # (Wand Specialization: aura 79 gated to wands via EquippedItemClass=2 weapon +
    # EquippedItemSubclass = the wand subclass bitmask 1<<19 = 524288, NOT 1<<18 which is CROSSBOW).
    # A player wand SHOT is a MAGIC-school shot (not physical), so the MOD_DAMAGE_PERCENT_DONE school
    # (EffectMiscValue) must be 126 (all-but-physical), not 1. Core's CheckAttackFitToAuraRequirement /
    # MeleeDamageBonusDone enforces the equip gate so it only touches wand swings.
    _apply_equip_gate(over, node)
    _apply_stances(over, node)
    return _row(over)


def _pet_passive_row(d, node, rank, tip):
    """The talent passive for a `pet:` node — APPLY_AURA(SPELL_AURA_DUMMY) at effect index 0 with
    ImplicitTargetA = 1 (TARGET_UNIT_CASTER), byte-for-byte the STOCK pet-aura talent shape (WotLK
    Master Demonologist 23785, tool-verified 2026-08-16). SpellMgr::LoadPetAuras accepts either a
    dummy EFFECT or a dummy AURA, but the two register differently at runtime: a bare
    SPELL_EFFECT_DUMMY (the pre-2026-08-16 emission) relies on Spell::EffectDummy firing at the
    learn-cast, which NEVER RAN for this row shape (live-verified: learned pet talents left
    m_petAuras empty — the 2026-08-16 'summon talents do nothing' bug); the dummy AURA registers in
    AuraEffect::HandleAuraDummy on AURA_EFFECT_HANDLE_REAL apply (AddPetAura) and symmetrically
    unregisters on remove (RemovePetAura), riding the same aura application every other era passive
    already uses. Owning this passive is what makes Pet::CastPetAuras cast the mapped pet-buff."""
    return _row({
        "ID": custom_passive_id(node["id"], rank), "Attributes": PASSIVE_ATTR,
        "CastingTimeIndex": 1, "RangeIndex": 1,
        "Effect_1": 6, "EffectAura_1": 4, "ImplicitTargetA_1": 1,   # APPLY_AURA(DUMMY) on self
        "SpellClassSet": 0, "SchoolMask": 0, "SpellIconID": 1, "SpellPriority": 50,
        "EquippedItemClass": -1,
        "Name_Lang_enUS": f"{node['name']} (Rank {rank})", "Name_Lang_Mask": 16712190,
        "Description_Lang_enUS": tip, "Description_Lang_Mask": 16712190,
    })


def _pet_buff_row(spec, buff_id, rank, tip, name):
    """The pet-buff spell the demon casts on ITSELF (ImplicitTargetA_1 = 1, TARGET_UNIT_CASTER —
    see Unit::CastPetAura, which does CastSpell(pet, aura, true)). Same base-point convention as
    _stat_row (EffectBasePoints = val-1, EffectDieSides = 1). `spec` is the node itself for the
    single/demon-agnostic shape, or one `petVariants:` entry for the per-demon shape; either way it
    supplies petAura (+ optional petMisc / petSchool) and `vals`."""
    eff = int(spec["vals"][rank - 1])
    return _row({
        "ID": buff_id, "Attributes": PASSIVE_ATTR,
        "CastingTimeIndex": 1, "RangeIndex": 1, "Effect_1": 6, "EffectDieSides_1": 1,
        "EffectBasePoints_1": eff - 1, "EffectAura_1": int(spec["petAura"]),
        "EffectMiscValue_1": int(spec.get("petMisc", 0)),
        "ImplicitTargetA_1": 1,                                   # TARGET_UNIT_CASTER (pet -> self)
        "SpellClassSet": 0, "SchoolMask": int(spec.get("petSchool", 0)),
        "SpellIconID": 1, "SpellPriority": 50, "EquippedItemClass": -1,
        "Name_Lang_enUS": f"{name} (Pet, Rank {rank})", "Name_Lang_Mask": 16712190,
        "Description_Lang_enUS": tip, "Description_Lang_Mask": 16712190,
    })


def _pet_bindings(node):
    """The (pet_entry, buff-spec) pairs for a `pet:` node, one per demon. Single/demon-agnostic
    shape -> a single (pet or 0, node) pair; per-demon `petVariants:` shape -> one pair per variant
    (each variant is a {pet, petAura, vals, ...} dict). Callers derive the pet-buff id per pair."""
    variants = node.get("petVariants")
    if variants:
        return [(int(v.get("pet", 0)), v) for v in variants]
    return [(int(node.get("pet", 0)), node)]


def _effect_fields(d, spec, rank, i):
    """Build the Effect_i/EffectAura_i/... columns for one effect (1-based `i`) of a multi-effect
    passive. `spec` is one entry of a node's `effects:` list: either a spellmod effect
    (type=spellmod: op/kind/vals + family class-mask from `affects`) or a plain stat aura
    (type=stat: aura/vals + misc/miscB). Returns a dict of column overrides for `_row`."""
    eff = int(spec["vals"][rank - 1])
    f = {f"Effect_{i}": 6, f"EffectDieSides_{i}": 1, f"EffectBasePoints_{i}": eff - 1}
    if spec["type"] == "spellmod":
        f[f"EffectAura_{i}"] = AURA[spec.get("kind", "flat")]
        f[f"EffectMiscValue_{i}"] = OP[spec["op"]]
        a, b, c = _resolve_affects_mask(d, spec)
        p = MASK_PREFIX[i]
        f[f"{p}_1"], f[f"{p}_2"], f[f"{p}_3"] = a, b, c
    else:  # stat
        f[f"EffectAura_{i}"] = int(spec["aura"])
        f[f"EffectMiscValue_{i}"] = int(spec.get("misc", 0))
        if spec.get("miscB") is not None:
            f[f"EffectMiscValueB_{i}"] = int(spec["miscB"])
        # A stat effect may ALSO carry a family class mask. Most don't (a plain stat aura is
        # spell-agnostic), but SPELL_AURA_ADD_TARGET_TRIGGER (109) is scoped BY mask: it means
        # "when you land a spell matching this mask, also cast `trigger` on that spell's target".
        if spec.get("affects") is not None or spec.get("affectsMask") is not None:
            a, b, c = _resolve_affects_mask(d, spec)
            p = MASK_PREFIX[i]
            f[f"{p}_1"], f[f"{p}_2"], f[f"{p}_3"] = a, b, c
        # `trigger:` — a single id shared by all ranks, or a per-rank list (the `procPassive:`
        # idiom). ImplicitTargetA 1 = the aura sits on the CASTER; the core picks the triggering
        # spell's target for the cast itself (Spell::DoTriggersOnSpellHit), matching stock
        # aura-109 carriers like Improved Sprint 13875.
        t = spec.get("trigger")
        if t is not None:
            f[f"EffectTriggerSpell_{i}"] = int(t[rank - 1]) if isinstance(t, list) else int(t)
            f[f"ImplicitTargetA_{i}"] = 1
    # `perLevel:` -> EffectRealPointsPerLevel_i (a FLOAT column). Core CalcValue
    # (SpellInfo.cpp SpellEffectInfo::CalcValue) adds
    #   RealPointsPerLevel * (min(casterLevel, MaxLevel or inf) - max(BaseLevel, SpellLevel))
    # to the base value, so a talent whose magnitude GROWS with the character's level is pure data.
    # A generated node passive leaves SpellLevel/BaseLevel/MaxLevel at 0, which is exactly the shape
    # the wago rows that need this carry, so the growth window is `casterLevel` and no level keys are
    # needed. Same key NAME and meaning as the `helpers:` effect key (helper_overrides below) — one
    # spelling for one column. Single float shared by all ranks, or a per-rank list (the `trigger:`
    # convention above).
    # First use: TBC rogue Serrated Blades (node 20455), whose armor-ignore half is
    # aura 123 MOD_TARGET_RESISTANCE with basePoints 0 and EffectRealPointsPerLevel
    # -2.67/-5.34/-8.00 — an int32 unpack of that column hides the whole mechanic, and flattening it
    # to the level-70 total would hand a level-40 rogue the level-70 armor reduction.
    # Written ONLY when present: no shipped dataset node effect carries the key (grepped), so this
    # is byte-neutral for every already-generated row.
    pl = spec.get("perLevel")
    if pl is not None:
        f[f"EffectRealPointsPerLevel_{i}"] = float(pl[rank - 1] if isinstance(pl, list) else pl)
    return f


def _multi_row(d, node, rank, tip):
    fam = int((d.get("_ref") or {}).get("familyName", 3))
    effects = node["effects"]
    # SpellClassSet must carry the family whenever ANY effect binds by class mask — spellmods
    # always do, and a stat effect does when it authors affects:/affectsMask: (aura 109
    # ADD_TARGET_TRIGGER). Getting this wrong is silent and severe: SpellInfo::IsAffected opens
    # with `if (!familyName) return true;`, so a family-0 masked effect matches EVERY spell.
    has_mask = any(e["type"] == "spellmod" or e.get("affects") is not None
                   or e.get("affectsMask") is not None for e in effects)
    over = {
        "ID": custom_passive_id(node["id"], rank), "Attributes": PASSIVE_ATTR,
        "CastingTimeIndex": 1, "RangeIndex": 1,
        # A pure-stat passive with no mask stays family-less so it can't accidentally match a spell.
        "SpellClassSet": fam if has_mask else 0, "SchoolMask": 0,
        "SpellIconID": 1, "SpellPriority": 50, "EquippedItemClass": -1,
        "Name_Lang_enUS": f"{node['name']} (Rank {rank})", "Name_Lang_Mask": 16712190,
        "Description_Lang_enUS": tip, "Description_Lang_Mask": 16712190,
    }
    for i, e in enumerate(effects, start=1):
        over.update(_effect_fields(d, e, rank, i))
    _apply_equip_gate(over, node)
    _apply_stances(over, node)
    return _row(over)


def _proc_passive_row(d, node, rank, tip):
    over = {
        "ID": custom_passive_id(node["id"], rank), "Attributes": PASSIVE_ATTR,
        "CastingTimeIndex": 1, "RangeIndex": 1, "Effect_1": 6,
        "SpellClassSet": 0, "SchoolMask": 0, "SpellIconID": 1, "SpellPriority": 50, "EquippedItemClass": -1,
        "Name_Lang_enUS": f"{node['name']} (Rank {rank})", "Name_Lang_Mask": 16712190,
        "Description_Lang_enUS": tip, "Description_Lang_Mask": 16712190,
    }
    mech = node.get("mechanic")
    if mech in ("proc-scripted", "scripted"):
        # A SPELL_AURA_DUMMY effect the C++ AuraScript hooks on OnEffectProc EFFECT_0. Its amount
        # carries the per-rank magnitude (pct) the script reads via aurEff->GetAmount() and applies
        # itself (e.g. Ignite's DoT %, Master of Elements' mana-refund %) — the script casts the
        # real stock trigger, so no PROC_TRIGGER_SPELL here.
        # `scripted` is the same DUMMY carrier but bound via a top-level `scriptBindings:` on some
        # OTHER spell, so it also stamps EffectMiscValue with the node's marker (the C++ side reads
        # the marker to find this passive) and emits NO spell_proc / spell_script_names of its own.
        val = int(node["vals"][rank - 1])
        over.update({"EffectAura_1": 4, "EffectDieSides_1": 1, "EffectBasePoints_1": val - 1})
        if mech == "scripted":
            over["EffectMiscValue_1"] = int(node["marker"])
    else:
        # Plain data proc: the engine auto-casts the trigger spell with its own base points.
        over.update({"EffectAura_1": 42, "EffectDieSides_1": 0, "EffectBasePoints_1": 0,
                     "EffectTriggerSpell_1": _proc_trigger(node, rank)})
    _apply_equip_gate(over, node)
    _apply_stances(over, node)
    return _row(over)


def _proc_chance(node, rank):
    chance = node["proc"].get("chance", DEFAULT_PROC_CHANCE)
    return int(chance[rank - 1]) if isinstance(chance, list) else int(chance)


def _proc_trigger(node, rank):
    """The data-proc trigger spell for this rank. `proc.trigger` is either a single id (all ranks
    cast the same spell) or a per-rank list (e.g. Improved Blizzard's Chilled 12484/12485/12486,
    one slow % per rank)."""
    t = node["proc"]["trigger"]
    return int(t[rank - 1]) if isinstance(t, list) else int(t)


# Proc-flag applicability masks, transcribed from the fork's SpellMgr.h (enum ProcFlags).
# LoadSpellProcs logs "won't be used" errors when a spell_proc field is set for ProcFlags
# outside these masks — mirror its predicates so generated rows never warn.
_SPELL_PROC_FLAG_MASK = 0x2FFFF0          # every DONE/TAKEN spell + periodic + trap flag (+ ranged auto-attack — the core's own quirk)
_PERIODIC_PROC_FLAG_MASK = 0x0C0000       # DONE_PERIODIC | TAKEN_PERIODIC
_DONE_HIT_PROC_FLAG_MASK = 0xE55554       # all DONE flags incl. auto/main/offhand
_TAKEN_HIT_PROC_FLAG_MASK = 0x1AAAA8      # all TAKEN flags incl. TAKEN_DAMAGE
_REQ_SPELL_PHASE_PROC_FLAG_MASK = _SPELL_PROC_FLAG_MASK & _DONE_HIT_PROC_FLAG_MASK  # 0x255550


def _spell_proc_row(d, node, rank):
    return _spell_proc_row_for(d, custom_passive_id(node["id"], rank), node["proc"],
                               _proc_chance(node, rank))


def _spell_proc_row_for(d, spell_id, proc, chance):
    cols = ("SpellId", "SchoolMask", "SpellFamilyName", "SpellFamilyMask0", "SpellFamilyMask1",
            "SpellFamilyMask2", "ProcFlags", "SpellTypeMask", "SpellPhaseMask", "HitMask",
            "AttributesMask", "DisableEffectsMask", "ProcsPerMinute", "Chance", "Cooldown",
            "Charges")
    # Optional `proc.affects` restricts the proc to a family + spell mask, so e.g. Impact fires
    # only on the mage's Fire spells (SpellMgr checks eventSpellInfo->IsAffected(family, mask)).
    # Alternatively `proc.school` (+ optional `proc.family`) restricts by damage SCHOOL instead of
    # the per-spell family mask — needed for "all your Shadow damage spells" where a member spell's
    # identity lives only in family-flag word 3 (e.g. Mind Flay 0,0,0x440): the word-3 mask matches
    # in theory but not in practice for the proc path, whereas a shadow SchoolMask is unambiguous
    # and more faithful (it catches every shadow spell, DoTs + channels included).
    fam, m0, m1, m2 = 0, 0, 0, 0
    # `proc.affects` = the OR of each named spell's FULL family identity; `proc.affectsMask
    # {a,b,c}` overrides that VERBATIM, exactly as it does for a spellmod node/effect
    # (_resolve_affects_mask). The override is REQUIRED when a named target carries a SHARED family
    # bit that would over-bind: SpellMgr::CanSpellTriggerProcOnEvent tests the event spell with
    # SpellInfo::IsAffected, which is true on ANY nonzero word intersection. Motivating case: TBC
    # rogue Find Weakness (node 20419) procs on FINISHING MOVES, TBC mask (0x3e0000, 0x9, 0) — but
    # Eviscerate/Envenom/Deadly Throw also carry family-8 word-0 bit 23, the shared
    # "offensive ability" bit 88 rogue spells have, so the identity OR (0xbe0000, 0x9, 0) would
    # also fire off Sinister Strike/Backstab/Ambush/Hemorrhage. Either key alone sets the family
    # from `_ref.familyName` (a family-0 row means "no family filter at all").
    if proc.get("affects") or proc.get("affectsMask") is not None:
        ref = d.get("_ref", {}) or {}
        fam = int(ref.get("familyName", 0))
        m0, m1, m2 = _resolve_affects_mask(d, proc)
    if proc.get("family"):
        fam = int(proc["family"])
    # `ppm` is read (and validated) before the row is built because it also decides whether an
    # authored `chance` is usable at all — see the Chance interaction below.
    _ppm = float(proc.get("ppm", 0) or 0)
    if _ppm < 0:
        raise ValueError(f"proc for spell {spell_id}: ppm={proc['ppm']} is negative — "
                         f"SpellMgr::LoadSpellProcs clamps it to 0 at boot and logs an sql.sql "
                         f"error, so the row would silently lose its rate; author a positive rate "
                         f"or drop the key")
    # CHANCE vs PPM — the same "an explicitly authored value the core would ignore is a hard error"
    # rule the ProcFlags-driven fields below follow. `Aura::CalcProcChance` (SpellAuras.cpp:2270)
    # seeds `chance` from procEntry.Chance and then OVERWRITES it wholesale with GetPPMProcChance()
    # whenever ProcsPerMinute != 0 and the event carries damage or heal info. So with a non-zero
    # ppm an authored Chance is dead for every ordinary proc event and resurrects only for a
    # damage-less, heal-less one — which is exactly the kind of half-live value that reads as
    # intentional and is not. Explicit non-zero => refuse; absent => zero it silently (the stock
    # PPM rows carry Chance 0, so that is also the faithful emission).
    if _ppm != 0:
        _authored = proc.get("chance")
        if _authored is not None:
            _vals_authored = _authored if isinstance(_authored, list) else [_authored]
            if any(float(v) != 0 for v in _vals_authored):
                raise ValueError(
                    f"proc for spell {spell_id}: chance={_authored} is unusable alongside "
                    f"ppm={proc['ppm']} — Aura::CalcProcChance overwrites Chance with the PPM "
                    f"result whenever the proc event carries damage or heal info, so the authored "
                    f"chance would apply only to damage-less events; author `chance: 0` (the stock "
                    f"PPM-row shape) or drop the ppm")
        else:
            chance = 0
    vals = {
        "SpellId": spell_id, "SchoolMask": int(proc.get("school", 0)),
        "SpellFamilyName": fam,
        "SpellFamilyMask0": m0, "SpellFamilyMask1": m1, "SpellFamilyMask2": m2,
        "ProcFlags": int(proc["flags"]), "SpellTypeMask": int(proc.get("typeMask", 1)),
        # HitMask default 0 = the core's own default (NORMAL|CRIT for taken, NORMAL|CRIT|ABSORB
        # for done — SpellMgr::CanSpellTriggerProcOnEvent). NEVER default to a specific hit bit:
        # the old default 8 (PROC_HIT_FULL_RESIST) silently made VE proc only on full resists.
        "SpellPhaseMask": int(proc.get("phaseMask", 2)), "HitMask": int(proc.get("hitMask", 0)),
        "AttributesMask": int(proc.get("attrMask", 0)), "DisableEffectsMask": 0,
        # `ppm` = spell_proc.ProcsPerMinute — a weapon-speed-scaled rate the flat `Chance` column
        # cannot express. The core prefers it when non-zero (SpellMgr::LoadSpellProcs only falls
        # back to the DBC ProcChance when BOTH Chance and ProcsPerMinute are 0), so a clone of a
        # PPM-rated stock proc MUST carry it or it reverts to a flat per-hit chance. First used by
        # the TBC shaman Shamanistic Rage clone (stock 30823's row is ProcsPerMinute 18 / Chance 0).
        # FRACTIONAL VALUES ARE REAL AND MUST SURVIVE. `spell_proc.ProcsPerMinute` is a **float**
        # column (data/sql/base/db_world/spell_proc.sql) that the core reads as a float
        # (SpellMgr.h `float ProcsPerMinute`), and the two PPM rows this repo already ships by hand
        # are BOTH 3.5 (the Vanilla and TBC druid Omen of Clarity procs). An `int()` cast here would
        # turn `ppm: 3.5` into 3 — a 14% rate error with no exception, no lint and no audit finding.
        # So: emit an int when the value is integral (keeps every existing row byte-identical —
        # `str(18)` not `str(18.0)`) and a float otherwise.
        "ProcsPerMinute": int(_ppm) if _ppm.is_integer() else _ppm,
        "Chance": int(chance), "Cooldown": int(proc.get("cooldown", 0)), "Charges": 0,
    }
    # Zero fields the core can't use for these ProcFlags (it logs "won't be used" otherwise).
    # An EXPLICITLY authored YAML value that the core would ignore is a hard error instead —
    # the generator's own defaults (key absent, or explicitly authored as 0) still zero silently.
    flags = vals["ProcFlags"]
    field_usable = {
        "SpellTypeMask": bool(flags & (_SPELL_PROC_FLAG_MASK | _PERIODIC_PROC_FLAG_MASK)),
        "SpellPhaseMask": bool(flags & _REQ_SPELL_PHASE_PROC_FLAG_MASK),
        "HitMask": bool(flags & (_TAKEN_HIT_PROC_FLAG_MASK | _DONE_HIT_PROC_FLAG_MASK)),
    }
    for yaml_key, field in (("typeMask", "SpellTypeMask"), ("phaseMask", "SpellPhaseMask"),
                             ("hitMask", "HitMask")):
        if field_usable[field]:
            continue
        if yaml_key in proc and int(proc[yaml_key]) != 0:
            raise ValueError(f"proc for spell {spell_id}: {yaml_key}={proc[yaml_key]} is unusable "
                              f"for flags={flags} (the core ignores it) — fix the flags or drop the key")
        vals[field] = 0
    row = "(" + ", ".join(str(vals[c]) for c in cols) + ")"
    return cols, row


# YAML helper key -> spell_dbc column, for the castable-metadata scalars.
_HELPER_SCALARS = [
    ("attributes", "Attributes"),
    # AttributesEx (ATTR1). Needed when a TEMPLATE carries ATTR1 bits the clone must not inherit —
    # in particular bits that SpellInfoCorrections.cpp clears from the template BY ID at load, which
    # a clone never receives. Motivating case: the TBC druid Improved Faerie Fire companion debuffs
    # (946277-946279) template off stock Faerie Fire 770, whose AttributesEx 98304 includes
    # SPELL_ATTR1_IMMUNITY_TO_HOSTILE_AND_FRIENDLY_EFFECTS — a bit the core strips from 770/16857 by
    # id, so an inheriting clone would bypass school immunity that blocks Faerie Fire itself. Author
    # `attributesEx: 0` to opt out. Unlisted on every existing helper, so adding this key is
    # byte-neutral for already-shipped data.
    ("attributesEx", "AttributesEx"),
    # AttributesEx2/3/5 (ATTR2/3/5). Same motivation as attributesEx above, in the OTHER direction:
    # `SpellInfoCorrections.cpp` ORs bits ONTO a stock spell BY ID at load, and a clone -- a different
    # id -- never receives them. **`era_audit.py::check_spellfix_inheritance` now enforces this
    # repo-wide** -- do not rely on a hand sweep, which is how this was missed: the original sweep
    # covered only the two shaman datasets while this comment said "these datasets", in a file all
    # twelve share. The keys below exist so a clone can reproduce a correction by hand:
    #   * ATTR5 SPELL_ATTR5_EXTRA_INITIAL_PERIOD 0x200 -- ApplySpellFix({8145, 6474}) makes Tremor and
    #     Earthbind tick IMMEDIATELY on apply. Without it a driver clone's first tick waits a full
    #     period (Earthbind ~3 s), which partly undoes the very defect the driver exists to fix.
    #   * ATTR2 SPELL_ATTR2_IGNORE_LINE_OF_SIGHT -- the Cleansing and Flametongue effect clones.
    #   * ATTR3 -- Stormstrike's DOT stacking rule, and the paladin Judgement clones' caster-proc
    #     suppression.
    # **DIRECTION IS A PROPERTY OF THE FIX, NOT OF THE KEY.** An earlier version of this comment
    # filed `attributesEx` under "bits a clone must not inherit" and the rest under "the OTHER
    # direction", which would send someone with a strip-direction ATTR3 case looking for a key that
    # is right here. `SpellInfoCorrections.cpp:4009` is exactly that: `AttributesEx3 &= ~...`, and it
    # hits cloned helper 932746. EVERY key here authors the FINAL value; either `|=` or `&= ~` can be
    # the reason you need it.
    # Unlisted on every existing helper, so adding these keys is byte-neutral for shipped data.
    ("attributesEx2", "AttributesEx2"),
    ("attributesEx3", "AttributesEx3"),
    # AttributesEx4 (ATTR4). NOT a SpellInfoCorrections case — the motivating bit is CLIENT-ONLY.
    # `SPELL_ATTR4_FORCE_DISPLAY_CASTBAR` (0x08000000) is never read anywhere in src/server (grepped
    # at fork HEAD: the only hits are the enum and its enuminfo reflection rows), so on the SERVER row
    # it is inert — but `tools/build_client_dbc.py` builds the CLIENT patch row through this SAME
    # `helper_overrides`, and there the bit is what makes a cast bar render. The case that added it:
    # the TBC hunter Aimed Shot clone chain 948370-948376 (era-data/_ref/tbc/hunter-spells.yaml
    # `attribute_divergences.individually_load_bearing.19434_aimed_shot`) — wago 2.5.4 19434 carries
    # AttributesEx4 0x18000000 (FORCE_DISPLAY_CASTBAR | IGNORE_COMBAT_TIMERS) where live 19434 carries
    # only 0x10000000, because WotLK made Aimed Shot instant and dropped the cast bar. The era clone
    # restores TBC's 2.5 s cast, so it must restore the bit or the 2.5 s cast has no bar.
    # DELIBERATELY NOT added to `era_audit.py::_SPELLFIX_KEYABLE`: `ApplySpellFix({19574})` (Bestial
    # Wrath) touches AttributesEx4, and both shipped BW clones (Vanilla 932967, TBC 948290) would newly
    # fail the audit — a verdict change on merged content, out of scope for the key itself.
    # Unlisted on every existing helper, so adding this key is byte-neutral for shipped data.
    ("attributesEx4", "AttributesEx4"),
    ("attributesEx5", "AttributesEx5"),
    # AttributesEx7 (ATTR7). Same motivating shape as attributesEx2/3/5 — a `SpellInfoCorrections.cpp`
    # by-id `|=` a clone can never inherit. The case that added it: `SpellInfoCorrections.cpp:3886`
    # ORs SPELL_ATTR7_CAN_CAUSE_INTERRUPT (0x800) onto {47476, 15487 Priest Silence, 5211/6798/8983
    # Druid Bash}, and the TBC priest Silence clone 948071 templates 15487 — whose own DBC ATTR7 is
    # 0 on BOTH sides, so without this key the era Silence applies the silence aura but does NOT
    # interrupt the target's in-progress cast (spec Amendment A.14). `era_audit.py`'s
    # `_SPELLFIX_KEYABLE` gained the matching `AttributesEx7 -> attributesEx7` row in the same
    # commit; before it, that fix landed in the "no generator key — acknowledge only" branch.
    # Unlisted on every existing helper, so adding this key is byte-neutral for shipped data.
    ("attributesEx7", "AttributesEx7"),
    # DefenseType (the DBC DmgClass column: 0 NONE / 1 MAGIC / 2 MELEE / 3 RANGED). LOAD-BEARING for
    # a triggered debuff a clone must let the target RESIST: `Unit::SpellHitResult` switches on it and
    # returns SPELL_MISS_NONE unconditionally for DmgClass 0, while DmgClass 1 routes through
    # `MagicSpellHitResult` (resist/miss roll). The case that added it: the TBC priest Blackout stun
    # 948079 is a FROM-SCRATCH reconstruction (wago 15269 is absent from 3.3.5a entirely), and wago's
    # own record carries DefenseType 1 — a from-scratch helper defaults every unlisted scalar to 0,
    # which would have shipped an unresistable stun. (The shipped VANILLA Blackout stun 932970 has
    # exactly that gap; it is recorded, not retro-fixed, because changing it is a live behaviour
    # change to merged content.) Unlisted on every existing helper -> byte-neutral for shipped data.
    ("defenseType", "DefenseType"),
    # StartRecoveryCategory + StartRecoveryTime — the GLOBAL COOLDOWN pair. StartRecoveryTime is the
    # GCD length in ms and is the ON/OFF switch: `Spell::TriggerGlobalCooldown` (Spell.cpp:8922) opens
    # `int32 gcd = m_spellInfo->StartRecoveryTime; if (!gcd) ... return;`, so a 0 there means the cast
    # starts no GCD at all. StartRecoveryCategory is the category the GCD is tracked under (133 = the
    # standard caster 1500 ms) and is separately read by `Spell::CheckPetCast` (Spell.cpp:6860) and by
    # TriggerGlobalCooldown's haste-scaling branch (:8948, which requires category 133 + time 1500);
    # `Spell::HasGlobalCooldown` (:8911) is the reader that asks whether one is already running.
    # The case that added them: TBC priest Silence 15487 is `StartRecoveryCategory 133 /
    # StartRecoveryTime 1500` (ON the GCD) while LIVE 15487 is 0/0 — WotLK took Silence off the GCD, a
    # real PvP divergence a template clone would inherit (spec Amendment A.9). Author BOTH: a time with
    # no category loses the haste scaling and the pet-cast gate, and a category with no time is inert.
    # Unlisted on every existing helper, so adding these keys is byte-neutral for shipped data.
    ("startRecoveryCategory", "StartRecoveryCategory"),
    ("startRecoveryTime", "StartRecoveryTime"),
    # PreventionType (SPELL_PREVENTION_TYPE_*): 1 = blocked by silence / school lockout. Both eras'
    # totem SUMMONS are 1 (wago 2.5.4 measured: all 72 comparable summons diverge wago 1 -> live 0;
    # WotLK made totems droppable while silenced) — see shaman-totems-tbc.yaml accepted_gaps id 14,
    # closed 2026-09-02 by authoring this on every real totem summon clone in both eras. Sentry 6495
    # is 0 on BOTH sides — never author it there. Unlisted keys default to the template's value, so
    # adding this key is byte-neutral for shipped data.
    ("preventionType", "PreventionType"),
    ("castTimeIndex", "CastingTimeIndex"),
    ("rangeIndex", "RangeIndex"), ("cooldown", "RecoveryTime"), ("powerCost", "ManaCost"),
    # Category cooldown (the DBC Category + CategoryRecoveryTime columns). A template clone inherits
    # its base spell's shared-cooldown category; some era clones must NEUTRALIZE it — the Vanilla
    # Aimed Shot clones (932952-957) set category:0 + categoryCooldown:0 so their 6s cooldown lives
    # purely in RecoveryTime, because stock Aimed Shot's 10s cooldown is a Category-85
    # CategoryRecoveryTime (not RecoveryTime), which the inherited row would otherwise carry through
    # and pin the clone at 10s regardless of the RecoveryTime override. Only meaningful on templated
    # helpers (legacy helpers default both to 0 like every unlisted scalar, unchanged behavior).
    ("category", "Category"), ("categoryCooldown", "CategoryRecoveryTime"),
    # SpellVisualID_1 — the client's cast/impact animation kit. From-scratch effect-carrier clones
    # default to 0 (no animation); a triggered damage/heal clone the client renders (e.g. the Vanilla
    # Holy Shock dmg/heal 932649-654) needs the stock spell's visual (Holy Shock dmg=128, heal=135)
    # so the impact plays. Client-only in effect (the server ignores it); harmless on the server row.
    ("spellVisual", "SpellVisualID_1"),
    ("durationIndex", "DurationIndex"), ("mechanic", "Mechanic"), ("family", "SpellClassSet"),
    ("mask", "SpellClassMask_1"), ("maskB", "SpellClassMask_2"), ("maskC", "SpellClassMask_3"),
    # `mask`/`maskB`/`maskC` set SpellClassMask_1/2/3 (family flag words A/B/C). `mask` (word A) is the
    # legacy key; `maskB`/`maskC` were added for the Paladin Seal of the Crusader reconstruction (node
    # 18633): every family-10 word-A bit is already used by a stock paladin spell, so the SotC seal +
    # Judgement of the Crusader debuff carry a FREE word-C bit (512 = bit9) that Improved SotC's spellmod
    # binds to via `affectsMask: {c: 512}` without over-binding any stock seal. On a templated helper an
    # unset word inherits the clone's value; on a from-scratch helper it defaults to 0 like every scalar.
    ("school", "SchoolMask"), ("stackAmount", "CumulativeAura"),
    # Weapon/armor proficiency + equip-gate columns (the DBC EquippedItemClass +
    # EquippedItemSubclass = SpellInfo::EquippedItemSubClassMask). Needed by the Shaman "Two-Handed
    # Axes and Maces" proficiency clone 932406: SPELL_EFFECT_PROFICIENCY (60) reads these to grant
    # proficiency (SpellEffects.cpp:2319), and a template clone of stock 197 inherits only the 2H-axe
    # subclass — this widens it to axe2|mace2 (34). Mirrors the node-level equipClass/equipSubclass
    # gate keys (same columns). On a templated helper an unset key inherits the clone's value; on a
    # from-scratch helper equipClass would otherwise be forced to the -1 default below.
    ("equipClass", "EquippedItemClass"), ("equipSubclass", "EquippedItemSubclass"),
    # Percentage-of-base-mana cost (the DBC ManaCostPct column). Needed by clones whose era cost
    # diverges in the PCT field (Vanilla Bestial Wrath 932967: 12% vs stock 19574's 10%) — the
    # flat `powerCost`/ManaCost column can't express it. Only meaningful on templated helpers
    # (legacy helpers default it to 0 like every unlisted scalar, unchanged behavior).
    ("manaCostPct", "ManaCostPct"),
    # Proc-charge count (the DBC ProcCharges column). Needed by clones whose era debuff carries a
    # different charge count than the flat default — the Vanilla Hemorrhage clone 932985 applies a
    # 30-charge debuff vs stock 16511's 10. Only meaningful on templated/authored helpers (legacy
    # helpers default it to 0 like every unlisted scalar, unchanged behavior).
    ("procCharges", "ProcCharges"),
    # Level bounds (the DBC SpellLevel/BaseLevel/MaxLevel columns) — needed by clones that carry
    # per-level effect scaling (see the effect-level `perLevel` key): core CalcValue adds
    # EffectRealPointsPerLevel * (min(casterLevel, MaxLevel) - SpellLevel), so the bounds are
    # load-bearing for the growth window. First used by the TBC Judgement of Command chain
    # (946103-108, wago 2.5.4 rendering). On a templated helper an unset key inherits the clone's
    # value; on a from-scratch helper it defaults to 0 (= no level gate / no growth cap base).
    ("spellLevel", "SpellLevel"), ("baseLevel", "BaseLevel"), ("maxLevel", "MaxLevel"),
    # Base proc chance (the DBC ProcChance column). Needed by a clone whose era proc fires at a
    # different DBC chance than the stock spell — the Vanilla Druid Nature's Grasp clone 932505 is a
    # 35%-chance/1-charge self-buff vs stock 16689's 100%/3-charge, so Improved Nature's Grasp
    # (op=procChance) has headroom to raise it toward 100% (the core adds the spellmod to a DBC
    # aura-42 proc's chance in Aura::CalcProcChance via GetSpellModOwner). Only meaningful on
    # templated/authored helpers (legacy helpers default it to 0 like every unlisted scalar; an
    # aura-42 proc with no spell_proc row falls back to this DBC column for its base chance).
    ("procChance", "ProcChance"),
    # DBC proc EVENT mask (the ProcTypeMask column) — which events an aura-42 PROC_TRIGGER_SPELL
    # listens to when its `spell_proc` row leaves ProcFlags 0 ("use the spell's own DBC events").
    # Needed by the TBC druid Omen of Clarity clone 946269: TBC 16864 is ProcTypeMask 20
    # (DONE_MELEE_AUTO_ATTACK 0x4 | DONE_SPELL_MELEE_DMG_CLASS 0x10 = melee only) where live 16864
    # is 81924 (melee + magic spell hits), so a template clone would proc off spellcasts too. Only
    # meaningful on templated/authored helpers (legacy helpers default it to 0 like every unlisted
    # scalar, which is what the un-keyed emission already produced — byte-neutral for shipped data).
    ("procTypeMask", "ProcTypeMask"),
    # CasterAuraSpell — the DBC "the caster must already hold this aura" gate. `Spell::CheckCast`
    # (Spell.cpp:5779) fails the cast with SPELL_FAILED_CASTER_AURASTATE when
    # `m_spellInfo->CasterAuraSpell && !m_caster->HasAura(...)`. Needed by the TBC Warrior Rampage
    # clones (947533-947535): TBC gates the ability on `CasterAuraState 11` ("scored a critical hit
    # recently"), a state AzerothCore's AuraStateType does not define and nothing in
    # src/server/game ever sets, so copying it literally would make the clone permanently
    # uncastable (the Mangle-DUMMY lesson). The era route reproduces the 5 s window as a REAL aura
    # applied by a crit-only proc passive and points this column at it. Only meaningful on
    # templated/authored helpers (legacy helpers default it to 0 like every unlisted scalar, which
    # is what the un-keyed emission already produced — byte-neutral for shipped data).
    ("casterAuraSpell", "CasterAuraSpell"),
    # TargetCreatureType — the DBC creature-type GATE (a bitmask of CREATURE_TYPE_* as
    # 1 << (type - 1): 64 = Humanoid, 1 = Beast, ...). `SpellInfo::CheckTarget` refuses a unit
    # whose creature type is outside the mask, so a clone whose era record only works on a
    # narrower victim set than its WotLK template needs it authored — a templated clone otherwise
    # inherits the stock (usually widened) value. Only meaningful on templated/authored helpers
    # (legacy helpers default it to 0 like every unlisted scalar, and 0 = no creature-type gate —
    # byte-neutral for shipped data).
    ("targetCreatureType", "TargetCreatureType"),
    # DispelType — the dispel CATEGORY (DISPEL_MAGIC/ENRAGE/...). Needed by a clone whose era
    # record is in a different dispel class than the stock template it clones. Motivating case:
    # the TBC Warrior Death Wish clone 947548 (era-data/_ref/tbc/warrior-spells.yaml accepted_gaps
    # id 5) — wago 2.5.4 12292 carries DispelType 0, live 12292 carries 9 (DISPEL_ENRAGE) because
    # WotLK made Death Wish an Enrage effect, so a templated clone inherited 9 and stayed removable
    # by an Enrage dispel that TBC's own record does not allow. Note `mechanic:` (the spell-level
    # MECHANIC_*) is a SEPARATE column and does not imply this one. Only meaningful on templated/
    # authored helpers (legacy helpers default it to 0 like every unlisted scalar — byte-neutral
    # for shipped data).
    ("dispelType", "DispelType"),
    # AuraInterruptFlags — the AURA_INTERRUPT_FLAG_* bitmask (which player actions break the aura
    # this spell applies). `Unit::RemoveAurasWithInterruptFlags` is the reader; the core also copies
    # it into `AuraApplication`'s effect mask at apply time, so it is per-SPELL, not per-effect, and
    # a templated clone inherits the WotLK value wholesale. The case that added it: the TBC hunter
    # Scatter Shot clone 948382 — wago 2.5.4 19503 carries 0x2 (AURA_INTERRUPT_FLAG_DAMAGE, i.e.
    # "any damage caused will remove the effect", exactly what TBC's tooltip promises) where live
    # 19503 carries 0x480002, WotLK having added TAKE_DAMAGE-family bits that break the disorient on
    # events TBC's own record does not list (era-data/_ref/tbc/hunter-spells.yaml line 3084:
    # "L:AuraInterruptFlags TBC0x2->LIVE0x480002" — the same divergence the whole Wyvern Sting
    # chain and Freezing Trap Effect carry). Only meaningful on templated/authored helpers (legacy
    # helpers default it to 0 like every unlisted scalar, which is what the un-keyed emission
    # already produced — byte-neutral for shipped data).
    ("auraInterruptFlags", "AuraInterruptFlags"),
]

# Helper keys that are LINT/AUDIT ONLY — read by tools/era_audit.py, never emitted as a spell_dbc
# column, and deliberately NOT in _HELPER_SCALARS. `helper_overrides` only ever reads the keys it
# knows about, so an unlisted key is silently ignored by the generator; this list exists so that
# "why does this key emit nothing?" has an answer in the generator rather than only in the audit.
#   * `spellFixAck` — acknowledges a `SpellInfoCorrections.cpp` by-id fix that this clone cannot
#     inherit and that is NOT reproducible through an attribute key (an `Effects[...]`/target
#     rewrite), or that the clone genuinely does not need. See era_audit.check_spellfix_inheritance.
_HELPER_LINT_ONLY_KEYS = ("spellFixAck",)


def helper_overrides(h, templated: bool = False) -> dict:
    """Column overrides for a fixed-id HELPER spell a module C++ script casts by id (not a
    talent-rank passive). Exposed (public) so tools/build_client_dbc.py builds the client row from
    the SAME fields. Effect shapes:
      * single-effect (legacy): top-level `aura`/`basePoints`/`misc`/`targetA` -> Effect_1.
      * multi-effect / castable: an `effects:` list (each {effect,aura,basePoints,misc,miscB,
        trigger,targetA,dieSides}) -> Effect_1..3, plus castable metadata (powerCost/rangeIndex/
        castTimeIndex/cooldown).

    `templated=True` (helper has a `template:` id): emit ONLY authored fields, so everything else is
    inherited from the cloned real record (SpellVisual/attrs/cast flags). The legacy path (default)
    force-fills defaults (SpellIconID=1, EquippedItemClass=-1, cast-time index 1, ...) because there
    is no template to inherit from. Used by PI (920920) and VE (921152/932950/932951)."""
    over = {"ID": int(h["id"]), "Name_Lang_enUS": h["name"], "Name_Lang_Mask": 16712190}

    for key, col in _HELPER_SCALARS:
        if key in h:
            over[col] = int(h[key])
        elif not templated:
            over[col] = int(h.get(key, {"castTimeIndex": 1, "rangeIndex": 1}.get(key, 0)))

    if not templated:
        over.update({"SpellIconID": 1, "SpellPriority": 50, "EquippedItemClass": -1})
    if "desc" in h or not templated:
        over["Description_Lang_enUS"] = h.get("desc", "")
        over["Description_Lang_Mask"] = 16712190

    effects = h.get("effects")
    if effects:
        # Templated: write all 3 slots (0-fill absent ones) so the template's own effects can't leak
        # through. Legacy: only the authored slots.
        slots = 3 if templated else len(effects)
        for i in range(1, slots + 1):
            e = effects[i - 1] if i - 1 < len(effects) else {}
            over[f"Effect_{i}"] = int(e.get("effect", 0))
            over[f"EffectDieSides_{i}"] = int(e.get("dieSides", 0))
            over[f"EffectBasePoints_{i}"] = int(e.get("basePoints", 0))
            over[f"EffectAura_{i}"] = int(e.get("aura", 0))
            over[f"EffectMiscValue_{i}"] = int(e.get("misc", 0))
            over[f"ImplicitTargetA_{i}"] = int(e.get("targetA", 1)) if e else 0
            # Per-effect secondary target (e.g. TARGET_UNIT_SRC_AREA_ENEMY=15 paired with a
            # TARGET_SRC_CASTER=22 source-setter, the Magma Totem Pulse 8187 shape) — written
            # ONLY when present (like miscB/radius/itemType/amplitude), so a templated helper
            # keeps INHERITING the cloned record's ImplicitTargetB for any effect that omits it.
            if e.get("targetB") is not None:
                over[f"ImplicitTargetB_{i}"] = int(e["targetB"])
            if e.get("miscB") is not None or templated:
                over[f"EffectMiscValueB_{i}"] = int(e.get("miscB", 0))
            if e.get("trigger") is not None or templated:
                over[f"EffectTriggerSpell_{i}"] = int(e.get("trigger", 0))
            # Area-aura effects (119 APPLY_AREA_AURA_PET / 143 ..._OWNER) need a radius or the
            # owner is never "within range" (stock MD buffs 23759-62 use index 12 = 100 yd).
            if e.get("radius") is not None:
                over[f"EffectRadiusIndex_{i}"] = int(e["radius"])
            # SPELL_EFFECT_CREATE_ITEM (24) stores the conjured item id in EffectItemType
            # (used by the Vanilla Create Firestone/Spellstone clones to retarget the old items).
            if e.get("itemType") is not None or (templated and e.get("effect") == 24):
                over[f"EffectItemType_{i}"] = int(e.get("itemType", 0))
            # Periodic-aura tick period (ms) — written ONLY when present (like radius/itemType),
            # so a templated helper keeps INHERITING the cloned record's period when omitted.
            # Needed because core's AuraEffect::CalculatePeriodic broken-dbc guard defaults an
            # unset/zero EffectAuraPeriod to 1000ms, silently doubling a custom periodic aura's
            # tick rate if the template's own period isn't inherited or explicitly set here.
            if e.get("amplitude") is not None:
                over[f"EffectAuraPeriod_{i}"] = int(e["amplitude"])
            # Per-effect MECHANIC_* (EffectMechanic) — written ONLY when present, so a templated
            # helper keeps INHERITING the cloned record's mechanic for any effect that omits it.
            # The core reads it for mechanic IMMUNITY (Unit::IsImmunedToSpellEffect, per effect
            # index) and for the diminishing-returns group
            # (SpellMgr::GetDiminishingReturnsGroupForSpell -> DIMINISHING_DISARM off
            # MECHANIC_DISARM, DIMINISHING_STUN off MECHANIC_STUN, ...), both of which read
            # SpellInfo::GetAllEffectsMechanicMask(), i.e. the per-effect column and NOT only the
            # spell-level `mechanic:` scalar. Needed whenever a clone re-authors a slot whose
            # template carries a DIFFERENT mechanic: the TBC rogue Riposte clone 947777 replaces
            # live 14251's Effect_2 (aura 138 MOD_MELEE_HASTE, EffectMechanic 8 = SLOW_ATTACK) with
            # TBC's aura 67 MOD_DISARM, which must read EffectMechanic 3 = DISARM — inheriting 8
            # would silently test a disarm-immune target for slow-attack immunity instead, and put
            # the disarm in the wrong DR group. Distinct from the spell-level `mechanic:` scalar
            # (_HELPER_SCALARS), which applies to the WHOLE spell and would also make a
            # disarm-immune target immune to the clone's damage half.
            if e.get("mechanic") is not None:
                over[f"EffectMechanic_{i}"] = int(e["mechanic"])
            # Per-level effect growth (EffectRealPointsPerLevel, a FLOAT column) — written ONLY when
            # present. Pair it with the helper-level spellLevel/baseLevel/maxLevel scalars or the
            # growth window is unbounded below / never capped. First used by the TBC Judgement of
            # Command chain (halved wago-2.5.4 RealPointsPerLevel, per the normal-hit halving rule).
            if e.get("perLevel") is not None:
                over[f"EffectRealPointsPerLevel_{i}"] = float(e["perLevel"])
            # Per-effect spell-class mask (which spells THIS effect's spellmod affects) — written
            # ONLY when the key is present, so a templated helper keeps INHERITING the cloned
            # record's per-effect masks for any effect that omits them (no 0-fill). Lets a single
            # helper carry two spellmod effects each bound to a distinct spell set — e.g. the
            # Vanilla Elemental Mastery 932413 (Effect_1 crit + Effect_2 cost, both on the 5-spell
            # fire/frost/nature damage set). Column convention (see MASK_PREFIX + the node emitter
            # at _spellmod_row): the LETTER = effect index (A=Effect_1, B=Effect_2, C=Effect_3) and
            # the _1/_2/_3 suffix = family word0/1/2. So `maskA`/`maskB`/`maskC` mean word0/1/2 (the
            # SAME meaning as `affectsMask {a,b,c}` and the node path), written under THIS effect's
            # letter MASK_PREFIX[i]. This is distinct from the helper-level `mask`/`maskB`/`maskC`
            # scalars, which set the spell's OWN family flags (SpellClassMask_1/2/3), not an effect mask.
            mletter = MASK_PREFIX[i]
            for widx, mkey in ((1, "maskA"), (2, "maskB"), (3, "maskC")):
                if e.get(mkey) is not None:
                    over[f"{mletter}_{widx}"] = int(e[mkey])
    elif not templated:
        over.update({
            "Effect_1": 6, "EffectDieSides_1": 0,
            "EffectBasePoints_1": int(h.get("basePoints", 0)),
            "EffectAura_1": int(h["aura"]),
            "EffectMiscValue_1": int(h.get("misc", 0)),
            "ImplicitTargetA_1": int(h.get("targetA", 1)),
        })
    # Optional top-level `stances:` form-restriction (Druid form-conditional helper clones).
    _apply_stances(over, h)
    return over


def _helper_row(h, templates: dict | None = None):
    tid = h.get("template")
    if tid is None:
        return _row(helper_overrides(h))
    if not templates or int(tid) not in templates:
        raise SystemExit(f"helper {h['id']}: template spell {tid} not found in base Spell.dbc")
    return _row(helper_overrides(h, templated=True), templates[int(tid)])


# POSITIONAL INSERT (no column list) — the ONE emitter here that relies on column ORDER. It is a
# byte-verbatim clone of stock totem 5873's creature_template row (55 columns, schema order), so it
# self-documents the layout and can only be trusted while the schema matches the pinned WotLK core.
# A creature_template column add/reorder would SILENTLY mis-map this row; the 55-column-count test
# (test_totem_creature_template_column_count) is the guard that fails loudly if the count drifts.
# If that test ever fails, re-clone this row from the current base creature_template.sql — do not
# hand-patch it. (type=11 CREATURE_TYPE_TOTEM @idx32, name @idx6, HealthModifier 0.05, faction 58.)
_TOTEM_CT_ROW = ("({entry},0,0,0,0,0,'{name}',NULL,NULL,0,1,80,0,58,0,1,1,1,1,18,0,0,1,"
                 "2000,2000,1,1,1,0,2048,0,0,11,0,0,0,0,0,0,0,0,'',0,1,0.05,1,1,1,0,0,1,0,0,"
                 "'{script}',12340)")
_TOTEM_DEFAULT_MODEL = 4588                # displayid from stock 5873; runtime GetModelForTotem
                                           # overrides it per totem-slot+race for player totems.


def _totem_rows(d) -> list:
    """Emit creature_template / _model / _spell rows for each `totems:` entry (creature-only;
    the summon + pulse are ordinary `helpers:`)."""
    lines = []
    for t in d.get("totems", []) or []:
        entry = int(t["creature"])
        name = str(t["name"]).replace("'", "''")   # SQL-escape single quotes
        script = str(t.get("script", "")).replace("'", "''")   # SQL-escape single quotes
        lines.append(f"DELETE FROM `creature_template` WHERE `entry`={entry};")
        lines.append("INSERT INTO `creature_template` VALUES\n"
                     + _TOTEM_CT_ROW.format(entry=entry, name=name, script=script) + ";")
        model = int(t.get("model", _TOTEM_DEFAULT_MODEL))
        lines.append(f"DELETE FROM `creature_template_model` WHERE `CreatureID`={entry};")
        lines.append("INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, "
                     "`CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`) "
                     f"VALUES\n({entry}, 0, {model}, 1, 1, 12340);")
        spell_rows = [f"({entry}, 0, {int(t['pulse'])}, 12340)"]
        if t.get("pulse2") is not None:
            spell_rows.append(f"({entry}, 1, {int(t['pulse2'])}, 12340)")
        lines.append(f"DELETE FROM `creature_template_spell` WHERE `CreatureID`={entry};")
        lines.append("INSERT INTO `creature_template_spell` (`CreatureID`, `Index`, `Spell`, "
                     "`VerifiedBuild`) VALUES\n" + ",\n".join(spell_rows) + ";")
    return lines


def emit_custom_sql(d, base_dbc=DEFAULT_BASE_DBC) -> str:
    lines = ["-- generated by tools/gen_era_talents.py --custom-sql — DO NOT EDIT"]
    cols = "(`" + "`, `".join(COLS) + "`)"
    helpers = d.get("helpers", []) or []
    # Load template rows only when a helper opts into clone-and-override (`template:`); decoding the
    # base Spell.dbc is a few seconds so datasets that don't use it pay nothing.
    templates = read_dbc_templates(base_dbc) if any("template" in h for h in helpers) else None
    for h in helpers:
        hid = int(h["id"])
        lines.append(f"DELETE FROM `spell_dbc` WHERE `ID`={hid};")
        lines.append(f"INSERT INTO `spell_dbc` {cols} VALUES\n{_helper_row(h, templates)};")
        if h.get("proc"):
            chance = h["proc"].get("chance", DEFAULT_PROC_CHANCE)
            proc_cols, proc_row = _spell_proc_row_for(d, hid, h["proc"], chance)
            proc_cols_sql = "(`" + "`, `".join(proc_cols) + "`)"
            lines.append(f"DELETE FROM `spell_proc` WHERE `SpellId`={hid};")
            lines.append(f"INSERT INTO `spell_proc` {proc_cols_sql} VALUES\n{proc_row};")
        # `singleAuraStack: true` = SPELL_ATTR0_CU_SINGLE_AURA_STACK (0x00400000) via the core's
        # spell_custom_attr table. Required for any SHARED cross-caster stacking debuff (the core
        # hardcodes it for Sunder Armor / Winter's Chill): without it each caster owns a separate
        # aura and Aura::CanStackWith REPLACES the rival's instead of incrementing one shared stack.
        if h.get("singleAuraStack"):
            lines.append(f"DELETE FROM `spell_custom_attr` WHERE `spell_id`={hid};")
            lines.append("INSERT INTO `spell_custom_attr` (`spell_id`, `attributes`) VALUES\n"
                         f"({hid}, 4194304);")
    # Bind a module SpellScript/AuraScript to STOCK spells the generator doesn't own (e.g. attach
    # era_ward_reflect to Fire/Frost Ward). A NEGATIVE spell id binds to every rank of that chain.
    #
    # DELETE semantics differ by id class (2026-09-02, found when rebinding Nature's Guardian):
    #   * BAND ids ([920000,950000)) are wholly module-owned, so the file owns ALL their bindings —
    #     one bare per-id DELETE (emitted before that id's first insert) makes a REBIND converge on
    #     a live DB. The old (id,script)-qualified delete leaked the previous script's row forever:
    #     both AuraScripts then attach (Nature's Guardian would have double-healed/double-threat).
    #     Multi-script ids (Earth Shield: spell_sha_earth_shield + isAllowedToCastSpell) still work
    #     because the delete runs once per id, then every insert follows.
    #   * STOCK / negative ids keep the (id,script)-qualified delete — a bare delete there would
    #     nuke the core's own legitimate bindings on that spell.
    _band_deleted: set = set()
    for b in d.get("scriptBindings", []) or []:
        sid, script = int(b["spell"]), str(b["script"])
        if 920000 <= sid < 950000:
            if sid not in _band_deleted:
                lines.append(f"DELETE FROM `spell_script_names` WHERE `spell_id`={sid};")
                _band_deleted.add(sid)
        else:
            lines.append(f"DELETE FROM `spell_script_names` WHERE `spell_id`={sid} AND `ScriptName`='{script}';")
        lines.append("INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES\n"
                     f"({sid}, '{script}');")
    for _tab, n in _nodes(d):
        mech = n.get("mechanic")
        if mech not in ("spellmod", "stat", "proc", "proc-scripted", "scripted", "multi"):
            continue
        for rank in range(1, int(n["maxRank"]) + 1):
            pid = custom_passive_id(n["id"], rank)
            tip = (n.get("tooltip") or [""] * rank)[rank - 1]
            if mech in ("proc", "proc-scripted", "scripted"):
                row = _proc_passive_row(d, n, rank, tip)
            elif mech == "multi":
                row = _multi_row(d, n, rank, tip)
            else:
                eff = int(n["vals"][rank - 1])
                row = _spellmod_row(d, n, rank, eff, tip) if mech == "spellmod" \
                    else _stat_row(d, n, rank, eff, tip)
            # DELETE-before-INSERT on every table so the file is idempotent: it is applied by
            # db-import on any checksum change AND may be hand-applied during iteration. Without
            # the spell_proc / spell_script_names deletes, a second apply hits a duplicate PK and
            # FAILS db-import (which blocks worldserver startup).
            lines.append(f"DELETE FROM `spell_dbc` WHERE `ID`={pid};")
            lines.append(f"INSERT INTO `spell_dbc` {cols} VALUES\n{row};")
            if mech in ("proc", "proc-scripted"):
                proc_cols, proc_row = _spell_proc_row(d, n, rank)
                proc_cols_sql = "(`" + "`, `".join(proc_cols) + "`)"
                lines.append(f"DELETE FROM `spell_proc` WHERE `SpellId`={pid};")
                lines.append(f"INSERT INTO `spell_proc` {proc_cols_sql} VALUES\n{proc_row};")
            # A spellmod node with a `procPassive:` block adds Effect_2 aura-42 PROC_TRIGGER_SPELL to
            # its passive (see _spellmod_row); emit the matching spell_proc row so the trigger fires.
            if mech == "spellmod" and n.get("procPassive"):
                pp = n["procPassive"]
                chance = pp.get("chance", DEFAULT_PROC_CHANCE)
                ch = int(chance[rank - 1]) if isinstance(chance, list) else int(chance)
                proc_cols, proc_row = _spell_proc_row_for(d, pid, pp, ch)
                proc_cols_sql = "(`" + "`, `".join(proc_cols) + "`)"
                lines.append(f"DELETE FROM `spell_proc` WHERE `SpellId`={pid};")
                lines.append(f"INSERT INTO `spell_proc` {proc_cols_sql} VALUES\n{proc_row};")
            if mech == "proc-scripted":
                lines.append(f"DELETE FROM `spell_script_names` WHERE `spell_id`={pid};")
                lines.append(
                    "INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES\n"
                    f"({pid}, '{n['script']}');")
    # `pet:` nodes (Warlock demon talents, Hunter pet talents, ...). Per rank emit the DUMMY talent
    # passive (the spell_pet_auras `spell`, required to carry a dummy effect at eff0) plus EITHER
    # the auto-derived pet-buff spell(s) the demon/pet casts on itself, OR — for a `petBuffIds:`
    # node — a mapping straight to the dataset's hand-authored carrier helper(s), one per rank (no
    # auto pet-buff row is emitted for those ranks: the helper IS the buff spell already). Either
    # way a spell_pet_auras row maps passive->buff per (rank, pet entry); spell_pet_auras keys on
    # (spell, effectId): the passive's DUMMY lives at effect index 0.
    pet_ordinals = _pet_node_ordinals(d)
    pet_base = int(d.get("petBuffBase", PET_BUFF_BASE))
    for _tab, n in _nodes(d):
        if n.get("mechanic") != "pet":
            continue
        ordinal = pet_ordinals[int(n["id"])]
        max_rank = int(n["maxRank"])
        buff_ids = n.get("petBuffIds")
        for rank in range(1, max_rank + 1):
            pid = custom_passive_id(n["id"], rank)
            tip = (n.get("tooltip") or [""] * rank)[rank - 1]
            lines.append(f"DELETE FROM `spell_dbc` WHERE `ID`={pid};")
            lines.append(f"INSERT INTO `spell_dbc` {cols} VALUES\n"
                         f"{_pet_passive_row(d, n, rank, tip)};")
            if buff_ids is not None:
                pet_entry = int(n.get("pet", 0))
                pet_rows = [f"({pid}, 0, {pet_entry}, {int(buff_ids[rank - 1])})"]
            else:
                pet_rows = []
                for vi, (pet_entry, spec) in enumerate(_pet_bindings(n)):
                    buff_id = pet_buff_id(pet_base, ordinal, rank, vi, max_rank)
                    lines.append(f"DELETE FROM `spell_dbc` WHERE `ID`={buff_id};")
                    lines.append(f"INSERT INTO `spell_dbc` {cols} VALUES\n"
                                 f"{_pet_buff_row(spec, buff_id, rank, tip, n['name'])};")
                    pet_rows.append(f"({pid}, 0, {pet_entry}, {buff_id})")
            # DELETE-before-INSERT keeps a re-apply idempotent (db-import fails on a duplicate PK).
            lines.append(f"DELETE FROM `spell_pet_auras` WHERE `spell`={pid};")
            lines.append(
                "INSERT INTO `spell_pet_auras` (`spell`, `effectId`, `pet`, `aura`) VALUES\n"
                + ",\n".join(pet_rows) + ";")
    lines.extend(_totem_rows(d))
    return "\n".join(lines) + "\n"


SENTINEL_SPELL_ID = 932999   # reserved top-of-band id for the generation-canary client spell;
                             # never author it (lint pre-seeds it into the collision map)


# Files whose bytes shape every emitted artifact: a change to ANY of them is a new
# generation even with untouched YAMLs (Tasks 1-2 changed emitted SQL yaml-free).
_STAMP_TOOL_FILES = ("gen_era_talents.py", "build_client_dbc.py",
                     "spell_dbc_columns.txt", "spell_dbc_coltypes.txt")


def generation_stamp(dataset_paths) -> str:
    """8-hex content hash over the dataset YAMLs PLUS the generator's own inputs
    (_STAMP_TOOL_FILES). This is THE generation identity: it ships to the world DB
    (era_talent_meta, via --meta-sql) and to the client MPQ (sentinel spell 932999's name, via
    build_client_dbc --stamp); the addon compares the two and warns on drift.

    Datasets are sorted AND salted by their last two path components (parent dir + basename), so
    argument order and directory layout don't matter but two same-basename files under different
    era dirs (era-data/vanilla/mage.yaml vs era-data/tbc/mage.yaml) can't alias into the same hash
    input. The tool files matter because artifacts are f(YAML, generator): a generator-only change
    (e.g. a new SpellModOp, a column-table fix) regenerates server SQL + client MPQ while a
    player's stale installed MPQ still stamp-matches on YAML-only hashing — a silent canary false
    negative. The trade-off is deliberate: a harmless tool-comment edit now also bumps the stamp
    (an "update your patch" false positive), which is far cheaper than the false negative it
    replaces."""
    import hashlib
    h = hashlib.sha1()
    for name in _STAMP_TOOL_FILES:
        h.update((pathlib.Path(__file__).parent / name).read_bytes())
    for p in sorted(dataset_paths, key=lambda x: (pathlib.Path(x).parent.name, pathlib.Path(x).name)):
        pp = pathlib.Path(p)
        h.update(f"{pp.parent.name}/{pp.name}\n".encode())
        h.update(pp.read_bytes())
    return h.hexdigest()[:8]


def emit_meta_sql(dataset_paths) -> str:
    stamp = generation_stamp(dataset_paths)
    return ("-- generated by tools/gen_era_talents.py --meta-sql — DO NOT EDIT\n"
            "DELETE FROM `era_talent_meta` WHERE `k`='generation';\n"
            f"INSERT INTO `era_talent_meta` (`k`, `v`) VALUES ('generation', '{stamp}');\n")


def _load(dataset_path: str) -> dict:
    """Load a dataset YAML and resolve `_ref`: an inline `_ref:` dict wins (used by unit tests);
    otherwise a top-level `ref: <relpath>` string is loaded relative to the dataset file's dir."""
    p = pathlib.Path(dataset_path)
    d = yaml.safe_load(p.read_text())
    if "_ref" not in d and d.get("ref"):
        ref_path = (p.parent / d["ref"]).resolve()
        d["_ref"] = yaml.safe_load(ref_path.read_text())
    return d


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--sql", metavar="YAML")
    ap.add_argument("--lua", metavar="YAML")
    ap.add_argument("--custom-sql", metavar="YAML")
    ap.add_argument("--meta-sql", nargs="+", metavar="YAML",
                    help="ALL dataset yamls; emits the era_talent_meta generation-stamp row")
    ap.add_argument("-o", "--out", required=True)
    a = ap.parse_args()
    if a.meta_sql:
        pathlib.Path(a.out).write_text(emit_meta_sql(a.meta_sql))
        return
    src = a.sql or a.lua or a.custom_sql
    d = _load(src)
    lint(d)
    if a.sql:
        out = emit_sql(d)
    elif a.lua:
        out = emit_lua(d)
    else:
        out = emit_custom_sql(d)
    pathlib.Path(a.out).write_text(out)


if __name__ == "__main__":
    main()
