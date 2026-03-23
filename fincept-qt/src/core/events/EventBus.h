#pragma once
#include <QHash>
#include <QList>
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
};

} // namespace fincept
