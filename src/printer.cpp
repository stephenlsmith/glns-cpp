// Copyright 2017 Stephen L. Smith and Frank Imeson
// Licensed under the Apache License, Version 2.0.  See LICENSE.
//
// Port of the output half of parse_print.jl: parameter listing, progress
// reporting, and the tour summary / output file.
//
// One deviation from the Julia version: the summary is only printed when
// verbose > 0, so that library use is silent by default.  The CLI restores
// the Julia behavior of always printing a summary when there is no output
// file.

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <unistd.h>

#include "internal.hpp"

namespace glns {
namespace detail {

namespace {

// format a tour as Julia prints an Int vector: [3, 1, 2] (1-indexed ids)
std::string tour_string(const std::vector<int>& tour) {
    std::ostringstream out;
    out << "[";
    for (std::size_t i = 0; i < tour.size(); ++i) {
        if (i > 0) out << ", ";
        out << tour[i] + 1;
    }
    out << "]";
    return out.str();
}

// progress bar string (progress_bar in parse_print.jl)
void progress_bar(int trials, double progress, Cost cost, double time_sec) {
    const int ticks = 6, trials_per_bar = 5, total_length = 31;
    if (progress == 1.0) progress -= 0.0001;
    const int n = static_cast<int>(progress * trials / trials_per_bar);
    const int start_number = n * trials_per_bar;
    const int trials_in_bar = std::min(trials_per_bar, trials - start_number);

    const double progress_in_bar = (progress * trials - start_number) / trials_in_bar;
    const int bar_length = std::min(total_length - 1, (trials - start_number) * ticks);

    std::string bar = "|";
    for (int i = 1; i <= total_length; ++i) {
        if (i == bar_length + 1) {
            bar += "|";
        } else if (i > bar_length + 1) {
            bar += " ";
        } else if (i % ticks == 1) {
            bar += std::to_string(
                start_number + static_cast<int>(std::ceil(static_cast<double>(i) / ticks)));
        } else if (i <= static_cast<int>(std::ceil(bar_length * progress_in_bar))) {
            bar += "=";
        } else {
            bar += " ";
        }
    }
    std::printf(" %s  Cost = %lld  Time = %.1f sec      \r", bar.c_str(),
                static_cast<long long>(cost), time_sec);
    std::fflush(stdout);
}

}  // namespace

void print_params(const Config& config) {
    if (config.print_output > 0) {
        std::printf("\n--------- Problem Data ------------\n");
        std::printf("Instance Name      : %s\n", config.instance_name.c_str());
        std::printf("Number of Vertices : %d\n", config.num_vertices);
        std::printf("Number of Sets     : %d\n", config.num_sets);
        std::printf("Initial Tour       : %s\n",
                    config.init_tour == "rand" ? "Random" : "Random Insertion");
        std::printf("Maximum Removals   : %d\n", config.max_removals);
        std::printf("Trials             : %d\n", config.cold_trials);
        std::printf("Restart Attempts   : %d\n", config.warm_trials);
        std::printf("Rate of Adaptation : %g\n", config.epsilon);
        std::printf("Prob of Reopt      : %g\n", config.prob_reopt);
        std::printf("Maximum Time       : %g\n", config.max_time);
        if (config.budget == kMinCost) {
            std::printf("Tour Budget        : None\n");
        } else {
            std::printf("Tour Budget        : %lld\n", static_cast<long long>(config.budget));
        }
        std::printf("RNG Seed           : %llu\n",
                    static_cast<unsigned long long>(config.seed));
        std::printf("-----------------------------------\n\n");
    }
}

void print_warm_trial(const Counters& count, const Config& config, const Tour& best,
                      std::int64_t iter_count) {
    if (config.print_output == 2) {
        std::printf("-- %d.%d - iterations %lld:  cost %lld\n", count.cold_trial,
                    count.warm_trial, static_cast<long long>(iter_count),
                    static_cast<long long>(best.cost));
    }
}

void print_best(Counters& count, const Config& config, const Tour& best, const Tour& lowest,
                double elapsed, bool budget_met, bool timeout) {
    if (config.print_output == 1 &&
        elapsed - count.print_time > config.print_time_interval) {
        count.print_time = elapsed;
        std::printf("-- trial %d.%d:  Cost = %lld  Time = %.1f sec\n", count.cold_trial,
                    count.warm_trial, static_cast<long long>(std::min(best.cost, lowest.cost)),
                    elapsed);
    } else if ((config.print_output == 3 && elapsed - count.print_time > 0.5) || budget_met ||
               timeout) {
        count.print_time = elapsed;
        double progress;
        if (config.warm_trials > 0) {
            progress = (count.cold_trial - 1) / static_cast<double>(config.cold_trials) +
                       static_cast<double>(count.warm_trial) / config.warm_trials /
                           config.cold_trials;
        } else {
            progress = (count.cold_trial - 1) / static_cast<double>(config.cold_trials);
        }
        if (config.print_output == 3) {
            progress_bar(config.cold_trials, progress, std::min(best.cost, lowest.cost),
                         elapsed);
        }
    }
}

void print_summary(const Tour& lowest, double timer, const std::vector<int>& member,
                   const Config& config, bool timeout, bool budget_met) {
    if (config.print_output == 3 && !timeout && !budget_met) {
        progress_bar(config.cold_trials, 1.0, lowest.cost, timer);
    }
    if (config.print_output > 0) {
        std::printf("\n\n--------- Tour Summary ------------\n");
        std::printf("Cost              : %lld\n", static_cast<long long>(lowest.cost));
        std::printf("Total Time        : %.2f sec\n", timer);
        std::printf("Solver Timeout?   : %s\n", timeout ? "true" : "false");
        std::printf("Tour is Feasible? : %s\n",
                    tour_feasibility(lowest.tour, member, config.num_sets) ? "true" : "false");
        std::printf("Output File       : %s\n", config.output_file.c_str());
        if (config.output_file == "None") {
            std::printf("Tour Ordering     : %s\n", tour_string(lowest.tour).c_str());
        } else {
            std::printf("Tour Ordering     : printed to %s\n", config.output_file.c_str());
        }
        std::printf("-----------------------------------\n");
    }
    if (config.output_file != "None") {
        char hostname[256] = "unknown";
        gethostname(hostname, sizeof(hostname));
        std::ofstream out(config.output_file);
        out << "Problem Instance : " << config.instance_name << "\n";
        out << "Vertices         : " << config.num_vertices << "\n";
        out << "Sets             : " << config.num_sets << "\n";
        out << "Comment          : Solved with the C++ port of GLNS\n";
        out << "Host Computer    : " << hostname << "\n";
        out << "Solver Time      : " << std::fixed;
        out.precision(3);
        out << timer << " sec\n";
        out << "Tour Cost        : " << lowest.cost << "\n";
        out << "Tour             : " << tour_string(lowest.tour);
    }
}

}  // namespace detail
}  // namespace glns
