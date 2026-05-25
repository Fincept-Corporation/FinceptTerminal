#include "app/PhaseOneMigrationRegistry.h"
#include "auth/AuthApi.h"
#include "multiuser/client/PhaseOneClientTransport.h"
#include "multiuser/server/PhaseOneAuthServer.h"
#include "multiuser/server/PhaseOneUserAdminServer.h"
#include "multiuser/server/http/PhaseOneAuthHttpRoutes.h"
#include "multiuser/server/http/PhaseOneHttpServer.h"
#include "multiuser/server/storage/PhaseOneAuditRepository.h"
#include "multiuser/server/storage/PhaseOneSessionRepository.h"
#include "multiuser/server/storage/PhaseOneUserRepository.h"
#include "storage/sqlite/Database.h"

#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest/QtTest>

using namespace fincept::auth;
using namespace fincept::multiuser;

namespace {

class RecordingBackend : public PhaseOneRequestBackend {
  public:
    void send(const QString& method, const QString& url, const QJsonObject& body, const QMap<QString, QString>& headers,
              Callback cb) override {
        ++call_count;
        last_method = method;
        last_url = url;
        last_body = body;
        last_headers = headers;
        if (cb)
            cb(next_response);
    }

    int call_count = 0;
    QString last_method;
    QString last_url;
    QJsonObject last_body;
    QMap<QString, QString> last_headers;
    PhaseOneBackendResponse next_response;
};

class PhaseOneAuthHarness {
  public:
    PhaseOneAuthHarness() : auth_server(&user_repository, &session_repository, &audit_repository),
                            user_admin_server(&user_repository, &audit_repository) {
        fincept::PhaseOneMigrationRegistry::register_all();
        reset_database();
        register_phase_one_auth_routes(http_server, auth_server);
    }

    ~PhaseOneAuthHarness() {
        fincept::Database::instance().close();
    }

    void reset_database() {
        fincept::Database::instance().close();
        const QString db_path = dir_.filePath(QStringLiteral("phase_one_auth.sqlite"));
        QFile::remove(db_path);
        const auto result = fincept::Database::instance().open(db_path);
        QVERIFY2(result.is_ok(), qPrintable(QString::fromStdString(result.error())));
    }

    PhaseOneHttpResponse login_request(const QString& username, const QString& password) {
        const QByteArray request = QByteArray("POST /phase1/auth/login HTTP/1.1\r\nHost: localhost\r\nContent-Type: application/json\r\n\r\n") +
                                   QJsonDocument(QJsonObject{{"username", username}, {"password", password}}).toJson(QJsonDocument::Compact);
        const auto response = http_server.dispatch(request);
        if (response.is_err()) {
            qFatal("%s", qPrintable(QString::fromStdString(response.error())));
            return {};
        }
        return response.value();
    }

    QJsonObject body(const PhaseOneHttpResponse& response) {
        return QJsonDocument::fromJson(response.body).object();
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

PhaseOneAuthHarness& harness() {
    static PhaseOneAuthHarness value;
    return value;
}

class PhaseOneAuthLoginTest : public QObject {
    Q_OBJECT

  private slots:
    void init();
    void phase_one_login_uses_expected_route_and_payload();
    void phase_one_login_returns_setup_required_when_bootstrap_is_open();
    void phase_one_login_rejects_invalid_credentials();
    void phase_one_login_returns_top_level_session_payload();
};

void PhaseOneAuthLoginTest::init() {
    harness().reset_database();
}

void PhaseOneAuthLoginTest::phase_one_login_uses_expected_route_and_payload() {
    auto backend = std::make_shared<RecordingBackend>();
    backend->next_response.status_code = 200;
    backend->next_response.body =
        R"({"user_id":1,"username":"admin","role":"admin","session_id":"sess-1","expires_at":"2026-05-30T12:00:00Z"})";
    PhaseOneClientTransport::set_backend_for_tests(backend);

    ApiResponse response;
    AuthApi::instance().phase_one_login("admin", "secret",
                                        [&response](ApiResponse r) { response = std::move(r); });

    QCOMPARE(backend->last_method, QString("POST"));
    QCOMPARE(backend->last_url, QString("/phase1/auth/login"));
    QCOMPARE(backend->last_body.value("username").toString(), QString("admin"));
    QCOMPARE(backend->last_body.value("password").toString(), QString("secret"));
    QVERIFY(response.success);
    QCOMPARE(response.data.value("session_id").toString(), QString("sess-1"));

    PhaseOneClientTransport::reset_backend_for_tests();
}

void PhaseOneAuthLoginTest::phase_one_login_returns_setup_required_when_bootstrap_is_open() {
    const auto response = harness().login_request(QStringLiteral("admin"), QStringLiteral("secret"));
    QCOMPARE(response.status_code, 401);
    QCOMPARE(harness().body(response).value("error_code").toString(), QStringLiteral("setup_required"));
}

void PhaseOneAuthLoginTest::phase_one_login_rejects_invalid_credentials() {
    QVERIFY(harness().user_admin_server.bootstrap(QStringLiteral("admin"), QStringLiteral("secret")).is_ok());

    const auto response = harness().login_request(QStringLiteral("admin"), QStringLiteral("wrong"));
    QCOMPARE(response.status_code, 401);
    QCOMPARE(harness().body(response).value("error_code").toString(), QStringLiteral("invalid_credentials"));
}

void PhaseOneAuthLoginTest::phase_one_login_returns_top_level_session_payload() {
    QVERIFY(harness().user_admin_server.bootstrap(QStringLiteral("admin"), QStringLiteral("secret")).is_ok());

    const auto response = harness().login_request(QStringLiteral("admin"), QStringLiteral("secret"));
    QCOMPARE(response.status_code, 200);

    const QJsonObject body = harness().body(response);
    QCOMPARE(body.value("username").toString(), QStringLiteral("admin"));
    QCOMPARE(body.value("role").toString(), QStringLiteral("admin"));
    QVERIFY(!body.value("session_id").toString().isEmpty());
    QVERIFY(!body.value("expires_at").toString().isEmpty());
}

} // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    PhaseOneAuthLoginTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_phase_one_auth_login.moc"
