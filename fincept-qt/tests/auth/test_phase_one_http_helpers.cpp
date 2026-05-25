#include "multiuser/server/http/PhaseOneHttpJsonResponse.h"
#include "multiuser/server/http/PhaseOneHttpRequestContext.h"
#include "multiuser/server/http/PhaseOneHttpServer.h"
#include "multiuser/server/http/PhaseOneUserAdminHttpRoutes.h"

#include <QJsonDocument>
#include <QtTest/QtTest>

using namespace fincept::multiuser;

namespace {

class PhaseOneHttpHelpersTest : public QObject {
    Q_OBJECT

  private slots:
    void parses_simple_request_line_and_headers();
    void extracts_bearer_header();
    void formats_canonical_json_success_response();
    void formats_canonical_json_error_response();
    void bootstrap_status_route_dispatches_stub_response();
};

void PhaseOneHttpHelpersTest::parses_simple_request_line_and_headers() {
    const QByteArray raw_request = "GET /phase1/auth/session HTTP/1.1\r\nHost: localhost:45450\r\nX-Trace-Id: abc123\r\n\r\n";

    const auto parsed = PhaseOneHttpRequestContext::parse(raw_request);

    QVERIFY(parsed.is_ok());
    const PhaseOneHttpRequestContext& context = parsed.value();
    QCOMPARE(context.method(), QByteArray("GET"));
    QCOMPARE(context.path(), QString("/phase1/auth/session"));
    QCOMPARE(context.header("Host"), QString("localhost:45450"));
    QCOMPARE(context.header("X-Trace-Id"), QString("abc123"));
}

void PhaseOneHttpHelpersTest::extracts_bearer_header() {
    const QByteArray raw_request =
        "GET /phase1/auth/session HTTP/1.1\r\nAuthorization: Bearer session-123\r\n\r\n";

    const auto parsed = PhaseOneHttpRequestContext::parse(raw_request);

    QVERIFY(parsed.is_ok());
    QCOMPARE(parsed.value().bearer_session_token(), QString("session-123"));
}

void PhaseOneHttpHelpersTest::formats_canonical_json_success_response() {
    const auto response = PhaseOneHttpJsonResponse::success(200, QJsonObject{{"session_id", "session-123"}});

    QCOMPARE(response.status_code, 200);
    QCOMPARE(response.headers.value("Content-Type"), QByteArray("application/json"));

    const QJsonDocument json = QJsonDocument::fromJson(response.body);
    QVERIFY(json.isObject());
    QCOMPARE(json.object().value("ok").toBool(), true);
    QCOMPARE(json.object().value("data").toObject().value("session_id").toString(), QString("session-123"));
}

void PhaseOneHttpHelpersTest::formats_canonical_json_error_response() {
    const auto response = PhaseOneHttpJsonResponse::error(401, "session_expired", "expired");

    QCOMPARE(response.status_code, 401);
    QCOMPARE(response.headers.value("Content-Type"), QByteArray("application/json"));

    const QJsonDocument json = QJsonDocument::fromJson(response.body);
    QVERIFY(json.isObject());
    QCOMPARE(json.object().value("ok").toBool(), false);
    QCOMPARE(json.object().value("error_code").toString(), QString("session_expired"));
    QCOMPARE(json.object().value("message").toString(), QString("expired"));
}

void PhaseOneHttpHelpersTest::bootstrap_status_route_dispatches_stub_response() {
    PhaseOneHttpServer server;
    register_phase_one_user_admin_routes(server);

    const auto response = server.dispatch("GET /phase1/admin/bootstrap-status HTTP/1.1\r\nHost: localhost:45450\r\n\r\n");

    QVERIFY(response.is_ok());
    QCOMPARE(response.value().status_code, 200);

    const QJsonDocument json = QJsonDocument::fromJson(response.value().body);
    QVERIFY(json.isObject());
    QVERIFY(json.object().contains("bootstrap_open"));
    QVERIFY(!json.object().contains("error_code"));
}

} // namespace

QTEST_APPLESS_MAIN(PhaseOneHttpHelpersTest)

#include "test_phase_one_http_helpers.moc"
