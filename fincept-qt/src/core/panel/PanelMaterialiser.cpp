#include "core/panel/PanelMaterialiser.h"

#include "core/logging/Logger.h"

namespace fincept {

namespace {
constexpr const char* kMatTag = "PanelMaterialiser";
} // namespace

PanelMaterialiser& PanelMaterialiser::instance() {
    static PanelMaterialiser s;
    return s;
}

PanelMaterialiser::PanelMaterialiser() {
    tick_timer_.setSingleShot(false);
    tick_timer_.setInterval(kDefaultTickIntervalMs);
    connect(&tick_timer_, &QTimer::timeout, this, &PanelMaterialiser::tick);
}

void PanelMaterialiser::enqueue(const QString& id, std::function<void()> work, int priority) {
    if (!work)
        return;
    Item item{id, std::move(work)};
    switch (priority) {
        case 0:  q_priority_0_.enqueue(std::move(item)); break;
        case 1:  q_priority_1_.enqueue(std::move(item)); break;
        default: q_priority_2_.enqueue(std::move(item)); break;
    }
    if (!tick_timer_.isActive())
        tick_timer_.start();
}

void PanelMaterialiser::drain_all_now() {
    auto run_all = [](QQueue<Item>& q) {
        while (!q.isEmpty()) {
            Item it = q.dequeue();
            if (it.work) it.work();
        }
    };
    run_all(q_priority_0_);
    run_all(q_priority_1_);
    run_all(q_priority_2_);
    tick_timer_.stop();
    emit queue_drained();
}

int PanelMaterialiser::pending_count() const {
    return q_priority_0_.size() + q_priority_1_.size() + q_priority_2_.size();
}

void PanelMaterialiser::set_tick_interval_ms(int ms) {
    if (ms < 1) ms = 1;
    tick_timer_.setInterval(ms);
}

void PanelMaterialiser::set_max_per_tick(int n) {
    if (n < 1) n = 1;
    max_per_tick_ = n;
}

void PanelMaterialiser::tick() {
    int budget = max_per_tick_;

    // Drain priority 0 first — foreground tabs are what the user actually
    // sees, so spending the entire tick on them is fine.
    while (budget > 0 && !q_priority_0_.isEmpty()) {
        Item it = q_priority_0_.dequeue();
        if (it.work) it.work();
        --budget;
    }
    while (budget > 0 && !q_priority_1_.isEmpty()) {
        Item it = q_priority_1_.dequeue();
        if (it.work) it.work();
        --budget;
    }
    while (budget > 0 && !q_priority_2_.isEmpty()) {
        Item it = q_priority_2_.dequeue();
        if (it.work) it.work();
        --budget;
    }

    if (pending_count() == 0) {
        tick_timer_.stop();
        LOG_DEBUG(kMatTag, "Queue drained");
        emit queue_drained();
    }
}

} // namespace fincept
