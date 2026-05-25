#include "auth/AuthApi.h"
#include "auth/AuthTypes.h"
#include "multiuser/client/PhaseOneClientTransport.h"
#include "multiuser/contracts/PhaseOneAuthTypes.h"

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

class PhaseOneAuthContractsTest : public QObject {
    Q_OBJECT

  private slots:
    void login_request_serializes_username_and_password_only();
    void session_response_parses_required_fields();
    void hosted_only_methods_bypass_phase_one_transport();
};

void PhaseOneAuthContractsTest::login_request_serializes_username_and_password_only() {
    PhaseOneLoginRequest request;
    request.username = "alice";
    request.password = "secret";

    const QJsonObject json = request.to_json();
    QCOMPARE(json.value("username").toString(), QString("alice"));
    QCOMPARE(json.value("password").toString(), QString("secret"));
    QCOMPARE(json.keys().size(), 2);
}

void PhaseOneAuthContractsTest::session_response_parses_required_fields() {
    const QJsonObject json{{"user_id", 42},
                           {"username", "alice"},
                           {"role", "admin"},
                           {"session_id", "session-123"},
                           {"expires_at", "2026-05-30T12:00:00Z"}};

    const PhaseOneSessionInfo info = PhaseOneSessionInfo::from_json(json);
    QCOMPARE(info.user_id, 42);
    QCOMPARE(info.username, QString("alice"));
    QCOMPARE(info.role, QString("admin"));
    QCOMPARE(info.session_id, QString("session-123"));
    QCOMPARE(info.expires_at, QString("2026-05-30T12:00:00Z"));
}

void PhaseOneAuthContractsTest::hosted_only_methods_bypass_phase_one_transport() {
    auto backend = std::make_shared<RecordingBackend>();
    PhaseOneClientTransport::set_backend_for_tests(backend);

    bool callback_invoked = false;
    AuthApi::instance().get_subscription_plans([&callback_invoked](ApiResponse) { callback_invoked = true; });
    QCoreApplication::processEvents();
    AuthApi::instance().generate_checkout_token("pro", [&callback_invoked](ApiResponse) { callback_invoked = true; });
    QCoreApplication::processEvents();

    QCOMPARE(backend->call_count, 0);

    PhaseOneClientTransport::reset_backend_for_tests();
}

} // namespace

QTEST_APPLESS_MAIN(PhaseOneAuthContractsTest)

#include "test_phase_one_auth_contracts.moc"
