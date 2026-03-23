// v004_cleanup — Drop legacy notes table (superseded by financial_notes).

#include "storage/sqlite/migrations/MigrationRunner.h"

#include <QSqlQuery>

namespace fincept {
namespace {

Result<void> apply_v004(QSqlDatabase& db) {
    // The old `notes` table was supposed to be migrated and dropped in v001,
    // but some installs still have it. Drop it unconditionally — all data
    // lives in financial_notes now.
    QSqlQuery q(db);
    q.exec("DROP TABLE IF EXISTS notes");
    return Result<void>::ok();
}

} // anonymous namespace

void register_migration_v004() {
    static bool done = false;
    if (done)
        return;
    done = true;
    MigrationRunner::register_migration({4, "cleanup_legacy_notes", apply_v004});
}

} // namespace fincept
