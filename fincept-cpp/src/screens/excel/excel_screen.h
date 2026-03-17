#pragma once
// Excel/Spreadsheet Screen — terminal-style spreadsheet with formulas.
// Layout: toolbar | formula bar | cell grid | sheet tabs

#include "excel_types.h"
#include <string>
#include <vector>

namespace fincept::excel {

class ExcelScreen {
public:
    void render();

private:
    void init();

    // Panel renderers
    void render_toolbar(float w);
    void render_formula_bar(float w);
    void render_grid(float w, float h);
    void render_sheet_tabs(float w);

    // Cell operations
    void set_cell(int row, int col, const std::string& value);
    void evaluate_cell(int row, int col);
    void evaluate_all();
    double evaluate_formula(const std::string& formula, int sheet_idx);

    // Formula helpers
    double resolve_cell_value(const std::string& ref, int sheet_idx);
    bool parse_range(const std::string& range, int& r1, int& c1, int& r2, int& c2);
    std::vector<double> get_range_values(int r1, int c1, int r2, int c2, int sheet_idx);

    // File operations
    void export_csv();
    void import_csv();
    void new_sheet();

    // State
    bool initialized_ = false;

    // Sheets
    std::vector<Sheet> sheets_;
    int active_sheet_ = 0;

    // Selection
    int sel_row_ = 0;
    int sel_col_ = 0;
    int sel_row_end_ = -1;    // for range selection
    int sel_col_end_ = -1;

    // Scroll
    int scroll_row_ = 0;
    int scroll_col_ = 0;

    // Formula bar editing
    char formula_buf_[4096] = {};
    bool editing_formula_ = false;

    // File state
    char file_name_[128] = "Untitled.xlsx";
    bool is_dirty_ = false;
};

} // namespace fincept::excel
