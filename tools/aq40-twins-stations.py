#!/usr/bin/env python3
"""aq40-twins-stations.py — author the Twin Emperors' two fixed stations A and B and every per-station spot
(waiting warlock, waiting warrior, warlock casting spot, up to 8 healer spots, up to 12 ranged-arc spots) plus the
form-up spots S_A / S_B, and verify them on the floor (AQ40 Twin Emperors v2 strategy, Task 2; spec 2026-10-22
§5.1 / §5.3, re-picked for Revision 2, §11, in Task 10a).

Needs a character (GM is fine) parked on map 531 in the Twins room (between Vek'lor's spawn
(-8868.3, 1206.0, -104.2) and Vek'nilash's (-9023.7, 1176.2, -104.2)), so the probe has a path owner and the
grids are loaded. The twins can stay alive. Run from the overlay repo root:

  python3 tools/aq40-twins-stations.py gen > /tmp/aq40twins-probe.cmds
  bash -c 'mapfile -t C < /tmp/aq40twins-probe.cmds; WGCONSOLE_PUMP=0.4 python3 tools/wgconsole.py /tmp/aq40twins-probe.log "${C[@]}"'
  python3 tools/aq40-twins-stations.py pick /tmp/aq40twins-probe.log     # writes JSON + verify cmds, prints C++
  bash -c 'mapfile -t C < /tmp/aq40twins-verify.cmds; WGCONSOLE_PUMP=1 python3 tools/wgconsole.py /tmp/aq40twins-verify.log "${C[@]}"'
  python3 tools/aq40-twins-stations.py verify /tmp/aq40twins-verify.log   # OK/CLIPPED/DETOUR per point + counts

Run time: gen emits ~2,100 probes (a ~1,050-cell 5 y grid over both spawns padded by 40 y, probed once from each
spawn), so the probe run takes ~15-20 min at WGCONSOLE_PUMP=0.4. pick takes ~1.5 min (B's casting-spot choice runs
the healer search per candidate). The verify run is one probe per emitted spot (each station's spots from that
station, S_A from A, S_B from B) plus the crossing A -> B and B -> A — 49 for the current JSON — at
WGCONSOLE_PUMP=1, a few minutes.

Floor map (gen + pick): every grid cell is probed from Vek'lor's spawn and again from Vek'nilash's spawn. A cell
is floor when either probe is REACHABLE, ends within END_TOL of the cell, and ends inside the room's floor z band
[MID_HALL_Z - SAME_FLOOR_DZ, spawn z + SAME_FLOOR_DZ]. Both spawns stand on ledges at z -104.2 and the mid-hall
floor is about -112.3, so a test against the start z alone would reject the whole mid-hall (where B lies) and
the descent between them; the band keeps both levels and the slope while still rejecting a probe that ends on
some other level of the temple.

Pick. "Inner" = a bearing from a station within HALF_PLANE_DEG (90 deg) of the bearing to the other station;
"outer" = everything else; the "lane" = +-LANE_DEG (40 deg) around the bearing to the other station. A spot is
floor only when all four enclosing grid cells are floor (a 5 y grid's nearest cell reaches 2.5 y past a ledge
edge), and every spot also needs a line of floor to its station.
  - A = Vek'lor's spawn snapped to the nearest floor cell, so A matches where he stands at the pull.
  - B (§11.1) = the floor cell FARTHEST from A within STATION_SEP_MIN..STATION_SEP_MAX (105-120 y), within
    +-B_BEARING_TOL (30 deg) of the A -> Vek'nilash-spawn bearing, with a line of floor to A (the crossing); ties
    break on clearance (distance to the nearest non-floor cell), then (x, y). A candidate whose stations cannot seat
    every required spot below is skipped for the next farthest.
  - Per station, in this order:
      1. the warlock waiting spot at WARLOCK_WAIT_DIST (10 y) and 2. the warrior waiting spot at WARRIOR_WAIT_DIST
         (14 y): OUTER half-plane (§11.3), first bearing of a 5 deg sweep from the outer axis alternating outward,
         more than MIN_SPOT_GAP (8 y) from the spots already placed;
      3. the warlock casting spot. A: NOT swept -- exactly CAST_DIST_LANE (26 y) toward B on the lane (ruling R16,
         kept by §11.3: outside the twins' ~23 y aggro, and also warlock A's staging/pull spot, emitted again as
         JSON "stage_a_warlock"; the C++ reads TWINS_STATIONS[A].warlockCast). B: OUTER half-plane at CAST_RADII
         (20-26 y); every outer candidate in sweep order (nearest radius first) is scored by the healer search below
         and the first seating the most healers wins (the 36 y reach to an outer casting spot is what limits
         healers); any non-lane bearing only if no outer spot exists (printed);
      4. healer spots: INNER half-plane outside the lane, at HEALER_RADII (20-26 y), within HEAL_REACH (36 y) of the
         station, the casting spot and both waiting spots, more than MIN_SPOT_GAP from those three spots and more
         than HEALER_GAP (7 y) from each other. Candidates run from the side bearings (90 deg off the lane) in
         toward the lane edge, both sides alternating; a depth-first search takes the lexicographically first
         HEALER_COUNT (8), else the most it can (fewer than HEALER_COUNT_MIN = 5 is BLOCKED). Every placed healer
         is re-checked against the four points;
      5. the ranged arc (§11.4): RANGED_COUNT (12) spots at RANGED_RADIUS (24 y; RANGED_RADII 22-26 y allowed)
         within +-RANGED_ARC_DEG (60 deg) of the lane, at least RANGED_GAP (4 y) from each other, the three tank
         spots and the healer spots. Rank 0 is on the lane axis and the rest alternate outward (+d, -d, +2d, ...),
         d the bearing step that spaces neighbours RANGED_GAP + 0.1 y at 24 y; each rank takes the candidate nearest
         its target bearing (within +-d/2), then nearest 24 y. Fewer than RANGED_COUNT_MIN (8) is BLOCKED.
  - S_A (ruling R11): STAGE_A_DIST (28 y) from A along A -> B, as GetTwinsStagingSpot computes it at runtime;
    emitted to the JSON for the verify run only.
  - S_B (warrior B's form-up staging spot, ruling R16): the point STAGE_B_DIST (30 y) from Vek'nilash's spawn
    along the line to station B, which lies on his path when he runs at warlock A. It must be floor (all four
    cells) with a line of floor to B; if not, the nearest floor cell within STAGE_B_SNAP (3 y) of it with a line
    of floor to B. Emitted as JSON "stage_b" and C++ TWINS_STAGE_B.
  Any failure is printed and pick exits 1 without the C++ block (the JSON and verify cmds are still written when
  both stations exist). That is the plan's BLOCKED path: stop and report with the JSON.

Verify: a point is OK only if REACHABLE, its end is within END_TOL (xy) and SAME_FLOOR_DZ (z) of the target, AND
its path is near-straight. `.bgbots probe` prints the path point count but not a path length, and its
PathGenerator runs FindSmoothPath (4 y steps), so a straight path has ~dist/4 + 1 points; the test is
points <= ceil(dist_xy / 4) + 2, and a point over that is DETOUR (not ok). The probe list is rebuilt from the JSON
(verify_plan) and each logged probe must match it. verify prints spots_ok=N/M crossing_ok=K/2, writes both counts
into the JSON, and exits 1 on any point that is not OK.
"""
import json
import math
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
JSON_OUT = os.path.join(ROOT, "tools", "aq40-twins-stations.json")
VERIFY_CMDS = "/tmp/aq40twins-verify.cmds"

MAP_ID = 531
VEKLOR = (-8868.31, 1205.97, -104.231)       # Vek'lor's spawn: A snaps here; floor-map probe start 1
VEKNILASH = (-9023.67, 1176.24, -104.226)    # Vek'nilash's spawn: B's bearing; floor-map probe start 2
STARTS = (VEKLOR, VEKNILASH)
MID_HALL_Z = -112.3                          # mid-hall floor between the ledges (old spec)
BOX_X = (min(VEKLOR[0], VEKNILASH[0]), max(VEKLOR[0], VEKNILASH[0]))
BOX_Y = (min(VEKLOR[1], VEKNILASH[1]), max(VEKLOR[1], VEKNILASH[1]))
BOX_PAD = 40.0
TARGET_Z = -104.0                            # z handed to a grid probe; the core snaps from z+5 down 50 y
GRID_STEP = 5.0
SAME_FLOOR_DZ = 3.0
FLOOR_Z_BAND = (MID_HALL_Z - SAME_FLOOR_DZ, max(VEKLOR[2], VEKNILASH[2]) + SAME_FLOOR_DZ)
OPEN_RADIUS = 30.0                           # B maximises floor cells within this
LOS_STEP = 2.0
# verify: FindSmoothPath steps SMOOTH_PATH_STEP_SIZE = 4 y and the probe's PathGenerator is not _useStraightPath,
# so a straight path has about dist/4 + 1 points; more than ceil(dist/SMOOTH_STEP) + DETOUR_SLACK_POINTS = a bend.
SMOOTH_STEP = 4.0
DETOUR_SLACK_POINTS = 2
END_TOL = 1.5                                # a probe "arrives" when its end is this close to the target (xy)

# Aq40TwinsRules.h (fork, Task 1) — keep in step.
STATION_SEP_MIN = 105.0                      # spec §11.1 (Revision 2): as far as floor allows, minimum 105 y
STATION_SEP_MAX = 120.0
WARLOCK_WAIT_DIST = 10.0
WARRIOR_WAIT_DIST = 14.0
CAST_SPOT_MIN = 20.0
CAST_SPOT_MAX = 26.0
HEALER_SPOT_MIN = 20.0
HEALER_SPOT_MAX = 26.0                       # Revision 2 brief: 20-26 y (Aq40TwinsRules.h still says 24; static only)
HEAL_REACH = 36.0

B_BEARING_TOL = 30.0                         # B within this of the A -> Vek'nilash-spawn bearing (deg)
SWEEP_STEP_DEG = 5
LANE_DEG = 40.0                              # skip this either side of the bearing to the other station
HALF_PLANE_DEG = 90.0                        # inner = within this of the bearing to the other station; outer = beyond
MIN_SPOT_GAP = 8.0                           # every spot more than this from every other spot at its station
# ...except healer-to-healer (Task 9): the 36 y reach confines healers to a narrow arc each side, and an 8 y gap
# does not fit 8 there (searched offline); 7 y does on A.
HEALER_GAP = 7.0
HEALER_SWEEP_STEP_DEG = 2.5                  # healer candidate bearings (finer than the 5 deg spot sweep)
HEALER_SEARCH_NODES = 2_000_000              # healer subset search budget per target count
CAST_DIST_LANE = 26.0                        # A's casting spot: this far from A toward B (R16; Revision 2 keeps it)
CAST_RADII = (20.0, 21.0, 22.0, 23.0, 24.0, 25.0, 26.0)   # B's outer casting spot radii, nearest first (heal reach)
STAGE_A_DIST = 28.0                          # S_A: this far from A along A -> B (Aq40TwinsRules.h STAGE_A_DIST)
STAGE_B_DIST = 30.0                          # S_B: this far from Vek'nilash's spawn toward station B (R16)
STAGE_B_SNAP = 3.0                           # S_B falls back to the nearest floor cell within this
HEALER_RADII = (22.0, 21.0, 23.0, 20.0, 24.0, 25.0, 26.0)
HEALER_COUNT = 8
HEALER_COUNT_MIN = 5                         # fewer is BLOCKED (Revision 2 brief)
# Ranged arc (spec §11.4): RANGED_COUNT spots at RANGED_RADIUS (RANGED_RADII allowed per spot) within
# +-RANGED_ARC_DEG of the bearing to the other station, at least RANGED_GAP apart (and from the tank + healer spots),
# rank 0 on the lane axis, the rest alternating outward (+, -, +, ...).
RANGED_COUNT = 12
RANGED_COUNT_MIN = 8
RANGED_RADIUS = 24.0
RANGED_RADII = (24.0, 23.0, 25.0, 22.0, 26.0)
RANGED_ARC_DEG = 60.0
RANGED_GAP = 4.0
RANGED_GAP_EPS = 0.01                        # float slack on the >= RANGED_GAP test
RANGED_STEP_DEG = 1.0                        # candidate bearing resolution around each rank's target bearing
SPOT_KEYS = ("warlock_wait", "warrior_wait", "warlock_cast")
assert CAST_SPOT_MIN <= CAST_DIST_LANE <= CAST_SPOT_MAX
assert all(CAST_SPOT_MIN <= r <= CAST_SPOT_MAX for r in CAST_RADII)
assert all(HEALER_SPOT_MIN <= r <= HEALER_SPOT_MAX for r in HEALER_RADII)

PROBE_RE = re.compile(r"\.bgbots probe 531 (\S+) (\S+) (\S+) (\S+) (\S+) (\S+)")
VERDICT_RE = re.compile(
    r"probe: (?:(REACHABLE|BLOCKED) calc=\d type=0x[0-9a-f]+ points=(\d+) "
    r"end=\(([-\d.]+),([-\d.]+),([-\d.]+)\)|(NO GROUND|no loaded instance|no player))")


def parse(log_path):
    """Pair every sent probe with its verdict line, in order (same contract as aq40-huhuran-stations.py)."""
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


def grid_origin():
    return (BOX_X[0] - BOX_PAD, BOX_Y[0] - BOX_PAD)


def grid_dims():
    nx = int(math.floor((BOX_X[1] + BOX_PAD - (BOX_X[0] - BOX_PAD)) / GRID_STEP)) + 1
    ny = int(math.floor((BOX_Y[1] + BOX_PAD - (BOX_Y[0] - BOX_PAD)) / GRID_STEP)) + 1
    return nx, ny


def cell_xy(i, j):
    x0, y0 = grid_origin()
    return (x0 + i * GRID_STEP, y0 + j * GRID_STEP)


def cmd_gen():
    nx, ny = grid_dims()
    for start in STARTS:
        for i in range(nx):
            for j in range(ny):
                x, y = cell_xy(i, j)
                print(f".bgbots probe {MAP_ID} {x:.2f} {y:.2f} {TARGET_Z:.2f} "
                      f"{start[0]:.2f} {start[1]:.2f} {start[2]:.2f}")


def on_floor_band(r):
    """REACHABLE, ended on the target cell, and inside the room's floor z band (gen/pick floor map)."""
    return (r["verdict"] == "REACHABLE" and math.dist(r["end"][:2], r["to"][:2]) < END_TOL
            and FLOOR_Z_BAND[0] <= r["end"][2] <= FLOOR_Z_BAND[1])


def arrived(r):
    """REACHABLE, ended on the target in xy, and on the target's own floor in z (verify)."""
    return (r["verdict"] == "REACHABLE" and math.dist(r["end"][:2], r["to"][:2]) < END_TOL
            and abs(r["end"][2] - r["to"][2]) < SAME_FLOOR_DZ)


def same_pt(a, b):
    return abs(a[0] - b[0]) < 0.02 and abs(a[1] - b[1]) < 0.02


def ang_diff(a, b):
    """Absolute difference of two bearings in degrees, in [0, 180]."""
    return abs((a - b + 180.0) % 360.0 - 180.0)


def bearing_deg(a, b):
    return math.degrees(math.atan2(b[1] - a[1], b[0] - a[0])) % 360.0


def sweep_bearings(away_deg, lane_deg, step=SWEEP_STEP_DEG):
    """`step` deg steps starting at the away bearing and alternating outward, minus the crossing lane."""
    out = [away_deg % 360.0]
    for k in range(1, int(round(180 / step)) + 1):
        for sgn in (1, -1):
            b = (away_deg + sgn * k * step) % 360.0
            if all(ang_diff(b, o) > 1e-6 for o in out):
                out.append(b)
    return [b for b in out if ang_diff(b, lane_deg) > LANE_DEG]


def cmd_pick(log_path):
    results = parse(log_path)
    x0, y0 = grid_origin()

    def cell(p):
        return (round((p[0] - x0) / GRID_STEP), round((p[1] - y0) / GRID_STEP))

    floor = {}          # cell -> end point (floor), or None (probed, not floor from any start)
    for r in results:
        if not any(same_pt(r["from"], s) for s in STARTS):
            continue
        c = cell(r["to"])
        if on_floor_band(r):
            # Prefer Vek'lor's end point when both starts arrive; either is the same ground.
            if floor.get(c) is None or same_pt(r["from"], VEKLOR):
                floor[c] = r["end"]
        else:
            floor.setdefault(c, None)
    floor_cells = [c for c, v in floor.items() if v is not None]
    if not floor_cells:
        sys.exit("pick: no floor cells in the log — is it a gen run from the Twins room on map 531?")

    def xy(c):
        return cell_xy(*c)

    def on_floor(q):
        return floor.get(cell(q)) is not None

    def spot_on_floor(q):
        """A spot is floor only when all four grid cells enclosing it are floor. The nearest-cell test alone
        accepted a healer spot 1.7 y past the last floor cell on A's ledge edge (verify: NO GROUND)."""
        fi, fj = (q[0] - x0) / GRID_STEP, (q[1] - y0) / GRID_STEP
        i0, j0 = math.floor(fi), math.floor(fj)
        return all(floor.get((i0 + di, j0 + dj)) is not None for di in (0, 1) for dj in (0, 1))

    def floor_z(q):
        return floor[cell(q)][2]

    def line_of_floor(a, b):
        steps = max(1, int(math.dist(a[:2], b[:2]) / LOS_STEP))
        return all(on_floor((a[0] + (b[0] - a[0]) * t / steps, a[1] + (b[1] - a[1]) * t / steps))
                   for t in range(steps + 1))

    nx, ny = grid_dims()
    nonfloor = [xy((i, j)) for i in range(nx) for j in range(ny) if floor.get((i, j)) is None]

    def clearance(c):
        return min((math.dist(xy(c), b) for b in nonfloor), default=math.inf)

    def open_count(c):
        return sum(1 for d in floor_cells if math.dist(xy(d), xy(c)) <= OPEN_RADIUS)

    # A: Vek'lor's spawn snapped to the nearest floor cell.
    ac = min(floor_cells, key=lambda c: (math.dist(xy(c), VEKLOR[:2]), xy(c)))
    a_pt = (xy(ac)[0], xy(ac)[1], floor[ac][2])
    print(f"A = ({a_pt[0]:.2f}, {a_pt[1]:.2f}, {a_pt[2]:.2f})  ({math.dist(a_pt[:2], VEKLOR[:2]):.2f} y from "
          f"Vek'lor's spawn); floor cells {len(floor_cells)}/{len(floor)}")

    def outer(b, lane):
        return ang_diff(b, lane) > HALF_PLANE_DEG

    def inner(b, lane):
        return ang_diff(b, lane) <= HALF_PLANE_DEG

    def at(station, deg, rad):
        a = math.radians(deg)
        return (station[0] + rad * math.cos(a), station[1] + rad * math.sin(a))

    def spot_ok(station, q, placed, gap=MIN_SPOT_GAP):
        """All four enclosing cells floor, a line of floor to the station, and more than `gap` from `placed`."""
        return (spot_on_floor(q) and line_of_floor(station, q)
                and all(math.dist(q, p[:2]) > gap for p in placed))

    def place_swept(station, lane, radii, placed, half):
        """First bearing (5 deg sweep from the outer axis, alternating outward, outside the lane) in `half` with a
        spot at one of `radii` (in order) that passes spot_ok. Returns (point, deg, rad) or None."""
        for deg in sweep_bearings((lane + 180.0) % 360.0, lane):
            if not half(deg, lane):
                continue
            for rad in radii:
                q = at(station, deg, rad)
                if spot_ok(station, q, placed):
                    return (q[0], q[1], floor_z(q)), deg, rad
        return None

    def healer_search(station, lane, reach_pts, placed):
        """Healer spots (Task 9 search, Revision 2 half-plane): every candidate in the INNER half-plane outside the
        lane, at HEALER_RADII, that passes floor + line of floor + HEAL_REACH to every point in `reach_pts` + more
        than MIN_SPOT_GAP from the tank spots; then the lexicographically first k of them pairwise more than
        HEALER_GAP apart, for the largest k <= HEALER_COUNT the depth-first search finds (deterministic)."""
        cands = []
        # Bearings from the side (HALF_PLANE_DEG off the lane) in toward the lane edge, both sides alternating --
        # the old away-side-first order restricted to the inner half-plane.
        order = []
        off = HALF_PLANE_DEG
        while off > LANE_DEG:
            for sgn in (1, -1):
                b = (lane + sgn * off) % 360.0
                if all(ang_diff(b, o) > 1e-6 for o in order):
                    order.append(b)
            off -= HEALER_SWEEP_STEP_DEG
        for deg in order:
            for rad in HEALER_RADII:
                q = at(station, deg, rad)
                if not spot_ok(station, q, placed):
                    continue
                if not all(math.dist(q, p[:2]) <= HEAL_REACH for p in reach_pts):
                    continue
                cands.append(((q[0], q[1], floor_z(q)), deg, rad))
        nodes = [0]
        for want in range(min(HEALER_COUNT, len(cands)), 0, -1):
            chosen = []

            def dfs(start):
                if len(chosen) == want:
                    return True
                for i in range(start, len(cands)):
                    if len(chosen) + (len(cands) - i) < want or nodes[0] >= HEALER_SEARCH_NODES * HEALER_COUNT:
                        return False
                    nodes[0] += 1
                    q = cands[i][0]
                    if all(math.dist(q[:2], cands[j][0][:2]) > HEALER_GAP for j in chosen):
                        chosen.append(i)
                        if dfs(i + 1):
                            return True
                        chosen.pop()
                return False

            if dfs(0):
                return [cands[i] for i in chosen], len(cands), nodes[0]
        return [], len(cands), nodes[0]

    def ranged_arc(station, lane, avoid):
        """Spec §11.4 ranged arc: rank 0 on the lane axis, then alternating outward (+d, -d, +2d, ...), d the bearing
        step that puts RANGED_GAP between neighbours at RANGED_RADIUS. Each rank takes the candidate nearest its
        target bearing (within +-d/2, RANGED_STEP_DEG resolution), then nearest RANGED_RADIUS (RANGED_RADII order),
        that is floor + line of floor + inside +-RANGED_ARC_DEG + more than RANGED_GAP from every ranged spot already
        placed and from `avoid` (the three tank spots and the healer spots). A rank with no candidate is skipped
        (fewer spots)."""
        d = math.degrees(2.0 * math.asin((RANGED_GAP + 0.1) / (2.0 * RANGED_RADIUS)))
        targets = [0.0]
        k = 1
        while len(targets) < RANGED_COUNT:
            for sgn in (1, -1):
                if len(targets) < RANGED_COUNT:
                    targets.append(sgn * k * d)
            k += 1
        out, skipped = [], []
        half = d / 2.0
        for rank, off in enumerate(targets):
            got = None
            steps = int(half / RANGED_STEP_DEG)
            offs = [0.0] + [s * j * RANGED_STEP_DEG for j in range(1, steps + 1) for s in (1, -1)]
            for doff in offs:
                rel = off + doff
                if abs(rel) > RANGED_ARC_DEG:
                    continue
                deg = (lane + rel) % 360.0
                for rad in RANGED_RADII:
                    q = at(station, deg, rad)
                    # ">= RANGED_GAP" (the brief's wording), so rank 0 can sit on the axis exactly RANGED_GAP in
                    # front of A's lane casting spot (26 y) at 22 y.
                    if not spot_ok(station, q, [p[0] for p in out], RANGED_GAP - RANGED_GAP_EPS):
                        continue
                    if any(math.dist(q, p[:2]) < RANGED_GAP - RANGED_GAP_EPS for p in avoid):
                        continue
                    got = ((q[0], q[1], floor_z(q)), rel, rad)
                    break
                if got:
                    break
            if got:
                out.append(got)
            else:
                skipped.append(rank)
        return out, skipped, d

    def build(name, station, other, lane_cast, quiet=False):
        """All spots for one station. Returns (spots dict, failures list, notes list)."""
        fails, notes = [], []
        lane = bearing_deg(station, other)
        out = {"station": station}
        placed = []
        for key, dist in (("warlock_wait", WARLOCK_WAIT_DIST), ("warrior_wait", WARRIOR_WAIT_DIST)):
            got = place_swept(station, lane, (dist,), placed, outer)
            if got is None:
                fails.append(f"{name}: no OUTER bearing for {key} at {dist:.0f} y (floor + line of floor + "
                             f">{MIN_SPOT_GAP:.0f} y gap)")
                return out, fails, notes
            out[key] = got[0]
            out["_" + key] = got[1:]
            placed.append(got[0])

        def healers_for(cast):
            reach_pts = [station, out["warlock_wait"], out["warrior_wait"], cast]
            return healer_search(station, lane, reach_pts, placed + [cast])

        if lane_cast:
            # A (R16, Revision 2 keeps it): on the lane at CAST_DIST_LANE, outside the ~23 y aggro for the pull.
            q = at(station, lane, CAST_DIST_LANE)
            if not spot_ok(station, q, placed):
                fails.append(f"{name}: the casting spot {CAST_DIST_LANE:.0f} y toward the other station fails floor + "
                             f"line of floor + >{MIN_SPOT_GAP:.0f} y gap")
                return out, fails, notes
            cast = ((q[0], q[1], floor_z(q)), lane, CAST_DIST_LANE)
            hs = healers_for(cast[0])
        else:
            # B (Revision 2): OUTER half-plane at CAST_RADII; among the outer candidates (sweep order from the outer
            # axis, nearest radius first) take the first that seats the most healers (the 36 y reach to the casting
            # spot is what limits them). Any non-lane bearing only if no outer spot exists.
            cands = []
            for deg in sweep_bearings((lane + 180.0) % 360.0, lane):
                if not outer(deg, lane):
                    continue
                for rad in CAST_RADII:
                    q = at(station, deg, rad)
                    if spot_ok(station, q, placed):
                        cands.append(((q[0], q[1], floor_z(q)), deg, rad))
            if not cands:
                notes.append(f"{name}: NO outer casting spot passes floor + line of floor + gap; using any non-lane "
                             f"bearing")
                for deg in sweep_bearings((lane + 180.0) % 360.0, lane):
                    for rad in CAST_RADII:
                        q = at(station, deg, rad)
                        if spot_ok(station, q, placed):
                            cands.append(((q[0], q[1], floor_z(q)), deg, rad))
            if not cands:
                fails.append(f"{name}: no casting spot at {CAST_SPOT_MIN:.0f}-{CAST_SPOT_MAX:.0f} y on any non-lane "
                             f"bearing")
                return out, fails, notes
            best = None
            for c in cands:
                h = healers_for(c[0])
                if best is None or len(h[0]) > len(best[1][0]):
                    best = (c, h)
                if len(h[0]) == HEALER_COUNT:
                    break
            cast, hs = best
            notes.append(f"{name}: casting spot chosen from {len(cands)} outer candidate(s) for "
                         f"{len(hs[0])} healer(s)")
        out["warlock_cast"] = cast[0]
        out["_warlock_cast"] = cast[1:]
        placed.append(cast[0])
        out["healers"] = [h[0] for h in hs[0]]
        out["_healers"] = [h[1:] for h in hs[0]]
        notes.append(f"{name}: healer search {hs[1]} candidate(s), {hs[2]} node(s)")
        if len(out["healers"]) < HEALER_COUNT_MIN:
            fails.append(f"{name}: only {len(out['healers'])}/{HEALER_COUNT} healer spots (minimum "
                         f"{HEALER_COUNT_MIN}) pass inner half-plane + floor + line of floor + "
                         f">{MIN_SPOT_GAP:.0f} y from tanks + >{HEALER_GAP:.0f} y apart + {HEAL_REACH:.0f} y reach")
        reach_pts = [station] + [out[k] for k in SPOT_KEYS]
        for k, h in enumerate(out["healers"]):
            for lbl, p in zip(("station",) + SPOT_KEYS, reach_pts):
                dd = math.dist(h[:2], p[:2])
                if dd > HEAL_REACH:
                    fails.append(f"{name}: healer {k} is {dd:.1f} y from {lbl} (> {HEAL_REACH:.0f})")
        rg, skipped, dstep = ranged_arc(station, lane, [out[k] for k in SPOT_KEYS] + out["healers"])
        out["ranged"] = [r[0] for r in rg]
        out["_ranged"] = [r[1:] for r in rg]
        notes.append(f"{name}: ranged arc step {dstep:.2f} deg, {len(rg)}/{RANGED_COUNT} placed"
                     + (f", skipped rank(s) {skipped}" if skipped else ""))
        if len(rg) < RANGED_COUNT_MIN:
            fails.append(f"{name}: only {len(rg)}/{RANGED_COUNT} ranged spots (minimum {RANGED_COUNT_MIN})")
        return out, fails, notes

    # B (spec §11.1): the floor cell FARTHEST from A within STATION_SEP_MIN..MAX, within +-B_BEARING_TOL of the
    # A -> Vek'nilash-spawn bearing, with a line of floor to A (the crossing). Ties break on clearance, then (x, y).
    # A candidate whose stations cannot seat every required spot (waiting spots, casting spot, >= HEALER_COUNT_MIN
    # healers, >= RANGED_COUNT_MIN ranged) is skipped in favour of the next farthest.
    nil_bear = bearing_deg(a_pt, VEKNILASH)
    cand = [c for c in floor_cells
            if STATION_SEP_MIN <= math.dist(xy(c), a_pt[:2]) <= STATION_SEP_MAX
            and ang_diff(bearing_deg(a_pt, xy(c)), nil_bear) <= B_BEARING_TOL]
    in_band = len(cand)
    cand = [c for c in cand if line_of_floor(a_pt, xy(c))]
    if not cand:
        sys.exit(f"pick: BLOCKED — no B: {in_band} floor cell(s) {STATION_SEP_MIN:.0f}-{STATION_SEP_MAX:.0f} y from "
                 f"A within +-{B_BEARING_TOL:.0f} deg of the A -> Vek'nilash bearing ({nil_bear:.1f} deg), none with "
                 f"a line of floor to A; no C++ block")
    cand.sort(key=lambda c: (-round(math.dist(xy(c), a_pt[:2]), 3), -round(clearance(c), 3), xy(c)))
    chosen = None
    for c in cand:
        b_try = (xy(c)[0], xy(c)[1], floor[c][2])
        sa_t, fa, na = build("A", a_pt, b_try, True)
        sb_t, fb, nb = build("B", b_try, a_pt, False)
        if not fa and not fb:
            chosen = (c, b_try, sa_t, sb_t, na + nb)
            break
        print(f"  skip B candidate ({b_try[0]:.2f}, {b_try[1]:.2f}) at {math.dist(b_try[:2], a_pt[:2]):.2f} y: "
              + "; ".join(fa + fb))
    if chosen is None:
        sys.exit(f"pick: BLOCKED — none of {len(cand)} B candidate(s) with a crossing seats every required spot; "
                 f"no C++ block")
    bc, b_pt, sa, sb, notes = chosen
    sep = math.dist(a_pt[:2], b_pt[:2])
    print(f"B = ({b_pt[0]:.2f}, {b_pt[1]:.2f}, {b_pt[2]:.2f})  sep {sep:.2f} y, bearing from A "
          f"{bearing_deg(a_pt, b_pt):.1f} deg (Vek'nilash {nil_bear:.1f}), clearance {clearance(bc):.1f} y, "
          f"{len(cand)} candidate(s) with a crossing")
    for line in notes:
        print("  " + line)
    for name, s, other in (("A", sa, b_pt), ("B", sb, a_pt)):
        lane = bearing_deg(s["station"], other)
        print(f"station {name} spots (lane {lane:.1f} deg):")
        for key in SPOT_KEYS:
            deg, rad = s["_" + key]
            q = s[key]
            print(f"  {name} {key:12s} {deg:5.1f} deg ({ang_diff(deg, lane):5.1f} off lane) {rad:4.1f} y  "
                  f"({q[0]:.2f}, {q[1]:.2f}, {q[2]:.2f})")
        for k, (q, (deg, rad)) in enumerate(zip(s["healers"], s["_healers"])):
            print(f"  {name} healer {k:<5d} {deg:5.1f} deg ({ang_diff(deg, lane):5.1f} off lane) {rad:4.1f} y  "
                  f"({q[0]:.2f}, {q[1]:.2f}, {q[2]:.2f})")
        for k, (q, (rel, rad)) in enumerate(zip(s["ranged"], s["_ranged"])):
            print(f"  {name} ranged {k:<5d} {rel:+6.1f} deg off lane {rad:4.1f} y  "
                  f"({q[0]:.2f}, {q[1]:.2f}, {q[2]:.2f})")

    failed = []

    # S_A (R11): STAGE_A_DIST from A along A -> B, as GetTwinsStagingSpot computes it; z is the floor map's.
    t_a = STAGE_A_DIST / sep
    sa_xy = (a_pt[0] + (b_pt[0] - a_pt[0]) * t_a, a_pt[1] + (b_pt[1] - a_pt[1]) * t_a)
    stage_a = (sa_xy[0], sa_xy[1], floor_z(sa_xy)) if on_floor(sa_xy) else None
    if stage_a is None:
        failed.append(f"S_A: ({sa_xy[0]:.2f}, {sa_xy[1]:.2f}) is not floor")
    else:
        print(f"S_A = ({stage_a[0]:.2f}, {stage_a[1]:.2f}, {stage_a[2]:.2f})  {STAGE_A_DIST:.0f} y along A -> B, "
              f"{math.dist(stage_a[:2], VEKLOR[:2]):.2f} y from Vek'lor's spawn")

    # S_B (R16): STAGE_B_DIST from Vek'nilash's spawn along the line to B, floor with a line of floor to B; else
    # the nearest floor cell within STAGE_B_SNAP of that point that has one.
    d_nb = math.dist(VEKNILASH[:2], b_pt[:2])
    t = STAGE_B_DIST / d_nb
    sb_ideal = (VEKNILASH[0] + (b_pt[0] - VEKNILASH[0]) * t, VEKNILASH[1] + (b_pt[1] - VEKNILASH[1]) * t)
    stage_b = None
    if spot_on_floor(sb_ideal) and line_of_floor(b_pt, sb_ideal):
        stage_b = (sb_ideal[0], sb_ideal[1], floor_z(sb_ideal))
        how = "exact"
    else:
        snap = sorted((c for c in floor_cells if math.dist(xy(c), sb_ideal) <= STAGE_B_SNAP
                       and line_of_floor(b_pt, xy(c))), key=lambda c: (math.dist(xy(c), sb_ideal), xy(c)))
        if snap:
            stage_b = (xy(snap[0])[0], xy(snap[0])[1], floor[snap[0]][2])
            how = f"snapped {math.dist(stage_b[:2], sb_ideal):.2f} y"
    if stage_b is None:
        failed.append(f"S_B: ({sb_ideal[0]:.2f}, {sb_ideal[1]:.2f}) is not floor with a line of floor to B, and no "
                      f"floor cell within {STAGE_B_SNAP:.0f} y of it has one")
    else:
        print(f"S_B = ({stage_b[0]:.2f}, {stage_b[1]:.2f}, {stage_b[2]:.2f})  {how}; "
              f"{math.dist(stage_b[:2], VEKNILASH[:2]):.2f} y from Vek'nilash's spawn, "
              f"{math.dist(stage_b[:2], b_pt[:2]):.2f} y from B (B is {d_nb:.2f} y from his spawn)")

    def r3(p):
        return [round(p[0], 2), round(p[1], 2), round(p[2], 2)]

    def js(s):
        o = {"station": r3(s["station"])}
        for k in SPOT_KEYS:
            o[k] = r3(s[k])
        o["healers"] = [r3(h) for h in s["healers"]]
        o["ranged"] = [r3(h) for h in s["ranged"]]
        return o

    data = {"A": js(sa), "B": js(sb), "stage_a_warlock": r3(sa["warlock_cast"]),
            "stage_a": r3(stage_a) if stage_a else None, "stage_b": r3(stage_b) if stage_b else None,
            "separation": round(sep, 2), "spots_ok": None, "crossing_ok": None}
    with open(JSON_OUT, "w") as f:
        json.dump(data, f, indent=2)
        f.write("\n")

    with open(VERIFY_CMDS, "w") as f:
        f.write("\n".join(f".bgbots probe {MAP_ID} {to[0]:.2f} {to[1]:.2f} {to[2]:.2f} "
                          f"{fr[0]:.2f} {fr[1]:.2f} {fr[2]:.2f}" for _, _, to, fr in verify_plan(data)) + "\n")
    print(f"\nwrote {JSON_OUT} and {VERIFY_CMDS} ({len(verify_plan(data))} probes)\n")

    if failed:
        for line in failed:
            print(line)
        sys.exit(f"pick: BLOCKED — {len(failed)} failure(s); no C++ block printed")

    def pos(p):
        return f"Position({p[0]:.2f}f, {p[1]:.2f}f, {p[2]:.2f}f)"

    print("// ---- replace the TWINS_STATIONS block in Aq40Helpers.h (Revision 2) ----")
    print("// struct TwinsStationSpots {")
    print("//     Position station, warriorWait, warlockWait, warlockCast;")
    print(f"//     Position healers[{HEALER_COUNT}]; uint8 healerCount;")
    print(f"//     Position ranged[{RANGED_COUNT}]; uint8 rangedCount;")
    print("// };")
    print("constexpr bool TWINS_STATIONS_PROVISIONAL = false;")
    print("inline TwinsStationSpots const TWINS_STATIONS[2] = {")
    for name in ("A", "B"):
        s = data[name]
        print(f"    {{ // {name}")
        print(f"        {pos(s['station'])},")
        print(f"        {pos(s['warrior_wait'])},")
        print(f"        {pos(s['warlock_wait'])},")
        print(f"        {pos(s['warlock_cast'])},")
        print("        {")
        for k, h in enumerate(s["healers"]):
            print(f"            {pos(h)}{',' if k < len(s['healers']) - 1 else ''}")
        print("        },")
        print(f"        {len(s['healers'])},")
        print("        {")
        for k, h in enumerate(s["ranged"]):
            print(f"            {pos(h)}{',' if k < len(s['ranged']) - 1 else ''}")
        print("        },")
        print(f"        {len(s['ranged'])},")
        print(f"    }}{',' if name == 'A' else ''}")
    print("};")
    print(f"// warrior B's form-up staging spot: {STAGE_B_DIST:.0f} y from Vek'nilash's spawn toward B (R16)")
    print(f"inline Position const TWINS_STAGE_B = {pos(data['stage_b'])};")


def verify_plan(data):
    """(label, kind, to, from) for every probe verify runs, in order: each station's spots from that station
    (tank spots, healers, ranged), S_A from A, S_B from B, then the crossing A -> B and B -> A. A's casting spot is
    also stage_a_warlock, so it is probed once."""
    plan = []
    for name in ("A", "B"):
        s = data[name]
        st = s["station"]
        plan += [(f"{name} {k}", "spot", s[k], st) for k in SPOT_KEYS]
        plan += [(f"{name} healer {k}", "spot", h, st) for k, h in enumerate(s["healers"])]
        plan += [(f"{name} ranged {k}", "spot", h, st) for k, h in enumerate(s["ranged"])]
    pa, pb = data["A"]["station"], data["B"]["station"]
    if data.get("stage_a"):
        plan.append(("S_A (from A)", "spot", data["stage_a"], pa))
    if data.get("stage_b"):
        plan.append(("S_B (from B)", "spot", data["stage_b"], pb))
    plan.append(("crossing A->B", "crossing", pb, pa))
    plan.append(("crossing B->A", "crossing", pa, pb))
    return plan


def cmd_verify(log_path):
    results = parse(log_path)
    data = json.load(open(JSON_OUT))
    plan = verify_plan(data)
    n_spots = sum(1 for p in plan if p[1] == "spot")
    if len(results) != len(plan):
        sys.exit(f"verify: expected {len(plan)} probes ({n_spots} spots + 2 crossing, from {JSON_OUT}), "
                 f"log has {len(results)}")
    ok_count = {"spot": 0, "crossing": 0}
    for (label, kind, to, fr), r in zip(plan, results):
        if not (same_pt(r["to"], to) and same_pt(r["from"], fr)):
            sys.exit(f"verify: probe for {label} does not match the JSON (log to={r['to']} from={r['from']}) — "
                     f"re-run pick and verify together")
        ok = arrived(r)
        detour = ok and r["points"] > math.ceil(math.dist(r["from"][:2], r["to"][:2]) / SMOOTH_STEP) + DETOUR_SLACK_POINTS
        ok = ok and not detour
        tag = "OK" if ok else ("DETOUR" if detour else "CLIPPED")
        print(f"{label:16s} ({r['to'][0]:.2f}, {r['to'][1]:.2f}, {r['to'][2]:.2f}): {tag} {r['verdict']} "
              f"points={r['points']} end=({r['end'][0]:.1f}, {r['end'][1]:.1f}, {r['end'][2]:.1f})")
        ok_count[kind] += ok
    print(f"spots_ok={ok_count['spot']}/{n_spots} crossing_ok={ok_count['crossing']}/2")
    data["spots_ok"], data["crossing_ok"] = ok_count["spot"], ok_count["crossing"]
    with open(JSON_OUT, "w") as f:
        json.dump(data, f, indent=2)
        f.write("\n")
    sys.exit(0 if ok_count["spot"] == n_spots and ok_count["crossing"] == 2 else 1)


if __name__ == "__main__":
    if len(sys.argv) >= 2 and sys.argv[1] == "gen":
        cmd_gen()
    elif len(sys.argv) == 3 and sys.argv[1] == "pick":
        cmd_pick(sys.argv[2])
    elif len(sys.argv) == 3 and sys.argv[1] == "verify":
        cmd_verify(sys.argv[2])
    else:
        sys.exit(__doc__)
