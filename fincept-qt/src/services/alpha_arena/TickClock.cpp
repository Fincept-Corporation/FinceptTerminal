#include "services/alpha_arena/TickClock.h"

#include "core/logging/Logger.h"

#include <QDateTime>

namespace fincept::services::alpha_arena {

TickClock::TickClock(QObject* parent) : QObject(parent), timer_(new QTimer(this)) {
    timer_->setTimerType(Qt::PreciseTimer);
    connect(timer_, &QTimer::timeout, this, &TickClock::on_timeout);
}

void TickClock::set_cadence_seconds(int seconds) {
    if (seconds < 10) seconds = 10;       // sanity floor
    if (seconds > 3600) seconds = 3600;   // sanity ceiling (1h)
    cadence_seconds_ = seconds;
    if (timer_->isActive()) {
        timer_->setInterval(cadence_seconds_ * 1000);
    }
}

void TickClock::start() {
    timer_->setInterval(cadence_seconds_ * 1000);
    timer_->start();
    LOG_INFO("AlphaArena.TickClock", QString("started, cadence=%1s").arg(cadence_seconds_));
}

void TickClock::stop() {
    timer_->stop();
    in_flight_ = false;
    LOG_INFO("AlphaArena.TickClock", "stopped");
}

void TickClock::tick_complete() {
    if (!in_flight_) return;
    in_flight_ = false;
    last_seq_ = next_seq_ - 1;  // next_seq_ was already advanced when we emitted
}

void TickClock::force_tick() {
    if (in_flight_) {
        emit tick_skipped(next_seq_);
        LOG_WARN("AlphaArena.TickClock",
                 QString("force_tick refused: tick %1 in flight").arg(next_seq_ - 1));
        return;
    }
    on_timeout();
}

void TickClock::on_timeout() {
    if (in_flight_) {
        LOG_WARN("AlphaArena.TickClock",
                 QString("tick_skipped seq=%1 (previous still in flight)").arg(next_seq_));
        emit tick_skipped(next_seq_);
        return;
    }

    Tick t;
    t.utc_ms = QDateTime::currentMSecsSinceEpoch();
    t.seq = next_seq_++;
    in_flight_ = true;
    emit tick(t);
}

} // namespace fincept::services::alpha_arena
