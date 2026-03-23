#include "storage/sqlite/migrations/MigrationRunner.h"

#include "core/logging/Logger.h"

#include <algorithm>

namespace fincept {

// ── Static migration registry ────────────────────────────────────────────────

static QVector<Migration>& migration_registry() {
    static QVector<Migration> registry;
    return registry;
}

void MigrationRunner::register_migration(Migration m) {
    auto& reg = migration_registry();
    reg.append(std::move(m));
    // Keep sorted by version
    std::sort(reg.begin(), reg.end(), [](const Migration& a, const Migration& b) { return a.version < b.version; });
}

const QVector<Migration>& MigrationRunner::all_migrations() {
    return migration_registry();
}

// ── Instance methods ─────────────────────────────────────────────────────────

MigrationRunner::MigrationRunner(QSqlDatabase& db) : db_(db) {}

Result<void> MigrationRunner::run() {
    auto r = ensure_schema_version_table();
    if (r.is_err())
        return r;

    int current = read_current_version();
    const auto& migrations = all_migrations();

    for (const auto& m : migrations) {
        if (m.version <= current)
            continue;

        LOG_INFO("DB", QString("Applying migration v%1: %2").arg(m.version).arg(m.name));

        // Begin transaction for this migration
        QSqlQuery q(db_);
        if (!q.exec("BEGIN IMMEDIATE")) {
            return Result<void>::err("Failed to begin transaction for migration v" + std::to_string(m.version) + ": " +
                                     q.lastError().text().toStdString());
        }

        // Apply
        auto ar = apply_migration(m);
        if (ar.is_err()) {
            q.exec("ROLLBACK");
            LOG_ERROR("DB", QString("Migration v%1 failed: %2").arg(m.version).arg(QString::fromStdString(ar.error())));
            return ar;
        }

        // Record version
        auto vr = record_version(m.version, m.name);
        if (vr.is_err()) {
            q.exec("ROLLBACK");
            return vr;
        }

        // Commit
        if (!q.exec("COMMIT")) {
            return Result<void>::err("Failed to commit migration v" + std::to_string(m.version) + ": " +
                                     q.lastError().text().toStdString());
        }

        LOG_INFO("DB", QString("Migration v%1 applied successfully").arg(m.version));
    }

    if (!migrations.isEmpty()) {
        LOG_INFO("DB", QString("Schema at version %1").arg(read_current_version()));
    }
    return Result<void>::ok();
}

int MigrationRunner::current_version() const {
    // const_cast because read uses QSqlQuery which needs non-const db
    return const_cast<MigrationRunner*>(this)->read_current_version();
}

// ── Private helpers ──────────────────────────────────────────────────────────

Result<void> MigrationRunner::ensure_schema_version_table() {
    QSqlQuery q(db_);
    if (!q.exec("CREATE TABLE IF NOT EXISTS schema_version ("
                "  version INTEGER PRIMARY KEY,"
                "  name TEXT NOT NULL,"
                "  applied_at TEXT DEFAULT (datetime('now'))"
                ")")) {
        return Result<void>::err("Failed to create schema_version table: " + q.lastError().text().toStdString());
    }
    return Result<void>::ok();
}

Result<void> MigrationRunner::apply_migration(const Migration& m) {
    return m.apply(db_);
}

Result<void> MigrationRunner::record_version(int version, const QString& name) {
    QSqlQuery q(db_);
    q.prepare("INSERT INTO schema_version (version, name) VALUES (?, ?)");
    q.bindValue(0, version);
    q.bindValue(1, name);
    if (!q.exec()) {
        return Result<void>::err("Failed to record migration version: " + q.lastError().text().toStdString());
    }
    return Result<void>::ok();
}

int MigrationRunner::read_current_version() {
    QSqlQuery q(db_);
    if (!q.exec("SELECT MAX(version) FROM schema_version")) {
        return 0;
    }
    if (!q.next())
        return 0;
    return q.value(0).toInt(); // returns 0 if NULL
}

} // namespace fincept
