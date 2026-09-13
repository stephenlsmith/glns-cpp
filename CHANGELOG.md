# Changelog

All notable changes to GLNS C++ will be documented in this file.

## 0.1.0 - Unreleased

### Added

- C++17 solver library and command-line interface.
- Plain C API and nanobind-based Python bindings.
- GTSPLIB and simple-format parsers.
- Reproducible runs through an optional random seed.
- Installable CMake package and cross-platform CI configuration.

### Validation

- Parser parity checks against the Julia implementation on GTSPLIB instances.
- Feasibility, determinism, C API, Python, malformed-input, and sanitizer tests.

### Fixed

- Honor timeout and budget limits after initialization and at restart boundaries,
  including runs with no search iterations; stop as soon as a zero-cost optimum
  is found during search.
- Include the terminating search iteration in `total_iterations`.
- Reject explicit instances without edge weights, noninteger Python vertex IDs,
  malformed CLI numbers, negative CLI seeds, and nonfinite C API options.
- Avoid progress-bar and trial-counter overflow for large trial limits.
- Reject a default-constructed, unfinalized C++ instance before parameter resolution.
