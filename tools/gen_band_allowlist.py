#!/usr/bin/env python3
"""era-data/band-allowlist.yaml -> modules/mod-era-talents/src/EraBandAllowlist.gen.h

The allowlist is the ONLY place a NON-node era-band spell's legitimacy is written down (markers,
trainer-taught chains whose root is itself trainer-taught, reconcile-granted partners, level
grants, proc passives). Node grants and spell_ranks members above a node-granted r1 must NOT be
listed — EraBandClassifier resolves those from content, and era_audit.py's check_band_ownership
fails an entry that overlaps a node grant of the same era+class (it would mask the rank check).

Usage: gen_band_allowlist.py era-data/band-allowlist.yaml -o modules/mod-era-talents/src/EraBandAllowlist.gen.h
"""
# NB: deliberately NO `from __future__ import annotations` — this module is loaded by
# era_audit.py / the tests via importlib.spec_from_file_location, which does NOT register it in
# sys.modules, and dataclasses' string-annotation path resolves ClassVar/KW_ONLY through
# sys.modules[cls.__module__] (AttributeError on None). Real annotation objects sidestep it.
import argparse, dataclasses, hashlib, pathlib, re, sys
import yaml

BAND_LO, BAND_HI = 920000, 950000          # keep in sync with EraTalents.h
ERA_IDS = {"vanilla": 0, "tbc": 1, "wotlk": 2}
CLASS_IDS = {"warrior": 1, "paladin": 2, "hunter": 3, "rogue": 4, "priest": 5, "deathknight": 6,
             "shaman": 7, "mage": 8, "warlock": 9, "druid": 11}
KINDS = ("marker", "trainer", "partner", "level-grant", "proc-passive")
HASH_RE = re.compile(r'kEraBandAllowlistHash = "([0-9a-f]+)"')


class AllowlistError(ValueError):
    pass


@dataclasses.dataclass
class Entry:
    lo: int
    hi: int
    era_mask: int
    class_mask: int
    node_by_era: list[int]          # index = EraId, 0 = ungated
    kind: str
    note: str
    eras: set[int]
    classes: set[int]

    def covers(self, spell_id: int) -> bool:
        return self.lo <= spell_id <= self.hi


@dataclasses.dataclass
class Allowlist:
    entries: list[Entry]
    stranded: list[int]
    sha: str                        # 12-hex prefix of sha256(yaml bytes)

    def entries_for(self, spell_id: int) -> list[Entry]:
        return [e for e in self.entries if e.covers(spell_id)]


def _err(i: int, msg: str) -> AllowlistError:
    return AllowlistError(f"band-allowlist.yaml entry #{i}: {msg}")


def _parse_entry(i: int, raw: dict) -> Entry:
    if not isinstance(raw, dict):
        raise _err(i, "not a mapping")
    if ("ids" in raw) == ("range" in raw):
        raise _err(i, "exactly one of ids/range is required (both ids and range, or neither)")
    if "ids" in raw:
        ids = raw["ids"]
        if not isinstance(ids, list) or not ids or not all(isinstance(x, int) for x in ids):
            raise _err(i, "ids must be a non-empty list of ints")
        lo, hi = min(ids), max(ids)
        if hi - lo + 1 != len(set(ids)):
            # non-contiguous ids: emit one entry per contiguous run would be nicer, but the
            # C++ walks [lo,hi]; refuse so an author cannot accidentally cover a hole.
            raise _err(i, f"ids must be contiguous (got {sorted(ids)}); split into two entries")
    else:
        rng = raw["range"]
        if not (isinstance(rng, list) and len(rng) == 2 and all(isinstance(x, int) for x in rng)):
            raise _err(i, "range must be [lo, hi]")
        lo, hi = rng
        if hi < lo:
            raise _err(i, f"range hi < lo ({lo}, {hi})")
    if not (BAND_LO <= lo < BAND_HI and BAND_LO <= hi < BAND_HI):
        raise _err(i, f"ids {lo}-{hi} are outside the band [{BAND_LO},{BAND_HI})")
    eras = raw.get("eras")
    if not isinstance(eras, list) or not eras:
        raise _err(i, "eras must be a non-empty list")
    era_ids = set()
    for e in eras:
        if str(e).lower() not in ERA_IDS:
            raise _err(i, f"unknown era token {e!r} (vanilla/tbc/wotlk)")
        era_ids.add(ERA_IDS[str(e).lower()])
    classes = raw.get("classes")
    if not isinstance(classes, list) or not classes:
        raise _err(i, "classes must be a non-empty list")
    class_ids = set()
    for c in classes:
        if str(c).lower() not in CLASS_IDS:
            raise _err(i, f"unknown class token {c!r}")
        class_ids.add(CLASS_IDS[str(c).lower()])
    kind = raw.get("kind")
    if kind not in KINDS:
        raise _err(i, f"unknown kind {kind!r} (one of {', '.join(KINDS)})")
    note = raw.get("note")
    if not isinstance(note, str) or not note.strip():
        raise _err(i, "note is required")
    node_by_era = [0, 0, 0]
    for tok, node in (raw.get("node") or {}).items():
        if str(tok).lower() not in ERA_IDS:
            raise _err(i, f"node for unknown era {tok!r}")
        eid = ERA_IDS[str(tok).lower()]
        if eid not in era_ids:
            raise _err(i, f"node for an unlisted era {tok!r}")
        if not isinstance(node, int) or node <= 0:
            raise _err(i, f"node id for {tok!r} must be a positive int")
        node_by_era[eid] = node
    return Entry(lo, hi, sum(1 << e for e in era_ids), sum(1 << c for c in class_ids),
                 node_by_era, kind, note.strip(), era_ids, class_ids)


def load_allowlist(path) -> Allowlist:
    p = pathlib.Path(path)
    data = p.read_bytes()
    doc = yaml.safe_load(data) or {}
    raw_entries = doc.get("entries")
    if not isinstance(raw_entries, list) or not raw_entries:
        raise AllowlistError("band-allowlist.yaml: 'entries' must be a non-empty list")
    entries = [_parse_entry(i, r) for i, r in enumerate(raw_entries, 1)]
    stranded = doc.get("stranded") or []
    if not isinstance(stranded, list) or not all(isinstance(x, int) for x in stranded):
        raise AllowlistError("band-allowlist.yaml: 'stranded' must be a list of ints")
    for s in stranded:
        if any(e.covers(s) for e in entries):
            raise AllowlistError(f"band-allowlist.yaml: stranded id {s} is allowlisted — pick one")
    return Allowlist(entries, stranded, hashlib.sha256(data).hexdigest()[:12])


def _cstr(s: str) -> str:
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


def emit_header(al: Allowlist) -> str:
    out = [
        f"// GENERATED by tools/gen_band_allowlist.py from era-data/band-allowlist.yaml (sha256 {al.sha}) — DO NOT EDIT.",
        "// Regenerate with tools/era-regen.sh. Read by EraBandClassifier (rule 3). Node grants and",
        "// spell_ranks members above a node-granted r1 are deliberately absent: content resolves them.",
        "#pragma once",
        "#include \"Define.h\"",
        "#include <cstddef>",
        "",
        "struct EraBandAllowEntry",
        "{",
        "    uint32 lo;              // inclusive id range",
        "    uint32 hi;",
        "    uint8  eraMask;         // bit = EraId (1 Vanilla, 2 TBC, 4 WotLK)",
        "    uint32 classMask;       // bit = CLASS_* id",
        "    uint32 nodeByEra[3];    // gating node per EraId, 0 = ungated in that era",
        "    char const* note;",
        "};",
        "",
        f'inline constexpr char const* kEraBandAllowlistHash = "{al.sha}";',
        f"inline constexpr size_t kEraBandAllowlistCount = {len(al.entries)};",
        "inline constexpr EraBandAllowEntry kEraBandAllowlist[] =",
        "{",
    ]
    for e in al.entries:
        nodes = ", ".join(str(n) for n in e.node_by_era)
        out.append(f"    {{ {e.lo}, {e.hi}, 0x{e.era_mask:x}, 0x{e.class_mask:x}, {{ {nodes} }}, {_cstr(e.note)} }},")
    out += ["};", ""]
    return "\n".join(out)


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("yaml")
    ap.add_argument("-o", "--out", required=True)
    a = ap.parse_args(argv)
    try:
        al = load_allowlist(a.yaml)
    except AllowlistError as ex:
        print(f"gen_band_allowlist: {ex}", file=sys.stderr)
        return 1
    pathlib.Path(a.out).write_text(emit_header(al))
    print(f"gen_band_allowlist: {len(al.entries)} entries, {len(al.stranded)} stranded, sha {al.sha} -> {a.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
