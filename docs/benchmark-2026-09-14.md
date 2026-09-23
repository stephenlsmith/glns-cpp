# Comparison with GLNS.jl — 2026-09-14

Ten runs per implementation on each of 45 GTSPLIB instances show comparable
solution quality and modestly lower C++ runtimes in default mode. The mean
gap to the reference best-known costs was **0.048% for Julia and 0.044% for
C++**. C++ used **21% less total solver time** and was faster on average on
all 45 instances.

## Results

| Metric | Julia | C++ |
| --- | ---: | ---: |
| Mean percentage gap to reference best | 0.048% | 0.044% |
| Median per-instance mean gap | 0.000% | 0.000% |
| Runs matching reference best | 339 / 450 | 355 / 450 |
| Instances matching reference best at least once in ten runs | 42 / 45 | 43 / 45 |
| Instances matching reference best in all ten runs | 28 / 45 | 28 / 45 |
| Total solver time for 450 runs | 23.96 min | 18.93 min |

C++ had the lower mean cost on 10 instances, Julia on 5, with 30 ties.
The aggregate mean-gap difference was only 0.0046 percentage points in
favor of C++. These results support comparable solution quality; they do
not imply identical outcomes for individual stochastic runs.

The total-time ratio was **1.27×** in favor of C++ (21.0% less time).
The geometric mean of the per-instance speedups was **1.37×**, with
individual mean-time ratios ranging from **1.11× to 1.71×**. Including
parsing and the surrounding library call gave a total-time ratio of
**1.28×**. The complete measurement took approximately 44 minutes,
including validation and benchmark overhead.

## Validation

All 45 instances agreed on dimensions, full distance-matrix checksums,
membership sums, and set-order checksums before timing began. Every one
of the 900 measured tours was checked for exactly one vertex per set and
an exactly matching recomputed tour cost. There were **no invalid tours,
incorrect costs, timeouts, or cost-budget exits**.

## Method

The benchmark contains 45 GTSPLIB instances, from 31 to 217 sets and 152 to
1,084 vertices. Each implementation made ten complete solver calls per
instance, for **450 calls per implementation and 900 in total**. Each call
used default mode: five cold trials, three warm restart attempts, 60
iterations per set, a 360-second time limit, and no cost budget. The ten
repetitions are separate solver calls; they do not change these internal
search settings.

Both implementations used seeds 1–10 for each instance. Their random number
generators differ, so matching seed numbers do not imply matching search
trajectories. Instance/repetition pairs were shuffled using schedule seed
`20260914`, with Julia and C++ alternating which ran first. Only one solver
ran at a time.

| Component | Configuration |
| --- | --- |
| Machine | Apple M4 Pro, 14 cores (10 performance, 4 efficiency), 24 GiB RAM, AC power |
| Operating system | macOS 26.6.2, arm64 |
| C++ source | [768dd12](https://github.com/stephenlsmith/glns-cpp/commit/768dd12b7a1e0c6580633ace20ddf80fb01e6f79) |
| Julia source | [GLNS.jl 1980c3c](https://github.com/stephenlsmith/GLNS.jl/commit/1980c3c59d3625a34c10d183d5da5de9b2d63294) |
| C++ build | AppleClang 21.0.0, CMake 4.4.3 Release, `-O3 -DNDEBUG -std=c++17 -fPIC -ffp-contract=off` |
| Julia | 1.12.7, one execution thread and one GC thread, startup file disabled |

Both solvers ran in persistent processes. Every instance was parsed before
measurement, and full solves on `39rat195` and `35ftv170` warmed the solver
paths before the recorded runs. Julia performed a full garbage collection
before each call, outside the measurement; collections triggered during a
solve remain included.

The main timing measure is each implementation's own unrounded solver
timer, which includes solver preprocessing but excludes input parsing,
process startup, and summary output. The raw data also contain
`call_seconds`, including parsing and the surrounding library call. The
Julia adapter captures results by replacing only the final printing
callback at runtime; its search code and source checkout are unchanged.

Reference costs are a frozen copy of the best-known values in the original
GLNS.jl benchmark spreadsheet, saved as
[reference.csv](benchmark-2026-09-14/reference.csv). The reported percentage
gap is `100 × (cost / reference − 1)`, averaged with equal weight for each
instance. “Hits” count runs that exactly match the reference cost.

Runtime speedup means Julia time divided by C++ time. The total-time ratio
weights longer-running instances more heavily; the geometric mean of the
45 per-instance ratios gives each instance equal weight. These are
descriptive comparisons on one machine in default mode, not a formal
statistical equivalence test or a guarantee for other workloads.

## Per-instance results

Costs are reported as mean ± sample standard deviation across ten runs.
Times are means in seconds. Each hit count is out of ten, and a speedup
above 1 favors C++. The [summary CSV](benchmark-2026-09-14/summary.csv) also
contains best costs, timing standard deviations, and median times.

| Instance | Reference best | Julia mean ± SD | C++ mean ± SD | Hits Julia / C++ | Julia (s) | C++ (s) | Speedup |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 31pr152 | 51576 | 51576.0 ± 0.0 | 51576.0 ± 0.0 | 10 / 10 | 0.078 | 0.048 | 1.61× |
| 32u159 | 22664 | 22664.0 ± 0.0 | 22664.0 ± 0.0 | 10 / 10 | 0.086 | 0.054 | 1.59× |
| 35ftv170 | 1205 | 1205.0 ± 0.0 | 1205.0 ± 0.0 | 10 / 10 | 0.096 | 0.057 | 1.67× |
| 35si175 | 5564 | 5564.0 ± 0.0 | 5564.0 ± 0.0 | 10 / 10 | 0.106 | 0.067 | 1.58× |
| 36brg180 | 4420 | 4420.0 ± 0.0 | 4420.0 ± 0.0 | 10 / 10 | 0.116 | 0.068 | 1.71× |
| 39rat195 | 854 | 854.0 ± 0.0 | 854.0 ± 0.0 | 10 / 10 | 0.139 | 0.099 | 1.40× |
| 40d198 | 10557 | 10557.0 ± 0.0 | 10557.0 ± 0.0 | 10 / 10 | 0.135 | 0.093 | 1.46× |
| 40kroa200 | 13406 | 13406.0 ± 0.0 | 13406.0 ± 0.0 | 10 / 10 | 0.146 | 0.096 | 1.52× |
| 40krob200 | 13111 | 13111.0 ± 0.0 | 13111.0 ± 0.0 | 10 / 10 | 0.138 | 0.096 | 1.44× |
| 41gr202 | 23301 | 23301.0 ± 0.0 | 23301.0 ± 0.0 | 10 / 10 | 0.142 | 0.091 | 1.57× |
| 45ts225 | 68340 | 68340.0 ± 0.0 | 68340.0 ± 0.0 | 10 / 10 | 0.225 | 0.172 | 1.30× |
| 45tsp225 | 1612 | 1612.0 ± 0.0 | 1612.0 ± 0.0 | 10 / 10 | 0.189 | 0.139 | 1.36× |
| 46gr229 | 71972 | 71972.0 ± 0.0 | 71972.0 ± 0.0 | 10 / 10 | 0.199 | 0.141 | 1.41× |
| 46pr226 | 64007 | 64007.0 ± 0.0 | 64007.0 ± 0.0 | 10 / 10 | 0.162 | 0.107 | 1.51× |
| 53gil262 | 1013 | 1013.0 ± 0.0 | 1013.0 ± 0.0 | 10 / 10 | 0.280 | 0.202 | 1.39× |
| 53pr264 | 29549 | 29549.0 ± 0.0 | 29549.0 ± 0.0 | 10 / 10 | 0.247 | 0.174 | 1.42× |
| 56a280 | 1079 | 1079.0 ± 0.0 | 1079.0 ± 0.0 | 10 / 10 | 0.321 | 0.241 | 1.34× |
| 60pr299 | 22615 | 22615.0 ± 0.0 | 22615.0 ± 0.0 | 10 / 10 | 0.413 | 0.314 | 1.32× |
| 64lin318 | 20765 | 20765.0 ± 0.0 | 20765.0 ± 0.0 | 10 / 10 | 0.396 | 0.295 | 1.34× |
| 65rbg323 | 471 | 471.0 ± 0.0 | 471.0 ± 0.0 | 10 / 10 | 0.654 | 0.457 | 1.43× |
| 72rbg358 | 693 | 693.0 ± 0.0 | 693.0 ± 0.0 | 10 / 10 | 0.908 | 0.633 | 1.44× |
| 80rd400 | 6361 | 6361.0 ± 0.0 | 6361.0 ± 0.0 | 10 / 10 | 0.801 | 0.626 | 1.28× |
| 81rbg403 | 1170 | 1170.0 ± 0.0 | 1170.0 ± 0.0 | 10 / 10 | 0.768 | 0.527 | 1.46× |
| 84fl417 | 9651 | 9651.0 ± 0.0 | 9651.0 ± 0.0 | 10 / 10 | 0.803 | 0.596 | 1.35× |
| 87gr431 | 101946 | 101946.0 ± 0.0 | 101946.0 ± 0.0 | 10 / 10 | 1.193 | 0.923 | 1.29× |
| 88pr439 | 60099 | 60099.0 ± 0.0 | 60099.0 ± 0.0 | 10 / 10 | 1.200 | 0.936 | 1.28× |
| 89pcb442 | 21657 | 21665.5 ± 26.2 | 21665.3 ± 26.2 | 8 / 9 | 1.296 | 0.977 | 1.33× |
| 89rbg443 | 632 | 634.9 ± 1.7 | 635.1 ± 1.8 | 1 / 1 | 1.790 | 1.196 | 1.50× |
| 99d493 | 20023 | 20028.3 ± 9.0 | 20026.2 ± 4.7 | 6 / 6 | 2.168 | 1.637 | 1.32× |
| 107ali535 | 128639 | 128648.8 ± 6.8 | 128647.4 ± 7.2 | 3 / 4 | 2.561 | 1.907 | 1.34× |
| 107att532 | 13464 | 13464.4 ± 1.3 | 13464.4 ± 1.3 | 9 / 9 | 2.053 | 1.646 | 1.25× |
| 107si535 | 13502 | 13506.6 ± 4.9 | 13506.1 ± 4.3 | 2 / 3 | 2.339 | 1.766 | 1.32× |
| 113pa561 | 1038 | 1039.2 ± 3.5 | 1038.1 ± 0.3 | 8 / 9 | 2.395 | 1.987 | 1.20× |
| 115rat575 | 2388 | 2394.6 ± 5.7 | 2394.6 ± 5.7 | 4 / 4 | 2.860 | 2.127 | 1.34× |
| 115u574 | 16689 | 16689.0 ± 0.0 | 16689.0 ± 0.0 | 10 / 10 | 2.377 | 1.940 | 1.23× |
| 131p654 | 27428 | 27428.0 ± 0.0 | 27428.0 ± 0.0 | 10 / 10 | 3.004 | 2.255 | 1.33× |
| 132d657 | 22498 | 22522.3 ± 19.1 | 22504.2 ± 13.1 | 1 / 8 | 4.303 | 3.634 | 1.18× |
| 134gr666 | 163028 | 163536.6 ± 482.1 | 163689.2 ± 638.7 | 4 / 4 | 4.554 | 3.540 | 1.29× |
| 145u724 | 17272 | 17282.5 ± 7.4 | 17278.3 ± 4.3 | 1 / 3 | 6.566 | 4.892 | 1.34× |
| 157rat783 | 3262 | 3270.7 ± 4.3 | 3266.7 ± 2.8 | 0 / 0 | 7.483 | 6.654 | 1.12× |
| 200dsj1000 | 9187884 | 9199350.9 ± 9237.7 | 9197118.8 ± 6034.2 | 0 / 2 | 15.716 | 12.117 | 1.30× |
| 201pr1002 | 114311 | 114344.2 ± 42.9 | 114327.6 ± 35.0 | 6 / 8 | 14.944 | 11.339 | 1.32× |
| 207si1032 | 22306 | 22326.1 ± 9.0 | 22327.2 ± 7.4 | 0 / 0 | 18.393 | 13.663 | 1.35× |
| 212u1060 | 106007 | 106109.9 ± 82.8 | 106142.4 ± 114.0 | 1 / 1 | 20.196 | 16.105 | 1.25× |
| 217vm1084 | 130704 | 130855.8 ± 160.5 | 130881.0 ± 152.6 | 5 / 4 | 18.808 | 16.939 | 1.11× |

## Reproduction and raw data

The [benchmark tools](../tools/benchmark/README.md) document the complete
procedure. The GTSPLIB instances are linked from the
[GLNS web page](https://ece.uwaterloo.ca/~sl2smith/GLNS/). With the
reference Julia checkout and the 45 instance files in a local `GTSPLIB`
directory, run from the C++ repository root:

```bash
cmake -S tools/benchmark -B build/benchmark -DCMAKE_BUILD_TYPE=Release
cmake --build build/benchmark --config Release --parallel
python3 tools/benchmark/run.py \
  --julia-repo ../GLNS.jl \
  --instances ../GTSPLIB \
  --best-known docs/benchmark-2026-09-14/reference.csv \
  --cpp-worker build/benchmark/glns_benchmark_worker \
  --output benchmark-results \
  --runs 10 \
  --machine "Record your machine and power configuration" \
  --build-description "Record your compiler version and Release flags"
python3 tools/benchmark/summarize.py benchmark-results
```

Use a fresh output directory. With a multi-configuration generator, the
worker is typically in `build/benchmark/Release/`. GTSPLIB input files are
not bundled with this repository; their names and SHA-256 hashes are recorded
in the manifest so that the same inputs can be identified.

- [All 900 runs](benchmark-2026-09-14/runs.csv): seeds, costs, timings,
  termination flags, and tours with 0-indexed vertex IDs.
- [Instance manifest](benchmark-2026-09-14/instances.csv): dimensions,
  reference costs, input hashes, and parser checksums.
- [Measurement metadata](benchmark-2026-09-14/metadata.json): source commits
  and hashes, adapter hashes, versions, settings, and measurement times.
- [Per-instance summary](benchmark-2026-09-14/summary.csv) and
  [aggregate statistics](benchmark-2026-09-14/aggregate.json).
