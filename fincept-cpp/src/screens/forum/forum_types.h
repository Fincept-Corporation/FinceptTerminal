#pragma once
// Forum types — data structs, enums, helpers

#include <imgui.h>
#include <string>
#include <vector>
#include <map>
#include <algorithm>

namespace fincept::forum {

struct ForumCategory {
    int id = 0;
    std::string name;
    std::string description;
    int post_count = 0;
};

struct ForumPost {
    std::string id;
    std::string time;
    std::string author;
    std::string title;
    std::string content;
    std::string category;
    int category_id = 0;
    std::vector<std::string> tags;
    int views = 0;
    int replies = 0;
    int likes = 0;
    int dislikes = 0;
    std::string sentiment;  // "bullish", "bearish", "neutral"
    std::string priority;   // "high", "medium", "low"
    bool verified = false;
};

struct ForumComment {
    std::string id;
    std::string author;
    std::string content;
    std::string time;
    int likes = 0;
    int dislikes = 0;
};

struct ForumStats {
    int total_categories = 0;
    int total_posts = 0;
    int total_comments = 0;
    int total_votes = 0;
    int active_users = 0;
    int total_users = 0;
    int posts_today = 0;
};

struct ForumUser {
    std::string username;
    int reputation = 0;
    int posts = 0;
    std::string joined;
    std::string status; // "online", "offline"
};

struct TrendingTopic {
    std::string topic;
    int mentions = 0;
    std::string sentiment;
};

struct RecentActivity {
    std::string user;
    std::string action;
    std::string target;
    std::string time;
};

enum class ForumView { Posts, PostDetail, CreatePost, Search };
enum class SortMode { Latest, Popular, Views };

// Generate top contributors from actual post data
inline std::vector<ForumUser> generate_top_contributors(const std::vector<ForumPost>& posts) {
    std::map<std::string, int> author_posts;
    std::map<std::string, int> author_rep;
    for (const auto& p : posts) {
        if (p.author.empty() || p.author == "anonymous") continue;
        author_posts[p.author]++;
        author_rep[p.author] += p.likes + p.views / 10 + p.replies;
    }

    std::vector<ForumUser> contributors;
    for (const auto& [name, count] : author_posts) {
        ForumUser u;
        u.username = name;
        u.posts = count;
        u.reputation = author_rep[name];
        u.status = (contributors.size() % 2 == 0) ? "online" : "offline";
        contributors.push_back(std::move(u));
    }

    std::sort(contributors.begin(), contributors.end(),
        [](const ForumUser& a, const ForumUser& b) { return a.reputation > b.reputation; });

    if (contributors.size() > 6) contributors.resize(6);
    return contributors;
}

// Helpers
inline ImVec4 forum_priority_color(const std::string& priority) {
    if (priority == "high")   return {0.96f, 0.30f, 0.30f, 1.0f};
    if (priority == "medium") return {1.0f, 0.78f, 0.08f, 1.0f};
    if (priority == "low")    return {0.25f, 0.84f, 0.42f, 1.0f};
    return {0.72f, 0.72f, 0.75f, 1.0f};
}

inline ImVec4 forum_sentiment_color(const std::string& sentiment) {
    if (sentiment == "bullish")  return {0.05f, 0.85f, 0.42f, 1.0f};
    if (sentiment == "bearish")  return {0.96f, 0.25f, 0.25f, 1.0f};
    if (sentiment == "neutral")  return {1.0f, 0.78f, 0.08f, 1.0f};
    return {0.53f, 0.53f, 0.57f, 1.0f};
}

inline const char* sort_mode_label(SortMode m) {
    switch (m) {
        case SortMode::Latest:  return "LATEST";
        case SortMode::Popular: return "HOT";
        case SortMode::Views:   return "TOP";
    }
    return "LATEST";
}

inline std::string sort_mode_api(SortMode m) {
    switch (m) {
        case SortMode::Latest:  return "latest";
        case SortMode::Popular: return "popular";
        case SortMode::Views:   return "views";
    }
    return "latest";
}

} // namespace fincept::forum
