#include "text_preprocessor.h"
#include <regex>
#include <sstream>
#include <algorithm>
#include <map>
#include <cctype>

namespace fincept::voice {

// ============================================================================
// Full pipeline — mirrors Tauri cleanText()
// ============================================================================
std::string TextPreprocessor::clean(const std::string& text) {
    std::string result = text;
    result = convert_formulas(result);
    result = convert_tables(result);
    result = convert_dates(result);
    result = convert_numbers(result);
    result = expand_abbreviations(result);
    result = strip_markdown(result);

    // Normalize whitespace
    std::regex multi_space(R"(\s{2,})");
    result = std::regex_replace(result, multi_space, " ");

    // Trim
    auto start = result.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    auto end = result.find_last_not_of(" \t\n\r");
    return result.substr(start, end - start + 1);
}

// ============================================================================
// LaTeX formulas → spoken math
// ============================================================================
std::string TextPreprocessor::convert_formulas(const std::string& text) {
    std::string result = text;

    // Remove block formulas: \[...\] and $$...$$
    result = std::regex_replace(result, std::regex(R"(\\\[.*?\\\])"), " mathematical expression ");
    result = std::regex_replace(result, std::regex(R"(\$\$.*?\$\$)"), " mathematical expression ");

    // Inline formulas: \(...\) and $...$
    // Simple conversions for common patterns
    static const std::vector<std::pair<std::regex, std::string>> formula_rules = {
        {std::regex(R"(\\frac\{([^}]*)\}\{([^}]*)\})"), "$1 divided by $2"},
        {std::regex(R"(\\sqrt\{([^}]*)\})"),             "square root of $1"},
        {std::regex(R"(\^(\{[^}]*\}|[0-9]))"),           " to the power of $1"},
        {std::regex(R"(\\sum)"),                          "summation of"},
        {std::regex(R"(\\int)"),                          "integral of"},
        {std::regex(R"(\\times)"),                        " times "},
        {std::regex(R"(\\div)"),                          " divided by "},
        {std::regex(R"(\\pm)"),                           " plus or minus "},
        {std::regex(R"(\\leq)"),                          " less than or equal to "},
        {std::regex(R"(\\geq)"),                          " greater than or equal to "},
        {std::regex(R"(\\neq)"),                          " not equal to "},
        {std::regex(R"(\\approx)"),                       " approximately "},
        {std::regex(R"(\\infty)"),                        " infinity "},
    };

    for (auto& [pattern, replacement] : formula_rules) {
        result = std::regex_replace(result, pattern, replacement);
    }

    // Greek letters
    static const std::map<std::string, std::string> greek = {
        {"\\alpha", "alpha"}, {"\\beta", "beta"}, {"\\gamma", "gamma"},
        {"\\delta", "delta"}, {"\\epsilon", "epsilon"}, {"\\theta", "theta"},
        {"\\lambda", "lambda"}, {"\\mu", "mu"}, {"\\pi", "pi"},
        {"\\sigma", "sigma"}, {"\\tau", "tau"}, {"\\phi", "phi"},
        {"\\omega", "omega"}, {"\\rho", "rho"}, {"\\nu", "nu"},
        {"\\chi", "chi"}, {"\\psi", "psi"}, {"\\eta", "eta"},
        {"\\kappa", "kappa"}, {"\\xi", "xi"}, {"\\zeta", "zeta"},
    };
    for (auto& [latex, spoken] : greek) {
        size_t pos = 0;
        while ((pos = result.find(latex, pos)) != std::string::npos) {
            result.replace(pos, latex.size(), spoken);
            pos += spoken.size();
        }
    }

    // Strip remaining $ delimiters
    result = std::regex_replace(result, std::regex(R"(\\\(|\\\)|\$)"), "");

    // Remove leftover braces
    result = std::regex_replace(result, std::regex(R"([{}])"), "");

    return result;
}

// ============================================================================
// Markdown tables → spoken rows
// ============================================================================
std::string TextPreprocessor::convert_tables(const std::string& text) {
    std::string result;
    std::istringstream stream(text);
    std::string line;
    std::vector<std::string> headers;
    bool in_table = false;
    int row_num = 0;

    while (std::getline(stream, line)) {
        // Check if line is a table row (starts with |)
        if (line.find('|') == std::string::npos || line.find('|') > 2) {
            if (in_table) {
                in_table = false;
                headers.clear();
                row_num = 0;
            }
            result += line + "\n";
            continue;
        }

        // Skip separator rows (|---|---|)
        if (line.find("---") != std::string::npos) continue;

        // Parse cells
        std::vector<std::string> cells;
        std::istringstream cell_stream(line);
        std::string cell;
        while (std::getline(cell_stream, cell, '|')) {
            // Trim
            auto s = cell.find_first_not_of(" \t");
            auto e = cell.find_last_not_of(" \t");
            if (s != std::string::npos && e != std::string::npos) {
                cells.push_back(cell.substr(s, e - s + 1));
            }
        }

        if (cells.empty()) {
            result += line + "\n";
            continue;
        }

        if (!in_table) {
            // First row = headers
            headers = cells;
            in_table = true;
            result += "Table with columns: ";
            for (size_t i = 0; i < headers.size(); i++) {
                if (i > 0) result += ", ";
                result += headers[i];
            }
            result += ". ";
        } else {
            // Data row
            row_num++;
            result += "Row " + std::to_string(row_num) + ": ";
            for (size_t i = 0; i < cells.size() && i < headers.size(); i++) {
                if (i > 0) result += ", ";
                result += headers[i] + " is " + cells[i];
            }
            result += ". ";
        }
    }

    return result;
}

// ============================================================================
// Dates → spoken dates
// ============================================================================
std::string TextPreprocessor::convert_dates(const std::string& text) {
    static const char* month_names[] = {
        "", "January", "February", "March", "April", "May", "June",
        "July", "August", "September", "October", "November", "December"
    };

    std::string result = text;

    // ISO dates: 2024-01-15
    std::regex iso_date(R"((\d{4})-(\d{2})-(\d{2}))");
    std::smatch match;
    std::string temp = result;
    result.clear();
    while (std::regex_search(temp, match, iso_date)) {
        result += match.prefix();
        int month = std::stoi(match[2].str());
        int day = std::stoi(match[3].str());
        if (month >= 1 && month <= 12) {
            result += std::string(month_names[month]) + " " +
                      std::to_string(day) + ", " + match[1].str();
        } else {
            result += match[0].str();
        }
        temp = match.suffix();
    }
    result += temp;

    // 24h time: 14:30 → 2:30 PM
    std::regex time_24h(R"((\d{2}):(\d{2})(?::(\d{2}))?)");
    temp = result;
    result.clear();
    while (std::regex_search(temp, match, time_24h)) {
        result += match.prefix();
        int hour = std::stoi(match[1].str());
        int min = std::stoi(match[2].str());
        std::string ampm = hour >= 12 ? "PM" : "AM";
        if (hour > 12) hour -= 12;
        if (hour == 0) hour = 12;
        result += std::to_string(hour) + ":" +
                  (min < 10 ? "0" : "") + std::to_string(min) + " " + ampm;
        temp = match.suffix();
    }
    result += temp;

    return result;
}

// ============================================================================
// Numbers / financial data → spoken
// ============================================================================
std::string TextPreprocessor::convert_numbers(const std::string& text) {
    std::string result = text;

    // Percentages: 50% → 50 percent
    result = std::regex_replace(result, std::regex(R"((\d+\.?\d*)%)"), "$1 percent");

    // Basis points: 100 bps → 100 basis points
    result = std::regex_replace(result,
        std::regex(R"((\d+)\s*bps\b)", std::regex::icase), "$1 basis points");

    // Currency: $1,234,567.89 → spoken
    // Billions
    result = std::regex_replace(result,
        std::regex(R"(\$(\d{1,3}),(\d{3}),(\d{3}),(\d{3}))"),
        "$1.$2 billion dollars");
    // Millions
    result = std::regex_replace(result,
        std::regex(R"(\$(\d{1,3}),(\d{3}),(\d{3}))"),
        "$1.$2 million dollars");
    // Thousands
    result = std::regex_replace(result,
        std::regex(R"(\$(\d{1,3}),(\d{3}))"),
        "$1 thousand $2 dollars");
    // Simple dollar amounts (keep as-is with "dollars")
    result = std::regex_replace(result,
        std::regex(R"(\$(\d+\.?\d*))"), "$1 dollars");

    // Ratios: 3:1 → 3 to 1
    result = std::regex_replace(result, std::regex(R"((\d+):(\d+))"), "$1 to $2");

    return result;
}

// ============================================================================
// Financial abbreviations → expanded
// ============================================================================
std::string TextPreprocessor::expand_abbreviations(const std::string& text) {
    // Use word-boundary matching to avoid partial replacements
    static const std::vector<std::pair<std::string, std::string>> abbrevs = {
        {"P/E",     "price to earnings ratio"},
        {"EPS",     "earnings per share"},
        {"ROI",     "return on investment"},
        {"ROE",     "return on equity"},
        {"ROA",     "return on assets"},
        {"EBITDA",  "earnings before interest, taxes, depreciation and amortization"},
        {"CAGR",    "compound annual growth rate"},
        {"YoY",     "year over year"},
        {"QoQ",     "quarter over quarter"},
        {"MoM",     "month over month"},
        {"IPO",     "initial public offering"},
        {"M&A",     "mergers and acquisitions"},
        {"ETF",     "exchange traded fund"},
        {"GDP",     "gross domestic product"},
        {"CPI",     "consumer price index"},
        {"PPI",     "producer price index"},
        {"FOMC",    "Federal Open Market Committee"},
        {"S&P",     "S and P"},
        {"NASDAQ",  "NASDAQ"},
        {"NYSE",    "New York Stock Exchange"},
    };

    std::string result = text;
    for (auto& [abbr, expanded] : abbrevs) {
        try {
            // Escape special regex characters in abbr
            std::string escaped;
            for (char c : abbr) {
                if (c == '/' || c == '&') escaped += '\\';
                escaped += c;
            }
            std::regex pattern("\\b" + escaped + "\\b");
            result = std::regex_replace(result, pattern, expanded);
        } catch (...) {
            // If regex fails, try simple replacement
            size_t pos = 0;
            while ((pos = result.find(abbr, pos)) != std::string::npos) {
                result.replace(pos, abbr.size(), expanded);
                pos += expanded.size();
            }
        }
    }

    return result;
}

// ============================================================================
// Strip markdown, HTML, code blocks, emojis
// ============================================================================
std::string TextPreprocessor::strip_markdown(const std::string& text) {
    std::string result = text;

    // Code blocks: ```...```
    result = std::regex_replace(result, std::regex(R"(```[\s\S]*?```)"), " code block ");

    // Inline code: `...`
    result = std::regex_replace(result, std::regex(R"(`[^`]+`)"), " code ");

    // Headers: # ## ###
    result = std::regex_replace(result, std::regex(R"(#{1,6}\s+)"), "");

    // Bold/italic: **text** *text* __text__ _text_
    result = std::regex_replace(result, std::regex(R"(\*\*([^*]+)\*\*)"), "$1");
    result = std::regex_replace(result, std::regex(R"(\*([^*]+)\*)"), "$1");
    result = std::regex_replace(result, std::regex(R"(__([^_]+)__)"), "$1");
    result = std::regex_replace(result, std::regex(R"(_([^_]+)_)"), "$1");

    // Links: [text](url)
    result = std::regex_replace(result, std::regex(R"(\[([^\]]+)\]\([^)]+\))"), "$1");

    // HTML tags
    result = std::regex_replace(result, std::regex(R"(<[^>]+>)"), "");

    // Bullet points
    result = std::regex_replace(result, std::regex(R"([\s]*[-*+]\s+)"), " ");

    // Numbered lists
    result = std::regex_replace(result, std::regex(R"(\d+\.\s+)"), " ");

    // Horizontal rules
    result = std::regex_replace(result, std::regex(R"([-*_]{3,})"), " ");

    // Blockquotes
    result = std::regex_replace(result, std::regex(R"(>\s+)"), "");

    return result;
}

} // namespace fincept::voice
