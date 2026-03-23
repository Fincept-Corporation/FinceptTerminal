#pragma once
#include "storage/repositories/BaseRepository.h"

#include <QStringList>

namespace fincept {

struct NewsMonitorRow {
    QString id;
    QString label;
    QStringList keywords;
    QString color;
    bool enabled = true;
};

class NewsMonitorRepository : public BaseRepository<NewsMonitorRow> {
  public:
    static NewsMonitorRepository& instance();

    Result<QVector<NewsMonitorRow>> list_all();
    Result<void> upsert(const NewsMonitorRow& m);
    Result<void> remove(const QString& id);
    Result<void> set_enabled(const QString& id, bool enabled);

  private:
    NewsMonitorRepository() = default;
    static NewsMonitorRow map_row(QSqlQuery& q);
};

} // namespace fincept
