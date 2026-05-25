# Phase 1 multi-user source buckets.

set(PHASE1_TEST_SOURCES
    tests/auth/test_phase_one_user_admin.cpp
    tests/auth/test_phase_one_audit_admin.cpp
    tests/auth/test_phase_one_auth_login.cpp
    tests/auth/test_phase_one_auth_session.cpp
    tests/auth/test_phase_one_auth_session_ui.cpp
    tests/auth/test_phase_one_migrations.cpp
    tests/auth/test_phase_one_server_runtime.cpp
    tests/auth/test_phase_one_http_helpers.cpp
    tests/auth/test_phase_one_auth_contracts.cpp
    tests/auth/test_phase_one_admin_contracts.cpp
    tests/auth/test_phase_one_endpoint_precedence.cpp
    tests/auth/test_phase_one_auth_ui_flow.cpp
    tests/auth/test_phase_one_portfolio_contracts.cpp
)

set(PHASE1_CLIENT_SOURCES
    src/multiuser/client/PhaseOneEndpointProvider.cpp
    src/multiuser/client/PhaseOneClientTransport.cpp
    src/multiuser/client/PhaseOneSessionStateGuard.cpp
    src/multiuser/client/PhaseOneUserAdminApi.cpp
    src/multiuser/client/PhaseOneAuditApi.cpp
    src/multiuser/client/PhaseOnePortfolioApi.cpp
)

set(PHASE1_SERVER_SOURCES
    src/multiuser/server/PhaseOneServerRuntime.cpp
    src/multiuser/server/PhaseOnePasswordHasher.cpp
    src/multiuser/server/PhaseOneUserAdminCommands.cpp
    src/multiuser/server/PhaseOneAuthServer.cpp
    src/multiuser/server/PhaseOneUserAdminServer.cpp
    src/multiuser/server/PhaseOneAuditServer.cpp
    src/multiuser/server/PhaseOnePortfolioServer.cpp
)

set(PHASE1_HTTP_SOURCES
    src/multiuser/server/http/PhaseOneHttpServer.cpp
    src/multiuser/server/http/PhaseOneHttpRequestContext.cpp
    src/multiuser/server/http/PhaseOneHttpJsonResponse.cpp
    src/multiuser/server/http/PhaseOneAuthHttpRoutes.cpp
    src/multiuser/server/http/PhaseOneUserAdminHttpRoutes.cpp
    src/multiuser/server/http/PhaseOneAuditHttpRoutes.cpp
    src/multiuser/server/http/PhaseOnePortfolioHttpRoutes.cpp
)

set(PHASE1_STORAGE_SOURCES
    src/storage/sqlite/migrations/v032_multiuser_phase_one.cpp
    src/multiuser/server/storage/PhaseOneUserRepository.cpp
    src/multiuser/server/storage/PhaseOneSessionRepository.cpp
    src/multiuser/server/storage/PhaseOneAuditRepository.cpp
)

set(PHASE1_APP_SOURCES
    # Raw CLI parsing and dedicated server bootstrap wiring.
    src/app/PhaseOneServerCli.cpp
    src/app/PhaseOneServerPaths.cpp
    src/app/PhaseOneClientLifecycle.cpp
    src/app/PhaseOneClientStartup.cpp
    src/app/PhaseOneClientUiStartup.cpp
    src/app/PhaseOneStartupCoordinator.cpp
    src/app/PhaseOneStartupCoordinator_Client.cpp
    src/app/PhaseOneMigrationRegistry.cpp
)

set(PHASE1_AUTH_SOURCES
    src/auth/PhaseOneAuthFlowBridge.cpp
    src/auth/PhaseOneAuthRecoveryBridge.cpp
    src/auth/PhaseOneSessionAuthBridge.cpp
)

set(PHASE1_SCREEN_SOURCES
    src/screens/auth/PhaseOneBootstrapScreen.cpp
)

set(PHASE1_UI_FLOW_TEST_SUPPORT_SOURCES
    "${PROJECT_SOURCE_DIR}/src/auth/PhaseOneAuthFlowBridge.cpp"
    "${PROJECT_SOURCE_DIR}/src/auth/PhaseOneSessionAuthBridge.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/logging/Logger.cpp"
    "${PROJECT_SOURCE_DIR}/src/screens/auth/PhaseOneBootstrapScreen.cpp"
    "${PROJECT_SOURCE_DIR}/src/screens/info/HelpScreen.cpp"
    "${PROJECT_SOURCE_DIR}/src/ui/theme/Theme.cpp"
    "${PROJECT_SOURCE_DIR}/src/ui/theme/ThemeManager.cpp"
    "${PROJECT_SOURCE_DIR}/src/ui/theme/StyleSheets.cpp"
)

list(APPEND NETWORK_SOURCES ${PHASE1_CLIENT_SOURCES})
list(APPEND NETWORK_SOURCES ${PHASE1_SERVER_SOURCES})
list(APPEND NETWORK_SOURCES ${PHASE1_HTTP_SOURCES})
list(APPEND AUTH_SOURCES ${PHASE1_AUTH_SOURCES})
list(APPEND STORAGE_SOURCES ${PHASE1_STORAGE_SOURCES})
list(APPEND APP_SOURCES ${PHASE1_APP_SOURCES})
list(APPEND SCREEN_SOURCES ${PHASE1_SCREEN_SOURCES})
