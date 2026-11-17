// src/services/relationship_map/RelationshipMapService.h
#pragma once

#include "datahub/Producer.h"
#include "screens/relationship_map/RelationshipMapTypes.h"

#include <QDateTime>
#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QSet>
#include <QStringList>

#include <atomic>
#include <functional>
#include <memory>

namespace fincept::services {

/// What a Relationship Map request asks for.
struct RelMapRequestOptions {
    /// Bypass the remembered payload (REFRESH, the quiet refresh of an old map).
    bool force = false;
    /// Non-empty: re-fetch only these sections (relmap::section ids) and merge them into the
    /// remembered payload of the ticker (RETRY of a partial map). Needs a remembered payload;
    /// without one the whole map is fetched.
    QStringList sections;
};

/// Corporate relationship data (relationship_map.py: yfinance, SEC EDGAR, computed
/// correlations) for any number of Relationship Map panels at once.
///
///   * request() returns an id; every answer carries it, and each panel acts on its own ids
///     only (the screen is multi-instance: two panels may show two companies).
///   * Two requests for the same ticker (and sections) share one script run; cancel() drops
///     a request and kills the run once nobody waits on it (the output is then discarded).
///   * While the script runs, each finished section is streamed (request_section) with a
///     snapshot of everything received so far; request_finished brings the whole payload,
///     parsed on a worker thread.
///   * Payloads are remembered in CacheManager ("relmap:v2:<TICKER>", JSON text incl. the
///     fetch time): a complete map for 36 h, a partial one for 30 min. A request without
///     `force` is answered from it (queued, from_cache = true).
///   * The DataHub topic geopolitics:relationship_graph:<ticker> is served as before (fetch()).
class RelationshipMapService : public QObject, public fincept::datahub::Producer {
    Q_OBJECT
  public:
    static RelationshipMapService& instance();

    /// A panel asks for `ticker` (Yahoo form). Never returns 0.
    quint64 request(const QString& ticker, const RelMapRequestOptions& opts = RelMapRequestOptions());
    /// No more signals for `request_id`; its run is killed when no other request (or the hub)
    /// waits on it. Unknown / finished ids are ignored.
    void cancel(quint64 request_id);
    /// True while `request_id` waits for its run.
    bool is_pending(quint64 request_id) const;

    /// The remembered payload of `ticker`, read and parsed off the GUI thread; `done` runs on
    /// the GUI thread (dropped when `ctx` is gone): found = false when nothing is remembered.
    void load_remembered(const QString& ticker, QObject* ctx,
                         std::function<void(bool found, const fincept::relmap::RelationshipData& data,
                                            const QDateTime& as_of)>
                             done);

    // ── DataHub (unchanged topic contract) ─────────────────────────────────
    void fetch(const QString& ticker);
    void ensure_registered_with_hub();
    QStringList topic_patterns() const override;
    void refresh(const QStringList& topics) override;
    int max_requests_per_sec() const override;

  signals:
    /// The script started: the sections it will deliver (their groups show "loading…").
    void request_planned(quint64 request_id, const QString& ticker, const QStringList& sections);
    /// One section arrived; `snapshot` = everything received so far for this run.
    void request_section(quint64 request_id, const fincept::relmap::RelationshipData& snapshot,
                         const QString& section_id);
    /// The whole map. `as_of` = when the data was fetched (UTC).
    void request_finished(quint64 request_id, const fincept::relmap::RelationshipData& data, const QDateTime& as_of,
                          bool from_cache);
    /// The run failed; `error` is a plain sentence.
    void request_failed(quint64 request_id, const QString& error);

  private:
    RelationshipMapService() = default;

    struct Run {
        QString key;    ///< ticker + "|" + sorted sections
        QString ticker;
        QStringList sections; ///< empty = full map
        QSet<quint64> requests;
        bool for_hub = false;
        std::shared_ptr<std::atomic<bool>> cancel;
        relmap::RelationshipData snapshot; ///< streamed sections so far
        qint64 started_ms = 0;
        quint64 seq = 0;
    };

    void start_run(const QString& key, const QString& ticker, const QStringList& sections);
    void on_run_line(const QString& key, quint64 seq, const QString& line);
    void on_run_done(const QString& key, quint64 seq, bool success, const QString& output, const QString& error);
    void finish_run(const QString& key, quint64 seq, bool ok, const relmap::RelationshipData& data,
                    const QString& error, const QDateTime& as_of);
    void publish_to_hub(const relmap::RelationshipData& data);
    void hub_error(const QString& ticker, const QString& message);

    QHash<QString, Run> runs_;             ///< run key -> run
    QHash<quint64, QString> request_run_;  ///< request id -> run key ("" = answered from cache)
    quint64 next_request_ = 0;
    quint64 next_seq_ = 0;
    bool hub_registered_ = false;
};

} // namespace fincept::services
