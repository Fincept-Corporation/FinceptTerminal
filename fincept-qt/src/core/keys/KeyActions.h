// src/core/keys/KeyActions.h
#pragma once

namespace fincept {

enum class KeyAction {
    // Global (MainWindow)
    Refresh,
    ToggleChat,
    FocusMode,
    Fullscreen,
    Screenshot,

    // Navigation (CommandBar)
    NavNext,
    NavPrev,
    NavAccept,
    NavEscape,

    // News Screen
    NewsNext,
    NewsPrev,
    NewsOpen,
    NewsClose,

    // Code Editor
    RunCell,
    RunAndNext,
    RenameCell,

    // Window / Panel Navigation (Phase 3)
    // Focus a specific MainWindow by its 1-based ordinal across open windows
    // (Ctrl+1..9 by default). Ctrl+1 always targets window_id = 0.
    FocusWindow1,
    FocusWindow2,
    FocusWindow3,
    FocusWindow4,
    FocusWindow5,
    FocusWindow6,
    FocusWindow7,
    FocusWindow8,
    FocusWindow9,

    // Cycle active dock widget inside the focused MainWindow.
    CyclePanelsForward,
    CyclePanelsBack,

    // Cycle focus between MainWindow instances (Bloomberg PANEL key).
    CycleWindowsForward,
    CycleWindowsBack,

    // Spawn a new MainWindow, placed on the next available monitor.
    NewWindowNextMonitor,

    // Move the current MainWindow to monitor N (Ctrl+Shift+1..9).
    MoveWindowToMonitor1,
    MoveWindowToMonitor2,
    MoveWindowToMonitor3,
    MoveWindowToMonitor4,
    MoveWindowToMonitor5,
    MoveWindowToMonitor6,
    MoveWindowToMonitor7,
    MoveWindowToMonitor8,
    MoveWindowToMonitor9,

    // Phase 6 — Component Browser
    BrowseComponents,

    // Phase 11 — Always-on-top toggle on the focused window.
    ToggleAlwaysOnTop,

    // Phase 12 — Manual lock. Immediately shows the PIN-unlock screen
    // without waiting for inactivity. Default binding: Ctrl+L.
    LockNow,
};

} // namespace fincept
