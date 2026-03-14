#include "news_screen.h"
#include "ui/theme.h"
#include <imgui.h>
#include <algorithm>
#include <cstdio>
#include <ctime>
#include <map>
#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

namespace fincept::news {

// ============================================================================
// Toolbar
// ============================================================================
void NewsScreen::render_toolbar() {
    using namespace theme::colors;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARK);
    ImGui::BeginChild("##news_toolbar", ImVec2(0, (ImGui::GetFrameHeight() + 4) * 2), ImGuiChildFlags_Borders);

    float avail_w = ImGui::GetContentRegionAvail().x;

    // Row 1: Refresh | Search | Article count
    if (loading_) {
        ImGui::TextColored(ACCENT, "LOADING...");
    } else {
        if (theme::AccentButton("REFRESH")) {
            fetch_news();
        }
    }
    ImGui::SameLine(0, 8);
    ImGui::TextColored(TEXT_DIM, "SEARCH:");
    ImGui::SameLine(0, 4);
    float search_w = std::min(200.0f, avail_w * 0.25f);
    ImGui::PushItemWidth(search_w);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, BG_WIDGET);
    ImGui::InputText("##news_search", search_buf_, sizeof(search_buf_));
    ImGui::PopStyleColor();
    ImGui::PopItemWidth();

    // Article count (right-aligned)
    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        char count_buf[64];
        std::snprintf(count_buf, sizeof(count_buf), "%d ARTICLES", (int)articles_.size());
        float tw = ImGui::CalcTextSize(count_buf).x + 8;
        float right_pos = avail_w - tw;
        if (right_pos > ImGui::GetCursorPosX() + 20) {
            ImGui::SameLine(right_pos);
            ImGui::TextColored(TEXT_DIM, "%s", count_buf);
        }
    }

    // Row 2: Time filters | Category filter
    ImGui::TextColored(TEXT_DIM, "TIME:");
    ImGui::SameLine(0, 4);
    const char* time_labels[] = {"ALL", "1H", "4H", "24H", "7D"};
    for (int i = 0; i < 5; i++) {
        bool active = (time_filter_ == i);
        if (active) {
            ImGui::PushStyleColor(ImGuiCol_Button, ACCENT);
            ImGui::PushStyleColor(ImGuiCol_Text, BG_DARKEST);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, BG_WIDGET);
            ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DIM);
        }
        char label[16];
        std::snprintf(label, sizeof(label), "%s##tf%d", time_labels[i], i);
        if (ImGui::SmallButton(label)) time_filter_ = i;
        ImGui::PopStyleColor(2);
        ImGui::SameLine(0, 2);
    }

    ImGui::SameLine(0, 12);
    ImGui::TextColored(TEXT_DIM, "CAT:");
    ImGui::SameLine(0, 4);
    float combo_w = std::min(110.0f, avail_w * 0.12f);
    ImGui::PushItemWidth(combo_w);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, BG_WIDGET);
    if (ImGui::BeginCombo("##cat_filter", CATEGORIES[category_filter_], ImGuiComboFlags_NoArrowButton)) {
        for (int i = 0; i < NUM_CATEGORIES; i++) {
            bool sel = (category_filter_ == i);
            if (ImGui::Selectable(CATEGORIES[i], sel))
                category_filter_ = i;
        }
        ImGui::EndCombo();
    }
    ImGui::PopStyleColor();
    ImGui::PopItemWidth();

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Wire Row — single dense article line
// ============================================================================
void NewsScreen::render_wire_row(int index, const NewsArticle& article, float width) {
    using namespace theme::colors;

    bool selected = (index == selected_article_);
    bool is_hot = (article.priority == Priority::FLASH || article.priority == Priority::URGENT);

    ImVec4 pri_color;
    switch (article.priority) {
        case Priority::FLASH:    pri_color = ERROR_RED; break;
        case Priority::URGENT:   pri_color = ACCENT; break;
        case Priority::BREAKING: pri_color = WARNING; break;
        default:                 pri_color = TEXT_DISABLED; break;
    }

    ImVec4 sent_color;
    switch (article.sentiment) {
        case Sentiment::BULLISH: sent_color = MARKET_GREEN; break;
        case Sentiment::BEARISH: sent_color = MARKET_RED; break;
        default:                 sent_color = WARNING; break;
    }

    ImVec2 cursor = ImGui::GetCursorScreenPos();
    float row_h = ImGui::GetTextLineHeightWithSpacing() + 4;
    ImVec4 bg = selected ? ImVec4(INFO.x, INFO.y, INFO.z, 0.08f) :
                ImVec4(0, 0, 0, 0);

    ImGui::PushID(index);

    if (ImGui::InvisibleButton("##row", ImVec2(width - 16, row_h))) {
        selected_article_ = index;
    }
    bool hovered = ImGui::IsItemHovered();
    if (hovered) bg = ImVec4(1, 1, 1, 0.03f);

    ImDrawList* dl = ImGui::GetWindowDrawList();

    dl->AddRectFilled(cursor, ImVec2(cursor.x + width - 16, cursor.y + row_h),
        ImGui::ColorConvertFloat4ToU32(bg));

    if (selected) {
        dl->AddRectFilled(cursor, ImVec2(cursor.x + 2, cursor.y + row_h),
            ImGui::ColorConvertFloat4ToU32(INFO));
    } else if (is_hot) {
        dl->AddRectFilled(cursor, ImVec2(cursor.x + 2, cursor.y + row_h),
            ImGui::ColorConvertFloat4ToU32(pri_color));
    }

    dl->AddLine(ImVec2(cursor.x, cursor.y + row_h),
                ImVec2(cursor.x + width - 16, cursor.y + row_h),
                ImGui::ColorConvertFloat4ToU32(BORDER_DIM));

    float row_w = width - 16;
    float x = cursor.x + 6;
    float y = cursor.y + 2;

    float time_w   = row_w * 0.05f;
    float dot_w    = 14.0f;
    float source_w = row_w * 0.10f;
    float sent_w   = 16.0f;
    float tail_w   = sent_w + 8;
    float head_w   = row_w - time_w - dot_w - source_w - tail_w - 12;

    // Time
    std::string rel = relative_time(article.sort_ts);
    ImGui::PushClipRect(ImVec2(x, y), ImVec2(x + time_w, y + ImGui::GetTextLineHeight()), true);
    dl->AddText(ImVec2(x, y), ImGui::ColorConvertFloat4ToU32(TEXT_DISABLED), rel.c_str());
    ImGui::PopClipRect();
    x += time_w;

    // Priority dot
    float dot_y = y + ImGui::GetTextLineHeight() * 0.5f;
    dl->AddCircleFilled(ImVec2(x + 5, dot_y), 2.5f,
        ImGui::ColorConvertFloat4ToU32(pri_color));
    if (is_hot) {
        dl->AddCircle(ImVec2(x + 5, dot_y), 4.0f,
            ImGui::ColorConvertFloat4ToU32(ImVec4(pri_color.x, pri_color.y, pri_color.z, 0.3f)));
    }
    x += dot_w;

    // Source
    ImGui::PushClipRect(ImVec2(x, y), ImVec2(x + source_w, y + ImGui::GetTextLineHeight()), true);
    dl->AddText(ImVec2(x, y), ImGui::ColorConvertFloat4ToU32(INFO), article.source.c_str());
    ImGui::PopClipRect();
    x += source_w + 4;

    // Headline
    if (head_w > 10) {
        ImVec4 hl_color = selected ? TEXT_PRIMARY :
                          is_hot ? TEXT_SECONDARY : TEXT_DIM;
        ImGui::PushClipRect(ImVec2(x, y), ImVec2(x + head_w, y + ImGui::GetTextLineHeight()), true);
        dl->AddText(ImVec2(x, y), ImGui::ColorConvertFloat4ToU32(hl_color),
            article.headline.c_str());
        ImGui::PopClipRect();
    }

    // Sentiment arrow
    float right_x = cursor.x + row_w - sent_w;
    const char* sent_icon = article.sentiment == Sentiment::BULLISH ? "^" :
                            article.sentiment == Sentiment::BEARISH ? "v" : "-";
    dl->AddText(ImVec2(right_x, y), ImGui::ColorConvertFloat4ToU32(sent_color), sent_icon);

    ImGui::PopID();
}

// ============================================================================
// Left Sidebar
// ============================================================================
void NewsScreen::render_left_sidebar(float width, float height) {
    using namespace theme::colors;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##news_left", ImVec2(width, height), ImGuiChildFlags_Borders);

    auto filtered = get_filtered_articles();

    // Sentiment summary
    ImGui::TextColored(ACCENT, "SENTIMENT");
    ImGui::Separator();
    int bullish = 0, bearish = 0, neutral = 0;
    for (const auto& a : filtered) {
        if (a.sentiment == Sentiment::BULLISH) bullish++;
        else if (a.sentiment == Sentiment::BEARISH) bearish++;
        else neutral++;
    }
    int total = (int)filtered.size();
    if (total > 0) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "BULLISH  %d (%.0f%%)", bullish, 100.0f * bullish / total);
        ImGui::TextColored(MARKET_GREEN, "%s", buf);
        std::snprintf(buf, sizeof(buf), "BEARISH  %d (%.0f%%)", bearish, 100.0f * bearish / total);
        ImGui::TextColored(MARKET_RED, "%s", buf);
        std::snprintf(buf, sizeof(buf), "NEUTRAL  %d (%.0f%%)", neutral, 100.0f * neutral / total);
        ImGui::TextColored(WARNING, "%s", buf);
    }

    ImGui::Spacing();
    ImGui::Spacing();

    // Top Stories
    ImGui::TextColored(ACCENT, "TOP STORIES");
    ImGui::Separator();

    auto top = filtered;
    std::sort(top.begin(), top.end(), [](const NewsArticle& a, const NewsArticle& b) {
        auto pri_weight = [](Priority p) -> int {
            switch (p) { case Priority::FLASH: return 4; case Priority::URGENT: return 3;
                         case Priority::BREAKING: return 2; default: return 1; }
        };
        int wa = pri_weight(a.priority) * 1000 + (int)(a.sort_ts / 60);
        int wb = pri_weight(b.priority) * 1000 + (int)(b.sort_ts / 60);
        return wa > wb;
    });

    int count = std::min((int)top.size(), 10);
    for (int i = 0; i < count; i++) {
        ImGui::PushID(i + 1000);

        char rank[8]; std::snprintf(rank, sizeof(rank), "%d", i + 1);
        ImGui::TextColored(ACCENT, "%s", rank);
        ImGui::SameLine(0, 6);

        float avail = ImGui::GetContentRegionAvail().x;
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + avail);

        ImVec4 hl_col = top[i].priority == Priority::FLASH ? TEXT_PRIMARY :
                        top[i].priority == Priority::URGENT ? TEXT_SECONDARY : TEXT_DIM;

        ImGui::TextColored(hl_col, "%s", top[i].headline.c_str());
        if (ImGui::IsItemClicked()) {
            for (int j = 0; j < (int)filtered.size(); j++) {
                if (filtered[j].id == top[i].id) { selected_article_ = j; break; }
            }
        }
        ImGui::PopTextWrapPos();

        ImGui::TextColored(INFO, "%s", top[i].source.c_str());
        ImGui::SameLine(0, 6);
        ImGui::TextColored(TEXT_DISABLED, "%s", relative_time(top[i].sort_ts).c_str());

        ImGui::Spacing();
        ImGui::PopID();
    }

    ImGui::Spacing();
    ImGui::Spacing();

    // Category counts
    ImGui::TextColored(ACCENT, "CATEGORIES");
    ImGui::Separator();
    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        std::map<std::string, int> cat_counts;
        for (const auto& a : articles_) cat_counts[a.category]++;
        float cat_avail = ImGui::GetContentRegionAvail().x;
        for (const auto& [cat, cnt] : cat_counts) {
            ImGui::TextColored(TEXT_DIM, "%s", cat.c_str());
            char cnt_buf[16]; std::snprintf(cnt_buf, sizeof(cnt_buf), "%d", cnt);
            ImGui::SameLine(cat_avail - ImGui::CalcTextSize(cnt_buf).x);
            ImGui::TextColored(TEXT_SECONDARY, "%s", cnt_buf);
        }
    }

    ImGui::Spacing();
    ImGui::Spacing();

    // Source counts
    ImGui::TextColored(ACCENT, "SOURCES");
    ImGui::Separator();
    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        std::map<std::string, int> src_counts;
        for (const auto& a : articles_) src_counts[a.source]++;
        float src_avail = ImGui::GetContentRegionAvail().x;
        for (const auto& [src, cnt] : src_counts) {
            ImGui::TextColored(TEXT_DIM, "%s", src.c_str());
            char cnt_buf[16]; std::snprintf(cnt_buf, sizeof(cnt_buf), "%d", cnt);
            ImGui::SameLine(src_avail - ImGui::CalcTextSize(cnt_buf).x);
            ImGui::TextColored(TEXT_SECONDARY, "%s", cnt_buf);
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Wire Feed — center panel
// ============================================================================
void NewsScreen::render_wire_feed(float width, float height) {
    using namespace theme::colors;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARKEST);
    ImGui::BeginChild("##wire_feed", ImVec2(width, height), ImGuiChildFlags_Borders);

    auto filtered = get_filtered_articles();

    if (loading_ && filtered.empty()) {
        ImGui::SetCursorPos(ImVec2(width * 0.4f, height * 0.4f));
        ImGui::TextColored(ACCENT, "LOADING NEWS FEEDS...");
        theme::LoadingSpinner("Fetching...");
    } else if (filtered.empty()) {
        ImGui::SetCursorPos(ImVec2(width * 0.3f, height * 0.4f));
        ImGui::TextColored(TEXT_DIM, "No articles match your filters");
    } else {
        float hw = ImGui::GetContentRegionAvail().x;
        ImGui::TextColored(TEXT_DISABLED, "TIME");
        ImGui::SameLine(hw * 0.07f);
        ImGui::TextColored(TEXT_DISABLED, "SOURCE");
        ImGui::SameLine(hw * 0.20f);
        ImGui::TextColored(TEXT_DISABLED, "HEADLINE");
        ImGui::Separator();

        ImGui::BeginChild("##wire_scroll", ImVec2(0, 0));
        for (int i = 0; i < (int)filtered.size(); i++) {
            render_wire_row(i, filtered[i], width);
        }
        ImGui::EndChild();
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Article Reader — right panel
// ============================================================================
void NewsScreen::render_article_reader(float width, float height) {
    using namespace theme::colors;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##article_reader", ImVec2(width, height), ImGuiChildFlags_Borders);

    auto filtered = get_filtered_articles();

    if (selected_article_ < 0 || selected_article_ >= (int)filtered.size()) {
        ImGui::SetCursorPos(ImVec2(width * 0.15f, height * 0.4f));
        ImGui::TextColored(TEXT_DISABLED, "Select an article to read");
    } else {
        const auto& a = filtered[selected_article_];

        // Priority badge
        const char* pri_label = "ROUTINE";
        ImVec4 pri_color = TEXT_DISABLED;
        switch (a.priority) {
            case Priority::FLASH:    pri_label = "FLASH"; pri_color = ERROR_RED; break;
            case Priority::URGENT:   pri_label = "URGENT"; pri_color = ACCENT; break;
            case Priority::BREAKING: pri_label = "BREAKING"; pri_color = WARNING; break;
            default: break;
        }
        ImGui::TextColored(pri_color, "[%s]", pri_label);
        ImGui::SameLine(0, 8);

        // Sentiment badge
        const char* sent_label = "NEUTRAL";
        ImVec4 sent_color = WARNING;
        switch (a.sentiment) {
            case Sentiment::BULLISH: sent_label = "BULLISH"; sent_color = MARKET_GREEN; break;
            case Sentiment::BEARISH: sent_label = "BEARISH"; sent_color = MARKET_RED; break;
            default: break;
        }
        ImGui::TextColored(sent_color, "[%s]", sent_label);

        ImGui::Spacing();

        // Headline
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + width - 20);
        ImGui::TextColored(TEXT_PRIMARY, "%s", a.headline.c_str());
        ImGui::PopTextWrapPos();

        ImGui::Spacing();

        // Metadata
        {
            char meta[256];
            std::snprintf(meta, sizeof(meta), "%s | %s | %s | %s",
                a.source.c_str(), a.time_str.c_str(), a.category.c_str(), a.region.c_str());
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + width - 20);
            ImGui::TextColored(INFO, "%s", meta);
            ImGui::PopTextWrapPos();
        }

        // Tickers
        if (!a.tickers.empty()) {
            ImGui::Spacing();
            for (const auto& t : a.tickers) {
                ImGui::SameLine(0, 4);
                ImGui::TextColored(WARNING, "$%s", t.c_str());
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Summary
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + width - 20);
        ImGui::TextColored(TEXT_SECONDARY, "%s", a.summary.empty() ? "No summary available." : a.summary.c_str());
        ImGui::PopTextWrapPos();

        ImGui::Spacing();
        ImGui::Spacing();

        // Open link button
        if (!a.link.empty()) {
            if (theme::AccentButton("OPEN IN BROWSER")) {
#ifdef _WIN32
                ShellExecuteA(nullptr, "open", a.link.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#elif __APPLE__
                std::string cmd = "open " + a.link;
                (void)system(cmd.c_str());
#else
                std::string cmd = "xdg-open " + a.link;
                (void)system(cmd.c_str());
#endif
            }
            ImGui::SameLine(0, 8);
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + width - 120);
            ImGui::TextColored(TEXT_DISABLED, "%s", a.link.c_str());
            ImGui::PopTextWrapPos();
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

} // namespace fincept::news
