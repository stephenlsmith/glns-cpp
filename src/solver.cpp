// Copyright 2017 Stephen L. Smith and Frank Imeson
// Licensed under the Apache License, Version 2.0.  See LICENSE.
//
// Port of GLNS.jl: the main solver loop (simulated annealing over
// adaptive removal-insertion moves, with warm and cold restarts).

#include <algorithm>
#include <chrono>
#include <cmath>
#include <stdexcept>

#include "internal.hpp"

namespace glns {

namespace {

using Clock = std::chrono::steady_clock;

double seconds_since(Clock::time_point start) {
    return std::chrono::duration<double>(Clock::now() - start).count();
}

}  // namespace

Solution solve(const Instance& instance, const Params& params) {
    using namespace detail;

    if (instance.num_vertices <= 0 || instance.num_sets <= 1 ||
        instance.num_sets > instance.num_vertices ||
        instance.dist.size() != instance.num_vertices ||
        instance.sets.size() != static_cast<std::size_t>(instance.num_sets) ||
        instance.membership.size() != static_cast<std::size_t>(instance.num_vertices)) {
        throw std::runtime_error("instance not finalized: call Instance::finalize() first");
    }

    const Config config = resolve_parameters(instance, params);
    Rng rng(config.seed);

    // mutable copy: the solver shuffles set orderings in place as it runs
    std::vector<std::vector<int>> sets = instance.sets;
    const Matrix& dist = instance.dist;
    const std::vector<int>& membership = instance.membership;

    const auto init_time = Clock::now();
    Counters count;

    Tour lowest{{}, kMaxCost};
    const Distsv setdist = set_vertex_dist(dist, config.num_sets, membership);
    Powers powers = initialize_powers(config);
    Workspace ws;
    Tour current, trial;  // persistent buffers for the inner loop

    bool timeout = false;
    bool budget_met = false;

    auto make_solution = [&](const Tour& tour, double timer) {
        Solution solution;
        solution.tour = tour.tour;
        solution.cost = tour.cost;
        solution.solve_time = timer;
        solution.timeout = timeout;
        solution.budget_met = budget_met;
        solution.total_iterations = count.total_iter;
        return solution;
    };

    auto stop_requested = [&](const Tour& best, double elapsed) {
        const Cost best_cost = std::min(best.cost, lowest.cost);
        timeout = elapsed >= config.max_time;
        budget_met = best_cost <= config.budget;
        // Nonnegative edge costs make zero a proof of global optimality.
        return best_cost == 0 || budget_met || timeout;
    };

    auto finish = [&](const Tour& best, double elapsed) {
        if (lowest.cost > best.cost) lowest = best;
        print_best(count, config, best, lowest, elapsed, budget_met, timeout);
        const double timer = seconds_since(init_time);
        print_summary(lowest, timer, membership, config, timeout, budget_met);
        return make_solution(lowest, timer);
    };

    while (count.cold_trial <= config.cold_trials) {
        if (count.cold_trial > 1) {
            const double restart_elapsed = seconds_since(init_time);
            if (stop_requested(lowest, restart_elapsed)) {
                return finish(lowest, restart_elapsed);
            }
        }
        // build tour from scratch on a cold restart
        Tour best = initial_tour(lowest, dist, sets, setdist, count.cold_trial, config, rng);
        Phase phase = kEarly;

        // Check even when the iteration threshold is too small to enter the loop.
        const double initial_elapsed = seconds_since(init_time);
        if (stop_requested(best, initial_elapsed)) {
            return finish(best, initial_elapsed);
        }

        if (count.cold_trial == 1) {
            powers = initialize_powers(config);
        } else {
            power_update(powers, config);
        }

        while (count.warm_trial <= config.warm_trials) {
            const double restart_elapsed = seconds_since(init_time);
            if (stop_requested(best, restart_elapsed)) {
                return finish(best, restart_elapsed);
            }
            std::int64_t iter_count = 1;
            current.tour = best.tour;
            current.cost = best.cost;
            double temperature =
                1.442 * config.accept_percentage * static_cast<double>(best.cost);
            // accept a solution with 50% higher cost with 0.05% chance after num_iterations
            const double cooling_rate =
                std::pow((0.0005 * static_cast<double>(lowest.cost)) /
                             (config.accept_percentage * static_cast<double>(current.cost)),
                         1.0 / static_cast<double>(config.num_iterations));

            if (count.warm_trial > 0) {  // on warm restarts, use a lower temperature
                temperature *= std::pow(cooling_rate,
                                        static_cast<double>(config.num_iterations) / 2.0);
                phase = kLate;
            }

            while (static_cast<double>(count.latest_improvement) <=
                   (count.first_improvement ? config.latest_improvement
                                            : config.first_improvement)) {
                if (phase == kEarly &&
                    static_cast<double>(iter_count) >
                        static_cast<double>(config.num_iterations) / 2.0) {
                    phase = kMid;  // move to mid phase after half the iterations
                }
                remove_insert(current, trial, dist, membership, setdist, sets, powers,
                              config, phase, ws, rng);

                // decide whether or not to accept the trial
                if (accepttrial_noparam(trial.cost, current.cost, config.prob_accept, rng) ||
                    accepttrial(trial.cost, current.cost, temperature, rng)) {
                    if (config.mode == "slow") {
                        opt_cycle(trial, dist, sets, membership, config, setdist,
                                  /*partial=*/false, ws, rng);
                    }
                    std::swap(current, trial);
                }
                if (current.cost < best.cost) {
                    count.latest_improvement = 1;
                    count.first_improvement = true;
                    if (count.cold_trial > 1 && count.warm_trial > 1) {
                        count.warm_trial = 1;
                    }
                    opt_cycle(current, dist, sets, membership, config, setdist,
                              /*partial=*/false, ws, rng);
                    best = current;
                } else {
                    count.latest_improvement += 1;
                }

                // Count the completed iteration even when it triggers termination.
                count.total_iter += 1;
                const double elapsed = seconds_since(init_time);
                if (stop_requested(best, elapsed)) {
                    return finish(best, elapsed);
                }

                temperature *= cooling_rate;  // cool the temperature
                iter_count += 1;
                print_best(count, config, best, lowest, elapsed, budget_met, timeout);
            }
            print_warm_trial(count, config, best, iter_count);
            count.latest_improvement = 1;
            count.first_improvement = false;
            // The configured limit may be INT_MAX; do not increment past it.
            if (count.warm_trial == config.warm_trials) break;
            count.warm_trial += 1;
        }
        if (lowest.cost > best.cost) lowest = best;
        count.warm_trial = 0;
        // As above, keep the terminating counter within the int range.
        if (count.cold_trial == config.cold_trials) break;
        count.cold_trial += 1;
    }

    const double timer = seconds_since(init_time);
    stop_requested(lowest, timer);
    print_summary(lowest, timer, membership, config, timeout, budget_met);
    return make_solution(lowest, timer);
}

}  // namespace glns
