#include "app/PhaseOneMigrationRegistry.h"
#include "storage/sqlite/migrations/MigrationRunner.h"

#include <QCoreApplication>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <QtTest/QtTest>

namespace {

QString unique_connection_name() {
    return QStringLiteral("phase_one_migration_test_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
}

bool sqlite_object_exists(QSqlDatabase& db, const QString& type, const QString& name) {
    QSqlQuery query(db);
    query.prepare("SELECT COUNT(*) FROM sqlite_master WHERE type = ? AND name = ?");
    query.addBindValue(type);
    query.addBindValue(name);
    if (!query.exec())
        return false;
    if (!query.next())
        return false;
    return query.value(0).toInt() > 0;
}

int count_migration_version(int version) {
    int count = 0;
    for (const auto& migration : fincept::MigrationRunner::all_migrations()) {
        if (migration.version == version)
            ++count;
    }
    return count;
}

class ScopedSqliteDb {
  public:
    ScopedSqliteDb() : connection_name_(unique_connection_name()) {
        db_ = QSqlDatabase::addDatabase("QSQLITE", connection_name_);
        db_.setDatabaseName(":memory:");
        opened_ = db_.open();
        error_text_ = db_.lastError().text();
    }

    ~ScopedSqliteDb() {
        db_.close();
        db_ = {};
        QSqlDatabase::removeDatabase(connection_name_);
    }

    QSqlDatabase& db() {
        return db_;
    }

    bool is_open() const {
        return opened_;
    }

    const QString& error_text() const {
        return error_text_;
    }

  private:
    QString connection_name_;
    QSqlDatabase db_;
    bool opened_ = false;
    QString error_text_;
};

class PhaseOneMigrationsTest : public QObject {
    Q_OBJECT

  private slots:
    void register_v032_is_directly_exposed_and_idempotent() {
        fincept::register_migration_v032();
        fincept::register_migration_v032();

        QCOMPARE(count_migration_version(32), 1);
    }

    void phase_one_registry_registers_full_migration_range_once() {
        fincept::PhaseOneMigrationRegistry::register_all();
        fincept::PhaseOneMigrationRegistry::register_all();

        const auto& migrations = fincept::MigrationRunner::all_migrations();
        QCOMPARE(migrations.size(), 32);
        for (int version = 1; version <= 32; ++version)
            QCOMPARE(count_migration_version(version), 1);
    }

    void migration_runner_creates_phase_one_tables_and_indexes() {
        fincept::PhaseOneMigrationRegistry::register_all();
        ScopedSqliteDb db;
        QVERIFY2(db.is_open(), qPrintable(db.error_text()));

        fincept::MigrationRunner runner(db.db());
        QVERIFY2(runner.run().is_ok(), "expected migration runner to succeed");
        QCOMPARE(runner.current_version(), 32);

        QVERIFY(sqlite_object_exists(db.db(), "table", "users"));
        QVERIFY(sqlite_object_exists(db.db(), "table", "sessions"));
        QVERIFY(sqlite_object_exists(db.db(), "table", "audit_events"));
        QVERIFY(sqlite_object_exists(db.db(), "index", "idx_users_username"));
        QVERIFY(sqlite_object_exists(db.db(), "index", "idx_users_single_active_admin"));
        QVERIFY(sqlite_object_exists(db.db(), "index", "idx_sessions_session_id"));
        QVERIFY(sqlite_object_exists(db.db(), "index", "idx_sessions_active_user"));

        QSqlQuery query(db.db());
        QVERIFY(query.exec("INSERT INTO users (username, password_hash, role, status) VALUES ('admin', 'hash', 'admin', 'active')"));
        QVERIFY(!query.exec("INSERT INTO users (username, password_hash, role, status) VALUES ('second-admin', 'hash', 'admin', 'active')"));
        QVERIFY(query.exec("INSERT INTO users (username, password_hash, role, status) VALUES ('disabled-admin', 'hash', 'admin', 'disabled')"));
        QVERIFY(query.exec("INSERT INTO users (username, password_hash, role, status) VALUES ('standard-user', 'hash', 'standard', 'active')"));
        QVERIFY(query.exec("INSERT INTO sessions (session_id, user_id, expires_at, invalidated) VALUES ('session-1', 4, datetime('now', '+1 day'), 0)"));
        QVERIFY(!query.exec("INSERT INTO sessions (session_id, user_id, expires_at, invalidated) VALUES ('session-2', 4, datetime('now', '+1 day'), 0)"));
        QVERIFY(query.exec("INSERT INTO sessions (session_id, user_id, expires_at, invalidated) VALUES ('session-3', 4, datetime('now', '+1 day'), 1)"));
    }

    void migration_runner_fails_loudly_on_incompatible_existing_schema() {
        fincept::PhaseOneMigrationRegistry::register_all();
        ScopedSqliteDb db;
        QVERIFY2(db.is_open(), qPrintable(db.error_text()));

        QSqlQuery query(db.db());
        QVERIFY(query.exec("CREATE TABLE schema_version (version INTEGER PRIMARY KEY, name TEXT NOT NULL, applied_at TEXT DEFAULT (datetime('now')))"));
        QVERIFY(query.exec("INSERT INTO schema_version (version, name) VALUES (31, 'rss_feeds')"));
        QVERIFY(query.exec("CREATE TABLE users (id INTEGER PRIMARY KEY AUTOINCREMENT)"));

        fincept::MigrationRunner runner(db.db());
        QVERIFY(runner.run().is_err());
    }
};

} // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    PhaseOneMigrationsTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_phase_one_migrations.moc"
