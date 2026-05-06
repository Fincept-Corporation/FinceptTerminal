#include "services/options/OISnapshotter.h"

#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "datahub/TopicPolicy.h"
#include "storage/repositories/OISnapshotsRepository.h"

#include <QDateTime>
#include <QVariant>
#include <QVector>

#include <algorithm>

namespace fincept::services::options {

namespace {

constexpr int kFlushIntervalMs = 60'000;       // 60s — minute-aligned writes
constexpr int kHousekeepingIntervalMs = 60 * 60'000; // hourly
constexpr int kRetentionDays = 7;
constexpr int kHistoryRequestsPerSec = 10;     // SQLite reads — generous

const QString kHistoryPrefix = QStringLiteral("oi:history:");

qint64 floor_to_minute_secs(qint64 ms) {
    qint64 secs = ms / 1000;
    return (secs / 60) * 60;
}

}  // namespace

OISnapshotter& OISnapshotter::instance() {
    static OISnapshotter s;
    return s;
}

OISnapshotter::OISnapshotter() {
    flush_timer_.setSingleShot(false);
    flush_timer_.setInterval(kFlushIntervalMs);
    connect(&flush_timer_, &QTimer::timeout, this, &OISnapshotter::flush_to_disk);

    housekeeping_timer_.setSingleShot(false);
    housekeeping_timer_.setInterval(kHousekeepingIntervalMs);
    connect(&housekeeping_timer_, &QTimer::timeout, this, &OISnapshotter::run_housekeeping);
}

void OISnapshotter::ensure_registered_with_hub() {
    if (registered_)
        return;
    auto& hub = fincept::datahub::DataHub::instance();
    hub.register_producer(this);

    fincept::datahub::TopicPolicy hist_pol;
    hist_pol.ttl_ms = kFlushIntervalMs;
    hist_pol.min_interval_ms = kFlushIntervalMs / 2;  // 30s — query is cheap, but no point flooding
    hub.set_policy_pattern(QStringLiteral("oi:history:*"), hist_pol);

    // Subscribe to every chain publish — feed the in-memory tick map.
    hub.subscribe_pattern(this, QStringLiteral("option:chain:*"),
                          [this](const QString& topic, const QVariant& v) { on_chain_published(topic, v); });

    flush_timer_.start();
    housekeeping_timer_.start();
    // Run a startup prune so the first session-of-the-day doesn't carry
    // last week's stale rows for too long.
    run_housekeeping();

    registered_ = true;
    LOG_INFO("OISnapshotter",
             QString("Registered with DataHub (oi:history:*) — flush every %1s, retention %2d")
                 .arg(kFlushIntervalMs / 1000).arg(kRetentionDays));
}

QString OISnapshotter::history_topic(const QString& broker_id, qint64 token, const QString& window) {
    return kHistoryPrefix + broker_id + ":" + QString::number(token) + ":" + window;
}

QStringList OISnapshotter::topic_patterns() const {
    return {QStringLiteral("oi:history:*")};
}

int OISnapshotter::max_requests_per_sec() const {
    return kHistoryRequestsPerSec;
}

void OISnapshotter::refresh(const QStringList& topics) {
    auto& hub = fincept::datahub::DataHub::instance();
    for (const auto& topic : topics) {
        QString broker;
        qint64 token = 0;
        QString window;
        if (!parse_history_topic(topic, broker, token, window)) {
            LOG_WARN("OISnapshotter", QString("malformed history topic: %1").arg(topic));
            hub.publish_error(topic, "malformed topic");
            continue;
        }
        const int days = days_for_window(window);
        const qint64 now_secs = QDateTime::currentSecsSinceEpoch();
        const qint64 since = now_secs - qint64(days) * 86400;
        auto r = OISnapshotsRepository::instance().get_window(token, since, now_secs);
        if (r.is_err()) {
            LOG_WARN("OISnapshotter", QString("get_window failed for %1: %2")
                                          .arg(topic)
                                          .arg(QString::fromStdString(r.error())));
            hub.publish_error(topic, QString::fromStdString(r.error()));
            continue;
        }
        const QVector<OISample> samples = r.value();
        hub.publish(topic, QVariant::fromValue(samples));
        LOG_DEBUG("OISnapshotter",
                  QString("Published %1 samples for %2 (window=%3)").arg(samples.size()).arg(topic).arg(window));
    }
}

void OISnapshotter::on_chain_published(const QString& /*topic*/, const QVariant& v) {
    if (!v.canConvert<OptionChain>())
        return;
    const OptionChain chain = v.value<OptionChain>();
    if (chain.timestamp_ms <= 0)
        return;
    const qint64 ts_minute = floor_to_minute_secs(chain.timestamp_ms);

    auto record_leg = [&](qint64 token, const fincept::trading::BrokerQuote& q, double iv) {
        if (token == 0)
            return;
        // Only treat the leg as "live" when there's at least an LTP. Empty
        // quotes (deep OTM with no recent trade) shouldn't pollute history.
        if (q.ltp <= 0 && q.oi <= 0)
            return;
        LastTick& lt = last_tick_[token];
        lt.sample.ts_minute = ts_minute;
        lt.sample.oi = q.oi;
        lt.sample.ltp = q.ltp;
        lt.sample.vol = qint64(q.volume);
        lt.sample.iv = iv;
        lt.dirty = true;
    };

    for (const auto& row : chain.rows) {
        record_leg(row.ce_token, row.ce_quote, row.ce_iv);
        record_leg(row.pe_token, row.pe_quote, row.pe_iv);
    }
}

void OISnapshotter::flush_to_disk() {
    if (last_tick_.isEmpty())
        return;

    // Group dirty entries by token so each token's row goes through one
    // upsert. The repository takes a per-token batch; in our case it's a
    // 1-element batch each, but the API generalises naturally to back-fill.
    int flushed = 0;
    auto& repo = OISnapshotsRepository::instance();
    for (auto it = last_tick_.begin(); it != last_tick_.end(); ++it) {
        LastTick& lt = it.value();
        if (!lt.dirty)
            continue;
        QVector<OISample> batch;
        batch.append(lt.sample);
        auto r = repo.upsert_batch(it.key(), batch);
        if (r.is_err()) {
            LOG_WARN("OISnapshotter", QString("upsert failed for token %1: %2")
                                          .arg(it.key())
                                          .arg(QString::fromStdString(r.error())));
            continue;
        }
        lt.dirty = false;
        ++flushed;
    }
    if (flushed > 0)
        LOG_DEBUG("OISnapshotter", QString("Flushed %1 token snapshots to oi_snapshots").arg(flushed));
}

void OISnapshotter::run_housekeeping() {
    const qint64 now_secs = QDateTime::currentSecsSinceEpoch();
    const qint64 cutoff = now_secs - qint64(kRetentionDays) * 86400;
    auto r = OISnapshotsRepository::instance().delete_older_than(cutoff);
    if (r.is_err()) {
        LOG_WARN("OISnapshotter", QString("housekeeping failed: %1").arg(QString::fromStdString(r.error())));
        return;
    }
    const int n = r.value();
    if (n > 0)
        LOG_INFO("OISnapshotter", QString("Pruned %1 oi_snapshots rows older than %2 days").arg(n).arg(kRetentionDays));
}

bool OISnapshotter::parse_history_topic(const QString& topic, QString& broker, qint64& token, QString& window) {
    if (!topic.startsWith(kHistoryPrefix))
        return false;
    const QString tail = topic.mid(kHistoryPrefix.size());
    const QStringList parts = tail.split(QLatin1Char(':'));
    if (parts.size() != 3)
        return false;
    broker = parts.at(0);
    bool ok = false;
    token = parts.at(1).toLongLong(&ok);
    window = parts.at(2);
    return ok && !broker.isEmpty() && !window.isEmpty();
}

int OISnapshotter::days_for_window(const QString& window) {
    if (window == QLatin1String("1d")) return 1;
    if (window == QLatin1String("5d")) return 5;
    if (window == QLatin1String("7d")) return 7;
    return 1;
}

} // namespace fincept::services::options
