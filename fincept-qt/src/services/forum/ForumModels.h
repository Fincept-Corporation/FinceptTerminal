// src/services/forum/ForumModels.h
#pragma once
#include <QColor>
#include <QString>
#include <QVector>

namespace fincept::services {

struct ForumCategory {
    int id = 0;
    QString name;
    QString description;
    QString color; // hex
    int post_count = 0;
    int display_order = 0;
    QString created_at;
};

struct ForumPost {
    int id = 0;
    QString post_uuid;
    QString title;
    QString content;
    int views = 0;
    int likes = 0;
    int reply_count = 0;
    QString created_at;
    QString updated_at;
    QString author_name;
    QString author_display_name;
    QString category_name;
    QString category_color;
    QString user_vote; // null/"up"
};

struct ForumComment {
    int id = 0;
    QString comment_uuid;
    QString content;
    int likes = 0;
    int dislikes = 0;
    QString created_at;
    QString author_name;
    QString author_display_name;
    int parent_comment_id = 0; // 0 = top-level
    QString user_vote;         // null/"up"/"down"
};

struct ForumProfile {
    int user_id = 0;
    QString username;
    QString display_name;
    QString bio;
    QString avatar_color;
    QString signature;
    int reputation = 0;
    int posts_count = 0;
    int comments_count = 0;
    int likes_received = 0;
    int likes_given = 0;
    QString email;
    QString created_at;
    QString last_active_at;
    bool is_own_profile = false;
    QVector<ForumPost> recent_posts;
};

struct ForumContributor {
    QString display_name;
    QString username;
    int reputation = 0;
    int posts_count = 0;
};

struct ForumStats {
    int total_categories = 0;
    int total_posts = 0;
    int total_comments = 0;
    int total_votes = 0;
    int recent_posts_24h = 0;
    QVector<ForumCategory> popular_categories;
    QVector<ForumContributor> top_contributors;
};

struct ForumPermissions {
    bool can_create_posts = false;
    bool can_vote = false;
    bool can_comment = false;
};

struct ForumPostDetail {
    ForumPost post;
    QVector<ForumComment> comments;
    int total_comments = 0;
    ForumPermissions permissions;
};

struct ForumPostsPage {
    QVector<ForumPost> posts;
    ForumCategory category;
    int page = 1;
    int limit = 20;
    int total = 0;
    int pages = 1;
    QString sort_by;
};

} // namespace fincept::services
