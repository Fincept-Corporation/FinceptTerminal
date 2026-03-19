#include "code_editor_screen.h"
#include "editor_internal.h"
#include "ui/theme.h"
#include <imgui.h>
#include <set>
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <sstream>

namespace fincept::code_editor {

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

} // namespace fincept::code_editor
