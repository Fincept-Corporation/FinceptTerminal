#pragma once
// ConnectivityService — DataHub producer for the Fincept API health probe.
//
// Topic: `system:health`. Payload: QJsonObject
//   {
//     "ok":            bool    — GET <api_base_url>/health answered 2xx
//     "latency_ms":    int     — round-trip of that request, -1 when it failed
//     "http_status":   int     — HTTP status, 0 when no response arrived
//     "error":         string  — short failure reason, empty when ok
//     "checked_at_ms": int64   — wall clock (ms since epoch) of the probe
//     "endpoint":      string  — host that was probed (no path / query)
//   }
//
// A failed probe is published as a value (ok=false), not via publish_error():
// "the API is unreachable" is the answer the status bar needs to show, and
// publish_error() would leave the last *good* reading on screen.
//
// The probe deliberately uses its own QNetworkAccessManager rather than
// HttpClient: it needs no session credentials, and a health check must not
// queue behind (or be rate-limited with) real API traffic.
//
// Cadence is owned by the hub (CLAUDE.md D3): TTL 30 s while healthy, 15 s
// after a failure so recovery shows up quickly; min_interval 10 s. Consumers
// subscribe in showEvent() and unsubscribe in hideEvent(); with no subscriber
// nothing is probed.

#include "datahub/Producer.h"

#include <QObject>
#include <QString>
#include <QStringList>

class QNetworkAccessManager;

namespace fincept::services {

class ConnectivityService : public QObject, public fincept::datahub::Producer {
    Q_OBJECT
  public:
    static ConnectivityService& instance();

    /// The single topic this producer owns.
    static QString health_topic();

    /// Register with the hub and install the topic policy. Idempotent — the
    /// status bar calls it before its first subscribe, so no main.cpp hook is
    /// required.
    void ensure_registered_with_hub();

    // ── fincept::datahub::Producer ────────────────────────────────────────
    QStringList topic_patterns() const override;
    void refresh(const QStringList& topics) override;
    int max_requests_per_sec() const override { return 1; }

  private:
    explicit ConnectivityService(QObject* parent = nullptr);
    Q_DISABLE_COPY(ConnectivityService)

    void probe();

    QNetworkAccessManager* nam_ = nullptr;
    bool hub_registered_ = false;
    bool probe_in_flight_ = false;
};

} // namespace fincept::services
