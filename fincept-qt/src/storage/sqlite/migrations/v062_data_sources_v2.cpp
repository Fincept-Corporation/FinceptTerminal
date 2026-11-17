// v062_data_sources_v2 — Data Sources connections that are really tested, and Data Mapping v2
// (tabular mappings from any source, saved as versioned specs, with a stored output).
//
//   data_sources            + last_status, last_code, last_message, last_tested_at, last_latency_ms
//                             (the result of the last real TEST: "ok" / "error", the connector-contract
//                             error code, a plain message, when, and the round-trip time)
//   data_mappings           + spec_json, spec_version, revision, last_run_at, last_run_status,
//                             last_run_rows, last_run_errors
//   data_mapping_revisions  (new) every saved spec of a mapping, so a run can be repeated against
//                           refreshed data with the exact mapping of any earlier save
//   mapped_datasets         (new) the last output of each mapping (columns, types, rows) — what the
//                           workflow / export paths read
//
// EXISTING ROWS ARE KEPT AS THEY ARE: a mapping saved before v062 has spec_version 0 and an empty
// spec_json; DataMappingRepository converts it on read (MappingSpec spec_from_legacy) and writes
// spec_json on its next save. Nothing is dropped or rewritten here.
//
// SAFE ON A POPULATED DATABASE AND IDEMPOTENT: every ADD COLUMN is guarded by PRAGMA table_info
// (only NOT NULL columns WITH a constant default, which SQLite allows on tables with rows), new
// tables use IF NOT EXISTS, and a missing base table is skipped so the in-memory reference database
// of MigrationRunner::inspect() migrates cleanly.
//
// REGISTRATION: declared in MigrationRunner.h, called from main.cpp after register_migration_v061(),
// listed in LegacyRootAdoption.cpp register_known_migrations() (kKnownHighestMigration = 62).

#include "storage/sqlite/migrations/MigrationRunner.h"

#include <QHash>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>

namespace fincept {

void register_migration_v062();

namespace {

struct V062Column {
    const char* table;
    const char* column;
    const char* decl;
};

constexpr V062Column kV062Columns[] = {
    {"data_sources", "last_status", "TEXT NOT NULL DEFAULT ''"},
    {"data_sources", "last_code", "TEXT NOT NULL DEFAULT ''"},
    {"data_sources", "last_message", "TEXT NOT NULL DEFAULT ''"},
    {"data_sources", "last_tested_at", "TEXT NOT NULL DEFAULT ''"},
    {"data_sources", "last_latency_ms", "INTEGER NOT NULL DEFAULT -1"},
    {"data_mappings", "spec_json", "TEXT NOT NULL DEFAULT ''"},
    {"data_mappings", "spec_version", "INTEGER NOT NULL DEFAULT 0"},
    {"data_mappings", "revision", "INTEGER NOT NULL DEFAULT 1"},
    {"data_mappings", "last_run_at", "TEXT NOT NULL DEFAULT ''"},
    {"data_mappings", "last_run_status", "TEXT NOT NULL DEFAULT ''"},
    {"data_mappings", "last_run_rows", "INTEGER NOT NULL DEFAULT 0"},
    {"data_mappings", "last_run_errors", "INTEGER NOT NULL DEFAULT 0"},
};

Result<QSet<QString>> v062_columns(QSqlDatabase& db, const QString& table, bool* exists) {
    QSqlQuery q(db);
    // The table names are compile-time constants above — never user input.
    if (!q.exec(QStringLiteral("PRAGMA table_info(%1)").arg(table)))
        return Result<QSet<QString>>::err(q.lastError().text().toStdString());
    QSet<QString> out;
    while (q.next())
        out.insert(q.value(1).toString().toLower());
    *exists = !out.isEmpty();
    return Result<QSet<QString>>::ok(out);
}

Result<void> v062_exec(QSqlDatabase& db, const char* stmt) {
    QSqlQuery q(db);
    if (!q.exec(QString::fromLatin1(stmt)))
        return Result<void>::err(QStringLiteral("v062: %1").arg(q.lastError().text()).toStdString());
    return Result<void>::ok();
}

Result<void> apply_v062(QSqlDatabase& db) {
    QHash<QString, QSet<QString>> known;
    QHash<QString, bool> present;
    for (const V062Column& c : kV062Columns) {
        const QString table = QString::fromLatin1(c.table);
        if (!present.contains(table)) {
            bool exists = false;
            auto cols = v062_columns(db, table, &exists);
            if (cols.is_err())
                return Result<void>::err(cols.error());
            present.insert(table, exists);
            known.insert(table, cols.value());
        }
        if (!present.value(table))
            continue;
        const QString column = QString::fromLatin1(c.column);
        if (known[table].contains(column))
            continue;
        QSqlQuery q(db);
        if (!q.exec(QStringLiteral("ALTER TABLE %1 ADD COLUMN %2 %3").arg(table, column, QLatin1String(c.decl))))
            return Result<void>::err(
                QStringLiteral("v062: %1.%2: %3").arg(table, column, q.lastError().text()).toStdString());
        known[table].insert(column);
    }

    if (present.value(QStringLiteral("data_mappings"))) {
        auto r = v062_exec(db, "CREATE TABLE IF NOT EXISTS data_mapping_revisions ("
                               "  mapping_id TEXT    NOT NULL REFERENCES data_mappings(id) ON DELETE CASCADE,"
                               "  revision   INTEGER NOT NULL,"
                               "  spec_json  TEXT    NOT NULL DEFAULT '',"
                               "  saved_at   TEXT    DEFAULT (datetime('now')),"
                               "  PRIMARY KEY (mapping_id, revision)"
                               ")");
        if (r.is_err())
            return r;
        r = v062_exec(db, "CREATE TABLE IF NOT EXISTS mapped_datasets ("
                          "  mapping_id   TEXT    PRIMARY KEY REFERENCES data_mappings(id) ON DELETE CASCADE,"
                          "  revision     INTEGER NOT NULL DEFAULT 1,"
                          "  schema_name  TEXT    NOT NULL DEFAULT '',"
                          "  columns_json TEXT    NOT NULL DEFAULT '[]',"
                          "  types_json   TEXT    NOT NULL DEFAULT '[]',"
                          "  rows_json    TEXT    NOT NULL DEFAULT '[]',"
                          "  row_count    INTEGER NOT NULL DEFAULT 0,"
                          "  error_cells  INTEGER NOT NULL DEFAULT 0,"
                          "  source       TEXT    NOT NULL DEFAULT '',"
                          "  produced_at  TEXT    DEFAULT (datetime('now'))"
                          ")");
        if (r.is_err())
            return r;
    }
    return Result<void>::ok();
}

} // anonymous namespace

void register_migration_v062() {
    static bool done = false;
    if (done)
        return;
    done = true;
    MigrationRunner::register_migration({62, "data_sources_v2", apply_v062});
}

} // namespace fincept
