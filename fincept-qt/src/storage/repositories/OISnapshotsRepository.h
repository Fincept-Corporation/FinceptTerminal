#pragma once
// OISnapshotsRepository — minute-aligned OI/LTP/Vol/IV history per token.
//
// Owned by `OISnapshotter` (services/options/). The chain producer fans out
// per-leg updates to the snapshotter, which buffers in memory and flushes to
// `oi_snapshots` (created in migration v025) every 60s. Retention is 7 days
// rolling — `delete_older_than` is called on app start and once per hour.
//
// Schema lives in migration v025.

#include "services/options/OptionChainTypes.h"
#include "storage/repositories/BaseRepository.h"

namespace fincept {

class OISnapshotsRepository : public BaseRepository<fincept::services::options::OISample> {
  public:
    static OISnapshotsRepository& instance();

    /// Bulk INSERT OR REPLACE — single transaction. Each sample is a
    /// minute-aligned snapshot for one token.
    Result<void> upsert_batch(qint64 token, const QVector<fincept::services::options::OISample>& samples);

    /// Most recent N samples for a token (ts_minute DESC).
    Result<QVector<fincept::services::options::OISample>> get_recent(qint64 token, int limit);

    /// Time-windowed read, ordered ascending — suitable for charting.
    /// Bounds inclusive. `until_minute=0` means "now".
    Result<QVector<fincept::services::options::OISample>> get_window(qint64 token, qint64 since_minute,
                                                                      qint64 until_minute = 0);

    /// Delete rows older than the given minute floor (epoch seconds).
    /// Returns the number of rows deleted on success.
    Result<int> delete_older_than(qint64 minute_floor);

  private:
    OISnapshotsRepository() = default;
    static fincept::services::options::OISample map_row(QSqlQuery& q);
};

} // namespace fincept
