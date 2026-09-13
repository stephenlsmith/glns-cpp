# Copyright 2017 Stephen L. Smith and Frank Imeson
# Licensed under the Apache License, Version 2.0.  See LICENSE.
"""GLNS: a large neighborhood search solver for the Generalized TSP.

Reference: S. L. Smith and F. Imeson, "GLNS: An Effective Large Neighborhood
Search Heuristic for the Generalized Traveling Salesman Problem," Computers &
Operations Research, vol. 87, pp. 1-19, 2017. Please cite this paper when
using GLNS.
"""

import operator as _operator

import numpy as _np

from ._core import Solution, solve_file
from . import _core as _c

__all__ = ["Solution", "solve", "solve_file"]
__version__ = "0.1.0"


def solve(dist, sets, **params):
    """Solve a GTSP instance given in memory.

    Args:
        dist: square matrix of integer arc costs; ``dist[i][j]`` is the cost
            of traveling from vertex ``i`` to vertex ``j`` (asymmetric
            instances are supported).  Anything convertible to a 2-D numpy
            integer array is accepted.  Each entry must be nonnegative and
            fit in 32 bits;
            scale and round floating-point costs before calling.
        sets: iterable of iterables of 0-indexed integer vertex ids; the sets must
            partition ``{0, ..., n-1}``.
        **params: solver options, all keyword-only: ``mode`` ("default",
            "fast", or "slow"), ``trials``, ``restarts``, ``max_time``
            (seconds), ``num_iterations`` (per set), ``init_tour`` ("rand" or
            "insertion"), ``reopt``, ``epsilon``, ``noise``,
            ``insertion_algs``, ``removal_algs``, ``budget``, ``seed`` (for
            reproducible runs), ``verbose`` (0-3), and ``output_file``.
            Unset options use the mode's defaults, exactly as in the Julia
            implementation.

    Returns:
        Solution with ``tour`` (one 0-indexed vertex per set, in visit
        order), ``cost``, ``solve_time``, ``timeout``, ``budget_met``, and
        ``total_iterations``.
    """
    dist = _np.asarray(dist)
    if dist.ndim != 2 or dist.shape[0] != dist.shape[1]:
        raise ValueError(f"dist must be a square matrix, got shape {dist.shape}")
    if not _np.issubdtype(dist.dtype, _np.integer):
        raise TypeError(
            f"dist must have an integer dtype, got {dist.dtype}; GLNS uses "
            "integer costs -- scale and round floating-point costs first "
            "(e.g. np.round(dist * 1000).astype(np.int64))"
        )
    dist = _np.ascontiguousarray(dist, dtype=_np.int64)
    try:
        sets = [[_operator.index(v) for v in s] for s in sets]
    except TypeError as exc:
        raise TypeError("sets must contain integer vertex ids") from exc
    return _c.solve(dist, sets, **params)
