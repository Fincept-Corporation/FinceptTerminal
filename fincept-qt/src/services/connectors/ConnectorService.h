#pragma once
// ConnectorService — runs catalogue data connectors (ConnectorCatalog) for every consumer.
//
// One runner, one de-dupe table and one TTL cache shared by:
//   - the Data Sources › CATALOG tab (run(..) + result_ready / error_occurred),
//   - the MCP `data_*` tools (run(.., callback) from MAAnalyticsTools.cpp),
//   - the workflow data-source nodes (run(.., callback) from DataSourceNodes.cpp),
//   - DataHub subscribers of `connector:<name>:<command>:<args-hash>` (see
//     docs/DATAHUB_TOPICS.md).
//
// Every call runs `python fincept_connector.py run <script> <command> [args...]` (the connector
// contract: one JSON envelope with typed tables or a classified failure code) through PythonRunner
// (never a raw QProcess). The envelope is parsed off the GUI thread into a TabularResult (one or
// several typed tables) plus the legacy projection (records + columns) and capped pretty/raw text.

#include "datahub/Producer.h"
#include "services/data_sources/SourceError.h"
#include "services/data_sources/TabularData.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

#include <atomic>
#include <functional>
#include <memory>

namespace fincept::services::connectors {

struct ConnectorResult {
    QString request_id;
    QString connector; ///< catalogue name, e.g. "data_fred"
    QString command;
    QStringList args;
    bool success = false;
    QString error;   ///< failure text (secrets redacted)
    /// Classified failure: code (missing_key, rate_limited, …), retry_after_s, env keys, docs url.
    fincept::services::datasources::SourceError failure;
    /// Every table of the reply with column types (multi-table payloads keep each table by name).
    fincept::services::datasources::TabularResult tabular;
    QString warning; ///< upstream error embedded in an otherwise-successful payload, if any

    QJsonValue data;     ///< full parsed payload (secrets redacted)
    QJsonArray table;    ///< rows for the table view (objects, scalars or arrays)
    QStringList columns; ///< table columns (object keys in first-seen order, or "value"/indices)
    QString table_path;  ///< where `table` came from, e.g. "data" or "(root)"
    QString table_kind;  ///< "records", "values", "dict", "key/value" or "scalar"

    QString pretty_text; ///< indented JSON, capped (see kDisplayCapBytes) with an explicit notice
    QString raw_text;    ///< compact JSON as received, capped likewise
    qint64 payload_bytes = 0;

    qint64 duration_ms = 0;
    qint64 fetched_at_ms = 0; ///< wall clock when the script finished (cache age = now - this)
    bool from_cache = false;
};

class ConnectorService : public QObject, public fincept::datahub::Producer {
    Q_OBJECT
  public:
    using Callback = std::function<void(const ConnectorResult&)>;

    static ConnectorService& instance();

    /// Size cap for pretty_text / raw_text (the full payload stays in `data`).
    static constexpr qint64 kDisplayCapBytes = 1024 * 1024;
    /// Default cache TTL when the catalogue sets none.
    static constexpr int kDefaultTtlSec = 15 * 60;
    /// Watchdog for one connector run.
    static constexpr int kRunTimeoutMs = 120 * 1000;

    struct RunOptions {
        bool force = false;  ///< skip the cache
        int timeout_ms = 0;  ///< 0 = kRunTimeoutMs
    };

    /// Runs `command` on connector `name`. Returns a request id immediately; the outcome
    /// arrives asynchronously (never inside this call) as result_ready / error_occurred and,
    /// when given, through `cb` — both on this service's thread. Identical in-flight requests
    /// are de-duplicated; results are cached per connector TTL unless `force` is set.
    /// Safe to call from any thread.
    QString run(const QString& name, const QString& command, const QStringList& args = {}, bool force = false,
                Callback cb = {});
    /// run() with options. Safe to call from any thread.
    QString run_with(const QString& name, const QString& command, const QStringList& args, const RunOptions& options,
                     Callback cb = {});
    /// Stop waiting for `request_id`: its callback gets a cancelled failure at once; the Python
    /// process is killed when no other identical request still waits for it. Any thread.
    void cancel(const QString& request_id);

    /// Recomputes env-key presence for the whole catalogue off the GUI thread; emits
    /// key_status_changed() when done.
    void refresh_key_status();
    /// Stores an API key in SecureStorage under its env-var name (the name PythonRunner
    /// injects) and updates the catalogue's presence cache. Never logs the value.
    bool store_key(const QString& env_key, const QString& value, QString* error = nullptr);

    /// DataHub topic for one request: `connector:<name>:<command>:<args-hash>`.
    static QString topic_for(const QString& name, const QString& command, const QStringList& args);

    /// Registers the producer and the `connector:*` TopicPolicies. Idempotent.
    void ensure_registered_with_hub();

    // ── fincept::datahub::Producer ────────────────────────────────────────
    QStringList topic_patterns() const override;
    void refresh(const QStringList& topics) override;
    int max_requests_per_sec() const override;

  signals:
    void result_ready(const QString& request_id, const fincept::services::connectors::ConnectorResult& result);
    void error_occurred(const QString& request_id, const QString& message);
    void request_started(const QString& request_id, const QString& connector, const QString& command);
    void key_status_changed();

  private:
    explicit ConnectorService(QObject* parent = nullptr);
    Q_DISABLE_COPY(ConnectorService)

    struct Waiter {
        QString request_id;
        Callback cb;
    };
    struct InFlight {
        QString connector;
        QString command;
        QStringList args;
        QVector<Waiter> waiters;
        qint64 started_ms = 0;
        int ttl_sec = 0;
        std::shared_ptr<std::atomic<bool>> cancel_token;
    };
    struct CacheEntry {
        ConnectorResult result;
        qint64 expires_ms = 0;
        qint64 last_used_ms = 0;
        qint64 bytes = 0;
    };
    struct TopicRecord {
        QString connector;
        QString command;
        QStringList args;
    };

    static QString request_key(const QString& name, const QString& command, const QStringList& args);
    void start_on_service_thread(const QString& request_id, const QString& name, const QString& command,
                                 const QStringList& args, const RunOptions& options, Callback cb);
    void cancel_on_service_thread(const QString& request_id);
    void deliver(const ConnectorResult& result, const Callback& cb);
    void finish(const QString& key, ConnectorResult result);
    void store_in_cache(const QString& key, const ConnectorResult& result, int ttl_sec);
    void evict_if_needed();
    QStringList secret_values_for(const QString& name) const;

    QHash<QString, InFlight> in_flight_;
    QHash<QString, CacheEntry> cache_;
    qint64 cache_bytes_ = 0;
    QHash<QString, TopicRecord> topics_; // hub topic -> how to re-run it
    std::atomic<quint64> next_id_{1};
    bool hub_registered_ = false;
    std::atomic<bool> key_refresh_running_{false};
};

} // namespace fincept::services::connectors

#include <QMetaType>
Q_DECLARE_METATYPE(fincept::services::connectors::ConnectorResult)
