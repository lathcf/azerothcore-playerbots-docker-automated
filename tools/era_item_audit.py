#!/usr/bin/env python3
"""era_item_audit.py — do item set bonuses / equip effects still affect era-talent clones?

Mechanism: item bonus spells carry ADD_FLAT_MODIFIER(107)/ADD_PCT_MODIFIER(108)/
ADD_TARGET_TRIGGER(109)/OVERRIDE_CLASS_SCRIPTS(112) auras whose EffectSpellClassMask
matches target spells by SpellFamilyName + SpellFamilyFlags (96-bit AND), and
PROC_TRIGGER_SPELL(42) auras whose proc condition (spell_proc row, or the effect's own
class mask) is the same match against the spell the wearer casts. Era clones replace
stock spells for era characters; if a clone's family/flags no longer intersect the
bonus mask, the item bonus silently stops working for era characters.

Replaced-spell detection is BY NAME: a stock spell that shares its enUS name with a
[920000,933000)-band spell_dbc row is treated as replaced, and each same-name clone is
tested against the bonus mask (OK / PARTIAL / BROKEN per clone).

CAVEATS (adjudicate the raw rows before believing them — see
docs/verification/era-talents-vanilla-item-audit.md for the worked 2026-08-26 pass):
  * SpellInfoCorrections.cpp can rewrite a bonus spell's masks/auras at runtime; this
    script reads the raw DBC. Cross-check flagged ids against the corrections file.
  * A BROKEN row is only real if the missed clone is the half that carries the modded
    quantity (cost mods bind the castable/summon, radius/allEffects bind the pulse,
    duration binds the aura...). Summon-vs-pulse and castable-vs-payload splits produce
    benign BROKEN rows.
  * NPC variants of player spell names produce false "stock matched" baselines.

Inputs, extracted into WORKDIR (argv[1]) beforehand:
  Spell.dbc, ItemSet.dbc   — docker exec ac-worldserver cat /azerothcore/env/dist/data/dbc/<f> > <f>
  clones.tsv    — SELECT ID, SpellClassSet, SpellClassMask_1, SpellClassMask_2, SpellClassMask_3,
                         EffectAura_1, EffectAura_2, EffectAura_3, Name_Lang_enUS
                  FROM spell_dbc WHERE ID>=920000 AND ID<933000   (mysql -N -B)
  items.tsv     — SELECT entry, class, subclass, RequiredLevel, ItemLevel, spellid_1, spelltrigger_1,
                         spellid_2, spelltrigger_2, spellid_3, spelltrigger_3, itemset, name
                  FROM item_template WHERE (itemset>0) OR (spellid_1>0 AND spelltrigger_1=1)
                        OR (spellid_2>0 AND spelltrigger_2=1) OR (spellid_3>0 AND spelltrigger_3=1)
  spell_proc.tsv        — SELECT SpellId, SchoolMask, SpellFamilyName, SpellFamilyMask0,
                                 SpellFamilyMask1, SpellFamilyMask2, ProcFlags, SpellTypeMask,
                                 SpellPhaseMask, HitMask FROM spell_proc
  spell_proc_event.tsv  — SELECT entry, SchoolMask, SpellFamilyName, SpellFamilyMask0,
                                 SpellFamilyMask1, SpellFamilyMask2, procFlags FROM spell_proc_event
                          (legacy — this fork only loads spell_proc; kept as a fallback signal)

Usage: python3 tools/era_item_audit.py <workdir>
"""
import struct, sys, collections, os

BASE = sys.argv[1] if len(sys.argv) > 1 else "."
COLS = os.path.join(os.path.dirname(os.path.abspath(__file__)), "spell_dbc_columns.txt")
TBC_ITEM_THRESHOLD = 23728  # first TBC item id (gear proxy used by patch 0020)

def u32(v): return v & 0xFFFFFFFF

# ---------- Spell.dbc ----------
colidx = {}
for line in open(COLS):
    parts = line.strip().split('\t')
    if len(parts) == 2:
        colidx[parts[1]] = int(parts[0])

def load_spell_dbc(path):
    data = open(path, 'rb').read()
    magic, nrec, nfield, recsize, strsize = struct.unpack_from('<4siiii', data, 0)
    assert magic == b'WDBC'
    strbase = 20 + nrec * recsize
    spells = {}
    gi = colidx  # shorthand
    need = ['ID','Name_Lang_enUS','SpellClassSet','SpellClassMask_1','SpellClassMask_2','SpellClassMask_3']
    # Per-effect class-mask column semantics (Spell.dbc AND the spell_dbc table): the LETTER is the
    # EFFECT index (A=effect1, B=effect2, C=effect3) and the _1/_2/_3 SUFFIX is the mask WORD —
    # effect 1's 96-bit mask is (EffectSpellClassMaskA_1, A_2, A_3). Reading (A_i, B_i, C_i) as one
    # effect's three words is the classic mis-map (it grabs word0 of all three effects instead).
    letter = {1: 'A', 2: 'B', 3: 'C'}
    eff = [(gi[f'Effect_{i}'], gi[f'EffectAura_{i}'], gi[f'EffectMiscValue_{i}'],
            gi[f'EffectBasePoints_{i}'], gi[f'EffectSpellClassMask{letter[i]}_1'],
            gi[f'EffectSpellClassMask{letter[i]}_2'], gi[f'EffectSpellClassMask{letter[i]}_3'],
            gi[f'EffectTriggerSpell_{i}']) for i in (1,2,3)]
    idc, namec, famc, m1c, m2c, m3c = (gi[n] for n in need)
    for r in range(nrec):
        off = 20 + r * recsize
        rec = struct.unpack_from(f'<{nfield}i', data, off)
        sid = rec[idc]
        nameoff = rec[namec]
        name = data[strbase+nameoff : data.index(b'\0', strbase+nameoff)].decode('utf-8', 'replace') if nameoff else ''
        effects = []
        for (e,a,m,bp,ma,mb,mc,tr) in eff:
            effects.append((rec[e], rec[a], rec[m], rec[bp], u32(rec[ma]), u32(rec[mb]), u32(rec[mc]), rec[tr]))
        spells[sid] = dict(name=name, family=rec[famc],
                           flags=(u32(rec[m1c]), u32(rec[m2c]), u32(rec[m3c])),
                           effects=effects)
    return spells

# ---------- ItemSet.dbc ----------
def load_itemset_dbc(path):
    data = open(path, 'rb').read()
    magic, nrec, nfield, recsize, strsize = struct.unpack_from('<4siiii', data, 0)
    assert magic == b'WDBC' and nfield == 53
    strbase = 20 + nrec * recsize
    sets = {}
    for r in range(nrec):
        off = 20 + r * recsize
        rec = struct.unpack_from('<53i', data, off)
        nameoff = rec[1]
        name = data[strbase+nameoff : data.index(b'\0', strbase+nameoff)].decode('utf-8','replace') if nameoff else ''
        items = [x for x in rec[18:35] if x]
        bonuses = [(rec[35+i], rec[43+i]) for i in range(8) if rec[35+i]]
        sets[rec[0]] = dict(name=name, items=items, bonuses=bonuses)
    return sets

print("loading Spell.dbc ...", file=sys.stderr)
spells = load_spell_dbc(f"{BASE}/Spell.dbc")
itemsets = load_itemset_dbc(f"{BASE}/ItemSet.dbc")

# ---------- DB dumps ----------
items = {}
for line in open(f"{BASE}/items.tsv"):
    f = line.rstrip('\n').split('\t')
    items[int(f[0])] = dict(cls=int(f[1]), sub=int(f[2]), reqlvl=int(f[3]), ilvl=int(f[4]),
        spells=[(int(f[5]),int(f[6])),(int(f[7]),int(f[8])),(int(f[9]),int(f[10]))],
        itemset=int(f[11]), name=f[12])

clones = {}
for line in open(f"{BASE}/clones.tsv"):
    f = line.rstrip('\n').split('\t')
    clones[int(f[0])] = dict(family=int(f[1]),
        flags=(u32(int(f[2])), u32(int(f[3])), u32(int(f[4]))),
        auras=(int(f[5]),int(f[6]),int(f[7])), name=f[8])

clones_by_name = collections.defaultdict(list)
for cid, c in clones.items():
    clones_by_name[c['name']].append(cid)

# stock spells that share a name with a clone = potentially replaced
replaced_names = set(clones_by_name)
stock_by_name = collections.defaultdict(list)
for sid, s in spells.items():
    if s['name'] in replaced_names:
        stock_by_name[s['name']].append(sid)

def mask_hits(family, mask3, spell):
    if spell['family'] != family or family == 0:
        return False
    return any(m & f for m, f in zip(mask3, spell['flags']))

def clone_mask_hits(family, mask3, clone):
    if clone['family'] != family or family == 0:
        return False
    return any(m & f for m, f in zip(mask3, clone['flags']))

MOD_AURAS = {107: 'FLAT_MOD', 108: 'PCT_MOD', 109: 'ADD_TARGET_TRIGGER', 112: 'OVERRIDE_SCRIPT'}
REVIEW_AURAS = {4: 'DUMMY', 42: 'PROC_TRIGGER_SPELL'}

# ---------- collect vanilla bonus-spell sources ----------
def set_is_vanilla(s):
    known = [i for i in s['items'] if i in items]
    if not known:
        return False
    return max(known) < TBC_ITEM_THRESHOLD

sources = []  # (kind, label, bonus_spell_id)
for setid, s in itemsets.items():
    if not set_is_vanilla(s):
        continue
    for spellid, thresh in s['bonuses']:
        sources.append(('set', f"[set {setid}] {s['name']} ({thresh}pc)", spellid))

for entry, it in items.items():
    if entry >= TBC_ITEM_THRESHOLD:
        continue
    for spellid, trig in it['spells']:
        if spellid > 0 and trig in (0, 1):  # ON_USE, ON_EQUIP
            sources.append(('item', f"[item {entry}] {it['name']}", spellid))

# ---------- analysis ----------
report = []
review = []
seen_reviews = set()
for kind, label, bsid in sources:
    b = spells.get(bsid)
    if not b:
        continue
    for ei, (eff, aura, misc, bp, ma, mb, mc, trig) in enumerate(b['effects'], 1):
        if aura in MOD_AURAS:
            mask3 = (ma, mb, mc)
            if not any(mask3):
                continue
            fam = b['family']
            # which replaced stock spells does this target?
            hit_names = set()
            for name, sids in stock_by_name.items():
                if any(mask_hits(fam, mask3, spells[sid]) for sid in sids):
                    hit_names.add(name)
            for name in sorted(hit_names):
                cl = clones_by_name[name]
                ok = [cid for cid in cl if clone_mask_hits(fam, mask3, clones[cid])]
                bad = [cid for cid in cl if cid not in ok]
                status = 'OK' if not bad else ('PARTIAL' if ok else 'BROKEN')
                report.append((status, label, bsid, b['name'], MOD_AURAS[aura], ei, misc, name,
                               sorted(ok), sorted(bad)))
        elif aura in REVIEW_AURAS and (kind, bsid, ei) not in seen_reviews:
            seen_reviews.add((kind, bsid, ei))
            review.append((label, bsid, b['name'], REVIEW_AURAS[aura], ei, misc, trig, b['family']))

# ---------- pass 2: aura-42 proc conditions ----------
# proc condition resolution order (AC): spell_proc row -> spell_proc_event row ->
# the proc aura effect's own EffectSpellClassMask (family = the bonus spell's own family).
spell_proc = {}
for line in open(f"{BASE}/spell_proc.tsv"):
    f = [int(x) for x in line.split('\t')]
    spell_proc[abs(f[0])] = dict(fam=f[2], mask=(u32(f[3]), u32(f[4]), u32(f[5])))
spell_proc_event = {}
for line in open(f"{BASE}/spell_proc_event.tsv"):
    f = [int(x) for x in line.split('\t')]
    spell_proc_event[f[0]] = dict(fam=f[2], mask=(u32(f[3]), u32(f[4]), u32(f[5])))

proc_report = []
proc_unfiltered = []
seen = set()
for kind, label, bsid in sources:
    b = spells.get(bsid)
    if not b or bsid in seen:
        continue
    seen.add(bsid)
    has42 = any(a == 42 for (_, a, *_rest) in [(e[0], e[1]) for e in b['effects']])
    if not any(e[1] == 42 for e in b['effects']):
        continue
    # resolve the proc condition
    cond = None; src = None
    if bsid in spell_proc and spell_proc[bsid]['fam']:
        cond = spell_proc[bsid]; src = 'spell_proc'
    elif bsid in spell_proc_event and spell_proc_event[bsid]['fam']:
        cond = spell_proc_event[bsid]; src = 'spell_proc_event'
    else:
        # per-effect class mask on the aura-42 effect itself
        for ei, e in enumerate(b['effects'], 1):
            if e[1] == 42 and any(e[4:7]):
                cond = dict(fam=b['family'], mask=(e[4], e[5], e[6])); src = f'dbc-effmask eff{ei}'
                break
    if not cond:
        proc_unfiltered.append((label, bsid, b['name']))
        continue
    fam, mask3 = cond['fam'], cond['mask']
    hit_names = set()
    for name, sids in stock_by_name.items():
        if any(mask_hits(fam, mask3, spells[sid]) for sid in sids):
            hit_names.add(name)
    if not hit_names:
        proc_report.append(('NO-REPLACED-TARGET', label, bsid, b['name'], src, fam, mask3, '', [], []))
        continue
    for name in sorted(hit_names):
        cl = clones_by_name[name]
        ok = [cid for cid in cl if clone_mask_hits(fam, mask3, clones[cid])]
        bad = [cid for cid in cl if cid not in ok]
        status = 'OK' if not bad else ('PARTIAL' if ok else 'BROKEN')
        proc_report.append((status, label, bsid, b['name'], src, fam, mask3, name, sorted(ok), sorted(bad)))

# ---------- output ----------
order = {'BROKEN': 0, 'PARTIAL': 1, 'OK': 2}
report.sort(key=lambda r: (order[r[0]], r[1]))
print(f"# Mask-based (spellmod) bonuses whose target spell has an era clone: {len(report)} rows")
for status, label, bsid, bname, kind, ei, misc, tname, ok, bad in report:
    print(f"{status:7} {label} :: bonus {bsid} '{bname}' {kind} eff{ei} misc={misc} -> target '{tname}' okClones={ok} badClones={bad}")

print(f"\n# Aura-42 proc-condition analysis: {len(proc_report)} rows")
proc_report.sort(key=lambda r: ({'BROKEN':0,'PARTIAL':1,'OK':2,'NO-REPLACED-TARGET':3}[r[0]], r[1]))
for status, label, bsid, bname, src, fam, mask3, tname, ok, bad in proc_report:
    if status == 'NO-REPLACED-TARGET':
        print(f"{status:18} {label} :: bonus {bsid} '{bname}' via {src} fam={fam} mask={mask3}")
    else:
        print(f"{status:18} {label} :: bonus {bsid} '{bname}' via {src} fam={fam} -> trigger-spell '{tname}' okClones={ok} badClones={bad}")
print(f"\n# Aura-42 with NO family filter (fire on generic proc flags — era-safe): {len(proc_unfiltered)}")
for label, bsid, bname in proc_unfiltered:
    print(f"UNFILTERED {label} :: bonus {bsid} '{bname}'")

print(f"\n# Manual-review bonuses (dummy/override) on vanilla items: {len(review)} rows")
for label, bsid, bname, kind, ei, misc, trig, fam in sorted(review, key=lambda r: (r[3], r[0])):
    if kind == 'PROC_TRIGGER_SPELL':
        continue  # handled by pass 2
    print(f"{kind:22} {label} :: bonus {bsid} '{bname}' eff{ei} misc={misc} trig={trig} fam={fam}")
