// Copyright 2017 Stephen L. Smith and Frank Imeson
// Licensed under the Apache License, Version 2.0.  See LICENSE.
//
// Basic tests: parser ground truth (extracted from the Julia reference
// implementation) and solver feasibility on the example instances.

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include "glns/glns.hpp"

namespace {

int failures = 0;

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);      \
            ++failures;                                                      \
        }                                                                    \
    } while (0)

struct GroundTruth {
    const char* file;
    int n;
    int m;
    std::uint64_t distchk;   // FNV-style row-major checksum of the matrix
    std::uint64_t memsum;    // sum of 1-based membership over all vertices
    std::uint64_t setchk;    // checksum of 1-based vertex ids in set order
};

// values extracted from the Julia implementation's read_file
const GroundTruth kGroundTruth[] = {
    {"tiny.gtsp", 6, 3, 5291534990474486506ULL, 12, 30569571ULL},
    {"test.gtsp", 266, 133, 13087035679522629147ULL, 17822, 10463717615380850981ULL},
    {"39rat195.gtsp", 195, 39, 12315225994511939875ULL, 3860, 8855808404433978968ULL},
    {"107si535.gtsp", 535, 107, 10027858960119227500ULL, 27005, 10758305491556570234ULL},
    {"207si1032.gtsp", 1032, 207, 10537040188494950250ULL, 90301, 17289603048801125470ULL},
};

void test_parser(const std::string& dir) {
    const std::uint64_t kFnv = 1099511628211ULL;
    for (const GroundTruth& gt : kGroundTruth) {
        const glns::Instance inst = glns::read_instance(dir + "/" + gt.file);
        CHECK(inst.num_vertices == gt.n);
        CHECK(inst.num_sets == gt.m);

        std::uint64_t distchk = 0;
        for (int i = 0; i < inst.num_vertices; ++i) {
            for (int j = 0; j < inst.num_vertices; ++j) {
                distchk = distchk * kFnv + static_cast<std::uint64_t>(inst.dist(i, j));
            }
        }
        CHECK(distchk == gt.distchk);

        std::uint64_t memsum = 0;
        for (int v = 0; v < inst.num_vertices; ++v) {
            memsum += static_cast<std::uint64_t>(inst.membership[v]) + 1;
        }
        CHECK(memsum == gt.memsum);

        std::uint64_t setchk = 0;
        for (const std::vector<int>& s : inst.sets) {
            for (int v : s) {
                setchk = setchk * 31 + static_cast<std::uint64_t>(v) + 1;
            }
        }
        CHECK(setchk == gt.setchk);
        std::printf("parsed %-16s n=%-5d m=%-4d checksums ok\n", gt.file,
                    inst.num_vertices, inst.num_sets);
    }
}

void test_parser_rejections(const std::string& dir) {
    for (const char* file : {"matrix-too-few.gtsp", "matrix-too-many.gtsp",
                             "matrix-invalid-number.gtsp", "coordinate-too-few.gtsp",
                             "set-missing-terminator.gtsp", "missing-weights.gtsp"}) {
        bool rejected = false;
        try {
            (void)glns::read_instance(dir + "/" + file);
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        CHECK(rejected);
    }
}

void test_parser_formats(const std::string& dir) {
    for (const char* file : {"lower-diag-row.gtsp", "lower-row.gtsp",
                             "upper-diag-row.gtsp", "upper-row.gtsp"}) {
        const glns::Instance inst = glns::read_instance(dir + "/" + file);
        CHECK(inst.num_vertices == 3);
        CHECK(inst.num_sets == 3);
        CHECK(inst.dist(0, 0) == 0);
        CHECK(inst.dist(0, 1) == 1);
        CHECK(inst.dist(1, 0) == 1);
        CHECK(inst.dist(0, 2) == 2);
        CHECK(inst.dist(2, 0) == 2);
        CHECK(inst.dist(1, 2) == 3);
        CHECK(inst.dist(2, 1) == 3);
    }

    struct CoordinateCase {
        const char* file;
        glns::Cost distance;
    };
    for (const CoordinateCase& test : {
             CoordinateCase{"ceil-2d.gtsp", 2},
             CoordinateCase{"man-2d.gtsp", 2},
             CoordinateCase{"att.gtsp", 2},
             CoordinateCase{"geo.gtsp", 1},
         }) {
        const glns::Instance inst = glns::read_instance(dir + "/" + test.file);
        CHECK(inst.num_vertices == 2);
        CHECK(inst.dist(0, 1) == test.distance);
        CHECK(inst.dist(1, 0) == test.distance);
    }
}

bool feasible(const glns::Instance& inst, const std::vector<int>& tour) {
    if (static_cast<int>(tour.size()) != inst.num_sets) return false;
    std::vector<bool> seen(inst.num_sets, false);
    for (int v : tour) {
        const int s = inst.membership[v];
        if (seen[s]) return false;
        seen[s] = true;
    }
    for (bool b : seen) {
        if (!b) return false;
    }
    return true;
}

glns::Cost recompute_cost(const glns::Instance& inst, const std::vector<int>& tour) {
    glns::Cost total = inst.dist(tour.back(), tour.front());
    for (std::size_t i = 0; i + 1 < tour.size(); ++i) {
        total += inst.dist(tour[i], tour[i + 1]);
    }
    return total;
}

void test_solver(const std::string& dir) {
    for (const char* file : {"tiny.gtsp", "39rat195.gtsp"}) {
        for (const char* mode : {"default", "fast", "slow"}) {
            glns::Params params;
            params.mode = mode;
            params.seed = 42;
            params.trials = 2;
            params.verbose = 0;
            const glns::Instance inst = glns::read_instance(dir + "/" + std::string(file));
            const glns::Solution sol = glns::solve(inst, params);
            CHECK(feasible(inst, sol.tour));
            CHECK(sol.cost == recompute_cost(inst, sol.tour));
            CHECK(sol.cost > 0);
            std::printf("solved %-16s mode=%-8s cost=%lld iters=%lld time=%.2fs\n", file,
                        mode, static_cast<long long>(sol.cost),
                        static_cast<long long>(sol.total_iterations), sol.solve_time);
        }
    }

    // determinism: same seed must give the same tour
    const glns::Instance inst = glns::read_instance(dir + "/39rat195.gtsp");
    glns::Params params;
    params.seed = 7;
    params.trials = 1;
    params.verbose = 0;
    const glns::Solution a = glns::solve(inst, params);
    const glns::Solution b = glns::solve(inst, params);
    CHECK(a.cost == b.cost);
    CHECK(a.tour == b.tour);
    std::printf("determinism ok (seed 7 twice -> cost %lld)\n",
                static_cast<long long>(a.cost));
}

void test_invalid_instance() {
    bool unfinalized_rejected = false;
    try {
        (void)glns::solve(glns::Instance{});
    } catch (const std::runtime_error& e) {
        unfinalized_rejected = std::string(e.what()).find("not finalized") !=
                               std::string::npos;
    }
    CHECK(unfinalized_rejected);

    glns::Instance inst;
    inst.num_vertices = 4;
    inst.num_sets = 2;
    inst.dist = glns::Matrix(4, 1);
    inst.sets = {{0, 1, 2, 3}, {}};

    bool rejected = false;
    try {
        inst.finalize();
    } catch (const std::runtime_error& e) {
        rejected = std::string(e.what()).find("empty") != std::string::npos;
    }
    CHECK(rejected);

    bool negative_rejected = false;
    try {
        glns::Matrix matrix(2, 0);
        matrix.set(0, 1, -1);
    } catch (const std::runtime_error& e) {
        negative_rejected = std::string(e.what()).find("negative") != std::string::npos;
    }
    CHECK(negative_rejected);

    bool dimension_rejected = false;
    try {
        (void)glns::Matrix(-1, 0);
    } catch (const std::runtime_error& e) {
        dimension_rejected = std::string(e.what()).find("dimension") != std::string::npos;
    }
    CHECK(dimension_rejected);
}

void test_invalid_parameters(const std::string& dir) {
    const glns::Instance inst = glns::read_instance(dir + "/tiny.gtsp");

    auto rejected = [&](const glns::Params& params, const char* message) {
        try {
            (void)glns::solve(inst, params);
            return false;
        } catch (const std::runtime_error& e) {
            return std::string(e.what()).find(message) != std::string::npos;
        }
    };

    glns::Params params;
    params.trials = 0;
    CHECK(rejected(params, "trials"));
    params = {};
    params.num_iterations = 0;
    CHECK(rejected(params, "num_iterations"));
    params = {};
    params.epsilon = 1.1;
    CHECK(rejected(params, "epsilon"));
    params = {};
    params.noise = "typo";
    CHECK(rejected(params, "noise"));
    params = {};
    params.verbose = 4;
    CHECK(rejected(params, "verbose"));
}

void test_zero_cost_optimum() {
    glns::Instance inst;
    inst.num_vertices = 4;
    inst.num_sets = 2;
    inst.dist = glns::Matrix(4, 0);
    inst.sets = {{0, 1}, {2, 3}};
    inst.finalize();

    glns::Params params;
    params.seed = 1;
    const glns::Solution sol = glns::solve(inst, params);
    CHECK(sol.cost == 0);
    CHECK(sol.total_iterations == 0);
    CHECK(!sol.timeout);
}

void test_stopping_conditions() {
    glns::Instance inst;
    inst.num_vertices = 2;
    inst.num_sets = 2;
    inst.sets = {{0}, {1}};
    inst.dist = glns::Matrix(2, 1);
    inst.finalize();

    for (const char* mode : {"default", "fast", "slow"}) {
        glns::Params params;
        params.mode = mode;
        params.seed = 1;
        params.num_iterations = 1;  // no inner-loop iterations in these modes
        params.trials = std::numeric_limits<int>::max();
        params.restarts = std::numeric_limits<int>::max();
        params.max_time = 0;
        const auto timed = glns::solve(inst, params);
        CHECK(timed.timeout);
        CHECK(!timed.budget_met);
        CHECK(timed.total_iterations == 0);
        CHECK(timed.cost == 2);
        CHECK(feasible(inst, timed.tour));

        params.max_time = 60;
        params.budget = 2;
        const auto budgeted = glns::solve(inst, params);
        CHECK(!budgeted.timeout);
        CHECK(budgeted.budget_met);
        CHECK(budgeted.total_iterations == 0);
        CHECK(budgeted.cost == 2);

        params.max_time = 0;
        const auto both = glns::solve(inst, params);
        CHECK(both.timeout);
        CHECK(both.budget_met);
    }

    // A positive deadline must also interrupt restarts when no iteration runs.
    glns::Params params;
    params.seed = 1;
    params.num_iterations = 1;
    params.trials = std::numeric_limits<int>::max();
    params.max_time = 0.01;
    for (int restarts : {0, std::numeric_limits<int>::max()}) {
        params.restarts = restarts;
        const auto timed = glns::solve(inst, params);
        CHECK(timed.timeout);
        CHECK(timed.total_iterations == 0);
    }
}

void test_terminating_iteration_count() {
    glns::Instance inst;
    inst.num_vertices = 4;
    inst.num_sets = 2;
    inst.sets = {{0, 1}, {2, 3}};
    inst.dist = glns::Matrix(4, 10);
    inst.dist.set(0, 2, 1);
    inst.dist.set(2, 0, 1);
    inst.finalize();

    // For two sets, reoptimization finds the global optimum in one iteration.
    // Select an initialization whose cost exceeds that optimum.
    bool checked = false;
    for (int seed = 0; seed < 16 && !checked; ++seed) {
        glns::Params params;
        params.seed = seed;
        params.max_time = 0;
        if (glns::solve(inst, params).cost == 2) continue;
        params.max_time = 60;
        params.budget = 2;
        const auto sol = glns::solve(inst, params);
        CHECK(sol.cost == 2);
        CHECK(sol.budget_met);
        CHECK(!sol.timeout);
        CHECK(sol.total_iterations == 1);
        checked = true;
    }
    CHECK(checked);
}

void test_zero_cost_found_during_search() {
    glns::Instance inst;
    inst.num_vertices = inst.num_sets = 8;
    inst.dist = glns::Matrix(8, 10);
    for (int v = 0; v < 8; ++v) {
        inst.sets.push_back({v});
        inst.dist.set(v, (v + 1) % 8, 0);
    }
    inst.finalize();
    glns::Params params;
    params.seed = 0;
    params.trials = 1;
    params.max_time = 0;
    CHECK(glns::solve(inst, params).cost > 0);

    params.max_time = 60;
    const auto sol = glns::solve(inst, params);
    CHECK(sol.cost == 0);
    CHECK(feasible(inst, sol.tour));
    CHECK(sol.total_iterations > 0);
    CHECK(sol.total_iterations < 480);
    CHECK(!sol.timeout);
    CHECK(!sol.budget_met);
}

void test_output_error(const std::string& instance_dir, const std::string& output_dir) {
    const glns::Instance inst = glns::read_instance(instance_dir + "/tiny.gtsp");
    glns::Params params;
    params.seed = 1;
    params.trials = 1;
    params.output_file = output_dir + "/missing/tour.txt";

    bool rejected = false;
    try {
        (void)glns::solve(inst, params);
    } catch (const std::runtime_error& e) {
        rejected = std::string(e.what()).find("output file") != std::string::npos;
    }
    CHECK(rejected);
}

void test_output_file(const std::string& instance_dir, const std::string& output_dir) {
    const glns::Instance inst = glns::read_instance(instance_dir + "/tiny.gtsp");
    const std::string output = output_dir + "/glns-test-tour.txt";
    glns::Params params;
    params.seed = 1;
    params.trials = 1;
    params.output_file = output;
    const glns::Solution sol = glns::solve(inst, params);

    std::ifstream file(output);
    const std::string contents((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());
    CHECK(file.good() || file.eof());
    CHECK(contents.find("Tour Cost        : " + std::to_string(sol.cost)) !=
          std::string::npos);
    CHECK(std::remove(output.c_str()) == 0);
}

}  // namespace

int main(int argc, char** argv) {
    const std::string dir = argc > 1 ? argv[1] : "examples";
    const std::string fixture_dir = argc > 2 ? argv[2] : "tests/fixtures";
    const std::string output_dir = argc > 3 ? argv[3] : ".";
    test_parser(dir);
    test_parser_rejections(fixture_dir);
    test_parser_formats(fixture_dir);
    test_solver(dir);
    test_invalid_instance();
    test_invalid_parameters(dir);
    test_zero_cost_optimum();
    test_stopping_conditions();
    test_terminating_iteration_count();
    test_zero_cost_found_during_search();
    test_output_error(dir, fixture_dir);
    test_output_file(dir, output_dir);
    if (failures == 0) {
        std::printf("\nALL TESTS PASSED\n");
        return 0;
    }
    std::printf("\n%d FAILURES\n", failures);
    return 1;
}
