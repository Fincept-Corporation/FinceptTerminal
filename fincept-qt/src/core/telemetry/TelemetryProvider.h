#pragma once
#include <QString>
#include <QVariantMap>

namespace fincept::telemetry {

/// Phase 10: pluggable telemetry sink. Default off (no provider registered).
/// `LocalTelemetrySink` writes to workspace.db unconditionally so the
/// session-history view works offline. Cloud upload is opt-in and runs
/// through whichever concrete `TelemetryProvider` the user has configured.
///
/// Privacy contract (decision 10.4): action invocations + counts only.
/// NEVER include symbol/ticker/credentials/account-id payloads. Callers
/// are responsible for sanitising args before invoking record().
///
/// Threading: providers may be invoked from any thread (action invocation
/// happens on the UI thread today, but Phase 9's stress mode will fire
/// from a worker). Implementations must be thread-safe internally.
class TelemetryProvider {
  public:
    virtual ~TelemetryProvider() = default;

    /// Record a single event. The implementation decides how to durably
    /// store / batch / upload it.
    virtual void record(const QString& event_id, const QVariantMap& payload) = 0;

    /// Whether the provider's upstream (e.g. cloud HTTPS endpoint) is
    /// considered healthy. Used by ErrorPipeline (Phase 9) to surface a
    /// "telemetry: degraded" indicator. Default true for providers that
    /// don't have an upstream (LocalTelemetrySink).
    virtual bool is_healthy() const { return true; }
};

/// Process-singleton facade. main.cpp / Settings → Telemetry decides
/// which concrete provider (if any) to install. ActionRegistry calls
/// `record()` on every invoke; if no provider is installed it's a no-op.
class TelemetrySink {
  public:
    static TelemetrySink& instance();

    /// Install a provider. Replaces any previous one. Pass nullptr to
    /// uninstall (telemetry effectively disabled).
    void set_provider(TelemetryProvider* provider);

    /// Cheap no-op when no provider is installed. The lock is internal
    /// so callers don't need to coordinate.
    void record(const QString& event_id, const QVariantMap& payload);

    bool has_provider() const;

  private:
    TelemetrySink() = default;
    TelemetryProvider* provider_ = nullptr;
};

} // namespace fincept::telemetry
