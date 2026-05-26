// v032_translation_cache — Persistent cache for runtime text translations.
//
// The active LLM is used to translate news headlines, summaries, and other
// short user-facing strings into the active UI language. Every translation
// the app produces is stored here so identical inputs never round-trip to
// the LLM twice — across runs, news refreshes, and language switches.
//
// Keying:
//   src_hash    — sha256 of the source text (32 hex chars used; cheap dedupe)
//   src_lang    — detected source language ("en", "fr", "auto"); empty if
//                 caller did not pre-detect
//   target_lang — Qt locale code ("zh_CN", "zh_TW"); matches LanguageManager
//
// (src_hash, target_lang) is the lookup key. Source text is stored verbatim
// for debugging and for the rare case where the hash collides (we re-check).
//
// `domain` is a free-text tag the caller sets to ease cache invalidation
// (e.g. "news.headline", "news.summary"). No FK — strings only.

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
    auto r = sql(db, "CREATE TABLE IF NOT EXISTS translation_cache ("
                     "  src_hash     TEXT NOT NULL,"
                     "  target_lang  TEXT NOT NULL,"
                     "  src_lang     TEXT DEFAULT '',"
                     "  domain       TEXT DEFAULT '',"
                     "  src_text     TEXT NOT NULL,"
                     "  translated   TEXT NOT NULL,"
                     "  provider     TEXT DEFAULT '',"
                     "  model        TEXT DEFAULT '',"
                     "  created_at   INTEGER DEFAULT (strftime('%s','now')),"
                     "  PRIMARY KEY (src_hash, target_lang)"
                     ")");
    if (r.is_err())
        return r;

    r = sql(db, "CREATE INDEX IF NOT EXISTS idx_translation_cache_domain "
                "ON translation_cache(domain)");
    if (r.is_err())
        return r;

    r = sql(db, "CREATE INDEX IF NOT EXISTS idx_translation_cache_created "
                "ON translation_cache(created_at DESC)");
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
    MigrationRunner::register_migration({32, "translation_cache", apply_v032});
}

} // namespace fincept
