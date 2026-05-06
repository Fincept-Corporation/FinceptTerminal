#pragma once
#include <QHash>
#include <QObject>
#include <QString>

namespace fincept {

/// Per-screen navigation counts driving Component Browser ordering.
/// Stored locally in QSettings under "component_usage/<id>"; monotonic, no decay.
class PopularityTracker : public QObject {
    Q_OBJECT
  public:
    static PopularityTracker& instance();

    /// No-op for empty id.
    void increment(const QString& id);

    int use_count(const QString& id) const;

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
