#include "excel_screen.h"
#include "ui/theme.h"
#include "ui/yoga_helpers.h"
#include "core/logger.h"
#include <imgui.h>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <numeric>
#include <sstream>
#include <cctype>

namespace fincept::excel {

using namespace theme::colors;
static constexpr const char* TAG = "Excel";

// ============================================================================
// Initialization
// ============================================================================

void ExcelScreen::init() {
    LOG_INFO(TAG, "Initializing spreadsheet");

    Sheet s;
    s.name = "Sheet1";
    s.ensure_size();
    sheets_.push_back(std::move(s));

    initialized_ = true;
}

// ============================================================================
// Main render
// ============================================================================

void ExcelScreen::render() {
    if (!initialized_) init();

    ui::ScreenFrame frame("##excel_screen", ImVec2(0, 0), BG_DARKEST);
    if (!frame.begin()) { frame.end(); return; }

    const float w = frame.width();
    const float h = frame.height();

    render_toolbar(w);
    render_formula_bar(w);

    const float grid_h = h - ImGui::GetCursorPosY() - SHEET_TAB_H;
    if (grid_h > 20.0f) {
        render_grid(w, grid_h);
    }
    render_sheet_tabs(w);

    frame.end();
}

// ============================================================================
// Toolbar
// ============================================================================

void ExcelScreen::render_toolbar(float w) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARKEST);
    ImGui::BeginChild("##xl_toolbar", ImVec2(w, 30.0f), false);
    ImGui::SetCursorPos(ImVec2(8.0f, 3.0f));

    // File name
    ImGui::SetNextItemWidth(160.0f);
    if (ImGui::InputText("##xl_name", file_name_, sizeof(file_name_))) {
        is_dirty_ = true;
    }

    ImGui::SameLine(0.0f, 12.0f);
    if (ImGui::SmallButton("New Sheet")) { new_sheet(); }
    ImGui::SameLine();
    if (ImGui::SmallButton("Import CSV")) { import_csv(); }
    ImGui::SameLine();
    if (ImGui::SmallButton("Export CSV")) { export_csv(); }

    ImGui::SameLine(0.0f, 20.0f);

    // Quick formatting
    if (ImGui::SmallButton("SUM")) {
        std::snprintf(formula_buf_, sizeof(formula_buf_), "=SUM(%s:%s)",
                      cell_ref(sel_row_, sel_col_).c_str(),
                      cell_ref(sel_row_ + 5, sel_col_).c_str());
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("AVG")) {
        std::snprintf(formula_buf_, sizeof(formula_buf_), "=AVG(%s:%s)",
                      cell_ref(sel_row_, sel_col_).c_str(),
                      cell_ref(sel_row_ + 5, sel_col_).c_str());
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("COUNT")) {
        std::snprintf(formula_buf_, sizeof(formula_buf_), "=COUNT(%s:%s)",
                      cell_ref(sel_row_, sel_col_).c_str(),
                      cell_ref(sel_row_ + 5, sel_col_).c_str());
    }

    // Status
    ImGui::SameLine(w - 200.0f);
    ImGui::TextColored(TEXT_DIM, "R%d C%d", sel_row_ + 1, sel_col_ + 1);
    ImGui::SameLine();
    if (is_dirty_) {
        ImGui::TextColored(WARNING, "[Modified]");
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Formula Bar
// ============================================================================

void ExcelScreen::render_formula_bar(float w) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARK);
    ImGui::BeginChild("##xl_fbar", ImVec2(w, FORMULA_BAR_H), false);
    ImGui::SetCursorPos(ImVec2(4.0f, 3.0f));

    // Cell reference label
    const std::string ref = cell_ref(sel_row_, sel_col_);
    ImGui::TextColored(ACCENT, "%s", ref.c_str());
    ImGui::SameLine();
    ImGui::TextColored(BORDER, "|");
    ImGui::SameLine();

    // Formula/value input
    ImGui::SetNextItemWidth(w - 120.0f);
    if (ImGui::InputText("##xl_formula", formula_buf_, sizeof(formula_buf_),
            ImGuiInputTextFlags_EnterReturnsTrue)) {
        set_cell(sel_row_, sel_col_, formula_buf_);
        // Move down after enter
        if (sel_row_ + 1 < sheets_[active_sheet_].row_count) {
            sel_row_++;
            const auto& cell = sheets_[active_sheet_].cells[sel_row_][sel_col_];
            std::strncpy(formula_buf_, cell.raw.c_str(), sizeof(formula_buf_) - 1);
        }
    }

    // Update formula bar when selection changes (handled in grid)

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Cell Grid
// ============================================================================

void ExcelScreen::render_grid(float w, float h) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##xl_grid", ImVec2(w, h), false,
                       ImGuiWindowFlags_HorizontalScrollbar);

    auto& sheet = sheets_[active_sheet_];
    sheet.ensure_size();

    const int vis_rows = std::min(VISIBLE_ROWS, sheet.row_count - scroll_row_);
    const int vis_cols = std::min(VISIBLE_COLS, sheet.col_count - scroll_col_);

    // Calculate table flags
    const ImGuiTableFlags flags =
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
        ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY |
        ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingFixedFit;

    if (ImGui::BeginTable("##xl_table", vis_cols + 1, flags, ImVec2(0, h - 4.0f))) {
        // Row header column
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, ROW_HEADER_WIDTH);
        // Data columns
        for (int c = 0; c < vis_cols; ++c) {
            const int col = scroll_col_ + c;
            ImGui::TableSetupColumn(col_label(col).c_str(),
                                    ImGuiTableColumnFlags_WidthFixed, DEFAULT_COL_WIDTH);
        }
        ImGui::TableSetupScrollFreeze(1, 1);
        ImGui::TableHeadersRow();

        for (int r = 0; r < vis_rows; ++r) {
            const int row = scroll_row_ + r;
            ImGui::TableNextRow(ImGuiTableRowFlags_None, ROW_HEIGHT);

            // Row header
            ImGui::TableNextColumn();
            ImGui::TextColored(TEXT_DIM, "%d", row + 1);

            // Data cells
            for (int c = 0; c < vis_cols; ++c) {
                const int col = scroll_col_ + c;
                ImGui::TableNextColumn();

                auto& cell = sheet.cells[row][col];
                const bool is_sel = (row == sel_row_ && col == sel_col_);

                // Cell ID
                char cell_id[32];
                std::snprintf(cell_id, sizeof(cell_id), "##c%d_%d", row, col);

                if (is_sel) {
                    // Editable selected cell
                    ImGui::PushStyleColor(ImGuiCol_FrameBg, ACCENT_BG);
                    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);

                    char buf[512] = {};
                    std::strncpy(buf, cell.raw.c_str(), sizeof(buf) - 1);

                    if (ImGui::InputText(cell_id, buf, sizeof(buf),
                            ImGuiInputTextFlags_EnterReturnsTrue)) {
                        set_cell(row, col, buf);
                        // Move down
                        if (sel_row_ + 1 < sheet.row_count) sel_row_++;
                        const auto& next = sheet.cells[sel_row_][sel_col_];
                        std::strncpy(formula_buf_, next.raw.c_str(), sizeof(formula_buf_) - 1);
                    }
                    ImGui::PopStyleColor();
                } else {
                    // Display cell — click to select
                    const char* display = cell.display.empty() ? "" : cell.display.c_str();

                    // Color based on type
                    ImVec4 col_color = TEXT_SECONDARY;
                    if (cell.type == CellType::number) col_color = TEXT_PRIMARY;
                    else if (cell.type == CellType::formula) col_color = INFO;
                    else if (cell.type == CellType::error) col_color = ERROR_RED;

                    ImGui::TextColored(col_color, "%s", display);

                    // Click detection
                    if (ImGui::IsItemClicked()) {
                        sel_row_ = row;
                        sel_col_ = col;
                        std::strncpy(formula_buf_, cell.raw.c_str(), sizeof(formula_buf_) - 1);
                    }
                }
            }
        }

        ImGui::EndTable();
    }

    // Keyboard navigation
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) {
        if (ImGui::IsKeyPressed(ImGuiKey_Tab)) {
            sel_col_ = std::min(sel_col_ + 1, sheet.col_count - 1);
            std::strncpy(formula_buf_, sheet.cells[sel_row_][sel_col_].raw.c_str(),
                         sizeof(formula_buf_) - 1);
        }
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow) && sel_row_ > 0) {
            sel_row_--;
            std::strncpy(formula_buf_, sheet.cells[sel_row_][sel_col_].raw.c_str(),
                         sizeof(formula_buf_) - 1);
        }
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow) && sel_row_ + 1 < sheet.row_count) {
            sel_row_++;
            std::strncpy(formula_buf_, sheet.cells[sel_row_][sel_col_].raw.c_str(),
                         sizeof(formula_buf_) - 1);
        }
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow) && sel_col_ > 0) {
            sel_col_--;
            std::strncpy(formula_buf_, sheet.cells[sel_row_][sel_col_].raw.c_str(),
                         sizeof(formula_buf_) - 1);
        }
        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow) && sel_col_ + 1 < sheet.col_count) {
            sel_col_++;
            std::strncpy(formula_buf_, sheet.cells[sel_row_][sel_col_].raw.c_str(),
                         sizeof(formula_buf_) - 1);
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Delete)) {
            set_cell(sel_row_, sel_col_, "");
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Sheet Tabs
// ============================================================================

void ExcelScreen::render_sheet_tabs(float w) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARKEST);
    ImGui::BeginChild("##xl_tabs", ImVec2(w, SHEET_TAB_H), false);
    ImGui::SetCursorPos(ImVec2(4.0f, 2.0f));

    for (int i = 0; i < static_cast<int>(sheets_.size()); ++i) {
        const bool active = (i == active_sheet_);
        if (active) ImGui::PushStyleColor(ImGuiCol_Button, ACCENT_DIM);

        char label[64];
        std::snprintf(label, sizeof(label), "%s##sheet_%d", sheets_[i].name.c_str(), i);
        if (ImGui::SmallButton(label)) {
            active_sheet_ = i;
            sel_row_ = 0;
            sel_col_ = 0;
            const auto& cell = sheets_[active_sheet_].cells[0][0];
            std::strncpy(formula_buf_, cell.raw.c_str(), sizeof(formula_buf_) - 1);
        }
        if (active) ImGui::PopStyleColor();
        ImGui::SameLine();
    }

    // Add sheet button
    if (static_cast<int>(sheets_.size()) < MAX_SHEETS) {
        if (ImGui::SmallButton("+")) {
            new_sheet();
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Cell Operations
// ============================================================================

void ExcelScreen::set_cell(int row, int col, const std::string& value) {
    auto& sheet = sheets_[active_sheet_];
    sheet.ensure_size();
    if (row < 0 || row >= sheet.row_count || col < 0 || col >= sheet.col_count) return;

    auto& cell = sheet.cells[row][col];
    cell.raw = value;

    if (value.empty()) {
        cell.type = CellType::empty;
        cell.display.clear();
        cell.numeric_value = 0.0;
    } else if (value[0] == '=') {
        cell.type = CellType::formula;
        evaluate_cell(row, col);
    } else {
        // Try parsing as number
        char* end = nullptr;
        const double num = std::strtod(value.c_str(), &end);
        if (end != value.c_str() && *end == '\0') {
            cell.type = CellType::number;
            cell.numeric_value = num;
            cell.display = value;
        } else {
            cell.type = CellType::text;
            cell.display = value;
            cell.numeric_value = 0.0;
        }
    }

    is_dirty_ = true;
    // Re-evaluate all formulas (simple approach)
    evaluate_all();
}

void ExcelScreen::evaluate_cell(int row, int col) {
    auto& sheet = sheets_[active_sheet_];
    auto& cell = sheet.cells[row][col];
    if (cell.type != CellType::formula) return;

    const std::string formula = cell.raw.substr(1); // strip '='
    try {
        const double result = evaluate_formula(formula, active_sheet_);
        cell.numeric_value = result;
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%.4g", result);
        cell.display = buf;
    } catch (...) {
        cell.type = CellType::error;
        cell.display = "#ERR!";
        cell.numeric_value = 0.0;
    }
}

void ExcelScreen::evaluate_all() {
    auto& sheet = sheets_[active_sheet_];
    for (int r = 0; r < sheet.row_count; ++r) {
        for (int c = 0; c < sheet.col_count; ++c) {
            if (sheet.cells[r][c].type == CellType::formula) {
                evaluate_cell(r, c);
            }
        }
    }
}

// ============================================================================
// Formula Evaluation (SUM, AVG, MIN, MAX, COUNT, basic arithmetic)
// ============================================================================

double ExcelScreen::evaluate_formula(const std::string& formula, int sheet_idx) {
    // Uppercase for matching
    std::string upper = formula;
    for (auto& ch : upper) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));

    // Function: SUM(range)
    auto extract_func = [&](const std::string& fname) -> std::string {
        const auto pos = upper.find(fname + "(");
        if (pos == std::string::npos) return "";
        const auto start = pos + fname.size() + 1;
        const auto end = upper.find(')', start);
        if (end == std::string::npos) return "";
        return formula.substr(start, end - start);
    };

    // SUM
    {
        const std::string arg = extract_func("SUM");
        if (!arg.empty()) {
            int r1, c1, r2, c2;
            if (parse_range(arg, r1, c1, r2, c2)) {
                const auto vals = get_range_values(r1, c1, r2, c2, sheet_idx);
                return std::accumulate(vals.begin(), vals.end(), 0.0);
            }
            return resolve_cell_value(arg, sheet_idx);
        }
    }

    // AVG / AVERAGE
    for (const auto& fn : {"AVERAGE", "AVG"}) {
        const std::string arg = extract_func(fn);
        if (!arg.empty()) {
            int r1, c1, r2, c2;
            if (parse_range(arg, r1, c1, r2, c2)) {
                const auto vals = get_range_values(r1, c1, r2, c2, sheet_idx);
                if (vals.empty()) return 0.0;
                return std::accumulate(vals.begin(), vals.end(), 0.0) /
                       static_cast<double>(vals.size());
            }
        }
    }

    // MIN
    {
        const std::string arg = extract_func("MIN");
        if (!arg.empty()) {
            int r1, c1, r2, c2;
            if (parse_range(arg, r1, c1, r2, c2)) {
                const auto vals = get_range_values(r1, c1, r2, c2, sheet_idx);
                if (vals.empty()) return 0.0;
                return *std::min_element(vals.begin(), vals.end());
            }
        }
    }

    // MAX
    {
        const std::string arg = extract_func("MAX");
        if (!arg.empty()) {
            int r1, c1, r2, c2;
            if (parse_range(arg, r1, c1, r2, c2)) {
                const auto vals = get_range_values(r1, c1, r2, c2, sheet_idx);
                if (vals.empty()) return 0.0;
                return *std::max_element(vals.begin(), vals.end());
            }
        }
    }

    // COUNT
    {
        const std::string arg = extract_func("COUNT");
        if (!arg.empty()) {
            int r1, c1, r2, c2;
            if (parse_range(arg, r1, c1, r2, c2)) {
                const auto vals = get_range_values(r1, c1, r2, c2, sheet_idx);
                return static_cast<double>(vals.size());
            }
        }
    }

    // Simple cell reference (e.g., "A1")
    if (std::isalpha(static_cast<unsigned char>(formula[0]))) {
        return resolve_cell_value(formula, sheet_idx);
    }

    // Simple numeric expression — try strtod
    char* end = nullptr;
    const double val = std::strtod(formula.c_str(), &end);
    if (end != formula.c_str()) return val;

    return 0.0;
}

double ExcelScreen::resolve_cell_value(const std::string& ref, int sheet_idx) {
    if (sheet_idx < 0 || sheet_idx >= static_cast<int>(sheets_.size())) return 0.0;
    auto& sheet = sheets_[sheet_idx];

    // Parse "A1" style reference
    std::string col_str;
    std::string row_str;
    for (const char ch : ref) {
        if (std::isalpha(static_cast<unsigned char>(ch))) {
            col_str += static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
        } else if (std::isdigit(static_cast<unsigned char>(ch))) {
            row_str += ch;
        }
    }
    if (col_str.empty() || row_str.empty()) return 0.0;

    const int col = parse_col(col_str);
    const int row = std::atoi(row_str.c_str()) - 1;

    if (row < 0 || row >= sheet.row_count || col < 0 || col >= sheet.col_count) return 0.0;

    return sheet.cells[row][col].numeric_value;
}

bool ExcelScreen::parse_range(const std::string& range, int& r1, int& c1, int& r2, int& c2) {
    const auto colon = range.find(':');
    if (colon == std::string::npos) return false;

    const std::string from = range.substr(0, colon);
    const std::string to = range.substr(colon + 1);

    // Parse "from" cell
    std::string from_col, from_row;
    for (const char ch : from) {
        if (std::isalpha(static_cast<unsigned char>(ch)))
            from_col += static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
        else if (std::isdigit(static_cast<unsigned char>(ch)))
            from_row += ch;
    }

    std::string to_col, to_row;
    for (const char ch : to) {
        if (std::isalpha(static_cast<unsigned char>(ch)))
            to_col += static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
        else if (std::isdigit(static_cast<unsigned char>(ch)))
            to_row += ch;
    }

    if (from_col.empty() || from_row.empty() || to_col.empty() || to_row.empty()) return false;

    c1 = parse_col(from_col);
    r1 = std::atoi(from_row.c_str()) - 1;
    c2 = parse_col(to_col);
    r2 = std::atoi(to_row.c_str()) - 1;

    return true;
}

std::vector<double> ExcelScreen::get_range_values(int r1, int c1, int r2, int c2, int sheet_idx) {
    std::vector<double> vals;
    if (sheet_idx < 0 || sheet_idx >= static_cast<int>(sheets_.size())) return vals;
    auto& sheet = sheets_[sheet_idx];

    const int min_r = std::min(r1, r2);
    const int max_r = std::max(r1, r2);
    const int min_c = std::min(c1, c2);
    const int max_c = std::max(c1, c2);

    for (int r = min_r; r <= max_r && r < sheet.row_count; ++r) {
        for (int c = min_c; c <= max_c && c < sheet.col_count; ++c) {
            const auto& cell = sheet.cells[r][c];
            if (cell.type == CellType::number || cell.type == CellType::formula) {
                vals.push_back(cell.numeric_value);
            }
        }
    }
    return vals;
}

// ============================================================================
// File Operations
// ============================================================================

void ExcelScreen::new_sheet() {
    if (static_cast<int>(sheets_.size()) >= MAX_SHEETS) return;

    Sheet s;
    s.name = "Sheet" + std::to_string(sheets_.size() + 1);
    s.ensure_size();
    sheets_.push_back(std::move(s));
    active_sheet_ = static_cast<int>(sheets_.size()) - 1;
    sel_row_ = 0;
    sel_col_ = 0;
    formula_buf_[0] = '\0';

    LOG_INFO(TAG, "Added sheet: %s", sheets_[active_sheet_].name.c_str());
}

void ExcelScreen::export_csv() {
    auto& sheet = sheets_[active_sheet_];
    std::ostringstream oss;

    // Find used range
    int max_row = 0;
    int max_col = 0;
    for (int r = 0; r < sheet.row_count; ++r) {
        for (int c = 0; c < sheet.col_count; ++c) {
            if (sheet.cells[r][c].type != CellType::empty) {
                max_row = std::max(max_row, r);
                max_col = std::max(max_col, c);
            }
        }
    }

    for (int r = 0; r <= max_row; ++r) {
        for (int c = 0; c <= max_col; ++c) {
            if (c > 0) oss << ',';
            const auto& cell = sheet.cells[r][c];
            // Quote if contains comma or newline
            if (cell.display.find(',') != std::string::npos) {
                oss << '"' << cell.display << '"';
            } else {
                oss << cell.display;
            }
        }
        oss << '\n';
    }

    LOG_INFO(TAG, "Exported CSV: %d rows, %d cols", max_row + 1, max_col + 1);
    // In a full implementation, write to file via dialog
}

void ExcelScreen::import_csv() {
    // Placeholder — would open file dialog and parse CSV
    LOG_INFO(TAG, "Import CSV requested");
}

} // namespace fincept::excel
