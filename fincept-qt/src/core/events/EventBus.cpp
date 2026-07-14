#include "core/events/EventBus.h"

#include <QMutexLocker>

namespace fincept {

EventBus& EventBus::instance() {
    static EventBus s;
    return s;
}

EventBus::HandlerId EventBus::subscribe(const QString& event, Handler handler) {
    QMutexLocker lock(&mutex_);
    HandlerId id = next_id_++;
    subscriptions_.append({id, event, std::move(handler)});
    return id;
}

void EventBus::unsubscribe(HandlerId id) {
    QMutexLocker lock(&mutex_);
    subscriptions_.removeIf([id](const Subscription& s) { return s.id == id; });
}

void EventBus::publish(const QString& event, const QVariantMap& data) {
    emit eventPublished(event, data);

    // Snapshot the matching handlers under the lock, then invoke them OUTSIDE the
    // lock. publish() can run on worker threads (e.g. a workflow "MCP Tool" node
    // executing on a QtConcurrent pool thread) while subscribe()/unsubscribe()
    // mutate subscriptions_ on the main thread — iterating the live QList raced a
    // reallocation → use-after-free. Invoking outside the lock also lets a
    // handler safely (un)subscribe without deadlocking or invalidating the loop.
    QList<Handler> matched;
    {
        QMutexLocker lock(&mutex_);
        for (const auto& sub : subscriptions_) {
            if (sub.event == event)
                matched.append(sub.handler);
        }
    }
    for (const auto& handler : matched)
        handler(data);
}

} // namespace fincept
