#pragma once
// OISnapshotter — captures per-token OI/LTP/Vol/IV from chain publishes and
// flushes minute-aligned snapshots to SQLite for the OI Analytics sub-tab.
//
// Pipeline
// ────────
//
//   option:chain:* publish ──► OISnapshotter::on_chain_published
//                              ├─ updates QHash<token, LastTick> last_tick_
//                              └─ marks the minute as dirty
//   60 s QTimer ──► flush()
//                   └─ INSERT OR REPLACE into oi_snapshots
//   1 h QTimer ──► housekeeping()
//                   └─ DELETE WHERE ts_minute < (now − 7 days)
//
// The snapshotter is also the Producer for `oi:history:<broker>:<token>:<window>`
// (window = "1d" | "5d"). `refresh()` queries the SQLite repo and publishes
// `QVector<OISample>` ordered ascending by ts_minute.
//
// Lifecycle
// ─────────
// Singleton, registered from main.cpp after the migration runner has applied
// v025. Internally subscribes to `option:chain:*` via DataHub on construction
// (no explicit Producer hand-off needed — the chain Producer publishes, the
// hub fans out, this object is one of the consumers).

#include "datahub/Producer.h"
#include "services/options/OptionChainTypes.h"

#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>

namespace fincept::services::options {

class OISnapshotter : public QObject, public fincept::datahub::Producer {
    Q_OBJECT
  public:
    static OISnapshotter& instance();

    /// Idempotent — registers the `oi:history:*` Producer, applies its
    /// policies, subscribes to `option:chain:*`, starts the flush + cleanup
    /// timers, and runs an initial 7-day prune.
    void ensure_registered_with_hub();

    /// Topic helpers.
    static QString history_topic(const QString& broker_id, qint64 token, const QString& window);

    // ── fincept::datahub::Producer ─────────────────────────────────────────
    QStringList topic_patterns() const override;
    void refresh(const QStringList& topics) override;
    int max_requests_per_sec() const override;

  private:
    OISnapshotter();
    ~OISnapshotter() override = default;
    OISnapshotter(const OISnapshotter&) = delete;
    OISnapshotter& operator=(const OISnapshotter&) = delete;

    /// Apply one chain publish to the in-memory tick map. Updates `dirty_`.
    void on_chain_published(const QString& topic, const QVariant& v);

    /// Flush pending in-memory ticks to oi_snapshots, minute-aligned.
    void flush_to_disk();

    /// Delete rows older than retention window.
    void run_housekeeping();

    /// Parse `oi:history:<broker>:<token>:<window>` into its parts.
    static bool parse_history_topic(const QString& topic, QString& broker, qint64& token, QString& window);

    /// Days covered by a window string (`"1d"` / `"5d"` …). Defaults to 1.
    static int days_for_window(const QString& window);

    bool registered_ = false;

    /// Per-token most-recent in-memory snapshot (overwritten as ticks land).
    /// Plus the ts_minute of the last publish so flush can write the right
    /// minute bucket.
    struct LastTick {
        OISample sample;
        bool dirty = false;
    };
    QHash<qint64, LastTick> last_tick_;

    QTimer flush_timer_;
    QTimer housekeeping_timer_;
};

} // namespace fincept::services::options
