#include <QApplication>
#include "app/MainWindow.h"
#include "core/logging/Logger.h"
#include "core/session/SessionManager.h"
#include "core/config/AppConfig.h"
#include "network/http/HttpClient.h"
#include "storage/sqlite/Database.h"
#include "storage/sqlite/CacheDatabase.h"
#include "ui/theme/Theme.h"
#include "auth/AuthManager.h"
#include "auth/SessionGuard.h"
#include <QStandardPaths>
#include <QDir>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("FinceptTerminal");
    app.setOrganizationName("Fincept");
    app.setApplicationVersion("4.0.0");

    // Initialize logging
    QString log_dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(log_dir);
    fincept::Logger::instance().set_file(log_dir + "/fincept.log");
    LOG_INFO("App", "Fincept Terminal v4.0.0 starting...");

    // Apply Bloomberg theme
    fincept::ui::apply_global_stylesheet();

    // Initialize config
    auto& config = fincept::AppConfig::instance();
    fincept::HttpClient::instance().set_base_url(config.api_base_url());
    fincept::HttpClient::instance().set_auth_header("fk_user_vU20qwUxKtPmg0fWpriNBhcAnBVGgOtJxsKiiwfD9Qo");

    // Open main database
    QString db_path = log_dir + "/fincept.db";
    auto db_result = fincept::Database::instance().open(db_path);
    if (db_result.is_err()) {
        LOG_ERROR("App", "Failed to open database: " + QString::fromStdString(db_result.error()));
    }

    // Open cache database (non-fatal if fails)
    QString cache_path = log_dir + "/cache.db";
    auto cache_result = fincept::CacheDatabase::instance().open(cache_path);
    if (cache_result.is_err()) {
        LOG_WARN("App", "Cache DB failed (non-fatal): " + QString::fromStdString(cache_result.error()));
    }

    // Start session
    fincept::SessionManager::instance().start_session();

    // Initialize auth (loads saved session, validates with server)
    fincept::auth::AuthManager::instance().initialize();

    // Session guard — auto-logout on 401
    fincept::auth::SessionGuard session_guard;

    // Launch main window
    fincept::MainWindow window;
    window.show();

    LOG_INFO("App", "Application ready");
    return app.exec();
}
