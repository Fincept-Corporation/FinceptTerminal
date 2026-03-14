#pragma once
// Forum screen — community forum with posts, comments, voting, search

#include "forum_types.h"
#include <imgui.h>
#include <vector>
#include <string>
#include <future>

namespace fincept::forum {

class ForumScreen {
public:
    void render();

private:
    // Data
    std::vector<ForumCategory> categories_;
    std::vector<ForumPost> posts_;
    std::vector<ForumPost> filtered_posts_;
    std::vector<ForumPost> search_results_;
    ForumStats stats_;
    std::vector<ForumComment> post_comments_;
    std::vector<TrendingTopic> trending_;
    std::vector<RecentActivity> recent_activity_;
    std::vector<ForumUser> top_contributors_;

    // State
    ForumView current_view_ = ForumView::Posts;
    std::string active_category_ = "ALL";
    int active_category_id_ = -1;
    SortMode sort_mode_ = SortMode::Latest;
    int selected_post_idx_ = -1;
    ForumPost selected_post_;
    bool loading_ = false;
    bool data_loaded_ = false;
    std::future<void> load_future_;
    std::string status_message_;

    // Search
    char search_buf_[256] = {};

    // Create post form
    char new_title_[256] = {};
    char new_content_[4096] = {};
    int new_category_idx_ = 0;

    // Comment form
    char new_comment_[2048] = {};

    // Render methods
    void render_header();
    void render_category_bar();
    void render_left_panel(float width, float height);
    void render_center_panel(float width, float height);
    void render_right_panel(float width, float height);
    void render_post_detail();
    void render_create_post();
    void render_search_view();
    void render_status_bar();
    void render_post_card(const ForumPost& post, int idx, float width);

    // Data methods
    void load_data();
    void load_posts();
    void load_post_detail(const std::string& post_id);
    void apply_category_filter();
    void generate_trending();
    void generate_activity();
    void do_search();
};

} // namespace fincept::forum
