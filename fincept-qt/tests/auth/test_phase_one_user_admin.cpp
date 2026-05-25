#include "app/PhaseOneMigrationRegistry.h"
#include "multiuser/contracts/PhaseOneAuditTypes.h"
#include "multiuser/server/PhaseOneUserAdminServer.h"
#include "multiuser/server/storage/PhaseOneAuditRepository.h"
#include "multiuser/server/storage/PhaseOneUserRepository.h"
#include "storage/sqlite/Database.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QThread>
#include <QtTest/QtTest>

using namespace fincept::multiuser;

namespace {

class PhaseOneServerDbHarness {
  public:
    PhaseOneServerDbHarness() {
        fincept::PhaseOneMigrationRegistry::register_all();
        reset_database();
    }

    ~PhaseOneServerDbHarness() {
        fincept::Database::instance().close();
    }

    void reset_database() {
        fincept::Database::instance().close();
        const QString db_path = dir_.filePath(QStringLiteral("phase_one_user_admin.sqlite"));
        QFile::remove(db_path);
        const auto result = fincept::Database::instance().open(db_path);
        QVERIFY2(result.is_ok(), qPrintable(QString::fromStdString(result.error())));
        clear_tables();
    }

    void clear_tables() {
        QSqlQuery query(fincept::Database::instance().raw_db());
        QVERIFY(query.exec("DELETE FROM sessions"));
        QVERIFY(query.exec("DELETE FROM audit_events"));
        QVERIFY(query.exec("DELETE FROM users"));
    }

    PhaseOneUserRepository user_repository;
    PhaseOneAuditRepository audit_repository;
    PhaseOneUserAdminServer user_admin_server{&user_repository, &audit_repository};

  private:
    QTemporaryDir dir_;
};

PhaseOneServerDbHarness& harness() {
    static PhaseOneServerDbHarness value;
    return value;
}

QVector<PhaseOneAuditEvent> all_audit_events() {
    const auto result = harness().audit_repository.list_events({});
    if (result.is_err())
        return {};
    return result.value();
}

int active_admin_count() {
    const auto users = harness().user_repository.list_users();
    if (users.is_err())
        return -1;

    int count = 0;
    for (const auto& user : users.value()) {
        if (user.role == QStringLiteral("admin") && user.status == QStringLiteral("active"))
            ++count;
    }
    return count;
}

class PhaseOneUserAdminTest : public QObject {
    Q_OBJECT

  private slots:
    void init();
    void bootstrap_is_open_only_until_first_admin_is_created();
    void bootstrap_returns_structured_error_when_user_count_lookup_fails();
    void bootstrap_status_fails_closed_when_user_count_lookup_fails();
    void set_initial_password_stores_secure_hash_and_not_plaintext();
    void set_initial_password_is_one_time_only();
    void create_user_enforces_active_user_cap_and_disabled_users_do_not_count();
    void blank_usernames_are_rejected();
    void sole_admin_cannot_be_disabled_and_transfer_preserves_exactly_one_active_admin();
    void transfer_admin_reports_missing_target_as_user_not_found();
};

void PhaseOneUserAdminTest::init() {
    harness().reset_database();
}

void PhaseOneUserAdminTest::bootstrap_is_open_only_until_first_admin_is_created() {
    const auto before = harness().user_admin_server.bootstrap_status();
    QVERIFY(before.is_ok());
    QVERIFY(before.value().bootstrap_open);

    const auto bootstrap = harness().user_admin_server.bootstrap(QStringLiteral("Admin"), QStringLiteral("secret"));
    QVERIFY2(bootstrap.is_ok(), bootstrap.is_ok() ? "" : bootstrap.error().c_str());

    const auto after = harness().user_admin_server.bootstrap_status();
    QVERIFY(after.is_ok());
    QVERIFY(!after.value().bootstrap_open);

    const auto admin = harness().user_repository.find_by_username(QStringLiteral("admin"));
    QVERIFY(admin.has_value());
    QCOMPARE(admin->username, QStringLiteral("admin"));
    QCOMPARE(admin->role, QStringLiteral("admin"));
    QCOMPARE(admin->status, QStringLiteral("active"));
    QVERIFY(!admin->password_hash.isEmpty());
    QVERIFY(admin->password_hash != QStringLiteral("secret"));
    QCOMPARE(active_admin_count(), 1);

    const auto second_bootstrap = harness().user_admin_server.bootstrap(QStringLiteral("other"), QStringLiteral("secret"));
    QVERIFY(second_bootstrap.is_err());
}

void PhaseOneUserAdminTest::bootstrap_returns_structured_error_when_user_count_lookup_fails() {
    fincept::Database::instance().close();

    const auto bootstrap = harness().user_admin_server.bootstrap(QStringLiteral("admin"), QStringLiteral("secret"));

    QVERIFY(bootstrap.is_err());
}

void PhaseOneUserAdminTest::bootstrap_status_fails_closed_when_user_count_lookup_fails() {
    fincept::Database::instance().close();

    const auto status = harness().user_admin_server.bootstrap_status();

    QVERIFY(status.is_ok());
    QVERIFY(!status.value().bootstrap_open);
}

void PhaseOneUserAdminTest::set_initial_password_stores_secure_hash_and_not_plaintext() {
    QVERIFY(harness().user_admin_server.bootstrap(QStringLiteral("admin"), QStringLiteral("secret")).is_ok());
    const auto create = harness().user_admin_server.create_user(QStringLiteral("analyst"));
    QVERIFY(create.is_ok());

    const auto analyst = harness().user_repository.find_by_username(QStringLiteral("analyst"));
    QVERIFY(analyst.has_value());
    QVERIFY(analyst->password_hash.isEmpty());

    const auto set_password = harness().user_admin_server.set_initial_password(analyst->user_id, QStringLiteral("launch-123"));
    QVERIFY(set_password.is_ok());

    const auto updated = harness().user_repository.find_by_id(analyst->user_id);
    QVERIFY(updated.has_value());
    QVERIFY(!updated->password_hash.isEmpty());
    QVERIFY(updated->password_hash != QStringLiteral("launch-123"));
    QVERIFY(updated->password_hash.startsWith(QStringLiteral("pbkdf2_sha256$200000$")));
}

void PhaseOneUserAdminTest::set_initial_password_is_one_time_only() {
    QVERIFY(harness().user_admin_server.bootstrap(QStringLiteral("admin"), QStringLiteral("secret")).is_ok());
    QVERIFY(harness().user_admin_server.create_user(QStringLiteral("analyst")).is_ok());

    const auto analyst = harness().user_repository.find_by_username(QStringLiteral("analyst"));
    QVERIFY(analyst.has_value());
    QVERIFY(harness().user_admin_server.set_initial_password(analyst->user_id, QStringLiteral("launch-123")).is_ok());

    const auto second_set = harness().user_admin_server.set_initial_password(analyst->user_id, QStringLiteral("reset-456"));
    QVERIFY(second_set.is_err());
    QCOMPARE(QString::fromStdString(second_set.error()), QStringLiteral("password_already_initialized"));
}

void PhaseOneUserAdminTest::create_user_enforces_active_user_cap_and_disabled_users_do_not_count() {
    QVERIFY(harness().user_admin_server.bootstrap(QStringLiteral("admin"), QStringLiteral("secret")).is_ok());
    QVERIFY(harness().user_admin_server.create_user(QStringLiteral("user-one")).is_ok());
    QVERIFY(harness().user_admin_server.create_user(QStringLiteral("user-two")).is_ok());

    const auto capped = harness().user_admin_server.create_user(QStringLiteral("user-three"));
    QVERIFY(capped.is_err());

    const auto user_one = harness().user_repository.find_by_username(QStringLiteral("user-one"));
    QVERIFY(user_one.has_value());
    QVERIFY(harness().user_admin_server.disable_user(user_one->user_id).is_ok());
    QVERIFY(harness().user_admin_server.create_user(QStringLiteral("user-three")).is_ok());

    const auto listed = harness().user_admin_server.list_users();
    QVERIFY(listed.is_ok());
    QCOMPARE(listed.value().users.size(), 4);
}

void PhaseOneUserAdminTest::blank_usernames_are_rejected() {
    const auto bootstrap = harness().user_admin_server.bootstrap(QStringLiteral("   "), QStringLiteral("secret"));
    QVERIFY(bootstrap.is_err());
    QCOMPARE(QString::fromStdString(bootstrap.error()), QStringLiteral("invalid_username"));

    QVERIFY(harness().user_admin_server.bootstrap(QStringLiteral("admin"), QStringLiteral("secret")).is_ok());
    const auto create = harness().user_admin_server.create_user(QStringLiteral("   "));
    QVERIFY(create.is_err());
    QCOMPARE(QString::fromStdString(create.error()), QStringLiteral("invalid_username"));
}

void PhaseOneUserAdminTest::sole_admin_cannot_be_disabled_and_transfer_preserves_exactly_one_active_admin() {
    QVERIFY(harness().user_admin_server.bootstrap(QStringLiteral("admin"), QStringLiteral("secret")).is_ok());
    QVERIFY(harness().user_admin_server.create_user(QStringLiteral("operator")).is_ok());

    const auto admin = harness().user_repository.find_by_username(QStringLiteral("admin"));
    const auto operator_user = harness().user_repository.find_by_username(QStringLiteral("operator"));
    QVERIFY(admin.has_value());
    QVERIFY(operator_user.has_value());

    const auto disable_admin = harness().user_admin_server.disable_user(admin->user_id);
    QVERIFY(disable_admin.is_err());

    const auto transfer = harness().user_admin_server.transfer_admin(operator_user->user_id);
    QVERIFY(transfer.is_ok());
    QCOMPARE(active_admin_count(), 1);

    const auto updated_admin = harness().user_repository.find_by_id(admin->user_id);
    const auto updated_operator = harness().user_repository.find_by_id(operator_user->user_id);
    QVERIFY(updated_admin.has_value());
    QVERIFY(updated_operator.has_value());
    QCOMPARE(updated_admin->role, QStringLiteral("standard"));
    QCOMPARE(updated_operator->role, QStringLiteral("admin"));

    const auto disable_new_admin = harness().user_admin_server.disable_user(updated_operator->user_id);
    QVERIFY(disable_new_admin.is_err());

    const auto audit = all_audit_events();
    QVERIFY(audit.size() >= 3);
}

void PhaseOneUserAdminTest::transfer_admin_reports_missing_target_as_user_not_found() {
    QVERIFY(harness().user_admin_server.bootstrap(QStringLiteral("admin"), QStringLiteral("secret")).is_ok());

    const auto transfer = harness().user_admin_server.transfer_admin(9999);

    QVERIFY(transfer.is_err());
    QCOMPARE(QString::fromStdString(transfer.error()), QStringLiteral("user_not_found"));
}

} // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    PhaseOneUserAdminTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_phase_one_user_admin.moc"
