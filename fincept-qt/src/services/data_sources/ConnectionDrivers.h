#pragma once
// ConnectionDrivers — the I/O half of Data Sources connections: runs one TEST / LIST / FETCH request
// against a real target with the driver ConnectionProfiles names (HTTP family incl. object stores
// and event streams, WebSocket sampling, local files incl. Excel, Qt SQL drivers, catalogue
// connectors, Redis / Memcached wire protocols). Blocking by design: ConnectionService runs every
// call on its own worker pool, never on the GUI thread. Every failure is a classified SourceError;
// secrets are never logged and are redacted from messages.

#include "services/data_sources/ConnectionProfiles.h"
#include "services/data_sources/SourceError.h"
#include "services/data_sources/TabularData.h"

#include <QJsonObject>
#include <QString>

#include <atomic>
#include <memory>

namespace fincept::services::datasources {

using CancelToken = std::shared_ptr<std::atomic<bool>>;

struct SourceRequest {
    SourceOp op = SourceOp::Test;
    QString type_id;       ///< connection type ("postgresql", "rest-api", …)
    QJsonObject config;    ///< the full config, secrets included — never logged
    QString entity;        ///< table / path / key / index / sheet / command
    QString query;         ///< the type's free-form query (SQL, PromQL, arguments, …)
    int limit = 200;       ///< max rows for LIST / FETCH
    int timeout_ms = 30000;
};

struct SourceReply {
    bool ok = false;
    SourceError error;
    TabularResult data;    ///< FETCH: the rows; LIST: name / kind / detail of each entity; TEST: a small sample
    QString summary;       ///< TEST: "PostgreSQL 16.2 · 12 tables", "HTTP 200 · 7 rows"
    QStringList notes;     ///< caveats ("Ran with FINNHUB_API_KEY from Settings > Credentials")
    qint64 latency_ms = 0;
};

/// What a type can do in THIS build on THIS machine: type_support() plus run-time facts (which Qt SQL
/// drivers load, whether QXlsx / Qt WebSockets were built in, whether a catalogue connector exists).
TypeSupport effective_support(const QString& type_id);

/// Run one request (blocking; worker threads only). Never throws; never returns ok with no data
/// for FETCH. `cancel` may be raised from any thread.
SourceReply execute_source_request(const SourceRequest& request, const CancelToken& cancel);

/// Qt SQL driver name for a flavour that loads here ("QPSQL", "QODBC", …), or "" (+ why / fix).
QString sql_driver_for(const QString& flavour, QString* why = nullptr, QString* fix = nullptr);

} // namespace fincept::services::datasources
