#pragma once
// ConnectionProfiles — what each Data Sources connection type really is and how it is reached.
//
// For every type id in the ConnectorRegistry (postgresql, rest-api, influxdb, …) this answers:
//   * which driver serves it (HTTP, event stream, WebSocket, file, Qt SQL, catalogue connector,
//     Redis/Memcached wire protocol) — or that nothing in this build can, with the reason and the
//     exact alternative (never a fake green tick);
//   * what TEST, LIST (schema browser) and FETCH (sample rows) send, as plain request plans the
//     HTTP executor runs, and how the answer becomes a table (post-processing).
//
// Pure (QtCore only): plans and post-processing are unit-tested without a network.

#include "services/data_sources/SourceError.h"
#include "services/data_sources/TableParsers.h"
#include "services/data_sources/TabularData.h"

#include <QByteArray>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QUrl>

namespace fincept::services::datasources {

enum class DriverKind { None, Http, EventStream, WebSocket, File, Sql, Catalogue, Redis, Memcached };

enum class SourceOp { Test, List, Fetch };

struct TypeSupport {
    QString type_id;
    DriverKind driver = DriverKind::None;
    /// HTTP profile id ("rest", "elasticsearch", …), SQL flavour ("sqlite", "postgres", "mysql",
    /// "mssql", "oracle", "snowflake", "vertica", "redshift"), file format, or catalogue connector name.
    QString profile;
    bool supported = false;   ///< false: nothing in this build can reach it (reason + fix say why / what instead)
    QString reason;
    QString fix;
    bool can_list = false;    ///< has a schema / entity browser
    bool can_fetch = false;   ///< returns rows
    QString entity_label;     ///< what an entity is: "Table", "Index", "Measurement", "Path", "Key", "Command", …
    QString query_label;      ///< the free-form query box: "SQL", "PromQL", "Flux", "Cypher", "Arguments", …
    QString query_hint;       ///< placeholder for that box
    QString parity;           ///< the outside tool command that reaches the same target (for the docs / tooltip)
};

/// Support for one connection type id (unknown ids → not supported).
TypeSupport type_support(const QString& type_id);
/// Every type id this table knows (for tests: each registry type must be covered).
QStringList known_type_ids();

// ── HTTP plans ────────────────────────────────────────────────────────────

struct HttpCall {
    QByteArray method = "GET";
    QUrl url;
    QList<QPair<QByteArray, QByteArray>> headers;  ///< may carry credentials — never log
    QByteArray body;
    QString what;                 ///< "the Elasticsearch node" — used in error messages
    ParseOptions parse;           ///< how to read the body (format, rows path)
    QString post;                 ///< post-processor id ("" = parse_table only)
    QStringList secrets;          ///< values to redact from any message
    bool expect_body = true;      ///< false: a 2xx status alone is the answer (health endpoints)
    /// Signing applied by the executor right before sending (time-dependent): "" | "aws_v4" | "azure_shared_key".
    QString sign;
    QJsonObject sign_params;      ///< aws_v4: access_key, secret_key, region, service; azure: account, key
    /// A token exchange the executor runs first: "" | "ibm_iam" (sign_params.api_key → Bearer).
    QString pre_auth;
    /// Object-store GET of a file: read the body by this file name's extension (csv/json/xlsx/…).
    QString object_name;
};

struct HttpPlan {
    bool ok = false;
    SourceError error;            ///< !ok: bad_args naming the missing field
    QList<HttpCall> calls;        ///< run in order; the LAST call's answer is the result
    QString summary_hint;         ///< TEST: what success means ("Elasticsearch answered")
    /// No request needed: the answer is this fixed table (e.g. the endpoints a vendor offers).
    QJsonArray static_rows;
};

/// The request(s) for `op` on an HTTP-served type. `entity` = table / index / path / endpoint id;
/// `query` = the type's free-form query (SQL, PromQL, GraphQL, …); `limit` = max rows.
HttpPlan http_plan(const TypeSupport& support, SourceOp op, const QJsonObject& config, const QString& entity,
                   const QString& query, int limit);

/// Turn a successful HTTP answer into tables (profile-specific shapes: Prometheus vectors,
/// QuestDB columns/dataset, ClickHouse meta/data, Elasticsearch hits, Neo4j rows, Influx annotated
/// CSV, …). `status_summary` receives a one-line TEST summary when the body carries one
/// (versions, cluster health).
struct PostResult {
    bool ok = false;
    TabularResult data;
    QString summary;
    SourceError error;
};
PostResult post_process(const HttpCall& call, SourceOp op, const QByteArray& body, const QString& content_type);

/// Generic credential headers / query items from a connection config: authType none | apikey |
/// bearer | basic | oauth2 with apiKey / apiToken / token / authToken / accessToken, apiKeyName,
/// apiKeyIn (header | query), username / password, and custom headers (JSON object or "Name: value"
/// lines). Applied to `call`. Unknown auth types → bad_args.
bool apply_generic_auth(HttpCall& call, const QJsonObject& config, SourceError* error);

/// base + path with exactly one '/' between them; an absolute http(s) path wins.
QUrl join_url(const QString& base, const QString& path);
/// "host" or "host:port" or a URL → http(s) URL with the given default port (keeps https when given).
QUrl url_from_host(const QString& host_or_url, int port, bool https = false);

} // namespace fincept::services::datasources
