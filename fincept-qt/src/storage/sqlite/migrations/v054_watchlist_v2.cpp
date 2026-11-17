// v054_watchlist_v2 — watchlist custom groups and price alerts.
//
//   watchlist_stocks.group_name  TEXT DEFAULT ''
//       A user-defined group per list entry (the Watchlist screen's "custom
//       group" grouping). Empty = ungrouped.
//
//   watchlist_alerts
//       Price / % change / volume spike / 52-week high-low / moving-average
//       cross alerts, evaluated on quote updates by WatchlistAlertService.
//       Keyed by symbol (an alert follows the instrument, whichever lists it is
//       on); `watchlist_id` only records the list it was created from.
//
// SAFE ON A POPULATED DATABASE: additive only. The column is added only when it
// is missing (SQLite has no ADD COLUMN IF NOT EXISTS), the table and index use
// IF NOT EXISTS, and no existing row is read, rewritten or deleted — every list,
// symbol, note and sort order stays exactly as it was. Re-running is a no-op.

#include "storage/sqlite/migrations/MigrationRunner.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace fincept {

void register_migration_v054();

namespace {

// Uniquely named — unity builds concatenate migration TUs into one batch.
Result<void> sql_v054(QSqlDatabase& db, const char* stmt) {
    QSqlQuery q(db);
    if (!q.exec(QString::fromLatin1(stmt)))
        return Result<void>::err(q.lastError().text().toStdString());
    return Result<void>::ok();
}

bool column_exists_v054(QSqlDatabase& db, const QString& table, const QString& column) {
    QSqlQuery q(db);
    if (!q.exec(QStringLiteral("PRAGMA table_info(%1)").arg(table)))
        return false;
    while (q.next()) {
        if (q.value(1).toString().compare(column, Qt::CaseInsensitive) == 0)
            return true;
    }
    return false;
}

Result<void> apply_v054(QSqlDatabase& db) {
    if (!column_exists_v054(db, QStringLiteral("watchlist_stocks"), QStringLiteral("group_name"))) {
        auto r = sql_v054(db, "ALTER TABLE watchlist_stocks ADD COLUMN group_name TEXT DEFAULT ''");
        if (r.is_err())
            return r;
    }

    auto r = sql_v054(db, "CREATE TABLE IF NOT EXISTS watchlist_alerts ("
                          "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
                          "  symbol TEXT NOT NULL,"
                          "  watchlist_id TEXT DEFAULT '',"
                          "  kind TEXT NOT NULL,"
                          "  threshold REAL,"
                          "  ma_period INTEGER DEFAULT 0,"
                          "  enabled INTEGER DEFAULT 1,"
                          "  repeat INTEGER DEFAULT 0,"
                          "  note TEXT DEFAULT '',"
                          "  last_triggered_at TEXT DEFAULT '',"
                          "  trigger_count INTEGER DEFAULT 0,"
                          "  created_at TEXT DEFAULT (datetime('now')),"
                          "  updated_at TEXT DEFAULT (datetime('now'))"
                          ")");
    if (r.is_err())
        return r;

    return sql_v054(db, "CREATE INDEX IF NOT EXISTS idx_watchlist_alerts_symbol ON watchlist_alerts(symbol)");
}

} // anonymous namespace

void register_migration_v054() {
    static bool done = false;
    if (done)
        return;
    done = true;
    MigrationRunner::register_migration({54, "watchlist_v2", apply_v054});
}

} // namespace fincept
