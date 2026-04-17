#include "storage/sqlite/CacheDatabase.h"

#include "core/logging/Logger.h"

#include <QMutexLocker>

namespace fincept {

CacheDatabase& CacheDatabase::instance() {
    static CacheDatabase s;
    return s;
}

Result<void> CacheDatabase::open(const QString& path) {
    db_ = QSqlDatabase::addDatabase("QSQLITE", "fincept_cache");
    db_.setDatabaseName(path);
    if (!db_.open()) {
        return Result<void>::err(db_.lastError().text().toStdString());
    }
    LOG_INFO("CacheDB", "Opened cache database: " + path);

    auto pr = apply_pragmas();
    if (pr.is_err())
        return pr;

    auto tr = create_tables();
    if (tr.is_err())
        return tr;

    // One-shot startup sweep of expired rows — prevents unbounded growth of unified_cache
    // across sessions. Cheap: idx_cache_expires makes this a range scan.
    auto sweep = execute("DELETE FROM unified_cache WHERE expires_at < datetime('now')");
    if (sweep.is_ok()) {
        const int removed = sweep.value().numRowsAffected();
        if (removed > 0)
            LOG_INFO("CacheDB", QString("Startup sweep removed %1 expired cache rows").arg(removed));
    }

    return Result<void>::ok();
}

void CacheDatabase::close() {
    if (db_.isOpen())
        db_.close();
}

bool CacheDatabase::is_open() const {
    return db_.isOpen();
}

Result<QSqlQuery> CacheDatabase::execute(const QString& sql, const QVariantList& params) {
    QMutexLocker lock(&mutex_);
    QSqlQuery query(db_);
    query.prepare(sql);
    for (int i = 0; i < params.size(); ++i) {
        query.bindValue(i, params[i]);
    }
    if (!query.exec()) {
        return Result<QSqlQuery>::err(query.lastError().text().toStdString());
    }
    return Result<QSqlQuery>::ok(std::move(query));
}

Result<void> CacheDatabase::exec(const QString& sql) {
    QMutexLocker lock(&mutex_);
    QSqlQuery query(db_);
    if (!query.exec(sql)) {
        return Result<void>::err(query.lastError().text().toStdString());
    }
    return Result<void>::ok();
}

Result<void> CacheDatabase::apply_pragmas() {
    // synchronous=NORMAL under WAL is the standard safe/fast combo. OFF risked full DB
    // corruption on power loss — now that cache.db also holds tab_sessions and
    // screen_state, durability matters more than the marginal write gain of OFF.
    const char* pragmas[] = {
        "PRAGMA journal_mode = WAL",  "PRAGMA synchronous = NORMAL",  "PRAGMA cache_size = -10000",
        "PRAGMA temp_store = MEMORY", "PRAGMA mmap_size = 134217728", "PRAGMA busy_timeout = 3000",
    };
    for (auto* p : pragmas) {
        auto r = exec(p);
        if (r.is_err()) {
            LOG_WARN("CacheDB", QString("PRAGMA failed: %1 — %2").arg(p, QString::fromStdString(r.error())));
        }
    }
    return Result<void>::ok();
}

Result<void> CacheDatabase::create_tables() {
    // Unified cache with TTL
    auto r = exec("CREATE TABLE IF NOT EXISTS unified_cache ("
                  "  key TEXT PRIMARY KEY,"
                  "  value TEXT NOT NULL,"
                  "  category TEXT DEFAULT 'general',"
                  "  content_type TEXT DEFAULT 'json',"
                  "  ttl_seconds INTEGER DEFAULT 300,"
                  "  expires_at TEXT NOT NULL,"
                  "  created_at TEXT DEFAULT (datetime('now')),"
                  "  hit_count INTEGER DEFAULT 0,"
                  "  size_bytes INTEGER DEFAULT 0"
                  ")");
    if (r.is_err())
        return r;

    r = exec("CREATE INDEX IF NOT EXISTS idx_cache_expires ON unified_cache(expires_at)");
    if (r.is_err())
        return r;

    r = exec("CREATE INDEX IF NOT EXISTS idx_cache_category ON unified_cache(category)");
    if (r.is_err())
        return r;

    // Tab session state persistence
    r = exec("CREATE TABLE IF NOT EXISTS tab_sessions ("
             "  tab_id TEXT PRIMARY KEY,"
             "  screen_name TEXT NOT NULL,"
             "  scroll_position REAL DEFAULT 0,"
             "  filters_json TEXT DEFAULT '{}',"
             "  selections_json TEXT DEFAULT '{}',"
             "  last_accessed TEXT DEFAULT (datetime('now')),"
             "  updated_at TEXT DEFAULT (datetime('now'))"
             ")");
    if (r.is_err())
        return r;

    r = exec("CREATE INDEX IF NOT EXISTS idx_tab_sessions_accessed ON tab_sessions(last_accessed)");
    if (r.is_err())
        return r;

    // Screen UI state — survives crashes, restored per screen on next open
    r = exec("CREATE TABLE IF NOT EXISTS screen_state ("
             "  screen_key    TEXT PRIMARY KEY,"
             "  state_version INTEGER NOT NULL DEFAULT 1,"
             "  state_json    TEXT    NOT NULL DEFAULT '{}',"
             "  updated_at    TEXT    NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now')),"
             "  session_id    TEXT    NOT NULL DEFAULT ''"
             ")");
    if (r.is_err())
        return r;

    r = exec("CREATE INDEX IF NOT EXISTS idx_screen_state_updated ON screen_state(updated_at)");
    if (r.is_err())
        return r;

    LOG_INFO("CacheDB", "Cache tables initialized");
    return Result<void>::ok();
}

} // namespace fincept
