// Copyright 2017 Stephen L. Smith and Frank Imeson
// Licensed under the Apache License, Version 2.0.  See LICENSE.
//
// Port of the input half of parse_print.jl: parser for GTSPLIB-format
// instances (an extension of TSPLIB) and for the "simple" matrix format.
//
// One deviation from the Julia version: its LOWER_ROW branch is marked
// untested and indexes out of bounds; here LOWER_ROW fills the strict
// lower triangle row by row.

#include <cctype>
#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "internal.hpp"

namespace glns {

namespace {

constexpr Cost kParserInf = 9999;  // INF in the Julia parser

std::string trim(const std::string& s) {
    std::size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

std::string upper(std::string s) {
    for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

// matches the Julia data-line regex ^[\d\se+-\.]+$ (non-empty line of
// numbers, whitespace, and number punctuation)
bool is_data_line(const std::string& line) {
    if (line.empty()) return false;
    for (char c : line) {
        if (!(std::isdigit(static_cast<unsigned char>(c)) ||
              std::isspace(static_cast<unsigned char>(c)) || c == 'e' || c == '+' ||
              c == '-' || c == '.')) {
            return false;
        }
    }
    return true;
}

bool contains_digit(const std::string& line) {
    for (char c : line) {
        if (std::isdigit(static_cast<unsigned char>(c))) return true;
    }
    return false;
}

// matches ^\s*KEY\s*:\s*<value> (line already trimmed; KEY uppercase)
bool is_header(const std::string& line, const std::string& key) {
    const std::string u = upper(line);
    if (u.rfind(key, 0) != 0) return false;
    std::size_t i = key.size();
    while (i < u.size() && (u[i] == ' ' || u[i] == '\t')) ++i;
    return i < u.size() && u[i] == ':';
}

// matches ^\s*KEY\s*:?\s*$ (a section marker)
bool is_section(const std::string& line, const std::string& key) {
    const std::string u = upper(line);
    if (u.rfind(key, 0) != 0) return false;
    std::size_t i = key.size();
    while (i < u.size() && (u[i] == ' ' || u[i] == '\t')) ++i;
    if (i < u.size() && u[i] == ':') ++i;
    while (i < u.size() && (u[i] == ' ' || u[i] == '\t')) ++i;
    return i == u.size();
}

// value after the last ':' (mirrors strip(split(line, ":")[end]))
std::string header_value(const std::string& line) {
    const std::size_t pos = line.find_last_of(':');
    return trim(pos == std::string::npos ? line : line.substr(pos + 1));
}

std::vector<std::string> tokens(const std::string& line) {
    std::istringstream in(line);
    std::vector<std::string> out;
    std::string tok;
    while (in >> tok) out.push_back(tok);
    return out;
}

std::int64_t to_int(const std::string& s) {
    try {
        return std::stoll(s);
    } catch (const std::exception&) {
        throw std::runtime_error("failed to parse integer: '" + s + "'");
    }
}

// nint as defined by TSPLIB
std::int64_t nint(double x) { return static_cast<std::int64_t>(std::floor(x + 0.5)); }

std::pair<double, double> degree_minutes(double num) {
    const double deg = num > 0 ? std::floor(num) : std::ceil(num);
    return {deg, num - deg};
}

}  // namespace

Instance read_instance(const std::string& filename) {
    std::ifstream file(filename);
    if (!file) {
        throw std::runtime_error("the problem instance " + filename + " does not exist");
    }

    enum class State {
        kUnknownFormat,
        kTsplibHeader,
        kTsplibMatrixData,
        kTsplibDisplayData,
        kTsplibCoordData,
        kTsplibSetData,
        kTsplibDone,
        kSimpleHeader,
        kSimpleSets,
        kSimpleMatrix,
        kSimpleDone,
    };

    State state = State::kUnknownFormat;
    std::string data_type, data_format;
    Matrix dist;
    std::vector<std::vector<int>> sets;         // 1-based ids during parsing
    std::vector<std::vector<double>> coords;
    std::vector<std::int64_t> set_data;
    int num_vertices = -1;
    int num_sets = -1;
    int vid0 = 0, vid1 = 0;  // 0-based fill position in the matrix

    std::string raw_line;
    while (std::getline(file, raw_line)) {
        const std::string line = trim(raw_line);

        // auto format select
        if (state == State::kUnknownFormat) {
            if (is_header(line, "NAME")) {
                state = State::kTsplibHeader;
            } else if (is_header(line, "N")) {
                state = State::kSimpleHeader;
            }
        }

        if (state == State::kTsplibHeader) {
            if (is_header(line, "DIMENSION")) {
                num_vertices = static_cast<int>(to_int(header_value(line)));
                dist = Matrix(num_vertices, 0);
            } else if (is_header(line, "GTSP_SETS")) {
                num_sets = static_cast<int>(to_int(header_value(line)));
            } else if (is_header(line, "EDGE_WEIGHT_TYPE")) {
                data_type = header_value(line);
                data_format = data_type;
            } else if (is_header(line, "EDGE_WEIGHT_FORMAT")) {
                if (data_type == "EXPLICIT") data_format = header_value(line);
            } else if (is_section(line, "EDGE_WEIGHT_SECTION")) {
                state = State::kTsplibMatrixData;
            } else if (is_section(line, "NODE_COORD_SECTION")) {
                state = State::kTsplibCoordData;
            }

        } else if (state == State::kTsplibMatrixData) {
            if (is_data_line(line)) {
                for (const std::string& tok : tokens(line)) {
                    const Cost cost = to_int(tok);
                    if (data_format == "FULL_MATRIX") {
                        dist(vid0, vid1) = cost;
                        if (++vid1 >= num_vertices) {
                            ++vid0;
                            vid1 = 0;
                        }
                    } else if (data_format == "LOWER_DIAG_ROW") {
                        dist(vid0, vid1) = cost;
                        dist(vid1, vid0) = cost;
                        if (++vid1 > vid0) {
                            ++vid0;
                            vid1 = 0;
                        }
                    } else if (data_format == "LOWER_ROW") {
                        // strict lower triangle, row by row starting at row 1
                        if (vid0 == 0) vid0 = 1;
                        dist(vid0, vid1) = cost;
                        dist(vid1, vid0) = cost;
                        if (++vid1 >= vid0) {
                            ++vid0;
                            vid1 = 0;
                        }
                    } else if (data_format == "UPPER_DIAG_ROW") {
                        dist(vid0, vid1) = cost;
                        dist(vid1, vid0) = cost;
                        if (++vid1 >= num_vertices) {
                            ++vid0;
                            vid1 = vid0;
                        }
                    } else if (data_format == "UPPER_ROW") {
                        if (vid0 == 0 && vid1 == 0) vid1 = 1;
                        dist(vid0, vid1) = cost;
                        dist(vid1, vid0) = cost;
                        if (++vid1 >= num_vertices) {
                            ++vid0;
                            vid1 = vid0 + 1;
                        }
                    } else {
                        throw std::runtime_error("edge weight format " + data_format +
                                                 " not supported");
                    }
                }
            } else if (is_section(line, "DISPLAY_DATA_SECTION")) {
                state = State::kTsplibDisplayData;
            } else if (is_section(line, "GTSP_SET_SECTION")) {
                state = State::kTsplibSetData;
            }

        } else if (state == State::kTsplibDisplayData) {
            if (is_section(line, "GTSP_SET_SECTION")) {
                state = State::kTsplibSetData;
            }

        } else if (state == State::kTsplibCoordData) {
            if (is_section(line, "GTSP_SET_SECTION")) {
                state = State::kTsplibSetData;
            } else if (contains_digit(line)) {
                const std::vector<std::string> toks = tokens(line);
                std::vector<double> coord;
                for (std::size_t i = 1; i < toks.size(); ++i) {
                    coord.push_back(std::stod(toks[i]));
                }
                coords.push_back(coord);
            }

        } else if (state == State::kTsplibSetData) {
            if (is_section(line, "EOF")) {
                state = State::kTsplibDone;
            } else if (contains_digit(line)) {
                for (const std::string& tok : tokens(line)) {
                    set_data.push_back(to_int(tok));
                }
            }

        } else if (state == State::kSimpleHeader) {
            if (is_header(line, "N")) {
                num_vertices = static_cast<int>(to_int(header_value(line)));
                dist = Matrix(num_vertices, 0);
            } else if (is_header(line, "M")) {
                num_sets = static_cast<int>(to_int(header_value(line)));
                state = State::kSimpleSets;
            }

        } else if (state == State::kSimpleSets) {
            if (is_data_line(line)) {
                const std::vector<std::string> toks = tokens(line);
                const std::int64_t sid = to_int(toks[0]);
                std::vector<int> set;
                for (std::size_t i = 1; i < toks.size(); ++i) {
                    set.push_back(static_cast<int>(to_int(toks[i])));
                }
                sets.push_back(set);
                if (sid == num_sets) state = State::kSimpleMatrix;
            }

        } else if (state == State::kSimpleMatrix) {
            if (is_data_line(line)) {
                for (const std::string& tok : tokens(line)) {
                    dist(vid0, vid1) = to_int(tok);
                    if (++vid1 >= num_vertices) {
                        ++vid0;
                        vid1 = 0;
                    }
                }
            } else {
                state = State::kSimpleDone;
            }
        }
    }

    const bool tsplib = state == State::kTsplibMatrixData ||
                        state == State::kTsplibDisplayData ||
                        state == State::kTsplibCoordData ||
                        state == State::kTsplibSetData || state == State::kTsplibDone;

    // convert coordinate data to matrix data
    if (tsplib && data_type != "EXPLICIT") {
        if (static_cast<int>(coords.size()) != num_vertices) {
            throw std::runtime_error("node coordinate count doesn't match DIMENSION");
        }
        if (data_format == "EUC_2D" || data_format == "MAN_2D" ||
            data_format == "CEIL_2D") {
            for (int i = 0; i < num_vertices; ++i) {
                for (int j = 0; j < num_vertices; ++j) {
                    if (i == j) {
                        dist(i, j) = kParserInf;
                        continue;
                    }
                    const double dx = coords[i][0] - coords[j][0];
                    const double dy = coords[i][1] - coords[j][1];
                    if (data_format == "EUC_2D") {
                        dist(i, j) = nint(std::sqrt(dx * dx + dy * dy));
                    } else if (data_format == "MAN_2D") {
                        dist(i, j) = nint(std::abs(dx) + std::abs(dy));
                    } else {  // CEIL_2D
                        dist(i, j) =
                            static_cast<Cost>(std::ceil(std::sqrt(dx * dx + dy * dy)));
                    }
                }
            }
        } else if (data_format == "GEO") {
            // geographic distance as defined by TSPLIB
            const double kRadius = 6378.388;
            const double kPi = 3.141592;
            std::vector<double> lat(num_vertices), lon(num_vertices);
            for (int i = 0; i < num_vertices; ++i) {
                auto [d1, m1] = degree_minutes(coords[i][0]);
                lat[i] = kPi * (d1 + 5.0 * m1 / 3.0) / 180.0;
                auto [d2, m2] = degree_minutes(coords[i][1]);
                lon[i] = kPi * (d2 + 5.0 * m2 / 3.0) / 180.0;
            }
            for (int i = 0; i < num_vertices; ++i) {
                for (int j = 0; j < num_vertices; ++j) {
                    if (i == j) {
                        dist(i, j) = kParserInf;
                        continue;
                    }
                    const double q1 = std::cos(lon[i] - lon[j]);
                    const double q2 = std::cos(lat[i] - lat[j]);
                    const double q3 = std::cos(lat[i] + lat[j]);
                    const double cost =
                        kRadius * std::acos(0.5 * ((1.0 + q1) * q2 - (1.0 - q1) * q3)) + 1.0;
                    dist(i, j) = static_cast<Cost>(std::floor(cost));
                }
            }
        } else if (data_format == "ATT") {
            for (int i = 0; i < num_vertices; ++i) {
                for (int j = 0; j < num_vertices; ++j) {
                    if (i == j) {
                        dist(i, j) = kParserInf;
                        continue;
                    }
                    const double dx = coords[i][0] - coords[j][0];
                    const double dy = coords[i][1] - coords[j][1];
                    const double r = std::sqrt((dx * dx + dy * dy) / 10.0);
                    dist(i, j) = nint(std::ceil(r));
                }
            }
        } else {
            throw std::runtime_error("coordinate type " + data_format + " not supported");
        }
    }

    // construct sets from the GTSP_SET_SECTION data
    if (tsplib) {
        std::vector<int> set;
        // drop the leading set id, then split on -1 terminators (each of
        // which is followed by the next set's id)
        std::size_t i = 1;
        while (i < set_data.size()) {
            const std::int64_t x = set_data[i];
            if (x == -1) {
                sets.push_back(set);
                set.clear();
                i += 1;  // skip the next set id
            } else {
                set.push_back(static_cast<int>(x));
            }
            i += 1;
        }
        if (num_sets != static_cast<int>(sets.size())) {
            throw std::runtime_error("number of sets doesn't match set size");
        }
    }

    if (sets.size() <= 1) {
        throw std::runtime_error("must have more than 1 set");
    }

    // convert to 0-based vertex ids
    for (std::vector<int>& s : sets) {
        for (int& v : s) v -= 1;
    }

    Instance inst;
    inst.num_vertices = num_vertices;
    inst.num_sets = num_sets;
    inst.sets = std::move(sets);
    inst.dist = std::move(dist);
    inst.name = filename;
    inst.finalize();
    return inst;
}

}  // namespace glns
