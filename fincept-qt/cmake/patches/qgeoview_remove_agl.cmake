# qgeoview_remove_agl.cmake
# Portable patch script — removes -framework AGL from QGeoView's CMakeLists.
#
# Background: AGL (Apple OpenGL) framework was removed in macOS 14 (Sonoma).
# QGeoView links it unconditionally in lib/CMakeLists.txt which causes a
# linker error on macOS 14+:  ld: framework 'AGL' not found
#
# This script is invoked via PATCH_COMMAND in FetchContent_Declare:
#   PATCH_COMMAND ${CMAKE_COMMAND} -DFILE=<SOURCE_DIR>/lib/CMakeLists.txt
#                                  -P .../qgeoview_remove_agl.cmake
#
# It uses only CMake built-ins so it works identically on Windows, macOS, Linux.

if(NOT DEFINED FILE)
    message(FATAL_ERROR "qgeoview_remove_agl.cmake: FILE variable not set")
endif()

if(NOT EXISTS "${FILE}")
    message(STATUS "qgeoview_remove_agl: ${FILE} not found — skipping patch")
    return()
endif()

file(READ "${FILE}" _content)

# Check if AGL is present before patching (idempotent — safe to run twice)
string(FIND "${_content}" "AGL" _agl_pos)
if(_agl_pos EQUAL -1)
    message(STATUS "qgeoview_remove_agl: AGL not found in ${FILE} — already patched or not needed")
    return()
endif()

# Remove all occurrences of AGL framework reference forms:
#   -framework AGL       (common CMakeLists form)
#   AGL                  (bare token in a list)
string(REPLACE "-framework AGL" "" _content "${_content}")
string(REPLACE " AGL "          " " _content "${_content}")
string(REPLACE "\nAGL\n"        "\n" _content "${_content}")
string(REPLACE "(AGL)"          "()" _content "${_content}")

file(WRITE "${FILE}" "${_content}")
message(STATUS "qgeoview_remove_agl: patched ${FILE} — removed AGL framework reference")
