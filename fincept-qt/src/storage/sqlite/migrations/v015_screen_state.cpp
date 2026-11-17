// v015_screen_state — historical slot, now a deliberate no-op.
//
// The screen_state table lives in cache.db, not fincept.db, and
// CacheDatabase::create_tables() creates it whenever cache.db is opened. This
// migration used to issue the same DDL through CacheDatabase::instance() from
// inside a fincept.db migration transaction, which (a) wrote to a second file
// that the transaction could not roll back, (b) recorded success even when
// cache.db was closed, and (c) made the migration depend on global state, so it
// could not run on the throwaway in-memory database that
// MigrationRunner::inspect() builds its reference schema on.
//
// The version number stays registered so the sequence has no gap and existing
// databases (which already recorded v15) are unaffected.

#include "storage/sqlite/migrations/MigrationRunner.h"

namespace fincept {
namespace {

Result<void> apply_v015(QSqlDatabase& /*db*/) {
    return Result<void>::ok();
}

} // anonymous namespace

void register_migration_v015() {
    static bool done = false;
    if (done)
        return;
    done = true;
    MigrationRunner::register_migration({15, "screen_state", apply_v015});
}

} // namespace fincept
