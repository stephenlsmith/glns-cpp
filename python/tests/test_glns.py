# Copyright 2017 Stephen L. Smith and Frank Imeson
# Licensed under the Apache License, Version 2.0.  See LICENSE.
"""Python binding tests.  Runnable directly (python test_glns.py [examples_dir])
or via pytest."""

import os
import sys

import numpy as np

import glns

EXAMPLES = os.environ.get(
    "GLNS_EXAMPLES",
    os.path.join(os.path.dirname(__file__), "..", "..", "examples"),
)

# the tiny.gtsp instance, as in-memory data
TINY_DIST = np.array(
    [
        [9999, 3, 4, 7, 8, 2],
        [3, 9999, 6, 5, 3, 9],
        [4, 6, 9999, 4, 2, 6],
        [7, 5, 4, 9999, 1, 7],
        [8, 3, 2, 1, 9999, 4],
        [2, 9, 6, 7, 4, 9999],
    ]
)
TINY_SETS = [[0, 1], [2, 3], [4, 5]]


def check_feasible(tour, sets, n):
    assert len(tour) == len(sets)
    membership = {}
    for i, s in enumerate(sets):
        for v in s:
            membership[v] = i
    visited = {membership[v] for v in tour}
    assert visited == set(range(len(sets)))
    assert all(0 <= v < n for v in tour)


def test_solve_in_memory():
    sol = glns.solve(TINY_DIST, TINY_SETS, seed=42)
    check_feasible(sol.tour, TINY_SETS, 6)
    assert sol.cost == 9  # known optimum
    assert sol.solve_time >= 0.0
    assert not sol.timeout


def test_seed_determinism():
    a = glns.solve(TINY_DIST, TINY_SETS, seed=7)
    b = glns.solve(TINY_DIST, TINY_SETS, seed=7)
    assert a.tour == b.tour
    assert a.cost == b.cost


def test_solve_file():
    sol = glns.solve_file(os.path.join(EXAMPLES, "39rat195.gtsp"), seed=1, trials=2)
    assert len(sol.tour) == 39
    assert sol.cost > 0


def test_asymmetric_and_list_input():
    # plain lists, asymmetric costs
    dist = [[0, 1, 10, 10], [10, 0, 1, 10], [10, 10, 0, 1], [1, 10, 10, 0]]
    sol = glns.solve(dist, [[0], [1], [2], [3]], seed=3)
    assert sol.cost == 4  # 0 -> 1 -> 2 -> 3 -> 0
    check_feasible(sol.tour, [[0], [1], [2], [3]], 4)


def test_float_dtype_rejected():
    try:
        glns.solve(TINY_DIST.astype(float), TINY_SETS)
    except TypeError as e:
        assert "integer" in str(e)
    else:
        raise AssertionError("float dist should raise TypeError")


def test_invalid_sets_rejected():
    try:
        glns.solve(TINY_DIST, [[0, 1], [1, 2], [4, 5]])  # vertex 1 twice, 3 missing
    except RuntimeError as e:
        assert "belongs" in str(e)
    else:
        raise AssertionError("overlapping sets should raise")


def test_budget_and_max_time_flags():
    sol = glns.solve(TINY_DIST, TINY_SETS, seed=1, budget=1_000_000)
    assert sol.budget_met  # any tour beats a huge budget


if __name__ == "__main__":
    if len(sys.argv) > 1:
        EXAMPLES = sys.argv[1]
    tests = [(k, v) for k, v in sorted(globals().items()) if k.startswith("test_")]
    for name, fn in tests:
        fn()
        print(f"ok {name}")
    print("ALL PYTHON TESTS PASSED")
