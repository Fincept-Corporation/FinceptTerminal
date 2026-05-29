// v032_pending_orders — Action Center (semi-auto order approval), Phase 3 §2.
//
// Stores orders queued for manual approval while an account is in SemiAuto
// order mode. The per-account order mode itself lives in the `settings` table
// (key "action_center.mode.<account_id>"), not here.
//
// order_data is the serialized order JSON (no credentials). Display fields
// (symbol/action/quantity/etc.) are derived at read time from order_data, so
// they are NOT stored as columns. status ∈ {pending, approved, rejected}.

#include "storage/sqlite/migrations/MigrationRunner.h"

#include <QSqlError>
#include <QSqlQuery>

namespace fincept {
namespace {

static Result<void> sql(QSqlDatabase& db, const char* stmt) {
    QSqlQuery q(db);
    if (!q.exec(stmt))
        return Result<void>::err(q.lastError().text().toStdString());
    return Result<void>::ok();
}

Result<void> apply_v032(QSqlDatabase& db) {
    auto r = sql(db, "CREATE TABLE IF NOT EXISTS pending_orders ("
                     "  id TEXT PRIMARY KEY,"
                     "  account_id TEXT NOT NULL,"
                     "  order_type TEXT NOT NULL,"
                     "  order_data TEXT NOT NULL,"
                     "  status TEXT NOT NULL DEFAULT 'pending',"
                     "  created_at TEXT,"
                     "  approved_at TEXT,"
                     "  rejected_at TEXT,"
                     "  rejection_reason TEXT,"
                     "  broker_order_id TEXT"
                     ")");
    if (r.is_err())
        return r;

    r = sql(db, "CREATE INDEX IF NOT EXISTS idx_pending_orders_account_status "
                "ON pending_orders(account_id, status)");
    if (r.is_err())
        return r;

    r = sql(db, "CREATE INDEX IF NOT EXISTS idx_pending_orders_status "
                "ON pending_orders(status)");
    if (r.is_err())
        return r;

    return Result<void>::ok();
}

} // anonymous namespace

void register_migration_v032() {
    static bool done = false;
    if (done)
        return;
    done = true;
    MigrationRunner::register_migration({32, "pending_orders", apply_v032});
}

} // namespace fincept
