// v039_algo_schema_fix — repair algo_trading schema drift on dev DBs that were
// migrated to v23 by an OLDER v023 which predated the entry_side / current_price
// ADD COLUMNs. Those ALTERs live in v023 now, but v23 is already marked applied on
// existing DBs, so they never ran there. At runtime the deploy INSERT
// (algo_deployments.entry_side) and the metrics UPSERT (algo_metrics.current_price)
// then fail with "no such column" / "Parameter count mismatch", so deployments
// never persist and metrics never save. Add the columns idempotently so freshly
// created DBs (which already have them via v023) and drifted DBs converge.

#include "storage/sqlite/migrations/MigrationRunner.h"

#include <QSqlError>
#include <QSqlQuery>

namespace fincept {
namespace {

bool v039_column_exists(QSqlDatabase& db, const QString& table, const QString& column) {
    QSqlQuery q(db);
    q.exec(QString("PRAGMA table_info(%1)").arg(table));
    while (q.next())
        if (q.value(1).toString().compare(column, Qt::CaseInsensitive) == 0)
            return true;
    return false;
}

Result<void> v039_add_if_missing(QSqlDatabase& db, const QString& table, const QString& column,
                                 const QString& decl) {
    if (v039_column_exists(db, table, column))
        return Result<void>::ok();
    QSqlQuery q(db);
    if (!q.exec(QString("ALTER TABLE %1 ADD COLUMN %2 %3").arg(table, column, decl)))
        return Result<void>::err(q.lastError().text().toStdString());
    return Result<void>::ok();
}

Result<void> apply_v039(QSqlDatabase& db) {
    struct Col {
        const char* table;
        const char* column;
        const char* decl;
    };
    const Col cols[] = {
        {"algo_deployments", "entry_side", "TEXT DEFAULT 'BUY'"},
        {"algo_metrics", "current_price", "REAL DEFAULT 0"},
        {"algo_metrics", "entry_side", "TEXT DEFAULT 'BUY'"},
    };
    for (const auto& c : cols) {
        auto r = v039_add_if_missing(db, c.table, c.column, c.decl);
        if (r.is_err())
            return r;
    }
    return Result<void>::ok();
}

} // namespace

void register_migration_v039() {
    static bool done = false;
    if (done)
        return;
    done = true;
    MigrationRunner::register_migration({39, "algo_schema_fix", apply_v039});
}

} // namespace fincept
