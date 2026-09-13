/* Copyright 2017 Stephen L. Smith and Frank Imeson
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * C API for the GLNS GTSP solver.  See glns.hpp for the C++ API.
 */

#ifndef GLNS_H
#define GLNS_H

#include <stdint.h>

#define GLNS_VERSION_MAJOR 0
#define GLNS_VERSION_MINOR 1
#define GLNS_VERSION_PATCH 0
#define GLNS_VERSION_STRING "0.1.0"

#ifdef __cplusplus
extern "C" {
#endif

/* Solver options.  Initialize with glns_params_init, then override fields.
 * Finite negative numeric values (and NULL strings) mean "use the mode's default",
 * matching the Julia implementation's parameter_defaults.jl.
 * NaN and infinite floating-point options are rejected. */
typedef struct glns_params {
    const char* mode;       /* "default", "fast", or "slow" */
    int trials;             /* cold trials; < 0 for mode default */
    int restarts;           /* warm trials; < 0 for mode default */
    double max_time;        /* seconds; < 0 for mode default */
    int64_t num_iterations; /* per set, multiplied by num_sets; < 0 for default */
    double reopt;           /* reopt probability in [0,1]; < 0 for default */
    double epsilon;         /* adaptation rate in [0,1]; < 0 for default (0.5) */
    const char* init_tour;  /* "rand" or "insertion"; NULL for mode default */
    const char* noise;      /* "None", "Add", "Subset", or "Both"; NULL = "Add" */
    const char* insertion_algs; /* "default", "cheapest", "randpdf", or "classic" */
    const char* removal_algs;   /* "default" or "classic" */
    int64_t budget;         /* stop at this cost; INT64_MIN for none */
    uint64_t seed;          /* RNG seed; only used when has_seed is nonzero */
    int has_seed;           /* 0: nondeterministic run */
    int verbose;            /* 0 (silent) .. 3 (progress bar) */
    const char* output_file; /* tour file path; NULL to skip */
} glns_params;

/* Return the same value as GLNS_VERSION_STRING.  The returned storage is
 * owned by the library and remains valid for the lifetime of the process. */
const char* glns_version(void);

/* Passing NULL is permitted and has no effect. */
void glns_params_init(glns_params* params);

typedef struct glns_solution {
    int* tour;      /* one vertex per set, 0-indexed; free with glns_solution_free */
    int tour_len;
    int64_t cost;
    double solve_time;       /* seconds */
    int timeout;             /* nonzero if max_time was hit */
    int budget_met;          /* nonzero if the cost budget was met */
    int64_t total_iterations; /* completed removal-insertion iterations */
} glns_solution;

/* Solve a GTSP instance given in memory.
 *
 * dist:          row-major num_vertices x num_vertices matrix;
 *                dist[i*num_vertices + j] is the cost of arc i -> j.
 *                Each entry must be nonnegative and fit in 32 bits.
 * set_sizes:     num_sets entries.
 * set_vertices:  the sets' 0-indexed vertex ids, concatenated in set order
 *                (sum of set_sizes entries in total); the sets must
 *                partition {0, ..., num_vertices-1}.
 * params:        NULL for all defaults.
 *
 * Returns 0 on success.  On failure returns nonzero and, if errbuf is not
 * NULL, writes a NUL-terminated error message into it.  When solution is
 * non-NULL, it is initialized to an empty state before solving and can safely
 * be passed to glns_solution_free after either success or failure. */
int glns_solve(int num_vertices, int num_sets, const int64_t* dist,
               const int* set_sizes, const int* set_vertices,
               const glns_params* params, glns_solution* solution,
               char* errbuf, int errbuf_len);

/* Solve an instance in GTSPLIB (or "simple") file format. */
int glns_solve_file(const char* path, const glns_params* params,
                    glns_solution* solution, char* errbuf, int errbuf_len);

void glns_solution_free(glns_solution* solution);

#ifdef __cplusplus
}
#endif

#endif /* GLNS_H */
