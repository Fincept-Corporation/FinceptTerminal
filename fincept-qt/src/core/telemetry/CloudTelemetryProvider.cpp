#include "core/telemetry/CloudTelemetryProvider.h"

#include "core/logging/Logger.h"
#include "storage/repositories/SettingsRepository.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QMutexLocker>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>

namespace fincept::telemetry {

namespace {
constexpr const char* kCloudTag = "CloudTelemetry";
} // namespace

CloudTelemetryProvider::CloudTelemetryProvider(TelemetryProvider* inner_sink, QObject* parent)
    : QObject(parent), inner_sink_(inner_sink) {
}

CloudTelemetryProvider::~CloudTelemetryProvider() {
    stop();
}

void CloudTelemetryProvider::start() {
    if (started_)
        return;
    if (!nam_)
        nam_ = new QNetworkAccessManager(this);
    if (!flush_timer_) {
        flush_timer_ = new QTimer(this);
        flush_timer_->setInterval(kFlushIntervalMs);
        connect(flush_timer_, &QTimer::timeout, this, &CloudTelemetryProvider::on_flush_tick);
    }
    flush_timer_->start();
    started_ = true;
    LOG_INFO(kCloudTag, "Cloud telemetry uploader started");
}

void CloudTelemetryProvider::stop() {
    if (!started_)
        return;
    if (flush_timer_)
        flush_timer_->stop();
    // Abort any in-flight POSTs so their finished-lambdas don't fire
    // against a half-destroyed provider during static teardown. The
    // QObject context (this) we passed to connect() also auto-disconnects
    // when this is destroyed, but aborting first lets QNetworkAccessManager
    // tear down cleanly without queued retries from socket-error paths.
    if (nam_) {
        const auto replies = nam_->findChildren<QNetworkReply*>();
        for (auto* r : replies) {
            if (r && r->isRunning())
                r->abort();
        }
    }
    started_ = false;
    LOG_INFO(kCloudTag, "Cloud telemetry uploader stopped");
}

void CloudTelemetryProvider::record(const QString& event_id, const QVariantMap& payload) {
    // Always tee to the inner sink first so a network outage doesn't
    // drop the durable on-disk record. The inner sink is expected to
    // be cheap (LocalTelemetrySink writes one row).
    if (inner_sink_)
        inner_sink_->record(event_id, payload);

    QJsonObject obj;
    for (auto it = payload.constBegin(); it != payload.constEnd(); ++it)
        obj.insert(it.key(), QJsonValue::fromVariant(it.value()));

    Event e;
    e.ts_ms = QDateTime::currentMSecsSinceEpoch();
    e.event_id = event_id;
    e.payload_json = QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));

    QMutexLocker lock(&buffer_mutex_);
    if (buffer_.size() >= kMaxBuffered) {
        // Drop the oldest events (FIFO overflow). Telemetry is best-effort
        // — we'd rather lose old events than block record() on a slow
        // network or unbounded memory growth.
        const int drop = buffer_.size() - kMaxBuffered + 1;
        buffer_.remove(0, drop);
    }
    buffer_.append(std::move(e));
}

int CloudTelemetryProvider::pending_count() const {
    QMutexLocker lock(&buffer_mutex_);
    return buffer_.size();
}

void CloudTelemetryProvider::on_flush_tick() {
    if (next_attempt_ms_ != 0 && QDateTime::currentMSecsSinceEpoch() < next_attempt_ms_)
        return; // still in backoff window

    QString endpoint, api_key;
    if (!read_config(endpoint, api_key))
        return; // unset → uploader idle, events keep buffering

    QVector<Event> raw;
    QJsonArray body;
    {
        QMutexLocker lock(&buffer_mutex_);
        if (buffer_.isEmpty())
            return;
        const int n = qMin(kBatchSize, buffer_.size());
        raw.reserve(n);
        for (int i = 0; i < n; ++i) {
            const Event& e = buffer_[i];
            QJsonObject row;
            row["ts_ms"] = e.ts_ms;
            row["event_id"] = e.event_id;
            // payload_json is already a serialised string — re-parse so
            // it lands as a structured object in the array, not an
            // escaped string.
            QJsonDocument doc = QJsonDocument::fromJson(e.payload_json.toUtf8());
            row["payload"] = doc.isObject() ? QJsonValue(doc.object()) : QJsonValue(e.payload_json);
            body.append(row);
            raw.append(e);
        }
        buffer_.remove(0, n);
    }

    post_batch(endpoint, api_key, body, std::move(raw));
}

void CloudTelemetryProvider::requeue_batch_front(QVector<Event> events) {
    if (events.isEmpty())
        return;
    QMutexLocker lock(&buffer_mutex_);
    // Insert at the front so retried events keep their original order.
    // If we've overflowed since the failure, the oldest of the requeued
    // batch may be dropped to honour kMaxBuffered — that's acceptable.
    QVector<Event> merged;
    merged.reserve(events.size() + buffer_.size());
    merged.append(events);
    merged.append(buffer_);
    if (merged.size() > kMaxBuffered)
        merged.remove(0, merged.size() - kMaxBuffered);
    buffer_ = std::move(merged);
}

bool CloudTelemetryProvider::read_config(QString& endpoint_out, QString& api_key_out) const {
    auto endpoint_r = SettingsRepository::instance().get("telemetry.cloud_endpoint");
    if (endpoint_r.is_err() || endpoint_r.value().trimmed().isEmpty())
        return false;
    endpoint_out = endpoint_r.value().trimmed();
    if (!endpoint_out.startsWith("http://") && !endpoint_out.startsWith("https://"))
        return false; // bad config; refuse rather than silently use raw text

    auto key_r = SettingsRepository::instance().get("telemetry.cloud_api_key");
    api_key_out = key_r.is_ok() ? key_r.value().trimmed() : QString{};
    return true;
}

void CloudTelemetryProvider::post_batch(const QString& endpoint, const QString& api_key,
                                        const QJsonArray& body,
                                        QVector<Event> raw_for_requeue) {
    QNetworkRequest req((QUrl(endpoint)));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setHeader(QNetworkRequest::UserAgentHeader, "FinceptTerminal/Telemetry");
    if (!api_key.isEmpty())
        req.setRawHeader("Authorization", QByteArray("Bearer ") + api_key.toUtf8());

    QJsonObject envelope;
    envelope["events"] = body;
    envelope["batch_size"] = body.size();
    const QByteArray payload = QJsonDocument(envelope).toJson(QJsonDocument::Compact);

    QNetworkReply* reply = nam_->post(req, payload);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, raw_for_requeue = std::move(raw_for_requeue)]() mutable {
                reply->deleteLater();
                const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                const auto err = reply->error();

                if (err == QNetworkReply::NoError && status >= 200 && status < 300) {
                    // Success: clear backoff, mark healthy.
                    healthy_.store(true);
                    if (backoff_seconds_ != 0) {
                        LOG_INFO(kCloudTag, "Recovered; backoff cleared");
                        backoff_seconds_ = 0;
                        next_attempt_ms_ = 0;
                    }
                    return;
                }

                if (err == QNetworkReply::NoError && status >= 400 && status < 500) {
                    // 4xx — caller-side bug (auth, malformed payload). Drop
                    // the batch; retrying won't help. Don't toggle health
                    // since /health is "is the upstream reachable" not
                    // "did we send the right thing".
                    LOG_WARN(kCloudTag,
                             QString("Drop batch (status %1, %2 events)")
                                 .arg(status).arg(raw_for_requeue.size()));
                    return;
                }

                // Network error or 5xx — requeue + grow backoff.
                healthy_.store(false);
                requeue_batch_front(std::move(raw_for_requeue));
                if (backoff_seconds_ == 0)
                    backoff_seconds_ = 5; // first hit
                else
                    backoff_seconds_ = qMin(kMaxBackoffSeconds, backoff_seconds_ * 2);
                next_attempt_ms_ = QDateTime::currentMSecsSinceEpoch()
                                   + qint64(backoff_seconds_) * 1000;
                LOG_WARN(kCloudTag,
                         QString("Upload failed (status=%1, err=%2); backing off %3s")
                             .arg(status).arg(err).arg(backoff_seconds_));
            });
}

} // namespace fincept::telemetry
