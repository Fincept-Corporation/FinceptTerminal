#pragma once

namespace fincept::crash {

/// Install the platform crash handler. On Windows this registers an
/// UnhandledExceptionFilter that writes a minidump to AppPaths::crashdumps()
/// when the process is about to die (access violation, stack overflow, GS
/// cookie check failure, etc.). No-op on other platforms.
///
/// Call once, as early as possible in main() — before any Qt object is
/// constructed. The handler does not allocate heap memory during a crash
/// (dbghelp is loaded up-front and the dump path is pre-resolved), so it
/// remains safe even when the heap is corrupted.
void install();

} // namespace fincept::crash
