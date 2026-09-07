#!/usr/bin/env python3
"""Build a client-side SkillLineAbility.dbc that ADDS rows mapping era-talents' custom CASTABLE
spells (Power Infusion 920920, Vampiric Embrace 921152) to a priest skill line, on top of a base
SkillLineAbility.dbc.

WHY: the 3.3.5a client decides which spellbook TAB a known spell goes in (General / Discipline /
Holy / Shadow) from SkillLineAbility.dbc (spell -> skill line) + the skill line's category. A custom
spell with no SkillLineAbility row can't be categorized, so the client dumps it in General. Adding a
row (SkillLine = the class-spec skill line, ClassMask = the class) puts it in the right tab.

This is the SkillLineAbility analogue of build_client_dbc.py (which does Spell.dbc). It reads every
helper carrying a `client:` block with a `skillLine:` field and emits one row per such spell. Like
the Spell.dbc merge, a row whose Spell id already exists in the base is REPLACED in place (so a
re-merge onto an already-merged archive is idempotent); a new id is appended.

SkillLineAbility.dbc 3.3.5a layout: 14 int32 fields, 56-byte records, and a (minimal) string block
that carries NO field references here — we keep the base's string block verbatim. Field layout used
(copied from the stock PI/VE rows): [ID, SkillLine, Spell, RaceMask=0, ClassMask, 0, 0,
MinSkillLineRank=1, 0, 0, 0, 0, 0, 0].

Usage:
    python3 tools/build_client_skilllineability.py --dataset era-data/vanilla/priest.yaml \
        --base-dbc azerothcore-wotlk/ip-dbc/SkillLineAbility.dbc --out /tmp/SkillLineAbility.dbc
"""
from __future__ import annotations
import argparse
import pathlib
import struct
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import gen_era_talents as G  # noqa: E402

HEADER_FMT = "<4siiii"
HEADER_SIZE = struct.calcsize(HEADER_FMT)
FIELD_COUNT = 14
RECORD_SIZE = FIELD_COUNT * 4  # 56
ID_OFF = 0
SPELL_FIELD = 2


def _rows(dataset_paths: list[str]) -> list[list[int]]:
    """One SkillLineAbility record (list of 14 ints) per helper carrying `client.skillLine`."""
    out: list[list[int]] = []
    for p in dataset_paths:
        d = G._load(p)
        class_id = int(d.get("class", 0))
        class_mask = (1 << (class_id - 1)) if class_id else 0
        for h in d.get("helpers", []) or []:
            c = h.get("client") or {}
            sl = c.get("skillLine")
            if sl is None:
                continue
            spell = int(h["id"])
            # ID = spell id: our custom ids (>=920000) sit far above every stock SkillLineAbility id,
            # so they're collision-free and deterministic.
            out.append([spell, int(sl), spell, 0, class_mask, 0, 0, 1, 0, 0, 0, 0, 0, 0])
        # Dataset-level `extraSkillLines:` — STOCK spells a node GRANTS that lack a stock
        # SkillLineAbility row (a talent-granted spell like Demonic Sacrifice 18788 was categorized
        # by the stock TALENT frame, not SkillLineAbility, so granting it as a regular spell dumps
        # it in the General tab). Each entry {spell, skillLine} appends/replaces a row exactly like
        # a helper's client.skillLine. Row ID = 900000 + spell keeps clear of stock row ids while
        # staying deterministic (a custom-band spell id is its own row id above; a stock spell id
        # would collide with real SkillLineAbility ids).
        for x in d.get("extraSkillLines", []) or []:
            spell = int(x["spell"])
            out.append([900000 + spell, int(x["skillLine"]), spell, 0, class_mask, 0, 0, 1, 0, 0, 0, 0, 0, 0])
    return out


def build(dataset_paths: list[str], base_dbc: pathlib.Path, out: pathlib.Path) -> int:
    raw = base_dbc.read_bytes()
    magic, rec_count, field_count, rec_size, str_size = struct.unpack(HEADER_FMT, raw[:HEADER_SIZE])
    if magic != b"WDBC" or field_count != FIELD_COUNT or rec_size != RECORD_SIZE:
        raise SystemExit(f"unexpected base DBC: magic={magic} fields={field_count} recsize={rec_size}")
    rec_start = HEADER_SIZE
    str_start = rec_start + rec_count * rec_size
    records = bytearray(raw[rec_start:str_start])
    str_block = raw[str_start:str_start + str_size]

    custom = _rows(dataset_paths)
    if not custom:
        raise SystemExit("no helpers with a `client.skillLine` found in the datasets")

    # index existing rows by Spell field so a re-merge REPLACES instead of duplicating
    pos_by_spell = {
        struct.unpack_from("<i", records, i * rec_size + SPELL_FIELD * 4)[0]: i
        for i in range(rec_count)
    }
    appended = 0
    for row in custom:
        encoded = struct.pack("<14i", *row)
        spell = row[SPELL_FIELD]
        if spell in pos_by_spell:
            i = pos_by_spell[spell]
            records[i * rec_size:(i + 1) * rec_size] = encoded
        else:
            records += encoded
            appended += 1

    total = rec_count + appended
    header = struct.pack(HEADER_FMT, b"WDBC", total, FIELD_COUNT, RECORD_SIZE, str_size)
    out.write_bytes(header + bytes(records) + str_block)
    return appended


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dataset", nargs="+", required=True)
    ap.add_argument("--base-dbc", required=True)
    ap.add_argument("--out", required=True)
    a = ap.parse_args()
    n = build(a.dataset, pathlib.Path(a.base_dbc), pathlib.Path(a.out))
    print(f"wrote {a.out} (+{n} custom SkillLineAbility rows)")


if __name__ == "__main__":
    main()
