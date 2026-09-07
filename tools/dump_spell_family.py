#!/usr/bin/env python3
"""dump_spell_family.py — extract SpellClassSet (family) + SpellClassMask_1/2/3
for one or more spells out of the binary WotLK Spell.dbc.

Family flags (SpellClassMask) are NOT present in the sparse `spell_dbc` SQL
table shipped with AzerothCore — they only live in the client/server DBC
itself. This tool reads the DBC directly so era-talents datasets can be
authored against live-verified values instead of guesses.

WDBC layout:
    header (20 bytes): magic("WDBC"), recordCount, fieldCount, recordSize,
                        stringBlockSize   (all little-endian int32)
    recordCount * recordSize bytes of records, each fieldCount little-endian
        int32 fields (string fields hold a byte offset into the trailing
        string block instead of the string itself)
    stringBlockSize bytes of NUL-terminated UTF-8 strings, starting right
        after the last record

Column names come from a companion text file where line i (0-indexed) is
"<index>\t<Name>" for field i of the record — e.g. AzerothCore's
`spell_dbc_columns.txt`. The column name for the spell id is "ID"; the family
fields are "SpellClassSet" and "SpellClassMask_1"/"_2"/"_3"; the localized
English name is "Name_Lang_enUS".

Usage:
    python3 tools/dump_spell_family.py Spell.dbc cols.txt --ids 133,116
    python3 tools/dump_spell_family.py Spell.dbc cols.txt --names Fireball,Frostbolt
    python3 tools/dump_spell_family.py Spell.dbc cols.txt --ids 133 --json

Stdlib only — no external dependencies.
"""
import argparse
import json
import struct
import sys

HEADER_FORMAT = "<4siiii"
HEADER_SIZE = struct.calcsize(HEADER_FORMAT)


def load_columns(cols_path):
    cols = []
    with open(cols_path, "r", encoding="utf-8") as f:
        for line in f:
            line = line.rstrip("\n")
            if not line.strip():
                continue
            parts = line.split()
            # "<index>\t<Name>" -> second whitespace token is the name
            cols.append(parts[1])
    return cols


class SpellDbc:
    def __init__(self, dbc_path, cols_path):
        self.cols = load_columns(cols_path)
        with open(dbc_path, "rb") as f:
            self.data = f.read()

        magic, record_count, field_count, record_size, string_block_size = (
            struct.unpack_from(HEADER_FORMAT, self.data, 0)
        )
        if magic != b"WDBC":
            raise ValueError(f"not a WDBC file (magic={magic!r})")
        if field_count != len(self.cols):
            raise ValueError(
                f"column file has {len(self.cols)} columns but DBC field_count is "
                f"{field_count} — wrong columns file for this DBC?"
            )

        self.record_count = record_count
        self.field_count = field_count
        self.record_size = record_size
        self.string_block_size = string_block_size
        self.string_block_offset = HEADER_SIZE + record_count * record_size

        self.idx = {name: i for i, name in enumerate(self.cols)}

        required = ["ID", "SpellClassSet", "SpellClassMask_1", "SpellClassMask_2", "SpellClassMask_3"]
        missing = [c for c in required if c not in self.idx]
        if missing:
            raise ValueError(f"columns file is missing required column(s): {missing}")

    def _record(self, row):
        offset = HEADER_SIZE + row * self.record_size
        return struct.unpack_from("<%di" % self.field_count, self.data, offset)

    def _read_string(self, offset):
        if offset == 0:
            return ""
        start = self.string_block_offset + offset
        end = self.data.index(b"\x00", start)
        return self.data[start:end].decode("utf-8", errors="replace")

    def _name_column(self):
        for candidate in ("Name_Lang_enUS", "Name_enUS", "Name_Lang_1"):
            if candidate in self.idx:
                return candidate
        raise ValueError("no recognized name column found in columns file")

    def iter_rows(self):
        for row in range(self.record_count):
            yield self._record(row)

    def find_by_ids(self, ids):
        wanted = set(ids)
        found = {}
        id_i = self.idx["ID"]
        for rec in self.iter_rows():
            sid = rec[id_i]
            if sid in wanted:
                found[sid] = rec
                if len(found) == len(wanted):
                    break
        return found

    def find_by_names(self, names):
        name_col = self._name_column()
        name_i = self.idx[name_col]
        wanted = {n.lower(): n for n in names}
        found = {}
        for rec in self.iter_rows():
            offset = rec[name_i]
            s = self._read_string(offset)
            key = s.lower()
            if key in wanted:
                found.setdefault(wanted[key], []).append(rec)
        return found

    def record_info(self, rec):
        name_col = self._name_column()
        name_i = self.idx[name_col]
        return {
            "id": rec[self.idx["ID"]],
            "name": self._read_string(rec[name_i]),
            "family": rec[self.idx["SpellClassSet"]],
            "mask": [
                rec[self.idx["SpellClassMask_1"]],
                rec[self.idx["SpellClassMask_2"]],
                rec[self.idx["SpellClassMask_3"]],
            ],
        }


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("dbc_path", help="path to Spell.dbc")
    ap.add_argument("cols_path", help="path to the DBC column-names text file")
    ap.add_argument("--ids", help="comma-separated list of spell ids")
    ap.add_argument("--names", help="comma-separated list of exact spell names (Name_Lang_enUS)")
    ap.add_argument("--json", action="store_true", help="emit JSON instead of plain lines")
    args = ap.parse_args()

    if not args.ids and not args.names:
        ap.error("pass --ids and/or --names")

    dbc = SpellDbc(args.dbc_path, args.cols_path)

    results = []  # list of (key, info) preserving requested order where possible

    if args.ids:
        ids = [int(x.strip()) for x in args.ids.split(",") if x.strip()]
        found = dbc.find_by_ids(ids)
        for sid in ids:
            if sid in found:
                results.append(dbc.record_info(found[sid]))
            else:
                print(f"WARNING: id {sid} not found in DBC", file=sys.stderr)

    if args.names:
        names = [x.strip() for x in args.names.split(",") if x.strip()]
        found = dbc.find_by_names(names)
        for name in names:
            recs = found.get(name)
            if not recs:
                print(f"WARNING: name '{name}' not found in DBC", file=sys.stderr)
                continue
            for rec in recs:
                results.append(dbc.record_info(rec))

    if args.json:
        out = {}
        for info in results:
            out[info["id"]] = {"name": info["name"], "family": info["family"], "mask": info["mask"]}
        print(json.dumps(out, indent=2, ensure_ascii=False))
    else:
        for info in results:
            m = info["mask"]
            print(f"{info['id']}\t{info['name']}\tfamily={info['family']}\tmask=({m[0]},{m[1]},{m[2]})")


if __name__ == "__main__":
    main()
