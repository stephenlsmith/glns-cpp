// Copyright 2017 Stephen L. Smith and Frank Imeson
// Licensed under the Apache License, Version 2.0.  See LICENSE.
//
// Port of parameter_defaults.jl: the default, fast, and slow settings.

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <random>
#include <stdexcept>
#include <string_view>

#include "internal.hpp"

namespace glns {
namespace detail {

namespace {

// Julia's round(Int64, x): round half to even
std::int64_t round_int(double x) { return std::llrint(x); }

std::string basename_of(const std::string& path) {
    const std::size_t pos = path.find_last_of('/');
    return pos == std::string::npos ? path : path.substr(pos + 1);
}

void validate_choice(std::string_view name, const std::string& value,
                     std::initializer_list<std::string_view> choices) {
    for (std::string_view choice : choices) {
        if (value == choice) return;
    }
    throw std::runtime_error(std::string(name) + " not recognized: " + value);
}

void validate_probability(std::string_view name, const std::optional<double>& value) {
    if (value && (!std::isfinite(*value) || *value < 0.0 || *value > 1.0)) {
        throw std::runtime_error(std::string(name) + " must be finite and in [0, 1]");
    }
}

std::int64_t scaled_iterations(std::int64_t per_set, int num_sets) {
    if (per_set <= 0) throw std::runtime_error("num_iterations must be positive");
    if (per_set > std::numeric_limits<std::int64_t>::max() / num_sets) {
        throw std::runtime_error("num_iterations is too large");
    }
    return per_set * num_sets;
}

void validate_parameters(const Params& params) {
    validate_choice("mode", params.mode, {"default", "fast", "slow"});
    if (params.trials && *params.trials <= 0) {
        throw std::runtime_error("trials must be positive");
    }
    if (params.restarts && *params.restarts < 0) {
        throw std::runtime_error("restarts must be nonnegative");
    }
    if (params.max_time && (!std::isfinite(*params.max_time) || *params.max_time < 0.0)) {
        throw std::runtime_error("max_time must be finite and nonnegative");
    }
    if (params.num_iterations && *params.num_iterations <= 0) {
        throw std::runtime_error("num_iterations must be positive");
    }
    validate_probability("reopt", params.reopt);
    validate_probability("epsilon", params.epsilon);
    if (params.verbose < 0 || params.verbose > 3) {
        throw std::runtime_error("verbose must be in [0, 3]");
    }
    if (params.init_tour) {
        validate_choice("init_tour", *params.init_tour, {"rand", "insertion"});
    }
    if (params.noise) {
        validate_choice("noise", *params.noise, {"None", "Add", "Subset", "Both"});
    }
    if (params.insertion_algs) {
        validate_choice("insertion_algs", *params.insertion_algs,
                        {"default", "cheapest", "randpdf", "classic"});
    }
    if (params.removal_algs) {
        validate_choice("removal_algs", *params.removal_algs, {"default", "classic"});
    }
}

}  // namespace

Config resolve_parameters(const Instance& inst, const Params& params) {
    const int num_sets = inst.num_sets;
    validate_parameters(params);
    Config c;
    c.mode = params.mode;

    if (params.mode == "default") {
        c.num_iterations = scaled_iterations(params.num_iterations.value_or(60), num_sets);
        c.cold_trials = params.trials.value_or(5);
        c.warm_trials = params.restarts.value_or(3);
        c.max_time = params.max_time.value_or(360);
        c.init_tour = params.init_tour.value_or("rand");
        c.prob_reopt = params.reopt.value_or(1.0);
        c.latest_improvement = c.num_iterations / 2.0;
        c.first_improvement = c.num_iterations / 4.0;
        c.max_removals = static_cast<int>(
            std::min<std::int64_t>(100, std::max<std::int64_t>(round_int(0.3 * num_sets), 1)));
        c.insertions = {"randpdf", "cheapest"};
    } else if (params.mode == "fast") {
        c.num_iterations = scaled_iterations(params.num_iterations.value_or(60), num_sets);
        c.cold_trials = params.trials.value_or(3);
        c.warm_trials = params.restarts.value_or(2);
        c.max_time = params.max_time.value_or(300);
        c.init_tour = params.init_tour.value_or("insertion");
        c.prob_reopt = params.reopt.value_or(0.2);
        c.latest_improvement = c.num_iterations / 4.0;
        c.first_improvement = c.num_iterations / 6.0;
        c.max_removals = static_cast<int>(
            std::min<std::int64_t>(20, std::max<std::int64_t>(round_int(0.1 * num_sets), 1)));
        c.insertions = {"randpdf"};
    } else if (params.mode == "slow") {
        c.num_iterations = scaled_iterations(params.num_iterations.value_or(150), num_sets);
        c.cold_trials = params.trials.value_or(10);
        c.warm_trials = params.restarts.value_or(5);
        c.max_time = params.max_time.value_or(1200);
        c.init_tour = params.init_tour.value_or("rand");
        c.prob_reopt = params.reopt.value_or(1.0);
        c.latest_improvement = c.num_iterations / 3.0;
        c.first_improvement = c.num_iterations / 6.0;
        c.max_removals =
            static_cast<int>(std::max<std::int64_t>(round_int(0.4 * num_sets), 1));
        c.insertions = {"randpdf", "cheapest"};
    }

    c.accept_percentage = 0.05;
    c.prob_accept = 10.0 / static_cast<double>(c.num_iterations);

    // insertion algorithms
    const std::string insertion_algs = params.insertion_algs.value_or("default");
    if (insertion_algs == "cheapest") {
        c.insertions = {"randpdf", "cheapest"};
    } else if (insertion_algs == "randpdf") {
        c.insertions = {"randpdf"};
    }

    // insertion powers
    if (insertion_algs == "classic") {
        c.insertion_powers = {-10.0, 0.0, 10.0};
    } else {
        c.insertion_powers = {-10.0, -1.0, -0.5, 0.0, 0.5, 1.0, 10.0};
    }

    // removal algorithms and powers
    if (params.removal_algs.value_or("default") == "classic") {
        c.removals = {"worst"};
        c.removal_powers = {0.0, 10.0};
    } else {
        c.removals = {"distance", "worst", "segment"};
        c.removal_powers = {-0.5, 0.0, 0.5, 1.0, 10.0};
    }

    // parameters common to all modes
    c.instance_name = inst.name.empty() ? "instance" : basename_of(inst.name);
    c.num_sets = num_sets;
    c.num_vertices = inst.num_vertices;
    c.output_file = params.output_file;
    c.print_output = params.verbose;
    c.epsilon = params.epsilon.value_or(0.5);
    c.noise_mode = params.noise.value_or("Add");
    c.print_time_interval = 5.0;
    c.budget = params.budget.value_or(kMinCost);
    c.min_set_index = min_set(inst.sets);
    c.min_removals = c.max_removals > 1 ? 2 : 1;
    c.seed = params.seed ? *params.seed
                         : (static_cast<std::uint64_t>(std::random_device{}()) << 32) ^
                               std::random_device{}();

    print_params(c);
    return c;
}

}  // namespace detail
}  // namespace glns
