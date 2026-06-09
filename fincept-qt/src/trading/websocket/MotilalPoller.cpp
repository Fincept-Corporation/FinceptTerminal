// MotilalPoller — periodic REST-quote poller masquerading as a streaming adapter.
//
// Reference: broker/motilal/streaming/motilal_adapter.py (which polls the
// MotilalWebSocket client's internal quote dictionaries on a 500ms cadence).
// Here there is no usable real-time feed, so we poll MotilalBroker::get_quotes
// directly and fan each BrokerQuote out via tick_received.

#include "trading/websocket/MotilalPoller.h"

#include "core/logging/Logger.h"
#include "trading/BrokerRegistry.h"

#include <QDateTime>
#include <QMetaObject>
#include <QtConcurrent/QtConcurrent>

namespace fincept::trading {

static constexpr const char* TAG_MO_POLL = "MotilalPoller";

MotilalPoller::MotilalPoller(const BrokerCredentials& creds, QObject* parent)
    : BrokerWebSocketBase(parent), creds_(creds) {
    // P3: never start the timer in the constructor — only configure it. open()
    // starts it; close() stops it.
    poll_timer_.setInterval(kPollIntervalMs);
    connect(&poll_timer_, &QTimer::timeout, this, &MotilalPoller::poll_once);
}

// ─────────────────────────────────────────────────────────────────────────────
// Lifecycle
// ─────────────────────────────────────────────────────────────────────────────

void MotilalPoller::open() {
    if (poll_timer_.isActive())
        return;
    LOG_INFO(TAG_MO_POLL, "Starting Motilal quote polling");
    poll_timer_.start();
    start_health_check();
    emit connected();
    // Kick an immediate first poll so consumers don't wait a full interval.
    if (!symbols_.isEmpty())
        poll_once();
}

void MotilalPoller::close() {
    if (!poll_timer_.isActive())
        return;
    LOG_INFO(TAG_MO_POLL, "Stopping Motilal quote polling");
    poll_timer_.stop();
    stop_health_check();
    emit disconnected();
}

bool MotilalPoller::is_connected() const {
    return poll_timer_.isActive();
}

// ─────────────────────────────────────────────────────────────────────────────
// Subscription
// ─────────────────────────────────────────────────────────────────────────────

void MotilalPoller::subscribe(const QVector<qint64>& tokens) {
    // Motilal quotes are symbol-keyed, so numeric tokens carry no meaning here.
    Q_UNUSED(tokens);
    LOG_DEBUG(TAG_MO_POLL, "Token-based subscribe ignored — use subscribe(QStringList)");
}

void MotilalPoller::subscribe(const QStringList& symbols) {
    QSet<QString> set(symbols_.begin(), symbols_.end());
    for (const QString& s : symbols) {
        if (!s.isEmpty())
            set.insert(s);
    }
    symbols_ = QStringList(set.begin(), set.end());
    LOG_INFO(TAG_MO_POLL, QString("Subscribed %1 symbols").arg(symbols_.size()));
}

void MotilalPoller::unsubscribe() {
    symbols_.clear();
    clear_tick_cache();
}

// ─────────────────────────────────────────────────────────────────────────────
// Polling
// ─────────────────────────────────────────────────────────────────────────────

void MotilalPoller::poll_once() {
    if (symbols_.isEmpty())
        return;

    // Skip if a previous fetch is still running — Motilal's REST endpoint can be
    // slower than the 200ms cadence, and we must not stack overlapping calls.
    bool expected = false;
    if (!fetching_.compare_exchange_strong(expected, true))
        return;

    const BrokerCredentials creds = creds_;
    const QVector<QString> symbols(symbols_.begin(), symbols_.end());

    // P8: QPointer guard — the poller may be destroyed before the worker
    // lambda runs; bail out instead of dereferencing a dangling this.
    QPointer<MotilalPoller> self = this;
    (void)QtConcurrent::run([self, creds, symbols]() {
        IBroker* broker = BrokerRegistry::instance().get("motilal");
        ApiResponse<QVector<BrokerQuote>> resp;
        if (broker)
            resp = broker->get_quotes(creds, symbols);
        else
            resp.error = QStringLiteral("motilal broker not registered");

        if (!self) {
            return; // poller gone — nothing to deliver
        }

        QMetaObject::invokeMethod(
            self,
            [self, resp]() {
                if (!self)
                    return;
                self->fetching_.store(false);

                if (!resp.success || !resp.data.has_value()) {
                    if (!resp.error.isEmpty())
                        LOG_WARN(TAG_MO_POLL, QString("Quote poll failed: %1").arg(resp.error));
                    return;
                }

                for (BrokerQuote q : resp.data.value()) {
                    self->note_tick();
                    // OHLC carryforward — preserve last non-zero values.
                    BrokerQuote merged = self->merge_tick(q.symbol, q);
                    if (merged.timestamp == 0)
                        merged.timestamp = QDateTime::currentMSecsSinceEpoch();
                    emit self->tick_received(merged);
                }
            },
            Qt::QueuedConnection);
    });
}

} // namespace fincept::trading
