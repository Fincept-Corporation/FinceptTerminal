cmake_minimum_required(VERSION 3.20)
project(FinceptTerminal VERSION 4.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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
find_path(STB_INCLUDE_DIRS "stb_image.h")

# OpenGL
find_package(OpenGL REQUIRED)

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

    # HTTP & Storage
    src/http/http_client.cpp
    src/storage/database.cpp
    src/storage/crypto.cpp

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
    src/screens/surface_analytics/surface_screen.cpp

    # UI — Tile Map
    src/ui/tile_map.cpp

    # Vendor — imnodes (node graph editor)
    src/vendor/imnodes/imnodes.cpp

    # Screens — Geopolitics
    src/screens/geopolitics/geopolitics_data.cpp
    src/screens/geopolitics/geopolitics_screen.cpp

    # Screens — Crypto Trading
    src/screens/crypto_trading/crypto_trading_screen.cpp

    # Screens — Node Editor
    src/screens/node_editor/node_registry.cpp
    src/screens/node_editor/node_defs.cpp
    src/screens/node_editor/editor_state.cpp
    src/screens/node_editor/workflow_store.cpp
    src/screens/node_editor/workflow_executor.cpp
    src/screens/node_editor/node_executor.cpp
    src/screens/node_editor/node_editor_screen.cpp
    src/screens/node_editor/workflow_templates.cpp

    # Screens — Agent Studio
    src/screens/agent_studio/agent_service.cpp
    src/screens/agent_studio/agent_discovery.cpp
    src/screens/agent_studio/agent_studio_screen.cpp

    # Screens — QuantLib
    src/screens/quantlib/quantlib_api.cpp
    src/screens/quantlib/quantlib_screen.cpp

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

    # Screens — Code Editor
    src/screens/code_editor/notebook_io.cpp
    src/screens/code_editor/code_editor_screen.cpp

    # Python bridge
    src/python/python_runner.cpp
    src/python/setup_manager.cpp
    src/python/setup_screen.cpp

    # Core (continued)
    src/core/device_id.cpp
)

# =============================================================================
# Executable
# =============================================================================
add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/fonts
    ${CMAKE_CURRENT_SOURCE_DIR}/src/vendor/imnodes
    ${STB_INCLUDE_DIRS}
)

# =============================================================================
# Compiler warnings (enable for code quality, CI enforcement)
# =============================================================================
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W3)
    target_compile_definitions(${PROJECT_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE
    glfw
    OpenGL::GL
    CURL::libcurl
    nlohmann_json::nlohmann_json
    unofficial::sqlite3::sqlite3
    OpenSSL::SSL
    OpenSSL::Crypto
    imgui::imgui
    implot::implot
    yoga::yogacore
)

# =============================================================================
# Platform-specific
# =============================================================================
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE ws2_32 crypt32 wldap32)
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
    )
    target_compile_definitions(${PROJECT_NAME} PRIVATE GL_SILENCE_DEPRECATION)
elseif(UNIX)
    # Linux
    find_package(PkgConfig REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        pthread
        dl
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
