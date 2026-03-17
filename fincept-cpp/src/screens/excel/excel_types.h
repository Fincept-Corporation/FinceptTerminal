#pragma once
// Excel/Spreadsheet Types — cell model, sheet, formula evaluation.

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace fincept::excel {

// ============================================================================
// Cell value type
// ============================================================================

enum class CellType { empty, text, number, formula, error };

// ============================================================================
// Cell — single spreadsheet cell
// ============================================================================

struct Cell {
    std::string raw;            // what user typed (e.g. "=SUM(A1:A5)")
    std::string display;        // computed display value
    double numeric_value = 0.0; // cached numeric result
    CellType type = CellType::empty;
    bool selected = false;
};

// ============================================================================
// Sheet — a single worksheet tab
// ============================================================================

struct Sheet {
    std::string name = "Sheet1";
    std::vector<std::vector<Cell>> cells;
    int row_count = 100;
    int col_count = 26;         // A-Z

    void ensure_size() {
        cells.resize(row_count);
        for (auto& row : cells) {
            row.resize(col_count);
        }
    }
};

// ============================================================================
// Column label helpers
// ============================================================================

inline std::string col_label(int col) {
    std::string label;
    int c = col;
    while (c >= 0) {
        label = static_cast<char>('A' + (c % 26)) + label;
        c = c / 26 - 1;
    }
    return label;
}

inline int parse_col(const std::string& label) {
    int col = 0;
    for (const char ch : label) {
        if (ch >= 'A' && ch <= 'Z') {
            col = col * 26 + (ch - 'A' + 1);
        } else {
            break;
        }
    }
    return col - 1;
}

inline std::string cell_ref(int row, int col) {
    return col_label(col) + std::to_string(row + 1);
}

// ============================================================================
// Default column widths
// ============================================================================

static constexpr float DEFAULT_COL_WIDTH = 80.0f;
static constexpr float ROW_HEADER_WIDTH  = 40.0f;
static constexpr float ROW_HEIGHT        = 22.0f;
static constexpr float FORMULA_BAR_H     = 28.0f;
static constexpr float SHEET_TAB_H       = 26.0f;
static constexpr int   MAX_SHEETS        = 16;
static constexpr int   VISIBLE_ROWS      = 40;
static constexpr int   VISIBLE_COLS      = 26;

} // namespace fincept::excel
