#pragma once
#include "storage/repositories/BaseRepository.h"

#include <QString>
#include <QVector>

namespace fincept {

/// Row in the `rss_feeds` overlay table (see v031 migration).
/// Built-in defaults are sourced from `NewsService::default_feeds()`;
/// this table only stores user overrides and user-added feeds.
struct RssFeedRow {
    QString id;
    QString name;
    QString url;
    QString category;
    QString region;
    QString source;
    int tier = 3;
    bool is_builtin = false;
    bool enabled = true;
};

class RssFeedRepository : public BaseRepository<RssFeedRow> {
  public:
    static RssFeedRepository& instance();

    Result<QVector<RssFeedRow>> list_all() const;
    Result<void> upsert(const RssFeedRow& row) const;
    Result<void> remove(const QString& id) const;
    Result<void> set_enabled(const QString& id, bool enabled) const;

  private:
    RssFeedRepository() = default;
    static RssFeedRow map_row(QSqlQuery& q);
};

} // namespace fincept
