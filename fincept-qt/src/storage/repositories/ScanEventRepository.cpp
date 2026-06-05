// src/storage/repositories/ScanEventRepository.cpp
#include "storage/repositories/ScanEventRepository.h"

#include <QUuid>

namespace fincept {

namespace {
// Coerce possibly-null QString to non-null '' so NOT NULL TEXT binds never become SQL NULL.
QString nn(const QString& s) { return s.isNull() ? QString::fromLatin1("") : s; }
const char* kCols = "id, watch_id, symbol, detail, fired_at";
} // namespace

ScanEventRepository& ScanEventRepository::instance() {
    static ScanEventRepository s;
    return s;
}

ScanWatchEvent ScanEventRepository::map_row(QSqlQuery& q) {
    ScanWatchEvent e;
    e.id       = q.value(0).toString();
    e.watch_id = q.value(1).toString();
    e.symbol   = q.value(2).toString();
    e.detail   = q.value(3).toString();
    e.fired_at = q.value(4).toLongLong();
    return e;
}

Result<void> ScanEventRepository::record(const QString& watch_id, const QString& symbol,
                                         const QString& detail, qint64 fired_at) {
    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    return exec_write(
        "INSERT INTO scan_watch_events (id, watch_id, symbol, detail, fired_at)"
        " VALUES (?,?,?,?,?)",
        {id, nn(watch_id), nn(symbol), nn(detail), fired_at});
}

Result<void> ScanEventRepository::clear_for(const QString& watch_id) {
    return exec_write("DELETE FROM scan_watch_events WHERE watch_id=?", {watch_id});
}

Result<QVector<ScanWatchEvent>> ScanEventRepository::recent(int limit) {
    return query_list(
        QString("SELECT %1 FROM scan_watch_events ORDER BY fired_at DESC LIMIT ?").arg(kCols),
        {limit}, map_row);
}

} // namespace fincept
