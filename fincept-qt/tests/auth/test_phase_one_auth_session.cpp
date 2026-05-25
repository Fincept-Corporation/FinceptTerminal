#include "app/PhaseOneMigrationRegistry.h"
#include "auth/AuthTypes.h"
#include "auth/PhaseOneSessionAuthBridge.h"
#include "multiuser/server/PhaseOneAuthServer.h"
#include "multiuser/server/PhaseOneUserAdminServer.h"
#include "multiuser/server/http/PhaseOneAuthHttpRoutes.h"
#include "multiuser/server/http/PhaseOneHttpServer.h"
#include "multiuser/server/storage/PhaseOneAuditRepository.h"
#include "multiuser/server/storage/PhaseOneSessionRepository.h"
#include "multiuser/server/storage/PhaseOneUserRepository.h"
#include "storage/sqlite/Database.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QJsonDocument>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include <algorithm>

using namespace fincept::auth;
using namespace fincept::multiuser;

namespace {

class PhaseOneSessionHarness {
  public:
    PhaseOneSessionHarness() : auth_server(&user_repository, &session_repository, &audit_repository),
                               user_admin_server(&user_repository, &audit_repository) {
        fincept::PhaseOneMigrationRegistry::register_all();
        reset_database();
        register_phase_one_auth_routes(http_server, auth_server);
    }

    ~PhaseOneSessionHarness() {
        fincept::Database::instance().close();
    }

    void reset_database() {
        fincept::Database::instance().close();
        const QString db_path = dir_.filePath(QStringLiteral("phase_one_session.sqlite"));
        QFile::remove(db_path);
        const auto result = fincept::Database::instance().open(db_path);
        QVERIFY2(result.is_ok(), qPrintable(QString::fromStdString(result.error())));
    }

    QString bootstrap_and_login_admin() {
        if (user_admin_server.bootstrap(QStringLiteral("admin"), QStringLiteral("secret")).is_err()) {
            qFatal("bootstrap failed");
            return {};
        }
        const auto login = auth_server.login(QStringLiteral("admin"), QStringLiteral("secret"));
        if (login.is_err()) {
            qFatal("login failed");
            return {};
        }
        return login.value().session_id;
    }

    PhaseOneUserRepository user_repository;
    PhaseOneSessionRepository session_repository;
    PhaseOneAuditRepository audit_repository;
    PhaseOneAuthServer auth_server;
    PhaseOneUserAdminServer user_admin_server;
    PhaseOneHttpServer http_server;

  private:
    QTemporaryDir dir_;
};

PhaseOneSessionHarness& harness() {
    static PhaseOneSessionHarness value;
    return value;
}

class PhaseOneAuthSessionTest : public QObject {
    Q_OBJECT

  private slots:
    void init();
    void session_data_round_trips_phase_one_fields();
    void old_persisted_session_payload_parses_safely();
    void phase_one_session_exposes_compatibility_credentials();
    void legacy_startup_auth_state_is_never_restored();
    void user_without_initial_password_receives_setup_required();
    void second_login_replaces_prior_session();
    void login_populates_expiry_and_current_session_refreshes_activity();
    void expired_session_is_invalidated_on_access();
    void disabled_user_login_is_denied_and_active_session_is_invalidated();
    void logout_invalidates_current_session();
    void auth_events_record_login_failed_login_and_logout();
};

void PhaseOneAuthSessionTest::init() {
    harness().reset_database();
}

void PhaseOneAuthSessionTest::session_data_round_trips_phase_one_fields() {
    SessionData session;
    session.authenticated = true;
    session.api_key = "";
    session.session_token = "compat-session-token";
    session.user_id = 12;
    session.username = "alice";
    session.role = "admin";
    session.session_id = "sess-12";
    session.expires_at = "2026-05-30T12:00:00Z";
    session.user_info.username = "alice";

    const SessionData parsed = SessionData::from_json(session.to_json());
    QVERIFY(parsed.authenticated);
    QCOMPARE(parsed.user_id, 12);
    QCOMPARE(parsed.username, QString("alice"));
    QCOMPARE(parsed.role, QString("admin"));
    QCOMPARE(parsed.session_id, QString("sess-12"));
    QCOMPARE(parsed.expires_at, QString("2026-05-30T12:00:00Z"));
    QCOMPARE(parsed.session_token, QString("compat-session-token"));
}

void PhaseOneAuthSessionTest::old_persisted_session_payload_parses_safely() {
    const QJsonObject legacy{{"authenticated", true},
                             {"api_key", "legacy-key"},
                             {"session_token", "legacy-session"},
                             {"device_id", "legacy-device"},
                             {"has_subscription", false},
                             {"user_info", QJsonObject{{"username", "legacy-user"}, {"account_type", "free"}}}};

    const SessionData parsed = SessionData::from_json(legacy);
    QVERIFY(parsed.authenticated);
    QCOMPARE(parsed.api_key, QString("legacy-key"));
    QCOMPARE(parsed.session_token, QString("legacy-session"));
    QCOMPARE(parsed.session_id, QString("legacy-session"));
    QCOMPARE(parsed.username, QString("legacy-user"));
    QCOMPARE(parsed.user_info.username, QString("legacy-user"));
}

void PhaseOneAuthSessionTest::phase_one_session_exposes_compatibility_credentials() {
    SessionData phase_one;
    phase_one.authenticated = true;
    phase_one.session_id = "phase-one-session";
    phase_one.session_token = "phase-one-session";

    QVERIFY(!phase_one.has_hosted_api_key());
    QVERIFY(phase_one.is_phase_one_session());
    QCOMPARE(phase_one.compatibility_api_key(), QString("phase-one-session"));
    QCOMPARE(phase_one.compatibility_session_token(), QString("phase-one-session"));

    SessionData hosted;
    hosted.authenticated = true;
    hosted.api_key = "hosted-key";
    hosted.session_token = "hosted-session";

    QVERIFY(hosted.has_hosted_api_key());
    QVERIFY(!hosted.is_phase_one_session());
    QCOMPARE(hosted.compatibility_api_key(), QString("hosted-key"));
    QCOMPARE(hosted.compatibility_session_token(), QString("hosted-session"));
}

void PhaseOneAuthSessionTest::legacy_startup_auth_state_is_never_restored() {
    const QString legacy_session = QString::fromUtf8(
        QJsonDocument(QJsonObject{{"authenticated", true},
                                  {"api_key", "legacy-key"},
                                  {"session_token", "legacy-session"},
                                  {"user_info", QJsonObject{{"username", "legacy-user"}}}})
            .toJson(QJsonDocument::Compact));

    QVERIFY(!PhaseOneSessionAuthBridge::should_restore_startup_auth(
        legacy_session, QStringLiteral("legacy-settings-key"), QStringLiteral("legacy-secure-key")));
}

void PhaseOneAuthSessionTest::user_without_initial_password_receives_setup_required() {
    QVERIFY(harness().user_repository.create_user(QStringLiteral("alice")).is_ok());

    const auto login = harness().auth_server.login(QStringLiteral("alice"), QStringLiteral("secret"));
    QVERIFY(login.is_err());
    QCOMPARE(QString::fromStdString(login.error()), QStringLiteral("setup_required"));
}

void PhaseOneAuthSessionTest::second_login_replaces_prior_session() {
    QVERIFY(harness().user_admin_server.bootstrap(QStringLiteral("admin"), QStringLiteral("secret")).is_ok());

    const auto first_login = harness().auth_server.login(QStringLiteral("admin"), QStringLiteral("secret"));
    QVERIFY(first_login.is_ok());
    const auto second_login = harness().auth_server.login(QStringLiteral("admin"), QStringLiteral("secret"));
    QVERIFY(second_login.is_ok());

    QVERIFY(first_login.value().session_id != second_login.value().session_id);
    QVERIFY(!harness().session_repository.find_active_session(first_login.value().session_id).has_value());
    QVERIFY(harness().session_repository.find_session(first_login.value().session_id)->invalidated);
    QVERIFY(harness().session_repository.find_active_session(second_login.value().session_id).has_value());

}

void PhaseOneAuthSessionTest::login_populates_expiry_and_current_session_refreshes_activity() {
    const QString session_id = harness().bootstrap_and_login_admin();
    QVERIFY(!session_id.isEmpty());

    const auto stored = harness().session_repository.find_active_session(session_id);
    QVERIFY(stored.has_value());
    QVERIFY(!stored->expires_at.isEmpty());
    QVERIFY(!stored->last_activity.isEmpty());

    QThread::sleep(1);
    const auto current = harness().auth_server.current_session(session_id);
    QVERIFY(current.is_ok());
    QCOMPARE(current.value().session_id, session_id);

    const auto refreshed = harness().session_repository.find_active_session(session_id);
    QVERIFY(refreshed.has_value());
    QVERIFY(refreshed->last_activity >= stored->last_activity);
}

void PhaseOneAuthSessionTest::expired_session_is_invalidated_on_access() {
    const QString session_id = harness().bootstrap_and_login_admin();
    QVERIFY(!session_id.isEmpty());

    const QString expired_at = QDateTime::currentDateTimeUtc().addSecs(-10).toString(Qt::ISODate);
    auto expire = fincept::Database::instance().execute(
        QStringLiteral("UPDATE sessions SET expires_at = ? WHERE session_id = ?"), {expired_at, session_id});
    QVERIFY(expire.is_ok());

    const auto current = harness().auth_server.current_session(session_id);
    QVERIFY(current.is_err());
    QCOMPARE(QString::fromStdString(current.error()), QStringLiteral("session_expired"));

    const auto stored = harness().session_repository.find_session(session_id);
    QVERIFY(stored.has_value());
    QVERIFY(stored->invalidated);

}

void PhaseOneAuthSessionTest::disabled_user_login_is_denied_and_active_session_is_invalidated() {
    QVERIFY(harness().user_admin_server.bootstrap(QStringLiteral("admin"), QStringLiteral("secret")).is_ok());
    QVERIFY(harness().user_admin_server.create_user(QStringLiteral("alice")).is_ok());

    const auto user = harness().user_repository.find_by_username(QStringLiteral("alice"));
    QVERIFY(user.has_value());
    QVERIFY(harness().user_admin_server.set_initial_password(user->user_id, QStringLiteral("secret")).is_ok());

    const auto login = harness().auth_server.login(QStringLiteral("alice"), QStringLiteral("secret"));
    QVERIFY(login.is_ok());

    QVERIFY(harness().user_admin_server.disable_user(user->user_id).is_ok());

    const auto disabled_login = harness().auth_server.login(QStringLiteral("alice"), QStringLiteral("secret"));
    QVERIFY(disabled_login.is_err());
    QCOMPARE(QString::fromStdString(disabled_login.error()), QStringLiteral("user_disabled"));

    const auto current = harness().auth_server.current_session(login.value().session_id);
    QVERIFY(current.is_err());
    QCOMPARE(QString::fromStdString(current.error()), QStringLiteral("user_disabled"));

    const auto stored = harness().session_repository.find_session(login.value().session_id);
    QVERIFY(stored.has_value());
    QVERIFY(stored->invalidated);

}

void PhaseOneAuthSessionTest::logout_invalidates_current_session() {
    const QString session_id = harness().bootstrap_and_login_admin();
    QVERIFY(!session_id.isEmpty());

    QVERIFY(harness().auth_server.logout(session_id).is_ok());
    QVERIFY(!harness().session_repository.find_active_session(session_id).has_value());

    const auto current = harness().auth_server.current_session(session_id);
    QVERIFY(current.is_err());
    QCOMPARE(QString::fromStdString(current.error()), QStringLiteral("session_revoked"));
}

void PhaseOneAuthSessionTest::auth_events_record_login_failed_login_and_logout() {
    QVERIFY(harness().user_admin_server.bootstrap(QStringLiteral("admin"), QStringLiteral("secret")).is_ok());

    const auto failed_login = harness().auth_server.login(QStringLiteral("admin"), QStringLiteral("wrong"));
    QVERIFY(failed_login.is_err());

    const auto success_login = harness().auth_server.login(QStringLiteral("admin"), QStringLiteral("secret"));
    QVERIFY(success_login.is_ok());
    QVERIFY(harness().auth_server.logout(success_login.value().session_id).is_ok());

    const auto events = harness().audit_repository.list_events({});
    QVERIFY(events.is_ok());
    QVERIFY(events.value().size() >= 3);
}

} // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    PhaseOneAuthSessionTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_phase_one_auth_session.moc"
