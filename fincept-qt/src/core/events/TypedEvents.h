#pragma once
// Phase 10 scaffolding — typed event manifest over the stringly-typed EventBus.
//
// The string-keyed EventBus has 39 callsites today. Misspellings, dead
// publishes, and missing subscribers are silent bugs. `TypedEvents` adds a
// compile-time layer that keeps the runtime bus intact:
//
//   - One canonical name per event, defined here in `event_name()`.
//   - One typed payload struct per event.
//   - `publish<E>(payload)` and `on<E>(owner, slot)` enforce the right shape.
//
// The runtime bus is unchanged, so existing string-keyed `publish/subscribe`
// callsites continue working. Migration is opportunistic — touched code
// upgrades to the typed API.

#include "core/events/EventBus.h"

#include <QObject>
#include <QString>
#include <QVariantMap>

#include <functional>
#include <utility>

namespace fincept::events {

// ── Event payloads ───────────────────────────────────────────────────────────

struct NavSwitchScreen {
    QString screen_id;
    QVariantMap to_map() const { return {{"screen_id", screen_id}}; }
    static NavSwitchScreen from_map(const QVariantMap& m) {
        return {m.value(QStringLiteral("screen_id")).toString()};
    }
    static constexpr const char* name() { return "nav.switch_screen"; }
};

struct EquityResearchLoadSymbol {
    QString symbol;
    QString exchange;
    QVariantMap to_map() const {
        return {{"symbol", symbol}, {"exchange", exchange}};
    }
    static EquityResearchLoadSymbol from_map(const QVariantMap& m) {
        return {m.value(QStringLiteral("symbol")).toString(),
                m.value(QStringLiteral("exchange")).toString()};
    }
    static constexpr const char* name() { return "equity_research.load_symbol"; }
};

// ── Typed API ────────────────────────────────────────────────────────────────

/// Publish a typed event. Delegates to the existing string bus.
template <typename E>
inline void publish(const E& e) {
    EventBus::instance().publish(QString::fromUtf8(E::name()), e.to_map());
}

/// Subscribe to a typed event. The owner controls lifetime — when the owner is
/// destroyed, the subscription auto-cancels via `QObject::destroyed`.
///
/// Returns the underlying `HandlerId` for explicit unsubscribe; callers usually
/// rely on owner-destruction.
template <typename E, typename Slot>
inline EventBus::HandlerId on(QObject* owner, Slot slot) {
    auto& bus = EventBus::instance();
    auto handler = [slot = std::forward<Slot>(slot)](const QVariantMap& m) {
        slot(E::from_map(m));
    };
    EventBus::HandlerId id = bus.subscribe(QString::fromUtf8(E::name()), std::move(handler));
    if (owner) {
        QObject::connect(owner, &QObject::destroyed, &bus, [id] { EventBus::instance().unsubscribe(id); });
    }
    return id;
}

} // namespace fincept::events
