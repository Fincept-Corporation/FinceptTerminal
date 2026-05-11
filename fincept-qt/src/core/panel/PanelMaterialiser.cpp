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

void PanelMaterialiser::enqueue(const QString& id, std::function<void()> work, int priority,
                                QObject* owner) {
    if (!work)
        return;
    Item item;
    item.id = id;
    item.work = std::move(work);
    if (owner) {
        item.owner = QPointer<QObject>(owner);
        item.has_owner = true;
    }
    switch (priority) {
        case 0:  q_priority_0_.enqueue(std::move(item)); break;
        case 1:  q_priority_1_.enqueue(std::move(item)); break;
        default: q_priority_2_.enqueue(std::move(item)); break;
    }
    if (!tick_timer_.isActive())
        tick_timer_.start();
}

void PanelMaterialiser::cancel_for(QObject* owner) {
    if (!owner)
        return;
    auto drop_matching = [owner](QQueue<Item>& q) {
        if (q.isEmpty())
            return;
        QQueue<Item> kept;
        kept.reserve(q.size());
        while (!q.isEmpty()) {
            Item it = q.dequeue();
            if (!it.has_owner || it.owner.data() != owner)
                kept.enqueue(std::move(it));
        }
        q = std::move(kept);
    };
    drop_matching(q_priority_0_);
    drop_matching(q_priority_1_);
    drop_matching(q_priority_2_);
    if (pending_count() == 0)
        tick_timer_.stop();
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

    // Run an Item from `q`, decrementing budget. Items whose owner has
    // been destroyed are silently dropped without consuming budget so
    // dead items don't slow real materialisation.
    auto run_one = [&budget](QQueue<Item>& q) {
        while (!q.isEmpty()) {
            Item it = q.dequeue();
            if (it.has_owner && !it.owner) {
                LOG_DEBUG(kMatTag, QString("Drop dead-owner item '%1'").arg(it.id));
                continue;
            }
            if (it.work)
                it.work();
            --budget;
            return; // exit inner while; outer loop re-enters until budget hits 0
        }
    };

    while (budget > 0 && !q_priority_0_.isEmpty())
        run_one(q_priority_0_);
    while (budget > 0 && !q_priority_1_.isEmpty())
        run_one(q_priority_1_);
    while (budget > 0 && !q_priority_2_.isEmpty())
        run_one(q_priority_2_);

    if (pending_count() == 0) {
        tick_timer_.stop();
        LOG_DEBUG(kMatTag, "Queue drained");
        emit queue_drained();
    }
}

} // namespace fincept
