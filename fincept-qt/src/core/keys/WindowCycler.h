#pragma once
#include <QObject>

namespace fincept {

class WindowFrame;

/// Keyboard policy for moving focus between MainWindows and spawning new
/// ones on specific monitors. Storage of which windows exist now lives in
/// `WindowRegistry` (Phase 1 split); WindowCycler is the thin policy layer
/// over it.
///
/// Public API is unchanged from the pre-split version so existing callers
/// (`Ctrl+1..9`, toolbar handlers, `--new-window` IPC) keep working as-is.
/// register_window/unregister_window are thin passthroughs to the registry.
///
/// Ordering: windows are surfaced to the user in ascending `window_id`
/// order, so Ctrl+1 = primary, Ctrl+2 = first secondary, and so on. Stable
/// across a session even if windows are created out of order.
class WindowCycler : public QObject {
    Q_OBJECT
  public:
    static WindowCycler& instance();

    /// Pass-through to WindowRegistry::register_frame for backward
    /// compatibility with existing call sites.
    void register_window(WindowFrame* w);
    void unregister_window(WindowFrame* w);

    /// Focus the Nth window (0-based) in window_id order. No-op if N is out
    /// of range. Activates, raises, and gives keyboard focus to the window.
    void focus_window_by_index(int n);

    /// Cycle focus forward/back through MainWindows in window_id order. No-op
    /// if fewer than 2 windows are open.
    void cycle_windows(bool forward);

    /// Cycle the focused panel inside whichever WindowFrame currently has
    /// focus. Delegates to that window's DockScreenRouter.
    void cycle_panels_in_current_window(bool forward);

    /// Spawn a new WindowFrame placed on the next available monitor. Per
    /// decision 5.6: prefer the first monitor with no frames at all; fall
    /// back to the first monitor that doesn't contain the focused window's
    /// centre; final fallback is primary screen.
    void new_window_on_next_monitor();

    /// Move the currently-focused WindowFrame to monitor N (0-based index
    /// into QGuiApplication::screens()). No-op if N is out of range or
    /// the window is already there.
    void move_current_window_to_monitor(int monitor_index);

    /// The frame that currently holds focus (active window), or the first
    /// registered frame if focus has wandered to a non-frame top-level
    /// (e.g. a modal dialog). Returns nullptr only when no frames exist.
    /// Phase 4 ActionRegistry handlers use this to resolve `focused_frame`
    /// live at action invocation time.
    WindowFrame* focused_frame() const { return current_focused(); }

  private:
    WindowCycler() = default;

    WindowFrame* current_focused() const;
};

} // namespace fincept
