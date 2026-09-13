/* Copyright 2017 Stephen L. Smith and Frank Imeson
 * Licensed under the Apache License, Version 2.0.  See LICENSE.
 *
 * C API smoke test: solves the tiny 6-vertex instance from examples/tiny.gtsp
 * (built in memory) and one instance from a file.
 */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "glns/glns.h"

static int failures = 0;

#define CHECK(cond)                                              \
    do {                                                         \
        if (!(cond)) {                                           \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            ++failures;                                          \
        }                                                        \
    } while (0)

int main(int argc, char** argv) {
    CHECK(GLNS_VERSION_MAJOR == 0);
    CHECK(strcmp(glns_version(), GLNS_VERSION_STRING) == 0);
    glns_params_init(NULL);

    /* the tiny.gtsp instance: 6 vertices, 3 sets, full matrix */
    const int64_t dist[36] = {
        9999, 3, 4, 7, 8, 2,
        3, 9999, 6, 5, 3, 9,
        4, 6, 9999, 4, 2, 6,
        7, 5, 4, 9999, 1, 7,
        8, 3, 2, 1, 9999, 4,
        2, 9, 6, 7, 4, 9999,
    };
    const int set_sizes[3] = {2, 2, 2};
    const int set_vertices[6] = {0, 1, 2, 3, 4, 5}; /* 0-indexed */

    glns_params params;
    glns_params_init(&params);
    params.seed = 42;
    params.has_seed = 1;
    params.insertion_algs = "randpdf";
    params.removal_algs = "classic";

    glns_solution solution;
    char err[256];
    int rc = glns_solve(6, 3, dist, set_sizes, set_vertices, &params, &solution, err,
                        sizeof(err));
    CHECK(rc == 0);
    if (rc == 0) {
        CHECK(solution.tour_len == 3);
        CHECK(solution.cost == 9); /* known optimum for tiny.gtsp */
        glns_solution_free(&solution);
        CHECK(solution.tour == NULL);
    } else {
        printf("error: %s\n", err);
    }

    /* error path: vertex in two sets */
    const int bad_vertices[6] = {0, 1, 1, 3, 4, 5};
    rc = glns_solve(6, 3, dist, set_sizes, bad_vertices, &params, &solution, err,
                    sizeof(err));
    CHECK(rc != 0);
    CHECK(strlen(err) > 0);
    CHECK(solution.tour == NULL);
    CHECK(solution.tour_len == 0);
    glns_solution_free(&solution);

    /* error path: invalid solver parameter */
    glns_params_init(&params);
    params.trials = 0;
    rc = glns_solve(6, 3, dist, set_sizes, set_vertices, &params, &solution, err,
                    sizeof(err));
    CHECK(rc != 0);
    CHECK(strstr(err, "trials") != NULL);

    /* error path: negative edge cost */
    int64_t negative_dist[36];
    memcpy(negative_dist, dist, sizeof(dist));
    negative_dist[1] = -1;
    glns_params_init(&params);
    rc = glns_solve(6, 3, negative_dist, set_sizes, set_vertices, &params, &solution,
                    err, sizeof(err));
    CHECK(rc != 0);
    CHECK(strstr(err, "negative") != NULL);

    /* error path: invalid set size */
    const int empty_set_sizes[3] = {2, 0, 4};
    rc = glns_solve(6, 3, dist, empty_set_sizes, set_vertices, &params, &solution,
                    err, sizeof(err));
    CHECK(rc != 0);
    CHECK(strstr(err, "set size") != NULL);

    glns_params_init(&params);
    params.seed = 42;
    params.has_seed = 1;

    /* Nonfinite values are errors, not negative-value default sentinels. */
    const double nonfinite[] = {NAN, INFINITY, -INFINITY};
    const char* fields[] = {"max_time", "reopt", "epsilon"};
    for (int field = 0; field < 3; ++field) {
        for (int value = 0; value < 3; ++value) {
            glns_params_init(&params);
            params.trials = 1;
            params.restarts = 0;
            params.seed = 1;
            params.has_seed = 1;
            if (field == 0) params.max_time = nonfinite[value];
            if (field == 1) params.reopt = nonfinite[value];
            if (field == 2) params.epsilon = nonfinite[value];
            rc = glns_solve(6, 3, dist, set_sizes, set_vertices, &params, &solution,
                            err, sizeof(err));
            CHECK(rc != 0);
            CHECK(strstr(err, fields[field]) != NULL);
            CHECK(solution.tour == NULL);
            glns_solution_free(&solution);
        }
    }
    glns_params_init(&params);
    params.seed = 42;
    params.has_seed = 1;

    /* file path, if an examples directory was given */
    if (argc > 1) {
        char path[512];
        snprintf(path, sizeof(path), "%s/39rat195.gtsp", argv[1]);
        rc = glns_solve_file(path, &params, &solution, err, sizeof(err));
        CHECK(rc == 0);
        if (rc == 0) {
            CHECK(solution.tour_len == 39);
            CHECK(solution.cost > 0);
            printf("39rat195 via C API: cost=%lld time=%.2fs\n",
                   (long long)solution.cost, solution.solve_time);
            glns_solution_free(&solution);
        } else {
            printf("error: %s\n", err);
        }
    }

    if (failures == 0) {
        printf("C API TESTS PASSED\n");
        return 0;
    }
    printf("%d FAILURES\n", failures);
    return 1;
}
