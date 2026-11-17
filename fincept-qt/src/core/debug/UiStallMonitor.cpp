// src/core/debug/UiStallMonitor.cpp
#include "core/debug/UiStallMonitor.h"

#include "core/logging/Logger.h"
#include "core/privacy/LogRedact.h"
#include "core/telemetry/UsageReporter.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QElapsedTimer>
#include <QObject>
#include <QTimer>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>

#if defined(Q_OS_LINUX)
#include <dlfcn.h>
#include <execinfo.h>
#include <pthread.h>
#include <signal.h>
#elif defined(Q_OS_WIN) && defined(_M_X64)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#define FINCEPT_STALL_WIN_STACKS 1
#endif

namespace fincept::debug {

namespace {

constexpr int kBeatIntervalMs = 20;
constexpr int kPollIntervalMs = 50;
constexpr int kDefaultThresholdMs = 250;
constexpr int kMaxFrames = 128;

std::atomic<qint64> g_last_beat_ms{0}; // monotonic ms of the GUI thread's last timer tick
std::atomic<bool> g_running{false};
std::thread g_watcher;
std::mutex g_context_mutex;
std::string g_context; // guarded by g_context_mutex
std::string g_log_path;
#ifdef Q_OS_WIN
std::wstring g_log_path_w; // the same path for _wfopen_s: a narrow path is read in the ANSI code page
#endif
int g_threshold_ms = kDefaultThresholdMs;
int g_sample_ms = 0;   // FINCEPT_UI_STALL_SAMPLE_MS: while stalled, one compact stack every N ms
QElapsedTimer g_clock; // started once in start(); elapsed() is thread-safe to read

qint64 now_ms() {
    return g_clock.elapsed();
}

#if defined(Q_OS_LINUX)
bool g_capture_stacks = false;
pthread_t g_gui_thread;
void* g_frames[kMaxFrames];
std::atomic<int> g_frame_count{-1}; // -1 = no capture pending / not yet delivered

void stack_signal_handler(int) {
    // Runs on the GUI thread, interrupting whatever it is doing.
    g_frame_count.store(backtrace(g_frames, kMaxFrames));
}
#elif defined(FINCEPT_STALL_WIN_STACKS)
HANDLE g_gui_thread_handle = nullptr;

/// Walk the GUI thread's stack while it is suspended. Uses only the x64 unwind
/// tables (RtlLookupFunctionEntry / RtlVirtualUnwind): no heap allocation and
/// no symbol engine, so it cannot deadlock on a lock the suspended thread holds.
int capture_suspended_stack(DWORD64* frames, int max_frames) {
    if (!g_gui_thread_handle || SuspendThread(g_gui_thread_handle) == static_cast<DWORD>(-1))
        return 0;
    int n = 0;
    CONTEXT ctx;
    ZeroMemory(&ctx, sizeof ctx);
    ctx.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
    if (GetThreadContext(g_gui_thread_handle, &ctx)) {
        while (n < max_frames && ctx.Rip != 0) {
            frames[n++] = ctx.Rip;
            DWORD64 image_base = 0;
            PRUNTIME_FUNCTION fn = RtlLookupFunctionEntry(ctx.Rip, &image_base, nullptr);
            if (!fn) {
                // Leaf function: the return address is at the top of the stack.
                if (IsBadReadPtr(reinterpret_cast<const void*>(ctx.Rsp), sizeof(DWORD64)))
                    break;
                ctx.Rip = *reinterpret_cast<const DWORD64*>(ctx.Rsp);
                ctx.Rsp += sizeof(DWORD64);
            } else {
                PVOID handler_data = nullptr;
                DWORD64 establisher = 0;
                RtlVirtualUnwind(UNW_FLAG_NHANDLER, image_base, ctx.Rip, fn, &ctx, &handler_data, &establisher,
                                 nullptr);
            }
        }
    }
    ResumeThread(g_gui_thread_handle);
    return n;
}
#endif

/// The stall log opened for appending, or nullptr. Windows: _wfopen_s (fopen is
/// deprecated under MSVC /W4, C4996, and cannot open a non-ANSI path).
std::FILE* open_for_append() {
#ifdef Q_OS_WIN
    std::FILE* f = nullptr;
    return _wfopen_s(&f, g_log_path_w.c_str(), L"a") == 0 ? f : nullptr;
#else
    return std::fopen(g_log_path.c_str(), "a");
#endif
}

void append(const std::string& text) {
    if (std::FILE* f = open_for_append()) {
        std::fputs(text.c_str(), f);
        std::fclose(f);
    }
}

std::string timestamp() {
    return QDateTime::currentDateTime().toString(Qt::ISODateWithMs).toStdString();
}

/// `compact`: one line, frames joined by ';' as <module basename>+0x<offset>
/// (the sampling form — see FINCEPT_UI_STALL_SAMPLE_MS); otherwise one
/// indented "#i module+0xoff symbol" line per frame.
std::string capture_gui_stack(bool compact = false) {
    std::string out;
#if defined(Q_OS_LINUX)
    if (!g_capture_stacks)
        return out;
    g_frame_count.store(-1);
    if (pthread_kill(g_gui_thread, SIGUSR2) != 0)
        return out;
    for (int i = 0; i < 40 && g_frame_count.load() < 0; ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    const int n = g_frame_count.load();
    for (int i = 0; i < n; ++i) {
        Dl_info info{};
        char line[768];
        if (dladdr(g_frames[i], &info) && info.dli_fname) {
            const auto offset = static_cast<unsigned long>(static_cast<const char*>(g_frames[i]) -
                                                           static_cast<const char*>(info.dli_fbase));
            if (compact) {
                const char* base = std::strrchr(info.dli_fname, '/');
                std::snprintf(line, sizeof line, "%s%s+0x%lx", i ? ";" : "", base ? base + 1 : info.dli_fname,
                              offset);
            } else {
                std::snprintf(line, sizeof line, "    #%d %s+0x%lx %s\n", i, info.dli_fname, offset,
                              info.dli_sname ? info.dli_sname : "");
            }
        } else if (compact) {
            std::snprintf(line, sizeof line, "%s?", i ? ";" : "");
        } else {
            std::snprintf(line, sizeof line, "    #%d %p\n", i, g_frames[i]);
        }
        out += line;
    }
#elif defined(FINCEPT_STALL_WIN_STACKS)
    DWORD64 frames[kMaxFrames];
    const int n = capture_suspended_stack(frames, kMaxFrames);
    // The thread is running again; resolving modules may now take locks freely.
    for (int i = 0; i < n; ++i) {
        char line[768];
        HMODULE module = nullptr;
        char path[MAX_PATH] = {0};
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               reinterpret_cast<LPCSTR>(frames[i]), &module) &&
            module && GetModuleFileNameA(module, path, MAX_PATH)) {
            const char* name = std::strrchr(path, '\\');
            const auto offset = static_cast<unsigned long long>(frames[i] - reinterpret_cast<DWORD64>(module));
            if (compact)
                std::snprintf(line, sizeof line, "%s%s+0x%llx", i ? ";" : "", name ? name + 1 : path, offset);
            else
                std::snprintf(line, sizeof line, "    #%d %s+0x%llx\n", i, name ? name + 1 : path, offset);
        } else if (compact) {
            std::snprintf(line, sizeof line, "%s?", i ? ";" : "");
        } else {
            std::snprintf(line, sizeof line, "    #%d 0x%llx\n", i, static_cast<unsigned long long>(frames[i]));
        }
        out += line;
    }
#else
    Q_UNUSED(compact);
#endif
    return out;
}

void watch() {
    bool stalled = false;
    qint64 stall_started = 0;
    std::string stall_context;
    while (g_running.load()) {
        // While a stall is in progress and sampling is on, wake at the sampling
        // interval and record where the GUI thread is now, so a long stall is
        // described by many stacks (a profile) rather than by its first 250 ms.
        const bool sampling = stalled && g_sample_ms > 0;
        std::this_thread::sleep_for(std::chrono::milliseconds(sampling ? g_sample_ms : kPollIntervalMs));
        const qint64 last = g_last_beat_ms.load();
        const qint64 lag = now_ms() - last;
        if (!stalled && lag >= g_threshold_ms) {
            stalled = true;
            stall_started = last;
            std::string context;
            {
                std::lock_guard<std::mutex> lock(g_context_mutex);
                context = g_context;
            }
            stall_context = context;
            append(timestamp() + " STALL >= " + std::to_string(g_threshold_ms) + " ms  context: " + context + "\n" +
                   capture_gui_stack());
        } else if (stalled && lag < g_threshold_ms) {
            stalled = false;
            append(timestamp() + " STALL END " + std::to_string(last - stall_started) + " ms\n");
            telemetry::UsageReporter::note_ui_stall(QString::fromStdString(stall_context), last - stall_started);
        } else if (sampling) {
            const std::string stack = capture_gui_stack(/*compact=*/true);
            // Re-check: the stall may have ended while the stack was taken.
            if (!stack.empty() && now_ms() - g_last_beat_ms.load() >= g_threshold_ms)
                append("    SAMPLE " + stack + "\n");
        }
    }
}

} // namespace

void UiStallMonitor::start(const QString& log_path) {
    if (g_running.load() || !QCoreApplication::instance())
        return;
    bool ok = false;
    const int from_env = qEnvironmentVariableIntValue("FINCEPT_UI_STALL_MS", &ok);
    if (ok) {
        if (from_env <= 0)
            return; // disabled
        g_threshold_ms = from_env;
    }
    g_sample_ms = qBound(0, qEnvironmentVariableIntValue("FINCEPT_UI_STALL_SAMPLE_MS"), 1000);
    g_log_path = log_path.toStdString();
#ifdef Q_OS_WIN
    g_log_path_w = log_path.toStdWString();
#endif
    g_clock.start();
    g_last_beat_ms.store(now_ms());

#if defined(Q_OS_LINUX)
    g_capture_stacks = qEnvironmentVariableIntValue("FINCEPT_UI_STALL_STACKS") == 1;
    if (g_capture_stacks) {
        g_gui_thread = pthread_self();
        void* warm[2];
        backtrace(warm, 2); // loads the unwinder now, not inside the signal handler
        struct sigaction action {};
        action.sa_handler = stack_signal_handler;
        sigemptyset(&action.sa_mask);
        action.sa_flags = SA_RESTART;
        sigaction(SIGUSR2, &action, nullptr);
    }
#elif defined(FINCEPT_STALL_WIN_STACKS)
    // A real handle to this (the GUI) thread for the watcher to suspend and read.
    DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &g_gui_thread_handle,
                    THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, 0);
#endif

    auto* beat = new QTimer(QCoreApplication::instance());
    beat->setTimerType(Qt::PreciseTimer);
    beat->setInterval(kBeatIntervalMs);
    QObject::connect(beat, &QTimer::timeout, beat, []() { g_last_beat_ms.store(now_ms()); });
    beat->start();

    g_running.store(true);
    g_watcher = std::thread(watch);
    // Some exit paths (the headless selftests) return from main() without the
    // event loop's aboutToQuit; a still-joinable std::thread at static
    // destruction would call std::terminate. Stop and join on any normal exit.
    std::atexit([]() { UiStallMonitor::stop(); });
    LOG_DEBUG("UiStall", QString("GUI stall monitor started threshold_ms=%1 path=%2")
                             .arg(g_threshold_ms)
                             .arg(privacy::redact_home(log_path)));
}

void UiStallMonitor::set_context(const QString& context) {
    std::lock_guard<std::mutex> lock(g_context_mutex);
    g_context = context.toStdString();
}

void UiStallMonitor::stop() {
    if (!g_running.exchange(false))
        return;
    if (g_watcher.joinable())
        g_watcher.join();
}

} // namespace fincept::debug
