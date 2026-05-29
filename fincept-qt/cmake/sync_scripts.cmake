# Incrementally sync the Python scripts/ tree into the build output, then prune
# junk — but skip the whole job when nothing under scripts/ has changed since
# the last sync. Runs under `cmake -P`, portable across Windows/macOS/Linux.
#
# Why this exists:
#   The old POST_BUILD did an unconditional `copy_directory` of 4000+ files
#   plus a prune on EVERY link, so a one-line C++ change paid a multi-minute
#   tax. copy_directory re-copies the whole tree every time. This guard makes
#   the steady state (no script edits) cost a single directory stat instead.
#
# Required defines (passed via -D):
#   SRC_DIR    — source scripts/ directory
#   DST_DIR    — destination scripts/ directory (next to the exe)
#   STAMP_FILE — path to the sync stamp file
#   PRUNE_SCRIPT — path to prune_scripts_junk.cmake

foreach(_var SRC_DIR DST_DIR STAMP_FILE PRUNE_SCRIPT)
    if(NOT DEFINED ${_var})
        message(FATAL_ERROR "sync_scripts.cmake: ${_var} must be set")
    endif()
endforeach()

if(NOT IS_DIRECTORY "${SRC_DIR}")
    message(STATUS "sync_scripts: source '${SRC_DIR}' missing — nothing to do")
    return()
endif()

# Build a cheap change signature from the newest mtime across the source tree.
# GLOB_RECURSE here walks paths only (no content hashing), which is fast even
# for thousands of files. If the destination + stamp already reflect the
# newest source mtime, the tree is unchanged and we can bail immediately.
file(GLOB_RECURSE _src_files LIST_DIRECTORIES false "${SRC_DIR}/*")

set(_newest 0)
foreach(_f IN LISTS _src_files)
    file(TIMESTAMP "${_f}" _ts "%s" UTC)
    if(_ts GREATER _newest)
        set(_newest "${_ts}")
    endif()
endforeach()

set(_prev "")
if(EXISTS "${STAMP_FILE}")
    file(READ "${STAMP_FILE}" _prev)
    string(STRIP "${_prev}" _prev)
endif()

if(_prev STREQUAL "${_newest}" AND IS_DIRECTORY "${DST_DIR}")
    # Unchanged since last sync — skip the expensive copy + prune entirely.
    return()
endif()

message(STATUS "sync_scripts: changes detected — syncing scripts/ to build output")

# copy_directory_if_different (CMake 3.26+) only writes files whose content
# actually differs, so even when this branch runs it avoids needless writes.
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E copy_directory_if_different "${SRC_DIR}" "${DST_DIR}"
    RESULT_VARIABLE _copy_rc
)
if(NOT _copy_rc EQUAL 0)
    message(FATAL_ERROR "sync_scripts: copy_directory_if_different failed (${_copy_rc})")
endif()

# Prune junk that must never ship (see prune_scripts_junk.cmake for rationale).
execute_process(
    COMMAND "${CMAKE_COMMAND}" -DSCRIPTS_DIR=${DST_DIR} -P "${PRUNE_SCRIPT}"
    RESULT_VARIABLE _prune_rc
)
if(NOT _prune_rc EQUAL 0)
    message(FATAL_ERROR "sync_scripts: prune step failed (${_prune_rc})")
endif()

file(WRITE "${STAMP_FILE}" "${_newest}")
