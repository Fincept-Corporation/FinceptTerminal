#include "app/PhaseOneClientStartup.h"

#include "app/PhaseOneMigrationRegistry.h"
#include "app/TerminalShell.h"
#include "app/PhaseOneStartupCoordinator.h"
#include "auth/AuthManager.h"
#include "core/components/ComponentCatalog.h"
#include "core/config/AppConfig.h"
#include "core/config/AppPaths.h"
#include "core/config/ProfileManager.h"
#include "core/i18n/LanguageManager.h"
#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "core/session/SessionManager.h"
#include "core/symbol/SymbolGroup.h"
#include "core/symbol/SymbolRef.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "datahub/TopicPolicy.h"
#include "mcp/McpInit.h"
#include "network/http/HttpClient.h"
#include "python/PythonSetupManager.h"
#include "services/agents/AgentService.h"
#include "services/alpha_arena/AlphaArenaEngine.h"
#include "services/billing/FeeDiscountService.h"
#include "services/billing/TierService.h"
#include "services/dbnomics/DBnomicsService.h"
#include "services/economics/EconomicsService.h"
#include "services/economics/MacroCalendarService.h"
#include "services/forum/ForumService.h"
#include "services/geopolitics/GeopoliticsService.h"
#include "services/gov_data/GovDataService.h"
#include "services/ma_analytics/MAAnalyticsService.h"
#include "services/maritime/MaritimeService.h"
#include "services/maritime/PortsCatalog.h"
#include "services/markets/MarketDataService.h"
#include "services/news/NewsService.h"
#include "services/options/FiiDiiService.h"
#include "services/options/OISnapshotter.h"
#include "services/options/OptionChainService.h"
#include "services/polymarket/PolymarketWebSocket.h"
#include "services/prediction/PredictionCredentialStore.h"
#include "services/prediction/PredictionExchangeRegistry.h"
#include "services/prediction/fincept_internal/FinceptInternalAdapter.h"
#include "services/prediction/kalshi/KalshiAdapter.h"
#include "services/prediction/polymarket/PolymarketAdapter.h"
#include "services/relationship_map/RelationshipMapService.h"
#include "services/report_builder/ReportBuilderService.h"
#include "services/wallet/BuybackBurnService.h"
#include "services/wallet/RealYieldService.h"
#include "services/wallet/StakingService.h"
#include "services/wallet/TokenMetadataService.h"
#include "services/wallet/TreasuryService.h"
#include "services/wallet/WalletService.h"
#include "storage/repositories/NewsArticleRepository.h"
#include "storage/repositories/SettingsRepository.h"
#include "storage/sqlite/CacheDatabase.h"
#include "storage/sqlite/Database.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"
#include "trading/DataStreamManager.h"
#include "trading/ExchangeSessionManager.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QSslSocket>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTimer>
#include <QUuid>

#include <cstdio>
#include <memory>

namespace fincept {

namespace {

void initialize_datahub_and_services(QApplication& app) {
    TerminalShell::instance().initialise();
    QObject::connect(&app, &QCoreApplication::aboutToQuit,
                     []() { TerminalShell::instance().shutdown(); });

    datahub::register_metatypes();
    qRegisterMetaType<SymbolRef>("fincept::SymbolRef");
    qRegisterMetaType<SymbolGroup>("fincept::SymbolGroup");

    ComponentCatalog::instance().load_with_fallbacks({
        QCoreApplication::applicationDirPath() + QStringLiteral("/resources/component_catalog.json"),
        QCoreApplication::applicationDirPath() + QStringLiteral("/component_catalog.json"),
        QStringLiteral("resources/component_catalog.json"),
    });
    services::MarketDataService::instance().ensure_registered_with_hub();

    services::NewsService::instance().ensure_registered_with_hub();
    services::EconomicsService::instance().ensure_registered_with_hub();
    services::MacroCalendarService::instance().ensure_registered_with_hub();
    trading::DataStreamManager::instance().ensure_registered_with_hub();
    services::geo::GeopoliticsService::instance().ensure_registered_with_hub();
    services::maritime::MaritimeService::instance().ensure_registered_with_hub();
    services::maritime::PortsCatalog::instance().ensure_registered_with_hub();
    services::RelationshipMapService::instance().ensure_registered_with_hub();
    services::ma::MAAnalyticsService::instance().ensure_registered_with_hub();
    wallet::TokenMetadataService::instance().load_from_storage();

    QTimer::singleShot(0, &app, []() {
        auto& hub = datahub::DataHub::instance();
        QStringList topics;

        const auto add_quotes = [&topics](const QStringList& symbols) {
            topics.reserve(topics.size() + symbols.size());
            for (const auto& symbol : symbols)
                topics.append(QStringLiteral("market:quote:") + symbol);
        };

        add_quotes(services::MarketDataService::indices_symbols());
        add_quotes(services::MarketDataService::forex_symbols());
        add_quotes(services::MarketDataService::crypto_symbols());
        add_quotes(services::MarketDataService::commodity_symbols());
        add_quotes({QStringLiteral("^GSPC"), QStringLiteral("^IXIC"), QStringLiteral("^DJI"),
                    QStringLiteral("^RUT"), QStringLiteral("^VIX"), QStringLiteral("GC=F")});
        add_quotes({QStringLiteral("^VIX"), QStringLiteral("SPY"), QStringLiteral("QQQ"),
                    QStringLiteral("IWM"), QStringLiteral("TLT"), QStringLiteral("NVDA"),
                    QStringLiteral("TSLA"), QStringLiteral("AMD"), QStringLiteral("META"),
                    QStringLiteral("PLTR"), QStringLiteral("COIN")});
        add_quotes({QStringLiteral("AAPL"), QStringLiteral("MSFT"), QStringLiteral("GOOGL"),
                    QStringLiteral("AMZN"), QStringLiteral("NVDA"), QStringLiteral("TSLA"),
                    QStringLiteral("META"), QStringLiteral("JPM")});

        topics.append(QStringLiteral("news:general"));
        topics.append(QStringLiteral("econ:fincept:upcoming_events"));
        topics.removeDuplicates();

        hub.request(topics, true);
        LOG_INFO("App", QString("Pre-warmed %1 dashboard topics during login screen")
                            .arg(topics.size()));
    });

    QTimer::singleShot(0, &app, []() {
        services::options::OptionChainService::instance().ensure_registered_with_hub();
        services::options::OISnapshotter::instance().ensure_registered_with_hub();
        services::options::FiiDiiService::instance().ensure_registered_with_hub();
        trading::ExchangeSessionManager::instance().ensure_registered_with_hub();
        services::polymarket::PolymarketWebSocket::instance().ensure_registered_with_hub();
        services::alpha_arena::AlphaArenaEngine::instance().init();
        {
            auto& registry = services::prediction::PredictionExchangeRegistry::instance();
            registry.register_adapter(
                std::make_unique<services::prediction::polymarket_ns::PolymarketAdapter>());
            registry.register_adapter(
                std::make_unique<services::prediction::kalshi_ns::KalshiAdapter>());
            registry.register_adapter(
                std::make_unique<services::prediction::fincept_internal::FinceptInternalAdapter>());

            if (auto* polymarket = dynamic_cast<services::prediction::polymarket_ns::PolymarketAdapter*>(
                    registry.adapter(QStringLiteral("polymarket")))) {
                polymarket->reload_credentials();
            }
            if (auto* kalshi = dynamic_cast<services::prediction::kalshi_ns::KalshiAdapter*>(
                    registry.adapter(QStringLiteral("kalshi")))) {
                if (const auto credentials = services::prediction::PredictionCredentialStore::load_kalshi())
                    kalshi->set_credentials(*credentials);
            }
            if (auto* fincept_adapter = registry.adapter(QStringLiteral("fincept")))
                fincept_adapter->ensure_registered_with_hub();
        }

        services::DBnomicsService::instance().ensure_registered_with_hub();
        services::GovDataService::instance().ensure_registered_with_hub();
        services::AgentService::instance().ensure_registered_with_hub();
        wallet::TokenMetadataService::instance().refresh_from_jupiter_async();
        wallet::WalletService::instance().ensure_registered_with_hub();
        wallet::WalletService::instance().restore_from_storage();

        {
            static billing::FeeDiscountService discount_service;
            auto& hub = datahub::DataHub::instance();
            hub.register_producer(&discount_service);
            datahub::TopicPolicy policy;
            policy.ttl_ms = 60 * 1000;
            policy.min_interval_ms = 15 * 1000;
            hub.set_policy_pattern(QStringLiteral("billing:fncpt_discount:*"), policy);
        }

        {
            static wallet::BuybackBurnService buyback_burn_service;
            static wallet::TreasuryService treasury_service;
            auto& hub = datahub::DataHub::instance();
            hub.register_producer(&buyback_burn_service);
            hub.register_producer(&treasury_service);

            datahub::TopicPolicy epoch_policy;
            epoch_policy.ttl_ms = 60 * 1000;
            epoch_policy.min_interval_ms = 30 * 1000;
            hub.set_policy_pattern(QStringLiteral("treasury:buyback_epoch"), epoch_policy);

            datahub::TopicPolicy burn_policy;
            burn_policy.ttl_ms = 5 * 60 * 1000;
            burn_policy.min_interval_ms = 60 * 1000;
            hub.set_policy_pattern(QStringLiteral("treasury:burn_total"), burn_policy);

            datahub::TopicPolicy supply_policy;
            supply_policy.ttl_ms = 60 * 60 * 1000;
            supply_policy.min_interval_ms = 5 * 60 * 1000;
            hub.set_policy_pattern(QStringLiteral("treasury:supply_history"), supply_policy);

            datahub::TopicPolicy reserves_policy;
            reserves_policy.ttl_ms = 5 * 60 * 1000;
            reserves_policy.min_interval_ms = 60 * 1000;
            hub.set_policy_pattern(QStringLiteral("treasury:reserves"), reserves_policy);

            datahub::TopicPolicy runway_policy;
            runway_policy.ttl_ms = 5 * 60 * 1000;
            runway_policy.min_interval_ms = 60 * 1000;
            hub.set_policy_pattern(QStringLiteral("treasury:runway"), runway_policy);
        }

        {
            auto& hub = datahub::DataHub::instance();
            hub.register_producer(&wallet::StakingService::instance());
            hub.register_producer(&wallet::RealYieldService::instance());
            hub.register_producer(&billing::TierService::instance());

            datahub::TopicPolicy locks_policy;
            locks_policy.ttl_ms = 60 * 1000;
            locks_policy.min_interval_ms = 30 * 1000;
            hub.set_policy_pattern(QStringLiteral("wallet:locks:*"), locks_policy);
            hub.set_policy_pattern(QStringLiteral("wallet:vefncpt:*"), locks_policy);

            datahub::TopicPolicy yield_policy;
            yield_policy.ttl_ms = 5 * 60 * 1000;
            yield_policy.min_interval_ms = 60 * 1000;
            hub.set_policy_pattern(QStringLiteral("wallet:yield:*"), yield_policy);

            datahub::TopicPolicy revenue_policy;
            revenue_policy.ttl_ms = 60 * 60 * 1000;
            revenue_policy.min_interval_ms = 5 * 60 * 1000;
            hub.set_policy_pattern(QStringLiteral("treasury:revenue"), revenue_policy);

            datahub::TopicPolicy tier_policy;
            tier_policy.ttl_ms = 60 * 1000;
            tier_policy.min_interval_ms = 15 * 1000;
            hub.set_policy_pattern(QStringLiteral("billing:tier:*"), tier_policy);
        }

        LOG_INFO("App", "Deferred service init complete");
    });
}

void initialize_directories_and_legacy_files() {
    AppPaths::ensure_all();

    {
        const QString old_base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        const auto migrate_file = [](const QString& old_path, const QString& new_path) {
            if (QFile::exists(old_path) && !QFile::exists(new_path))
                QFile::rename(old_path, new_path);
        };
        migrate_file(old_base + QStringLiteral("/fincept.db"), AppPaths::data() + QStringLiteral("/fincept.db"));
        migrate_file(old_base + QStringLiteral("/cache.db"), AppPaths::data() + QStringLiteral("/cache.db"));
        migrate_file(old_base + QStringLiteral("/fincept.log"), AppPaths::logs() + QStringLiteral("/fincept.log"));
        migrate_file(old_base + QStringLiteral("/fincept-files"), AppPaths::files());
        QFile::remove(old_base + QStringLiteral("/fincept.db-wal"));
        QFile::remove(old_base + QStringLiteral("/fincept.db-shm"));
        QFile::remove(old_base + QStringLiteral("/cache.db-wal"));
        QFile::remove(old_base + QStringLiteral("/cache.db-shm"));
    }

    {
        const QString local_dir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
        const QString legacy1 = local_dir.section('/', 0, -3) + QStringLiteral("/FinceptTerminal/fincept_settings.db");
        const QString legacy2 =
            QString(local_dir).replace(QStringLiteral("Fincept/FinceptTerminal"), QStringLiteral("FinceptTerminal"))
            + QStringLiteral("/fincept_settings.db");
        QFile::remove(legacy1 + QStringLiteral("-wal"));
        QFile::remove(legacy1 + QStringLiteral("-shm"));
        QFile::remove(legacy2 + QStringLiteral("-wal"));
        QFile::remove(legacy2 + QStringLiteral("-shm"));
    }
}

void initialize_logging_config() {
    auto& log = Logger::instance();
    auto& config = AppConfig::instance();

    const QString global_level = config.get(QStringLiteral("log/global_level"), QStringLiteral("Info")).toString();
    const QHash<QString, LogLevel> level_map = {{QStringLiteral("Trace"), LogLevel::Trace},
                                                {QStringLiteral("Debug"), LogLevel::Debug},
                                                {QStringLiteral("Info"), LogLevel::Info},
                                                {QStringLiteral("Warn"), LogLevel::Warn},
                                                {QStringLiteral("Error"), LogLevel::Error},
                                                {QStringLiteral("Fatal"), LogLevel::Fatal}};
    log.set_level(level_map.value(global_level, LogLevel::Info));
    log.set_json_mode(config.get(QStringLiteral("log/json_mode"), false).toBool());

    const int count = config.get(QStringLiteral("log/tag_count"), 0).toInt();
    for (int i = 0; i < count; ++i) {
        const QString tag = config.get(QStringLiteral("log/tag_%1_name").arg(i)).toString();
        const QString level = config.get(QStringLiteral("log/tag_%1_level").arg(i)).toString();
        if (!tag.isEmpty() && level_map.contains(level))
            log.set_tag_level(tag, level_map.value(level));
    }
}

void initialize_config_and_databases() {
    auto& config = AppConfig::instance();
    HttpClient::instance().set_base_url(config.api_base_url());

    PhaseOneMigrationRegistry::register_all();

    const QString db_path = AppPaths::data() + QStringLiteral("/fincept.db");
    const auto db_result = Database::instance().open(db_path);
    if (db_result.is_err()) {
        LOG_ERROR("App", QStringLiteral("Failed to open database: ") + QString::fromStdString(db_result.error()));
        ui::apply_global_stylesheet();
    } else {
        const int64_t news_cutoff = QDateTime::currentSecsSinceEpoch() - (30LL * 86400);
        QTimer::singleShot(0, [news_cutoff]() {
            NewsArticleRepository::instance().prune_older_than(news_cutoff);
            LOG_INFO("App", "News articles pruned (keeping 30 days)");
        });

        auto& repo = SettingsRepository::instance();
        auto& theme_manager = ui::ThemeManager::instance();
        const auto font_family_result = repo.get(QStringLiteral("appearance.font_family"));
        const auto font_size_result = repo.get(QStringLiteral("appearance.font_size"));
        const QString family = font_family_result.is_ok() ? font_family_result.value() : QStringLiteral("Consolas");
        const QString size_string = font_size_result.is_ok() ? font_size_result.value() : QStringLiteral("14px");
        int size_px = size_string.left(size_string.indexOf(QStringLiteral("px"))).toInt();
        if (size_px <= 0)
            size_px = 14;
        theme_manager.apply_font(family, size_px);
        theme_manager.apply_theme(QStringLiteral("Obsidian"));
        LOG_INFO("App", QStringLiteral("Theme: Obsidian, font: ") + family + QStringLiteral(" ") + size_string);

        i18n::LanguageManager::instance().initialize();
    }

    const QString cache_path = AppPaths::data() + QStringLiteral("/cache.db");
    const auto cache_result = CacheDatabase::instance().open(cache_path);
    if (cache_result.is_err()) {
        LOG_WARN("App", QStringLiteral("Cache DB failed (non-fatal): ")
                             + QString::fromStdString(cache_result.error()));
    }

    const QString session_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    ScreenStateManager::instance().set_session_id(session_id);
    LOG_INFO("App", QStringLiteral("Session ID: ") + session_id);

    LOG_INFO("App", "Checking settings for legacy migration...");
    {
        LOG_INFO("App", "Querying settings...");
        const auto existing = SettingsRepository::instance().get(QStringLiteral("fincept_session"));
        LOG_INFO("App", "Settings query done");
        const bool new_db_empty = existing.is_err() || existing.value().isEmpty();
        if (new_db_empty) {
            QString local_base = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
            QString old_db_path = local_base.section('/', 0, -3) + QStringLiteral("/FinceptTerminal/fincept_settings.db");
            if (!QFile::exists(old_db_path)) {
                old_db_path = local_base.replace(QStringLiteral("Fincept/FinceptTerminal"),
                                                 QStringLiteral("FinceptTerminal"))
                              + QStringLiteral("/fincept_settings.db");
            }
            if (QFile::exists(old_db_path)) {
                QSqlDatabase old_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                                                QStringLiteral("legacy_migration"));
                old_db.setDatabaseName(old_db_path);
                if (old_db.open()) {
                    QSqlQuery source(old_db);
                    if (source.exec(QStringLiteral("SELECT key, value FROM settings"))) {
                        int count = 0;
                        while (source.next()) {
                            SettingsRepository::instance().set(source.value(0).toString(),
                                                               source.value(1).toString(),
                                                               QStringLiteral("migrated"));
                            ++count;
                        }
                        LOG_INFO("App", QStringLiteral("Migrated %1 settings from legacy DB").arg(count));
                    }
                    old_db.close();
                }
                QSqlDatabase::removeDatabase(QStringLiteral("legacy_migration"));
            }
        }
    }
}

void initialize_auth_and_tools() {
    LOG_INFO("App", "Starting session manager...");
    SessionManager::instance().start_session();
    TerminalShell::instance().bootstrap_auth();

    (void)services::ReportBuilderService::instance();
    (void)services::ForumService::instance();

    mcp::initialize_all_tools();
}

} // namespace

void PhaseOneClientStartup::initialize_pre_app(const PhaseOneServerCliOptions& options) {
    ProfileManager::instance().set_active(options.profile);
    QDir().mkpath(AppPaths::root());
    fprintf(stderr, "[PhaseOneStartup] client profile=%s root=%s\n",
            qUtf8Printable(ProfileManager::instance().active()), qUtf8Printable(AppPaths::root()));
}

PhaseOneClientStartup::Result PhaseOneClientStartup::initialize(
    QApplication& app, const PhaseOneStartupCoordinator& coordinator) const {
    initialize_datahub_and_services(app);
    initialize_directories_and_legacy_files();
    coordinator.configure_client_logging();
    initialize_logging_config();

    LOG_INFO("App", "Fincept Terminal v4.0.3 starting...");
    LOG_INFO("App", QStringLiteral("TLS backend: %1 (available: %2)")
                        .arg(QSslSocket::activeBackend(), QSslSocket::availableBackends().join(QStringLiteral(", "))));

    initialize_config_and_databases();
    initialize_auth_and_tools();

    Result result;
    result.setup_status = python::PythonSetupManager::instance().check_status();
    return result;
}

void PhaseOneClientStartup::schedule_agent_discovery(QApplication& app) const {
    QTimer::singleShot(0, &app, []() {
        services::AgentService::instance().discover_agents();
    });
}

} // namespace fincept
