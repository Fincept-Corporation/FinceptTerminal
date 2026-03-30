// v011_custom_indices — Custom Index creation and tracking.
//
// Introduces two tables:
//   custom_indices      — named index definitions (method, base value, constituents JSON)
//   custom_index_values — daily computed index values for performance tracking

#include "storage/sqlite/migrations/MigrationRunner.h"

#include <QSqlError>
#include <QSqlQuery>

namespace fincept {
namespace {

static Result<void> sql_v011(QSqlDatabase& db, const char* stmt) {
    QSqlQuery q(db);
    if (!q.exec(stmt))
        return Result<void>::err(q.lastError().text().toStdString());
    return Result<void>::ok();
}

Result<void> apply_v011(QSqlDatabase& db) {

    // ── custom_indices ────────────────────────────────────────────────────────
    // Stores user-created index definitions.
    // constituents_json: JSON array of {symbol, weight, price_at_creation}
    auto r = sql_v011(db,
        "CREATE TABLE IF NOT EXISTS custom_indices ("
        "  id                  TEXT    PRIMARY KEY,"           // UUID
        "  name                TEXT    NOT NULL UNIQUE,"       // user label
        "  method              TEXT    NOT NULL,"              // e.g. 'Equal Weighted'
        "  base_value          REAL    NOT NULL DEFAULT 1000," // starting index level
        "  portfolio_id        TEXT,"                          // optional source portfolio
        "  constituents_json   TEXT    NOT NULL DEFAULT '[]'," // JSON array
        "  created_at          TEXT    DEFAULT (datetime('now')),"
        "  updated_at          TEXT    DEFAULT (datetime('now'))"
        ")"
    );
    if (r.is_err()) return r;

    r = sql_v011(db,
        "CREATE INDEX IF NOT EXISTS idx_custom_indices_name "
        "ON custom_indices(name)"
    );
    if (r.is_err()) return r;

    // ── custom_index_values ───────────────────────────────────────────────────
    // One row per index per date — stores the computed index level.
    r = sql_v011(db,
        "CREATE TABLE IF NOT EXISTS custom_index_values ("
        "  id          INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  index_id    TEXT    NOT NULL"
        "    REFERENCES custom_indices(id) ON DELETE CASCADE,"
        "  date        TEXT    NOT NULL,"                      // YYYY-MM-DD
        "  value       REAL    NOT NULL,"                      // computed index level
        "  UNIQUE(index_id, date)"
        ")"
    );
    if (r.is_err()) return r;

    r = sql_v011(db,
        "CREATE INDEX IF NOT EXISTS idx_custom_index_values_index_date "
        "ON custom_index_values(index_id, date DESC)"
    );
    if (r.is_err()) return r;

    return Result<void>::ok();
}

} // anonymous namespace

void register_migration_v011() {
    static bool done = false;
    if (done) return;
    done = true;
    MigrationRunner::register_migration({11, "custom_indices", apply_v011});
}

} // namespace fincept
