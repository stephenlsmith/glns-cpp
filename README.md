# GLNS (C++)

A C++ port of **GLNS**, an effective large neighborhood search solver for the
Generalized Traveling Salesman Problem (GTSP). The original reference
implementation is written in Julia and lives at
[stephenlsmith/GLNS.jl](https://github.com/stephenlsmith/GLNS.jl); more
information on the solver is available at
<https://ece.uwaterloo.ca/~sl2smith/GLNS/>.

**Status: work in progress.** The core solver, command-line interface,
C API, and Python bindings are complete; the solver is validated against the
Julia implementation on the GTSPLIB benchmark set (see `docs/`). Prebuilt
wheels and CI are planned.

## Citing this work

The GLNS solver and its settings are described in the following paper
[[DOI]](https://doi.org/10.1016/j.cor.2017.05.010)
[[PDF]](https://ece.uwaterloo.ca/~sl2smith/papers/2017COR-GLNS.pdf):

    @Article{Smith2017GLNS,
        author =    {S. L. Smith and F. Imeson},
        title =     {{GLNS}: An Effective Large Neighborhood Search Heuristic
                     for the Generalized Traveling Salesman Problem},
        journal =   {Computers \& Operations Research},
        volume =    87,
        pages =     {1-19},
        year =      2017,
    }

Please cite this paper when using GLNS.

## Building

Requires a C++17 compiler. With CMake:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build
```

Or directly, without CMake:

```bash
clang++ -std=c++17 -O3 -ffp-contract=off -Iinclude -Isrc src/*.cpp cli/main.cpp -o glns
```

(`-ffp-contract=off` keeps TSPLIB coordinate-to-distance rounding identical
to the Julia implementation; fused multiply-adds can shift a distance that
lands exactly on a rounding boundary by one unit.)

## Command-line usage

The CLI is flag-compatible with the Julia `GLNScmd.jl`:

```bash
./build/glns examples/39rat195.gtsp
./build/glns examples/39rat195.gtsp -mode=fast -output=tour.txt

# run "persistently" for a given amount of time (60 seconds here)
./build/glns examples/39rat195.gtsp -max_time=60 -trials=100000

# reproducible runs
./build/glns examples/39rat195.gtsp -seed=42
```

Run `./build/glns -help` for the full list of flags. The input is a text file
in [GTSPLIB format](http://www.cs.rhul.ac.uk/home/zvero/GTSPLIB/), an
extension of the TSPLIB format.

## Library usage

```cpp
#include "glns/glns.hpp"

// from a GTSPLIB file
glns::Instance instance = glns::read_instance("examples/39rat195.gtsp");

// ... or build an instance in memory (0-indexed vertices)
// glns::Instance instance;
// instance.num_vertices = ...; instance.num_sets = ...;
// instance.dist = glns::Matrix(n, 0);  instance.dist.set(i, j, ...);
// instance.sets = {{0, 1}, {2, 3}, ...};
// instance.finalize();

glns::Params params;
params.mode = "default";   // or "fast" / "slow"
params.seed = 42;          // omit for a nondeterministic run

glns::Solution solution = glns::solve(instance, params);
// solution.tour (0-indexed, one vertex per set), solution.cost, ...
```

## Python

Install from the repository root (requires CMake >= 3.18 and a C++17
compiler; published wheels will remove this requirement):

```bash
pip install .
```

```python
import numpy as np
import glns

# from a GTSPLIB file
sol = glns.solve_file("examples/39rat195.gtsp", seed=42)

# ... or in memory: integer cost matrix + sets partitioning the vertices
dist = np.array([[0, 3, 4, 7], [3, 0, 6, 5], [4, 6, 0, 4], [7, 5, 4, 0]])
sol = glns.solve(dist, [[0, 1], [2], [3]], mode="default", seed=42)

sol.tour        # one 0-indexed vertex per set, in visit order
sol.cost        # tour cost
sol.solve_time  # seconds; the GIL is released while solving
```

All solver options (`mode`, `trials`, `restarts`, `max_time`,
`num_iterations`, `budget`, `seed`, `verbose`, ...) are keyword arguments
with the same defaults as the Julia implementation. Costs must be integers;
scale and round floating-point costs first.

## C API

`include/glns/glns.h` exposes the solver to C and to anything with a C FFI
(MATLAB, Rust, Go, Java, ...). See `tests/test_c_api.c` for a complete
example:

```c
glns_params params;
glns_params_init(&params);
params.seed = 42; params.has_seed = 1;

glns_solution solution;
char err[256];
if (glns_solve(n, m, dist, set_sizes, set_vertices, &params,
               &solution, err, sizeof err) == 0) {
    /* solution.tour[0..tour_len), solution.cost */
    glns_solution_free(&solution);
}
```

## Relation to the Julia implementation

This is a faithful port: the algorithm, parameter defaults, and GTSPLIB
parser follow the Julia code (the file layout of `src/` mirrors the Julia
sources). Because GLNS is a stochastic anytime algorithm, individual runs
differ, but solution-quality distributions match the Julia implementation.
Known intentional deviations:

- The library API is silent by default (`verbose = 0`) and returns the tour
  in memory; the Julia default prints a progress bar and summary. The CLI
  keeps the Julia behavior (`-verbose=3`).
- An RNG seed can be supplied for reproducible runs.
- The `LOWER_ROW` edge weight format is implemented correctly rather than
  ported from the (untested and broken) Julia branch.
- Individual distances must fit in 32 bits (they are stored narrow to halve
  the solver's cache footprint; arithmetic and tour costs are still 64-bit).
  `Matrix::set` throws a clear error if a distance is out of range.

## License

Copyright 2018 Stephen L. Smith and Frank Imeson

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at <http://www.apache.org/licenses/LICENSE-2.0>

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

## Contact information

Prof. Stephen L. Smith
Department of Electrical and Computer Engineering
University of Waterloo
Waterloo, ON Canada
web: <https://ece.uwaterloo.ca/~sl2smith/>
email: <stephen.smith@uwaterloo.ca>
