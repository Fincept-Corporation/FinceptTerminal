#pragma once
// Documentation screen — static docs viewer with sidebar, content, search

#include "docs_types.h"
#include <imgui.h>
#include <vector>
#include <map>
#include <string>

namespace fincept::docs {

class DocsScreen {
public:
    void render();

private:
    // Data
    std::vector<DocSection> sections_;
    bool initialized_ = false;

    // Navigation state
    std::map<std::string, bool> expanded_sections_;
    std::string active_section_id_;
    std::string active_subsection_id_;

    // Search
    char search_buf_[256] = {};
    std::string search_query_;
    bool show_search_results_ = false;

    struct SearchResult {
        std::string section_id;
        std::string section_title;
        std::string sub_id;
        std::string sub_title;
        std::string context;
    };
    std::vector<SearchResult> search_results_;

    // Render
    void render_header();
    void render_sidebar(float width);
    void render_content();
    void render_output_panel(float width);
    void render_footer();

    // Navigation
    void navigate_to(const std::string& section_id, const std::string& sub_id);
    void navigate_relative(int direction);

    // Search
    void perform_search();

    // Content helpers
    void render_text_block(const std::string& text);
    void render_code_block(const std::string& code);
};

} // namespace fincept::docs
