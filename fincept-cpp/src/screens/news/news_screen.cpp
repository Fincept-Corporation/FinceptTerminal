#include "news_screen.h"
#include "http/http_client.h"
#include "core/logger.h"
#include "ui/yoga_helpers.h"
#include <imgui.h>
#include <algorithm>
#include <ctime>
#include <cstdio>
#include <thread>

namespace fincept::news {

// ============================================================================
// Init
// ============================================================================
void NewsScreen::init() {
    if (initialized_) return;
    initialized_ = true;
    fetch_news();
}

// ============================================================================
// Fetch news from all feeds (background thread)
// ============================================================================
void NewsScreen::fetch_news() {
    if (loading_) return;
    loading_ = true;
    error_.clear();

    std::thread([this]() {
        auto feeds = get_default_feeds();
        auto& http = http::HttpClient::instance();
        std::vector<NewsArticle> all_articles;

        int success_count = 0;
        int fail_count = 0;

        for (const auto& feed : feeds) {
            if (!feed.enabled) continue;

            http::Headers hdrs;
            hdrs["User-Agent"] = "FinceptTerminal/4.0.0";
            hdrs["Accept"] = "application/rss+xml, application/xml, text/xml, */*";

            auto resp = http.get(feed.url, hdrs);
            if (resp.success && !resp.body.empty()) {
                auto parsed = parse_rss_xml(resp.body, feed);
                all_articles.insert(all_articles.end(), parsed.begin(), parsed.end());
                success_count++;
            } else {
                fail_count++;
            }
        }

        // Sort by timestamp descending (newest first)
        std::sort(all_articles.begin(), all_articles.end(),
            [](const NewsArticle& a, const NewsArticle& b) { return a.sort_ts > b.sort_ts; });

        {
            std::lock_guard<std::mutex> lock(data_mutex_);
            articles_ = std::move(all_articles);
            if (success_count == 0) {
                error_ = "Failed to fetch news from all feeds";
            } else if (fail_count > 0) {
                char buf[64];
                std::snprintf(buf, sizeof(buf), "Fetched %d feeds (%d failed)", success_count, fail_count);
                error_ = buf;
            }
        }

        loading_ = false;
        last_fetch_time_ = (double)std::time(nullptr);
        LOG_INFO("News", "Fetched %d articles from %d feeds", (int)articles_.size(), success_count);
    }).detach();
}

// ============================================================================
// Get filtered articles based on UI state
// ============================================================================
std::vector<NewsArticle> NewsScreen::get_filtered_articles() {
    std::lock_guard<std::mutex> lock(data_mutex_);
    std::vector<NewsArticle> filtered;

    int64_t now = (int64_t)std::time(nullptr);
    int64_t time_cutoff = 0;
    switch (time_filter_) {
        case 1: time_cutoff = now - 3600; break;
        case 2: time_cutoff = now - 4 * 3600; break;
        case 3: time_cutoff = now - 86400; break;
        case 4: time_cutoff = now - 7 * 86400; break;
    }

    std::string search_lower;
    if (search_buf_[0]) {
        search_lower = search_buf_;
        std::transform(search_lower.begin(), search_lower.end(), search_lower.begin(), ::tolower);
    }

    for (const auto& a : articles_) {
        if (time_cutoff > 0 && a.sort_ts < time_cutoff) continue;

        if (category_filter_ > 0) {
            if (a.category != CATEGORIES[category_filter_]) continue;
        }

        if (!search_lower.empty()) {
            std::string h = a.headline;
            std::transform(h.begin(), h.end(), h.begin(), ::tolower);
            std::string s = a.source;
            std::transform(s.begin(), s.end(), s.begin(), ::tolower);
            if (h.find(search_lower) == std::string::npos &&
                s.find(search_lower) == std::string::npos)
                continue;
        }

        filtered.push_back(a);
    }

    return filtered;
}

// ============================================================================
// Main Render — Yoga-based responsive layout
// ============================================================================
void NewsScreen::render() {
    init();

    using namespace theme::colors;

    // ScreenFrame replaces all manual viewport math
    ui::ScreenFrame frame("##news", ImVec2(4, 4), BG_DARKEST);
    if (!frame.begin()) { frame.end(); return; }

    // Title bar
    float content_w = ImGui::GetContentRegionAvail().x;
    ImGui::TextColored(ACCENT, "NEWS TERMINAL");

    {
        char info[64];
        std::snprintf(info, sizeof(info), "%d FEEDS | %d ARTICLES",
            (int)get_default_feeds().size(), (int)articles_.size());
        float info_w = ImGui::CalcTextSize(info).x + 8;
        float right_pos = content_w - info_w;
        if (right_pos > ImGui::GetCursorPosX() + 50) {
            ImGui::SameLine(right_pos);
            ImGui::TextColored(TEXT_DISABLED, "%s", info);
        }
    }

    ImGui::Separator();

    // Toolbar
    render_toolbar();

    // Error banner
    if (!error_.empty()) {
        bool is_error = error_.find("Failed") != std::string::npos;
        ImGui::TextColored(is_error ? ERROR_RED : WARNING, "%s", error_.c_str());
    }

    // Responsive three-panel layout via Yoga
    float avail_w = ImGui::GetContentRegionAvail().x;
    float avail_h = ImGui::GetContentRegionAvail().y;
    auto panels = ui::three_panel_layout(avail_w, avail_h);

    if (frame.is_compact()) {
        // Stack vertically on narrow screens
        render_left_sidebar(panels.left_w, panels.left_h);
        render_wire_feed(panels.center_w, panels.center_h);
        render_article_reader(panels.right_w, panels.right_h);
    } else if (frame.is_medium()) {
        // Two panels: sidebar + feed (no article reader)
        render_left_sidebar(panels.left_w, panels.left_h);
        ImGui::SameLine(0, panels.gap);
        render_wire_feed(panels.center_w, panels.center_h);
    } else {
        // Full three-panel
        render_left_sidebar(panels.left_w, panels.left_h);
        ImGui::SameLine(0, panels.gap);
        render_wire_feed(panels.center_w, panels.center_h);
        ImGui::SameLine(0, panels.gap);
        render_article_reader(panels.right_w, panels.right_h);
    }

    frame.end();

    // Auto-refresh every 5 minutes
    double now = (double)std::time(nullptr);
    if (!loading_ && last_fetch_time_ > 0 && (now - last_fetch_time_) > 300) {
        fetch_news();
    }
}

} // namespace fincept::news
