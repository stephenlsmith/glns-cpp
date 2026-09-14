# GLNS (C++)

GLNS is a solver for the Generalized Traveling Salesman Problem (GTSP).
This repository contains a C++ implementation of GLNS, with a command-line
solver, C and C++ library interfaces, and Python bindings. Both symmetric
and asymmetric instances are supported.

The original Julia implementation is available at
[**stephenlsmith/GLNS.jl**](https://github.com/stephenlsmith/GLNS.jl).
This port follows the same large neighborhood search algorithm and mode
defaults. It can be used independently of Julia.

More information on the solver is given at
<https://ece.uwaterloo.ca/~sl2smith/GLNS/>.

Version 0.1.0 is in preparation. The installation instructions below build
from source; see the [changelog](CHANGELOG.md) for release notes.

## Citing this work

The GLNS solver and its settings are described in the following paper
[[DOI]](https://doi.org/10.1016/j.cor.2017.05.010)
[[PDF]](https://ece.uwaterloo.ca/~sl2smith/papers/2017COR-GLNS.pdf):

```bibtex
@Article{Smith2017GLNS,
    author =    {S. L. Smith and F. Imeson},
    title =     {{GLNS}: An Effective Large Neighborhood Search Heuristic
                 for the Generalized Traveling Salesman Problem},
    journal =   {Computers \& Operations Research},
    volume =    87,
    pages =     {1-19},
    year =      2017,
    doi =       {10.1016/j.cor.2017.05.010},
}
```

Please cite this paper when using GLNS.

## Using the solver

The solver can be run from the command line, called from Python, or linked
into a C or C++ application. GLNS is a heuristic: it searches for good
feasible tours without generally certifying optimality.

### Installation

To build the command-line solver and C/C++ library, install CMake 3.18 or
newer and a C++17 compiler. Run the following from the repository root:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

The command-line executable is `build/glns`. With Visual Studio, it is
usually `build/Release/glns.exe`; use that path in the examples below.

Run the C/C++ tests with:

```bash
cd build
ctest -C Release --output-on-failure
cd ..
```

For Python, use Python 3.9 or newer and install from the repository root:

```bash
python -m pip install .
```

This builds the Python extension and installs its NumPy dependency. A
C++17 compiler is required; the Python build backend can obtain CMake and
Ninja as needed. Installing the Python package builds the library bindings;
use the CMake commands above to build the command-line executable.

### Input instances

The file input is a text file in
[GTSPLIB format](http://www.cs.rhul.ac.uk/home/zvero/GTSPLIB/), an extension
of the [TSPLIB format](https://www.iwr.uni-heidelberg.de/groups/comopt/software/TSPLIB95/).
Example inputs are provided in [examples/](examples/).

Supported distance types are `EUC_2D`, `CEIL_2D`, `MAN_2D`, `GEO`, `ATT`,
and `EXPLICIT`. Supported explicit formats are `FULL_MATRIX`, `LOWER_ROW`,
`LOWER_DIAG_ROW`, `UPPER_ROW`, and `UPPER_DIAG_ROW`. The parser also accepts
the Julia solver's simple matrix format.

Instances must contain at least two nonempty sets that partition the
vertices: each vertex belongs to exactly one set. A tour selects one vertex
from each set and returns to its starting vertex. Individual edge costs
must be integers in the range **0 to 2,147,483,647**. Tour costs use 64-bit
integers and may exceed that range. Scale and round floating-point costs
before passing a matrix to the library.

**Indexing:** vertices in the C, C++, and Python APIs are **0-indexed**.
GTSPLIB files and the CLI's printed tours and tour files use **1-indexed**
vertex IDs.

### Solver usage and examples

GLNS has three modes: `default`, `fast`, and `slow`. These provide different
tradeoffs between search effort and solution quality. Options can also set
a time limit or stop the search when a cost threshold is met.

*Example 1:* Solve `39rat195.gtsp` with the default settings and write the
tour and its cost to `tour.txt`:

```bash
./build/glns examples/39rat195.gtsp -output=tour.txt
```

*Example 2:* Run with the `slow` setting:

```bash
./build/glns examples/39rat195.gtsp -mode=slow
```

*Example 3:* Allow repeated trials with a 60-second time limit, stopping
earlier if a tour of cost 13,505 or less is found:

```bash
./build/glns examples/107si535.gtsp -max_time=60 -budget=13505 -trials=100000
```

*Example 4:* Supply a seed for reproducible runs:

```bash
./build/glns examples/39rat195.gtsp -seed=42
```

The command-line syntax follows the Julia `GLNScmd.jl` interface:

```bash
./build/glns filename.gtsp -option=value
./build/glns --help
```

Common options are:

| CLI option | Meaning |
| --- | --- |
| `-mode=default` | Choose `default`, `fast`, or `slow`. |
| `-max_time=60` | Set the solver time limit in seconds. |
| `-budget=13505` | Stop when a tour with cost at most this value is found. |
| `-trials=5` | Number of cold trials, each starting from a new tour. |
| `-restarts=3` | Warm restart attempts within each cold trial. |
| `-num_iterations=60` | Iteration scale per set; controls cooling and stagnation thresholds, rather than imposing a hard iteration cap. |
| `-seed=42` | Use an unsigned 64-bit RNG seed. |
| `-verbose=0` | Set verbosity from 0 to 3; the CLI defaults to 3. |
| `-output=tour.txt` | Write the tour summary to a file. |

The values in the table are examples. Unset search options use the chosen
mode's defaults. `--help` also lists the reoptimization, noise, insertion,
removal, and adaptation options. With `-verbose=0`, the CLI prints just the
cost unless an output file is requested.

Time and budget limits are checked after building the initial feasible
tour, at restart boundaries, and after each search iteration. An operation
already in progress completes before the time limit is checked, so the
limit is not a strict wall-clock deadline. `max_time=0` returns an initial
tour with zero search iterations. A zero-cost tour stops the search
immediately, since nonnegative edge costs prove it is optimal.

A fixed seed reproduces a run with the same instance, options, and build
when time does not determine termination. Runs cut short by a time limit
can differ with machine speed and load; the seed does not synchronize the
C++ and Julia random-number streams.

### Calling from Python

Solve an instance from a file:

```python
import glns

sol = glns.solve_file("examples/39rat195.gtsp", mode="default", seed=42)
print(sol.cost)
print(sol.tour)  # 0-indexed vertex IDs
```

Or supply a distance matrix and the sets directly:

```python
import numpy as np
import glns

dist = np.array([
    [0, 3, 4, 7],
    [3, 0, 6, 5],
    [4, 6, 0, 4],
    [7, 5, 4, 0],
], dtype=np.int64)
sets = [[0, 1], [2], [3]]

sol = glns.solve(dist, sets, mode="fast", seed=42)
print(sol.cost, sol.tour)
```

`dist[i, j]` is the cost of traveling from vertex `i` to vertex `j`. Nested
Python lists are also accepted. Costs and vertex IDs must be integers;
floating-point arrays and fractional vertex IDs are rejected.

Solver options are keyword arguments with the same names as the CLI,
except that `-output` becomes `output_file`. For example:

```python
sol = glns.solve_file(
    "examples/107si535.gtsp",
    max_time=60, budget=13505, trials=100000, output_file="tour.txt",
)
```

The returned `Solution` has `tour`, `cost`, `solve_time` (seconds),
`timeout`, `budget_met`, and `total_iterations` (completed search iterations).
The Python API is silent by default (`verbose=0`) and releases the GIL
while the C++ solver runs.

### Calling from C++

The C++ API is declared in [include/glns/glns.hpp](include/glns/glns.hpp):

```cpp
#include <iostream>
#include <glns/glns.hpp>

int main() {
    glns::Instance instance = glns::read_instance("examples/39rat195.gtsp");
    glns::Params params;
    params.mode = "default";
    params.seed = 42;

    const glns::Solution sol = glns::solve(instance, params);
    std::cout << "Cost: " << sol.cost << '\n';
    for (int v : sol.tour) std::cout << v << ' ';
    std::cout << '\n';
}
```

To construct an instance in memory, replace the `read_instance` call with:

```cpp
glns::Instance instance;
instance.num_vertices = 4;
instance.num_sets = 3;
instance.sets = {{0, 1}, {2}, {3}};
instance.dist = glns::Matrix(4, 0);
const glns::Cost costs[4][4] = {
    {0, 3, 4, 7}, {3, 0, 6, 5}, {4, 6, 0, 4}, {7, 5, 4, 0}
};
for (int i = 0; i < 4; ++i)
    for (int j = 0; j < 4; ++j)
        instance.dist.set(i, j, costs[i][j]);
instance.finalize();
```

Call `finalize()` after constructing or modifying an instance, before
calling `solve()`. `read_instance()` does this automatically. Parsing and
validation errors are reported as C++ exceptions. As with Python, the
library is silent by default and returns the solution in memory.

To install the C/C++ library, headers, CLI, and CMake package:

```bash
cmake --install build --config Release --prefix /absolute/path/to/install
```

A downstream application's `CMakeLists.txt` can then contain:

```cmake
cmake_minimum_required(VERSION 3.18)
project(my_solver LANGUAGES CXX)
find_package(glns 0.1 CONFIG REQUIRED)
add_executable(my_solver main.cpp)
target_link_libraries(my_solver PRIVATE glns::core)
```

Set `-DCMAKE_PREFIX_PATH=/absolute/path/to/install` when configuring that
application. Projects using `add_subdirectory` can also link `glns::core`.

### Calling from C

[include/glns/glns.h](include/glns/glns.h) provides a C interface, also
suitable for use through a foreign-function interface:

```c
#include <stdio.h>
#include <glns/glns.h>

int main(void) {
    glns_params params;
    glns_params_init(&params);
    params.seed = 42;
    params.has_seed = 1;

    glns_solution sol;
    char error[256];
    int rc = glns_solve_file("examples/39rat195.gtsp", &params,
                             &sol, error, sizeof(error));
    if (rc != 0) {
        fprintf(stderr, "%s\n", error);
        glns_solution_free(&sol);
        return 1;
    }
    printf("Cost: %lld\n", (long long)sol.cost);
    glns_solution_free(&sol);
    return 0;
}
```

Use `glns_solve()` for an in-memory row-major `int64_t` distance matrix,
set sizes, and concatenated vertex IDs. See
[tests/test_c_api.c](tests/test_c_api.c) for an example. Initialize options
with `glns_params_init()`, and free each returned solution with
`glns_solution_free()` before reusing it. Functions return a nonzero status
and an error message on failure.

The implementation requires the C++ runtime. A CMake consumer can use
`project(my_solver LANGUAGES C CXX)` and link `glns::core`; see
[tests/install/](tests/install/). The header provides `GLNS_VERSION_*`
macros and `glns_version()` for version checks. The C and C++ APIs and ABIs
may change between minor versions before 1.0.

## Relation to the Julia implementation

This C++ port was developed with assistance from AI coding tools, including
Claude and OpenAI Codex, for implementation, testing, review, and documentation.
The original Julia implementation was written by Stephen L. Smith without
AI assistance.

The algorithm and mode defaults follow
[GLNS.jl](https://github.com/stephenlsmith/GLNS.jl). Individual stochastic
runs differ. The [benchmark comparison](docs/benchmark-2026-09-14.md)
uses ten runs per implementation on each of 45 GTSPLIB instances. It found
comparable solution quality and 21% less total solver time for C++, with
lower mean runtimes on all 45 instances on the benchmark machine. The
report includes per-instance results, validation, and reproduction tools.

The C++ version adds library interfaces, seed control, stricter input
validation, and stopping checks at initialization and restart boundaries.
It also handles zero-cost optima and the `LOWER_ROW` matrix format. Edge
costs are stored as signed 32-bit integers, while tour arithmetic uses
64-bit integers. Library calls are silent by default; the CLI retains the
Julia solver's default verbosity.

CMake configures floating-point contraction to preserve coordinate-distance
rounding against the Julia parser. For a direct Clang/GCC build, use:

```bash
c++ -std=c++17 -O3 -ffp-contract=off -Iinclude -Isrc src/*.cpp cli/main.cpp -o glns
```

## Tests

The CTest suite covers parsing, solver feasibility and cost consistency,
stopping conditions, reproducibility, validation, the C API, and CLI
options. To run the Python tests after installing the package:

```bash
python -m pip install pytest
python -m pytest -q python/tests
```

The [CI workflow](.github/workflows/ci.yml) configures C/C++ builds on Linux,
macOS, and Windows, an AddressSanitizer/UndefinedBehaviorSanitizer build,
and Python tests on versions 3.9 and 3.14.

## Index of files

- [cli/main.cpp](cli/main.cpp) — command-line solver.
- [include/glns/](include/glns/) — public C and C++ headers.
- [src/](src/) — solver, parser, tour optimizations, and supporting routines;
  the organization follows the Julia source files.
- [python/glns/](python/glns/) — Python interface.
- [bindings/python/](bindings/python/) — bindings to the C++ solver.
- [examples/](examples/) — sample GTSP instances.
- [tests/](tests/) and [python/tests/](python/tests/) — test suites.
- [docs/](docs/) — comparison with the Julia implementation.
- [CMakeLists.txt](CMakeLists.txt), [cmake/](cmake/), and
  [pyproject.toml](pyproject.toml) — build and packaging configuration.

## License

Copyright 2018 Stephen L. Smith and Frank Imeson

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
<http://www.apache.org/licenses/LICENSE-2.0>.

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See [LICENSE](LICENSE) for the specific language governing permissions and
limitations under the License.

## Contact information

Prof. Stephen L. Smith<br>
Department of Electrical and Computer Engineering<br>
University of Waterloo<br>
Waterloo, ON Canada<br>
web: <https://ece.uwaterloo.ca/~sl2smith/><br>
email: <stephen.smith@uwaterloo.ca>
