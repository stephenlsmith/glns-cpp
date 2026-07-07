// Copyright 2017 Stephen L. Smith and Frank Imeson
// Licensed under the Apache License, Version 2.0.  See LICENSE.
//
// Prints parser checksums for instance files in the same format as the
// Julia ground-truth script, for cross-validation of the two parsers.

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "glns/glns.hpp"

int main(int argc, char** argv) {
    const std::uint64_t kFnv = 1099511628211ULL;
    for (int a = 1; a < argc; ++a) {
        const std::string path = argv[a];
        try {
            const glns::Instance inst = glns::read_instance(path);
            std::uint64_t distchk = 0;
            for (int i = 0; i < inst.num_vertices; ++i) {
                for (int j = 0; j < inst.num_vertices; ++j) {
                    distchk = distchk * kFnv + static_cast<std::uint64_t>(inst.dist(i, j));
                }
            }
            std::uint64_t memsum = 0;
            for (int v = 0; v < inst.num_vertices; ++v) {
                memsum += static_cast<std::uint64_t>(inst.membership[v]) + 1;
            }
            std::uint64_t setchk = 0;
            for (const std::vector<int>& s : inst.sets) {
                for (int v : s) setchk = setchk * 31 + static_cast<std::uint64_t>(v) + 1;
            }
            const std::size_t slash = path.find_last_of('/');
            const std::string base =
                slash == std::string::npos ? path : path.substr(slash + 1);
            std::printf("%s n=%d m=%d distchk=%llu memsum=%llu setchk=%llu\n", base.c_str(),
                        inst.num_vertices, inst.num_sets,
                        static_cast<unsigned long long>(distchk),
                        static_cast<unsigned long long>(memsum),
                        static_cast<unsigned long long>(setchk));
        } catch (const std::exception& e) {
            std::printf("%s ERROR %s\n", path.c_str(), e.what());
        }
    }
    return 0;
}
