#pragma once
// MotilalPoller — Motilal Oswal market-data adapter (REST polling, not a WS).
//
// Motilal Oswal's market-data WebSocket uses an opaque binary protocol that the
// OpenAlgo reference can only partially decode, and it provides no reliable
// real-time push for most field types. OpenAlgo's adapter therefore polls the
// REST quote endpoint on a fixed cadence. We replicate that here: a QTimer ticks
// every kPollIntervalMs and triggers a (background) REST fetch via the registered
// MotilalBroker, emitting tick_received(BrokerQuote) for each quote.
//
// This subclasses BrokerWebSocketBase so AccountDataStream can treat it like any
// other streaming adapter (same tick_received / depth_received / connected
// signals), even though no socket is involved. is_connected() reflects whether
// the poll timer is running.
//
// Threading: the REST call is wrapped in QtConcurrent::run with a QPointer guard
// (P8) so a slow/blocking broker call never freezes the UI thread, and results
// are posted back via QMetaObject::invokeMethod (Qt::QueuedConnection).

#include "trading/TradingTypes.h"
#include "trading/websocket/BrokerWebSocketBase.h"

#include <QPointer>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVector>

#include <atomic>

namespace fincept::trading {

class MotilalPoller : public BrokerWebSocketBase {
    Q_OBJECT
  public:
    /// creds: stored so the poller can call MotilalBroker::get_quotes itself.
    explicit MotilalPoller(const BrokerCredentials& creds, QObject* parent = nullptr);

    void open() override;  // starts the poll timer
    void close() override; // stops the poll timer
    bool is_connected() const override;

    /// Token-based subscribe is part of the base contract but Motilal quotes are
    /// keyed by symbol, so this is a no-op kept for interface compatibility.
    void subscribe(const QVector<qint64>& tokens) override;
    void unsubscribe() override;

    /// Symbol-based subscription — this is the API callers should use.
    void subscribe(const QStringList& symbols);

  private slots:
    void poll_once();

  private:
    BrokerCredentials creds_;
    QTimer poll_timer_;
    QStringList symbols_;

    // Guards against overlapping fetches when a poll takes longer than the
    // interval (set true while a background fetch is in flight).
    std::atomic<bool> fetching_{false};

    static constexpr int kPollIntervalMs = 200;
};

} // namespace fincept::trading
