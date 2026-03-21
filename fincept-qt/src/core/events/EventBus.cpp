#include "core/events/EventBus.h"

namespace fincept {

EventBus& EventBus::instance() {
    static EventBus s;
    return s;
}

EventBus::HandlerId EventBus::subscribe(const QString& event, Handler handler) {
    HandlerId id = next_id_++;
    subscriptions_.append({id, event, std::move(handler)});
    return id;
}

void EventBus::unsubscribe(HandlerId id) {
    subscriptions_.removeIf([id](const Subscription& s) { return s.id == id; });
}

void EventBus::publish(const QString& event, const QVariantMap& data) {
    emit eventPublished(event, data);
    for (const auto& sub : subscriptions_) {
        if (sub.event == event) {
            sub.handler(data);
        }
    }
}

} // namespace fincept
