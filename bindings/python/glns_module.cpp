// Copyright 2017 Stephen L. Smith and Frank Imeson
// Licensed under the Apache License, Version 2.0.  See LICENSE.
//
// nanobind Python bindings.  The user-facing API (numpy coercion, argument
// validation) lives in python/glns/__init__.py; this module is the thin
// typed core underneath it.

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <nanobind/nanobind.h>
#include <nanobind/ndarray.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>

#include "glns/glns.hpp"

namespace nb = nanobind;

namespace {

using DistArray =
    nb::ndarray<const std::int64_t, nb::ndim<2>, nb::c_contig, nb::device::cpu>;

glns::Params make_params(
    const std::string& mode, std::optional<int> trials, std::optional<int> restarts,
    std::optional<double> max_time, std::optional<std::int64_t> num_iterations,
    std::optional<std::string> init_tour, std::optional<double> reopt,
    std::optional<double> epsilon, std::optional<std::string> noise,
    std::optional<std::string> insertion_algs, std::optional<std::string> removal_algs,
    std::optional<std::int64_t> budget, std::optional<std::uint64_t> seed, int verbose,
    std::optional<std::string> output_file) {
    glns::Params params;
    params.mode = mode;
    params.trials = trials;
    params.restarts = restarts;
    params.max_time = max_time;
    params.num_iterations = num_iterations;
    params.init_tour = std::move(init_tour);
    params.reopt = reopt;
    params.epsilon = epsilon;
    params.noise = std::move(noise);
    params.insertion_algs = std::move(insertion_algs);
    params.removal_algs = std::move(removal_algs);
    params.budget = budget;
    params.seed = seed;
    params.verbose = verbose;
    if (output_file) params.output_file = *output_file;
    return params;
}

// shared keyword list for solve/solve_file
#define GLNS_PARAM_ARGS                                                               \
    nb::kw_only(), nb::arg("mode") = "default", nb::arg("trials") = nb::none(),       \
        nb::arg("restarts") = nb::none(), nb::arg("max_time") = nb::none(),           \
        nb::arg("num_iterations") = nb::none(), nb::arg("init_tour") = nb::none(),    \
        nb::arg("reopt") = nb::none(), nb::arg("epsilon") = nb::none(),               \
        nb::arg("noise") = nb::none(), nb::arg("insertion_algs") = nb::none(),        \
        nb::arg("removal_algs") = nb::none(), nb::arg("budget") = nb::none(),         \
        nb::arg("seed") = nb::none(), nb::arg("verbose") = 0,                         \
        nb::arg("output_file") = nb::none()

}  // namespace

NB_MODULE(_core, m) {
    m.doc() = "GLNS GTSP solver (C++ core)";

    nb::class_<glns::Solution>(m, "Solution")
        .def_ro("tour", &glns::Solution::tour,
                "one vertex per set, 0-indexed, in visit order")
        .def_ro("cost", &glns::Solution::cost)
        .def_ro("solve_time", &glns::Solution::solve_time)
        .def_ro("timeout", &glns::Solution::timeout)
        .def_ro("budget_met", &glns::Solution::budget_met)
        .def_ro("total_iterations", &glns::Solution::total_iterations)
        .def("__repr__", [](const glns::Solution& s) {
            return "Solution(cost=" + std::to_string(s.cost) +
                   ", tour_len=" + std::to_string(s.tour.size()) +
                   ", solve_time=" + std::to_string(s.solve_time) + ")";
        });

    m.def(
        "solve",
        [](DistArray dist, const std::vector<std::vector<int>>& sets,
           const std::string& mode, std::optional<int> trials, std::optional<int> restarts,
           std::optional<double> max_time, std::optional<std::int64_t> num_iterations,
           std::optional<std::string> init_tour, std::optional<double> reopt,
           std::optional<double> epsilon, std::optional<std::string> noise,
           std::optional<std::string> insertion_algs,
           std::optional<std::string> removal_algs, std::optional<std::int64_t> budget,
           std::optional<std::uint64_t> seed, int verbose,
           std::optional<std::string> output_file) {
            const int n = static_cast<int>(dist.shape(0));
            if (dist.shape(1) != dist.shape(0)) {
                throw std::invalid_argument("distance matrix must be square");
            }
            glns::Instance inst;
            inst.num_vertices = n;
            inst.num_sets = static_cast<int>(sets.size());
            inst.sets = sets;
            inst.dist = glns::Matrix(n, 0);
            const std::int64_t* data = dist.data();
            for (int i = 0; i < n; ++i) {
                for (int j = 0; j < n; ++j) {
                    inst.dist.set(i, j, data[static_cast<std::size_t>(i) * n + j]);
                }
            }
            inst.finalize();
            const glns::Params params = make_params(
                mode, trials, restarts, max_time, num_iterations, std::move(init_tour),
                reopt, epsilon, std::move(noise), std::move(insertion_algs),
                std::move(removal_algs), budget, seed, verbose, std::move(output_file));
            nb::gil_scoped_release release;
            return glns::solve(inst, params);
        },
        nb::arg("dist"), nb::arg("sets"), GLNS_PARAM_ARGS);

    m.def(
        "solve_file",
        [](const std::string& path, const std::string& mode, std::optional<int> trials,
           std::optional<int> restarts, std::optional<double> max_time,
           std::optional<std::int64_t> num_iterations, std::optional<std::string> init_tour,
           std::optional<double> reopt, std::optional<double> epsilon,
           std::optional<std::string> noise, std::optional<std::string> insertion_algs,
           std::optional<std::string> removal_algs, std::optional<std::int64_t> budget,
           std::optional<std::uint64_t> seed, int verbose,
           std::optional<std::string> output_file) {
            const glns::Params params = make_params(
                mode, trials, restarts, max_time, num_iterations, std::move(init_tour),
                reopt, epsilon, std::move(noise), std::move(insertion_algs),
                std::move(removal_algs), budget, seed, verbose, std::move(output_file));
            const glns::Instance inst = glns::read_instance(path);
            nb::gil_scoped_release release;
            return glns::solve(inst, params);
        },
        nb::arg("path"), GLNS_PARAM_ARGS);

    m.def("read_instance_summary", [](const std::string& path) {
        const glns::Instance inst = glns::read_instance(path);
        return nb::make_tuple(inst.num_vertices, inst.num_sets);
    });
}
