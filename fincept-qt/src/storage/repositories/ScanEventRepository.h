// src/storage/repositories/ScanEventRepository.h
#pragma once
#include "storage/repositories/BaseRepository.h"

#include <QString>
#include <QVector>

namespace fincept {

struct ScanWatchEvent {
    QString id;
    QString watch_id;
    QString symbol;
    QString detail;
    qint64  fired_at = 0;
};

class ScanEventRepository : public BaseRepository<ScanWatchEvent> {
  public:
    static ScanEventRepository& instance();

    Result<void> record(const QString& watch_id, const QString& symbol,
                        const QString& detail, qint64 fired_at);
    Result<void> clear_for(const QString& watch_id);   // cascade on watch delete
    Result<QVector<ScanWatchEvent>> recent(int limit = 100);

  private:
    ScanEventRepository() = default;
    static ScanWatchEvent map_row(QSqlQuery& q);
};

} // namespace fincept
