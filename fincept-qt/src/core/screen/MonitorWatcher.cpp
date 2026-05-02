#include "core/screen/MonitorWatcher.h"

#include "core/logging/Logger.h"
#include "core/screen/MonitorTopology.h"

#include <QGuiApplication>
#include <QScreen>

namespace fincept {

namespace {
constexpr int kDebounceMs = 500;
} // namespace

MonitorWatcher& MonitorWatcher::instance() {
    static MonitorWatcher s;
    return s;
}

MonitorWatcher::MonitorWatcher() {
    debounce_.setSingleShot(true);
    debounce_.setInterval(kDebounceMs);
    connect(&debounce_, &QTimer::timeout, this, &MonitorWatcher::on_debounce_fired);

    // Seed the cached topology before any consumer asks.
    current_topology_ = MonitorTopology::current_key();

    auto* app = QGuiApplication::instance();
    if (auto* gapp = qobject_cast<QGuiApplication*>(app)) {
        connect(gapp, &QGuiApplication::screenAdded, this,
                [this](QScreen*) { on_raw_screen_event(); });
        connect(gapp, &QGuiApplication::screenRemoved, this,
                [this](QScreen*) { on_raw_screen_event(); });
        connect(gapp, &QGuiApplication::primaryScreenChanged, this,
                [this](QScreen*) { on_raw_screen_event(); });
    }

    LOG_INFO("MonitorWatcher",
             QString("Initialised; topology=%1").arg(current_topology_.value));
}

void MonitorWatcher::on_raw_screen_event() {
    // Restart the debounce on every raw event so the timer only fires
    // 500 ms after the last event in a burst. Update the cached topology
    // immediately so a consumer asking via current_topology() during a
    // burst sees the latest snapshot.
    current_topology_ = MonitorTopology::current_key();
    debounce_.start();
}

void MonitorWatcher::on_debounce_fired() {
    // Re-read at the end of the debounce window — the raw events updated
    // the cache during the burst, but we want to be doubly sure on emit.
    const auto fresh = MonitorTopology::current_key();
    if (fresh.value != current_topology_.value)
        current_topology_ = fresh;
    LOG_INFO("MonitorWatcher",
             QString("Topology settled to: %1").arg(current_topology_.value));
    emit topology_changed(current_topology_);
}

} // namespace fincept
