// src/core/debug/UiStallMonitor.h
// Detects when the GUI thread stops processing events ("the terminal froze")
// and records how long it lasted and, where the platform allows, what the GUI
// thread was executing.
//
// How it works: a timer on the GUI thread stamps the time every few
// milliseconds; a watcher thread checks the stamp. If it goes stale for longer
// than the threshold the GUI thread is busy inside one event handler — a
// stall. The watcher appends one record per stall to <logs>/ui_stalls.log:
//
//   2026-11-16T12:40:26.300 STALL >= 250 ms  context: screen=portfolio
//     #0 /work/build/FinceptTerminal+0x1a2b3c
//     #1 ...
//   2026-11-16T12:40:32.540 STALL END 6240 ms
//
// The stack of the GUI thread is recorded as module+offset frames:
//   * Windows (x64): always — the watcher suspends the GUI thread for an
//     instant and unwinds it; resolve offsets with the linker map
//     (FinceptTerminal.map next to the executable).
//   * Linux: when FINCEPT_UI_STALL_STACKS=1 — a signal is sent to the GUI
//     thread and it records its own backtrace; resolve with addr2line.
// Elsewhere only durations and the context are recorded.
//
// Always on: the cost is one timer tick every 20 ms and a thread that sleeps.
// FINCEPT_UI_STALL_MS overrides the threshold (0 disables the monitor).
// FINCEPT_UI_STALL_SAMPLE_MS=N (where stacks are recorded) additionally takes
// a stack every N ms for as long as a stall lasts, one line per sample:
//     SAMPLE FinceptTerminal+0x1a2b3c;libQt6Widgets.so.6+0x...;...
// (leaf first) — a sampling profile of the stall for finding dominant frames.
#pragma once

#include <QString>

namespace fincept::debug {

class UiStallMonitor {
  public:
    /// Start monitoring. Call once, on the GUI thread, after QApplication
    /// exists. `log_path` is the file stall records are appended to.
    static void start(const QString& log_path);

    /// A short label for what the user is doing (active screen, running
    /// action), written with the next stall record. Cheap; GUI thread only.
    static void set_context(const QString& context);

    /// Stop the watcher thread (application shutdown).
    static void stop();
};

} // namespace fincept::debug
