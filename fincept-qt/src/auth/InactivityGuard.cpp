#include "auth/InactivityGuard.h"

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

    connect(timer_, &QTimer::timeout, this, [this]() {
        if (enabled_) {
            LOG_INFO("Auth", "Inactivity timeout — requesting lock");
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
                break;
            default:
                break;
        }
    }
    return QObject::eventFilter(obj, event);
}

} // namespace fincept::auth
