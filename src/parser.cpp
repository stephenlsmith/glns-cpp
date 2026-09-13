// Copyright 2017 Stephen L. Smith and Frank Imeson
// Licensed under the Apache License, Version 2.0.  See LICENSE.
//
// Port of the input half of parse_print.jl: parser for GTSPLIB-format
// instances (an extension of TSPLIB) and for the "simple" matrix format.
//
// One deviation from the Julia version: its LOWER_ROW branch is marked
// untested and indexes out of bounds; here LOWER_ROW fills the strict
// lower triangle row by row.

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <limits>
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
        std::size_t parsed = 0;
        const std::int64_t value = std::stoll(s, &parsed);
        if (parsed != s.size()) throw std::invalid_argument("trailing characters");
        return value;
    } catch (const std::exception&) {
        throw std::runtime_error("failed to parse integer: '" + s + "'");
    }
}

double to_double(const std::string& s, const std::string& field) {
    try {
        std::size_t parsed = 0;
        const double value = std::stod(s, &parsed);
        if (parsed != s.size() || !std::isfinite(value)) {
            throw std::invalid_argument("invalid floating-point value");
        }
        return value;
    } catch (const std::exception&) {
        throw std::runtime_error("failed to parse " + field + ": '" + s + "'");
    }
}

int to_positive_int(const std::string& s, const std::string& field) {
    const std::int64_t value = to_int(s);
    if (value <= 0 || value > std::numeric_limits<int>::max()) {
        throw std::runtime_error(field + " must be a positive integer");
    }
    return static_cast<int>(value);
}

std::size_t matrix_value_count(int n, const std::string& format) {
    const std::size_t size = static_cast<std::size_t>(n);
    const std::size_t max = std::numeric_limits<std::size_t>::max();
    if (format == "FULL_MATRIX") {
        if (size > max / size) throw std::runtime_error("distance matrix is too large");
        return size * size;
    }
    if (format == "LOWER_DIAG_ROW" || format == "UPPER_DIAG_ROW") {
        std::size_t a = size;
        std::size_t b = size + 1;
        if (a % 2 == 0) {
            a /= 2;
        } else {
            b /= 2;
        }
        if (a > max / b) throw std::runtime_error("distance matrix is too large");
        return a * b;
    }
    if (format == "LOWER_ROW" || format == "UPPER_ROW") {
        std::size_t a = size;
        std::size_t b = size - 1;
        if (a % 2 == 0) {
            a /= 2;
        } else {
            b /= 2;
        }
        if (b != 0 && a > max / b) {
            throw std::runtime_error("distance matrix is too large");
        }
        return a * b;
    }
    throw std::runtime_error("edge weight format " + format + " not supported");
}

Cost checked_distance(double value) {
    if (!std::isfinite(value) || value < 0.0 ||
        value > static_cast<double>(std::numeric_limits<std::int32_t>::max())) {
        throw std::runtime_error("computed distance is outside the supported 32-bit range");
    }
    return static_cast<Cost>(value);
}

// nint as defined by TSPLIB
Cost nint(double x) { return checked_distance(std::floor(x + 0.5)); }

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
    std::vector<bool> coord_seen;
    int coord_count = 0;
    std::vector<std::int64_t> set_data;
    int num_vertices = -1;
    int num_sets = -1;
    int vid0 = 0, vid1 = 0;  // 0-based fill position in the matrix
    std::size_t matrix_values = 0;
    std::size_t expected_matrix_values = 0;
    bool matrix_size_known = false;

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
                num_vertices =
                    to_positive_int(header_value(line), "DIMENSION");
                dist = Matrix(num_vertices, 0);
                coords.assign(num_vertices, {});
                coord_seen.assign(num_vertices, false);
            } else if (is_header(line, "GTSP_SETS")) {
                num_sets = to_positive_int(header_value(line), "GTSP_SETS");
            } else if (is_header(line, "EDGE_WEIGHT_TYPE")) {
                data_type = upper(header_value(line));
                data_format = data_type;
            } else if (is_header(line, "EDGE_WEIGHT_FORMAT")) {
                if (data_type == "EXPLICIT") data_format = upper(header_value(line));
            } else if (is_section(line, "EDGE_WEIGHT_SECTION")) {
                if (num_vertices <= 0) {
                    throw std::runtime_error("EDGE_WEIGHT_SECTION precedes DIMENSION");
                }
                expected_matrix_values = matrix_value_count(num_vertices, data_format);
                matrix_size_known = true;
                state = State::kTsplibMatrixData;
            } else if (is_section(line, "NODE_COORD_SECTION")) {
                if (num_vertices <= 0) {
                    throw std::runtime_error("NODE_COORD_SECTION precedes DIMENSION");
                }
                state = State::kTsplibCoordData;
            }

        } else if (state == State::kTsplibMatrixData) {
            if (is_data_line(line)) {
                for (const std::string& tok : tokens(line)) {
                    if (matrix_values >= expected_matrix_values) {
                        throw std::runtime_error("edge weight matrix has more values than expected");
                    }
                    const Cost cost = to_int(tok);
                    if (data_format == "FULL_MATRIX") {
                        dist.set(vid0, vid1, cost);
                        if (++vid1 >= num_vertices) {
                            ++vid0;
                            vid1 = 0;
                        }
                    } else if (data_format == "LOWER_DIAG_ROW") {
                        dist.set(vid0, vid1, cost);
                        dist.set(vid1, vid0, cost);
                        if (++vid1 > vid0) {
                            ++vid0;
                            vid1 = 0;
                        }
                    } else if (data_format == "LOWER_ROW") {
                        // strict lower triangle, row by row starting at row 1
                        if (vid0 == 0) vid0 = 1;
                        dist.set(vid0, vid1, cost);
                        dist.set(vid1, vid0, cost);
                        if (++vid1 >= vid0) {
                            ++vid0;
                            vid1 = 0;
                        }
                    } else if (data_format == "UPPER_DIAG_ROW") {
                        dist.set(vid0, vid1, cost);
                        dist.set(vid1, vid0, cost);
                        if (++vid1 >= num_vertices) {
                            ++vid0;
                            vid1 = vid0;
                        }
                    } else if (data_format == "UPPER_ROW") {
                        if (vid0 == 0 && vid1 == 0) vid1 = 1;
                        dist.set(vid0, vid1, cost);
                        dist.set(vid1, vid0, cost);
                        if (++vid1 >= num_vertices) {
                            ++vid0;
                            vid1 = vid0 + 1;
                        }
                    }
                    ++matrix_values;
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
                if (toks.size() != 3) {
                    throw std::runtime_error(
                        "node coordinate row must contain an id and two coordinates");
                }
                const int id = to_positive_int(toks[0], "node id");
                if (id > num_vertices) {
                    throw std::runtime_error("node id " + std::to_string(id) + " out of range");
                }
                if (coord_seen[id - 1]) {
                    throw std::runtime_error("duplicate node id " + std::to_string(id));
                }
                try {
                    coords[id - 1] = {to_double(toks[1], "coordinate"),
                                      to_double(toks[2], "coordinate")};
                } catch (const std::exception&) {
                    throw std::runtime_error("failed to parse coordinates for node " +
                                             std::to_string(id));
                }
                coord_seen[id - 1] = true;
                ++coord_count;
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
                num_vertices = to_positive_int(header_value(line), "N");
                dist = Matrix(num_vertices, 0);
            } else if (is_header(line, "M")) {
                num_sets = to_positive_int(header_value(line), "M");
                if (num_vertices <= 0) {
                    throw std::runtime_error("M header precedes N header");
                }
                state = State::kSimpleSets;
            }

        } else if (state == State::kSimpleSets) {
            if (is_data_line(line)) {
                const std::vector<std::string> toks = tokens(line);
                const std::int64_t sid = to_int(toks[0]);
                const std::int64_t expected_sid =
                    static_cast<std::int64_t>(sets.size()) + 1;
                if (sid != expected_sid || sid > num_sets) {
                    throw std::runtime_error("unexpected set id " + std::to_string(sid) +
                                             "; expected " +
                                             std::to_string(expected_sid));
                }
                std::vector<int> set;
                for (std::size_t i = 1; i < toks.size(); ++i) {
                    set.push_back(to_positive_int(toks[i], "vertex id"));
                }
                sets.push_back(set);
                if (sid == num_sets) {
                    expected_matrix_values =
                        matrix_value_count(num_vertices, "FULL_MATRIX");
                    matrix_size_known = true;
                    state = State::kSimpleMatrix;
                }
            }

        } else if (state == State::kSimpleMatrix) {
            if (is_data_line(line)) {
                for (const std::string& tok : tokens(line)) {
                    if (matrix_values >= expected_matrix_values) {
                        throw std::runtime_error("distance matrix has more values than expected");
                    }
                    dist.set(vid0, vid1, to_int(tok));
                    ++matrix_values;
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

    if (tsplib && data_type == "EXPLICIT" && !matrix_size_known) {
        throw std::runtime_error("EXPLICIT instances require an EDGE_WEIGHT_SECTION");
    }
    if (matrix_size_known && matrix_values != expected_matrix_values) {
        throw std::runtime_error("distance matrix has " + std::to_string(matrix_values) +
                                 " values; expected " +
                                 std::to_string(expected_matrix_values));
    }

    // convert coordinate data to matrix data
    if (tsplib && data_type != "EXPLICIT") {
        if (coord_count != num_vertices) {
            throw std::runtime_error("node coordinate count doesn't match DIMENSION");
        }
        if (data_format == "EUC_2D" || data_format == "MAN_2D" ||
            data_format == "CEIL_2D") {
            for (int i = 0; i < num_vertices; ++i) {
                for (int j = 0; j < num_vertices; ++j) {
                    if (i == j) {
                        dist.set(i, j, kParserInf);
                        continue;
                    }
                    const double dx = coords[i][0] - coords[j][0];
                    const double dy = coords[i][1] - coords[j][1];
                    if (data_format == "EUC_2D") {
                        dist.set(i, j, nint(std::sqrt(dx * dx + dy * dy)));
                    } else if (data_format == "MAN_2D") {
                        dist.set(i, j, nint(std::abs(dx) + std::abs(dy)));
                    } else {  // CEIL_2D
                        dist.set(i, j,
                                 checked_distance(std::ceil(std::sqrt(dx * dx + dy * dy))));
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
                        dist.set(i, j, kParserInf);
                        continue;
                    }
                    const double q1 = std::cos(lon[i] - lon[j]);
                    const double q2 = std::cos(lat[i] - lat[j]);
                    const double q3 = std::cos(lat[i] + lat[j]);
                    const double acos_arg =
                        std::max(-1.0, std::min(1.0,
                            0.5 * ((1.0 + q1) * q2 - (1.0 - q1) * q3)));
                    const double cost = kRadius * std::acos(acos_arg) + 1.0;
                    dist.set(i, j, checked_distance(std::floor(cost)));
                }
            }
        } else if (data_format == "ATT") {
            for (int i = 0; i < num_vertices; ++i) {
                for (int j = 0; j < num_vertices; ++j) {
                    if (i == j) {
                        dist.set(i, j, kParserInf);
                        continue;
                    }
                    const double dx = coords[i][0] - coords[j][0];
                    const double dy = coords[i][1] - coords[j][1];
                    const double r = std::sqrt((dx * dx + dy * dy) / 10.0);
                    dist.set(i, j, nint(std::ceil(r)));
                }
            }
        } else {
            throw std::runtime_error("coordinate type " + data_format + " not supported");
        }
    }

    // construct sets from the GTSP_SET_SECTION data
    if (tsplib) {
        std::size_t i = 0;
        int expected_sid = 1;
        while (i < set_data.size()) {
            const std::int64_t sid = set_data[i++];
            if (sid != expected_sid) {
                throw std::runtime_error("unexpected set id " + std::to_string(sid) +
                                         "; expected " +
                                         std::to_string(expected_sid));
            }
            std::vector<int> set;
            while (i < set_data.size() && set_data[i] != -1) {
                set.push_back(to_positive_int(std::to_string(set_data[i]), "vertex id"));
                ++i;
            }
            if (i == set_data.size()) {
                throw std::runtime_error("set " + std::to_string(sid) +
                                         " is missing its -1 terminator");
            }
            ++i;  // -1 terminator
            sets.push_back(std::move(set));
            ++expected_sid;
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
