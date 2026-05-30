// v036_instrument_broker_token — add a TEXT column carrying each broker's native
// non-numeric instrument key (Upstox "NSE_EQ|INE…", Samco "758960_NSE"). Numeric-
// token brokers leave it empty and keep using instrument_token. Needed so the
// catalog can route market-data/orders for string-token brokers, and so those
// rows get a distinct numeric instrument_token (via norm::stable_token) instead
// of all colliding on 0 under UNIQUE(instrument_token, broker_id).

#include "storage/sqlite/migrations/MigrationRunner.h"

#include <QSqlError>
#include <QSqlQuery>

namespace fincept {
namespace {

Result<void> apply_v036(QSqlDatabase& db) {
    QSqlQuery q(db);
    // MigrationRunner guarantees a migration applies at most once, so a plain
    // ADD COLUMN (which errors if the column already exists) is safe here.
    if (!q.exec("ALTER TABLE instruments ADD COLUMN broker_token TEXT NOT NULL DEFAULT ''"))
        return Result<void>::err(q.lastError().text().toStdString());
    return Result<void>::ok();
}

} // namespace

void register_migration_v036() {
    static bool done = false;
    if (done)
        return;
    done = true;
    MigrationRunner::register_migration({36, "instrument_broker_token", apply_v036});
}

} // namespace fincept
