# Comparing GLNS C++ and Julia

These adapters run the current C++ library and a separate GLNS.jl checkout
with default search settings. Each repetition is a complete solver call:
`mode=default`, five cold trials, three warm restarts, 60 iterations per set,
360-second time limit, and no cost budget. The repetition count does not
change the solver's internal trial count.

## Run

Requires a C++17 compiler, CMake 3.18+, Python 3.9+, and Julia (tested with
1.12.7). No third-party Python or Julia packages are required. Supply a
GLNS.jl checkout and a GTSPLIB directory containing the desired `.gtsp`
files at its top level.
The original `benchmark-results.csv` or the saved `reference.csv` supplies
the reference best-known costs; its first two columns must be instance name
and integer cost. The GTSPLIB instance files are external inputs and are not
bundled with the C++ repository.

From the C++ repository root:

```bash
cmake -S tools/benchmark -B build/benchmark -DCMAKE_BUILD_TYPE=Release
cmake --build build/benchmark --config Release --parallel
python3 tools/benchmark/run.py \
  --julia-repo ../GLNS.jl \
  --instances ../GLNS.jl/benchmark/GTSPLIB \
  --best-known docs/benchmark-2026-09-14/reference.csv \
  --cpp-worker build/benchmark/glns_benchmark_worker \
  --output benchmark-results \
  --runs 10 \
  --machine "Record CPU, memory, and power configuration here" \
  --build-description "Record compiler version and Release flags here"
python3 tools/benchmark/summarize.py benchmark-results
```

With a multi-configuration generator, the worker is typically under
`build/benchmark/Release/`. Use a fresh output directory: the runner refuses
to overwrite existing `runs.csv` data. A smoke run can use `--runs 1 --only
39rat195 35ftv170` with a different output directory. Keep the computer on
AC power and avoid other heavy workloads during timing. On macOS, prefix
`python3` with `caffeinate -i` to prevent idle sleep during the benchmark.

The runner uses seeds 1 through the repetition count for every instance in
both implementations. Equal seed numbers do not imply equal random streams
between languages. It shuffles the instance/repetition pairs with a fixed
schedule seed (default `20260914`), then alternates which implementation runs
first in each pair. Only one solver executes at a time.

## Measurement and validation

- Both implementations run in persistent worker processes. Julia uses one
  execution thread and one GC thread. Every instance is parsed before
  timing, followed by full warm-up solves on `39rat195` and `35ftv170` when
  available, or on the first instance for a custom subset.
- `solver_seconds` is the implementation's own unrounded timer, excluding
  file parsing, process startup, and summary output. `call_seconds` also
  includes parsing and the surrounding library call, but excludes process
  startup, command transport, and independent validation.
- Julia performs a full GC before each solver call, outside the measurement.
  Garbage collections caused by the measured solve remain included.
- Julia's original solver returns its result through a printing function.
  `worker.jl` replaces only that final output callback to capture the tour,
  cost, timer, and termination flags. The checkout and its search code are
  not edited. This adapter targets the API in GLNS.jl commit `1980c3c`;
  changes to that internal printing signature may require adapting the wrapper.
- Before measured runs, the workers compare dimensions, row-major full-matrix
  checksums, membership sums, and set-order checksums for every instance.
- Each C++ result is checked for one vertex per set and exact cost. Every
  Julia result is also checked by the C++ adapter against the instance.
  Those checks are outside the measured time. Any mismatch aborts the run.

## Output

- `runs.csv`: all individual observations, seeds, timings, termination flags,
  and tours normalized to **0-indexed** vertices. Each completed run is flushed
  to disk immediately.
- `instances.csv`: reference costs, dimensions, input-file SHA-256 digests,
  and matching parser checksums.
- `metadata.json`: source commits and hashes, adapter and binary hashes,
  versions, machine/build description, schedule, and timing methodology.
  A completed run is marked `complete: true`.
- `*.stderr.log`: diagnostic output from each worker; empty files are normal.
- `summary.csv`: generated per-instance statistics, including best/mean cost,
  sample standard deviation, reference hits, timeouts, and timing statistics.
- `aggregate.json`: generated aggregate quality and runtime statistics.
- `reference.csv`: generated reference costs, usable as `--best-known` without
  needing the original comparison spreadsheet.
- `table.md`: generated per-instance Markdown table for the report.

All instances receive equal weight in the aggregate mean percentage gap.
The speedup is Julia time divided by C++ time; values above one favor C++.
The aggregate total-time ratio weights expensive instances more heavily;
the geometric mean of per-instance ratios gives every instance equal weight.
Ten repetitions characterize observed variation on this benchmark set;
these descriptive statistics alone do not establish formal equivalence.
