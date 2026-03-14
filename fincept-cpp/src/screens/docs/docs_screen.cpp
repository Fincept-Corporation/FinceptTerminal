#include "docs_screen.h"
#include "docs_content.h"
#include "ui/theme.h"
#include "core/config.h"
#include <imgui.h>
#include <algorithm>
#include <cctype>

namespace fincept::docs {

// =============================================================================
// Helpers
// =============================================================================
static std::string to_lower(const std::string& s) {
    std::string out = s;
    for (auto& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

// =============================================================================
// Main render
// =============================================================================
void DocsScreen::render() {
    using namespace theme::colors;

    if (!initialized_) {
        sections_ = get_doc_sections();
        if (!sections_.empty()) {
            active_section_id_ = sections_[0].id;
            expanded_sections_[sections_[0].id] = true;
            if (!sections_[0].subsections.empty())
                active_subsection_id_ = sections_[0].subsections[0].id;
        }
        initialized_ = true;
    }

    ImGuiViewport* vp = ImGui::GetMainViewport();
    float tab_h = ImGui::GetFrameHeight() + 4;
    float footer_h = ImGui::GetFrameHeight();

    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x, vp->WorkPos.y + tab_h));
    ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x, vp->WorkSize.y - tab_h - footer_h));

    ImGui::PushStyleColor(ImGuiCol_WindowBg, BG_DARK);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

    ImGui::Begin("##docs_screen", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoBringToFrontOnFocus);

    render_header();

    float avail_w = ImGui::GetContentRegionAvail().x;
    float avail_h = ImGui::GetContentRegionAvail().y;
    float sidebar_w = 220.0f;
    float output_w = 260.0f;
    float content_w = avail_w - sidebar_w - output_w;
    if (content_w < 300.0f) {
        output_w = 0;
        content_w = avail_w - sidebar_w;
    }

    // 3-panel layout
    render_sidebar(sidebar_w);
    ImGui::SameLine(0, 0);

    ImGui::BeginChild("##docs_content_area", ImVec2(content_w, avail_h), ImGuiChildFlags_Borders);
    if (show_search_results_) {
        // Search results overlay
        theme::SectionHeader("SEARCH RESULTS");
        if (search_results_.empty()) {
            ImGui::TextColored(TEXT_DIM, "No results found for \"%s\"", search_query_.c_str());
        } else {
            ImGui::TextColored(TEXT_DIM, "%zu results for \"%s\"",
                search_results_.size(), search_query_.c_str());
            ImGui::Spacing();
            for (auto& r : search_results_) {
                ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
                char rid[128];
                std::snprintf(rid, sizeof(rid), "##sr_%s_%s", r.section_id.c_str(), r.sub_id.c_str());
                ImGui::BeginChild(rid, ImVec2(0, 0),
                    ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders);

                ImGui::SetCursorPos(ImVec2(12, 8));
                ImGui::TextColored(ACCENT, "%s > %s", r.section_title.c_str(), r.sub_title.c_str());
                ImGui::SetCursorPosX(12);
                ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - 12);
                ImGui::TextColored(TEXT_DIM, "%s", r.context.c_str());
                ImGui::PopTextWrapPos();
                ImGui::Spacing();

                ImGui::EndChild();
                ImGui::PopStyleColor();

                if (ImGui::IsItemClicked()) {
                    navigate_to(r.section_id, r.sub_id);
                    show_search_results_ = false;
                }
                ImGui::Spacing();
            }
        }
        ImGui::Spacing();
        if (theme::SecondaryButton("Close Search")) {
            show_search_results_ = false;
        }
    } else {
        render_content();
    }
    ImGui::EndChild();

    if (output_w > 0) {
        ImGui::SameLine(0, 0);
        render_output_panel(output_w);
    }

    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

// =============================================================================
// Header
// =============================================================================
void DocsScreen::render_header() {
    using namespace theme::colors;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARKEST);
    ImGui::BeginChild("##docs_header", ImVec2(0, 36), ImGuiChildFlags_None);

    ImGui::SetCursorPos(ImVec2(12, 8));
    ImGui::TextColored(ACCENT, "FINCEPT DOCS");

    // Breadcrumb
    if (!active_section_id_.empty()) {
        ImGui::SameLine(0, 12);
        ImGui::TextColored(BORDER_BRIGHT, ">");
        ImGui::SameLine(0, 8);
        for (auto& s : sections_) {
            if (s.id == active_section_id_) {
                ImGui::TextColored(TEXT_SECONDARY, "%s", s.title.c_str());
                if (!active_subsection_id_.empty()) {
                    for (auto& sub : s.subsections) {
                        if (sub.id == active_subsection_id_) {
                            ImGui::SameLine(0, 8);
                            ImGui::TextColored(BORDER_BRIGHT, ">");
                            ImGui::SameLine(0, 8);
                            ImGui::TextColored(TEXT_PRIMARY, "%s", sub.title.c_str());
                            break;
                        }
                    }
                }
                break;
            }
        }
    }

    // Search on right
    float search_w = 200.0f;
    ImGui::SameLine(ImGui::GetWindowWidth() - search_w - 16);
    ImGui::PushItemWidth(search_w);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, BG_INPUT);
    if (ImGui::InputTextWithHint("##doc_search", "Search docs...", search_buf_, sizeof(search_buf_),
            ImGuiInputTextFlags_EnterReturnsTrue)) {
        search_query_ = search_buf_;
        if (!search_query_.empty()) {
            perform_search();
            show_search_results_ = true;
        }
    }
    ImGui::PopStyleColor();
    ImGui::PopItemWidth();

    // Stats
    ImGui::SameLine(0, 8);

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// =============================================================================
// Sidebar
// =============================================================================
void DocsScreen::render_sidebar(float width) {
    using namespace theme::colors;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##docs_sidebar", ImVec2(width, 0), ImGuiChildFlags_Borders);

    ImGui::Spacing();
    ImGui::SetCursorPosX(12);
    ImGui::TextColored(TEXT_DIM, "SECTIONS");
    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

    for (auto& section : sections_) {
        bool is_expanded = expanded_sections_[section.id];
        bool is_active_section = (section.id == active_section_id_);

        // Section header with expand/collapse
        ImGui::SetCursorPosX(8);

        if (is_active_section) {
            ImGui::PushStyleColor(ImGuiCol_Text, ACCENT);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, TEXT_SECONDARY);
        }

        char label[128];
        std::snprintf(label, sizeof(label), "%s %s %s",
            is_expanded ? "v" : ">", section.icon_label.c_str(), section.title.c_str());

        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, BG_HOVER);

        if (ImGui::Selectable(label, is_active_section, 0, ImVec2(width - 16, 0))) {
            expanded_sections_[section.id] = !is_expanded;
            if (!is_expanded && !section.subsections.empty()) {
                navigate_to(section.id, section.subsections[0].id);
            }
        }

        ImGui::PopStyleColor(3);

        // Subsections
        if (is_expanded) {
            for (auto& sub : section.subsections) {
                bool is_active_sub = (sub.id == active_subsection_id_ &&
                                      section.id == active_section_id_);

                ImGui::SetCursorPosX(24);

                if (is_active_sub) {
                    // Orange left accent bar
                    ImVec2 p = ImGui::GetCursorScreenPos();
                    ImGui::GetWindowDrawList()->AddRectFilled(
                        ImVec2(p.x - 4, p.y), ImVec2(p.x - 1, p.y + ImGui::GetTextLineHeight()),
                        ImGui::ColorConvertFloat4ToU32(ACCENT));
                }

                ImGui::PushStyleColor(ImGuiCol_Text, is_active_sub ? ACCENT : TEXT_DIM);
                ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0, 0, 0, 0));
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, BG_HOVER);

                if (ImGui::Selectable(sub.title.c_str(), is_active_sub, 0,
                        ImVec2(width - 32, 0))) {
                    navigate_to(section.id, sub.id);
                }

                ImGui::PopStyleColor(3);
            }
            ImGui::Spacing();
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// =============================================================================
// Content
// =============================================================================
void DocsScreen::render_content() {
    using namespace theme::colors;

    ImGui::Spacing();

    // Find active subsection
    const DocSubsection* active_sub = nullptr;
    for (auto& s : sections_) {
        if (s.id == active_section_id_) {
            for (auto& sub : s.subsections) {
                if (sub.id == active_subsection_id_) {
                    active_sub = &sub;
                    break;
                }
            }
            break;
        }
    }

    if (!active_sub) {
        ImGui::SetCursorPos(ImVec2(20, 40));
        ImGui::TextColored(TEXT_DIM, "Select a topic from the sidebar.");
        return;
    }

    float pad = 20.0f;
    ImGui::SetCursorPosX(pad);

    // Title
    ImGui::PushStyleColor(ImGuiCol_Text, ACCENT);
    ImGui::SetWindowFontScale(1.2f);
    ImGui::Text("%s", active_sub->title.c_str());
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();

    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

    // Content text
    ImGui::SetCursorPosX(pad);
    ImGui::PushTextWrapPos(ImGui::GetWindowWidth() - pad);
    render_text_block(active_sub->content);
    ImGui::PopTextWrapPos();

    // Code example
    if (active_sub->code_example.has_value()) {
        ImGui::Spacing(); ImGui::Spacing();
        ImGui::SetCursorPosX(pad);
        ImGui::TextColored(TEXT_DIM, "Example:");
        ImGui::Spacing();
        render_code_block(active_sub->code_example.value());
    }

    // Prev / Next navigation
    ImGui::Spacing(); ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

    ImGui::SetCursorPosX(pad);
    if (theme::SecondaryButton("<< Previous")) {
        navigate_relative(-1);
    }
    ImGui::SameLine(0, 12);
    if (theme::SecondaryButton("Next >>")) {
        navigate_relative(1);
    }
}

// =============================================================================
// Output Panel
// =============================================================================
void DocsScreen::render_output_panel(float width) {
    using namespace theme::colors;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##docs_output", ImVec2(width, 0), ImGuiChildFlags_Borders);

    ImGui::Spacing();
    ImGui::SetCursorPosX(12);
    ImGui::TextColored(TEXT_DIM, "OUTPUT");
    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

    ImGui::SetCursorPosX(12);
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + width - 24);
    ImGui::TextColored(TEXT_DISABLED, "Code execution is not available in this version.");
    ImGui::Spacing(); ImGui::Spacing();
    ImGui::TextColored(TEXT_DIM, "Documentation v%s", config::APP_VERSION);
    ImGui::TextColored(TEXT_DIM, "Sections: %zu", sections_.size());

    int total_topics = 0;
    for (auto& s : sections_) total_topics += static_cast<int>(s.subsections.size());
    ImGui::TextColored(TEXT_DIM, "Topics: %d", total_topics);
    ImGui::PopTextWrapPos();

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// =============================================================================
// Footer (rendered by App — not used here)
// =============================================================================
void DocsScreen::render_footer() {}

// =============================================================================
// Navigation
// =============================================================================
void DocsScreen::navigate_to(const std::string& section_id, const std::string& sub_id) {
    active_section_id_ = section_id;
    active_subsection_id_ = sub_id;
    expanded_sections_[section_id] = true;
}

void DocsScreen::navigate_relative(int direction) {
    // Flatten all subsections into a linear list
    struct FlatItem { std::string sec_id, sub_id; };
    std::vector<FlatItem> flat;
    for (auto& s : sections_) {
        for (auto& sub : s.subsections) {
            flat.push_back({s.id, sub.id});
        }
    }

    // Find current
    int cur = -1;
    for (int i = 0; i < static_cast<int>(flat.size()); i++) {
        if (flat[i].sec_id == active_section_id_ && flat[i].sub_id == active_subsection_id_) {
            cur = i;
            break;
        }
    }

    if (cur < 0) return;

    int next = cur + direction;
    if (next >= 0 && next < static_cast<int>(flat.size())) {
        navigate_to(flat[next].sec_id, flat[next].sub_id);
    }
}

// =============================================================================
// Search
// =============================================================================
void DocsScreen::perform_search() {
    search_results_.clear();
    std::string q = to_lower(search_query_);

    for (auto& s : sections_) {
        for (auto& sub : s.subsections) {
            std::string lower_title = to_lower(sub.title);
            std::string lower_content = to_lower(sub.content);

            if (lower_title.find(q) != std::string::npos ||
                lower_content.find(q) != std::string::npos) {
                // Extract context around match
                std::string ctx;
                auto pos = lower_content.find(q);
                if (pos != std::string::npos) {
                    int start = static_cast<int>(pos) - 40;
                    if (start < 0) start = 0;
                    int len = 120;
                    if (start + len > static_cast<int>(sub.content.size()))
                        len = static_cast<int>(sub.content.size()) - start;
                    ctx = "..." + sub.content.substr(start, len) + "...";
                } else {
                    ctx = sub.content.substr(0, 120) + "...";
                }

                search_results_.push_back({s.id, s.title, sub.id, sub.title, ctx});
            }
        }
    }
}

// =============================================================================
// Text rendering — basic formatting
// =============================================================================
void DocsScreen::render_text_block(const std::string& text) {
    using namespace theme::colors;

    // Split by lines and render with basic formatting
    size_t pos = 0;
    while (pos < text.size()) {
        size_t eol = text.find('\n', pos);
        if (eol == std::string::npos) eol = text.size();

        std::string line = text.substr(pos, eol - pos);
        pos = eol + 1;

        if (line.empty()) {
            ImGui::Spacing();
            continue;
        }

        // Detect formatting
        if (line.size() >= 2 && line[0] == '-' && line[1] == ' ') {
            // Bullet point
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 8);
            ImGui::TextColored(ACCENT, "*");
            ImGui::SameLine(0, 6);
            ImGui::TextWrapped("%s", line.c_str() + 2);
        } else if (line.size() >= 2 && line[0] == ' ' && line[1] == ' ') {
            // Indented line (like code or description)
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 16);
            ImGui::TextColored(TEXT_DIM, "%s", line.c_str() + 2);
        } else {
            // Normal text — check for heading-like lines (all caps or ending with :)
            bool is_heading = (!line.empty() && line.back() == ':');
            if (is_heading) {
                ImGui::TextColored(TEXT_PRIMARY, "%s", line.c_str());
            } else {
                ImGui::TextColored(TEXT_SECONDARY, "%s", line.c_str());
            }
        }
    }
}

void DocsScreen::render_code_block(const std::string& code) {
    using namespace theme::colors;

    float pad = 20.0f;
    ImGui::SetCursorPosX(pad);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARKEST);
    ImGui::BeginChild("##code_block", ImVec2(ImGui::GetContentRegionAvail().x - pad, 0),
        ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders);

    ImGui::SetCursorPos(ImVec2(12, 8));
    ImGui::PushTextWrapPos(ImGui::GetWindowWidth() - 12);

    // Render code with monospace color
    ImGui::TextColored(MARKET_GREEN, "%s", code.c_str());

    ImGui::PopTextWrapPos();
    ImGui::Spacing();
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

} // namespace fincept::docs
