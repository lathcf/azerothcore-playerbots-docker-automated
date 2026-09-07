#!/usr/bin/env python3
"""dump_spell_effects.py — print selected columns of chosen spells from the binary WotLK
Spell.dbc, so era-talents genuine-aura nodes can copy a real analog talent's exact effect
encoding (Effect_N / EffectAura_N / EffectMiscValue_N / EffectBasePoints_N / class masks)
instead of guessing. Stdlib only. Companion to dump_spell_family.py (same WDBC layout).

Usage:
    python3 tools/dump_spell_effects.py Spell.dbc cols.txt --ids 15058,15059 \
        [--cols Effect_1,EffectAura_1,...]   # default: the effect-relevant columns
"""
import argparse
import struct

HEADER_FORMAT = "<4siiii"
HEADER_SIZE = struct.calcsize(HEADER_FORMAT)

DEFAULT_COLS = []
for _i in (1, 2, 3):
    DEFAULT_COLS += [f"Effect_{_i}", f"EffectAura_{_i}", f"EffectBasePoints_{_i}",
                     f"EffectDieSides_{_i}", f"EffectMiscValue_{_i}", f"EffectMiscValueB_{_i}",
                     f"EffectTriggerSpell_{_i}",
                     f"EffectSpellClassMask{'ABC'[_i-1]}_1",
                     f"EffectSpellClassMask{'ABC'[_i-1]}_2",
                     f"EffectSpellClassMask{'ABC'[_i-1]}_3"]
DEFAULT_COLS += ["SpellClassSet", "SchoolMask", "Attributes", "DurationIndex"]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("dbc")
    ap.add_argument("cols")
    ap.add_argument("--ids", required=True)
    ap.add_argument("--cols", dest="want")
    a = ap.parse_args()
    names = [ln.split()[1] for ln in open(a.cols).read().splitlines() if ln.strip()]
    want = a.want.split(",") if a.want else DEFAULT_COLS
    ids = {int(x) for x in a.ids.split(",")}
    idx_of = {n: i for i, n in enumerate(names)}
    id_col = idx_of["ID"]
    with open(a.dbc, "rb") as f:
        magic, rec_count, field_count, rec_size, _sb = struct.unpack(
            HEADER_FORMAT, f.read(HEADER_SIZE))
        assert magic == b"WDBC", magic
        data = f.read(rec_count * rec_size)
    for r in range(rec_count):
        rec = data[r * rec_size:(r + 1) * rec_size]
        fields = struct.unpack(f"<{field_count}i", rec)
        sid = fields[id_col]
        if sid in ids:
            print(f"# spell {sid}")
            for c in want:
                print(f"  {c} = {fields[idx_of[c]]}")


if __name__ == "__main__":
    main()
