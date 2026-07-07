// Copyright 2017 Stephen L. Smith and Frank Imeson
// Licensed under the Apache License, Version 2.0.  See LICENSE.
//
// Internal types and small hot-path utilities (port of utilities.jl).

#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "glns/glns.hpp"
#include "rng.hpp"

namespace glns {
namespace detail {

constexpr Cost kMaxCost = std::numeric_limits<Cost>::max();
constexpr Cost kMinCost = std::numeric_limits<Cost>::min();

// ---------------------------------------------------------------------------
// Tour

struct Tour {
    std::vector<int> tour;
    Cost cost = kMaxCost;
};

// ---------------------------------------------------------------------------
// Adaptive phases (early/mid/late in the Julia code)

enum Phase : int { kEarly = 0, kMid = 1, kLate = 2 };
constexpr int kNumPhases = 3;

// ---------------------------------------------------------------------------
// Resolved solver configuration (the param Dict in parameter_defaults.jl)

struct Config {
    std::string mode;
    int cold_trials = 0;
    int warm_trials = 0;
    double max_time = 0.0;
    std::string init_tour;

    double prob_reopt = 0.0;
    double accept_percentage = 0.0;
    double prob_accept = 0.0;

    std::int64_t num_iterations = 0;
    double latest_improvement = 0.0;
    double first_improvement = 0.0;
    int max_removals = 0;
    int min_removals = 0;

    std::vector<std::string> insertions;
    std::vector<double> insertion_powers;
    std::vector<std::string> removals;
    std::vector<double> removal_powers;

    double epsilon = 0.0;
    std::string noise_mode;
    Cost budget = kMinCost;

    int print_output = 0;             // verbosity 0..3
    double print_time_interval = 5.0; // seconds, for verbose == 1
    std::string output_file = "None";
    std::string instance_name;

    int num_vertices = 0;
    int num_sets = 0;
    int min_set_index = 0;  // index of the smallest set

    std::uint64_t seed = 0;
};

Config resolve_parameters(const Instance& inst, const Params& params);

// ---------------------------------------------------------------------------
// Set <-> vertex minimum distances (set_vertex_dist / Distsv in utilities.jl)

// stored as 32 bits like Matrix; every entry is a real minimum distance
struct Distsv {
    int num_sets = 0;
    int num_vertices = 0;
    std::vector<std::int32_t> set_vert;  // sv(s, v): min over u in s of dist(u, v)
    std::vector<std::int32_t> vert_set;  // vs(v, s): min over u in s of dist(v, u)
    std::vector<std::int32_t> min_sv;    // minsv(s, v): direction-agnostic minimum

    Cost sv(int s, int v) const { return set_vert[static_cast<std::size_t>(s) * num_vertices + v]; }
    Cost vs(int v, int s) const { return vert_set[static_cast<std::size_t>(v) * num_sets + s]; }
    Cost minsv(int s, int v) const { return min_sv[static_cast<std::size_t>(s) * num_vertices + v]; }
};

Distsv set_vertex_dist(const Matrix& dist, int num_sets, const std::vector<int>& member);

// ---------------------------------------------------------------------------
// Reusable scratch buffers threaded through the solver so the hot loop does
// not allocate.  Buffers are only valid within one call; functions that use
// one state it in their signature.

struct Workspace {
    std::vector<Cost> pdf_scratch;     // pdf_select's nth_element input
    std::vector<Cost> removal_costs;   // worst_vertices output
    std::vector<Cost> mindist;         // randpdf_insertion / distance_removal
    std::vector<int> deleted_vertices; // distance_removal
    std::vector<int> sets_to_insert;   // removal output -> insertion input
    std::vector<int> tour_inds;        // insert_subset_lb / moveopt_rand
    std::vector<int> prev;             // reopt_tour relaxation
    std::vector<Cost> cost_to_come;    // reopt_tour relaxation
    std::vector<int> rotated;          // reopt_tour rotated set order
    std::vector<int> new_tour;         // reopt_tour result
};

// ---------------------------------------------------------------------------
// Small inline utilities

// vertex before tour[i] on the cyclic tour
inline int prev_tour(const std::vector<int>& tour, std::size_t i) {
    return i != 0 ? tour[i - 1] : tour.back();
}

// rotate the tour to start at a random position (pivot_tour! in utilities.jl)
void pivot_tour(std::vector<int>& tour, Rng& rng);

// length of the cyclic tour
inline Cost tour_cost(const std::vector<int>& tour, const Matrix& dist) {
    Cost total = dist(tour.back(), tour.front());
    for (std::size_t i = 0; i + 1 < tour.size(); ++i) {
        total += dist(tour[i], tour[i + 1]);
    }
    return total;
}

// true if the tour visits each set exactly once
bool tour_feasibility(const std::vector<int>& tour, const std::vector<int>& membership,
                      int num_sets);

// index of the set with the fewest vertices
int min_set(const std::vector<std::vector<int>>& sets);

// simulated annealing acceptance
inline bool accepttrial(Cost trial_cost, Cost current_cost, double temperature, Rng& rng) {
    if (trial_cost <= current_cost) return true;
    const double accept_prob =
        std::exp(static_cast<double>(current_cost - trial_cost) / temperature);
    return rng.next_double() < accept_prob;
}

// unconditional-probability acceptance
inline bool accepttrial_noparam(Cost trial_cost, Cost current_cost, double prob_accept,
                                Rng& rng) {
    if (trial_cost <= current_cost) return true;
    return rng.next_double() < prob_accept;
}

// swap a random element of a[i..end] into position i and return it
// (incremental_shuffle! in utilities.jl)
inline int incremental_shuffle(std::vector<int>& a, std::size_t i, Rng& rng) {
    const std::size_t j = i + rng.bounded(a.size() - i);
    std::swap(a[j], a[i]);
    return a[i];
}

// uniformly random index among all positions of `a` holding `val`
// (rand_select in utilities.jl, via reservoir sampling instead of collecting)
inline int rand_select(const std::vector<Cost>& a, Cost val, Rng& rng) {
    std::uint64_t count = 0;
    int selected = 0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (a[i] == val) {
            ++count;
            if (rng.bounded(count) == 0) selected = static_cast<int>(i);
        }
    }
    return selected;
}

// ---------------------------------------------------------------------------
// Adaptive powers (adaptive_powers.jl)

struct Power {
    std::string name;
    double value = 0.0;
    std::array<double, kNumPhases> weight{{1.0, 1.0, 1.0}};
    std::array<double, kNumPhases> scores{{0.0, 0.0, 0.0}};
    std::array<std::int64_t, kNumPhases> count{{0, 0, 0}};
};

struct Powers {
    std::vector<Power> insertions;
    std::vector<Power> removals;
    std::vector<Power> noise;
    std::array<double, kNumPhases> insertion_total{};
    std::array<double, kNumPhases> removal_total{};
    std::array<double, kNumPhases> noise_total{};
};

Powers initialize_powers(const Config& config);
Power& power_select(std::vector<Power>& powers, const std::array<double, kNumPhases>& total,
                    Phase phase, Rng& rng);
void power_update(Powers& powers, const Config& config);

// ---------------------------------------------------------------------------
// Insertions, removals, initial tours (insertion_deletion.jl)

// performs the remove-insert step from `current` into the reusable `trial`
void remove_insert(const Tour& current, Tour& trial, const Matrix& dist,
                   const std::vector<int>& member, const Distsv& setdist,
                   std::vector<std::vector<int>>& sets, Powers& powers, const Config& config,
                   Phase phase, Workspace& ws, Rng& rng);

Tour initial_tour(Tour& lowest, const Matrix& dist, std::vector<std::vector<int>>& sets,
                  const Distsv& setdist, int trial_num, const Config& config, Rng& rng);

// pdf-biased index selection over integer weights (pdf_select in insertion_deletion.jl)
int pdf_select(const std::vector<Cost>& weights, double power, Rng& rng,
               std::vector<Cost>& scratch);

// ---------------------------------------------------------------------------
// Tour optimizations (tour_optimizations.jl)

// insertion position search shared by cheapest insertion and moveopt; updates
// (bestv, bestpos) when a cost below best_cost is found and returns the new best
Cost insert_cost_lb(const std::vector<int>& tour, const Matrix& dist,
                    const std::vector<int>& set, int setind, const Distsv& setdist,
                    int& bestv, int& bestpos, Cost best_cost);

void opt_cycle(Tour& current, const Matrix& dist, std::vector<std::vector<int>>& sets,
               const std::vector<int>& member, const Config& config, const Distsv& setdist,
               bool partial, Workspace& ws, Rng& rng);

// ---------------------------------------------------------------------------
// Printing (parse_print.jl output half)

struct Counters {
    std::int64_t latest_improvement = 1;
    bool first_improvement = false;
    int warm_trial = 0;
    int cold_trial = 1;
    std::int64_t total_iter = 0;
    double print_time = 0.0;  // seconds since solver start of last print
};

void print_params(const Config& config);
void print_warm_trial(const Counters& count, const Config& config, const Tour& best,
                      std::int64_t iter_count);
void print_best(Counters& count, const Config& config, const Tour& best, const Tour& lowest,
                double elapsed, bool budget_met, bool timeout);
void print_summary(const Tour& lowest, double timer, const std::vector<int>& member,
                   const Config& config, bool timeout, bool budget_met);

}  // namespace detail
}  // namespace glns
