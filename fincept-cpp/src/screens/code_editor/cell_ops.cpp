#include "code_editor_screen.h"
#include "editor_internal.h"
#include <string>
#include <algorithm>

namespace fincept::code_editor {

void CodeEditorScreen::add_cell(int after_idx, const std::string& type) {
    NotebookCell cell;
    cell.id = generate_cell_id();
    cell.cell_type = type;

    auto& cells = nb().cells;
    if (after_idx >= 0 && after_idx < static_cast<int>(cells.size())) {
        cells.insert(cells.begin() + after_idx + 1, std::move(cell));
        tab().selected_cell = after_idx + 1;
    } else {
        cells.push_back(std::move(cell));
        tab().selected_cell = static_cast<int>(cells.size()) - 1;
    }
    tab().unsaved = true;
    active_edit_cell_ = -1;
}

void CodeEditorScreen::delete_cell(int idx) {
    auto& cells = nb().cells;
    if (cells.size() <= 1) return;
    if (idx < 0 || idx >= static_cast<int>(cells.size())) return;

    cells.erase(cells.begin() + idx);
    if (tab().selected_cell >= static_cast<int>(cells.size()))
        tab().selected_cell = static_cast<int>(cells.size()) - 1;
    tab().unsaved = true;
    active_edit_cell_ = -1;
}

void CodeEditorScreen::move_cell(int idx, int direction) {
    int target = idx + direction;
    auto& cells = nb().cells;
    if (target < 0 || target >= static_cast<int>(cells.size())) return;

    std::swap(cells[idx], cells[target]);
    tab().selected_cell = target;
    tab().unsaved = true;
    active_edit_cell_ = -1;
}

void CodeEditorScreen::toggle_cell_type(int idx) {
    auto& cells = nb().cells;
    if (idx < 0 || idx >= static_cast<int>(cells.size())) return;
    auto& cell = cells[idx];
    cell.cell_type = (cell.cell_type == "code") ? "markdown" : "code";
    cell.outputs.clear();
    cell.execution_count = -1;
    cell.exec_time_ms = -1;
    tab().unsaved = true;
}

void CodeEditorScreen::clear_cell_output(int idx) {
    auto& cells = nb().cells;
    if (idx < 0 || idx >= static_cast<int>(cells.size())) return;
    cells[idx].outputs.clear();
    cells[idx].execution_count = -1;
    cells[idx].exec_time_ms = -1;
}

void CodeEditorScreen::clear_all_outputs() {
    for (auto& cell : nb().cells) {
        cell.outputs.clear();
        cell.execution_count = -1;
        cell.exec_time_ms = -1;
    }
    set_status("All outputs cleared");
}

// =============================================================================
// Undo/Redo (point 5)
// =============================================================================
void CodeEditorScreen::push_undo(const std::string& description) {
    flush_edit_buffer();
    UndoSnapshot snap;
    snap.notebook = nb();
    snap.selected_cell = tab().selected_cell;
    snap.description = description;
    tab().undo_stack.push_back(std::move(snap));
    if (static_cast<int>(tab().undo_stack.size()) > NotebookTab::MAX_UNDO)
        tab().undo_stack.pop_front();
    tab().redo_stack.clear();
}

void CodeEditorScreen::undo() {
    if (tab().undo_stack.empty()) return;
    flush_edit_buffer();

    // Save current as redo
    UndoSnapshot redo_snap;
    redo_snap.notebook = nb();
    redo_snap.selected_cell = tab().selected_cell;
    redo_snap.description = "undo";
    tab().redo_stack.push_back(std::move(redo_snap));

    auto& snap = tab().undo_stack.back();
    nb() = std::move(snap.notebook);
    tab().selected_cell = snap.selected_cell;
    tab().undo_stack.pop_back();
    tab().unsaved = true;
    active_edit_cell_ = -1;
    set_status("Undo: " + tab().redo_stack.back().description);
}

void CodeEditorScreen::redo() {
    if (tab().redo_stack.empty()) return;
    flush_edit_buffer();

    UndoSnapshot undo_snap;
    undo_snap.notebook = nb();
    undo_snap.selected_cell = tab().selected_cell;
    undo_snap.description = "redo";
    tab().undo_stack.push_back(std::move(undo_snap));

    auto& snap = tab().redo_stack.back();
    nb() = std::move(snap.notebook);
    tab().selected_cell = snap.selected_cell;
    tab().redo_stack.pop_back();
    tab().unsaved = true;
    active_edit_cell_ = -1;
    set_status("Redo");
}

} // namespace fincept::code_editor
