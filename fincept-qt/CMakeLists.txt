cmake_minimum_required(VERSION 3.27)
project(FinceptTerminal VERSION 4.0.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ── Pinned toolchain enforcement ─────────────────────────────────────────────
# Fincept Terminal ships on 3 platforms and cannot tolerate compiler-version
# drift. These checks fail the build early with an actionable message rather
# than letting the user hit a cryptic template error 10 minutes into compile.
if(MSVC AND MSVC_VERSION LESS 1938)
    message(FATAL_ERROR
        "MSVC 19.38 (Visual Studio 2022 17.8) or newer required.\n"
        "Detected MSVC_VERSION: ${MSVC_VERSION}\n"
        "Install VS 2022 17.8+ from https://visualstudio.microsoft.com/downloads/")
endif()
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 12.3)
    message(FATAL_ERROR
        "GCC 12.3 or newer required. Detected: ${CMAKE_CXX_COMPILER_VERSION}\n"
        "Ubuntu 22.04+ / Debian 12+ ship a compatible GCC.")
endif()
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 15.0)
    message(FATAL_ERROR
        "Clang 15.0 or newer required (Apple Clang 15.0 = Xcode 15.2).\n"
        "Detected: ${CMAKE_CXX_COMPILER_VERSION}")
endif()

# ── Compiler cache (ccache / sccache) ────────────────────────────────────────
# Detected at configure time. If present, routes every C/C++ compilation
# through the cache. Cuts clean rebuilds by 5-20x when source hasn't changed.
find_program(FINCEPT_CCACHE_PROGRAM NAMES ccache sccache)
if(FINCEPT_CCACHE_PROGRAM)
    set(CMAKE_CXX_COMPILER_LAUNCHER "${FINCEPT_CCACHE_PROGRAM}")
    set(CMAKE_C_COMPILER_LAUNCHER   "${FINCEPT_CCACHE_PROGRAM}")
    message(STATUS "Compiler cache enabled: ${FINCEPT_CCACHE_PROGRAM}")
endif()

# ── macOS rpath configuration ────────────────────────────────────────────────
# Set install rpaths so the binary looks inside its own bundle for frameworks.
# Without this, CMake embeds build-tree rpaths (e.g. _deps/qgeoview-build/lib)
# and CPack IFW fails trying to delete them after macdeployqt already rewrote them.
if(APPLE)
    set(CMAKE_MACOSX_RPATH ON)
    set(CMAKE_INSTALL_RPATH "@executable_path/../Frameworks;@executable_path/../lib")
    set(CMAKE_BUILD_WITH_INSTALL_RPATH OFF)
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH FALSE)
endif()

# Export compile_commands.json for clang-tidy
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Build performance ─────────────────────────────────────────────────────────
# Unity build for infrastructure layers (safe — no local symbol conflicts)
# Screens are excluded below via SKIP_UNITY_BUILD_INCLUSION (too many local statics/functions)
set(CMAKE_UNITY_BUILD ON)
# Batch size 14: sweet spot for MSVC — keeps each unity TU under ~30s to compile.
# 32 was too large: one TU change forced a 32-file recompile; 14 limits blast radius.
set(CMAKE_UNITY_BUILD_BATCH_SIZE 14)

# FetchContent: default to skipping the update step after first download to cut
# configure time by 5-15s on incremental configures. Not FORCEd so developers
# can toggle it off (-DFETCHCONTENT_UPDATES_DISCONNECTED=OFF) when a GIT_TAG is
# bumped to a commit not yet present in the local clone — otherwise the build
# wedges with "Requested git ref ... is not present locally". CMake docs:
# https://cmake.org/cmake/help/latest/module/FetchContent.html#variable:FETCHCONTENT_UPDATES_DISCONNECTED
set(FETCHCONTENT_UPDATES_DISCONNECTED ON CACHE BOOL "Skip FetchContent update step after initial download")

# MSVC compiler flags for maximum build throughput:
#   /MP        — multi-processor compilation (uses all logical cores within one project)
#   /Z7        — embed debug info in .obj instead of shared PDB → no PDB write contention
#                that serialises /MP builds (major speedup on 16+ core machines)
#   /Oi        — enable intrinsics (faster codegen, no extra passes)
#   /bigobj    — allow >65535 sections (needed by large unity TUs and MOC output)
#   /d2FuncCache7 — MSVC internal: increase function-level cache for template-heavy code
if(MSVC)
    # /permissive- : enforces ISO C++ conformance — catches MS-specific non-portable
    #                code that would break on GCC/Clang (two-phase lookup, etc.)
    # /W4          : level-4 warnings (highest practical without /Wall noise)
    # /WX only in CI so local iteration isn't blocked by new warnings
    add_compile_options(/MP /Z7 /Oi /bigobj /permissive- /W4)
    if(DEFINED ENV{CI})
        add_compile_options(/WX)
    endif()
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

# ── GCC / Clang warning flags ────────────────────────────────────────────────
# Mirror MSVC /W4 + /permissive- behavior on Unix compilers.
# -Werror only in CI so local iteration isn't blocked.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
    if(DEFINED ENV{CI})
        add_compile_options(-Werror)
    endif()
endif()

# Suppress Windows min/max macros that break std::min/std::max in unity batches
add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN)

# ── Test suite (opt-in) ──────────────────────────────────────────────────────
# OFF by default so a normal build is unchanged. Turn on with
# -DFINCEPT_BUILD_TESTS=ON to compile the Qt Test executables under
# fincept-qt/tests/. Run via `ctest` in the build directory.
option(FINCEPT_BUILD_TESTS "Build the Fincept Terminal test suite" OFF)

# ── Qt packages ──────────────────────────────────────────────────────────────
# Qt is pinned to 6.8.3 across all platforms. Releases must build
# on exactly this version to guarantee binary/ABI stability of the shipped
# installer. Developers may override for local experimentation with
# -DFINCEPT_ALLOW_QT_DRIFT=ON (not for CI or release builds).
set(FINCEPT_QT_VERSION 6.8.3)

# Qt 6.8 FindWrapOpenGL.cmake hard-codes "-framework AGL" as a fallback when
# find_library(AGL) fails. Apple removed AGL in macOS 14+ (Sonoma), causing
# linker errors on modern SDKs. Pre-create WrapOpenGL::WrapOpenGL without AGL
# so Qt's FindWrapOpenGL takes its early-return path (target already exists).
if(APPLE AND NOT TARGET WrapOpenGL::WrapOpenGL)
    find_package(OpenGL QUIET)
    if(OpenGL_FOUND)
        add_library(WrapOpenGL::WrapOpenGL INTERFACE IMPORTED)
        get_target_property(_fincept_ogl_loc OpenGL::GL IMPORTED_LOCATION)
        if(_fincept_ogl_loc AND NOT _fincept_ogl_loc MATCHES "/([^/]+)\\.framework$")
            get_filename_component(_fincept_ogl_path "${_fincept_ogl_loc}" DIRECTORY)
        endif()
        if(NOT _fincept_ogl_path)
            set(_fincept_ogl_path "-framework OpenGL")
        endif()
        target_link_libraries(WrapOpenGL::WrapOpenGL INTERFACE "${_fincept_ogl_path}")
        set(WrapOpenGL_FOUND ON)
    endif()
endif()

if(FINCEPT_ALLOW_QT_DRIFT)
    find_package(Qt6 REQUIRED COMPONENTS
        Widgets Charts PrintSupport Network Sql Concurrent Multimedia)
else()
    find_package(Qt6 ${FINCEPT_QT_VERSION} EXACT REQUIRED COMPONENTS
        Widgets Charts PrintSupport Network Sql Concurrent Multimedia)
endif()

# Optional: WebSockets (not always installed)
find_package(Qt6 ${FINCEPT_QT_VERSION} EXACT QUIET COMPONENTS WebSockets)
if(Qt6WebSockets_FOUND)
    set(HAS_WEBSOCKETS TRUE)
endif()

# Optional: MultimediaWidgets (for inline video/stream playback)
# Qt6::Multimedia is already required above (needed for QMediaPlayer in AiChatBubble / VideoPlayerWidget).
find_package(Qt6 ${FINCEPT_QT_VERSION} EXACT QUIET COMPONENTS MultimediaWidgets)
if(Qt6MultimediaWidgets_FOUND)
    set(HAS_MULTIMEDIA TRUE)
endif()

# Optional: TextToSpeech (cross-platform TTS — SAPI/AVSpeech/speech-dispatcher)
find_package(Qt6 ${FINCEPT_QT_VERSION} EXACT QUIET COMPONENTS TextToSpeech)
if(Qt6TextToSpeech_FOUND)
    set(HAS_TTS TRUE)
endif()

# macOS 14+ / Xcode 15+ removes the AGL framework. Qt6's WrapOpenGL target (pulled in
# by Qt6::Gui) hardcodes a fallback to `-framework AGL` if it isn't found on the system.
# We strip it from the INTERFACE_LINK_LIBRARIES to prevent `ld: framework 'AGL' not found`.
if(APPLE AND TARGET WrapOpenGL::WrapOpenGL)
    get_target_property(_gl_libs WrapOpenGL::WrapOpenGL INTERFACE_LINK_LIBRARIES)
    if(_gl_libs)
        set(_new_gl_libs "")
        foreach(_lib IN LISTS _gl_libs)
            if(NOT _lib MATCHES "AGL")
                list(APPEND _new_gl_libs "${_lib}")
            endif()
        endforeach()
        set_target_properties(WrapOpenGL::WrapOpenGL PROPERTIES INTERFACE_LINK_LIBRARIES "${_new_gl_libs}")
    endif()
endif()

# ── FetchContent dependencies ─────────────────────────────────────────────────
# Guard: prevent all FetchContent subdirectories from registering install()
# rules that would pollute CPack and cause "file not found" errors on packaging.
# Use CACHE FORCE so the flag is visible inside FetchContent sub-scopes
# (plain set() only propagates into add_subdirectory, not FetchContent processes).
include(FetchContent)
set(CMAKE_SKIP_INSTALL_RULES TRUE CACHE BOOL "" FORCE)

# ── Qt6::GuiPrivate availability check ───────────────────────────────────────
# QXlsx requires Qt6::GuiPrivate (uses private/qzipreader_p.h internally).
# The Qt6::GuiPrivate target is created as a side-effect of loading Qt6::Gui
# (which is pulled in by Qt6::Widgets above). There is NO separate cmake
# package called "Qt6GuiPrivate" — so find_package(Qt6 COMPONENTS GuiPrivate)
# always fails. The correct check is: if(TARGET Qt6::GuiPrivate).
#
# The official Qt installer (jurplel/install-qt-action, aqtinstall) always
# includes private headers. System Qt packages require an extra package:
#   Ubuntu/Debian : sudo apt-get install qt6-base-private-dev
#   Fedora        : sudo dnf install qt6-qtbase-private-devel
#   Arch Linux    : qt6-base (private headers included by default)
#   macOS Homebrew: included by default
# When private headers are absent, QXlsx is disabled and Excel export shows
# a "not available" message rather than crashing or failing to build.
if(TARGET Qt6::GuiPrivate)
    set(FINCEPT_HAS_PRIVATE_HEADERS TRUE)
else()
    set(FINCEPT_HAS_PRIVATE_HEADERS FALSE)
    message(WARNING
        "\n"
        "  Qt6::GuiPrivate not found — Excel export (QXlsx) will be disabled.\n"
        "  To enable it, install Qt6 private headers:\n"
        "    Ubuntu/Debian : sudo apt-get install qt6-base-private-dev\n"
        "    Fedora        : sudo dnf install qt6-qtbase-private-devel\n"
        "    Arch Linux    : qt6-base (already includes private headers)\n"
        "    macOS Homebrew: already included\n"
        "    CI/official   : jurplel/install-qt-action always includes them\n"
    )
endif()

# ── QXlsx (Excel file I/O) ────────────────────────────────────────────────────
# Only fetched and built when Qt6::GuiPrivate is available.
# A stub INTERFACE target is created when unavailable so all
# target_link_libraries(... QXlsx::QXlsx) calls still compile cleanly.
if(FINCEPT_HAS_PRIVATE_HEADERS)
    FetchContent_Declare(
        QXlsx
        GIT_REPOSITORY https://github.com/QtExcel/QXlsx.git
        GIT_TAG        3192b808a283f8eb18c97b9a087bf085054e9bb0  # v1.4.9
        SOURCE_SUBDIR  QXlsx
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(QXlsx)

    # QXlsx uses implicit const char* -> QString conversions which are banned
    # by Qt6's strict cast macros. Undefine them for this target only.
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
else()
    # Stub target — satisfies target_link_libraries without pulling in QXlsx
    add_library(QXlsx_stub INTERFACE)
    add_library(QXlsx::QXlsx ALIAS QXlsx_stub)
endif()
# NOTE: FINCEPT_HAS_QXLSX compile definition is applied to FinceptTerminal
# after add_executable() — see the target_compile_definitions block below.

# ── Auto-update service ─────────────────────────────────────────────────────
# The updater is implemented in-tree (src/services/updater/UpdateService.cpp)
# using Qt6::Network directly — no third-party dependency. See the class
# docstring for the manifest schema.

# ── md4c (fast CommonMark markdown parser → HTML) ────────────────────────────
FetchContent_Declare(
    md4c
    GIT_REPOSITORY https://github.com/mity/md4c.git
    GIT_TAG        729e6b8b320caa96328968ab27d7db2235e4fb47  # release-0.5.2
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(md4c)
# md4c builds md4c (parser) and md4c-html (HTML renderer) as static libs.
# Disable any example/test targets it may expose.
set_target_properties(md4c PROPERTIES UNITY_BUILD OFF)
if(TARGET md4c-html)
    set_target_properties(md4c-html PROPERTIES UNITY_BUILD OFF)
endif()
# md4c's src/CMakeLists.txt installs from ${CMAKE_BINARY_DIR}/src/md4c.pc but
# configure_file() writes to ${CMAKE_CURRENT_BINARY_DIR}/md4c.pc (the subbuild
# dir). On CI these are different paths, causing CPack's "file INSTALL cannot
# find md4c.pc: File exists" error. Copy the generated .pc files at configure
# time to where md4c's install rules expect them.
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/src")
foreach(_pc md4c.pc md4c-html.pc)
    if(EXISTS "${md4c_BINARY_DIR}/src/${_pc}")
        configure_file(
            "${md4c_BINARY_DIR}/src/${_pc}"
            "${CMAKE_BINARY_DIR}/src/${_pc}"
            COPYONLY
        )
    endif()
endforeach()

# ── QGeoView (interactive map widget with OSM tiles) ─────────────────────────
# PATCH_COMMAND removes -framework AGL which was deleted in macOS 14 (Sonoma).
# Uses a portable CMake script instead of sed (sed is unavailable in MSVC
# Developer Command Prompt on Windows and is fragile across platforms).
FetchContent_Declare(
    QGeoView
    GIT_REPOSITORY https://github.com/AmonRaNet/QGeoView.git
    GIT_TAG        4b2c52c80940e1073c259ded1a57df83036ba1c9  # master @ 2026-04-11
    GIT_SHALLOW    TRUE
    PATCH_COMMAND  ${CMAKE_COMMAND}
        -DFILE=<SOURCE_DIR>/lib/CMakeLists.txt
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/patches/qgeoview_remove_agl.cmake"
)
set(QGV_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
set(QGV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(QGeoView)
# QGeoView has anonymous-namespace symbol clashes under unity build
set_target_properties(qgeoview PROPERTIES UNITY_BUILD OFF)

# Qt 6.8.3's QtGui on macOS carries a transitive "-framework AGL" in its link
# interface. Apple removed AGL in macOS 14 (Sonoma), so the link fails on 14+
# with: ld: framework 'AGL' not found. Scrub AGL from qgeoview's resolved link
# libraries on Apple — OpenGL framework (also in Qt's interface) is sufficient.
# See GitHub issue #140.
if(APPLE)
    foreach(_prop LINK_LIBRARIES INTERFACE_LINK_LIBRARIES)
        get_target_property(_libs qgeoview ${_prop})
        if(_libs)
            list(REMOVE_ITEM _libs "-framework AGL" "AGL")
            set_target_properties(qgeoview PROPERTIES ${_prop} "${_libs}")
        endif()
    endforeach()
endif()

# ── SingleApplication (single-process enforcement + new-window IPC) ──────────
# When the user re-launches the exe, the secondary instance sends "--new-window"
# to the primary, which opens a new MainWindow instead of a second process.
# QAPPLICATION_CLASS must be set before FetchContent_MakeAvailable so the lib
# compiles inheriting QApplication (not QCoreApplication).
set(QT_DEFAULT_MAJOR_VERSION 6 CACHE STRING "" FORCE)
set(QAPPLICATION_CLASS QApplication CACHE STRING "" FORCE)
FetchContent_Declare(
    SingleApplication
    GIT_REPOSITORY https://github.com/itay-grudev/SingleApplication.git
    GIT_TAG        a8da87d78238ac9ac92d2cda41793d4879c188b3  # v3.5.4
    GIT_SHALLOW    TRUE
    UPDATE_DISCONNECTED TRUE
)
FetchContent_MakeAvailable(SingleApplication)
if(MSVC)
    target_compile_options(SingleApplication PRIVATE /W0)
endif()

# ── Qt Advanced Docking System (detachable panels) ───────────────────────────
# Panels become CDockWidgets — drag to float, dock, move to any monitor.
# LGPL — compatible with AGPL. Used in Qt Creator itself.
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_STATIC   ON  CACHE BOOL "" FORCE)  # static lib: simpler deployment, no extra DLL
set(QT_VERSION_MAJOR 6 CACHE STRING "" FORCE)
FetchContent_Declare(
    QtADS
    GIT_REPOSITORY https://github.com/githubuser0xFFFF/Qt-Advanced-Docking-System.git
    GIT_TAG        87cffe5d40307c61141c88b88c10c5e7d181d880  # 4.5.0
    GIT_SHALLOW    TRUE
    UPDATE_DISCONNECTED TRUE
)
FetchContent_MakeAvailable(QtADS)
# ADS target name is version-stamped: qtadvanceddocking-qt6
# Disable unity build — ADS uses anonymous namespaces internally
set_target_properties(qtadvanceddocking-qt6 PROPERTIES UNITY_BUILD OFF)

# Restore install rules for the main application targets
set(CMAKE_SKIP_INSTALL_RULES FALSE CACHE BOOL "" FORCE)
if(MSVC)
    target_compile_options(qtadvanceddocking-qt6 PRIVATE /W0)
endif()

# ── Source files by module ────────────────────────────────────────────────────

# Core
set(CORE_SOURCES
    src/core/config/AppConfig.cpp
    src/core/config/AppPaths.cpp
    src/core/config/ProfileManager.cpp
    src/core/logging/Logger.cpp
    src/core/events/EventBus.cpp
    src/core/session/SessionManager.cpp
    src/core/session/ScreenStateManager.cpp
    src/core/keys/KeyConfigManager.cpp
)

# Network
set(NETWORK_SOURCES
    src/network/http/HttpClient.cpp
)
# WebSocketClient is always compiled; it stubs out when HAS_QT_WEBSOCKETS is not defined
list(APPEND NETWORK_SOURCES src/network/websocket/WebSocketClient.cpp)

# DataHub (Phase 0 scaffolding — see fincept-qt/DATAHUB_ARCHITECTURE.md)
# Headers are always built so Q_OBJECT MOC runs; the .cpp bodies are only
# active when the feature flag is ON. Phase 1 will flesh out the stub.
set(DATAHUB_SOURCES
    src/datahub/DataHub.cpp
    src/datahub/DataHubMetaTypes.cpp
)

# Auth
set(AUTH_SOURCES
    src/auth/AuthManager.cpp
    src/auth/AuthApi.cpp
    src/auth/UserApi.cpp
    src/auth/SessionGuard.cpp
    src/auth/PinManager.cpp
    src/auth/InactivityGuard.cpp
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

    # Storage manager
    src/storage/StorageManager.cpp
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
    src/storage/repositories/AccountRepository.cpp

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

    # LLM tools toggle
    src/storage/sqlite/migrations/v014_llm_tools_toggle.cpp

    # Screen UI state — crash-safe per-screen state in cache.db
    src/storage/sqlite/migrations/v015_screen_state.cpp

    # Multi-account broker support
    src/storage/sqlite/migrations/v016_broker_accounts.cpp
    src/storage/sqlite/migrations/v017_perf_indexes.cpp
    src/storage/sqlite/migrations/v018_widget_config.cpp
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
    src/mcp/tools/DataHubTools.cpp
)

# Trading
set(TRADING_SOURCES
    src/trading/PaperTrading.cpp
    src/trading/OrderMatcher.cpp
    src/trading/BrokerRegistry.cpp
    src/trading/UnifiedTrading.cpp
    src/trading/AccountManager.cpp
    src/trading/AccountDataStream.cpp
    src/trading/DataStreamManager.cpp
    src/trading/ExchangeService.cpp
    src/trading/brokers/BrokerHttp.cpp
    # Trading WebSocket
    src/trading/websocket/ZerodhaWebSocket.cpp
    src/trading/websocket/AngelOneWebSocket.cpp
    # Indian brokers
    src/trading/brokers/zerodha/ZerodhaBroker.cpp
    src/trading/brokers/zerodha/Totp.cpp
    src/trading/brokers/zerodha/ZerodhaAutoLogin.cpp
    src/trading/auth/RedirectServer.cpp
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
    src/trading/instruments/GrowwInstrumentParser.cpp
    src/trading/instruments/InstrumentService.cpp
)

# Services
set(SERVICE_SOURCES
    src/services/akshare/AkShareService.cpp
    src/services/alpha_arena/AlphaArenaService.cpp
    src/services/asia_markets/AsiaMarketsService.cpp
    src/services/crypto/TotpService.cpp
    src/services/python_cli/PythonCliService.cpp
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
    src/services/equity/MarketSentimentService.cpp
    src/services/equity/MarketSentimentSupport.cpp
    src/services/ma_analytics/MAAnalyticsService.cpp
    src/services/geopolitics/GeopoliticsService.cpp
    src/services/maritime/MaritimeService.cpp
    src/services/ai_quant_lab/AIQuantLabService.cpp
    src/services/backtesting/BacktestingService.cpp
    src/services/algo_trading/AlgoTradingService.cpp
    src/services/portfolio/PortfolioService.cpp
    src/services/portfolio/PortfolioAnalyticsService.cpp
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

    # Prediction Markets — exchange-agnostic abstraction (Polymarket, Kalshi, …)
    src/services/prediction/PredictionExchangeAdapter.cpp
    src/services/prediction/PredictionExchangeRegistry.cpp
    src/services/prediction/PredictionCredentialStore.cpp
    src/services/prediction/polymarket/PolymarketTypeMap.cpp
    src/services/prediction/polymarket/PolymarketAdapter.cpp
    src/services/prediction/kalshi/KalshiRestClient.cpp
    src/services/prediction/kalshi/KalshiWsClient.cpp
    src/services/prediction/kalshi/KalshiAdapter.cpp

    # Speech-to-text service (Python speech_recognition + Google API)
    src/services/stt/SpeechService.cpp

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
    src/ui/navigation/DockToolBar.cpp
    src/ui/navigation/DockStatusBar.cpp

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
    src/screens/auth/LockScreen.cpp
    src/screens/devtools/DataHubInspector.cpp
    src/screens/profile/ProfileScreen.cpp
    src/screens/settings/SettingsScreen.cpp
    src/screens/settings/LlmConfigSection.cpp
    src/screens/settings/McpServersSection.cpp
    src/screens/settings/KeybindingsSection.cpp
    src/screens/settings/PythonEnvSection.cpp
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
    src/screens/chat_mode/TerminalToolBridge.cpp
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
    src/screens/markets/MarketPanelStore.cpp
    src/screens/markets/MarketPanelEditor.cpp
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
    src/screens/dashboard/widgets/OpenPositionsWidget.cpp
    src/screens/dashboard/widgets/OrderBookMiniWidget.cpp
    src/screens/dashboard/widgets/MarginUsageWidget.cpp
    src/screens/dashboard/widgets/TodayPnLWidget.cpp
    src/screens/dashboard/widgets/BrokerHoldingsWidget.cpp
    src/screens/dashboard/widgets/MarketQuoteStripWidget.cpp
    src/screens/dashboard/widgets/CryptoTickerWidget.cpp
    src/screens/dashboard/widgets/PolymarketPriceWidget.cpp
    src/screens/dashboard/widgets/AgentErrorsWidget.cpp
    src/screens/dashboard/widgets/SparklineStripWidget.cpp
    src/screens/dashboard/widgets/TradeTapeWidget.cpp
    src/screens/dashboard/widgets/NewsCategoryWidget.cpp
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
    src/screens/equity_trading/AccountManagementDialog.cpp
    src/screens/equity_trading/BroadcastOrderDialog.cpp
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
    src/screens/gov_data/GovDataFrancePanel.cpp
    src/screens/gov_data/GovDataHKPanel.cpp
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

    # Polymarket / Prediction Markets
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
    src/screens/polymarket/PredictionAccountDialog.cpp
    src/screens/polymarket/ExchangePresentation.cpp

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
    src/screens/equity_research/EquitySentimentTab.cpp

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
    src/app/DockScreenRouter.cpp
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
    src/screens/auth/LockScreen.cpp
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
    src/screens/equity_trading/AccountManagementDialog.cpp
    src/screens/equity_trading/BroadcastOrderDialog.cpp
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
    src/screens/dashboard/widgets/OpenPositionsWidget.cpp
    src/screens/dashboard/widgets/OrderBookMiniWidget.cpp
    src/screens/dashboard/widgets/MarginUsageWidget.cpp
    src/screens/dashboard/widgets/TodayPnLWidget.cpp
    src/screens/dashboard/widgets/BrokerHoldingsWidget.cpp
    src/screens/dashboard/widgets/MarketQuoteStripWidget.cpp
    src/screens/dashboard/widgets/CryptoTickerWidget.cpp
    src/screens/dashboard/widgets/PolymarketPriceWidget.cpp
    src/screens/dashboard/widgets/AgentErrorsWidget.cpp
    src/screens/dashboard/widgets/SparklineStripWidget.cpp
    src/screens/dashboard/widgets/TradeTapeWidget.cpp
    src/screens/dashboard/widgets/NewsCategoryWidget.cpp
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
    src/screens/equity_research/EquitySentimentTab.cpp
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
    src/screens/gov_data/GovDataFrancePanel.cpp
    src/screens/gov_data/GovDataHKPanel.cpp
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
    src/screens/markets/MarketPanelStore.cpp
    src/screens/markets/MarketPanelEditor.cpp
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
    src/screens/settings/KeybindingsSection.cpp
    src/screens/settings/PythonEnvSection.cpp
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
    src/storage/repositories/AccountRepository.cpp
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
    src/storage/sqlite/migrations/v014_llm_tools_toggle.cpp
    src/storage/sqlite/migrations/v015_screen_state.cpp
    src/storage/sqlite/migrations/v016_broker_accounts.cpp
    src/storage/sqlite/migrations/v017_perf_indexes.cpp
    src/storage/sqlite/migrations/v018_widget_config.cpp
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
    src/screens/polymarket/PredictionAccountDialog.cpp
    src/trading/websocket/ZerodhaWebSocket.cpp
    src/trading/websocket/AngelOneWebSocket.cpp
    # Trading broker files — each defines static now_ts() in same namespace
    src/trading/brokers/zerodha/ZerodhaBroker.cpp
    src/trading/brokers/zerodha/Totp.cpp
    src/trading/brokers/zerodha/ZerodhaAutoLogin.cpp
    src/trading/auth/RedirectServer.cpp
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
    PROPERTIES SKIP_UNITY_BUILD_INCLUSION TRUE
)

# ADS files use anonymous namespaces and heavy Qt meta-object includes —
# exclude from unity to avoid ODR / symbol clashes in large batches
set_source_files_properties(
    src/app/MainWindow.cpp
    src/app/DockScreenRouter.cpp
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
    src/trading/brokers/zerodha/Totp.cpp
    src/trading/brokers/zerodha/ZerodhaAutoLogin.cpp
    src/trading/auth/RedirectServer.cpp
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
    ${DATAHUB_SOURCES}
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
    ${SingleApplication_SOURCE_DIR}
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
                -no-strip -always-overwrite
            COMMENT "Running macdeployqt to bundle Qt frameworks..."
        )
        # Remove unused SQL driver plugins that reference libraries not present
        # on typical macOS installs (ODBC, PostgreSQL, Mimer). These cause
        # harmless but noisy "ERROR: no file at ..." warnings from macdeployqt.
        add_custom_command(TARGET FinceptTerminal POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E rm -f
                "$<TARGET_BUNDLE_DIR:FinceptTerminal>/Contents/PlugIns/sqldrivers/libqsqlodbc.dylib"
                "$<TARGET_BUNDLE_DIR:FinceptTerminal>/Contents/PlugIns/sqldrivers/libqsqlpsql.dylib"
                "$<TARGET_BUNDLE_DIR:FinceptTerminal>/Contents/PlugIns/sqldrivers/libqsqlmimer.dylib"
            COMMENT "Removing unused SQL driver plugins (ODBC/PostgreSQL/Mimer)..."
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

# ── OpenSSL — needed for Google Sheets JWT signing ────────────────────────────
# OPENSSL_ROOT_DIR can be set via:
#   - cmake -DOPENSSL_ROOT_DIR=<path>          (CI: passed from workflow)
#   - environment variable OPENSSL_ROOT_DIR    (CI: set by setup-openssl action)
#   - vcpkg on dev machine (auto-detected below if not already set)
if(WIN32 AND NOT OPENSSL_ROOT_DIR AND NOT DEFINED ENV{OPENSSL_ROOT_DIR})
    # Dev machine vcpkg fallback
    if(DEFINED ENV{VCPKG_INSTALLED_DIR} AND EXISTS "$ENV{VCPKG_INSTALLED_DIR}/x64-windows/lib/libcrypto.lib")
        set(OPENSSL_ROOT_DIR "$ENV{VCPKG_INSTALLED_DIR}/x64-windows" CACHE PATH "" FORCE)
    elseif(DEFINED ENV{VCPKG_ROOT} AND EXISTS "$ENV{VCPKG_ROOT}/installed/x64-windows/lib/libcrypto.lib")
        set(OPENSSL_ROOT_DIR "$ENV{VCPKG_ROOT}/installed/x64-windows" CACHE PATH "" FORCE)
    elseif(EXISTS "C:/windowsdisk/fincept-cpp/vcpkg_installed/x64-windows/lib/libcrypto.lib")
        set(OPENSSL_ROOT_DIR "C:/windowsdisk/fincept-cpp/vcpkg_installed/x64-windows" CACHE PATH "" FORCE)
    endif()
endif()

if(OPENSSL_ROOT_DIR)
    message(STATUS "OPENSSL_ROOT_DIR: ${OPENSSL_ROOT_DIR}")
endif()

find_package(OpenSSL REQUIRED)
message(STATUS "OpenSSL found: ${OPENSSL_VERSION} — include: ${OPENSSL_INCLUDE_DIR}")

target_include_directories(FinceptTerminal PRIVATE
    "${OPENSSL_INCLUDE_DIR}"
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
    md4c
    md4c-html
    SingleApplication::SingleApplication
    qtadvanceddocking-qt6
    OpenSSL::SSL
    OpenSSL::Crypto
)

# macOS: also link Security.framework for Keychain in SecureStorage
if(APPLE)
    target_link_libraries(FinceptTerminal PRIVATE "-framework Security")
endif()

# Copy OpenSSL DLLs to build output — Windows only
if(WIN32)
    get_target_property(OPENSSL_CRYPTO_DLL OpenSSL::Crypto IMPORTED_LOCATION)
    get_target_property(OPENSSL_SSL_DLL    OpenSSL::SSL    IMPORTED_LOCATION)

    # IMPORTED_LOCATION may point to the .lib on Windows — find the matching .dll
    if(OPENSSL_CRYPTO_DLL MATCHES "\\.lib$" OR NOT OPENSSL_CRYPTO_DLL)
        # Search common locations for the DLL
        find_file(OPENSSL_CRYPTO_DLL_FILE
            NAMES libcrypto-3-x64.dll libcrypto-3.dll libcrypto.dll
            HINTS
                "${OPENSSL_ROOT_DIR}/bin"
                "$ENV{QT_ROOT_DIR}/bin"
                "C:/Program Files/OpenSSL/bin"
                "C:/Program Files/OpenSSL-Win64/bin"
        )
        find_file(OPENSSL_SSL_DLL_FILE
            NAMES libssl-3-x64.dll libssl-3.dll libssl.dll
            HINTS
                "${OPENSSL_ROOT_DIR}/bin"
                "$ENV{QT_ROOT_DIR}/bin"
                "C:/Program Files/OpenSSL/bin"
                "C:/Program Files/OpenSSL-Win64/bin"
        )
    else()
        set(OPENSSL_CRYPTO_DLL_FILE "${OPENSSL_CRYPTO_DLL}")
        set(OPENSSL_SSL_DLL_FILE    "${OPENSSL_SSL_DLL}")
    endif()

    if(OPENSSL_CRYPTO_DLL_FILE AND OPENSSL_SSL_DLL_FILE)
        add_custom_command(TARGET FinceptTerminal POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${OPENSSL_CRYPTO_DLL_FILE}"
                "$<TARGET_FILE_DIR:FinceptTerminal>"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${OPENSSL_SSL_DLL_FILE}"
                "$<TARGET_FILE_DIR:FinceptTerminal>"
            COMMENT "Copying OpenSSL DLLs..."
        )
        message(STATUS "OpenSSL DLLs to deploy: ${OPENSSL_CRYPTO_DLL_FILE}, ${OPENSSL_SSL_DLL_FILE}")
    else()
        message(WARNING "OpenSSL DLLs not found for POST_BUILD copy — deploy manually")
    endif()
endif()

# ── Application version (single source of truth) ─────────────────────────────
# FINCEPT_VERSION_STRING is consumed by main.cpp so QApplication::applicationVersion()
# always matches the project() VERSION declared at the top of this file.
target_compile_definitions(FinceptTerminal PRIVATE
    FINCEPT_VERSION_STRING="${CMAKE_PROJECT_VERSION}")

# ── Optional feature compile definitions (applied after add_executable) ──────
if(FINCEPT_HAS_PRIVATE_HEADERS)
    target_compile_definitions(FinceptTerminal PRIVATE FINCEPT_HAS_QXLSX)
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

# ── Copy requirements files to build output ──────────────────────────────────
# PythonSetupManager::find_requirements_file() searches exe_dir/resources/ first.
# Without this POST_BUILD step the files are only available via cmake --install,
# causing setup to silently fail in dev/debug builds.
add_custom_command(TARGET FinceptTerminal POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:FinceptTerminal>/resources"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/requirements-numpy1.txt"
        "$<TARGET_FILE_DIR:FinceptTerminal>/resources/requirements-numpy1.txt"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/requirements-numpy2.txt"
        "$<TARGET_FILE_DIR:FinceptTerminal>/resources/requirements-numpy2.txt"
    COMMENT "Copying requirements files to build output..."
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
    install(TARGETS FinceptTerminal RUNTIME DESTINATION "." COMPONENT core)
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/scripts/"
            DESTINATION "scripts" COMPONENT scripts)
    # Requirements files — needed by PythonSetupManager at first run to install
    # Python packages. Must be deployed beside the exe so find_requirements_file()
    # can locate them at C:/Program Files/FinceptTerminal/resources/*.txt.
    install(FILES
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/requirements-numpy1.txt"
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/requirements-numpy2.txt"
            DESTINATION "resources" COMPONENT core)

elseif(APPLE)
    # macOS: install the .app bundle
    install(TARGETS FinceptTerminal BUNDLE DESTINATION "." COMPONENT core)
    # Requirements files — deployed beside the .app so PythonSetupManager finds them
    install(FILES
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/requirements-numpy1.txt"
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/requirements-numpy2.txt"
            DESTINATION "FinceptTerminal.app/Contents/Resources/resources" COMPONENT core)

else()
    # Linux: FHS-compliant layout
    install(TARGETS FinceptTerminal
            RUNTIME DESTINATION "bin" COMPONENT core)

    # Python analytics scripts
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/scripts/"
            DESTINATION "share/fincept-terminal/scripts"
            USE_SOURCE_PERMISSIONS COMPONENT scripts)

    # App icon (PNG required for hicolor theme; convert .ico → .png in CI)
    set(_ICON_PNG "${CMAKE_CURRENT_SOURCE_DIR}/resources/fincept.png")
    if(EXISTS "${_ICON_PNG}")
        install(FILES "${_ICON_PNG}"
                DESTINATION "share/icons/hicolor/256x256/apps"
                RENAME "fincept-terminal.png" COMPONENT core)
    else()
        message(STATUS "Linux icon not found at ${_ICON_PNG} — "
                       "convert resources/fincept.ico to fincept.png for proper desktop integration")
    endif()

    # .desktop file
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/packaging/linux/fincept-terminal.desktop"
            DESTINATION "share/applications" COMPONENT core)

    # AppStream metadata
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/packaging/linux/fincept-terminal.appdata.xml"
            DESTINATION "share/metainfo" COMPONENT core)

    # yt-dlp (if bundled)
    if(FINCEPT_YTDLP_TO_BUNDLE AND EXISTS "${FINCEPT_YTDLP_TO_BUNDLE}")
        install(PROGRAMS "${FINCEPT_YTDLP_TO_BUNDLE}"
                DESTINATION "bin"
                RENAME "yt-dlp" COMPONENT core)
    endif()
endif()

# ── Installer (QtIFW via CPackIFW) ───────────────────────────────────────────
# Generate cross-platform installer with uninstaller:
#   cmake -B build -DFINCEPT_BUILD_INSTALLER=ON -DDEPLOY_QT=ON ...
#   cmake --build build --config Release
#   cd build && cpack -G IFW -C Release
#
# Requires: Qt Installer Framework (binarycreator on PATH or in Qt install)
# Output:   FinceptTerminal-<ver>-<platform>-setup.exe/.run/.app

option(FINCEPT_BUILD_INSTALLER "Generate QtIFW installer via CPack" OFF)

if(FINCEPT_BUILD_INSTALLER)

    # ── Windows: capture windeployqt output for the installer ────────────────
    if(WIN32 AND DEPLOY_QT)
        install(DIRECTORY "$<TARGET_FILE_DIR:FinceptTerminal>/"
                DESTINATION "."
                COMPONENT core
                FILES_MATCHING
                PATTERN "*.dll")
        # Python analytics scripts (4000+ files) — required at runtime by
        # PythonRunner / ExchangeService. Without this the Crypto tab and all
        # Python-backed screens crash on an installed build.
        install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/scripts/"
                DESTINATION "scripts" COMPONENT scripts)
        # Requirements files — read by PythonSetupManager::install_packages()
        # during first-run setup. Missing files → setup fails on installed build.
        install(FILES
                "${CMAKE_CURRENT_SOURCE_DIR}/resources/requirements-numpy1.txt"
                "${CMAKE_CURRENT_SOURCE_DIR}/resources/requirements-numpy2.txt"
                DESTINATION "resources" COMPONENT core)
        # Qt plugin directories
        install(DIRECTORY "$<TARGET_FILE_DIR:FinceptTerminal>/platforms"
                DESTINATION "." COMPONENT core OPTIONAL)
        install(DIRECTORY "$<TARGET_FILE_DIR:FinceptTerminal>/sqldrivers"
                DESTINATION "." COMPONENT core OPTIONAL)
        install(DIRECTORY "$<TARGET_FILE_DIR:FinceptTerminal>/imageformats"
                DESTINATION "." COMPONENT core OPTIONAL)
        install(DIRECTORY "$<TARGET_FILE_DIR:FinceptTerminal>/styles"
                DESTINATION "." COMPONENT core OPTIONAL)
        install(DIRECTORY "$<TARGET_FILE_DIR:FinceptTerminal>/tls"
                DESTINATION "." COMPONENT core OPTIONAL)
        install(DIRECTORY "$<TARGET_FILE_DIR:FinceptTerminal>/multimedia"
                DESTINATION "." COMPONENT core OPTIONAL)
        install(DIRECTORY "$<TARGET_FILE_DIR:FinceptTerminal>/networkinformation"
                DESTINATION "." COMPONENT core OPTIONAL)
        install(DIRECTORY "$<TARGET_FILE_DIR:FinceptTerminal>/generic"
                DESTINATION "." COMPONENT core OPTIONAL)
    endif()

    # ── CPack general metadata ───────────────────────────────────────────────
    set(CPACK_PACKAGE_NAME "FinceptTerminal")
    set(CPACK_PACKAGE_VENDOR "Fincept Corporation")
    set(CPACK_PACKAGE_VERSION "${CMAKE_PROJECT_VERSION}")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
        "Professional financial intelligence terminal with AI analytics")
    set(CPACK_PACKAGE_HOMEPAGE_URL
        "https://github.com/Fincept-Corporation/FinceptTerminal")
    set(CPACK_RESOURCE_FILE_LICENSE
        "${CMAKE_CURRENT_SOURCE_DIR}/packaging/installer/packages/com.fincept.terminal.core/meta/license.txt")
    set(CPACK_PACKAGE_ICON
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/fincept.ico")

    # ── Generator: IFW only ──────────────────────────────────────────────────
    set(CPACK_GENERATOR "IFW")

    # ── QtIFW tool discovery ─────────────────────────────────────────────────
    find_program(BINARYCREATOR_EXE binarycreator
        HINTS
            "$ENV{QT_IFW_DIR}/bin"
            "$ENV{QT_ROOT_DIR}/../../Tools/QtInstallerFramework/4.8/bin"
            "C:/Qt/Tools/QtInstallerFramework/4.8/bin"
            "C:/Qt/Tools/QtInstallerFramework/4.7/bin"
            "/opt/Qt/Tools/QtInstallerFramework/4.8/bin"
            "$ENV{HOME}/Qt/Tools/QtInstallerFramework/4.8/bin"
    )
    if(BINARYCREATOR_EXE)
        get_filename_component(_IFW_BIN_DIR "${BINARYCREATOR_EXE}" DIRECTORY)
        set(CPACK_IFW_ROOT "${_IFW_BIN_DIR}/..")
        message(STATUS "QtIFW found: ${BINARYCREATOR_EXE}")
    else()
        message(WARNING "binarycreator not found — set QT_IFW_DIR or install Qt Installer Framework")
    endif()

    # ── CPackIFW installer configuration ─────────────────────────────────────
    set(CPACK_IFW_PACKAGE_TITLE "Fincept Terminal")
    set(CPACK_IFW_PACKAGE_NAME "com.fincept.terminal")
    set(CPACK_IFW_PACKAGE_PUBLISHER "Fincept Corporation")
    set(CPACK_IFW_PACKAGE_START_MENU_DIRECTORY "Fincept")
    set(CPACK_IFW_PACKAGE_ICON
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/fincept.ico")
    set(CPACK_IFW_PACKAGE_MAINTENANCE_TOOL_NAME "FinceptMaintenanceTool")
    set(CPACK_IFW_PACKAGE_WIZARD_STYLE "Modern")
    set(CPACK_IFW_PACKAGE_ALLOW_SPACE_IN_PATH ON)
    set(CPACK_IFW_PACKAGE_ALLOW_NON_ASCII_CHARACTERS ON)

    # Target directory defaults per platform
    if(WIN32)
        set(CPACK_IFW_TARGET_DIRECTORY "@ApplicationsDirX64@/FinceptTerminal")
    elseif(APPLE)
        set(CPACK_IFW_TARGET_DIRECTORY "/Applications/FinceptTerminal")
    else()
        set(CPACK_IFW_TARGET_DIRECTORY "@HomeDir@/FinceptTerminal")
    endif()

    # Output file naming — arch-aware so the auto-updater manifest can point
    # at predictable, stable URLs. Scheme:
    #   FinceptTerminal-<version>-<platform>-<arch>-setup
    # Platform: windows | linux | macos
    # Arch:     x64 | arm64
    # The CPack IFW generator appends .exe on Windows, .app on macOS, .run on Linux.
    if(WIN32)
        # CMAKE_SYSTEM_PROCESSOR on Windows is "AMD64" for x64 and "ARM64" for arm64.
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64|aarch64")
            set(_FINCEPT_PKG_ARCH "arm64")
        else()
            set(_FINCEPT_PKG_ARCH "x64")
        endif()
        set(CPACK_PACKAGE_FILE_NAME
            "FinceptTerminal-${CMAKE_PROJECT_VERSION}-windows-${_FINCEPT_PKG_ARCH}-setup")
    elseif(APPLE)
        # On macOS CMAKE_OSX_ARCHITECTURES is the authoritative source
        # (it drives the actual binary slice). Fall back to host processor.
        if(CMAKE_OSX_ARCHITECTURES)
            list(LENGTH CMAKE_OSX_ARCHITECTURES _osx_arch_count)
            if(_osx_arch_count GREATER 1)
                set(_FINCEPT_PKG_ARCH "universal")
            elseif("${CMAKE_OSX_ARCHITECTURES}" STREQUAL "arm64")
                set(_FINCEPT_PKG_ARCH "arm64")
            else()
                set(_FINCEPT_PKG_ARCH "x64")
            endif()
        elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
            set(_FINCEPT_PKG_ARCH "arm64")
        else()
            set(_FINCEPT_PKG_ARCH "x64")
        endif()
        set(CPACK_PACKAGE_FILE_NAME
            "FinceptTerminal-${CMAKE_PROJECT_VERSION}-macos-${_FINCEPT_PKG_ARCH}-setup")
    else()
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
            set(_FINCEPT_PKG_ARCH "arm64")
        else()
            set(_FINCEPT_PKG_ARCH "x64")
        endif()
        set(CPACK_PACKAGE_FILE_NAME
            "FinceptTerminal-${CMAKE_PROJECT_VERSION}-linux-${_FINCEPT_PKG_ARCH}-setup")
    endif()
    message(STATUS "  Installer filename: ${CPACK_PACKAGE_FILE_NAME}")

    # ── Include CPack + IFW module ───────────────────────────────────────────
    include(CPack)
    include(CPackIFW)

    # ── Define install components ────────────────────────────────────────────
    cpack_add_component(core
        DISPLAY_NAME "Fincept Terminal"
        DESCRIPTION  "Core application executable and Qt runtime libraries"
        REQUIRED)

    cpack_add_component(scripts
        DISPLAY_NAME "Python Analytics Scripts"
        DESCRIPTION  "1300+ CFA-level Python analytics scripts (equity, derivatives, portfolio, quant)"
        DEPENDS core)

    # ── CPackIFW component configuration ─────────────────────────────────────
    cpack_ifw_configure_component(core
        NAME "com.fincept.terminal.core"
        VERSION "${CMAKE_PROJECT_VERSION}"
        SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/packaging/installer/packages/com.fincept.terminal.core/meta/installscript.qs"
        LICENSES "AGPL-3.0" "${CMAKE_CURRENT_SOURCE_DIR}/packaging/installer/packages/com.fincept.terminal.core/meta/license.txt"
        SORTING_PRIORITY 100
        DEFAULT TRUE
        FORCED_INSTALLATION)

    cpack_ifw_configure_component(scripts
        NAME "com.fincept.terminal.scripts"
        VERSION "${CMAKE_PROJECT_VERSION}"
        SORTING_PRIORITY 50
        DEFAULT TRUE
        DEPENDS "com.fincept.terminal.core")

endif() # FINCEPT_BUILD_INSTALLER

# ── Build configuration summary ──────────────────────────────────────────────
message(STATUS "")
message(STATUS "============================================================")
message(STATUS "  Fincept Terminal ${CMAKE_PROJECT_VERSION} — build summary")
message(STATUS "============================================================")
message(STATUS "  Qt version        : ${Qt6Core_VERSION}")
message(STATUS "  Build type        : ${CMAKE_BUILD_TYPE}")
message(STATUS "  Platform          : ${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "  Compiler          : ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "------------------------------------------------------------")
message(STATUS "  Excel export      : ${FINCEPT_HAS_PRIVATE_HEADERS}  (Qt6::GuiPrivate / QXlsx)")
if(DEFINED HAS_WEBSOCKETS)
message(STATUS "  WebSockets        : ${HAS_WEBSOCKETS}")
endif()
if(DEFINED HAS_MULTIMEDIA)
message(STATUS "  Multimedia        : ${HAS_MULTIMEDIA}")
endif()
if(DEFINED HAS_TTS)
message(STATUS "  TextToSpeech      : ${HAS_TTS}")
endif()
message(STATUS "  Tests             : ${FINCEPT_BUILD_TESTS}")
message(STATUS "============================================================")
message(STATUS "")

# ── Test suite (opt-in; see fincept-qt/tests/CMakeLists.txt) ─────────────────
if(FINCEPT_BUILD_TESTS)
    add_subdirectory(tests)
endif()
