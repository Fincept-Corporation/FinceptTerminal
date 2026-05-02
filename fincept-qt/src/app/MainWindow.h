#pragma once
//
// Compatibility forwarder.
//
// `MainWindow` was renamed to `WindowFrame` in Phase 3 of the multi-window
// refactor (see `.grill-me/multi-window-implementation-plan.md`). All
// implementation lives in `app/WindowFrame.h`/`app/WindowFrame.cpp`.
//
// This header is kept for one release so external consumers (and stale
// `#include "app/MainWindow.h"` lines that haven't been touched yet) keep
// compiling. New code should prefer `#include "app/WindowFrame.h"` and the
// `WindowFrame` type directly.
//
// To remove: search for `#include "app/MainWindow.h"` across the codebase,
// replace with `#include "app/WindowFrame.h"`, then delete this file plus
// the `using` alias.
//
#include "app/WindowFrame.h"

namespace fincept {
using MainWindow = WindowFrame;
}
