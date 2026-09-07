#!/usr/bin/env python3
"""Mechanical audit of the era-talents generated spell data (see docs/era-talents-framework.md).

Runs AFTER lint/generation, over the exact rows the generator emits — a belt-and-suspenders
REPORT layer on classes of defect that already bit us once each:

  1. Proc sanity: every generated `spell_proc` row (helper procs + per-rank node procs, mirroring
     emit_custom_sql's iteration). Reports HitMask == 8 (PROC_HIT_FULL_RESIST only — on a DONE proc it can
     literally never fire on a landed hit; the old generator DEFAULT, which silently broke VE.
     TAKEN-only procs are exempt: full-resist-only is Magic Absorption's real contract) and
     any SpellTypeMask/SpellPhaseMask/HitMask that is non-zero while the ProcFlags applicability
     masks say the core ignores the field (LoadSpellProcs "won't be used").
  1b. Proc DAMAGE-typeMask reachability: a hit-class proc row whose SpellTypeMask includes
     DAMAGE (1) but not NO_DMG_HEAL (4) while NONE of its `affects:`-named spells has a damage
     effect in the base Spell.dbc — the hit event computes SpellTypeMask NO_DMG_HEAL and the
     proc can never fire (the Improved Wing Clip 18336 bug, caught in review of fcf5bce).
     Direct-cast procs only: attrMask-2 (TRIGGERED_CAN_PROC) rows are skipped, because their
     event spell is a triggered family-mask sibling, not the named base row (Improved Blizzard).
  2. AuraDescription leaks: a helper with a `client:` block cloning a `template:` whose
     AuraDescription is non-empty, whose effective effects apply an aura (effect 6), and which
     authors no `client.auraDescription` — the buff tooltip would show the stock WotLK text
     (the custom-PI "haste" tooltip bug). Report-mode duplicate of build_client_dbc's hard error.
  3. Corrections-sweep collisions: the core's SpellInfoCorrections.cpp ends in a pattern-matching
     sweep over EVERY loaded spell (fork file, ~line 5245) that rewrites SpellInfo by
     family/icon/effect PATTERNS — not spell id — AFTER our spell_dbc rows load. A custom row
     matching a predicate is silently rewritten. Every generated spell_dbc row is run through a
     transcription of each predicate our rows could possibly match.

Usage:
    uv run --with pyyaml python tools/era_audit.py [yamls...]

Defaults to every dataset listed in era-data/datasets.txt (the manifest) — see DEFAULT_DATASETS.
Exit 0 = clean, 1 = findings (each printed on its own indented line).
"""
from __future__ import annotations
import importlib.util, pathlib, re, sys

_TOOLS = pathlib.Path(__file__).resolve().parent
_spec = importlib.util.spec_from_file_location("gen_era_talents", _TOOLS / "gen_era_talents.py")
G = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(G)

IDX = {c: i for i, c in enumerate(G.COLS)}

# The dataset list self-derives from the manifest (era-data/datasets.txt) — the same source
# era-regen.sh reads — so a newly-wired dataset can never be silently skipped by a bare
# `era_audit.py` run (warlock once shipped that way).
def _load_manifest_datasets():
    """Parse era-data/datasets.txt, validating exactly like era-regen.sh's awk pre-check and
    the pytest helper (_regen_datasets): every non-comment/non-blank line must have 3
    whitespace-separated fields, and no dataset path may repeat. era_audit.py is the
    "invoked BARE everywhere" backstop for a missing/miskeyed dataset, so under-validating this
    parse is the single worst place to do it — raise loudly rather than silently truncate."""
    out, seen = [], set()
    for line in (_TOOLS.parent / "era-data" / "datasets.txt").read_text().splitlines():
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        parts = line.split()
        if len(parts) != 3:
            raise ValueError(f"era-data/datasets.txt: bad line (need 3 fields): {line!r}")
        rel = parts[0]
        if rel in seen:
            raise ValueError(f"era-data/datasets.txt: duplicate dataset {rel}")
        seen.add(rel)
        out.append(str(_TOOLS.parent / "era-data" / rel))
    return out


DEFAULT_DATASETS = _load_manifest_datasets()


def _parse_values(row: str):
    """Split a '(v1, v2, ...)' VALUES tuple string into fields, respecting single-quoted strings.
    Copied from tools/test_gen_era_talents.py::_parse_values (same logic, attributed here) so the
    audit parses emitted SQL exactly the way the test suite does."""
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


def _typed(s: str):
    """One parsed SQL field -> python value (str / int / float).
    _parse_values already converts '' -> ' while a string is open, so the field is unescaped by
    the time it gets here — re-unescaping would corrupt any value with consecutive literal quotes."""
    if s.startswith("'"):
        return s[1:-1]
    try:
        return int(s)
    except ValueError:
        return float(s)


# ---------------------------------------------------------------------------
# Check 1 — proc sanity
# ---------------------------------------------------------------------------

def _audit_proc_row(label: str, cols, row: str, findings: list):
    vals = dict(zip(cols, (int(v) for v in _parse_values(row))))
    flags = vals["ProcFlags"]
    # HitMask 8 = PROC_HIT_FULL_RESIST only. On a DONE proc that is a proc that never fires on a
    # landed hit (the old generator DEFAULT that silently broke VE). On a TAKEN-only proc it is the
    # intended contract: Unit.cpp raises PROC_HIT_FULL_RESIST on HITINFO_FULL_RESIST / SPELL_MISS_RESIST,
    # and Magic Absorption (live spell_proc -29441; era clones 932326-932330 / 948762-948766) restores
    # mana precisely and only then. Exempt TAKEN-only rows (TBC Phase 9, 2026-09-05).
    taken_only = bool(flags & G._TAKEN_HIT_PROC_FLAG_MASK) and not (flags & G._DONE_HIT_PROC_FLAG_MASK)
    if vals["HitMask"] == 8 and not taken_only:
        findings.append(f"{label}: HitMask == 8 (PROC_HIT_FULL_RESIST only — never procs on a DONE proc)")
    # Same applicability predicates the core's LoadSpellProcs warns on ("won't be used") and
    # _spell_proc_row_for zeroes/rejects at generation time — re-checked here independently.
    usable = {
        "SpellTypeMask": bool(flags & (G._SPELL_PROC_FLAG_MASK | G._PERIODIC_PROC_FLAG_MASK)),
        "SpellPhaseMask": bool(flags & G._REQ_SPELL_PHASE_PROC_FLAG_MASK),
        "HitMask": bool(flags & (G._TAKEN_HIT_PROC_FLAG_MASK | G._DONE_HIT_PROC_FLAG_MASK)),
    }
    for field, ok in usable.items():
        if not ok and vals[field] != 0:
            findings.append(f"{label}: {field}={vals[field]} is unusable for ProcFlags={flags} "
                            f"(the core ignores it)")


def check_proc_sanity(path: str, d: dict, findings: list):
    # Mirror emit_custom_sql's iteration EXACTLY: helper procs first (raw `chance` value straight
    # from the YAML, defaulting to DEFAULT_PROC_CHANCE), then per-rank node proc rows.
    for h in d.get("helpers", []) or []:
        if h.get("proc"):
            chance = h["proc"].get("chance", G.DEFAULT_PROC_CHANCE)
            cols, row = G._spell_proc_row_for(d, int(h["id"]), h["proc"], chance)
            _audit_proc_row(f"{path}: helper {h['id']} ({h['name']})", cols, row, findings)
    for _tab, n in G._nodes(d):
        if n.get("mechanic") in ("proc", "proc-scripted"):
            for rank in range(1, int(n["maxRank"]) + 1):
                cols, row = G._spell_proc_row(d, n, rank)
                _audit_proc_row(f"{path}: node {n['id']} ({n['name']}) rank {rank}",
                                cols, row, findings)


# ---------------------------------------------------------------------------
# Check 1b — DAMAGE typeMask on a no-damage affected spell (the Improved Wing Clip bug)
# ---------------------------------------------------------------------------
# A DONE-spell-hit proc event only carries SpellTypeMask DAMAGE (1) when the landing spell
# actually produced damageInfo; a no-damage spell's hit computes NO_DMG_HEAL (4) instead
# (Unit::ProcSkillsAndAuras' damage/heal branch), and CanSpellTriggerProcOnEvent then rejects a
# typeMask-1 row outright (1 & 4 == 0) — the proc can literally never fire. Wing Clip 2974 is
# the live case: its Vanilla physical hit was removed in 3.x, leaving one APPLY_AURA(33) effect,
# and the generator's DEFAULT typeMask is 1. Caught in review of fcf5bce; this check makes the
# class of bug a headless audit finding. Damage-capable shapes, ids verified against the fork's
# SharedDefines.h / SpellAuraDefines.h: SCHOOL_DAMAGE 2, HEALTH_LEECH 9, WEAPON_DAMAGE_NOSCHOOL
# 17, WEAPON_PERCENT_DAMAGE 31, WEAPON_DAMAGE 58, NORMALIZED_WEAPON_DMG 121; periodic-damage
# auras 3 / 53 (leech) / 89.
_DAMAGE_EFFECTS = {2, 9, 17, 31, 58, 121}
_DAMAGE_AURAS = {3, 53, 89}
# DONE spell-hit classes whose events carry the damage-vs-NO_DMG_HEAL distinction above.
_HIT_CLASS_DONE_FLAGS = 0x10 | 0x100 | 0x1000 | 0x10000   # melee / ranged / none-neg / magic-neg


def _spell_deals_damage(row) -> bool:
    for i in (1, 2, 3):
        eff = int(row[IDX[f"Effect_{i}"]])
        if eff in _DAMAGE_EFFECTS:
            return True
        if eff == 6 and int(row[IDX[f"EffectAura_{i}"]]) in _DAMAGE_AURAS:
            return True
    return False


def check_proc_damage_reachability(path: str, d: dict, templates: dict | None, findings: list):
    if templates is None:                       # no base Spell.dbc available — nothing to resolve
        return
    ref_spells = (d.get("_ref") or {}).get("spells", {}) or {}
    for _tab, n in G._nodes(d):
        if n.get("mechanic") not in ("proc", "proc-scripted"):
            continue
        proc = n["proc"]
        flags = int(proc.get("flags", 0))
        type_mask = int(proc.get("typeMask", 1))    # the generator's default is 1 (DAMAGE)
        if not (flags & _HIT_CLASS_DONE_FLAGS) or not (type_mask & 1) or (type_mask & 4):
            continue                                # not a hit-class proc, or already NO_DMG-safe
        if int(proc.get("attrMask", 0)) & 2:
            # PROC_ATTR_TRIGGERED_CAN_PROC: the proc deliberately fires off TRIGGERED casts,
            # whose spellInfo (a family-mask sibling like Blizzard's damage tick 42208) is NOT
            # the `affects:`-named base row — the named row's no-damage shape proves nothing
            # about the real event spell, so resolving it would false-positive (Improved
            # Blizzard 18042). Skip; the base-spell resolution below is only sound for procs
            # keyed to direct player casts.
            continue
        names = proc.get("affects") or []
        rows = [templates.get(int(ref_spells[nm]["id"])) for nm in names if nm in ref_spells]
        rows = [r for r in rows if r is not None]
        if rows and not any(_spell_deals_damage(r) for r in rows):
            findings.append(
                f"{path}: node {n['id']} ({n['name']}): proc typeMask includes DAMAGE(1) but not "
                f"NO_DMG_HEAL(4), and no affected spell ({', '.join(names)}) has a damage effect "
                f"— the hit event carries SpellTypeMask NO_DMG_HEAL(4) and the proc can never "
                f"fire (author typeMask: 5; the Improved Wing Clip bug)")


# ---------------------------------------------------------------------------
# Check 2 — AuraDescription leaks (report-mode twin of build_client_dbc's hard error)
# ---------------------------------------------------------------------------

def check_aura_desc_leaks(path: str, d: dict, templates: dict | None, findings: list):
    for h in d.get("helpers", []) or []:
        c = h.get("client")
        tid = h.get("template")
        if not c or tid is None:
            continue
        tmpl = (templates or {}).get(int(tid))
        if tmpl is None:
            findings.append(f"{path}: helper {h['id']}: template spell {tid} not in base Spell.dbc")
            continue
        if "auraDescription" in c:
            continue
        if not tmpl[IDX["AuraDescription_Lang_enUS"]]:
            continue
        # Authored effects overwrite all template slots; with none authored the template's own
        # effects ship wholesale, so check THOSE (independent of lint, which now rejects that shape).
        if h.get("effects"):
            applies_aura = any(int(e.get("effect", 0)) == 6 for e in h["effects"])
        else:
            applies_aura = any(int(tmpl[IDX[f"Effect_{i}"]]) == 6 for i in (1, 2, 3))
        if applies_aura:
            findings.append(
                f"{path}: helper {h['id']} ({h['name']}): template {tid} has a non-empty "
                f"AuraDescription and the spell applies an aura, but client.auraDescription is "
                f"not authored — the buff tooltip would show the stock WotLK text")


# ---------------------------------------------------------------------------
# Check 2b — non-3.3.5a icon basenames
# ---------------------------------------------------------------------------
# A talent button whose `icon:` names a texture the 3.3.5a client does not have renders BLANK:
# no error, no log line, nothing a headless check can see. It is therefore only catchable by a
# real-client pass — which is exactly how it escaped twice (Vanilla shaman nodes 18215/18240,
# hand-fixed; TBC druid node 20122, reported by the user on 2026-08-31).
#
# The source is the Wowhead Classic-family tooltip endpoints that tools/import_tbc_talents.py and
# tools/import_daribon_talents.py read: for spells whose art was re-cut for WoW Classic (2019) they
# serve the CLASSIC basename, `classic_`-prefixed. The un-prefixed basename is the 3.3.5a one.
#
# Scope note: this is a PREFIX check, not an existence check. Verifying every icon against the real
# client icon set would need SpellIcon.dbc plus the MPQ texture listing, neither of which is in this
# repo (ip-dbc/ carries only Spell/SkillLine*/SpellItemEnchantment), so it is not cheap. The
# `classic_` prefix is the only mechanism that has actually produced a blank button here, and it is
# free to check.
_BAD_ICON_PREFIXES = ("classic_",)


def check_icon_names(path: str, d: dict, findings: list):
    for _tab, n in G._nodes(d):
        icon = (n.get("icon") or "")
        for bad in _BAD_ICON_PREFIXES:
            if icon.startswith(bad):
                findings.append(
                    f"{path}: node {n['id']} ({n.get('name')}): icon {icon!r} carries the "
                    f"non-3.3.5a `{bad}` prefix — the talent button renders BLANK in a 3.3.5a "
                    f"client. Drop the prefix ({icon[len(bad):]!r}).")


# Wand-subclass bit for ITEM_SUBCLASS_WEAPON_WAND (1<<19). The old, wrong value 262144 is 1<<18
# ITEM_SUBCLASS_WEAPON_CROSSBOW.
_WAND_SUBCLASS_BIT = 524288


def check_wand_spec_gating(path: str, d: dict, findings: list):
    """A wand-gated aura 79 MOD_DAMAGE_PERCENT_DONE is a live landmine (see
    docs/superpowers/specs/2026-09-04-era-talents-wand-spec-leak-fix-design.md):

    - L1: a node named "wand" that is not gated to the wand subclass bit (1<<19) applies to nothing
      (the shipped bug gated to 262144 = 1<<18 CROSSBOW).
    - L2: a weapon-equip-gated aura-79 effect that does not author its school leaves the generator's
      silent EffectMiscValue default (127) — a damage aura must state its school explicitly.
    - L3: a wand-gated aura-79 effect with school 1 (Physical) is inert — a player wand SHOT carries
      the equipped wand's MAGIC school, never Physical.
    """
    for _tab, n in G._nodes(d):
        name = (n.get("name") or "")
        equip_sub = n.get("equipSubclass")
        # L1 — a wand-named talent must gate to the wand subclass bit.
        if "wand" in name.lower() and equip_sub != _WAND_SUBCLASS_BIT:
            findings.append(
                f"{path}: node {n['id']} ({name}): wand talent has equipSubclass={equip_sub!r}, "
                f"expected {_WAND_SUBCLASS_BIT} (1<<19 ITEM_SUBCLASS_WEAPON_WAND) — 262144 is 1<<18 "
                f"CROSSBOW and gates the aura to nothing.")
        # Collect (aura, school, school_explicit) per effect. A `stat` node carries one aura/school
        # at node level; a `multi` node carries each in its effect dicts (school key is `misc`).
        effs = []
        if n.get("mechanic") == "multi":
            for e in (n.get("effects") or []):
                effs.append((e.get("aura"), e.get("misc"), "misc" in e))
        else:
            effs.append((n.get("aura"), n.get("school"), "school" in n))
        for aura, school, school_explicit in effs:
            if aura != 79:
                continue
            # L2 — a weapon-gated damage aura must state its school (generator defaults to 127).
            if equip_sub is not None and not school_explicit:
                findings.append(
                    f"{path}: node {n['id']} ({name}): aura-79 MOD_DAMAGE_PERCENT_DONE is "
                    f"equip-gated (equipSubclass={equip_sub}) but does not author `school:` — the "
                    f"generator defaults EffectMiscValue to 127; state the school explicitly.")
            # L3 — Physical school never applies to a magic wand shot.
            if equip_sub is not None and (equip_sub & _WAND_SUBCLASS_BIT) and school == 1:
                findings.append(
                    f"{path}: node {n['id']} ({name}): aura-79 is wand-gated but school==1 "
                    f"(Physical) — a magic wand shot never carries the Physical school, so the aura "
                    f"is inert. Use school 126 (all schools but Physical).")


# ---------------------------------------------------------------------------
# Check 3 — SpellInfoCorrections pattern-sweep collisions
# ---------------------------------------------------------------------------
# Transcribed from the fork's src/server/game/Spells/SpellInfoCorrections.cpp pattern sweep
# (the `for (uint32 i = 0; i < GetSpellInfoStoreSize(); ++i)` block, ~line 5245): every predicate
# there that a generated row could POSSIBLY match, expressed over the 234-value spell_dbc row.
# Our rows only ever carry SpellClassSet 0 (GENERIC), 3 (MAGE), 4 (WARRIOR), 5 (WARLOCK), 6 (PRIEST),
# 7 (DRUID), 8 (ROGUE), 9 (HUNTER) or 10 (PALADIN), so predicates keyed to other class families are unreachable
# and get a comment line, not code:
#   - SPELLFAMILY_DEATHKNIGHT (15): SpellIconID 2721 + flags 0x2 -> Icy Touch flag extend — unreachable (family differs)
# The sweep's `switch (SpellFamilyName)` (SpellInfoCorrections.cpp:5299) has NO case for
# SPELLFAMILY_WARLOCK (5) — nor for MAGE (3), PRIEST (6) or GENERIC (0) — so those families carry
# NO family-specific sweep predicate to transcribe; their rows are only ever exposed to the
# family-agnostic predicates below (charge/jump self-gates on GENERIC, the Aimed Shot predicate
# on HUNTER; self-resurrect / trajectory / flight-icon match any family). The only Warlock touch in the
# whole file is an explicit-id ApplySpellFix (Death Coil ranks, :3713), which selects by spell id,
# not by pattern — and our custom ids live in the reserved [920000,933000) band, so it can never
# match a generated row. Adding 5 to the vetted set is therefore the whole Warlock coverage.
#
# SPELLFAMILY_ROGUE (8): the switch also has NO case for it (only PALADIN/DEATHKNIGHT/HUNTER do,
# per `grep -n "case SPELLFAMILY_" SpellInfoCorrections.cpp`), so there is no family-wide pattern
# sweep to transcribe. The file's only SPELLFAMILY_ROGUE touch at all is an id-pinned ApplySpellFix
# for Master of Subtlety (31221, 31222, 31223, :712-716) that forces SpellFamilyName back to ROGUE
# on those three stock ids — an id list, not a `SpellFamilyName == SPELLFAMILY_ROGUE` pattern gate,
# so like the Warlock Death Coil fix it selects by id and can never match a row in the reserved
# [920000,933000) band. Adding 8 to the vetted set is therefore the whole Rogue coverage — same
# reasoning as Warlock (5), MAGE (3), PRIEST (6), GENERIC (0).
#
# SPELLFAMILY_HUNTER (9) is the FIRST family we author that the switch actually has a case for
# (:5311-5318), so unlike 3/5/6 it needs real code, not just a vetted-set widening: the Aimed Shot
# arm rewrites RecoveryTime AND sets an attribute on any family-9 spell whose OWN SpellFamilyFlags[0]
# carries 0x00020000. Note "own": the sweep reads the spell-level mask (spell_dbc SpellClassMask_1,
# DBC field 209 = SpellEntry::SpellFamilyFlags[0]), NOT a per-effect EffectSpellClassMaskA_*, so a
# talent passive whose EFFECT mask targets Aimed Shot is unaffected — the row at risk is a custom
# Aimed Shot itself (e.g. a Vanilla-era clone of 19434, whose stock mask is exactly 0x20000 and
# whose Vanilla cooldown is 6s, not the 10s the sweep would force). Transcribed as a predicate below.
#
# SPELLFAMILY_PALADIN (10) also has a real `switch (SpellFamilyName)` case (SpellInfoCorrections.cpp
# :5301-5305) — the ORIGINAL reason it was listed "unreachable" above (before Paladin was an authored
# family) no longer holds: "Seals of the Pure should affect Seal of Righteousness" — `if
# spellInfo->SpellIconID == 25 && (spellInfo->Attributes & SPELL_ATTR0_PASSIVE)) spellInfo->
# Effects[EFFECT_0].SpellClassMask[1] |= 0x20000000;`. It keys on the SPELL's own SpellIconID (spell_dbc
# field, column `SpellIconID`) being 25 AND the PASSIVE attribute bit (SPELL_ATTR0_PASSIVE = 0x40, in
# `Attributes`) being set — a custom family-10 passive placed at icon 25 would silently gain a bit in
# EFFECT_0's SpellClassMask word 1 (spell_dbc column `EffectSpellClassMaskA_2`, the second of the three
# 32-bit words that make up Effect_1's 96-bit family mask). Transcribed as a predicate below. (The
# file's only OTHER SPELLFAMILY_PALADIN touch, the Redemption SpellFamilyName reassignment at :555-559,
# is an id-pinned ApplySpellFix({7328, 10322, 10324, 20772, 20773, 48949, 48950}), not a pattern gate —
# like the Warlock/Rogue id fixes it can never match a reserved [920000,933000) row.)
#
# SPELLFAMILY_WARRIOR (4) is the second authored family the sweep touches, but NOT via the
# `switch (SpellFamilyName)` (which has no WARRIOR case — only PALADIN/DEATHKNIGHT/HUNTER). It is a
# family-gated predicate in the loop BODY, right after the switch (SpellInfoCorrections.cpp:5321-5325):
# `if (spellInfo->CategoryEntry == sSpellCategoryStore.LookupEntry(132) && SpellFamilyName ==
# SPELLFAMILY_WARRIOR) AttributesEx6 |= SPELL_ATTR6_NO_CATEGORY_COOLDOWN_MODS` — the
# Recklessness/Shield Wall/Retaliation shared-cooldown fix. It keys on the SPELL's Category field
# (spell_dbc Category, DBC field 1 == SpellInfo::CategoryEntry->ID) being 132, so a custom family-4
# row placed in category 132 would silently gain SPELL_ATTR6_NO_CATEGORY_COOLDOWN_MODS. Transcribed
# as a predicate below. (The file's only OTHER SPELLFAMILY_WARRIOR touch, the Vigilance
# SpellFamilyName reassignment at :1184-1188, is an id-pinned ApplySpellFix({50720}), not a pattern
# gate — like the Warlock/Rogue id fixes it can never match a reserved [920000,933000) row.)
#
# SPELLFAMILY_DRUID (7): `grep -nE "SPELLFAMILY_DRUID" SpellInfoCorrections.cpp` returns ZERO
# matches — no family-wide `SpellFamilyName == SPELLFAMILY_DRUID` pattern gate anywhere in the
# file, and the `switch (SpellFamilyName)` case list is only PALADIN/DEATHKNIGHT/HUNTER (confirmed
# by the `case SPELLFAMILY_` grep above), so there is no DRUID case to transcribe either. A
# case-insensitive `druid` grep over the whole file turns up only unrelated matches (an NPC name
# comment, a T8 4-piece item comment, a Faction Champions comment, and an id-pinned
# `ApplySpellFix({47476, 15487, 5211, 6798, 8983}, ...)` at :3886-3895 — Strangulate/Silence/Bash
# R1-R3 -> adds SPELL_ATTR7_CAN_CAUSE_INTERRUPT). That last one selects by spell id, not by a
# family/icon/flag pattern, so like the Warlock/Rogue id fixes it can never match a row in the
# reserved [920000,933000) band. Adding 7 to the vetted set is therefore the whole Druid coverage —
# no family-wide SPELLFAMILY_DRUID pattern sweep in SpellInfoCorrections as of fork HEAD 190184a0
# (file last touched ac6592ae, 2026-07-24); id-pinned fixes only.
# A full read of SpellInfoCorrections.cpp (5427 lines; fork HEAD 190184a0, file last touched
# ac6592ae 2026-07-24) confirms this is the file's ONLY spell-store pattern sweep (one
# `for (uint32 i = 0; i < GetSpellInfoStoreSize(); ++i)` at :5245) and that no family gate anywhere
# is written as a bare magic number — every one spells out a SPELLFAMILY_* token, so the grep for
# those tokens is complete, not merely suggestive. Re-verify on a core bump that touches this file.
# SPELLFAMILY_SHAMAN (11): `grep -nE "SPELLFAMILY_SHAMAN" SpellInfoCorrections.cpp` returns ZERO
# matches — no family-wide `SpellFamilyName == SPELLFAMILY_SHAMAN` pattern gate anywhere in the
# file, and the `switch (SpellFamilyName)` case list is only PALADIN/DEATHKNIGHT/HUNTER (confirmed
# by the `case SPELLFAMILY_` grep above), so there is no SHAMAN case to transcribe either. A
# case-insensitive `shaman` grep over the whole file turns up only one match, an id-pinned
# `ApplySpellFix({66063}, ...)` at :2344-2348 (Trial of the Crusader Faction Champions NPC's Earth
# Shield -> rewrites it to a proc-trigger). That selects by spell id, not by a family/icon/flag
# pattern, so like the Warlock/Rogue/Druid id fixes it can never match a row in the reserved
# [920000,933000) band. Adding 11 to the vetted set is therefore the whole Shaman coverage — no
# family-wide SPELLFAMILY_SHAMAN pattern sweep in SpellInfoCorrections as of fork HEAD 190184a0
# (file last touched ac6592ae, 2026-07-24); id-pinned fixes only.
# That "unreachable" claim only holds for rows whose family is actually in {0, 3, 4, 5, 6, 7, 8, 9, 10, 11} —
# it is NOT hardcoded here; check_sweep_collisions below self-enforces it per row and raises a finding the
# moment a row's SpellClassSet drifts outside that set, so a future cross-class dataset can't
# silently trust stale unreachability reasoning.

_EFFECT_IDX = [IDX[f"Effect_{i}"] for i in (1, 2, 3)]
_TARGETA_IDX = [IDX[f"ImplicitTargetA_{i}"] for i in (1, 2, 3)]
_TARGETB_IDX = [IDX[f"ImplicitTargetB_{i}"] for i in (1, 2, 3)]

# SPELL_EFFECT_JUMP=41, JUMP_DEST=42, CHARGE=96, LEAP_BACK=138, CHARGE_DEST=149 (SharedDefines.h)
_CHARGE_EFFECTS = {41, 42, 96, 138, 149}
_SELF_RESURRECT = 94          # SPELL_EFFECT_SELF_RESURRECT
_TARGET_DEST_TRAJ = 89        # TARGET_DEST_TRAJ
# SpellEntry::SpellFamilyFlags[0] (DBC field 209) == spell_dbc SpellClassMask_1 — the SPELL's own
# family mask, not a per-effect one. 0x00020000 is Aimed Shot's bit on every stock rank.
_AIMED_SHOT_FAMILY_FLAG = 0x00020000
# spell_dbc Category (DBC field 1) == SpellInfo::CategoryEntry->ID. 132 is the shared cooldown
# category the warrior Recklessness/Shield Wall/Retaliation sweep gates on.
_WARRIOR_SHARED_CD_CATEGORY = 132
# SPELL_ATTR0_PASSIVE (SharedDefines.h) == 0x00000040, tested against spell_dbc `Attributes`.
# SpellIconID 25 is Seal of Righteousness's icon — the paladin Seals-of-the-Pure sweep gates on it.
_SPELL_ATTR0_PASSIVE = 0x00000040
_PALADIN_SEAL_ICON_ID = 25

SWEEP_PREDICATES = [
    ("charge/jump-speed rewrite: SpellFamilyName==GENERIC + Speed==0 + a charge/jump/leap effect "
     "-> core forces Speed=SPEED_CHARGE",
     lambda v: v[IDX["SpellClassSet"]] == 0 and float(v[IDX["Speed"]]) == 0.0
               and any(v[e] in _CHARGE_EFFECTS for e in _EFFECT_IDX)),
    ("self-resurrect target rewrite: any Effect==94 (SELF_RESURRECT) "
     "-> core forces that effect's TargetA=TARGET_UNIT_CASTER",
     lambda v: any(v[e] == _SELF_RESURRECT for e in _EFFECT_IDX)),
    ("trajectory range rewrite: any effect with TargetA/B==89 (TARGET_DEST_TRAJ) "
     "-> core rewrites the TRIGGERED spell's RangeEntry",
     lambda v: any(v[_EFFECT_IDX[k]] != 0
                   and (v[_TARGETA_IDX[k]] == _TARGET_DEST_TRAJ
                        or v[_TARGETB_IDX[k]] == _TARGET_DEST_TRAJ)
                   for k in range(3))),
    ("flight-icon passive rewrite: ActiveIconID==2158 -> core forces SPELL_ATTR0_PASSIVE",
     lambda v: v[IDX["ActiveIconID"]] == 2158),
    ("Aimed Shot cooldown rewrite (SpellInfoCorrections.cpp:5311-5318): SpellClassSet==HUNTER + "
     "SpellClassMask_1 & 0x00020000 -> core forces RecoveryTime=10000ms and sets "
     "SPELL_ATTR6_NO_CATEGORY_COOLDOWN_MODS (a Vanilla 6s Aimed Shot would silently become 10s)",
     lambda v: v[IDX["SpellClassSet"]] == 9
               and (v[IDX["SpellClassMask_1"]] & _AIMED_SHOT_FAMILY_FLAG) != 0),
    ("Recklessness/Shield Wall/Retaliation shared-cooldown rewrite "
     "(SpellInfoCorrections.cpp:5321-5325): SpellClassSet==WARRIOR + Category==132 -> core forces "
     "SPELL_ATTR6_NO_CATEGORY_COOLDOWN_MODS on the row",
     lambda v: v[IDX["SpellClassSet"]] == 4
               and v[IDX["Category"]] == _WARRIOR_SHARED_CD_CATEGORY),
    ("Seals of the Pure mask-extend rewrite (SpellInfoCorrections.cpp:5301-5305): "
     "SpellClassSet==PALADIN + SpellIconID==25 + SPELL_ATTR0_PASSIVE -> core ORs 0x20000000 into "
     "Effect_1's SpellClassMask word 2 (EffectSpellClassMaskA_2)",
     lambda v: v[IDX["SpellClassSet"]] == 10
               and v[IDX["SpellIconID"]] == _PALADIN_SEAL_ICON_ID
               and (v[IDX["Attributes"]] & _SPELL_ATTR0_PASSIVE) != 0),
]


def _quote_open(text: str, in_str: bool) -> bool:
    """Whether `text` leaves a single-quoted SQL string open, given the entry state."""
    i = 0
    while i < len(text):
        if in_str and text[i] == "'" and text[i:i + 2] == "''":
            i += 2; continue
        if text[i] == "'":
            in_str = not in_str
        i += 1
    return in_str


def _spell_dbc_rows(sql: str):
    """Yield (spell_id, [234 typed values]) for every spell_dbc INSERT in emit_custom_sql output.
    A row tuple starts on the line after its INSERT header but can SPAN lines — template-cloned
    rows carry localized strings with embedded newlines — so accumulate quote-aware until the
    statement closes. The header tracking skips spell_proc / spell_script_names rows."""
    lines = sql.splitlines()
    i = 0
    while i < len(lines):
        if lines[i].startswith("INSERT INTO `spell_dbc`"):
            buf, in_str = lines[i + 1], _quote_open(lines[i + 1], False)
            i += 1
            while in_str or not buf.rstrip().endswith(";"):
                i += 1
                in_str = _quote_open(lines[i], in_str)
                buf += "\n" + lines[i]
            vals = [_typed(v) for v in _parse_values(buf)]
            assert len(vals) == len(G.COLS), (len(vals), buf[:80])
            yield vals[IDX["ID"]], vals
        i += 1


_TRANSCRIBED_SWEEP_FAMILIES = {0, 3, 4, 5, 6, 7, 8, 9, 10, 11}
# 4 = WARRIOR: the Recklessness/Shield Wall/Retaliation category-132 shared-cooldown sweep is
# transcribed in SWEEP_PREDICATES (SpellInfoCorrections.cpp:5321-5325); the Vigilance touch (:1184)
# is an id-pinned ApplySpellFix, not a pattern gate — see the comment block above.
# 7 = DRUID: no family-wide SPELLFAMILY_DRUID pattern sweep in SpellInfoCorrections as of fork HEAD
# 190184a0; id-pinned Strangulate/Silence/Bash-R1-3 interrupt fix (47476/15487/5211/6798/8983) only
# — see the comment block above.
# 8 = ROGUE: no family-wide SPELLFAMILY_ROGUE pattern sweep in SpellInfoCorrections as of fork
# HEAD 190184a0; id-pinned Master of Subtlety fix (31221-3) only — see the comment block above.
# 10 = PALADIN: the Seals-of-the-Pure SpellIconID-25+passive mask-extend sweep (SpellInfoCorrections
# .cpp:5301-5305) is transcribed in SWEEP_PREDICATES; the Redemption touch (:555-559) is an
# id-pinned ApplySpellFix, not a pattern gate — see the comment block above.
# 11 = SHAMAN: no family-wide SPELLFAMILY_SHAMAN pattern sweep in SpellInfoCorrections as of fork
# HEAD 190184a0; id-pinned Faction Champions Earth Shield fix (66063) only — see the comment block
# above.


def check_sweep_collisions(path: str, d: dict, findings: list):
    for spell_id, vals in _spell_dbc_rows(G.emit_custom_sql(d)):
        fam = vals[IDX["SpellClassSet"]]
        if fam not in _TRANSCRIBED_SWEEP_FAMILIES:
            findings.append(
                f"{path}: spell_dbc {spell_id} family {fam} outside the transcribed sweep set "
                f"{{0,3,4,5,6,7,8,9,10,11}} — extend SWEEP_PREDICATES for this class before trusting the audit")
        for desc, pred in SWEEP_PREDICATES:
            if pred(vals):
                findings.append(f"{path}: spell {spell_id}: corrections-sweep collision — {desc}")


# Per-family "never bind alone" category bits: shared flag bits carried by dozens of same-family
# spells. A SPELLMOD that DERIVES its EffectSpellClassMask from `affects:` (the OR of each named
# spell's _ref identity, via G._class_mask) must not pull one of these in, because
# SpellInfo::IsAffected ORs across the three flag words -- any overlap matches -- so a derived mask
# carrying a shared bit leaks the modifier onto every spell that shares it. Keyed by _ref familyName
# (== SpellClassSet); each entry maps a family word (0/1/2) to the shared-bit mask forbidden in that
# word. Family 6 (PRIEST) from family6_bit_census in era-data/_ref/tbc/priest-spells.yaml:
#   word0 bit23 0x00800000 (126 spells, TBC Mind Flay's ONLY identity bit); word1 bit12 0x00001000
#   (43 spells: Fade, Devouring Plague, Fear Ward, Levitate, ...); word2 bit10 0x00000400 (154 spells).
# Curated denylist of *category* bits, NOT every shared bit: an identity bit shared BY DESIGN (e.g.
# priest word0 bit5, Divine Spirit + Prayer of Spirit) is a legitimate derived target and is absent.
# Extend per class as each family's census is dumped.
_SHARED_CATEGORY_BITS = {
    6: {0: 0x00800000, 1: 0x00001000, 2: 0x00000400},
}


def _affects_spellmod_specs(d: dict):
    """Yield (node_id, spec) for every spellmod-family binding site whose mask is DERIVED from
    `affects:` -- i.e. it has an `affects:` list and NO `affectsMask:` override. An explicit
    affectsMask: means the author took verbatim control of the emitted mask (the documented override
    for the shared-bit case, e.g. TBC Silent Resolve reproducing its enumerated mask on purpose), so
    those sites are deliberately excluded. Covers the four sites the generator masks by family:
    a `mechanic: spellmod` node, a `multi` node's spellmod/stat effects, and a proc's `affects:`."""
    for _tab, n in G._nodes(d):
        mech = n.get("mechanic")
        candidates = []
        if mech == "spellmod":
            candidates.append(n)
        elif mech == "multi":
            candidates.extend(n.get("effects", []) or [])
        elif mech in ("proc", "proc-scripted") and isinstance(n.get("proc"), dict):
            candidates.append(n["proc"])
        for spec in candidates:
            if spec.get("affects") is not None and spec.get("affectsMask") is None:
                yield n["id"], spec


def check_shared_family_bits(path: str, d: dict, findings: list):
    """FAIL when an `affects:`-DERIVED spellmod mask includes a shared "never bind alone" category
    bit for its family (see _SHARED_CATEGORY_BITS). This is the general backstop for the Improved
    Fade class of bug: the Vanilla ref carried Fade's shared word1 bit12, so the mask DERIVED from
    `affects: [Fade]` reduced the cooldown of all 43 spells sharing that bit. Fix by dropping the
    shared bit from the affects: ref row (its identity is the unique bit alone) or, when the shared
    bit is genuinely wanted, authoring an explicit affectsMask: {a,b,c} override (which this check
    then trusts). Sites with an affectsMask: override are intentionally NOT linted here."""
    ref = d.get("_ref") or {}
    if not ref.get("spells"):
        return
    fam = int(ref.get("familyName", 3))
    forbidden = _SHARED_CATEGORY_BITS.get(fam)
    if not forbidden:
        return
    for node_id, spec in _affects_spellmod_specs(d):
        try:
            words = G._class_mask(d, spec["affects"])
        except ValueError:
            continue  # unknown affects name -- the generator's own lint reports it
        for widx, bits in forbidden.items():
            if words[widx] & bits:
                findings.append(
                    f"{path}: node {node_id}: affects:-derived mask word{widx} = {words[widx]} "
                    f"includes shared family-{fam} category bit {hex(bits)} (never bind alone) -- "
                    f"drop it from the affects: ref row, or add an explicit affectsMask: override")


def check_totem_summons(datasets: list, findings: list, resolution_only: list | None = None):
    """Totem linkage: a `totems:` creature must be referenced by a summon effect (a generated spell
    whose EffectMiscValue_{1,2,3} lands in the reserved creature band [TOTEM_CREATURE_BASE,
    TOTEM_CREATURE_END) — detected by misc-VALUE-IN-BAND rather than by SPELL_EFFECT_SUMMON's effect
    code, which keeps the check format-agnostic), and each totem's pulse/pulse2 must resolve to an
    emitted custom spell id (creature_template_spell FK).

    RESOLUTION IS ALWAYS AGAINST THE FULL MANIFEST, never just the datasets named on the command
    line, because a later era deliberately REUSES an earlier era's totem substrate: where a TBC
    pulse measures identical to the shipped Vanilla clone, the TBC summon points straight at the
    Vanilla creature entry rather than duplicating the creature + pulse rows. `resolution_only`
    carries the manifest datasets the caller did NOT ask about. Without this, auditing a reusing era
    ALONE reported every deliberate cross-era reuse as "no totems: entry" — a false finding on a
    by-design arrangement, which is exactly the kind of trap a developer then chases.

    TWO invariants, deliberately different, because over-reporting and mis-reporting are not equally
    bad:
      * a finding is only RAISED for a dataset on the run — a run the caller did not ask for cannot
        be actioned by them, and the bare sweep catches it anyway;
      * the `emitted` pulse-FK pool is manifest-wide, matching runtime: every dataset's SQL loads
        into ONE database, so a pulse owned by another dataset really does resolve.

    ORDERING IS LOAD-BEARING. Creature declarations are walked `resolution_only + datasets` so the
    dataset NOT under edit takes the incumbent slot. Walking the run first made the newly-broken
    file the incumbent and reported the shipped, correct file as the offender — which for this
    project means sending an author to edit merged, real-client-verified content on master."""
    resolution_only = list(resolution_only or [])
    on_run = {path for path, _d in datasets}
    # resolution_only FIRST — see the ordering note above.
    all_totems = [(path, t) for path, d in resolution_only + datasets
                  for t in (d.get("totems") or [])]

    creatures: dict[int, tuple[str, dict]] = {}
    for path, t in all_totems:
        entry = int(t["creature"])
        if entry in creatures:
            if path in on_run or creatures[entry][0] in on_run:
                findings.append(f"{path}: totem creature {entry} ({t.get('name','?')}) is "
                                f"already declared by {creatures[entry][0]}")
        else:
            creatures[entry] = (path, t)

    per_path_rows = [(path, list(_spell_dbc_rows(G.emit_custom_sql(d))))
                     for path, d in resolution_only + datasets]
    emitted = {spell_id for _path, rows in per_path_rows for spell_id, _vals in rows}
    referenced = set()
    for path, rows in per_path_rows:
        for spell_id, vals in rows:
            for i in (1, 2, 3):
                misc = int(vals[IDX[f"EffectMiscValue_{i}"]])
                if G.TOTEM_CREATURE_BASE <= misc < G.TOTEM_CREATURE_END:
                    referenced.add(misc)
                    if misc not in creatures and path in on_run:
                        findings.append(f"{path}: spell {spell_id} Effect_{i} references totem "
                                        f"creature {misc} with no totems: entry")

    for entry, (path, t) in creatures.items():
        if entry not in referenced and path in on_run:
            findings.append(f"{path}: totem creature {entry} ({t.get('name','?')}) is never "
                            f"referenced by a summon effect")
    # Walk ALL totems, not the creatures dict: the LOSER of a duplicate-entry collision is absent
    # from that dict, and skipping it would leave the broken row's own pulse FK unchecked.
    for path, t in all_totems:
        if path not in on_run:
            continue
        for key in ("pulse", "pulse2"):
            pid = t.get(key)
            if pid is not None and int(pid) not in emitted:
                findings.append(f"{path}: totem {t.get('name','?')} {key} {pid} is not an "
                                f"emitted custom spell id")


_SPELLFIX_SRC = (_TOOLS.parent / "azerothcore-wotlk" / "src" / "server" / "game" / "Spells"
                 / "SpellInfoCorrections.cpp")

# spellInfo->Field the correction body mutates -> the helper key that can reproduce it.
_SPELLFIX_KEYABLE = {
    "Attributes": "attributes", "AttributesEx": "attributesEx", "AttributesEx2": "attributesEx2",
    "AttributesEx3": "attributesEx3", "AttributesEx5": "attributesEx5",
    # AttributesEx7 gained a generator key with the TBC priest Silence clone (spec Amendment A.14):
    # SpellInfoCorrections.cpp:3886 ORs SPELL_ATTR7_CAN_CAUSE_INTERRUPT onto 15487, and without the
    # key that fix fell into the "no generator key — acknowledge only" branch below, which would have
    # let a silent functional loss (silence without interrupt) be waved through with a spellFixAck.
    "AttributesEx7": "attributesEx7",
}
_APPLYFIX_RE = re.compile(r"ApplySpellFix\(\s*\{([^}]*)\}", re.S)
_FIELD_RE = re.compile(r"spellInfo->(\w+)")


def _blank_comments_and_strings(src: str) -> str:
    """`src` with // and /* */ comments and "..."/'...' literals replaced by spaces — same length,
    so byte offsets and line numbers still line up with the original.

    The brace matcher below scans THIS copy. An unbalanced `{`/`}` inside a comment or inside a
    format string (`LOG_ERROR("... {}", spellId)`) would otherwise close a fix body early and
    silently drop every field mentioned after it — a false NEGATIVE, which is the worst failure
    mode for a check whose entire job is to find silent losses. Field names are harvested from the
    blanked copy too, so a `spellInfo->Foo` named only in a comment is not counted as a fix."""
    out, i, n = list(src), 0, len(src)

    def blank(k):
        if k < n and src[k] != "\n":  # keep newlines so line numbers survive
            out[k] = " "

    while i < n:
        two = src[i:i + 2]
        if two == "//":
            while i < n and src[i] != "\n":
                blank(i); i += 1
        elif two == "/*":
            blank(i); blank(i + 1); i += 2
            while i < n and src[i:i + 2] != "*/":
                blank(i); i += 1
            if i < n:                 # the closing */
                blank(i); blank(i + 1); i += 2
        elif src[i] in "\"'":
            quote = src[i]
            blank(i); i += 1
            while i < n and src[i] != quote:
                if src[i] == "\\":       # escape: consume the escaped char too
                    blank(i); i += 1
                if i < n:
                    blank(i); i += 1
            if i < n:
                blank(i); i += 1
        else:
            i += 1
    assert len(out) == n
    return "".join(out)


def _spellfix_sets():
    """[(line_no, {ids}, {fields})] parsed from the fork's SpellInfoCorrections.cpp.

    Returns [] when the fork is absent (same convention as check_trainer_wrapper_leaks) so the audit
    still runs on a checkout without azerothcore-wotlk/."""
    if not _SPELLFIX_SRC.exists():
        return []
    src = _blank_comments_and_strings(_SPELLFIX_SRC.read_text(errors="replace"))
    out = []
    for m in _APPLYFIX_RE.finditer(src):
        ids = {int(x) for x in re.findall(r"\d+", m.group(1))}
        if not ids:
            continue
        # Brace-match the lambda body that follows, so a nested block cannot truncate it.
        i = src.find("{", m.end())
        if i < 0:
            continue
        depth, j = 0, i
        while j < len(src):
            if src[j] == "{": depth += 1
            elif src[j] == "}":
                depth -= 1
                if depth == 0: break
            j += 1
        body = src[i:j]
        out.append((src.count("\n", 0, m.start()) + 1, ids, set(_FIELD_RE.findall(body))))
    return out


def check_spellfix_inheritance(datasets: list, findings: list):
    """A CLONE never receives `SpellInfoCorrections.cpp`'s by-id fixes.

    The core mutates stock SpellInfo objects BY SPELL ID at load (`ApplySpellFix({...}, ...)`).
    A cloned helper has a DIFFERENT id, so every such fix silently fails to apply to it — no
    generator error, no runtime error, and until this check, no audit finding. Found the expensive
    way in Plan 3b batch 4a, after it had already degraded a driver clone minted two commits earlier
    (the Earthbind driver lost SPELL_ATTR5_EXTRA_INITIAL_PERIOD and so waited a full period for its
    first tick, partly undoing the defect it existed to fix).

    Reports a helper that templates a corrected id and neither authors the matching attribute key nor
    acknowledges the fix. Two escape hatches, because not every correction is reproducible in data:
      * author the key (`attributes`, `attributesEx`, `attributesEx2/3/5`) with the post-fix value;
      * or set `spellFixAck: "<why this clone does not need it>"` on the helper.
    Corrections that touch anything else (`Effects[...]`, `TargetA`, interrupt flags) have no key, so
    they can only be acknowledged — deliberately, since reproducing them needs a human judgement."""
    fixes = _spellfix_sets()
    if not fixes:
        return                      # no fork checkout — nothing to audit
    for path, d in datasets:
        for h in (d.get("helpers") or []):
            tid = h.get("template")
            if tid is None:
                continue
            if h.get("spellFixAck"):
                continue
            for line, ids, fields in fixes:
                if int(tid) not in ids:
                    continue
                keyable = {f: _SPELLFIX_KEYABLE[f] for f in fields if f in _SPELLFIX_KEYABLE}
                missing = sorted(k for f, k in keyable.items() if k not in h)
                other = sorted(f for f in fields if f not in _SPELLFIX_KEYABLE and f != "Id")
                if missing:
                    findings.append(
                        f"{path}: helper {h.get('id')} templates {tid}, which SpellInfoCorrections.cpp"
                        f":{line} fixes ({', '.join(sorted(keyable))}) — a clone does not inherit it. "
                        f"Author {', '.join(missing)} with the post-fix value, or set spellFixAck:")
                elif other and not keyable:
                    findings.append(
                        f"{path}: helper {h.get('id')} templates {tid}, which SpellInfoCorrections.cpp"
                        f":{line} fixes ({', '.join(other)}) — a clone does not inherit it, and this "
                        f"correction has no generator key. Set spellFixAck: with the reasoning.")


# ScriptNames the mod-era-talents module registers (keep in sync with EraTalentTotemScripts.cpp).
_KNOWN_CREATURE_SCRIPTS = {"npc_era_fire_nova_totem"}

def check_totem_scripts(path, d, findings):
    for t in d.get("totems", []) or []:
        s = t.get("script")
        if s and s not in _KNOWN_CREATURE_SCRIPTS:
            findings.append(f"{path}: totem creature {t.get('creature')} names unknown "
                            f"ScriptName '{s}' (not in _KNOWN_CREATURE_SCRIPTS / module)")


# ---------------------------------------------------------------------------
# Check 7 — trainer learn-wrapper leaks (global, not per-dataset)
# ---------------------------------------------------------------------------
# The 2026-08-25 paladin "Judgement" bug: a stock trainer_spell row can be an Effect-36
# (SPELL_EFFECT_LEARN_SPELL) WRAPPER — the row's SpellId is the wrapper (10321 "Judgement")
# and the spells actually taught are its EffectTriggerSpell targets (20271 Judgement of
# Light + 21084 WotLK Seal of Righteousness), so a cleanup that searches trainer_spell for
# the TAUGHT ids finds nothing and concludes "not a trainer row". This sweep rebuilds the
# EFFECTIVE trainer_spell table (base dump, minus module DELETEs, plus module INSERTs) and
# fails when any UNGATED row (ReqAbility1=0) teaches — directly or via Effect-36 — a STOCK
# id (< G.CUSTOM_PASSIVE_BASE) that the module removes from Vanilla characters. The
# forbidden set is harvested from the module SQL's own trainer_spell DELETE statements,
# plus _CPP_STRIPPED_STOCK (transcribed from EraTalents.cpp strip lists that have no
# matching SQL delete — keep in sync, the SWEEP_PREDICATES precedent). Skips silently when
# the gitignored fork checkout (base dump / Spell.dbc) is absent.
# Limitation: `UPDATE trainer_spell SET ReqAbility1=...` statements (the 2026_08_14_00
# priest Holy Nova gate pattern) are NOT replayed — today the base rows already carry
# those chains, but a future file that gates a leaking stock row via UPDATE would
# false-positive here; convert such a gate to DELETE+INSERT or extend the replay.

_MODULE_SQL_DIR = _TOOLS.parent / "modules" / "mod-era-talents" / "data" / "sql" / "world" / "base"
_BASE_TRAINER_SQL = (_TOOLS.parent / "azerothcore-wotlk" / "data" / "sql" / "base"
                     / "db_world" / "trainer_spell.sql")

_BAND_ALLOWLIST = _TOOLS.parent / "era-data" / "band-allowlist.yaml"
_gb_spec = importlib.util.spec_from_file_location("gen_band_allowlist", _TOOLS / "gen_band_allowlist.py")
GB = importlib.util.module_from_spec(_gb_spec)
_gb_spec.loader.exec_module(GB)

# Keep in sync with EraTalents.cpp kStockPalSwap (stock ids stripped from Vanilla paladins
# in C++ only — they have no direct trainer rows, which is exactly why a wrapper teaching
# them is invisible to SQL-side searches).
_CPP_STRIPPED_STOCK = {
    20154, 21084, 20164, 20165, 20166,   # seals (21084 = the 10321 wrapper's SoR)
    20271, 53407, 53408,                 # judgement buttons
    633, 2800, 10310, 27154,             # Lay on Hands
}

_TUPLE_RE = re.compile(r"\((\s*\d+\s*(?:,\s*\d+\s*){9})\)")
_ID_LIST_RE = re.compile(r"`?(TrainerId|SpellId)`?\s*(?:IN\s*\(([^)]*)\)|=\s*(\d+))", re.I)


def _sql_int_tuples(text: str):
    """Yield 10-int tuples from trainer_spell INSERT VALUES lists (base dump and module
    files share the column order: TrainerId,SpellId,MoneyCost,ReqSkillLine,ReqSkillRank,
    ReqAbility1,ReqAbility2,ReqAbility3,ReqLevel,VerifiedBuild)."""
    for m in _TUPLE_RE.finditer(text):
        yield tuple(int(x) for x in m.group(1).split(","))


def _effective_trainer_rows(findings: list):
    """(TrainerId, SpellId, ReqAbility1) rows after replaying the module SQL over the base
    dump, plus the set of spell ids the module SQL ever DELETEs from trainer_spell; appends
    a finding for any trainer_spell DELETE the replay cannot decompose.
    Returns (rows, deleted_ids) or (None, None) when the base dump is absent."""
    if not _BASE_TRAINER_SQL.is_file():
        return None, None
    rows = {(t[0], t[1]): t[5] for t in _sql_int_tuples(_BASE_TRAINER_SQL.read_text())}
    deleted_ids: set[int] = set()
    for path in sorted(_MODULE_SQL_DIR.glob("*.sql")):
        for raw in path.read_text().split(";"):
            stmt = "\n".join(ln for ln in raw.splitlines()
                             if not ln.lstrip().startswith("--"))
            if "trainer_spell" not in stmt:
                continue
            ids = {"TrainerId": None, "SpellId": None}
            for col, inlist, eq in _ID_LIST_RE.findall(stmt):
                ids[col] = {int(eq)} if eq else {int(x) for x in inlist.split(",")}
            if "DELETE" in stmt.upper():
                if not ids["SpellId"]:
                    findings.append(
                        f"{path.name}: trainer_spell DELETE this sweep cannot decompose "
                        f"(no SpellId list found) — extend _effective_trainer_rows or "
                        f"rewrite the statement as a SpellId-list DELETE")
                    continue
                deleted_ids |= ids["SpellId"]
                tids = ids["TrainerId"]     # None = no TrainerId clause -> all trainers
                for key in [k for k in rows
                            if (tids is None or k[0] in tids) and k[1] in ids["SpellId"]]:
                    del rows[key]
            elif "INSERT" in stmt.upper():
                for t in _sql_int_tuples(stmt):
                    rows[(t[0], t[1])] = t[5]
    return rows, deleted_ids


def check_trainer_wrapper_leaks(templates: dict | None, findings: list):
    rows, deleted_ids = _effective_trainer_rows(findings)
    if rows is None:
        return                                  # no fork checkout — nothing to audit
    if templates is None:
        if not pathlib.Path(G.DEFAULT_BASE_DBC).is_file():
            return
        templates = G.read_dbc_templates(G.DEFAULT_BASE_DBC)
    forbidden = {i for i in (_CPP_STRIPPED_STOCK | deleted_ids)
                 if i < G.CUSTOM_PASSIVE_BASE}
    for (tid, sid), req1 in sorted(rows.items()):
        if req1 != 0:
            continue                            # gated behind a known-spell chain — fine
        taught = {sid}
        row = templates.get(sid)
        if row is not None:
            for i in (1, 2, 3):
                if int(row[IDX[f"Effect_{i}"]]) == 36:
                    taught.add(int(row[IDX[f"EffectTriggerSpell_{i}"]]))
        for hit in sorted(taught & forbidden):
            via = f" via Effect-36 wrapper {sid}" if hit != sid else ""
            findings.append(
                f"trainer_spell: TrainerId {tid} SpellId {sid} is UNGATED and teaches "
                f"stock {hit}{via} — a version-swapped/stripped id must not be trainer-"
                f"obtainable (delete the row or gate it behind an era marker)")


def check_cross_dataset_ids(paths: list[str], findings: list):
    """Cross-DATASET id-collision check (lint is per-dataset and cannot see these). Flags:
    a node id claimed by two datasets (era_talent PK is global AND the passive formula would
    alias), a hand-helper id claimed twice, and overlapping pet-buff windows."""
    import yaml
    node_owner, helper_owner, pet_windows = {}, {}, []
    for p in paths:
        d = yaml.safe_load(pathlib.Path(p).read_text())
        for tab, n in G._nodes(d):
            nid = int(n["id"])
            if nid in node_owner:
                findings.append(f"{p}: node id {nid} already claimed by {node_owner[nid]}")
            else:
                node_owner[nid] = p
        for h in d.get("helpers") or []:
            hid = int(h["id"])
            if hid in helper_owner:
                findings.append(f"{p}: helper id {hid} already claimed by {helper_owner[hid]}")
            else:
                helper_owner[hid] = p
        n_pet = sum(1 for tab, n in G._nodes(d) if n.get("mechanic") == "pet")
        if n_pet:
            era = int(d.get("era", 0))
            default_base = G.ERA_PET_WINDOW[era][0] if era in G.ERA_PET_WINDOW else G.PET_BUFF_BASE
            base = int(d.get("petBuffBase", default_base))
            lo, hi = base, base + n_pet * G.PET_BUFF_BLOCK
            for (olo, ohi, op) in pet_windows:
                if lo < ohi and olo < hi:
                    findings.append(f"{p}: pet-buff window [{lo},{hi}) overlaps {op} [{olo},{ohi})")
            pet_windows.append((lo, hi, p))


# ---------------------------------------------------------------------------
# Check 9 — band ownership (Phase 9.5): every era-band spell a character can LEARN must be
# classifiable by EraBandClassifier — a node grant (era_talent_rank), a spell_ranks member above a
# node-granted r1, or an entry in era-data/band-allowlist.yaml. Anything else the runtime STRIPS
# as UNCLASSIFIED, so an incomplete allowlist would silently remove a legitimate spell; this check
# is the guard that makes "strip the unclassified remainder" safe.
# ---------------------------------------------------------------------------
_INSERT_HDR_RE = re.compile(r"INSERT\s+(?:IGNORE\s+)?INTO\s+`?(\w+)`?", re.I)
# A line that is nothing but all-int tuples. The `+` is load-bearing: the hand-written chain files
# pack SEVERAL tuples per line (`(948870,948870,1),(948870,948871,2),...`), and a single-tuple
# anchor silently degraded every such line to its first int via _ROW_FIRST_INT_RE — which dropped
# whole spell_ranks chains and made check 1 report their members as unclassifiable.
_ROW_INTS_RE = re.compile(r"^\s*(?:\(\s*\d+\s*(?:,\s*\d+\s*)*\)\s*,?\s*)+;?\s*$")
_TUPLE_INTS_RE = re.compile(r"\(\s*(\d+\s*(?:,\s*\d+\s*)*)\)")
_ROW_FIRST_INT_RE = re.compile(r"^\s*\(\s*(\d+)\s*,")
# ~124 hand-written trainer_spell rows carry a trailing `-- what this rank is` comment; without
# stripping it the row degraded to its first int (the TrainerId) and vanished from the check
# entirely — a silent false NEGATIVE in the very guard that is supposed to be exhaustive. Only
# applied when the stripped text is pure int tuples, so a spell_dbc row whose description text
# happens to contain `--` still takes the first-int path against the ORIGINAL line.
_TRAILING_SQL_COMMENT_RE = re.compile(r"\s--.*$")


def _module_rows(table: str):
    """Yield tuples of ints from every `INSERT INTO <table> ... VALUES` block in the module SQL
    (all-int rows only; a row with strings yields just its first int so spell_dbc ids resolve)."""
    for path in sorted(_MODULE_SQL_DIR.glob("*.sql")):
        current = None
        for ln in path.read_text().splitlines():
            m = _INSERT_HDR_RE.search(ln)
            if m:
                current = m.group(1)
                continue
            if current != table:
                continue
            if ln.strip().startswith("--") or not ln.lstrip().startswith("("):
                if ln.strip().endswith(";") and not ln.lstrip().startswith("("):
                    current = None
                continue
            stripped = _TRAILING_SQL_COMMENT_RE.sub("", ln)
            all_ints = _ROW_INTS_RE.match(stripped)
            if all_ints:
                for m2 in _TUPLE_INTS_RE.finditer(stripped):
                    yield tuple(int(x) for x in m2.group(1).split(","))
            else:
                mf = _ROW_FIRST_INT_RE.match(ln)
                if mf:
                    yield (int(mf.group(1)),)
            # A block-final row may carry a trailing comment too — miss the `;` and `current`
            # leaks into the next statement's continuation lines. Only trust the comment-stripped
            # text for this test when the all-int regex actually matched the line: a string-bearing
            # spell_dbc row can carry a literal `;` in its own text before a ` --` comment, and
            # stripping that comment would then make the stripped remainder end in `;` even though
            # the real row does not close the block.
            if ln.rstrip().endswith(";") or (all_ints and stripped.rstrip().endswith(";")):
                current = None


def check_band_ownership(findings: list):
    lo, hi = GB.BAND_LO, GB.BAND_HI
    in_band = lambda s: lo <= s < hi
    try:
        al = GB.load_allowlist(_BAND_ALLOWLIST)
    except GB.AllowlistError as ex:
        findings.append(f"band-allowlist: {ex}")
        return
    node_owner = {r[0]: (r[1], r[2]) for r in _module_rows("era_talent") if len(r) >= 3}   # talentId -> (era, class)
    grants: dict[int, set[tuple[int, int]]] = {}                                             # spell -> {(era, class)}
    for r in _module_rows("era_talent_rank"):
        if len(r) >= 3 and r[2] and in_band(r[2]) and r[0] in node_owner:
            grants.setdefault(r[2], set()).add(node_owner[r[0]])
    chain_root: dict[int, int] = {}                                                          # spell -> first_spell_id
    for r in _module_rows("spell_ranks"):
        if len(r) >= 2:
            chain_root[r[1]] = r[0]
    dbc_ids = {r[0] for r in _module_rows("spell_dbc")}
    trainer = [(r[0], r[1], r[5]) for r in _module_rows("trainer_spell") if len(r) >= 6]

    def classifiable(s: int) -> str | None:
        """'node' | 'chain' | 'allowlist' | None — mirrors EraBandClassifier's three sources."""
        if s in grants:
            return "node"
        root = chain_root.get(s, s)
        if root != s and in_band(root) and root in grants:
            return "chain"
        if al.entries_for(s):
            return "allowlist"
        return None

    # 1. trainer-taught band spells
    for tid, sid, req in trainer:
        if in_band(sid) and classifiable(sid) is None:
            findings.append(f"band-ownership: trainer_spell (TrainerId {tid}) teaches band spell {sid} "
                            f"that is neither a node grant, a spell_ranks rank above a node-granted r1, "
                            f"nor allowlisted — add a band-allowlist.yaml entry or fix the chain")
    # 2. band ReqAbility1 anchors
    for tid, sid, req in trainer:
        if in_band(req) and classifiable(req) is None:
            findings.append(f"band-ownership: trainer_spell ReqAbility1 {req} (teaching {sid}) is a band id "
                            f"that is neither a node grant, a chain member, nor allowlisted (a bare marker "
                            f"needs a `marker` entry)")
    # 3. chains
    # A chain only matters here if a character can LEARN some rank of it: the classifier never sees
    # a chain nobody holds. Three authored chains are pure non-stacking scaffolding for SCRIPT-CAST
    # payloads that are never learned and never trained (TBC druid Improved Faerie Fire companion
    # debuffs 946277-946279, TBC priest Misery debuffs 948073-948077, TBC hunter Wyvern Sting
    # wake-DoT 948334-948337) — allowlisting those would assert "a character may legitimately KNOW
    # this", which is false and would suppress a real orphan signal, so they are scoped out here
    # instead. Every learnable shape still reports (and check 1/2 flag the trainer side too).
    trainer_taught = {sid for _t, sid, _r in trainer}
    chain_members: dict[int, set[int]] = {}
    for s, root in chain_root.items():
        chain_members.setdefault(root, set()).add(s)
    for s, root in chain_root.items():
        if in_band(s) and not in_band(root):
            findings.append(f"band-ownership: spell_ranks member {s} has chain root {root} outside the band "
                            f"— a band clone must not be spliced into a stock chain")
        elif (in_band(root) and root not in grants and not al.entries_for(root)
              and any(m in grants or m in trainer_taught for m in chain_members[root])):
            findings.append(f"band-ownership: spell_ranks chain root {root} (member {s}) is neither a node "
                            f"grant nor allowlisted")
    # 4. allowlist hygiene
    for i, e in enumerate(al.entries, 1):
        for s in range(e.lo, e.hi + 1):
            if s not in dbc_ids:
                findings.append(f"band-ownership: allowlist entry #{i} id {s} has no spell_dbc row in the module SQL (typo?)")
            for era, cls in grants.get(s, ()):
                if era in e.eras and cls in e.classes:
                    findings.append(f"band-ownership: allowlist entry #{i} covers {s}, a node grant of the same "
                                    f"era {era} / class {cls} — it would mask the rank check; split the "
                                    f"range around it")
            root = chain_root.get(s, s)
            if root != s and in_band(root):
                for era, cls in grants.get(root, ()):
                    if era in e.eras and cls in e.classes:
                        findings.append(f"band-ownership: allowlist entry #{i} covers {s}, a spell_ranks rank above "
                                        f"node-granted root {root} of the same era {era} / class {cls} — rule 2 "
                                        f"already resolves it and the entry would keep it legit after a respec; "
                                        f"remove it from the allowlist")
    # 5. stranded regression list
    for s in al.stranded:
        how = classifiable(s)
        if how is not None:
            findings.append(f"band-ownership: stranded id {s} is classifiable via {how} — it would no longer be stripped; remove it from `stranded`")
    # WARN (not a finding): allowlist entries nothing references
    referenced = {sid for _t, sid, _r in trainer} | {r for _t, _s, r in trainer} | set(chain_root)
    cpp = "\n".join(p.read_text() for p in (_TOOLS.parent / "modules" / "mod-era-talents" / "src").glob("*.cpp"))
    for i, e in enumerate(al.entries, 1):
        ids = range(e.lo, e.hi + 1)
        if not any(s in referenced or re.search(rf"\b{s}\b", cpp) for s in ids):
            print(f"era_audit WARN: band-allowlist entry #{i} ({e.lo}-{e.hi}, {e.note}) is referenced by no "
                  f"trainer row, chain, or C++ literal", file=sys.stderr)


# ---------------------------------------------------------------------------

def main(argv: list[str]) -> int:
    paths = argv or DEFAULT_DATASETS
    datasets = [(p, G._load(p)) for p in paths]
    # Templates are needed by check 2 (templated helpers) and check 1b (proc affects-spell
    # damage-capability resolution); decode once.
    templates = None
    if any("template" in h for _p, d in datasets for h in (d.get("helpers", []) or [])) or \
       any(n.get("mechanic") in ("proc", "proc-scripted")
           for _p, d in datasets for _t, n in G._nodes(d)):
        templates = G.read_dbc_templates(G.DEFAULT_BASE_DBC)
    findings: list[str] = []
    for p, d in datasets:
        check_proc_sanity(p, d, findings)
        check_proc_damage_reachability(p, d, templates, findings)
        check_aura_desc_leaks(p, d, templates, findings)
        check_icon_names(p, d, findings)
        check_wand_spec_gating(p, d, findings)
        check_sweep_collisions(p, d, findings)
        check_shared_family_bits(p, d, findings)
        check_totem_scripts(p, d, findings)
        check_spellfix_inheritance([(p, d)], findings)
    # Cross-dataset: an era reuses the previous era's totem creatures/pulses, so linkage resolves
    # against the FULL manifest -- including datasets not named on this run -- while only the
    # datasets on the run are reported on (see the function's docstring).
    # Compare RESOLVED paths: DEFAULT_DATASETS holds absolute paths while argv is typically
    # relative, so a naive `not in` reloads the very dataset under audit and every one of its own
    # creatures then reports as "already declared by" itself.
    _seen = {pathlib.Path(p).resolve() for p in paths}
    resolution_only = [(p, G._load(p)) for p in DEFAULT_DATASETS
                       if pathlib.Path(p).resolve() not in _seen]
    check_totem_summons(datasets, findings, resolution_only)
    if len(paths) > 1:
        check_cross_dataset_ids(paths, findings)
    check_trainer_wrapper_leaks(templates, findings)
    check_band_ownership(findings)
    print(f"era_audit: {len(findings)} finding(s)")
    for f in findings:
        print(f"  {f}")
    return 1 if findings else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
