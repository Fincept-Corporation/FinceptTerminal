#pragma once
#include <QHash>
#include <QObject>
#include <QString>

namespace fincept {

/// Tracks how often each screen (component) has been navigated to. Used by
/// the Component Browser to surface frequently-used components at the top
/// of the grid — a cheap equivalent of Bloomberg's popularity stars on
/// Launchpad's component catalogue.
///
/// Storage: QSettings under "component_usage/<id>" = int. Per-user,
/// per-profile (scoped through the existing "Fincept/FinceptTerminal"
/// QSettings organisation/app). Does NOT sync to the cloud — a user's
/// usage fingerprint is intentionally local.
///
/// The counter is monotonically increasing; we don't decay it. For a
/// first pass that's fine; if rankings get stale after months of usage we
/// can add a time-window later without breaking the schema.
class PopularityTracker : public QObject {
    Q_OBJECT
  public:
    static PopularityTracker& instance();

    /// Bump the use count for a screen id. Idempotent — safe to call from
    /// any navigation path. No-op for empty id.
    void increment(const QString& id);

    /// Current use count. Returns 0 for unknown ids (never navigated).
    int use_count(const QString& id) const;

    /// Reset all counts. Used by the "Clear usage history" setting.
    void clear_all();

  signals:
    void usage_changed(const QString& id, int new_count);

  private:
    PopularityTracker();
    void load();
    void save(const QString& id, int count);

    QHash<QString, int> counts_;
};

} // namespace fincept
