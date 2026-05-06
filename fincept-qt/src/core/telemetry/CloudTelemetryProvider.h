#pragma once
#include "core/telemetry/TelemetryProvider.h"

#include <QDateTime>
#include <QJsonArray>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QVector>

#include <atomic>

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

namespace fincept::telemetry {

/// Phase 10 / decision 10.4: opt-in cloud telemetry uploader.
///
/// Wraps a `LocalTelemetrySink` so events are always durable on disk
/// (in workspace.db) regardless of network state, and additionally
/// batches them for HTTPS POST to a user-configured endpoint.
///
/// **Endpoint config** — read from `SettingsRepository`:
///   - `telemetry.cloud_endpoint` — full URL (empty = uploader idle)
///   - `telemetry.cloud_api_key`  — sent as `Authorization: Bearer <key>`
///
/// **Privacy contract** (decision 10.4): caller is responsible for
/// what's in payloads. The provider does NOT inspect payloads beyond
/// JSON-encoding them. Action invocations + counts only — no symbols,
/// credentials, or account-id payloads.
///
/// **Buffering**: in-memory queue capped at `kMaxBuffered` (5000).
/// Overflow drops oldest events (record() takes the mutex briefly).
/// On every flush tick, up to `kBatchSize` (50) events are POSTed in a
/// single request body.
///
/// **Failure mode**: 4xx responses log + drop the batch (caller bug;
/// retrying won't help). 5xx and network errors trigger exponential
/// backoff capped at `kMaxBackoffSeconds` (300). On backoff resume, the
/// queue keeps draining; events that arrived during the outage are
/// delivered in order.
///
/// **Threading**: `record()` is callable from any thread (mutex-guarded
/// enqueue). Network I/O runs on the thread that owns the provider —
/// typically the UI thread, where `QNetworkAccessManager` is created.
class CloudTelemetryProvider : public QObject, public TelemetryProvider {
    Q_OBJECT
  public:
    /// Construct. The provider is dormant until `start()` is called.
    /// Pass an inner sink (e.g. LocalTelemetrySink*) to chain durable
    /// local persistence; nullptr = upload-only.
    explicit CloudTelemetryProvider(TelemetryProvider* inner_sink = nullptr,
                                    QObject* parent = nullptr);
    ~CloudTelemetryProvider() override;

    /// Begin periodic flushes. Reads endpoint + api_key from
    /// SettingsRepository on every flush tick (cheap), so changing the
    /// endpoint via Settings → Telemetry takes effect on the next tick
    /// without a restart. Idempotent.
    void start();

    /// Stop the flush timer. Pending in-flight POSTs are awaited via
    /// reply->deleteLater(); no synchronous wait. Idempotent.
    void stop();

    /// TelemetryProvider — enqueue + persist locally if `inner_sink_`
    /// was provided. Safe from any thread.
    void record(const QString& event_id, const QVariantMap& payload) override;

    /// True while the latest POST attempt succeeded with 2xx.
    /// Atomic — readable from any thread (ErrorPipeline scans
    /// provider health without holding any of our locks).
    bool is_healthy() const override { return healthy_.load(); }

    /// Test/diagnostics accessor — number of events still buffered for
    /// upload. Reads under the buffer mutex.
    int pending_count() const;

    static constexpr int kMaxBuffered = 5000;
    static constexpr int kBatchSize = 50;
    static constexpr int kFlushIntervalMs = 30 * 1000;
    static constexpr int kMaxBackoffSeconds = 300;

  private slots:
    void on_flush_tick();

  private:
    struct Event {
        qint64 ts_ms = 0;
        QString event_id;
        QString payload_json;
    };

    /// Re-add events to the front of the buffer after a transient POST
    /// failure so they're retried in original order on the next flush.
    void requeue_batch_front(QVector<Event> events);

    /// Read endpoint + api_key from SettingsRepository. Returns false if
    /// telemetry.cloud_endpoint is unset; callers skip POST.
    bool read_config(QString& endpoint_out, QString& api_key_out) const;

    /// One-shot POST. Owns the reply via deleteLater. Updates healthy_,
    /// resets or grows backoff. On 4xx, drops the batch; on 5xx/network,
    /// requeues and grows the backoff.
    void post_batch(const QString& endpoint, const QString& api_key,
                    const QJsonArray& body, QVector<Event> raw_for_requeue);

    TelemetryProvider* inner_sink_ = nullptr;
    QNetworkAccessManager* nam_ = nullptr;
    QTimer* flush_timer_ = nullptr;

    mutable QMutex buffer_mutex_;
    QVector<Event> buffer_;

    // Transient health/backoff state. `healthy_` is atomic because
    // is_healthy() may be called from any thread; the rest are touched
    // only on the UI thread (timer + reply lambda).
    std::atomic<bool> healthy_{true};
    int backoff_seconds_ = 0; // 0 = no backoff; flushes proceed normally
    qint64 next_attempt_ms_ = 0;

    bool started_ = false;
};

} // namespace fincept::telemetry
