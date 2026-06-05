// v040_paper_trading_realism — make the paper simulator behave like a real Indian
// broker. pt_positions gains a `product` column (MIS/CNC/NRML) so the engine can
// tell intraday from delivery (for the 15:30 MIS auto-square and MIS->CNC convert),
// and a `held_margin` column so an open position keeps its margin locked from
// available balance until it is closed (previously margin was released on fill, so
// available balance never reflected open positions — the "stuck at 10 lakh" bug).
//
// Columns are added idempotently (add-if-missing) so freshly created DBs (which get
// them here) and any DB already at >=40 converge. pt_orders already carries
// product/exchange (v001) and pt_holdings already exists (v001) — no change needed
// there; the repository simply starts persisting/reading them.

#include "storage/sqlite/migrations/MigrationRunner.h"

#include <QSqlError>
#include <QSqlQuery>

namespace fincept {
namespace {

bool v040_column_exists(QSqlDatabase& db, const QString& table, const QString& column) {
    QSqlQuery q(db);
    q.exec(QString("PRAGMA table_info(%1)").arg(table));
    while (q.next())
        if (q.value(1).toString().compare(column, Qt::CaseInsensitive) == 0)
            return true;
    return false;
}

Result<void> v040_add_if_missing(QSqlDatabase& db, const QString& table, const QString& column,
                                 const QString& decl) {
    if (v040_column_exists(db, table, column))
        return Result<void>::ok();
    QSqlQuery q(db);
    if (!q.exec(QString("ALTER TABLE %1 ADD COLUMN %2 %3").arg(table, column, decl)))
        return Result<void>::err(q.lastError().text().toStdString());
    return Result<void>::ok();
}

Result<void> apply_v040(QSqlDatabase& db) {
    struct Col {
        const char* table;
        const char* column;
        const char* decl;
    };
    const Col cols[] = {
        // Pre-existing positions have no product intent — default them to delivery
        // (CNC, carry-forward) so the 15:30 intraday auto-square never closes them.
        // New orders set the real product (MIS/CNC/NRML) explicitly.
        {"pt_positions", "product", "TEXT DEFAULT 'CNC'"},
        {"pt_positions", "held_margin", "REAL DEFAULT 0"},
    };
    for (const auto& c : cols) {
        auto r = v040_add_if_missing(db, c.table, c.column, c.decl);
        if (r.is_err())
            return r;
    }
    return Result<void>::ok();
}

} // namespace

void register_migration_v040() {
    static bool done = false;
    if (done)
        return;
    done = true;
    MigrationRunner::register_migration({40, "paper_trading_realism", apply_v040});
}

} // namespace fincept
