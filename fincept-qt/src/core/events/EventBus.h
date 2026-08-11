#pragma once
#include <QHash>
#include <QList>
#include <QMutex>
#include <QObject>
#include <QVariantMap>

#include <functional>

namespace fincept {

/// Application-wide event bus using Qt signals.
/// Decouples screens from services — screens emit events, services listen.
class EventBus : public QObject {
    Q_OBJECT
  public:
    static EventBus& instance();

    using Handler = std::function<void(const QVariantMap&)>;
    using HandlerId = int;

    /// Subscribe for as long as `owner` lives.
    ///
    /// `owner` is mandatory: handlers capture their owner, and the bus is a
    /// process-lifetime singleton, so a handler outliving its owner is a
    /// use-after-free the next time anyone publishes that event. The
    /// destroyed() connection makes that impossible by construction — there is
    /// deliberately no ownerless overload, because every previous caller
    /// discarded the returned id and never unsubscribed.
    HandlerId subscribe(QObject* owner, const QString& event, Handler handler);

    /// Explicit early unsubscribe. Rarely needed — owner destruction is
    /// already handled — but useful to stop listening while staying alive.
    void unsubscribe(HandlerId id);

    /// Drop every subscription owned by `owner`. Safe to call more than once.
    void unsubscribe_owner(QObject* owner);

    void publish(const QString& event, const QVariantMap& data = {});

  signals:
    void eventPublished(const QString& event, const QVariantMap& data);

  private:
    EventBus() = default;

    struct Subscription {
        HandlerId id;
        QString event;
        Handler handler;
        QObject* owner = nullptr; // not owned; only compared, never dereferenced
    };

    QList<Subscription> subscriptions_;
    HandlerId next_id_ = 1;
    // Guards subscriptions_/next_id_. publish() may run on worker threads while
    // subscribe()/unsubscribe() run on the main thread (see EventBus.cpp).
    QMutex mutex_;
};

} // namespace fincept
