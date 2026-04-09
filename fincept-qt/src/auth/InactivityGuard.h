#pragma once
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

  signals:
    /// Emitted when the idle timeout expires — MainWindow should show lock screen.
    void lock_requested();

  protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

  private:
    InactivityGuard();

    QTimer* timer_ = nullptr;
    bool enabled_ = false;
};

} // namespace fincept::auth
