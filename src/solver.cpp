// Copyright 2017 Stephen L. Smith and Frank Imeson
// Licensed under the Apache License, Version 2.0.  See LICENSE.
//
// Port of GLNS.jl: the main solver loop (simulated annealing over
// adaptive removal-insertion moves, with warm and cold restarts).

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

    if (static_cast<int>(instance.membership.size()) != instance.num_vertices) {
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

    while (count.cold_trial <= config.cold_trials) {
        // build tour from scratch on a cold restart
        Tour best = initial_tour(lowest, dist, sets, setdist, count.cold_trial, config, rng);
        Phase phase = kEarly;

        if (count.cold_trial == 1) {
            powers = initialize_powers(config);
        } else {
            power_update(powers, config);
        }

        while (count.warm_trial <= config.warm_trials) {
            std::int64_t iter_count = 1;
            Tour current{best.tour, best.cost};
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
                Tour trial = remove_insert(current, dist, membership, setdist, sets, powers,
                                           config, phase, rng);

                // decide whether or not to accept the trial
                if (accepttrial_noparam(trial.cost, current.cost, config.prob_accept, rng) ||
                    accepttrial(trial.cost, current.cost, temperature, rng)) {
                    if (config.mode == "slow") {
                        opt_cycle(trial, dist, sets, membership, config, setdist,
                                  /*partial=*/false, rng);
                    }
                    current = trial;
                }
                if (current.cost < best.cost) {
                    count.latest_improvement = 1;
                    count.first_improvement = true;
                    if (count.cold_trial > 1 && count.warm_trial > 1) {
                        count.warm_trial = 1;
                    }
                    opt_cycle(current, dist, sets, membership, config, setdist,
                              /*partial=*/false, rng);
                    best = current;
                } else {
                    count.latest_improvement += 1;
                }

                // if we've come in under budget, or we're out of time, exit
                const double elapsed = seconds_since(init_time);
                if (best.cost <= config.budget || elapsed > config.max_time) {
                    timeout = elapsed > config.max_time;
                    budget_met = best.cost <= config.budget;
                    if (lowest.cost > best.cost) lowest = best;
                    print_best(count, config, best, lowest, elapsed, budget_met, timeout);
                    const double timer = seconds_since(init_time);
                    print_summary(lowest, timer, membership, config, timeout, budget_met);
                    return make_solution(lowest, timer);
                }

                temperature *= cooling_rate;  // cool the temperature
                iter_count += 1;
                count.total_iter += 1;
                print_best(count, config, best, lowest, elapsed, budget_met, timeout);
            }
            print_warm_trial(count, config, best, iter_count);
            count.warm_trial += 1;
            count.latest_improvement = 1;
            count.first_improvement = false;
        }
        if (lowest.cost > best.cost) lowest = best;
        count.warm_trial = 0;
        count.cold_trial += 1;
    }

    const double timer = seconds_since(init_time);
    print_summary(lowest, timer, membership, config, timeout, budget_met);
    return make_solution(lowest, timer);
}

}  // namespace glns
