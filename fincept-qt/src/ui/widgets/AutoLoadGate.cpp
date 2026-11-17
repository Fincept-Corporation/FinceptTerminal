// src/ui/widgets/AutoLoadGate.cpp — see the header.

#include "ui/widgets/AutoLoadGate.h"

#include <QMetaObject>

namespace fincept::ui {

AutoLoadGate& AutoLoadGate::instance() {
    static AutoLoadGate gate;
    return gate;
}

void AutoLoadGate::set_capacity(int capacity) {
    queue_.set_capacity(capacity);
}

bool AutoLoadGate::enter(QObject* owner, std::function<void()> start_later, int priority, quint64* ticket) {
    const quint64 t = ++next_ticket_;
    if (ticket)
        *ticket = t;
    const bool now = queue_.request(t, priority);
    if (!now)
        entries_.insert(t, Entry{QPointer<QObject>(owner), std::move(start_later)});
    emit queue_changed();
    return now;
}

void AutoLoadGate::leave(quint64 ticket) {
    if (ticket == 0)
        return;
    entries_.remove(ticket);
    const QList<quint64> started = queue_.release(ticket);
    for (const quint64 t : started) {
        // Start out of the caller's stack: leave() is called from deliver()/fail()
        // handlers, and a start may answer synchronously.
        QMetaObject::invokeMethod(
            this,
            [this, t]() {
                const auto it = entries_.find(t);
                if (it == entries_.end())
                    return; // left meanwhile
                const Entry e = it.value();
                entries_.erase(it);
                if (!e.owner || !e.start_later) {
                    leave(t); // owner gone: hand the slot on
                    return;
                }
                e.start_later();
            },
            Qt::QueuedConnection);
    }
    emit queue_changed();
}

} // namespace fincept::ui
