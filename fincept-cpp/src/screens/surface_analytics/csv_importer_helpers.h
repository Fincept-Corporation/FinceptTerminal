#pragma once
// csv_importer_helpers.h — private static helpers shared across csv_importer_*.cpp TUs.
// Include ONLY from csv_importer_*.cpp files.

#include <string>
#include <vector>
#include <cctype>

namespace fincept::surface {

static std::string trim(const std::string& s) {
    std::size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return {};
    std::size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

static std::vector<std::string> split_csv_line(const std::string& line) {
    std::vector<std::string> fields;
    std::string field;
    bool in_quotes = false;
    for (std::size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (c == '"') {
            if (in_quotes && i + 1 < line.size() && line[i + 1] == '"') {
                field += '"';
                ++i;
            } else {
                in_quotes = !in_quotes;
            }
        } else if (c == ',' && !in_quotes) {
            fields.push_back(trim(field));
            field.clear();
        } else {
            field += c;
        }
    }
    fields.push_back(trim(field));
    return fields;
}

static int col_idx(const std::vector<std::string>& header, const std::string& name) {
    for (int i = 0; i < static_cast<int>(header.size()); ++i) {
        if (header[i] == name) return i;
    }
    return -1;
}

static float to_f(const std::string& s) { return std::stof(s); }
static int   to_i(const std::string& s) { return std::stoi(s); }

template<typename T>
static int find_or_insert(std::vector<T>& vec, const T& val) {
    for (int i = 0; i < static_cast<int>(vec.size()); ++i) {
        if (vec[i] == val) return i;
    }
    vec.push_back(val);
    return static_cast<int>(vec.size()) - 1;
}

} // namespace fincept::surface
