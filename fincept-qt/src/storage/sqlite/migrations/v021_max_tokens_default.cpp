// v021_max_tokens_default — Reset legacy 2000-token defaults so they fall
// back to ModelCatalog::output_cap(provider, model) at runtime.
//
// Background: v002 created llm_global_settings with `max_tokens INTEGER
// DEFAULT 2000` and v010 carried that value into per-profile rows. The
// 2000 figure was a debug holdover that throttled every LLM reply to
// 2000 tokens regardless of model. Modern models support far more
// (Kimi K2.6 → 256k context / 65k+ output, Sonnet 4.6 → 64k, gpt-5 →
// 128k, etc.). LlmService now resolves max_tokens dynamically via
// ModelCatalog when the stored value is 0/NULL.
//
// This migration:
//   1. Sets the global default row's max_tokens from 2000 → 0.
//   2. Sets every llm_profiles row that still has the seeded 2000 to 0.
//
// Users who deliberately configured 2000 are indistinguishable from
// users who never touched the setting — that's fine. 2000 was never a
// reasonable user choice, so resetting them all to "use model default"
// is the right call.

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

static Result<void> apply_v021(QSqlDatabase& db) {
    // Reset legacy 2000 in global settings. Use 0 as the "no override —
    // resolve from ModelCatalog" sentinel.
    if (auto r = sql(db, "UPDATE llm_global_settings SET max_tokens = 0 WHERE max_tokens = 2000");
        r.is_err())
        return r;

    // Same for per-profile rows. The table only exists from v010 onward;
    // a fresh DB will pass through v010 before reaching v021, so this
    // statement is always safe.
    if (auto r = sql(db, "UPDATE llm_profiles SET max_tokens = 0 WHERE max_tokens = 2000");
        r.is_err())
        return r;

    return Result<void>::ok();
}

} // anonymous namespace

void register_migration_v021() {
    static bool done = false;
    if (done)
        return;
    done = true;
    MigrationRunner::register_migration({21, "max_tokens_default", apply_v021});
}

} // namespace fincept
