cmake_minimum_required(VERSION 3.20)
project(FinceptTerminal VERSION 4.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ── Qt packages ──────────────────────────────────────────────────────────────
find_package(Qt6 REQUIRED COMPONENTS
    Widgets
    Charts
    PrintSupport
    Network
    Sql
    Concurrent
)

# Optional: WebSockets (not always installed)
find_package(Qt6 QUIET COMPONENTS WebSockets)
if(Qt6WebSockets_FOUND)
    set(HAS_WEBSOCKETS TRUE)
endif()

# ── Source files by module ────────────────────────────────────────────────────

# Core
set(CORE_SOURCES
    src/core/config/AppConfig.cpp
    src/core/logging/Logger.cpp
    src/core/events/EventBus.cpp
    src/core/session/SessionManager.cpp
)

# Network
set(NETWORK_SOURCES
    src/network/http/HttpClient.cpp
)
if(HAS_WEBSOCKETS)
    list(APPEND NETWORK_SOURCES src/network/websocket/WebSocketClient.cpp)
endif()

# Auth
set(AUTH_SOURCES
    src/auth/AuthManager.cpp
    src/auth/AuthApi.cpp
    src/auth/UserApi.cpp
    src/auth/SessionGuard.cpp
)

# Storage
set(STORAGE_SOURCES
    # Core databases
    src/storage/sqlite/Database.cpp
    src/storage/sqlite/CacheDatabase.cpp

    # Migration system
    src/storage/sqlite/migrations/MigrationRunner.cpp
    src/storage/sqlite/migrations/v001_initial.cpp
    src/storage/sqlite/migrations/v002_llm_chat.cpp
    src/storage/sqlite/migrations/v003_data_mcp_agents.cpp

    # Cache + tab sessions
    src/storage/cache/CacheManager.cpp
    src/storage/cache/TabSessionStore.cpp

    # Secure storage (unchanged)
    src/storage/secure/SecureStorage.cpp

    # Repositories
    src/storage/repositories/SettingsRepository.cpp
    src/storage/repositories/WatchlistRepository.cpp
    src/storage/repositories/NotesRepository.cpp
    src/storage/repositories/ReportRepository.cpp
    src/storage/repositories/PaperTradingRepository.cpp
    src/storage/repositories/NewsMonitorRepository.cpp
    src/storage/repositories/PortfolioHoldingsRepository.cpp
    src/storage/repositories/ChatRepository.cpp
    src/storage/repositories/LlmConfigRepository.cpp
    src/storage/repositories/DataSourceRepository.cpp
    src/storage/repositories/McpServerRepository.cpp
    src/storage/repositories/AgentConfigRepository.cpp
    src/storage/repositories/ContextRecordingRepository.cpp
)

# Python runner
set(PYTHON_SOURCES
    src/python/PythonRunner.cpp
)

# Trading
set(TRADING_SOURCES
    src/trading/PaperTrading.cpp
    src/trading/OrderMatcher.cpp
    src/trading/BrokerRegistry.cpp
    src/trading/UnifiedTrading.cpp
    src/trading/ExchangeService.cpp
    src/trading/brokers/BrokerHttp.cpp
    src/trading/brokers/FyersBroker.cpp
    src/trading/brokers/IndianBrokers.cpp
    src/trading/brokers/GlobalBrokers.cpp
)

# Services
set(SERVICE_SOURCES
    src/services/markets/MarketDataService.cpp
    src/services/news/NewsService.cpp
    src/services/news/NewsClusterService.cpp
    src/services/news/NewsMonitorService.cpp
    src/services/databento/DatabentoService.cpp
    src/services/dbnomics/DBnomicsService.cpp
)

# UI framework
set(UI_SOURCES
    src/ui/theme/Theme.cpp
    src/ui/theme/StyleSheets.cpp
    src/ui/widgets/Card.cpp
    src/ui/widgets/StatusBadge.cpp
    src/ui/widgets/SearchBar.cpp
    src/ui/widgets/GeometricBackground.cpp
    src/ui/tables/DataTable.cpp
    src/ui/charts/ChartFactory.cpp
    src/ui/navigation/NavigationBar.cpp
    src/ui/navigation/FKeyBar.cpp
    src/ui/navigation/StatusBar.cpp
    src/ui/navigation/ToolBar.cpp
)

# Screens
set(SCREEN_SOURCES
    src/screens/auth/LoginScreen.cpp
    src/screens/auth/RegisterScreen.cpp
    src/screens/auth/ForgotPasswordScreen.cpp
    src/screens/auth/PricingScreen.cpp
    src/screens/profile/ProfileScreen.cpp
    src/screens/settings/SettingsScreen.cpp
    src/screens/about/AboutScreen.cpp
    src/screens/support/SupportScreen.cpp
    src/screens/ComingSoonScreen.cpp
    src/screens/markets/MarketsScreen.cpp
    src/screens/markets/MarketPanel.cpp
    src/screens/dashboard/DashboardScreen.cpp
    src/screens/dashboard/DashboardToolBar.cpp
    src/screens/dashboard/DashboardStatusBar.cpp
    src/screens/dashboard/MarketPulsePanel.cpp
    src/screens/dashboard/WidgetGrid.cpp
    src/screens/dashboard/TickerBar.cpp
    src/screens/dashboard/widgets/BaseWidget.cpp
    src/screens/dashboard/widgets/QuoteTableWidget.cpp
    src/screens/dashboard/widgets/TopMoversWidget.cpp
    src/screens/dashboard/widgets/NewsWidget.cpp
    src/screens/dashboard/widgets/SectorHeatmapWidget.cpp
    src/screens/dashboard/widgets/StockQuoteWidget.cpp
    src/screens/dashboard/widgets/WatchlistWidget.cpp
    src/screens/dashboard/widgets/PerformanceWidget.cpp
    src/screens/dashboard/widgets/MarketSentimentWidget.cpp
    src/screens/dashboard/widgets/EconomicCalendarWidget.cpp
    src/screens/dashboard/widgets/ScreenerWidget.cpp
    src/screens/dashboard/widgets/RiskMetricsWidget.cpp
    src/screens/dashboard/widgets/PortfolioSummaryWidget.cpp
    src/screens/dashboard/widgets/QuickTradeWidget.cpp
    src/screens/report_builder/ReportBuilderScreen.cpp
    src/screens/report_builder/ComponentToolbar.cpp
    src/screens/report_builder/DocumentCanvas.cpp
    src/screens/report_builder/PropertiesPanel.cpp
    src/screens/news/NewsScreen.cpp
    src/screens/news/NewsFilterBar.cpp
    src/screens/news/NewsSidebarPanel.cpp
    src/screens/news/NewsWirePanel.cpp
    src/screens/news/NewsReaderPanel.cpp
    src/screens/news/WireRow.cpp
    src/screens/news/NewsClusterCard.cpp
    src/screens/news/BreakingBanner.cpp
    src/screens/crypto_trading/CryptoTradingScreen.cpp
    src/screens/crypto_trading/CryptoTickerBar.cpp
    src/screens/crypto_trading/CryptoWatchlist.cpp
    src/screens/crypto_trading/CryptoChart.cpp
    src/screens/crypto_trading/CryptoOrderEntry.cpp
    src/screens/crypto_trading/CryptoOrderBook.cpp
    src/screens/crypto_trading/CryptoBottomPanel.cpp
    src/screens/crypto_trading/CryptoCredentials.cpp
    src/screens/notes/NotesScreen.cpp
    src/screens/watchlist/WatchlistScreen.cpp
    src/screens/surface_analytics/SurfaceDemoData.cpp
    src/screens/surface_analytics/SurfaceCsvImporter.cpp
    src/screens/surface_analytics/Surface3DWidget.cpp
    src/screens/surface_analytics/SurfaceTableWidget.cpp
    src/screens/surface_analytics/SurfaceMetricsPanel.cpp
    src/screens/surface_analytics/SurfaceDatabentoPanel.cpp
    src/screens/surface_analytics/SurfaceAnalyticsScreen.cpp
    src/screens/dbnomics/DBnomicsDataTable.cpp
)

# App
set(APP_SOURCES
    src/app/main.cpp
    src/app/MainWindow.cpp
    src/app/ScreenRouter.cpp
)

# ── Executable ────────────────────────────────────────────────────────────────
add_executable(FinceptTerminal WIN32
    ${CORE_SOURCES}
    ${NETWORK_SOURCES}
    ${AUTH_SOURCES}
    ${STORAGE_SOURCES}
    ${PYTHON_SOURCES}
    ${TRADING_SOURCES}
    ${SERVICE_SOURCES}
    ${UI_SOURCES}
    ${SCREEN_SOURCES}
    ${APP_SOURCES}
)

target_include_directories(FinceptTerminal PRIVATE src)

target_link_libraries(FinceptTerminal PRIVATE
    Qt6::Widgets
    Qt6::Charts
    Qt6::PrintSupport
    Qt6::Network
    Qt6::Sql
    Qt6::Concurrent
)

if(HAS_WEBSOCKETS)
    target_link_libraries(FinceptTerminal PRIVATE Qt6::WebSockets)
    target_compile_definitions(FinceptTerminal PRIVATE HAS_QT_WEBSOCKETS)
endif()

# ── Windows: deploy Qt DLLs ──────────────────────────────────────────────────
if(WIN32)
    find_program(WINDEPLOYQT_EXE windeployqt HINTS "C:/Qt/6.8.3/msvc2022_64/bin")
    if(WINDEPLOYQT_EXE)
        add_custom_command(TARGET FinceptTerminal POST_BUILD
            COMMAND "${WINDEPLOYQT_EXE}"
            ARGS "--no-translations" "$<TARGET_FILE:FinceptTerminal>"
            COMMENT "Running windeployqt..."
        )
    endif()
endif()
