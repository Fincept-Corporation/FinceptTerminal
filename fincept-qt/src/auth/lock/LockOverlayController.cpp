#include "auth/lock/LockOverlayController.h"

#include "app/WindowFrame.h"
#include "auth/InactivityGuard.h"
#include "core/logging/Logger.h"
#include "core/window/WindowRegistry.h"
#include "screens/auth/LockScreen.h"

// ──────────────────────────────────────────────────────────────────────────
// PIN SECURITY AUDIT INVARIANT CHECKLIST (project_security_audit memory)
// ──────────────────────────────────────────────────────────────────────────
// This file is on the lock-screen lifecycle path. Any change must preserve:
//
//   1. **PIN lifecycle**: PinManager owns PIN state. This controller MUST
//      NOT call set_pin / change_pin / clear_pin / verify_pin. Verified by
//      grep of this file: zero PinManager calls. LockScreen drives PIN
//      operations via its own existing wiring; controller just owns the
//      widget instance.
//
//   2. **Lock authority**: `InactivityGuard::terminal_locked_` is the
//      single source of truth for "is the terminal locked?". This
//      controller MUST NOT call set_terminal_locked. Verified: only
//      observes via terminal_locked_changed signal.
//
//   3. **Multi-window**: every WindowFrame must apply lock state in
//      lockstep. Pre-Phase-1c, every frame had its own connect() to
//      InactivityGuard::terminal_locked_changed → apply_lock_state.
//      That connection is RETAINED — the controller's parallel
//      connection is additive observation, not a replacement. Running
//      apply_lock_state twice is idempotent (it short-circuits when
//      stack_->currentIndex() is already 3).
//
//   4. **Timer-restart**: `InactivityGuard::set_timeout_minutes`
//      restarts the timer when the guard is enabled. This controller
//      never touches the InactivityGuard timer. Verified: zero
//      timer_->start / set_timeout_minutes / set_enabled calls.
//
//   5. **Ladder sizing**: `PinManager`'s attempt ladder (free attempts,
//      then 30s/60s lockouts) is unchanged. PinManager.cpp not edited
//      in this lift.
//
// If any of these invariants need to change for a real reason, update
// this checklist + the project_security_audit memory entry first.
// ──────────────────────────────────────────────────────────────────────────

namespace fincept::auth {

namespace {
constexpr const char* kLockTag = "LockOverlay";
} // namespace

LockOverlayController& LockOverlayController::instance() {
    static LockOverlayController s;
    return s;
}

void LockOverlayController::initialise() {
    if (initialised_)
        return;
    initialised_ = true;

    auto& reg = WindowRegistry::instance();
    connect(&reg, &WindowRegistry::frame_added, this, &LockOverlayController::on_frame_added);
    connect(&reg, &WindowRegistry::frame_removing, this, &LockOverlayController::on_frame_removing);
    for (auto* w : reg.frames())
        on_frame_added(w);

    // Subscribe once to the process-wide locked flag. Replaces the
    // pre-Phase-1c per-frame connections (which still exist as a
    // belt-and-braces; apply_lock_state is idempotent).
    connect(&InactivityGuard::instance(), &InactivityGuard::terminal_locked_changed,
            this, &LockOverlayController::on_terminal_locked_changed);

    LOG_INFO(kLockTag,
             QString("Initialised; tracking %1 frame(s)").arg(tracked_frames_.size()));
}

fincept::screens::LockScreen*
LockOverlayController::lock_screen_for(fincept::WindowFrame* frame) {
    if (!frame)
        return nullptr;

    auto it = tracked_frames_.find(frame);
    if (it != tracked_frames_.end() && it.value().lock_screen) {
        return it.value().lock_screen.data();
    }

    // Mint a fresh LockScreen with no QWidget parent (the controller is a
    // QObject, not a QWidget — can't be a Qt parent for a widget). The
    // frame's master_stack will reparent it via addWidget() — that's
    // normal QStackedWidget behaviour. The QPointer in `Tracked` goes
    // null when master_stack destroys the widget at frame teardown.
    auto* ls = new fincept::screens::LockScreen(nullptr);

    Tracked t;
    t.frame = QPointer<fincept::WindowFrame>(frame);
    t.lock_screen = QPointer<fincept::screens::LockScreen>(ls);
    tracked_frames_.insert(frame, t);

    LOG_DEBUG(kLockTag,
              QString("Minted LockScreen for frame %1 (tracking %2)")
                  .arg(reinterpret_cast<quintptr>(frame), 0, 16)
                  .arg(tracked_frames_.size()));
    return ls;
}

void LockOverlayController::on_frame_added(fincept::WindowFrame* w) {
    if (!w)
        return;
    // The frame's constructor calls lock_screen_for(this) before this
    // signal fires (constructor-time call is direct, signal fires after
    // register_frame). So the entry typically already exists; this
    // path just confirms tracking.
    if (!tracked_frames_.contains(w))
        tracked_frames_.insert(w, Tracked{QPointer<fincept::WindowFrame>(w), {}});
}

void LockOverlayController::on_frame_removing(fincept::WindowFrame* w) {
    if (!w)
        return;
    tracked_frames_.remove(w);
    LOG_DEBUG(kLockTag,
              QString("Frame removed; tracking %1").arg(tracked_frames_.size()));
}

void LockOverlayController::on_terminal_locked_changed(bool locked) {
    LOG_INFO(kLockTag,
             QString("Terminal lock state: %1 (broadcasting to %2 frames)")
                 .arg(locked ? "locked" : "unlocked").arg(tracked_frames_.size()));
    // The actual UI flip happens through the frame's own subscription
    // to InactivityGuard::terminal_locked_changed → apply_lock_state.
    // Every tracked frame runs apply_lock_state idempotently.
    //
    // The controller's signal-handling here is a centralised observation
    // point for telemetry / future surface-level work. We don't call
    // apply_lock_state directly because WindowFrame's connection has
    // already done it — running twice would be wasteful, not wrong.
}

} // namespace fincept::auth
