#include "core/debug/StressLoad.h"

#include "app/WindowFrame.h"
#include "core/logging/Logger.h"
#include "core/window/WindowRegistry.h"
#include "datahub/DataHub.h"

#include <QDateTime>
#include <QString>
#include <QVariant>

#include <algorithm>

namespace fincept::debug_tools {

namespace {
constexpr const char* kStressTag = "StressLoad";
} // namespace

StressLoad& StressLoad::instance() {
    static StressLoad s;
    return s;
}

StressLoad::StressLoad() {
    tick_timer_.setSingleShot(false);
    connect(&tick_timer_, &QTimer::timeout, this, &StressLoad::on_tick);
}

bool StressLoad::start(int n_frames, int m_panels_per_frame, int k_ticks_per_sec) {
    if (running_)
        stop();

    n_frames_ = std::clamp(n_frames, 1, 16);
    m_panels_ = std::clamp(m_panels_per_frame, 1, 64);
    k_ticks_per_sec_ = std::clamp(k_ticks_per_sec, 1, 10000);

    LOG_INFO(kStressTag, QString("Starting stress: %1 frames × %2 panels, %3 ticks/sec")
                             .arg(n_frames_).arg(m_panels_).arg(k_ticks_per_sec_));

    // Spawn frames beyond what already exists. Each spawn registers with
    // WindowRegistry automatically via WindowFrame's constructor.
    const int existing = WindowRegistry::instance().frame_count();
    for (int i = existing; i < n_frames_; ++i) {
        auto* w = new WindowFrame(WindowFrame::next_window_id());
        w->setAttribute(Qt::WA_DeleteOnClose);
        spawned_window_ids_.append(w->window_id());
        w->show();
    }

    // Panel materialisation in frames: drive via the existing route. We
    // can't easily open M arbitrary panels without enumerating registered
    // factories; for v1 the harness focuses on DataHub fanout pressure
    // rather than panel-spawn cost. Phase 6's full layout-load benchmark
    // is the right place for "open 50 panels" — see PanelMaterialiser tick.

    // Tick interval: 1000ms / K. At K=1 → 1000ms (one tick/sec).
    const int interval_ms = std::max(1, 1000 / k_ticks_per_sec_);
    tick_timer_.setInterval(interval_ms);
    total_ticks_ = 0;
    running_ = true;
    tick_timer_.start();
    return true;
}

void StressLoad::stop() {
    if (!running_)
        return;
    tick_timer_.stop();
    close_spawned_frames();
    LOG_INFO(kStressTag, QString("Stopped after %1 total ticks").arg(total_ticks_));
    running_ = false;
}

void StressLoad::on_tick() {
    // Fan a synthetic publish across N×M topics — each frame's slice gets
    // its own band so DataHub's fanout exercises the full subscriber map.
    auto& hub = datahub::DataHub::instance();
    const qint64 now_ms = QDateTime::currentMSecsSinceEpoch();
    for (int f = 0; f < n_frames_; ++f) {
        for (int p = 0; p < m_panels_; ++p) {
            const QString topic = QString("stress:tick:%1:%2").arg(f).arg(p);
            // Payload: a small map with timestamp + sequence. Cheap to
            // serialise; mimics the shape of real tick data.
            QVariantMap v;
            v.insert("ts_ms", now_ms);
            v.insert("seq", total_ticks_);
            hub.publish(topic, QVariant::fromValue(v));
        }
    }
    ++total_ticks_;
}

void StressLoad::close_spawned_frames() {
    // Close frames the harness created. Other (user) frames are untouched.
    const auto frames = WindowRegistry::instance().frames();
    for (WindowFrame* w : frames) {
        if (!w) continue;
        if (spawned_window_ids_.contains(w->window_id()))
            w->close();
    }
    spawned_window_ids_.clear();
}

} // namespace fincept::debug_tools
