#include "app/MainWindow.h"
#include "auth/AuthManager.h"
#include "auth/InactivityGuard.h"
#include "auth/PinManager.h"
#include "auth/SessionGuard.h"
#include "core/config/AppConfig.h"
#include "core/config/AppPaths.h"
#include "core/config/ProfileManager.h"
#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
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

#include <SingleApplication.h>
#include <QDir>
#include <QFile>
#include <QLockFile>
#include <QUuid>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QStandardPaths>

#ifdef Q_OS_WIN
#  include <Windows.h>
#endif

int main(int argc, char* argv[]) {
    // ── Parse --profile <name> from argv before Qt initialises ───────────────
    // This must happen first so that:
    //   1. AppPaths returns the correct per-profile directories
    //   2. SingleApplication uses a profile-scoped IPC key so two different
    //      profiles can run simultaneously as independent primary instances
    {
        for (int i = 1; i < argc - 1; ++i) {
            if (qstrcmp(argv[i], "--profile") == 0) {
                fincept::ProfileManager::instance().set_active(
                    QString::fromUtf8(argv[i + 1]));
                break;
            }
        }
        // AppPaths::root() must exist before ensure_all() so ProfileManager can
        // write the manifest. Create root now (single mkdir, idempotent).
        QDir().mkpath(fincept::AppPaths::root());
    }

    // Required before QApplication when any dock panel contains an OpenGL widget
    // (Qt Charts, QOpenGLWidget) — prevents black rendering in floating windows.
    QApplication::setAttribute(Qt::AA_ShareOpenGLContexts);

    // SingleApplication enforces one process per profile.
    // The instance key is scoped to the active profile name, so
    // "FinceptTerminal --profile work" and "FinceptTerminal --profile personal"
    // are treated as two separate primary instances and run simultaneously.
    // allowSecondary=true: secondary instances send "--new-window" and exit.
    const QString profile_key = QString("FinceptTerminal-%1")
                                    .arg(fincept::ProfileManager::instance().active());
    SingleApplication app(argc, argv, /*allowSecondary=*/true,
                          SingleApplication::Mode::User,
                          100,
                          profile_key.toUtf8());
    app.setApplicationName("FinceptTerminal");
    app.setOrganizationName("Fincept");
    app.setApplicationVersion("4.0.0");

    // ── Secondary instance: signal primary to open a new window, then exit ───
    // The primary receives receivedMessage() and calls open_new_window().
    if (app.isSecondary()) {
#ifdef Q_OS_WIN
        // Grant the primary process permission to bring its new window to the
        // foreground — Windows blocks focus-steal without this.
        AllowSetForegroundWindow(static_cast<DWORD>(app.primaryPid()));
#endif
        app.sendMessage(QByteArray("--new-window"));
        return 0;
    }

    // ── Primary instance from here on ────────────────────────────────────────

    // Create all application directories under %LOCALAPPDATA%\com.fincept.terminal\
    fincept::AppPaths::ensure_all();

    // ── One-time migration from legacy %APPDATA% location ─────────────────
    // Current locations (under %LOCALAPPDATA%\com.fincept.terminal\):
    //   Log: <root>/logs/fincept.log    DB: <root>/data/fincept.db
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

    // ── WAL/SHM cleanup — gated behind QLockFile ────────────────────────────
    // Deleting WAL/SHM files while another process has the DB open corrupts it.
    // We only clean up orphaned files from a previous crash when we are the sole
    // running instance. The lock file is held for the entire process lifetime.
    QLockFile db_lock(fincept::AppPaths::data() + "/fincept.db.lock");
    db_lock.setStaleLockTime(0); // never auto-expire; we control the lifecycle
    const bool sole_instance = db_lock.tryLock(0);
    if (sole_instance) {
        QFile::remove(fincept::AppPaths::data() + "/fincept.db-wal");
        QFile::remove(fincept::AppPaths::data() + "/fincept.db-shm");
        QFile::remove(fincept::AppPaths::data() + "/cache.db-wal");
        QFile::remove(fincept::AppPaths::data() + "/cache.db-shm");
    }
    // db_lock remains held (stack-allocated) until main() returns.
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
    fincept::register_migration_v014();
    fincept::register_migration_v015();

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

    // Assign a unique session ID so ScreenStateManager can tag each state write.
    // This lets us distinguish cross-session restores from same-session saves.
    {
        const QString sid = QUuid::createUuid().toString(QUuid::WithoutBraces);
        fincept::ScreenStateManager::instance().set_session_id(sid);
        LOG_INFO("App", "Session ID: " + sid);
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

    // Initialize PinManager — loads PIN state from SecureStorage
    (void)fincept::auth::PinManager::instance();

    // Inactivity guard — auto-lock after idle timeout.
    // Load timeout setting from DB (default 10 minutes).
    {
        auto& guard = fincept::auth::InactivityGuard::instance();
        auto timeout_r = fincept::SettingsRepository::instance().get("security.lock_timeout_minutes");
        if (timeout_r.is_ok() && !timeout_r.value().isEmpty()) {
            int minutes = timeout_r.value().toInt();
            if (minutes > 0)
                guard.set_timeout_minutes(minutes);
        }
        // Guard is installed on qApp and enabled by MainWindow::on_terminal_unlocked()
        // after the user successfully enters their PIN.
    }

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

            auto* window = new fincept::MainWindow(0); // primary window
            window->setAttribute(Qt::WA_DeleteOnClose);
            window->show();

            // Wire new-window handler now that the primary window exists
            QObject::connect(&app, &SingleApplication::receivedMessage,
                [](quint32 /*instanceId*/, QByteArray /*message*/) {
                    auto* w = new fincept::MainWindow(fincept::MainWindow::next_window_id());
                    w->setAttribute(Qt::WA_DeleteOnClose);
                    w->show();
                    w->raise();
                    w->activateWindow();
                    LOG_INFO("App", "New window opened via secondary instance request");
                });
            QObject::connect(&app, &QApplication::lastWindowClosed,
                &app, &QApplication::quit);

            if (!fincept::ai_chat::LlmService::instance().is_configured())
                LOG_WARN("App", "LLM provider not configured — AI chat will prompt user to configure Settings → LLM Config");
            LOG_INFO("App", "Application ready (after setup)");
        });

        return app.exec();
    }

    // Python already set up — launch main window directly
    fincept::MainWindow window;
    window.show();

    // ── New-window handler: fires when the user re-launches the exe ──────────
    // The secondary instance sends "--new-window" and exits. We construct a new
    // independent MainWindow in this process. WA_DeleteOnClose ensures cleanup.
    // QApplication::lastWindowClosed() → quit() handles the final exit.
    QObject::connect(&app, &SingleApplication::receivedMessage,
        [](quint32 /*instanceId*/, QByteArray /*message*/) {
            auto* w = new fincept::MainWindow(fincept::MainWindow::next_window_id());
            w->setAttribute(Qt::WA_DeleteOnClose);
            w->show();
            w->raise();
            w->activateWindow();
            LOG_INFO("App", "New window opened via secondary instance request");
        });

    // Quit when the last window is closed (standard Qt SDI behaviour).
    QObject::connect(&app, &QApplication::lastWindowClosed,
        &app, &QApplication::quit);

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
