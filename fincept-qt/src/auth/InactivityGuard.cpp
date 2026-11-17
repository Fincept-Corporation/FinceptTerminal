#include "auth/InactivityGuard.h"

#include "auth/SecurityAuditLog.h"
#include "core/logging/Logger.h"

#include <QApplicationStateChangeEvent>
#include <QCoreApplication>
#include <QEvent>
#include <QGuiApplication>
#include <QWindow>

#include <algorithm>

namespace fincept::auth {

// Why the next lock was requested, for the one "Terminal locked" line written when the
// process-wide flag flips (set_terminal_locked). GUI thread only.
static const char* s_inactivity_guard_lock_reason = nullptr;

InactivityGuard& InactivityGuard::instance() {
    static InactivityGuard s;
    return s;
}

InactivityGuard::InactivityGuard() : QObject(nullptr) {
    timer_ = new QTimer(this);
    timer_->setSingleShot(true);
    timer_->setInterval(10 * 60 * 1000); // 10 minutes default
    last_activity_ = QDateTime::currentDateTime();

    connect(timer_, &QTimer::timeout, this, [this]() {
        if (enabled_) {
            LOG_DEBUG("Auth", "Inactivity timeout — requesting lock");
            s_inactivity_guard_lock_reason = "inactivity";
            SecurityAuditLog::instance().record("inactivity_lock");
            emit lock_requested();
        }
    });
}

void InactivityGuard::set_enabled(bool enabled) {
    enabled_ = enabled;
    if (enabled) {
        timer_->start();
        LOG_DEBUG("Auth", QString("Inactivity guard enabled (%1 min timeout)").arg(timeout_minutes()));
    } else {
        timer_->stop();
        LOG_DEBUG("Auth", "Inactivity guard disabled");
    }
}

void InactivityGuard::set_timeout_minutes(int minutes) {
    minutes = qBound(1, minutes, 60);
    timer_->setInterval(minutes * 60 * 1000);
    // QTimer::setInterval() does NOT re-arm a running single-shot timer — the
    // already-pending fire still uses the old interval. Restart so the new
    // value applies on the very next lock cycle, otherwise saving "60 min"
    // in Settings while the guard is running on the 10-min default leaves
    // the next lock to fire at the old 10-min mark.
    if (enabled_) {
        timer_->stop();
        timer_->start();
        last_activity_ = QDateTime::currentDateTime();
        LOG_DEBUG("Auth", QString("Inactivity timeout updated to %1 min").arg(minutes));
    }
}

int InactivityGuard::timeout_minutes() const {
    return timer_->interval() / 60000;
}

void InactivityGuard::set_terminal_locked(bool locked) {
    if (terminal_locked_ == locked)
        return;
    terminal_locked_ = locked;
    // The one lock/unlock line: this flag is the single source of truth (every window and
    // LockOverlayController follow it). reason=other: a lock at start-up or after sign-in.
    if (locked)
        LOG_INFO("Auth", logf("Terminal locked", {{"reason", s_inactivity_guard_lock_reason
                                                                 ? s_inactivity_guard_lock_reason
                                                                 : "other"}}));
    else
        LOG_INFO("Auth", "Terminal unlocked");
    s_inactivity_guard_lock_reason = nullptr;
    emit terminal_locked_changed(locked);
}

void InactivityGuard::reassert_lock() {
    if (terminal_locked_)
        emit terminal_locked_changed(true);
}

void InactivityGuard::reset_timer() {
    if (enabled_ && timer_)
        timer_->start(); // restart from full interval
    last_activity_ = QDateTime::currentDateTime();
}

bool InactivityGuard::check_for_resume_lock() {
    if (!enabled_)
        return false;
    if (!last_activity_.isValid()) {
        last_activity_ = QDateTime::currentDateTime();
        return false;
    }
    const qint64 elapsed_ms = last_activity_.msecsTo(QDateTime::currentDateTime());
    const int interval_ms = timer_->interval();
    if (elapsed_ms >= interval_ms) {
        LOG_DEBUG("Auth", QString("Resume detected — wall-clock elapsed %1ms >= timeout %2ms, forcing lock")
                              .arg(elapsed_ms)
                              .arg(interval_ms));
        s_inactivity_guard_lock_reason = "resume";
        SecurityAuditLog::instance().record("resume_lock",
                                            QString("elapsed_ms=%1 timeout_ms=%2").arg(elapsed_ms).arg(interval_ms));
        emit lock_requested();
        return true;
    }
    return false;
}

void InactivityGuard::trigger_manual_lock() {
    LOG_DEBUG("Auth", "Manual lock requested");
    s_inactivity_guard_lock_reason = "manual";
    SecurityAuditLog::instance().record("manual_lock");
    emit lock_requested();
}

void InactivityGuard::ensure_input_tracking() {
    if (input_tracking_ || !QCoreApplication::instance())
        return;
    // installEventFilter() never installs the same filter twice, so the
    // auto-lock path installing it again at unlock is harmless.
    QCoreApplication::instance()->installEventFilter(this);
    input_tracking_ = true;
    last_input_ms_ = QDateTime::currentMSecsSinceEpoch(); // starting the app counts as use
}

qint64 InactivityGuard::ms_since_input() const {
    if (!input_tracking_)
        return 0;
    return std::max<qint64>(0, QDateTime::currentMSecsSinceEpoch() - last_input_ms_);
}

bool InactivityGuard::user_active() const {
    if (terminal_locked_)
        return false;
    if (ms_since_input() >= idle_after_ms_)
        return false;
    // Minimised or hidden: nobody can see what a poll would refresh. A plain
    // QCoreApplication (tests, CLI) has no windows to look at.
    if (qobject_cast<QGuiApplication*>(QCoreApplication::instance())) {
        const auto windows = QGuiApplication::topLevelWindows();
        return std::any_of(windows.cbegin(), windows.cend(), [](const QWindow* w) {
            const Qt::WindowType type = w->type();
            const bool frame = type == Qt::Window || type == Qt::Dialog || type == Qt::Tool;
            return frame && w->isVisible() && !(w->windowStates() & Qt::WindowMinimized);
        });
    }
    return true;
}

bool InactivityGuard::eventFilter(QObject* obj, QEvent* event) {
    switch (event->type()) {
        case QEvent::MouseMove:
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonRelease:
        case QEvent::KeyPress:
        case QEvent::KeyRelease:
        case QEvent::Wheel:
        case QEvent::TouchBegin:
            last_input_ms_ = QDateTime::currentMSecsSinceEpoch();
            if (enabled_) {
                timer_->start(); // reset on any user interaction
                last_activity_ = QDateTime::currentDateTime();
            }
            break;
        case QEvent::ApplicationActivate:
            // Switching back to the terminal is use (for polling — not for the
            // auto-lock timer, which keeps its own rules). Sent before
            // applicationStateChanged(), so a refresh hooked to that signal
            // already sees the user as active.
            last_input_ms_ = QDateTime::currentMSecsSinceEpoch();
            break;
        case QEvent::ApplicationStateChange:
            if (static_cast<QApplicationStateChangeEvent*>(event)->applicationState() == Qt::ApplicationActive)
                last_input_ms_ = QDateTime::currentMSecsSinceEpoch();
            break;
        default:
            break;
    }
    return QObject::eventFilter(obj, event);
}

} // namespace fincept::auth
