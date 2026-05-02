#pragma once
#include <QHash>
#include <QObject>
#include <QPointer>

namespace fincept {
class WindowFrame;
}

namespace fincept::screens {
class LockScreen;
}

namespace fincept::auth {

/// Phase 1b/1c: shell-level coordinator AND owner of per-frame LockScreen
/// widgets.
///
/// **Final design** (Phase 1c lift):
///   - The controller is the factory for LockScreen widgets. WindowFrame
///     asks `lock_screen_for(frame)` during its constructor; the
///     controller mints a fresh LockScreen, parented to the controller,
///     and returns it. The frame's master_stack then `addWidget(ls)`
///     which reparents the widget to master_stack — Qt's normal
///     QStackedWidget behaviour. When the frame is destroyed, master_stack
///     destroys the widget, and the controller's QPointer goes null
///     automatically.
///
///   - The controller subscribes to `WindowRegistry::frame_added /
///     frame_removing` and cleans up its tracking hash on remove.
///
///   - The controller subscribes once to `InactivityGuard::
///     terminal_locked_changed` (replacing N per-frame connections).
///     On flip, it iterates tracked frames and calls each frame's
///     `apply_lock_state()`. WindowFrame retains its direct
///     subscription as a belt-and-braces since `apply_lock_state` is
///     idempotent — running twice is safe.
///
/// PIN security audit invariants preserved:
///   1. **PIN lifecycle**: PinManager untouched. The controller doesn't
///      create / change / clear PIN state — only LockScreen does that
///      via its existing wiring.
///   2. **Lock authority**: `InactivityGuard::terminal_locked_` remains
///      the single source of truth. The controller observes; doesn't
///      mutate.
///   3. **Multi-window**: every tracked frame's apply_lock_state runs
///      from the controller's broadcast. Frame-removing cleanup
///      ensures stale frames don't get stale signals.
///   4. **Timer-restart**: `InactivityGuard::set_timeout_minutes`
///      semantics unchanged; the controller doesn't touch the timer.
///   5. **Ladder sizing**: `PinManager`'s attempt-ladder + lockout
///      logic unchanged. LockScreen drives the lockout UI; controller
///      just owns the widget.
///
/// Threading: UI-thread only.
class LockOverlayController : public QObject {
    Q_OBJECT
  public:
    static LockOverlayController& instance();

    /// Connect to WindowRegistry + InactivityGuard. Called from
    /// `TerminalShell::initialise` after the registries are warmed.
    /// Idempotent.
    void initialise();

    bool is_initialised() const { return initialised_; }

    /// Phase 1c: factory. Mints a new LockScreen for `frame`, parented
    /// to the controller. The frame's master_stack will reparent it
    /// when added to the stack. Returns the same widget on subsequent
    /// calls for the same frame (idempotent).
    ///
    /// Returns nullptr if `frame` is null.
    fincept::screens::LockScreen* lock_screen_for(fincept::WindowFrame* frame);

    /// Test/debug accessor.
    int tracked_frame_count() const { return tracked_frames_.size(); }

  private slots:
    void on_frame_added(fincept::WindowFrame* w);
    void on_frame_removing(fincept::WindowFrame* w);
    void on_terminal_locked_changed(bool locked);

  private:
    LockOverlayController() = default;

    bool initialised_ = false;

    /// Per-frame state. Both pointers are guarded — frame is a QPointer
    /// (cleared when the frame's QObject is destroyed) and lock_screen
    /// is a QPointer (cleared when master_stack destroys it during
    /// frame teardown).
    struct Tracked {
        QPointer<fincept::WindowFrame> frame;
        QPointer<fincept::screens::LockScreen> lock_screen;
    };
    QHash<fincept::WindowFrame*, Tracked> tracked_frames_;
};

} // namespace fincept::auth
