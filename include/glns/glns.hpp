// Copyright 2017 Stephen L. Smith and Frank Imeson
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// C++ port of the GLNS GTSP solver (https://github.com/stephenlsmith/GLNS.jl).
// Reference: S. L. Smith and F. Imeson, "GLNS: An Effective Large Neighborhood
// Search Heuristic for the Generalized Traveling Salesman Problem,"
// Computers & Operations Research, vol. 87, pp. 1-19, 2017.

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace glns {

using Cost = std::int64_t;

// Dense row-major cost matrix; operator()(i, j) is the cost of arc i -> j.
class Matrix {
public:
    Matrix() = default;
    Matrix(int n, Cost fill) : n_(n), data_(static_cast<std::size_t>(n) * n, fill) {}
    Cost& operator()(int i, int j) { return data_[static_cast<std::size_t>(i) * n_ + j]; }
    Cost operator()(int i, int j) const { return data_[static_cast<std::size_t>(i) * n_ + j]; }
    int size() const { return n_; }

private:
    int n_ = 0;
    std::vector<Cost> data_;
};

// A GTSP instance.  Vertices are 0-indexed; the sets must partition
// {0, ..., num_vertices - 1}.
struct Instance {
    int num_vertices = 0;
    int num_sets = 0;
    std::vector<std::vector<int>> sets;
    Matrix dist;
    std::string name = "instance";

    // vertex -> index of the set containing it; filled by finalize()
    std::vector<int> membership;

    // Validates that the sets partition the vertices and fills membership.
    // Throws std::runtime_error on invalid input.
    void finalize();
};

// User-facing solver options.  Unset fields fall back to the chosen mode's
// defaults, exactly as in the Julia implementation (parameter_defaults.jl).
struct Params {
    std::string mode = "default";           // "default", "fast", or "slow"
    std::optional<int> trials;              // cold trials (restarts from scratch)
    std::optional<int> restarts;            // warm trials per cold trial
    std::optional<double> max_time;         // seconds before timing out
    std::optional<std::int64_t> num_iterations;  // per set; multiplied by num_sets
    std::optional<std::string> init_tour;   // "rand" or "insertion"
    std::optional<double> reopt;            // probability of reopt per iteration
    std::optional<double> epsilon;          // adaptive weight smoothing in [0, 1]
    std::optional<std::string> noise;       // "None", "Add", "Subset", or "Both"
    std::optional<std::string> insertion_algs;  // "default", "cheapest", "randpdf", "classic"
    std::optional<std::string> removal_algs;    // "default" or "classic"
    std::optional<Cost> budget;             // quit once a tour of this cost is found
    std::optional<std::uint64_t> seed;      // RNG seed; unset -> nondeterministic
    int verbose = 0;                        // 0 silent ... 3 progress bar
    std::string output_file = "None";       // tour file path, "None" to skip
};

struct Solution {
    std::vector<int> tour;     // one vertex per set, 0-indexed
    Cost cost = 0;
    double solve_time = 0.0;   // seconds
    bool timeout = false;      // stopped because max_time was hit
    bool budget_met = false;   // stopped because the cost budget was met
    std::int64_t total_iterations = 0;
};

// Parse a GTSPLIB-format (or "simple"-format) instance file.
// The returned instance is finalized.  Throws std::runtime_error on failure.
Instance read_instance(const std::string& filename);

// Solve a GTSP instance.  instance.finalize() must have been called
// (read_instance does this for you).
Solution solve(const Instance& instance, const Params& params = {});

}  // namespace glns
