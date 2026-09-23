// Benchmark adapter; solver code and default search settings are unchanged.
#include <glns/glns.hpp>

#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using Clock = std::chrono::steady_clock;

std::vector<std::string> split(const std::string& text, char delimiter) {
    std::vector<std::string> result;
    std::istringstream input(text);
    std::string part;
    while (std::getline(input, part, delimiter)) result.push_back(part);
    return result;
}

void validate(const glns::Instance& instance, const std::vector<int>& tour,
              glns::Cost cost) {
    if (tour.size() != static_cast<std::size_t>(instance.num_sets))
        throw std::runtime_error("wrong tour length");
    std::vector<bool> seen(instance.num_sets, false);
    glns::Cost actual = 0;
    for (std::size_t i = 0; i < tour.size(); ++i) {
        const int vertex = tour[i];
        if (vertex < 0 || vertex >= instance.num_vertices)
            throw std::runtime_error("invalid tour vertex");
        const int set = instance.membership[vertex];
        if (seen[set]) throw std::runtime_error("repeated tour set");
        seen[set] = true;
    }
    for (std::size_t i = 0; i < tour.size(); ++i)
        actual += instance.dist(tour[i], tour[(i + 1) % tour.size()]);
    if (actual != cost) throw std::runtime_error("incorrect tour cost");
}

int main() {
    std::cout << std::setprecision(17) << "ready\n" << std::flush;
    std::string line;
    try {
        while (std::getline(std::cin, line)) {
            const auto args = split(line, '\t');
            if (args.at(0) == "quit") break;
            const auto begin = Clock::now();
            const auto instance = glns::read_instance(args.at(1));
            if (args[0] == "parse") {
                std::uint64_t dist = 0, member = 0, sets = 0;
                for (int i = 0; i < instance.num_vertices; ++i) {
                    member += instance.membership[i] + 1;
                    for (int j = 0; j < instance.num_vertices; ++j)
                        dist = dist * 1099511628211ULL + instance.dist(i, j);
                }
                for (const auto& set : instance.sets)
                    for (int v : set) sets = sets * 31 + v + 1;
                std::cout << "parse\t" << instance.num_vertices << '\t'
                          << instance.num_sets << '\t' << dist << '\t'
                          << member << '\t' << sets << '\n' << std::flush;
            } else if (args[0] == "solve") {
                glns::Params params;
                params.seed = std::stoull(args.at(2));
                const auto solution = glns::solve(instance, params);
                const double call_seconds =
                    std::chrono::duration<double>(Clock::now() - begin).count();
                validate(instance, solution.tour, solution.cost);
                std::cout << "solve\t" << solution.cost << '\t'
                          << solution.solve_time << '\t' << call_seconds << '\t'
                          << solution.timeout << '\t' << solution.budget_met << '\t';
                for (std::size_t i = 0; i < solution.tour.size(); ++i) {
                    if (i) std::cout << ',';
                    std::cout << solution.tour[i];
                }
                std::cout << '\n' << std::flush;
            } else if (args[0] == "check") {
                std::vector<int> tour;
                for (const auto& v : split(args.at(3), ',')) tour.push_back(std::stoi(v));
                validate(instance, tour, std::stoll(args.at(2)));
                std::cout << "check\tok\n" << std::flush;
            } else {
                throw std::runtime_error("unknown benchmark command");
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "benchmark worker: " << e.what() << '\n';
        return 1;
    }
}
