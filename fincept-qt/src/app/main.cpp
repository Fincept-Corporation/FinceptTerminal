#include "ai_chat/LlmService.h"
#include "app/WindowFrame.h"
#include "app/TerminalShell.h"
#include "auth/AuthManager.h"
#include "auth/InactivityGuard.h"
#include "auth/PinManager.h"
#include "auth/SessionGuard.h"
#include "core/config/AppConfig.h"
#include "core/config/AppPaths.h"
#include "core/config/ProfileManager.h"
#include "core/components/ComponentCatalog.h"
#include "core/crash/CrashHandler.h"
#include "core/keys/KeyConfigManager.h"
#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "core/session/SessionManager.h"
#include "core/symbol/SymbolGroup.h"
#include "core/symbol/SymbolRef.h"
#include "datahub/DataHubMetaTypes.h"
#include "mcp/McpInit.h"
#include "network/http/HttpClient.h"
#include "python/PythonSetupManager.h"
#include "screens/launchpad/LaunchpadScreen.h"
#include "screens/recovery/CrashRecoveryDialog.h"
#include "screens/setup/SetupScreen.h"
#include "storage/workspace/CrashRecovery.h"
#include "storage/workspace/WorkspaceSnapshotRing.h"
#include "services/agents/AgentService.h"
#include "services/dbnomics/DBnomicsService.h"
#include "services/economics/EconomicsService.h"
#include "services/economics/MacroCalendarService.h"
#include "services/geopolitics/GeopoliticsService.h"
#include "services/gov_data/GovDataService.h"
#include "services/ma_analytics/MAAnalyticsService.h"
#include "services/maritime/MaritimeService.h"
#include "services/maritime/PortsCatalog.h"
#include "services/markets/MarketDataService.h"
#include "services/options/FiiDiiService.h"
#include "services/options/OISnapshotter.h"
#include "services/options/OptionChainService.h"
#include "services/alpha_arena/AlphaArenaEngine.h"
#include "services/news/NewsService.h"
#include "services/polymarket/PolymarketWebSocket.h"
#include "services/prediction/PredictionCredentialStore.h"
#include "services/prediction/PredictionExchangeRegistry.h"
#include "services/prediction/fincept_internal/FinceptInternalAdapter.h"
#include "services/prediction/kalshi/KalshiAdapter.h"
#include "services/prediction/polymarket/PolymarketAdapter.h"
#include "services/forum/ForumService.h"
#include "services/relationship_map/RelationshipMapService.h"
#include "services/report_builder/ReportBuilderService.h"
#include "datahub/DataHub.h"
#include "datahub/TopicPolicy.h"
#include "services/billing/FeeDiscountService.h"
#include "services/billing/TierService.h"
#include "services/wallet/BuybackBurnService.h"
#include "services/wallet/RealYieldService.h"
#include "services/wallet/StakingService.h"
#include "services/wallet/TokenMetadataService.h"
#include "services/wallet/TreasuryService.h"
#include "services/wallet/WalletService.h"
#include "trading/DataStreamManager.h"
#include "trading/ExchangeService.h"
#include "trading/ExchangeSessionManager.h"
#include "storage/repositories/NewsArticleRepository.h"
#include "storage/repositories/SettingsRepository.h"
#include "storage/sqlite/CacheDatabase.h"
#include "storage/sqlite/Database.h"
#include "storage/sqlite/migrations/MigrationRunner.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QLibrary>
#include <QPointer>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSslSocket>
#include <QStandardPaths>
#include <QTimer>
#include <QUuid>

#include <memory>

#include "app/InstanceLock.h"

#ifdef Q_OS_WIN
#    include <Windows.h>
#endif

// FT_MARK: write a sequence number to a file so we can see how far startup progressed.
// GUI-subsystem apps don't have a useful stderr, so we go straight to disk.
#ifdef Q_OS_WIN
#  define FT_MARK(n) do { char _msg[64]; _snprintf_s(_msg, 64, _TRUNCATE, "FT_MARK %d\n", (n)); OutputDebugStringA(_msg); HANDLE _h = CreateFileA("C:\\Users\\Tilak\\AppData\\Local\\Temp\\ft_marks.txt", FILE_APPEND_DATA, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL); if (_h != INVALID_HANDLE_VALUE) { DWORD _w; SetFilePointer(_h, 0, NULL, FILE_END); WriteFile(_h, _msg, (DWORD)strlen(_msg), &_w, NULL); CloseHandle(_h); } } while(0)
#else
#  define FT_MARK(n) do { fprintf(stderr, "FT_MARK %d\n", (n)); fflush(stderr); } while(0)
#endif

// Wire the two app-level lifecycle handlers that fire after the primary
// window exists: InstanceLock::message_received (a re-launch of the exe
// asks us to open another WindowFrame — args ignored, the request itself is
// the trigger) and QApplication::lastWindowClosed (surface the Launchpad
// instead of quitting; the Launchpad's own close handler quits explicitly).
// Called from both the post-setup-screen path and the no-setup path so the
// two branches stay in sync.
static void wire_app_lifecycle(QApplication& app, fincept::InstanceLock& lock) {
    QObject::connect(&lock, &fincept::InstanceLock::message_received,
                     [](const QStringList& /*args*/) {
                         auto* w = new fincept::WindowFrame(fincept::WindowFrame::next_window_id());
                         w->setAttribute(Qt::WA_DeleteOnClose);
                         w->show();
                         w->raise();
                         w->activateWindow();
                         LOG_INFO("App", "New window opened via secondary instance request");
                     });
    QObject::connect(&app, &QApplication::lastWindowClosed, &app, []() {
        // Settings → General → "On last window close" controls behaviour.
        // Default = "quit" so closing the last window quits the app like
        // every normal desktop app. Power users opt in to the Launchpad.
        const auto r = fincept::SettingsRepository::instance().get(
            QStringLiteral("general.on_last_window_close"), QStringLiteral("quit"));
        const QString choice = r.is_ok() ? r.value() : QStringLiteral("quit");

        if (choice == QStringLiteral("show_launchpad")) {
            fincept::screens::LaunchpadScreen::instance()->surface();
        } else {
            // "quit" or any unknown value → quit (the safe default).
            QCoreApplication::quit();
        }
    });
}

int main(int argc, char* argv[]) {
    FT_MARK(1);

    // ── TLS backend selection (must happen before any Qt plugin loading) ────
    // Force QtNetwork to use the OpenSSL TLS backend across platforms.
    //   - macOS: Apple SecureTransport (qtls_st.cpp) double-frees inside
    //     SSLWrite when QWebSocket sends a close frame after a protocol
    //     error. Reproducible by opening the crypto tab — Kraken/Polymarket
    //     WS reconnect path crashes the main thread. SecureTransport was
    //     deprecated by Apple in 10.15.
    //   - Windows: Schannel has had similar instability around WS close
    //     and certificate revocation paths in Qt 6.8. OpenSSL is the
    //     consistent backend on every platform Qt supports.
    // QT_TLS_BACKEND is read by QTlsBackendFactory at plugin-loader time, so
    // it has to be set BEFORE QCoreApplication/QApplication is constructed —
    // setActiveBackend() after the fact does not always take effect because
    // singleton factories may already be bound.
    qputenv("QT_TLS_BACKEND", "openssl");

#ifdef Q_OS_MACOS
    // Pre-load OpenSSL from Homebrew so the openssl plugin's runtime dlopen
    // succeeds. Qt's QLibrary search defaults don't include /opt/homebrew/...
    // Order matters: libcrypto first (libssl depends on it).
    {
        const QStringList crypto_candidates = {
            QStringLiteral("/opt/homebrew/opt/openssl@3/lib/libcrypto.3.dylib"),
            QStringLiteral("/usr/local/opt/openssl@3/lib/libcrypto.3.dylib"),
        };
        const QStringList ssl_candidates = {
            QStringLiteral("/opt/homebrew/opt/openssl@3/lib/libssl.3.dylib"),
            QStringLiteral("/usr/local/opt/openssl@3/lib/libssl.3.dylib"),
        };
        for (const auto& p : crypto_candidates) {
            if (QFile::exists(p)) { QLibrary(p).load(); break; }
        }
        for (const auto& p : ssl_candidates) {
            if (QFile::exists(p)) { QLibrary(p).load(); break; }
        }
    }
#endif

    // ── Parse --profile <name> from argv before Qt initialises ───────────────
    // This must happen first so that:
    //   1. AppPaths returns the correct per-profile directories
    //   2. InstanceLock uses a profile-scoped IPC key so two different
    //      profiles can run simultaneously as independent primary instances
    {
        for (int i = 1; i < argc - 1; ++i) {
            if (qstrcmp(argv[i], "--profile") == 0) {
                fincept::ProfileManager::instance().set_active(QString::fromUtf8(argv[i + 1]));
                break;
            }
        }
        // AppPaths::root() must exist before ensure_all() so ProfileManager can
        // write the manifest. Create root now (single mkdir, idempotent).
        QDir().mkpath(fincept::AppPaths::root());
    }
    FT_MARK(2);

    // Install the unhandled-exception filter BEFORE any Qt object is
    // constructed. On Windows this writes a minidump to AppPaths::crashdumps()
    // when the process dies from an access violation, stack overflow, or GS
    // cookie check failure (STATUS_STACK_BUFFER_OVERRUN — see issue #215).
    // Do it early so a crash in Qt's own startup still produces a dump.
    fincept::crash::install();

    // Required before QApplication when any dock panel contains an OpenGL widget
    // (Qt Charts, QOpenGLWidget) — prevents black rendering in floating windows.
    QApplication::setAttribute(Qt::AA_ShareOpenGLContexts);

    // QApplication first — InstanceLock's QLocalServer needs an event loop
    // owner. Then the lock probes for an existing primary; if found, it
    // ships our argv to the primary (which opens a new WindowFrame in
    // response) and we exit cleanly.
    //
    // Why not SingleApplication? Its QSharedMemory + QSystemSemaphore lock
    // leaks on macOS Qt 6.6+ (QTBUG-111855), causing silent exits on Finder
    // launch. See InstanceLock.h for the fuller story (issues #234, #252).
    //
    // The instance key is scoped to the active profile name, so
    // "FinceptTerminal --profile work" and "FinceptTerminal --profile personal"
    // run as two independent primaries.
    QApplication app(argc, argv);
    app.setApplicationName("FinceptTerminal");
    app.setOrganizationName("Fincept");
#ifndef FINCEPT_VERSION_STRING
#    define FINCEPT_VERSION_STRING "0.0.0-dev"
#endif
    app.setApplicationVersion(QStringLiteral(FINCEPT_VERSION_STRING));

    // Quit only when LaunchpadScreen::closeEvent calls QCoreApplication::quit().
    // Default Qt behaviour fires lastWindowClosed AND schedules an auto-quit;
    // we connect a slot to surface() the Launchpad on lastWindowClosed, and
    // the auto-quit would race that slot — sometimes killing the app while
    // the launchpad is mid-show. With this off, the only quit path is the
    // explicit one in LaunchpadScreen::closeEvent.
    app.setQuitOnLastWindowClosed(false);

    // Belt-and-braces: if QT_TLS_BACKEND wasn't honoured for some reason
    // (e.g. plugin load order on a particular platform), retry the switch
    // explicitly. This is a no-op if openssl is already active.
    {
        const auto backends = QSslSocket::availableBackends();
        if (QSslSocket::activeBackend() != QStringLiteral("openssl")
            && backends.contains(QStringLiteral("openssl"))) {
            QSslSocket::setActiveBackend(QStringLiteral("openssl"));
        }
    }

    // ── Single-instance lock + new-window IPC ────────────────────────────────
    const QString profile_key = QString("FinceptTerminal-%1").arg(fincept::ProfileManager::instance().active());
    fincept::InstanceLock instance_lock;
    const auto lock_status = instance_lock.acquire(profile_key, QCoreApplication::arguments());

    // ── Secondary instance: argv was already shipped to the primary. Exit. ──
    if (lock_status == fincept::InstanceLock::Status::Secondary) {
#ifdef Q_OS_WIN
        // Grant the primary process permission to bring its new window to
        // the foreground — Windows blocks focus-steal without this. Pre-
        // SingleApplication this used app.primaryPid(); without that we
        // call AllowSetForegroundWindow(ASFW_ANY) which whitelists the
        // whole foreground request from this process.
        AllowSetForegroundWindow(ASFW_ANY);
#endif
        return 0;
    }

    // ── Primary instance from here on ────────────────────────────────────────

    // Bring up the TerminalShell. This is the multi-window refactor's
    // process-level coordinator. Phase 1 ships a skeleton: it bootstraps
    // ProfilePaths, resolves the active ProfileId, and warms the registries
    // (WindowRegistry, ActionRegistry) so WindowFrame constructors don't race
    // their singleton init.
    //
    // Must run BEFORE any service init so future phases that lift services
    // into the shell can rely on it being present.
    FT_MARK(10);
    fincept::TerminalShell::instance().initialise();
    FT_MARK(11);
    QObject::connect(&app, &QCoreApplication::aboutToQuit,
                     []() { fincept::TerminalShell::instance().shutdown(); });
    FT_MARK(12);

    // Register DataHub payload meta-types (QuoteData, HistoryPoint, InfoData,
    // NewsArticle, EconomicsResult) so they can flow through QVariant-keyed
    // topics and cross-thread queued signals. Phase 0 — see
    // fincept-qt/DATAHUB_ARCHITECTURE.md.
    // Phase 2: register MarketDataService as the `market:quote:*` producer.
    fincept::datahub::register_metatypes();
    // SymbolContext payload types — signals cross threads when a producer
    // service (not just UI) publishes a group change.
    qRegisterMetaType<fincept::SymbolRef>("fincept::SymbolRef");
    qRegisterMetaType<fincept::SymbolGroup>("fincept::SymbolGroup");
    // Phase 6: load the Component Browser catalogue. Try the build-side copy
    // first (present after cmake configure copies resources) and fall back to
    // the source-tree path for local dev runs without install step.
    fincept::ComponentCatalog::instance().load_with_fallbacks({
        QCoreApplication::applicationDirPath() + "/resources/component_catalog.json",
        QCoreApplication::applicationDirPath() + "/component_catalog.json",
        "resources/component_catalog.json",
    });
    FT_MARK(20);
    fincept::services::MarketDataService::instance().ensure_registered_with_hub();
    FT_MARK(21);

    // ── Sync services needed by the default dashboard ─────────────────────────
    // Anything a default dashboard widget subscribes to during its first show
    // must be registered with the hub before the window paints. Everything
    // else is deferred to a single QTimer::singleShot(0) below — the event
    // loop runs that batch immediately after the first paint, so cold-start
    // perceived latency drops without changing functional behavior.
    fincept::services::NewsService::instance().ensure_registered_with_hub();
    fincept::services::EconomicsService::instance().ensure_registered_with_hub();
    fincept::services::MacroCalendarService::instance().ensure_registered_with_hub();
    fincept::trading::DataStreamManager::instance().ensure_registered_with_hub();
    fincept::services::geo::GeopoliticsService::instance().ensure_registered_with_hub();
    fincept::services::maritime::MaritimeService::instance().ensure_registered_with_hub();
    fincept::services::maritime::PortsCatalog::instance().ensure_registered_with_hub();
    fincept::services::RelationshipMapService::instance().ensure_registered_with_hub();
    fincept::services::ma::MAAnalyticsService::instance().ensure_registered_with_hub();
    FT_MARK(30);
    fincept::wallet::TokenMetadataService::instance().load_from_storage();
    FT_MARK(35);

    // ── Deferred service init — fires after first window paint ───────────────
    // These services back tab-specific screens (F&O, prediction markets,
    // alpha arena, agents, wallet/treasury/staking, etc.) — none of them is
    // needed for the dashboard's first paint, so registering them here would
    // only add latency to the user-visible cold start. Late registration is
    // safe: the hub's scheduler tick picks up matching subscriptions on the
    // next pass once the producer is registered.
    QTimer::singleShot(0, qApp, []() {
        // F&O / Options chain — `option:chain:*`, `option:tick:*`,
        // `option:atm_iv:*`, `fno:pcr:*`, `fno:max_pain:*`.
        fincept::services::options::OptionChainService::instance().ensure_registered_with_hub();
        // F&O OI snapshotter — subscribes to option:chain:* and persists
        // minute-aligned OI/LTP/Vol/IV rows to SQLite. Producer for
        // oi:history:* (window queries).
        fincept::services::options::OISnapshotter::instance().ensure_registered_with_hub();
        // F&O FII/DII flows — daily NSE cash-market institutional buy/sell.
        fincept::services::options::FiiDiiService::instance().ensure_registered_with_hub();
        // Multi-broker session manager — `ws:kraken:*` / `ws:hyperliquid:*`.
        fincept::trading::ExchangeSessionManager::instance().ensure_registered_with_hub();
        // Prediction Markets — `prediction:polymarket:*`.
        fincept::services::polymarket::PolymarketWebSocket::instance().ensure_registered_with_hub();
        // Alpha Arena engine — TickClock, ModelDispatcher, OrderRouter,
        // PaperVenue. Not a DataHub Producer (callback-style by design).
        // init() is idempotent; pre-resolves crash-recovery state.
        fincept::services::alpha_arena::AlphaArenaEngine::instance().init();
        {
            auto& reg = fincept::services::prediction::PredictionExchangeRegistry::instance();
            reg.register_adapter(
                std::make_unique<fincept::services::prediction::polymarket_ns::PolymarketAdapter>());
            reg.register_adapter(
                std::make_unique<fincept::services::prediction::kalshi_ns::KalshiAdapter>());
            // Fincept internal prediction-market adapter (demo mode until
            // `fincept.markets_endpoint` is configured).
            reg.register_adapter(
                std::make_unique<fincept::services::prediction::fincept_internal::FinceptInternalAdapter>());

            // Hydrate credentials from SecureStorage if previously saved.
            if (auto* pm = dynamic_cast<fincept::services::prediction::polymarket_ns::PolymarketAdapter*>(
                    reg.adapter(QStringLiteral("polymarket")))) {
                pm->reload_credentials();
            }
            if (auto* ks = dynamic_cast<fincept::services::prediction::kalshi_ns::KalshiAdapter*>(
                    reg.adapter(QStringLiteral("kalshi")))) {
                if (auto creds = fincept::services::prediction::PredictionCredentialStore::load_kalshi()) {
                    ks->set_credentials(*creds);
                }
            }
            if (auto* fi = reg.adapter(QStringLiteral("fincept"))) {
                fi->ensure_registered_with_hub();
            }
        }
        // Specialized data sources.
        fincept::services::DBnomicsService::instance().ensure_registered_with_hub();
        fincept::services::GovDataService::instance().ensure_registered_with_hub();
        // Agents — `agent:*` push-only producer.
        fincept::services::AgentService::instance().ensure_registered_with_hub();
        // Token metadata refresh — network call to Jupiter aggregator.
        fincept::wallet::TokenMetadataService::instance().refresh_from_jupiter_async();
        // Wallet — `wallet:balance:*`, `market:price:token:*`.
        fincept::wallet::WalletService::instance().ensure_registered_with_hub();
        fincept::wallet::WalletService::instance().restore_from_storage();

        // Fee-discount eligibility producer (paid screens only).
        {
            static fincept::billing::FeeDiscountService discount_service;
            auto& hub = fincept::datahub::DataHub::instance();
            hub.register_producer(&discount_service);
            fincept::datahub::TopicPolicy p;
            p.ttl_ms = 60 * 1000;
            p.min_interval_ms = 15 * 1000;
            hub.set_policy_pattern(QStringLiteral("billing:fncpt_discount:*"), p);
        }

        // Buyback & burn / treasury producers (treasury:*).
        {
            static fincept::wallet::BuybackBurnService buyback_burn_service;
            static fincept::wallet::TreasuryService treasury_service;
            auto& hub = fincept::datahub::DataHub::instance();
            hub.register_producer(&buyback_burn_service);
            hub.register_producer(&treasury_service);

            fincept::datahub::TopicPolicy epoch_p;
            epoch_p.ttl_ms = 60 * 1000;
            epoch_p.min_interval_ms = 30 * 1000;
            hub.set_policy_pattern(QStringLiteral("treasury:buyback_epoch"), epoch_p);

            fincept::datahub::TopicPolicy burn_p;
            burn_p.ttl_ms = 5 * 60 * 1000;
            burn_p.min_interval_ms = 60 * 1000;
            hub.set_policy_pattern(QStringLiteral("treasury:burn_total"), burn_p);

            fincept::datahub::TopicPolicy supply_p;
            supply_p.ttl_ms = 60 * 60 * 1000;
            supply_p.min_interval_ms = 5 * 60 * 1000;
            hub.set_policy_pattern(QStringLiteral("treasury:supply_history"), supply_p);

            fincept::datahub::TopicPolicy reserves_p;
            reserves_p.ttl_ms = 5 * 60 * 1000;
            reserves_p.min_interval_ms = 60 * 1000;
            hub.set_policy_pattern(QStringLiteral("treasury:reserves"), reserves_p);

            fincept::datahub::TopicPolicy runway_p;
            runway_p.ttl_ms = 5 * 60 * 1000;
            runway_p.min_interval_ms = 60 * 1000;
            hub.set_policy_pattern(QStringLiteral("treasury:runway"), runway_p);
        }

        // STAKE tab producers (veFNCPT lock + real-yield + tier system).
        {
            auto& hub = fincept::datahub::DataHub::instance();
            hub.register_producer(&fincept::wallet::StakingService::instance());
            hub.register_producer(&fincept::wallet::RealYieldService::instance());
            hub.register_producer(&fincept::billing::TierService::instance());

            fincept::datahub::TopicPolicy locks_p;
            locks_p.ttl_ms = 60 * 1000;
            locks_p.min_interval_ms = 30 * 1000;
            hub.set_policy_pattern(QStringLiteral("wallet:locks:*"), locks_p);
            hub.set_policy_pattern(QStringLiteral("wallet:vefncpt:*"), locks_p);

            fincept::datahub::TopicPolicy yield_p;
            yield_p.ttl_ms = 5 * 60 * 1000;
            yield_p.min_interval_ms = 60 * 1000;
            hub.set_policy_pattern(QStringLiteral("wallet:yield:*"), yield_p);

            fincept::datahub::TopicPolicy revenue_p;
            revenue_p.ttl_ms = 60 * 60 * 1000;
            revenue_p.min_interval_ms = 5 * 60 * 1000;
            hub.set_policy_pattern(QStringLiteral("treasury:revenue"), revenue_p);

            // billing:tier:* derived from wallet:vefncpt:* — TTL is just a
            // safety net; the service republishes whenever vefncpt emits.
            fincept::datahub::TopicPolicy tier_p;
            tier_p.ttl_ms = 60 * 1000;
            tier_p.min_interval_ms = 15 * 1000;
            hub.set_policy_pattern(QStringLiteral("billing:tier:*"), tier_p);
        }

        LOG_INFO("App", "Deferred service init complete");
    });

    FT_MARK(40);
    // Create all application directories under %LOCALAPPDATA%/com.fincept.terminal
    fincept::AppPaths::ensure_all();
    FT_MARK(41);

    // ── One-time migration from legacy %APPDATA% location ─────────────────
    // Current locations (under %LOCALAPPDATA%\com.fincept.terminal\):
    //   Log: <root>/logs/fincept.log    DB: <root>/data/fincept.db
    {
        const QString old_base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        const auto migrate_file = [](const QString& old_path, const QString& new_path) {
            if (QFile::exists(old_path) && !QFile::exists(new_path))
                QFile::rename(old_path, new_path);
        };
        migrate_file(old_base + "/fincept.db", fincept::AppPaths::data() + "/fincept.db");
        migrate_file(old_base + "/cache.db", fincept::AppPaths::data() + "/cache.db");
        migrate_file(old_base + "/fincept.log", fincept::AppPaths::logs() + "/fincept.log");
        migrate_file(old_base + "/fincept-files", fincept::AppPaths::files());
        // Remove stale WAL/SHM from old location too
        QFile::remove(old_base + "/fincept.db-wal");
        QFile::remove(old_base + "/fincept.db-shm");
        QFile::remove(old_base + "/cache.db-wal");
        QFile::remove(old_base + "/cache.db-shm");
    }

    // SQLite owns its own .db-wal / .db-shm files. Pre-deleting them is
    // destructive: any committed transaction that has not yet been checkpointed
    // back into the main .db file lives entirely in the WAL. Auto-checkpoint
    // only fires past ~1000 pages, and the on-close checkpoint can fail
    // silently — small writes (e.g. pin_hash on the secure_credentials table)
    // routinely persist only in the WAL across runs. SQLite reads the WAL on
    // open to recover any uncheckpointed or crash-truncated state, so we leave
    // these files for SQLite to manage. Single-instance enforcement is owned
    // by InstanceLock above.

    // Clean legacy v3 DB location (these paths are no longer live DBs)
    {
        const QString local_dir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
        const QString legacy1 = local_dir.section('/', 0, -3) + "/FinceptTerminal/fincept_settings.db";
        const QString legacy2 =
            QString(local_dir).replace("Fincept/FinceptTerminal", "FinceptTerminal") + "/fincept_settings.db";
        QFile::remove(legacy1 + "-wal");
        QFile::remove(legacy1 + "-shm");
        QFile::remove(legacy2 + "-wal");
        QFile::remove(legacy2 + "-shm");
    }

    FT_MARK(50);
    fincept::Logger::instance().set_file(fincept::AppPaths::logs() + "/fincept.log");
    FT_MARK(51);

    // P3.18 — route Qt's own qDebug/qWarning/qCritical messages into our log
    // file so framework/3rd-party warnings are visible in Release builds.
    qInstallMessageHandler([](QtMsgType type, const QMessageLogContext& ctx, const QString& msg) {
        const char* category = (ctx.category && *ctx.category) ? ctx.category : "Qt";
        switch (type) {
        case QtDebugMsg:    fincept::Logger::instance().debug(category, msg); break;
        case QtInfoMsg:     fincept::Logger::instance().info(category, msg); break;
        case QtWarningMsg:  fincept::Logger::instance().warn(category, msg); break;
        case QtCriticalMsg: fincept::Logger::instance().error(category, msg); break;
        case QtFatalMsg:
            fincept::Logger::instance().error(category, msg);
            fincept::Logger::instance().flush_and_close();
            break;
        }
    });
    {
        auto& log = fincept::Logger::instance();
        auto& cfg = fincept::AppConfig::instance();

        // Global level
        const QString gl = cfg.get("log/global_level", "Info").toString();
        const QHash<QString, fincept::LogLevel> lvl_map = {{"Trace", fincept::LogLevel::Trace},
                                                           {"Debug", fincept::LogLevel::Debug},
                                                           {"Info", fincept::LogLevel::Info},
                                                           {"Warn", fincept::LogLevel::Warn},
                                                           {"Error", fincept::LogLevel::Error},
                                                           {"Fatal", fincept::LogLevel::Fatal}};
        log.set_level(lvl_map.value(gl, fincept::LogLevel::Info));

        // JSON output mode (persisted in Settings → Logging)
        log.set_json_mode(cfg.get("log/json_mode", false).toBool());

        // Per-tag overrides
        const int count = cfg.get("log/tag_count", 0).toInt();
        for (int i = 0; i < count; ++i) {
            const QString tag = cfg.get(QString("log/tag_%1_name").arg(i)).toString();
            const QString level = cfg.get(QString("log/tag_%1_level").arg(i)).toString();
            if (!tag.isEmpty() && lvl_map.contains(level))
                log.set_tag_level(tag, lvl_map.value(level));
        }
    }
    LOG_INFO("App", "Fincept Terminal v4.0.3 starting...");
    LOG_INFO("App", QString("TLS backend: %1 (available: %2)")
                        .arg(QSslSocket::activeBackend(),
                             QSslSocket::availableBackends().join(", ")));

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
    fincept::register_migration_v016();
    fincept::register_migration_v017();
    fincept::register_migration_v018();
    fincept::register_migration_v019();
    fincept::register_migration_v020();
    fincept::register_migration_v021();
    fincept::register_migration_v022();
    fincept::register_migration_v023();
    fincept::register_migration_v024();
    fincept::register_migration_v025();
    fincept::register_migration_v026();
    fincept::register_migration_v027();
    fincept::register_migration_v028();
    fincept::register_migration_v029();

    // Open main database
    QString db_path = fincept::AppPaths::data() + "/fincept.db";
    auto db_result = fincept::Database::instance().open(db_path);
    if (db_result.is_err()) {
        LOG_ERROR("App", "Failed to open database: " + QString::fromStdString(db_result.error()));
        // DB unavailable — apply theme with built-in defaults so the UI is at least styled
        fincept::ui::apply_global_stylesheet();
    } else {
        // Prune news articles older than 30 days — deferred to run after the event loop
        // starts so the startup critical path is not blocked.
        // NewsArticleRepository uses the main-thread DB connection (not thread-safe),
        // so we must not run this on a worker thread — QTimer::singleShot(0) posts it
        // to the main thread's event queue instead.
        {
            int64_t news_cutoff = QDateTime::currentSecsSinceEpoch() - (30LL * 86400);
            QTimer::singleShot(0, [news_cutoff]() {
                fincept::NewsArticleRepository::instance().prune_older_than(news_cutoff);
                LOG_INFO("App", "News articles pruned (keeping 30 days)");
            });
        }

        // Load persisted font settings and apply before any window is shown
        // — eliminates flash/wrong-font-on-startup. Theme is always Obsidian.
        {
            auto& repo = fincept::SettingsRepository::instance();
            auto& tm = fincept::ui::ThemeManager::instance();
            auto r_family = repo.get("appearance.font_family");
            auto r_size = repo.get("appearance.font_size");
            QString family = r_family.is_ok() ? r_family.value() : "Consolas";
            QString size_s = r_size.is_ok() ? r_size.value() : "14px";
            int size_px = size_s.left(size_s.indexOf("px")).toInt();
            if (size_px <= 0)
                size_px = 14;
            tm.apply_font(family, size_px);
            tm.apply_theme("Obsidian");
            LOG_INFO("App", "Theme: Obsidian, font: " + family + " " + size_s);
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

    // Phase 1 final lift: shell owns auth/lock service initialisation.
    // bootstrap_auth() runs AuthManager::initialize(), warms PinManager
    // from SecureStorage, and configures InactivityGuard's lock timeout
    // from SettingsRepository. The previous in-line block here is folded
    // into TerminalShell::bootstrap_auth.
    fincept::TerminalShell::instance().bootstrap_auth();

    // Session guard — auto-logout on 401. Lives on the stack here so its
    // destructor runs on shutdown via QApplication::exec returning.
    fincept::auth::SessionGuard session_guard;

    // Force the ReportBuilderService singleton onto the main thread before
    // MCP tools register — tools route into it via QMetaObject::invokeMethod
    // with BlockingQueuedConnection from worker threads, so the service must
    // already exist with main-thread affinity.
    (void)fincept::services::ReportBuilderService::instance();

    // Same reason for ForumService: ForumTools (MCP) and ForumScreen (GUI)
    // share one singleton that owns a QNetworkAccessManager. Whichever caller
    // hits instance() first dictates thread affinity. If a worker thread
    // touches it before the GUI does, every subsequent fetch from the Forum
    // tab queues its reply onto a thread with no live event loop and the
    // callback never fires — the tab spins on "loading" forever. Forcing
    // construction here pins it to the main thread up front.
    (void)fincept::services::ForumService::instance();

    // Initialize MCP tool system — registers all internal tools and starts
    // external MCP servers in the background (non-blocking).
    fincept::mcp::initialize_all_tools();

    // ── Python environment check ─────────────────────────────────────────────
    // check_status() fast path (sentinel + markers present) is synchronous and
    // cheap. The slow path (first run) can spawn processes — but at this point
    // no window is visible yet so the brief block is acceptable. The SetupScreen
    // itself offloads prefill_completed_steps() to a background thread (P1).
    auto setup_status = fincept::python::PythonSetupManager::instance().check_status();

    if (setup_status.needs_setup) {
        LOG_INFO("App", "Python environment not ready — showing setup screen");

        // Use QPointer so the setup_complete lambda is safe against double-fire
        // (e.g. user somehow triggers it twice before the window is hidden).
        auto* setup_screen = new fincept::screens::SetupScreen;
        QPointer<fincept::screens::SetupScreen> screen_guard(setup_screen);
        setup_screen->setWindowTitle("Fincept Terminal — First-Time Setup");
        setup_screen->resize(800, 600);
        setup_screen->show();

        // When setup completes, hide setup screen and launch main window.
        // The connection uses Qt::SingleShotConnection (Qt 6.0+) so the lambda
        // fires exactly once even if setup_complete is somehow emitted twice.
        QObject::connect(setup_screen, &fincept::screens::SetupScreen::setup_complete,
                         [&app, &instance_lock, screen_guard]() {
            if (!screen_guard)
                return; // already cleaned up — ignore
            screen_guard->hide();
            screen_guard->deleteLater();

            fincept::KeyConfigManager::instance(); // init before WindowFrame registers shortcuts

            // Phase 6 final: if the previous session ended uncleanly and a
            // workspace snapshot is available, give the user the option to
            // restore. On accept, WorkspaceShell::apply already constructs
            // the frames it needs — we skip our own primary-window creation
            // path. On skip (or no recovery available), fall through.
            bool recovered = false;
            if (auto* recovery = fincept::TerminalShell::instance().crash_recovery();
                recovery && recovery->needs_recovery()) {
                fincept::screens::CrashRecoveryDialog dlg(
                    recovery, fincept::TerminalShell::instance().snapshot_ring());
                dlg.exec();
                recovered = dlg.was_restored();
            }

            if (!recovered) {
                auto* window = new fincept::WindowFrame(0); // primary window
                window->setAttribute(Qt::WA_DeleteOnClose);
                window->show();
            }

            // Restore any secondary windows that were open at last shutdown so
            // multi-monitor layouts survive across relaunches. Each window
            // restores its own geometry + dock layout from SessionManager.
            // Skip when recovered — WorkspaceShell::apply has already built
            // the right frame set.
            if (!recovered)
            {
                const QList<int> saved_ids =
                    fincept::SessionManager::instance().load_window_ids();
                for (int id : saved_ids) {
                    if (id <= 0) continue; // 0 = primary, already created
                    auto* w = new fincept::WindowFrame(id);
                    w->setAttribute(Qt::WA_DeleteOnClose);
                    w->show();
                }
                if (!saved_ids.isEmpty())
                    LOG_INFO("App", QString("Restored %1 secondary window(s) from last session")
                                        .arg(saved_ids.size() > 0 ? saved_ids.size() - 1 : 0));
            }

            // Wire new-window handler + Launchpad surface now that the
            // primary window exists. Single source of truth — see
            // wire_app_lifecycle() at the top of this file.
            wire_app_lifecycle(app, instance_lock);

            if (!fincept::ai_chat::LlmService::instance().is_configured())
                LOG_WARN("App",
                         "LLM provider not configured — AI chat will prompt user to configure Settings → LLM Config");

            // Warm agent discovery cache (same reason as the main path).
            QTimer::singleShot(0, &app, []() {
                fincept::services::AgentService::instance().discover_agents();
            });

            LOG_INFO("App", "Application ready (after setup)");
        });

        return app.exec();
    }

    // Ensure KeyConfigManager is initialized before WindowFrame registers shortcuts
    fincept::KeyConfigManager::instance();

    // Phase 6 final: offer crash recovery before constructing the primary
    // window. If the user accepts and restoration succeeds, WorkspaceShell
    // has already built the frames it needs from the snapshot — we skip
    // both the primary-window creation and the SessionManager-based
    // secondary-window restoration paths to avoid duplicating windows.
    bool recovered = false;
    if (auto* recovery = fincept::TerminalShell::instance().crash_recovery();
        recovery && recovery->needs_recovery()) {
        fincept::screens::CrashRecoveryDialog dlg(
            recovery, fincept::TerminalShell::instance().snapshot_ring());
        dlg.exec();
        recovered = dlg.was_restored();
    }

    // Heap-allocate the primary window so we can skip it on a successful
    // recovery without leaving a dead stack object behind. WA_DeleteOnClose
    // matches the secondary-window lifecycle below.
    if (!recovered) {
        auto* primary = new fincept::WindowFrame(0);
        primary->setAttribute(Qt::WA_DeleteOnClose);
        primary->show();
    }

    // Restore any secondary windows that were open at last shutdown. The
    // primary window owns its own lifetime via WA_DeleteOnClose; restored
    // secondaries use WA_DeleteOnClose and self-remove from
    // QApplication::topLevelWidgets. Skip when recovered — WorkspaceShell
    // has already built the right frame set.
    if (!recovered) {
        const QList<int> saved_ids =
            fincept::SessionManager::instance().load_window_ids();
        for (int id : saved_ids) {
            if (id <= 0) continue; // 0 = primary, already created
            auto* w = new fincept::WindowFrame(id);
            w->setAttribute(Qt::WA_DeleteOnClose);
            w->show();
        }
        if (saved_ids.size() > 1)
            LOG_INFO("App", QString("Restored %1 secondary window(s) from last session")
                                .arg(saved_ids.size() - 1));
    }

    // Wire new-window handler + Launchpad surface — see wire_app_lifecycle()
    // at the top of this file for the contract.
    wire_app_lifecycle(app, instance_lock);

    // If requirements files changed (app update), sync packages in background
    // without blocking the user. Connect setup_complete so failures are logged
    // (no SetupScreen in this path, so we only log — don't show UI).
    if (setup_status.needs_package_sync) {
        LOG_INFO("App", "Requirements changed — syncing packages in background");
        auto& mgr = fincept::python::PythonSetupManager::instance();
        QObject::connect(&mgr, &fincept::python::PythonSetupManager::setup_complete,
                         &mgr, [](bool success, const QString& error) {
            if (success)
                LOG_INFO("App", "Background package sync completed successfully");
            else
                LOG_WARN("App", "Background package sync failed (non-fatal): " + error);
        }, Qt::SingleShotConnection);
        mgr.run_setup();
    }

    if (!fincept::ai_chat::LlmService::instance().is_configured())
        LOG_WARN("App", "LLM provider not configured — AI chat will prompt user to configure Settings → LLM Config");

    // Warm the agent discovery cache on startup. This populates
    // AgentService::cached_agents() so any screen that lists agents
    // (Agent Config, Portfolio → Agent Runner, Node Editor) shows the
    // full finagent_core set immediately instead of falling back to the
    // much smaller DB-only list. Run deferred so Python is fully ready.
    QTimer::singleShot(0, &app, []() {
        fincept::services::AgentService::instance().discover_agents();
    });

    LOG_INFO("App", "Application ready");
    return app.exec();
}
