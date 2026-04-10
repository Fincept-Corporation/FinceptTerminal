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
};

} // namespace fincept
