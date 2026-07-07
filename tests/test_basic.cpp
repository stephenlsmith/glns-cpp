// Copyright 2017 Stephen L. Smith and Frank Imeson
// Licensed under the Apache License, Version 2.0.  See LICENSE.
//
// Basic tests: parser ground truth (extracted from the Julia reference
// implementation) and solver feasibility on the example instances.

#include <cstdint>
#include <cstdio>
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

}  // namespace

int main(int argc, char** argv) {
    const std::string dir = argc > 1 ? argv[1] : "examples";
    test_parser(dir);
    test_solver(dir);
    if (failures == 0) {
        std::printf("\nALL TESTS PASSED\n");
        return 0;
    }
    std::printf("\n%d FAILURES\n", failures);
    return 1;
}
