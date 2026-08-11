#include "storage/sqlite/CacheDatabase.h"

#include "core/logging/Logger.h"

#include <QSqlRecord>
#include <QThread>

namespace fincept {

namespace {
constexpr const char* kCacheConnectionName = "fincept_cache";
constexpr const char* kCacheDbTag = "CacheDB";

QString cache_thread_label(QThread* t) {
    if (!t)
        return QStringLiteral("?");
    const QString name = t->objectName();
    return name.isEmpty() ? QStringLiteral("0x%1").arg(reinterpret_cast<quintptr>(t), 0, 16) : name;
}
} // namespace

CacheDatabase& CacheDatabase::instance() {
    static CacheDatabase s;
    return s;
}

Result<void> CacheDatabase::open(const QString& path) {
    main_thread_ = QThread::currentThread();
    db_path_ = path;

    db_ = QSqlDatabase::addDatabase("QSQLITE", QString::fromLatin1(kCacheConnectionName));
    db_.setDatabaseName(path);
    if (!db_.open()) {
        return Result<void>::err(db_.lastError().text().toStdString());
    }
    LOG_INFO(kCacheDbTag, "Opened cache database: " + path);

    auto pr = apply_pragmas(db_, /*include_database_wide=*/true);
    if (pr.is_err())
        return pr;

    auto tr = create_tables();
    if (tr.is_err())
        return tr;

    // Startup sweep of expired rows. StorageManager::start_retention_sweeper()
    // repeats this on a ~15 min timer — a terminal left open for days used to
    // accumulate expired rows until the next relaunch.
    const int removed = sweep_expired();
    if (removed > 0)
        LOG_INFO(kCacheDbTag, QString("Startup sweep removed %1 expired cache rows").arg(removed));

    return Result<void>::ok();
}

void CacheDatabase::close() {
    if (db_.isOpen())
        db_.close();
}

bool CacheDatabase::is_open() const {
    return db_.isOpen();
}

QSqlDatabase CacheDatabase::connection() {
    QThread* t = QThread::currentThread();

    // Main-thread fast path — the connection opened by `open()`.
    if (t == main_thread_)
        return db_;

    // Per-thread cloned connection. The thread_local Guard owns the
    // connection-name lifetime so the registry entry is removed when the thread
    // exits — this handles QtConcurrent pool workers, which keep their
    // connection across tasks and clean up at pool shutdown.
    struct Guard {
        QString name;
        ~Guard() {
            if (!name.isEmpty())
                QSqlDatabase::removeDatabase(name);
        }
    };
    thread_local Guard guard;

    if (guard.name.isEmpty()) {
        if (db_path_.isEmpty()) {
            LOG_ERROR(kCacheDbTag, "connection() called before open() — refusing to clone");
            return {};
        }

        guard.name = QStringLiteral("fincept_cache_thread_%1_%2")
                         .arg(cache_thread_label(t))
                         .arg(reinterpret_cast<quintptr>(t), 0, 16);

        // Clone by connection NAME: the QSqlDatabase-object overload would read
        // the main thread's handle and its live driver from this thread.
        QSqlDatabase clone = QSqlDatabase::cloneDatabase(QString::fromLatin1(kCacheConnectionName), guard.name);
        if (!clone.open()) {
            const QString err = clone.lastError().text();
            LOG_ERROR(kCacheDbTag,
                      QString("Failed to open per-thread cache connection on [%1]: %2").arg(cache_thread_label(t), err));
            QSqlDatabase::removeDatabase(guard.name);
            guard.name.clear();
            return {};
        }
        // journal_mode is file-global and already set by the main connection —
        // only re-apply the per-connection PRAGMAs.
        apply_pragmas(clone, /*include_database_wide=*/false);
        const int n = ++per_thread_connections_;
        LOG_INFO(kCacheDbTag,
                 QString("Opened per-thread cache connection on [%1] (total=%2)").arg(cache_thread_label(t)).arg(n));
    }
    return QSqlDatabase::database(guard.name, /*open=*/false);
}

Result<QSqlQuery> CacheDatabase::execute(const QString& sql, const QVariantList& params) {
    QSqlDatabase conn = connection();
    if (!conn.isOpen()) {
        return Result<QSqlQuery>::err("Cache DB connection unavailable on this thread");
    }
    QSqlQuery query(conn);

    // Check prepare() — see the identical note in Database::execute(). An
    // unchecked prepare turns "no such column" into a misleading "Parameter
    // count mismatch" at exec time, because bindValue() no-ops on a statement
    // that was never prepared.
    if (!query.prepare(sql)) {
        return Result<QSqlQuery>::err("prepare failed: " + query.lastError().text().toStdString());
    }

    for (int i = 0; i < params.size(); ++i) {
        query.bindValue(i, params[i]);
    }
    if (!query.exec()) {
        return Result<QSqlQuery>::err(query.lastError().text().toStdString());
    }
    return Result<QSqlQuery>::ok(std::move(query));
}

Result<QVector<QVariantList>> CacheDatabase::query_rows(const QString& sql, const QVariantList& params) {
    auto r = execute(sql, params);
    if (r.is_err())
        return Result<QVector<QVariantList>>::err(r.error());

    QVector<QVariantList> rows;
    auto& q = r.value();
    const int cols = q.record().count();
    while (q.next()) {
        QVariantList row;
        row.reserve(cols);
        for (int c = 0; c < cols; ++c)
            row.append(q.value(c));
        rows.append(std::move(row));
    }
    return Result<QVector<QVariantList>>::ok(std::move(rows));
}

int CacheDatabase::sweep_expired() {
    if (!is_open())
        return -1;
    // idx_cache_expires makes this a range scan, not a table scan.
    auto r = execute("DELETE FROM unified_cache WHERE expires_at < datetime('now')");
    if (r.is_err()) {
        LOG_WARN(kCacheDbTag, QString("Expiry sweep failed: %1").arg(QString::fromStdString(r.error())));
        return -1;
    }
    return r.value().numRowsAffected();
}

Result<void> CacheDatabase::exec(const QString& sql) {
    QSqlDatabase conn = connection();
    if (!conn.isOpen()) {
        return Result<void>::err("Cache DB connection unavailable on this thread");
    }
    QSqlQuery query(conn);
    if (!query.exec(sql)) {
        return Result<void>::err(query.lastError().text().toStdString());
    }
    return Result<void>::ok();
}

Result<void> CacheDatabase::apply_pragmas(QSqlDatabase& conn, bool include_database_wide) {
    // journal_mode is a property of the FILE — one connection sets it for all.
    // Everything else is per-connection and must be re-applied to every clone or
    // the workers silently run with SQLite defaults (synchronous=FULL, 0 ms busy
    // timeout → spurious SQLITE_BUSY under concurrent writers).
    static const char* kDatabaseWide[] = {
        "PRAGMA journal_mode = WAL",
    };
    // synchronous=NORMAL under WAL is the standard safe/fast combo. OFF risked full DB
    // corruption on power loss — now that cache.db also holds tab_sessions and
    // screen_state, durability matters more than the marginal write gain of OFF.
    // journal_size_limit bounds the -wal file: without it SQLite never truncates
    // it back after a checkpoint, so a one-off burst leaves it at high water.
    static const char* kPerConnection[] = {
        "PRAGMA synchronous = NORMAL",  "PRAGMA cache_size = -10000",
        "PRAGMA temp_store = MEMORY",   "PRAGMA mmap_size = 134217728",
        "PRAGMA busy_timeout = 3000",   "PRAGMA journal_size_limit = 67108864",
    };

    auto run = [&conn](const char* sql) {
        QSqlQuery q(conn);
        if (!q.exec(QLatin1String(sql))) {
            LOG_WARN(kCacheDbTag, QString("PRAGMA failed: %1 — %2").arg(sql, q.lastError().text()));
        }
    };

    if (include_database_wide) {
        for (auto* p : kDatabaseWide)
            run(p);
    }
    for (auto* p : kPerConnection)
        run(p);

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

    // Phase 4b: add instance_uuid column to screen_state for the panel-UUID
    // refactor. Pre-Phase-4 rows are keyed only by screen_key (the screen
    // type id, e.g. "watchlist"). New rows get an instance_uuid (per-panel
    // stable id) so two open watchlists never collide on save.
    //
    // Idempotent: PRAGMA table_info() lists existing columns; ALTER TABLE
    // ADD COLUMN runs only if instance_uuid is missing. Safe to call on
    // every CacheDatabase::open across upgrades.
    {
        QSqlDatabase conn = connection();
        QSqlQuery info(conn);
        if (info.exec("PRAGMA table_info(screen_state)")) {
            bool has_instance_uuid = false;
            while (info.next()) {
                if (info.value(1).toString() == "instance_uuid") {
                    has_instance_uuid = true;
                    break;
                }
            }
            if (!has_instance_uuid) {
                QSqlQuery alter(conn);
                if (alter.exec("ALTER TABLE screen_state ADD COLUMN instance_uuid TEXT")) {
                    LOG_INFO(kCacheDbTag, "screen_state migrated: added instance_uuid column");
                } else {
                    LOG_WARN(kCacheDbTag,
                             QString("Failed to add instance_uuid column: %1").arg(alter.lastError().text()));
                }
            }
        }
    }
    // Index on instance_uuid for fast UUID-keyed lookups. NOT UNIQUE — old
    // rows have NULL instance_uuid and SQLite treats multiple NULLs as
    // distinct in a UNIQUE index, but we'd rather not rely on that subtlety.
    r = exec("CREATE INDEX IF NOT EXISTS idx_screen_state_instance_uuid "
             "ON screen_state(instance_uuid) WHERE instance_uuid IS NOT NULL");
    if (r.is_err())
        return r;

    LOG_INFO(kCacheDbTag, "Cache tables initialized");
    return Result<void>::ok();
}

} // namespace fincept
