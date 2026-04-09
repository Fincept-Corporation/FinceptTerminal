// ForumTools.cpp — Forum tab MCP tools (13 tools)
// Covers: categories, posts, threads, comments, voting, search, profiles.

#include "mcp/tools/ForumTools.h"

#include "core/logging/Logger.h"
#include "services/forum/ForumModels.h"
#include "services/forum/ForumService.h"

#include <QEventLoop>
#include <QJsonArray>
#include <QJsonObject>
#include <QTimer>

namespace fincept::mcp::tools {

using namespace fincept::services;

static constexpr int kTimeoutMs = 15000;

// ── Serialisers ──────────────────────────────────────────────────────────────

static QJsonObject category_to_json(const ForumCategory& c) {
    return QJsonObject{
        {"id", c.id},
        {"name", c.name},
        {"description", c.description},
        {"color", c.color},
        {"post_count", c.post_count},
        {"display_order", c.display_order},
        {"created_at", c.created_at},
    };
}

static QJsonObject post_to_json(const ForumPost& p) {
    return QJsonObject{
        {"uuid", p.post_uuid},
        {"title", p.title},
        {"content", p.content},
        {"views", p.views},
        {"likes", p.likes},
        {"reply_count", p.reply_count},
        {"author", p.author_name},
        {"author_display_name", p.author_display_name},
        {"category", p.category_name},
        {"category_color", p.category_color},
        {"user_vote", p.user_vote},
        {"created_at", p.created_at},
        {"updated_at", p.updated_at},
    };
}

static QJsonObject comment_to_json(const ForumComment& c) {
    return QJsonObject{
        {"uuid", c.comment_uuid},
        {"content", c.content},
        {"likes", c.likes},
        {"dislikes", c.dislikes},
        {"author", c.author_name},
        {"author_display_name", c.author_display_name},
        {"parent_comment_id", c.parent_comment_id},
        {"user_vote", c.user_vote},
        {"created_at", c.created_at},
    };
}

static QJsonObject profile_to_json(const ForumProfile& p) {
    QJsonArray recent;
    for (const auto& rp : p.recent_posts)
        recent.append(post_to_json(rp));
    return QJsonObject{
        {"user_id", p.user_id},
        {"username", p.username},
        {"display_name", p.display_name},
        {"bio", p.bio},
        {"avatar_color", p.avatar_color},
        {"signature", p.signature},
        {"reputation", p.reputation},
        {"posts_count", p.posts_count},
        {"comments_count", p.comments_count},
        {"likes_received", p.likes_received},
        {"likes_given", p.likes_given},
        {"created_at", p.created_at},
        {"last_active_at", p.last_active_at},
        {"is_own_profile", p.is_own_profile},
        {"recent_posts", recent},
    };
}

// ── Async helpers ─────────────────────────────────────────────────────────────

// Wraps a ForumService callback-based call into a synchronous ToolResult.
// `trigger` must call the ForumService method and invoke `done_cb` when complete.
template <typename T>
static ToolResult run_forum_sync(std::function<void(std::function<void(bool, T)>)> trigger,
                                 std::function<QJsonValue(const T&)> serialise) {
    bool ok = false;
    QString err_msg = "timeout";
    QJsonValue result_data;
    bool fired = false;

    QEventLoop loop;
    QTimer::singleShot(kTimeoutMs, &loop, &QEventLoop::quit);

    trigger([&](bool success, T data) {
        fired = true;
        ok = success;
        if (success)
            result_data = serialise(data);
        else
            err_msg = "Forum service returned an error";
        loop.quit();
    });

    if (!fired)
        loop.exec();

    if (!ok)
        return ToolResult::fail(err_msg);

    if (result_data.isArray())
        return ToolResult::ok_data(result_data.toArray());
    if (result_data.isObject())
        return ToolResult::ok_data(result_data.toObject());
    return ToolResult::ok("OK");
}

// BoolCallback variant (write operations)
static ToolResult run_forum_bool(std::function<void(ForumService::BoolCallback)> trigger) {
    bool ok = false;
    QString msg = "timeout";
    bool fired = false;

    QEventLoop loop;
    QTimer::singleShot(kTimeoutMs, &loop, &QEventLoop::quit);

    trigger([&](bool success, QString message) {
        fired = true;
        ok = success;
        msg = message;
        loop.quit();
    });

    if (!fired)
        loop.exec();

    if (!ok)
        return ToolResult::fail(msg.isEmpty() ? "Forum operation failed" : msg);
    return ToolResult::ok(msg.isEmpty() ? "OK" : msg);
}

// ── Tool registration ─────────────────────────────────────────────────────────

std::vector<ToolDef> get_forum_tools() {
    std::vector<ToolDef> tools;

    // ── forum_get_categories ─────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "forum_get_categories";
        t.description = "List all forum categories with their IDs, names, descriptions, "
                        "colors, and post counts. Also returns current user permissions.";
        t.category = "forum";
        t.input_schema.properties = QJsonObject{};
        t.handler = [](const QJsonObject&) -> ToolResult {
            bool ok = false;
            QString err;
            QJsonObject result;
            bool fired = false;

            QEventLoop loop;
            QTimer::singleShot(kTimeoutMs, &loop, &QEventLoop::quit);

            ForumService::instance().fetch_categories(
                [&](bool success, QVector<ForumCategory> cats, ForumPermissions perms) {
                    fired = true;
                    ok = success;
                    if (success) {
                        QJsonArray arr;
                        for (const auto& c : cats)
                            arr.append(category_to_json(c));
                        result = QJsonObject{
                            {"categories", arr},
                            {"permissions",
                             QJsonObject{
                                 {"can_create_posts", perms.can_create_posts},
                                 {"can_vote", perms.can_vote},
                                 {"can_comment", perms.can_comment},
                             }},
                        };
                    } else {
                        err = "Failed to load categories";
                    }
                    loop.quit();
                });

            if (!fired)
                loop.exec();
            if (!ok)
                return ToolResult::fail(err);
            return ToolResult::ok_data(result);
        };
        tools.push_back(std::move(t));
    }

    // ── forum_get_stats ──────────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "forum_get_stats";
        t.description = "Get community-wide forum statistics: total posts, comments, votes, "
                        "24h activity, popular categories, and top contributors.";
        t.category = "forum";
        t.input_schema.properties = QJsonObject{};
        t.handler = [](const QJsonObject&) -> ToolResult {
            return run_forum_sync<ForumStats>(
                [](auto cb) { ForumService::instance().fetch_stats(cb); },
                [](const ForumStats& s) -> QJsonValue {
                    QJsonArray popular;
                    for (const auto& c : s.popular_categories)
                        popular.append(category_to_json(c));
                    QJsonArray contributors;
                    for (const auto& c : s.top_contributors)
                        contributors.append(QJsonObject{
                            {"display_name", c.display_name},
                            {"username", c.username},
                            {"reputation", c.reputation},
                            {"posts_count", c.posts_count},
                        });
                    return QJsonObject{
                        {"total_categories", s.total_categories}, {"total_posts", s.total_posts},
                        {"total_comments", s.total_comments},     {"total_votes", s.total_votes},
                        {"recent_posts_24h", s.recent_posts_24h}, {"popular_categories", popular},
                        {"top_contributors", contributors},
                    };
                });
        };
        tools.push_back(std::move(t));
    }

    // ── forum_get_posts ──────────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "forum_get_posts";
        t.description = "Get paginated posts for a forum category. "
                        "Use forum_get_categories first to obtain category IDs.";
        t.category = "forum";
        t.input_schema.properties = QJsonObject{
            {"category_id", QJsonObject{{"type", "integer"}, {"description", "Category ID"}}},
            {"page", QJsonObject{{"type", "integer"}, {"description", "Page number (default: 1)"}}},
            {"sort", QJsonObject{{"type", "string"}, {"description", "Sort order: latest (default) or top"}}},
        };
        t.input_schema.required = {"category_id"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            int cat_id = args["category_id"].toInt(-1);
            if (cat_id < 0)
                return ToolResult::fail("Missing or invalid 'category_id'");
            int page = args["page"].toInt(1);
            QString sort = args["sort"].toString("latest");

            return run_forum_sync<ForumPostsPage>(
                [cat_id, page, sort](auto cb) { ForumService::instance().fetch_posts(cat_id, page, sort, cb); },
                [](const ForumPostsPage& pg) -> QJsonValue {
                    QJsonArray arr;
                    for (const auto& p : pg.posts)
                        arr.append(post_to_json(p));
                    return QJsonObject{
                        {"posts", arr},      {"page", pg.page},       {"pages", pg.pages},
                        {"total", pg.total}, {"sort_by", pg.sort_by}, {"category", category_to_json(pg.category)},
                    };
                });
        };
        tools.push_back(std::move(t));
    }

    // ── forum_get_trending ───────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "forum_get_trending";
        t.description = "Get currently trending forum posts across all categories.";
        t.category = "forum";
        t.input_schema.properties = QJsonObject{};
        t.handler = [](const QJsonObject&) -> ToolResult {
            return run_forum_sync<ForumPostsPage>([](auto cb) { ForumService::instance().fetch_trending(cb); },
                                                  [](const ForumPostsPage& pg) -> QJsonValue {
                                                      QJsonArray arr;
                                                      for (const auto& p : pg.posts)
                                                          arr.append(post_to_json(p));
                                                      return QJsonObject{
                                                          {"posts", arr},
                                                          {"total", pg.total},
                                                      };
                                                  });
        };
        tools.push_back(std::move(t));
    }

    // ── forum_get_post ───────────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "forum_get_post";
        t.description = "Get a full forum post including all comments/replies by post UUID.";
        t.category = "forum";
        t.input_schema.properties = QJsonObject{
            {"uuid", QJsonObject{{"type", "string"}, {"description", "Post UUID"}}},
        };
        t.input_schema.required = {"uuid"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString uuid = args["uuid"].toString().trimmed();
            if (uuid.isEmpty())
                return ToolResult::fail("Missing 'uuid'");

            return run_forum_sync<ForumPostDetail>([uuid](auto cb) { ForumService::instance().fetch_post(uuid, cb); },
                                                   [](const ForumPostDetail& d) -> QJsonValue {
                                                       QJsonArray comments;
                                                       for (const auto& c : d.comments)
                                                           comments.append(comment_to_json(c));
                                                       return QJsonObject{
                                                           {"post", post_to_json(d.post)},
                                                           {"comments", comments},
                                                           {"total_comments", d.total_comments},
                                                           {"permissions",
                                                            QJsonObject{
                                                                {"can_create_posts", d.permissions.can_create_posts},
                                                                {"can_vote", d.permissions.can_vote},
                                                                {"can_comment", d.permissions.can_comment},
                                                            }},
                                                       };
                                                   });
        };
        tools.push_back(std::move(t));
    }

    // ── forum_search ─────────────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "forum_search";
        t.description = "Search forum posts by keyword. Returns paginated results.";
        t.category = "forum";
        t.input_schema.properties = QJsonObject{
            {"query", QJsonObject{{"type", "string"}, {"description", "Search keyword"}}},
            {"page", QJsonObject{{"type", "integer"}, {"description", "Page number (default: 1)"}}},
        };
        t.input_schema.required = {"query"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString query = args["query"].toString().trimmed();
            if (query.isEmpty())
                return ToolResult::fail("Missing 'query'");
            int page = args["page"].toInt(1);

            return run_forum_sync<ForumPostsPage>(
                [query, page](auto cb) { ForumService::instance().search(query, page, cb); },
                [](const ForumPostsPage& pg) -> QJsonValue {
                    QJsonArray arr;
                    for (const auto& p : pg.posts)
                        arr.append(post_to_json(p));
                    return QJsonObject{
                        {"posts", arr},
                        {"page", pg.page},
                        {"pages", pg.pages},
                        {"total", pg.total},
                    };
                });
        };
        tools.push_back(std::move(t));
    }

    // ── forum_create_post ────────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "forum_create_post";
        t.description = "Create a new forum post in a category. "
                        "Requires can_create_posts permission (check forum_get_categories).";
        t.category = "forum";
        t.input_schema.properties = QJsonObject{
            {"category_id", QJsonObject{{"type", "integer"}, {"description", "Category ID to post in"}}},
            {"title", QJsonObject{{"type", "string"}, {"description", "Post title"}}},
            {"content", QJsonObject{{"type", "string"}, {"description", "Post body content"}}},
        };
        t.input_schema.required = {"category_id", "title", "content"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            int cat_id = args["category_id"].toInt(-1);
            QString title = args["title"].toString().trimmed();
            QString content = args["content"].toString().trimmed();
            if (cat_id < 0)
                return ToolResult::fail("Missing or invalid 'category_id'");
            if (title.isEmpty())
                return ToolResult::fail("Missing 'title'");
            if (content.isEmpty())
                return ToolResult::fail("Missing 'content'");

            return run_forum_bool([cat_id, title, content](auto cb) {
                ForumService::instance().create_post(cat_id, title, content, cb);
            });
        };
        tools.push_back(std::move(t));
    }

    // ── forum_create_comment ─────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "forum_create_comment";
        t.description = "Reply to a forum post by its UUID. "
                        "Requires can_comment permission.";
        t.category = "forum";
        t.input_schema.properties = QJsonObject{
            {"post_uuid", QJsonObject{{"type", "string"}, {"description", "UUID of the post to reply to"}}},
            {"content", QJsonObject{{"type", "string"}, {"description", "Reply content"}}},
        };
        t.input_schema.required = {"post_uuid", "content"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString uuid = args["post_uuid"].toString().trimmed();
            QString content = args["content"].toString().trimmed();
            if (uuid.isEmpty())
                return ToolResult::fail("Missing 'post_uuid'");
            if (content.isEmpty())
                return ToolResult::fail("Missing 'content'");

            return run_forum_bool(
                [uuid, content](auto cb) { ForumService::instance().create_comment(uuid, content, cb); });
        };
        tools.push_back(std::move(t));
    }

    // ── forum_vote_post ──────────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "forum_vote_post";
        t.description = "Vote on a forum post. vote_type: 'up' to upvote, '' (empty) to clear vote. "
                        "Requires can_vote permission.";
        t.category = "forum";
        t.input_schema.properties = QJsonObject{
            {"post_uuid", QJsonObject{{"type", "string"}, {"description", "Post UUID"}}},
            {"vote_type", QJsonObject{{"type", "string"}, {"description", "'up' to upvote, '' to clear"}}},
        };
        t.input_schema.required = {"post_uuid", "vote_type"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString uuid = args["post_uuid"].toString().trimmed();
            QString vote_type = args["vote_type"].toString();
            if (uuid.isEmpty())
                return ToolResult::fail("Missing 'post_uuid'");

            return run_forum_bool(
                [uuid, vote_type](auto cb) { ForumService::instance().vote_post(uuid, vote_type, cb); });
        };
        tools.push_back(std::move(t));
    }

    // ── forum_vote_comment ───────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "forum_vote_comment";
        t.description = "Vote on a forum comment. vote_type: 'up', 'down', or '' to clear. "
                        "Requires can_vote permission.";
        t.category = "forum";
        t.input_schema.properties = QJsonObject{
            {"comment_uuid", QJsonObject{{"type", "string"}, {"description", "Comment UUID"}}},
            {"vote_type", QJsonObject{{"type", "string"}, {"description", "'up', 'down', or '' to clear"}}},
        };
        t.input_schema.required = {"comment_uuid", "vote_type"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString uuid = args["comment_uuid"].toString().trimmed();
            QString vote_type = args["vote_type"].toString();
            if (uuid.isEmpty())
                return ToolResult::fail("Missing 'comment_uuid'");

            return run_forum_bool(
                [uuid, vote_type](auto cb) { ForumService::instance().vote_comment(uuid, vote_type, cb); });
        };
        tools.push_back(std::move(t));
    }

    // ── forum_get_my_profile ─────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "forum_get_my_profile";
        t.description = "Get the current user's forum profile including reputation, "
                        "post count, comment count, likes, and recent posts.";
        t.category = "forum";
        t.input_schema.properties = QJsonObject{};
        t.handler = [](const QJsonObject&) -> ToolResult {
            return run_forum_sync<ForumProfile>([](auto cb) { ForumService::instance().fetch_my_profile(cb); },
                                                [](const ForumProfile& p) -> QJsonValue { return profile_to_json(p); });
        };
        tools.push_back(std::move(t));
    }

    // ── forum_get_profile ────────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "forum_get_profile";
        t.description = "Get another user's forum profile by username.";
        t.category = "forum";
        t.input_schema.properties = QJsonObject{
            {"username", QJsonObject{{"type", "string"}, {"description", "Username to look up"}}},
        };
        t.input_schema.required = {"username"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString username = args["username"].toString().trimmed();
            if (username.isEmpty())
                return ToolResult::fail("Missing 'username'");

            return run_forum_sync<ForumProfile>(
                [username](auto cb) { ForumService::instance().fetch_profile(username, cb); },
                [](const ForumProfile& p) -> QJsonValue { return profile_to_json(p); });
        };
        tools.push_back(std::move(t));
    }

    // ── forum_update_profile ─────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "forum_update_profile";
        t.description = "Update the current user's forum profile "
                        "(display name, bio, signature, avatar color).";
        t.category = "forum";
        t.input_schema.properties = QJsonObject{
            {"display_name", QJsonObject{{"type", "string"}, {"description", "Display name"}}},
            {"bio", QJsonObject{{"type", "string"}, {"description", "Short biography"}}},
            {"signature", QJsonObject{{"type", "string"}, {"description", "Post signature"}}},
            {"avatar_color", QJsonObject{{"type", "string"}, {"description", "Hex color for avatar (e.g. #d97706)"}}},
        };
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString display_name = args["display_name"].toString();
            QString bio = args["bio"].toString();
            QString signature = args["signature"].toString();
            QString avatar_color = args["avatar_color"].toString();

            return run_forum_bool([display_name, bio, signature, avatar_color](auto cb) {
                ForumService::instance().update_profile(display_name, bio, signature, avatar_color, cb);
            });
        };
        tools.push_back(std::move(t));
    }

    return tools;
}

} // namespace fincept::mcp::tools
