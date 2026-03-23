// src/services/forum/ForumService.cpp
#include "services/forum/ForumService.h"

#include "auth/AuthManager.h"
#include "core/logging/Logger.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>

namespace fincept::services {

ForumService& ForumService::instance() {
    static ForumService s;
    return s;
}

ForumService::ForumService() {
    nam_ = new QNetworkAccessManager(this);
}

QString ForumService::api_key() const {
    return auth::AuthManager::instance().session().api_key;
}

// ── Low-level HTTP helpers ────────────────────────────────────────────────────

void ForumService::get(const QString& path, std::function<void(bool, QJsonObject)> cb) {
    QNetworkRequest req(QUrl(QString(BASE) + path));
    req.setRawHeader("X-API-KEY", api_key().toUtf8());
    req.setRawHeader("Accept", "application/json");
    req.setTransferTimeout(10000);

    auto* reply = nam_->get(req);
    connect(reply, &QNetworkReply::finished, this, [reply, cb]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            LOG_WARN("ForumService", QString("GET error: %1").arg(reply->errorString()));
            cb(false, {});
            return;
        }
        auto doc = QJsonDocument::fromJson(reply->readAll());
        auto root = doc.object();
        if (!root.value("success").toBool()) {
            LOG_WARN("ForumService", QString("API error: %1").arg(root.value("message").toString()));
            cb(false, root);
            return;
        }
        cb(true, root.value("data").toObject());
    });
}

void ForumService::post_req(const QString& path, const QJsonObject& body,
                            std::function<void(bool, QJsonObject)> cb) {
    QNetworkRequest req(QUrl(QString(BASE) + path));
    req.setRawHeader("X-API-KEY", api_key().toUtf8());
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setTransferTimeout(10000);

    auto* reply = nam_->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [reply, cb]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            LOG_WARN("ForumService", QString("POST error: %1").arg(reply->errorString()));
            cb(false, {});
            return;
        }
        auto doc = QJsonDocument::fromJson(reply->readAll());
        auto root = doc.object();
        cb(root.value("success").toBool(), root.value("data").toObject());
    });
}

void ForumService::put_req(const QString& path, const QJsonObject& body,
                           std::function<void(bool, QJsonObject)> cb) {
    QNetworkRequest req(QUrl(QString(BASE) + path));
    req.setRawHeader("X-API-KEY", api_key().toUtf8());
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setTransferTimeout(10000);

    auto* reply = nam_->put(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [reply, cb]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            cb(false, {});
            return;
        }
        auto doc = QJsonDocument::fromJson(reply->readAll());
        auto root = doc.object();
        cb(root.value("success").toBool(), root.value("data").toObject());
    });
}

// ── Parsers ───────────────────────────────────────────────────────────────────

ForumCategory ForumService::parse_category(const QJsonObject& o) {
    ForumCategory c;
    c.id            = o.value("id").toInt();
    c.name          = o.value("name").toString();
    c.description   = o.value("description").toString();
    c.color         = o.value("color").toString();
    c.post_count    = o.value("post_count").toInt();
    c.display_order = o.value("display_order").toInt();
    c.created_at    = o.value("created_at").toString();
    return c;
}

ForumPost ForumService::parse_post(const QJsonObject& o) {
    ForumPost p;
    p.id                  = o.value("id").toInt();
    p.post_uuid           = o.value("post_uuid").toString();
    p.title               = o.value("title").toString();
    p.content             = o.value("content").toString();
    p.views               = o.value("views").toInt();
    p.likes               = o.value("likes").toInt();
    p.reply_count         = o.value("reply_count").toInt();
    p.created_at          = o.value("created_at").toString();
    p.updated_at          = o.value("updated_at").toString();
    p.author_name         = o.value("author_name").toString();
    p.author_display_name = o.value("author_display_name").toString();
    p.category_name       = o.value("category_name").toString();
    p.category_color      = o.value("category_color").toString();
    p.user_vote           = o.value("user_vote").toString();
    return p;
}

ForumComment ForumService::parse_comment(const QJsonObject& o) {
    ForumComment c;
    c.id                  = o.value("id").toInt();
    c.comment_uuid        = o.value("comment_uuid").toString();
    c.content             = o.value("content").toString();
    c.likes               = o.value("likes").toInt();
    c.dislikes            = o.value("dislikes").toInt();
    c.created_at          = o.value("created_at").toString();
    c.author_name         = o.value("author_name").toString();
    c.author_display_name = o.value("author_display_name").toString();
    c.parent_comment_id   = o.value("parent_comment_id").toInt(0);
    c.user_vote           = o.value("user_vote").toString();
    return c;
}

ForumStats ForumService::parse_stats(const QJsonObject& o) {
    ForumStats s;
    s.total_categories  = o.value("total_categories").toInt();
    s.total_posts       = o.value("total_posts").toInt();
    s.total_comments    = o.value("total_comments").toInt();
    s.total_votes       = o.value("total_votes").toInt();
    s.recent_posts_24h  = o.value("recent_posts_24h").toInt();
    for (const auto& v : o.value("popular_categories").toArray())
        s.popular_categories.append(parse_category(v.toObject()));
    for (const auto& v : o.value("top_contributors").toArray()) {
        auto obj = v.toObject();
        ForumContributor c;
        c.display_name = obj.value("display_name").toString();
        c.username     = obj.value("username").toString();
        c.reputation   = obj.value("reputation").toInt();
        c.posts_count  = obj.value("posts_count").toInt();
        s.top_contributors.append(c);
    }
    return s;
}

ForumProfile ForumService::parse_profile(const QJsonObject& o) {
    auto pobj = o.value("profile").toObject();
    ForumProfile p;
    p.user_id        = pobj.value("user_id").toInt();
    p.username       = pobj.value("username").toString();
    p.display_name   = pobj.value("display_name").toString();
    p.bio            = pobj.value("bio").toString();
    p.avatar_color   = pobj.value("avatar_color").toString("#d97706");
    p.signature      = pobj.value("signature").toString();
    p.reputation     = pobj.value("reputation").toInt();
    p.posts_count    = pobj.value("posts_count").toInt();
    p.comments_count = pobj.value("comments_count").toInt();
    p.likes_received = pobj.value("likes_received").toInt();
    p.likes_given    = pobj.value("likes_given").toInt();
    p.email          = pobj.value("email").toString();
    p.created_at     = pobj.value("created_at").toString();
    p.last_active_at = pobj.value("last_active_at").toString();
    p.is_own_profile = o.value("is_own_profile").toBool();
    for (const auto& v : o.value("recent_posts").toArray())
        p.recent_posts.append(parse_post(v.toObject()));
    return p;
}

// ── Public API ────────────────────────────────────────────────────────────────

void ForumService::fetch_categories(CategoriesCallback cb) {
    get("/forum/categories", [cb](bool ok, QJsonObject data) {
        if (!ok) { cb(false, {}, {}); return; }
        QVector<ForumCategory> cats;
        for (const auto& v : data.value("categories").toArray())
            cats.append(parse_category(v.toObject()));
        ForumPermissions perms;
        auto po = data.value("permissions").toObject();
        perms.can_create_posts = po.value("can_create_posts").toBool();
        perms.can_vote         = po.value("can_vote").toBool();
        perms.can_comment      = po.value("can_comment").toBool();
        cb(true, cats, perms);
    });
}

void ForumService::fetch_posts(int category_id, int page, const QString& sort, PostsCallback cb) {
    QString path = QString("/forum/categories/%1/posts?page=%2&sort=%3")
                       .arg(category_id).arg(page).arg(sort.isEmpty() ? "latest" : sort);
    get(path, [cb](bool ok, QJsonObject data) {
        if (!ok) { cb(false, {}); return; }
        ForumPostsPage result;
        for (const auto& v : data.value("posts").toArray())
            result.posts.append(parse_post(v.toObject()));
        auto pag = data.value("pagination").toObject();
        result.page  = pag.value("page").toInt(1);
        result.limit = pag.value("limit").toInt(20);
        result.total = pag.value("total").toInt();
        result.pages = pag.value("pages").toInt(1);
        result.sort_by = data.value("sort_by").toString();
        auto cobj = data.value("category").toObject();
        result.category.id    = cobj.value("id").toInt();
        result.category.name  = cobj.value("name").toString();
        result.category.color = cobj.value("color").toString();
        cb(true, result);
    });
}

void ForumService::fetch_post(const QString& post_uuid, PostDetailCallback cb) {
    get("/forum/posts/" + post_uuid, [cb](bool ok, QJsonObject data) {
        if (!ok) { cb(false, {}); return; }
        ForumPostDetail detail;
        detail.post = parse_post(data.value("post").toObject());
        for (const auto& v : data.value("comments").toArray())
            detail.comments.append(parse_comment(v.toObject()));
        detail.total_comments = data.value("total_comments").toInt();
        auto po = data.value("permissions").toObject();
        detail.permissions.can_comment = po.value("can_comment").toBool();
        detail.permissions.can_vote    = po.value("can_vote").toBool();
        cb(true, detail);
    });
}

void ForumService::fetch_stats(StatsCallback cb) {
    get("/forum/stats", [cb](bool ok, QJsonObject data) {
        if (!ok) { cb(false, {}); return; }
        cb(true, parse_stats(data));
    });
}

void ForumService::fetch_trending(PostsCallback cb) {
    get("/forum/posts/trending", [cb](bool ok, QJsonObject data) {
        if (!ok) { cb(false, {}); return; }
        ForumPostsPage result;
        for (const auto& v : data.value("trending_posts").toArray())
            result.posts.append(parse_post(v.toObject()));
        result.total = data.value("total").toInt();
        cb(true, result);
    });
}

void ForumService::search(const QString& query, int page, PostsCallback cb) {
    QString path = QString("/forum/search?q=%1&page=%2")
                       .arg(QString::fromUtf8(QUrl::toPercentEncoding(query))).arg(page);
    get(path, [cb](bool ok, QJsonObject data) {
        if (!ok) { cb(false, {}); return; }
        ForumPostsPage result;
        auto res = data.value("results").toObject();
        for (const auto& v : res.value("posts").toArray())
            result.posts.append(parse_post(v.toObject()));
        result.total = res.value("total_results").toInt();
        auto pag = data.value("pagination").toObject();
        result.page  = pag.value("page").toInt(1);
        result.limit = pag.value("limit").toInt(20);
        cb(true, result);
    });
}

void ForumService::fetch_my_profile(ProfileCallback cb) {
    get("/forum/profile", [cb](bool ok, QJsonObject data) {
        if (!ok) { cb(false, {}); return; }
        cb(true, parse_profile(data));
    });
}

void ForumService::fetch_profile(const QString& username, ProfileCallback cb) {
    get("/forum/profile/" + username, [cb](bool ok, QJsonObject data) {
        if (!ok) { cb(false, {}); return; }
        cb(true, parse_profile(data));
    });
}

void ForumService::create_post(int category_id, const QString& title,
                               const QString& content, BoolCallback cb) {
    QJsonObject body;
    body["title"]   = title;
    body["content"] = content;
    post_req(QString("/forum/categories/%1/posts").arg(category_id), body,
             [cb](bool ok, QJsonObject) { cb(ok, ok ? "Post created" : "Failed to create post"); });
}

void ForumService::create_comment(const QString& post_uuid, const QString& content, BoolCallback cb) {
    QJsonObject body;
    body["content"] = content;
    post_req("/forum/posts/" + post_uuid + "/comments", body,
             [cb](bool ok, QJsonObject) { cb(ok, ok ? "Comment posted" : "Failed to post comment"); });
}

void ForumService::vote_post(const QString& post_uuid, const QString& vote_type, BoolCallback cb) {
    QJsonObject body;
    body["vote_type"] = vote_type;
    post_req("/forum/posts/" + post_uuid + "/vote", body,
             [cb](bool ok, QJsonObject) { cb(ok, {}); });
}

void ForumService::vote_comment(const QString& comment_uuid, const QString& vote_type, BoolCallback cb) {
    QJsonObject body;
    body["vote_type"] = vote_type;
    post_req("/forum/comments/" + comment_uuid + "/vote", body,
             [cb](bool ok, QJsonObject) { cb(ok, {}); });
}

void ForumService::update_profile(const QString& display_name, const QString& bio,
                                  const QString& signature, const QString& avatar_color,
                                  BoolCallback cb) {
    QJsonObject body;
    body["display_name"]  = display_name;
    body["bio"]           = bio;
    body["signature"]     = signature;
    body["avatar_color"]  = avatar_color;
    put_req("/forum/profile", body,
            [cb](bool ok, QJsonObject) { cb(ok, ok ? "Profile updated" : "Failed to update profile"); });
}

} // namespace fincept::services
