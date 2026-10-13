// v053_instrument_indexes — index the instruments table by broker.
//
// Every per-broker statement on `instruments` was a full-table scan, because no
// index (v048 recreated five) starts with broker_id:
//   - InstrumentRepository::last_updated()  SELECT MAX(updated_at) … WHERE broker_id = ?
//     — the freshness gate InstrumentService::refresh() runs on the UI thread;
//   - InstrumentRepository::count()         SELECT COUNT(*) … WHERE broker_id = ?
//   - the cache loaders                      SELECT … FROM instruments WHERE broker_id = ?
//   - InstrumentRepository::replace_all()   DELETE FROM instruments WHERE broker_id = ?
// With a few broker masters on disk that is several hundred thousand rows per call.
// (broker_id, updated_at) serves all four, and makes MAX(updated_at) a single
// index seek.
//
// SAFE ON A POPULATED DATABASE: CREATE INDEX IF NOT EXISTS adds an index and
// touches no table rows.

#include "storage/sqlite/migrations/MigrationRunner.h"

#include <QSqlError>
#include <QSqlQuery>

namespace fincept {
namespace {

// Uniquely named (not a bare `sql`) — unity builds concatenate ~20 TUs, and two
// anonymous-namespace helpers sharing a name in one batch is a redefinition.
Result<void> sql_v053(QSqlDatabase& db, const char* stmt) {
    QSqlQuery q(db);
    if (!q.exec(stmt))
        return Result<void>::err(q.lastError().text().toStdString());
    return Result<void>::ok();
}

Result<void> apply_v053(QSqlDatabase& db) {
    return sql_v053(db, "CREATE INDEX IF NOT EXISTS idx_instruments_broker_updated "
                        "ON instruments(broker_id, updated_at)");
}

} // anonymous namespace

void register_migration_v053() {
    static bool done = false;
    if (done)
        return;
    done = true;
    MigrationRunner::register_migration({53, "instrument_indexes", apply_v053});
}

} // namespace fincept
