#include "multiuser/server/PhaseOneServerRuntime.h"

#include "app/PhaseOneMigrationRegistry.h"
#include "core/logging/Logger.h"
#include "multiuser/server/http/PhaseOneAuditHttpRoutes.h"
#include "multiuser/server/http/PhaseOneAuthHttpRoutes.h"
#include "multiuser/server/http/PhaseOneHttpJsonResponse.h"
#include "multiuser/server/http/PhaseOneHttpServer.h"
#include "multiuser/server/http/PhaseOnePortfolioHttpRoutes.h"
#include "multiuser/server/http/PhaseOneUserAdminHttpRoutes.h"
#include "multiuser/server/PhaseOnePortfolioServer.h"
#include "storage/sqlite/Database.h"

namespace fincept::multiuser {

PhaseOneServerRuntime::PhaseOneServerRuntime() = default;
PhaseOneServerRuntime::~PhaseOneServerRuntime() = default;

PhaseOneServerStartResult PhaseOneServerRuntime::start(const fincept::PhaseOneResolvedServerOptions& options) {
    if (options.port == 0) {
        return {false,
                {QStringLiteral("invalid_server_port"), QStringLiteral("Phase one server port must be greater than zero.")}};
    }

    if (options.host.trimmed().isEmpty()) {
        return {false, {QStringLiteral("invalid_server_host"), QStringLiteral("Phase one server host is required.")}};
    }

    if (options.host.trimmed() == QStringLiteral("0.0.0.0")) {
        return {false,
                {QStringLiteral("invalid_server_host"),
                 QStringLiteral("Phase one server host must not use 0.0.0.0 for live server runs.")}};
    }

    if (options.db_path.trimmed().isEmpty()) {
        return {false, {QStringLiteral("invalid_server_db_path"), QStringLiteral("Phase one server database path is required.")}};
    }

    PhaseOneMigrationRegistry::register_all();
    const auto db_result = fincept::Database::instance().open(options.db_path);
    if (db_result.is_err()) {
        return {false, {QStringLiteral("server_database_open_failed"), QString::fromStdString(db_result.error())}};
    }

    http_server_ = std::make_unique<PhaseOneHttpServer>();
    register_routes();

    const auto bind_result = http_server_->bind(QHostAddress(options.host), options.port);
    if (bind_result.is_err()) {
        http_server_.reset();
        return {false, {QStringLiteral("server_bind_failed"), QString::fromStdString(bind_result.error())}};
    }

    const auto start_result = http_server_->start();
    if (start_result.is_err()) {
        http_server_.reset();
        return {false, {QStringLiteral("server_start_failed"), QString::fromStdString(start_result.error())}};
    }

    LOG_INFO("PhaseOneServer",
             QString("Server listening on %1:%2 using %3").arg(options.host).arg(options.port).arg(options.db_path));
    return {true, {}};
}

void PhaseOneServerRuntime::register_routes() {
    portfolio_server_ = std::make_unique<PhaseOnePortfolioServer>();
    http_server_->register_route("GET", QStringLiteral("/phase1/health"), [](const PhaseOneHttpRequestContext&) {
        return PhaseOneHttpJsonResponse::success(200, QJsonObject{{"status", "starting"}});
    });
    register_phase_one_auth_routes(*http_server_);
    register_phase_one_user_admin_routes(*http_server_);
    register_phase_one_audit_routes(*http_server_);
    register_phase_one_portfolio_routes(*http_server_, *portfolio_server_);
}

} // namespace fincept::multiuser
