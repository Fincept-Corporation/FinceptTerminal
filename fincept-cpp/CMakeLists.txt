cmake_minimum_required(VERSION 3.20)
project(FinceptTerminal VERSION 4.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# =============================================================================
# Build acceleration: Unity builds (merge .cpp files into batches)
# =============================================================================
option(FINCEPT_UNITY_BUILD "Enable unity (jumbo) builds for faster compilation" ON)
if(FINCEPT_UNITY_BUILD)
    set(CMAKE_UNITY_BUILD ON)
    set(CMAKE_UNITY_BUILD_BATCH_SIZE 12)
endif()


# =============================================================================
# Dependencies via vcpkg
# =============================================================================
find_package(glfw3 CONFIG REQUIRED)
find_package(CURL CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(unofficial-sqlite3 CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(yoga CONFIG REQUIRED)
find_package(implot CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_path(STB_INCLUDE_DIRS "stb_image.h")
find_path(MINIAUDIO_INCLUDE_DIRS "miniaudio.h")

# OpenGL
find_package(OpenGL REQUIRED)

# =============================================================================
# Optional: libmpv for inline video playback (YouTube widget)
# Cross-platform: Windows uses third_party/mpv, Linux/macOS use pkg-config
# =============================================================================
option(FINCEPT_ENABLE_MPV "Enable mpv video playback in YouTube widget" ON)
set(FINCEPT_HAS_MPV FALSE)
set(MPV_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/mpv")

if(FINCEPT_ENABLE_MPV)
    if(WIN32 AND EXISTS "${MPV_DIR}/include/mpv/client.h")
        # Windows: use prebuilt library from third_party/mpv
        set(FINCEPT_HAS_MPV TRUE)
        set(MPV_INCLUDE_DIR "${MPV_DIR}/include")
        set(MPV_LIBRARY "${MPV_DIR}/mpv.lib")
        message(STATUS "libmpv found (Windows prebuilt): ${MPV_DIR}")
    elseif(NOT WIN32)
        # Linux/macOS: find via pkg-config
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(MPV QUIET mpv)
            if(MPV_FOUND)
                set(FINCEPT_HAS_MPV TRUE)
                set(MPV_INCLUDE_DIR ${MPV_INCLUDE_DIRS})
                set(MPV_LIBRARY ${MPV_LIBRARIES})
                message(STATUS "libmpv found (pkg-config): ${MPV_LIBRARIES}")
            endif()
        endif()
        if(NOT FINCEPT_HAS_MPV)
            message(STATUS "libmpv not found — install libmpv-dev (apt) or mpv (brew). YouTube widget will use link-only mode")
        endif()
    else()
        message(STATUS "libmpv not found at ${MPV_DIR} — YouTube widget will use link-only mode")
    endif()
endif()

# =============================================================================
# Sources
# =============================================================================
set(SOURCES
    # Core infrastructure
    src/main.cpp
    src/app.cpp
    src/core/window.cpp
    src/core/font_loader.cpp
    src/core/hot_reload.cpp
    src/core/notification.cpp

    # HTTP & Storage
    src/http/http_client.cpp
    src/storage/database.cpp
    src/storage/db_core.cpp
    src/storage/db_cache.cpp
    src/storage/db_ops_settings.cpp
    src/storage/db_ops_chat.cpp
    src/storage/db_ops_watchlist.cpp
    src/storage/db_ops_cache.cpp
    src/storage/db_ops_sessions.cpp
    src/storage/db_ops_notes_algo.cpp
    src/storage/crypto.cpp
    src/storage/cache_service.cpp

    # Auth
    src/auth/auth_api.cpp
    src/auth/auth_manager.cpp
    src/auth/login_screen.cpp
    src/auth/register_screen.cpp
    src/auth/forgot_password.cpp
    src/auth/pricing_screen.cpp

    # Screens — Dashboard
    src/screens/dashboard/dashboard_screen.cpp
    src/screens/dashboard/dashboard_widgets.cpp
    src/screens/dashboard/dashboard_widgets_ext.cpp
    src/screens/dashboard/dashboard_panels.cpp
    src/screens/dashboard/dashboard_data.cpp
    src/screens/dashboard/stock_ticker.cpp
    src/screens/dashboard/sidebar.cpp
    src/screens/dashboard/status_bar.cpp
    src/screens/dashboard/widgets/account_info.cpp
    src/screens/dashboard/widgets/market_overview.cpp

    # Screens — Profile, News, Markets
    src/screens/profile/profile_screen.cpp
    src/screens/profile/profile_sections.cpp
    src/screens/profile/profile_api.cpp
    src/screens/news/news_screen.cpp
    src/screens/news/news_parser.cpp
    src/screens/news/news_panels.cpp
    src/screens/markets/markets_data.cpp
    src/screens/markets/markets_screen.cpp

    # Screens — Research
    src/screens/research/research_data.cpp
    src/screens/research/research_screen.cpp
    src/screens/research/overview_panel.cpp
    src/screens/research/financials_panel.cpp
    src/screens/research/analysis_panel.cpp
    src/screens/research/technicals_panel.cpp
    src/screens/research/news_panel.cpp

    # Screens — Surface Analytics
    src/screens/surface_analytics/demo_data.cpp
    src/screens/surface_analytics/demo_data_derivatives.cpp
    src/screens/surface_analytics/demo_data_fixed_income.cpp
    src/screens/surface_analytics/demo_data_fx.cpp
    src/screens/surface_analytics/demo_data_credit.cpp
    src/screens/surface_analytics/demo_data_commodities.cpp
    src/screens/surface_analytics/demo_data_risk.cpp
    src/screens/surface_analytics/demo_data_macro.cpp
    src/screens/surface_analytics/surface_screen.cpp
    src/screens/surface_analytics/surface_3d_renderer.cpp
    src/screens/surface_analytics/surface_tables.cpp
    src/screens/surface_analytics/panels_derivatives.cpp
    src/screens/surface_analytics/panels_fixed_income.cpp
    src/screens/surface_analytics/panels_fx.cpp
    src/screens/surface_analytics/panels_credit.cpp
    src/screens/surface_analytics/panels_commodities.cpp
    src/screens/surface_analytics/panels_risk.cpp
    src/screens/surface_analytics/panels_macro.cpp
    src/screens/surface_analytics/tables_extended.cpp
    src/screens/surface_analytics/csv_importer.cpp
    src/screens/surface_analytics/import_dialog.cpp

    # UI — Tile Map
    src/ui/tile_map.cpp

    # Vendor — imnodes (node graph editor)
    src/vendor/imnodes/imnodes.cpp

    # Screens — Geopolitics
    src/screens/geopolitics/geopolitics_data.cpp
    src/screens/geopolitics/geopolitics_screen.cpp

    # Screens — Crypto Trading
    src/screens/crypto_trading/crypto_trading_screen.cpp
    src/screens/crypto_trading/crypto_lifecycle.cpp
    src/screens/crypto_trading/crypto_screen.cpp
    src/screens/crypto_trading/crypto_watchlist.cpp
    src/screens/crypto_trading/crypto_chart.cpp
    src/screens/crypto_trading/crypto_bottom_tabs.cpp
    src/screens/crypto_trading/crypto_order_entry.cpp
    src/screens/crypto_trading/crypto_order_actions.cpp
    src/screens/crypto_trading/crypto_credentials.cpp

    # Screens — Node Editor
    src/screens/node_editor/node_registry.cpp
    src/screens/node_editor/node_defs.cpp
    src/screens/node_editor/editor_state.cpp
    src/screens/node_editor/workflow_store.cpp
    src/screens/node_editor/workflow_executor.cpp
    src/screens/node_editor/node_executor.cpp
    src/screens/node_editor/node_editor_screen.cpp
    src/screens/node_editor/workflow_templates.cpp

    # Screens — AI Chat
    src/screens/ai_chat/llm_service.cpp
    src/screens/ai_chat/ai_chat_screen.cpp

    # Screens — Agent Studio
    src/screens/agent_studio/agent_service.cpp
    src/screens/agent_studio/agent_discovery.cpp
    src/screens/agent_studio/agent_studio_screen.cpp

    # Screens — QuantLib
    src/screens/quantlib/quantlib_api.cpp
    src/screens/quantlib/quantlib_screen.cpp
    src/screens/quantlib/quantlib_ui.cpp
    src/screens/quantlib/quantlib_helpers.cpp
    src/screens/quantlib/panels_core.cpp
    src/screens/quantlib/panels_models.cpp
    src/screens/quantlib/panels_analytics.cpp
    src/screens/quantlib/panels_instruments.cpp
    src/screens/quantlib/panels_quant.cpp

    # Screens — Watchlist
    src/screens/watchlist/watchlist_data.cpp
    src/screens/watchlist/watchlist_screen.cpp

    # Screens — Settings
    src/screens/settings/settings_screen.cpp
    src/screens/settings/sections/credentials_section.cpp
    src/screens/settings/sections/llm_config_section.cpp
    src/screens/settings/sections/terminal_appearance_section.cpp
    src/screens/settings/sections/storage_cache_section.cpp
    src/screens/settings/sections/notifications_section.cpp
    src/screens/settings/sections/general_section.cpp

    # Portfolio & Paper Trading
    src/portfolio/portfolio_service.cpp
    src/portfolio/portfolio_metrics.cpp
    src/trading/paper_trading.cpp
    src/trading/order_matcher.cpp
    src/trading/exchange_service.cpp
    src/trading/broker_registry.cpp
    src/trading/brokers/fyers_broker.cpp
    src/trading/brokers/zerodha_broker.cpp
    src/trading/brokers/angelone_broker.cpp
    src/trading/brokers/upstox_broker.cpp
    src/trading/brokers/dhan_broker.cpp
    src/trading/brokers/kotak_broker.cpp
    src/trading/brokers/groww_broker.cpp
    src/trading/brokers/aliceblue_broker.cpp
    src/trading/brokers/fivepaisa_broker.cpp
    src/trading/brokers/iifl_broker.cpp
    src/trading/brokers/motilal_broker.cpp
    src/trading/brokers/shoonya_broker.cpp
    src/trading/brokers/alpaca_broker.cpp
    src/trading/brokers/ibkr_broker.cpp
    src/trading/brokers/tradier_broker.cpp
    src/trading/brokers/saxobank_broker.cpp
    src/screens/portfolio/portfolio_screen.cpp
    src/screens/portfolio/views/analytics_view.cpp
    src/screens/portfolio/views/performance_view.cpp
    src/screens/portfolio/views/risk_view.cpp
    src/screens/portfolio/views/reports_view.cpp
    src/screens/portfolio/views/planning_view.cpp
    src/screens/portfolio/views/economics_view.cpp
    src/screens/portfolio/views/optimization_view.cpp
    src/screens/portfolio/views/quantstats_view.cpp
    src/screens/portfolio/views/indices_view.cpp

    # Screens — Economics
    src/screens/economics/economics_screen.cpp

    # Screens — DBnomics
    src/screens/dbnomics/dbnomics_screen.cpp

    # Screens — Notes
    src/screens/notes/notes_screen.cpp

    # Screens — Data Sources (91 adapters are header-only in adapters/)
    src/screens/data_sources/data_sources_screen.cpp
    src/screens/data_sources/connectors/connector_registry.cpp

    # Screens — M&A Analytics
    src/screens/mna/mna_screen.cpp
    src/screens/mna/mna_data.cpp
    src/screens/mna/panels/valuation_panel.cpp
    src/screens/mna/panels/merger_panel.cpp
    src/screens/mna/panels/deal_db_panel.cpp
    src/screens/mna/panels/startup_panel.cpp
    src/screens/mna/panels/fairness_panel.cpp
    src/screens/mna/panels/industry_panel.cpp
    src/screens/mna/panels/advanced_panel.cpp
    src/screens/mna/panels/comparison_panel.cpp

    # Screens — Forum
    src/screens/forum/forum_screen.cpp

    # Screens — Documentation
    src/screens/docs/docs_screen.cpp

    # Screens — Support
    src/screens/support/support_screen.cpp

    # Screens — About
    src/screens/about/about_screen.cpp

    # Screens — Government Data
    src/screens/gov_data/gov_data.cpp
    src/screens/gov_data/gov_data_screen.cpp
    src/screens/gov_data/providers/us_treasury.cpp
    src/screens/gov_data/providers/us_congress.cpp
    src/screens/gov_data/providers/canada_gov.cpp
    src/screens/gov_data/providers/openafrica.cpp
    src/screens/gov_data/providers/spain_data.cpp
    src/screens/gov_data/providers/pxweb.cpp
    src/screens/gov_data/providers/swiss_gov.cpp
    src/screens/gov_data/providers/french_gov.cpp
    src/screens/gov_data/providers/universal_ckan.cpp
    src/screens/gov_data/providers/data_gov_hk.cpp

    # Screens — Backtesting
    src/screens/backtesting/backtesting_data.cpp
    src/screens/backtesting/backtesting_screen.cpp

    # Screens — Algo Trading
    src/screens/algo_trading/algo_trading_screen.cpp
    src/screens/algo_trading/algo_service.cpp

    # Trading — Unified Trading
    src/trading/unified_trading.cpp

    # Screens — AI Quant Lab
    src/screens/ai_quant_lab/ai_quant_lab_api.cpp
    src/screens/ai_quant_lab/ai_quant_lab_screen.cpp

    # Screens — Code Editor
    src/screens/code_editor/notebook_io.cpp
    src/screens/code_editor/code_editor_screen.cpp
    src/screens/code_editor/syntax_highlight.cpp
    src/screens/code_editor/cell_render.cpp
    src/screens/code_editor/toolbar_panels.cpp
    src/screens/code_editor/cell_ops.cpp
    src/screens/code_editor/execution.cpp
    src/screens/code_editor/file_ops.cpp
    src/screens/code_editor/search_ops.cpp
    src/screens/code_editor/editor_utils.cpp

    # Media — mpv video player
    src/media/mpv_player.cpp

    # Python bridge
    src/python/python_runner.cpp
    src/python/setup_manager.cpp
    src/python/setup_screen.cpp

    # MCP — Model Context Protocol
    src/mcp/mcp_provider.cpp
    src/mcp/mcp_client.cpp
    src/mcp/mcp_manager.cpp
    src/mcp/mcp_service.cpp
    src/mcp/mcp_init.cpp
    src/mcp/tools/navigation_tools.cpp
    src/mcp/tools/market_data_tools.cpp
    src/mcp/tools/settings_tools.cpp
    src/mcp/tools/portfolio_tools.cpp
    src/mcp/tools/trading_tools.cpp
    src/mcp/tools/portfolio_tools_ext.cpp
    src/mcp/tools/python_tools.cpp
    src/mcp/tools/exchange_tools.cpp
    src/mcp/tools/system_tools.cpp

    # Screens — Data Mapping
    src/screens/data_mapping/data_mapping_engine.cpp
    src/screens/data_mapping/data_mapping_db.cpp
    src/screens/data_mapping/data_mapping_screen.cpp

    # Screens — Screener (FRED Economic Data)
    src/screens/screener/screener_screen.cpp

    # Screens — Equity Trading
    src/screens/equity_trading/equity_trading_screen.cpp

    # Screens — Report Builder
    src/screens/report_builder/report_builder_screen.cpp

    # Screens — Derivatives
    src/screens/derivatives/derivatives_screen.cpp

    # Screens — Excel / Spreadsheet
    src/screens/excel/excel_screen.cpp

    # Screens — Polymarket
    src/screens/polymarket/polymarket_screen.cpp

    # Screens — AKShare Data Explorer
    src/screens/akshare/akshare_screen.cpp

    # Screens — Asia Markets
    src/screens/asia_markets/asia_markets_screen.cpp

    # Screens — Relationship Map
    src/screens/relationship_map/relationship_map_screen.cpp

    # Screens — Alternative Investments
    src/screens/alt_investments/alt_investments_screen.cpp

    # Screens — Maritime Intelligence
    src/screens/maritime/maritime_screen.cpp

    # Screens — MCP Servers
    src/screens/mcp_servers/mcp_servers_screen.cpp

    # Screens — Alpha Arena
    src/screens/alpha_arena/alpha_arena_screen.cpp

    # Screens — Trade Visualization
    src/screens/trade_viz/trade_viz_screen.cpp

    # Voice — audio capture, playback, TTS, STT, service
    src/voice/audio_capture.cpp
    src/voice/audio_player.cpp
    src/voice/tts_engine.cpp
    src/voice/stt_engine.cpp
    src/voice/text_preprocessor.cpp
    src/voice/voice_service.cpp

    # Settings — Voice section
    src/screens/settings/sections/voice_section.cpp

    # Core (continued)
    src/core/device_id.cpp
)

# =============================================================================
# Executable
# =============================================================================
add_executable(${PROJECT_NAME} ${SOURCES})

# Files that conflict under unity builds (ODR: duplicate static symbols across files)
set_source_files_properties(
    src/screens/gov_data/providers/us_treasury.cpp
    src/screens/gov_data/providers/us_congress.cpp
    src/screens/gov_data/providers/canada_gov.cpp
    src/screens/gov_data/providers/openafrica.cpp
    src/screens/gov_data/providers/spain_data.cpp
    src/screens/gov_data/providers/pxweb.cpp
    src/screens/gov_data/providers/swiss_gov.cpp
    src/screens/gov_data/providers/french_gov.cpp
    src/screens/gov_data/providers/universal_ckan.cpp
    src/screens/gov_data/providers/data_gov_hk.cpp
    src/screens/research/research_screen.cpp
    src/screens/research/overview_panel.cpp
    src/screens/research/financials_panel.cpp
    src/screens/research/analysis_panel.cpp
    src/screens/research/technicals_panel.cpp
    src/screens/research/news_panel.cpp
    src/media/mpv_player.cpp
    src/voice/audio_capture.cpp
    src/voice/audio_player.cpp
    src/voice/tts_engine.cpp
    src/screens/surface_analytics/demo_data.cpp
    src/screens/surface_analytics/demo_data_derivatives.cpp
    src/screens/surface_analytics/demo_data_fixed_income.cpp
    src/screens/surface_analytics/demo_data_fx.cpp
    src/screens/surface_analytics/demo_data_credit.cpp
    src/screens/surface_analytics/demo_data_commodities.cpp
    src/screens/surface_analytics/demo_data_risk.cpp
    src/screens/surface_analytics/demo_data_macro.cpp
    src/screens/surface_analytics/surface_screen.cpp
    src/screens/surface_analytics/surface_3d_renderer.cpp
    src/screens/surface_analytics/surface_tables.cpp
    src/screens/surface_analytics/panels_derivatives.cpp
    src/screens/surface_analytics/panels_fixed_income.cpp
    src/screens/surface_analytics/panels_fx.cpp
    src/screens/surface_analytics/panels_credit.cpp
    src/screens/surface_analytics/panels_commodities.cpp
    src/screens/surface_analytics/panels_risk.cpp
    src/screens/surface_analytics/panels_macro.cpp
    src/screens/surface_analytics/tables_extended.cpp
    src/screens/surface_analytics/csv_importer.cpp
    src/screens/surface_analytics/import_dialog.cpp
    PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/fonts
    ${CMAKE_CURRENT_SOURCE_DIR}/src/vendor/imnodes
    ${STB_INCLUDE_DIRS}
    ${MINIAUDIO_INCLUDE_DIRS}
)

# mpv include path
if(FINCEPT_HAS_MPV)
    target_include_directories(${PROJECT_NAME} PRIVATE ${MPV_INCLUDE_DIR})
endif()

# =============================================================================
# Precompiled Header (PCH) — compiles heavy headers once
# =============================================================================
option(FINCEPT_USE_PCH "Enable precompiled headers for faster builds" ON)
if(FINCEPT_USE_PCH)
    target_precompile_headers(${PROJECT_NAME} PRIVATE
        # C++ standard library (used everywhere)
        <string>
        <vector>
        <unordered_map>
        <unordered_set>
        <mutex>
        <thread>
        <atomic>
        <functional>
        <chrono>
        <memory>
        <optional>
        <algorithm>
        <cstdio>
        <cmath>
        <sstream>
        <fstream>
        # Heavy third-party headers
        <nlohmann/json.hpp>
    )
endif()

# =============================================================================
# Compiler cache (sccache/ccache) — caches compiled objects across builds
# =============================================================================
find_program(CCACHE_PROGRAM ccache)
find_program(SCCACHE_PROGRAM sccache)
if(SCCACHE_PROGRAM)
    message(STATUS "Using sccache: ${SCCACHE_PROGRAM}")
    set(CMAKE_C_COMPILER_LAUNCHER ${SCCACHE_PROGRAM})
    set(CMAKE_CXX_COMPILER_LAUNCHER ${SCCACHE_PROGRAM})
elseif(CCACHE_PROGRAM)
    message(STATUS "Using ccache: ${CCACHE_PROGRAM}")
    set(CMAKE_C_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
    set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
endif()

# =============================================================================
# Compiler warnings (enable for code quality, CI enforcement)
# =============================================================================
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W3 /FS /MP)
    target_compile_definitions(${PROJECT_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()

# =============================================================================
# Static Analysis / Linting (opt-in for fast CI and local quality checks)
# Usage:
#   cmake -DFINCEPT_LINT_CPPCHECK=ON ..    # enable cppcheck on build
#   cmake -DFINCEPT_LINT_CLANG_TIDY=ON ..  # enable clang-tidy on build
#   cmake --build build --target lint       # run cppcheck on all sources
# =============================================================================
option(FINCEPT_LINT_CPPCHECK   "Run cppcheck during build"   OFF)
option(FINCEPT_LINT_CLANG_TIDY "Run clang-tidy during build" OFF)

# --- cppcheck: fast, low false-positive bug detection ---
if(FINCEPT_LINT_CPPCHECK)
    find_program(CPPCHECK_EXE NAMES cppcheck)
    if(CPPCHECK_EXE)
        message(STATUS "cppcheck enabled: ${CPPCHECK_EXE}")
        set(CMAKE_CXX_CPPCHECK
            ${CPPCHECK_EXE}
            --enable=warning,performance,portability
            --suppress=missingIncludeSystem
            --inline-suppr
            --quiet
            --std=c++20
        )
    else()
        message(WARNING "FINCEPT_LINT_CPPCHECK=ON but cppcheck not found")
    endif()
endif()

# --- clang-tidy: deep analysis using .clang-tidy config ---
if(FINCEPT_LINT_CLANG_TIDY)
    find_program(CLANG_TIDY_EXE NAMES clang-tidy)
    if(CLANG_TIDY_EXE)
        message(STATUS "clang-tidy enabled: ${CLANG_TIDY_EXE}")
        set(CMAKE_CXX_CLANG_TIDY
            ${CLANG_TIDY_EXE}
            --extra-arg=-std=c++20
        )
    else()
        message(WARNING "FINCEPT_LINT_CLANG_TIDY=ON but clang-tidy not found")
    endif()
endif()

# Enable 32-bit Unicode (emoji support) and FreeType color glyphs
target_compile_definitions(${PROJECT_NAME} PRIVATE
    IMGUI_DEFINE_MATH_OPERATORS
    IMGUI_USE_WCHAR32
    IMGUI_ENABLE_FREETYPE
)

# mpv compile definition
if(FINCEPT_HAS_MPV)
    target_compile_definitions(${PROJECT_NAME} PRIVATE FINCEPT_HAS_MPV)
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE
    glfw
    OpenGL::GL
    CURL::libcurl
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    unofficial::sqlite3::sqlite3
    OpenSSL::SSL
    OpenSSL::Crypto
    imgui::imgui
    implot::implot
    yoga::yogacore
)

# mpv link library
if(FINCEPT_HAS_MPV)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${MPV_LIBRARY})
    if(NOT WIN32)
        target_link_directories(${PROJECT_NAME} PRIVATE ${MPV_LIBRARY_DIRS})
    endif()
endif()

# =============================================================================
# Platform-specific
# =============================================================================
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE ws2_32 crypt32 wldap32 sapi ole32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    # Hide console window in production (Windows GUI subsystem)
    # Use /SUBSYSTEM:WINDOWS with mainCRTStartup so standard main() still works
    set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE TRUE)
    target_link_options(${PROJECT_NAME} PRIVATE "/ENTRY:mainCRTStartup")
elseif(APPLE)
    find_library(COCOA_FRAMEWORK Cocoa)
    find_library(IOKIT_FRAMEWORK IOKit)
    find_library(COREVIDEO_FRAMEWORK CoreVideo)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        ${COCOA_FRAMEWORK}
        ${IOKIT_FRAMEWORK}
        ${COREVIDEO_FRAMEWORK}
        "-framework OpenGL"
        "-framework Foundation"
    )
    target_compile_definitions(${PROJECT_NAME} PRIVATE GL_SILENCE_DEPRECATION)
    # macOS notification bridge (Obj-C++)
    target_sources(${PROJECT_NAME} PRIVATE src/core/notification_mac.mm)
elseif(UNIX)
    # Linux
    find_package(PkgConfig REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        pthread
        dl
    )
endif()

# =============================================================================
# Copy mpv DLL to build directory (Windows only — Linux/macOS use system lib)
# =============================================================================
if(FINCEPT_HAS_MPV AND WIN32)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${MPV_DIR}/libmpv-2.dll"
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
    )
endif()

# =============================================================================
# Copy fonts to build directory
# =============================================================================
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/fonts
    $<TARGET_FILE_DIR:${PROJECT_NAME}>/fonts
)

# =============================================================================
# Copy resources (requirements files) to build directory
# =============================================================================
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/resources
    $<TARGET_FILE_DIR:${PROJECT_NAME}>/resources
)

# =============================================================================
# Copy Python scripts to build directory
# =============================================================================
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/scripts
    $<TARGET_FILE_DIR:${PROJECT_NAME}>/scripts
)

# =============================================================================
# Install rules (for packaging)
# =============================================================================
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/fonts DESTINATION bin/fonts)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/resources DESTINATION bin/resources)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/scripts DESTINATION bin/scripts)

# =============================================================================
# Test: standalone equity trading UI test (AngelOne broker auth)
# =============================================================================
add_executable(TestEquityUI tests/test_equity_ui.cpp)
set_target_properties(TestEquityUI PROPERTIES CXX_STANDARD 20)
target_link_libraries(TestEquityUI PRIVATE
    glfw
    OpenGL::GL
    imgui::imgui
)
if(WIN32)
    target_link_libraries(TestEquityUI PRIVATE ws2_32)
    target_compile_definitions(TestEquityUI PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

# =============================================================================
# Lint targets (on-demand — run without rebuilding the whole project)
#   cmake --build build --target lint         # fast cppcheck scan
#   cmake --build build --target lint-tidy    # deep clang-tidy analysis
# =============================================================================
find_program(CPPCHECK_EXE_STANDALONE NAMES cppcheck)
if(CPPCHECK_EXE_STANDALONE)
    add_custom_target(lint
        COMMAND ${CPPCHECK_EXE_STANDALONE}
            --enable=warning,performance,portability
            --suppress=missingIncludeSystem
            --suppress=unusedFunction
            --inline-suppr
            --quiet
            --std=c++20
            -I ${CMAKE_SOURCE_DIR}/src
            ${CMAKE_SOURCE_DIR}/src
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running cppcheck static analysis on src/..."
        VERBATIM
    )
endif()
