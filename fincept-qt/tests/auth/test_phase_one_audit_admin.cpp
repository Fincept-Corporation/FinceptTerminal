#include "app/PhaseOneMigrationRegistry.h"
#include "multiuser/server/PhaseOneAuthServer.h"
#include "multiuser/server/PhaseOneAuditServer.h"
#include "multiuser/server/PhaseOneUserAdminServer.h"
#include "multiuser/server/http/PhaseOneAuditHttpRoutes.h"
#include "multiuser/server/http/PhaseOneAuthHttpRoutes.h"
#include "multiuser/server/http/PhaseOneHttpServer.h"
#include "multiuser/server/http/PhaseOneUserAdminHttpRoutes.h"
#include "multiuser/server/storage/PhaseOneAuditRepository.h"
#include "multiuser/server/storage/PhaseOneSessionRepository.h"
#include "multiuser/server/storage/PhaseOneUserRepository.h"
#include "storage/sqlite/Database.h"

#include <QFile>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QtTest/QtTest>

using namespace fincept::multiuser;

namespace {

class PhaseOneAuditHarness {
  public:
    PhaseOneAuditHarness()
        : auth_server(&user_repository, &session_repository, &audit_repository),
          user_admin_server(&user_repository, &audit_repository),
          audit_server(&audit_repository) {
        fincept::PhaseOneMigrationRegistry::register_all();
        reset_database();
        register_phase_one_auth_routes(http_server, auth_server);
        register_phase_one_user_admin_routes(http_server, user_admin_server);
        register_phase_one_audit_routes(http_server, audit_server);
    }

    ~PhaseOneAuditHarness() {
        fincept::Database::instance().close();
    }

    void reset_database() {
        fincept::Database::instance().close();
        const QString db_path = dir_.filePath(QStringLiteral("phase_one_audit.sqlite"));
        QFile::remove(db_path);
        const auto result = fincept::Database::instance().open(db_path);
        QVERIFY2(result.is_ok(), qPrintable(QString::fromStdString(result.error())));
    }

    PhaseOneHttpResponse dispatch(const QByteArray& raw_request) {
        const auto result = http_server.dispatch(raw_request);
        if (result.is_err()) {
            qFatal("%s", qPrintable(QString::fromStdString(result.error())));
            return {};
        }
        return result.value();
    }

    QJsonObject json_body(const PhaseOneHttpResponse& response) {
        return QJsonDocument::fromJson(response.body).object();
    }

    QString login(const QString& username, const QString& password) {
        const QByteArray request = QByteArray("POST /phase1/auth/login HTTP/1.1\r\nHost: localhost\r\nContent-Type: application/json\r\n\r\n") +
                                   QJsonDocument(QJsonObject{{"username", username}, {"password", password}}).toJson(QJsonDocument::Compact);
        const auto response = dispatch(request);
        return json_body(response).value("session_id").toString();
    }

    PhaseOneUserRepository user_repository;
    PhaseOneSessionRepository session_repository;
    PhaseOneAuditRepository audit_repository;
    PhaseOneAuthServer auth_server;
    PhaseOneUserAdminServer user_admin_server;
    PhaseOneAuditServer audit_server;
    PhaseOneHttpServer http_server;

  private:
    QTemporaryDir dir_;
};

PhaseOneAuditHarness& harness() {
    static PhaseOneAuditHarness value;
    return value;
}

class PhaseOneAuditAdminTest : public QObject {
    Q_OBJECT

  private slots:
    void init();
    void admin_routes_require_bearer_authentication_and_admin_role();
    void admin_can_retrieve_recent_audit_events_with_filters();
};

void PhaseOneAuditAdminTest::init() {
    harness().reset_database();
    QVERIFY(harness().user_admin_server.bootstrap(QStringLiteral("admin"), QStringLiteral("secret")).is_ok());
    QVERIFY(harness().user_admin_server.create_user(QStringLiteral("analyst")).is_ok());
    const auto analyst = harness().user_repository.find_by_username(QStringLiteral("analyst"));
    QVERIFY(analyst.has_value());
    QVERIFY(harness().user_admin_server.set_initial_password(analyst->user_id, QStringLiteral("launch-123")).is_ok());
}

void PhaseOneAuditAdminTest::admin_routes_require_bearer_authentication_and_admin_role() {
    const auto no_auth = harness().dispatch("GET /phase1/admin/audit-events HTTP/1.1\r\nHost: localhost\r\n\r\n");
    QCOMPARE(no_auth.status_code, 401);

    const QString user_session = harness().login(QStringLiteral("analyst"), QStringLiteral("launch-123"));
    QVERIFY(!user_session.isEmpty());
    const QByteArray user_request = QByteArray("GET /phase1/admin/audit-events HTTP/1.1\r\nHost: localhost\r\nAuthorization: Bearer ") +
                                    user_session.toUtf8() + QByteArray("\r\n\r\n");
    const auto forbidden = harness().dispatch(user_request);
    QCOMPARE(forbidden.status_code, 403);
    QCOMPARE(harness().json_body(forbidden).value("error_code").toString(), QStringLiteral("forbidden"));
}

void PhaseOneAuditAdminTest::admin_can_retrieve_recent_audit_events_with_filters() {
    const QString admin_session = harness().login(QStringLiteral("admin"), QStringLiteral("secret"));
    QVERIFY(!admin_session.isEmpty());

    const auto analyst = harness().user_repository.find_by_username(QStringLiteral("analyst"));
    QVERIFY(analyst.has_value());
    QVERIFY(harness().user_admin_server.disable_user(analyst->user_id).is_ok());

    const QByteArray all_request = QByteArray("GET /phase1/admin/audit-events HTTP/1.1\r\nHost: localhost\r\nAuthorization: Bearer ") +
                                   admin_session.toUtf8() + QByteArray("\r\n\r\n");
    const auto all_response = harness().dispatch(all_request);
    QCOMPARE(all_response.status_code, 200);
    const QJsonArray all_events = harness().json_body(all_response).value("audit_events").toArray();
    QVERIFY(all_events.size() >= 3);

    const QByteArray filtered_request = QByteArray("GET /phase1/admin/audit-events?user_identity=admin&action_type=bootstrap_admin HTTP/1.1\r\nHost: localhost\r\nAuthorization: Bearer ") +
                                        admin_session.toUtf8() + QByteArray("\r\n\r\n");
    const auto filtered_response = harness().dispatch(filtered_request);
    QCOMPARE(filtered_response.status_code, 200);
    const QJsonArray filtered_events = harness().json_body(filtered_response).value("audit_events").toArray();
    QCOMPARE(filtered_events.size(), 1);
    QCOMPARE(filtered_events.first().toObject().value("action_type").toString(), QStringLiteral("bootstrap_admin"));
}

} // namespace

int run_phase_one_audit_admin_smoke_test(int argc, char** argv) {
    PhaseOneAuditAdminTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_phase_one_audit_admin.moc"
