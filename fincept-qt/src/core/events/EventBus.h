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

    HandlerId subscribe(const QString& event, Handler handler);
    void unsubscribe(HandlerId id);
    void publish(const QString& event, const QVariantMap& data = {});

  signals:
    void eventPublished(const QString& event, const QVariantMap& data);

  private:
    EventBus() = default;

    struct Subscription {
        HandlerId id;
        QString event;
        Handler handler;
    };

    QList<Subscription> subscriptions_;
    HandlerId next_id_ = 1;
    // Guards subscriptions_/next_id_. publish() may run on worker threads while
    // subscribe()/unsubscribe() run on the main thread (see EventBus.cpp).
    QMutex mutex_;
};

} // namespace fincept
