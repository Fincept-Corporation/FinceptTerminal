// v049_order_baskets — Zerodha-style saved order baskets, broker-agnostic.
// A basket is a named group of order legs (symbol/side/qty/type/price/product)
// stored as JSON (ActionCenter::serialize_unified_order shape). Execution is
// per-account at runtime via UnifiedTrading::place_basket_orders, so the same
// basket runs on any broker — or several at once.

#include "storage/sqlite/migrations/MigrationRunner.h"

#include <QSqlError>
#include <QSqlQuery>

namespace fincept {
namespace {

Result<void> apply_v049(QSqlDatabase& db) {
    QSqlQuery q(db);
    if (!q.exec("CREATE TABLE IF NOT EXISTS order_baskets ("
                "  id         TEXT PRIMARY KEY,"          // UUID
                "  name       TEXT NOT NULL,"
                "  legs       TEXT NOT NULL DEFAULT '[]'," // JSON array of serialized UnifiedOrders
                "  created_at TEXT DEFAULT (datetime('now')),"
                "  updated_at TEXT DEFAULT (datetime('now'))"
                ")"))
        return Result<void>::err(q.lastError().text().toStdString());
    return Result<void>::ok();
}

} // namespace

void register_migration_v049() {
    static bool done = false;
    if (done)
        return;
    done = true;
    MigrationRunner::register_migration({49, "order_baskets", apply_v049});
}

} // namespace fincept
