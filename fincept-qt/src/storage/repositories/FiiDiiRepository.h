#pragma once
// FiiDiiRepository — daily institutional flows (NSE cash-market).
//
// Owned by `FiiDiiService`. Schema lives in migration v027.

#include "services/options/FiiDiiTypes.h"
#include "storage/repositories/BaseRepository.h"

namespace fincept {

class FiiDiiRepository : public BaseRepository<fincept::services::options::FiiDiiDay> {
  public:
    static FiiDiiRepository& instance();

    /// Idempotent — INSERT OR REPLACE keyed on date_iso.
    Result<void> upsert(const fincept::services::options::FiiDiiDay& day);
    Result<void> upsert_batch(const QVector<fincept::services::options::FiiDiiDay>& days);

    /// Most recent N days (DESC order — caller flips if needed).
    Result<QVector<fincept::services::options::FiiDiiDay>> get_recent(int limit);

    /// ISO date window (inclusive). Returns ASC by date_iso.
    Result<QVector<fincept::services::options::FiiDiiDay>> get_window(const QString& since_iso,
                                                                       const QString& until_iso = {});

  private:
    FiiDiiRepository() = default;
    static fincept::services::options::FiiDiiDay map_row(QSqlQuery& q);
};

} // namespace fincept
