cmake_minimum_required(VERSION 3.27)

# ── RC compiler override (clang-cl + Windows only) ───────────────────────────
# When building with clang-cl, CMake's Windows-Clang module auto-enables the
# two-step `cmake -E cmake_llvm_rc` wrapper: preprocess the .rc with clang-cl
# then pipe to rc.exe. That wrapper inherits incompatible C flags (-MD, -FS,
# /showIncludes) and fails silently with exit 1 on Windows SDK rc.exe. Our
# app.rc is a trivial ICON declaration with no macros or #includes, so we
# bypass the wrapper and use rc.exe directly. Tracked as CMake issue #20198.
# Must run before project() so CMAKE_RC_COMPILER is set before RC language init.
if(WIN32 AND CMAKE_HOST_WIN32)
    # Heuristic: if CC/CXX env vars point to clang-cl, or the generator is likely
    # to pick up clang-cl from PATH, force MSVC rc.exe. Harmless on pure MSVC too.
    #
    # Discovery order:
    #   1. WindowsSdkVerBinPath env var (set by VS Developer PowerShell)
    #   2. WindowsSdkDir + WindowsSDKVersion env vars
    #   3. Glob over installed SDK versions — survives SDK upgrades without code edits
    set(_fincept_sdk_hints
        "$ENV{WindowsSdkVerBinPath}x64"
        "$ENV{WindowsSdkDir}bin/$ENV{WindowsSDKVersion}x64")
    file(GLOB _fincept_sdk_dirs
        "C:/Program Files (x86)/Windows Kits/10/bin/10.*/x64"
        "C:/Program Files/Windows Kits/10/bin/10.*/x64")
    list(SORT _fincept_sdk_dirs ORDER DESCENDING)  # newest first
    list(APPEND _fincept_sdk_hints ${_fincept_sdk_dirs})
    find_program(_fincept_rc_exe NAMES rc rc.exe HINTS ${_fincept_sdk_hints})
    if(_fincept_rc_exe)
        set(CMAKE_RC_COMPILER "${_fincept_rc_exe}" CACHE FILEPATH "Windows resource compiler" FORCE)
        set(CMAKE_RC_COMPILER_INIT "${_fincept_rc_exe}")
    endif()
    unset(_fincept_sdk_hints)
    unset(_fincept_sdk_dirs)
endif()

project(FinceptTerminal VERSION 4.0.3 LANGUAGES C CXX)

# Strip C/C++ flags out of RC invocations — rc.exe only understands /D, /I, /fo.
if(WIN32)
    set(CMAKE_RC_FLAGS "/nologo" CACHE STRING "" FORCE)
    set(CMAKE_RC_FLAGS_RELEASE "" CACHE STRING "" FORCE)
    set(CMAKE_RC_FLAGS_DEBUG "" CACHE STRING "" FORCE)
    set(CMAKE_RC_FLAGS_RELWITHDEBINFO "" CACHE STRING "" FORCE)
    set(CMAKE_RC_FLAGS_MINSIZEREL "" CACHE STRING "" FORCE)
endif()

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
#
# PCH SUPPORT — without this, ccache rejects 99% of calls with
# "Could not use precompiled header" because MSVC-style PCH files include
# a serialised path that defeats the cache key. The sloppiness flags +
# CCACHE_PCH_EXTSUM tell ccache to hash the PCH's contents instead of its
# path. This must be set both at configure time (when CMake records the
# launcher script) and at build time (when the launcher actually runs),
# so we wrap ccache in a CMake-generated script that exports the env vars
# unconditionally.
find_program(FINCEPT_CCACHE_PROGRAM NAMES ccache sccache)
if(FINCEPT_CCACHE_PROGRAM)
    # Expose the same env vars to interactive `ccache -s` runs by writing a
    # ccache.conf in the cache dir. This is idempotent — ccache reads it on
    # every invocation, so changes here propagate without a clean rebuild.
    if(WIN32)
        set(_fincept_ccache_dir "$ENV{LOCALAPPDATA}/ccache")
    elseif(APPLE)
        set(_fincept_ccache_dir "$ENV{HOME}/Library/Caches/ccache")
    else()
        set(_fincept_ccache_dir "$ENV{HOME}/.ccache")
    endif()
    if(_fincept_ccache_dir AND IS_DIRECTORY "${_fincept_ccache_dir}")
        # ccache.conf format: KEY = VALUE per line. We write only the keys we
        # need; ccache merges with defaults for everything else.
        file(WRITE "${_fincept_ccache_dir}/ccache.conf"
            "sloppiness = pch_defines,time_macros,include_file_mtime,include_file_ctime,locale\n"
            "pch_external_checksum = true\n"
            "depend_mode = true\n"
            "compiler_check = content\n"
            "max_size = 5G\n")
        message(STATUS "Wrote ccache config to ${_fincept_ccache_dir}/ccache.conf")
    endif()

    # Wrap ccache in a launcher script that exports the env vars unconditionally
    # — belt-and-braces in case ccache.conf isn't picked up (e.g. when the user
    # has CCACHE_DIR set to a non-default location).
    if(WIN32)
        set(_fincept_ccache_launcher "${CMAKE_BINARY_DIR}/ccache_launcher.cmd")
        file(WRITE "${_fincept_ccache_launcher}"
            "@echo off\r\n"
            "set CCACHE_SLOPPINESS=pch_defines,time_macros,include_file_mtime,include_file_ctime,locale\r\n"
            "set CCACHE_PCH_EXTSUM=true\r\n"
            "set CCACHE_DEPEND=1\r\n"
            "set CCACHE_COMPILERCHECK=content\r\n"
            "\"${FINCEPT_CCACHE_PROGRAM}\" %*\r\n")
    else()
        set(_fincept_ccache_launcher "${CMAKE_BINARY_DIR}/ccache_launcher.sh")
        file(WRITE "${_fincept_ccache_launcher}"
            "#!/bin/sh\n"
            "export CCACHE_SLOPPINESS=pch_defines,time_macros,include_file_mtime,include_file_ctime,locale\n"
            "export CCACHE_PCH_EXTSUM=true\n"
            "export CCACHE_DEPEND=1\n"
            "export CCACHE_COMPILERCHECK=content\n"
            "exec \"${FINCEPT_CCACHE_PROGRAM}\" \"$@\"\n")
        execute_process(COMMAND chmod +x "${_fincept_ccache_launcher}")
    endif()

    set(CMAKE_CXX_COMPILER_LAUNCHER "${_fincept_ccache_launcher}")
    set(CMAKE_C_COMPILER_LAUNCHER   "${_fincept_ccache_launcher}")
    message(STATUS "Compiler cache enabled: ${FINCEPT_CCACHE_PROGRAM} (PCH-aware launcher)")
endif()

# ── macOS rpath configuration ────────────────────────────────────────────────
# Set install rpaths so the binary looks inside its own bundle for frameworks.
# Without this, CMake embeds build-tree rpaths (e.g. _deps/qgeoview-build/lib)
# and CPack IFW fails trying to delete them after macdeployqt already rewrote them.
if(APPLE)
    # If not explicitly provided by the caller/preset, pin the minimum
    # supported macOS version so builds on newer runners do not accidentally
    # produce bundles that require that newer host OS at runtime.
    if(NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET OR CMAKE_OSX_DEPLOYMENT_TARGET STREQUAL "")
        set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING
            "Minimum macOS version supported by Fincept Terminal" FORCE)
    endif()
    set(CMAKE_MACOSX_RPATH ON)
    set(CMAKE_INSTALL_RPATH "@executable_path/../Frameworks;@executable_path/../lib")
    set(CMAKE_BUILD_WITH_INSTALL_RPATH OFF)
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH FALSE)
    message(STATUS "macOS deployment target: ${CMAKE_OSX_DEPLOYMENT_TARGET}")
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

# ── LTO (whole-program optimisation) ─────────────────────────────────────────
# /GL + /LTCG produce the smallest, fastest binary but make every incremental
# link a full whole-program rebuild over 400+ object files — easily 30-90s per
# link on a developer machine. Default OFF so daily iteration is fast; CI and
# explicit release builds opt in.
option(FINCEPT_ENABLE_LTO
    "Enable MSVC /GL + /LTCG and equivalent on GCC/Clang (slow link, smaller/faster binary)"
    OFF)
if(DEFINED ENV{CI})
    set(FINCEPT_ENABLE_LTO ON CACHE BOOL "" FORCE)
endif()

# MSVC compiler flags for maximum build throughput:
#   /Z7        — embed debug info in .obj instead of shared PDB → no PDB write
#                contention (major speedup with parallel compilation)
#   /Oi        — enable intrinsics (faster codegen, no extra passes)
#   /bigobj    — allow >65535 sections (needed by large unity TUs and MOC output)
#
# /MP is intentionally NOT set: Ninja already parallelises at the cl.exe level
# (one cl.exe per TU, capped at host CPU count). Adding /MP makes each cl.exe
# fork N more workers, oversubscribing the box and *slowing* builds. /MP is
# only useful under MSBuild, which parallelises by project not by TU.
if(MSVC)
    # /permissive- : enforces ISO C++ conformance — catches MS-specific non-portable
    #                code that would break on GCC/Clang (two-phase lookup, etc.)
    # /W4          : level-4 warnings (highest practical without /Wall noise)
    # /WX only in CI so local iteration isn't blocked by new warnings
    add_compile_options(/Z7 /Oi /bigobj /permissive- /W4)
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

    # Whole-program optimisation. /GL marks each .obj as IL; /LTCG merges at
    # link time. Both required together — using one without the other warns
    # and silently disables. Gated behind FINCEPT_ENABLE_LTO so incremental
    # dev links stay fast (incremental linking is incompatible with /LTCG).
    if(FINCEPT_ENABLE_LTO)
        add_compile_options($<$<CONFIG:Release>:/GL>)
        add_link_options($<$<CONFIG:Release>:/LTCG:incremental>)
    else()
        # /INCREMENTAL with debug info needs /DEBUG to emit a usable PDB even
        # though we use /Z7 (objects carry debug). Keep linker incremental for
        # fast Release-mode iteration. CMake's default Release link flags set
        # /INCREMENTAL:NO; override here.
        add_link_options($<$<CONFIG:Release>:/INCREMENTAL>)
        add_link_options($<$<CONFIG:RelWithDebInfo>:/INCREMENTAL>)
        # Replace any /INCREMENTAL:NO that CMake's defaults inserted.
        foreach(flag_var
                CMAKE_EXE_LINKER_FLAGS_RELEASE
                CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO
                CMAKE_SHARED_LINKER_FLAGS_RELEASE
                CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO
                CMAKE_MODULE_LINKER_FLAGS_RELEASE
                CMAKE_MODULE_LINKER_FLAGS_RELWITHDEBINFO)
            string(REPLACE "/INCREMENTAL:NO" "/INCREMENTAL" ${flag_var} "${${flag_var}}")
        endforeach()
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

# ── Alpha Arena live-mode geofencing ─────────────────────────────────────────
# Live mode is gated by best-effort locale detection in AlphaArenaScreen.
# A handful of jurisdictions (US, NK, IR, …) are blocked. This flag exists
# *only* for development on machines that report a blocked locale; release
# builds must leave it OFF.
option(FINCEPT_UNSAFE_DISABLE_GEOFENCE
       "Alpha Arena: disable live-mode geofence (DEV ONLY)" OFF)
if(FINCEPT_UNSAFE_DISABLE_GEOFENCE)
    add_compile_definitions(FINCEPT_UNSAFE_DISABLE_GEOFENCE=1)
    message(WARNING "Alpha Arena: geofence DISABLED — do not ship this build.")
endif()

# ── Qt packages ──────────────────────────────────────────────────────────────
# Qt is pinned across all platforms. The shipped release artifact must use
# exactly the documented version to guarantee binary/ABI stability of the
# installer. Local development can tolerate patch drift.
#
# Pin modes (set via -DFINCEPT_QT_PIN_MODE=...):
#   EXACT  — require 6.8.3 exactly. Use for CI / release builds.
#   MINOR  — require 6.8.x (any patch). Default for developer builds — lets
#            "qt maintenance tool" patch updates work without breaking config.
#   ANY    — accept any Qt6 found on the system. Local experimentation only.
#
# Legacy FINCEPT_ALLOW_QT_DRIFT=ON is honoured and maps to ANY for back-compat.
set(FINCEPT_QT_VERSION 6.8.3)
set(FINCEPT_QT_VERSION_MINOR 6.8)
set(FINCEPT_QT_PIN_MODE "MINOR" CACHE STRING "Qt version pin mode: EXACT|MINOR|ANY")
set_property(CACHE FINCEPT_QT_PIN_MODE PROPERTY STRINGS EXACT MINOR ANY)
if(FINCEPT_ALLOW_QT_DRIFT)
    set(FINCEPT_QT_PIN_MODE "ANY")
endif()
# Force EXACT for tagged release builds.
if(FINCEPT_RELEASE_BUILD)
    set(FINCEPT_QT_PIN_MODE "EXACT")
    message(STATUS "FINCEPT_RELEASE_BUILD=ON → forcing Qt pin mode to EXACT (${FINCEPT_QT_VERSION})")
endif()

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

# Build the version arg list once based on pin mode, then reuse it.
if(FINCEPT_QT_PIN_MODE STREQUAL "EXACT")
    set(_FINCEPT_QT_VER_ARGS ${FINCEPT_QT_VERSION} EXACT)
elseif(FINCEPT_QT_PIN_MODE STREQUAL "MINOR")
    set(_FINCEPT_QT_VER_ARGS ${FINCEPT_QT_VERSION_MINOR})  # any 6.8.x
else()
    set(_FINCEPT_QT_VER_ARGS "")  # ANY: no version constraint
endif()

find_package(Qt6 ${_FINCEPT_QT_VER_ARGS} REQUIRED COMPONENTS
    Widgets Charts PrintSupport Network Sql Concurrent Multimedia)

# Optional: WebSockets (not always installed)
find_package(Qt6 ${_FINCEPT_QT_VER_ARGS} QUIET COMPONENTS WebSockets)
if(Qt6WebSockets_FOUND)
    set(HAS_WEBSOCKETS TRUE)
endif()

# Optional: MultimediaWidgets (for inline video/stream playback)
# Qt6::Multimedia is already required above (needed for QMediaPlayer in AiChatBubble / VideoPlayerWidget).
find_package(Qt6 ${_FINCEPT_QT_VER_ARGS} QUIET COMPONENTS MultimediaWidgets)
if(Qt6MultimediaWidgets_FOUND)
    set(HAS_MULTIMEDIA TRUE)
endif()

# Optional: TextToSpeech (cross-platform TTS — SAPI/AVSpeech/speech-dispatcher)
find_package(Qt6 ${_FINCEPT_QT_VER_ARGS} QUIET COMPONENTS TextToSpeech)
if(Qt6TextToSpeech_FOUND)
    set(HAS_TTS TRUE)
endif()

# Optional: WaylandClient — required to deploy Qt Wayland QPA plugin on Linux.
# Without this plugin, apps fall back to XWayland (often visually broken on
# fractional-scale Fedora/Ubuntu sessions) or fail outright on pure-Wayland
# compositors. Detection only; install rule is below in the Linux block.
if(UNIX AND NOT APPLE)
    find_package(Qt6 ${_FINCEPT_QT_VER_ARGS} QUIET COMPONENTS WaylandClient)
    if(Qt6WaylandClient_FOUND)
        set(HAS_WAYLAND TRUE)
    endif()
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
    # Also silence upstream warnings so CI's -Werror doesn't fail on third-party
    # code. /WX- is critical on MSVC: /W0 suppresses warnings but does NOT
    # disable the directory-inherited /WX (warnings-as-errors) that promotes
    # any surviving warning to a build-breaking error.
    if(MSVC)
        target_compile_options(QXlsx PRIVATE
            /UQT_NO_CAST_FROM_ASCII /UQT_NO_CAST_FROM_BYTEARRAY
            /UQT_NO_CAST_TO_ASCII   /UQT_NO_URL_CAST_FROM_STRING
            /W0 /WX-
        )
    else()
        target_compile_options(QXlsx PRIVATE
            -UQT_NO_CAST_FROM_ASCII -UQT_NO_CAST_FROM_BYTEARRAY
            -UQT_NO_CAST_TO_ASCII   -UQT_NO_URL_CAST_FROM_STRING
            -w -Wno-error
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
# Silence upstream warnings so CI's -Werror doesn't fail on third-party code.
# /WX- / -Wno-error needed in addition to /W0 / -w because CI adds -Werror (/WX)
# at the directory level, which suppression flags alone don't override.
foreach(_md4c_tgt md4c md4c-html md2html)
    if(TARGET ${_md4c_tgt})
        if(MSVC)
            target_compile_options(${_md4c_tgt} PRIVATE /W0 /WX-)
        else()
            target_compile_options(${_md4c_tgt} PRIVATE -w -Wno-error)
        endif()
    endif()
endforeach()
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
# Silence upstream warnings so CI's -Werror doesn't fail on third-party code.
if(MSVC)
    target_compile_options(qgeoview PRIVATE /W0 /WX-)
else()
    target_compile_options(qgeoview PRIVATE -w -Wno-error)
endif()

# Qt 6.8.3's FindWrapOpenGL.cmake adds "-framework AGL" which was removed in
# macOS 14 (Sonoma). The patch in setup.sh fixes this at the Qt install level.
# As a safety net, scrub AGL from qgeoview's resolved link libraries too.
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

# ── Single-instance enforcement: in-tree InstanceLock (replaced SingleApplication)
# Previously used the SingleApplication library, which combined QSharedMemory
# and QSystemSemaphore. That stack leaked on macOS Qt 6.6+ (QTBUG-111855),
# silently exiting on Finder / `open` launches — see issues #234 and #252.
# We now ship src/app/InstanceLock.{h,cpp}: a small QLocalServer-based lock
# that uses Unix domain sockets / Windows named pipes. Same effective API
# (primary/secondary + IPC for "open new window"), no semaphore leaks, no
# path-length traps, no sandbox quirks.

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
# Third-party code: silence warnings so CI's -Werror doesn't turn a benign
# upstream warning (e.g. -Wunused-but-set-variable on Clang) into a build failure.
if(MSVC)
    target_compile_options(qtadvanceddocking-qt6 PRIVATE /W0 /WX-)
else()
    target_compile_options(qtadvanceddocking-qt6 PRIVATE -w -Wno-error)
endif()

# ── ed25519 (orlp/ed25519) ───────────────────────────────────────────────────
# Public-domain Ed25519 reference implementation. We only use ed25519_verify
# in WalletService (signed-message challenge for Solana wallet connect).
# Upstream has no CMakeLists, so we build the .c files into a small static lib.
set(CMAKE_SKIP_INSTALL_RULES TRUE CACHE BOOL "" FORCE)
FetchContent_Declare(
    ed25519
    GIT_REPOSITORY https://github.com/orlp/ed25519.git
    GIT_TAG        b1f19fab4aebe607805620d25a5e42566ce46a0e  # master @ 2026-04-27
    GIT_SHALLOW    TRUE
    UPDATE_DISCONNECTED TRUE
)
FetchContent_MakeAvailable(ed25519)
file(GLOB FINCEPT_ED25519_SRC "${ed25519_SOURCE_DIR}/src/*.c")
add_library(fincept_ed25519 STATIC ${FINCEPT_ED25519_SRC})
target_include_directories(fincept_ed25519 PUBLIC "${ed25519_SOURCE_DIR}/src")
set_target_properties(fincept_ed25519 PROPERTIES
    UNITY_BUILD OFF
    POSITION_INDEPENDENT_CODE ON
    C_STANDARD 99
)
if(MSVC)
    target_compile_options(fincept_ed25519 PRIVATE /W0 /WX-)
else()
    target_compile_options(fincept_ed25519 PRIVATE -w -Wno-error)
endif()
set(CMAKE_SKIP_INSTALL_RULES FALSE CACHE BOOL "" FORCE)

# ── Source files by module ────────────────────────────────────────────────────

# Core
set(CORE_SOURCES
    src/core/actions/ActionRegistry.cpp
    src/core/actions/builtin_actions.cpp
    src/core/config/AppConfig.cpp
    src/core/config/AppPaths.cpp
    src/core/config/ProfileManager.cpp
    src/core/logging/Logger.cpp
    src/core/events/EventBus.cpp
    src/core/layout/LayoutTypes.cpp
    src/core/layout/LayoutCatalog.cpp
    src/core/layout/LayoutTemplates.cpp
    src/core/layout/WorkspaceMatcher.cpp
    src/core/layout/WorkspaceShell.cpp
    src/core/panel/PanelHandle.cpp
    src/core/panel/PanelMaterialiser.cpp
    src/core/panel/PanelRegistry.cpp
    src/core/profile/ProfilePaths.cpp
    src/core/screen/MonitorTopology.cpp
    src/core/screen/MonitorWatcher.cpp
    src/core/telemetry/TelemetryProvider.cpp
    src/core/telemetry/LocalTelemetrySink.cpp
    src/core/telemetry/CloudTelemetryProvider.cpp
    src/core/debug/StressLoad.cpp
    src/core/session/SessionManager.cpp
    src/core/session/ScreenStateManager.cpp
    src/core/keys/KeyConfigManager.cpp
    src/core/keys/WindowCycler.cpp
    src/core/net/NetSpeedMeter.cpp
    src/core/symbol/SymbolContext.cpp
    src/core/symbol/SymbolGroupRegistry.cpp
    src/core/symbol/SymbolMime.cpp
    src/core/symbol/SymbolDragSource.cpp
    src/core/components/ComponentCatalog.cpp
    src/core/components/PopularityTracker.cpp
    src/core/crash/CrashHandler.cpp
    src/core/window/WindowRegistry.cpp
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
    src/auth/SecurityAuditLog.cpp
    src/auth/lock/LockOverlayController.cpp
)

# Storage
set(STORAGE_SOURCES
    # Core databases
    src/storage/sqlite/Database.cpp
    src/storage/sqlite/CacheDatabase.cpp

    # Workspace persistence (Phase 2 — multi-window refactor)
    src/storage/workspace/WorkspaceDb.cpp
    src/storage/workspace/WorkspaceSnapshotRing.cpp
    src/storage/workspace/CrashRecovery.cpp
    src/storage/workspace/WorkspaceFwspImporter.cpp

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
    src/storage/repositories/OISnapshotsRepository.cpp
    src/storage/repositories/StrategiesRepository.cpp
    src/storage/repositories/FiiDiiRepository.cpp
    src/storage/repositories/IvHistoryRepository.cpp
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
    src/storage/sqlite/migrations/v019_sector_cache.cpp
    src/storage/sqlite/migrations/v020_security_audit_log.cpp
    src/storage/sqlite/migrations/v021_max_tokens_default.cpp
    src/storage/sqlite/migrations/v022_portfolio_broker_link.cpp
    src/storage/sqlite/migrations/v023_algo_trading_deploy.cpp
    src/storage/sqlite/migrations/v024_alpha_arena.cpp
    src/storage/sqlite/migrations/v025_oi_snapshots.cpp
    src/storage/sqlite/migrations/v026_strategies.cpp
    src/storage/sqlite/migrations/v027_fii_dii.cpp
    src/storage/sqlite/migrations/v028_iv_history.cpp
    src/storage/sqlite/migrations/v029_secure_credentials.cpp
)

# Python runner
set(PYTHON_SOURCES
    src/python/PythonRunner.cpp
    src/python/PythonSetupManager.cpp
    src/python/PythonWorker.cpp
    src/python/OptionGreeksWorker.cpp
)

# AI Chat
set(AI_CHAT_SOURCES
    src/ai_chat/LlmService.cpp
    src/ai_chat/LlmContentExtractors.cpp
    src/ai_chat/LlmHttpHelpers.cpp
    src/ai_chat/LlmRequestBuilders.cpp
    src/ai_chat/LlmModelsApi.cpp
    src/ai_chat/LlmFinceptAsync.cpp
    src/ai_chat/LlmToolLoop.cpp
    src/ai_chat/ModelCatalog.cpp
    src/ai_chat/ChatBubbleFactory.cpp
    src/ai_chat/AiChatScreen.cpp
    src/ai_chat/AiChatBubble.cpp
    src/ai_chat/ChatBubbleController.cpp
)

# MCP — Model Context Protocol system
set(MCP_SOURCES
    # Core
    src/mcp/McpProvider.cpp
    src/mcp/McpService.cpp
    src/mcp/McpClient.cpp
    src/mcp/McpManager.cpp
    src/mcp/McpInit.cpp
    src/mcp/SchemaValidator.cpp
    src/mcp/ToolRetriever.cpp
    src/mcp/TerminalMcpBridge.cpp

    # Phase 5 — provider-neutral tool-call dispatcher + adapters.
    src/mcp/dispatch/ToolDispatcher.cpp
    src/mcp/dispatch/OpenAiAdapter.cpp
    src/mcp/dispatch/AdapterFactory.cpp

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
    src/mcp/tools/ReportBuilderTools.cpp
    src/mcp/tools/MetaTools.cpp
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
    src/trading/ExchangeSession.cpp
    src/trading/ExchangeSessionManager.cpp
    src/trading/ExchangeDaemonPool.cpp
    # Native Kraken WebSocket v2 client
    src/trading/exchanges/kraken/KrakenSymbolMapper.cpp
    src/trading/exchanges/kraken/KrakenBook.cpp
    src/trading/exchanges/kraken/KrakenWsClient.cpp
    # Hyperliquid (Alpha Arena live mode — Phase 5c partial)
    src/trading/exchanges/hyperliquid/HyperliquidClient.cpp
    src/trading/exchanges/hyperliquid/Keccak256.cpp
    src/trading/exchanges/hyperliquid/HyperliquidSigner.cpp
    src/trading/exchanges/hyperliquid/HyperliquidVenue.cpp
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
    src/services/alpha_arena/AlphaArenaJson.cpp
    src/services/alpha_arena/Indicators.cpp
    src/services/alpha_arena/ContextBuilder.cpp
    src/services/alpha_arena/AlphaArenaRepo.cpp
    src/services/alpha_arena/RiskEngine.cpp
    src/services/alpha_arena/PaperVenue.cpp
    src/services/alpha_arena/SecretRedemptionServer.cpp
    src/services/alpha_arena/TickClock.cpp
    src/services/alpha_arena/ModelDispatcher.cpp
    src/services/alpha_arena/OrderRouter.cpp
    src/services/alpha_arena/AlphaArenaEngine.cpp
    src/services/alpha_arena/AlphaArenaService.cpp
    src/services/asia_markets/AsiaMarketsService.cpp
    src/services/crypto/TotpService.cpp
    src/services/python_cli/PythonCliService.cpp
    src/services/markets/MarketDataService.cpp
    src/services/options/OptionChainService.cpp
    src/services/options/OISnapshotter.cpp
    src/services/options/StrategyTemplates.cpp
    src/services/options/OptionPricing.cpp
    src/services/options/StrategyAnalytics.cpp
    src/services/options/FiiDiiService.cpp
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
    src/services/maritime/PortsCatalog.cpp
    src/services/ai_quant_lab/AIQuantLabService.cpp
    src/services/backtesting/BacktestingService.cpp
    src/services/algo_trading/AlgoTradingService.cpp
    src/services/portfolio/PortfolioService.cpp
    src/services/portfolio/PortfolioAnalyticsService.cpp
    src/services/quantlib/QuantLibClient.cpp
    src/services/economics/EconomicsService.cpp
    src/services/economics/MacroCalendarService.cpp
    src/services/agents/AgentService.cpp
    src/services/file_manager/FileManagerService.cpp

    # Wallet (Phase 1 — Solana wallet identity + balance display)
    src/services/wallet/Ed25519Verifier.cpp
    src/services/wallet/SolanaRpcClient.cpp
    src/services/wallet/LocalWalletBridge.cpp
    src/services/wallet/ConnectWalletDialog.cpp
    src/services/wallet/WalletService.cpp
    src/services/wallet/WalletTypes.cpp
    src/services/wallet/WalletBalanceProducer.cpp
    src/services/wallet/TokenPriceProducer.cpp
    # Wallet (Phase 2 — transaction-signing bridge + PumpPortal swap)
    src/services/wallet/WalletTxBridge.cpp
    src/services/wallet/SignTransactionDialog.cpp
    src/services/wallet/PumpFunSwapService.cpp
    # Wallet (Phase 2 Stage 2A.5 — multi-token holdings)
    src/services/wallet/TokenMetadataService.cpp
    # Wallet (Phase 2 Stage 2B — activity feed)
    src/services/wallet/WalletActivityProducer.cpp
    # Billing (Phase 2 Stage 2C — fee discount eligibility)
    src/services/billing/FeeDiscountService.cpp
    # Wallet (Phase 5 — buyback & burn dashboard producers)
    src/services/wallet/BuybackBurnService.cpp
    src/services/wallet/TreasuryService.cpp
    # Wallet (Phase 3 — STAKE / veFNCPT / real-yield)
    src/services/wallet/StakingService.cpp
    src/services/wallet/RealYieldService.cpp
    # Billing (Phase 3 — tier service driving cross-screen gating)
    src/services/billing/TierService.cpp
    src/services/sectors/SectorResolver.cpp
    src/services/workflow/NodeRegistry.cpp
    src/services/workflow/WorkflowService.cpp
    src/services/workflow/WorkflowExecutor.cpp
    src/services/workflow/ExpressionEngine.cpp

    src/services/pushpins/PushpinService.cpp
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
    # Phase 4 — Fincept Internal prediction-market adapter (demo mode until
    # `fincept.markets_endpoint` is configured and `fincept_market` lands).
    src/services/prediction/fincept_internal/FinceptInternalAdapter.cpp

    # Speech-to-text service (Python speech_recognition + Google API)
    src/services/stt/SpeechService.cpp

    # Text-to-speech service (Python pyttsx3 — used as a portable substitute
    # for Qt6::TextToSpeech when the Qt Speech module isn't installed).
    src/services/tts/TtsService.cpp

    # Clap-to-start trigger — lightweight pyaudio loop that fires a signal
    # when it hears a (double-)clap. Wakes up the AiChatBubble mic.
    src/services/voice_trigger/ClapDetectorService.cpp

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
    src/ui/widgets/GroupBadge.cpp
    src/ui/widgets/StatusBadge.cpp
    src/ui/pushpins/SymbolChip.cpp
    src/ui/pushpins/PushpinBar.cpp
    src/ui/components/ComponentCard.cpp
    src/ui/components/ComponentBrowserDialog.cpp
    src/ui/widgets/SearchBar.cpp
    src/ui/widgets/GeometricBackground.cpp
    src/ui/widgets/WorldMapWidget.cpp
    src/ui/widgets/LoadingOverlay.cpp
    src/ui/widgets/ConfettiOverlay.cpp
    src/ui/widgets/SpeedSparkline.cpp
    src/ui/tables/DataTable.cpp
    src/ui/charts/ChartFactory.cpp
    src/ui/navigation/NavigationBar.cpp
    src/ui/navigation/FKeyBar.cpp
    src/ui/navigation/StatusBar.cpp
    src/ui/navigation/ToolBar.cpp
    src/ui/navigation/CommandBar.cpp
    src/ui/navigation/DockToolBar.cpp
    src/ui/navigation/DockStatusBar.cpp
    src/ui/debug/DebugOverlay.cpp
    src/ui/command/CommandParser.cpp
    src/ui/command/SuggestionIndex.cpp
    src/ui/command/QuickCommandBar.cpp
    src/ui/command/CommandPalette.cpp
    src/ui/notifications/NotificationService.cpp
    src/ui/error/ErrorPipeline.cpp

    src/ui/workspace/LayoutOpenDialog.cpp
    src/ui/workspace/LayoutSaveAsDialog.cpp

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
    src/screens/settings/CredentialsSection.cpp
    src/screens/settings/AppearanceSection.cpp
    src/screens/settings/GeneralSection.cpp
    src/screens/settings/NotificationsSection.cpp
    src/screens/settings/StorageSection.cpp
    src/screens/settings/DataSourcesSection.cpp
    src/screens/settings/LlmConfigSection.cpp
    src/screens/settings/McpServersSection.cpp
    src/screens/settings/LoggingSection.cpp
    src/screens/settings/SecuritySection.cpp
    src/screens/settings/ProfilesSection.cpp
    src/screens/settings/KeybindingsSection.cpp
    src/screens/settings/PythonEnvSection.cpp
    src/screens/settings/DeveloperSection.cpp
    src/screens/settings/VoiceConfigSection.cpp
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
    src/screens/launchpad/LaunchpadScreen.cpp
    src/screens/launchpad/OnboardingTour.cpp
    src/screens/recovery/CrashRecoveryDialog.cpp
    src/screens/info/ContactScreen.cpp
    src/screens/info/TermsScreen.cpp
    src/screens/info/PrivacyScreen.cpp
    src/screens/info/TrademarksScreen.cpp
    src/screens/info/HelpScreen.cpp
    src/screens/file_manager/FileManagerScreen.cpp
    src/screens/relationship_map/RelationshipMapScreen.cpp
    src/screens/relationship_map/RelationshipGraphScene.cpp
    src/services/relationship_map/RelationshipMapService.cpp
    src/services/report_builder/ReportBuilderService.cpp
    src/services/report_builder/ReportBuilderTemplates.cpp
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
    src/screens/dashboard/widgets/LoadingOverlay.cpp
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
    src/screens/dashboard/widgets/WebScraperWidget.cpp
    src/screens/dashboard/widgets/GeopoliticsEventsWidget.cpp
    src/screens/dashboard/widgets/MaritimeVesselsWidget.cpp
    src/screens/dashboard/widgets/NotesWidget.cpp
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
    src/screens/data_sources/DataSourcesHelpers.cpp
    src/screens/data_sources/ConnectionConfigDialog.cpp
    src/screens/data_sources/ConnectionTester.cpp
    src/screens/data_sources/ImportExportConnections.cpp
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
    src/screens/surface_analytics/SurfaceLineWidget.cpp
    src/screens/surface_analytics/SurfaceControlPanel.cpp
    src/screens/surface_analytics/SurfaceDataInspector.cpp
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

    # F&O / Options (Sensibull-style tab)
    src/screens/fno/FnoScreen.cpp
    src/screens/fno/FnoHeaderBar.cpp
    src/screens/fno/OptionChainModel.cpp
    src/screens/fno/OptionChainTable.cpp
    src/screens/fno/ChainSubTab.cpp
    src/screens/fno/BuilderSubTab.cpp
    src/screens/fno/BuilderAnalyticsRibbon.cpp
    src/screens/fno/TemplatePickerPanel.cpp
    src/screens/fno/LegEditorTable.cpp
    src/screens/fno/PayoffChartWidget.cpp
    src/screens/fno/OrderConfirmDialog.cpp
    src/screens/fno/OISubTab.cpp
    src/screens/fno/MultiStrikeOIChart.cpp
    src/screens/fno/MaxPainChart.cpp
    src/screens/fno/OIBuildupTable.cpp
    src/screens/fno/IntradayOIChart.cpp
    src/screens/fno/FiiDiiSubTab.cpp
    src/screens/fno/FiiDiiChart.cpp
    src/screens/fno/MultiStraddleSubTab.cpp
    src/screens/fno/MultiStraddleChart.cpp
    src/screens/fno/ScreenerSubTab.cpp

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
    src/screens/ai_quant_lab/QuantModulePanel_GS.cpp
    src/screens/ai_quant_lab/QuantModulePanel_Functime.cpp
    src/screens/ai_quant_lab/QuantModulePanel_Statsmodels.cpp
    src/screens/ai_quant_lab/QuantModulePanel_Fortitudo.cpp
    src/screens/ai_quant_lab/QuantModulePanel_Gluonts.cpp
    src/screens/ai_quant_lab/QuantModulePanel_CFA.cpp
    src/screens/ai_quant_lab/QuantModulePanel_Backtesting.cpp
    src/screens/ai_quant_lab/QuantModulePanel_RL.cpp
    src/screens/ai_quant_lab/QuantModulePanel_DeepAgent.cpp
    src/screens/ai_quant_lab/QuantModulePanel_QuantReporting.cpp
    src/screens/ai_quant_lab/QuantModulePanel_AdvancedDisplays.cpp
    src/screens/ai_quant_lab/QuantModulePanel_AIMLDisplays.cpp
    src/screens/ai_quant_lab/QuantModulePanel_CoreDisplays.cpp
    src/screens/ai_quant_lab/QuantModulePanel_Misc.cpp
    src/screens/ai_quant_lab/QuantModulePanel_HFT.cpp
    src/screens/ai_quant_lab/QuantModulePanel_LiveLearning.cpp

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
    src/screens/polymarket/PolymarketOrderBlotter.cpp

    # Alpha Arena
    src/screens/alpha_arena/AlphaArenaScreen.cpp
    src/screens/alpha_arena/panels/LeaderboardPanel.cpp
    src/screens/alpha_arena/panels/ModelChatPanel.cpp
    src/screens/alpha_arena/panels/StatePanels.cpp
    src/screens/alpha_arena/panels/LiveModeGateDialog.cpp

    # Alternative Investments
    src/screens/alt_investments/AltInvestmentsScreen.cpp

    # MCP Servers
    src/screens/mcp_servers/McpServersScreen.cpp

    # QuantLib Suite
    src/screens/quantlib/QuantLibScreen.cpp

    # M&A Analytics
    src/screens/ma_analytics/MAAnalyticsScreen.cpp
    src/screens/ma_analytics/MAModulePanel.cpp
    src/screens/ma_analytics/MAValuationPanel.cpp
    src/screens/ma_analytics/MAMergerPanel.cpp
    src/screens/ma_analytics/MADealsPanel.cpp
    src/screens/ma_analytics/MAStartupPanel.cpp
    src/screens/ma_analytics/MAFairnessPanel.cpp
    src/screens/ma_analytics/MAIndustryPanel.cpp
    src/screens/ma_analytics/MAAdvancedPanel.cpp
    src/screens/ma_analytics/MAComparisonPanel.cpp

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
    src/screens/portfolio/PortfolioInsightsPanel.cpp
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

    # Crypto Center
    src/screens/crypto_center/CryptoCenterScreen.cpp
    src/screens/crypto_center/HoldingsBar.cpp
    src/screens/crypto_center/WalletActionConfirmDialog.cpp
    src/screens/crypto_center/tabs/ComingSoonTab.cpp
    src/screens/crypto_center/tabs/HomeTab.cpp
    src/screens/crypto_center/tabs/SettingsTab.cpp
    src/screens/crypto_center/tabs/TradeTab.cpp
    src/screens/crypto_center/tabs/ActivityTab.cpp
    src/screens/crypto_center/panels/SwapPanel.cpp
    src/screens/crypto_center/panels/HoldingsTable.cpp
    src/screens/crypto_center/panels/FeeDiscountPanel.cpp
    # Crypto Center — Phase 5 (buyback & burn dashboard)
    src/screens/crypto_center/tabs/RoadmapTab.cpp
    src/screens/crypto_center/panels/BuybackBurnPanel.cpp
    src/screens/crypto_center/panels/SupplyChartPanel.cpp
    src/screens/crypto_center/panels/TreasuryPanel.cpp
    src/screens/crypto_center/tabs/StakeTab.cpp
    src/screens/crypto_center/panels/ActiveLocksPanel.cpp
    src/screens/crypto_center/panels/LockPanel.cpp
    src/screens/crypto_center/panels/TierPanel.cpp
    # Crypto Center — Phase 4 (MARKETS tab — internal prediction markets)
    src/screens/crypto_center/tabs/MarketsTab.cpp
    src/screens/crypto_center/panels/MarketsListPanel.cpp
)

# App
set(APP_SOURCES
    src/app/main.cpp
    src/app/InstanceLock.cpp
    src/app/WindowFrame.cpp
    src/app/ScreenRouter.cpp
    src/app/DockScreenRouter.cpp
    src/app/TerminalShell.cpp
    src/app/MonitorPickerDialog.cpp
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
    src/screens/ai_quant_lab/QuantModulePanel_GS.cpp
    src/screens/ai_quant_lab/QuantModulePanel_Functime.cpp
    src/screens/ai_quant_lab/QuantModulePanel_Statsmodels.cpp
    src/screens/ai_quant_lab/QuantModulePanel_Fortitudo.cpp
    src/screens/ai_quant_lab/QuantModulePanel_Gluonts.cpp
    src/screens/ai_quant_lab/QuantModulePanel_CFA.cpp
    src/screens/ai_quant_lab/QuantModulePanel_Backtesting.cpp
    src/screens/ai_quant_lab/QuantModulePanel_RL.cpp
    src/screens/ai_quant_lab/QuantModulePanel_DeepAgent.cpp
    src/screens/ai_quant_lab/QuantModulePanel_QuantReporting.cpp
    src/screens/ai_quant_lab/QuantModulePanel_AdvancedDisplays.cpp
    src/screens/ai_quant_lab/QuantModulePanel_AIMLDisplays.cpp
    src/screens/ai_quant_lab/QuantModulePanel_CoreDisplays.cpp
    src/screens/ai_quant_lab/QuantModulePanel_Misc.cpp
    src/screens/ai_quant_lab/QuantModulePanel_HFT.cpp
    src/screens/ai_quant_lab/QuantModulePanel_LiveLearning.cpp
    src/screens/akshare/AkShareScreen.cpp
    src/screens/algo_trading/AlgoTradingScreen.cpp
    src/screens/algo_trading/DeploymentDashboard.cpp
    src/screens/algo_trading/ScannerPanel.cpp
    src/screens/algo_trading/StrategyBuilderPanel.cpp
    src/screens/algo_trading/StrategyListPanel.cpp
    src/screens/alpha_arena/AlphaArenaScreen.cpp
    src/screens/alpha_arena/panels/LeaderboardPanel.cpp
    src/screens/alpha_arena/panels/ModelChatPanel.cpp
    src/screens/alpha_arena/panels/StatePanels.cpp
    src/screens/alpha_arena/panels/LiveModeGateDialog.cpp
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
    src/screens/data_sources/DataSourcesHelpers.cpp
    src/screens/data_sources/ConnectionConfigDialog.cpp
    src/screens/data_sources/ConnectionTester.cpp
    src/screens/data_sources/ImportExportConnections.cpp
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
    src/screens/dashboard/canvas/AddWidgetDialog.cpp
    src/screens/dashboard/canvas/DashboardCanvas.cpp
    src/screens/dashboard/canvas/DashboardTemplates.cpp
    src/screens/dashboard/canvas/PlaceholderOverlay.cpp
    src/screens/dashboard/canvas/TemplatePicker.cpp
    src/screens/dashboard/canvas/WidgetRegistry.cpp
    src/screens/dashboard/canvas/WidgetTile.cpp
    src/screens/dashboard/widgets/BaseWidget.cpp
    src/screens/dashboard/widgets/LoadingOverlay.cpp
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
    src/screens/dashboard/widgets/WebScraperWidget.cpp
    src/screens/dashboard/widgets/GeopoliticsEventsWidget.cpp
    src/screens/dashboard/widgets/MaritimeVesselsWidget.cpp
    src/screens/dashboard/widgets/NotesWidget.cpp
    src/screens/data_mapping/DataMappingScreen.cpp
    src/screens/crypto_center/CryptoCenterScreen.cpp
    src/screens/crypto_center/HoldingsBar.cpp
    src/screens/crypto_center/WalletActionConfirmDialog.cpp
    src/screens/crypto_center/tabs/ComingSoonTab.cpp
    src/screens/crypto_center/tabs/HomeTab.cpp
    src/screens/crypto_center/tabs/SettingsTab.cpp
    src/screens/crypto_center/tabs/TradeTab.cpp
    src/screens/crypto_center/tabs/ActivityTab.cpp
    src/screens/crypto_center/panels/SwapPanel.cpp
    src/screens/crypto_center/panels/HoldingsTable.cpp
    src/screens/crypto_center/panels/FeeDiscountPanel.cpp
    src/screens/crypto_center/tabs/RoadmapTab.cpp
    src/screens/crypto_center/panels/BuybackBurnPanel.cpp
    src/screens/crypto_center/panels/SupplyChartPanel.cpp
    src/screens/crypto_center/panels/TreasuryPanel.cpp
    src/screens/crypto_center/tabs/StakeTab.cpp
    src/screens/crypto_center/panels/ActiveLocksPanel.cpp
    src/screens/crypto_center/panels/LockPanel.cpp
    src/screens/crypto_center/panels/TierPanel.cpp
    src/screens/crypto_center/tabs/MarketsTab.cpp
    src/screens/crypto_center/panels/MarketsListPanel.cpp
    src/screens/dbnomics/DBnomicsChartWidget.cpp
    src/screens/dbnomics/DBnomicsDataTable.cpp
    src/screens/dbnomics/DBnomicsScreen.cpp
    src/screens/dbnomics/DBnomicsSelectionPanel.cpp
    src/screens/docs/DocsScreen.cpp
    src/screens/derivatives/DerivativesScreen.cpp
    src/screens/fno/FnoScreen.cpp
    src/screens/fno/FnoHeaderBar.cpp
    src/screens/fno/OptionChainModel.cpp
    src/screens/fno/OptionChainTable.cpp
    src/screens/fno/ChainSubTab.cpp
    src/screens/fno/BuilderSubTab.cpp
    src/screens/fno/BuilderAnalyticsRibbon.cpp
    src/screens/fno/TemplatePickerPanel.cpp
    src/screens/fno/LegEditorTable.cpp
    src/screens/fno/PayoffChartWidget.cpp
    src/screens/fno/OrderConfirmDialog.cpp
    src/screens/fno/OISubTab.cpp
    src/screens/fno/MultiStrikeOIChart.cpp
    src/screens/fno/MaxPainChart.cpp
    src/screens/fno/OIBuildupTable.cpp
    src/screens/fno/IntradayOIChart.cpp
    src/screens/fno/FiiDiiSubTab.cpp
    src/screens/fno/FiiDiiChart.cpp
    src/screens/fno/MultiStraddleSubTab.cpp
    src/screens/fno/MultiStraddleChart.cpp
    src/screens/fno/ScreenerSubTab.cpp
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
    src/screens/ma_analytics/MAValuationPanel.cpp
    src/screens/ma_analytics/MAMergerPanel.cpp
    src/screens/ma_analytics/MADealsPanel.cpp
    src/screens/ma_analytics/MAStartupPanel.cpp
    src/screens/ma_analytics/MAFairnessPanel.cpp
    src/screens/ma_analytics/MAIndustryPanel.cpp
    src/screens/ma_analytics/MAAdvancedPanel.cpp
    src/screens/ma_analytics/MAComparisonPanel.cpp
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
    src/screens/portfolio/PortfolioInsightsPanel.cpp
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
    src/screens/settings/AppearanceSection.cpp
    src/screens/settings/CredentialsSection.cpp
    src/screens/settings/DataSourcesSection.cpp
    src/screens/settings/DeveloperSection.cpp
    src/screens/settings/GeneralSection.cpp
    src/screens/settings/KeybindingsSection.cpp
    src/screens/settings/LlmConfigSection.cpp
    src/screens/settings/LoggingSection.cpp
    src/screens/settings/McpServersSection.cpp
    src/screens/settings/NotificationsSection.cpp
    src/screens/settings/ProfilesSection.cpp
    src/screens/settings/PythonEnvSection.cpp
    src/screens/settings/SecuritySection.cpp
    src/screens/settings/SettingsScreen.cpp
    src/screens/settings/StorageSection.cpp
    src/screens/setup/SetupScreen.cpp
    src/screens/launchpad/LaunchpadScreen.cpp
    src/screens/launchpad/OnboardingTour.cpp
    src/screens/recovery/CrashRecoveryDialog.cpp
    src/screens/support/SupportScreen.cpp
    src/screens/surface_analytics/Surface3DWidget.cpp
    src/screens/surface_analytics/SurfaceAnalyticsScreen.cpp
    src/screens/surface_analytics/SurfaceControlPanel.cpp
    src/screens/surface_analytics/SurfaceCsvImporter.cpp
    src/screens/surface_analytics/SurfaceDataInspector.cpp
    src/screens/surface_analytics/SurfaceDemoData.cpp
    src/screens/surface_analytics/SurfaceLineWidget.cpp
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
    src/storage/sqlite/migrations/v019_sector_cache.cpp
    src/storage/sqlite/migrations/v020_security_audit_log.cpp
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
    src/app/WindowFrame.cpp
    src/app/DockScreenRouter.cpp
    src/app/TerminalShell.cpp
    PROPERTIES SKIP_UNITY_BUILD_INCLUSION TRUE
)

# Alpha Arena services and Hyperliquid integration use anonymous namespaces
# and Q_OBJECT-bearing helpers — exclude from unity to avoid the kind of
# ODR clashes that masked compile errors in early Phase 5c integration.
set_source_files_properties(
    src/services/alpha_arena/AlphaArenaJson.cpp
    src/services/alpha_arena/Indicators.cpp
    src/services/alpha_arena/ContextBuilder.cpp
    src/services/alpha_arena/AlphaArenaRepo.cpp
    src/services/alpha_arena/RiskEngine.cpp
    src/services/alpha_arena/PaperVenue.cpp
    src/services/alpha_arena/SecretRedemptionServer.cpp
    src/services/alpha_arena/TickClock.cpp
    src/services/alpha_arena/ModelDispatcher.cpp
    src/services/alpha_arena/OrderRouter.cpp
    src/services/alpha_arena/AlphaArenaEngine.cpp
    src/services/alpha_arena/AlphaArenaService.cpp
    src/trading/exchanges/hyperliquid/HyperliquidClient.cpp
    src/trading/exchanges/hyperliquid/Keccak256.cpp
    src/trading/exchanges/hyperliquid/HyperliquidSigner.cpp
    src/trading/exchanges/hyperliquid/HyperliquidVenue.cpp
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
    src/mcp/SchemaValidator.cpp
    src/mcp/ToolRetriever.cpp
    src/mcp/TerminalMcpBridge.cpp
    src/mcp/dispatch/ToolDispatcher.cpp
    src/mcp/dispatch/OpenAiAdapter.cpp
    src/mcp/dispatch/AdapterFactory.cpp
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
    src/mcp/tools/MetaTools.cpp
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
    # Generate app.rc from app.rc.in so FILEVERSION / PRODUCTVERSION / string
    # table reflect the project() VERSION. Without this the exe's PE version
    # resource is 0.0.0.0 and Windows Error Reporting cannot triage crashes
    # by app version (see issue #215). The .ico reference is relative to the
    # generated file's directory, so copy fincept.ico next to it.
    set(FINCEPT_VER_MAJOR ${PROJECT_VERSION_MAJOR})
    set(FINCEPT_VER_MINOR ${PROJECT_VERSION_MINOR})
    set(FINCEPT_VER_PATCH ${PROJECT_VERSION_PATCH})
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/app.rc.in"
        "${CMAKE_CURRENT_BINARY_DIR}/app.rc"
        @ONLY)
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/fincept.ico"
        "${CMAKE_CURRENT_BINARY_DIR}/fincept.ico"
        COPYONLY)
    set(_FINCEPT_PLATFORM_SOURCES "${CMAKE_CURRENT_BINARY_DIR}/app.rc")
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

# ── macOS: make Qt's openssl TLS plugin discoverable ─────────────────────────
# Qt's libqopensslbackend.dylib only searches /usr/local/lib, /opt/local/lib,
# /sw/lib, /usr/lib and Qt's own lib dir for libssl/libcrypto. Homebrew on
# Apple Silicon installs to /opt/homebrew which Qt does not search, so the
# openssl plugin fails to register and Qt falls back to the deprecated Apple
# SecureTransport backend — which double-frees in SSLWrite during QWebSocket
# close and crashes the crypto tab.
#
# Symlink the OpenSSL dylibs from OPENSSL_ROOT_DIR into Qt's lib dir at
# configure time so the openssl plugin loads and `QT_TLS_BACKEND=openssl`
# (set in main.cpp) actually takes effect. Runs at configure (idempotent),
# so it survives clean checkouts and Qt updates.
if(APPLE AND OPENSSL_ROOT_DIR)
    cmake_path(GET Qt6_DIR PARENT_PATH _ft_qt_cmake_dir)            # .../lib/cmake
    cmake_path(GET _ft_qt_cmake_dir PARENT_PATH _ft_qt_lib_dir)     # .../lib
    set(_ft_openssl_libs
        libcrypto.dylib       libssl.dylib
        libcrypto.3.dylib     libssl.3.dylib
    )
    foreach(_ft_lib IN LISTS _ft_openssl_libs)
        set(_ft_src "${OPENSSL_ROOT_DIR}/lib/${_ft_lib}")
        set(_ft_dst "${_ft_qt_lib_dir}/${_ft_lib}")
        if(EXISTS "${_ft_src}" AND NOT EXISTS "${_ft_dst}")
            execute_process(
                COMMAND "${CMAKE_COMMAND}" -E create_symlink "${_ft_src}" "${_ft_dst}"
                RESULT_VARIABLE _ft_rc
            )
            if(_ft_rc EQUAL 0)
                message(STATUS "Symlinked ${_ft_lib} into Qt lib dir for TLS plugin discovery")
            endif()
        endif()
    endforeach()
endif()

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
    qtadvanceddocking-qt6
    OpenSSL::SSL
    OpenSSL::Crypto
    fincept_ed25519
)

# SecureStorage no longer needs platform-specific credential frameworks —
# it's pure SQLite + AES-256-GCM (OpenSSL) on every platform. See
# storage/secure/SecureStorage.cpp.

# Windows: link dbghelp for the crash-handler minidump writer (see
# src/core/crash/CrashHandler.cpp).
if(WIN32)
    target_link_libraries(FinceptTerminal PRIVATE dbghelp)
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
    # Derive the Qt bin/ dir from Qt6_DIR (which CMake set during find_package),
    # so we don't hardcode "C:/Qt/<version>/msvc2022_64/bin" — that path breaks
    # the moment a developer installs to a non-default location or upgrades.
    # Qt6_DIR points at <prefix>/lib/cmake/Qt6, so go up three levels.
    set(_fincept_qt_bin_hints "$ENV{QT_ROOT_DIR}/bin")
    if(Qt6_DIR)
        get_filename_component(_fincept_qt_prefix "${Qt6_DIR}/../../.." ABSOLUTE)
        list(APPEND _fincept_qt_bin_hints "${_fincept_qt_prefix}/bin")
    endif()
    find_program(WINDEPLOYQT_EXE windeployqt HINTS ${_fincept_qt_bin_hints})
    unset(_fincept_qt_bin_hints)
    if(WINDEPLOYQT_EXE)
        # Always deploy Qt DLLs/plugins next to the exe. windeployqt is
        # idempotent and copy_if_different — incremental cost is ~1-2 s
        # when nothing changed. The previous DEPLOY_QT=ON gate caused the
        # exe to run against a stale plugin set after a clean rebuild,
        # producing 0xc0000005 crashes on startup.
        add_custom_command(TARGET FinceptTerminal POST_BUILD
            COMMAND "${WINDEPLOYQT_EXE}"
            ARGS "--no-translations" "--no-compiler-runtime" "$<TARGET_FILE:FinceptTerminal>"
            COMMENT "Running windeployqt..."
        )
    endif()

    # ── Optional: override sqldrivers from a known-consistent Qt install ──────
    # Some local Qt installs end up with Qt6Core/Qt6Sql at one patch level and
    # sqldrivers/qsqlite.dll at an older one (the Qt Maintenance Tool sometimes
    # skips plugin updates), causing 0xc0000005 in qsqlite.dll on startup.
    # If you have a clean Qt install (e.g. from aqtinstall) and want to source
    # the SQL plugins from there, set FINCEPT_QT_CONSISTENT_PLUGINS_DIR to its
    # plugins/ directory. Empty (default) = skip override entirely.
    set(FINCEPT_QT_CONSISTENT_PLUGINS_DIR "" CACHE PATH
        "Optional: path to a clean Qt plugins/ dir to overwrite sqldrivers from")
    if(FINCEPT_QT_CONSISTENT_PLUGINS_DIR
       AND EXISTS "${FINCEPT_QT_CONSISTENT_PLUGINS_DIR}/sqldrivers/qsqlite.dll")
        add_custom_command(TARGET FinceptTerminal POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:FinceptTerminal>/sqldrivers"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${FINCEPT_QT_CONSISTENT_PLUGINS_DIR}/sqldrivers/qsqlite.dll"
                "$<TARGET_FILE_DIR:FinceptTerminal>/sqldrivers/qsqlite.dll"
            COMMENT "Overwriting sqldrivers from FINCEPT_QT_CONSISTENT_PLUGINS_DIR..."
        )
    elseif(FINCEPT_QT_CONSISTENT_PLUGINS_DIR)
        message(WARNING
            "FINCEPT_QT_CONSISTENT_PLUGINS_DIR set to '${FINCEPT_QT_CONSISTENT_PLUGINS_DIR}' "
            "but sqldrivers/qsqlite.dll not found there — skipping override.")
    endif()
endif()

# ── Copy Python scripts to build output ──────────────────────────────────────
# POST_BUILD drags the whole scripts/ tree over. Afterwards we prune junk
# that must never ship — backup folders (agents.deleted.*), test caches,
# local SQLite databases — because on macOS anything unexpected in the
# .app bundle makes codesign reject it ("bundle format unrecognized").
add_custom_command(TARGET FinceptTerminal POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/scripts"
        "$<TARGET_FILE_DIR:FinceptTerminal>/scripts"
    COMMAND ${CMAKE_COMMAND} -DSCRIPTS_DIR=$<TARGET_FILE_DIR:FinceptTerminal>/scripts
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/prune_scripts_junk.cmake"
    COMMENT "Copying Python scripts to build output + pruning junk..."
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
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/component_catalog.json"
        "$<TARGET_FILE_DIR:FinceptTerminal>/resources/component_catalog.json"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:FinceptTerminal>/resources/wallet"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/wallet/connect.html"
        "$<TARGET_FILE_DIR:FinceptTerminal>/resources/wallet/connect.html"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/wallet/swap.html"
        "$<TARGET_FILE_DIR:FinceptTerminal>/resources/wallet/swap.html"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:FinceptTerminal>/resources/wallet/vendor"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/wallet/vendor"
        "$<TARGET_FILE_DIR:FinceptTerminal>/resources/wallet/vendor"
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
            DESTINATION "scripts" COMPONENT scripts
            REGEX "(\\.deleted\\.|\\.pytest_cache|\\.benchmarks|__pycache__|\\.pyc$|\\.db$|\\.sqlite$|\\.sqlite3$|\\.bak$|\\.orig$)" EXCLUDE)
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
    # Linux: FHS-compliant layout. Architecture-neutral — works for x86_64
    # and aarch64 (ARM64) without changes; CMake just installs the binary
    # produced for the target arch into share/fincept-terminal/. To build for
    # ARM64 cross-compile, configure with `-DCMAKE_SYSTEM_PROCESSOR=aarch64`
    # and an aarch64 Qt SDK on CMAKE_PREFIX_PATH.
    install(TARGETS FinceptTerminal
            RUNTIME DESTINATION "bin" COMPONENT core)

    # Qt Wayland QPA plugin — required so the app renders correctly on
    # Wayland sessions (Fedora 36+, Ubuntu 22.04+ default to Wayland).
    # Without this, Qt falls back to XWayland which has fractional-scale
    # bugs, broken HiDPI, and may fail entirely on pure-Wayland compositors.
    if(HAS_WAYLAND AND Qt6_DIR)
        get_filename_component(_fincept_qt_prefix "${Qt6_DIR}/../../.." ABSOLUTE)
        set(_fincept_wayland_plugin "${_fincept_qt_prefix}/plugins/platforms/libqwayland-generic.so")
        if(EXISTS "${_fincept_wayland_plugin}")
            install(FILES "${_fincept_wayland_plugin}"
                    DESTINATION "share/fincept-terminal/plugins/platforms"
                    COMPONENT core)
        endif()
        # Also bundle wayland-decoration-client and wayland shell integration plugins.
        foreach(_dir wayland-decoration-client wayland-shell-integration wayland-graphics-integration-client)
            if(EXISTS "${_fincept_qt_prefix}/plugins/${_dir}")
                install(DIRECTORY "${_fincept_qt_prefix}/plugins/${_dir}"
                        DESTINATION "share/fincept-terminal/plugins"
                        COMPONENT core)
            endif()
        endforeach()
    endif()

    # Python analytics scripts
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/scripts/"
            DESTINATION "share/fincept-terminal/scripts"
            USE_SOURCE_PERMISSIONS COMPONENT scripts
            REGEX "(\\.deleted\\.|\\.pytest_cache|\\.benchmarks|__pycache__|\\.pyc$|\\.db$|\\.sqlite$|\\.sqlite3$|\\.bak$|\\.orig$)" EXCLUDE)

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
        # Full resources/ tree — icons, images, stylesheets, data files, etc.
        # Also catches requirements-*.txt. Without this, asset paths resolved
        # via AppPaths::resources_dir() return an empty dir and screens that
        # load PNGs / JSON config crash on missing-file errors.
        install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/resources/"
                DESTINATION "resources" COMPONENT core
                PATTERN ".git*" EXCLUDE
                PATTERN "*.pyc" EXCLUDE)
        # qt.conf — pins plugin/library paths relative to the exe. Without
        # this Qt uses hardcoded build-machine paths that don't exist on the
        # user's machine, producing "could not load the Qt platform plugin".
        install(FILES "$<TARGET_FILE_DIR:FinceptTerminal>/qt.conf"
                DESTINATION "." COMPONENT core OPTIONAL)
        # Qt plugin directories. OPTIONAL keeps the install non-fatal when
        # a plugin dir isn't produced by windeployqt on a given host (e.g.
        # texttospeech only appears when Qt TTS is present).
        foreach(_plugin_dir
                platforms sqldrivers imageformats iconengines styles tls
                multimedia texttospeech networkinformation platforminputcontexts
                printsupport generic)
            install(DIRECTORY "$<TARGET_FILE_DIR:FinceptTerminal>/${_plugin_dir}"
                    DESTINATION "." COMPONENT core OPTIONAL)
        endforeach()
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

    # ── Generators ────────────────────────────────────────────────────────────
    # Default: IFW everywhere. On Linux, additionally produce native package
    # formats if the corresponding tools are present on PATH.
    set(CPACK_GENERATOR "IFW")
    if(UNIX AND NOT APPLE)
        find_program(_FINCEPT_DPKG_EXE dpkg)
        find_program(_FINCEPT_RPMBUILD_EXE rpmbuild)
        if(_FINCEPT_DPKG_EXE)
            list(APPEND CPACK_GENERATOR "DEB")
            set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Fincept Corporation <support@fincept.in>")
            set(CPACK_DEBIAN_PACKAGE_SECTION "misc")
            set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
            set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "${CPACK_PACKAGE_HOMEPAGE_URL}")
            set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
            # Hard runtime deps: glibc 2.31+ (Ubuntu 22.04 baseline), libsecret
            # for credential storage, fontconfig for Qt text rendering.
            set(CPACK_DEBIAN_PACKAGE_DEPENDS
                "libc6 (>= 2.31), libsecret-1-0, libfontconfig1, libxkbcommon0, libdbus-1-3")
            # CPack DEB cannot generate arm64 from inside an x86_64 build host;
            # CPACK_DEBIAN_ARCHITECTURE follows CMAKE_SYSTEM_PROCESSOR.
            if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
                set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "arm64")
            else()
                set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
            endif()
            message(STATUS "CPack: DEB generator enabled (${CPACK_DEBIAN_PACKAGE_ARCHITECTURE})")
        endif()
        if(_FINCEPT_RPMBUILD_EXE)
            list(APPEND CPACK_GENERATOR "RPM")
            set(CPACK_RPM_PACKAGE_LICENSE "AGPL-3.0-or-later")
            set(CPACK_RPM_PACKAGE_GROUP "Applications/Productivity")
            set(CPACK_RPM_PACKAGE_URL "${CPACK_PACKAGE_HOMEPAGE_URL}")
            set(CPACK_RPM_PACKAGE_REQUIRES
                "glibc >= 2.31, libsecret, fontconfig, libxkbcommon, dbus-libs")
            set(CPACK_RPM_PACKAGE_AUTOREQPROV ON)
            if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
                set(CPACK_RPM_PACKAGE_ARCHITECTURE "aarch64")
            else()
                set(CPACK_RPM_PACKAGE_ARCHITECTURE "x86_64")
            endif()
            message(STATUS "CPack: RPM generator enabled (${CPACK_RPM_PACKAGE_ARCHITECTURE})")
        endif()
        if(NOT _FINCEPT_DPKG_EXE AND NOT _FINCEPT_RPMBUILD_EXE)
            message(STATUS "CPack: only IFW generator enabled — install dpkg-dev / rpm-build "
                           "for native .deb/.rpm output")
        endif()
    endif()

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
    # Explicitly enable target-dir removal on uninstall. Defaults to ON in
    # CPackIFW, but #240 surfaced a case where the install root survived a
    # `purge` run — set explicit so the generated config.xml always carries
    # <RemoveTargetDir>true</RemoveTargetDir> regardless of CPackIFW version.
    set(CPACK_IFW_PACKAGE_REMOVE_TARGET_DIR ON)
    # Controller script for headless invocations (e.g. `purge --confirm-command
    # --accept-messages --default-answer`). Without it, IFW blocks on wizard
    # pages even with --confirm-command set, because page auto-advance is only
    # wired for the install path.
    set(CPACK_IFW_PACKAGE_CONTROL_SCRIPT
        "${CMAKE_CURRENT_SOURCE_DIR}/packaging/installer/config/controlscript.qs")

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
        DESCRIPTION  "1300+ Python analytics scripts (equity, derivatives, portfolio, quant)"
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

    # ── Linux: AppImage helper target ────────────────────────────────────────
    # `linuxdeploy` + `linuxdeploy-plugin-qt` are external tools; we don't
    # install them ourselves. If found on PATH, expose `make appimage` (or
    # `cmake --build . --target appimage`) so CI can produce a single-file
    # portable AppImage. Distro-agnostic; runs on glibc 2.31+ targets.
    if(UNIX AND NOT APPLE)
        find_program(_FINCEPT_LINUXDEPLOY linuxdeploy)
        find_program(_FINCEPT_LINUXDEPLOY_QT linuxdeploy-plugin-qt)
        if(_FINCEPT_LINUXDEPLOY AND _FINCEPT_LINUXDEPLOY_QT)
            add_custom_target(appimage
                COMMAND ${CMAKE_COMMAND} --install "${CMAKE_BINARY_DIR}"
                    --prefix "${CMAKE_BINARY_DIR}/AppDir/usr"
                COMMAND ${CMAKE_COMMAND} -E env
                    QMAKE=${_fincept_qt_prefix}/bin/qmake
                    "${_FINCEPT_LINUXDEPLOY}"
                    --appdir "${CMAKE_BINARY_DIR}/AppDir"
                    --plugin qt
                    --output appimage
                    --desktop-file "${CMAKE_BINARY_DIR}/AppDir/usr/share/applications/fincept-terminal.desktop"
                WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
                COMMENT "Building FinceptTerminal AppImage..."
                VERBATIM)
            message(STATUS "AppImage target available — run `cmake --build . --target appimage`")
        else()
            message(STATUS "linuxdeploy / linuxdeploy-plugin-qt not on PATH — "
                           "AppImage target unavailable. See https://github.com/linuxdeploy")
        endif()
    endif()

endif() # FINCEPT_BUILD_INSTALLER

# ── Code signing helpers ─────────────────────────────────────────────────────
# These are opt-in custom targets, not POST_BUILD steps, so signing only runs
# when explicitly invoked (CI / release pipeline). All inputs live in cache
# variables; nothing is hardcoded.

# macOS: codesign + notarytool + stapler
if(APPLE)
    set(FINCEPT_MACOS_SIGN_IDENTITY "" CACHE STRING
        "Codesign identity (e.g. 'Developer ID Application: Fincept Corp (TEAMID)')")
    set(FINCEPT_MACOS_NOTARY_APPLE_ID "" CACHE STRING "Apple ID for notarytool submission")
    set(FINCEPT_MACOS_NOTARY_TEAM_ID  "" CACHE STRING "Apple Developer Team ID")
    set(FINCEPT_MACOS_NOTARY_PASSWORD "" CACHE STRING "App-specific password for notarytool")

    if(FINCEPT_MACOS_SIGN_IDENTITY)
        find_program(_FINCEPT_MACDEPLOYQT macdeployqt HINTS "${_fincept_qt_prefix}/bin")
        find_program(_FINCEPT_NOTARYTOOL  notarytool)
        find_program(_FINCEPT_STAPLER     stapler)
        find_program(_FINCEPT_DITTO       ditto)

        # Sign-only target: useful for dev signing (not notarized).
        add_custom_target(sign-macos
            COMMAND "${_FINCEPT_MACDEPLOYQT}"
                "$<TARGET_BUNDLE_DIR:FinceptTerminal>"
                "-sign-for-notarization=${FINCEPT_MACOS_SIGN_IDENTITY}"
            COMMENT "Signing FinceptTerminal.app with ${FINCEPT_MACOS_SIGN_IDENTITY}..."
            VERBATIM)

        # Full release pipeline: sign → zip → submit → wait → staple.
        if(FINCEPT_MACOS_NOTARY_APPLE_ID
           AND FINCEPT_MACOS_NOTARY_TEAM_ID
           AND FINCEPT_MACOS_NOTARY_PASSWORD)
            add_custom_target(notarize-macos
                COMMAND "${_FINCEPT_DITTO}" -c -k --keepParent
                    "$<TARGET_BUNDLE_DIR:FinceptTerminal>"
                    "${CMAKE_BINARY_DIR}/FinceptTerminal-notarize.zip"
                COMMAND xcrun notarytool submit
                    "${CMAKE_BINARY_DIR}/FinceptTerminal-notarize.zip"
                    --apple-id "${FINCEPT_MACOS_NOTARY_APPLE_ID}"
                    --team-id  "${FINCEPT_MACOS_NOTARY_TEAM_ID}"
                    --password "${FINCEPT_MACOS_NOTARY_PASSWORD}"
                    --wait
                COMMAND xcrun stapler staple "$<TARGET_BUNDLE_DIR:FinceptTerminal>"
                DEPENDS sign-macos
                COMMENT "Notarizing + stapling FinceptTerminal.app..."
                VERBATIM)
        endif()
    endif()
endif()

# Windows: Authenticode signing via signtool
if(WIN32)
    set(FINCEPT_WIN_SIGN_CERT     "" CACHE FILEPATH "Path to .pfx code-signing certificate")
    set(FINCEPT_WIN_SIGN_PASSWORD "" CACHE STRING   "Password for the .pfx certificate")
    set(FINCEPT_WIN_SIGN_TIMESTAMP_URL
        "http://timestamp.digicert.com" CACHE STRING "RFC 3161 timestamp URL")

    if(FINCEPT_WIN_SIGN_CERT)
        find_program(_FINCEPT_SIGNTOOL signtool
            HINTS ${_fincept_sdk_dirs}
                  "$ENV{WindowsSdkVerBinPath}x64")
        if(_FINCEPT_SIGNTOOL)
            add_custom_target(sign-windows
                COMMAND "${_FINCEPT_SIGNTOOL}" sign
                    /fd SHA256
                    /tr "${FINCEPT_WIN_SIGN_TIMESTAMP_URL}"
                    /td SHA256
                    /f  "${FINCEPT_WIN_SIGN_CERT}"
                    /p  "${FINCEPT_WIN_SIGN_PASSWORD}"
                    "$<TARGET_FILE:FinceptTerminal>"
                COMMENT "Signing FinceptTerminal.exe with Authenticode..."
                VERBATIM)
        else()
            message(WARNING "FINCEPT_WIN_SIGN_CERT set but signtool.exe not found")
        endif()
    endif()
endif()

# ── Build configuration summary ──────────────────────────────────────────────
message(STATUS "")
message(STATUS "============================================================")
message(STATUS "  Fincept Terminal ${CMAKE_PROJECT_VERSION} — build summary")
message(STATUS "============================================================")
message(STATUS "  Qt version        : ${Qt6Core_VERSION}  (pin mode: ${FINCEPT_QT_PIN_MODE})")
message(STATUS "  Build type        : ${CMAKE_BUILD_TYPE}")
message(STATUS "  Platform          : ${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "  Compiler          : ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
if(APPLE AND CMAKE_OSX_ARCHITECTURES)
message(STATUS "  macOS arch        : ${CMAKE_OSX_ARCHITECTURES}")
endif()
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
if(DEFINED HAS_WAYLAND)
message(STATUS "  Wayland (Linux)   : ${HAS_WAYLAND}")
endif()
if(APPLE AND FINCEPT_MACOS_SIGN_IDENTITY)
message(STATUS "  macOS signing     : ON   (target: sign-macos / notarize-macos)")
endif()
if(WIN32 AND FINCEPT_WIN_SIGN_CERT)
message(STATUS "  Windows signing   : ON   (target: sign-windows)")
endif()
message(STATUS "  Tests             : ${FINCEPT_BUILD_TESTS}")
message(STATUS "============================================================")
message(STATUS "")

# ── Test suite (opt-in; see fincept-qt/tests/CMakeLists.txt) ─────────────────
if(FINCEPT_BUILD_TESTS)
    add_subdirectory(tests)
endif()
