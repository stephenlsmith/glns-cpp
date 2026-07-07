# GLNS (C++)

A C++ port of **GLNS**, an effective large neighborhood search solver for the
Generalized Traveling Salesman Problem (GTSP). The original reference
implementation is written in Julia and lives at
[stephenlsmith/GLNS.jl](https://github.com/stephenlsmith/GLNS.jl); more
information on the solver is available at
<https://ece.uwaterloo.ca/~sl2smith/GLNS/>.

**Status: work in progress.** The core solver and command-line interface are
complete and validated against the Julia implementation on the GTSPLIB
benchmark set. A C API and Python bindings are planned.

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
clang++ -std=c++17 -O2 -ffp-contract=off -Iinclude -Isrc src/*.cpp cli/main.cpp -o glns
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
// instance.dist = glns::Matrix(n, 0);  instance.dist(i, j) = ...;
// instance.sets = {{0, 1}, {2, 3}, ...};
// instance.finalize();

glns::Params params;
params.mode = "default";   // or "fast" / "slow"
params.seed = 42;          // omit for a nondeterministic run

glns::Solution solution = glns::solve(instance, params);
// solution.tour (0-indexed, one vertex per set), solution.cost, ...
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
