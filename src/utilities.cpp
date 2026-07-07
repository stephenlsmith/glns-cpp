// Copyright 2017 Stephen L. Smith and Frank Imeson
// Licensed under the Apache License, Version 2.0.  See LICENSE.
//
// Port of utilities.jl (Tour helpers, set/vertex distances, membership).

#include <algorithm>
#include <stdexcept>

#include "internal.hpp"

namespace glns {

std::int32_t Matrix::checked(Cost value) {
    if (value > std::numeric_limits<std::int32_t>::max() ||
        value < std::numeric_limits<std::int32_t>::min()) {
        throw std::runtime_error(
            "distance " + std::to_string(value) +
            " does not fit in 32 bits; scale the costs down (tour costs may "
            "exceed 32 bits, individual distances may not)");
    }
    return static_cast<std::int32_t>(value);
}

void Instance::finalize() {
    if (static_cast<int>(sets.size()) != num_sets) {
        throw std::runtime_error("number of sets doesn't match num_sets");
    }
    if (sets.size() <= 1) {
        throw std::runtime_error("must have more than 1 set");
    }
    if (dist.size() != num_vertices) {
        throw std::runtime_error("distance matrix size doesn't match num_vertices");
    }
    membership.assign(num_vertices, -1);
    for (std::size_t s = 0; s < sets.size(); ++s) {
        for (int v : sets[s]) {
            if (v < 0 || v >= num_vertices) {
                throw std::runtime_error("vertex " + std::to_string(v) + " out of range");
            }
            if (membership[v] != -1) {
                throw std::runtime_error("vertex " + std::to_string(v) +
                                         " belongs to more than one set");
            }
            membership[v] = static_cast<int>(s);
        }
    }
    for (int v = 0; v < num_vertices; ++v) {
        if (membership[v] == -1) {
            throw std::runtime_error("vertex " + std::to_string(v) + " belongs to no set");
        }
    }
}

namespace detail {

void pivot_tour(std::vector<int>& tour, Rng& rng) {
    const std::size_t pivot = rng.bounded(tour.size());
    if (pivot != 0) {
        std::rotate(tour.begin(), tour.begin() + pivot, tour.end());
    }
}

bool tour_feasibility(const std::vector<int>& tour, const std::vector<int>& membership,
                      int num_sets) {
    if (static_cast<int>(tour.size()) != num_sets) return false;
    std::vector<bool> set_test(num_sets, false);
    for (int v : tour) {
        const int set_v = membership[v];
        if (set_test[set_v]) return false;  // a set is visited twice
        set_test[set_v] = true;
    }
    for (bool visited : set_test) {
        if (!visited) return false;
    }
    return true;
}

int min_set(const std::vector<std::vector<int>>& sets) {
    std::size_t min_size = sets[0].size();
    int min_index = 0;
    for (std::size_t i = 1; i < sets.size(); ++i) {
        if (sets[i].size() < min_size) {
            min_size = sets[i].size();
            min_index = static_cast<int>(i);
        }
    }
    return min_index;
}

Distsv set_vertex_dist(const Matrix& dist, int num_sets, const std::vector<int>& member) {
    const int numv = dist.size();
    const std::int32_t sentinel = std::numeric_limits<std::int32_t>::max();
    Distsv d;
    d.num_sets = num_sets;
    d.num_vertices = numv;
    d.set_vert.assign(static_cast<std::size_t>(num_sets) * numv, sentinel);
    d.vert_set.assign(static_cast<std::size_t>(numv) * num_sets, sentinel);
    d.min_sv.assign(static_cast<std::size_t>(num_sets) * numv, sentinel);

    auto set_vert = [&](int s, int v) -> std::int32_t& {
        return d.set_vert[static_cast<std::size_t>(s) * numv + v];
    };
    auto vert_set = [&](int v, int s) -> std::int32_t& {
        return d.vert_set[static_cast<std::size_t>(v) * num_sets + s];
    };
    auto min_sv = [&](int s, int v) -> std::int32_t& {
        return d.min_sv[static_cast<std::size_t>(s) * numv + v];
    };

    for (int i = 0; i < numv; ++i) {
        for (int j = 0; j < numv; ++j) {
            const std::int32_t c = static_cast<std::int32_t>(dist(j, i));
            int set = member[j];
            if (c < set_vert(set, i)) set_vert(set, i) = c;   // set containing j -> i
            if (c < min_sv(set, i)) min_sv(set, i) = c;
            set = member[i];
            if (c < vert_set(j, set)) vert_set(j, set) = c;   // j -> set containing i
            if (c < min_sv(set, j)) min_sv(set, j) = c;
        }
    }
    return d;
}

}  // namespace detail
}  // namespace glns
