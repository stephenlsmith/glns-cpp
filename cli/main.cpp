// Copyright 2017 Stephen L. Smith and Frank Imeson
// Licensed under the Apache License, Version 2.0.  See LICENSE.
//
// Command-line GLNS solver; flag-compatible with GLNScmd.jl, plus -seed.

#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>

#include "glns/glns.hpp"

namespace {

void print_usage() {
    std::printf("Usage:  glns [filename] [optional flags]\n\n");
    std::printf("Optional flags (values are given in square brackets) :\n\n");
    std::printf("-mode=[default, fast, slow]      (default is default)\n");
    std::printf("-max_time=[Int]                  (default set by mode)\n");
    std::printf("-trials=[Int]                    (default set by mode)\n");
    std::printf("-restarts=[Int]                  (default set by mode)\n");
    std::printf("-noise=[None, Both, Subset, Add] (default is Add)\n");
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
        std::printf("no input instance given\n");
        return 0;
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
                if (filename.empty()) filename = arg;
                continue;
            }
            const std::string flag = arg.substr(0, eq);
            const std::string value = arg.substr(eq + 1);
            if (flag == "-max_time") {
                params.max_time = std::stod(value);
            } else if (flag == "-trials") {
                params.trials = std::stoi(value);
            } else if (flag == "-restarts") {
                params.restarts = std::stoi(value);
            } else if (flag == "-verbose") {
                params.verbose = std::stoi(value);
            } else if (flag == "-budget") {
                params.budget = std::stoll(value);
            } else if (flag == "-num_iterations") {
                params.num_iterations = std::stoll(value);
            } else if (flag == "-seed") {
                params.seed = std::stoull(value);
            } else if (flag == "-epsilon") {
                params.epsilon = std::stod(value);
            } else if (flag == "-reopt") {
                params.reopt = std::stod(value);
            } else if (flag == "-mode") {
                params.mode = value;
            } else if (flag == "-output") {
                params.output_file = value;
            } else if (flag == "-noise") {
                params.noise = value;
            } else {
                std::printf("WARNING: skipping unknown flag %s in command line arguments\n",
                            flag.c_str());
            }
        }

        if (filename.empty()) {
            std::printf("no input instance given\n");
            return 0;
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
