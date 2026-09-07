#!/usr/bin/env python3
"""Author + validate era bot build orders, then emit the kAuthoredOrders C++ table.

Reads EVERY dataset in era-data/datasets.txt (the manifest — same source of truth as
era-regen.sh / era_audit.py), so the table is keyed (era, class, specTab): a TBC-band bot
must find TBC node ids, never Vanilla ones (a key miss falls through to the greedy tier-fill
the table exists to prevent — every TBC class shipped with that miss until 2026-09-03).

Simulates EraTalents::TryLearn's exact legality rules (same-era node, in-tab prereqPoints,
prereqTalent at max rank, maxRank) with an UNLIMITED budget: an order that is skip-free under
unlimited budget is level-proportional-correct at every budget, because a budget cutoff just
truncates the walk (TryLearn stops on points, never skips a legal node). Any gate failure =
hard error. Each order must cover the era's full tree budget (Vanilla 51 / TBC 61 — the
AvailablePoints cap) so the top of the band never reaches the greedy fill either.
Also prints cumulative point counts so keystone timing is reviewable.

Coverage is enforced: every (era, class) with nodes in the manifest needs an order for
specTabs 0-2 (druid additionally 3 = playerbots' feral-cat pseudo-spec), so shipping a new
class dataset FAILS this tool until its orders are authored — author them, re-run, paste.
"""
import sys, pathlib, yaml

REPO = pathlib.Path(__file__).resolve().parents[1]
CLASS_IDS = {"WARRIOR":1,"PALADIN":2,"HUNTER":3,"ROGUE":4,"PRIEST":5,"SHAMAN":7,"MAGE":8,"WARLOCK":9,"DRUID":11}
# eraName (as written in the YAML) -> (C++ EraId symbol, AvailablePoints tree budget)
ERAS = {"Vanilla": ("ERA_VANILLA", 51), "TBC": ("ERA_TBC", 61), "WotLK": ("ERA_WOTLK", 71)}

def manifest_datasets():
    for line in (REPO/"era-data/datasets.txt").read_text().splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        yield REPO/"era-data"/line.split()[0]

nodes = {}       # id -> dict(tab, tier, maxRank, pp, pre, slug, cls, era)
present = set()  # (eraName, className) with >=1 node
for f in manifest_datasets():
    d = yaml.safe_load(f.read_text())
    cn, en = d["className"], d["eraName"]
    if en not in ERAS:
        sys.exit(f"{f}: unknown eraName {en!r} (add it to ERAS)")
    for tab in d["tabs"]:
        for n in tab["nodes"]:
            if n["id"] in nodes:
                sys.exit(f"{f}: duplicate node id {n['id']}")
            nodes[n["id"]] = dict(tab=tab["key"], tier=n["tier"], maxRank=n["maxRank"],
                                  pp=n.get("prereqPoints",0), pre=n.get("prereqTalentId",0),
                                  slug=n["slug"], cls=cn, era=en)
            present.add((en, cn))

# (eraName, className, specTab) -> ordered node ids, each learned to max in order.
# specTab is playerbots' 0-based tab; specTab 3 = the druid "cat" pseudo-spec (InitTalentsTree
# maps feral cats to 3; patch 0020 folds it to 1 on the factory path, module sites may pass 3).
BUILDS = {
 # =========================== VANILLA (nodes 18xxx) ===========================
 # ---- PRIEST ----
 ("Vanilla","PRIEST",0): [18102,18105,18104,18108,18107,18103,18110,18112,18113,18114,18115,18109,
                18118,18120,18117],
 ("Vanilla","PRIEST",1): [18118,18120,18117,18122,18125,18123,18128,18129,18130,18131,18124,18121,18116,  # Holy
                18102,18105,18104,18108],
 ("Vanilla","PRIEST",2): [18132,18136,18138,18139,18135,18142,18144,18145,18146,18147,18141,18134,18133,  # Shadow
                18102,18104,18105],
 # ---- MAGE ----
 ("Vanilla","MAGE",0): [18002,18006,18008,18009,18005,18012,18013,18014,18015,18016,18003,18001,  # Arcane
              18017,18019,18024,18018],
 ("Vanilla","MAGE",1): [18017,18019,18024,18018,18021,18026,18029,18030,18031,18032,18028,18025,18020,  # Fire
              18002,18006,18001],
 ("Vanilla","MAGE",2): [18034,18036,18038,18040,18045,18046,18044,18043,18048,18049,18047,18037,18035,18041,  # Frost
              18002,18006],
 # ---- WARLOCK ----
 ("Vanilla","WARLOCK",0): [18202,18205,18206,18207,18209,18208,18211,18213,18210,18216,18217,18212,18204,18203,
                 18235,18236,18237],
 ("Vanilla","WARLOCK",1): [18220,18219,18223,18225,18226,18228,18230,18227,18232,18233,18222,18224,  # Demonology
                 18235,18236,18237],
 ("Vanilla","WARLOCK",2): [18235,18236,18237,18241,18242,18248,18243,18244,18246,18247,18249,18250,18238,  # Destruction
                 18202,18205,18206],
 # ---- HUNTER ----
 ("Vanilla","HUNTER",0): [18301,18302,18309,18311,18313,18312,18310,18315,18316,18305,18308,18307,
                18318,18320,18321,18322],
 ("Vanilla","HUNTER",1): [18318,18320,18321,18322,18325,18327,18326,18329,18330,18323,18324,18319,
                18333,18334,18338],
 ("Vanilla","HUNTER",2): [18333,18335,18336,18338,18337,18340,18341,18343,18345,18346,18339,18344,18334,
                18318,18320,18321],
 # ---- ROGUE ----
 ("Vanilla","ROGUE",0): [18403,18404,18405,18407,18409,18410,18412,18411,18414,18415,18401,18402,
               18417,18418,18421,18420],
 ("Vanilla","ROGUE",1): [18417,18418,18421,18420,18423,18427,18430,18429,18433,18432,18434,18416,18422,  # Combat
               18403,18404,18405,18407],
 ("Vanilla","ROGUE",2): [18435,18436,18440,18442,18443,18445,18447,18449,18448,18450,18446,18451,18439,  # Subtlety
               18417,18418,18421],
 # ---- WARRIOR ----
 ("Vanilla","WARRIOR",0): [18503,18501,18505,18509,18508,18511,18510,18513,18515,18507,18518,18502,18504,  # Arms
                 18520,18522,18526],
 ("Vanilla","WARRIOR",1): [18520,18522,18526,18529,18531,18528,18525,18534,18535,18527,18524,18523,  # Fury
                 18502,18505,18503],
 ("Vanilla","WARRIOR",2): [18536,18537,18542,18543,18544,18545,18549,18538,18541,18551,18552,18547,18539,  # Protection
                 18502,18505],
 # ---- PALADIN ----
 ("Vanilla","PALADIN",0): [18602,18603,18605,18608,18609,18606,18610,18611,18612,18613,18614,18607,18601,
                 18615,18618,18619],
 ("Vanilla","PALADIN",1): [18616,18615,18622,18620,18623,18624,18626,18627,18628,18629,18619,18617,  # Protection
                 18601,18602],
 ("Vanilla","PALADIN",2): [18630,18631,18637,18636,18632,18634,18641,18643,18644,18642,18635,18640,18633,  # Retribution
                 18601,18602],
 # ---- SHAMAN ----
 ("Vanilla","SHAMAN",0): [18801,18800,18807,18804,18805,18810,18812,18811,18813,18814,18806,18803,
                18832,18831,18836],
 ("Vanilla","SHAMAN",1): [18815,18818,18822,18823,18824,18827,18826,18829,18830,18821,18825,
                18801,18800,18804],
 ("Vanilla","SHAMAN",2): [18831,18832,18835,18838,18840,18841,18843,18844,18845,18842,18837,  # Restoration
                18815,18816],
 # ---- DRUID ----
 ("Vanilla","DRUID",0): [18700,18704,18706,18709,18710,18712,18713,18707,18714,18711,18715,18701,18702,
               18732,18735,18733],
 ("Vanilla","DRUID",1): [18716,18720,18718,18723,18722,18725,18727,18728,18729,18730,18731,18721,18724,18717,  # Feral (bear/tank)
               18732,18733],
 ("Vanilla","DRUID",2): [18732,18735,18734,18738,18740,18742,18743,18745,18746,18741,18737,  # Restoration
               18700,18704,18706],
 ("Vanilla","DRUID",3): [18716,18717,18723,18719,18727,18725,18728,18726,18721,18730,18731,18722,18724,18718,18720,  # Feral (cat)
               18733,18732],

 # ============================= TBC (nodes 20xxx) =============================
 # 61-point budget: ~41-46 in the primary tree (keystones as early as their tier gate
 # allows, then filler), then a classic off-tree spill to >=61.
 # ---- PALADIN ----
 ("TBC","PALADIN",0): [20001,20002,20004,20007,20009,20008,20011,20005,20012,20014,20016,20015,20010,20018,20019,20017,  # Holy: Illumination@22 Divine Favor@23 Holy Shock@33 Divine Illumination@45
                 20020,20024,20023,20025],  # Prot spill: Imp Devotion, Toughness, Guardian's Favor, BoK
 ("TBC","PALADIN",1): [20021,20020,20026,20024,20028,20033,20027,20036,20038,20039,20037,20040,20041,20035,20023,  # Protection: BoSanc@24 Holy Shield@33 Avenger's Shield@46
                 20043,20046,20044],                                                                    # Ret spill: Benediction, Deflection, Imp Judgement
 ("TBC","PALADIN",2): [20043,20044,20045,20049,20048,20053,20050,20055,20056,20057,20060,20054,20059,20062,20063,20058,20042,  # Retribution: SoC@11 Sanctity@23 Repentance@31 Crusader Strike@43
                 20000,20001],  # Holy spill: Divine Strength, Divine Intellect
 # ---- DRUID ----
 ("TBC","DRUID",0): [20100,20104,20105,20103,20107,20108,20109,20112,20110,20111,20113,20114,20117,20116,20119,20120,20115,  # Balance: Insect Swarm@13 Nature's Grace@21 Moonkin@36 Force of Nature@45
               20143,20144,20146,20147],  # Resto spill: Furor, Naturalist, Natural Shapeshifter, Intensity
 ("TBC","DRUID",1): [20121,20125,20123,20127,20128,20130,20131,20133,20132,20126,20136,20135,20138,20137,20140,20141,20139,20124,20122,  # Feral (bear/tank): Feral Charge@12 HotW@33 LotP@34 Mangle@43
               20143,20144],  # Resto spill: Furor, Naturalist
 ("TBC","DRUID",2): [20142,20145,20147,20149,20144,20151,20152,20153,20156,20158,20157,20160,20161,20155,20159,20150,  # Restoration: Omen@14 NS@23 Swiftmend@34 Tree of Life@43
               20100,20104,20105],  # Balance spill: Starlight Wrath, Focused Starlight, Imp Moonfire
 ("TBC","DRUID",3): [20121,20122,20128,20127,20126,20129,20130,20131,20132,20133,20135,20136,20138,20139,20140,20141,20123,20124,20125,  # Feral (cat): Primal Fury@23 HotW@31 LotP@35 Mangle@43
               20143,20144],  # Resto spill: Furor, Naturalist
 # ---- SHAMAN ----
 ("TBC","SHAMAN",0): [20201,20200,20205,20207,20204,20209,20212,20211,20213,20215,20216,20214,20218,20219,20217,  # Elemental: Elemental Focus@11 Elemental Fury@23 Elemental Mastery@36 Totem of Wrath@45
                20242,20245,20246],  # Resto spill: Tidal Focus, Totemic Focus, Nature's Guidance
 ("TBC","SHAMAN",1): [20220,20223,20227,20226,20222,20229,20232,20233,20231,20235,20237,20238,20236,20234,20239,20240,  # Enhancement: Shamanistic Focus@11 Flurry@20 Dual Wield@32 Stormstrike@33 Shamanistic Rage@45
                20201,20200,20205,20207],  # Ele spill: Concussion, Convection, Elemental Focus, Call of Thunder
 ("TBC","SHAMAN",2): [20242,20241,20248,20245,20250,20253,20251,20255,20256,20252,20258,20259,20260,20246,20244,20247,  # Restoration: Totemic Mastery@11 NS@22 Mana Tide@33 Earth Shield@42
                20200,20203,20202],                                                                   # Ele spill: Convection, Elemental Warding, Earth's Grasp
 # ---- WARRIOR ----
 ("TBC","WARRIOR",0): [20300,20301,20303,20308,20307,20302,20310,20309,20312,20314,20318,20319,20320,20321,20322,20306,20316,  # Arms: Anger Mgmt@14 Death Wish@25 Mortal Strike@33 Endless Rage@41
                 20324,20326,20325],  # Fury spill: Cruelty, Unbridled Wrath, Imp Demo Shout
 ("TBC","WARRIOR",1): [20324,20326,20330,20333,20335,20331,20338,20340,20336,20341,20342,20343,20339,20332,20337,  # Fury: Sweeping Strikes@21 Flurry@31 Bloodthirst@32 Rampage@42
                 20300,20301,20303,20308],                                                            # Arms spill: Imp Heroic Strike, Deflection, Imp Charge, Deep Wounds
 ("TBC","WARRIOR",2): [20346,20347,20350,20352,20349,20348,20357,20351,20353,20360,20362,20363,20364,20365,20359,20361,20345,20344,  # Protection: Last Stand@15 Concussion Blow@21 Shield Slam@33 Devastate@42
                 20300,20301,20303],                                                                  # Arms spill: Imp Heroic Strike, Deflection, Imp Charge
 # ---- ROGUE ----
 ("TBC","ROGUE",0): [20402,20403,20404,20406,20408,20405,20410,20412,20415,20417,20409,20419,20420,20416,20414,20400,20411,  # Assassination: Relentless Strikes@11 Cold Blood@25 Seal Fate@30 Vigor@31 Mutilate@42
               20423,20426,20424],                                                                    # Combat spill: Lightning Reflexes, Precision, Imp Slice and Dice
 ("TBC","ROGUE",1): [20422,20423,20426,20424,20432,20434,20435,20438,20439,20441,20440,20437,20443,20444,20427,20442,20421,  # Combat (swords): Blade Flurry@21 Adrenaline Rush@32 Surprise Attacks@42
               20402,20403,20404,20406,20408],                                                        # Assassination spill: Malice, Ruthlessness, Murder, Relentless Strikes, Lethality
 ("TBC","ROGUE",2): [20445,20446,20450,20452,20455,20448,20457,20459,20453,20460,20461,20463,20465,20466,20458,20454,20464,20447,  # Subtlety: Preparation@22 Hemorrhage@23 Premeditation@35 Shadowstep@41
               20402,20403,20404,20406,20408],                                                        # Assassination spill: Malice, Ruthlessness, Murder, Relentless Strikes, Lethality
 # ---- PRIEST ----
 ("TBC","PRIEST",0): [20500,20502,20503,20504,20507,20508,20510,20512,20513,20514,20516,20515,20518,20519,20520,20521,  # Discipline: Inner Focus@16 Divine Spirit@30 Power Infusion@40 Pain Suppression@51
                20522,20523,20524],                                                                   # Holy spill: Healing Focus, Imp Renew, Holy Specialization
 ("TBC","PRIEST",1): [20524,20523,20522,20526,20527,20529,20531,20530,20532,20535,20534,20533,20537,20536,20538,20539,20541,20542,  # Holy: Holy Nova@16 Spirit of Redemption@32 Lightwell@45 Circle of Healing@51
                20500,20503,20504],                                                                   # Discipline spill: Unbreakable Will, Imp PW:F, Imp PW:S
 ("TBC","PRIEST",2): [20543,20547,20546,20545,20550,20549,20548,20553,20552,20555,20556,20557,20554,20559,20560,20561,20562,20563,  # Shadow: Mind Flay@16 VE@31 Silence@37 Shadowform@43 Vampiric Touch@54
                20500,20503],                                                                         # Discipline spill: Unbreakable Will, Imp PW:F
 # ---- HUNTER ----
 ("TBC","HUNTER",0): [20600,20601,20602,20604,20608,20610,20609,20612,20611,20613,20615,20614,20617,20616,20619,20620,  # BM: Intimidation@26 Frenzy@32 Bestial Wrath@40 The Beast Within@49
                20622,20624,20623],                                                                   # MM spill: Lethal Shots, Efficiency, Imp. Hunter's Mark (truncates at 61)
 ("TBC","HUNTER",1): [20622,20624,20623,20627,20626,20630,20629,20632,20633,20631,20635,20634,20637,20636,20638,20639,20640,  # MM: Aimed Shot@16 Scatter Shot@32 Trueshot@43 Silencing Shot@55
                20643],                                                                               # SV spill: Hawk Eye
 ("TBC","HUNTER",2): [20642,20643,20644,20646,20645,20649,20650,20652,20651,20655,20656,20654,20658,20657,20660,20661,20659,20662,20663,  # SV: Deterrence@22 Counterattack@31 Wyvern Sting@42 Readiness@54
                20622,20600],                                                                         # MM/BM spill: Lethal Shots, Imp. Aspect of the Hawk (both tier 1)
 # ---- WARLOCK ----
 ("TBC","WARLOCK",0): [20701,20700,20704,20705,20702,20708,20706,20710,20711,20713,20714,20712,20715,20717,20716,20719,20720,  # Affliction: Amplify@11 Siphon Life@22 Dark Pact@32 UA@41
                20743,20744,20745],                                                                          # Destruction spill: ISB, Cataclysm, Bane
 ("TBC","WARLOCK",1): [20723,20722,20726,20725,20729,20728,20730,20732,20731,20734,20735,20737,20736,20739,20740,20738,20741,20742,  # Demonology: Fel Dom@18 Dem Sac@29 MD@36 Soul Link@40 Felguard@52
                20701,20700],                                                                                # Affliction spill
 ("TBC","WARLOCK",2): [20743,20744,20745,20749,20750,20752,20751,20756,20755,20758,20760,20761,20759,20762,20763,  # Destruction: Shadowburn@21 Ruin@26 Conflagrate@37 Shadowfury@49
                20701,20700,20704],                                                                          # Affliction spill
 # ---- MAGE ----
 ("TBC","MAGE",0): [20801,20802,20805,20807,20808,20811,20810,20813,20814,20816,20817,20819,20820,20818,20821,20822,  # Arcane: PoM@25 Arcane Power@37 Slow@48
                20823,20825,20826,20827],                                                                    # Fire spill: Imp. Fireball, Ignite, Flame Throwing, Imp. Fire Blast (truncates at 61)
 ("TBC","MAGE",1): [20823,20824,20825,20830,20828,20831,20834,20832,20836,20837,20835,20839,20841,20842,20840,20843,20844,  # Fire: Pyroblast@16 Blast Wave@30 Combustion@39 Dragon's Breath@50
                20801,20802,20805],                                                                          # Arcane spill (truncates at 61)
 ("TBC","MAGE",2): [20846,20847,20848,20850,20853,20852,20857,20856,20859,20858,20862,20863,20864,20865,20866,20849,  # Frost: Icy Veins@16 Shatter 5/5@24 Cold Snap@28 Ice Barrier@37 Water Elemental@48
                20801,20802],                                                                                # Arcane spill
}

fail = False

# Coverage: every dataset in the manifest has authored orders for tabs 0-2 (+3 for druid).
for (en, cn) in sorted(present):
    for tab in (0, 1, 2, 3) if cn == "DRUID" else (0, 1, 2):
        if (en, cn, tab) not in BUILDS:
            print(f"FAIL {en}/{cn}/{tab}: dataset in manifest but no authored build order"); fail = True
for (en, cn, tab) in BUILDS:
    if (en, cn) not in present:
        print(f"FAIL {en}/{cn}/{tab}: authored order for a class/era with no dataset in the manifest"); fail = True

for (en, cn, tab), order in sorted(BUILDS.items()):
    budget = ERAS[en][1]
    ranks = {}                      # id -> learned rank
    tab_spent = {}                  # tabKey -> points
    total = 0
    marks = []
    seen = set()
    for nid in order:
        n = nodes.get(nid)
        if not n or n["cls"] != cn or n["era"] != en:
            print(f"FAIL {en}/{cn}/{tab}: {nid} not a {en} {cn} node"); fail = True; continue
        if nid in seen:
            print(f"FAIL {en}/{cn}/{tab}: {n['slug']} ({nid}) listed twice"); fail = True; continue
        seen.add(nid)
        for _ in range(n["maxRank"]):
            if tab_spent.get(n["tab"], 0) < n["pp"]:
                print(f"FAIL {en}/{cn}/{tab}: {n['slug']} ({nid}) needs {n['pp']} in tab {n['tab']}, "
                      f"has {tab_spent.get(n['tab'],0)} (cum total {total})"); fail = True; break
            if n["pre"] and ranks.get(n["pre"], 0) != nodes[n["pre"]]["maxRank"]:
                print(f"FAIL {en}/{cn}/{tab}: {n['slug']} ({nid}) prereq {nodes[n['pre']]['slug']} not maxed"); fail = True; break
            ranks[nid] = ranks.get(nid, 0) + 1
            tab_spent[n["tab"]] = tab_spent.get(n["tab"], 0) + 1
            total += 1
        else:
            marks.append(f"{n['slug']}@{total}")
            continue
        break
    # The requested tab must hold the most points (the spec-tab bridge reads max tab as spec).
    prim = tab + 1 if tab != 3 else 2
    if tab_spent and max(tab_spent, key=tab_spent.get) != prim:
        print(f"FAIL {en}/{cn}/{tab}: primary tab {prim} is not the max tab ({tab_spent})"); fail = True
    cover = "OK " if total >= budget else f"SHORT({total}) "
    if total < budget:
        print(f"FAIL {en}/{cn}/{tab}: order only covers {total} points (< {budget} budget)"); fail = True
    print(f"{cover}{en} {cn} tab{tab}: {total}pts | " + " ".join(marks))

if fail:
    sys.exit("ORDER VALIDATION FAILED")

# Emit the C++ table.
out = []
out.append("    const std::map<std::tuple<uint8, uint8, int>, std::vector<uint32>> kAuthoredOrders = {")
for (en, cn, tab), order in sorted(BUILDS.items(), key=lambda kv: (list(ERAS).index(kv[0][0]), CLASS_IDS[kv[0][1]], kv[0][2])):
    slugs = ", ".join(nodes[i]["slug"] for i in order[:4])
    out.append(f"        // {en} {cn.title()} specTab {tab}: {slugs}, ...")
    ids = ", ".join(str(i) for i in order)
    out.append(f"        {{ {{ {ERAS[en][0]}, CLASS_{cn}, {tab} }}, {{ {ids} }} }},")
out.append("    };")
print("\n----- C++ table -----")
print("\n".join(out))
