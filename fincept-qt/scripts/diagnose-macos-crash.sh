#!/usr/bin/env bash
# diagnose-macos-crash.sh — launch FinceptTerminal under macOS' malloc
# debugging instrumentation so a use-after-free / wild-pointer crash
# fails LOUDLY at the use site (not 2+ hours later when the freed
# region happens to get reused).
#
# Usage:
#   scripts/diagnose-macos-crash.sh                       # signed bundle from build/macos-release
#   scripts/diagnose-macos-crash.sh path/to/Fincept.app   # any bundle
#   scripts/diagnose-macos-crash.sh --asan                # build/macos-asan binary
#
# When the app crashes:
#   1. Note the crash address (often shown as `far=0x…` or in x-registers).
#   2. If the address is `0x55…55` — that's MallocScribble's signature.
#      The freed region was written before reuse → confirmed UAF.
#   3. Run:
#        malloc_history $(pgrep FinceptTerminal) -allEvents <addr>
#      (or pass a memgraph file if the app already exited).
#   4. The output shows the EXACT alloc backtrace + free backtrace
#      for that address. Root cause becomes a single grep.
#
# References:
#   - https://developer.apple.com/library/archive/documentation/Performance/Conceptual/ManagingMemory/Articles/MallocDebug.html
#   - https://keith.github.io/xcode-man-pages/malloc_history.1.html
#   - https://forum.qt.io/topic/135220/random-application-crash-on-apple-silicon-m1-qt-6-2-3/42

set -euo pipefail

# ── Locate the .app bundle ────────────────────────────────────────────────────
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

MODE="signed"
TARGET=""
while [[ $# -gt 0 ]]; do
    case "$1" in
        --asan)
            MODE="asan"
            shift
            ;;
        --help|-h)
            sed -n '2,/^set -e/p' "$0" | sed 's/^# \{0,1\}//'
            exit 0
            ;;
        *)
            TARGET="$1"
            shift
            ;;
    esac
done

if [[ -z "${TARGET}" ]]; then
    if [[ "${MODE}" == "asan" ]]; then
        TARGET="${REPO_DIR}/build/macos-asan/FinceptTerminal.app"
    else
        TARGET="${REPO_DIR}/build/macos-release/FinceptTerminal.app"
    fi
fi

if [[ ! -d "${TARGET}" ]]; then
    echo "error: bundle not found at ${TARGET}" >&2
    echo "       build it first:" >&2
    if [[ "${MODE}" == "asan" ]]; then
        echo "         cmake --preset macos-asan && cmake --build build/macos-asan" >&2
    else
        echo "         cmake --preset macos-release && cmake --build build/macos-release" >&2
    fi
    exit 1
fi

BIN="${TARGET}/Contents/MacOS/FinceptTerminal"
if [[ ! -x "${BIN}" ]]; then
    echo "error: executable missing or not executable: ${BIN}" >&2
    exit 1
fi

# ── Malloc instrumentation (works on signed bundles too) ──────────────────────
# MallocStackLoggingNoCompact: record every alloc + free with full backtrace.
# MallocScribble:              fill freed memory with 0x55 → crash at use site.
# MallocGuardEdges:            place guard pages around allocations.
# MallocCheckHeapEach:         periodic heap consistency check (slows runtime).
#
# These work on shipped, signed, notarised apps — no rebuild required.
# Performance cost: app runs ~2-5× slower; memory usage ~2-3× higher.
# That's fine for a diagnostic session; it's not a release config.
export MallocStackLoggingNoCompact=1
export MallocScribble=1
export MallocGuardEdges=1
export MallocErrorAbort=1
export MallocCheckHeapEach=10000

# ── Qt logging — surface socket / network / TLS lifecycle ─────────────────────
# These categories print to stderr; redirect to a file via the wrapper below.
# Disabled by default in Release builds; explicit rules re-enable them.
export QT_LOGGING_RULES="qt.network.*=true;qt.qpa.events=false;qt.qpa.input.events=false"
export QT_FATAL_WARNINGS=0    # set to 1 to abort on first Qt warning (sometimes useful)

# ── ASan options if running an ASan build ─────────────────────────────────────
if [[ "${MODE}" == "asan" ]]; then
    export ASAN_OPTIONS="detect_leaks=0:halt_on_error=1:abort_on_error=1:print_stacktrace=1:strict_string_checks=1:detect_stack_use_after_return=1:check_initialization_order=1:strict_init_order=1"
    export UBSAN_OPTIONS="halt_on_error=1:abort_on_error=1:print_stacktrace=1"
fi

# ── Launch ────────────────────────────────────────────────────────────────────
LOG_DIR="${REPO_DIR}/diagnostic-logs"
mkdir -p "${LOG_DIR}"
STAMP="$(date +%Y%m%d-%H%M%S)"
LOG_FILE="${LOG_DIR}/fincept-${MODE}-${STAMP}.log"

echo "──────────────────────────────────────────────────────────────────────"
echo "  Diagnostic launch:  ${MODE}"
echo "  Binary:             ${BIN}"
echo "  Log file:           ${LOG_FILE}"
echo ""
echo "  When it crashes:"
echo "    1. Note the crash address (look for far= or x19/x20 in crash log)"
echo "    2. Run: malloc_history \$(pgrep FinceptTerminal) -allEvents <addr>"
echo "       (or post-mortem on the ips file under ~/Library/Logs/DiagnosticReports/)"
echo "    3. The alloc + free stacks pinpoint the leaking owner."
echo "──────────────────────────────────────────────────────────────────────"

# Use `tee` so we can both watch the run live AND keep a permanent log.
# `script` would capture pty colors but we want plain text for grep.
"${BIN}" 2>&1 | tee "${LOG_FILE}"
EXIT_CODE=${PIPESTATUS[0]}

if [[ "${EXIT_CODE}" -ne 0 ]]; then
    echo ""
    echo "── Process exited with code ${EXIT_CODE} ──────────────────────────────"
    echo "Log saved to: ${LOG_FILE}"
    # If the crash registered with ReportCrash, point at it.
    LATEST_IPS="$(ls -t ~/Library/Logs/DiagnosticReports/FinceptTerminal-*.ips 2>/dev/null | head -1 || true)"
    if [[ -n "${LATEST_IPS}" ]]; then
        echo "Latest crash report: ${LATEST_IPS}"
        echo ""
        echo "Faulting addresses (from crash report):"
        # 'far' is the Translation-fault target address on ARM64; x19/x20 are
        # commonly the stale-pointer holders in doActivate-style crashes.
        grep -E 'far:|x19:|x20:' "${LATEST_IPS}" | head -5 || true
    fi
fi

exit "${EXIT_CODE}"
