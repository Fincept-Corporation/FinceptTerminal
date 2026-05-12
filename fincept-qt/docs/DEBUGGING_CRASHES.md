# Debugging long-running crashes on macOS

This page is a runbook for diagnosing the class of crash where the app runs
for hours and then dies inside Qt internals with `EXC_BAD_ACCESS` — typically
inside `doActivate`, `channelReadyRead`, or `qt_mac_socket_callback`. These
are almost always use-after-free / wild-pointer reads against an object that
was destroyed minutes-to-hours earlier; the stack trace shows the *use* site
but tells you nothing about *which allocation went bad*.

The runbook below pinpoints the exact alloc + free pair with full backtraces.

---

## TL;DR

```bash
# 1. Launch the signed bundle under macOS malloc instrumentation.
./scripts/diagnose-macos-crash.sh

# 2. Use the app normally until it crashes (could be minutes to hours).

# 3. Open the crash report:
#       ~/Library/Logs/DiagnosticReports/FinceptTerminal-*.ips
#    Find the faulting address (look for `far:` in the ARM thread state,
#    or `Exception Subtype: KERN_INVALID_ADDRESS at 0x…`).
#
#    With MallocScribble enabled, a use-after-free shows up as the address
#    `0x5555…` or pattern-derived bytes — that's the smoking gun.

# 4. Get the alloc + free backtraces for that address:
malloc_history $(pgrep -n FinceptTerminal) -allEvents 0x<address>
# If the process has already exited, run against the snapshot:
malloc_history ~/Library/Logs/DiagnosticReports/FinceptTerminal-*.ips 0x<address>
```

You now have:
- the line of code that **allocated** the object,
- the line of code that **freed** it,
- the line of code that **tried to use it after free** (from the crash stack).

Root cause is a 3-way grep, not a guess.

---

## Why the malloc-debug approach is right here

| Approach | Catches this bug? | Requires rebuild? | Works on signed bundle? |
|---|---|---|---|
| **MallocStackLoggingNoCompact + MallocScribble** | yes (UAF) | no | **yes** |
| AddressSanitizer | yes (UAF + UB) | yes (incl. Qt) | no |
| Crashpad / Sentry telemetry | statistically, across many users | no | yes |
| ThreadSanitizer | only data races | yes | no |
| Static analysis (clang-tidy) | sometimes (patterns) | no | no |
| Code review / guess | no — wastes time | no | no |

`MallocScribble` is the killer feature: every `free()` writes `0x55` over the
returned region, so a subsequent read crashes **at the use site** instead of
2+ hours later when the region happens to be reused for an int. The crash
becomes deterministic in the sense that the bad read becomes immediate.

For details on each env var see Apple's
[Malloc Debug Environment Variables](https://developer.apple.com/library/archive/releasenotes/DeveloperTools/RN-MallocOptions/)
and the
[`malloc_history(1)` man page](https://keith.github.io/xcode-man-pages/malloc_history.1.html).

---

## When to reach for AddressSanitizer instead

ASan beats `malloc_history` when:

- You can **reproduce the crash on demand** (under a synthetic stress harness, in
  a unit test, etc.).
- You care about **uninitialised reads / heap overflows / stack UAF**, which
  `MallocScribble` doesn't cover.

To build it:

```bash
cmake --preset macos-asan
cmake --build build/macos-asan
./scripts/diagnose-macos-crash.sh --asan
```

The `macos-asan` preset is defined in `CMakePresets.json`. It uses
`-fsanitize=address,undefined` and disables unity builds so backtraces are
clean.

> Caveat: ASan only instruments **your** code by default — Qt's frameworks
> are untouched. Most QObject UAFs still get caught (ASan flags the use, even
> if the alloc is in Qt) but bugs inside Qt itself are invisible. For those,
> stay on `malloc_history`.

---

## When to ship telemetry instead

If the crash only happens for **specific users** (specific brokers configured,
specific LLM providers, specific OS versions) and you can't repro locally:

- Integrate **Crashpad** (Chromium's crash reporter, used by Slack/Spotify).
  ~200 LOC to wire to `QApplication`, uploads to your own backend or
  Backtrace/Sentry. After a week of real users you'll have N similar crashes
  with full state.

This is more work than the malloc-debug recipe, but it's the right answer
when "use the app for 2 hours then it crashes" can't be reproduced on a
developer machine.

---

## Reading the crash report

The `.ips` files under `~/Library/Logs/DiagnosticReports/` are JSON-formatted
crash reports. Useful fields:

```
exception.type:    EXC_BAD_ACCESS / EXC_BREAKPOINT / ...
exception.subtype: "KERN_INVALID_ADDRESS at 0x…"        ← the faulting address
faultingThread:    <index into threads[]>
threads[*].frames: array of {imageOffset, symbol, …}    ← per-frame symbols
threadState.far:   the address that triggered the page fault on ARM64
```

For the kind of crash this runbook covers:

- `far` will be a small number (`0x3f`, `0x40`, etc.) — that means the object
  pointer was 0 and the code read a member at that offset, **or** it'll be
  a `0x5555…` pattern — that's `MallocScribble`'s signature confirming the
  region was freed before reuse.
- The frame at index 0 is the *use* site. It's the most-recently-executed
  code touching the bad pointer, **not** the bug location.
- The bug is at the line that *freed* the object too early. That's what
  `malloc_history -allEvents <far>` tells you.

---

## Background reading

- [Apple — Enabling the Malloc Debugging Features](https://developer.apple.com/library/archive/documentation/Performance/Conceptual/ManagingMemory/Articles/MallocDebug.html)
- [Apple — Malloc Debug Environment Variables Release Notes](https://developer.apple.com/library/archive/releasenotes/DeveloperTools/RN-MallocOptions/)
- [`malloc_history(1)` man page](https://keith.github.io/xcode-man-pages/malloc_history.1.html)
- [Qt forum — Random crashes on Apple Silicon (Qt 6)](https://forum.qt.io/topic/135220/random-application-crash-on-apple-silicon-m1-qt-6-2-3/42)
- [KDAB — Building Qt with AddressSanitizer](https://www.kdab.com/qt-asan-windows/)
