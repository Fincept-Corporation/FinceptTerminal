// v025_oi_snapshots — Intraday Open Interest snapshots for F&O.
//
// Owned by `OISnapshotter` (services/options/). Populated from the chain
// producer's per-publish fan-out; flushed every 60s on minute boundaries.
// Retention is 7 days rolling — older rows are deleted by the snapshotter
// on its hourly housekeeping pass.
//
// Schema notes:
//   - `ts_minute` is a unix epoch second floored to its minute. Composite
//     PK (token, ts_minute) means we INSERT OR REPLACE per minute per leg.
//   - WITHOUT ROWID keeps the row footprint small (5 numeric columns +
//     composite key = ~32 bytes/row) and makes the PK index the table.
//   - One supporting index over token + ts_minute DESC for the common
//     `oi:history:<broker>:<token>:<window>` query (most-recent-N).

#include "storage/sqlite/migrations/MigrationRunner.h"

#include <QSqlError>
#include <QSqlQuery>

namespace fincept {
namespace {

static Result<void> v025_sql(QSqlDatabase& db, const char* stmt) {
    QSqlQuery q(db);
    if (!q.exec(stmt))
        return Result<void>::err(q.lastError().text().toStdString());
    return Result<void>::ok();
}

Result<void> apply_v025(QSqlDatabase& db) {
    {
        auto r = v025_sql(db, R"sql(
            CREATE TABLE IF NOT EXISTS oi_snapshots (
                token       INTEGER NOT NULL,
                ts_minute   INTEGER NOT NULL,
                oi          INTEGER NOT NULL DEFAULT 0,
                ltp         REAL    NOT NULL DEFAULT 0,
                vol         INTEGER NOT NULL DEFAULT 0,
                iv          REAL    NOT NULL DEFAULT 0,
                PRIMARY KEY (token, ts_minute)
            ) WITHOUT ROWID
        )sql");
        if (r.is_err()) return r;
    }
    {
        auto r = v025_sql(db, "CREATE INDEX IF NOT EXISTS idx_oi_snapshots_token_ts "
                              "ON oi_snapshots(token, ts_minute DESC)");
        if (r.is_err()) return r;
    }
    return Result<void>::ok();
}

} // anonymous namespace

void register_migration_v025() {
    static bool done = false;
    if (done) return;
    done = true;
    MigrationRunner::register_migration({25, "oi_snapshots", apply_v025});
}

} // namespace fincept
