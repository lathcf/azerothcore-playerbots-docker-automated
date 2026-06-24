import math
from geo import distance2d, nearest, direction


def test_distance2d():
    assert distance2d(0, 0, 3, 4) == 5.0


def test_nearest_picks_closest_and_returns_distance():
    rows = [
        {"name": "Far", "x": 100.0, "y": 0.0},
        {"name": "Near", "x": 10.0, "y": 0.0},
    ]
    row, dist = nearest(rows, 0.0, 0.0)
    assert row["name"] == "Near"
    assert round(dist) == 10


def test_nearest_empty():
    assert nearest([], 0.0, 0.0) == (None, None)


def test_direction_cardinals():
    # +X north, +Y west
    assert direction(0, 0, 100, 0) == "north"
    assert direction(0, 0, -100, 0) == "south"
    assert direction(0, 0, 0, 100) == "west"
    assert direction(0, 0, 0, -100) == "east"
    assert direction(0, 0, 100, 100) == "northwest"


def test_direction_same_point():
    assert direction(5, 5, 5, 5) == "right here"
