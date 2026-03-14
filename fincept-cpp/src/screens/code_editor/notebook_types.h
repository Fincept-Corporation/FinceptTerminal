#pragma once
// Notebook data model — Jupyter-compatible cell/output structures
// Extended with execution timing, undo history, search, multi-tab support

#include <string>
#include <vector>
#include <deque>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <cstdio>

namespace fincept::code_editor {

struct CellOutput {
    std::string output_type; // "stream", "error", "execute_result", "display_data"
    std::string text;        // joined text for stream/result
    std::string ename;       // error name (for error type)
    std::string evalue;      // error value
    std::string traceback;   // joined traceback lines
};

struct NotebookCell {
    std::string id;
    std::string cell_type;   // "code" or "markdown"
    std::string source;      // full cell content (joined lines)
    std::vector<CellOutput> outputs;
    int execution_count = -1; // -1 = not executed
    double exec_time_ms = -1; // execution duration in ms, -1 = not timed
};

struct Notebook {
    std::vector<NotebookCell> cells;
    std::string kernel_name = "python3";
    std::string language = "python";
    int nbformat = 4;
    int nbformat_minor = 5;
};

// Generate a simple unique cell ID
inline std::string generate_cell_id() {
    static int counter = 0;
    char buf[32];
    std::snprintf(buf, sizeof(buf), "cell_%d_%d", static_cast<int>(std::time(nullptr)), ++counter);
    return buf;
}

// =============================================================================
// Undo/Redo — snapshot-based
// =============================================================================
struct UndoSnapshot {
    Notebook notebook;
    int selected_cell;
    std::string description; // e.g. "delete cell", "move cell", "edit cell"
};

// =============================================================================
// Search state
// =============================================================================
struct SearchMatch {
    int cell_idx;
    int char_offset; // offset into cell.source
    int length;
};

// =============================================================================
// Notebook tab (multi-tab support)
// =============================================================================
struct NotebookTab {
    Notebook notebook;
    std::string file_path;
    std::string name;
    bool unsaved = false;
    int selected_cell = 0;
    int execution_counter = 1;

    // Undo/Redo stacks
    std::deque<UndoSnapshot> undo_stack;
    std::deque<UndoSnapshot> redo_stack;
    static constexpr int MAX_UNDO = 50;
};

// =============================================================================
// Syntax highlighting token types
// =============================================================================
enum class TokenType {
    Default,
    Keyword,
    Builtin,
    String,
    Comment,
    Number,
    Decorator,
    Operator,
    Function,
    ClassName,
};

struct SyntaxToken {
    int start;
    int length;
    TokenType type;
};

} // namespace fincept::code_editor
