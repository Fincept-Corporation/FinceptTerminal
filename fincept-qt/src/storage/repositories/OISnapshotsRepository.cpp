#include "storage/repositories/OISnapshotsRepository.h"

#include "core/logging/Logger.h"

#include <QSqlError>
#include <QSqlQuery>

namespace fincept {

using fincept::services::options::OISample;

OISnapshotsRepository& OISnapshotsRepository::instance() {
    static OISnapshotsRepository s;
    return s;
}

OISample OISnapshotsRepository::map_row(QSqlQuery& q) {
    OISample s;
    s.ts_minute = q.value(0).toLongLong();
    s.oi = q.value(1).toLongLong();
    s.ltp = q.value(2).toDouble();
    s.vol = q.value(3).toLongLong();
    s.iv = q.value(4).toDouble();
    return s;
}

Result<void> OISnapshotsRepository::upsert_batch(qint64 token, const QVector<OISample>& samples) {
    if (samples.isEmpty())
        return Result<void>::ok();

    auto begin = db().begin_transaction();
    const bool in_tx = begin.is_ok();
    if (!in_tx) {
        LOG_WARN("OISnapshotsRepo", "transaction begin failed; using individual statements");
    }

    const QString sql = QStringLiteral(
        "INSERT OR REPLACE INTO oi_snapshots (token, ts_minute, oi, ltp, vol, iv) "
        "VALUES (?, ?, ?, ?, ?, ?)");

    for (const auto& s : samples) {
        auto r = db().execute(sql, {token, s.ts_minute, s.oi, s.ltp, s.vol, s.iv});
        if (r.is_err()) {
            if (in_tx)
                db().rollback();
            return Result<void>::err(r.error());
        }
    }
    if (in_tx) {
        auto c = db().commit();
        if (c.is_err())
            return c;
    }
    return Result<void>::ok();
}

Result<QVector<OISample>> OISnapshotsRepository::get_recent(qint64 token, int limit) {
    return query_list(
        "SELECT ts_minute, oi, ltp, vol, iv FROM oi_snapshots "
        "WHERE token = ? ORDER BY ts_minute DESC LIMIT ?",
        {token, limit}, &OISnapshotsRepository::map_row);
}

Result<QVector<OISample>> OISnapshotsRepository::get_window(qint64 token, qint64 since_minute,
                                                            qint64 until_minute) {
    if (until_minute <= 0) {
        return query_list(
            "SELECT ts_minute, oi, ltp, vol, iv FROM oi_snapshots "
            "WHERE token = ? AND ts_minute >= ? ORDER BY ts_minute ASC",
            {token, since_minute}, &OISnapshotsRepository::map_row);
    }
    return query_list(
        "SELECT ts_minute, oi, ltp, vol, iv FROM oi_snapshots "
        "WHERE token = ? AND ts_minute >= ? AND ts_minute <= ? ORDER BY ts_minute ASC",
        {token, since_minute, until_minute}, &OISnapshotsRepository::map_row);
}

Result<int> OISnapshotsRepository::delete_older_than(qint64 minute_floor) {
    auto r = db().execute("DELETE FROM oi_snapshots WHERE ts_minute < ?", {minute_floor});
    if (r.is_err())
        return Result<int>::err(r.error());
    return Result<int>::ok(r.value().numRowsAffected());
}

} // namespace fincept
