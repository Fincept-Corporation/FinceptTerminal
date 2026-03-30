#include "app/MainWindow.h"
#include "auth/AuthManager.h"
#include "auth/SessionGuard.h"
#include "core/config/AppConfig.h"
#include "core/logging/Logger.h"
#include "core/session/SessionManager.h"
#include "mcp/McpInit.h"
#include "network/http/HttpClient.h"
#include "python/PythonSetupManager.h"
#include "screens/setup/SetupScreen.h"
#include "storage/repositories/SettingsRepository.h"
#include "storage/sqlite/CacheDatabase.h"
#include "storage/sqlite/Database.h"
#include "storage/sqlite/migrations/MigrationRunner.h"
#include "trading/instruments/InstrumentService.h"
#include "ui/theme/Theme.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QStandardPaths>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("FinceptTerminal");
    app.setOrganizationName("Fincept");
    app.setApplicationVersion("4.0.0");

    // Initialize logging
    QString log_dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(log_dir);

    // Remove stale SQLite WAL/SHM lock files left by a previous crash.
    // qsqlite.dll crashes on WAL recovery if the lock files are orphaned.
    for (const QString& db : {log_dir + "/fincept.db", log_dir + "/cache.db"}) {
        QFile::remove(db + "-wal");
        QFile::remove(db + "-shm");
    }
    // Also clean legacy DB location
    {
        const QString local_dir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
        const QString legacy1 = local_dir.section('/', 0, -3) + "/FinceptTerminal/fincept_settings.db";
        const QString legacy2 = QString(local_dir).replace("Fincept/FinceptTerminal", "FinceptTerminal")
                                + "/fincept_settings.db";
        for (const QString& db : {legacy1, legacy2}) {
            QFile::remove(db + "-wal");
            QFile::remove(db + "-shm");
        }
    }
    fincept::Logger::instance().set_file(log_dir + "/fincept.log");
    LOG_INFO("App", "Fincept Terminal v4.0.0 starting...");

    // Apply Bloomberg theme
    fincept::ui::apply_global_stylesheet();

    // Initialize config
    auto& config = fincept::AppConfig::instance();
    fincept::HttpClient::instance().set_base_url(config.api_base_url());
    const QString saved_token = config.auth_token();
    if (!saved_token.isEmpty()) {
        fincept::HttpClient::instance().set_auth_header(saved_token);
    }

    // Register migrations explicitly (avoids MSVC /OPT:REF stripping static-init TUs)
    fincept::register_migration_v001();
    fincept::register_migration_v002();
    fincept::register_migration_v003();
    fincept::register_migration_v004();
    fincept::register_migration_v005();
    fincept::register_migration_v006();
    fincept::register_migration_v007();
    fincept::register_migration_v008();
    fincept::register_migration_v009();
    fincept::register_migration_v010();
    fincept::register_migration_v011();

    // Open main database
    QString db_path = log_dir + "/fincept.db";
    auto db_result = fincept::Database::instance().open(db_path);
    if (db_result.is_err()) {
        LOG_ERROR("App", "Failed to open database: " + QString::fromStdString(db_result.error()));
    } else {
        // Warm InstrumentService cache from DB (non-blocking, no network).
        // If the table is empty on first run, the first broker login triggers a refresh.
        LOG_INFO("App", "Loading instruments from DB...");
        fincept::trading::InstrumentService::instance().load_from_db("zerodha");
        LOG_INFO("App", "Zerodha instruments loaded");
        fincept::trading::InstrumentService::instance().load_from_db("angelone");
        LOG_INFO("App", "AngelOne instruments loaded");
    }

    // Open cache database (non-fatal if fails)
    QString cache_path = log_dir + "/cache.db";
    auto cache_result = fincept::CacheDatabase::instance().open(cache_path);
    if (cache_result.is_err()) {
        LOG_WARN("App", "Cache DB failed (non-fatal): " + QString::fromStdString(cache_result.error()));
    }

    LOG_INFO("App", "Checking settings for legacy migration...");
    // One-time migration: copy settings from old DB (Local\FinceptTerminal\fincept_settings.db)
    // to new DB (Roaming\Fincept\FinceptTerminal\fincept.db) if the new DB has no settings yet.
    {
        LOG_INFO("App", "Querying settings...");
        auto existing = fincept::SettingsRepository::instance().get("fincept_session");
        LOG_INFO("App", "Settings query done");
        bool new_db_empty = existing.is_err() || existing.value().isEmpty();
        if (new_db_empty) {
            QString local_base = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
            // AppLocalDataLocation = .../Local/Fincept/FinceptTerminal — strip to .../Local/FinceptTerminal
            QString old_db_path = local_base.section('/', 0, -3) + "/FinceptTerminal/fincept_settings.db";
            if (!QFile::exists(old_db_path)) {
                // Try without the org subfolder
                old_db_path = local_base.replace("Fincept/FinceptTerminal", "FinceptTerminal") + "/fincept_settings.db";
            }
            if (QFile::exists(old_db_path)) {
                QSqlDatabase old_db = QSqlDatabase::addDatabase("QSQLITE", "legacy_migration");
                old_db.setDatabaseName(old_db_path);
                if (old_db.open()) {
                    QSqlQuery src(old_db);
                    if (src.exec("SELECT key, value FROM settings")) {
                        int count = 0;
                        while (src.next()) {
                            fincept::SettingsRepository::instance().set(src.value(0).toString(),
                                                                        src.value(1).toString(), "migrated");
                            ++count;
                        }
                        LOG_INFO("App", QString("Migrated %1 settings from legacy DB").arg(count));
                    }
                    old_db.close();
                }
                QSqlDatabase::removeDatabase("legacy_migration");
            }
        }
    }

    LOG_INFO("App", "Starting session manager...");
    // Start session
    fincept::SessionManager::instance().start_session();

    // Initialize auth (loads saved session, validates with server)
    fincept::auth::AuthManager::instance().initialize();

    // Session guard — auto-logout on 401
    fincept::auth::SessionGuard session_guard;

    // Initialize MCP tool system — registers all internal tools and starts
    // external MCP servers in the background (non-blocking).
    fincept::mcp::initialize_all_tools();

    // ── Python environment check ─────────────────────────────────────────────
    // On first run, Python + venvs may not be installed. Show a setup screen
    // that downloads and configures everything before launching the main app.
    auto setup_status = fincept::python::PythonSetupManager::instance().check_status();

    if (setup_status.needs_setup) {
        LOG_INFO("App", "Python environment not ready — showing setup screen");

        auto* setup_screen = new fincept::screens::SetupScreen;
        setup_screen->setWindowTitle("Fincept Terminal — First-Time Setup");
        setup_screen->resize(800, 600);
        setup_screen->show();

        // When setup completes, hide setup screen and launch main window
        QObject::connect(setup_screen, &fincept::screens::SetupScreen::setup_complete, [&app, setup_screen]() {
            setup_screen->hide();
            setup_screen->deleteLater();

            auto* window = new fincept::MainWindow;
            window->show();
            LOG_INFO("App", "Application ready (after setup)");
        });

        return app.exec();
    }

    // Python already set up — launch main window directly
    fincept::MainWindow window;
    window.show();

    // If requirements files changed (app update), sync packages in background
    // without blocking the user.
    if (setup_status.needs_package_sync) {
        LOG_INFO("App", "Requirements changed — syncing packages in background");
        fincept::python::PythonSetupManager::instance().run_setup();
    }

    LOG_INFO("App", "Application ready");
    return app.exec();
}
