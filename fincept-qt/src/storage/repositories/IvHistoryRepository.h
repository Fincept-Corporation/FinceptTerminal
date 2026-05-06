#pragma once
// IvHistoryRepository — daily ATM IV per underlying.
//
// Auto-populated by OptionChainService whenever a chain refresh produces
// an ATM IV value. Read by FnoHeaderBar's IV percentile pill (trailing
// 90-day percentile rank).

#include "storage/repositories/BaseRepository.h"

#include <QString>

namespace fincept {

struct IvHistoryRow {
    QString underlying;
    QString date_iso;
    double atm_iv = 0;
};

class IvHistoryRepository : public BaseRepository<IvHistoryRow> {
  public:
    static IvHistoryRepository& instance();

    /// INSERT OR REPLACE — last write wins per (underlying, date).
    Result<void> upsert(const QString& underlying, const QString& date_iso, double atm_iv);

    /// Trailing window for `underlying` from `since_iso` (inclusive) to
    /// today. Ascending by date.
    Result<QVector<IvHistoryRow>> get_window(const QString& underlying, const QString& since_iso);

    /// Today's row for `underlying`, or std::nullopt when not yet populated.
    std::optional<IvHistoryRow> get_today(const QString& underlying);

  private:
    IvHistoryRepository() = default;
    static IvHistoryRow map_row(QSqlQuery& q);
};

} // namespace fincept
