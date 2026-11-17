#pragma once
#include <QHash>
#include <QObject>
#include <QString>

class QTimer;

namespace fincept {

/// Per-screen navigation counts driving Component Browser ordering.
/// Stored locally in QSettings under "component_usage/<id>"; monotonic, no decay.
///
/// increment() runs inside every navigation click, so it never touches the
/// settings file itself: counts are updated in memory, and the changed ones are
/// written in one batch off the GUI thread after a short quiet period, plus
/// synchronously at quit (aboutToQuit). A crash loses at most that window.
class PopularityTracker : public QObject {
    Q_OBJECT
  public:
    static PopularityTracker& instance();

    /// No-op for empty id.
    void increment(const QString& id);

    int use_count(const QString& id) const;

    void clear_all();

    /// Write the pending counts now, on the calling thread (quit path).
    void flush_now();

  signals:
    void usage_changed(const QString& id, int new_count);

  private:
    PopularityTracker();
    void load();
    void flush_async();

    QHash<QString, int> counts_;
    QHash<QString, int> pending_; ///< changed since the last write: id -> count
    QTimer* flush_timer_ = nullptr;
};

} // namespace fincept
