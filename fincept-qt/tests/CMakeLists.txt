include(CTest)

find_package(Qt6 ${_FINCEPT_QT_VER_ARGS} REQUIRED COMPONENTS Test)

add_library(fincept_phase_one_test_support INTERFACE)
target_compile_features(fincept_phase_one_test_support INTERFACE cxx_std_20)
target_include_directories(fincept_phase_one_test_support INTERFACE
    "${PROJECT_SOURCE_DIR}/src"
    "${OPENSSL_INCLUDE_DIR}"
    "${md4c_SOURCE_DIR}/src"
)
target_compile_definitions(fincept_phase_one_test_support INTERFACE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    FINCEPT_VERSION_STRING="${CMAKE_PROJECT_VERSION}"
)

if(FINCEPT_HAS_PRIVATE_HEADERS)
    target_compile_definitions(fincept_phase_one_test_support INTERFACE FINCEPT_HAS_QXLSX)
endif()

if(HAS_WEBSOCKETS)
    target_compile_definitions(fincept_phase_one_test_support INTERFACE HAS_QT_WEBSOCKETS)
endif()

if(HAS_MULTIMEDIA)
    target_compile_definitions(fincept_phase_one_test_support INTERFACE HAS_QT_MULTIMEDIA)
endif()

if(HAS_TTS)
    target_compile_definitions(fincept_phase_one_test_support INTERFACE HAS_QT_TTS)
endif()

function(fincept_add_phase_one_test target)
    cmake_parse_arguments(PARSE_ARGV 1 PHASE1 "WIDGETS" "" "SOURCES;LINK_LIBS;EXTRA_SOURCES")

    add_executable(${target}
        ${PHASE1_SOURCES}
        ${PHASE1_EXTRA_SOURCES}
    )

    set_target_properties(${target} PROPERTIES UNITY_BUILD OFF)

    target_link_libraries(${target} PRIVATE
        fincept_phase_one_test_support
        Qt6::Test
        Qt6::Core
        ${PHASE1_LINK_LIBS}
    )

    add_test(NAME ${target} COMMAND ${target})

    set(test_environment)
    if(PHASE1_WIDGETS)
        list(APPEND test_environment "QT_QPA_PLATFORM=offscreen")
    endif()
    if(target STREQUAL "fincept_auth_server_runtime_tests")
        list(APPEND test_environment "FINCEPT_TERMINAL_BINARY=$<TARGET_FILE:FinceptTerminal>")
    endif()
    if(test_environment)
        set_tests_properties(${target} PROPERTIES ENVIRONMENT "${test_environment}")
    endif()
endfunction()

set(PHASE1_MIGRATION_TEST_SUPPORT_SOURCES
    "${PROJECT_SOURCE_DIR}/src/core/logging/Logger.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/CacheDatabase.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/MigrationRunner.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v001_initial.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v002_llm_chat.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v003_data_mcp_agents.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v004_cleanup.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v005_dashboard_layout.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v006_portfolio_multi.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v007_news_baselines.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v008_workflows.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v009_instruments.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v010_llm_profiles.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v011_custom_indices.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v012_data_normalization.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v013_news_articles.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v014_llm_tools_toggle.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v015_screen_state.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v016_broker_accounts.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v017_perf_indexes.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v018_widget_config.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v019_sector_cache.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v020_security_audit_log.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v021_max_tokens_default.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v022_portfolio_broker_link.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v023_algo_trading_deploy.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v024_alpha_arena.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v025_oi_snapshots.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v026_strategies.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v027_fii_dii.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v028_iv_history.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v029_secure_credentials.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v030_max_tool_rounds.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v031_rss_feeds.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v032_multiuser_phase_one.cpp"
    "${PROJECT_SOURCE_DIR}/src/app/PhaseOneMigrationRegistry.cpp"
)

set(PHASE1_ENDPOINT_TEST_SUPPORT_SOURCES
    "${PROJECT_SOURCE_DIR}/src/core/config/AppConfig.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/config/AppPaths.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/config/ProfileManager.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/logging/Logger.cpp"
    "${PROJECT_SOURCE_DIR}/src/multiuser/client/PhaseOneEndpointProvider.cpp"
)

set(PHASE1_CLIENT_TEST_SUPPORT_SOURCES
    ${PHASE1_ENDPOINT_TEST_SUPPORT_SOURCES}
    "${PROJECT_SOURCE_DIR}/src/network/http/HttpClient.cpp"
    "${PROJECT_SOURCE_DIR}/src/auth/AuthApi.cpp"
    "${PROJECT_SOURCE_DIR}/src/multiuser/client/PhaseOneClientTransport.cpp"
    "${PROJECT_SOURCE_DIR}/src/multiuser/client/PhaseOneSessionStateGuard.cpp"
    "${PROJECT_SOURCE_DIR}/src/multiuser/client/PhaseOneUserAdminApi.cpp"
    "${PROJECT_SOURCE_DIR}/src/multiuser/client/PhaseOneAuditApi.cpp"
)

set(PHASE1_SERVER_RUNTIME_TEST_SUPPORT_SOURCES
    "${PROJECT_SOURCE_DIR}/src/app/PhaseOneServerCli.cpp"
    "${PROJECT_SOURCE_DIR}/src/app/PhaseOneMigrationRegistry.cpp"
    "${PROJECT_SOURCE_DIR}/src/app/PhaseOneServerPaths.cpp"
    "${PROJECT_SOURCE_DIR}/src/app/PhaseOneStartupCoordinator.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/config/AppConfig.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/config/AppPaths.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/config/ProfileManager.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/crash/CrashHandler.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/logging/Logger.cpp"
    "${PROJECT_SOURCE_DIR}/src/multiuser/client/PhaseOneEndpointProvider.cpp"
    "${PROJECT_SOURCE_DIR}/src/multiuser/client/PhaseOneClientTransport.cpp"
    "${PROJECT_SOURCE_DIR}/src/multiuser/client/PhaseOneSessionStateGuard.cpp"
    "${PROJECT_SOURCE_DIR}/src/multiuser/client/PhaseOnePortfolioApi.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/repositories/PortfolioRepository.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/repositories/PortfolioHoldingsRepository.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/CacheDatabase.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/Database.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/MigrationRunner.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v001_initial.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v002_llm_chat.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v003_data_mcp_agents.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v004_cleanup.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v005_dashboard_layout.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v006_portfolio_multi.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v007_news_baselines.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v008_workflows.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v009_instruments.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v010_llm_profiles.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v011_custom_indices.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v012_data_normalization.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v013_news_articles.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v014_llm_tools_toggle.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v015_screen_state.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v016_broker_accounts.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v017_perf_indexes.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v018_widget_config.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v019_sector_cache.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v020_security_audit_log.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v021_max_tokens_default.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v022_portfolio_broker_link.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v023_algo_trading_deploy.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v024_alpha_arena.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v025_oi_snapshots.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v026_strategies.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v027_fii_dii.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v028_iv_history.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v029_secure_credentials.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v030_max_tool_rounds.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v031_rss_feeds.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v032_multiuser_phase_one.cpp"
    "${PROJECT_SOURCE_DIR}/src/multiuser/server/PhaseOneServerRuntime.cpp"
    "${PROJECT_SOURCE_DIR}/src/multiuser/server/PhaseOneAuthServer.cpp"
    "${PROJECT_SOURCE_DIR}/src/multiuser/server/PhaseOnePasswordHasher.cpp"
    "${PROJECT_SOURCE_DIR}/src/multiuser/server/PhaseOnePortfolioServer.cpp"
    "${PROJECT_SOURCE_DIR}/src/multiuser/server/PhaseOneUserAdminCommands.cpp"
    "${PROJECT_SOURCE_DIR}/src/multiuser/server/PhaseOneUserAdminServer.cpp"
    "${PROJECT_SOURCE_DIR}/src/multiuser/server/PhaseOneAuditServer.cpp"
    "${PROJECT_SOURCE_DIR}/src/multiuser/server/storage/PhaseOneUserRepository.cpp"
    "${PROJECT_SOURCE_DIR}/src/multiuser/server/storage/PhaseOneSessionRepository.cpp"
    "${PROJECT_SOURCE_DIR}/src/multiuser/server/storage/PhaseOneAuditRepository.cpp"
    "${PROJECT_SOURCE_DIR}/src/multiuser/server/http/PhaseOneHttpJsonResponse.cpp"
    "${PROJECT_SOURCE_DIR}/src/multiuser/server/http/PhaseOneHttpRequestContext.cpp"
    "${PROJECT_SOURCE_DIR}/src/multiuser/server/http/PhaseOneHttpServer.cpp"
    "${PROJECT_SOURCE_DIR}/src/multiuser/server/http/PhaseOneAuthHttpRoutes.cpp"
    "${PROJECT_SOURCE_DIR}/src/multiuser/server/http/PhaseOneUserAdminHttpRoutes.cpp"
    "${PROJECT_SOURCE_DIR}/src/multiuser/server/http/PhaseOneAuditHttpRoutes.cpp"
    "${PROJECT_SOURCE_DIR}/src/multiuser/server/http/PhaseOnePortfolioHttpRoutes.cpp"
)

set(PHASE1_SERVER_TEST_SUPPORT_SOURCES
    "${PROJECT_SOURCE_DIR}/src/core/logging/Logger.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/Database.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/CacheDatabase.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/MigrationRunner.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v001_initial.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v002_llm_chat.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v003_data_mcp_agents.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v004_cleanup.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v005_dashboard_layout.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v006_portfolio_multi.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v007_news_baselines.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v008_workflows.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v009_instruments.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v010_llm_profiles.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v011_custom_indices.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v012_data_normalization.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v013_news_articles.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v014_llm_tools_toggle.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v015_screen_state.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v016_broker_accounts.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v017_perf_indexes.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v018_widget_config.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v019_sector_cache.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v020_security_audit_log.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v021_max_tokens_default.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v022_portfolio_broker_link.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v023_algo_trading_deploy.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v024_alpha_arena.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v025_oi_snapshots.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v026_strategies.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v027_fii_dii.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v028_iv_history.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v029_secure_credentials.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v030_max_tool_rounds.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v031_rss_feeds.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v032_multiuser_phase_one.cpp"
    "${PROJECT_SOURCE_DIR}/src/app/PhaseOneMigrationRegistry.cpp"
    "${PROJECT_SOURCE_DIR}/src/multiuser/server/PhaseOneAuthServer.cpp"
    "${PROJECT_SOURCE_DIR}/src/multiuser/server/PhaseOnePasswordHasher.cpp"
    "${PROJECT_SOURCE_DIR}/src/multiuser/server/PhaseOneUserAdminCommands.cpp"
    "${PROJECT_SOURCE_DIR}/src/multiuser/server/PhaseOneUserAdminServer.cpp"
    "${PROJECT_SOURCE_DIR}/src/multiuser/server/PhaseOneAuditServer.cpp"
    "${PROJECT_SOURCE_DIR}/src/multiuser/server/storage/PhaseOneUserRepository.cpp"
    "${PROJECT_SOURCE_DIR}/src/multiuser/server/storage/PhaseOneSessionRepository.cpp"
    "${PROJECT_SOURCE_DIR}/src/multiuser/server/storage/PhaseOneAuditRepository.cpp"
    "${PROJECT_SOURCE_DIR}/src/multiuser/server/http/PhaseOneHttpJsonResponse.cpp"
    "${PROJECT_SOURCE_DIR}/src/multiuser/server/http/PhaseOneHttpRequestContext.cpp"
    "${PROJECT_SOURCE_DIR}/src/multiuser/server/http/PhaseOneHttpServer.cpp"
    "${PROJECT_SOURCE_DIR}/src/multiuser/server/http/PhaseOneAuthHttpRoutes.cpp"
    "${PROJECT_SOURCE_DIR}/src/multiuser/server/http/PhaseOneUserAdminHttpRoutes.cpp"
    "${PROJECT_SOURCE_DIR}/src/multiuser/server/http/PhaseOneAuditHttpRoutes.cpp"
)

set(PHASE1_HTTP_HELPER_TEST_SUPPORT_SOURCES
    "${PROJECT_SOURCE_DIR}/src/multiuser/server/http/PhaseOneHttpJsonResponse.cpp"
    "${PROJECT_SOURCE_DIR}/src/multiuser/server/http/PhaseOneHttpRequestContext.cpp"
    "${PROJECT_SOURCE_DIR}/src/multiuser/server/http/PhaseOneHttpServer.cpp"
    "${PROJECT_SOURCE_DIR}/src/multiuser/server/http/PhaseOneAuthHttpRoutes.cpp"
    "${PROJECT_SOURCE_DIR}/src/multiuser/server/http/PhaseOneUserAdminHttpRoutes.cpp"
    "${PROJECT_SOURCE_DIR}/src/multiuser/server/http/PhaseOneAuditHttpRoutes.cpp"
    "${PROJECT_SOURCE_DIR}/src/multiuser/server/PhaseOneAuthServer.cpp"
    "${PROJECT_SOURCE_DIR}/src/multiuser/server/PhaseOnePasswordHasher.cpp"
    "${PROJECT_SOURCE_DIR}/src/multiuser/server/PhaseOneUserAdminCommands.cpp"
    "${PROJECT_SOURCE_DIR}/src/multiuser/server/PhaseOneUserAdminServer.cpp"
    "${PROJECT_SOURCE_DIR}/src/multiuser/server/PhaseOneAuditServer.cpp"
    "${PROJECT_SOURCE_DIR}/src/multiuser/server/storage/PhaseOneUserRepository.cpp"
    "${PROJECT_SOURCE_DIR}/src/multiuser/server/storage/PhaseOneSessionRepository.cpp"
    "${PROJECT_SOURCE_DIR}/src/multiuser/server/storage/PhaseOneAuditRepository.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/logging/Logger.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/Database.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/CacheDatabase.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/MigrationRunner.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v001_initial.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v002_llm_chat.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v003_data_mcp_agents.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v004_cleanup.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v005_dashboard_layout.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v006_portfolio_multi.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v007_news_baselines.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v008_workflows.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v009_instruments.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v010_llm_profiles.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v011_custom_indices.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v012_data_normalization.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v013_news_articles.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v014_llm_tools_toggle.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v015_screen_state.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v016_broker_accounts.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v017_perf_indexes.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v018_widget_config.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v019_sector_cache.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v020_security_audit_log.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v021_max_tokens_default.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v022_portfolio_broker_link.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v023_algo_trading_deploy.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v024_alpha_arena.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v025_oi_snapshots.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v026_strategies.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v027_fii_dii.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v028_iv_history.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v029_secure_credentials.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v030_max_tool_rounds.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v031_rss_feeds.cpp"
    "${PROJECT_SOURCE_DIR}/src/storage/sqlite/migrations/v032_multiuser_phase_one.cpp"
)

fincept_add_phase_one_test(fincept_auth_user_admin_tests
    SOURCES
        "${PROJECT_SOURCE_DIR}/tests/auth/test_phase_one_user_admin.cpp"
        "${PROJECT_SOURCE_DIR}/tests/auth/test_phase_one_audit_admin.cpp"
    EXTRA_SOURCES ${PHASE1_SERVER_TEST_SUPPORT_SOURCES}
    LINK_LIBS
        Qt6::Sql
        Qt6::Network
)

fincept_add_phase_one_test(fincept_auth_login_tests
    SOURCES "${PROJECT_SOURCE_DIR}/tests/auth/test_phase_one_auth_login.cpp"
    EXTRA_SOURCES
        ${PHASE1_CLIENT_TEST_SUPPORT_SOURCES}
        ${PHASE1_SERVER_TEST_SUPPORT_SOURCES}
    LINK_LIBS Qt6::Network Qt6::Sql
)

fincept_add_phase_one_test(fincept_auth_session_tests
    SOURCES "${PROJECT_SOURCE_DIR}/tests/auth/test_phase_one_auth_session.cpp"
    EXTRA_SOURCES
        ${PHASE1_SERVER_TEST_SUPPORT_SOURCES}
        "${PROJECT_SOURCE_DIR}/src/auth/PhaseOneSessionAuthBridge.cpp"
    LINK_LIBS Qt6::Network Qt6::Sql
)

fincept_add_phase_one_test(fincept_auth_session_ui_tests
    WIDGETS
    SOURCES "${PROJECT_SOURCE_DIR}/tests/auth/test_phase_one_auth_session_ui.cpp"
    EXTRA_SOURCES
        "${PROJECT_SOURCE_DIR}/src/auth/PhaseOneSessionAuthBridge.cpp"
        "${PROJECT_SOURCE_DIR}/src/multiuser/client/PhaseOneSessionStateGuard.cpp"
    LINK_LIBS Qt6::Widgets Qt6::Network Qt6::Sql
)

fincept_add_phase_one_test(fincept_auth_migration_tests
    SOURCES "${PROJECT_SOURCE_DIR}/tests/auth/test_phase_one_migrations.cpp"
    EXTRA_SOURCES ${PHASE1_MIGRATION_TEST_SUPPORT_SOURCES}
    LINK_LIBS Qt6::Sql
)

fincept_add_phase_one_test(fincept_auth_server_runtime_tests
    SOURCES "${PROJECT_SOURCE_DIR}/tests/auth/test_phase_one_server_runtime.cpp"
    EXTRA_SOURCES ${PHASE1_SERVER_RUNTIME_TEST_SUPPORT_SOURCES}
    LINK_LIBS Qt6::Widgets Qt6::Network Qt6::Sql
)

fincept_add_phase_one_test(fincept_auth_http_helper_tests
    SOURCES "${PROJECT_SOURCE_DIR}/tests/auth/test_phase_one_http_helpers.cpp"
    EXTRA_SOURCES ${PHASE1_HTTP_HELPER_TEST_SUPPORT_SOURCES}
    LINK_LIBS Qt6::Network Qt6::Sql
)

fincept_add_phase_one_test(fincept_auth_contract_tests
    SOURCES "${PROJECT_SOURCE_DIR}/tests/auth/test_phase_one_auth_contracts.cpp"
    EXTRA_SOURCES ${PHASE1_CLIENT_TEST_SUPPORT_SOURCES}
    LINK_LIBS Qt6::Network
)

fincept_add_phase_one_test(fincept_auth_admin_contract_tests
    SOURCES "${PROJECT_SOURCE_DIR}/tests/auth/test_phase_one_admin_contracts.cpp"
    EXTRA_SOURCES ${PHASE1_CLIENT_TEST_SUPPORT_SOURCES}
    LINK_LIBS Qt6::Network
)

fincept_add_phase_one_test(fincept_auth_portfolio_contract_tests
    SOURCES "${PROJECT_SOURCE_DIR}/tests/auth/test_phase_one_portfolio_contracts.cpp"
    EXTRA_SOURCES
        ${PHASE1_CLIENT_TEST_SUPPORT_SOURCES}
        ${PHASE1_SERVER_TEST_SUPPORT_SOURCES}
        "${PROJECT_SOURCE_DIR}/src/multiuser/client/PhaseOnePortfolioApi.cpp"
        "${PROJECT_SOURCE_DIR}/src/multiuser/server/PhaseOnePortfolioServer.cpp"
        "${PROJECT_SOURCE_DIR}/src/multiuser/server/http/PhaseOnePortfolioHttpRoutes.cpp"
        "${PROJECT_SOURCE_DIR}/src/storage/repositories/PortfolioRepository.cpp"
        "${PROJECT_SOURCE_DIR}/src/storage/repositories/PortfolioHoldingsRepository.cpp"
    LINK_LIBS Qt6::Network Qt6::Sql
)

fincept_add_phase_one_test(fincept_auth_endpoint_precedence_tests
    SOURCES "${PROJECT_SOURCE_DIR}/tests/auth/test_phase_one_endpoint_precedence.cpp"
    EXTRA_SOURCES ${PHASE1_ENDPOINT_TEST_SUPPORT_SOURCES}
    LINK_LIBS Qt6::Network
)

fincept_add_phase_one_test(fincept_auth_ui_flow_tests
    WIDGETS
    SOURCES "${PROJECT_SOURCE_DIR}/tests/auth/test_phase_one_auth_ui_flow.cpp"
    EXTRA_SOURCES ${PHASE1_UI_FLOW_TEST_SUPPORT_SOURCES}
    LINK_LIBS Qt6::Widgets
)

target_compile_definitions(fincept_auth_ui_flow_tests PRIVATE FINCEPT_PHASE_ONE_UI_FLOW_TESTS)
