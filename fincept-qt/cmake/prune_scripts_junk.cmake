# Prune files/directories that accidentally live under scripts/ but must
# never ship inside the built bundle. Runs under `cmake -P` — portable
# across Windows / macOS / Linux.
#
# Rationale:
#   - *.deleted.* / *.bak / *.orig  — leftover backups from refactors
#   - .pytest_cache / .benchmarks   — pytest caches committed by mistake
#   - __pycache__                   — Python bytecode caches
#   - *.db / *.sqlite*              — local SQLite DBs (e.g. memories_alice.db)
#
# On macOS anything unexpected inside an .app bundle trips codesign with
# "bundle format unrecognized, invalid, or unsuitable". On Linux/Windows
# these just bloat the installer; removing them shrinks it meaningfully.


# `cmake -P` starts with every policy OLD, which turns the IN_LIST operator
# below back into a plain string comparison (CMP0057).
cmake_minimum_required(VERSION 3.20)
if(NOT DEFINED SCRIPTS_DIR)
    message(FATAL_ERROR "prune_scripts_junk.cmake: SCRIPTS_DIR must be set")
endif()

if(NOT IS_DIRECTORY "${SCRIPTS_DIR}")
    # Nothing to do — scripts dir isn't there.
    return()
endif()

# Collect directories to nuke by walking and matching NAMES.
#
# This deliberately does NOT use file(GLOB_RECURSE ... LIST_DIRECTORIES true):
# that flag makes CMake add every directory it traverses to the result, not
# only the ones matching the pattern, so the "junk" list came back containing
# every subdirectory of scripts/ — the prune then deleted Analytics/, agents/,
# strategies/ and friends out of the build output on every single sync
# (measured: 1,100 of 1,449 synced files removed).
set(_junk_dir_names "__pycache__" ".pytest_cache" ".benchmarks")
set(_junk_dir_regex "\\.deleted") # agents.deleted.2026-05-01/, foo.deleted/

function(fincept_collect_junk_dirs dir out_var)
    set(_acc "")
    file(GLOB _entries LIST_DIRECTORIES true "${dir}/*")
    foreach(_e IN LISTS _entries)
        if(NOT IS_DIRECTORY "${_e}")
            continue()
        endif()
        get_filename_component(_name "${_e}" NAME)
        if(_name IN_LIST _junk_dir_names OR _name MATCHES "${_junk_dir_regex}")
            list(APPEND _acc "${_e}")
            continue() # whole subtree goes — no need to descend
        endif()
        fincept_collect_junk_dirs("${_e}" _sub)
        list(APPEND _acc ${_sub})
    endforeach()
    set(${out_var} "${_acc}" PARENT_SCOPE)
endfunction()

fincept_collect_junk_dirs("${SCRIPTS_DIR}" _junk_dirs)

# Glob stray files (local DBs + editor/backup artifacts).
file(GLOB_RECURSE _junk_files
    "${SCRIPTS_DIR}/*.db"
    "${SCRIPTS_DIR}/*.sqlite"
    "${SCRIPTS_DIR}/*.sqlite3"
    "${SCRIPTS_DIR}/*.bak"
    "${SCRIPTS_DIR}/*.orig"
    "${SCRIPTS_DIR}/*.pyc"
)

set(_removed 0)
foreach(_path IN LISTS _junk_dirs _junk_files)
    if(IS_DIRECTORY "${_path}")
        file(REMOVE_RECURSE "${_path}")
        math(EXPR _removed "${_removed} + 1")
    elseif(EXISTS "${_path}")
        file(REMOVE "${_path}")
        math(EXPR _removed "${_removed} + 1")
    endif()
endforeach()

if(_removed GREATER 0)
    message(STATUS "prune_scripts_junk: removed ${_removed} junk path(s) under ${SCRIPTS_DIR}")
endif()
