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
# Batch size 14: sweet spot for MSVC — keeps each unity TU under ~30s to compile.
# 32 was too large: one TU change forced a 32-file recompile; 14 limits blast radius.
set(CMAKE_UNITY_BUILD_BATCH_SIZE 14)

# FetchContent: after first successful download, never hit the network again.
# Cuts configure time by 5-15s on incremental configures (no git fetch for 5 deps).
set(FETCHCONTENT_UPDATES_DISCONNECTED ON CACHE BOOL "" FORCE)

# MSVC compiler flags for maximum build throughput:
#   /MP        — multi-processor compilation (uses all logical cores within one project)
#   /Z7        — embed debug info in .obj instead of shared PDB → no PDB write contention
#                that serialises /MP builds (major speedup on 16+ core machines)
#   /Oi        — enable intrinsics (faster codegen, no extra passes)
#   /bigobj    — allow >65535 sections (needed by large unity TUs and MOC output)
#   /d2FuncCache7 — MSVC internal: increase function-level cache for template-heavy code
if(MSVC)
    add_compile_options(/MP /Z7 /Oi /bigobj)
    # Replace /Zi and /ZI (shared PDB) with /Z7 in all configs
    foreach(flag_var
            CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELWITHDEBINFO
            CMAKE_CXX_FLAGS_RELEASE CMAKE_CXX_FLAGS_MINSIZEREL)
        string(REPLACE "/Zi" "/Z7" ${flag_var} "${${flag_var}}")
        string(REPLACE "/ZI" "/Z7" ${flag_var} "${${flag_var}}")
    endforeach()
    # Release: enable whole-program optimisation + link-time code generation.
    # /GL compiles to IL; /LTCG merges at link time for cross-TU inlining & DCE.
    # Only for Release — Debug/RelWithDebInfo stay fast for iteration.
    if(CMAKE_BUILD_TYPE STREQUAL "Release" OR
       (DEFINED CMAKE_CONFIGURATION_TYPES AND "Release" IN_LIST CMAKE_CONFIGURATION_TYPES))
        add_compile_options($<$<CONFIG:Release>:/GL>)
        add_link_options($<$<CONFIG:Release>:/LTCG:incremental>)
    endif()
endif()

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
    Multimedia
)

# Optional: WebSockets (not always installed)
find_package(Qt6 QUIET COMPONENTS WebSockets)
if(Qt6WebSockets_FOUND)
    set(HAS_WEBSOCKETS TRUE)
endif()

# Optional: MultimediaWidgets (for inline video/stream playback)
# Qt6::Multimedia is already required above (needed for QAudioSource / WhisperService).
find_package(Qt6 QUIET COMPONENTS MultimediaWidgets)
if(Qt6MultimediaWidgets_FOUND)
    set(HAS_MULTIMEDIA TRUE)
endif()

# Optional: TextToSpeech (cross-platform TTS — SAPI/AVSpeech/speech-dispatcher)
find_package(Qt6 QUIET COMPONENTS TextToSpeech)
if(Qt6TextToSpeech_FOUND)
    set(HAS_TTS TRUE)
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

# QXlsx uses implicit const char* -> QString conversions which are banned by Qt6's
# strict cast macros. Undefine them for this third-party target only (GCC/Clang: -U,
# MSVC: /U). These macros are set by Qt6's cmake config, not by us globally.
if(MSVC)
    target_compile_options(QXlsx PRIVATE
        /UQT_NO_CAST_FROM_ASCII /UQT_NO_CAST_FROM_BYTEARRAY
        /UQT_NO_CAST_TO_ASCII   /UQT_NO_URL_CAST_FROM_STRING
    )
else()
    target_compile_options(QXlsx PRIVATE
        -UQT_NO_CAST_FROM_ASCII -UQT_NO_CAST_FROM_BYTEARRAY
        -UQT_NO_CAST_TO_ASCII   -UQT_NO_URL_CAST_FROM_STRING
    )
endif()

# ── whisper.cpp (offline speech-to-text, cross-platform) ─────────────────────
# Disable all whisper examples/tests — we only need the static library + header.
set(WHISPER_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS      OFF CACHE BOOL "" FORCE)
# SDL2 is only needed by the stream example — not required when embedding.
set(WHISPER_SDL2           OFF CACHE BOOL "" FORCE)

# Whisper/GGML build guards — prevent CI failures across all platforms:
#
# GGML_NATIVE: detects host CPU features at configure time and enables
#   architecture-specific intrinsics. Breaks in three ways on CI:
#   - macOS ARM64 native: runner may lack i8mm support → vmmlaq_s32 build error
#   - macOS ARM64 → x86_64 cross: detects apple-m3 CPU, invalid for x86 target
#   - Windows ARM64 (clang-cl): emits 'native+nodotprod+noi8mm+nosve', unsupported
#   We embed whisper for STT only — native CPU tuning gives negligible benefit
#   vs the build fragility it introduces across CI runners.
#
# GGML_METAL: uses Objective-C (.m files). GGML's CMake unity build merges .m
#   into a .c translation unit, which breaks ObjC syntax (@class, @protocol etc.)
#   on all macOS jobs. We use CPU inference only (use_gpu=false in WhisperService),
#   so Metal provides no benefit.
if(APPLE)
    set(GGML_NATIVE OFF CACHE BOOL "" FORCE)
    set(GGML_METAL  OFF CACHE BOOL "" FORCE)
elseif(MSVC AND CMAKE_GENERATOR_PLATFORM STREQUAL "ARM64")
    # Windows ARM64 cross-compile: clang-cl rejects GGML's native ARM feature string.
    set(GGML_NATIVE OFF CACHE BOOL "" FORCE)
endif()
FetchContent_Declare(
    whisper
    GIT_REPOSITORY https://github.com/ggml-org/whisper.cpp.git
    GIT_TAG        v1.7.4
    GIT_SHALLOW    TRUE
    # After first fetch, never touch the network again — saves 3-8s per configure
    UPDATE_DISCONNECTED TRUE
)
FetchContent_MakeAvailable(whisper)
# whisper.cpp uses C-style casts internally — suppress warnings for third-party code
if(MSVC)
    target_compile_options(whisper PRIVATE /W0)
else()
    target_compile_options(whisper PRIVATE -w)
endif()

# ── QSimpleUpdater (cross-platform auto-update) ──────────────────────────────
set(QSIMPLE_UPDATER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    QSimpleUpdater
    GIT_REPOSITORY https://github.com/alex-spataru/QSimpleUpdater.git
    GIT_TAG        master
)
FetchContent_MakeAvailable(QSimpleUpdater)
# QSimpleUpdater's CMakeLists builds a proper STATIC library target with
# AUTOMOC — we just link against it and add its include dir.

# ── md4c (fast CommonMark markdown parser → HTML) ────────────────────────────
FetchContent_Declare(
    md4c
    GIT_REPOSITORY https://github.com/mity/md4c.git
    GIT_TAG        release-0.5.2
)
FetchContent_MakeAvailable(md4c)
# md4c builds md4c (parser) and md4c-html (HTML renderer) as static libs.
# Disable any example/test targets it may expose.
set_target_properties(md4c PROPERTIES UNITY_BUILD OFF)
if(TARGET md4c-html)
    set_target_properties(md4c-html PROPERTIES UNITY_BUILD OFF)
endif()

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
    src/core/config/AppPaths.cpp
    src/core/logging/Logger.cpp
    src/core/events/EventBus.cpp
    src/core/session/SessionManager.cpp
)

# Network
set(NETWORK_SOURCES
    src/network/http/HttpClient.cpp
)
# WebSocketClient is always compiled; it stubs out when HAS_QT_WEBSOCKETS is not defined
list(APPEND NETWORK_SOURCES src/network/websocket/WebSocketClient.cpp)

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
    src/storage/repositories/NewsArticleRepository.cpp
    src/storage/repositories/PortfolioHoldingsRepository.cpp
    src/storage/repositories/ChatRepository.cpp
    src/storage/repositories/LlmConfigRepository.cpp
    src/storage/repositories/LlmProfileRepository.cpp
    src/storage/repositories/DataSourceRepository.cpp
    src/storage/repositories/McpServerRepository.cpp
    src/storage/repositories/AgentConfigRepository.cpp
    src/storage/repositories/ContextRecordingRepository.cpp
    src/storage/repositories/PortfolioRepository.cpp
    src/storage/repositories/WorkflowRepository.cpp
    src/storage/repositories/CustomIndexRepository.cpp
    src/storage/repositories/DataMappingRepository.cpp

    # Workflow migration
    src/storage/sqlite/migrations/v008_workflows.cpp

    # Instrument master contract (Phase 1)
    src/storage/sqlite/migrations/v009_instruments.cpp

    # LLM Profiles — multi-provider support
    src/storage/sqlite/migrations/v010_llm_profiles.cpp

    # Custom Indices — user-defined index creation
    src/storage/sqlite/migrations/v011_custom_indices.cpp

    # Data normalization — mapping persistence and normalized data storage
    src/storage/sqlite/migrations/v012_data_normalization.cpp

    # News articles — dedicated news article storage
    src/storage/sqlite/migrations/v013_news_articles.cpp
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
    src/ai_chat/AiChatBubble.cpp
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
    src/mcp/tools/MarketsTools.cpp
    src/mcp/tools/WatchlistTools.cpp
    src/mcp/tools/NewsTools.cpp
    src/mcp/tools/NotesTools.cpp
    src/mcp/tools/FileManagerTools.cpp
    src/mcp/tools/AiChatTools.cpp
    src/mcp/tools/PortfolioTools.cpp
    src/mcp/tools/CryptoTradingTools.cpp
    src/mcp/tools/PaperTradingTools.cpp
    src/mcp/tools/EdgarTools.cpp
    src/mcp/tools/MAAnalyticsTools.cpp
    src/mcp/tools/AltInvestmentsTools.cpp
    src/mcp/tools/DataSourcesTools.cpp
    src/mcp/tools/ForumTools.cpp
    src/mcp/tools/ProfileTools.cpp
    src/mcp/tools/SettingsTools.cpp
    src/mcp/tools/PythonTools.cpp
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
    # Trading WebSocket
    src/trading/websocket/ZerodhaWebSocket.cpp
    src/trading/websocket/AngelOneWebSocket.cpp
    # Indian brokers
    src/trading/brokers/zerodha/ZerodhaBroker.cpp
    src/trading/brokers/angelone/AngelOneBroker.cpp
    src/trading/brokers/fyers/FyersBroker.cpp
    src/trading/brokers/upstox/UpstoxBroker.cpp
    src/trading/brokers/dhan/DhanBroker.cpp
    src/trading/brokers/kotak/KotakBroker.cpp
    src/trading/brokers/groww/GrowwBroker.cpp
    src/trading/brokers/aliceblue/AliceBlueBroker.cpp
    src/trading/brokers/fivepaisa/FivePaisaBroker.cpp
    src/trading/brokers/iifl/IIFLBroker.cpp
    src/trading/brokers/motilal/MotilalBroker.cpp
    src/trading/brokers/shoonya/ShoonyaBroker.cpp
    # Global brokers
    src/trading/brokers/alpaca/AlpacaBroker.cpp
    src/trading/brokers/ibkr/IBKRBroker.cpp
    src/trading/brokers/tradier/TradierBroker.cpp
    src/trading/brokers/saxo/SaxoBankBroker.cpp

    # Instrument system (Phase 1)
    src/trading/instruments/InstrumentRepository.cpp
    src/trading/instruments/ZerodhaInstrumentParser.cpp
    src/trading/instruments/InstrumentService.cpp
)

# Services
set(SERVICE_SOURCES
    src/services/markets/MarketDataService.cpp
    src/services/data_normalization/DataNormalizationService.cpp
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
    src/services/quantlib/QuantLibClient.cpp
    src/services/economics/EconomicsService.cpp
    src/services/agents/AgentService.cpp
    src/services/file_manager/FileManagerService.cpp
    src/services/workflow/NodeRegistry.cpp
    src/services/workflow/WorkflowService.cpp
    src/services/workflow/WorkflowExecutor.cpp
    src/services/workflow/ExpressionEngine.cpp

    src/services/workspace/WorkspaceSerializer.cpp
    src/services/workspace/WorkspaceManager.cpp
    src/services/workspace/WorkspaceTemplates.cpp
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
    src/services/polymarket/PolymarketWebSocket.cpp

    # Whisper STT service (cross-platform offline speech recognition)
    src/services/stt/WhisperService.cpp

    # Modular notification system — service + 15 providers
    src/services/notifications/NotificationService.cpp
    src/services/notifications/providers/TelegramProvider.cpp
    src/services/notifications/providers/DiscordProvider.cpp
    src/services/notifications/providers/SlackProvider.cpp
    src/services/notifications/providers/EmailProvider.cpp
    src/services/notifications/providers/WhatsAppProvider.cpp
    src/services/notifications/providers/PushoverProvider.cpp
    src/services/notifications/providers/NtfyProvider.cpp
    src/services/notifications/providers/PushbulletProvider.cpp
    src/services/notifications/providers/GotifyProvider.cpp
    src/services/notifications/providers/MattermostProvider.cpp
    src/services/notifications/providers/TeamsProvider.cpp
    src/services/notifications/providers/WebhookProvider.cpp
    src/services/notifications/providers/PagerDutyProvider.cpp
    src/services/notifications/providers/OpsgenieProvider.cpp
    src/services/notifications/providers/SMSProvider.cpp
)

# UI framework
set(UI_SOURCES
    src/ui/markdown/MarkdownRenderer.cpp
    src/ui/theme/Theme.cpp
    src/ui/theme/ThemeManager.cpp
    src/ui/theme/StyleSheets.cpp
    src/ui/widgets/Card.cpp
    src/ui/widgets/StatusBadge.cpp
    src/ui/widgets/SearchBar.cpp
    src/ui/widgets/GeometricBackground.cpp
    src/ui/widgets/WorldMapWidget.cpp
    src/ui/widgets/LoadingOverlay.cpp
    src/ui/widgets/ConfettiOverlay.cpp
    src/ui/tables/DataTable.cpp
    src/ui/charts/ChartFactory.cpp
    src/ui/navigation/NavigationBar.cpp
    src/ui/navigation/FKeyBar.cpp
    src/ui/navigation/StatusBar.cpp
    src/ui/navigation/ToolBar.cpp
    src/ui/navigation/CommandBar.cpp

    src/ui/workspace/WorkspaceNewDialog.cpp
    src/ui/workspace/WorkspaceOpenDialog.cpp
    src/ui/workspace/WorkspaceSaveAsDialog.cpp

    # In-app notification widgets
    src/ui/widgets/NotifToast.cpp
    src/ui/widgets/NotifBell.cpp
    src/ui/widgets/NotifPanel.cpp
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
    src/screens/chat_mode/ChatModeService.cpp
    src/screens/chat_mode/ChatSessionPanel.cpp
    src/screens/chat_mode/ChatMessagePanel.cpp
    src/screens/chat_mode/ChatAgentPanel.cpp
    src/screens/chat_mode/ChatModeScreen.cpp
    src/screens/setup/SetupScreen.cpp
    src/screens/info/ContactScreen.cpp
    src/screens/info/TermsScreen.cpp
    src/screens/info/PrivacyScreen.cpp
    src/screens/info/TrademarksScreen.cpp
    src/screens/info/HelpScreen.cpp
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
    src/screens/dashboard/widgets/RecentFilesWidget.cpp
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
    src/screens/forum/ForumSidebarPanel.cpp
    src/screens/forum/ForumPostListPanel.cpp
    src/screens/forum/ForumPostReaderPanel.cpp
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
    src/screens/data_sources/connectors/AlternativeData.cpp
    src/screens/data_sources/connectors/OpenBanking.cpp
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
    src/screens/gov_data/GovDataCanadaPanel.cpp
    src/screens/gov_data/GovDataSwissPanel.cpp
    src/screens/gov_data/GovDataFrancePanel.cpp
    src/screens/gov_data/GovDataHKPanel.cpp
    src/screens/gov_data/GovDataCKANPanel.cpp
    src/screens/gov_data/GovDataUKPanel.cpp
    src/screens/gov_data/GovDataAustraliaPanel.cpp

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
    src/screens/economics/panels/EconPanelBase.cpp
    src/screens/economics/panels/WorldBankPanel.cpp
    src/screens/economics/panels/ImfPanel.cpp
    src/screens/economics/panels/FredPanel.cpp
    src/screens/economics/panels/FredAnalyticsPanel.cpp
    src/screens/economics/panels/OecdPanel.cpp
    src/screens/economics/panels/EcbPanel.cpp
    src/screens/economics/panels/EurostatPanel.cpp
    src/screens/economics/panels/BlsPanel.cpp
    src/screens/economics/panels/CensusPanel.cpp
    src/screens/economics/panels/EconDbPanel.cpp
    src/screens/economics/panels/TradingEconomicsPanel.cpp
    src/screens/economics/panels/EconomicCalendarPanel.cpp
    src/screens/economics/panels/BcbPanel.cpp
    src/screens/economics/panels/AkShareChinaPanel.cpp
    src/screens/economics/panels/OnsPanel.cpp
    src/screens/economics/panels/GlobalCentralBanksPanel.cpp
    src/screens/economics/panels/FederalReservePanel.cpp
    src/screens/economics/panels/FiscalDataPanel.cpp
    src/screens/economics/panels/OwIdPanel.cpp
    src/screens/economics/panels/StatCanPanel.cpp
    src/screens/economics/panels/IlostatPanel.cpp
    src/screens/economics/panels/UnComtradePanel.cpp
    src/screens/economics/panels/WorldBankHealthPanel.cpp
    src/screens/economics/panels/BisPanel.cpp
    src/screens/economics/panels/AdbPanel.cpp
    src/screens/economics/panels/CftcPanel.cpp
    src/screens/economics/panels/EiaPanel.cpp
    src/screens/economics/panels/WtoPanel.cpp
    src/screens/economics/panels/UnescoPanel.cpp
    src/screens/economics/panels/BeaPanel.cpp
    src/screens/economics/panels/FinceptMacroPanel.cpp

    # Polymarket
    src/screens/polymarket/PolymarketScreen.cpp
    src/screens/polymarket/PolymarketCommandBar.cpp
    src/screens/polymarket/PolymarketMarketCard.cpp
    src/screens/polymarket/PolymarketBrowsePanel.cpp
    src/screens/polymarket/PolymarketDetailPanel.cpp
    src/screens/polymarket/PolymarketOrderBook.cpp
    src/screens/polymarket/PolymarketPriceChart.cpp
    src/screens/polymarket/PolymarketLeaderboard.cpp
    src/screens/polymarket/PolymarketActivityFeed.cpp
    src/screens/polymarket/PolymarketStatusBar.cpp

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
    src/screens/portfolio/PortfolioTxnPanel.cpp
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
    src/screens/data_sources/connectors/AlternativeData.cpp
    src/screens/data_sources/connectors/OpenBanking.cpp
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
    src/screens/dashboard/widgets/RecentFilesWidget.cpp
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
    src/screens/gov_data/GovDataCanadaPanel.cpp
    src/screens/gov_data/GovDataSwissPanel.cpp
    src/screens/gov_data/GovDataFrancePanel.cpp
    src/screens/gov_data/GovDataHKPanel.cpp
    src/screens/gov_data/GovDataCKANPanel.cpp
    src/screens/gov_data/GovDataUKPanel.cpp
    src/screens/gov_data/GovDataAustraliaPanel.cpp
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
    src/screens/portfolio/PortfolioTxnPanel.cpp
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
    src/storage/repositories/NewsArticleRepository.cpp
    src/storage/repositories/NotesRepository.cpp
    src/storage/repositories/PaperTradingRepository.cpp
    src/storage/repositories/PortfolioHoldingsRepository.cpp
    src/storage/repositories/PortfolioRepository.cpp
    src/storage/repositories/ReportRepository.cpp
    src/storage/repositories/SettingsRepository.cpp
    src/storage/repositories/WatchlistRepository.cpp
    src/storage/repositories/WorkflowRepository.cpp
    src/storage/repositories/CustomIndexRepository.cpp
    src/storage/repositories/DataMappingRepository.cpp
    # Migration files — each defines sql() helper in anonymous namespace
    src/storage/sqlite/migrations/v001_initial.cpp
    src/storage/sqlite/migrations/v002_llm_chat.cpp
    src/storage/sqlite/migrations/v003_data_mcp_agents.cpp
    src/storage/sqlite/migrations/v004_cleanup.cpp
    src/storage/sqlite/migrations/v005_dashboard_layout.cpp
    src/storage/sqlite/migrations/v006_portfolio_multi.cpp
    src/storage/sqlite/migrations/v007_news_baselines.cpp
    src/storage/sqlite/migrations/v008_workflows.cpp
    src/storage/sqlite/migrations/v009_instruments.cpp
    src/storage/sqlite/migrations/v010_llm_profiles.cpp
    src/storage/sqlite/migrations/v011_custom_indices.cpp
    src/storage/sqlite/migrations/v012_data_normalization.cpp
    src/storage/sqlite/migrations/v013_news_articles.cpp
    # Polymarket screen files — each defines static fmt_* helpers in same namespace
    src/screens/polymarket/PolymarketScreen.cpp
    src/screens/polymarket/PolymarketCommandBar.cpp
    src/screens/polymarket/PolymarketMarketCard.cpp
    src/screens/polymarket/PolymarketBrowsePanel.cpp
    src/screens/polymarket/PolymarketDetailPanel.cpp
    src/screens/polymarket/PolymarketOrderBook.cpp
    src/screens/polymarket/PolymarketPriceChart.cpp
    src/screens/polymarket/PolymarketLeaderboard.cpp
    src/screens/polymarket/PolymarketActivityFeed.cpp
    src/screens/polymarket/PolymarketStatusBar.cpp
    src/trading/websocket/ZerodhaWebSocket.cpp
    src/trading/websocket/AngelOneWebSocket.cpp
    # Trading broker files — each defines static now_ts() in same namespace
    src/trading/brokers/zerodha/ZerodhaBroker.cpp
    src/trading/brokers/angelone/AngelOneBroker.cpp
    src/trading/brokers/fyers/FyersBroker.cpp
    src/trading/brokers/upstox/UpstoxBroker.cpp
    src/trading/brokers/dhan/DhanBroker.cpp
    src/trading/brokers/kotak/KotakBroker.cpp
    src/trading/brokers/groww/GrowwBroker.cpp
    src/trading/brokers/aliceblue/AliceBlueBroker.cpp
    src/trading/brokers/fivepaisa/FivePaisaBroker.cpp
    src/trading/brokers/iifl/IIFLBroker.cpp
    src/trading/brokers/motilal/MotilalBroker.cpp
    src/trading/brokers/shoonya/ShoonyaBroker.cpp
    src/trading/brokers/alpaca/AlpacaBroker.cpp
    src/trading/brokers/ibkr/IBKRBroker.cpp
    src/trading/brokers/tradier/TradierBroker.cpp
    src/trading/brokers/saxo/SaxoBankBroker.cpp
    # WhisperService has a static local QStringList — exclude from unity to avoid ODR issues
    src/services/stt/WhisperService.cpp
    PROPERTIES SKIP_UNITY_BUILD_INCLUSION TRUE
)

# MCP core + tools: each defines TAG or kTimeoutMs in shared namespaces —
# must stay excluded from unity (batch size 1 = each file its own TU)
set_source_files_properties(
    src/mcp/McpProvider.cpp
    src/mcp/McpService.cpp
    src/mcp/McpClient.cpp
    src/mcp/McpManager.cpp
    src/mcp/McpInit.cpp
    src/mcp/tools/NavigationTools.cpp
    src/mcp/tools/MarketsTools.cpp
    src/mcp/tools/WatchlistTools.cpp
    src/mcp/tools/NewsTools.cpp
    src/mcp/tools/NotesTools.cpp
    src/mcp/tools/FileManagerTools.cpp
    src/mcp/tools/AiChatTools.cpp
    src/mcp/tools/PortfolioTools.cpp
    src/mcp/tools/CryptoTradingTools.cpp
    src/mcp/tools/PaperTradingTools.cpp
    src/mcp/tools/EdgarTools.cpp
    src/mcp/tools/MAAnalyticsTools.cpp
    src/mcp/tools/AltInvestmentsTools.cpp
    src/mcp/tools/DataSourcesTools.cpp
    src/mcp/tools/ForumTools.cpp
    src/mcp/tools/ProfileTools.cpp
    src/mcp/tools/SettingsTools.cpp
    src/mcp/tools/PythonTools.cpp
    src/mcp/tools/SystemTools.cpp
    PROPERTIES SKIP_UNITY_BUILD_INCLUSION TRUE
)

# Trading brokers: each defines static now_ts() / TAG at file scope —
# ODR collision if batched, must stay excluded from unity
set_source_files_properties(
    src/trading/brokers/zerodha/ZerodhaBroker.cpp
    src/trading/brokers/angelone/AngelOneBroker.cpp
    src/trading/brokers/fyers/FyersBroker.cpp
    src/trading/brokers/upstox/UpstoxBroker.cpp
    src/trading/brokers/dhan/DhanBroker.cpp
    src/trading/brokers/kotak/KotakBroker.cpp
    src/trading/brokers/groww/GrowwBroker.cpp
    src/trading/brokers/aliceblue/AliceBlueBroker.cpp
    src/trading/brokers/fivepaisa/FivePaisaBroker.cpp
    src/trading/brokers/iifl/IIFLBroker.cpp
    src/trading/brokers/motilal/MotilalBroker.cpp
    src/trading/brokers/shoonya/ShoonyaBroker.cpp
    src/trading/brokers/alpaca/AlpacaBroker.cpp
    src/trading/brokers/ibkr/IBKRBroker.cpp
    src/trading/brokers/tradier/TradierBroker.cpp
    src/trading/brokers/saxo/SaxoBankBroker.cpp
    src/trading/websocket/ZerodhaWebSocket.cpp
    src/trading/websocket/AngelOneWebSocket.cpp
    PROPERTIES SKIP_UNITY_BUILD_INCLUSION TRUE
)

# Storage repositories: each defines kCols — safe to batch in groups of 3
set_source_files_properties(
    src/storage/repositories/AgentConfigRepository.cpp
    src/storage/repositories/ChatRepository.cpp
    src/storage/repositories/ContextRecordingRepository.cpp
    src/storage/repositories/DashboardLayoutRepository.cpp
    src/storage/repositories/DataSourceRepository.cpp
    src/storage/repositories/LlmConfigRepository.cpp
    src/storage/repositories/McpServerRepository.cpp
    src/storage/repositories/NewsMonitorRepository.cpp
    src/storage/repositories/NewsArticleRepository.cpp
    src/storage/repositories/NotesRepository.cpp
    src/storage/repositories/PaperTradingRepository.cpp
    src/storage/repositories/PortfolioHoldingsRepository.cpp
    src/storage/repositories/PortfolioRepository.cpp
    src/storage/repositories/ReportRepository.cpp
    src/storage/repositories/SettingsRepository.cpp
    src/storage/repositories/WatchlistRepository.cpp
    src/storage/repositories/WorkflowRepository.cpp
    src/storage/repositories/CustomIndexRepository.cpp
    src/storage/repositories/DataMappingRepository.cpp
    PROPERTIES UNITY_BUILD_BATCH_SIZE 3
)

# ── Executable ────────────────────────────────────────────────────────────────
# WIN32     — suppress console window on Windows
# MACOSX_BUNDLE — create a proper .app bundle on macOS
if(WIN32)
    set(_FINCEPT_EXE_FLAGS WIN32)
    set(_FINCEPT_PLATFORM_SOURCES resources/app.rc)
elseif(APPLE)
    set(_FINCEPT_EXE_FLAGS MACOSX_BUNDLE)
    set(_FINCEPT_PLATFORM_SOURCES "")
else()
    set(_FINCEPT_EXE_FLAGS "")
    set(_FINCEPT_PLATFORM_SOURCES "")
endif()

add_executable(FinceptTerminal ${_FINCEPT_EXE_FLAGS}
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
    src/services/updater/UpdateService.cpp
    ${_FINCEPT_PLATFORM_SOURCES}
)

target_include_directories(FinceptTerminal PRIVATE
    src
    ${whisper_SOURCE_DIR}/include
    ${whisper_SOURCE_DIR}
)

# ── macOS bundle properties ───────────────────────────────────────────────────
if(APPLE)
    set_target_properties(FinceptTerminal PROPERTIES
        MACOSX_BUNDLE_GUI_IDENTIFIER    "com.fincept.terminal"
        MACOSX_BUNDLE_BUNDLE_NAME       "Fincept Terminal"
        MACOSX_BUNDLE_BUNDLE_VERSION    "${CMAKE_PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${CMAKE_PROJECT_VERSION}"
        MACOSX_BUNDLE_ICON_FILE         "fincept.icns"
        MACOSX_BUNDLE_COPYRIGHT         "Copyright © 2025 Fincept Corporation"
    )
    # Copy .icns into the bundle Resources folder at build time
    set(_FINCEPT_ICNS "${CMAKE_CURRENT_SOURCE_DIR}/resources/fincept.icns")
    if(EXISTS "${_FINCEPT_ICNS}")
        target_sources(FinceptTerminal PRIVATE "${_FINCEPT_ICNS}")
        set_source_files_properties("${_FINCEPT_ICNS}"
            PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
    else()
        message(WARNING "macOS icon not found at ${_FINCEPT_ICNS} — app will show generic icon. "
                        "Generate it with: iconutil -c icns -o resources/fincept.icns <iconset>")
    endif()
endif()

# ── macOS Qt deployment (equivalent of windeployqt) ──────────────────────────
if(APPLE AND DEPLOY_QT)
    find_program(MACDEPLOYQT_EXECUTABLE macdeployqt
        HINTS "${Qt6_DIR}/../../../bin"
              "$ENV{QTDIR}/bin"
    )
    if(MACDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET FinceptTerminal POST_BUILD
            COMMAND "${MACDEPLOYQT_EXECUTABLE}"
                "$<TARGET_BUNDLE_DIR:FinceptTerminal>"
                -no-strip
            COMMENT "Running macdeployqt to bundle Qt frameworks..."
        )
    else()
        message(WARNING "macdeployqt not found — Qt frameworks will not be bundled. "
                        "Pass -DCMAKE_PREFIX_PATH=<Qt install dir> to help CMake find it.")
    endif()
endif()

# Precompiled header — Qt + std headers parsed once, reused across all TUs
# QDebug excluded — it adds overhead in Release and is only needed for debugging
target_precompile_headers(FinceptTerminal PRIVATE
    # ── Core Qt objects ───────────────────────────────────────────────────────
    <QObject>
    <QWidget>
    <QString>
    <QStringList>
    <QByteArray>
    <QList>
    <QVector>
    <QMap>
    <QHash>
    <QSet>
    <QVariant>
    <QPointer>
    # ── JSON ──────────────────────────────────────────────────────────────────
    <QJsonObject>
    <QJsonArray>
    <QJsonDocument>
    <QJsonValue>
    # ── Layouts ───────────────────────────────────────────────────────────────
    <QVBoxLayout>
    <QHBoxLayout>
    <QGridLayout>
    <QFormLayout>
    <QStackedLayout>
    # ── Common widgets ────────────────────────────────────────────────────────
    <QLabel>
    <QPushButton>
    <QToolButton>
    <QLineEdit>
    <QPlainTextEdit>
    <QTextEdit>
    <QComboBox>
    <QSpinBox>
    <QDoubleSpinBox>
    <QCheckBox>
    <QRadioButton>
    <QSlider>
    <QProgressBar>
    <QScrollArea>
    <QScrollBar>
    <QFrame>
    <QSplitter>
    <QTabWidget>
    <QStackedWidget>
    <QTableWidget>
    <QTableView>
    <QTreeWidget>
    <QTreeView>
    <QListWidget>
    <QListView>
    <QGroupBox>
    <QMessageBox>
    <QDialog>
    <QHeaderView>
    <QAbstractItemView>
    # ── Item models ───────────────────────────────────────────────────────────
    <QStandardItemModel>
    <QStandardItem>
    <QSortFilterProxyModel>
    # ── Timers & dates ────────────────────────────────────────────────────────
    <QTimer>
    <QDateTime>
    <QDate>
    <QTime>
    # ── Network ───────────────────────────────────────────────────────────────
    <QNetworkAccessManager>
    <QNetworkRequest>
    <QNetworkReply>
    <QUrl>
    <QUrlQuery>
    # ── Concurrency & process ─────────────────────────────────────────────────
    <QProcess>
    <QtConcurrent/QtConcurrent>
    <QThread>
    <QMutex>
    <QMutexLocker>
    <QReadWriteLock>
    <QAtomicInt>
    <QFuture>
    <QFutureWatcher>
    # ── I/O & paths ───────────────────────────────────────────────────────────
    <QSettings>
    <QStandardPaths>
    <QDir>
    <QFile>
    <QFileInfo>
    <QSaveFile>
    <QTemporaryFile>
    <QIODevice>
    # ── Graphics & painting ───────────────────────────────────────────────────
    <QPainter>
    <QPen>
    <QBrush>
    <QColor>
    <QFont>
    <QFontMetrics>
    <QPixmap>
    <QIcon>
    <QPalette>
    <QCursor>
    # ── Events ───────────────────────────────────────────────────────────────
    <QEvent>
    <QKeyEvent>
    <QMouseEvent>
    <QResizeEvent>
    <QWheelEvent>
    # ── Charts (Qt6::Charts — required dep) ──────────────────────────────────
    <QChart>
    <QChartView>
    <QLineSeries>
    <QBarSeries>
    <QBarSet>
    <QValueAxis>
    <QDateTimeAxis>
    <QBarCategoryAxis>
    # ── Multimedia (always available — required dep) ───────────────────────────
    <QAudioFormat>
    <QMediaDevices>
    # ── Misc Qt ──────────────────────────────────────────────────────────────
    <QUuid>
    <QRegularExpression>
    <QSqlQuery>
    <QApplication>
    <QAbstractScrollArea>
    # ── std ──────────────────────────────────────────────────────────────────
    <functional>
    <memory>
    <vector>
    <string>
    <optional>
    <variant>
    <atomic>
    <mutex>
    <algorithm>
    <numeric>
    <cmath>
    <cstdint>
    <utility>
    <tuple>
    <array>
    <span>
)

# ── OpenSSL (from vcpkg) — needed for Google Sheets JWT signing ───────────────
# Override via: cmake -DVCPKG_OPENSSL=<path>  or set VCPKG_INSTALLED_DIR env var.
if(NOT DEFINED VCPKG_OPENSSL)
    if(DEFINED ENV{VCPKG_INSTALLED_DIR})
        set(VCPKG_OPENSSL "$ENV{VCPKG_INSTALLED_DIR}/x64-windows")
    elseif(DEFINED ENV{VCPKG_ROOT})
        set(VCPKG_OPENSSL "$ENV{VCPKG_ROOT}/installed/x64-windows")
    else()
        # Developer machine default — override on CI via cmake cache variable
        set(VCPKG_OPENSSL "C:/windowsdisk/fincept-cpp/vcpkg_installed/x64-windows"
            CACHE PATH "Path to vcpkg x64-windows installed directory")
    endif()
endif()
message(STATUS "VCPKG_OPENSSL resolved to: ${VCPKG_OPENSSL}")
target_include_directories(FinceptTerminal PRIVATE
    "${VCPKG_OPENSSL}/include"
    "${md4c_SOURCE_DIR}/src"
)
target_link_libraries(FinceptTerminal PRIVATE
    Qt6::Widgets
    Qt6::Charts
    Qt6::PrintSupport
    Qt6::Network
    Qt6::Sql
    Qt6::Concurrent
    Qt6::Multimedia
    QXlsx::QXlsx
    qgeoview
    QSimpleUpdater
    whisper
    md4c
    md4c-html
)

# OpenSSL — Windows only (from vcpkg). macOS uses system OpenSSL; Linux uses system packages.
if(WIN32)
    target_link_libraries(FinceptTerminal PRIVATE
        "${VCPKG_OPENSSL}/lib/libcrypto.lib"
        "${VCPKG_OPENSSL}/lib/libssl.lib"
    )
elseif(APPLE)
    # macOS Security.framework for Keychain in SecureStorage
    target_link_libraries(FinceptTerminal PRIVATE
        "-framework Security"
    )
    find_package(OpenSSL QUIET)
    if(OpenSSL_FOUND)
        target_link_libraries(FinceptTerminal PRIVATE OpenSSL::SSL OpenSSL::Crypto)
    endif()
elseif(UNIX)
    find_package(OpenSSL REQUIRED)
    target_link_libraries(FinceptTerminal PRIVATE OpenSSL::SSL OpenSSL::Crypto)
endif()

# Copy OpenSSL DLLs to build output — Windows only
if(WIN32)
    add_custom_command(TARGET FinceptTerminal POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${VCPKG_OPENSSL}/bin/libcrypto-3-x64.dll"
            "$<TARGET_FILE_DIR:FinceptTerminal>/libcrypto-3-x64.dll"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${VCPKG_OPENSSL}/bin/libssl-3-x64.dll"
            "$<TARGET_FILE_DIR:FinceptTerminal>/libssl-3-x64.dll"
        COMMENT "Copying OpenSSL DLLs..."
    )
endif()

if(HAS_WEBSOCKETS)
    target_link_libraries(FinceptTerminal PRIVATE Qt6::WebSockets)
    target_compile_definitions(FinceptTerminal PRIVATE HAS_QT_WEBSOCKETS)
endif()

if(HAS_MULTIMEDIA)
    target_link_libraries(FinceptTerminal PRIVATE Qt6::MultimediaWidgets)
    target_compile_definitions(FinceptTerminal PRIVATE HAS_QT_MULTIMEDIA)
endif()

if(HAS_TTS)
    target_link_libraries(FinceptTerminal PRIVATE Qt6::TextToSpeech)
    target_compile_definitions(FinceptTerminal PRIVATE HAS_QT_TTS)
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
    find_program(WINDEPLOYQT_EXE windeployqt
        HINTS
            "$ENV{QT_ROOT_DIR}/bin"
            "C:/Qt/6.8.3/msvc2022_64/bin"
            "C:/Qt/6.8.2/msvc2022_64/bin"
    )
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

    # ── Fix: Qt Maintenance Tool partial-update leaves plugins mismatched ──────
    # The local Qt 6.8.3 install has Qt6Core/Qt6Sql updated (March 2026) but
    # sqldrivers/qsqlite.dll still at the original March 2025 build, causing a
    # crash on startup (0xc0000005 in qsqlite.dll).
    # Qt_temp contains a clean, fully-consistent Qt 6.8.3 install from aqtinstall.
    # Always overwrite the sql plugins with the consistent set after every build.
    set(QT_CONSISTENT_PLUGINS "C:/Qt_temp/6.8.3/msvc2022_64/plugins")
    if(EXISTS "${QT_CONSISTENT_PLUGINS}/sqldrivers/qsqlite.dll")
        add_custom_command(TARGET FinceptTerminal POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:FinceptTerminal>/sqldrivers"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${QT_CONSISTENT_PLUGINS}/sqldrivers/qsqlite.dll"
                "$<TARGET_FILE_DIR:FinceptTerminal>/sqldrivers/qsqlite.dll"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${QT_CONSISTENT_PLUGINS}/sqldrivers/qsqlmimer.dll"
                "$<TARGET_FILE_DIR:FinceptTerminal>/sqldrivers/qsqlmimer.dll"
            COMMENT "Overwriting sqldrivers with consistent Qt_temp plugins..."
        )
    endif()
endif()

# ── Copy Python scripts to build output ──────────────────────────────────────
add_custom_command(TARGET FinceptTerminal POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/scripts"
        "$<TARGET_FILE_DIR:FinceptTerminal>/scripts"
    COMMENT "Copying Python scripts to build output..."
)

# Bundle yt-dlp for dashboard YouTube playback on all platforms.
# Override path with: -DFINCEPT_YTDLP_EXE=/abs/path/to/yt-dlp[.exe]
set(FINCEPT_YTDLP_EXE "" CACHE FILEPATH "Path to yt-dlp executable to bundle with FinceptTerminal")
# Default OFF so builds on CI / macOS / Linux succeed without yt-dlp pre-installed.
# Set ON only for official release builds where yt-dlp must be present.
option(FINCEPT_REQUIRE_BUNDLED_YTDLP "Fail configure if yt-dlp is missing" OFF)

set(FINCEPT_YTDLP_TO_BUNDLE "")
if(FINCEPT_YTDLP_EXE)
    set(FINCEPT_YTDLP_TO_BUNDLE "${FINCEPT_YTDLP_EXE}")
else()
    find_program(FINCEPT_YTDLP_AUTO_EXE NAMES yt-dlp yt-dlp.exe)
    if(FINCEPT_YTDLP_AUTO_EXE)
        set(FINCEPT_YTDLP_TO_BUNDLE "${FINCEPT_YTDLP_AUTO_EXE}")
    endif()
endif()

if(FINCEPT_YTDLP_TO_BUNDLE AND EXISTS "${FINCEPT_YTDLP_TO_BUNDLE}")
    if(WIN32)
        set(FINCEPT_YTDLP_OUTPUT_NAME "yt-dlp.exe")
    else()
        set(FINCEPT_YTDLP_OUTPUT_NAME "yt-dlp")
    endif()

    add_custom_command(TARGET FinceptTerminal POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${FINCEPT_YTDLP_TO_BUNDLE}"
            "$<TARGET_FILE_DIR:FinceptTerminal>/${FINCEPT_YTDLP_OUTPUT_NAME}"
        COMMENT "Copying yt-dlp for dashboard video playback..."
    )

    if(UNIX)
        add_custom_command(TARGET FinceptTerminal POST_BUILD
            COMMAND chmod +x "$<TARGET_FILE_DIR:FinceptTerminal>/${FINCEPT_YTDLP_OUTPUT_NAME}"
            COMMENT "Setting executable permission on bundled yt-dlp..."
        )
    endif()
else()
    set(FINCEPT_YTDLP_MISSING_MSG
        "yt-dlp not found. Set FINCEPT_YTDLP_EXE or install yt-dlp on build machine. "
        "Bundling is required so end users need no setup."
    )
    if(FINCEPT_REQUIRE_BUNDLED_YTDLP)
        message(FATAL_ERROR "${FINCEPT_YTDLP_MISSING_MSG}")
    else()
        message(WARNING "${FINCEPT_YTDLP_MISSING_MSG}")
    endif()
endif()

# ── Install rules ─────────────────────────────────────────────────────────────
# Used by: cmake --install build --prefix /usr
# Required for AppImage, .deb, .rpm, and system-wide installation on Linux.

if(WIN32)
    # Windows: install exe + all DLLs beside it (windeployqt already copies them)
    install(TARGETS FinceptTerminal RUNTIME DESTINATION ".")
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/scripts/"
            DESTINATION "scripts")

elseif(APPLE)
    # macOS: install the .app bundle
    install(TARGETS FinceptTerminal BUNDLE DESTINATION ".")

else()
    # Linux: FHS-compliant layout
    install(TARGETS FinceptTerminal
            RUNTIME DESTINATION "bin")

    # Python analytics scripts
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/scripts/"
            DESTINATION "share/fincept-terminal/scripts"
            USE_SOURCE_PERMISSIONS)

    # App icon (PNG required for hicolor theme; convert .ico → .png in CI)
    set(_ICON_PNG "${CMAKE_CURRENT_SOURCE_DIR}/resources/fincept.png")
    if(EXISTS "${_ICON_PNG}")
        install(FILES "${_ICON_PNG}"
                DESTINATION "share/icons/hicolor/256x256/apps"
                RENAME "fincept-terminal.png")
    else()
        message(STATUS "Linux icon not found at ${_ICON_PNG} — "
                       "convert resources/fincept.ico to fincept.png for proper desktop integration")
    endif()

    # .desktop file
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/packaging/linux/fincept-terminal.desktop"
            DESTINATION "share/applications")

    # AppStream metadata
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/packaging/linux/fincept-terminal.appdata.xml"
            DESTINATION "share/metainfo")

    # yt-dlp (if bundled)
    if(FINCEPT_YTDLP_TO_BUNDLE AND EXISTS "${FINCEPT_YTDLP_TO_BUNDLE}")
        install(PROGRAMS "${FINCEPT_YTDLP_TO_BUNDLE}"
                DESTINATION "bin"
                RENAME "yt-dlp")
    endif()
endif()
