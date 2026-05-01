#pragma once
#include "core/telemetry/TelemetryProvider.h"

#include <QMutex>

namespace fincept::telemetry {

/// Phase 10: writes events to workspace.db's `telemetry_events` table.
/// Powers the in-app session-history view (Phase 9 reads from here).
/// Always-on once installed — does NOT do any network upload. Cloud
/// upload is a separate provider (`CloudTelemetryProvider`) that wraps
/// or replaces this one.
///
/// Schema (created lazily on first record() if missing):
///   telemetry_events(
///     id INTEGER PRIMARY KEY AUTOINCREMENT,
///     ts_ms INTEGER NOT NULL,
///     event_id TEXT NOT NULL,
///     payload_json TEXT NOT NULL,
///     session_id TEXT
///   )
///
/// Inserts go through WorkspaceDb's mutex so concurrent recorders from
/// different threads serialise correctly.
class LocalTelemetrySink : public TelemetryProvider {
  public:
    LocalTelemetrySink();

    void record(const QString& event_id, const QVariantMap& payload) override;
    bool is_healthy() const override { return healthy_; }

  private:
    void ensure_schema();
    bool schema_ensured_ = false;
    bool healthy_ = true;
    QMutex schema_mutex_;
};

} // namespace fincept::telemetry
