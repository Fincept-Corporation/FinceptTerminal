# Incrementally sync the Python scripts/ tree into the build output, then prune
# junk — but skip the whole job when nothing shippable under scripts/ has changed
# since the last sync. Runs under `cmake -P`, portable across Windows/macOS/Linux.
#
# Why this exists:
#   The old POST_BUILD did an unconditional `copy_directory` of the whole tree
#   plus a prune on EVERY link, so a one-line C++ change paid a multi-minute
#   tax. copy_directory re-copies the whole tree every time. This guard makes
#   the steady state (no script edits) cost a filtered directory walk instead.
#
# Why the walk is FILTERED (the second, bigger tax):
#   scripts/ holds ~1.4k tracked files, but a working tree also accumulates
#   scripts/.venv (a full Python virtualenv), __pycache__ dirs, .firecrawl
#   caches and *.deleted backups — measured at 117,473 files on a dev box
#   against 1,447 tracked. Walking and timestamping all of them from CMake
#   script, then copy_directory_if_different'ing them into the build output,
#   turned a ~5 s incremental build into a 25-minute one and shipped a venv
#   into build/. Both the freshness scan and the copy now prune those
#   directories, matching the exclude list in .github/scripts/sync_scripts.sh
#   so local builds and CI packaging agree on what "the scripts tree" is.
#
# Required defines (passed via -D):
#   SRC_DIR    — source scripts/ directory
#   DST_DIR    — destination scripts/ directory (next to the exe)
#   STAMP_FILE — path to the sync stamp file
#   PRUNE_SCRIPT — path to prune_scripts_junk.cmake

# `cmake -P` starts with every policy OLD, which turns the IN_LIST operator
# below back into a plain string comparison (CMP0057).
cmake_minimum_required(VERSION 3.20)

foreach(_var SRC_DIR DST_DIR STAMP_FILE PRUNE_SCRIPT)
    if(NOT DEFINED ${_var})
        message(FATAL_ERROR "sync_scripts: ${_var} must be set")
    endif()
endforeach()

if(NOT IS_DIRECTORY "${SRC_DIR}")
    message(STATUS "sync_scripts: source '${SRC_DIR}' missing — nothing to do")
    return()
endif()

# Directory names never worth walking, copying, or shipping. Keep in sync with
# .github/scripts/sync_scripts.sh.
set(_skip_dirs
    "__pycache__" ".venv" "venv" "env" ".pytest_cache" ".benchmarks"
    ".firecrawl" ".git" ".mypy_cache" ".ruff_cache" "node_modules")
# File suffixes that are build artefacts or local state, not source.
set(_skip_file_regex "\\.(pyc|pyo|bak|log|db|sqlite|sqlite3)$")
# Anything a contributor parked as a backup (agents.deleted.2026-05-01/, x.deleted).
set(_skip_name_regex "\\.deleted")

# Recursive walk that PRUNES excluded directories instead of listing their
# contents — file(GLOB_RECURSE) has no way to skip a subtree, so it would still
# enumerate every file inside .venv before we could filter them out.
function(fincept_collect_scripts dir out_var)
    set(_acc "")
    file(GLOB _entries LIST_DIRECTORIES true "${dir}/*")
    foreach(_e IN LISTS _entries)
        get_filename_component(_name "${_e}" NAME)
        if(_name MATCHES "${_skip_name_regex}")
            continue()
        endif()
        if(IS_DIRECTORY "${_e}")
            if(_name IN_LIST _skip_dirs)
                continue()
            endif()
            fincept_collect_scripts("${_e}" _sub)
            list(APPEND _acc ${_sub})
        else()
            if(_name MATCHES "${_skip_file_regex}")
                continue()
            endif()
            list(APPEND _acc "${_e}")
        endif()
    endforeach()
    set(${out_var} "${_acc}" PARENT_SCOPE)
endfunction()

fincept_collect_scripts("${SRC_DIR}" _src_files)

# Surface a polluted source tree. The sync itself no longer cares (the walk
# above prunes these), but a virtualenv parked under scripts/ still slows down
# greps, IDE indexing and every other tool that walks the repo — and it is how
# this tree grew to 117,473 files against 1,447 tracked ones. One line, only
# when there is something to report.
set(_polluted "")
foreach(_d IN LISTS _skip_dirs)
    if(IS_DIRECTORY "${SRC_DIR}/${_d}")
        list(APPEND _polluted "${_d}")
    endif()
endforeach()
if(_polluted)
    string(REPLACE ";" ", " _polluted_str "${_polluted}")
    message(STATUS "sync_scripts: NOT shipping local junk under scripts/: ${_polluted_str} "
                   "(safe to delete — the app's venvs live under AppPaths::root())")
endif()

# Cheap change signature: newest mtime + file count over the filtered set. The
# count catches deletions, which a max-mtime alone would miss.
set(_newest 0)
foreach(_f IN LISTS _src_files)
    file(TIMESTAMP "${_f}" _ts "%s" UTC)
    if(_ts GREATER _newest)
        set(_newest "${_ts}")
    endif()
endforeach()
list(LENGTH _src_files _count)
set(_signature "${_newest}:${_count}")

set(_prev "")
if(EXISTS "${STAMP_FILE}")
    file(READ "${STAMP_FILE}" _prev)
    string(STRIP "${_prev}" _prev)
endif()

if(_prev STREQUAL "${_signature}" AND IS_DIRECTORY "${DST_DIR}")
    # Unchanged since last sync — skip the expensive copy + prune entirely.
    return()
endif()

message(STATUS "sync_scripts: syncing ${_count} script file(s) to build output")

# Copy exactly the files the filtered walk found, preserving their relative
# layout. Driving the copy from the collected list (rather than file(COPY) with
# PATTERN ... EXCLUDE over the whole tree) keeps one definition of "shippable"
# for both the freshness check and the copy — and never re-walks the pruned
# subtrees. file(COPY) preserves timestamps and skips a file already present
# with the same one, so a re-sync after a single edit rewrites a single file.
foreach(_f IN LISTS _src_files)
    file(RELATIVE_PATH _rel "${SRC_DIR}" "${_f}")
    get_filename_component(_rel_dir "${_rel}" DIRECTORY)
    if(_rel_dir STREQUAL "")
        file(COPY "${_f}" DESTINATION "${DST_DIR}")
    else()
        file(COPY "${_f}" DESTINATION "${DST_DIR}/${_rel_dir}")
    endif()
endforeach()

# Prune junk that must never ship (see prune_scripts_junk.cmake for rationale).
# The exclude patterns above keep it from ever reaching DST_DIR, but a stale
# build output from before this filter existed still needs cleaning.
execute_process(
    COMMAND "${CMAKE_COMMAND}" -DSCRIPTS_DIR=${DST_DIR} -P "${PRUNE_SCRIPT}"
    RESULT_VARIABLE _prune_rc
)
if(NOT _prune_rc EQUAL 0)
    message(FATAL_ERROR "sync_scripts: prune step failed (${_prune_rc})")
endif()

file(WRITE "${STAMP_FILE}" "${_signature}")
