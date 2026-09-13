// Copyright 2017 Stephen L. Smith and Frank Imeson
// Licensed under the Apache License, Version 2.0.  See LICENSE.
//
// C API wrapper around the C++ solver (see include/glns/glns.h).

#include <climits>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <stdexcept>
#include <string>

#include "glns/glns.h"
#include "glns/glns.hpp"

namespace {

void write_error(char* errbuf, int errbuf_len, const char* message) {
    if (errbuf == nullptr || errbuf_len <= 0) return;
    std::strncpy(errbuf, message, static_cast<std::size_t>(errbuf_len) - 1);
    errbuf[errbuf_len - 1] = '\0';
}

glns::Params to_cpp_params(const glns_params* p) {
    glns::Params params;
    if (p == nullptr) return params;
    if (!std::isfinite(p->max_time)) {
        throw std::runtime_error("max_time must be finite");
    }
    if (!std::isfinite(p->reopt)) {
        throw std::runtime_error("reopt must be finite");
    }
    if (!std::isfinite(p->epsilon)) {
        throw std::runtime_error("epsilon must be finite");
    }
    if (p->mode != nullptr) params.mode = p->mode;
    if (p->trials >= 0) params.trials = p->trials;
    if (p->restarts >= 0) params.restarts = p->restarts;
    if (p->max_time >= 0) params.max_time = p->max_time;
    if (p->num_iterations >= 0) params.num_iterations = p->num_iterations;
    if (p->reopt >= 0) params.reopt = p->reopt;
    if (p->epsilon >= 0) params.epsilon = p->epsilon;
    if (p->init_tour != nullptr) params.init_tour = p->init_tour;
    if (p->noise != nullptr) params.noise = p->noise;
    if (p->insertion_algs != nullptr) params.insertion_algs = p->insertion_algs;
    if (p->removal_algs != nullptr) params.removal_algs = p->removal_algs;
    if (p->budget != INT64_MIN) params.budget = p->budget;
    if (p->has_seed != 0) params.seed = p->seed;
    params.verbose = p->verbose;
    if (p->output_file != nullptr) params.output_file = p->output_file;
    return params;
}

int fill_solution(const glns::Solution& sol, glns_solution* out, char* errbuf,
                  int errbuf_len) {
    int* tour = static_cast<int*>(std::malloc(sizeof(int) * sol.tour.size()));
    if (tour == nullptr) {
        write_error(errbuf, errbuf_len, "out of memory");
        return 2;
    }
    std::memcpy(tour, sol.tour.data(), sizeof(int) * sol.tour.size());
    out->tour = tour;
    out->tour_len = static_cast<int>(sol.tour.size());
    out->cost = sol.cost;
    out->solve_time = sol.solve_time;
    out->timeout = sol.timeout ? 1 : 0;
    out->budget_met = sol.budget_met ? 1 : 0;
    out->total_iterations = sol.total_iterations;
    return 0;
}

}  // namespace

extern "C" {

const char* glns_version(void) { return GLNS_VERSION_STRING; }

void glns_params_init(glns_params* params) {
    if (params == nullptr) return;
    params->mode = nullptr;
    params->trials = -1;
    params->restarts = -1;
    params->max_time = -1.0;
    params->num_iterations = -1;
    params->reopt = -1.0;
    params->epsilon = -1.0;
    params->init_tour = nullptr;
    params->noise = nullptr;
    params->insertion_algs = nullptr;
    params->removal_algs = nullptr;
    params->budget = INT64_MIN;
    params->seed = 0;
    params->has_seed = 0;
    params->verbose = 0;
    params->output_file = nullptr;
}

int glns_solve(int num_vertices, int num_sets, const int64_t* dist,
               const int* set_sizes, const int* set_vertices,
               const glns_params* params, glns_solution* solution,
               char* errbuf, int errbuf_len) {
    if (solution != nullptr) *solution = glns_solution{};
    write_error(errbuf, errbuf_len, "");
    if (dist == nullptr || set_sizes == nullptr || set_vertices == nullptr ||
        solution == nullptr) {
        write_error(errbuf, errbuf_len, "null argument");
        return 1;
    }
    try {
        if (num_vertices <= 0) {
            throw std::runtime_error("num_vertices must be positive");
        }
        if (num_sets <= 1) {
            throw std::runtime_error("num_sets must be greater than 1");
        }
        glns::Instance inst;
        inst.num_vertices = num_vertices;
        inst.num_sets = num_sets;
        inst.dist = glns::Matrix(num_vertices, 0);
        for (int i = 0; i < num_vertices; ++i) {
            for (int j = 0; j < num_vertices; ++j) {
                inst.dist.set(i, j, dist[static_cast<std::size_t>(i) * num_vertices + j]);
            }
        }
        inst.sets.resize(num_sets);
        const int* v = set_vertices;
        int total_vertices = 0;
        for (int s = 0; s < num_sets; ++s) {
            if (set_sizes[s] <= 0) {
                throw std::runtime_error("set size must be positive");
            }
            if (set_sizes[s] > num_vertices - total_vertices) {
                throw std::runtime_error("set sizes exceed num_vertices");
            }
            inst.sets[s].assign(v, v + set_sizes[s]);
            v += set_sizes[s];
            total_vertices += set_sizes[s];
        }
        if (total_vertices != num_vertices) {
            throw std::runtime_error("set sizes do not sum to num_vertices");
        }
        inst.finalize();
        return fill_solution(glns::solve(inst, to_cpp_params(params)), solution, errbuf,
                             errbuf_len);
    } catch (const std::exception& e) {
        write_error(errbuf, errbuf_len, e.what());
        return 1;
    }
}

int glns_solve_file(const char* path, const glns_params* params, glns_solution* solution,
                    char* errbuf, int errbuf_len) {
    if (solution != nullptr) *solution = glns_solution{};
    write_error(errbuf, errbuf_len, "");
    if (path == nullptr || solution == nullptr) {
        write_error(errbuf, errbuf_len, "null argument");
        return 1;
    }
    try {
        const glns::Instance inst = glns::read_instance(path);
        return fill_solution(glns::solve(inst, to_cpp_params(params)), solution, errbuf,
                             errbuf_len);
    } catch (const std::exception& e) {
        write_error(errbuf, errbuf_len, e.what());
        return 1;
    }
}

void glns_solution_free(glns_solution* solution) {
    if (solution != nullptr) {
        std::free(solution->tour);
        *solution = glns_solution{};
    }
}

}  // extern "C"
