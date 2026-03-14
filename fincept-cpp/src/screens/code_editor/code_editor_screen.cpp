#include "code_editor_screen.h"
#include "notebook_io.h"
#include "ui/theme.h"
#include "core/config.h"
#include "python/python_runner.h"
#include <imgui.h>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <thread>
#include <chrono>
#include <cctype>
#include <sstream>
#include <set>
#include <regex>

#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#endif

namespace fincept::code_editor {

namespace fs = std::filesystem;

// =============================================================================
// Convenience accessors
// =============================================================================
NotebookTab& CodeEditorScreen::tab() { return tabs_[active_tab_idx_]; }
Notebook& CodeEditorScreen::nb() { return tabs_[active_tab_idx_].notebook; }

// =============================================================================
// Helper: write string to temp file, return path
// =============================================================================
static std::string write_temp_script(const std::string& source) {
    fs::path tmp_dir = fs::temp_directory_path();
    fs::path tmp_file = tmp_dir / "fincept_cell.py";
    std::ofstream f(tmp_file);
    if (f.is_open()) { f << source; f.close(); }
    return tmp_file.string();
}

// =============================================================================
// Native File Dialogs (point 11)
// =============================================================================
#ifdef _WIN32
std::string CodeEditorScreen::native_open_dialog(const char* filter, const char* title) {
    char filename[MAX_PATH] = {};
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = title;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameA(&ofn)) return std::string(filename);
    return "";
}

std::string CodeEditorScreen::native_save_dialog(const char* filter, const char* title) {
    char filename[MAX_PATH] = {};
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = title;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
    if (GetSaveFileNameA(&ofn)) return std::string(filename);
    return "";
}
#else
std::string CodeEditorScreen::native_open_dialog(const char*, const char*) { return ""; }
std::string CodeEditorScreen::native_save_dialog(const char*, const char*) { return ""; }
#endif

// =============================================================================
// Python syntax highlighting (point 1)
// =============================================================================
static const std::set<std::string> PY_KEYWORDS = {
    "False", "None", "True", "and", "as", "assert", "async", "await",
    "break", "class", "continue", "def", "del", "elif", "else", "except",
    "finally", "for", "from", "global", "if", "import", "in", "is",
    "lambda", "nonlocal", "not", "or", "pass", "raise", "return",
    "try", "while", "with", "yield"
};

static const std::set<std::string> PY_BUILTINS = {
    "abs", "all", "any", "bin", "bool", "bytes", "callable", "chr",
    "classmethod", "compile", "complex", "delattr", "dict", "dir",
    "divmod", "enumerate", "eval", "exec", "filter", "float", "format",
    "frozenset", "getattr", "globals", "hasattr", "hash", "help", "hex",
    "id", "input", "int", "isinstance", "issubclass", "iter", "len",
    "list", "locals", "map", "max", "memoryview", "min", "next",
    "object", "oct", "open", "ord", "pow", "print", "property",
    "range", "repr", "reversed", "round", "set", "setattr", "slice",
    "sorted", "staticmethod", "str", "sum", "super", "tuple", "type",
    "vars", "zip", "Exception", "ValueError", "TypeError", "KeyError",
    "IndexError", "RuntimeError", "StopIteration", "AttributeError",
    "ImportError", "FileNotFoundError", "OSError", "ZeroDivisionError",
    "NotImplementedError", "OverflowError", "NameError", "SyntaxError",
    "self", "cls"
};

std::vector<SyntaxToken> CodeEditorScreen::tokenize_python(const std::string& src) {
    std::vector<SyntaxToken> tokens;
    int i = 0, n = static_cast<int>(src.size());

    while (i < n) {
        char c = src[i];

        // Comments
        if (c == '#') {
            int start = i;
            while (i < n && src[i] != '\n') i++;
            tokens.push_back({start, i - start, TokenType::Comment});
            continue;
        }

        // Strings (single/double, triple quotes)
        if (c == '\'' || c == '"') {
            int start = i;
            char q = c;
            bool triple = (i + 2 < n && src[i+1] == q && src[i+2] == q);
            if (triple) {
                i += 3;
                while (i < n) {
                    if (src[i] == q && i + 2 < n && src[i+1] == q && src[i+2] == q) {
                        i += 3; break;
                    }
                    if (src[i] == '\\' && i + 1 < n) i++;
                    i++;
                }
            } else {
                i++;
                while (i < n && src[i] != q && src[i] != '\n') {
                    if (src[i] == '\\' && i + 1 < n) i++;
                    i++;
                }
                if (i < n && src[i] == q) i++;
            }
            tokens.push_back({start, i - start, TokenType::String});
            continue;
        }

        // Decorators
        if (c == '@' && (i == 0 || src[i-1] == '\n' || (i > 0 && std::isspace(static_cast<unsigned char>(src[i-1]))))) {
            int start = i;
            i++;
            while (i < n && (std::isalnum(static_cast<unsigned char>(src[i])) || src[i] == '_' || src[i] == '.')) i++;
            tokens.push_back({start, i - start, TokenType::Decorator});
            continue;
        }

        // Numbers
        if (std::isdigit(static_cast<unsigned char>(c)) ||
            (c == '.' && i + 1 < n && std::isdigit(static_cast<unsigned char>(src[i+1])))) {
            int start = i;
            if (c == '0' && i + 1 < n && (src[i+1] == 'x' || src[i+1] == 'X' || src[i+1] == 'b' || src[i+1] == 'B' || src[i+1] == 'o' || src[i+1] == 'O')) {
                i += 2;
                while (i < n && (std::isxdigit(static_cast<unsigned char>(src[i])) || src[i] == '_')) i++;
            } else {
                while (i < n && (std::isdigit(static_cast<unsigned char>(src[i])) || src[i] == '.' || src[i] == '_' || src[i] == 'e' || src[i] == 'E' || src[i] == 'j' || src[i] == 'J')) {
                    if ((src[i] == 'e' || src[i] == 'E') && i + 1 < n && (src[i+1] == '+' || src[i+1] == '-')) i++;
                    i++;
                }
            }
            tokens.push_back({start, i - start, TokenType::Number});
            continue;
        }

        // Identifiers / keywords
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            int start = i;
            while (i < n && (std::isalnum(static_cast<unsigned char>(src[i])) || src[i] == '_')) i++;
            std::string word = src.substr(start, i - start);

            // Check if followed by ( for function/class detection
            bool followed_by_paren = false;
            int j = i;
            while (j < n && std::isspace(static_cast<unsigned char>(src[j]))) j++;
            if (j < n && src[j] == '(') followed_by_paren = true;

            if (PY_KEYWORDS.count(word)) {
                // "class" keyword followed by name
                if (word == "class" || word == "def") {
                    tokens.push_back({start, i - start, TokenType::Keyword});
                    // Skip whitespace and grab the name
                    while (i < n && std::isspace(static_cast<unsigned char>(src[i]))) i++;
                    int name_start = i;
                    while (i < n && (std::isalnum(static_cast<unsigned char>(src[i])) || src[i] == '_')) i++;
                    if (i > name_start) {
                        tokens.push_back({name_start, i - name_start,
                            word == "class" ? TokenType::ClassName : TokenType::Function});
                    }
                } else {
                    tokens.push_back({start, i - start, TokenType::Keyword});
                }
            } else if (PY_BUILTINS.count(word)) {
                tokens.push_back({start, i - start, TokenType::Builtin});
            } else if (followed_by_paren) {
                tokens.push_back({start, i - start, TokenType::Function});
            } else {
                tokens.push_back({start, i - start, TokenType::Default});
            }
            continue;
        }

        // Operators
        if (c == '+' || c == '-' || c == '*' || c == '/' || c == '%' ||
            c == '=' || c == '<' || c == '>' || c == '!' || c == '&' ||
            c == '|' || c == '^' || c == '~') {
            tokens.push_back({i, 1, TokenType::Operator});
            i++;
            continue;
        }

        // Everything else (whitespace, parens, brackets, etc.)
        i++;
    }

    return tokens;
}

static ImVec4 token_color(TokenType type) {
    using namespace fincept::theme::colors;
    switch (type) {
        case TokenType::Keyword:   return ImVec4(0.78f, 0.45f, 0.85f, 1.0f); // purple
        case TokenType::Builtin:   return ImVec4(0.40f, 0.75f, 0.95f, 1.0f); // cyan
        case TokenType::String:    return ImVec4(0.60f, 0.85f, 0.45f, 1.0f); // green
        case TokenType::Comment:   return ImVec4(0.45f, 0.50f, 0.45f, 1.0f); // dim green
        case TokenType::Number:    return ImVec4(0.85f, 0.70f, 0.35f, 1.0f); // gold
        case TokenType::Decorator: return ImVec4(0.95f, 0.75f, 0.30f, 1.0f); // yellow
        case TokenType::Operator:  return ImVec4(0.85f, 0.45f, 0.40f, 1.0f); // red-ish
        case TokenType::Function:  return ImVec4(0.40f, 0.70f, 0.95f, 1.0f); // blue
        case TokenType::ClassName: return ImVec4(0.30f, 0.85f, 0.70f, 1.0f); // teal
        default:                   return TEXT_PRIMARY;
    }
}

void CodeEditorScreen::render_highlighted_text(const std::string& source, float wrap_width) {
    if (source.empty()) return;

    auto tokens = tokenize_python(source);

    // Sort tokens by start position
    std::sort(tokens.begin(), tokens.end(), [](const SyntaxToken& a, const SyntaxToken& b) {
        return a.start < b.start;
    });

    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList* draw = ImGui::GetWindowDrawList();
    float x = pos.x;
    float y = pos.y;
    float line_h = ImGui::GetTextLineHeight();
    float start_x = pos.x;

    int tok_idx = 0;
    int n = static_cast<int>(source.size());

    for (int i = 0; i < n; ) {
        // Determine color for this character
        ImVec4 col = theme::colors::TEXT_PRIMARY;

        // Check if we're inside a token
        while (tok_idx < static_cast<int>(tokens.size()) && tokens[tok_idx].start + tokens[tok_idx].length <= i)
            tok_idx++;

        if (tok_idx < static_cast<int>(tokens.size()) && tokens[tok_idx].start <= i) {
            col = token_color(tokens[tok_idx].type);
        }

        if (source[i] == '\n') {
            x = start_x;
            y += line_h;
            i++;
            continue;
        }

        // Gather consecutive chars of same color on same line
        int run_start = i;
        int cur_tok = tok_idx;
        while (i < n && source[i] != '\n') {
            // Check token boundary
            int new_tok = cur_tok;
            while (new_tok < static_cast<int>(tokens.size()) && tokens[new_tok].start + tokens[new_tok].length <= i)
                new_tok++;

            ImVec4 new_col = theme::colors::TEXT_PRIMARY;
            if (new_tok < static_cast<int>(tokens.size()) && tokens[new_tok].start <= i) {
                new_col = token_color(tokens[new_tok].type);
            }

            if (new_col.x != col.x || new_col.y != col.y || new_col.z != col.z) break;

            cur_tok = new_tok;
            i++;

            // Wrap check
            if (wrap_width > 0 && (x - start_x) > wrap_width) {
                break;
            }
        }

        // Draw the run
        std::string run = source.substr(run_start, i - run_start);
        ImU32 col32 = ImGui::ColorConvertFloat4ToU32(col);
        draw->AddText(ImVec2(x, y), col32, run.c_str());
        x += ImGui::CalcTextSize(run.c_str()).x;
    }

    // Advance ImGui cursor past the rendered text
    float total_h = y - pos.y + line_h;
    ImGui::Dummy(ImVec2(wrap_width > 0 ? wrap_width : (x - start_x), total_h));
}

// =============================================================================
// Markdown rendering (point 6)
// =============================================================================
void CodeEditorScreen::render_markdown(const std::string& source) {
    using namespace theme::colors;

    std::istringstream iss(source);
    std::string line;

    while (std::getline(iss, line)) {
        // Trim trailing \r
        if (!line.empty() && line.back() == '\r') line.pop_back();

        if (line.empty()) {
            ImGui::Spacing();
            continue;
        }

        // Headings
        if (line.starts_with("### ")) {
            ImGui::TextColored(ACCENT, "%s", line.c_str() + 4);
        } else if (line.starts_with("## ")) {
            ImGui::PushStyleColor(ImGuiCol_Text, ACCENT);
            float scale = 1.15f;
            ImGui::SetWindowFontScale(scale);
            ImGui::TextWrapped("%s", line.c_str() + 3);
            ImGui::SetWindowFontScale(1.0f);
            ImGui::PopStyleColor();
        } else if (line.starts_with("# ")) {
            ImGui::PushStyleColor(ImGuiCol_Text, ACCENT);
            float scale = 1.3f;
            ImGui::SetWindowFontScale(scale);
            ImGui::TextWrapped("%s", line.c_str() + 2);
            ImGui::SetWindowFontScale(1.0f);
            ImGui::PopStyleColor();
        }
        // Horizontal rule
        else if (line == "---" || line == "***" || line == "___") {
            ImGui::Separator();
        }
        // Blockquote
        else if (line.starts_with("> ")) {
            ImVec2 p = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddRectFilled(
                p, ImVec2(p.x + 3, p.y + ImGui::GetTextLineHeight()),
                ImGui::ColorConvertFloat4ToU32(ACCENT_DIM));
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 12);
            ImGui::TextColored(TEXT_SECONDARY, "%s", line.c_str() + 2);
        }
        // Bullet points
        else if (line.starts_with("- ") || line.starts_with("* ")) {
            ImGui::TextColored(ACCENT, "  *");
            ImGui::SameLine(0, 4);
            ImGui::TextWrapped("%s", line.c_str() + 2);
        }
        // Bold: **text**
        else if (line.find("**") != std::string::npos) {
            // Simple bold rendering — render whole line with emphasis
            ImGui::TextColored(TEXT_PRIMARY, "%s", line.c_str());
        }
        // Code span: `code`
        else if (line.find('`') != std::string::npos) {
            ImGui::TextColored(ImVec4(0.60f, 0.85f, 0.45f, 1.0f), "%s", line.c_str());
        }
        // Default
        else {
            ImGui::TextWrapped("%s", line.c_str());
        }
    }
}

// =============================================================================
// Subprocess execution helper
// =============================================================================
static python::PythonResult run_cell_subprocess(const std::string& tmp_path) {
    python::PythonResult result;

    fs::path py = python::resolve_python_path("");
    if (py.empty() || !fs::exists(py)) {
#ifdef _WIN32
        py = "python";
#else
        py = "python3";
#endif
    }

    std::string cmd = "\"" + py.string() + "\" -u -B \"" + tmp_path + "\" 2>&1";

#ifdef _WIN32
    FILE* pipe = _popen(cmd.c_str(), "r");
#else
    FILE* pipe = popen(cmd.c_str(), "r");
#endif
    if (!pipe) {
        result.error = "Failed to start Python subprocess";
        return result;
    }

    std::string output;
    char buffer[4096];
    while (std::fgets(buffer, sizeof(buffer), pipe)) {
        output += buffer;
    }

#ifdef _WIN32
    int status = _pclose(pipe);
#else
    int status = pclose(pipe);
    status = WEXITSTATUS(status);
#endif

    result.exit_code = status;
    if (status == 0) {
        result.success = true;
        result.output = output;
    } else {
        result.error = output.empty() ? "Python exited with code " + std::to_string(status) : output;
    }
    return result;
}

// =============================================================================
// Main render
// =============================================================================
void CodeEditorScreen::render() {
    using namespace theme::colors;

    if (!initialized_) {
        new_tab();
        detect_python_version();
        last_autosave_time_ = ImGui::GetTime();
        initialized_ = true;
    }

    if (tabs_.empty()) { new_tab(); }

    // Check async execution result
    if (exec_future_.valid() &&
        exec_future_.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
        exec_future_.get();
        exec_pending_ = false;
        running_cell_ = -1;
    }

    // Apply pending results from background thread (thread-safe)
    {
        std::lock_guard<std::mutex> lock(result_mutex_);
        for (auto& pr : pending_results_) {
            if (pr.tab_idx >= 0 && pr.tab_idx < static_cast<int>(tabs_.size())) {
                auto& t = tabs_[pr.tab_idx];
                if (pr.cell_idx >= 0 && pr.cell_idx < static_cast<int>(t.notebook.cells.size())) {
                    auto& c = t.notebook.cells[pr.cell_idx];
                    c.outputs.clear();
                    c.execution_count = pr.exec_num;
                    c.exec_time_ms = pr.elapsed_ms;
                    if (pr.success) {
                        if (!pr.output.empty()) {
                            CellOutput out;
                            out.output_type = "stream";
                            out.text = pr.output;
                            c.outputs.push_back(std::move(out));
                        }
                    } else {
                        CellOutput out;
                        out.output_type = "error";
                        out.ename = "ExecutionError";
                        out.evalue = pr.error.empty() ? "Unknown error" : pr.error;
                        out.traceback = pr.error;
                        c.outputs.push_back(std::move(out));
                    }
                }
            }
        }
        pending_results_.clear();
    }

    // Auto-save (point 10)
    double now = ImGui::GetTime();
    if (now - last_autosave_time_ > AUTOSAVE_INTERVAL) {
        auto_save();
        last_autosave_time_ = now;
    }

    // Clear stale status messages
    if (!status_message_.empty() && now - status_message_time_ > 5.0) {
        status_message_.clear();
    }

    // Handle keyboard navigation (point 17)
    handle_keyboard_navigation();

    ImGuiViewport* vp = ImGui::GetMainViewport();
    float tab_h = ImGui::GetFrameHeight() + 4;
    float footer_h = ImGui::GetFrameHeight();

    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x, vp->WorkPos.y + tab_h));
    ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x, vp->WorkSize.y - tab_h - footer_h));

    ImGui::PushStyleColor(ImGuiCol_WindowBg, BG_DARK);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

    ImGui::Begin("##code_editor_screen", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoBringToFrontOnFocus);

    render_tab_bar();
    render_toolbar();
    if (show_search_) render_search_bar();
    render_cells();
    render_status_bar();
    render_file_dialog();
    render_pip_dialog();

    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

// =============================================================================
// Multi-tab bar (point 3)
// =============================================================================
void CodeEditorScreen::render_tab_bar() {
    using namespace theme::colors;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARKEST);
    ImGui::BeginChild("##ce_tabbar", ImVec2(0, 28), ImGuiChildFlags_Borders);

    ImGui::SetCursorPos(ImVec2(4, 3));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 3));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 0));

    int close_idx = -1;
    for (int i = 0; i < static_cast<int>(tabs_.size()); i++) {
        bool is_active = (i == active_tab_idx_);
        auto& t = tabs_[i];

        std::string label = t.name;
        if (t.unsaved) label += " *";
        label += "##ntab_" + std::to_string(i);

        if (is_active) {
            ImGui::PushStyleColor(ImGuiCol_Button, ACCENT_BG);
            ImGui::PushStyleColor(ImGuiCol_Text, ACCENT);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
            ImGui::PushStyleColor(ImGuiCol_Text, TEXT_SECONDARY);
        }

        if (ImGui::SmallButton(label.c_str())) {
            flush_edit_buffer();
            active_tab_idx_ = i;
            active_edit_cell_ = -1;
        }
        ImGui::PopStyleColor(2);

        // Close button
        ImGui::SameLine(0, 0);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
        ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DIM);
        char close_id[32];
        std::snprintf(close_id, sizeof(close_id), "x##close_%d", i);
        if (ImGui::SmallButton(close_id)) {
            close_idx = i;
        }
        ImGui::PopStyleColor(2);
        ImGui::SameLine(0, 4);
    }

    // "+" button for new tab
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_Text, ACCENT);
    if (ImGui::SmallButton("+##new_tab")) {
        new_tab();
    }
    ImGui::PopStyleColor(2);

    ImGui::PopStyleVar(2);
    ImGui::EndChild();
    ImGui::PopStyleColor();

    if (close_idx >= 0) close_tab(close_idx);
}

// =============================================================================
// Toolbar
// =============================================================================
void CodeEditorScreen::render_toolbar() {
    using namespace theme::colors;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARKEST);
    ImGui::BeginChild("##ce_toolbar", ImVec2(0, 42), ImGuiChildFlags_Borders);
    ImGui::SetCursorPos(ImVec2(12, 7));

    // File actions
    if (theme::SecondaryButton("NEW")) { new_tab(); }
    ImGui::SameLine(0, 4);

    if (theme::SecondaryButton("OPEN")) { open_notebook_dialog(); }
    ImGui::SameLine(0, 4);

    // Save button — yellow when unsaved
    if (tab().unsaved) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.17f, 0.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.25f, 0.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, WARNING);
    }
    if (ImGui::Button("SAVE")) { save_current(); }
    if (tab().unsaved) { ImGui::PopStyleColor(3); }

    ImGui::SameLine(0, 4);

    // Export to .py (point 16)
    if (theme::SecondaryButton("EXPORT .PY")) {
        std::string path = native_save_dialog(
            "Python Files (*.py)\0*.py\0All Files\0*.*\0",
            "Export as Python");
        if (path.empty()) {
            show_export_dialog_ = true;
            std::memset(path_buf_, 0, sizeof(path_buf_));
        } else {
            export_to_py(path);
        }
    }

    ImGui::SameLine(0, 12);
    ImGui::TextColored(BORDER_BRIGHT, "|");
    ImGui::SameLine(0, 12);

    // Run All
    if (running_all_) {
        ImGui::PushStyleColor(ImGuiCol_Button, BG_WIDGET);
        ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DISABLED);
        ImGui::Button("RUNNING...");
        ImGui::PopStyleColor(2);
    } else {
        if (theme::AccentButton("RUN ALL")) { run_all_cells(); }
    }
    ImGui::SameLine(0, 4);

    if (running_all_ || exec_pending_) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.05f, 0.05f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.1f, 0.1f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ERROR_RED);
        if (ImGui::Button("STOP")) { interrupt_execution(); }
        ImGui::PopStyleColor(3);
        ImGui::SameLine(0, 4);
    }

    // Clear all outputs
    if (theme::SecondaryButton("CLEAR ALL")) { clear_all_outputs(); }

    ImGui::SameLine(0, 12);
    ImGui::TextColored(BORDER_BRIGHT, "|");
    ImGui::SameLine(0, 12);

    // Undo/Redo (point 5)
    bool can_undo = !tab().undo_stack.empty();
    bool can_redo = !tab().redo_stack.empty();

    if (!can_undo) ImGui::BeginDisabled();
    if (theme::SecondaryButton("UNDO")) { undo(); }
    if (!can_undo) ImGui::EndDisabled();
    ImGui::SameLine(0, 4);

    if (!can_redo) ImGui::BeginDisabled();
    if (theme::SecondaryButton("REDO")) { redo(); }
    if (!can_redo) ImGui::EndDisabled();

    ImGui::SameLine(0, 12);
    ImGui::TextColored(BORDER_BRIGHT, "|");
    ImGui::SameLine(0, 12);

    // Find (point 13)
    if (theme::SecondaryButton("FIND")) { show_search_ = !show_search_; }
    ImGui::SameLine(0, 4);

    // Pip install (point 14)
    if (theme::SecondaryButton("PIP")) {
        show_pip_dialog_ = true;
        std::memset(pip_pkg_buf_, 0, sizeof(pip_pkg_buf_));
    }

    // Right side: filename
    float right_w = 300.0f;
    float avail = ImGui::GetContentRegionAvail().x;
    if (avail > right_w + 50) {
        ImGui::SameLine(ImGui::GetWindowWidth() - right_w);
        ImGui::TextColored(ACCENT, "[>]");
        ImGui::SameLine(0, 4);
        std::string display_name = tab().file_path.empty() ?
            (tab().name + ".ipynb") :
            fs::path(tab().file_path).filename().string();
        ImGui::TextColored(TEXT_PRIMARY, "%s", display_name.c_str());
        if (tab().unsaved) {
            ImGui::SameLine(0, 8);
            ImGui::TextColored(WARNING, "MODIFIED");
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// =============================================================================
// Search bar (point 13)
// =============================================================================
void CodeEditorScreen::render_search_bar() {
    using namespace theme::colors;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##ce_search", ImVec2(0, 32), ImGuiChildFlags_Borders);
    ImGui::SetCursorPos(ImVec2(12, 5));

    ImGui::TextColored(TEXT_DIM, "Find:");
    ImGui::SameLine(0, 4);
    ImGui::PushItemWidth(200);
    if (ImGui::InputText("##search_input", search_buf_, sizeof(search_buf_),
            ImGuiInputTextFlags_EnterReturnsTrue)) {
        perform_search();
    }
    ImGui::PopItemWidth();

    ImGui::SameLine(0, 8);
    ImGui::TextColored(TEXT_DIM, "Replace:");
    ImGui::SameLine(0, 4);
    ImGui::PushItemWidth(200);
    ImGui::InputText("##replace_input", replace_buf_, sizeof(replace_buf_));
    ImGui::PopItemWidth();

    ImGui::SameLine(0, 8);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 2));

    if (ImGui::SmallButton("Find##do_find")) perform_search();
    ImGui::SameLine(0, 2);
    if (ImGui::SmallButton("Prev##find_prev")) navigate_match(-1);
    ImGui::SameLine(0, 2);
    if (ImGui::SmallButton("Next##find_next")) navigate_match(1);
    ImGui::SameLine(0, 4);
    if (ImGui::SmallButton("Replace##do_repl")) replace_current();
    ImGui::SameLine(0, 2);
    if (ImGui::SmallButton("All##repl_all")) replace_all();
    ImGui::SameLine(0, 8);

    ImGui::Checkbox("Case##search_case", &search_case_sensitive_);

    ImGui::SameLine(0, 8);
    if (!search_matches_.empty()) {
        ImGui::TextColored(INFO, "%d/%d",
            current_match_ >= 0 ? current_match_ + 1 : 0,
            static_cast<int>(search_matches_.size()));
    }

    ImGui::SameLine(0, 8);
    ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DIM);
    if (ImGui::SmallButton("X##close_search")) {
        show_search_ = false;
        search_matches_.clear();
    }
    ImGui::PopStyleColor();

    ImGui::PopStyleVar();
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// =============================================================================
// Cells area
// =============================================================================
void CodeEditorScreen::render_cells() {
    using namespace theme::colors;

    float status_h = 28.0f;
    float avail_h = ImGui::GetContentRegionAvail().y - status_h;

    ImGui::BeginChild("##ce_cells_area", ImVec2(0, avail_h), ImGuiChildFlags_None);

    float avail_w = ImGui::GetContentRegionAvail().x;
    float pad_x = (avail_w > 900.0f) ? (avail_w - 860.0f) * 0.5f : 20.0f;

    ImGui::SetCursorPosX(pad_x);
    ImGui::BeginChild("##ce_cells_inner", ImVec2(avail_w - pad_x * 2, 0), ImGuiChildFlags_None);

    ImGui::Spacing();

    auto& cells = nb().cells;
    for (int i = 0; i < static_cast<int>(cells.size()); i++) {
        render_cell(i);
        render_add_cell_divider(i);
    }

    if (cells.empty()) {
        ImGui::Spacing(); ImGui::Spacing();
        ImGui::TextColored(TEXT_DIM, "No cells. Click + CODE or + MARKDOWN to add one.");
        render_add_cell_divider(-1);
    }

    ImGui::Spacing();
    ImGui::EndChild();
    ImGui::EndChild();
}

// =============================================================================
// Single cell
// =============================================================================
void CodeEditorScreen::render_cell(int idx) {
    using namespace theme::colors;

    auto& cell = nb().cells[idx];
    bool is_selected = (idx == tab().selected_cell);
    bool is_running = (idx == running_cell_.load());
    bool has_error = false;
    bool has_output = !cell.outputs.empty();
    for (auto& o : cell.outputs) {
        if (o.output_type == "error") { has_error = true; break; }
    }
    bool has_success = has_output && !has_error;

    // Execution order warning (point 15)
    bool order_warn = has_execution_order_warning(idx);

    ImVec4 accent = BORDER_DIM;
    if (is_running)       accent = WARNING;
    else if (has_error)   accent = ERROR_RED;
    else if (has_success) accent = SUCCESS;
    else if (is_selected) accent = ACCENT;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);

    char cid[32];
    std::snprintf(cid, sizeof(cid), "##cell_%d", idx);
    ImGui::BeginChild(cid, ImVec2(0, 0),
        ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders);

    // Left colored border
    ImVec2 p = ImGui::GetWindowPos();
    ImVec2 s = ImGui::GetWindowSize();
    ImGui::GetWindowDrawList()->AddRectFilled(
        p, ImVec2(p.x + 4, p.y + s.y),
        ImGui::ColorConvertFloat4ToU32(accent));

    // Drag reorder (point 7) — source
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
        drag_source_cell_ = idx;
        ImGui::SetDragDropPayload("CELL_REORDER", &idx, sizeof(int));
        ImGui::Text("Move cell %d", idx + 1);
        ImGui::EndDragDropSource();
    }
    // Drag target
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CELL_REORDER")) {
            int src = *static_cast<const int*>(payload->Data);
            if (src != idx && src >= 0 && src < static_cast<int>(nb().cells.size())) {
                push_undo("reorder cell");
                auto cell_copy = std::move(nb().cells[src]);
                nb().cells.erase(nb().cells.begin() + src);
                int target = (src < idx) ? idx - 1 : idx;
                nb().cells.insert(nb().cells.begin() + target, std::move(cell_copy));
                tab().selected_cell = target;
                tab().unsaved = true;
                active_edit_cell_ = -1;
            }
        }
        ImGui::EndDragDropTarget();
    }

    // Cell header
    render_cell_header(idx);

    ImGui::Spacing();

    // Cell editor / preview
    render_cell_editor(idx);

    // Click to select
    if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) && ImGui::IsMouseClicked(0)) {
        if (tab().selected_cell != idx) {
            flush_edit_buffer();
            tab().selected_cell = idx;
            active_edit_cell_ = -1;
        }
    }

    // Cell output (point 9: scroll-capped)
    if (has_output && cell.cell_type == "code") {
        ImGui::Spacing();
        render_cell_output(idx);
    }

    // Execution order warning badge (point 15)
    if (order_warn) {
        ImGui::SetCursorPosX(16);
        ImGui::TextColored(WARNING, "[!] Cells executed out of order");
    }

    ImGui::Spacing();
    ImGui::EndChild();
    ImGui::PopStyleColor(); // ChildBg

    ImGui::Spacing();
}

// =============================================================================
// Cell header
// =============================================================================
void CodeEditorScreen::render_cell_header(int idx) {
    using namespace theme::colors;

    auto& cell = nb().cells[idx];
    bool is_selected = (idx == tab().selected_cell);
    bool is_running = (idx == running_cell_.load());
    bool has_error = false;
    bool has_output = !cell.outputs.empty();
    for (auto& o : cell.outputs) {
        if (o.output_type == "error") { has_error = true; break; }
    }
    bool has_success = has_output && !has_error;

    ImGui::SetCursorPos(ImVec2(16, 6));

    // Execution count badge with timer (point 8)
    if (cell.cell_type == "code") {
        ImVec4 badge_col = is_running ? WARNING :
                           has_error ? ERROR_RED :
                           has_success ? SUCCESS : ACCENT;

        char badge[48];
        if (is_running)
            std::snprintf(badge, sizeof(badge), "In [*]");
        else if (cell.execution_count >= 0) {
            if (cell.exec_time_ms >= 0) {
                if (cell.exec_time_ms >= 1000)
                    std::snprintf(badge, sizeof(badge), "In [%d] %.1fs", cell.execution_count, cell.exec_time_ms / 1000.0);
                else
                    std::snprintf(badge, sizeof(badge), "In [%d] %dms", cell.execution_count, static_cast<int>(cell.exec_time_ms));
            } else {
                std::snprintf(badge, sizeof(badge), "In [%d]", cell.execution_count);
            }
        }
        else
            std::snprintf(badge, sizeof(badge), "In [ ]");

        ImGui::TextColored(badge_col, "%s", badge);
    } else {
        ImGui::TextColored(ImVec4(0.62f, 0.31f, 0.87f, 1.0f), "MD");
    }

    ImGui::SameLine(0, 8);
    ImGui::TextColored(TEXT_DIM, "%s",
        cell.cell_type == "code" ? "PYTHON" : "MARKDOWN");

    // Action buttons (when selected)
    if (is_selected) {
        float btn_x = ImGui::GetContentRegionAvail().x - 220;
        if (btn_x > 100) ImGui::SameLine(btn_x);
        else ImGui::SameLine();

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 2));

        if (cell.cell_type == "code") {
            if (is_running) {
                ImGui::TextColored(WARNING, "RUNNING...");
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.15f, 0.05f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_Text, SUCCESS);
                char run_id[32];
                std::snprintf(run_id, sizeof(run_id), "RUN##run_%d", idx);
                if (ImGui::SmallButton(run_id)) { run_cell(idx); }
                ImGui::PopStyleColor(2);
            }
            ImGui::SameLine(0, 4);
        }

        ImGui::PushStyleColor(ImGuiCol_Button, BG_WIDGET);
        ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DIM);

        // Toggle type
        char toggle_id[32];
        std::snprintf(toggle_id, sizeof(toggle_id), "%s##tog_%d",
            cell.cell_type == "code" ? "->MD" : "->PY", idx);
        if (ImGui::SmallButton(toggle_id)) {
            push_undo("toggle type");
            toggle_cell_type(idx);
        }
        ImGui::SameLine(0, 2);

        // Move up
        char up_id[32];
        std::snprintf(up_id, sizeof(up_id), "^##up_%d", idx);
        if (idx > 0) {
            if (ImGui::SmallButton(up_id)) {
                push_undo("move cell up");
                move_cell(idx, -1);
            }
        }
        ImGui::SameLine(0, 2);

        // Move down
        char dn_id[32];
        std::snprintf(dn_id, sizeof(dn_id), "v##dn_%d", idx);
        if (idx < static_cast<int>(nb().cells.size()) - 1) {
            if (ImGui::SmallButton(dn_id)) {
                push_undo("move cell down");
                move_cell(idx, 1);
            }
        }
        ImGui::SameLine(0, 2);

        // Clear output
        if (has_output) {
            char clr_id[32];
            std::snprintf(clr_id, sizeof(clr_id), "CLR##clr_%d", idx);
            if (ImGui::SmallButton(clr_id)) clear_cell_output(idx);
            ImGui::SameLine(0, 2);
        }

        ImGui::PopStyleColor(2);

        // Delete
        if (nb().cells.size() > 1) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.02f, 0.02f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, ERROR_RED);
            char del_id[32];
            std::snprintf(del_id, sizeof(del_id), "X##del_%d", idx);
            if (ImGui::SmallButton(del_id)) {
                push_undo("delete cell");
                delete_cell(idx);
                ImGui::PopStyleColor(2);
                ImGui::PopStyleVar();
                // Early exit — cell no longer valid
                return;
            }
            ImGui::PopStyleColor(2);
        }

        ImGui::PopStyleVar();
    }
}

// =============================================================================
// Cell line numbers (point 12)
// =============================================================================
void CodeEditorScreen::render_cell_line_numbers(const std::string& source, float height) {
    using namespace theme::colors;

    int line_count = 1;
    for (char c : source) if (c == '\n') line_count++;

    float line_h = ImGui::GetTextLineHeight();
    float gutter_w = 35.0f;

    ImGui::BeginChild("##gutter", ImVec2(gutter_w, height), ImGuiChildFlags_None);
    ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DISABLED);
    for (int i = 1; i <= line_count; i++) {
        ImGui::Text("%3d", i);
    }
    ImGui::PopStyleColor();
    ImGui::EndChild();
    (void)line_h; // suppress unused warning
}

// =============================================================================
// Cell editor
// =============================================================================
void CodeEditorScreen::render_cell_editor(int idx) {
    using namespace theme::colors;

    auto& cell = nb().cells[idx];
    bool is_selected = (idx == tab().selected_cell);

    ImGui::SetCursorPosX(16);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, BG_DARKEST);

    char editor_id[32];
    std::snprintf(editor_id, sizeof(editor_id), "##celledit_%d", idx);

    int line_count = 1;
    for (char c : cell.source) if (c == '\n') line_count++;
    float line_h = ImGui::GetTextLineHeight();
    float editor_h = std::max(3, std::min(line_count + 1, 30)) * line_h + 12;

    if (is_selected || active_edit_cell_ == idx) {
        if (active_edit_cell_ != idx) {
            active_edit_cell_ = idx;
            edit_buf_.resize(cell.source.size() + 16384);
            std::memcpy(edit_buf_.data(), cell.source.c_str(), cell.source.size() + 1);
        }

        // Line numbers (point 12)
        char gutter_id[32];
        std::snprintf(gutter_id, sizeof(gutter_id), "##gutter_%d", idx);
        ImGui::BeginChild(gutter_id, ImVec2(35, editor_h), ImGuiChildFlags_None);
        ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DISABLED);
        for (int i = 1; i <= line_count; i++) {
            ImGui::Text("%3d", i);
        }
        ImGui::PopStyleColor();
        ImGui::EndChild();

        ImGui::SameLine(0, 0);

        float edit_w = ImGui::GetContentRegionAvail().x - 16;
        if (ImGui::InputTextMultiline(editor_id, edit_buf_.data(), edit_buf_.size(),
                ImVec2(edit_w, editor_h),
                ImGuiInputTextFlags_AllowTabInput)) {
            cell.source = edit_buf_.data();
            tab().unsaved = true;
        }

        // Ctrl+Enter / Shift+Enter
        if (ImGui::IsItemFocused()) {
            if (ImGui::IsKeyPressed(ImGuiKey_Enter) &&
                (ImGui::GetIO().KeyCtrl || ImGui::GetIO().KeyShift)) {
                cell.source = edit_buf_.data();
                if (cell.cell_type == "code") {
                    run_cell(idx);
                    if (ImGui::GetIO().KeyShift) {
                        if (idx < static_cast<int>(nb().cells.size()) - 1) {
                            tab().selected_cell = idx + 1;
                            active_edit_cell_ = -1;
                        } else {
                            add_cell(idx, "code");
                        }
                    }
                }
            }
        }
    } else {
        // Read-only preview with syntax highlighting
        ImGui::SetCursorPosX(16);

        // Line numbers for preview too
        char gutter_id[32];
        std::snprintf(gutter_id, sizeof(gutter_id), "##gutter_p_%d", idx);
        ImGui::BeginChild(gutter_id, ImVec2(35, editor_h), ImGuiChildFlags_None);
        ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DISABLED);
        for (int i = 1; i <= line_count; i++) {
            ImGui::Text("%3d", i);
        }
        ImGui::PopStyleColor();
        ImGui::EndChild();

        ImGui::SameLine(0, 0);

        float preview_w = ImGui::GetContentRegionAvail().x - 16;
        char prev_id[32];
        std::snprintf(prev_id, sizeof(prev_id), "##preview_%d", idx);
        ImGui::BeginChild(prev_id, ImVec2(preview_w, editor_h), ImGuiChildFlags_None);

        if (cell.source.empty()) {
            ImGui::TextColored(TEXT_DISABLED,
                cell.cell_type == "code" ? "# Enter Python code..." : "Enter markdown text...");
        } else if (cell.cell_type == "code") {
            render_highlighted_text(cell.source, preview_w - 8);
        } else {
            render_markdown(cell.source);
        }

        ImGui::EndChild();
    }

    ImGui::PopStyleColor(); // FrameBg
}

// =============================================================================
// Cell Output (point 9: scroll capped at 300px)
// =============================================================================
void CodeEditorScreen::render_cell_output(int idx) {
    using namespace theme::colors;

    auto& cell = nb().cells[idx];
    bool has_error = false;
    for (auto& o : cell.outputs) {
        if (o.output_type == "error") { has_error = true; break; }
    }

    ImGui::SetCursorPosX(16);
    ImGui::TextColored(has_error ? ERROR_RED : SUCCESS,
        has_error ? "ERROR OUTPUT" : "OUTPUT");
    if (cell.execution_count >= 0) {
        ImGui::SameLine(0, 8);
        ImGui::TextColored(BORDER_BRIGHT, "|");
        ImGui::SameLine(0, 8);
        ImGui::TextColored(INFO, "Out [%d]", cell.execution_count);
    }
    if (cell.exec_time_ms >= 0) {
        ImGui::SameLine(0, 8);
        ImGui::TextColored(BORDER_BRIGHT, "|");
        ImGui::SameLine(0, 8);
        if (cell.exec_time_ms >= 1000)
            ImGui::TextColored(TEXT_DIM, "%.2fs", cell.exec_time_ms / 1000.0);
        else
            ImGui::TextColored(TEXT_DIM, "%dms", static_cast<int>(cell.exec_time_ms));
    }

    ImGui::Spacing();

    // Scroll-capped output container (point 9)
    float max_output_h = 300.0f;
    char scroll_id[32];
    std::snprintf(scroll_id, sizeof(scroll_id), "##out_scroll_%d", idx);

    ImGui::SetCursorPosX(16);
    ImGui::BeginChild(scroll_id, ImVec2(ImGui::GetContentRegionAvail().x - 16, max_output_h),
        ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar);

    for (auto& out : cell.outputs) {
        if (out.output_type == "stream") {
            ImGui::PushTextWrapPos(ImGui::GetWindowWidth() - 8);
            ImGui::TextColored(TEXT_PRIMARY, "%s", out.text.c_str());
            ImGui::PopTextWrapPos();
        } else if (out.output_type == "error") {
            // Red left border
            ImVec2 p2 = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddRectFilled(
                p2, ImVec2(p2.x + 3, p2.y + ImGui::GetTextLineHeight() * 3),
                ImGui::ColorConvertFloat4ToU32(ERROR_RED));

            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 8);
            if (!out.ename.empty()) {
                ImGui::TextColored(ERROR_RED, "%s: %s", out.ename.c_str(), out.evalue.c_str());
            }
            if (!out.traceback.empty()) {
                ImGui::PushTextWrapPos(ImGui::GetWindowWidth() - 8);
                ImGui::TextColored(ImVec4(0.96f, 0.30f, 0.30f, 0.7f), "%s", out.traceback.c_str());
                ImGui::PopTextWrapPos();
            }
        } else if (out.output_type == "execute_result" || out.output_type == "display_data") {
            ImGui::PushTextWrapPos(ImGui::GetWindowWidth() - 8);
            ImGui::TextColored(INFO, "%s", out.text.c_str());
            ImGui::PopTextWrapPos();
        }
    }

    ImGui::EndChild();
}

// =============================================================================
// Add cell divider
// =============================================================================
void CodeEditorScreen::render_add_cell_divider(int after_idx) {
    using namespace theme::colors;

    float avail_w = ImGui::GetContentRegionAvail().x;

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, BG_HOVER);
    ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DIM);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 2));

    float btn_w = ImGui::CalcTextSize("+ CODE").x + ImGui::CalcTextSize("+ MARKDOWN").x + 60;
    float center_x = (avail_w - btn_w) * 0.5f;
    if (center_x > 0) ImGui::SetCursorPosX(center_x);

    char code_id[32], md_id[32];
    std::snprintf(code_id, sizeof(code_id), "+ CODE##add_c_%d", after_idx);
    std::snprintf(md_id, sizeof(md_id), "+ MARKDOWN##add_m_%d", after_idx);

    if (ImGui::SmallButton(code_id)) {
        push_undo("add code cell");
        add_cell(after_idx, "code");
    }
    ImGui::SameLine(0, 4);
    if (ImGui::SmallButton(md_id)) {
        push_undo("add markdown cell");
        add_cell(after_idx, "markdown");
    }

    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);
}

// =============================================================================
// Status bar
// =============================================================================
void CodeEditorScreen::render_status_bar() {
    using namespace theme::colors;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARKEST);
    ImGui::BeginChild("##ce_status", ImVec2(0, 28), ImGuiChildFlags_Borders);
    ImGui::SetCursorPos(ImVec2(12, 6));

    // Python version badge
    if (!python_version_.empty()) {
        ImGui::TextColored(SUCCESS, "%s", python_version_.c_str());
    } else {
        ImGui::TextColored(TEXT_DIM, "PYTHON");
    }

    // Cell counts
    int code_count = 0, md_count = 0, executed = 0;
    for (auto& c : nb().cells) {
        if (c.cell_type == "code") code_count++;
        else md_count++;
        if (c.execution_count >= 0) executed++;
    }

    ImGui::SameLine(0, 12);
    ImGui::TextColored(BORDER_BRIGHT, "|");
    ImGui::SameLine(0, 8);
    ImGui::TextColored(INFO, "%d", code_count);
    ImGui::SameLine(0, 2);
    ImGui::TextColored(TEXT_DIM, "CODE");

    ImGui::SameLine(0, 8);
    ImGui::TextColored(ImVec4(0.62f, 0.31f, 0.87f, 1.0f), "%d", md_count);
    ImGui::SameLine(0, 2);
    ImGui::TextColored(TEXT_DIM, "MD");

    ImGui::SameLine(0, 12);
    ImGui::TextColored(BORDER_BRIGHT, "|");
    ImGui::SameLine(0, 8);
    ImGui::TextColored(TEXT_DIM, "EXEC:");
    ImGui::SameLine(0, 4);
    ImGui::TextColored(INFO, "%d/%d", executed, code_count);

    // Tab count
    ImGui::SameLine(0, 12);
    ImGui::TextColored(BORDER_BRIGHT, "|");
    ImGui::SameLine(0, 8);
    ImGui::TextColored(TEXT_DIM, "TABS:");
    ImGui::SameLine(0, 4);
    ImGui::TextColored(INFO, "%d", static_cast<int>(tabs_.size()));

    // Undo depth
    ImGui::SameLine(0, 8);
    ImGui::TextColored(TEXT_DIM, "UNDO:");
    ImGui::SameLine(0, 4);
    ImGui::TextColored(INFO, "%d", static_cast<int>(tab().undo_stack.size()));

    if (tab().unsaved) {
        ImGui::SameLine(0, 12);
        ImGui::TextColored(BORDER_BRIGHT, "|");
        ImGui::SameLine(0, 8);
        ImGui::TextColored(WARNING, "UNSAVED");
    }

    if (!status_message_.empty()) {
        ImGui::SameLine(0, 12);
        ImGui::TextColored(BORDER_BRIGHT, "|");
        ImGui::SameLine(0, 8);
        ImGui::TextColored(INFO, "%s", status_message_.c_str());
    }

    // Right side: keyboard hints (point 17)
    const char* hints = "Ctrl+Enter:RUN | Shift+Enter:RUN+NEXT | Ctrl+Z:UNDO | Ctrl+F:FIND | a/b:ADD | dd:DEL";
    float hint_w = ImGui::CalcTextSize(hints).x + 20;
    float win_w = ImGui::GetWindowWidth();
    if (win_w > hint_w + 100) {
        ImGui::SameLine(win_w - hint_w);
        ImGui::TextColored(TEXT_DISABLED, "%s", hints);
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// =============================================================================
// File dialogs
// =============================================================================
void CodeEditorScreen::render_file_dialog() {
    using namespace theme::colors;

    if (show_open_dialog_) {
        ImGui::OpenPopup("Open Notebook##ce_open");
        show_open_dialog_ = false;
    }
    if (show_save_dialog_) {
        ImGui::OpenPopup("Save Notebook##ce_save");
        show_save_dialog_ = false;
    }
    if (show_export_dialog_) {
        ImGui::OpenPopup("Export Python##ce_export");
        show_export_dialog_ = false;
    }

    // Open dialog (fallback if native failed)
    ImGui::SetNextWindowSize(ImVec2(500, 120));
    if (ImGui::BeginPopupModal("Open Notebook##ce_open", nullptr, ImGuiWindowFlags_NoResize)) {
        ImGui::Text("Enter path to .ipynb file:");
        ImGui::PushItemWidth(-1);
        bool enter = ImGui::InputText("##open_path", path_buf_, sizeof(path_buf_),
            ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::PopItemWidth();
        ImGui::Spacing();
        if (theme::AccentButton("Open") || enter) {
            if (std::strlen(path_buf_) > 0) {
                open_notebook(path_buf_);
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine(0, 8);
        if (theme::SecondaryButton("Cancel")) { ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }

    // Save dialog
    ImGui::SetNextWindowSize(ImVec2(500, 120));
    if (ImGui::BeginPopupModal("Save Notebook##ce_save", nullptr, ImGuiWindowFlags_NoResize)) {
        ImGui::Text("Enter save path (.ipynb):");
        ImGui::PushItemWidth(-1);
        bool enter = ImGui::InputText("##save_path", path_buf_, sizeof(path_buf_),
            ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::PopItemWidth();
        ImGui::Spacing();
        if (theme::AccentButton("Save") || enter) {
            if (std::strlen(path_buf_) > 0) {
                save_notebook_to(path_buf_);
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine(0, 8);
        if (theme::SecondaryButton("Cancel")) { ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }

    // Export dialog
    ImGui::SetNextWindowSize(ImVec2(500, 120));
    if (ImGui::BeginPopupModal("Export Python##ce_export", nullptr, ImGuiWindowFlags_NoResize)) {
        ImGui::Text("Enter export path (.py):");
        ImGui::PushItemWidth(-1);
        bool enter = ImGui::InputText("##export_path", path_buf_, sizeof(path_buf_),
            ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::PopItemWidth();
        ImGui::Spacing();
        if (theme::AccentButton("Export") || enter) {
            if (std::strlen(path_buf_) > 0) {
                export_to_py(path_buf_);
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine(0, 8);
        if (theme::SecondaryButton("Cancel")) { ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }
}

// =============================================================================
// Pip install dialog (point 14)
// =============================================================================
void CodeEditorScreen::render_pip_dialog() {
    using namespace theme::colors;

    if (show_pip_dialog_) {
        ImGui::OpenPopup("Install Package##pip");
        show_pip_dialog_ = false;
    }

    ImGui::SetNextWindowSize(ImVec2(400, 130));
    if (ImGui::BeginPopupModal("Install Package##pip", nullptr, ImGuiWindowFlags_NoResize)) {
        ImGui::Text("Package name (e.g. pandas, numpy==1.24):");
        ImGui::PushItemWidth(-1);
        bool enter = ImGui::InputText("##pip_pkg", pip_pkg_buf_, sizeof(pip_pkg_buf_),
            ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::PopItemWidth();
        ImGui::Spacing();
        if (theme::AccentButton("Install") || enter) {
            if (std::strlen(pip_pkg_buf_) > 0) {
                pip_install(pip_pkg_buf_);
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine(0, 8);
        if (theme::SecondaryButton("Cancel")) { ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }
}

// =============================================================================
// Cell operations
// =============================================================================
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

// =============================================================================
// Execution (with timer — point 8, persistent feel via sequential cells — point 2)
// =============================================================================
void CodeEditorScreen::run_cell(int idx) {
    auto& cells = nb().cells;
    if (idx < 0 || idx >= static_cast<int>(cells.size())) return;
    auto& cell = cells[idx];
    if (cell.cell_type != "code" || cell.source.empty()) return;
    if (exec_pending_) return;

    flush_edit_buffer();
    running_cell_ = idx;
    exec_pending_ = true;

    int exec_num = tab().execution_counter++;
    int tab_idx = active_tab_idx_;
    std::string source = cell.source;

    // Persistent session (point 2): gather all previously executed cells' source
    // and prepend to current cell so variables carry over
    std::string full_source;
    for (int i = 0; i < idx; i++) {
        if (cells[i].cell_type == "code" && cells[i].execution_count >= 0 && !cells[i].source.empty()) {
            full_source += cells[i].source;
            if (!cells[i].source.empty() && cells[i].source.back() != '\n') full_source += '\n';
        }
    }
    full_source += source;

    exec_future_ = std::async(std::launch::async, [this, idx, exec_num, tab_idx, full_source]() {
        auto t_start = std::chrono::steady_clock::now();
        std::string tmp_path = write_temp_script(full_source);
        python::PythonResult result = run_cell_subprocess(tmp_path);
        try { fs::remove(tmp_path); } catch (...) {}
        auto t_end = std::chrono::steady_clock::now();
        double elapsed_ms = std::chrono::duration<double, std::milli>(t_end - t_start).count();

        // If we prepended previous cells, strip their output
        // The combined script runs everything, but we only want output from the last cell
        // This is imperfect but workable — the last cell's print statements will include
        // earlier cells' outputs too. A true persistent session needs a live REPL process.

        std::lock_guard<std::mutex> lock(result_mutex_);
        PendingResult pr;
        pr.tab_idx = tab_idx;
        pr.cell_idx = idx;
        pr.exec_num = exec_num;
        pr.success = result.success;
        pr.output = result.output;
        pr.error = result.error;
        pr.elapsed_ms = elapsed_ms;
        pending_results_.push_back(std::move(pr));
    });
}

void CodeEditorScreen::run_all_cells() {
    if (exec_pending_) return;
    flush_edit_buffer();

    running_all_ = true;

    struct CellJob { int idx; int exec_num; std::string source; };
    std::vector<CellJob> jobs;
    for (int i = 0; i < static_cast<int>(nb().cells.size()); i++) {
        auto& c = nb().cells[i];
        if (c.cell_type != "code" || c.source.empty()) continue;
        jobs.push_back({i, tab().execution_counter++, c.source});
    }
    int tab_idx = active_tab_idx_;

    exec_future_ = std::async(std::launch::async, [this, jobs = std::move(jobs), tab_idx]() {
        // Persistent session: accumulate source for sequential execution
        std::string accumulated;

        for (auto& job : jobs) {
            if (!running_all_) break;

            running_cell_ = job.idx;
            accumulated += job.source;
            if (!job.source.empty() && job.source.back() != '\n') accumulated += '\n';

            auto t_start = std::chrono::steady_clock::now();
            std::string tmp_path = write_temp_script(accumulated);
            python::PythonResult result = run_cell_subprocess(tmp_path);
            try { fs::remove(tmp_path); } catch (...) {}
            auto t_end = std::chrono::steady_clock::now();
            double elapsed_ms = std::chrono::duration<double, std::milli>(t_end - t_start).count();

            std::lock_guard<std::mutex> lock(result_mutex_);
            PendingResult pr;
            pr.tab_idx = tab_idx;
            pr.cell_idx = job.idx;
            pr.exec_num = job.exec_num;
            pr.success = result.success;
            pr.output = result.output;
            pr.error = result.error;
            pr.elapsed_ms = elapsed_ms;
            pending_results_.push_back(std::move(pr));

            if (!result.success) break; // stop on error
        }
        running_cell_ = -1;
        running_all_ = false;
    });

    exec_pending_ = true;
}

void CodeEditorScreen::interrupt_execution() {
    running_all_ = false;
    set_status("Execution interrupted");
}

// =============================================================================
// File operations (point 3: multi-tab)
// =============================================================================
void CodeEditorScreen::new_tab() {
    NotebookTab t;
    t.notebook = io::create_new_notebook();
    t.name = "fincept" + std::to_string(tab_counter_++);
    t.selected_cell = 1;
    tabs_.push_back(std::move(t));
    active_tab_idx_ = static_cast<int>(tabs_.size()) - 1;
    active_edit_cell_ = -1;
    set_status("New notebook created");
}

void CodeEditorScreen::close_tab(int idx) {
    if (idx < 0 || idx >= static_cast<int>(tabs_.size())) return;
    tabs_.erase(tabs_.begin() + idx);
    if (tabs_.empty()) new_tab();
    if (active_tab_idx_ >= static_cast<int>(tabs_.size()))
        active_tab_idx_ = static_cast<int>(tabs_.size()) - 1;
    active_edit_cell_ = -1;
}

void CodeEditorScreen::open_notebook_dialog() {
    std::string path = native_open_dialog(
        "Jupyter Notebooks (*.ipynb)\0*.ipynb\0All Files\0*.*\0",
        "Open Notebook");
    if (!path.empty()) {
        open_notebook(path);
    } else {
        show_open_dialog_ = true;
        std::memset(path_buf_, 0, sizeof(path_buf_));
    }
}

void CodeEditorScreen::open_notebook(const std::string& path) {
    if (!fs::exists(path)) {
        set_status("File not found: " + path);
        return;
    }

    Notebook loaded = io::load_notebook(path);
    if (loaded.cells.empty()) {
        set_status("Failed to load notebook");
        return;
    }

    NotebookTab t;
    t.notebook = std::move(loaded);
    t.file_path = path;
    t.name = fs::path(path).stem().string();
    t.selected_cell = 0;
    tabs_.push_back(std::move(t));
    active_tab_idx_ = static_cast<int>(tabs_.size()) - 1;
    active_edit_cell_ = -1;
    set_status("Opened: " + fs::path(path).filename().string());
}

void CodeEditorScreen::save_current() {
    flush_edit_buffer();
    if (tab().file_path.empty()) {
        std::string path = native_save_dialog(
            "Jupyter Notebooks (*.ipynb)\0*.ipynb\0All Files\0*.*\0",
            "Save Notebook");
        if (!path.empty()) {
            save_notebook_to(path);
        } else {
            show_save_dialog_ = true;
            std::memset(path_buf_, 0, sizeof(path_buf_));
        }
    } else {
        save_notebook_to(tab().file_path);
    }
}

void CodeEditorScreen::save_notebook_to(const std::string& path) {
    if (io::save_notebook(path, nb())) {
        tab().file_path = path;
        tab().name = fs::path(path).stem().string();
        tab().unsaved = false;
        set_status("Saved: " + fs::path(path).filename().string());
    } else {
        set_status("Failed to save");
    }
}

void CodeEditorScreen::export_to_py(const std::string& path) {
    flush_edit_buffer();
    if (io::export_to_python(path, nb())) {
        set_status("Exported: " + fs::path(path).filename().string());
    } else {
        set_status("Export failed");
    }
}

// Auto-save (point 10)
void CodeEditorScreen::auto_save() {
    for (auto& t : tabs_) {
        if (t.unsaved && !t.file_path.empty()) {
            std::string autosave_path = t.file_path + ".autosave";
            io::save_notebook(autosave_path, t.notebook);
        }
    }
}

// =============================================================================
// Search (point 13)
// =============================================================================
void CodeEditorScreen::perform_search() {
    search_matches_.clear();
    current_match_ = -1;

    std::string query(search_buf_);
    if (query.empty()) return;

    auto& cells = nb().cells;
    for (int i = 0; i < static_cast<int>(cells.size()); i++) {
        const auto& src = cells[i].source;
        std::string hay = src;
        std::string needle = query;

        if (!search_case_sensitive_) {
            std::transform(hay.begin(), hay.end(), hay.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            std::transform(needle.begin(), needle.end(), needle.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        }

        size_t pos = 0;
        while ((pos = hay.find(needle, pos)) != std::string::npos) {
            search_matches_.push_back({i, static_cast<int>(pos), static_cast<int>(query.size())});
            pos += query.size();
        }
    }

    if (!search_matches_.empty()) {
        current_match_ = 0;
        tab().selected_cell = search_matches_[0].cell_idx;
        active_edit_cell_ = -1;
    }
    set_status(std::to_string(search_matches_.size()) + " matches found");
}

void CodeEditorScreen::navigate_match(int direction) {
    if (search_matches_.empty()) return;
    current_match_ = (current_match_ + direction + static_cast<int>(search_matches_.size())) %
                      static_cast<int>(search_matches_.size());
    tab().selected_cell = search_matches_[current_match_].cell_idx;
    active_edit_cell_ = -1;
}

void CodeEditorScreen::replace_current() {
    if (current_match_ < 0 || current_match_ >= static_cast<int>(search_matches_.size())) return;
    auto& m = search_matches_[current_match_];
    if (m.cell_idx < 0 || m.cell_idx >= static_cast<int>(nb().cells.size())) return;

    push_undo("replace");
    auto& src = nb().cells[m.cell_idx].source;
    std::string repl(replace_buf_);
    src = src.substr(0, m.char_offset) + repl + src.substr(m.char_offset + m.length);
    tab().unsaved = true;
    active_edit_cell_ = -1;
    perform_search(); // refresh matches
}

void CodeEditorScreen::replace_all() {
    if (search_matches_.empty()) return;
    push_undo("replace all");

    std::string query(search_buf_);
    std::string repl(replace_buf_);
    if (query.empty()) return;

    for (auto& cell : nb().cells) {
        if (search_case_sensitive_) {
            size_t pos = 0;
            while ((pos = cell.source.find(query, pos)) != std::string::npos) {
                cell.source.replace(pos, query.size(), repl);
                pos += repl.size();
            }
        } else {
            std::string lower_src = cell.source;
            std::string lower_q = query;
            std::transform(lower_q.begin(), lower_q.end(), lower_q.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

            // Replace from end to start to preserve offsets
            std::vector<size_t> positions;
            std::transform(lower_src.begin(), lower_src.end(), lower_src.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            size_t pos = 0;
            while ((pos = lower_src.find(lower_q, pos)) != std::string::npos) {
                positions.push_back(pos);
                pos += lower_q.size();
            }
            for (auto it = positions.rbegin(); it != positions.rend(); ++it) {
                cell.source.replace(*it, query.size(), repl);
            }
        }
    }

    tab().unsaved = true;
    active_edit_cell_ = -1;
    perform_search();
    set_status("Replaced all");
}

// =============================================================================
// Pip install (point 14)
// =============================================================================
void CodeEditorScreen::pip_install(const std::string& package) {
    set_status("Installing " + package + "...");

    std::thread([this, package]() {
        fs::path py = python::resolve_python_path("");
        if (py.empty() || !fs::exists(py)) {
#ifdef _WIN32
            py = "python";
#else
            py = "python3";
#endif
        }

        std::string cmd = "\"" + py.string() + "\" -m pip install " + package + " 2>&1";

#ifdef _WIN32
        FILE* pipe = _popen(cmd.c_str(), "r");
#else
        FILE* pipe = popen(cmd.c_str(), "r");
#endif
        if (!pipe) { set_status("pip: failed to start"); return; }

        std::string output;
        char buf[4096];
        while (std::fgets(buf, sizeof(buf), pipe)) output += buf;

#ifdef _WIN32
        int status = _pclose(pipe);
#else
        int status = pclose(pipe);
        status = WEXITSTATUS(status);
#endif

        if (status == 0) {
            set_status("Installed: " + package);
        } else {
            set_status("pip install failed: " + package);
        }
    }).detach();
}

// =============================================================================
// Keyboard navigation (point 17)
// =============================================================================
void CodeEditorScreen::handle_keyboard_navigation() {
    if (tabs_.empty()) return;

    auto& io = ImGui::GetIO();

    // Only handle when no text input is active
    bool text_active = ImGui::IsAnyItemActive();

    // Ctrl+Z = Undo, Ctrl+Y = Redo
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z) && !io.KeyShift) { undo(); return; }
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y)) { redo(); return; }
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z) && io.KeyShift) { redo(); return; }

    // Ctrl+F = Find
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_F)) { show_search_ = !show_search_; return; }

    // Ctrl+S = Save
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S)) { save_current(); return; }

    // When text input is active, don't intercept other keys
    if (text_active) return;

    auto& cells = nb().cells;
    int sel = tab().selected_cell;

    // Arrow up/down — move between cells
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow) || ImGui::IsKeyPressed(ImGuiKey_K)) {
        if (sel > 0) {
            flush_edit_buffer();
            tab().selected_cell = sel - 1;
            active_edit_cell_ = -1;
        }
    }
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow) || ImGui::IsKeyPressed(ImGuiKey_J)) {
        if (sel < static_cast<int>(cells.size()) - 1) {
            flush_edit_buffer();
            tab().selected_cell = sel + 1;
            active_edit_cell_ = -1;
        }
    }

    // 'a' — add cell above
    if (ImGui::IsKeyPressed(ImGuiKey_A)) {
        push_undo("add cell above");
        add_cell(sel - 1, "code");
    }

    // 'b' — add cell below
    if (ImGui::IsKeyPressed(ImGuiKey_B)) {
        push_undo("add cell below");
        add_cell(sel, "code");
    }

    // 'dd' — delete cell (double-d, simple: just 'd' for now with confirmation next frame)
    // Using 'x' as single-key delete instead, more intuitive
    if (ImGui::IsKeyPressed(ImGuiKey_X) && cells.size() > 1) {
        push_undo("delete cell");
        delete_cell(sel);
    }

    // 'm' — toggle to markdown
    if (ImGui::IsKeyPressed(ImGuiKey_M)) {
        if (sel >= 0 && sel < static_cast<int>(cells.size()) && cells[sel].cell_type == "code") {
            push_undo("to markdown");
            toggle_cell_type(sel);
        }
    }

    // 'y' — toggle to code
    if (ImGui::IsKeyPressed(ImGuiKey_Y)) {
        if (sel >= 0 && sel < static_cast<int>(cells.size()) && cells[sel].cell_type == "markdown") {
            push_undo("to code");
            toggle_cell_type(sel);
        }
    }

    // Enter — start editing selected cell
    if (ImGui::IsKeyPressed(ImGuiKey_Enter) && !io.KeyCtrl && !io.KeyShift) {
        active_edit_cell_ = -1; // will be re-initialized in render_cell_editor
    }

    // Escape — deselect edit mode
    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        flush_edit_buffer();
        active_edit_cell_ = -1;
        show_search_ = false;
    }
}

// =============================================================================
// Execution order warning (point 15)
// =============================================================================
bool CodeEditorScreen::has_execution_order_warning(int idx) {
    auto& cells = nb().cells;
    if (idx < 0 || idx >= static_cast<int>(cells.size())) return false;
    auto& cell = cells[idx];
    if (cell.cell_type != "code" || cell.execution_count < 0) return false;

    // Check if any earlier code cell has a higher execution count
    for (int i = 0; i < idx; i++) {
        if (cells[i].cell_type == "code" && cells[i].execution_count > cell.execution_count) {
            return true;
        }
    }
    return false;
}

// =============================================================================
// Utility
// =============================================================================
void CodeEditorScreen::detect_python_version() {
    std::thread([this]() {
        try {
            fs::path py = python::resolve_python_path("");
            if (py.empty() || !fs::exists(py)) {
#ifdef _WIN32
                py = "python";
#else
                py = "python3";
#endif
            }

            std::string cmd = "\"" + py.string() + "\" -c \"import sys; print(f'Python {sys.version_info.major}.{sys.version_info.minor}.{sys.version_info.micro}')\" 2>&1";

#ifdef _WIN32
            FILE* pipe = _popen(cmd.c_str(), "r");
#else
            FILE* pipe = popen(cmd.c_str(), "r");
#endif
            if (!pipe) { python_version_ = "Python N/A"; return; }

            std::string output;
            char buf[256];
            while (std::fgets(buf, sizeof(buf), pipe)) output += buf;

#ifdef _WIN32
            _pclose(pipe);
#else
            pclose(pipe);
#endif

            while (!output.empty() && (output.back() == '\n' || output.back() == '\r' || output.back() == ' '))
                output.pop_back();

            python_version_ = output.empty() ? "Python N/A" : output;
        } catch (...) {
            python_version_ = "Python N/A";
        }
    }).detach();
}

void CodeEditorScreen::set_status(const std::string& msg) {
    status_message_ = msg;
    status_message_time_ = ImGui::GetTime();
}

void CodeEditorScreen::flush_edit_buffer() {
    if (active_edit_cell_ >= 0 && !tabs_.empty() &&
        active_edit_cell_ < static_cast<int>(nb().cells.size())) {
        nb().cells[active_edit_cell_].source = edit_buf_.data();
    }
}

} // namespace fincept::code_editor
