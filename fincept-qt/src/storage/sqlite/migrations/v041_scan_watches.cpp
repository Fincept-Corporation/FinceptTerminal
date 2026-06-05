// v041_scan_watches — persisted alert watches for the Algo→Scanner live monitor.
// One row per watch: serialized conditions (JSON array), symbols, data source,
// poll/realtime mode, interval, cooldown, action config (JSON), and live status.
// CREATE-IF-NOT-EXISTS so fresh and existing DBs converge.

#include "storage/sqlite/migrations/MigrationRunner.h"

#include <QSqlError>
#include <QSqlQuery>

namespace fincept {
namespace {

Result<void> apply_v041(QSqlDatabase& db) {
    QSqlQuery q(db);
    const char* sql =
        "CREATE TABLE IF NOT EXISTS scan_watches ("
        " id TEXT PRIMARY KEY,"
        " name TEXT NOT NULL,"
        " conditions_json TEXT NOT NULL DEFAULT '[]',"
        " logic TEXT NOT NULL DEFAULT 'AND',"
        " symbols TEXT NOT NULL DEFAULT '',"
        " timeframe TEXT NOT NULL DEFAULT '1m',"
        " lookback_days INTEGER NOT NULL DEFAULT 5,"
        " data_source TEXT NOT NULL DEFAULT 'Broker',"
        " broker_id TEXT NOT NULL DEFAULT '',"
        " account_id TEXT NOT NULL DEFAULT '',"
        " mode TEXT NOT NULL DEFAULT 'poll',"
        " interval_sec INTEGER NOT NULL DEFAULT 60,"
        " cooldown_min INTEGER NOT NULL DEFAULT 15,"
        " actions_json TEXT NOT NULL DEFAULT '{}',"
        " active INTEGER NOT NULL DEFAULT 1,"
        " status TEXT NOT NULL DEFAULT 'idle',"
        " last_eval_at INTEGER NOT NULL DEFAULT 0,"
        " last_fired_at INTEGER NOT NULL DEFAULT 0,"
        " created_at INTEGER NOT NULL DEFAULT 0,"
        " updated_at INTEGER NOT NULL DEFAULT 0"
        ")";
    if (!q.exec(QString::fromUtf8(sql)))
        return Result<void>::err(q.lastError().text().toStdString());
    return Result<void>::ok();
}

} // namespace

void register_migration_v041() {
    static bool done = false;
    if (done)
        return;
    done = true;
    MigrationRunner::register_migration({41, "scan_watches", apply_v041});
}

} // namespace fincept
