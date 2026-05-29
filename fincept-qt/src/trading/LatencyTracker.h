#pragma once
// LatencyTracker — per-order latency monitoring for the trading layer.
//
// Tracks the timing of an order through its execution stages (validation ->
// broker round-trip -> response processing) and keeps an in-memory ring of
// completed orders for stats/reporting. Modelled on OpenAlgo's
// utils/latency_monitor.py (stage-based timing + RTT comparable to Postman).
//
// Usage (caller picks a unique tracking_id, e.g. a UUID or temp order ref):
//   auto& lt = LatencyTracker::instance();
//   lt.begin(tid);
//   ... run validation ...
//   lt.mark_validation_done(tid);
//   lt.mark_broker_request_sent(tid);
//   ... fire broker HTTP call ...
//   lt.mark_broker_response_received(tid);
//   ... process response ...
//   lt.mark_complete(tid, order_id, account_id, broker, symbol, "PLACE",
//                    success ? "SUCCESS" : "FAILED", error);
//
// All methods are thread-safe.

#include <QDateTime>
#include <QElapsedTimer>
#include <QHash>
#include <QMutex>
#include <QString>
#include <QVector>

namespace fincept::trading {

class LatencyTracker {
  public:
    struct OrderLatency {
        QString order_id;
        QString account_id;
        QString broker;
        QString symbol;
        QString order_type; // "PLACE","SMART","CANCEL","CLOSE_ALL","BASKET","SPLIT"
        double validation_ms = 0;
        double broker_rtt_ms = 0;
        double response_ms = 0;
        double total_ms = 0;
        QString status; // "SUCCESS","FAILED"
        QString error;
        QDateTime timestamp;
    };

    static LatencyTracker& instance();

    // Lightweight in-flight tracking keyed by a caller-chosen tracking_id.
    void begin(const QString& tracking_id);
    void mark_validation_done(const QString& tracking_id);
    void mark_broker_request_sent(const QString& tracking_id);
    void mark_broker_response_received(const QString& tracking_id);
    void mark_complete(const QString& tracking_id, const QString& order_id,
                       const QString& account_id, const QString& broker, const QString& symbol,
                       const QString& order_type, const QString& status, const QString& error = {});

    QVector<OrderLatency> get_history(const QString& account_id = {}, int limit = 100) const;

    struct LatencyStats {
        double avg_rtt_ms = 0, p50_rtt_ms = 0, p95_rtt_ms = 0, p99_rtt_ms = 0, avg_total_ms = 0;
        int total_orders = 0, failed_orders = 0;
    };
    LatencyStats get_stats(const QString& broker = {}, int last_n_hours = 24) const;

  private:
    LatencyTracker() = default;
    struct InFlight {
        QElapsedTimer total;
        QElapsedTimer stage;
        double validation_ms = 0;
        double broker_rtt_ms = 0;
    };
    QHash<QString, InFlight> in_flight_;
    QVector<OrderLatency> history_; // in-memory ring; cap at kMaxHistory entries
    mutable QMutex mutex_;
    static constexpr int kMaxHistory = 5000;
};

} // namespace fincept::trading
