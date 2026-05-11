#include "storage/sqlite/Database.h"

#include "core/logging/Logger.h"
#include "storage/sqlite/migrations/MigrationRunner.h"

#include <QSqlError>
#include <QThread>

namespace fincept {

namespace {
constexpr const char* kMainConnectionName = "fincept_main";
constexpr const char* kDbTag = "DB";

QString thread_label(QThread* t) {
    if (!t)
        return QStringLiteral("?");
    const QString name = t->objectName();
    return name.isEmpty()
               ? QStringLiteral("0x%1").arg(reinterpret_cast<quintptr>(t), 0, 16)
               : name;
}
} // namespace

Database& Database::instance() {
    static Database s;
    return s;
}

Result<void> Database::open(const QString& path) {
    main_thread_ = QThread::currentThread();
    db_path_ = path;

    db_ = QSqlDatabase::addDatabase("QSQLITE", kMainConnectionName);
    db_.setDatabaseName(path);
    if (!db_.open()) {
        return Result<void>::err(db_.lastError().text().toStdString());
    }
    LOG_INFO(kDbTag, "Opened database: " + path);

    auto pr = apply_pragmas(db_, /*include_database_wide=*/true);
    if (pr.is_err())
        return pr;

    // Run versioned migrations on the main connection.
    MigrationRunner runner(db_);
    return runner.run();
}

void Database::close() {
    if (db_.isOpen())
        db_.close();
}

bool Database::is_open() const {
    return db_.isOpen();
}

QSqlDatabase Database::connection() {
    QThread* t = QThread::currentThread();

    // Main-thread fast path — the connection opened by `open()`.
    if (t == main_thread_)
        return db_;

    // Per-thread cloned connection. The thread_local Guard owns the
    // connection-name lifetime so the registry entry is removed when the
    // thread exits. This handles QtConcurrent thread-pool workers correctly:
    // they keep their connection across tasks and clean up at pool shutdown.
    struct Guard {
        QString name;
        ~Guard() {
            if (!name.isEmpty()) {
                // Remove from Qt's connection registry so file descriptors
                // and the QSqlDriver are released. Logs an inert warning if
                // any QSqlDatabase handle is still alive — harmless.
                QSqlDatabase::removeDatabase(name);
            }
        }
    };
    thread_local Guard guard;

    if (guard.name.isEmpty()) {
        if (db_path_.isEmpty()) {
            LOG_ERROR(kDbTag, "connection() called before open() — refusing to clone");
            return {};
        }

        // Unique connection name per thread. Encode pointer + objectName so
        // log messages are debuggable.
        guard.name = QStringLiteral("fincept_thread_%1_%2")
                         .arg(thread_label(t))
                         .arg(reinterpret_cast<quintptr>(t), 0, 16);

        QSqlDatabase clone = QSqlDatabase::cloneDatabase(db_, guard.name);
        if (!clone.open()) {
            const QString err = clone.lastError().text();
            LOG_ERROR(kDbTag, QString("Failed to open per-thread connection on [%1]: %2")
                                .arg(thread_label(t), err));
            // Clean up the registry entry we created via cloneDatabase.
            QSqlDatabase::removeDatabase(guard.name);
            guard.name.clear();
            return {};
        }
        // Database-wide PRAGMAs (journal_mode) are already set by the main
        // connection — only re-apply per-connection ones.
        apply_pragmas(clone, /*include_database_wide=*/false);
        const int n = ++per_thread_connections_;
        LOG_INFO(kDbTag, QString("Opened per-thread connection on [%1] (total=%2)")
                           .arg(thread_label(t))
                           .arg(n));
    }
    return QSqlDatabase::database(guard.name, /*open=*/false);
}

Result<QSqlQuery> Database::execute(const QString& sql, const QVariantList& params) {
    QSqlDatabase conn = connection();
    if (!conn.isOpen()) {
        return Result<QSqlQuery>::err("DB connection unavailable on this thread");
    }
    QSqlQuery query(conn);
    query.prepare(sql);
    for (int i = 0; i < params.size(); ++i) {
        query.bindValue(i, params[i]);
    }
    if (!query.exec()) {
        return Result<QSqlQuery>::err(query.lastError().text().toStdString());
    }
    return Result<QSqlQuery>::ok(std::move(query));
}

Result<void> Database::exec(const QString& sql) {
    QSqlDatabase conn = connection();
    if (!conn.isOpen()) {
        return Result<void>::err("DB connection unavailable on this thread");
    }
    QSqlQuery query(conn);
    if (!query.exec(sql)) {
        return Result<void>::err(query.lastError().text().toStdString());
    }
    return Result<void>::ok();
}

Result<void> Database::begin_transaction() {
    QSqlDatabase conn = connection();
    if (!conn.isOpen())
        return Result<void>::err("DB connection unavailable on this thread");
    QSqlQuery q(conn);
    if (!q.exec("BEGIN IMMEDIATE")) {
        return Result<void>::err(q.lastError().text().toStdString());
    }
    return Result<void>::ok();
}

Result<void> Database::commit() {
    QSqlDatabase conn = connection();
    if (!conn.isOpen())
        return Result<void>::err("DB connection unavailable on this thread");
    QSqlQuery q(conn);
    if (!q.exec("COMMIT")) {
        return Result<void>::err(q.lastError().text().toStdString());
    }
    return Result<void>::ok();
}

Result<void> Database::rollback() {
    QSqlDatabase conn = connection();
    if (!conn.isOpen())
        return Result<void>::err("DB connection unavailable on this thread");
    QSqlQuery q(conn);
    if (!q.exec("ROLLBACK")) {
        return Result<void>::err(q.lastError().text().toStdString());
    }
    return Result<void>::ok();
}

Result<void> Database::apply_pragmas(QSqlDatabase& conn, bool include_database_wide) {
    // Database-wide PRAGMAs only need to be set once (any connection sets them
    // for the file). Per-connection PRAGMAs (foreign_keys, busy_timeout, …)
    // have to be re-applied for every cloned connection or cascade deletes
    // and contention behaviour will silently regress.
    static const char* kDatabaseWide[] = {
        "PRAGMA journal_mode = WAL",
    };
    static const char* kPerConnection[] = {
        "PRAGMA synchronous = NORMAL",
        "PRAGMA cache_size = -20000",
        "PRAGMA temp_store = MEMORY",
        "PRAGMA mmap_size = 268435456",
        "PRAGMA foreign_keys = ON",
        "PRAGMA busy_timeout = 5000",
    };

    auto run = [&conn](const char* sql) {
        QSqlQuery q(conn);
        if (!q.exec(QLatin1String(sql))) {
            LOG_WARN(kDbTag, QString("PRAGMA failed: %1 — %2").arg(sql, q.lastError().text()));
        }
    };

    if (include_database_wide) {
        for (auto* p : kDatabaseWide)
            run(p);
    }
    for (auto* p : kPerConnection)
        run(p);

    if (include_database_wide)
        LOG_INFO(kDbTag, "PRAGMAs applied (WAL, foreign_keys, cache_size=20MB)");
    return Result<void>::ok();
}

} // namespace fincept
