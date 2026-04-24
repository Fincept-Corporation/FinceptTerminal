#include "core/crash/CrashHandler.h"

#ifdef _WIN32
#    include "core/config/AppPaths.h"

#    include <QDateTime>
#    include <QDir>
#    include <QString>

#    include <windows.h>
// dbghelp.h MUST come after windows.h
#    include <dbghelp.h>

#    pragma comment(lib, "dbghelp.lib")

// These NTSTATUS values are the exception codes we care about for crash
// capture. Windows SDK only exposes them via <ntstatus.h>, which conflicts
// with <winnt.h> in the same TU. Define them locally.
#    ifndef STATUS_STACK_BUFFER_OVERRUN
#        define STATUS_STACK_BUFFER_OVERRUN ((DWORD)0xC0000409L)
#    endif
#    ifndef STATUS_HEAP_CORRUPTION
#        define STATUS_HEAP_CORRUPTION ((DWORD)0xC0000374L)
#    endif

// ─────────────────────────────────────────────────────────────────────────────
// Minidump writer for Windows.
//
// When FinceptTerminal.exe crashes (access violation, stack overflow,
// FAST_FAIL_STACK_COOKIE_CHECK_FAILURE, etc.) the default behaviour is that
// Windows Error Reporting writes a dump to AppData\CrashDumps — BUT only if
// the user has WER enabled, and WER strips user data and is hard to retrieve.
//
// This filter writes our own minidump the moment the unhandled exception
// fires, to a directory we control, with a deterministic filename. We can
// then ship it from support tickets or bug reports and open it in WinDbg /
// Visual Studio.
//
// Safety rules inside the filter (heap may be corrupt):
//   - All strings are resolved at install() time and stored in static wide
//     buffers, so no allocation happens during the crash.
//   - dbghelp.dll is implicitly linked via pragma, so no LoadLibrary inside
//     the filter.
//   - We fall through to EXCEPTION_CONTINUE_SEARCH so WER still gets its
//     chance (so the existing AppCrash report the user sees still fires).
// ─────────────────────────────────────────────────────────────────────────────

namespace {

// Static pre-allocated buffer for the dump directory, resolved once at
// install() time. MAX_PATH is deliberately small — anything longer than
// MAX_PATH on Windows needs \\?\ prefixes that MiniDumpWriteDump won't
// accept, so we keep the dump under the LOCALAPPDATA profile root where
// paths are always well under the limit.
constexpr size_t kDirBufferLen = MAX_PATH;
wchar_t g_dump_dir[kDirBufferLen] = {0};
bool    g_installed = false;

LONG WINAPI unhandled_exception_filter(EXCEPTION_POINTERS* ep) {
    if (!g_installed || !ep || g_dump_dir[0] == L'\0') {
        return EXCEPTION_CONTINUE_SEARCH;
    }

    // Build timestamped filename: crash_YYYYMMDD_HHMMSS_<pid>.dmp
    SYSTEMTIME st;
    GetLocalTime(&st);

    wchar_t path[kDirBufferLen + 64] = {0};
    // swprintf_s is safe inside the filter — it writes to a stack buffer and
    // does not allocate. The %04d/%02d specifiers are size-bounded so the
    // final path cannot overflow the buffer.
    swprintf_s(path, L"%s\\crash_%04d%02d%02d_%02d%02d%02d_%lu.dmp",
               g_dump_dir,
               st.wYear, st.wMonth, st.wDay,
               st.wHour, st.wMinute, st.wSecond,
               GetCurrentProcessId());

    HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, nullptr,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file != INVALID_HANDLE_VALUE) {
        MINIDUMP_EXCEPTION_INFORMATION mei = {};
        mei.ThreadId          = GetCurrentThreadId();
        mei.ExceptionPointers = ep;
        mei.ClientPointers    = FALSE;

        // MiniDumpWithDataSegs + MiniDumpWithThreadInfo give us enough to
        // reconstruct a usable stack + local variables in WinDbg without
        // producing a multi-GB dump. MiniDumpWithIndirectlyReferencedMemory
        // pulls in pointers-to-pointers so struct fields show up.
        const MINIDUMP_TYPE dump_type = static_cast<MINIDUMP_TYPE>(
            MiniDumpWithDataSegs |
            MiniDumpWithThreadInfo |
            MiniDumpWithIndirectlyReferencedMemory |
            MiniDumpWithUnloadedModules);

        MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(),
                          file, dump_type, &mei, nullptr, nullptr);
        CloseHandle(file);
    }

    // Let WER + debugger attach still see the exception. Returning
    // EXCEPTION_EXECUTE_HANDLER would swallow it and skip WER, which means
    // the user loses the crash dialog. We want both our dump AND WER.
    return EXCEPTION_CONTINUE_SEARCH;
}

} // namespace
#endif // _WIN32

namespace fincept::crash {

void install() {
#ifdef _WIN32
    // Resolve and create the dump directory once. AppPaths uses QString so we
    // snapshot it into a static wchar_t buffer for the filter to use without
    // touching the heap during a crash.
    const QString dir = fincept::AppPaths::crashdumps();
    QDir().mkpath(dir);

    // Normalise to native Windows separators — MiniDumpWriteDump's CreateFileW
    // call can handle '/' but WinDbg is happier with backslashes in reports.
    const QString native = QDir::toNativeSeparators(dir);
    const std::wstring wide = native.toStdWString();

    if (wide.size() + 1 >= kDirBufferLen) {
        // Path too long for our static buffer — skip install rather than
        // risk a truncated path that writes to the wrong location.
        return;
    }
    wcsncpy_s(g_dump_dir, wide.c_str(), wide.size());

    // Register the filter. A previously-installed filter (Qt's WER stub, say)
    // is chained via EXCEPTION_CONTINUE_SEARCH so we don't clobber it.
    SetUnhandledExceptionFilter(unhandled_exception_filter);

    // Also install a vectored handler as a second line of defence. The MSVC
    // runtime can silently call SetUnhandledExceptionFilter(nullptr) from
    // inside __CxxFrameHandler when a C++ exception propagates unwind, which
    // would disable our top-level filter. Vectored handlers run before
    // SEH's __except blocks and cannot be overridden by the CRT. We return
    // EXCEPTION_CONTINUE_SEARCH so normal SEH unwinding still runs — we are
    // only here to capture the dump on the way past.
    AddVectoredExceptionHandler(/*FirstHandler=*/0,
        [](EXCEPTION_POINTERS* ep) -> LONG {
            // Only capture the "this is about to kill us" codes. Filtering
            // keeps benign SEH (e.g. C++ exceptions, which are SEH internally
            // with code 0xE06D7363) from triggering a dump on every throw.
            if (!ep || !ep->ExceptionRecord) return EXCEPTION_CONTINUE_SEARCH;
            const DWORD code = ep->ExceptionRecord->ExceptionCode;
            switch (code) {
                case EXCEPTION_ACCESS_VIOLATION:
                case EXCEPTION_STACK_OVERFLOW:
                case EXCEPTION_ILLEGAL_INSTRUCTION:
                case EXCEPTION_INT_DIVIDE_BY_ZERO:
                case EXCEPTION_PRIV_INSTRUCTION:
                case STATUS_STACK_BUFFER_OVERRUN: // 0xC0000409 — GS cookie, FAST_FAIL
                case STATUS_HEAP_CORRUPTION:      // 0xC0000374
                    unhandled_exception_filter(ep);
                    break;
                default:
                    break;
            }
            return EXCEPTION_CONTINUE_SEARCH;
        });

    // Make sure MSVC's runtime doesn't silently replace our filter when the
    // CRT internally catches an exception. The call below is a no-op on most
    // modern MSVC runtimes but is harmless and documented to help.
    // https://learn.microsoft.com/cpp/c-runtime-library/reference/set-invalid-parameter-handler
    _set_invalid_parameter_handler([](const wchar_t*, const wchar_t*, const wchar_t*,
                                      unsigned int, uintptr_t) {
        // Trigger our filter by raising a non-continuable exception with the
        // current context, rather than letting the CRT call abort() which
        // exits without running our handler.
        RaiseException(0xE0000001, EXCEPTION_NONCONTINUABLE, 0, nullptr);
    });

    g_installed = true;
#endif
}

} // namespace fincept::crash
