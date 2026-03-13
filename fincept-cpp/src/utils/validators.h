#pragma once
// Input validation utilities matching the TypeScript validators

#include <string>
#include <regex>
#include <algorithm>

namespace fincept::utils {

struct ValidationResult {
    bool valid = false;
    std::string error;
};

inline ValidationResult validate_email(const std::string& email) {
    if (email.empty()) return {false, "Email is required"};

    // Standard email regex
    static const std::regex email_re(R"([^\s@]+@[^\s@]+\.[^\s@]+)");
    if (!std::regex_match(email, email_re)) {
        return {false, "Invalid email format"};
    }
    return {true, ""};
}

struct PasswordStrength {
    bool min_length = false;
    bool has_upper = false;
    bool has_lower = false;
    bool has_number = false;
    bool has_special = false;

    bool is_valid() const {
        return min_length && has_upper && has_lower && has_number && has_special;
    }

    int score() const {
        return (int)min_length + (int)has_upper + (int)has_lower + (int)has_number + (int)has_special;
    }
};

inline PasswordStrength validate_password(const std::string& pw) {
    PasswordStrength s;
    s.min_length = pw.size() >= 8;
    s.has_upper = std::any_of(pw.begin(), pw.end(), ::isupper);
    s.has_lower = std::any_of(pw.begin(), pw.end(), ::islower);
    s.has_number = std::any_of(pw.begin(), pw.end(), ::isdigit);
    s.has_special = std::any_of(pw.begin(), pw.end(), [](char c) {
        return std::string("!@#$%^&*(),.?\":{}|<>").find(c) != std::string::npos;
    });
    return s;
}

// Sanitize input string (strip leading/trailing whitespace, remove control chars)
inline std::string sanitize_input(const std::string& input) {
    std::string result;
    result.reserve(input.size());
    for (char c : input) {
        if (c >= 32 || c == '\t') { // Allow printable chars and tab
            result += c;
        }
    }
    // Trim
    size_t start = result.find_first_not_of(" \t\r\n");
    size_t end = result.find_last_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    return result.substr(start, end - start + 1);
}

// Lowercase a string
inline std::string to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s;
}

} // namespace fincept::utils
