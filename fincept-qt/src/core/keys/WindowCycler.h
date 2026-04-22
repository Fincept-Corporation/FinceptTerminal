#pragma once
#include <QList>
#include <QObject>
#include <QPointer>

namespace fincept {

class MainWindow;

/// Process-global registry of live MainWindow instances, plus the keyboard
/// navigation policies that move focus between them. Kept out of
/// QApplication/QCoreApplication so tests can introspect it without needing
/// a full Qt app instance.
///
/// MainWindow::MainWindow() calls register_window(this); the destructor calls
/// unregister_window(this) — the cycler holds weak (QPointer) refs so it
/// never keeps a dying window alive.
///
/// Ordering: windows are stored in insertion order but surfaced to the user
/// in ascending window_id order, so Ctrl+1 = primary, Ctrl+2 = first
/// secondary, and so on. That mapping is stable across a session even if
/// windows are created out of order.
class WindowCycler : public QObject {
    Q_OBJECT
  public:
    static WindowCycler& instance();

    void register_window(MainWindow* w);
    void unregister_window(MainWindow* w);

    /// Return all live MainWindows sorted by window_id (ascending).
    QList<MainWindow*> windows() const;

    /// Focus the Nth window (0-based) in window_id order. No-op if N is out
    /// of range. Activates, raises, and gives keyboard focus to the window.
    void focus_window_by_index(int n);

    /// Cycle focus forward/back through MainWindows in window_id order. No-op
    /// if fewer than 2 windows are open.
    void cycle_windows(bool forward);

    /// Cycle the focused panel inside whichever MainWindow currently has
    /// focus. Delegates to that window's DockScreenRouter.
    void cycle_panels_in_current_window(bool forward);

    /// Spawn a new MainWindow placed on the next available monitor (the
    /// first monitor that doesn't already host the currently-focused
    /// MainWindow's centre). Falls back to the primary screen.
    void new_window_on_next_monitor();

    /// Move the currently-focused MainWindow to monitor N (0-based index
    /// into QGuiApplication::screens()). No-op if N is out of range or
    /// the window is already there.
    void move_current_window_to_monitor(int monitor_index);

  private:
    WindowCycler() = default;

    MainWindow* current_focused() const;

    /// Holds weak refs so a destroyed window is silently skipped until the
    /// explicit unregister call cleans up the slot.
    QList<QPointer<MainWindow>> windows_;
};

} // namespace fincept
