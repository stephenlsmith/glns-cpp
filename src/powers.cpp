// Copyright 2017 Stephen L. Smith and Frank Imeson
// Licensed under the Apache License, Version 2.0.  See LICENSE.
//
// Port of adaptive_powers.jl: adaptive selection of removal/insertion/noise
// methods with per-phase roulette weights.

#include "internal.hpp"

namespace glns {
namespace detail {

namespace {

// initialize the noise methods: none, low, and high additive noise plus
// low and high subset noise (initialize_noise in adaptive_powers.jl)
std::vector<Power> initialize_noise(const std::string& noise_mode) {
    const Power none{"additive", 0.0, {{1.0, 1.0, 1.0}}, {{0, 0, 0}}, {{0, 0, 0}}};
    const Power low{"additive", 0.25, {{1.0, 1.0, 1.0}}, {{0, 0, 0}}, {{0, 0, 0}}};
    const Power high{"additive", 0.75, {{1.0, 1.0, 1.0}}, {{0, 0, 0}}, {{0, 0, 0}}};
    const Power sublow{"subset", 0.5, {{1.0, 1.0, 1.0}}, {{0, 0, 0}}, {{0, 0, 0}}};
    const Power subhigh{"subset", 0.25, {{1.0, 1.0, 1.0}}, {{0, 0, 0}}, {{0, 0, 0}}};

    if (noise_mode == "None") return {none};
    if (noise_mode == "Add") return {none, low, high};
    if (noise_mode == "Subset") return {none, sublow, subhigh};
    return {none, low, high, sublow, subhigh};  // "Both"
}

std::array<double, kNumPhases> total_power_weight(const std::vector<Power>& powers) {
    std::array<double, kNumPhases> total{};
    for (int phase = 0; phase < kNumPhases; ++phase) {
        for (const Power& p : powers) {
            total[phase] += p.weight[phase];
        }
    }
    return total;
}

// update weights from the scores accumulated over the finished trial
void power_weight_update(std::vector<Power>& powers, const Config& config) {
    for (int phase = 0; phase < kNumPhases; ++phase) {
        for (Power& power : powers) {
            if (power.count[phase] > 0) {
                power.weight[phase] =
                    config.epsilon * power.scores[phase] / power.count[phase] +
                    (1.0 - config.epsilon) * power.weight[phase];
            }
            power.scores[phase] = 0.0;
            power.count[phase] = 0;
        }
    }
}

}  // namespace

Powers initialize_powers(const Config& config) {
    Powers powers;

    for (const std::string& insertion : config.insertions) {
        if (insertion == "cheapest") {
            powers.insertions.push_back(Power{insertion, 0.0, {{1.0, 1.0, 1.0}},
                                              {{0, 0, 0}}, {{0, 0, 0}}});
        } else {
            for (double value : config.insertion_powers) {
                powers.insertions.push_back(Power{insertion, value, {{1.0, 1.0, 1.0}},
                                                  {{0, 0, 0}}, {{0, 0, 0}}});
            }
        }
    }

    // only positive powers for worst removal -- negative would be "best removal"
    for (const std::string& removal : config.removals) {
        if (removal == "segment") {
            powers.removals.push_back(Power{removal, 0.0, {{1.0, 1.0, 1.0}},
                                            {{0, 0, 0}}, {{0, 0, 0}}});
        } else {
            for (double value : config.removal_powers) {
                if (removal == "distance") {
                    if (value == 0.0) continue;  // equivalent to worst removal with 0.0
                    value *= -1.0;               // for distance we want the closest vertices
                }
                powers.removals.push_back(Power{removal, value, {{1.0, 1.0, 1.0}},
                                                {{0, 0, 0}}, {{0, 0, 0}}});
            }
        }
    }

    powers.noise = initialize_noise(config.noise_mode);

    powers.insertion_total = total_power_weight(powers.insertions);
    powers.removal_total = total_power_weight(powers.removals);
    powers.noise_total = total_power_weight(powers.noise);
    return powers;
}

Power& power_select(std::vector<Power>& powers, const std::array<double, kNumPhases>& total,
                    Phase phase, Rng& rng) {
    double selection = rng.next_double() * total[phase];
    for (Power& p : powers) {
        if (selection < p.weight[phase]) return p;
        selection -= p.weight[phase];
    }
    return powers.front();  // numerical fallback, as in the Julia code
}

void power_update(Powers& powers, const Config& config) {
    power_weight_update(powers.insertions, config);
    power_weight_update(powers.removals, config);
    power_weight_update(powers.noise, config);
    powers.insertion_total = total_power_weight(powers.insertions);
    powers.removal_total = total_power_weight(powers.removals);
    powers.noise_total = total_power_weight(powers.noise);
}

}  // namespace detail
}  // namespace glns
