#include "app/PhaseOneMigrationRegistry.h"
#include "auth/AuthTypes.h"
#include "multiuser/client/PhaseOneClientTransport.h"
#include "multiuser/client/PhaseOnePortfolioApi.h"
#include "multiuser/client/PhaseOneSessionStateGuard.h"
#include "multiuser/contracts/PhaseOneAuditTypes.h"
#include "multiuser/contracts/PhaseOnePortfolioTypes.h"
#include "multiuser/server/PhaseOneAuthServer.h"
#include "multiuser/server/PhaseOnePortfolioServer.h"
#include "multiuser/server/PhaseOneUserAdminServer.h"
#include "multiuser/server/http/PhaseOneHttpServer.h"
#include "multiuser/server/http/PhaseOnePortfolioHttpRoutes.h"
#include "multiuser/server/storage/PhaseOneAuditRepository.h"
#include "multiuser/server/storage/PhaseOneSessionRepository.h"
#include "multiuser/server/storage/PhaseOneUserRepository.h"
#include "storage/repositories/PortfolioHoldingsRepository.h"
#include "storage/repositories/PortfolioRepository.h"
#include "storage/sqlite/Database.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUrlQuery>
#include <memory>
#include <QtTest/QtTest>

using namespace fincept::multiuser;

namespace {

class RecordingBackend : public PhaseOneRequestBackend {
  public:
    void send(const QString& method, const QString& url, const QJsonObject& body, const QMap<QString, QString>& headers,
              Callback cb) override {
        last_method = method;
        last_url = url;
        last_body = body;
        last_headers = headers;
        if (cb)
            cb(next_response);
    }

    QString last_method;
    QString last_url;
    QJsonObject last_body;
    QMap<QString, QString> last_headers;
    PhaseOneBackendResponse next_response;
};

class PhaseOnePortfolioHarness {
  public:
    PhaseOnePortfolioHarness()
        : portfolio_repository(fincept::PortfolioRepository::instance()),
          holdings_repository(fincept::PortfolioHoldingsRepository::instance()) {
        fincept::PhaseOneMigrationRegistry::register_all();
        reset_database();
    }

    ~PhaseOnePortfolioHarness() {
        fincept::Database::instance().close();
    }

    void reset_database() {
        fincept::Database::instance().close();
        const QString db_path = dir_.filePath(QStringLiteral("phase_one_portfolio_contracts.sqlite"));
        QFile::remove(db_path);

        const auto result = fincept::Database::instance().open(db_path);
        QVERIFY2(result.is_ok(), qPrintable(QString::fromStdString(result.error())));

        QSqlQuery query(fincept::Database::instance().raw_db());
        QVERIFY(query.exec(QStringLiteral("DELETE FROM portfolio_transactions")));
        QVERIFY(query.exec(QStringLiteral("DELETE FROM portfolio_assets")));
        QVERIFY(query.exec(QStringLiteral("DELETE FROM portfolios")));
        QVERIFY(query.exec(QStringLiteral("DELETE FROM portfolio_holdings")));
        QVERIFY(query.exec(QStringLiteral("DELETE FROM sessions")));
        QVERIFY(query.exec(QStringLiteral("DELETE FROM audit_events")));
        QVERIFY(query.exec(QStringLiteral("DELETE FROM users")));

        user_admin_server = std::make_unique<PhaseOneUserAdminServer>(&user_repository, &audit_repository);
        auth_server = std::make_unique<PhaseOneAuthServer>(&user_repository, &session_repository, &audit_repository);
        portfolio_server =
            std::make_unique<PhaseOnePortfolioServer>(&portfolio_repository, &holdings_repository, &audit_repository);
        http_server = std::make_unique<PhaseOneHttpServer>();
        register_phase_one_portfolio_routes(*http_server, *portfolio_server, *auth_server);
    }

    QString create_standard_session() {
        if (!user_admin_server->bootstrap(QStringLiteral("admin"), QStringLiteral("secret")).is_ok())
            qFatal("bootstrap failed");
        if (!user_admin_server->create_user(QStringLiteral("analyst")).is_ok())
            qFatal("create_user failed");

        const auto analyst = user_repository.find_by_username(QStringLiteral("analyst"));
        if (!analyst.has_value())
            qFatal("analyst user not found");
        if (!user_admin_server->set_initial_password(analyst->user_id, QStringLiteral("analyst-pass")).is_ok())
            qFatal("set_initial_password failed");

        const auto login = auth_server->login(QStringLiteral("analyst"), QStringLiteral("analyst-pass"));
        if (!login.is_ok())
            qFatal("login failed");
        return login.value().session_id;
    }

    QByteArray request(const QByteArray& method, const QString& path, const QString& session_id = {},
                       const QJsonObject& body = {}) {
        QByteArray raw = method + ' ' + path.toUtf8() + " HTTP/1.1\r\nHost: localhost:45450\r\n";
        if (!session_id.isEmpty())
            raw += "Authorization: Bearer " + session_id.toUtf8() + "\r\n";
        if (!body.isEmpty())
            raw += "Content-Type: application/json\r\n";
        raw += "\r\n";
        if (!body.isEmpty())
            raw += QJsonDocument(body).toJson(QJsonDocument::Compact);
        return raw;
    }

    PhaseOneHttpResponse dispatch(const QByteArray& method, const QString& path, const QString& session_id = {},
                                  const QJsonObject& body = {}) {
        const auto response = http_server->dispatch(request(method, path, session_id, body));
        if (!response.is_ok())
            qFatal("dispatch failed: %s", response.error().c_str());
        return response.value();
    }

    QJsonObject parse_object(const PhaseOneHttpResponse& response) const {
        const QJsonDocument document = QJsonDocument::fromJson(response.body);
        if (!document.isObject())
            qFatal("response body was not a JSON object");
        return document.object();
    }

    QVector<PhaseOneAuditEvent> audit_events() const {
        const auto result = audit_repository.list_events({});
        if (result.is_err())
            return {};
        return result.value();
    }

    fincept::PortfolioRepository& portfolio_repository;
    fincept::PortfolioHoldingsRepository& holdings_repository;
    PhaseOneUserRepository user_repository;
    PhaseOneSessionRepository session_repository;
    PhaseOneAuditRepository audit_repository;
    std::unique_ptr<PhaseOneUserAdminServer> user_admin_server;
    std::unique_ptr<PhaseOneAuthServer> auth_server;
    std::unique_ptr<PhaseOnePortfolioServer> portfolio_server;
    std::unique_ptr<PhaseOneHttpServer> http_server;

  private:
    QTemporaryDir dir_;
};

PhaseOnePortfolioHarness& harness() {
    static PhaseOnePortfolioHarness value;
    return value;
}

class PhaseOnePortfolioContractsTest : public QObject {
    Q_OBJECT

  private slots:
    void init();
    void portfolio_routes_reject_unauthenticated_access();
    void standard_users_can_list_mutate_and_fetch_portfolios_and_holdings();
    void revoked_portfolio_requests_clear_client_auth_state();
};

void PhaseOnePortfolioContractsTest::init() {
    harness().reset_database();
    PhaseOneClientTransport::reset_backend_for_tests();
    PhaseOneSessionStateGuard::set_invalid_session_handler({});
}

void PhaseOnePortfolioContractsTest::portfolio_routes_reject_unauthenticated_access() {
    const auto portfolio_response = harness().dispatch("GET", QStringLiteral("/phase1/portfolios"));
    QCOMPARE(portfolio_response.status_code, 401);
    QCOMPARE(harness().parse_object(portfolio_response).value("error_code").toString(), QString("session_invalid"));

    const auto holdings_response = harness().dispatch("GET", QStringLiteral("/phase1/holdings"));
    QCOMPARE(holdings_response.status_code, 401);
    QCOMPARE(harness().parse_object(holdings_response).value("error_code").toString(), QString("session_invalid"));
}

void PhaseOnePortfolioContractsTest::standard_users_can_list_mutate_and_fetch_portfolios_and_holdings() {
    const QString session_id = harness().create_standard_session();

    const auto empty_portfolios = harness().dispatch("GET", QStringLiteral("/phase1/portfolios"), session_id);
    QCOMPARE(empty_portfolios.status_code, 200);
    QCOMPARE(harness().parse_object(empty_portfolios).value("portfolios").toArray().size(), 0);

    const auto created_portfolio = harness().dispatch(
        "POST", QStringLiteral("/phase1/portfolios/create"), session_id,
        QJsonObject{{"name", QStringLiteral("Shared Book")},
                    {"owner", QStringLiteral("Family Office")},
                    {"currency", QStringLiteral("USD")},
                    {"description", QStringLiteral("Server-backed book")}});
    QCOMPARE(created_portfolio.status_code, 200);

    const QJsonObject created_portfolio_json = harness().parse_object(created_portfolio).value("portfolio").toObject();
    QVERIFY(!created_portfolio_json.value("id").toString().isEmpty());
    const QString portfolio_id = created_portfolio_json.value("id").toString();

    QUrlQuery portfolio_query;
    portfolio_query.addQueryItem(QStringLiteral("portfolio_id"), portfolio_id);
    const auto fetched_portfolio = harness().dispatch(
        "GET", QStringLiteral("/phase1/portfolios?") + portfolio_query.toString(QUrl::FullyEncoded), session_id);
    QCOMPARE(fetched_portfolio.status_code, 200);
    QCOMPARE(harness().parse_object(fetched_portfolio).value("portfolio").toObject().value("id").toString(), portfolio_id);

    const auto updated_portfolio = harness().dispatch(
        "POST", QStringLiteral("/phase1/portfolios/update"), session_id,
        QJsonObject{{"id", portfolio_id},
                    {"name", QStringLiteral("Shared Book Updated")},
                    {"owner", QStringLiteral("Family Office")},
                    {"currency", QStringLiteral("EUR")},
                    {"description", QStringLiteral("Updated description")}});
    QCOMPARE(updated_portfolio.status_code, 200);
    QCOMPARE(harness().parse_object(updated_portfolio).value("portfolio").toObject().value("currency").toString(),
             QString("EUR"));

    const auto empty_holdings = harness().dispatch("GET", QStringLiteral("/phase1/holdings"), session_id);
    QCOMPARE(empty_holdings.status_code, 200);
    QCOMPARE(harness().parse_object(empty_holdings).value("holdings").toArray().size(), 0);

    const auto created_holding = harness().dispatch(
        "POST", QStringLiteral("/phase1/holdings/create"), session_id,
        QJsonObject{{"symbol", QStringLiteral("AAPL")},
                    {"name", QStringLiteral("Apple Inc.")},
                    {"shares", 12.0},
                    {"avg_cost", 182.5}});
    QCOMPARE(created_holding.status_code, 200);
    const int holding_id = harness().parse_object(created_holding).value("holding").toObject().value("id").toInt();
    QVERIFY(holding_id > 0);

    const auto updated_holding = harness().dispatch(
        "POST", QStringLiteral("/phase1/holdings/update"), session_id,
        QJsonObject{{"id", holding_id}, {"shares", 15.0}, {"avg_cost", 190.0}});
    QCOMPARE(updated_holding.status_code, 200);
    QCOMPARE(harness().parse_object(updated_holding).value("holding").toObject().value("shares").toDouble(), 15.0);

    const auto removed_holding = harness().dispatch(
        "POST", QStringLiteral("/phase1/holdings/remove"), session_id, QJsonObject{{"id", holding_id}});
    QCOMPARE(removed_holding.status_code, 200);

    const auto events = harness().audit_events();
    QVERIFY(events.size() >= 5);

    QStringList actions;
    for (const auto& event : events) {
        if (event.user_identity == QStringLiteral("analyst"))
            actions.append(event.action_type);
    }

    QVERIFY(actions.contains(QStringLiteral("portfolio_create")));
    QVERIFY(actions.contains(QStringLiteral("portfolio_update")));
    QVERIFY(actions.contains(QStringLiteral("holding_create")));
    QVERIFY(actions.contains(QStringLiteral("holding_update")));
    QVERIFY(actions.contains(QStringLiteral("holding_delete")));
}

void PhaseOnePortfolioContractsTest::revoked_portfolio_requests_clear_client_auth_state() {
    auto backend = std::make_shared<RecordingBackend>();
    backend->next_response.status_code = 401;
    backend->next_response.body =
        QByteArrayLiteral("{\"error_code\":\"session_revoked\",\"message\":\"Session was revoked.\"}");
    PhaseOneClientTransport::set_backend_for_tests(backend);
    PhaseOneClientTransport::instance().set_session_id(QStringLiteral("revoked-session"));

    int invalidation_count = 0;
    PhaseOneSessionStateGuard::set_invalid_session_handler([&invalidation_count]() { ++invalidation_count; });

    bool callback_invoked = false;
    fincept::auth::ApiResponse callback_response;
    PhaseOnePortfolioApi::instance().list_portfolios([&](fincept::auth::ApiResponse response) {
        callback_invoked = true;
        callback_response = response;
    });

    QVERIFY(callback_invoked);
    QCOMPARE(backend->last_method, QString("GET"));
    QVERIFY(backend->last_url.contains(QStringLiteral("/phase1/portfolios")));
    QCOMPARE(backend->last_headers.value(QStringLiteral("Authorization")), QString("Bearer revoked-session"));
    QVERIFY(!callback_response.success);
    QCOMPARE(callback_response.status_code, 401);
    QCOMPARE(callback_response.error_code, QString("session_revoked"));
    QCOMPARE(invalidation_count, 1);
}

} // namespace

QTEST_GUILESS_MAIN(PhaseOnePortfolioContractsTest)

#include "test_phase_one_portfolio_contracts.moc"
