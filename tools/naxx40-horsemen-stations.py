#!/usr/bin/env python3
"""naxx40-horsemen-stations.py — author the Naxx40 Four Horsemen tanking corners, tank ready spots,
Blaumeux's circle, ranged/healer spots and the safe spot (patch 0022, spec 2026-10-27 §5).

Needs a character (GM is fine) parked in the Horsemen's room on map 533, so the probe has a path owner
and the grids are loaded. Run from the overlay repo root:

  python3 tools/naxx40-horsemen-stations.py gen > /tmp/nx40hm-probe.cmds
  bash -c 'mapfile -t C < /tmp/nx40hm-probe.cmds; WGCONSOLE_PUMP=0.4 python3 tools/wgconsole.py /tmp/nx40hm-probe.log "${C[@]}"'
  python3 tools/naxx40-horsemen-stations.py pick /tmp/nx40hm-probe.log     # writes JSON + verify cmds + the C++ header
  bash -c 'mapfile -t C < /tmp/nx40hm-verify.cmds; WGCONSOLE_PUMP=1 python3 tools/wgconsole.py /tmp/nx40hm-verify.log "${C[@]}"'
  python3 tools/naxx40-horsemen-stations.py verify /tmp/nx40hm-verify.log

`pick` checks every spec §5 constraint and, if any fails, prints `FAIL <constraint> <horse> <best distance>`,
exits 1 and writes NOTHING (no JSON, no header). It never relaxes a constraint. `verify` re-checks the same
constraints from the JSON plus reachability of every spot from the probe log.
"""
import heapq
import json
import math
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
JSON_OUT = os.path.join(ROOT, "tools", "naxx40-horsemen-stations.json")
HEADER_OUT = os.path.join(ROOT, "azerothcore-wotlk", "modules", "mod-playerbots", "src", "Ai", "Raid", "Naxx",
                          "Naxx40HorsemenStations.h")
VERIFY_CMDS = "/tmp/nx40hm-verify.cmds"

MAP_ID = 533
SPAWN = (2520.0, -2950.0, 241.64)        # platform spawn: reachability is measured from here
LEASH_CENTER = (2535.1, -2968.7, 241.3)  # IP IsInRoom: a horseman > 100 y from here evades
LEASH_RADIUS = 100.0
FLOOR_Z = LEASH_CENTER[2]
SAME_FLOOR_DZ = 3.0
GRID_STEP = 5.0
LOS_STEP = 2.0

MARK_RADIUS = 55.0                       # keep in sync with Naxx40HorsemenRules.h
DRIFT = 5.0                              # how far a boss drifts off its spot while being tanked
SEP = MARK_RADIUS + DRIFT                # every player spot must be farther than this from other tank spots
MELEE_RING = 4.0                         # melee stand within this of the tank spot (16 angles checked)
READY_MARGIN = DRIFT + 1.5               # ready spot: > MARK_RADIUS + this from the boss's own tank spot (his drift
                                         # plus the 1.5 y player reach the core adds to an area search); 1.0 let a
                                         # Mograine mark reach the ready tank (pull 4)
TANK_LEASH_MAX = 95.0
RANGED_MAX = 30.0
HEALER_MAX = 34.0                        # inside the bots' 36 y tank-heal reach (Naxx40HorsemenRules HEAL_REACH)
RANGED_MIN = 12.0                        # builder choices (spec §7): ranged/healers stay off the melee clump
HEALER_MIN = 10.0
ZELIEK_TANK_GAP = 12.0                   # keep in sync with Naxx40HorsemenRules.h (Holy Wrath jumps 10 y)
ZELIEK_SUPPORT_MIN = ZELIEK_TANK_GAP + 1.0   # Zeliek only: ranged/healer spots at least this from his tank spot
RANGED_TARGET = 20.0
HEALER_TARGET = 28.0
CLEARANCE = 5.0                          # ranged/healer/safe: no orthogonal probe neighbour is non-floor
BLAU_RADIUS_MAX = 15.0                   # Blaumeux's tank points: probe-grid floor within this of her tank spot
BLAU_MIN_POINTS = 6                      # a Void Zone lasts 90 s and she casts one every 15 s: up to 6 alive at once.
                                         # 9 before pull 10; 6 is all that clears the other corners' ranged/healer spots
                                         # by SEP from her original tank spot (doable mode casts no zones at all)
BLAU_ARRIVE = 2.5                        # keep in sync with NaxxActions_Horsemen40.cpp (her tank stands this close)
SEED_RADIUS = 20.0                       # tank spot candidates: floor within this of the WotLK corner endpoint
MAX_COMBO_CHECKS = 5000                  # full placements tried, in order of total drift from the seeds

HORSES = ["Korthazz", "Mograine", "Blaumeux", "Zeliek"]   # index = Naxx40HorsemenRules::Horse
DISPLAY = {"Korthazz": "Korth'azz", "Mograine": "Mograine", "Blaumeux": "Blaumeux", "Zeliek": "Zeliek"}
SEEDS = {"Korthazz": (2542.9, -3015.0), "Mograine": (2583.9, -2971.6),
         "Blaumeux": (2469.4, -2947.6), "Zeliek": (2517.8, -2896.6)}
PHASE2 = ["Blaumeux", "Zeliek"]          # the spirits of Korth'azz/Mograine stand where those two die
FRONT = ["Korthazz", "Mograine"]
HEALER_PAIRS = [("Korthazz", "Mograine"), ("Blaumeux", "Zeliek")]   # healer groups trade these corners (spec §4.3)
# The guides' safe spot: the middle of the room, in front of the platform steps, out of all four marks.
# It is the centre of the four WotLK corner seeds; the safe spot and every ready spot must be inside it,
# so tank spots are pushed into their corners as far as that needs (owner decision 2026-10-27).
MIDDLE = (sum(s[0] for s in SEEDS.values()) / 4, sum(s[1] for s in SEEDS.values()) / 4)
MIDDLE_RADIUS = 12.0

PROBE_RE = re.compile(r"\.bgbots probe 533 (\S+) (\S+) (\S+) (\S+) (\S+) (\S+)")
VERDICT_RE = re.compile(
    r"probe: (?:(REACHABLE|BLOCKED) calc=\d type=0x[0-9a-f]+ points=(\d+) "
    r"end=\(([-\d.]+),([-\d.]+),([-\d.]+)\)|(NO GROUND|no loaded instance|no player))")


# ---------------------------------------------------------------- probe log

def parse(log_path):
    """Pair every sent probe with its verdict line, in order (same contract as bwl-nefarian-stations.py)."""
    text = open(log_path, "rb").read().decode("utf-8", "replace")
    cmds = [tuple(float(v) for v in m.groups()) for m in PROBE_RE.finditer(text) if ">>> SENDING" in
            text[max(0, m.start() - 20):m.start()]]
    verdicts = list(VERDICT_RE.finditer(text))
    if len(cmds) != len(verdicts):
        sys.exit(f"parse: {len(cmds)} probes sent but {len(verdicts)} verdicts — re-run with a larger WGCONSOLE_PUMP")
    out = []
    for c, v in zip(cmds, verdicts):
        if v.group(6) in ("no loaded instance", "no player"):
            sys.exit(f"parse: probe failed outright ({v.group(6)}) — is a character on map {MAP_ID}?")
        if v.group(6):
            out.append({"to": c[:3], "from": c[3:], "verdict": "NO GROUND", "points": 0, "end": c[:3]})
            continue
        out.append({"to": c[:3], "from": c[3:], "verdict": v.group(1), "points": int(v.group(2)),
                    "end": (float(v.group(3)), float(v.group(4)), float(v.group(5)))})
    return out


def grid_cells():
    k = int(LEASH_RADIUS // GRID_STEP)
    return [(i, j) for i in range(-k, k + 1) for j in range(-k, k + 1)
            if math.hypot(i * GRID_STEP, j * GRID_STEP) <= LEASH_RADIUS]


def probe_line(to):
    return (f".bgbots probe {MAP_ID} {to[0]:.2f} {to[1]:.2f} {to[2]:.2f} "
            f"{SPAWN[0]:.2f} {SPAWN[1]:.2f} {SPAWN[2]:.2f}")


def cmd_gen():
    cx, cy = LEASH_CENTER[0], LEASH_CENTER[1]
    for i, j in grid_cells():
        print(probe_line((cx + i * GRID_STEP, cy + j * GRID_STEP, FLOOR_Z)))


# ---------------------------------------------------------------- floor map

class Floor:
    def __init__(self, results):
        cx, cy = LEASH_CENTER[0], LEASH_CENTER[1]
        self.cx, self.cy = cx, cy
        self.cells = {}
        for r in results:
            cell = (round((r["to"][0] - cx) / GRID_STEP), round((r["to"][1] - cy) / GRID_STEP))
            ok = (r["verdict"] == "REACHABLE" and math.dist(r["end"][:2], r["to"][:2]) < 1.5
                  and abs(r["end"][2] - FLOOR_Z) < SAME_FLOOR_DZ)
            self.cells[cell] = r["end"] if ok else None
        self.blocked = [(cx + i * GRID_STEP, cy + j * GRID_STEP) for (i, j), v in self.cells.items() if v is None]
        # Grid points that are floor (x, y, z), the search domain for every on-grid spot.
        self.points = [(cx + i * GRID_STEP, cy + j * GRID_STEP, v[2]) for (i, j), v in sorted(self.cells.items())
                       if v is not None]

    def cell(self, p):
        return (round((p[0] - self.cx) / GRID_STEP), round((p[1] - self.cy) / GRID_STEP))

    def on_floor(self, p):
        return self.cells.get(self.cell(p)) is not None

    def z_at(self, p):
        v = self.cells.get(self.cell(p))
        return v[2] if v else FLOOR_Z

    def clearance(self, p):
        return min((math.dist(p[:2], b) for b in self.blocked), default=math.inf)

    def line_of_floor(self, a, b):
        steps = max(1, int(math.dist(a[:2], b[:2]) / LOS_STEP))
        return all(self.on_floor((a[0] + (b[0] - a[0]) * t / steps, a[1] + (b[1] - a[1]) * t / steps))
                   for t in range(steps + 1))


# ---------------------------------------------------------------- constraint model

def ring_points(tank, radius=MELEE_RING, n=16):
    return [(tank[0] + radius * math.cos(2 * math.pi * k / n), tank[1] + radius * math.sin(2 * math.pi * k / n))
            for k in range(n)]


def player_spots(h, st):
    """Every place a player of horseman h's corner can stand (the ready spot is a waiting spot, checked apart)."""
    hs = st["horses"][h]
    spots = [hs["tank"], hs["ranged"], hs["healer"]] + ring_points(hs["tank"])
    if h == "Blaumeux":
        spots += hs["blau"]
    return spots


def check_constraints(st):
    """Return [(constraint, horse, best_distance)] for every spec §5 constraint that fails. `best_distance` is the
    measured worst (smallest margin-relevant) distance, so the owner sees how far off it is."""
    fails = []
    tanks = {h: st["horses"][h]["tank"] for h in HORSES}
    for h in HORSES:
        hs = st["horses"][h]
        others = [o for o in HORSES if o != h]
        worst = min(math.dist(p[:2], tanks[o][:2]) for p in player_spots(h, st) for o in others)
        if not worst > SEP:
            fails.append(("separation", h, worst))
        rd = math.dist(hs["ready"][:2], tanks[h][:2])
        ro = min(math.dist(hs["ready"][:2], tanks[o][:2]) for o in others)
        if not rd > MARK_RADIUS + READY_MARGIN:
            fails.append(("ready-own-circle", h, rd))
        if not ro > SEP:       # drift counts for every other horseman (the K/M spirits too), not just +1
            fails.append(("ready-other-circle", h, ro))
        d = math.dist(hs["ready"][:2], MIDDLE)
        if not d <= MIDDLE_RADIUS:
            fails.append(("ready-middle", h, d))
        d = math.dist(hs["ranged"][:2], tanks[h][:2])
        if not d <= RANGED_MAX:
            fails.append(("ranged-range", h, d))
        d = math.dist(hs["healer"][:2], tanks[h][:2])
        if not d <= HEALER_MAX:
            fails.append(("healer-range", h, d))
        if h == "Blaumeux":    # every tank point: her marks stay off the waiting spots, her healers and ranged reach
            protect = [st["safe"]] + [st["horses"][o]["ready"] for o in HORSES]
            if len(hs["blau"]) < BLAU_MIN_POINTS:
                fails.append(("blau-points", h, float(len(hs["blau"]))))
            for p in hs["blau"]:
                d = min(math.dist(p[:2], x[:2]) for x in protect)
                if not d > MARK_RADIUS + READY_MARGIN:
                    fails.append(("blau-mark-reach", h, d))
                d = min(math.dist(p[:2], st["horses"][o][k][:2]) for o in HORSES if o != h for k in ("ranged", "healer"))
                if not d > SEP:
                    fails.append(("blau-support-sep", h, d))
                d = math.dist(p[:2], hs["healer"][:2])
                if not d <= HEALER_MAX - BLAU_ARRIVE:
                    fails.append(("blau-healer-range", h, d))
                d = math.dist(p[:2], hs["ranged"][:2])
                if not d <= RANGED_MAX:
                    fails.append(("blau-ranged-range", h, d))
                d = math.dist(p[:2], tanks[h][:2])
                if not d <= BLAU_RADIUS_MAX:
                    fails.append(("blau-radius", h, d))
        if h == "Zeliek":      # Holy Wrath's 10 y chain must not reach ranged / healers from his tank
            for kind in ("ranged", "healer"):
                d = math.dist(hs[kind][:2], tanks[h][:2])
                if not d >= ZELIEK_SUPPORT_MIN:
                    fails.append((f"zeliek-wrath-gap-{kind}", h, d))
        d = math.dist(tanks[h][:2], LEASH_CENTER[:2])
        if not d <= TANK_LEASH_MAX:
            fails.append(("leash", h, d))
    safe_d = min(math.dist(st["safe"][:2], t[:2]) for t in tanks.values())
    if not safe_d > SEP:
        fails.append(("safe-spot", "all", safe_d))
    d = math.dist(st["safe"][:2], MIDDLE)
    if not d <= MIDDLE_RADIUS:
        fails.append(("safe-middle", "all", d))
    for h in PHASE2:
        worst = min(math.dist(p[:2], tanks[f][:2]) for p in player_spots(h, st) for f in FRONT)
        if not worst > SEP:
            fails.append(("phase2-spirits", h, worst))
    return fails


def spot_list(st):
    """(label, spot) for every spot that must be reachable from the platform spawn, in a fixed order."""
    out = []
    for h in HORSES:
        hs = st["horses"][h]
        for key in ("tank", "ready", "ranged", "healer"):
            out.append((f"{h}.{key}", hs[key]))
        if h == "Blaumeux":
            for i, p in enumerate(hs["blau"]):
                out.append((f"{h}.blau{i}", p))
    out.append(("safe", st["safe"]))
    return out


# ---------------------------------------------------------------- search

def rnd(p, floor):
    return [round(p[0], 2), round(p[1], 2), round(floor.z_at(p), 2)]


def tank_candidates(floor):
    cands = {}
    for h in HORSES:
        sx, sy = SEEDS[h]
        cands[h] = [p for p in floor.points
                    if math.hypot(p[0] - sx, p[1] - sy) <= SEED_RADIUS
                    and math.dist(p[:2], LEASH_CENTER[:2]) <= TANK_LEASH_MAX]
    return cands


def drift_of(combo):
    return sum(math.hypot(p[0] - SEEDS[h][0], p[1] - SEEDS[h][1]) for h, p in zip(HORSES, combo))


def middle_points(floor):
    return [p for p in floor.points if math.dist(p[:2], MIDDLE) <= MIDDLE_RADIUS and floor.clearance(p) > CLEARANCE]


def combos(cands, thresh, mids):
    """Yield every tank-spot combination whose pairwise distances all exceed `thresh` and that leaves at least one
    middle point more than SEP from all four tank spots."""
    def rec(i, chosen, left):
        if i == len(HORSES):
            yield tuple(chosen)
            return
        for p in cands[HORSES[i]]:
            if all(math.dist(p[:2], q[:2]) > thresh for q in chosen):
                still = [m for m in left if math.dist(m[:2], p[:2]) > SEP]
                if still:
                    yield from rec(i + 1, chosen + [p], still)
    yield from rec(0, [], mids)


def support_candidates(floor, h, tanks, kind, avoid):
    """Every valid ranged or healer point for h: floor, clearance, line of floor to the tank spot, the kind's
    own-distance window, > SEP from every other tank spot) and >= 5 y from `avoid` (the other kind's
    spot, or None). Returns ([(point, own distance)], best min distance to another tank among geometric points)."""
    lo, hi = (RANGED_MIN, RANGED_MAX) if kind == "ranged" else (HEALER_MIN, HEALER_MAX)
    if h == "Zeliek":
        lo = max(lo, ZELIEK_SUPPORT_MIN)   # Holy Wrath's 10 y chain off his tank (check_constraints re-checks)
    out, best_other = [], 0.0
    for p in floor.points:
        d = math.dist(p[:2], tanks[h][:2])
        if d < lo or d > hi or floor.clearance(p) <= CLEARANCE:
            continue
        if avoid is not None and math.dist(p[:2], avoid[:2]) < 5.0:
            continue
        if not floor.line_of_floor(tanks[h], p):
            continue
        other = min(math.dist(p[:2], tanks[o][:2]) for o in HORSES if o != h)
        best_other = max(best_other, other)
        if other > SEP:
            out.append((p, d))
    return out, best_other


def place_pair(floor, a, b, tanks, horses, kind):
    """Ranged or healer spots for a pair of corners that the DPS halves and healer groups trade (spec §4.2-§4.3,
    §8.1 pull 3 and pull 10): the valid pair with the SHORTEST crossing between them, ties to the one nearest the
    kind's target distance from both tanks. A healer cannot cast while it runs, and a ranged DPS does no damage, so
    the crossing is dead time. Healers are placed first; ranged keep 5 y off the healer spot. Returns (pa, pb, best
    other) or (None, None, best min distance to another tank among geometrically valid points)."""
    target = RANGED_TARGET if kind == "ranged" else HEALER_TARGET
    other_kind = "healer" if kind == "ranged" else "ranged"
    ca, oa = support_candidates(floor, a, tanks, kind, horses[a][other_kind])
    cb, ob = support_candidates(floor, b, tanks, kind, horses[b][other_kind])
    if not ca or not cb:
        return None, None, min(oa, ob)
    key = lambda pq: (round(math.dist(pq[0][0][:2], pq[1][0][:2]), 2),
                      abs(pq[0][1] - target) + abs(pq[1][1] - target))
    (pa, _), (pb, _) = min(((x, y) for x in ca for y in cb), key=key)
    return pa, pb, min(oa, ob)


def place_ready(floor, h, tank, other_tanks):
    """Closest floor point to h's tank spot that is in the room's middle, outside h's own circle (> MARK_RADIUS + READY_MARGIN)
    and more than SEP (circle + drift) from every other horseman's tank spot. Returns (spot or None, best min distance to another tank among points outside h's own
    circle)."""
    best, best_d, best_other = None, math.inf, 0.0
    lim = MARK_RADIUS + READY_MARGIN
    for p in floor.points:
        if math.dist(p[:2], MIDDLE) > MIDDLE_RADIUS:
            continue
        d = math.dist(p[:2], tank[:2])
        if not d > lim:
            continue
        other = min(math.dist(p[:2], o[:2]) for o in other_tanks)
        best_other = max(best_other, other)
        if other > SEP and (d < best_d):
            best, best_d = p, d
    return best, best_other


def place_blau(floor, tank, other_tanks, protect, other_support, healer, ranged):
    """Blaumeux's tank points: every probe-grid floor point within BLAU_RADIUS_MAX of her tank spot, on a straight
    line of floor from it, more than SEP from the other tank spots and from the other corners' ranged and healer
    spots (`other_support`, pull 10), with her marks (from the point) off every `protect` spot, and within reach of
    her healer and ranged spots. Her tank steps to the nearest point clear of
    Void Zones (spec §4.2c). Ordered by distance from the tank spot (index 0 = the tank spot itself). Returns
    (points or None, how many qualified)."""
    pts = []
    for p in floor.points:
        if math.dist(p[:2], tank[:2]) > BLAU_RADIUS_MAX or not floor.line_of_floor(tank, p):
            continue
        if min(math.dist(p[:2], o[:2]) for o in other_tanks + other_support) <= SEP:
            continue
        if min(math.dist(p[:2], x[:2]) for x in protect) <= MARK_RADIUS + READY_MARGIN:
            continue
        if math.dist(p[:2], healer[:2]) > HEALER_MAX - BLAU_ARRIVE or not floor.line_of_floor(healer, p):
            continue
        if math.dist(p[:2], ranged[:2]) > RANGED_MAX:
            continue
        pts.append(p)
    pts.sort(key=lambda p: (round(math.dist(p[:2], tank[:2]), 3), p[0], p[1]))
    return (pts if len(pts) >= BLAU_MIN_POINTS else None), len(pts)


def place_safe(floor, tanks):
    best, best_key, best_min = None, None, 0.0
    for p in middle_points(floor):
        m = min(math.dist(p[:2], t[:2]) for t in tanks)
        best_min = max(best_min, m)
        if not m > SEP:
            continue
        key = (min(m - SEP, 12.0), -math.dist(p[:2], MIDDLE))
        if best_key is None or key > best_key:
            best, best_key = p, key
    return best, best_min


def try_combo(floor, combo):
    """Place everything else for one tank-spot combination. Returns (stations or None, [fail tuples])."""
    tanks = dict(zip(HORSES, combo))
    fails = []
    horses = {}
    for h in HORSES:
        others = [tanks[o] for o in HORSES if o != h]
        ready, y_best = place_ready(floor, h, tanks[h], others)
        if ready is None:
            fails.append(("ready", h, y_best))
        horses[h] = {"tank": rnd(tanks[h], floor), "ready": rnd(ready, floor) if ready else None,
                     "ranged": None, "healer": None, "blau": None}
    for kind in ("healer", "ranged"):
        for a, b in HEALER_PAIRS:
            pa, pb, best = place_pair(floor, a, b, tanks, horses, kind)
            if pa is None:
                fails.append((kind, f"{a}-{b}", best))
                continue
            horses[a][kind], horses[b][kind] = rnd(pa, floor), rnd(pb, floor)
    safe, s_best = place_safe(floor, list(combo))
    if safe is None:
        fails.append(("safe-spot", "all", s_best))
    if fails:
        return None, fails
    b = horses["Blaumeux"]
    protect = [safe] + [horses[o]["ready"] for o in HORSES]
    others = [o for o in HORSES if o != "Blaumeux"]
    support = [horses[o][k] for o in others for k in ("ranged", "healer")]
    blau, b_count = place_blau(floor, tanks["Blaumeux"], [tanks[o] for o in others], protect, support,
                               b["healer"], b["ranged"])
    if blau is None:
        fails.append(("blau-points", "Blaumeux", float(b_count)))
    else:
        b["blau"] = [rnd(p, floor) for p in blau]
    if fails:
        return None, fails
    return {"horses": horses, "safe": rnd(safe, floor)}, []


def search(floor):
    cands = tank_candidates(floor)
    fails = []
    for h in HORSES:
        if not cands[h]:
            fails.append(("tank-floor", h, 0.0))
    if fails:
        return None, fails

    thresh = SEP + MELEE_RING          # tank-tank; the 4 y melee ring is then > SEP from the other tank too
    mids = middle_points(floor)
    if not mids:
        return None, [("middle-floor", "all", 0.0)]
    pool = heapq.nsmallest(MAX_COMBO_CHECKS, combos(cands, thresh, mids), key=drift_of)
    if not pool:
        # Find how close the geometry gets: lower the threshold until a joint placement exists.
        t = thresh
        while t > 0:
            t -= 1.0
            first = next(combos(cands, t, mids), None)
            if first is not None:
                pair = min(((math.dist(first[i][:2], first[j][:2]), HORSES[i], HORSES[j])
                            for i in range(4) for j in range(i + 1, 4)))
                return None, [("tank-separation", f"{pair[1]}-{pair[2]}", pair[0])]
        return None, [("tank-separation", "all", 0.0)]

    best_fails = None
    for combo in pool:
        st, fails = try_combo(floor, combo)
        if st is not None:
            bad = check_constraints(st)       # backstop: the search and the checker must agree
            if not bad:
                return st, []
            fails = bad
        if best_fails is None or len(fails) < len(best_fails):
            best_fails = fails
    return None, best_fails


# ---------------------------------------------------------------- output

def render_header(st, provisional=False, note=None):
    def sp(p):
        return f"{{ {p[0]:.2f}f, {p[1]:.2f}f, {p[2]:.2f}f }}"

    lines = [
        "// GENERATED by tools/naxx40-horsemen-stations.py — do not hand-edit (spec 2026-10-27 §5).",
    ]
    if note:
        lines.append(note)
    lines += [
        "#ifndef PLAYERBOTS_NAXX40HORSEMENSTATIONS_H",
        "#define PLAYERBOTS_NAXX40HORSEMENSTATIONS_H",
        "namespace Naxx40HorsemenStations",
        "{",
        "    struct Spot { float x, y, z; };",
        "    struct HorseSpots { Spot tankSpot, readySpot, rangedSpot, healerSpot; };",
        f"    constexpr bool PROVISIONAL = {'true' if provisional else 'false'};",
        "    // Index = Naxx40HorsemenRules::Horse: 0 Korth'azz, 1 Mograine, 2 Blaumeux, 3 Zeliek.",
        "    constexpr HorseSpots HORSE[4] = {",
    ]
    for n, h in enumerate(HORSES):
        hs = st["horses"][h]
        comma = "," if n < len(HORSES) - 1 else ""
        lines.append(f"        // {DISPLAY[h]}")
        lines.append(f"        {{ {sp(hs['tank'])}, {sp(hs['ready'])}, {sp(hs['ranged'])}, {sp(hs['healer'])} }}{comma}")
    blau = st["horses"]["Blaumeux"]["blau"]
    lines += [
        "    };",
        "    // Blaumeux's tank points, nearest her tank spot first (index 0 = the tank spot); her tank steps to the",
        "    // nearest one clear of Void Zones.",
        f"    constexpr unsigned BLAU_POINT_COUNT = {len(blau)};",
        "    constexpr Spot BLAU_POINTS[BLAU_POINT_COUNT] = {",
    ]
    lines += [f"        {sp(p)}{',' if i < len(blau) - 1 else ''}" for i, p in enumerate(blau)]
    lines += [
        "    };",
        f"    constexpr Spot SAFE_SPOT = {sp(st['safe'])};",
        "}",
        "#endif",
        "",
    ]
    return "\n".join(lines)


def report_fails(fails):
    for c, h, d in fails:
        print(f"FAIL {c} {h} {d:.2f}")
    sys.exit(1)


def cmd_pick(log_path):
    results = parse(log_path)
    cx, cy = LEASH_CENTER[0], LEASH_CENTER[1]
    parsed = {(round((r["to"][0] - cx) / GRID_STEP), round((r["to"][1] - cy) / GRID_STEP)) for r in results}
    expected = set(grid_cells())
    if not expected <= parsed:
        print(f"FAIL probe-log-incomplete all {len(parsed & expected)}/{len(expected)}")
        sys.exit(1)
    floor = Floor(results)
    if not floor.points:
        sys.exit("pick: no REACHABLE floor in the probe log — is the character parked in the Horsemen's room?")
    st, fails = search(floor)
    if st is None:
        report_fails(fails)

    header = render_header(st)
    with open(JSON_OUT, "w") as f:
        json.dump(st, f, indent=2)
    with open(HEADER_OUT, "w") as f:
        f.write(header)
    with open(VERIFY_CMDS, "w") as f:
        f.write("\n".join(probe_line(p) for _, p in spot_list(st)) + "\n")

    tanks = {h: st["horses"][h]["tank"] for h in HORSES}
    for h in HORSES:
        hs = st["horses"][h]
        print(f"{DISPLAY[h]:9s} tank {hs['tank']}  ready {hs['ready']} ({math.dist(hs['ready'][:2], hs['tank'][:2]):.1f} y)"
              f"  ranged {hs['ranged']}  healer {hs['healer']}")
    print(f"safe {st['safe']}  min dist to a tank spot "
          f"{min(math.dist(st['safe'][:2], t[:2]) for t in tanks.values()):.1f} y")
    print(f"wrote {JSON_OUT}, {HEADER_OUT} and {VERIFY_CMDS} ({len(spot_list(st))} probes)\n")
    print(header)


def cmd_verify(log_path):
    if not os.path.exists(JSON_OUT):
        sys.exit(f"verify: {JSON_OUT} missing — run pick first")
    st = json.load(open(JSON_OUT))
    results = parse(log_path)
    spots = spot_list(st)
    if len(results) != len(spots):
        sys.exit(f"verify: {len(results)} probe results for {len(spots)} spots — re-run the verify commands")
    bad = 0
    for (label, p), r in zip(spots, results):
        if math.dist(r["to"][:2], p[:2]) > 0.1:
            print(f"FAIL probe-mismatch {label} {math.dist(r['to'][:2], p[:2]):.2f}  "
                  f"(probed ({r['to'][0]:.2f}, {r['to'][1]:.2f}), spot ({p[0]:.2f}, {p[1]:.2f}) — stale verify log?)")
            bad += 1
            continue
        ok = (r["verdict"] == "REACHABLE" and math.dist(r["end"][:2], p[:2]) < 1.5
              and abs(r["end"][2] - FLOOR_Z) < SAME_FLOOR_DZ)
        print(f"spawn -> {label} ({p[0]:.2f}, {p[1]:.2f}): {r['verdict']} points={r['points']}"
              f"{'' if ok else '  <-- NOT REACHABLE'}")
        bad += not ok
    fails = check_constraints(st)
    for c, h, d in fails:
        print(f"FAIL {c} {h} {d:.2f}")
    if not fails:
        print("constraints: all hold")
    sys.exit(1 if bad or fails else 0)


if __name__ == "__main__":
    if len(sys.argv) >= 2 and sys.argv[1] == "gen":
        cmd_gen()
    elif len(sys.argv) == 3 and sys.argv[1] == "pick":
        cmd_pick(sys.argv[2])
    elif len(sys.argv) == 3 and sys.argv[1] == "verify":
        cmd_verify(sys.argv[2])
    else:
        sys.exit(__doc__)
