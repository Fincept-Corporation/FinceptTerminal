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
