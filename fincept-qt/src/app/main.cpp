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
#include "services/geopolitics/GeopoliticsService.h"
#include "services/gov_data/GovDataService.h"
#include "services/ma_analytics/MAAnalyticsService.h"
#include "services/maritime/MaritimeService.h"
#include "services/maritime/PortsCatalog.h"
#include "services/markets/MarketDataService.h"
#include "services/news/NewsService.h"
#include "services/polymarket/PolymarketWebSocket.h"
#include "services/prediction/PredictionCredentialStore.h"
#include "services/prediction/PredictionExchangeRegistry.h"
#include "services/prediction/fincept_internal/FinceptInternalAdapter.h"
#include "services/prediction/kalshi/KalshiAdapter.h"
#include "services/prediction/polymarket/PolymarketAdapter.h"
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
#include <QLockFile>
#include <QPointer>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTimer>
#include <QUuid>

#include <memory>

#include <singleapplication.h>

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

int main(int argc, char* argv[]) {
    FT_MARK(1);
    // ── Parse --profile <name> from argv before Qt initialises ───────────────
    // This must happen first so that:
    //   1. AppPaths returns the correct per-profile directories
    //   2. SingleApplication uses a profile-scoped IPC key so two different
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

    // ── chdir to a writable, app-stable directory ──────────────────────────
    // SingleApplication (below) calls QSharedMemory::setNativeKey(<hash>,
    // QNativeIpcKey::Type::SystemV). On Unix, Qt's SystemV path resolves a
    // bare key as a CWD-relative filename for ftok(), then must create it.
    // Launching from Finder (cwd=/) or any non-writable cwd → "unable to
    // make key" → SingleApplication::abortSafely() → ::exit(EXIT_FAILURE)
    // before main() can recover. AppPaths::root() was just mkpath'd above,
    // so it always exists and is writable. The Qt token file
    // (Base64-named) ends up here instead of polluting cwd/tmp.
    QDir::setCurrent(fincept::AppPaths::root());

    // Install the unhandled-exception filter BEFORE any Qt object is
    // constructed. On Windows this writes a minidump to AppPaths::crashdumps()
    // when the process dies from an access violation, stack overflow, or GS
    // cookie check failure (STATUS_STACK_BUFFER_OVERRUN — see issue #215).
    // Do it early so a crash in Qt's own startup still produces a dump.
    fincept::crash::install();

    // Required before QApplication when any dock panel contains an OpenGL widget
    // (Qt Charts, QOpenGLWidget) — prevents black rendering in floating windows.
    QApplication::setAttribute(Qt::AA_ShareOpenGLContexts);

    // SingleApplication enforces one process per profile.
    // The instance key is scoped to the active profile name, so
    // "FinceptTerminal --profile work" and "FinceptTerminal --profile personal"
    // are treated as two separate primary instances and run simultaneously.
    // allowSecondary=true: secondary instances send "--new-window" and exit.
    const QString profile_key = QString("FinceptTerminal-%1").arg(fincept::ProfileManager::instance().active());
    SingleApplication app(argc, argv, /*allowSecondary=*/true, SingleApplication::Mode::User, 100,
                          profile_key.toUtf8());
    app.setApplicationName("FinceptTerminal");
    app.setOrganizationName("Fincept");
#ifndef FINCEPT_VERSION_STRING
#    define FINCEPT_VERSION_STRING "0.0.0-dev"
#endif
    app.setApplicationVersion(QStringLiteral(FINCEPT_VERSION_STRING));

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
    // Phase 2 (multi-broker refactor): ExchangeSessionManager is the hub
    // producer for `ws:kraken:*` / `ws:hyperliquid:*`. Individual sessions
    // (created lazily by the manager) publish through its SessionPublisher
    // seam. ExchangeService keeps a back-compat shim but no longer registers
    // itself with the hub.
    fincept::trading::ExchangeSessionManager::instance().ensure_registered_with_hub();
    // Prediction Markets — multi-exchange tab (Polymarket, Kalshi, …).
    // PolymarketWebSocket is the hub producer for `prediction:polymarket:*`;
    // the adapter layer translates exchange-local types into the unified
    // prediction::* data model for the screen.
    fincept::services::polymarket::PolymarketWebSocket::instance().ensure_registered_with_hub();
    {
        auto& reg = fincept::services::prediction::PredictionExchangeRegistry::instance();
        reg.register_adapter(
            std::make_unique<fincept::services::prediction::polymarket_ns::PolymarketAdapter>());
        reg.register_adapter(
            std::make_unique<fincept::services::prediction::kalshi_ns::KalshiAdapter>());
        // Phase 4 §4.3: Fincept internal prediction-market adapter joins the
        // registry as a sibling of Polymarket / Kalshi. Ships in demo mode
        // (curated mock dataset) until `fincept.markets_endpoint` is
        // configured and the `fincept_market` Anchor program is deployed.
        reg.register_adapter(
            std::make_unique<fincept::services::prediction::fincept_internal::FinceptInternalAdapter>());

        // Hydrate credentials from SecureStorage if previously saved. This
        // has to happen after registration so the adapters exist.
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
    // Phase 5: NewsService as the news:general / news:symbol:* /
    // news:category:* / news:cluster:* producer.
    fincept::services::NewsService::instance().ensure_registered_with_hub();
    // Phase 6: Economics + DBnomics + GovData producers.
    fincept::services::EconomicsService::instance().ensure_registered_with_hub();
    fincept::services::DBnomicsService::instance().ensure_registered_with_hub();
    fincept::services::GovDataService::instance().ensure_registered_with_hub();
    // Phase 7: DataStreamManager as the broker:* producer (positions,
    // orders, balance, holdings, quote, ticks). Per-account topic shape
    // `broker:<id>:<account_id>:<channel>`.
    fincept::trading::DataStreamManager::instance().ensure_registered_with_hub();
    // Phase 8: Geopolitics / Maritime / RelationshipMap / MAAnalytics.
    fincept::services::geo::GeopoliticsService::instance().ensure_registered_with_hub();
    fincept::services::maritime::MaritimeService::instance().ensure_registered_with_hub();
    fincept::services::maritime::PortsCatalog::instance().ensure_registered_with_hub();
    fincept::services::RelationshipMapService::instance().ensure_registered_with_hub();
    fincept::services::ma::MAAnalyticsService::instance().ensure_registered_with_hub();
    // Phase 9: AgentService as agent:* push-only producer (output/stream/status/routing/error).
    fincept::services::AgentService::instance().ensure_registered_with_hub();
    // Crypto: WalletService owns wallet:balance:* and market:price:token:*.
    // TokenMetadataService loads its symbol/name cache from SecureStorage
    // first so the very first balance publish has labels for known tokens;
    // a daily background refresh fires in the background after.
    FT_MARK(30);
    fincept::wallet::TokenMetadataService::instance().load_from_storage();
    FT_MARK(31);
    fincept::wallet::TokenMetadataService::instance().refresh_from_jupiter_async();
    FT_MARK(32);
    // Restore_from_storage runs after the hub is up so a soft-connected
    // wallet's balance topic resolves to a registered producer immediately.
    FT_MARK(33);
    fincept::wallet::WalletService::instance().ensure_registered_with_hub();
    FT_MARK(34);
    fincept::wallet::WalletService::instance().restore_from_storage();
    FT_MARK(35);

    // Phase 2 §2C: fee-discount eligibility producer. Lives in billing/
    // because it's consumed by other paid screens later; for Phase 2 it
    // only feeds HoldingsBar's chip and TradeTab's FeeDiscountPanel.
    {
        static fincept::billing::FeeDiscountService discount_service;
        auto& hub = fincept::datahub::DataHub::instance();
        hub.register_producer(&discount_service);
        fincept::datahub::TopicPolicy p;
        // Eligibility is derived from wallet:balance — refresh cadence here
        // is just a fallback; the service also republishes whenever the
        // balance topic emits.
        p.ttl_ms = 60 * 1000;
        p.min_interval_ms = 15 * 1000;
        hub.set_policy_pattern(QStringLiteral("billing:fncpt_discount:*"), p);
    }

    // Phase 5: buyback & burn dashboard producers (terminal-wide treasury:*
    // topics). Both producers ship in mock mode until the buyback worker
    // endpoint is configured via SecureStorage `fincept.treasury_endpoint`
    // and `fincept.treasury_pubkey`.
    {
        static fincept::wallet::BuybackBurnService buyback_burn_service;
        static fincept::wallet::TreasuryService treasury_service;
        auto& hub = fincept::datahub::DataHub::instance();
        hub.register_producer(&buyback_burn_service);
        hub.register_producer(&treasury_service);

        // Per-topic policies. Cadences mirror Phase 5 plan §5.3.
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

    // Phase 3: STAKE tab producers (veFNCPT lock + real-yield + tier system).
    // All three ship in mock mode until SecureStorage `fincept.lock_program_id`
    // and `fincept.yield_endpoint` are configured. The mock payloads carry
    // `is_mock=true` so panels can surface a "not yet deployed" chip.
    {
        auto& hub = fincept::datahub::DataHub::instance();
        hub.register_producer(&fincept::wallet::StakingService::instance());
        hub.register_producer(&fincept::wallet::RealYieldService::instance());
        hub.register_producer(&fincept::billing::TierService::instance());

        // Plan §3.4 cadences.
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

        // billing:tier:* is derived from wallet:vefncpt:* — TTL is just a
        // safety net; the service republishes whenever vefncpt emits.
        fincept::datahub::TopicPolicy tier_p;
        tier_p.ttl_ms = 60 * 1000;
        tier_p.min_interval_ms = 15 * 1000;
        hub.set_policy_pattern(QStringLiteral("billing:tier:*"), tier_p);
    }

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
    LOG_INFO("App", "Fincept Terminal v4.0.2 starting...");

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
        // Guard is installed on qApp and enabled by WindowFrame::on_terminal_unlocked()
        // after the user successfully enters their PIN.
    }

    // Force the ReportBuilderService singleton onto the main thread before
    // MCP tools register — tools route into it via QMetaObject::invokeMethod
    // with BlockingQueuedConnection from worker threads, so the service must
    // already exist with main-thread affinity.
    (void)fincept::services::ReportBuilderService::instance();

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
                         [&app, screen_guard]() {
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

            // Wire new-window handler now that the primary window exists
            QObject::connect(&app, &SingleApplication::receivedMessage,
                             [](quint32 /*instanceId*/, QByteArray /*message*/) {
                                 auto* w = new fincept::WindowFrame(fincept::WindowFrame::next_window_id());
                                 w->setAttribute(Qt::WA_DeleteOnClose);
                                 w->show();
                                 w->raise();
                                 w->activateWindow();
                                 LOG_INFO("App", "New window opened via secondary instance request");
                             });
            // Phase 9 trim: surface Launchpad instead of quitting on last
            // frame close. Closing the Launchpad itself quits.
            QObject::connect(&app, &QApplication::lastWindowClosed, &app, []() {
                fincept::screens::LaunchpadScreen::instance()->surface();
            });

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

    // ── New-window handler: fires when the user re-launches the exe ──────────
    // The secondary instance sends "--new-window" and exits. We construct a new
    // independent WindowFrame in this process. WA_DeleteOnClose ensures cleanup.
    // QApplication::lastWindowClosed() → quit() handles the final exit.
    QObject::connect(&app, &SingleApplication::receivedMessage, [](quint32 /*instanceId*/, QByteArray /*message*/) {
        auto* w = new fincept::WindowFrame(fincept::WindowFrame::next_window_id());
        w->setAttribute(Qt::WA_DeleteOnClose);
        w->show();
        w->raise();
        w->activateWindow();
        LOG_INFO("App", "New window opened via secondary instance request");
    });

    // Phase 9 trim: surface Launchpad instead of quitting on last frame
    // close. The launchpad's own close event quits explicitly.
    QObject::connect(&app, &QApplication::lastWindowClosed, &app, []() {
        fincept::screens::LaunchpadScreen::instance()->surface();
    });

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
