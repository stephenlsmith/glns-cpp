// Copyright 2017 Stephen L. Smith and Frank Imeson
// Licensed under the Apache License, Version 2.0.  See LICENSE.
//
// Command-line GLNS solver; flag-compatible with GLNScmd.jl, plus -seed.

#include <cstdio>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>

#include "glns/glns.hpp"

namespace {

template <class T>
T parse_number(const std::string& value, const std::string& flag) {
    try {
        std::size_t parsed = 0;
        T result;
        if constexpr (std::is_same_v<T, double>) {
            result = std::stod(value, &parsed);
        } else if constexpr (std::is_same_v<T, std::uint64_t>) {
            const std::size_t first = value.find_first_not_of(" \t\n\r\f\v");
            if (first != std::string::npos && value[first] == '-') {
                throw std::invalid_argument("negative seed");
            }
            result = std::stoull(value, &parsed);
        } else {
            const auto integer = std::stoll(value, &parsed);
            if (integer < std::numeric_limits<T>::min() ||
                integer > std::numeric_limits<T>::max()) {
                throw std::out_of_range("integer out of range");
            }
            result = static_cast<T>(integer);
        }
        if (parsed != value.size()) throw std::invalid_argument("trailing characters");
        return result;
    } catch (const std::exception&) {
        throw std::runtime_error("invalid value for " + flag + ": '" + value + "'");
    }
}

void print_usage() {
    std::printf("Usage:  glns [filename] [optional flags]\n\n");
    std::printf("Optional flags (values are given in square brackets) :\n\n");
    std::printf("-mode=[default, fast, slow]      (default is default)\n");
    std::printf("-max_time=[Int]                  (default set by mode)\n");
    std::printf("-trials=[Int]                    (default set by mode)\n");
    std::printf("-restarts=[Int]                  (default set by mode)\n");
    std::printf("-noise=[None, Both, Subset, Add] (default is Add)\n");
    std::printf("-init_tour=[rand, insertion]      (default set by mode)\n");
    std::printf("-insertion_algs=[default, cheapest, randpdf, classic]\n");
    std::printf("-removal_algs=[default, classic]\n");
    std::printf("-num_iterations=[Int]            (default set by mode. "
                "Number multiplied by # of sets)\n");
    std::printf("-verbose=[0, 1, 2, 3]            (default is 3. 0 is no output, "
                "3 is most verbose)\n");
    std::printf("-output=[filename]               (default is None)\n");
    std::printf("-epsilon=[Float in [0,1]]        (default is 0.5)\n");
    std::printf("-reopt=[Float in [0,1]]          (default is 1.0)\n");
    std::printf("-budget=[Int]                    (default has no budget)\n");
    std::printf("-seed=[Int]                      (default is nondeterministic)\n");
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "error: no input instance given\n");
        return 1;
    }
    const std::string first = argv[1];
    if (first == "-help" || first == "--help") {
        print_usage();
        return 0;
    }

    std::string filename;
    glns::Params params;
    params.verbose = 3;

    try {
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            const std::size_t eq = arg.find('=');
            if (eq == std::string::npos) {
                if (!arg.empty() && arg.front() == '-') {
                    throw std::runtime_error("unknown flag: " + arg);
                }
                if (!filename.empty()) {
                    throw std::runtime_error("unexpected positional argument: " + arg);
                }
                filename = arg;
                continue;
            }
            const std::string flag = arg.substr(0, eq);
            const std::string value = arg.substr(eq + 1);
            if (flag == "-max_time") {
                params.max_time = parse_number<double>(value, flag);
            } else if (flag == "-trials") {
                params.trials = parse_number<int>(value, flag);
            } else if (flag == "-restarts") {
                params.restarts = parse_number<int>(value, flag);
            } else if (flag == "-verbose") {
                params.verbose = parse_number<int>(value, flag);
            } else if (flag == "-budget") {
                params.budget = parse_number<std::int64_t>(value, flag);
            } else if (flag == "-num_iterations") {
                params.num_iterations = parse_number<std::int64_t>(value, flag);
            } else if (flag == "-seed") {
                params.seed = parse_number<std::uint64_t>(value, flag);
            } else if (flag == "-epsilon") {
                params.epsilon = parse_number<double>(value, flag);
            } else if (flag == "-reopt") {
                params.reopt = parse_number<double>(value, flag);
            } else if (flag == "-mode") {
                params.mode = value;
            } else if (flag == "-output") {
                params.output_file = value;
            } else if (flag == "-noise") {
                params.noise = value;
            } else if (flag == "-init_tour") {
                params.init_tour = value;
            } else if (flag == "-insertion_algs") {
                params.insertion_algs = value;
            } else if (flag == "-removal_algs") {
                params.removal_algs = value;
            } else {
                throw std::runtime_error("unknown flag: " + flag);
            }
        }

        if (filename.empty()) {
            throw std::runtime_error("no input instance given");
        }

        const glns::Instance instance = glns::read_instance(filename);
        const glns::Solution solution = glns::solve(instance, params);

        // like the Julia solver, never finish completely silently: if output
        // is suppressed and no tour file was written, print the cost
        if (params.verbose == 0 && params.output_file == "None") {
            std::printf("Cost: %lld\n", static_cast<long long>(solution.cost));
        }
    } catch (const std::exception& e) {
        std::fprintf(stderr, "error: %s\n", e.what());
        return 1;
    }
    return 0;
}
