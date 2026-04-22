#include "auth/InactivityGuard.h"

#include "auth/SecurityAuditLog.h"
#include "core/logging/Logger.h"

#include <QEvent>

namespace fincept::auth {

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
            LOG_INFO("Auth", "Inactivity timeout — requesting lock");
            SecurityAuditLog::instance().record("inactivity_lock");
            emit lock_requested();
        }
    });
}

void InactivityGuard::set_enabled(bool enabled) {
    enabled_ = enabled;
    if (enabled) {
        timer_->start();
        LOG_INFO("Auth", QString("Inactivity guard enabled (%1 min timeout)").arg(timeout_minutes()));
    } else {
        timer_->stop();
        LOG_INFO("Auth", "Inactivity guard disabled");
    }
}

void InactivityGuard::set_timeout_minutes(int minutes) {
    minutes = qBound(1, minutes, 60);
    timer_->setInterval(minutes * 60 * 1000);
    if (enabled_ && !timer_->isActive())
        timer_->start();
}

int InactivityGuard::timeout_minutes() const {
    return timer_->interval() / 60000;
}

void InactivityGuard::reset_timer() {
    if (enabled_ && timer_)
        timer_->start(); // restart from full interval
    last_activity_ = QDateTime::currentDateTime();
}

bool InactivityGuard::check_for_resume_lock() {
    if (!enabled_) return false;
    if (!last_activity_.isValid()) {
        last_activity_ = QDateTime::currentDateTime();
        return false;
    }
    const qint64 elapsed_ms = last_activity_.msecsTo(QDateTime::currentDateTime());
    const int interval_ms = timer_->interval();
    if (elapsed_ms >= interval_ms) {
        LOG_INFO("Auth", QString("Resume detected — wall-clock elapsed %1ms >= timeout %2ms, forcing lock")
                             .arg(elapsed_ms).arg(interval_ms));
        SecurityAuditLog::instance().record(
            "resume_lock", QString("elapsed_ms=%1 timeout_ms=%2").arg(elapsed_ms).arg(interval_ms));
        emit lock_requested();
        return true;
    }
    return false;
}

void InactivityGuard::trigger_manual_lock() {
    LOG_INFO("Auth", "Manual lock requested");
    SecurityAuditLog::instance().record("manual_lock");
    emit lock_requested();
}

bool InactivityGuard::eventFilter(QObject* obj, QEvent* event) {
    if (enabled_) {
        switch (event->type()) {
            case QEvent::MouseMove:
            case QEvent::MouseButtonPress:
            case QEvent::MouseButtonRelease:
            case QEvent::KeyPress:
            case QEvent::KeyRelease:
            case QEvent::Wheel:
            case QEvent::TouchBegin:
                timer_->start(); // reset on any user interaction
                last_activity_ = QDateTime::currentDateTime();
                break;
            default:
                break;
        }
    }
    return QObject::eventFilter(obj, event);
}

} // namespace fincept::auth
