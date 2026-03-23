#include "storage/sqlite/Database.h"

#include "core/logging/Logger.h"
#include "storage/sqlite/migrations/MigrationRunner.h"

namespace fincept {

Database& Database::instance() {
    static Database s;
    return s;
}

Result<void> Database::open(const QString& path) {
    db_ = QSqlDatabase::addDatabase("QSQLITE", "fincept_main");
    db_.setDatabaseName(path);
    if (!db_.open()) {
        return Result<void>::err(db_.lastError().text().toStdString());
    }
    LOG_INFO("DB", "Opened database: " + path);

    auto pr = apply_pragmas();
    if (pr.is_err())
        return pr;

    // Run versioned migrations
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

Result<QSqlQuery> Database::execute(const QString& sql, const QVariantList& params) {
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

Result<void> Database::exec(const QString& sql) {
    QSqlQuery query(db_);
    if (!query.exec(sql)) {
        return Result<void>::err(query.lastError().text().toStdString());
    }
    return Result<void>::ok();
}

Result<void> Database::begin_transaction() {
    QSqlQuery q(db_);
    if (!q.exec("BEGIN IMMEDIATE")) {
        return Result<void>::err(q.lastError().text().toStdString());
    }
    return Result<void>::ok();
}

Result<void> Database::commit() {
    QSqlQuery q(db_);
    if (!q.exec("COMMIT")) {
        return Result<void>::err(q.lastError().text().toStdString());
    }
    return Result<void>::ok();
}

Result<void> Database::rollback() {
    QSqlQuery q(db_);
    if (!q.exec("ROLLBACK")) {
        return Result<void>::err(q.lastError().text().toStdString());
    }
    return Result<void>::ok();
}

Result<void> Database::apply_pragmas() {
    const char* pragmas[] = {
        "PRAGMA journal_mode = WAL",  "PRAGMA synchronous = NORMAL",  "PRAGMA cache_size = -20000",
        "PRAGMA temp_store = MEMORY", "PRAGMA mmap_size = 268435456", "PRAGMA foreign_keys = ON",
        "PRAGMA busy_timeout = 5000",
    };
    for (auto* p : pragmas) {
        auto r = exec(p);
        if (r.is_err()) {
            LOG_WARN("DB", QString("PRAGMA failed: %1 — %2").arg(p, QString::fromStdString(r.error())));
        }
    }
    LOG_INFO("DB", "PRAGMAs applied (WAL, foreign_keys, cache_size=20MB)");
    return Result<void>::ok();
}

} // namespace fincept
