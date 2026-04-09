// v012_data_normalization — Persistent mapping configs and normalized data storage.
//
// Introduces two tables:
//   data_mappings    — user-defined field mapping configs (persists DataMappingScreen state)
//   normalized_data  — output of normalization runs (raw + normalized JSON, per schema)

#include "storage/sqlite/migrations/MigrationRunner.h"

#include <QSqlError>
#include <QSqlQuery>

namespace fincept {
namespace {

static Result<void> sql_v012(QSqlDatabase& db, const char* stmt) {
    QSqlQuery q(db);
    if (!q.exec(stmt))
        return Result<void>::err(q.lastError().text().toStdString());
    return Result<void>::ok();
}

Result<void> apply_v012(QSqlDatabase& db) {

    // ── data_mappings ─────────────────────────────────────────────────────────
    // Persists user-created field mapping configs from DataMappingScreen.
    // field_mappings_json: JSON array of {target, expression, transform, default_val}
    auto r = sql_v012(db,
                      "CREATE TABLE IF NOT EXISTS data_mappings ("
                      "  id                  TEXT    PRIMARY KEY,"
                      "  name                TEXT    NOT NULL,"
                      "  source_id           TEXT"
                      "    REFERENCES data_sources(id) ON DELETE SET NULL,"
                      "  schema_name         TEXT    NOT NULL," // OHLCV, QUOTE, TICK, etc.
                      "  base_url            TEXT    NOT NULL DEFAULT '',"
                      "  endpoint            TEXT    NOT NULL DEFAULT '',"
                      "  method              TEXT    NOT NULL DEFAULT 'GET',"
                      "  auth_type           TEXT    NOT NULL DEFAULT 'None',"
                      "  auth_token          TEXT    NOT NULL DEFAULT '',"
                      "  headers             TEXT    NOT NULL DEFAULT '',"
                      "  body                TEXT    NOT NULL DEFAULT '',"
                      "  parser              TEXT    NOT NULL DEFAULT 'JSONPath',"
                      "  cache_enabled       INTEGER NOT NULL DEFAULT 1,"
                      "  cache_ttl           INTEGER NOT NULL DEFAULT 300,"
                      "  field_mappings_json TEXT    NOT NULL DEFAULT '[]',"
                      "  created_at          TEXT    DEFAULT (datetime('now')),"
                      "  updated_at          TEXT    DEFAULT (datetime('now'))"
                      ")");
    if (r.is_err())
        return r;

    r = sql_v012(db, "CREATE INDEX IF NOT EXISTS idx_data_mappings_source "
                     "ON data_mappings(source_id)");
    if (r.is_err())
        return r;

    r = sql_v012(db, "CREATE INDEX IF NOT EXISTS idx_data_mappings_schema "
                     "ON data_mappings(schema_name)");
    if (r.is_err())
        return r;

    // ── normalized_data ───────────────────────────────────────────────────────
    // Stores the output of each normalization run.
    // normalized_json: validated, schema-conformant JSON object
    // raw_json:        original API response (for audit/debugging)
    // validation_errors: JSON array of error strings (empty if clean)
    r = sql_v012(db,
                 "CREATE TABLE IF NOT EXISTS normalized_data ("
                 "  id                  TEXT    PRIMARY KEY,"
                 "  mapping_id          TEXT    NOT NULL"
                 "    REFERENCES data_mappings(id) ON DELETE CASCADE,"
                 "  source_id           TEXT"
                 "    REFERENCES data_sources(id) ON DELETE SET NULL,"
                 "  schema_name         TEXT    NOT NULL,"
                 "  normalized_json     TEXT    NOT NULL DEFAULT '{}',"
                 "  raw_json            TEXT    NOT NULL DEFAULT '{}',"
                 "  validation_errors   TEXT    NOT NULL DEFAULT '[]',"
                 "  data_hash           TEXT    NOT NULL DEFAULT ''," // SHA256 of normalized_json
                 "  extracted_at        TEXT    DEFAULT (datetime('now'))"
                 ")");
    if (r.is_err())
        return r;

    r = sql_v012(db, "CREATE INDEX IF NOT EXISTS idx_normalized_data_mapping "
                     "ON normalized_data(mapping_id, extracted_at DESC)");
    if (r.is_err())
        return r;

    r = sql_v012(db, "CREATE INDEX IF NOT EXISTS idx_normalized_data_schema "
                     "ON normalized_data(schema_name, extracted_at DESC)");
    if (r.is_err())
        return r;

    return Result<void>::ok();
}

} // anonymous namespace

void register_migration_v012() {
    static bool done = false;
    if (done)
        return;
    done = true;
    MigrationRunner::register_migration({12, "data_normalization", apply_v012});
}

} // namespace fincept
