#include "app/MainWindow.h"
#include "auth/AuthManager.h"
#include "auth/SessionGuard.h"
#include "core/config/AppConfig.h"
#include "core/config/AppPaths.h"
#include "core/logging/Logger.h"
#include "core/session/SessionManager.h"
#include "mcp/McpInit.h"
#include "network/http/HttpClient.h"
#include "python/PythonSetupManager.h"
#include "screens/setup/SetupScreen.h"
#include "storage/repositories/NewsArticleRepository.h"
#include "storage/repositories/SettingsRepository.h"
#include "storage/sqlite/CacheDatabase.h"
#include "storage/sqlite/Database.h"
#include "storage/sqlite/migrations/MigrationRunner.h"
#include "trading/instruments/InstrumentService.h"
#include "ai_chat/LlmService.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

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

    // Create all application directories under %LOCALAPPDATA%\com.fincept.terminal\
    fincept::AppPaths::ensure_all();

    // ── One-time migration: move files from old scattered locations ──────────
    // Old log location: %APPDATA%\Fincept\FinceptTerminal\fincept.log
    // Old DB  location: %APPDATA%\Fincept\FinceptTerminal\fincept.db / cache.db
    {
        const QString old_base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        const auto migrate_file = [](const QString& old_path, const QString& new_path) {
            if (QFile::exists(old_path) && !QFile::exists(new_path))
                QFile::rename(old_path, new_path);
        };
        migrate_file(old_base + "/fincept.db",   fincept::AppPaths::data() + "/fincept.db");
        migrate_file(old_base + "/cache.db",     fincept::AppPaths::data() + "/cache.db");
        migrate_file(old_base + "/fincept.log",  fincept::AppPaths::logs() + "/fincept.log");
        migrate_file(old_base + "/fincept-files", fincept::AppPaths::files());
        // Remove stale WAL/SHM from old location too
        QFile::remove(old_base + "/fincept.db-wal");
        QFile::remove(old_base + "/fincept.db-shm");
        QFile::remove(old_base + "/cache.db-wal");
        QFile::remove(old_base + "/cache.db-shm");
    }

    // Remove stale SQLite WAL/SHM lock files left by a previous crash.
    // qsqlite.dll crashes on WAL recovery if the lock files are orphaned.
    QFile::remove(fincept::AppPaths::data() + "/fincept.db-wal");
    QFile::remove(fincept::AppPaths::data() + "/fincept.db-shm");
    QFile::remove(fincept::AppPaths::data() + "/cache.db-wal");
    QFile::remove(fincept::AppPaths::data() + "/cache.db-shm");
    // Also clean legacy v3 DB location
    {
        const QString local_dir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
        const QString legacy1 = local_dir.section('/', 0, -3) + "/FinceptTerminal/fincept_settings.db";
        const QString legacy2 = QString(local_dir).replace("Fincept/FinceptTerminal", "FinceptTerminal")
                                + "/fincept_settings.db";
        QFile::remove(legacy1 + "-wal");
        QFile::remove(legacy1 + "-shm");
        QFile::remove(legacy2 + "-wal");
        QFile::remove(legacy2 + "-shm");
    }

    fincept::Logger::instance().set_file(fincept::AppPaths::logs() + "/fincept.log");
    {
        auto& log = fincept::Logger::instance();
        auto& cfg = fincept::AppConfig::instance();

        // Global level
        const QString gl = cfg.get("log/global_level", "Info").toString();
        const QHash<QString, fincept::LogLevel> lvl_map = {
            {"Debug", fincept::LogLevel::Debug}, {"Info",  fincept::LogLevel::Info},
            {"Warn",  fincept::LogLevel::Warn},  {"Error", fincept::LogLevel::Error}
        };
        log.set_level(lvl_map.value(gl, fincept::LogLevel::Info));

        // Per-tag overrides
        const int count = cfg.get("log/tag_count", 0).toInt();
        for (int i = 0; i < count; ++i) {
            const QString tag   = cfg.get(QString("log/tag_%1_name").arg(i)).toString();
            const QString level = cfg.get(QString("log/tag_%1_level").arg(i)).toString();
            if (!tag.isEmpty() && lvl_map.contains(level))
                log.set_tag_level(tag, lvl_map.value(level));
        }
    }
    LOG_INFO("App", "Fincept Terminal v4.0.0 starting...");

    // Theme is applied after DB is open so saved font/theme are respected from the start.

    // Initialize config
    auto& config = fincept::AppConfig::instance();
    fincept::HttpClient::instance().set_base_url(config.api_base_url());
    // Note: auth tokens are managed by AuthManager::initialize() which loads
    // from SecureStorage (DPAPI) and SQLite — not from QSettings/Registry.

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
    fincept::register_migration_v012();
    fincept::register_migration_v013();

    // Open main database
    QString db_path = fincept::AppPaths::data() + "/fincept.db";
    auto db_result = fincept::Database::instance().open(db_path);
    if (db_result.is_err()) {
        LOG_ERROR("App", "Failed to open database: " + QString::fromStdString(db_result.error()));
        // DB unavailable — apply theme with built-in defaults so the UI is at least styled
        fincept::ui::apply_global_stylesheet();
    } else {
        // Warm InstrumentService cache from DB (non-blocking, no network).
        // If the table is empty on first run, the first broker login triggers a refresh.
        LOG_INFO("App", "Loading instruments from DB...");
        fincept::trading::InstrumentService::instance().load_from_db("zerodha");
        LOG_INFO("App", "Zerodha instruments loaded");
        fincept::trading::InstrumentService::instance().load_from_db("angelone");
        LOG_INFO("App", "AngelOne instruments loaded");

        // Prune news articles older than 30 days to keep DB size bounded
        int64_t news_cutoff = QDateTime::currentSecsSinceEpoch() - (30LL * 86400);
        fincept::NewsArticleRepository::instance().prune_older_than(news_cutoff);
        LOG_INFO("App", "News articles pruned (keeping 30 days)");

        // Load persisted appearance settings and apply theme+font in one shot,
        // before any window is shown — eliminates the flash/wrong-font-on-startup issue.
        {
            auto& repo    = fincept::SettingsRepository::instance();
            auto& tm      = fincept::ui::ThemeManager::instance();
            auto r_theme  = repo.get("appearance.theme");
            auto r_family = repo.get("appearance.font_family");
            auto r_size   = repo.get("appearance.font_size");
            QString theme  = r_theme.is_ok()  ? r_theme.value()  : "Obsidian";
            QString family = r_family.is_ok() ? r_family.value() : "Consolas";
            QString size_s = r_size.is_ok()   ? r_size.value()   : "14px";
            int size_px    = size_s.left(size_s.indexOf("px")).toInt();
            if (size_px <= 0) size_px = 14;
            // Set font first so rebuild_and_apply inside apply_theme uses correct values
            tm.apply_font(family, size_px);
            tm.apply_theme(theme);
            LOG_INFO("App", "Theme applied: " + theme + ", font: " + family + " " + size_s);
        }
    }


    // Open cache database (non-fatal if fails)
    QString cache_path = fincept::AppPaths::data() + "/cache.db";
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
            if (!fincept::ai_chat::LlmService::instance().is_configured())
                LOG_WARN("App", "LLM provider not configured — AI chat will prompt user to configure Settings → LLM Config");
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

    if (!fincept::ai_chat::LlmService::instance().is_configured())
        LOG_WARN("App", "LLM provider not configured — AI chat will prompt user to configure Settings → LLM Config");
    LOG_INFO("App", "Application ready");
    return app.exec();
}
