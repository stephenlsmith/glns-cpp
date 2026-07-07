// Copyright 2017 Stephen L. Smith and Frank Imeson
// Licensed under the Apache License, Version 2.0.  See LICENSE.
//
// Port of tour_optimizations.jl: move-opt local search and the
// fixed-set-order tour re-optimization (shortest path through the sets).

#include <algorithm>

#include "internal.hpp"

namespace glns {
namespace detail {

namespace {

// cost of removing the vertex at position i from the cyclic tour
Cost removal_cost(const std::vector<int>& tour, const Matrix& dist, std::size_t i) {
    const std::size_t n = tour.size();
    if (i == 0) {
        return dist(tour[n - 1], tour[i]) + dist(tour[i], tour[i + 1]) -
               dist(tour[n - 1], tour[i + 1]);
    }
    if (i == n - 1) {
        return dist(tour[i - 1], tour[i]) + dist(tour[i], tour[0]) -
               dist(tour[i - 1], tour[0]);
    }
    return dist(tour[i - 1], tour[i]) + dist(tour[i], tour[i + 1]) -
           dist(tour[i - 1], tour[i + 1]);
}

// sequentially move each vertex to its best position until no improvement
void moveopt(std::vector<int>& tour, const Matrix& dist,
             const std::vector<std::vector<int>>& sets, const std::vector<int>& member,
             const Distsv& setdist) {
    bool improvement_found = true;
    int number_of_moves = 0;
    std::size_t start_position = 0;

    while (improvement_found && number_of_moves < 10) {
        improvement_found = false;
        for (std::size_t i = start_position; i < tour.size(); ++i) {
            const int select_vertex = tour[i];
            const Cost delete_cost = removal_cost(tour, dist, i);
            const int set_ind = member[select_vertex];
            tour.erase(tour.begin() + i);

            int v = select_vertex;
            int pos = static_cast<int>(i);
            const Cost cost =
                insert_cost_lb(tour, dist, sets[set_ind], set_ind, setdist, v, pos, delete_cost);
            tour.insert(tour.begin() + pos, v);

            if (cost < delete_cost) {
                improvement_found = true;
                ++number_of_moves;
                // resume where the tour change began
                start_position = std::min<std::size_t>(pos, i);
                break;
            }
        }
    }
}

// randomized variant: try to move `iters` randomly chosen vertices
void moveopt_rand(std::vector<int>& tour, const Matrix& dist,
                  const std::vector<std::vector<int>>& sets, const std::vector<int>& member,
                  int iters, const Distsv& setdist, std::vector<int>& tour_inds, Rng& rng) {
    tour_inds.resize(tour.size());
    for (std::size_t i = 0; i < tour.size(); ++i) tour_inds[i] = static_cast<int>(i);

    for (int idx = 0; idx < iters; ++idx) {
        const std::size_t i =
            static_cast<std::size_t>(incremental_shuffle(tour_inds, idx, rng));
        const int select_vertex = tour[i];

        const Cost delete_cost = removal_cost(tour, dist, i);
        const int set_ind = member[select_vertex];
        tour.erase(tour.begin() + i);

        int v = select_vertex;
        int pos = static_cast<int>(i);
        insert_cost_lb(tour, dist, sets[set_ind], set_ind, setdist, v, pos, delete_cost);
        tour.insert(tour.begin() + pos, v);
    }
}

// position in the tour of the smallest set
std::size_t min_setv(const std::vector<int>& tour, const std::vector<int>& member,
                     const Config& config) {
    for (std::size_t i = 0; i < tour.size(); ++i) {
        if (member[tour[i]] == config.min_set_index) return i;
    }
    return 0;
}

// Relax the cost of each vertex in set2 from the vertices in set1, in place.
// The loop nest iterates set1 on the outside so every dist() access walks a
// row of the matrix; the sequence of comparisons per v2 (and thus the result,
// including tie-breaking) is identical to the set2-outer form.
void relax_in(std::vector<Cost>& cost, const Matrix& dist, std::vector<int>& prev,
              const int* set1, std::size_t n1, const std::vector<int>& set2) {
    const int v0 = set1[0];
    const Cost c0 = cost[v0];
    for (int v2 : set2) {
        cost[v2] = c0 + dist(v0, v2);
        prev[v2] = v0;
    }
    for (std::size_t i = 1; i < n1; ++i) {
        const int v1 = set1[i];
        const Cost c1 = cost[v1];  // v1 is never in set2: sets are disjoint
        for (int v2 : set2) {
            const Cost newcost = c1 + dist(v1, v2);
            if (cost[v2] > newcost) {
                cost[v2] = newcost;
                prev[v2] = v1;
            }
        }
    }
}

// cheapest way to close the cycle from set1 back to vertex v2
std::pair<Cost, int> relax(const std::vector<Cost>& cost, const Matrix& dist,
                           const std::vector<int>& set1, int v2) {
    int min_prev = set1[0];
    Cost min_cost = cost[min_prev] + dist(min_prev, v2);
    for (std::size_t i = 1; i < set1.size(); ++i) {
        const int v1 = set1[i];
        const Cost newcost = cost[v1] + dist(v1, v2);
        if (min_cost > newcost) {
            min_cost = newcost;
            min_prev = v1;
        }
    }
    return {min_cost, min_prev};
}

// walk the prev pointers back from start_prev to recover the tour, into `out`
void extract_tour(const std::vector<int>& prev, int start_vertex, int start_prev,
                  std::vector<int>& out) {
    out.clear();
    out.push_back(start_vertex);
    int vertex_step = start_prev;
    while (prev[vertex_step] != -1) {
        out.push_back(vertex_step);
        vertex_step = prev[vertex_step];
    }
    std::reverse(out.begin(), out.end());
}

// Given the set ordering of `tour`, find the optimal vertex in each set by
// shortest-path relaxations, trying every start vertex in the smallest set.
// Rewrites `tour` in place with the best tour found.
void reopt_tour(std::vector<int>& tour, const Matrix& dist,
                const std::vector<std::vector<int>>& sets, const std::vector<int>& member,
                const Config& config, Workspace& ws) {
    Cost best_tour_cost = tour_cost(tour, dist);
    ws.new_tour.assign(tour.begin(), tour.end());
    const std::size_t min_index = min_setv(tour, member, config);
    ws.rotated.clear();
    ws.rotated.insert(ws.rotated.end(), tour.begin() + min_index, tour.end());
    ws.rotated.insert(ws.rotated.end(), tour.begin(), tour.begin() + min_index);
    const std::vector<int>& rotated = ws.rotated;

    ws.prev.assign(config.num_vertices, -1);
    ws.cost_to_come.assign(config.num_vertices, 0);

    for (const int start_vertex : sets[member[rotated[0]]]) {
        relax_in(ws.cost_to_come, dist, ws.prev, &start_vertex, 1, sets[member[rotated[1]]]);
        for (std::size_t i = 2; i < rotated.size(); ++i) {
            relax_in(ws.cost_to_come, dist, ws.prev, sets[member[rotated[i - 1]]].data(),
                     sets[member[rotated[i - 1]]].size(), sets[member[rotated[i]]]);
        }
        // cost to close the cycle back to the start vertex
        const auto [path_cost, start_prev] =
            relax(ws.cost_to_come, dist, sets[member[rotated.back()]], start_vertex);
        if (path_cost < best_tour_cost) {
            best_tour_cost = path_cost;
            extract_tour(ws.prev, start_vertex, start_prev, ws.new_tour);
        }
    }
    tour.assign(ws.new_tour.begin(), ws.new_tour.end());
}

}  // namespace

Cost insert_cost_lb(const std::vector<int>& tour, const Matrix& dist,
                    const std::vector<int>& set, int setind, const Distsv& setdist,
                    int& bestv, int& bestpos, Cost best_cost) {
    for (std::size_t i = 0; i < tour.size(); ++i) {
        const int v1 = prev_tour(tour, i);
        const Cost lb =
            setdist.vs(v1, setind) + setdist.sv(setind, tour[i]) - dist(v1, tour[i]);
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
    return best_cost;
}

// alternate reopt and moveopt until there is no improvement (at most 5 rounds)
void opt_cycle(Tour& current, const Matrix& dist, std::vector<std::vector<int>>& sets,
               const std::vector<int>& member, const Config& config, const Distsv& setdist,
               bool partial, Workspace& ws, Rng& rng) {
    current.cost = tour_cost(current.tour, dist);
    Cost prev_cost = current.cost;
    for (int i = 1; i <= 5; ++i) {
        if (i % 2 == 1) {
            reopt_tour(current.tour, dist, sets, member, config, ws);
        } else if (config.mode == "fast" || partial) {
            moveopt_rand(current.tour, dist, sets, member, config.max_removals, setdist,
                         ws.tour_inds, rng);
        } else {
            moveopt(current.tour, dist, sets, member, setdist);
        }
        current.cost = tour_cost(current.tour, dist);
        if (i > 1 && (current.cost >= prev_cost || partial)) {
            return;
        }
        prev_cost = current.cost;
    }
}

}  // namespace detail
}  // namespace glns
