# Fincept Terminal — test suite (opt-in)
#
# Enable by configuring with -DFINCEPT_BUILD_TESTS=ON. Disabled by default so
# normal dev / release builds remain unchanged. Each test is a self-contained
# Qt Test executable that links the minimum subset of sources it needs
# (no linking against the full FinceptTerminal target — keeps tests fast).

find_package(Qt6 ${FINCEPT_QT_VERSION} EXACT REQUIRED COMPONENTS Test)

enable_testing()

# ── DataHub tests ────────────────────────────────────────────────────────────
# Links against the datahub sources and the Logger (a direct dependency).

add_executable(test_datahub
    datahub/test_datahub.cpp
    ../src/datahub/DataHub.cpp
    ../src/core/logging/Logger.cpp
)

target_include_directories(test_datahub PRIVATE
    ../src
)

target_link_libraries(test_datahub PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

# Tests shouldn't need the app's unity build settings.
set_target_properties(test_datahub PROPERTIES
    UNITY_BUILD OFF
    AUTOMOC ON
)

add_test(NAME datahub COMMAND test_datahub)

# ── MCP schema validator tests (Phase 3) ────────────────────────────────────
# Header-only ToolSchema/ToolSchemaBuilder + the SchemaValidator translation unit.
# No dependency on the rest of the MCP system — McpProvider/McpService are not linked.

add_executable(test_validator
    mcp/test_validator.cpp
    ../src/mcp/SchemaValidator.cpp
)

target_include_directories(test_validator PRIVATE
    ../src
)

target_link_libraries(test_validator PRIVATE
    Qt6::Core
    Qt6::Test
)

set_target_properties(test_validator PROPERTIES
    UNITY_BUILD OFF
    AUTOMOC ON
)

add_test(NAME mcp_validator COMMAND test_validator)

# ── MCP async dispatch tests (Phase 4) ──────────────────────────────────────
# Links McpProvider + SchemaValidator + Logger. Doesn't link McpService /
# McpManager / external client stack — async tests don't need them.

add_executable(test_async
    mcp/test_async.cpp
    ../src/mcp/McpProvider.cpp
    ../src/mcp/SchemaValidator.cpp
    ../src/core/logging/Logger.cpp
)

target_include_directories(test_async PRIVATE
    ../src
)

target_link_libraries(test_async PRIVATE
    Qt6::Core
    Qt6::Test
)

set_target_properties(test_async PROPERTIES
    UNITY_BUILD OFF
    AUTOMOC ON
)

add_test(NAME mcp_async COMMAND test_async)

# ── MCP dispatcher tests (Phase 5) ──────────────────────────────────────────
# Test source kept (mcp/test_dispatcher.cpp) but executable target is disabled:
# wiring it up requires linking the full McpManager + storage stack
# (McpServerRepository → Database.h → QSqlDatabase + every other repository
# transitively pulled). That blows up the test footprint without buying
# coverage Phase 5 doesn't already get from integration. Re-enable when
# McpService gains a dependency-injectable external-dispatch hook.
#
# add_executable(test_dispatcher
#     mcp/test_dispatcher.cpp
#     ...
# )
# add_test(NAME mcp_dispatcher COMMAND test_dispatcher)

# ─── Alpha Arena tests (Phase 2/3/4/5/6/7/8) ───────────────────────────────
# Each test target links the minimum source set needed. See the corresponding
# test_*.cpp header comment for the slice it covers.

# Helper: schema/JSON parsing (Phase 2). No Qt SQL, no engine.
add_executable(test_aa_schema_roundtrip
    alpha_arena/test_schema_roundtrip.cpp
    ../src/services/alpha_arena/AlphaArenaJson.cpp
)
target_include_directories(test_aa_schema_roundtrip PRIVATE ../src)
target_link_libraries(test_aa_schema_roundtrip PRIVATE Qt6::Core Qt6::Test)
set_target_properties(test_aa_schema_roundtrip PROPERTIES UNITY_BUILD OFF AUTOMOC ON)
add_test(NAME aa_schema_roundtrip COMMAND test_aa_schema_roundtrip)

# Schema-Python mirror — JSON + the C++ schema header (header-only).
add_executable(test_aa_schema_python_mirror
    alpha_arena/test_schema_python_mirror.cpp
)
target_include_directories(test_aa_schema_python_mirror PRIVATE ../src)
target_compile_definitions(test_aa_schema_python_mirror PRIVATE
    FINCEPT_SCHEMA_JSON="${CMAKE_SOURCE_DIR}/scripts/alpha_arena/schema.json")
target_link_libraries(test_aa_schema_python_mirror PRIVATE Qt6::Core Qt6::Test)
set_target_properties(test_aa_schema_python_mirror PROPERTIES UNITY_BUILD OFF AUTOMOC ON)
add_test(NAME aa_schema_python_mirror COMMAND test_aa_schema_python_mirror)

# Indicators (Phase 3) — pure C++ math.
add_executable(test_aa_indicators
    alpha_arena/test_indicators.cpp
    ../src/services/alpha_arena/Indicators.cpp
)
target_include_directories(test_aa_indicators PRIVATE ../src)
target_link_libraries(test_aa_indicators PRIVATE Qt6::Core Qt6::Test)
set_target_properties(test_aa_indicators PROPERTIES UNITY_BUILD OFF AUTOMOC ON)
add_test(NAME aa_indicators COMMAND test_aa_indicators)

# Context builder (Phase 3) — needs ContextBuilder + Schema (header-only).
add_executable(test_aa_context_builder
    alpha_arena/test_context_builder.cpp
    ../src/services/alpha_arena/ContextBuilder.cpp
)
target_include_directories(test_aa_context_builder PRIVATE ../src)
target_link_libraries(test_aa_context_builder PRIVATE Qt6::Core Qt6::Test)
set_target_properties(test_aa_context_builder PROPERTIES UNITY_BUILD OFF AUTOMOC ON)
add_test(NAME aa_context_builder COMMAND test_aa_context_builder)

# RiskEngine (Phase 5a) — pure function.
add_executable(test_aa_risk_engine
    alpha_arena/test_risk_engine.cpp
    ../src/services/alpha_arena/RiskEngine.cpp
)
target_include_directories(test_aa_risk_engine PRIVATE ../src)
target_link_libraries(test_aa_risk_engine PRIVATE Qt6::Core Qt6::Test)
set_target_properties(test_aa_risk_engine PROPERTIES UNITY_BUILD OFF AUTOMOC ON)
add_test(NAME aa_risk_engine COMMAND test_aa_risk_engine)

# PaperVenue (Phase 5b) — needs PaperVenue + IExchangeVenue header + Logger.
add_executable(test_aa_paper_venue
    alpha_arena/test_paper_venue.cpp
    ../src/services/alpha_arena/PaperVenue.cpp
    ../src/core/logging/Logger.cpp
)
target_include_directories(test_aa_paper_venue PRIVATE ../src)
target_link_libraries(test_aa_paper_venue PRIVATE Qt6::Core Qt6::Test)
set_target_properties(test_aa_paper_venue PROPERTIES UNITY_BUILD OFF AUTOMOC ON)
add_test(NAME aa_paper_venue COMMAND test_aa_paper_venue)

# TickClock (Phase 6) — pure scheduler.
add_executable(test_aa_tick_clock
    alpha_arena/test_tick_clock.cpp
    ../src/services/alpha_arena/TickClock.cpp
    ../src/core/logging/Logger.cpp
)
target_include_directories(test_aa_tick_clock PRIVATE ../src)
target_link_libraries(test_aa_tick_clock PRIVATE Qt6::Core Qt6::Test)
set_target_properties(test_aa_tick_clock PROPERTIES UNITY_BUILD OFF AUTOMOC ON)
add_test(NAME aa_tick_clock COMMAND test_aa_tick_clock)

# HyperliquidSigner (Phase 5c) — header + cpp; pure function until secp256k1
# lands.
add_executable(test_aa_hyperliquid_signer
    alpha_arena/test_hyperliquid_signer.cpp
    ../src/trading/exchanges/hyperliquid/HyperliquidSigner.cpp
)
target_include_directories(test_aa_hyperliquid_signer PRIVATE ../src)
target_link_libraries(test_aa_hyperliquid_signer PRIVATE Qt6::Core Qt6::Test)
set_target_properties(test_aa_hyperliquid_signer PROPERTIES UNITY_BUILD OFF AUTOMOC ON)
add_test(NAME aa_hyperliquid_signer COMMAND test_aa_hyperliquid_signer)

# Repository tests (Phase 4) — link Database + migration runner + the v024
# migration + AlphaArenaRepo + Logger.
set(_AA_REPO_SOURCES
    ../src/services/alpha_arena/AlphaArenaRepo.cpp
    ../src/storage/sqlite/Database.cpp
    ../src/storage/sqlite/migrations/MigrationRunner.cpp
    ../src/storage/sqlite/migrations/v024_alpha_arena.cpp
    ../src/core/logging/Logger.cpp
)

add_executable(test_aa_repo
    alpha_arena/test_repo.cpp
    ${_AA_REPO_SOURCES}
)
target_include_directories(test_aa_repo PRIVATE ../src)
target_link_libraries(test_aa_repo PRIVATE Qt6::Core Qt6::Sql Qt6::Test)
set_target_properties(test_aa_repo PROPERTIES UNITY_BUILD OFF AUTOMOC ON)
add_test(NAME aa_repo COMMAND test_aa_repo)

# Ops (Phase 8) — same source set as repo tests.
add_executable(test_aa_ops
    alpha_arena/test_ops.cpp
    ${_AA_REPO_SOURCES}
)
target_include_directories(test_aa_ops PRIVATE ../src)
target_link_libraries(test_aa_ops PRIVATE Qt6::Core Qt6::Sql Qt6::Test)
set_target_properties(test_aa_ops PROPERTIES UNITY_BUILD OFF AUTOMOC ON)
add_test(NAME aa_ops COMMAND test_aa_ops)

# 50-tick replay (CI gate) — same source set as repo tests.
add_executable(test_aa_50tick_replay
    alpha_arena/test_50tick_replay.cpp
    ${_AA_REPO_SOURCES}
)
target_include_directories(test_aa_50tick_replay PRIVATE ../src)
target_link_libraries(test_aa_50tick_replay PRIVATE Qt6::Core Qt6::Sql Qt6::Test)
set_target_properties(test_aa_50tick_replay PROPERTIES UNITY_BUILD OFF AUTOMOC ON)
add_test(NAME aa_50tick_replay COMMAND test_aa_50tick_replay)

# Panels lifecycle — needs the panel cpps + repo + Database.
# StatePanels.cpp includes AlphaArenaEngine.h which transitively pulls
# SecretRedemptionServer.h (QLocalServer), so Qt6::Network must be linked.
add_executable(test_aa_panels_lifecycle
    alpha_arena/test_panels_lifecycle.cpp
    ../src/screens/alpha_arena/panels/LeaderboardPanel.cpp
    ../src/screens/alpha_arena/panels/ModelChatPanel.cpp
    ../src/screens/alpha_arena/panels/StatePanels.cpp
    ${_AA_REPO_SOURCES}
)
target_include_directories(test_aa_panels_lifecycle PRIVATE ../src)
target_link_libraries(test_aa_panels_lifecycle PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Sql Qt6::Network Qt6::Test)
set_target_properties(test_aa_panels_lifecycle PROPERTIES UNITY_BUILD OFF AUTOMOC ON)
add_test(NAME aa_panels_lifecycle COMMAND test_aa_panels_lifecycle)
