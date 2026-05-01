#pragma once
#include <QDateTime>
#include <QObject>
#include <QTimer>

class QEvent;

namespace fincept::auth {

/// Global event filter that monitors user activity (mouse/keyboard) and
/// emits lock_requested() after a configurable idle timeout.
///
/// Install on QApplication: qApp->installEventFilter(&InactivityGuard::instance())
///
/// Default timeout: 10 minutes. Configurable via set_timeout_minutes().
/// Timer only runs when enabled (call set_enabled(true) after PIN is configured).
class InactivityGuard : public QObject {
    Q_OBJECT
  public:
    static InactivityGuard& instance();

    /// Start monitoring user activity. Call after PIN setup is complete.
    void set_enabled(bool enabled);
    bool is_enabled() const { return enabled_; }

    /// Set idle timeout in minutes. Minimum 1, maximum 60. Default 10.
    void set_timeout_minutes(int minutes);
    int timeout_minutes() const;

    /// Reset the inactivity timer (called internally on user activity).
    void reset_timer();

    /// Check whether enough wall-clock time has elapsed since the last
    /// recorded user activity that a lock should be forced now. Used after
    /// resume from sleep/suspend — QTimer pauses while the OS is asleep, so
    /// it cannot be relied on to fire immediately on wake. If the wall-clock
    /// delta exceeds the configured timeout, emit lock_requested() directly.
    /// Returns true if a lock was forced.
    bool check_for_resume_lock();

    /// Force an immediate lock (emits lock_requested). Called by the
    /// manual "Lock Now" action and by minimize-to-lock.
    void trigger_manual_lock();

    /// Terminal-locked flag. The first WindowFrame that handles a lock event
    /// sets this to true; every other WindowFrame listens for terminal_locked_changed
    /// and locks its own UI from the same source of truth. Critical for
    /// multi-window correctness — without it, only the window that fielded
    /// lock_requested would actually lock, leaving secondary windows showing
    /// live data behind the back of the user's lock screen.
    void set_terminal_locked(bool locked);
    bool is_terminal_locked() const { return terminal_locked_; }

  signals:
    /// Emitted when the idle timeout expires — WindowFrame should show lock screen.
    void lock_requested();

    /// Emitted whenever the process-wide locked flag flips. Every WindowFrame
    /// listens for this so secondary windows lock/unlock in lockstep with
    /// the window that initiated the transition.
    void terminal_locked_changed(bool locked);

  protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

  private:
    InactivityGuard();

    QTimer* timer_ = nullptr;
    bool enabled_ = false;
    bool terminal_locked_ = false;
    // Wall-clock timestamp of the last recorded user activity. Used by
    // check_for_resume_lock() to decide whether the machine slept through
    // the inactivity window (QTimer pauses during OS suspend).
    QDateTime last_activity_;
};

} // namespace fincept::auth
