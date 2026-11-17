// src/services/economics/EconomicsService.h
// Singleton service for all economics data sources.
// Wraps PythonRunner, caches results, emits result_ready.
#pragma once

#include "datahub/Producer.h"

#include <QDateTime>
#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>

#include <atomic>
#include <memory>

namespace fincept::services {

struct EconomicsResult {
    bool success = false;
    QJsonObject data;
    QString error; ///< "[CODE] message" when the script named a failure code
    QString source_id; // which provider produced this
    /// UTC time the script produced `data`. A cache hit keeps the ORIGINAL time, so panels can say
    /// "retrieved 14:05 (cached)" instead of passing a 10-minute-old answer off as fresh.
    QDateTime fetched_at;
    bool from_cache = false;
    /// Failure code the script gave ("error_code" or the connector contract's "code": missing_key,
    /// auth_failed, rate_limited, upstream_down, bad_args, no_data, not_supported); empty otherwise.
    QString error_code;
    /// The credential a missing_key / auth_failed failure names ("BLS_API_KEY"), when the script says.
    QString env_key;
};

/// Phase 6 — DataHub producer for `econ:<source>:<request_id>` topics.
/// The service is a thin Python-script dispatcher; hub topics are keyed
/// by (source_id, request_id) matching the legacy `result_ready` signal
/// tuple. Panels can migrate to `hub.subscribe("econ:fred:gdp_quarterly",
/// ...)` and the service fans out dual — hub publish + legacy signal —
/// so migration is incremental.
class EconomicsService : public QObject, public fincept::datahub::Producer {
    Q_OBJECT
  public:
    static EconomicsService& instance();

    /// Run a script command asynchronously.
    /// @param source_id  caller tag echoed back in result_ready
    /// @param script     python script filename
    /// @param command    first CLI argument
    /// @param args       additional arguments
    /// @param request_id unique id to correlate response
    ///
    /// Every run is tracked by its request id, so a stale answer never reaches result_ready:
    ///   * the same request id with the same script / command / args while a run is in flight
    ///     joins that run (one process, one result_ready for everybody waiting on the id);
    ///   * the same request id with OTHER arguments supersedes the run in flight — its script is
    ///     killed and its answer dropped (a cache hit supersedes it too);
    ///   * cancel(request_id) kills the run and drops its answer.
    /// Nothing heavy happens on the calling (GUI) thread: the cache lookup, the JSON parse of the
    /// script's answer and the cache write run on the service's decode pool, and result_ready is
    /// emitted afterwards on the service's thread — also for a cache hit, never from inside
    /// execute(). A run cancelled or superseded while its answer is being decoded is dropped.
    void execute(const QString& source_id, const QString& script, const QString& command, const QStringList& args,
                 const QString& request_id);

    /// Stop waiting for `request_id`: a script still queued or running for it is killed
    /// (PythonRunner cancel token) and no result_ready is emitted for it. Callers that joined
    /// the same run count separately — the run stops when the last of them cancels.
    /// No effect when nothing runs for the id. GUI thread.
    void cancel(const QString& request_id);
    /// A run for `request_id` is queued or running.
    bool is_running(const QString& request_id) const { return inflight_.contains(request_id); }

    /// Record how to produce `econ:<source_id>:<request_id>` WITHOUT running it, so a pure hub
    /// subscriber (e.g. a dashboard tile) gets its data from refresh() alone: the hub's cold-start
    /// fetch, TTL refreshes and `request(topic, true)`. Same arguments as execute(); returns the topic.
    /// Calling execute() and then subscribing would run the script twice on a cache miss (execute()
    /// plus the hub's cold-start refresh()).
    QString remember_topic(const QString& source_id, const QString& script, const QString& command,
                           const QStringList& args, const QString& request_id);

    /// Register with the hub + install econ:* policies. Idempotent.
    void ensure_registered_with_hub();

    // ── fincept::datahub::Producer ────────────────────────────────────────
    /// Patterns: `econ:*`. Individual series topic shape is
    /// `econ:<source>:<request_id>` (request_id is caller-assigned and
    /// usually carries the series id or query hash).
    QStringList topic_patterns() const override;
    /// Split topic by `:` → source, request_id. If a pending dispatch
    /// record exists for that topic (left by execute() or remember_topic()),
    /// re-execute it. Otherwise no-op — scheduler-driven refresh without
    /// a prior execute() call is a no-op since we don't know the script.
    void refresh(const QStringList& topics) override;
    int max_requests_per_sec() const override; // 2 — Python spawn pacing

  signals:
    void result_ready(const QString& request_id, const fincept::services::EconomicsResult& result);

  private:
    explicit EconomicsService(QObject* parent = nullptr);
    Q_DISABLE_COPY(EconomicsService)

    static constexpr int kCacheTtlSec = 10 * 60; // 10 min — passed to CacheManager

    QString cache_key(const QString& script, const QString& command, const QStringList& args) const;

    /// Compose hub topic: `econ:<source>:<request_id>`.
    static QString hub_topic(const QString& source_id, const QString& request_id);
    /// Remember dispatch params so `refresh(topic)` can replay the call.
    struct DispatchRecord {
        QString source_id;
        QString script;
        QString command;
        QStringList args;
        QString request_id;
    };
    QHash<QString, DispatchRecord> dispatch_records_; // topic -> last dispatch
    bool hub_registered_ = false;

    /// The run a request id's next result_ready answers. A reply whose sequence number no longer
    /// matches (cancelled, superseded) is dropped.
    struct Inflight {
        quint64 seq = 0;
        QString key; ///< cache key of the call (script, command, args)
        std::shared_ptr<std::atomic<bool>> token;
        int waiters = 1;
        bool fresh = false; ///< refresh(): a cached answer (lookup still in flight) does not count
    };
    QHash<QString, Inflight> inflight_; // request_id -> run in flight
    quint64 next_seq_ = 0;
    /// Kill and forget the run in flight for `request_id` (no-op when none).
    void drop_inflight(const QString& request_id, const char* why);
    /// execute(); `fresh` skips the cached answer (refresh()).
    void start(const QString& source_id, const QString& script, const QString& command, const QStringList& args,
               const QString& request_id, bool fresh);
    /// The cache had nothing (or the run is fresh): spawn the script for run `seq`.
    void start_script(const QString& source_id, const QString& script, const QString& command,
                      const QStringList& args, const QString& request_id, quint64 seq);
    /// A decoded answer for run `seq`: dropped when the run was cancelled / superseded meanwhile,
    /// else result_ready + the hub topic.
    void finish(const QString& request_id, quint64 seq, const EconomicsResult& result);
};

} // namespace fincept::services

#include <QMetaType>
Q_DECLARE_METATYPE(fincept::services::EconomicsResult)
