cmake_minimum_required(VERSION 3.20)
project(FinceptTerminal VERSION 4.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Export compile_commands.json for clang-tidy
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Build performance ─────────────────────────────────────────────────────────
# Unity build for infrastructure layers (safe — no local symbol conflicts)
# Screens are excluded below via SKIP_UNITY_BUILD_INCLUSION (too many local statics/functions)
set(CMAKE_UNITY_BUILD ON)
set(CMAKE_UNITY_BUILD_BATCH_SIZE 8)

# Suppress Windows min/max macros that break std::min/std::max in unity batches
add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN)

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

# Optional: Multimedia (for inline video/stream playback)
find_package(Qt6 QUIET COMPONENTS Multimedia MultimediaWidgets)
if(Qt6Multimedia_FOUND AND Qt6MultimediaWidgets_FOUND)
    set(HAS_MULTIMEDIA TRUE)
endif()

# ── QXlsx (Excel file I/O) ────────────────────────────────────────────────────
include(FetchContent)
FetchContent_Declare(
    QXlsx
    GIT_REPOSITORY https://github.com/QtExcel/QXlsx.git
    GIT_TAG        v1.4.9
    SOURCE_SUBDIR  QXlsx
)
FetchContent_MakeAvailable(QXlsx)

# ── QGeoView (interactive map widget with OSM tiles) ─────────────────────────
FetchContent_Declare(
    QGeoView
    GIT_REPOSITORY https://github.com/AmonRaNet/QGeoView.git
    GIT_TAG        master
)
set(QGV_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
set(QGV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(QGeoView)
# QGeoView has anonymous-namespace symbol clashes under unity build
set_target_properties(qgeoview PROPERTIES UNITY_BUILD OFF)

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
    src/storage/sqlite/migrations/v004_cleanup.cpp
    src/storage/sqlite/migrations/v005_dashboard_layout.cpp
    src/storage/sqlite/migrations/v006_portfolio_multi.cpp
    src/storage/sqlite/migrations/v007_news_baselines.cpp

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
    src/storage/repositories/PortfolioRepository.cpp
    src/storage/repositories/WorkflowRepository.cpp

    # Workflow migration
    src/storage/sqlite/migrations/v008_workflows.cpp
)

# Python runner
set(PYTHON_SOURCES
    src/python/PythonRunner.cpp
    src/python/PythonSetupManager.cpp
)

# AI Chat
set(AI_CHAT_SOURCES
    src/ai_chat/LlmService.cpp
    src/ai_chat/AiChatScreen.cpp
)

# MCP — Model Context Protocol system
set(MCP_SOURCES
    # Core
    src/mcp/McpProvider.cpp
    src/mcp/McpService.cpp
    src/mcp/McpClient.cpp
    src/mcp/McpManager.cpp
    src/mcp/McpInit.cpp

    # Tool modules
    src/mcp/tools/NavigationTools.cpp
    src/mcp/tools/MarketDataTools.cpp
    src/mcp/tools/SettingsTools.cpp
    src/mcp/tools/PortfolioTools.cpp
    src/mcp/tools/TradingTools.cpp
    src/mcp/tools/PortfolioToolsExt.cpp
    src/mcp/tools/PythonTools.cpp
    src/mcp/tools/ExchangeTools.cpp
    src/mcp/tools/SystemTools.cpp
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
    src/services/news/NewsNlpService.cpp
    src/services/news/NewsCorrelationService.cpp
    src/services/databento/DatabentoService.cpp
    src/services/dbnomics/DBnomicsService.cpp
    src/services/gov_data/GovDataService.cpp
    src/services/equity/EquityResearchService.cpp
    src/services/ma_analytics/MAAnalyticsService.cpp
    src/services/geopolitics/GeopoliticsService.cpp
    src/services/maritime/MaritimeService.cpp
    src/services/ai_quant_lab/AIQuantLabService.cpp
    src/services/backtesting/BacktestingService.cpp
    src/services/algo_trading/AlgoTradingService.cpp
    src/services/portfolio/PortfolioService.cpp
    src/services/agents/AgentService.cpp
    src/services/workflow/NodeRegistry.cpp
    src/services/workflow/WorkflowService.cpp
    src/services/workflow/WorkflowExecutor.cpp
    src/services/workflow/ExpressionEngine.cpp
    src/services/workflow/nodes/TriggerNodes.cpp
    src/services/workflow/nodes/ControlFlowNodes.cpp
    src/services/workflow/nodes/UtilityNodes.cpp
    src/services/workflow/nodes/MarketDataNodes.cpp
    src/services/workflow/nodes/TradingNodes.cpp
    src/services/workflow/nodes/AnalyticsNodes.cpp
    src/services/workflow/nodes/SafetyNodes.cpp
    src/services/workflow/nodes/NotificationNodes.cpp
    src/services/workflow/nodes/AgentNodes.cpp
    src/services/workflow/nodes/FileNodes.cpp
    src/services/workflow/nodes/DataFormatNodes.cpp
    src/services/workflow/nodes/IntegrationNodes.cpp

    # Workflow infrastructure
    src/services/workflow/ExecutionHooks.cpp
    src/services/workflow/AuditLogger.cpp
    src/services/workflow/ConfirmationService.cpp
    src/services/workflow/RiskManager.cpp
    src/services/workflow/WorkflowCache.cpp
    src/services/workflow/Extensions.cpp
    src/services/workflow/ParameterProcessor.cpp
    src/services/workflow/adapters/ServiceBridges.cpp
    src/services/polymarket/PolymarketService.cpp
)

# UI framework
set(UI_SOURCES
    src/ui/theme/Theme.cpp
    src/ui/theme/StyleSheets.cpp
    src/ui/widgets/Card.cpp
    src/ui/widgets/StatusBadge.cpp
    src/ui/widgets/SearchBar.cpp
    src/ui/widgets/GeometricBackground.cpp
    src/ui/widgets/WorldMapWidget.cpp
    src/ui/widgets/LoadingOverlay.cpp
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
    src/screens/settings/LlmConfigSection.cpp
    src/screens/settings/McpServersSection.cpp
    src/screens/about/AboutScreen.cpp
    src/screens/support/SupportScreen.cpp
    src/screens/trade_viz/TradeVizScreen.cpp
    src/screens/docs/DocsScreen.cpp
    src/screens/ComingSoonScreen.cpp
    src/screens/setup/SetupScreen.cpp
    src/screens/info/ContactScreen.cpp
    src/screens/info/TermsScreen.cpp
    src/screens/info/PrivacyScreen.cpp
    src/screens/info/TrademarksScreen.cpp
    src/screens/info/HelpScreen.cpp
    src/screens/payment/PaymentProcessingScreen.cpp
    src/screens/payment/PaymentSuccessScreen.cpp
    src/screens/file_manager/FileManagerScreen.cpp
    src/screens/relationship_map/RelationshipMapScreen.cpp
    src/screens/relationship_map/RelationshipGraphScene.cpp
    src/services/relationship_map/RelationshipMapService.cpp
    src/screens/markets/MarketsScreen.cpp
    src/screens/markets/MarketPanel.cpp
    src/screens/dashboard/DashboardScreen.cpp
    src/screens/dashboard/DashboardToolBar.cpp
    src/screens/dashboard/DashboardStatusBar.cpp
    src/screens/dashboard/MarketPulsePanel.cpp
    src/screens/dashboard/TickerBar.cpp
    src/screens/dashboard/canvas/DashboardCanvas.cpp
    src/screens/dashboard/canvas/WidgetTile.cpp
    src/screens/dashboard/canvas/PlaceholderOverlay.cpp
    src/screens/dashboard/canvas/DashboardTemplates.cpp
    src/screens/dashboard/canvas/WidgetRegistry.cpp
    src/screens/dashboard/canvas/AddWidgetDialog.cpp
    src/screens/dashboard/canvas/TemplatePicker.cpp
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
    src/screens/dashboard/widgets/VideoPlayerWidget.cpp
    src/screens/report_builder/ReportBuilderScreen.cpp
    src/screens/report_builder/ComponentToolbar.cpp
    src/screens/report_builder/DocumentCanvas.cpp
    src/screens/report_builder/PropertiesPanel.cpp
    src/screens/news/NewsScreen.cpp
    src/screens/news/NewsCommandBar.cpp
    src/screens/news/NewsSidePanel.cpp
    src/screens/news/NewsFeedPanel.cpp
    src/screens/news/NewsFeedModel.cpp
    src/screens/news/NewsFeedDelegate.cpp
    src/screens/news/NewsDetailPanel.cpp
    src/screens/news/NewsTickerStrip.cpp
    src/screens/forum/ForumScreen.cpp
    src/screens/forum/ForumFeedPanel.cpp
    src/screens/forum/ForumThreadPanel.cpp
    src/services/forum/ForumService.cpp
    src/screens/crypto_trading/CryptoTradingScreen.cpp
    src/screens/crypto_trading/CryptoTickerBar.cpp
    src/screens/crypto_trading/CryptoWatchlist.cpp
    src/screens/crypto_trading/CryptoChart.cpp
    src/screens/crypto_trading/CryptoOrderEntry.cpp
    src/screens/crypto_trading/CryptoOrderBook.cpp
    src/screens/crypto_trading/CryptoBottomPanel.cpp
    src/screens/crypto_trading/CryptoCredentials.cpp
    src/screens/crypto_trading/CryptoTimeSales.cpp
    src/screens/crypto_trading/CryptoDepthChart.cpp
    src/screens/equity_trading/EquityTradingScreen.cpp
    src/screens/equity_trading/EquityTickerBar.cpp
    src/screens/equity_trading/EquityWatchlist.cpp
    src/screens/equity_trading/EquityChart.cpp
    src/screens/equity_trading/EquityOrderEntry.cpp
    src/screens/equity_trading/EquityOrderBook.cpp
    src/screens/equity_trading/EquityBottomPanel.cpp
    src/screens/equity_trading/EquityCredentials.cpp
    src/screens/data_sources/DataSourcesScreen.cpp
    src/screens/data_sources/ConnectorRegistry.cpp
    src/screens/data_sources/connectors/RelationalDatabases.cpp
    src/screens/data_sources/connectors/NoSqlDatabases.cpp
    src/screens/data_sources/connectors/TimeSeriesDatabases.cpp
    src/screens/data_sources/connectors/FileSources.cpp
    src/screens/data_sources/connectors/ApiStreaming.cpp
    src/screens/data_sources/connectors/CloudStorage.cpp
    src/screens/data_sources/connectors/MarketData.cpp
    src/screens/data_sources/connectors/SearchWarehouse.cpp
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
    src/screens/dbnomics/DBnomicsChartWidget.cpp
    src/screens/dbnomics/DBnomicsSelectionPanel.cpp
    src/screens/dbnomics/DBnomicsScreen.cpp

    # Code Editor (Python Colab)
    src/screens/code_editor/CodeEditorScreen.cpp

    # Excel Spreadsheet
    src/screens/excel/ExcelScreen.cpp
    src/screens/excel/SpreadsheetWidget.cpp

    # Government Data
    src/screens/gov_data/GovDataScreen.cpp
    src/screens/gov_data/GovDataProviderPanel.cpp
    src/screens/gov_data/GovDataTreasuryPanel.cpp
    src/screens/gov_data/GovDataCongressPanel.cpp

    # Derivatives Pricing
    src/screens/derivatives/DerivativesScreen.cpp

    # AkShare Data Explorer
    src/screens/akshare/AkShareScreen.cpp

    # Asia Markets
    src/screens/asia_markets/AsiaMarketsScreen.cpp

    # Geopolitics
    src/screens/geopolitics/GeopoliticsScreen.cpp
    src/screens/geopolitics/ConflictMonitorPanel.cpp
    src/screens/geopolitics/HDXDataPanel.cpp
    src/screens/geopolitics/RelationshipPanel.cpp
    src/screens/geopolitics/TradeAnalysisPanel.cpp

    # Maritime Intelligence
    src/screens/maritime/MaritimeScreen.cpp

    # AI Quant Lab
    src/screens/ai_quant_lab/AIQuantLabScreen.cpp
    src/screens/ai_quant_lab/QuantModulePanel.cpp

    # Backtesting
    src/screens/backtesting/BacktestingScreen.cpp

    # Algo Trading
    src/screens/algo_trading/AlgoTradingScreen.cpp
    src/screens/algo_trading/StrategyBuilderPanel.cpp
    src/screens/algo_trading/StrategyListPanel.cpp
    src/screens/algo_trading/ScannerPanel.cpp
    src/screens/algo_trading/DeploymentDashboard.cpp

    # Data Mapping
    src/screens/data_mapping/DataMappingScreen.cpp

    # Economics
    src/screens/economics/EconomicsScreen.cpp

    # Polymarket
    src/screens/polymarket/PolymarketScreen.cpp

    # Alpha Arena
    src/screens/alpha_arena/AlphaArenaScreen.cpp

    # Alternative Investments
    src/screens/alt_investments/AltInvestmentsScreen.cpp

    # MCP Servers
    src/screens/mcp_servers/McpServersScreen.cpp

    # QuantLib Suite
    src/screens/quantlib/QuantLibScreen.cpp

    # M&A Analytics
    src/screens/ma_analytics/MAAnalyticsScreen.cpp
    src/screens/ma_analytics/MAModulePanel.cpp

    # Equity Research (7 tabs)
    src/screens/equity_research/EquityResearchScreen.cpp
    src/screens/equity_research/EquityOverviewTab.cpp
    src/screens/equity_research/EquityFinancialsTab.cpp
    src/screens/equity_research/EquityAnalysisTab.cpp
    src/screens/equity_research/EquityTechnicalsTab.cpp
    src/screens/equity_research/EquityTalippTab.cpp
    src/screens/equity_research/EquityPeersTab.cpp
    src/screens/equity_research/EquityNewsTab.cpp

    # Portfolio
    src/screens/portfolio/PortfolioScreen.cpp
    src/screens/portfolio/PortfolioCommandBar.cpp
    src/screens/portfolio/PortfolioStatsRibbon.cpp
    src/screens/portfolio/PortfolioStatusBar.cpp
    src/screens/portfolio/PortfolioDialogs.cpp
    src/screens/portfolio/PortfolioHeatmap.cpp
    src/screens/portfolio/PortfolioPerfChart.cpp
    src/screens/portfolio/PortfolioSectorPanel.cpp
    src/screens/portfolio/PortfolioBlotter.cpp
    src/screens/portfolio/PortfolioSparkline.cpp
    src/screens/portfolio/PortfolioOrderPanel.cpp
    src/screens/portfolio/PortfolioDetailWrapper.cpp
    src/screens/portfolio/views/AnalyticsSectorsView.cpp
    src/screens/portfolio/views/PerformanceRiskView.cpp
    src/screens/portfolio/views/RiskManagementView.cpp
    src/screens/portfolio/views/PortfolioOptimizationView.cpp
    src/screens/portfolio/views/QuantStatsView.cpp
    src/screens/portfolio/views/ReportsView.cpp
    src/screens/portfolio/views/CustomIndexView.cpp
    src/screens/portfolio/views/PlanningView.cpp
    src/screens/portfolio/views/EconomicsView.cpp
    src/screens/portfolio/PortfolioFFNView.cpp
    src/screens/portfolio/PortfolioAiPanel.cpp
    src/screens/portfolio/PortfolioAgentPanel.cpp
    src/screens/agent_config/AgentConfigScreen.cpp
    src/screens/agent_config/AgentsViewPanel.cpp
    src/screens/agent_config/SystemViewPanel.cpp
    src/screens/agent_config/CreateAgentPanel.cpp
    src/screens/agent_config/ToolsViewPanel.cpp
    src/screens/agent_config/AgentChatPanel.cpp
    src/screens/agent_config/TeamsViewPanel.cpp
    src/screens/agent_config/WorkflowsViewPanel.cpp
    src/screens/agent_config/PlannerViewPanel.cpp

    # Node Editor
    src/screens/node_editor/NodeEditorScreen.cpp
    src/screens/node_editor/canvas/NodeCanvas.cpp
    src/screens/node_editor/canvas/NodeScene.cpp
    src/screens/node_editor/canvas/NodeItem.cpp
    src/screens/node_editor/canvas/PortItem.cpp
    src/screens/node_editor/canvas/EdgeItem.cpp
    src/screens/node_editor/canvas/TempEdge.cpp
    src/screens/node_editor/canvas/MiniMap.cpp
    src/screens/node_editor/palette/NodePalette.cpp
    src/screens/node_editor/properties/NodePropertiesPanel.cpp
    src/screens/node_editor/properties/ParameterWidgets.cpp
    src/screens/node_editor/properties/ExecutionResultsPanel.cpp
    src/screens/node_editor/toolbar/NodeEditorToolbar.cpp
    src/screens/node_editor/toolbar/DeployDialog.cpp
)

# App
set(APP_SOURCES
    src/app/main.cpp
    src/app/MainWindow.cpp
    src/app/ScreenRouter.cpp
)

# All screen files compiled individually — they have local statics/functions that
# collide when batched, and many use QLineEdit/etc without explicit includes.
# Infrastructure layers (core, network, auth, storage, services) still benefit from unity.
set_source_files_properties(
    src/screens/ComingSoonScreen.cpp
    src/screens/about/AboutScreen.cpp
    src/screens/agent_config/AgentChatPanel.cpp
    src/screens/agent_config/AgentConfigScreen.cpp
    src/screens/agent_config/AgentsViewPanel.cpp
    src/screens/agent_config/CreateAgentPanel.cpp
    src/screens/agent_config/PlannerViewPanel.cpp
    src/screens/agent_config/SystemViewPanel.cpp
    src/screens/agent_config/TeamsViewPanel.cpp
    src/screens/agent_config/ToolsViewPanel.cpp
    src/screens/agent_config/WorkflowsViewPanel.cpp
    src/screens/ai_quant_lab/AIQuantLabScreen.cpp
    src/screens/ai_quant_lab/QuantModulePanel.cpp
    src/screens/akshare/AkShareScreen.cpp
    src/screens/algo_trading/AlgoTradingScreen.cpp
    src/screens/algo_trading/DeploymentDashboard.cpp
    src/screens/algo_trading/ScannerPanel.cpp
    src/screens/algo_trading/StrategyBuilderPanel.cpp
    src/screens/algo_trading/StrategyListPanel.cpp
    src/screens/alpha_arena/AlphaArenaScreen.cpp
    src/screens/alt_investments/AltInvestmentsScreen.cpp
    src/screens/asia_markets/AsiaMarketsScreen.cpp
    src/screens/auth/ForgotPasswordScreen.cpp
    src/screens/auth/LoginScreen.cpp
    src/screens/auth/PricingScreen.cpp
    src/screens/auth/RegisterScreen.cpp
    src/screens/backtesting/BacktestingScreen.cpp
    src/screens/code_editor/CodeEditorScreen.cpp
    src/screens/crypto_trading/CryptoBottomPanel.cpp
    src/screens/crypto_trading/CryptoChart.cpp
    src/screens/crypto_trading/CryptoCredentials.cpp
    src/screens/crypto_trading/CryptoOrderBook.cpp
    src/screens/crypto_trading/CryptoOrderEntry.cpp
    src/screens/crypto_trading/CryptoTickerBar.cpp
    src/screens/crypto_trading/CryptoTimeSales.cpp
    src/screens/crypto_trading/CryptoTradingScreen.cpp
    src/screens/crypto_trading/CryptoWatchlist.cpp
    src/screens/crypto_trading/CryptoDepthChart.cpp
    src/screens/equity_trading/EquityTradingScreen.cpp
    src/screens/equity_trading/EquityTickerBar.cpp
    src/screens/equity_trading/EquityWatchlist.cpp
    src/screens/equity_trading/EquityChart.cpp
    src/screens/equity_trading/EquityOrderEntry.cpp
    src/screens/equity_trading/EquityOrderBook.cpp
    src/screens/equity_trading/EquityBottomPanel.cpp
    src/screens/equity_trading/EquityCredentials.cpp
    src/screens/data_sources/DataSourcesScreen.cpp
    src/screens/data_sources/ConnectorRegistry.cpp
    src/screens/data_sources/connectors/RelationalDatabases.cpp
    src/screens/data_sources/connectors/NoSqlDatabases.cpp
    src/screens/data_sources/connectors/TimeSeriesDatabases.cpp
    src/screens/data_sources/connectors/FileSources.cpp
    src/screens/data_sources/connectors/ApiStreaming.cpp
    src/screens/data_sources/connectors/CloudStorage.cpp
    src/screens/data_sources/connectors/MarketData.cpp
    src/screens/data_sources/connectors/SearchWarehouse.cpp
    src/screens/dashboard/DashboardScreen.cpp
    src/screens/dashboard/DashboardStatusBar.cpp
    src/screens/dashboard/DashboardToolBar.cpp
    src/screens/dashboard/MarketPulsePanel.cpp
    src/screens/dashboard/TickerBar.cpp
    src/screens/dashboard/WidgetGrid.cpp
    src/screens/dashboard/canvas/AddWidgetDialog.cpp
    src/screens/dashboard/canvas/DashboardCanvas.cpp
    src/screens/dashboard/canvas/DashboardTemplates.cpp
    src/screens/dashboard/canvas/PlaceholderOverlay.cpp
    src/screens/dashboard/canvas/TemplatePicker.cpp
    src/screens/dashboard/canvas/WidgetRegistry.cpp
    src/screens/dashboard/canvas/WidgetTile.cpp
    src/screens/dashboard/widgets/BaseWidget.cpp
    src/screens/dashboard/widgets/EconomicCalendarWidget.cpp
    src/screens/dashboard/widgets/MarketSentimentWidget.cpp
    src/screens/dashboard/widgets/NewsWidget.cpp
    src/screens/dashboard/widgets/PerformanceWidget.cpp
    src/screens/dashboard/widgets/PortfolioSummaryWidget.cpp
    src/screens/dashboard/widgets/QuickTradeWidget.cpp
    src/screens/dashboard/widgets/QuoteTableWidget.cpp
    src/screens/dashboard/widgets/RiskMetricsWidget.cpp
    src/screens/dashboard/widgets/ScreenerWidget.cpp
    src/screens/dashboard/widgets/SectorHeatmapWidget.cpp
    src/screens/dashboard/widgets/StockQuoteWidget.cpp
    src/screens/dashboard/widgets/TopMoversWidget.cpp
    src/screens/dashboard/widgets/VideoPlayerWidget.cpp
    src/screens/dashboard/widgets/WatchlistWidget.cpp
    src/screens/data_mapping/DataMappingScreen.cpp
    src/screens/dbnomics/DBnomicsChartWidget.cpp
    src/screens/dbnomics/DBnomicsDataTable.cpp
    src/screens/dbnomics/DBnomicsScreen.cpp
    src/screens/dbnomics/DBnomicsSelectionPanel.cpp
    src/screens/docs/DocsScreen.cpp
    src/screens/derivatives/DerivativesScreen.cpp
    src/screens/economics/EconomicsScreen.cpp
    src/screens/equity_research/EquityAnalysisTab.cpp
    src/screens/equity_research/EquityFinancialsTab.cpp
    src/screens/equity_research/EquityNewsTab.cpp
    src/screens/equity_research/EquityOverviewTab.cpp
    src/screens/equity_research/EquityPeersTab.cpp
    src/screens/equity_research/EquityResearchScreen.cpp
    src/screens/equity_research/EquityTalippTab.cpp
    src/screens/equity_research/EquityTechnicalsTab.cpp
    src/screens/excel/ExcelScreen.cpp
    src/screens/excel/SpreadsheetWidget.cpp
    src/screens/file_manager/FileManagerScreen.cpp
    src/screens/forum/ForumFeedPanel.cpp
    src/screens/forum/ForumPostListPanel.cpp
    src/screens/forum/ForumPostReaderPanel.cpp
    src/screens/forum/ForumScreen.cpp
    src/screens/forum/ForumSidebarPanel.cpp
    src/screens/forum/ForumThreadPanel.cpp
    src/screens/geopolitics/ConflictMonitorPanel.cpp
    src/screens/geopolitics/GeopoliticsScreen.cpp
    src/screens/geopolitics/HDXDataPanel.cpp
    src/screens/geopolitics/RelationshipPanel.cpp
    src/screens/geopolitics/TradeAnalysisPanel.cpp
    src/screens/gov_data/GovDataCongressPanel.cpp
    src/screens/gov_data/GovDataProviderPanel.cpp
    src/screens/gov_data/GovDataScreen.cpp
    src/screens/gov_data/GovDataTreasuryPanel.cpp
    src/screens/info/ContactScreen.cpp
    src/screens/info/HelpScreen.cpp
    src/screens/info/PrivacyScreen.cpp
    src/screens/info/TermsScreen.cpp
    src/screens/info/TrademarksScreen.cpp
    src/screens/ma_analytics/MAAnalyticsScreen.cpp
    src/screens/ma_analytics/MAModulePanel.cpp
    src/screens/maritime/MaritimeScreen.cpp
    src/screens/markets/MarketPanel.cpp
    src/screens/markets/MarketsScreen.cpp
    src/screens/mcp_servers/McpServersScreen.cpp
    src/screens/news/NewsCommandBar.cpp
    src/screens/news/NewsDetailPanel.cpp
    src/screens/news/NewsFeedDelegate.cpp
    src/screens/news/NewsFeedModel.cpp
    src/screens/news/NewsFeedPanel.cpp
    src/screens/news/NewsScreen.cpp
    src/screens/news/NewsSidePanel.cpp
    src/screens/news/NewsTickerStrip.cpp
    src/screens/node_editor/NodeEditorScreen.cpp
    src/screens/node_editor/canvas/EdgeItem.cpp
    src/screens/node_editor/canvas/MiniMap.cpp
    src/screens/node_editor/canvas/NodeCanvas.cpp
    src/screens/node_editor/canvas/NodeItem.cpp
    src/screens/node_editor/canvas/NodeScene.cpp
    src/screens/node_editor/canvas/PortItem.cpp
    src/screens/node_editor/canvas/TempEdge.cpp
    src/screens/node_editor/palette/NodePalette.cpp
    src/screens/node_editor/properties/ExecutionResultsPanel.cpp
    src/screens/node_editor/properties/NodePropertiesPanel.cpp
    src/screens/node_editor/properties/ParameterWidgets.cpp
    src/screens/node_editor/toolbar/DeployDialog.cpp
    src/screens/node_editor/toolbar/NodeEditorToolbar.cpp
    src/screens/notes/NotesScreen.cpp
    src/screens/payment/PaymentProcessingScreen.cpp
    src/screens/payment/PaymentSuccessScreen.cpp
    src/screens/polymarket/PolymarketScreen.cpp
    src/screens/portfolio/PortfolioAgentPanel.cpp
    src/screens/portfolio/PortfolioAiPanel.cpp
    src/screens/portfolio/PortfolioBlotter.cpp
    src/screens/portfolio/PortfolioCommandBar.cpp
    src/screens/portfolio/PortfolioDetailWrapper.cpp
    src/screens/portfolio/PortfolioDialogs.cpp
    src/screens/portfolio/PortfolioFFNView.cpp
    src/screens/portfolio/PortfolioHeatmap.cpp
    src/screens/portfolio/PortfolioOrderPanel.cpp
    src/screens/portfolio/PortfolioPerfChart.cpp
    src/screens/portfolio/PortfolioScreen.cpp
    src/screens/portfolio/PortfolioSectorPanel.cpp
    src/screens/portfolio/PortfolioSparkline.cpp
    src/screens/portfolio/PortfolioStatsRibbon.cpp
    src/screens/portfolio/PortfolioStatusBar.cpp
    src/screens/portfolio/views/AnalyticsSectorsView.cpp
    src/screens/portfolio/views/CustomIndexView.cpp
    src/screens/portfolio/views/EconomicsView.cpp
    src/screens/portfolio/views/PerformanceRiskView.cpp
    src/screens/portfolio/views/PlanningView.cpp
    src/screens/portfolio/views/PortfolioOptimizationView.cpp
    src/screens/portfolio/views/QuantStatsView.cpp
    src/screens/portfolio/views/ReportsView.cpp
    src/screens/portfolio/views/RiskManagementView.cpp
    src/screens/profile/ProfileScreen.cpp
    src/screens/relationship_map/RelationshipGraphScene.cpp
    src/screens/relationship_map/RelationshipMapScreen.cpp
    src/screens/report_builder/ComponentToolbar.cpp
    src/screens/report_builder/DocumentCanvas.cpp
    src/screens/report_builder/PropertiesPanel.cpp
    src/screens/report_builder/ReportBuilderScreen.cpp
    src/screens/settings/LlmConfigSection.cpp
    src/screens/settings/McpServersSection.cpp
    src/screens/settings/SettingsScreen.cpp
    src/screens/setup/SetupScreen.cpp
    src/screens/support/SupportScreen.cpp
    src/screens/surface_analytics/Surface3DWidget.cpp
    src/screens/surface_analytics/SurfaceAnalyticsScreen.cpp
    src/screens/surface_analytics/SurfaceCsvImporter.cpp
    src/screens/surface_analytics/SurfaceDatabentoPanel.cpp
    src/screens/surface_analytics/SurfaceDemoData.cpp
    src/screens/surface_analytics/SurfaceMetricsPanel.cpp
    src/screens/surface_analytics/SurfaceTableWidget.cpp
    src/screens/trade_viz/TradeVizScreen.cpp
    src/screens/watchlist/WatchlistScreen.cpp
    # Storage repositories — each defines kCols in namespace fincept
    src/storage/repositories/AgentConfigRepository.cpp
    src/storage/repositories/ChatRepository.cpp
    src/storage/repositories/ContextRecordingRepository.cpp
    src/storage/repositories/DashboardLayoutRepository.cpp
    src/storage/repositories/DataSourceRepository.cpp
    src/storage/repositories/LlmConfigRepository.cpp
    src/storage/repositories/McpServerRepository.cpp
    src/storage/repositories/NewsMonitorRepository.cpp
    src/storage/repositories/NotesRepository.cpp
    src/storage/repositories/PaperTradingRepository.cpp
    src/storage/repositories/PortfolioHoldingsRepository.cpp
    src/storage/repositories/PortfolioRepository.cpp
    src/storage/repositories/ReportRepository.cpp
    src/storage/repositories/SettingsRepository.cpp
    src/storage/repositories/WatchlistRepository.cpp
    src/storage/repositories/WorkflowRepository.cpp
    # Migration files — each defines sql() helper in anonymous namespace
    src/storage/sqlite/migrations/v001_initial.cpp
    src/storage/sqlite/migrations/v002_llm_chat.cpp
    src/storage/sqlite/migrations/v003_data_mcp_agents.cpp
    src/storage/sqlite/migrations/v004_cleanup.cpp
    src/storage/sqlite/migrations/v005_dashboard_layout.cpp
    src/storage/sqlite/migrations/v006_portfolio_multi.cpp
    src/storage/sqlite/migrations/v007_news_baselines.cpp
    src/storage/sqlite/migrations/v008_workflows.cpp
    # Trading broker files — each defines static now_ts() in same namespace
    src/trading/brokers/IndianBrokers.cpp
    src/trading/brokers/GlobalBrokers.cpp
    # MCP core + tool files — each defines static constexpr TAG in same namespace
    src/mcp/McpProvider.cpp
    src/mcp/McpService.cpp
    src/mcp/McpClient.cpp
    src/mcp/McpManager.cpp
    src/mcp/McpInit.cpp
    src/mcp/tools/NavigationTools.cpp
    src/mcp/tools/MarketDataTools.cpp
    src/mcp/tools/SettingsTools.cpp
    src/mcp/tools/PortfolioTools.cpp
    src/mcp/tools/TradingTools.cpp
    src/mcp/tools/PortfolioToolsExt.cpp
    src/mcp/tools/PythonTools.cpp
    src/mcp/tools/ExchangeTools.cpp
    src/mcp/tools/SystemTools.cpp
    PROPERTIES SKIP_UNITY_BUILD_INCLUSION TRUE
)

# ── Executable ────────────────────────────────────────────────────────────────
add_executable(FinceptTerminal WIN32
    ${CORE_SOURCES}
    ${NETWORK_SOURCES}
    ${AUTH_SOURCES}
    ${STORAGE_SOURCES}
    ${PYTHON_SOURCES}
    ${AI_CHAT_SOURCES}
    ${MCP_SOURCES}
    ${TRADING_SOURCES}
    ${SERVICE_SOURCES}
    ${UI_SOURCES}
    ${SCREEN_SOURCES}
    ${APP_SOURCES}
)

target_include_directories(FinceptTerminal PRIVATE src)

# Precompiled header — Qt headers parsed once, reused across all TUs
target_precompile_headers(FinceptTerminal PRIVATE
    <QWidget>
    <QObject>
    <QString>
    <QStringList>
    <QList>
    <QMap>
    <QHash>
    <QVector>
    <QVariant>
    <QJsonObject>
    <QJsonArray>
    <QJsonDocument>
    <QLabel>
    <QPushButton>
    <QVBoxLayout>
    <QHBoxLayout>
    <QGridLayout>
    <QScrollArea>
    <QFrame>
    <QTimer>
    <QDateTime>
    <QDebug>
    <QtConcurrent/QtConcurrent>
    <QNetworkReply>
    <QPointer>
)

target_link_libraries(FinceptTerminal PRIVATE
    Qt6::Widgets
    Qt6::Charts
    Qt6::PrintSupport
    Qt6::Network
    Qt6::Sql
    Qt6::Concurrent
    QXlsx::QXlsx
    qgeoview
)

if(HAS_WEBSOCKETS)
    target_link_libraries(FinceptTerminal PRIVATE Qt6::WebSockets)
    target_compile_definitions(FinceptTerminal PRIVATE HAS_QT_WEBSOCKETS)
endif()

if(HAS_MULTIMEDIA)
    target_link_libraries(FinceptTerminal PRIVATE Qt6::Multimedia Qt6::MultimediaWidgets)
    target_compile_definitions(FinceptTerminal PRIVATE HAS_QT_MULTIMEDIA)
endif()

# ── Copy qgeoview DLL next to the exe so it's found at runtime ────────────────
if(WIN32)
    add_custom_command(TARGET FinceptTerminal POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:qgeoview>"
            "$<TARGET_FILE_DIR:FinceptTerminal>"
        COMMENT "Copying qgeoview.dll..."
    )
endif()

# ── Windows: deploy Qt DLLs ──────────────────────────────────────────────────
if(WIN32)
    find_program(WINDEPLOYQT_EXE windeployqt HINTS "C:/Qt/6.8.3/msvc2022_64/bin")
    if(WINDEPLOYQT_EXE)
        # Only deploy Qt DLLs when explicitly requested via -DDEPLOY_QT=ON
        # Avoids 10-30s overhead on every incremental build
        if(DEPLOY_QT)
            add_custom_command(TARGET FinceptTerminal POST_BUILD
                COMMAND "${WINDEPLOYQT_EXE}"
                ARGS "--no-translations" "$<TARGET_FILE:FinceptTerminal>"
                COMMENT "Running windeployqt..."
            )
        endif()
    endif()
endif()
