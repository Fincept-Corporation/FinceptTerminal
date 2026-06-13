# deploy_qt_once.cmake — run windeployqt at most once per configure (dev builds).
#
# Invoked from CMakeLists.txt as a POST_BUILD step:
#   cmake -DWDQT=<windeployqt> -DEXE=<exe> -DSTAMP=<stampfile> -P deploy_qt_once.cmake
#
# windeployqt re-walks the ENTIRE Qt dependency tree on every run (every
# QtQuick/Controls QML file, every plugin, every WebEngine .pak) — ~30 s on this
# project even when it copies nothing. On a dev machine that tax lands on every
# incremental relink. To avoid it, the dev build deploys once and drops a stamp;
# the configure step deletes the stamp (see CMakeLists "Windows: deploy Qt DLLs"),
# so any CMake re-run — including the auto-reconfigure CMake does when you edit
# CMakeLists to add a Qt module — forces exactly one fresh redeploy. Plain .cpp
# edits skip it. Force a redeploy by deleting the stamp or reconfiguring.
if(EXISTS "${STAMP}")
    message(STATUS "windeployqt: Qt runtime already deployed since last configure — skipping (~30 s saved). Delete '${STAMP}' to force.")
    return()
endif()

message(STATUS "windeployqt: deploying Qt runtime (first build since configure)...")
execute_process(
    COMMAND "${WDQT}" "--no-translations" "--no-compiler-runtime" "${EXE}"
    RESULT_VARIABLE _wdqt_rc
)
if(NOT _wdqt_rc EQUAL 0)
    message(FATAL_ERROR "windeployqt failed with exit code ${_wdqt_rc}")
endif()
file(WRITE "${STAMP}" "deployed\n")
