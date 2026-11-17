#pragma once
// ConnectionService — the one entry point for using Data Sources connections: TEST (a real round
// trip, health persisted), LIST (schema / entity browser) and FETCH (bounded sample or a query) for
// saved connections, unsaved form configs, local files and inline HTTP sources.
//
// Every request runs on this service's own worker pool (never the GUI thread), is cancellable and
// time-limited, and answers once on the GUI thread through the callback — dropped if `context` is
// gone. Callers keep the returned job id to cancel, and to ignore a reply that no longer matches
// what is on screen.

#include "services/data_sources/ConnectionDrivers.h"

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QString>

#include <functional>

class QThreadPool;

namespace fincept::services::datasources {

class ConnectionService : public QObject {
    Q_OBJECT
  public:
    using Callback = std::function<void(const SourceReply&)>;

    static ConnectionService& instance();

    /// TEST a saved connection; the outcome is written to data_sources.last_* and announced by health_changed().
    quint64 test_connection(const QString& connection_id, QObject* context, Callback cb);
    /// LIST the entities (tables, indexes, keys, endpoints, sheets, commands) of a saved connection.
    quint64 list_connection(const QString& connection_id, QObject* context, Callback cb, int limit = 1000);
    /// FETCH rows from a saved connection: `entity` (table / path / key / command) and/or `query`.
    quint64 fetch_connection(const QString& connection_id, const QString& entity, const QString& query, int limit,
                             QObject* context, Callback cb);
    /// Any request on a config the caller holds (the unsaved form, a file, an inline HTTP source).
    quint64 run(const SourceRequest& request, QObject* context, Callback cb);
    void cancel(quint64 job);

  signals:
    void health_changed(const QString& connection_id, bool ok, const QString& summary);

  private:
    explicit ConnectionService(QObject* parent = nullptr);
    Q_DISABLE_COPY(ConnectionService)

    quint64 start(const QString& connection_id, SourceRequest request, bool persist_health, QObject* context, Callback cb);

    QThreadPool* pool_ = nullptr;
    QHash<quint64, CancelToken> jobs_;
    quint64 next_job_ = 0;
};

} // namespace fincept::services::datasources
