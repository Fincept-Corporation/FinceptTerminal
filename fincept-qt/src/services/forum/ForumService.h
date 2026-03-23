// src/services/forum/ForumService.h
#pragma once
#include "services/forum/ForumModels.h"

#include <QNetworkAccessManager>
#include <QObject>
#include <QString>
#include <QVector>

#include <functional>

namespace fincept::services {

class ForumService : public QObject {
    Q_OBJECT
  public:
    static ForumService& instance();

    using CategoriesCallback  = std::function<void(bool, QVector<ForumCategory>, ForumPermissions)>;
    using PostsCallback       = std::function<void(bool, ForumPostsPage)>;
    using PostDetailCallback  = std::function<void(bool, ForumPostDetail)>;
    using StatsCallback       = std::function<void(bool, ForumStats)>;
    using ProfileCallback     = std::function<void(bool, ForumProfile)>;
    using BoolCallback        = std::function<void(bool, QString)>;

    // Read
    void fetch_categories(CategoriesCallback cb);
    void fetch_posts(int category_id, int page, const QString& sort, PostsCallback cb);
    void fetch_post(const QString& post_uuid, PostDetailCallback cb);
    void fetch_stats(StatsCallback cb);
    void fetch_trending(PostsCallback cb);
    void search(const QString& query, int page, PostsCallback cb);
    void fetch_my_profile(ProfileCallback cb);
    void fetch_profile(const QString& username, ProfileCallback cb);

    // Write
    void create_post(int category_id, const QString& title, const QString& content, BoolCallback cb);
    void create_comment(const QString& post_uuid, const QString& content, BoolCallback cb);
    void vote_post(const QString& post_uuid, const QString& vote_type, BoolCallback cb);
    void vote_comment(const QString& comment_uuid, const QString& vote_type, BoolCallback cb);
    void update_profile(const QString& display_name, const QString& bio,
                        const QString& signature, const QString& avatar_color, BoolCallback cb);

  signals:
    void categories_loaded(QVector<ForumCategory> cats, ForumPermissions perms);
    void posts_loaded(ForumPostsPage page);
    void post_detail_loaded(ForumPostDetail detail);
    void stats_loaded(ForumStats stats);
    void error_occurred(QString message);

  private:
    ForumService();
    QString api_key() const;

    void get(const QString& path, std::function<void(bool, QJsonObject)> cb);
    void post_req(const QString& path, const QJsonObject& body, std::function<void(bool, QJsonObject)> cb);
    void put_req(const QString& path, const QJsonObject& body, std::function<void(bool, QJsonObject)> cb);

    static ForumCategory parse_category(const QJsonObject& o);
    static ForumPost     parse_post(const QJsonObject& o);
    static ForumComment  parse_comment(const QJsonObject& o);
    static ForumStats    parse_stats(const QJsonObject& o);
    static ForumProfile  parse_profile(const QJsonObject& o);

    QNetworkAccessManager* nam_ = nullptr;
    static constexpr const char* BASE = "https://api.fincept.in";
};

} // namespace fincept::services
