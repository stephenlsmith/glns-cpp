// Copyright 2017 Stephen L. Smith and Frank Imeson
// Licensed under the Apache License, Version 2.0.  See LICENSE.
//
// Port of insertion_deletion.jl: removal and insertion methods and the
// remove-insert step at the heart of the adaptive large neighborhood search.

#include <algorithm>
#include <cmath>

#include "internal.hpp"

namespace glns {
namespace detail {

namespace {

// forward declarations of the file-local helpers, in Julia source order
int select_k(int num, double power, Rng& rng);
void randpdf_insertion(std::vector<int>& tour, std::vector<int>& sets_to_insert,
                       const Matrix& dist, const Distsv& setdist,
                       std::vector<std::vector<int>>& sets, double power, const Power& noise,
                       Workspace& ws, Rng& rng);
void cheapest_insertion(std::vector<int>& tour, std::vector<int>& sets_to_insert,
                        const Matrix& dist, const Distsv& setdist,
                        std::vector<std::vector<int>>& sets);
void insert_lb(const std::vector<int>& tour, const Matrix& dist, const std::vector<int>& set,
               int setind, const Distsv& setdist, double noise, Rng& rng, int& bestv,
               int& bestpos);
void insert_subset_lb(const std::vector<int>& tour, const Matrix& dist,
                      const std::vector<int>& set, int setind, const Distsv& setdist,
                      double noise, std::vector<int>& tour_inds, Rng& rng, int& bestv,
                      int& bestpos);
void random_insertion(std::vector<int>& tour, std::vector<int>& sets_to_insert,
                      const Matrix& dist, std::vector<std::vector<int>>& sets,
                      const Distsv& setdist, Rng& rng);
void random_initial_tour(std::vector<int>& tour, std::vector<int>& sets_to_insert,
                         std::vector<std::vector<int>>& sets, Rng& rng);
void worst_removal(std::vector<int>& tour, const Matrix& dist, int num_to_remove,
                   const std::vector<int>& member, double power, Workspace& ws, Rng& rng);
void segment_removal(std::vector<int>& tour, int num_to_remove,
                     const std::vector<int>& member, std::vector<int>& deleted_sets,
                     Rng& rng);
void distance_removal(std::vector<int>& tour, const Matrix& dist, int num_to_remove,
                      const std::vector<int>& member, double power, Workspace& ws, Rng& rng);
void worst_vertices(const std::vector<int>& tour, const Matrix& dist,
                    std::vector<Cost>& removal_cost);

}  // namespace

// Select a removal and an insertion method using the adaptive powers, then
// perform removal followed by insertion on `trial` (a reusable copy of
// `current`; its capacity persists across iterations).
void remove_insert(const Tour& current, Tour& trial, const Matrix& dist,
                   const std::vector<int>& member, const Distsv& setdist,
                   std::vector<std::vector<int>>& sets, Powers& powers, const Config& config,
                   Phase phase, Workspace& ws, Rng& rng) {
    trial.tour = current.tour;
    trial.cost = current.cost;
    pivot_tour(trial.tour, rng);
    const int num_removals = rng.rand_int(config.min_removals, config.max_removals);

    Power& removal = power_select(powers.removals, powers.removal_total, phase, rng);
    if (removal.name == "distance") {
        distance_removal(trial.tour, dist, num_removals, member, removal.value, ws, rng);
    } else if (removal.name == "worst") {
        worst_removal(trial.tour, dist, num_removals, member, removal.value, ws, rng);
    } else {
        segment_removal(trial.tour, num_removals, member, ws.sets_to_insert, rng);
    }

    for (int s : ws.sets_to_insert) {
        rng.shuffle(sets[s]);
    }

    Power& insertion = power_select(powers.insertions, powers.insertion_total, phase, rng);
    Power& noise = power_select(powers.noise, powers.noise_total, phase, rng);
    if (insertion.name == "cheapest") {
        cheapest_insertion(trial.tour, ws.sets_to_insert, dist, setdist, sets);
    } else {
        randpdf_insertion(trial.tour, ws.sets_to_insert, dist, setdist, sets, insertion.value,
                          noise, ws, rng);
    }

    if (rng.next_double() < config.prob_reopt) {
        opt_cycle(trial, dist, sets, member, config, setdist, /*partial=*/true, ws, rng);
    } else {
        trial.cost = tour_cost(trial.tour, dist);
    }

    // update the scores of the methods used
    const double score =
        current.cost > 0
            ? 100.0 * static_cast<double>(std::max<Cost>(current.cost - trial.cost, 0)) /
                  static_cast<double>(current.cost)
            : 0.0;
    insertion.scores[phase] += score;
    insertion.count[phase] += 1;
    removal.scores[phase] += score;
    removal.count[phase] += 1;
    noise.scores[phase] += score;
    noise.count[phase] += 1;
}

// pdf-biased selection: pick k via a geometric-like distribution shaped by
// power, then return a uniformly random index holding the kth smallest weight
int pdf_select(const std::vector<Cost>& weights, double power, Rng& rng,
               std::vector<Cost>& scratch) {
    if (power == 0.0) return static_cast<int>(rng.bounded(weights.size()));
    if (power > 9.0) {
        return rand_select(weights, *std::max_element(weights.begin(), weights.end()), rng);
    }
    if (power < -9.0) {
        return rand_select(weights, *std::min_element(weights.begin(), weights.end()), rng);
    }

    const int k = select_k(static_cast<int>(weights.size()), power, rng);
    if (k == 1) {
        return rand_select(weights, *std::min_element(weights.begin(), weights.end()), rng);
    }
    if (k == static_cast<int>(weights.size())) {
        return rand_select(weights, *std::max_element(weights.begin(), weights.end()), rng);
    }
    scratch.assign(weights.begin(), weights.end());
    std::nth_element(scratch.begin(), scratch.begin() + (k - 1), scratch.end());
    return rand_select(weights, scratch[k - 1], rng);
}

// Build a tour from scratch on a cold restart; updates lowest if improved.
Tour initial_tour(Tour& lowest, const Matrix& dist, std::vector<std::vector<int>>& sets,
                  const Distsv& setdist, int trial_num, const Config& config, Rng& rng) {
    std::vector<int> sets_to_insert(config.num_sets);
    for (int i = 0; i < config.num_sets; ++i) sets_to_insert[i] = i;
    Tour best{{}, kMaxCost};

    // randomly choose between a random tour and an insertion tour past trial 1
    if (config.init_tour == "rand" && trial_num > 1 && rng.next_double() < 0.5) {
        random_initial_tour(best.tour, sets_to_insert, sets, rng);
    } else {
        random_insertion(best.tour, sets_to_insert, dist, sets, setdist, rng);
    }
    best.cost = tour_cost(best.tour, dist);
    if (lowest.cost > best.cost) {
        lowest = best;
    }
    return best;
}

namespace {

// Select an integer in [1, num] according to an exponential-style
// distribution with lambda = power: from the top of the range if power is
// positive, from the bottom if negative.
int select_k(int num, double power, Rng& rng) {
    const double base = std::pow(0.5, std::abs(power));
    // (1 - base^num) / (1 - base) is the sum of the geometric series
    double selection = (1.0 - std::pow(base, num)) / (1.0 - base) * rng.next_double();
    double bin = 1.0;
    for (int k = 1; k <= num; ++k) {
        if (selection < bin) return power >= 0 ? num - k + 1 : k;
        selection -= bin;
        bin *= base;
    }
    return power >= 0 ? num : 1;
}

// choose each set with pdf_select, then insert its best vertex (with noise)
void randpdf_insertion(std::vector<int>& tour, std::vector<int>& sets_to_insert,
                       const Matrix& dist, const Distsv& setdist,
                       std::vector<std::vector<int>>& sets, double power, const Power& noise,
                       Workspace& ws, Rng& rng) {
    std::vector<Cost>& mindist = ws.mindist;
    mindist.assign(sets_to_insert.size(), kMaxCost);
    for (std::size_t i = 0; i < sets_to_insert.size(); ++i) {
        const int set = sets_to_insert[i];
        for (int vertex : tour) {
            if (setdist.minsv(set, vertex) < mindist[i]) {
                mindist[i] = setdist.minsv(set, vertex);
            }
        }
    }

    int new_vertex_in_tour = -1;
    while (!sets_to_insert.empty()) {
        if (new_vertex_in_tour != -1) {
            for (std::size_t i = 0; i < sets_to_insert.size(); ++i) {
                const int set = sets_to_insert[i];
                if (setdist.minsv(set, new_vertex_in_tour) < mindist[i]) {
                    mindist[i] = setdist.minsv(set, new_vertex_in_tour);
                }
            }
        }
        const int set_index = pdf_select(mindist, power, rng, ws.pdf_scratch);
        const int nearest_set = sets_to_insert[set_index];
        int bestv = -1, bestpos = -1;
        if (noise.name == "subset") {
            insert_subset_lb(tour, dist, sets[nearest_set], nearest_set, setdist, noise.value,
                             ws.tour_inds, rng, bestv, bestpos);
        } else {
            insert_lb(tour, dist, sets[nearest_set], nearest_set, setdist, noise.value, rng,
                      bestv, bestpos);
        }
        tour.insert(tour.begin() + bestpos, bestv);
        new_vertex_in_tour = bestv;
        sets_to_insert.erase(sets_to_insert.begin() + set_index);
        mindist.erase(mindist.begin() + set_index);
    }
}

// repeatedly insert the vertex that can be inserted most cheaply
void cheapest_insertion(std::vector<int>& tour, std::vector<int>& sets_to_insert,
                        const Matrix& dist, const Distsv& setdist,
                        std::vector<std::vector<int>>& sets) {
    while (!sets_to_insert.empty()) {
        Cost best_cost = kMaxCost;
        int best_v = -1;
        int best_pos = -1;
        int best_set = -1;
        for (std::size_t i = 0; i < sets_to_insert.size(); ++i) {
            const int set_ind = sets_to_insert[i];
            const Cost cost = insert_cost_lb(tour, dist, sets[set_ind], set_ind, setdist,
                                             best_v, best_pos, best_cost);
            if (cost < best_cost) {
                best_set = static_cast<int>(i);
                best_cost = cost;
            }
        }
        tour.insert(tour.begin() + best_pos, best_v);
        sets_to_insert.erase(sets_to_insert.begin() + best_set);
    }
}

// find the vertex in `set` with minimum insertion cost and its position,
// pruning with the set-vertex distance lower bound
void insert_lb(const std::vector<int>& tour, const Matrix& dist, const std::vector<int>& set,
               int setind, const Distsv& setdist, double noise, Rng& rng, int& bestv,
               int& bestpos) {
    Cost best_cost = kMaxCost;
    for (std::size_t i = 0; i < tour.size(); ++i) {
        const int v1 = prev_tour(tour, i);
        const Cost lb = setdist.vs(v1, setind) + setdist.sv(setind, tour[i]) - dist(v1, tour[i]);
        if (lb > best_cost) continue;

        for (int v : set) {
            Cost insert_cost = dist(v1, v) + dist(v, tour[i]) - dist(v1, tour[i]);
            if (noise > 0.0) {
                insert_cost += std::llround(noise * rng.next_double() *
                                            std::abs(static_cast<double>(insert_cost)));
            }
            if (insert_cost < best_cost) {
                best_cost = insert_cost;
                bestv = v;
                bestpos = static_cast<int>(i);
            }
        }
    }
}

// like insert_lb but only examines a random fraction `noise` of tour positions
void insert_subset_lb(const std::vector<int>& tour, const Matrix& dist,
                      const std::vector<int>& set, int setind, const Distsv& setdist,
                      double noise, std::vector<int>& tour_inds, Rng& rng, int& bestv,
                      int& bestpos) {
    Cost best_cost = kMaxCost;
    tour_inds.resize(tour.size());
    for (std::size_t i = 0; i < tour.size(); ++i) tour_inds[i] = static_cast<int>(i);

    const std::size_t limit =
        static_cast<std::size_t>(std::ceil(static_cast<double>(tour.size()) * noise));
    for (std::size_t idx = 0; idx < limit; ++idx) {
        const std::size_t i = static_cast<std::size_t>(incremental_shuffle(tour_inds, idx, rng));
        const int v1 = prev_tour(tour, i);
        const Cost lb = setdist.vs(v1, setind) + setdist.sv(setind, tour[i]) - dist(v1, tour[i]);
        if (lb > best_cost) continue;

        for (int v : set) {
            const Cost insert_cost = dist(v1, v) + dist(v, tour[i]) - dist(v1, tour[i]);
            if (insert_cost < best_cost) {
                best_cost = insert_cost;
                bestv = v;
                bestpos = static_cast<int>(i);
            }
        }
    }
}

// shuffle the sets, then insert the best vertex of each set in shuffled order
void random_insertion(std::vector<int>& tour, std::vector<int>& sets_to_insert,
                      const Matrix& dist, std::vector<std::vector<int>>& sets,
                      const Distsv& setdist, Rng& rng) {
    rng.shuffle(sets_to_insert);
    for (int set : sets_to_insert) {
        int best_vertex, best_position;
        if (tour.empty()) {
            best_vertex = rng.pick(sets[set]);
            best_position = 0;
        } else {
            best_vertex = -1;
            best_position = -1;
            insert_lb(tour, dist, sets[set], set, setdist, 0.75, rng, best_vertex,
                      best_position);
        }
        tour.insert(tour.begin() + best_position, best_vertex);
    }
}

// shuffle the sets, then pick a random vertex from each set
void random_initial_tour(std::vector<int>& tour, std::vector<int>& sets_to_insert,
                         std::vector<std::vector<int>>& sets, Rng& rng) {
    rng.shuffle(sets_to_insert);
    for (int set : sets_to_insert) {
        tour.push_back(rng.pick(sets[set]));
    }
}

// remove vertices biased towards those that add the most length to the tour
void worst_removal(std::vector<int>& tour, const Matrix& dist, int num_to_remove,
                   const std::vector<int>& member, double power, Workspace& ws, Rng& rng) {
    std::vector<int>& deleted_sets = ws.sets_to_insert;
    deleted_sets.clear();
    while (static_cast<int>(deleted_sets.size()) < num_to_remove) {
        worst_vertices(tour, dist, ws.removal_costs);
        const int ind = pdf_select(ws.removal_costs, power, rng, ws.pdf_scratch);
        deleted_sets.push_back(member[tour[ind]]);
        tour.erase(tour.begin() + ind);
    }
}

// remove a single continuous segment of the tour
void segment_removal(std::vector<int>& tour, int num_to_remove,
                     const std::vector<int>& member, std::vector<int>& deleted_sets,
                     Rng& rng) {
    std::size_t i = rng.bounded(tour.size());
    deleted_sets.clear();
    while (static_cast<int>(deleted_sets.size()) < num_to_remove) {
        if (i >= tour.size()) i = 0;
        deleted_sets.push_back(member[tour[i]]);
        tour.erase(tour.begin() + i);
    }
}

// pick a random vertex and remove vertices biased towards its neighbors
void distance_removal(std::vector<int>& tour, const Matrix& dist, int num_to_remove,
                      const std::vector<int>& member, double power, Workspace& ws, Rng& rng) {
    std::vector<int>& deleted_sets = ws.sets_to_insert;
    std::vector<int>& deleted_vertices = ws.deleted_vertices;
    deleted_sets.clear();
    deleted_vertices.clear();

    const std::size_t seed_index = rng.bounded(tour.size());
    deleted_sets.push_back(member[tour[seed_index]]);
    deleted_vertices.push_back(tour[seed_index]);
    tour.erase(tour.begin() + seed_index);

    std::vector<Cost>& mindist = ws.mindist;
    while (static_cast<int>(deleted_sets.size()) < num_to_remove) {
        const int seed_vertex = rng.pick(deleted_vertices);
        mindist.resize(tour.size());
        for (std::size_t i = 0; i < tour.size(); ++i) {
            mindist[i] = std::min(dist(seed_vertex, tour[i]), dist(tour[i], seed_vertex));
        }
        const int del_index = pdf_select(mindist, power, rng, ws.pdf_scratch);
        deleted_sets.push_back(member[tour[del_index]]);
        deleted_vertices.push_back(tour[del_index]);
        tour.erase(tour.begin() + del_index);
    }
}

// cost of removing each vertex from the tour, given that all others remain
void worst_vertices(const std::vector<int>& tour, const Matrix& dist,
                    std::vector<Cost>& removal_cost) {
    const std::size_t n = tour.size();
    removal_cost.resize(n);
    for (std::size_t i = 0; i < n; ++i) {
        if (i == 0) {
            removal_cost[i] =
                dist(tour[n - 1], tour[i]) + dist(tour[i], tour[i + 1]) -
                dist(tour[n - 1], tour[i + 1]);
        } else if (i == n - 1) {
            removal_cost[i] = dist(tour[i - 1], tour[i]) + dist(tour[i], tour[0]) -
                              dist(tour[i - 1], tour[0]);
        } else {
            removal_cost[i] = dist(tour[i - 1], tour[i]) + dist(tour[i], tour[i + 1]) -
                              dist(tour[i - 1], tour[i + 1]);
        }
    }
}

}  // namespace

}  // namespace detail
}  // namespace glns
