"""Spatial helpers. AzerothCore world coords: +X north, +Y west."""
from __future__ import annotations

import math
from typing import Optional

# Several WotLK map ids are geographically DISCONTIGUOUS — one map holds regions separated
# by an empty coordinate gulf. The worst offender is map 530 ("Outland"), which also carries
# the Blood Elf zones (Eversong/Ghostlands/Silvermoon) and the Draenei isles (Azuremyst/
# Bloodmyst/Exodar) at far-flung coordinates. Raw 2D "nearest" within a map can therefore
# return an NPC ~17000y away in another region (observed: a paladin trainer in Eversong while
# the bot stood in Hellfire). A real same-region service NPC is essentially never this far, so
# we treat anything past this cap as cross-region and refuse to give bogus distance/direction.
# Comfortably above any useful intra-region distance, well below the observed ~17000y gap.
SAME_REGION_MAX_YARDS = 8000.0


def distance2d(x1: float, y1: float, x2: float, y2: float) -> float:
    return math.hypot(x2 - x1, y2 - y1)


def nearest(rows: list[dict], x: float, y: float):
    """Return (row, distance) for the closest row by (x, y), or (None, None)."""
    best = None
    best_d = None
    for r in rows:
        d = distance2d(x, y, float(r["x"]), float(r["y"]))
        if best_d is None or d < best_d:
            best, best_d = r, d
    return best, best_d


def direction(bx: float, by: float, tx: float, ty: float) -> str:
    """Rough cardinal/intercardinal direction from bot to target."""
    north = tx - bx
    west = ty - by
    if abs(north) < 5 and abs(west) < 5:
        return "right here"
    parts = []
    if abs(north) >= abs(west) * 0.4:
        parts.append("north" if north >= 0 else "south")
    if abs(west) >= abs(north) * 0.4:
        parts.append("west" if west >= 0 else "east")
    return "".join(parts) or ("north" if north >= 0 else "south")
