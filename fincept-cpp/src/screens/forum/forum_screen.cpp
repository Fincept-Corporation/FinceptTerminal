#include "forum_screen.h"
#include "ui/theme.h"
#include "core/config.h"
#include "auth/auth_manager.h"
#include "auth/auth_api.h"
#include "http/http_client.h"
#include <imgui.h>
#include <nlohmann/json.hpp>
#include <cstdio>
#include <cstring>
#include <algorithm>

namespace fincept::forum {

using json = nlohmann::json;

static const char* BASE_URL = "https://api.fincept.in";

// =============================================================================
// Main render
// =============================================================================
void ForumScreen::render() {
    using namespace theme::colors;

    ImGuiViewport* vp = ImGui::GetMainViewport();
    float tab_h = ImGui::GetFrameHeight() + 4;
    float footer_h = ImGui::GetFrameHeight();
    float status_h = 28.0f;

    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x, vp->WorkPos.y + tab_h));
    ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x, vp->WorkSize.y - tab_h - footer_h));

    ImGui::PushStyleColor(ImGuiCol_WindowBg, BG_DARK);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

    ImGui::Begin("##forum_screen", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoBringToFrontOnFocus);

    // Initialize
    if (!data_loaded_) {
        top_contributors_ = get_top_contributors();
        data_loaded_ = true;
        load_data();
    }

    // Check async
    if (load_future_.valid() &&
        load_future_.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
        load_future_.get();
        loading_ = false;
    }

    render_header();

    if (current_view_ == ForumView::Posts) {
        render_category_bar();

        float avail_w = ImGui::GetContentRegionAvail().x;
        float avail_h = ImGui::GetContentRegionAvail().y - status_h;
        float left_w = 260.0f;
        float right_w = 300.0f;
        float center_w = avail_w - left_w - right_w;

        if (center_w < 300.0f) {
            right_w = 0;
            center_w = avail_w - left_w;
        }

        render_left_panel(left_w, avail_h);
        ImGui::SameLine(0, 0);
        render_center_panel(center_w, avail_h);
        if (right_w > 0) {
            ImGui::SameLine(0, 0);
            render_right_panel(right_w, avail_h);
        }
    } else if (current_view_ == ForumView::PostDetail) {
        render_post_detail();
    } else if (current_view_ == ForumView::CreatePost) {
        render_create_post();
    } else if (current_view_ == ForumView::Search) {
        render_search_view();
    }

    render_status_bar();

    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

// =============================================================================
// Header
// =============================================================================
void ForumScreen::render_header() {
    using namespace theme::colors;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARKEST);
    ImGui::BeginChild("##forum_header", ImVec2(0, 40), ImGuiChildFlags_None);

    ImGui::SetCursorPos(ImVec2(12, 10));
    ImGui::TextColored(ACCENT, "GLOBAL FORUM");

    // Stats
    ImGui::SameLine(0, 20);
    ImGui::TextColored(TEXT_DIM, "Posts:");
    ImGui::SameLine(0, 4);
    ImGui::TextColored(INFO, "%d", stats_.total_posts);

    ImGui::SameLine(0, 16);
    ImGui::TextColored(TEXT_DIM, "Online:");
    ImGui::SameLine(0, 4);
    ImGui::TextColored(SUCCESS, "%d", stats_.active_users);

    ImGui::SameLine(0, 16);
    ImGui::TextColored(TEXT_DIM, "Today:");
    ImGui::SameLine(0, 4);
    ImGui::TextColored(WARNING, "%d", stats_.posts_today);

    // Action buttons on right
    float right_x = ImGui::GetWindowWidth() - 320;
    ImGui::SameLine(right_x);

    if (theme::AccentButton("+ New Post")) {
        current_view_ = ForumView::CreatePost;
        std::memset(new_title_, 0, sizeof(new_title_));
        std::memset(new_content_, 0, sizeof(new_content_));
        new_category_idx_ = 0;
    }
    ImGui::SameLine(0, 8);
    if (theme::SecondaryButton("Search")) {
        current_view_ = ForumView::Search;
        std::memset(search_buf_, 0, sizeof(search_buf_));
        search_results_.clear();
    }
    ImGui::SameLine(0, 8);
    if (theme::SecondaryButton("Refresh")) {
        load_data();
    }

    if (loading_) {
        ImGui::SameLine(0, 8);
        theme::LoadingSpinner();
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// =============================================================================
// Category Bar
// =============================================================================
void ForumScreen::render_category_bar() {
    using namespace theme::colors;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##forum_catbar", ImVec2(0, 32), ImGuiChildFlags_Borders);

    ImGui::SetCursorPos(ImVec2(8, 4));

    // ALL button
    bool is_all = (active_category_ == "ALL");
    if (is_all) {
        ImGui::PushStyleColor(ImGuiCol_Button, ACCENT_BG);
        ImGui::PushStyleColor(ImGuiCol_Text, ACCENT);
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_Text, TEXT_SECONDARY);
    }
    if (ImGui::SmallButton("ALL")) {
        active_category_ = "ALL";
        active_category_id_ = -1;
        apply_category_filter();
    }
    ImGui::PopStyleColor(2);

    // Category buttons
    for (auto& cat : categories_) {
        ImGui::SameLine(0, 4);
        bool is_active = (active_category_ == cat.name);

        if (is_active) {
            ImGui::PushStyleColor(ImGuiCol_Button, ACCENT_BG);
            ImGui::PushStyleColor(ImGuiCol_Text, ACCENT);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_Text, TEXT_SECONDARY);
        }

        char label[64];
        std::snprintf(label, sizeof(label), "%s (%d)", cat.name.c_str(), cat.post_count);
        if (ImGui::SmallButton(label)) {
            active_category_ = cat.name;
            active_category_id_ = cat.id;
            apply_category_filter();
        }
        ImGui::PopStyleColor(2);
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// =============================================================================
// Left Panel — stats, contributors, categories
// =============================================================================
void ForumScreen::render_left_panel(float width, float height) {
    using namespace theme::colors;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##forum_left", ImVec2(width, height), ImGuiChildFlags_Borders);

    ImGui::Spacing();

    // Statistics
    ImGui::SetCursorPosX(12);
    theme::SectionHeader("STATISTICS");

    auto stat_row = [&](const char* label, int val, ImVec4 col) {
        ImGui::SetCursorPosX(12);
        ImGui::TextColored(TEXT_DIM, "%s", label);
        ImGui::SameLine(width - 60);
        ImGui::TextColored(col, "%d", val);
    };

    stat_row("Total Posts",   stats_.total_posts,    INFO);
    stat_row("Active Users",  stats_.active_users,   SUCCESS);
    stat_row("Posts Today",   stats_.posts_today,    WARNING);
    stat_row("Total Users",   stats_.total_users,    TEXT_SECONDARY);

    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

    // Top Contributors
    ImGui::SetCursorPosX(12);
    theme::SectionHeader("TOP CONTRIBUTORS");

    for (auto& user : top_contributors_) {
        ImGui::SetCursorPosX(12);
        ImVec4 dot_col = (user.status == "online") ? SUCCESS : TEXT_DISABLED;
        ImGui::TextColored(dot_col, "*");
        ImGui::SameLine(0, 4);
        ImGui::TextColored(TEXT_PRIMARY, "%s", user.username.c_str());
        ImGui::SameLine(width - 60);
        ImGui::TextColored(ACCENT, "%d", user.reputation);
    }

    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

    // Categories list
    ImGui::SetCursorPosX(12);
    theme::SectionHeader("CATEGORIES");

    for (auto& cat : categories_) {
        ImGui::SetCursorPosX(12);
        ImGui::TextColored(TEXT_SECONDARY, "%s", cat.name.c_str());
        ImGui::SameLine(width - 40);
        ImGui::TextColored(TEXT_DIM, "%d", cat.post_count);
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// =============================================================================
// Center Panel — post list
// =============================================================================
void ForumScreen::render_center_panel(float width, float height) {
    using namespace theme::colors;

    ImGui::BeginChild("##forum_center", ImVec2(width, height), ImGuiChildFlags_Borders);

    // Sort controls
    ImGui::SetCursorPos(ImVec2(12, 8));
    ImGui::TextColored(TEXT_DIM, "Sort:");
    ImGui::SameLine(0, 8);

    SortMode modes[] = {SortMode::Latest, SortMode::Popular, SortMode::Views};
    for (auto m : modes) {
        bool is_active = (sort_mode_ == m);
        if (is_active) {
            ImGui::PushStyleColor(ImGuiCol_Button, ACCENT_BG);
            ImGui::PushStyleColor(ImGuiCol_Text, ACCENT);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_Text, TEXT_SECONDARY);
        }
        if (ImGui::SmallButton(sort_mode_label(m))) {
            sort_mode_ = m;
            load_posts();
        }
        ImGui::PopStyleColor(2);
        ImGui::SameLine(0, 4);
    }

    ImGui::Spacing();
    ImGui::Separator();

    // Posts count
    ImGui::SetCursorPosX(12);
    ImGui::TextColored(TEXT_DIM, "%d posts", static_cast<int>(filtered_posts_.size()));
    ImGui::Spacing();

    // Post cards
    float card_w = width - 24;
    for (int i = 0; i < static_cast<int>(filtered_posts_.size()); i++) {
        render_post_card(filtered_posts_[i], i, card_w);
    }

    if (filtered_posts_.empty() && !loading_) {
        ImGui::SetCursorPosX(12);
        ImGui::TextColored(TEXT_DIM, "No posts found.");
    }

    ImGui::EndChild();
}

// =============================================================================
// Post Card
// =============================================================================
void ForumScreen::render_post_card(const ForumPost& post, int idx, float width) {
    using namespace theme::colors;

    ImVec4 border_col = forum_priority_color(post.priority);

    ImGui::SetCursorPosX(12);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);

    char cid[32];
    std::snprintf(cid, sizeof(cid), "##fpost_%d", idx);
    ImGui::BeginChild(cid, ImVec2(width, 110), ImGuiChildFlags_Borders);

    // Left colored border
    ImVec2 p = ImGui::GetWindowPos();
    ImVec2 s = ImGui::GetWindowSize();
    ImGui::GetWindowDrawList()->AddRectFilled(
        p, ImVec2(p.x + 4, p.y + s.y),
        ImGui::ColorConvertFloat4ToU32(border_col));

    // Row 1: meta info
    ImGui::SetCursorPos(ImVec2(16, 6));
    ImGui::TextColored(TEXT_DIM, "%s", post.time.c_str());
    ImGui::SameLine(0, 8);
    ImGui::TextColored(INFO, "@%s", post.author.c_str());
    ImGui::SameLine(0, 8);
    ImGui::TextColored(ACCENT_DIM, "[%s]", post.category.c_str());

    // Badges
    if (!post.priority.empty()) {
        ImGui::SameLine(0, 8);
        ImGui::TextColored(forum_priority_color(post.priority), "%s", post.priority.c_str());
    }
    if (!post.sentiment.empty()) {
        ImGui::SameLine(0, 8);
        ImGui::TextColored(forum_sentiment_color(post.sentiment), "%s", post.sentiment.c_str());
    }
    if (post.verified) {
        ImGui::SameLine(0, 6);
        ImGui::TextColored(SUCCESS, "[v]");
    }

    // Row 2: title
    ImGui::SetCursorPosX(16);
    ImGui::TextColored(TEXT_PRIMARY, "%s", post.title.c_str());

    // Row 3: content preview
    ImGui::SetCursorPosX(16);
    std::string preview = post.content;
    if (preview.size() > 150) preview = preview.substr(0, 150) + "...";
    ImGui::TextColored(TEXT_DIM, "%s", preview.c_str());

    // Row 4: tags + stats
    ImGui::SetCursorPosX(16);

    // Tags
    for (auto& tag : post.tags) {
        ImGui::TextColored(ACCENT_DIM, "#%s", tag.c_str());
        ImGui::SameLine(0, 6);
    }

    // Stats on right
    float stat_x = width - 180;
    ImGui::SameLine(stat_x);
    ImGui::TextColored(TEXT_DIM, "Views:%d", post.views);
    ImGui::SameLine(0, 8);
    ImGui::TextColored(TEXT_DIM, "Re:%d", post.replies);
    ImGui::SameLine(0, 8);
    ImGui::TextColored(MARKET_GREEN, "+%d", post.likes);
    ImGui::SameLine(0, 4);
    ImGui::TextColored(MARKET_RED, "-%d", post.dislikes);

    ImGui::EndChild();
    ImGui::PopStyleColor();

    if (ImGui::IsItemClicked()) {
        selected_post_idx_ = idx;
        selected_post_ = post;
        current_view_ = ForumView::PostDetail;
        load_post_detail(post.id);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }

    ImGui::Spacing();
}

// =============================================================================
// Right Panel — trending, activity, sentiment
// =============================================================================
void ForumScreen::render_right_panel(float width, float height) {
    using namespace theme::colors;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##forum_right", ImVec2(width, height), ImGuiChildFlags_Borders);

    ImGui::Spacing();

    // Trending Topics
    ImGui::SetCursorPosX(12);
    theme::SectionHeader("TRENDING TOPICS");

    for (auto& t : trending_) {
        ImGui::SetCursorPosX(12);
        ImGui::TextColored(TEXT_PRIMARY, "%s", t.topic.c_str());
        ImGui::SameLine(width - 90);
        ImGui::TextColored(TEXT_DIM, "%d", t.mentions);
        ImGui::SameLine(0, 8);
        ImGui::TextColored(forum_sentiment_color(t.sentiment), "%s", t.sentiment.c_str());
    }

    if (trending_.empty()) {
        ImGui::SetCursorPosX(12);
        ImGui::TextColored(TEXT_DIM, "No trending topics");
    }

    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

    // Recent Activity
    ImGui::SetCursorPosX(12);
    theme::SectionHeader("RECENT ACTIVITY");

    for (auto& a : recent_activity_) {
        ImGui::SetCursorPosX(12);
        ImGui::TextColored(INFO, "@%s", a.user.c_str());
        ImGui::SameLine(0, 4);
        ImGui::TextColored(TEXT_DIM, "%s", a.action.c_str());
        ImGui::SameLine(0, 4);
        ImGui::TextColored(TEXT_SECONDARY, "%s", a.target.c_str());
        ImGui::SetCursorPosX(12);
        ImGui::TextColored(TEXT_DISABLED, "%s", a.time.c_str());
    }

    if (recent_activity_.empty()) {
        ImGui::SetCursorPosX(12);
        ImGui::TextColored(TEXT_DIM, "No recent activity");
    }

    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

    // Sentiment Analysis
    ImGui::SetCursorPosX(12);
    theme::SectionHeader("SENTIMENT ANALYSIS");

    if (!posts_.empty()) {
        int bullish = 0, bearish = 0, neutral_count = 0;
        for (auto& post : posts_) {
            if (post.sentiment == "bullish") bullish++;
            else if (post.sentiment == "bearish") bearish++;
            else neutral_count++;
        }
        int total = static_cast<int>(posts_.size());
        float bull_pct = total > 0 ? (bullish * 100.0f / total) : 0;
        float bear_pct = total > 0 ? (bearish * 100.0f / total) : 0;
        float neut_pct = total > 0 ? (neutral_count * 100.0f / total) : 0;

        float bar_w = width - 100;

        // Bullish bar
        ImGui::SetCursorPosX(12);
        ImGui::TextColored(MARKET_GREEN, "Bullish");
        ImGui::SameLine(80);
        ImVec2 bp = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddRectFilled(
            bp, ImVec2(bp.x + bar_w * bull_pct / 100.0f, bp.y + 12),
            ImGui::ColorConvertFloat4ToU32(MARKET_GREEN));
        ImGui::GetWindowDrawList()->AddRect(
            bp, ImVec2(bp.x + bar_w, bp.y + 12),
            ImGui::ColorConvertFloat4ToU32(BORDER_DIM));
        ImGui::Dummy(ImVec2(bar_w, 14));
        ImGui::SameLine(0, 4);
        ImGui::TextColored(TEXT_DIM, "%.0f%%", bull_pct);

        // Bearish bar
        ImGui::SetCursorPosX(12);
        ImGui::TextColored(MARKET_RED, "Bearish");
        ImGui::SameLine(80);
        bp = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddRectFilled(
            bp, ImVec2(bp.x + bar_w * bear_pct / 100.0f, bp.y + 12),
            ImGui::ColorConvertFloat4ToU32(MARKET_RED));
        ImGui::GetWindowDrawList()->AddRect(
            bp, ImVec2(bp.x + bar_w, bp.y + 12),
            ImGui::ColorConvertFloat4ToU32(BORDER_DIM));
        ImGui::Dummy(ImVec2(bar_w, 14));
        ImGui::SameLine(0, 4);
        ImGui::TextColored(TEXT_DIM, "%.0f%%", bear_pct);

        // Neutral bar
        ImGui::SetCursorPosX(12);
        ImGui::TextColored(WARNING, "Neutral");
        ImGui::SameLine(80);
        bp = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddRectFilled(
            bp, ImVec2(bp.x + bar_w * neut_pct / 100.0f, bp.y + 12),
            ImGui::ColorConvertFloat4ToU32(WARNING));
        ImGui::GetWindowDrawList()->AddRect(
            bp, ImVec2(bp.x + bar_w, bp.y + 12),
            ImGui::ColorConvertFloat4ToU32(BORDER_DIM));
        ImGui::Dummy(ImVec2(bar_w, 14));
        ImGui::SameLine(0, 4);
        ImGui::TextColored(TEXT_DIM, "%.0f%%", neut_pct);

        ImGui::Spacing();

        // Overall score
        float score = bull_pct - bear_pct;
        ImGui::SetCursorPosX(12);
        ImGui::TextColored(TEXT_DIM, "Overall:");
        ImGui::SameLine(0, 4);
        ImGui::TextColored(score > 0 ? MARKET_GREEN : (score < 0 ? MARKET_RED : WARNING),
            "%.1f", score);
    } else {
        ImGui::SetCursorPosX(12);
        ImGui::TextColored(TEXT_DIM, "No sentiment data");
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// =============================================================================
// Post Detail
// =============================================================================
void ForumScreen::render_post_detail() {
    using namespace theme::colors;

    ImGui::BeginChild("##post_detail_area", ImVec2(0, 0), ImGuiChildFlags_None);

    float avail_w = ImGui::GetContentRegionAvail().x;
    float detail_w = (avail_w > 800.0f) ? 800.0f : avail_w - 32;
    float pad_x = (avail_w - detail_w) * 0.5f;
    if (pad_x < 16) pad_x = 16;

    ImGui::SetCursorPos(ImVec2(pad_x, 16));
    ImGui::BeginChild("##post_detail", ImVec2(detail_w, 0), ImGuiChildFlags_None);

    // Back button
    if (theme::SecondaryButton("<< Back to Posts")) {
        current_view_ = ForumView::Posts;
        selected_post_idx_ = -1;
    }
    ImGui::Spacing();

    // Post header
    ImGui::TextColored(ACCENT, "%s", selected_post_.title.c_str());
    ImGui::TextColored(TEXT_DIM, "by @%s | %s | %s",
        selected_post_.author.c_str(),
        selected_post_.category.c_str(),
        selected_post_.time.c_str());
    ImGui::Spacing();

    // Badges
    if (!selected_post_.priority.empty()) {
        ImGui::TextColored(forum_priority_color(selected_post_.priority),
            "[%s]", selected_post_.priority.c_str());
        ImGui::SameLine(0, 8);
    }
    if (!selected_post_.sentiment.empty()) {
        ImGui::TextColored(forum_sentiment_color(selected_post_.sentiment),
            "[%s]", selected_post_.sentiment.c_str());
        ImGui::SameLine(0, 8);
    }
    ImGui::TextColored(TEXT_DIM, "Views: %d | Likes: %d | Dislikes: %d",
        selected_post_.views, selected_post_.likes, selected_post_.dislikes);

    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

    // Full content
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + detail_w - 20);
    ImGui::TextColored(TEXT_SECONDARY, "%s", selected_post_.content.c_str());
    ImGui::PopTextWrapPos();

    // Tags
    if (!selected_post_.tags.empty()) {
        ImGui::Spacing();
        for (auto& tag : selected_post_.tags) {
            ImGui::TextColored(ACCENT_DIM, "#%s", tag.c_str());
            ImGui::SameLine(0, 6);
        }
        ImGui::NewLine();
    }

    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

    // Vote buttons
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 4));
    ImGui::PushStyleColor(ImGuiCol_Button, BG_WIDGET);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, BG_HOVER);

    ImGui::PushStyleColor(ImGuiCol_Text, MARKET_GREEN);
    if (ImGui::Button("+ Like")) {
        // Vote
        auto headers = auth::AuthApi::instance().get_auth_headers(
            auth::AuthManager::instance().session().api_key);
        json body;
        body["vote_type"] = "like";
        std::string url = std::string(BASE_URL) + "/forum/posts/" + selected_post_.id + "/vote";
        load_future_ = std::async(std::launch::async, [url, body, headers]() {
            try { http::HttpClient::instance().post_json(url, body, headers); } catch (...) {}
        });
        selected_post_.likes++;
    }
    ImGui::PopStyleColor();

    ImGui::SameLine(0, 8);

    ImGui::PushStyleColor(ImGuiCol_Text, MARKET_RED);
    if (ImGui::Button("- Dislike")) {
        auto headers = auth::AuthApi::instance().get_auth_headers(
            auth::AuthManager::instance().session().api_key);
        json body;
        body["vote_type"] = "dislike";
        std::string url = std::string(BASE_URL) + "/forum/posts/" + selected_post_.id + "/vote";
        load_future_ = std::async(std::launch::async, [url, body, headers]() {
            try { http::HttpClient::instance().post_json(url, body, headers); } catch (...) {}
        });
        selected_post_.dislikes++;
    }
    ImGui::PopStyleColor();

    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar();

    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

    // Comments
    theme::SectionHeader("COMMENTS");

    if (post_comments_.empty()) {
        ImGui::TextColored(TEXT_DIM, "No comments yet. Be the first to comment!");
    }

    for (int i = 0; i < static_cast<int>(post_comments_.size()); i++) {
        auto& c = post_comments_[i];

        ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
        char ccid[32];
        std::snprintf(ccid, sizeof(ccid), "##comment_%d", i);
        ImGui::BeginChild(ccid, ImVec2(0, 0),
            ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders);

        ImGui::SetCursorPos(ImVec2(12, 8));
        ImGui::TextColored(INFO, "@%s", c.author.c_str());
        ImGui::SameLine(0, 8);
        ImGui::TextColored(TEXT_DIM, "%s", c.time.c_str());
        ImGui::SameLine(detail_w - 80);
        ImGui::TextColored(MARKET_GREEN, "+%d", c.likes);
        ImGui::SameLine(0, 4);
        ImGui::TextColored(MARKET_RED, "-%d", c.dislikes);

        ImGui::SetCursorPosX(12);
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + detail_w - 24);
        ImGui::TextColored(TEXT_SECONDARY, "%s", c.content.c_str());
        ImGui::PopTextWrapPos();

        ImGui::Spacing();
        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::Spacing();
    }

    // Add comment
    ImGui::Spacing();
    ImGui::TextColored(TEXT_SECONDARY, "Add a comment:");
    ImGui::PushItemWidth(-1);
    ImGui::InputTextMultiline("##new_comment", new_comment_, sizeof(new_comment_),
        ImVec2(-1, 80));
    ImGui::PopItemWidth();
    ImGui::Spacing();

    if (theme::AccentButton("Post Comment")) {
        if (std::strlen(new_comment_) > 0) {
            std::string comment_text = new_comment_;
            std::string post_id = selected_post_.id;

            load_future_ = std::async(std::launch::async, [this, comment_text, post_id]() {
                try {
                    auto headers = auth::AuthApi::instance().get_auth_headers(
                        auth::AuthManager::instance().session().api_key);
                    json body;
                    body["content"] = comment_text;
                    std::string url = std::string(BASE_URL) + "/forum/posts/" + post_id + "/comments";
                    http::HttpClient::instance().post_json(url, body, headers);

                    // Add locally
                    ForumComment c;
                    auto& auth = auth::AuthManager::instance();
                    c.author = auth.session().user_info.username.empty()
                        ? auth.session().user_info.email
                        : auth.session().user_info.username;
                    c.content = comment_text;
                    c.time = "just now";
                    post_comments_.push_back(c);
                } catch (...) {}
            });
            std::memset(new_comment_, 0, sizeof(new_comment_));
        }
    }

    ImGui::EndChild();
    ImGui::EndChild();
}

// =============================================================================
// Create Post
// =============================================================================
void ForumScreen::render_create_post() {
    using namespace theme::colors;

    ImGui::BeginChild("##create_post_area", ImVec2(0, 0), ImGuiChildFlags_None);

    float avail_w = ImGui::GetContentRegionAvail().x;
    float form_w = (avail_w > 700.0f) ? 700.0f : avail_w - 32;
    float pad_x = (avail_w - form_w) * 0.5f;
    if (pad_x < 16) pad_x = 16;

    ImGui::SetCursorPos(ImVec2(pad_x, 16));
    ImGui::BeginChild("##create_post_form", ImVec2(form_w, 0), ImGuiChildFlags_None);

    // Back
    if (theme::SecondaryButton("<< Back to Posts")) {
        current_view_ = ForumView::Posts;
    }
    ImGui::Spacing();

    theme::SectionHeader("CREATE NEW POST");
    ImGui::Spacing();

    // Title
    ImGui::TextColored(TEXT_SECONDARY, "Title");
    ImGui::PushItemWidth(-1);
    ImGui::InputText("##post_title", new_title_, sizeof(new_title_));
    ImGui::PopItemWidth();
    ImGui::Spacing();

    // Category
    ImGui::TextColored(TEXT_SECONDARY, "Category");
    if (!categories_.empty()) {
        ImGui::PushItemWidth(-1);
        if (ImGui::BeginCombo("##post_cat", categories_.empty() ? "Select..."
                : categories_[new_category_idx_ < static_cast<int>(categories_.size())
                    ? new_category_idx_ : 0].name.c_str())) {
            for (int i = 0; i < static_cast<int>(categories_.size()); i++) {
                if (ImGui::Selectable(categories_[i].name.c_str(), i == new_category_idx_)) {
                    new_category_idx_ = i;
                }
            }
            ImGui::EndCombo();
        }
        ImGui::PopItemWidth();
    } else {
        ImGui::TextColored(TEXT_DIM, "Loading categories...");
    }
    ImGui::Spacing();

    // Content
    ImGui::TextColored(TEXT_SECONDARY, "Content");
    ImGui::PushItemWidth(-1);
    ImGui::InputTextMultiline("##post_content", new_content_, sizeof(new_content_),
        ImVec2(-1, 250));
    ImGui::PopItemWidth();
    ImGui::Spacing(); ImGui::Spacing();

    // Submit
    if (theme::AccentButton("Submit Post")) {
        if (std::strlen(new_title_) > 0 && !categories_.empty()) {
            std::string title = new_title_;
            std::string content = new_content_;
            int cat_id = categories_[new_category_idx_ < static_cast<int>(categories_.size())
                ? new_category_idx_ : 0].id;

            loading_ = true;
            load_future_ = std::async(std::launch::async, [this, title, content, cat_id]() {
                try {
                    auto headers = auth::AuthApi::instance().get_auth_headers(
                        auth::AuthManager::instance().session().api_key);
                    json body;
                    body["title"] = title;
                    body["content"] = content;

                    char url_buf[128];
                    std::snprintf(url_buf, sizeof(url_buf),
                        "%s/forum/categories/%d/posts", BASE_URL, cat_id);
                    auto resp = http::HttpClient::instance().post_json(
                        std::string(url_buf), body, headers);

                    if (resp.status_code >= 200 && resp.status_code < 300) {
                        status_message_ = "Post created!";
                        current_view_ = ForumView::Posts;
                        load_posts();
                    } else {
                        status_message_ = "Failed to create post";
                    }
                } catch (...) {
                    status_message_ = "Error creating post";
                }
            });
        }
    }
    ImGui::SameLine(0, 12);
    if (theme::SecondaryButton("Cancel")) {
        current_view_ = ForumView::Posts;
    }

    ImGui::EndChild();
    ImGui::EndChild();
}

// =============================================================================
// Search View
// =============================================================================
void ForumScreen::render_search_view() {
    using namespace theme::colors;

    ImGui::BeginChild("##search_area", ImVec2(0, 0), ImGuiChildFlags_None);

    float avail_w = ImGui::GetContentRegionAvail().x;
    float form_w = (avail_w > 700.0f) ? 700.0f : avail_w - 32;
    float pad_x = (avail_w - form_w) * 0.5f;
    if (pad_x < 16) pad_x = 16;

    ImGui::SetCursorPos(ImVec2(pad_x, 16));
    ImGui::BeginChild("##search_content", ImVec2(form_w, 0), ImGuiChildFlags_None);

    if (theme::SecondaryButton("<< Back to Posts")) {
        current_view_ = ForumView::Posts;
    }
    ImGui::Spacing();

    theme::SectionHeader("SEARCH FORUM");
    ImGui::Spacing();

    ImGui::PushItemWidth(form_w - 120);
    bool do_search_now = ImGui::InputText("##forum_search", search_buf_, sizeof(search_buf_),
        ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::PopItemWidth();
    ImGui::SameLine(0, 8);
    if (theme::AccentButton("Search") || do_search_now) {
        do_search();
    }

    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

    if (search_results_.empty()) {
        ImGui::TextColored(TEXT_DIM, "Enter a search term to find posts.");
    } else {
        ImGui::TextColored(TEXT_DIM, "%d results found", static_cast<int>(search_results_.size()));
        ImGui::Spacing();

        float card_w = form_w - 12;
        for (int i = 0; i < static_cast<int>(search_results_.size()); i++) {
            render_post_card(search_results_[i], 1000 + i, card_w);
        }
    }

    ImGui::EndChild();
    ImGui::EndChild();
}

// =============================================================================
// Status Bar
// =============================================================================
void ForumScreen::render_status_bar() {
    using namespace theme::colors;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARKEST);
    ImGui::BeginChild("##forum_status", ImVec2(0, 28), ImGuiChildFlags_None);

    ImGui::SetCursorPos(ImVec2(12, 6));
    ImGui::TextColored(ACCENT, "FORUM");
    ImGui::SameLine(0, 12);
    ImGui::TextColored(BORDER_BRIGHT, "|");
    ImGui::SameLine(0, 8);
    ImGui::TextColored(TEXT_DIM, "POSTS: %d", stats_.total_posts);
    ImGui::SameLine(0, 12);
    ImGui::TextColored(BORDER_BRIGHT, "|");
    ImGui::SameLine(0, 8);
    ImGui::TextColored(TEXT_DIM, "USERS: %d", stats_.total_users);
    ImGui::SameLine(0, 12);
    ImGui::TextColored(BORDER_BRIGHT, "|");
    ImGui::SameLine(0, 8);
    ImGui::TextColored(TEXT_DIM, "CATEGORY: %s", active_category_.c_str());
    ImGui::SameLine(0, 12);
    ImGui::TextColored(BORDER_BRIGHT, "|");
    ImGui::SameLine(0, 8);
    ImGui::TextColored(TEXT_DIM, "SORT: %s", sort_mode_label(sort_mode_));
    ImGui::SameLine(0, 12);
    ImGui::TextColored(BORDER_BRIGHT, "|");
    ImGui::SameLine(0, 8);
    ImGui::TextColored(SUCCESS, "STATUS: LIVE");

    if (!status_message_.empty()) {
        ImGui::SameLine(0, 12);
        ImGui::TextColored(BORDER_BRIGHT, "|");
        ImGui::SameLine(0, 8);
        ImGui::TextColored(INFO, "%s", status_message_.c_str());
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// =============================================================================
// Data loading
// =============================================================================
void ForumScreen::load_data() {
    loading_ = true;
    status_message_ = "Loading...";

    load_future_ = std::async(std::launch::async, [this]() {
        try {
            auto headers = auth::AuthApi::instance().get_auth_headers(
                auth::AuthManager::instance().session().api_key);

            // Load categories
            {
                std::string url = std::string(BASE_URL) + "/forum/categories";
                auto resp = http::HttpClient::instance().get(url, headers);
                auto j = resp.json_body();

                if (j.contains("data")) {
                    auto& data = j["data"];
                    auto& cats_arr = data.contains("data") ? data["data"] : data;
                    if (cats_arr.is_array()) {
                        categories_.clear();
                        for (auto& item : cats_arr) {
                            ForumCategory cat;
                            cat.id = item.value("id", 0);
                            cat.name = item.value("name", "");
                            cat.description = item.value("description", "");
                            cat.post_count = item.value("post_count", 0);
                            categories_.push_back(std::move(cat));
                        }
                    }
                }
            }

            // Load stats
            {
                std::string url = std::string(BASE_URL) + "/forum/stats";
                auto resp = http::HttpClient::instance().get(url, headers);
                auto j = resp.json_body();

                if (j.contains("data")) {
                    auto& d = j["data"];
                    stats_.total_categories = d.value("total_categories", 0);
                    stats_.total_posts = d.value("total_posts", 0);
                    stats_.total_comments = d.value("total_comments", 0);
                    stats_.total_votes = d.value("total_votes", 0);
                    stats_.active_users = d.value("active_users", 0);
                    stats_.total_users = d.value("total_users", 0);
                    stats_.posts_today = d.value("posts_today", 0);
                }
            }

            status_message_ = "Data loaded";
        } catch (...) {
            status_message_ = "Error loading forum data";
        }

        // Load posts separately
        load_posts();
        generate_trending();
        generate_activity();
    });
}

void ForumScreen::load_posts() {
    try {
        auto headers = auth::AuthApi::instance().get_auth_headers(
            auth::AuthManager::instance().session().api_key);

        std::string sort = sort_mode_api(sort_mode_);
        std::string url;

        if (active_category_id_ > 0) {
            char buf[256];
            std::snprintf(buf, sizeof(buf),
                "%s/forum/categories/%d/posts?sort_by=%s&limit=50",
                BASE_URL, active_category_id_, sort.c_str());
            url = buf;
        } else {
            // Load from first category or all
            if (!categories_.empty()) {
                char buf[256];
                std::snprintf(buf, sizeof(buf),
                    "%s/forum/categories/%d/posts?sort_by=%s&limit=50",
                    BASE_URL, categories_[0].id, sort.c_str());
                url = buf;
            } else {
                return;
            }
        }

        auto resp = http::HttpClient::instance().get(url, headers);
        auto j = resp.json_body();

        if (j.contains("data")) {
            auto& data = j["data"];
            auto& posts_arr = data.contains("data") ? data["data"] : data;
            if (posts_arr.is_array()) {
                posts_.clear();
                for (auto& item : posts_arr) {
                    ForumPost p;
                    p.id = item.value("id", item.value("uuid", ""));
                    p.title = item.value("title", "");
                    p.content = item.value("content", "");
                    p.author = item.value("author", item.value("username", "anonymous"));
                    p.time = item.value("created_at", "");
                    p.category = item.value("category_name", active_category_);
                    p.category_id = item.value("category_id", 0);
                    p.views = item.value("views", 0);
                    p.replies = item.value("reply_count", item.value("replies", 0));
                    p.likes = item.value("likes", item.value("upvotes", 0));
                    p.dislikes = item.value("dislikes", item.value("downvotes", 0));
                    p.verified = item.value("verified", false);

                    // Derive sentiment from likes/dislikes
                    if (p.likes > p.dislikes * 2) p.sentiment = "bullish";
                    else if (p.dislikes > p.likes * 2) p.sentiment = "bearish";
                    else p.sentiment = "neutral";

                    // Derive priority from engagement
                    int engagement = p.views + p.replies + p.likes;
                    if (engagement > 100) p.priority = "high";
                    else if (engagement > 20) p.priority = "medium";
                    else p.priority = "low";

                    // Extract tags from content
                    size_t pos = 0;
                    while ((pos = p.content.find('#', pos)) != std::string::npos) {
                        size_t end = pos + 1;
                        while (end < p.content.size() && (std::isalnum(p.content[end]) || p.content[end] == '_'))
                            end++;
                        if (end > pos + 1)
                            p.tags.push_back(p.content.substr(pos + 1, end - pos - 1));
                        pos = end;
                    }

                    posts_.push_back(std::move(p));
                }
            }
        }
    } catch (...) {
        status_message_ = "Error loading posts";
    }

    apply_category_filter();
}

void ForumScreen::load_post_detail(const std::string& post_id) {
    loading_ = true;
    post_comments_.clear();
    std::memset(new_comment_, 0, sizeof(new_comment_));

    std::string pid = post_id;
    load_future_ = std::async(std::launch::async, [this, pid]() {
        try {
            auto headers = auth::AuthApi::instance().get_auth_headers(
                auth::AuthManager::instance().session().api_key);

            std::string url = std::string(BASE_URL) + "/forum/posts/" + pid;
            auto resp = http::HttpClient::instance().get(url, headers);
            auto j = resp.json_body();

            if (j.contains("data")) {
                auto& d = j["data"];
                auto& post_data = d.contains("data") ? d["data"] : d;

                // Update selected post with full data
                selected_post_.content = post_data.value("content", selected_post_.content);
                selected_post_.views = post_data.value("views", selected_post_.views);

                // Load comments
                if (post_data.contains("comments") && post_data["comments"].is_array()) {
                    for (auto& c : post_data["comments"]) {
                        ForumComment comment;
                        comment.id = c.value("id", c.value("uuid", ""));
                        comment.author = c.value("author", c.value("username", "anonymous"));
                        comment.content = c.value("content", "");
                        comment.time = c.value("created_at", "");
                        comment.likes = c.value("likes", c.value("upvotes", 0));
                        comment.dislikes = c.value("dislikes", c.value("downvotes", 0));
                        post_comments_.push_back(std::move(comment));
                    }
                }
            }
            status_message_ = "";
        } catch (...) {
            status_message_ = "Error loading post details";
        }
    });
}

void ForumScreen::apply_category_filter() {
    if (active_category_ == "ALL") {
        filtered_posts_ = posts_;
    } else {
        filtered_posts_.clear();
        for (auto& p : posts_) {
            if (p.category == active_category_ || p.category_id == active_category_id_) {
                filtered_posts_.push_back(p);
            }
        }
    }
}

void ForumScreen::generate_trending() {
    trending_.clear();

    // Generate from post tags
    std::map<std::string, int> tag_counts;
    for (auto& p : posts_) {
        for (auto& t : p.tags) {
            tag_counts[t]++;
        }
    }

    // Sort by count
    std::vector<std::pair<std::string, int>> sorted_tags(tag_counts.begin(), tag_counts.end());
    std::sort(sorted_tags.begin(), sorted_tags.end(),
        [](const auto& a, const auto& b) { return a.second > b.second; });

    for (int i = 0; i < std::min(8, static_cast<int>(sorted_tags.size())); i++) {
        TrendingTopic t;
        t.topic = sorted_tags[i].first;
        t.mentions = sorted_tags[i].second;
        t.sentiment = (i % 3 == 0) ? "bullish" : (i % 3 == 1 ? "neutral" : "bearish");
        trending_.push_back(t);
    }

    // Fallback if no tags
    if (trending_.empty()) {
        trending_ = {
            {"markets", 24, "bullish"},
            {"fed", 18, "bearish"},
            {"earnings", 15, "neutral"},
            {"AI", 12, "bullish"},
            {"crypto", 10, "neutral"},
        };
    }
}

void ForumScreen::generate_activity() {
    recent_activity_.clear();

    // Generate from recent posts
    for (int i = 0; i < std::min(6, static_cast<int>(posts_.size())); i++) {
        RecentActivity a;
        a.user = posts_[i].author;
        a.action = (i % 3 == 0) ? "posted" : (i % 3 == 1 ? "commented on" : "liked");
        a.target = posts_[i].title.substr(0, 30);
        if (posts_[i].title.size() > 30) a.target += "...";
        a.time = posts_[i].time;
        recent_activity_.push_back(a);
    }

    // Fallback
    if (recent_activity_.empty()) {
        recent_activity_ = {
            {"quanttrader", "posted", "Market Analysis Update", "2 min ago"},
            {"alphaseeker", "commented on", "Fed Decision Impact", "5 min ago"},
            {"datadriven", "liked", "Crypto Portfolio Strategy", "12 min ago"},
        };
    }
}

void ForumScreen::do_search() {
    if (std::strlen(search_buf_) == 0) return;

    loading_ = true;
    std::string query = search_buf_;

    load_future_ = std::async(std::launch::async, [this, query]() {
        try {
            auto headers = auth::AuthApi::instance().get_auth_headers(
                auth::AuthManager::instance().session().api_key);

            // URL-encode the query (basic)
            std::string encoded;
            for (char c : query) {
                if (std::isalnum(c) || c == '-' || c == '_' || c == '.') {
                    encoded += c;
                } else if (c == ' ') {
                    encoded += '+';
                } else {
                    char buf[4];
                    std::snprintf(buf, sizeof(buf), "%%%02X", static_cast<unsigned char>(c));
                    encoded += buf;
                }
            }

            std::string url = std::string(BASE_URL) + "/forum/search?q=" + encoded + "&limit=20";
            auto resp = http::HttpClient::instance().get(url, headers);
            auto j = resp.json_body();

            search_results_.clear();
            if (j.contains("data")) {
                auto& data = j["data"];
                auto& results = data.contains("data") ? data["data"] : data;
                if (results.is_array()) {
                    for (auto& item : results) {
                        ForumPost p;
                        p.id = item.value("id", item.value("uuid", ""));
                        p.title = item.value("title", "");
                        p.content = item.value("content", "");
                        p.author = item.value("author", item.value("username", "anonymous"));
                        p.time = item.value("created_at", "");
                        p.views = item.value("views", 0);
                        p.replies = item.value("reply_count", 0);
                        p.likes = item.value("likes", 0);
                        p.dislikes = item.value("dislikes", 0);
                        p.sentiment = "neutral";
                        p.priority = "medium";
                        search_results_.push_back(std::move(p));
                    }
                }
            }
            status_message_ = std::to_string(search_results_.size()) + " results found";
        } catch (...) {
            status_message_ = "Search error";
        }
    });
}

} // namespace fincept::forum
