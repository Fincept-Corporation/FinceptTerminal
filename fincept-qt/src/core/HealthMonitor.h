#pragma once
// HealthMonitor — aggregates local subsystem health checks for the desktop app.
// See OPENALGO_BRIDGE_PHASE3.md §18.
//
// Unlike OpenAlgo's server-side health blueprint, Fincept is a desktop app, so
// this monitor focuses on the live trading / data subsystems that can silently
// degrade: broker connection states, per-account WebSocket streams, the Python
// process pool, and the DataHub producer/topic layer.
//
// Each check is defensively wrapped so a single failing/uninitialised subsystem
// reports as a failed Check rather than crashing the whole snapshot. Where a
// subsystem does not expose the information needed for a meaningful check, the
// Check reports "not available" instead of inventing an API call.

#include <QJsonObject>
#include <QString>
#include <QVector>

namespace fincept {

/// Snapshot of all health checks at one point in time.
struct HealthStatus {
    struct Check {
        QString name;     ///< short identifier, e.g. "broker_connections"
        bool ok = false;  ///< true if the subsystem is healthy
        QString detail;   ///< human-readable summary / failure reason
    };

    QVector<Check> checks;

    /// True only if every check passed.
    bool all_ok() const;

    /// Serialise to a JSON object suitable for the MCP system_health_check tool.
    /// Shape: { "all_ok": bool, "checks": [ { name, ok, detail }, ... ] }
    QJsonObject to_json() const;
};

/// Singleton that runs all health checks synchronously on demand.
class HealthMonitor {
  public:
    static HealthMonitor& instance();

    /// Run every check now and return a snapshot. Safe to call from the main
    /// thread (each check is non-blocking — reads in-memory state only).
    HealthStatus check() const;

  private:
    HealthMonitor() = default;
    ~HealthMonitor() = default;
    HealthMonitor(const HealthMonitor&) = delete;
    HealthMonitor& operator=(const HealthMonitor&) = delete;

    // Individual checks — each returns a single Check and never throws.
    HealthStatus::Check check_broker_connections() const;
    HealthStatus::Check check_websocket_streams() const;
    HealthStatus::Check check_python_pool() const;
    HealthStatus::Check check_datahub() const;
};

} // namespace fincept
