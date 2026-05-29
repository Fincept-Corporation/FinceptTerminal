// LatencyTracker — per-order latency monitoring (in-memory v1).
//
// TODO(persistence): mirror OpenAlgo's latency_db and persist completed
// OrderLatency records to SQLite (keep ORDER-type logs forever, auto-purge
// DATA-type logs after N days). For this pass we keep a bounded in-memory ring
// which is sufficient for live stats/reporting without DB I/O on the hot path.

#include "trading/LatencyTracker.h"

#include "core/logging/Logger.h"

#include <QMutexLocker>

#include <algorithm>
#include <cmath>

namespace fincept::trading {

LatencyTracker& LatencyTracker::instance() {
    static LatencyTracker s;
    return s;
}

void LatencyTracker::begin(const QString& tracking_id) {
    QMutexLocker lock(&mutex_);
    InFlight f;
    f.total.start();
    f.stage.start();
    in_flight_.insert(tracking_id, f);
}

void LatencyTracker::mark_validation_done(const QString& tracking_id) {
    QMutexLocker lock(&mutex_);
    auto it = in_flight_.find(tracking_id);
    if (it == in_flight_.end())
        return;
    // restart() returns elapsed-since-last-restart in ms and resets the timer.
    it->validation_ms = static_cast<double>(it->stage.restart());
}

void LatencyTracker::mark_broker_request_sent(const QString& tracking_id) {
    QMutexLocker lock(&mutex_);
    auto it = in_flight_.find(tracking_id);
    if (it == in_flight_.end())
        return;
    // Marks the start of the broker call — discard any time between validation
    // and the request actually firing.
    it->stage.restart();
}

void LatencyTracker::mark_broker_response_received(const QString& tracking_id) {
    QMutexLocker lock(&mutex_);
    auto it = in_flight_.find(tracking_id);
    if (it == in_flight_.end())
        return;
    it->broker_rtt_ms = static_cast<double>(it->stage.restart());
}

void LatencyTracker::mark_complete(const QString& tracking_id, const QString& order_id,
                                   const QString& account_id, const QString& broker,
                                   const QString& symbol, const QString& order_type,
                                   const QString& status, const QString& error) {
    QMutexLocker lock(&mutex_);
    auto it = in_flight_.find(tracking_id);
    if (it == in_flight_.end()) {
        LOG_WARN("LatencyTracker",
                 QString("mark_complete for unknown tracking_id: %1").arg(tracking_id));
        return;
    }

    OrderLatency entry;
    entry.order_id = order_id;
    entry.account_id = account_id;
    entry.broker = broker;
    entry.symbol = symbol;
    entry.order_type = order_type;
    entry.validation_ms = it->validation_ms;
    entry.broker_rtt_ms = it->broker_rtt_ms;
    entry.response_ms = static_cast<double>(it->stage.elapsed()); // since broker response received
    entry.total_ms = static_cast<double>(it->total.elapsed());
    entry.status = status;
    entry.error = error;
    entry.timestamp = QDateTime::currentDateTime();

    in_flight_.erase(it);

    history_.append(entry);
    while (history_.size() > kMaxHistory)
        history_.removeFirst();
}

QVector<LatencyTracker::OrderLatency> LatencyTracker::get_history(const QString& account_id,
                                                                  int limit) const {
    QMutexLocker lock(&mutex_);
    QVector<OrderLatency> out;
    if (limit <= 0)
        return out;

    // Walk newest-first, collect up to `limit` matches, then restore chronological order.
    for (int i = history_.size() - 1; i >= 0 && out.size() < limit; --i) {
        const OrderLatency& e = history_.at(i);
        if (!account_id.isEmpty() && e.account_id != account_id)
            continue;
        out.append(e);
    }
    std::reverse(out.begin(), out.end());
    return out;
}

LatencyTracker::LatencyStats LatencyTracker::get_stats(const QString& broker,
                                                       int last_n_hours) const {
    QMutexLocker lock(&mutex_);
    LatencyStats stats;

    const QDateTime cutoff =
        (last_n_hours > 0) ? QDateTime::currentDateTime().addSecs(-qint64(last_n_hours) * 3600)
                           : QDateTime();

    QVector<double> rtts;
    double sum_rtt = 0;
    double sum_total = 0;

    for (const OrderLatency& e : history_) {
        if (!broker.isEmpty() && e.broker != broker)
            continue;
        if (cutoff.isValid() && e.timestamp < cutoff)
            continue;

        ++stats.total_orders;
        if (e.status == QStringLiteral("FAILED"))
            ++stats.failed_orders;

        rtts.append(e.broker_rtt_ms);
        sum_rtt += e.broker_rtt_ms;
        sum_total += e.total_ms;
    }

    if (stats.total_orders == 0)
        return stats;

    stats.avg_rtt_ms = sum_rtt / stats.total_orders;
    stats.avg_total_ms = sum_total / stats.total_orders;

    std::sort(rtts.begin(), rtts.end());
    auto percentile = [&rtts](double p) -> double {
        if (rtts.isEmpty())
            return 0;
        // Nearest-rank percentile on the sorted copy.
        int idx = static_cast<int>(std::ceil(p / 100.0 * rtts.size())) - 1;
        if (idx < 0)
            idx = 0;
        if (idx >= rtts.size())
            idx = rtts.size() - 1;
        return rtts.at(idx);
    };
    stats.p50_rtt_ms = percentile(50);
    stats.p95_rtt_ms = percentile(95);
    stats.p99_rtt_ms = percentile(99);

    return stats;
}

} // namespace fincept::trading
