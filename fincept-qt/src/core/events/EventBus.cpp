#include "core/events/EventBus.h"

#include "core/logging/Logger.h"

#include <QMutexLocker>

namespace fincept {

EventBus& EventBus::instance() {
    static EventBus s;
    return s;
}

EventBus::HandlerId EventBus::subscribe(QObject* owner, const QString& event, Handler handler) {
    if (!owner) {
        LOG_ERROR("EventBus", QString("subscribe('%1') called with a null owner — ignored").arg(event));
        return 0;
    }

    HandlerId id = 0;
    {
        QMutexLocker lock(&mutex_);
        id = next_id_++;
        subscriptions_.append({id, event, std::move(handler), owner});
    }

    // Connected outside the lock: destroyed() can fire on the owner's thread
    // while we hold mutex_, and unsubscribe() would then deadlock on it.
    // Qt::DirectConnection is deliberate — by the time a queued slot ran, the
    // owner would already be gone and a publish() in between would call the
    // dangling handler, which is the bug this whole overload exists to prevent.
    QObject::connect(owner, &QObject::destroyed, this, [this, id]() { unsubscribe(id); },
                     Qt::DirectConnection);
    return id;
}

void EventBus::unsubscribe(HandlerId id) {
    QMutexLocker lock(&mutex_);
    subscriptions_.removeIf([id](const Subscription& s) { return s.id == id; });
}

void EventBus::unsubscribe_owner(QObject* owner) {
    if (!owner)
        return;
    QMutexLocker lock(&mutex_);
    subscriptions_.removeIf([owner](const Subscription& s) { return s.owner == owner; });
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
