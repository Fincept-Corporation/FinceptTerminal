// ConnectionProfiles.cpp — see ConnectionProfiles.h.

#include "services/data_sources/ConnectionProfiles.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSet>
#include <QTimeZone>
#include <QUrlQuery>

#include <algorithm>
#include <utility>

namespace fincept::services::datasources {

namespace {

QString cp_tr(const char* text) {
    return QCoreApplication::translate("ConnectionProfiles", text);
}

QString cp_str(const QJsonObject& cfg, const char* key) {
    const QJsonValue v = cfg.value(QLatin1String(key));
    if (v.isDouble())
        return QString::number(v.toDouble(), 'g', 15);
    if (v.isBool())
        return v.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    return v.toString().trimmed();
}

int cp_int(const QJsonObject& cfg, const char* key, int def) {
    const QJsonValue v = cfg.value(QLatin1String(key));
    if (v.isDouble() && v.toDouble() > 0)
        return int(v.toDouble());
    bool ok = false;
    const int i = v.toString().trimmed().toInt(&ok);
    return ok && i > 0 ? i : def;
}

bool cp_bool(const QJsonObject& cfg, const char* key) {
    const QJsonValue v = cfg.value(QLatin1String(key));
    if (v.isBool())
        return v.toBool();
    const QString s = v.toString().trimmed().toLower();
    return s == QLatin1String("true") || s == QLatin1String("1") || s == QLatin1String("yes");
}

QByteArray cp_json(const QJsonObject& o) {
    return QJsonDocument(o).toJson(QJsonDocument::Compact);
}

QByteArray cp_basic(const QString& user, const QString& pass) {
    return "Basic " + (user + QLatin1Char(':') + pass).toUtf8().toBase64();
}

HttpPlan cp_bad(const QString& field, const QString& message) {
    HttpPlan p;
    p.error = SourceError::make(ErrorCode::BadArgs, message, field);
    return p;
}

HttpPlan cp_one(HttpCall c, const QString& hint = QString()) {
    HttpPlan p;
    p.ok = true;
    p.summary_hint = hint;
    p.calls << std::move(c);
    return p;
}

HttpPlan cp_static(const QJsonArray& rows) {
    HttpPlan p;
    p.ok = true;
    p.static_rows = rows;
    return p;
}

QJsonArray cp_entities(std::initializer_list<std::pair<const char*, const char*>> items) {
    QJsonArray a;
    for (const auto& it : items)
        a.append(QJsonObject{{"name", QString::fromLatin1(it.first)}, {"detail", QCoreApplication::translate(
                                                                                     "ConnectionProfiles", it.second)}});
    return a;
}

/// Base URL of a type that stores a URL field (url / node / host / endpoint …).
QUrl cp_base(const QJsonObject& cfg, std::initializer_list<const char*> keys) {
    for (const char* k : keys) {
        const QString v = cp_str(cfg, k);
        if (!v.isEmpty())
            return url_from_host(v, -1);
    }
    return QUrl();
}

void cp_add_basic_if(HttpCall& c, const QJsonObject& cfg) {
    const QString user = cp_str(cfg, "username");
    if (!user.isEmpty()) {
        const QString pass = cfg.value(QStringLiteral("password")).toString();
        c.headers.emplaceBack("Authorization", cp_basic(user, pass));
        if (!pass.isEmpty())
            c.secrets << pass;
    }
}

/// The SQL text for "rows of `entity`" unless the user typed a query.
QString cp_select(const QString& entity, const QString& query, int limit, QChar quote = QLatin1Char('"')) {
    if (!query.trimmed().isEmpty())
        return query.trimmed();
    QString e = entity;
    if (!e.contains(QLatin1Char('.')) && !e.contains(quote))
        e = quote + QString(e).replace(quote, QString(2, quote)) + quote;
    return QStringLiteral("SELECT * FROM %1 LIMIT %2").arg(e).arg(limit);
}

TypeSupport cp_type(const char* id, DriverKind d, const char* profile, bool list, bool fetch, const char* entity,
                    const char* qlabel, const char* qhint, const char* parity) {
    TypeSupport t;
    t.type_id = QString::fromLatin1(id);
    t.driver = d;
    t.profile = QString::fromLatin1(profile);
    t.supported = d != DriverKind::None;
    t.can_list = list;
    t.can_fetch = fetch;
    t.entity_label = QCoreApplication::translate("ConnectionProfiles", entity);
    t.query_label = QCoreApplication::translate("ConnectionProfiles", qlabel);
    t.query_hint = QString::fromUtf8(qhint);
    t.parity = QString::fromUtf8(parity);
    return t;
}

TypeSupport cp_none(const char* id, const char* reason, const char* fix) {
    TypeSupport t;
    t.type_id = QString::fromLatin1(id);
    t.driver = DriverKind::None;
    t.supported = false;
    t.reason = QCoreApplication::translate("ConnectionProfiles", reason);
    t.fix = QCoreApplication::translate("ConnectionProfiles", fix);
    return t;
}

const char* const kEnterprise = "This vendor's API needs an enterprise contract and an OAuth / entitlement sign-in that "
                                "this build has no client for.";
const char* const kEnterpriseFix = "Export the data from the vendor's own tool (CSV / Excel) and add it as a file "
                                   "source, or use a catalogue connector with a keyless or free-key alternative.";
const char* const kOpenBanking = "Open-banking data needs an end-user consent flow (OAuth / Link) that the terminal "
                                 "does not run.";
const char* const kOpenBankingFix = "Download statements as CSV from the provider's dashboard and add them as a CSV "
                                    "file source.";
const char* const kArrow = "ORC and Avro files are not read: the local file connector reads CSV, JSON, Excel, "
                           "Parquet and Arrow / Feather.";
const char* const kArrowFix = "Convert the file to Parquet or CSV, e.g. python -c \"import pyarrow.orc as o, "
                              "pyarrow.parquet as pq; pq.write_table(o.read_table('in.orc'), 'out.parquet')\", and add "
                              "it as a Parquet or CSV file source.";

QHash<QString, TypeSupport> cp_build_table() {
    using D = DriverKind;
    QHash<QString, TypeSupport> m;
    auto add = [&m](TypeSupport t) { m.insert(t.type_id, std::move(t)); };

    // ── APIs & streams ──
    add(cp_type("rest-api", D::Http, "rest", false, true, "Path", "Query string", "symbol=AAPL&limit=100",
                "curl -H 'Authorization: Bearer <token>' https://host/path"));
    add(cp_type("graphql", D::Http, "graphql", true, true, "Field", "GraphQL query", "{ prices { date close } }",
                "curl -X POST -H 'Content-Type: application/json' -d '{\"query\":\"{__typename}\"}' <endpoint>"));
    add(cp_type("odata", D::Http, "odata", true, true, "Entity set", "$filter", "Symbol eq 'AAPL'",
                "curl '<service root>/<EntitySet>?$top=10&$format=json'"));
    add(cp_type("soap", D::Http, "soap", true, true, "Operation", "SOAP envelope", "<soap:Envelope …>",
                "curl -X POST -H 'Content-Type: text/xml' --data @envelope.xml <endpoint>"));
    add(cp_type("sse", D::EventStream, "sse", false, true, "Path", "Query string", "",
                "curl -N -H 'Accept: text/event-stream' <url>"));
    add(cp_type("websocket", D::WebSocket, "websocket", false, true, "", "Subscribe message",
                "{\"op\":\"subscribe\",\"channel\":\"ticker\"}", "websocat <url>"));
    add(cp_type("apache-flink", D::Http, "flink", true, true, "Job id", "", "",
                "curl <job manager>/overview"));
    add(cp_type("rabbitmq", D::Http, "rabbitmq", true, true, "Queue", "", "",
                "curl -u user:pass http://host:15672/api/overview"));
    add(cp_none("grpc", "gRPC needs the service's .proto contract compiled into a client; this build has no gRPC client.",
                "Expose the service through grpc-gateway or another HTTP/JSON endpoint and add it as a REST API "
                "source."));
    add(cp_none("mqtt", "This build has no MQTT client.",
                "Enable the broker's WebSocket listener with a JSON bridge (or an HTTP bridge) and add it as a "
                "WebSocket or REST API source."));
    add(cp_none("kafka", "Kafka's binary protocol needs a Kafka client library, which this build does not ship.",
                "Run the Kafka REST Proxy and add it as a REST API source (GET /topics, consumer endpoints)."));
    add(cp_none("redpanda", "Kafka's binary protocol needs a Kafka client library, which this build does not ship.",
                "Use Redpanda's built-in HTTP Proxy (port 8082) and add it as a REST API source."));
    add(cp_none("nats", "This build has no NATS client.",
                "Enable a NATS websocket listener (websocket { port: 8080, no_tls: true } in nats-server.conf) and "
                "add it as a WebSocket source with the subscribe message: CONNECT {} then SUB <subject> 1."));
    add(cp_none("fix-protocol", "FIX is a stateful order / market-data session protocol; the terminal does not run "
                                "FIX sessions.",
                "Use your broker's REST / WebSocket market-data API, or the broker connections in Equity / Crypto "
                "Trading."));

    // ── Files ──
    add(cp_type("csv", D::File, "csv", false, true, "", "", "", "head -5 file.csv"));
    add(cp_type("json", D::File, "json", false, true, "", "", "", "python -m json.tool file.json"));
    add(cp_type("xml", D::File, "xml", false, true, "", "", "", "xmllint --format file.xml"));
    add(cp_type("excel", D::File, "excel", true, true, "Sheet", "", "", "python -c \"import openpyxl\""));
    // Parquet / Feather / Arrow IPC: the keyless catalogue connector data_local_file (pyarrow).
    for (const char* id : {"parquet", "feather", "arrow-ipc", "geoparquet"})
        add(cp_type(id, D::File, "connector", false, true, "", "", "",
                    "python scripts/fincept_connector.py run local_file_data.py read <path> 20"));
    for (const char* id : {"avro", "orc"})
        add(cp_none(id, kArrow, kArrowFix));
    add(cp_none("delta-lake", "Delta Lake tables are Parquet files plus a transaction log; the log is not read here.",
                "Read one snapshot's Parquet files as a Parquet source, or export the table to Parquet / CSV with "
                "deltalake (python -m pip install deltalake) and add that file."));
    add(cp_type("apache-iceberg", D::Http, "iceberg", true, true, "Table", "", "",
                "curl <catalog uri>/v1/config"));
    add(cp_none("ftp", "Qt 6 has no FTP client and this build ships none.",
                "Download the file (curl -O ftp://host/path/file.csv) and add it as a file source."));
    add(cp_none("sftp", "This build has no SSH / SFTP client.",
                "Copy the file locally (scp user@host:/path/file.csv .) and add it as a file source."));

    // ── Relational (Qt SQL drivers; availability is checked at run time per platform) ──
    add(cp_type("sqlite", D::Sql, "sqlite", true, true, "Table", "SQL", "SELECT * FROM prices LIMIT 100",
                "sqlite3 file.db '.tables'"));
    for (const char* id : {"postgresql", "cockroachdb", "timescaledb"})
        add(cp_type(id, D::Sql, "postgres", true, true, "Table", "SQL", "SELECT * FROM prices LIMIT 100",
                    "psql -h host -p 5432 -U user -d db -c 'SELECT 1'"));
    add(cp_type("redshift", D::Sql, "postgres", true, true, "Table", "SQL", "SELECT * FROM sales LIMIT 100",
                "psql -h cluster.region.redshift.amazonaws.com -p 5439 -U user -d dev"));
    for (const char* id : {"mysql", "mariadb"})
        add(cp_type(id, D::Sql, "mysql", true, true, "Table", "SQL", "SELECT * FROM prices LIMIT 100",
                    "mysql -h host -P 3306 -u user -p db -e 'SELECT 1'"));
    add(cp_type("mssql", D::Sql, "mssql", true, true, "Table", "SQL", "SELECT TOP 100 * FROM prices",
                "sqlcmd -S host,1433 -U user -P *** -Q 'SELECT 1'"));
    add(cp_type("synapse", D::Sql, "mssql", true, true, "Table", "SQL", "SELECT TOP 100 * FROM prices",
                "sqlcmd -S server.sql.azuresynapse.net -U user -d db -Q 'SELECT 1'"));
    add(cp_type("oracle", D::Sql, "oracle", true, true, "Table", "SQL", "SELECT * FROM prices FETCH FIRST 100 ROWS ONLY",
                "sqlplus user/***@host:1521/ORCL"));
    add(cp_type("snowflake", D::Sql, "snowflake", true, true, "Table", "SQL", "SELECT * FROM prices LIMIT 100",
                "snowsql -a <account> -u <user>"));
    add(cp_type("vertica", D::Sql, "vertica", true, true, "Table", "SQL", "SELECT * FROM prices LIMIT 100",
                "vsql -h host -p 5433 -U user"));

    // ── NoSQL ──
    add(cp_type("redis", D::Redis, "redis", true, true, "Key", "Key pattern", "price:*", "redis-cli -h host PING"));
    add(cp_type("memcached", D::Memcached, "memcached", false, true, "", "", "", "printf 'stats\\r\\nquit\\r\\n' | nc host 11211"));
    add(cp_type("couchdb", D::Http, "couchdb", true, true, "Database", "Mango selector (JSON)", "{\"symbol\": \"AAPL\"}",
                "curl -u user:pass http://host:5984/_all_dbs"));
    add(cp_type("neo4j", D::Http, "neo4j", true, true, "Label", "Cypher", "MATCH (n) RETURN n LIMIT 25",
                "curl -u neo4j:*** -H 'Content-Type: application/json' -d '{\"statements\":[{\"statement\":\"RETURN 1\"}]}' "
                "http://host:7474/db/neo4j/tx/commit"));
    add(cp_type("arangodb", D::Http, "arangodb", true, true, "Collection", "AQL", "FOR d IN prices LIMIT 10 RETURN d",
                "curl -u root:*** http://host:8529/_api/version"));
    add(cp_type("etcd", D::Http, "etcd", true, true, "Key prefix", "", "", "etcdctl get --prefix ''"));
    add(cp_type("orientdb", D::Http, "orientdb", true, true, "Class", "SQL", "SELECT FROM Price LIMIT 20",
                "curl -u root:*** http://host:2480/listDatabases"));
    add(cp_none("mongodb", "MongoDB's wire protocol needs a driver this build does not ship.",
                "Export the collection (mongoexport --jsonArray -c prices -o prices.json) and add it as a JSON file "
                "source, or expose it over HTTP (Atlas Data API) and add a REST API source."));
    add(cp_none("cassandra", "Cassandra's CQL protocol needs a driver this build does not ship.",
                "Export with cqlsh: COPY ks.table TO 'table.csv' WITH HEADER = true; then add the CSV as a file "
                "source."));
    add(cp_none("scylladb", "ScyllaDB speaks Cassandra's CQL protocol, which needs a driver this build does not ship.",
                "Export with cqlsh: COPY ks.table TO 'table.csv' WITH HEADER = true; then add the CSV as a file "
                "source."));
    add(cp_none("dynamodb", "DynamoDB's API is not implemented in this build.",
                "Export the table (aws dynamodb scan --table-name T --output json > t.json) and add it as a JSON file "
                "source."));
    add(cp_none("rethinkdb", "RethinkDB's protocol needs a driver this build does not ship.",
                "Export with rethinkdb export -e db.table --format json and add it as a JSON file source."));
    add(cp_none("firestore", "Google Cloud APIs need a service-account OAuth sign-in this build does not implement.",
                "Export the collection (gcloud firestore export) to JSON and add it as a JSON file source."));
    add(cp_none("hbase", "HBase's native client protocol is not available in this build.",
                "Start the HBase REST server (hbase rest start) and add it as a REST API source (GET /<table>/*)."));
    add(cp_none("cosmosdb", "Cosmos DB's REST API (account-key request signing) is not implemented in this build.",
                "Export the container to JSON (Azure Data Studio / dt.exe) and add it as a JSON file source."));

    // ── Time series ──
    add(cp_type("influxdb", D::Http, "influxdb", true, true, "Measurement", "Flux",
                "from(bucket: \"b\") |> range(start: -1d) |> limit(n: 100)",
                "curl -H 'Authorization: Token ***' '<url>/api/v2/buckets?org=<org>'"));
    add(cp_type("questdb", D::Http, "questdb", true, true, "Table", "SQL", "SELECT * FROM trades LIMIT 100",
                "curl -G --data-urlencode 'query=SELECT 1' http://host:9000/exec"));
    add(cp_type("prometheus", D::Http, "prometheus", true, true, "Metric", "PromQL", "rate(http_requests_total[5m])",
                "curl '<url>/api/v1/query?query=up'"));
    add(cp_type("victoriametrics", D::Http, "prometheus", true, true, "Metric", "PromQL (MetricsQL)", "up",
                "curl '<url>/api/v1/query?query=up'"));
    add(cp_type("opentsdb", D::Http, "opentsdb", true, true, "Metric", "", "", "curl <url>/api/version"));
    add(cp_type("clickhouse", D::Http, "clickhouse", true, true, "Table", "SQL", "SELECT * FROM trades LIMIT 100",
                "curl 'http://host:8123/?query=SELECT%20version()'"));
    add(cp_type("kdb", D::Http, "kdb", true, true, "Table", "q expression", "select[100] from trade",
                "curl 'http://host:5000/q.csv?tables[]'"));

    // ── Search & warehouses ──
    for (const char* id : {"elasticsearch", "opensearch"})
        add(cp_type(id, D::Http, "elasticsearch", true, true, "Index", "Query string", "symbol:AAPL AND price:>100",
                    "curl -u user:pass <node>/_cat/indices?format=json"));
    add(cp_type("solr", D::Http, "solr", true, true, "Collection", "Query (q)", "symbol:AAPL",
                "curl '<host>/solr/admin/cores?action=STATUS&wt=json'"));
    add(cp_type("algolia", D::Http, "algolia", true, true, "Index", "Search text", "apple",
                "curl -H 'X-Algolia-Application-Id: <app>' -H 'X-Algolia-API-Key: ***' https://<app>-dsn.algolia.net/1/indexes"));
    add(cp_type("meilisearch", D::Http, "meilisearch", true, true, "Index", "Search text", "apple",
                "curl -H 'Authorization: Bearer ***' <host>/indexes"));
    add(cp_type("databricks", D::Http, "databricks", true, true, "Table", "SQL", "SELECT * FROM samples.nyctaxi.trips LIMIT 100",
                "curl -H 'Authorization: Bearer ***' https://<host>/api/2.0/sql/warehouses/<id>"));
    add(cp_none("bigquery", "Google Cloud APIs need a service-account OAuth sign-in this build does not implement.",
                "Export the table (bq extract --destination_format CSV) and add the CSV as a file source."));

    // ── Cloud object stores ──
    for (const char* id : {"aws-s3", "digitalocean-spaces", "minio", "wasabi", "cloudflare-r2", "oracle-cloud-storage",
                           "backblaze-b2"})
        add(cp_type(id, D::Http, "s3", true, true, "Object key", "Prefix", "data/2024/",
                    "aws s3 ls s3://<bucket>/ --endpoint-url <endpoint>"));
    add(cp_type("ibm-cloud-storage", D::Http, "ibm-cos", true, true, "Object key", "Prefix", "data/",
                "ibmcloud cos objects --bucket <bucket>"));
    add(cp_type("azure-blob", D::Http, "azure-blob", true, true, "Blob", "Prefix", "data/",
                "az storage blob list --account-name <acct> --container-name <c>"));
    add(cp_none("google-cloud-storage", "Google Cloud APIs need a service-account OAuth sign-in this build does not "
                                        "implement.",
                "Create HMAC keys (gcloud storage hmac create <service-account>) and add the bucket as a MinIO / "
                "S3-compatible connection with endpoint https://storage.googleapis.com."));

    // ── Market data: catalogue connectors (one runner, the connector contract) ──
    const std::pair<const char*, const char*> catalogue[] = {
        {"yahoo-finance", "data_yfinance"}, {"alpha-vantage", "data_alphavantage"}, {"eod-historical", "data_eodhd"},
        {"coingecko", "data_coingecko"},    {"coinmarketcap", "data_coinmarketcap"}, {"finnhub", "data_finnhub"},
        {"intrinio", "data_intrinio"},      {"marketstack", "data_marketstack"},    {"openfigi", "data_openfigi"},
        {"polygon-io", "data_polygon_io"},  {"tiingo", "data_tiingo"},             {"twelve-data", "data_twelve"},
        {"quandl", "data_quandl_nasdaq"},
    };
    for (const auto& c : catalogue)
        add(cp_type(c.first, D::Catalogue, c.second, true, true, "Command", "Arguments", "AAPL",
                    "python scripts/fincept_connector.py run <script> <command> [args]"));
    // ── Market data over plain HTTP ──
    add(cp_type("binance", D::Http, "binance", true, true, "Endpoint", "Symbol", "BTCUSDT",
                "curl 'https://api.binance.com/api/v3/klines?symbol=BTCUSDT&interval=1d&limit=5'"));
    add(cp_type("coinbase", D::Http, "coinbase", true, true, "Endpoint", "Product", "BTC-USD",
                "curl https://api.exchange.coinbase.com/products/BTC-USD/candles?granularity=86400"));
    add(cp_type("kraken", D::Http, "kraken", true, true, "Endpoint", "Pair", "XBTUSD",
                "curl 'https://api.kraken.com/0/public/OHLC?pair=XBTUSD&interval=1440'"));
    add(cp_type("tradier", D::Http, "tradier", true, true, "Endpoint", "Symbols", "AAPL,MSFT",
                "curl -H 'Authorization: Bearer ***' -H 'Accept: application/json' https://api.tradier.com/v1/markets/clock"));
    add(cp_type("finage", D::Http, "finage", true, true, "Endpoint", "Symbol", "AAPL",
                "curl 'https://api.finage.co.uk/last/stock/AAPL?apikey=***'"));
    add(cp_type("newsapi", D::Http, "newsapi", true, true, "Endpoint", "Search", "earnings",
                "curl -H 'X-Api-Key: ***' https://newsapi.org/v2/top-headlines/sources"));
    add(cp_type("adanos-market-sentiment", D::Http, "adanos", true, true, "Source", "Tickers", "AAPL,MSFT",
                "curl -H 'X-API-Key: ***' 'https://api.adanos.org/news/stocks/v1/compare?tickers=AAPL&days=7'"));
    add(cp_none("fincept", "The Fincept Data API already powers the terminal's own screens with your sign-in; a "
                           "separate connection adds nothing.",
                "Use the Markets / Economics screens, or a catalogue connector, for the same data."));
    for (const char* id : {"reuters", "refinitiv", "factset", "pitchbook", "nasdaq-totalview"})
        add(cp_none(id, kEnterprise, kEnterpriseFix));
    for (const char* id : {"ravenpack", "bloomberg-second-measure", "safegraph", "orbital-insight",
                           "refinitiv-tick-history", "earnest-research", "thinknum", "revelio-labs"})
        add(cp_none(id, kEnterprise, kEnterpriseFix));
    for (const char* id : {"plaid", "mx", "finicity", "truelayer", "fdx-api"})
        add(cp_none(id, kOpenBanking, kOpenBankingFix));
    return m;
}

const QHash<QString, TypeSupport>& cp_table() {
    static const QHash<QString, TypeSupport> kTable = cp_build_table();
    return kTable;
}

// ── Post-processing helpers ───────────────────────────────────────────────

QJsonValue cp_parse_json(const QByteArray& body, SourceError* err) {
    QJsonParseError pe{};
    const QJsonDocument doc = QJsonDocument::fromJson(body, &pe);
    if (pe.error != QJsonParseError::NoError) {
        if (err)
            *err = SourceError::make(ErrorCode::Unknown,
                                     cp_tr("The answer is not JSON: %1").arg(QString::fromUtf8(body.left(120)).simplified()));
        return QJsonValue(QJsonValue::Undefined);
    }
    return doc.isArray() ? QJsonValue(doc.array()) : QJsonValue(doc.object());
}

PostResult cp_table_result(DataTable t, const QString& summary = QString()) {
    PostResult r;
    if (t.columns.isEmpty()) {
        r.error = SourceError::make(ErrorCode::NoData, cp_tr("The answer holds no rows"));
        r.summary = summary;
        return r;
    }
    if (t.types.size() != t.columns.size()) {
        t.types = QVector<ColumnType>(t.columns.size(), ColumnType::Unknown);
        infer_column_types(t);
    }
    r.ok = true;
    r.summary = summary;
    r.data.tables << t;
    return r;
}

/// Flatten one level of nested objects into dotted columns (Elasticsearch _source, Neo4j nodes).
QJsonObject cp_flatten(const QJsonObject& o, const QString& prefix = QString(), int depth = 0) {
    QJsonObject out;
    for (auto it = o.constBegin(); it != o.constEnd(); ++it) {
        const QString key = prefix.isEmpty() ? it.key() : prefix + QLatin1Char('.') + it.key();
        if (it->isObject() && depth < 1) {
            const QJsonObject inner = cp_flatten(it->toObject(), key, depth + 1);
            for (auto jt = inner.constBegin(); jt != inner.constEnd(); ++jt)
                out.insert(jt.key(), jt.value());
        } else {
            out.insert(key, it.value());
        }
    }
    return out;
}

QString cp_first_error_message(const QJsonValue& errors) {
    const QJsonArray a = errors.toArray();
    if (a.isEmpty())
        return QString();
    const QJsonValue e = a.first();
    if (e.isObject()) {
        const QJsonObject o = e.toObject();
        for (const char* k : {"message", "error", "reason", "detail"})
            if (o.value(QLatin1String(k)).isString())
                return o.value(QLatin1String(k)).toString();
        return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
    }
    return e.toString();
}

/// Influx annotated CSV: drop "#…" annotation lines and the bookkeeping columns.
QByteArray cp_strip_influx_annotations(const QByteArray& body) {
    QByteArray out;
    bool header_seen = false;
    for (const QByteArray& line : body.split('\n')) {
        const QByteArray l = line.trimmed();
        if (l.startsWith('#'))
            continue;
        if (l.isEmpty())
            continue;
        if (header_seen && l.startsWith(",result,table"))
            continue; // a second table's header repeats
        header_seen = true;
        out += (l.startsWith(',') ? l.mid(1) : l) + '\n';
    }
    return out;
}

} // namespace

TypeSupport type_support(const QString& type_id) {
    const auto it = cp_table().constFind(type_id);
    if (it != cp_table().constEnd())
        return it.value();
    TypeSupport t;
    t.type_id = type_id;
    t.reason = cp_tr("Unknown connection type \"%1\".").arg(type_id);
    return t;
}

QStringList known_type_ids() {
    QStringList ids = cp_table().keys();
    std::sort(ids.begin(), ids.end());
    return ids;
}

QUrl join_url(const QString& base, const QString& path) {
    const QString p = path.trimmed();
    if (p.startsWith(QLatin1String("http://"), Qt::CaseInsensitive) ||
        p.startsWith(QLatin1String("https://"), Qt::CaseInsensitive))
        return QUrl(p);
    QString b = base.trimmed();
    if (p.isEmpty())
        return QUrl(b);
    while (b.endsWith(QLatin1Char('/')))
        b.chop(1);
    if (p.startsWith(QLatin1Char('?')))
        return QUrl(b + p);
    return QUrl(b + (p.startsWith(QLatin1Char('/')) ? p : QLatin1Char('/') + p));
}

QUrl url_from_host(const QString& host_or_url, int port, bool https) {
    QString h = host_or_url.trimmed();
    if (h.isEmpty())
        return QUrl();
    if (!h.contains(QLatin1String("://")))
        h = (https ? QStringLiteral("https://") : QStringLiteral("http://")) + h;
    QUrl u(h);
    if (port > 0 && u.port(-1) < 0)
        u.setPort(port);
    return u;
}

bool apply_generic_auth(HttpCall& call, const QJsonObject& cfg, SourceError* error) {
    QString type = cp_str(cfg, "authType").toLower();
    QString token;
    for (const char* k : {"apiKey", "apiToken", "token", "authToken", "accessToken"}) {
        token = cfg.value(QLatin1String(k)).toString().trimmed();
        if (!token.isEmpty())
            break;
    }
    if (type.isEmpty())
        type = token.isEmpty() ? QStringLiteral("none") : QStringLiteral("bearer");
    if (!token.isEmpty())
        call.secrets << token;
    if (type == QLatin1String("none")) {
        // nothing
    } else if (type == QLatin1String("bearer") || type == QLatin1String("oauth2") || type == QLatin1String("token")) {
        if (token.isEmpty()) {
            if (error)
                *error = SourceError::make(ErrorCode::MissingKey, cp_tr("Enter the bearer token"), QStringLiteral("apiKey"));
            return false;
        }
        call.headers.emplaceBack("Authorization", "Bearer " + token.toUtf8());
    } else if (type == QLatin1String("apikey") || type == QLatin1String("api_key") || type == QLatin1String("header") ||
               type == QLatin1String("query")) {
        if (token.isEmpty()) {
            if (error)
                *error = SourceError::make(ErrorCode::MissingKey, cp_tr("Enter the API key"), QStringLiteral("apiKey"));
            return false;
        }
        QString in = cp_str(cfg, "keyIn").toLower();
        if (type == QLatin1String("query"))
            in = QStringLiteral("query");
        QString name = cp_str(cfg, "keyName");
        if (in == QLatin1String("query")) {
            if (name.isEmpty())
                name = QStringLiteral("api_key");
            QUrlQuery q(call.url);
            q.removeAllQueryItems(name);
            q.addQueryItem(name, token);
            call.url.setQuery(q);
        } else {
            if (name.isEmpty())
                name = QStringLiteral("X-API-Key");
            call.headers.emplaceBack(name.toUtf8(), token.toUtf8());
        }
    } else if (type == QLatin1String("basic")) {
        const QString user = cp_str(cfg, "username");
        const QString pass = cfg.value(QStringLiteral("password")).toString();
        if (user.isEmpty() && token.isEmpty()) {
            if (error)
                *error = SourceError::make(ErrorCode::MissingKey, cp_tr("Enter the user name and password"),
                                           QStringLiteral("username"));
            return false;
        }
        // Older connections kept "user:password" in the key field.
        const QByteArray creds = user.isEmpty() ? token.toUtf8() : (user + QLatin1Char(':') + pass).toUtf8();
        call.headers.emplaceBack("Authorization", "Basic " + creds.toBase64());
        if (!pass.isEmpty())
            call.secrets << pass;
    } else {
        if (error)
            *error = SourceError::make(ErrorCode::BadArgs, cp_tr("Unknown authentication type \"%1\"").arg(type),
                                       QStringLiteral("authType"));
        return false;
    }

    // Custom headers: a JSON object, or "Name: value" lines.
    const QString headers = cfg.value(QStringLiteral("headers")).toString().trimmed();
    if (!headers.isEmpty()) {
        const QJsonDocument doc = QJsonDocument::fromJson(headers.toUtf8());
        if (doc.isObject()) {
            const QJsonObject h = doc.object();
            for (auto it = h.constBegin(); it != h.constEnd(); ++it) {
                call.headers.emplaceBack(it.key().toUtf8(), it.value().toString().toUtf8());
                call.secrets << it.value().toString();
            }
        } else {
            for (const QString& line : headers.split(QLatin1Char('\n'))) {
                const int colon = int(line.indexOf(QLatin1Char(':')));
                if (colon <= 0) {
                    if (!line.trimmed().isEmpty()) {
                        if (error)
                            *error = SourceError::make(ErrorCode::BadArgs,
                                                       cp_tr("Custom headers must be a JSON object or \"Name: value\" "
                                                             "lines"),
                                                       QStringLiteral("headers"));
                        return false;
                    }
                    continue;
                }
                const QString value = line.mid(colon + 1).trimmed();
                call.headers.emplaceBack(line.left(colon).trimmed().toUtf8(), value.toUtf8());
                call.secrets << value;
            }
        }
    }
    return true;
}

HttpPlan http_plan(const TypeSupport& sup, SourceOp op, const QJsonObject& cfg, const QString& entity_in,
                   const QString& query, int limit) {
    const QString p = sup.profile;
    const QString entity = entity_in.trimmed();
    limit = std::clamp(limit, 1, 100000);
    HttpCall c;
    c.headers.emplaceBack("Accept", "application/json");
    SourceError err;

    // ── Generic APIs ──
    if (p == QLatin1String("rest")) {
        const QString base = cp_str(cfg, "baseUrl");
        if (base.isEmpty())
            return cp_bad(QStringLiteral("baseUrl"), cp_tr("Enter the base URL"));
        QString path = entity.isEmpty() ? cp_str(cfg, "path") : entity;
        c.url = join_url(base, path);
        if (!query.trimmed().isEmpty()) {
            QUrlQuery q(c.url);
            for (const auto& item : QUrlQuery(query.trimmed()).queryItems(QUrl::FullyDecoded))
                q.addQueryItem(item.first, item.second);
            c.url.setQuery(q);
        }
        c.headers.clear();
        c.headers.emplaceBack("Accept", "application/json, text/csv;q=0.9, application/xml;q=0.8, */*;q=0.5");
        const QString method = cp_str(cfg, "method").toUpper();
        if (method == QLatin1String("POST") || method == QLatin1String("PUT")) {
            c.method = method.toUtf8();
            c.body = cfg.value(QStringLiteral("body")).toString().toUtf8();
            c.headers.emplaceBack("Content-Type", "application/json");
        }
        if (!apply_generic_auth(c, cfg, &err)) {
            HttpPlan bad;
            bad.error = err;
            return bad;
        }
        c.what = c.url.host();
        c.parse.format = text_format_from_name(cp_str(cfg, "format"));
        c.parse.rows_path = cp_str(cfg, "rowsPath");
        c.parse.file_name = c.url.path();
        c.parse.max_rows = op == SourceOp::Test ? 50 : limit;
        return cp_one(c, cp_tr("the endpoint answered"));
    }
    if (p == QLatin1String("sse")) {
        const QString url = cp_str(cfg, "url");
        if (url.isEmpty())
            return cp_bad(QStringLiteral("url"), cp_tr("Enter the event-stream URL"));
        c.url = join_url(url, entity);
        c.headers.clear();
        c.headers.emplaceBack("Accept", "text/event-stream");
        c.headers.emplaceBack("Cache-Control", "no-cache");
        if (!apply_generic_auth(c, cfg, &err)) {
            HttpPlan bad;
            bad.error = err;
            return bad;
        }
        c.what = c.url.host();
        c.parse.format = TextFormat::EventStream;
        c.parse.max_rows = op == SourceOp::Test ? 1 : std::min(limit, cp_int(cfg, "maxEvents", 50));
        return cp_one(c);
    }
    if (p == QLatin1String("graphql")) {
        const QString ep = cp_str(cfg, "endpoint");
        if (ep.isEmpty())
            return cp_bad(QStringLiteral("endpoint"), cp_tr("Enter the GraphQL endpoint"));
        c.url = QUrl(ep);
        c.method = "POST";
        c.headers.emplaceBack("Content-Type", "application/json");
        QJsonObject cfg2 = cfg;
        if (!cp_str(cfg, "authToken").isEmpty() && cp_str(cfg, "authType").isEmpty()) {
            QString t = cp_str(cfg, "authToken");
            if (t.startsWith(QLatin1String("Bearer "), Qt::CaseInsensitive))
                t = t.mid(7).trimmed();
            cfg2.insert(QStringLiteral("authToken"), t);
        }
        if (!apply_generic_auth(c, cfg2, &err)) {
            HttpPlan bad;
            bad.error = err;
            return bad;
        }
        QString q;
        if (op == SourceOp::Test)
            q = QStringLiteral("{ __typename }");
        else if (op == SourceOp::List)
            q = QStringLiteral("{ __schema { queryType { fields { name description type { name kind } } } } }");
        else
            q = !query.trimmed().isEmpty() ? query.trimmed()
                : !entity.isEmpty()        ? QStringLiteral("{ %1 }").arg(entity)
                                           : cp_str(cfg, "query");
        if (q.isEmpty())
            return cp_bad(QStringLiteral("query"), cp_tr("Enter a GraphQL query"));
        QJsonObject body{{"query", q}};
        const QJsonDocument vars = QJsonDocument::fromJson(cfg.value(QStringLiteral("variables")).toString().toUtf8());
        if (op == SourceOp::Fetch && vars.isObject())
            body.insert(QStringLiteral("variables"), vars.object());
        c.body = cp_json(body);
        c.what = c.url.host();
        c.post = QStringLiteral("graphql");
        c.parse.rows_path = cp_str(cfg, "rowsPath");
        c.parse.max_rows = limit;
        return cp_one(c);
    }
    if (p == QLatin1String("odata")) {
        const QString root = cp_str(cfg, "serviceRoot");
        if (root.isEmpty())
            return cp_bad(QStringLiteral("serviceRoot"), cp_tr("Enter the OData service root URL"));
        QJsonObject cfg2 = cfg;
        if (cp_str(cfg, "authType") == QLatin1String("oauth2"))
            cfg2.insert(QStringLiteral("authType"), QStringLiteral("bearer"));
        if (op == SourceOp::Fetch) {
            const QString set = entity.isEmpty() ? cp_str(cfg, "entitySet") : entity;
            if (set.isEmpty())
                return cp_bad(QStringLiteral("entitySet"), cp_tr("Pick an entity set"));
            c.url = join_url(root, set);
            QUrlQuery q(c.url);
            q.addQueryItem(QStringLiteral("$top"), QString::number(limit));
            if (!query.trimmed().isEmpty())
                q.addQueryItem(QStringLiteral("$filter"), query.trimmed());
            c.url.setQuery(q);
            c.parse.rows_path = QStringLiteral("value");
        } else {
            c.url = join_url(root, QStringLiteral("/"));
            c.parse.rows_path = QStringLiteral("value");
        }
        if (!apply_generic_auth(c, cfg2, &err)) {
            HttpPlan bad;
            bad.error = err;
            return bad;
        }
        c.what = c.url.host();
        c.parse.max_rows = limit;
        return cp_one(c, cp_tr("the OData service document was read"));
    }
    if (p == QLatin1String("soap")) {
        const QString wsdl = cp_str(cfg, "wsdlUrl");
        if (wsdl.isEmpty())
            return cp_bad(QStringLiteral("wsdlUrl"), cp_tr("Enter the WSDL URL"));
        cp_add_basic_if(c, cfg);
        c.headers.removeIf([](const auto& h) { return h.first == "Accept"; });
        c.what = QUrl(wsdl).host();
        if (op != SourceOp::Fetch) {
            c.url = QUrl(wsdl);
            c.post = op == SourceOp::Test ? QStringLiteral("wsdl_test") : QStringLiteral("wsdl_ops");
            return cp_one(c);
        }
        const QString envelope = query.trimmed().isEmpty() ? cfg.value(QStringLiteral("envelope")).toString().trimmed()
                                                            : query.trimmed();
        if (envelope.isEmpty())
            return cp_bad(QStringLiteral("envelope"), cp_tr("Paste the SOAP request envelope"));
        QUrl endpoint(wsdl);
        endpoint.setQuery(QString());
        c.url = endpoint;
        c.method = "POST";
        c.body = envelope.toUtf8();
        c.headers.emplaceBack("Content-Type", "text/xml; charset=utf-8");
        const QString action = entity.isEmpty() ? cp_str(cfg, "soapAction") : entity;
        if (!action.isEmpty())
            c.headers.emplaceBack("SOAPAction", '"' + action.toUtf8() + '"');
        c.parse.format = TextFormat::Xml;
        c.parse.rows_path = cp_str(cfg, "recordElement");
        c.parse.max_rows = limit;
        return cp_one(c);
    }

    // ── Search ──
    if (p == QLatin1String("elasticsearch")) {
        const QUrl base = cp_base(cfg, {"node", "url", "host"});
        if (base.isEmpty())
            return cp_bad(QStringLiteral("node"), cp_tr("Enter the node URL"));
        cp_add_basic_if(c, cfg);
        c.what = base.host();
        if (op == SourceOp::Test) {
            c.url = join_url(base.toString(), QStringLiteral("/"));
            c.post = QStringLiteral("es_root");
            return cp_one(c);
        }
        if (op == SourceOp::List) {
            c.url = join_url(base.toString(), QStringLiteral("/_cat/indices?format=json&h=index,docs.count,store.size,health"));
            return cp_one(c);
        }
        const QString index = entity.isEmpty() ? cp_str(cfg, "index") : entity;
        if (index.isEmpty())
            return cp_bad(QStringLiteral("index"), cp_tr("Pick an index"));
        c.url = join_url(base.toString(), QStringLiteral("/%1/_search").arg(index));
        c.method = "POST";
        c.headers.emplaceBack("Content-Type", "application/json");
        QJsonObject q{{"size", limit}};
        if (query.trimmed().isEmpty())
            q.insert(QStringLiteral("query"), QJsonObject{{"match_all", QJsonObject{}}});
        else
            q.insert(QStringLiteral("query"), QJsonObject{{"query_string", QJsonObject{{"query", query.trimmed()}}}});
        c.body = cp_json(q);
        c.post = QStringLiteral("es_hits");
        return cp_one(c);
    }
    if (p == QLatin1String("solr")) {
        const QUrl base = cp_base(cfg, {"host"});
        if (base.isEmpty())
            return cp_bad(QStringLiteral("host"), cp_tr("Enter the Solr URL"));
        QString root = base.toString();
        if (!root.contains(QLatin1String("/solr")))
            root = join_url(root, QStringLiteral("/solr")).toString();
        c.what = base.host();
        if (op == SourceOp::Test) {
            c.url = join_url(root, QStringLiteral("/admin/info/system?wt=json"));
            c.post = QStringLiteral("solr_system");
            return cp_one(c);
        }
        if (op == SourceOp::List) {
            c.url = join_url(root, QStringLiteral("/admin/cores?action=STATUS&wt=json"));
            c.post = QStringLiteral("solr_cores");
            return cp_one(c);
        }
        const QString coll = entity.isEmpty() ? cp_str(cfg, "collection") : entity;
        if (coll.isEmpty())
            return cp_bad(QStringLiteral("collection"), cp_tr("Pick a collection"));
        c.url = join_url(root, QStringLiteral("/%1/select").arg(coll));
        QUrlQuery q;
        q.addQueryItem(QStringLiteral("q"), query.trimmed().isEmpty() ? QStringLiteral("*:*") : query.trimmed());
        q.addQueryItem(QStringLiteral("rows"), QString::number(limit));
        q.addQueryItem(QStringLiteral("wt"), QStringLiteral("json"));
        c.url.setQuery(q);
        c.parse.rows_path = QStringLiteral("response.docs");
        return cp_one(c);
    }
    if (p == QLatin1String("meilisearch")) {
        const QUrl base = cp_base(cfg, {"host"});
        if (base.isEmpty())
            return cp_bad(QStringLiteral("host"), cp_tr("Enter the MeiliSearch URL"));
        const QString key = cp_str(cfg, "apiKey");
        if (!key.isEmpty()) {
            c.headers.emplaceBack("Authorization", "Bearer " + key.toUtf8());
            c.secrets << key;
        }
        c.what = base.host();
        if (op == SourceOp::Test) {
            c.url = join_url(base.toString(), QStringLiteral("/version"));
            c.post = QStringLiteral("summary_keys");
            return cp_one(c);
        }
        if (op == SourceOp::List) {
            c.url = join_url(base.toString(), QStringLiteral("/indexes?limit=200"));
            c.parse.rows_path = QStringLiteral("results");
            return cp_one(c);
        }
        const QString index = entity.isEmpty() ? cp_str(cfg, "index") : entity;
        if (index.isEmpty())
            return cp_bad(QStringLiteral("index"), cp_tr("Pick an index"));
        c.url = join_url(base.toString(), QStringLiteral("/indexes/%1/search").arg(index));
        c.method = "POST";
        c.headers.emplaceBack("Content-Type", "application/json");
        c.body = cp_json(QJsonObject{{"q", query.trimmed()}, {"limit", limit}});
        c.parse.rows_path = QStringLiteral("hits");
        return cp_one(c);
    }
    if (p == QLatin1String("algolia")) {
        const QString app = cp_str(cfg, "appId");
        const QString key = cp_str(cfg, "apiKey");
        if (app.isEmpty())
            return cp_bad(QStringLiteral("appId"), cp_tr("Enter the Algolia application id"));
        if (key.isEmpty())
            return cp_bad(QStringLiteral("apiKey"), cp_tr("Enter the Algolia API key"));
        const QString base = QStringLiteral("https://%1-dsn.algolia.net").arg(app);
        c.headers.emplaceBack("X-Algolia-Application-Id", app.toUtf8());
        c.headers.emplaceBack("X-Algolia-API-Key", key.toUtf8());
        c.secrets << key;
        c.what = QStringLiteral("Algolia");
        if (op != SourceOp::Fetch) {
            c.url = join_url(base, QStringLiteral("/1/indexes"));
            c.parse.rows_path = QStringLiteral("items");
            return cp_one(c);
        }
        const QString index = entity.isEmpty() ? cp_str(cfg, "index") : entity;
        if (index.isEmpty())
            return cp_bad(QStringLiteral("index"), cp_tr("Pick an index"));
        c.url = join_url(base, QStringLiteral("/1/indexes/%1/query").arg(index));
        c.method = "POST";
        c.headers.emplaceBack("Content-Type", "application/json");
        c.body = cp_json(QJsonObject{{"query", query.trimmed()}, {"hitsPerPage", std::min(limit, 1000)}});
        c.parse.rows_path = QStringLiteral("hits");
        return cp_one(c);
    }

    // ── Document / graph / key-value stores ──
    if (p == QLatin1String("couchdb")) {
        const QUrl base = cp_base(cfg, {"url"});
        if (base.isEmpty())
            return cp_bad(QStringLiteral("url"), cp_tr("Enter the CouchDB URL"));
        cp_add_basic_if(c, cfg);
        c.what = base.host();
        if (op == SourceOp::Test) {
            c.url = join_url(base.toString(), QStringLiteral("/_session"));
            c.post = QStringLiteral("couch_session");
            return cp_one(c);
        }
        if (op == SourceOp::List) {
            c.url = join_url(base.toString(), QStringLiteral("/_all_dbs"));
            c.post = QStringLiteral("values_as_names");
            return cp_one(c);
        }
        const QString db = entity.isEmpty() ? cp_str(cfg, "database") : entity;
        if (db.isEmpty())
            return cp_bad(QStringLiteral("database"), cp_tr("Pick a database"));
        if (!query.trimmed().isEmpty()) {
            const QJsonDocument sel = QJsonDocument::fromJson(query.trimmed().toUtf8());
            if (!sel.isObject())
                return cp_bad(QStringLiteral("query"), cp_tr("The Mango selector must be a JSON object"));
            c.url = join_url(base.toString(), QStringLiteral("/%1/_find").arg(db));
            c.method = "POST";
            c.headers.emplaceBack("Content-Type", "application/json");
            c.body = cp_json(QJsonObject{{"selector", sel.object()}, {"limit", limit}});
            c.parse.rows_path = QStringLiteral("docs");
        } else {
            c.url = join_url(base.toString(), QStringLiteral("/%1/_all_docs?include_docs=true&limit=%2").arg(db).arg(limit));
            c.parse.rows_path = QStringLiteral("rows[*].doc");
        }
        return cp_one(c);
    }
    if (p == QLatin1String("neo4j")) {
        QUrl uri(cp_str(cfg, "uri"));
        if (uri.host().isEmpty())
            return cp_bad(QStringLiteral("uri"), cp_tr("Enter the Neo4j URI"));
        QUrl http = uri;
        if (uri.scheme() != QLatin1String("http") && uri.scheme() != QLatin1String("https")) {
            // bolt:// / neo4j:// → the HTTP API of the same server (port 7474, 7473 with TLS).
            const bool tls = uri.scheme().endsWith(QLatin1String("+s")) || uri.scheme().endsWith(QLatin1String("+ssc"));
            http = QUrl(QStringLiteral("%1://%2:%3").arg(tls ? QStringLiteral("https") : QStringLiteral("http"), uri.host())
                            .arg(tls ? 7473 : 7474));
        }
        const QString db = cp_str(cfg, "database").isEmpty() ? QStringLiteral("neo4j") : cp_str(cfg, "database");
        cp_add_basic_if(c, cfg);
        c.url = join_url(http.toString(), QStringLiteral("/db/%1/tx/commit").arg(db));
        c.method = "POST";
        c.headers.emplaceBack("Content-Type", "application/json");
        QString stmt;
        if (op == SourceOp::Test)
            stmt = QStringLiteral("CALL dbms.components() YIELD name, versions, edition RETURN name, versions[0] AS version, edition");
        else if (op == SourceOp::List)
            stmt = QStringLiteral("CALL db.labels() YIELD label RETURN label AS name");
        else
            stmt = !query.trimmed().isEmpty() ? query.trimmed()
                                              : QStringLiteral("MATCH (n:`%1`) RETURN n LIMIT %2").arg(entity).arg(limit);
        if (op == SourceOp::Fetch && query.trimmed().isEmpty() && entity.isEmpty())
            return cp_bad(QStringLiteral("query"), cp_tr("Pick a label or enter a Cypher query"));
        c.body = cp_json(QJsonObject{{"statements", QJsonArray{QJsonObject{{"statement", stmt}}}}});
        c.what = http.host();
        c.post = QStringLiteral("neo4j");
        return cp_one(c);
    }
    if (p == QLatin1String("arangodb")) {
        const QUrl base = cp_base(cfg, {"url"});
        if (base.isEmpty())
            return cp_bad(QStringLiteral("url"), cp_tr("Enter the ArangoDB URL"));
        const QString db = cp_str(cfg, "database").isEmpty() ? QStringLiteral("_system") : cp_str(cfg, "database");
        cp_add_basic_if(c, cfg);
        c.what = base.host();
        const QString root = join_url(base.toString(), QStringLiteral("/_db/%1").arg(db)).toString();
        if (op == SourceOp::Test) {
            c.url = join_url(root, QStringLiteral("/_api/version"));
            c.post = QStringLiteral("summary_keys");
            return cp_one(c);
        }
        if (op == SourceOp::List) {
            c.url = join_url(root, QStringLiteral("/_api/collection?excludeSystem=true"));
            c.parse.rows_path = QStringLiteral("result");
            return cp_one(c);
        }
        if (query.trimmed().isEmpty() && entity.isEmpty())
            return cp_bad(QStringLiteral("query"), cp_tr("Pick a collection or enter an AQL query"));
        c.url = join_url(root, QStringLiteral("/_api/cursor"));
        c.method = "POST";
        c.headers.emplaceBack("Content-Type", "application/json");
        QJsonObject body{{"batchSize", limit}};
        if (!query.trimmed().isEmpty()) {
            body.insert(QStringLiteral("query"), query.trimmed());
        } else {
            body.insert(QStringLiteral("query"), QStringLiteral("FOR d IN @@c LIMIT @n RETURN d"));
            body.insert(QStringLiteral("bindVars"), QJsonObject{{"@c", entity}, {"n", limit}});
        }
        c.body = cp_json(body);
        c.parse.rows_path = QStringLiteral("result");
        return cp_one(c);
    }
    if (p == QLatin1String("etcd")) {
        const QString hosts = cp_str(cfg, "hosts");
        const QUrl base = url_from_host(hosts.split(QLatin1Char(','), Qt::SkipEmptyParts).value(0).trimmed(), 2379);
        if (base.isEmpty())
            return cp_bad(QStringLiteral("hosts"), cp_tr("Enter the etcd endpoint"));
        if (!cp_str(cfg, "username").isEmpty())
            return cp_bad(QStringLiteral("username"),
                          cp_tr("etcd user authentication is not supported here; use an endpoint without auth (or a "
                                "client-certificate proxy)."));
        c.what = base.host();
        if (op == SourceOp::Test) {
            c.url = join_url(base.toString(), QStringLiteral("/version"));
            c.post = QStringLiteral("summary_keys");
            return cp_one(c);
        }
        c.url = join_url(base.toString(), QStringLiteral("/v3/kv/range"));
        c.method = "POST";
        c.headers.emplaceBack("Content-Type", "application/json");
        QByteArray key = entity.toUtf8();
        QByteArray range_end;
        if (key.isEmpty()) {
            key = QByteArray(1, '\0');
            range_end = QByteArray(1, '\0');
        } else {
            range_end = key;
            range_end[range_end.size() - 1] = char(range_end.at(range_end.size() - 1) + 1); // prefix end
        }
        QJsonObject body{{"key", QString::fromLatin1(key.toBase64())},
                         {"range_end", QString::fromLatin1(range_end.toBase64())},
                         {"limit", limit}};
        if (op == SourceOp::List)
            body.insert(QStringLiteral("keys_only"), true);
        c.body = cp_json(body);
        c.post = QStringLiteral("etcd");
        return cp_one(c);
    }
    if (p == QLatin1String("orientdb")) {
        const QString host = cp_str(cfg, "host");
        if (host.isEmpty())
            return cp_bad(QStringLiteral("host"), cp_tr("Enter the OrientDB host"));
        int port = cp_int(cfg, "port", 2480);
        if (port == 2424)
            port = 2480; // binary port → the HTTP API of the same server
        const QUrl base = url_from_host(host, port);
        const QString db = cp_str(cfg, "database");
        cp_add_basic_if(c, cfg);
        c.what = base.host();
        if (op == SourceOp::Test) {
            c.url = join_url(base.toString(), QStringLiteral("/listDatabases"));
            c.post = QStringLiteral("summary_keys");
            return cp_one(c);
        }
        if (db.isEmpty())
            return cp_bad(QStringLiteral("database"), cp_tr("Enter the database"));
        if (op == SourceOp::List) {
            c.url = join_url(base.toString(), QStringLiteral("/database/%1").arg(db));
            c.parse.rows_path = QStringLiteral("classes");
            return cp_one(c);
        }
        const QString sql = !query.trimmed().isEmpty() ? query.trimmed() : QStringLiteral("SELECT FROM %1").arg(entity);
        c.url = join_url(base.toString(), QStringLiteral("/query/%1/sql/%2/%3")
                                              .arg(db, QString::fromUtf8(QUrl::toPercentEncoding(sql)))
                                              .arg(limit));
        c.parse.rows_path = QStringLiteral("result");
        return cp_one(c);
    }

    // ── Time series ──
    if (p == QLatin1String("influxdb")) {
        const QUrl base = cp_base(cfg, {"url"});
        const QString token = cp_str(cfg, "token");
        const QString org = cp_str(cfg, "organization");
        const QString bucket = cp_str(cfg, "bucket");
        if (base.isEmpty())
            return cp_bad(QStringLiteral("url"), cp_tr("Enter the InfluxDB URL"));
        if (token.isEmpty())
            return cp_bad(QStringLiteral("token"), cp_tr("Enter the API token"));
        c.headers.emplaceBack("Authorization", "Token " + token.toUtf8());
        c.secrets << token;
        c.what = base.host();
        if (op == SourceOp::Test) {
            QString path = QStringLiteral("/api/v2/buckets?limit=100");
            if (!org.isEmpty())
                path += QStringLiteral("&org=") + QString::fromUtf8(QUrl::toPercentEncoding(org));
            c.url = join_url(base.toString(), path);
            c.parse.rows_path = QStringLiteral("buckets");
            c.post = QStringLiteral("count_summary");
            return cp_one(c);
        }
        if (org.isEmpty())
            return cp_bad(QStringLiteral("organization"), cp_tr("Enter the organization"));
        if (bucket.isEmpty() && query.trimmed().isEmpty())
            return cp_bad(QStringLiteral("bucket"), cp_tr("Enter the bucket"));
        QString flux;
        if (op == SourceOp::List)
            flux = QStringLiteral("import \"influxdata/influxdb/schema\"\nschema.measurements(bucket: \"%1\")").arg(bucket);
        else if (!query.trimmed().isEmpty())
            flux = query.trimmed();
        else
            flux = QStringLiteral("from(bucket: \"%1\") |> range(start: -30d)%2 |> limit(n: %3)")
                       .arg(bucket,
                            entity.isEmpty() ? QString()
                                             : QStringLiteral(" |> filter(fn: (r) => r._measurement == \"%1\")").arg(entity))
                       .arg(limit);
        c.url = join_url(base.toString(), QStringLiteral("/api/v2/query?org=") +
                                              QString::fromUtf8(QUrl::toPercentEncoding(org)));
        c.method = "POST";
        c.headers.removeIf([](const auto& h) { return h.first == "Accept"; });
        c.headers.emplaceBack("Accept", "application/csv");
        c.headers.emplaceBack("Content-Type", "application/vnd.flux");
        c.body = flux.toUtf8();
        c.post = QStringLiteral("influx_csv");
        c.parse.max_rows = limit;
        return cp_one(c);
    }
    if (p == QLatin1String("prometheus")) {
        const QUrl base = cp_base(cfg, {"url"});
        if (base.isEmpty())
            return cp_bad(QStringLiteral("url"), cp_tr("Enter the server URL"));
        cp_add_basic_if(c, cfg);
        c.what = base.host();
        if (op == SourceOp::Test) {
            c.url = join_url(base.toString(), QStringLiteral("/api/v1/query?query=vector(1)"));
            c.post = QStringLiteral("prom");
            return cp_one(c);
        }
        if (op == SourceOp::List) {
            c.url = join_url(base.toString(), QStringLiteral("/api/v1/label/__name__/values"));
            c.post = QStringLiteral("prom_names");
            return cp_one(c);
        }
        const QString q = !query.trimmed().isEmpty() ? query.trimmed() : !entity.isEmpty() ? entity : cp_str(cfg, "query");
        if (q.isEmpty())
            return cp_bad(QStringLiteral("query"), cp_tr("Pick a metric or enter a PromQL query"));
        c.url = join_url(base.toString(), QStringLiteral("/api/v1/query"));
        QUrlQuery uq;
        uq.addQueryItem(QStringLiteral("query"), q);
        c.url.setQuery(uq);
        c.post = QStringLiteral("prom");
        c.parse.max_rows = limit;
        return cp_one(c);
    }
    if (p == QLatin1String("opentsdb")) {
        const QUrl base = cp_base(cfg, {"url"});
        if (base.isEmpty())
            return cp_bad(QStringLiteral("url"), cp_tr("Enter the OpenTSDB URL"));
        c.what = base.host();
        if (op == SourceOp::Test) {
            c.url = join_url(base.toString(), QStringLiteral("/api/version"));
            c.post = QStringLiteral("summary_keys");
            return cp_one(c);
        }
        if (op == SourceOp::List) {
            c.url = join_url(base.toString(), QStringLiteral("/api/suggest?type=metrics&max=1000"));
            c.post = QStringLiteral("values_as_names");
            return cp_one(c);
        }
        if (entity.isEmpty())
            return cp_bad(QStringLiteral("entity"), cp_tr("Pick a metric"));
        c.url = join_url(base.toString(), QStringLiteral("/api/query?start=%1&m=sum:%2")
                                              .arg(query.trimmed().isEmpty() ? QStringLiteral("1d-ago") : query.trimmed(),
                                                   entity));
        c.post = QStringLiteral("opentsdb");
        c.parse.max_rows = limit;
        return cp_one(c);
    }
    if (p == QLatin1String("questdb")) {
        const QString host = cp_str(cfg, "host");
        if (host.isEmpty())
            return cp_bad(QStringLiteral("host"), cp_tr("Enter the QuestDB host"));
        const QUrl base = url_from_host(host, cp_int(cfg, "port", 9000));
        cp_add_basic_if(c, cfg);
        c.what = base.host();
        QString sql;
        if (op == SourceOp::Test)
            sql = QStringLiteral("SELECT version()");
        else if (op == SourceOp::List)
            sql = QStringLiteral("tables()");
        else
            sql = cp_select(entity, query, limit);
        c.url = join_url(base.toString(), QStringLiteral("/exec"));
        QUrlQuery q;
        q.addQueryItem(QStringLiteral("query"), sql);
        if (op == SourceOp::Fetch)
            q.addQueryItem(QStringLiteral("limit"), QString::number(limit));
        c.url.setQuery(q);
        c.post = QStringLiteral("questdb");
        return cp_one(c);
    }
    if (p == QLatin1String("clickhouse")) {
        const QString host = cp_str(cfg, "host");
        if (host.isEmpty())
            return cp_bad(QStringLiteral("host"), cp_tr("Enter the ClickHouse host"));
        const QUrl base = url_from_host(host, cp_int(cfg, "port", 8123));
        const QString user = cp_str(cfg, "username");
        if (!user.isEmpty()) {
            c.headers.emplaceBack("X-ClickHouse-User", user.toUtf8());
            const QString pass = cfg.value(QStringLiteral("password")).toString();
            c.headers.emplaceBack("X-ClickHouse-Key", pass.toUtf8());
            if (!pass.isEmpty())
                c.secrets << pass;
        }
        c.what = base.host();
        QString sql;
        if (op == SourceOp::Test)
            sql = QStringLiteral("SELECT version() AS version, currentDatabase() AS database");
        else if (op == SourceOp::List)
            sql = QStringLiteral("SELECT name, engine, total_rows FROM system.tables WHERE database = currentDatabase() "
                                 "ORDER BY name");
        else
            sql = cp_select(entity, query, limit, QLatin1Char('`'));
        if (!sql.contains(QLatin1String("FORMAT "), Qt::CaseInsensitive))
            sql += QStringLiteral(" FORMAT JSON");
        QUrl u = join_url(base.toString(), QStringLiteral("/"));
        QUrlQuery q;
        if (!cp_str(cfg, "database").isEmpty())
            q.addQueryItem(QStringLiteral("database"), cp_str(cfg, "database"));
        u.setQuery(q);
        c.url = u;
        c.method = "POST";
        c.body = sql.toUtf8();
        c.post = QStringLiteral("clickhouse");
        return cp_one(c);
    }
    if (p == QLatin1String("kdb")) {
        const QString host = cp_str(cfg, "host");
        if (host.isEmpty())
            return cp_bad(QStringLiteral("host"), cp_tr("Enter the kdb+ host"));
        const QUrl base = url_from_host(host, cp_int(cfg, "port", 5000));
        cp_add_basic_if(c, cfg);
        c.what = base.host();
        QString expr;
        if (op == SourceOp::Test)
            expr = QStringLiteral("([]ok:enlist 1b)");
        else if (op == SourceOp::List)
            expr = QStringLiteral("([]name:tables[])");
        else
            expr = !query.trimmed().isEmpty() ? query.trimmed() : QStringLiteral("select[%1] from %2").arg(limit).arg(entity);
        c.url = QUrl(base.toString() + QStringLiteral("/q.csv?") + QString::fromUtf8(QUrl::toPercentEncoding(expr)));
        c.headers.clear();
        c.parse.format = TextFormat::Csv;
        c.parse.max_rows = limit;
        return cp_one(c);
    }

    // ── Warehouses ──
    if (p == QLatin1String("databricks")) {
        const QString host = cp_str(cfg, "serverHostname");
        const QString token = cp_str(cfg, "accessToken");
        const QString path = cp_str(cfg, "httpPath");
        if (host.isEmpty())
            return cp_bad(QStringLiteral("serverHostname"), cp_tr("Enter the server hostname"));
        if (token.isEmpty())
            return cp_bad(QStringLiteral("accessToken"), cp_tr("Enter the access token"));
        const QString warehouse = path.section(QLatin1Char('/'), -1);
        if (!path.contains(QLatin1String("/warehouses/")) || warehouse.isEmpty())
            return cp_bad(QStringLiteral("httpPath"),
                          cp_tr("The HTTP path must be a SQL warehouse path (/sql/1.0/warehouses/<id>)"));
        const QUrl base = url_from_host(host, -1, true);
        c.headers.emplaceBack("Authorization", "Bearer " + token.toUtf8());
        c.secrets << token;
        c.what = base.host();
        if (op == SourceOp::Test) {
            c.url = join_url(base.toString(), QStringLiteral("/api/2.0/sql/warehouses/%1").arg(warehouse));
            c.post = QStringLiteral("summary_keys");
            return cp_one(c);
        }
        const QString sql = op == SourceOp::List ? QStringLiteral("SHOW TABLES") : cp_select(entity, query, limit, QLatin1Char('`'));
        c.url = join_url(base.toString(), QStringLiteral("/api/2.0/sql/statements"));
        c.method = "POST";
        c.headers.emplaceBack("Content-Type", "application/json");
        c.body = cp_json(QJsonObject{{"warehouse_id", warehouse},
                                     {"statement", sql},
                                     {"wait_timeout", "30s"},
                                     {"format", "JSON_ARRAY"},
                                     {"disposition", "INLINE"}});
        c.post = QStringLiteral("databricks");
        return cp_one(c);
    }
    if (p == QLatin1String("iceberg")) {
        if (cp_str(cfg, "catalog").toLower() != QLatin1String("rest") && !cp_str(cfg, "catalog").isEmpty())
            return cp_bad(QStringLiteral("catalog"), cp_tr("Only the Iceberg REST catalog is supported in this build"));
        const QUrl base = cp_base(cfg, {"catalogUri"});
        if (base.isEmpty())
            return cp_bad(QStringLiteral("catalogUri"), cp_tr("Enter the REST catalog URI"));
        const QString ns = cp_str(cfg, "namespace");
        const QString token = cp_str(cfg, "token");
        if (!token.isEmpty()) {
            c.headers.emplaceBack("Authorization", "Bearer " + token.toUtf8());
            c.secrets << token;
        }
        c.what = base.host();
        if (op == SourceOp::Test) {
            QString path = QStringLiteral("/v1/config");
            if (!cp_str(cfg, "warehouse").isEmpty())
                path += QStringLiteral("?warehouse=") + QString::fromUtf8(QUrl::toPercentEncoding(cp_str(cfg, "warehouse")));
            c.url = join_url(base.toString(), path);
            c.post = QStringLiteral("summary_keys");
            return cp_one(c);
        }
        if (op == SourceOp::List) {
            c.url = join_url(base.toString(), ns.isEmpty() ? QStringLiteral("/v1/namespaces")
                                                           : QStringLiteral("/v1/namespaces/%1/tables").arg(ns));
            c.post = QStringLiteral("iceberg_list");
            return cp_one(c);
        }
        const QString table = entity.isEmpty() ? cp_str(cfg, "table") : entity;
        if (ns.isEmpty() || table.isEmpty())
            return cp_bad(QStringLiteral("table"), cp_tr("Enter the namespace and pick a table"));
        c.url = join_url(base.toString(), QStringLiteral("/v1/namespaces/%1/tables/%2").arg(ns, table));
        c.post = QStringLiteral("iceberg_schema");
        return cp_one(c);
    }

    // ── Streams over HTTP ──
    if (p == QLatin1String("flink")) {
        const QUrl base = cp_base(cfg, {"jobManagerUrl"});
        if (base.isEmpty())
            return cp_bad(QStringLiteral("jobManagerUrl"), cp_tr("Enter the job manager URL"));
        c.what = base.host();
        if (op == SourceOp::Test) {
            c.url = join_url(base.toString(), QStringLiteral("/overview"));
            c.post = QStringLiteral("summary_keys");
            return cp_one(c);
        }
        c.url = join_url(base.toString(), entity.isEmpty() || op == SourceOp::List ? QStringLiteral("/jobs/overview")
                                                                                   : QStringLiteral("/jobs/%1").arg(entity));
        c.parse.rows_path = entity.isEmpty() || op == SourceOp::List ? QStringLiteral("jobs") : QStringLiteral("vertices");
        return cp_one(c);
    }
    if (p == QLatin1String("rabbitmq")) {
        const QString host = cp_str(cfg, "host");
        if (host.isEmpty())
            return cp_bad(QStringLiteral("host"), cp_tr("Enter the RabbitMQ host"));
        int port = cp_int(cfg, "port", 15672);
        if (port == 5672 || port == 5671)
            port = 15672; // AMQP port → the management API of the same broker
        const QUrl base = url_from_host(host, port);
        const QString vhost = cp_str(cfg, "vhost").isEmpty() ? QStringLiteral("/") : cp_str(cfg, "vhost");
        const QString vh = QString::fromUtf8(QUrl::toPercentEncoding(vhost));
        cp_add_basic_if(c, cfg);
        c.what = QStringLiteral("the RabbitMQ management API at %1").arg(base.authority());
        if (op == SourceOp::Test) {
            c.url = join_url(base.toString(), QStringLiteral("/api/overview"));
            c.post = QStringLiteral("summary_keys");
            return cp_one(c);
        }
        if (op == SourceOp::List || entity.isEmpty()) {
            c.url = join_url(base.toString(), QStringLiteral("/api/queues/%1?columns=name,messages,consumers,state").arg(vh));
            return cp_one(c);
        }
        // Read messages without consuming them (ack_requeue_true puts them back).
        c.url = join_url(base.toString(), QStringLiteral("/api/queues/%1/%2/get")
                                              .arg(vh, QString::fromUtf8(QUrl::toPercentEncoding(entity))));
        c.method = "POST";
        c.headers.emplaceBack("Content-Type", "application/json");
        c.body = cp_json(QJsonObject{{"count", std::min(limit, 500)}, {"ackmode", "ack_requeue_true"}, {"encoding", "auto"}});
        return cp_one(c);
    }

    // ── Object stores ──
    if (p == QLatin1String("s3") || p == QLatin1String("ibm-cos")) {
        const QString type = sup.type_id;
        QString endpoint, region = cp_str(cfg, "region"), bucket = cp_str(cfg, "bucket");
        QString access = cp_str(cfg, "accessKeyId");
        if (access.isEmpty())
            access = cp_str(cfg, "accessKey");
        QString secret = cfg.value(QStringLiteral("secretAccessKey")).toString().trimmed();
        if (secret.isEmpty())
            secret = cfg.value(QStringLiteral("secretKey")).toString().trimmed();
        if (type == QLatin1String("aws-s3")) {
            if (region.isEmpty())
                region = QStringLiteral("us-east-1");
            endpoint = QStringLiteral("https://s3.%1.amazonaws.com").arg(region);
        } else if (type == QLatin1String("digitalocean-spaces")) {
            bucket = cp_str(cfg, "space");
            endpoint = QStringLiteral("https://%1.digitaloceanspaces.com").arg(region.isEmpty() ? QStringLiteral("nyc3") : region);
            region = QStringLiteral("us-east-1");
        } else if (type == QLatin1String("wasabi")) {
            if (region.isEmpty())
                region = QStringLiteral("us-east-1");
            endpoint = QStringLiteral("https://s3.%1.wasabisys.com").arg(region);
        } else if (type == QLatin1String("cloudflare-r2")) {
            endpoint = QStringLiteral("https://%1.r2.cloudflarestorage.com").arg(cp_str(cfg, "accountId"));
            region = QStringLiteral("auto");
        } else if (type == QLatin1String("oracle-cloud-storage")) {
            endpoint = QStringLiteral("https://%1.compat.objectstorage.%2.oraclecloud.com").arg(cp_str(cfg, "namespace"), region);
        } else if (type == QLatin1String("backblaze-b2")) {
            access = cp_str(cfg, "keyId");
            secret = cfg.value(QStringLiteral("applicationKey")).toString().trimmed();
            endpoint = cp_str(cfg, "s3Endpoint");
            if (bucket.isEmpty())
                bucket = cp_str(cfg, "bucketName");
            if (endpoint.isEmpty())
                return cp_bad(QStringLiteral("s3Endpoint"),
                              cp_tr("Enter the bucket's S3 endpoint (shown on the B2 bucket page, e.g. "
                                    "https://s3.us-west-004.backblazeb2.com)"));
            region = QUrl(endpoint).host().section(QLatin1Char('.'), 1, 1);
        } else if (type == QLatin1String("minio") || type == QLatin1String("ibm-cloud-storage")) {
            endpoint = cp_str(cfg, "endpoint");
            if (region.isEmpty())
                region = QStringLiteral("us-east-1");
        }
        if (endpoint.isEmpty() || QUrl(endpoint).host().isEmpty())
            return cp_bad(QStringLiteral("endpoint"), cp_tr("Enter the storage endpoint"));
        if (!endpoint.contains(QLatin1String("://")))
            endpoint = QStringLiteral("https://") + endpoint;
        if (p == QLatin1String("ibm-cos")) {
            const QString api_key = cfg.value(QStringLiteral("apiKey")).toString().trimmed();
            if (api_key.isEmpty())
                return cp_bad(QStringLiteral("apiKey"), cp_tr("Enter the IBM Cloud API key"));
            c.pre_auth = QStringLiteral("ibm_iam");
            c.sign_params = QJsonObject{{"api_key", api_key}};
            c.secrets << api_key;
            if (!cp_str(cfg, "serviceInstanceId").isEmpty())
                c.headers.emplaceBack("ibm-service-instance-id", cp_str(cfg, "serviceInstanceId").toUtf8());
        } else {
            if (access.isEmpty() || secret.isEmpty())
                return cp_bad(QStringLiteral("accessKeyId"), cp_tr("Enter the access key and secret"));
            c.sign = QStringLiteral("aws_v4");
            c.sign_params = QJsonObject{{"access_key", access}, {"secret_key", secret}, {"region", region}, {"service", "s3"}};
            c.secrets << secret;
        }
        c.headers.clear();
        c.what = QUrl(endpoint).host();
        if (op == SourceOp::Test || (op == SourceOp::List && bucket.isEmpty())) {
            c.url = QUrl(bucket.isEmpty() || op == SourceOp::List ? endpoint + QStringLiteral("/")
                                                                  : endpoint + QStringLiteral("/%1?list-type=2&max-keys=1").arg(bucket));
            if (op == SourceOp::Test && !bucket.isEmpty())
                c.url = QUrl(endpoint + QStringLiteral("/%1").arg(bucket));
            if (op == SourceOp::Test && !bucket.isEmpty()) {
                QUrlQuery q;
                q.addQueryItem(QStringLiteral("list-type"), QStringLiteral("2"));
                q.addQueryItem(QStringLiteral("max-keys"), QStringLiteral("1"));
                c.url.setQuery(q);
            }
            c.parse.format = TextFormat::Xml;
            c.parse.rows_path = bucket.isEmpty() ? QStringLiteral("Bucket") : QStringLiteral("Contents");
            c.post = QStringLiteral("s3_list");
            return cp_one(c);
        }
        if (bucket.isEmpty())
            return cp_bad(QStringLiteral("bucket"), cp_tr("Enter the bucket"));
        if (op == SourceOp::List || entity.isEmpty() || entity.endsWith(QLatin1Char('/'))) {
            c.url = QUrl(endpoint + QStringLiteral("/%1").arg(bucket));
            QUrlQuery q;
            q.addQueryItem(QStringLiteral("list-type"), QStringLiteral("2"));
            q.addQueryItem(QStringLiteral("max-keys"), QString::number(std::min(limit, 1000)));
            const QString prefix = !entity.isEmpty() ? entity : !query.trimmed().isEmpty() ? query.trimmed() : cp_str(cfg, "prefix");
            if (!prefix.isEmpty())
                q.addQueryItem(QStringLiteral("prefix"), prefix);
            c.url.setQuery(q);
            c.parse.format = TextFormat::Xml;
            c.parse.rows_path = QStringLiteral("Contents");
            c.post = QStringLiteral("s3_list");
            return cp_one(c);
        }
        c.url = QUrl(endpoint + QStringLiteral("/%1/%2").arg(bucket, entity));
        c.object_name = entity;
        c.parse.file_name = entity;
        c.parse.max_rows = limit;
        return cp_one(c);
    }
    if (p == QLatin1String("azure-blob")) {
        const QString account = cp_str(cfg, "accountName");
        const QString key = cfg.value(QStringLiteral("accountKey")).toString().trimmed();
        const QString container = cp_str(cfg, "container");
        if (account.isEmpty())
            return cp_bad(QStringLiteral("accountName"), cp_tr("Enter the storage account name"));
        if (key.isEmpty())
            return cp_bad(QStringLiteral("accountKey"), cp_tr("Enter the account key or a SAS token"));
        const QString base = QStringLiteral("https://%1.blob.core.windows.net").arg(account);
        const bool sas = key.contains(QLatin1String("sv=")) && key.contains(QLatin1String("sig="));
        c.headers.clear();
        c.secrets << key;
        c.what = QStringLiteral("%1.blob.core.windows.net").arg(account);
        QUrl u;
        if (op == SourceOp::Test || container.isEmpty()) {
            u = QUrl(base + QStringLiteral("/"));
            QUrlQuery q;
            q.addQueryItem(QStringLiteral("comp"), QStringLiteral("list"));
            q.addQueryItem(QStringLiteral("maxresults"), QStringLiteral("100"));
            u.setQuery(q);
            c.parse.rows_path = QStringLiteral("Container");
            c.post = QStringLiteral("azure_list");
            if (op == SourceOp::Fetch && container.isEmpty())
                return cp_bad(QStringLiteral("container"), cp_tr("Enter the container"));
        } else if (op == SourceOp::List || entity.isEmpty() || entity.endsWith(QLatin1Char('/'))) {
            u = QUrl(base + QStringLiteral("/%1").arg(container));
            QUrlQuery q;
            q.addQueryItem(QStringLiteral("restype"), QStringLiteral("container"));
            q.addQueryItem(QStringLiteral("comp"), QStringLiteral("list"));
            q.addQueryItem(QStringLiteral("maxresults"), QString::number(std::min(limit, 1000)));
            const QString prefix = !entity.isEmpty() ? entity : query.trimmed();
            if (!prefix.isEmpty())
                q.addQueryItem(QStringLiteral("prefix"), prefix);
            u.setQuery(q);
            c.parse.rows_path = QStringLiteral("Blob");
            c.post = QStringLiteral("azure_list");
        } else {
            u = QUrl(base + QStringLiteral("/%1/%2").arg(container, entity));
            c.object_name = entity;
            c.parse.file_name = entity;
            c.parse.max_rows = limit;
        }
        if (sas) {
            QString sas_q = key.startsWith(QLatin1Char('?')) ? key.mid(1) : key;
            QUrlQuery q(u);
            for (const auto& item : QUrlQuery(sas_q).queryItems(QUrl::FullyDecoded))
                q.addQueryItem(item.first, item.second);
            u.setQuery(q);
        } else {
            c.sign = QStringLiteral("azure_shared_key");
            c.sign_params = QJsonObject{{"account", account}, {"key", key}};
        }
        c.url = u;
        if (c.object_name.isEmpty())
            c.parse.format = TextFormat::Xml;
        return cp_one(c);
    }

    // ── Market data over HTTP ──
    if (p == QLatin1String("binance")) {
        const QString base = cp_bool(cfg, "testnet") ? QStringLiteral("https://testnet.binance.vision")
                                                     : QStringLiteral("https://api.binance.com");
        const QString sym = query.trimmed().isEmpty() ? QStringLiteral("BTCUSDT") : query.trimmed().toUpper();
        c.what = QStringLiteral("Binance");
        if (op == SourceOp::Test) {
            c.url = QUrl(base + QStringLiteral("/api/v3/time"));
            c.post = QStringLiteral("summary_keys");
            return cp_one(c);
        }
        if (op == SourceOp::List)
            return cp_static(cp_entities({{"klines", "Daily candles for the symbol"},
                                          {"ticker_24hr", "24-hour ticker statistics (symbol, or all when empty)"},
                                          {"depth", "Order book for the symbol"},
                                          {"exchange_info", "Every trading pair and its rules"}}));
        const QString e = entity.isEmpty() ? QStringLiteral("klines") : entity;
        if (e == QLatin1String("ticker_24hr")) {
            c.url = QUrl(base + QStringLiteral("/api/v3/ticker/24hr") +
                         (query.trimmed().isEmpty() ? QString() : QStringLiteral("?symbol=") + sym));
        } else if (e == QLatin1String("depth")) {
            c.url = QUrl(base + QStringLiteral("/api/v3/depth?symbol=%1&limit=%2").arg(sym).arg(std::min(limit, 1000)));
            c.post = QStringLiteral("book");
        } else if (e == QLatin1String("exchange_info")) {
            c.url = QUrl(base + QStringLiteral("/api/v3/exchangeInfo"));
            c.parse.rows_path = QStringLiteral("symbols");
        } else {
            c.url = QUrl(base + QStringLiteral("/api/v3/klines?symbol=%1&interval=1d&limit=%2").arg(sym).arg(std::min(limit, 1000)));
            c.post = QStringLiteral("binance_klines");
        }
        c.parse.max_rows = limit;
        return cp_one(c);
    }
    if (p == QLatin1String("coinbase")) {
        const QString base = QStringLiteral("https://api.exchange.coinbase.com");
        const QString product = query.trimmed().isEmpty() ? QStringLiteral("BTC-USD") : query.trimmed().toUpper();
        c.what = QStringLiteral("Coinbase");
        c.headers.emplaceBack("User-Agent", "FinceptTerminal");
        if (op == SourceOp::Test) {
            c.url = QUrl(base + QStringLiteral("/time"));
            c.post = QStringLiteral("summary_keys");
            return cp_one(c);
        }
        if (op == SourceOp::List)
            return cp_static(cp_entities({{"candles", "Daily candles for the product"},
                                          {"ticker", "Last trade for the product"},
                                          {"products", "Every product"}}));
        const QString e = entity.isEmpty() ? QStringLiteral("candles") : entity;
        if (e == QLatin1String("products"))
            c.url = QUrl(base + QStringLiteral("/products"));
        else if (e == QLatin1String("ticker"))
            c.url = QUrl(base + QStringLiteral("/products/%1/ticker").arg(product));
        else {
            c.url = QUrl(base + QStringLiteral("/products/%1/candles?granularity=86400").arg(product));
            c.post = QStringLiteral("coinbase_candles");
        }
        c.parse.max_rows = limit;
        return cp_one(c);
    }
    if (p == QLatin1String("kraken")) {
        const QString base = QStringLiteral("https://api.kraken.com/0/public");
        const QString pair = query.trimmed().isEmpty() ? QStringLiteral("XBTUSD") : query.trimmed().toUpper();
        c.what = QStringLiteral("Kraken");
        c.post = QStringLiteral("kraken");
        if (op == SourceOp::Test) {
            c.url = QUrl(base + QStringLiteral("/Time"));
            return cp_one(c);
        }
        if (op == SourceOp::List)
            return cp_static(cp_entities({{"ohlc", "Daily OHLC for the pair"},
                                          {"ticker", "Ticker for the pair"},
                                          {"asset_pairs", "Every tradable pair"}}));
        const QString e = entity.isEmpty() ? QStringLiteral("ohlc") : entity;
        if (e == QLatin1String("asset_pairs"))
            c.url = QUrl(base + QStringLiteral("/AssetPairs"));
        else if (e == QLatin1String("ticker"))
            c.url = QUrl(base + QStringLiteral("/Ticker?pair=%1").arg(pair));
        else
            c.url = QUrl(base + QStringLiteral("/OHLC?pair=%1&interval=1440").arg(pair));
        c.parse.max_rows = limit;
        return cp_one(c);
    }
    if (p == QLatin1String("tradier")) {
        const QString token = cp_str(cfg, "accessToken");
        if (token.isEmpty())
            return cp_bad(QStringLiteral("accessToken"), cp_tr("Enter the access token"));
        const QString base = cp_bool(cfg, "sandbox") ? QStringLiteral("https://sandbox.tradier.com/v1")
                                                     : QStringLiteral("https://api.tradier.com/v1");
        c.headers.emplaceBack("Authorization", "Bearer " + token.toUtf8());
        c.secrets << token;
        c.what = QStringLiteral("Tradier");
        const QString syms = query.trimmed().isEmpty() ? QStringLiteral("AAPL") : query.trimmed().toUpper();
        if (op == SourceOp::Test) {
            c.url = QUrl(base + QStringLiteral("/markets/clock"));
            c.parse.rows_path = QStringLiteral("clock");
            return cp_one(c);
        }
        if (op == SourceOp::List)
            return cp_static(cp_entities({{"quotes", "Quotes for the symbols"}, {"history", "Daily history for the symbol"}}));
        if (entity == QLatin1String("history")) {
            c.url = QUrl(base + QStringLiteral("/markets/history?symbol=%1&interval=daily").arg(syms.section(QLatin1Char(','), 0, 0)));
            c.parse.rows_path = QStringLiteral("history.day");
        } else {
            c.url = QUrl(base + QStringLiteral("/markets/quotes?symbols=%1").arg(syms));
            c.parse.rows_path = QStringLiteral("quotes.quote");
        }
        return cp_one(c);
    }
    if (p == QLatin1String("finage")) {
        const QString key = cp_str(cfg, "apiKey");
        if (key.isEmpty())
            return cp_bad(QStringLiteral("apiKey"), cp_tr("Enter the API key"));
        const QString sym = query.trimmed().isEmpty() ? QStringLiteral("AAPL") : query.trimmed().toUpper();
        c.secrets << key;
        c.what = QStringLiteral("Finage");
        if (op == SourceOp::List)
            return cp_static(cp_entities({{"last", "Last quote for the stock"}, {"prev_close", "Previous close aggregate"}}));
        const QString path = entity == QLatin1String("prev_close") ? QStringLiteral("/agg/stock/prev-close/%1").arg(sym)
                                                                    : QStringLiteral("/last/stock/%1").arg(sym);
        c.url = QUrl(QStringLiteral("https://api.finage.co.uk") + path + QStringLiteral("?apikey=") + key);
        return cp_one(c);
    }
    if (p == QLatin1String("newsapi")) {
        const QString key = cp_str(cfg, "apiKey");
        if (key.isEmpty())
            return cp_bad(QStringLiteral("apiKey"), cp_tr("Enter the NewsAPI key"));
        c.headers.emplaceBack("X-Api-Key", key.toUtf8());
        c.secrets << key;
        c.what = QStringLiteral("NewsAPI");
        if (op == SourceOp::Test || entity == QLatin1String("sources")) {
            c.url = QUrl(QStringLiteral("https://newsapi.org/v2/top-headlines/sources"));
            c.parse.rows_path = QStringLiteral("sources");
            c.post = op == SourceOp::Test ? QStringLiteral("count_summary") : QString();
            return cp_one(c);
        }
        if (op == SourceOp::List)
            return cp_static(cp_entities({{"everything", "Articles matching the search"},
                                          {"top_headlines", "Top business headlines (US)"},
                                          {"sources", "Every news source"}}));
        QUrlQuery q;
        if (entity == QLatin1String("top_headlines")) {
            c.url = QUrl(QStringLiteral("https://newsapi.org/v2/top-headlines"));
            q.addQueryItem(QStringLiteral("category"), QStringLiteral("business"));
            q.addQueryItem(QStringLiteral("country"), QStringLiteral("us"));
            if (!query.trimmed().isEmpty())
                q.addQueryItem(QStringLiteral("q"), query.trimmed());
        } else {
            c.url = QUrl(QStringLiteral("https://newsapi.org/v2/everything"));
            q.addQueryItem(QStringLiteral("q"), query.trimmed().isEmpty() ? QStringLiteral("markets") : query.trimmed());
            q.addQueryItem(QStringLiteral("sortBy"), QStringLiteral("publishedAt"));
        }
        q.addQueryItem(QStringLiteral("pageSize"), QString::number(std::min(limit, 100)));
        c.url.setQuery(q);
        c.parse.rows_path = QStringLiteral("articles");
        return cp_one(c);
    }
    if (p == QLatin1String("adanos")) {
        const QString key = cp_str(cfg, "apiKey");
        if (key.isEmpty())
            return cp_bad(QStringLiteral("apiKey"), cp_tr("Enter the API key"));
        c.headers.emplaceBack("X-API-Key", key.toUtf8());
        c.secrets << key;
        c.what = QStringLiteral("Adanos");
        if (op == SourceOp::List)
            return cp_static(cp_entities({{"news", "News sentiment"}, {"reddit", "Reddit sentiment"}, {"x", "X / Twitter sentiment"}}));
        const QString src = entity.isEmpty() || op == SourceOp::Test ? QStringLiteral("news") : entity;
        QString tickers = query.trimmed().isEmpty() ? cp_str(cfg, "defaultUniverse") : query.trimmed();
        if (tickers.isEmpty())
            tickers = QStringLiteral("AAPL");
        c.url = QUrl(QStringLiteral("https://api.adanos.org/%1/stocks/v1/compare").arg(src));
        QUrlQuery q;
        q.addQueryItem(QStringLiteral("tickers"), tickers.toUpper().remove(QLatin1Char(' ')));
        q.addQueryItem(QStringLiteral("days"), QStringLiteral("7"));
        c.url.setQuery(q);
        return cp_one(c);
    }

    HttpPlan none;
    none.error = SourceError::make(ErrorCode::NotSupported, cp_tr("No HTTP plan for \"%1\"").arg(sup.type_id));
    return none;
}

PostResult post_process(const HttpCall& call, SourceOp op, const QByteArray& body, const QString& content_type) {
    const QString post = call.post;
    SourceError err;
    auto generic = [&]() {
        PostResult r;
        ParseOptions o = call.parse;
        o.content_type = content_type;
        const ParseOutcome po = parse_table(body, o);
        if (!po.ok) {
            r.error = po.error;
            return r;
        }
        r.ok = true;
        r.data.tables << po.table;
        r.data.truncated = po.truncated;
        if (!po.note.isEmpty())
            r.data.meta.insert(QStringLiteral("note"), po.note);
        r.summary = cp_tr("%1 row(s)").arg(po.table.row_count());
        return r;
    };
    if (post.isEmpty())
        return generic();

    if (post == QLatin1String("s3_list") || post == QLatin1String("azure_list")) {
        // An empty bucket / container is a valid answer, not a failure.
        PostResult r = generic();
        if (!r.ok && r.error.code == ErrorCode::NoData) {
            DataTable t;
            t.columns = {QStringLiteral("name")};
            t.types = {ColumnType::String};
            r = cp_table_result(t, cp_tr("credentials accepted · nothing listed here yet"));
        } else if (r.ok) {
            r.summary = cp_tr("credentials accepted · %1 item(s) listed").arg(r.data.total_rows());
        }
        return r;
    }

    if (post == QLatin1String("influx_csv")) {
        ParseOptions o = call.parse;
        o.format = TextFormat::Csv;
        o.delimiter = QLatin1Char(',');
        const QByteArray clean = cp_strip_influx_annotations(body);
        if (clean.trimmed().isEmpty()) {
            PostResult r;
            r.error = SourceError::make(ErrorCode::NoData, cp_tr("The Flux query returned no rows"));
            return r;
        }
        PostResult r;
        ParseOutcome po = parse_table(clean, o);
        if (!po.ok) {
            r.error = po.error;
            return r;
        }
        // Drop Influx bookkeeping columns.
        for (const char* drop : {"result", "table"}) {
            const int idx = po.table.column_index(QLatin1String(drop));
            if (idx >= 0) {
                po.table.columns.removeAt(idx);
                po.table.types.removeAt(idx);
                for (QJsonArray& row : po.table.rows)
                    row.removeAt(idx);
            }
        }
        return cp_table_result(po.table, cp_tr("%1 row(s)").arg(po.table.row_count()));
    }
    if (post == QLatin1String("wsdl_test") || post == QLatin1String("wsdl_ops")) {
        PostResult r;
        const QString text = QString::fromUtf8(body);
        if (!text.contains(QLatin1String("definitions")) && !text.contains(QLatin1String("description"))) {
            r.error = SourceError::make(ErrorCode::BadArgs, cp_tr("The WSDL URL did not return a WSDL document"));
            return r;
        }
        ParseOptions o;
        o.format = TextFormat::Xml;
        o.rows_path = QStringLiteral("operation");
        ParseOutcome po = parse_table(body, o);
        DataTable t;
        t.columns = {QStringLiteral("name")};
        t.types = {ColumnType::String};
        QSet<QString> seen;
        const int name_col = po.ok ? po.table.column_index(QStringLiteral("@name")) : -1;
        for (int i = 0; po.ok && name_col >= 0 && i < po.table.row_count(); ++i) {
            const QString n = po.table.cell(i, name_col).toString();
            if (!n.isEmpty() && !seen.contains(n)) {
                seen.insert(n);
                t.rows.append(QJsonArray{n});
            }
        }
        r.ok = true;
        r.summary = cp_tr("WSDL read: %1 operation(s)").arg(t.row_count());
        r.data.tables << t;
        return r;
    }

    // JSON post-processors.
    const QJsonValue root = cp_parse_json(body, &err);
    if (root.isUndefined()) {
        PostResult r;
        r.error = err;
        return r;
    }
    const QJsonObject o = root.toObject();

    if (post == QLatin1String("summary_keys") || post == QLatin1String("es_root") || post == QLatin1String("solr_system") ||
        post == QLatin1String("couch_session")) {
        DataTable t = root.isObject() ? table_from_key_values(o) : table_from_json_array(root.toArray());
        QString summary;
        if (post == QLatin1String("es_root"))
            summary = QStringLiteral("%1 %2 · cluster %3")
                          .arg(o.value("version").toObject().value("distribution").toString(QStringLiteral("Elasticsearch")),
                               o.value("version").toObject().value("number").toString(), o.value("cluster_name").toString());
        else if (post == QLatin1String("solr_system"))
            summary = QStringLiteral("Solr %1").arg(o.value("lucene").toObject().value("solr-spec-version").toString());
        else if (post == QLatin1String("couch_session"))
            summary = cp_tr("signed in as %1")
                          .arg(o.value("userCtx").toObject().value("name").toString(cp_tr("anonymous")));
        else {
            for (const char* k : {"version", "pkgVersion", "rabbitmq_version", "flink-version", "serverTime", "name", "state",
                                  "etcdserver", "server"}) {
                const QJsonValue v = o.value(QLatin1String(k));
                if (v.isString() || v.isDouble()) {
                    summary = QStringLiteral("%1 %2").arg(QLatin1String(k), cell_text(v));
                    break;
                }
            }
        }
        return cp_table_result(t, summary);
    }
    if (post == QLatin1String("count_summary")) {
        PostResult r = generic();
        if (r.ok)
            r.summary = cp_tr("%1 item(s) visible with these credentials").arg(r.data.total_rows());
        else if (r.error.code == ErrorCode::NoData) {
            r = cp_table_result(table_from_key_values(o), cp_tr("credentials accepted"));
        }
        return r;
    }
    if (post == QLatin1String("values_as_names")) {
        DataTable t;
        t.columns = {QStringLiteral("name")};
        t.types = {ColumnType::String};
        for (const QJsonValue& v : root.toArray())
            t.rows.append(QJsonArray{v.toString()});
        return cp_table_result(t, cp_tr("%1 item(s)").arg(t.row_count()));
    }
    if (post == QLatin1String("graphql")) {
        PostResult r;
        const QString gerr = cp_first_error_message(o.value(QStringLiteral("errors")));
        if (!gerr.isEmpty() && !o.value(QStringLiteral("data")).isObject()) {
            r.error = SourceError::make(ErrorCode::BadArgs, cp_tr("GraphQL error: %1").arg(gerr), QStringLiteral("query"));
            return r;
        }
        const QJsonValue data = o.value(QStringLiteral("data"));
        if (op == SourceOp::Test)
            return cp_table_result(table_from_key_values(data.toObject()), cp_tr("GraphQL endpoint answered"));
        if (op == SourceOp::List) {
            QJsonArray rows;
            for (const QJsonValue& f : data.toObject().value("__schema").toObject().value("queryType").toObject().value("fields").toArray()) {
                const QJsonObject fo = f.toObject();
                rows.append(QJsonObject{{"name", fo.value("name")},
                                        {"type", fo.value("type").toObject().value("name").toString(
                                                     fo.value("type").toObject().value("kind").toString())},
                                        {"detail", fo.value("description")}});
            }
            return cp_table_result(table_from_json_array(rows), cp_tr("%1 query field(s)").arg(rows.size()));
        }
        ParseOptions po_opts = call.parse;
        po_opts.format = TextFormat::Json;
        const ParseOutcome po = parse_table(QJsonDocument(data.toObject()).toJson(QJsonDocument::Compact), po_opts);
        if (!po.ok) {
            r.error = po.error;
            return r;
        }
        PostResult ok = cp_table_result(po.table, cp_tr("%1 row(s)").arg(po.table.row_count()));
        if (!gerr.isEmpty())
            ok.data.meta.insert(QStringLiteral("note"), cp_tr("partial result — GraphQL error: %1").arg(gerr));
        return ok;
    }
    if (post == QLatin1String("es_hits")) {
        QJsonArray rows;
        for (const QJsonValue& h : o.value("hits").toObject().value("hits").toArray()) {
            QJsonObject row{{"_id", h.toObject().value("_id")}};
            const QJsonObject src = cp_flatten(h.toObject().value("_source").toObject());
            for (auto it = src.constBegin(); it != src.constEnd(); ++it)
                row.insert(it.key(), it.value());
            rows.append(row);
        }
        PostResult r = cp_table_result(table_from_json_array(rows));
        const QJsonValue total = o.value("hits").toObject().value("total");
        r.data.truncated = total.isObject() ? total.toObject().value("value").toInt() > rows.size() : total.toInt() > rows.size();
        return r;
    }
    if (post == QLatin1String("solr_cores")) {
        QJsonArray rows;
        const QJsonObject status = o.value("status").toObject();
        for (auto it = status.constBegin(); it != status.constEnd(); ++it) {
            const QJsonObject idx = it->toObject().value("index").toObject();
            rows.append(QJsonObject{{"name", it.key()}, {"docs", idx.value("numDocs")}, {"size", idx.value("size")}});
        }
        return cp_table_result(table_from_json_array(rows), cp_tr("%1 core(s)").arg(rows.size()));
    }
    if (post == QLatin1String("neo4j")) {
        PostResult r;
        const QString nerr = cp_first_error_message(o.value(QStringLiteral("errors")));
        if (!nerr.isEmpty()) {
            const bool auth = nerr.contains(QLatin1String("auth"), Qt::CaseInsensitive) ||
                              nerr.contains(QLatin1String("credentials"), Qt::CaseInsensitive);
            r.error = SourceError::make(auth ? ErrorCode::AuthFailed : ErrorCode::BadArgs, cp_tr("Neo4j: %1").arg(nerr));
            return r;
        }
        const QJsonObject res = o.value("results").toArray().first().toObject();
        QStringList cols;
        for (const QJsonValue& c : res.value("columns").toArray())
            cols << c.toString();
        QJsonArray rows;
        for (const QJsonValue& d : res.value("data").toArray()) {
            const QJsonArray row = d.toObject().value("row").toArray();
            QJsonObject out;
            for (int i = 0; i < cols.size(); ++i) {
                const QJsonValue v = row.at(i);
                if (v.isObject() && cols.size() == 1) {
                    const QJsonObject flat = cp_flatten(v.toObject());
                    for (auto it = flat.constBegin(); it != flat.constEnd(); ++it)
                        out.insert(it.key(), it.value());
                } else {
                    out.insert(cols[i], v);
                }
            }
            rows.append(out);
        }
        QString summary;
        if (op == SourceOp::Test && !rows.isEmpty())
            summary = QStringLiteral("%1 %2 %3")
                          .arg(rows.first().toObject().value("name").toString(), rows.first().toObject().value("version").toString(),
                               rows.first().toObject().value("edition").toString());
        if (rows.isEmpty() && op != SourceOp::Fetch) {
            DataTable t;
            t.columns = cols.isEmpty() ? QStringList{QStringLiteral("name")} : cols;
            return cp_table_result(t, summary);
        }
        return cp_table_result(table_from_json_array(rows), summary);
    }
    if (post == QLatin1String("etcd")) {
        QJsonArray rows;
        for (const QJsonValue& kv : o.value("kvs").toArray()) {
            const QJsonObject k = kv.toObject();
            QJsonObject row{{"key", QString::fromUtf8(QByteArray::fromBase64(k.value("key").toString().toLatin1()))}};
            if (k.contains(QStringLiteral("value")))
                row.insert(QStringLiteral("value"),
                           typed_text_cell(QString::fromUtf8(QByteArray::fromBase64(k.value("value").toString().toLatin1()))));
            row.insert(QStringLiteral("version"), k.value("version"));
            rows.append(row);
        }
        if (rows.isEmpty()) {
            DataTable t;
            t.columns = {QStringLiteral("key"), QStringLiteral("value")};
            return cp_table_result(t, cp_tr("no keys"));
        }
        return cp_table_result(table_from_json_array(rows), cp_tr("%1 key(s)").arg(rows.size()));
    }
    if (post == QLatin1String("prom") || post == QLatin1String("prom_names")) {
        PostResult r;
        if (o.value("status").toString() != QLatin1String("success")) {
            r.error = SourceError::make(ErrorCode::BadArgs,
                                        cp_tr("Query rejected: %1").arg(o.value("error").toString(cp_tr("unknown error"))),
                                        QStringLiteral("query"));
            return r;
        }
        if (post == QLatin1String("prom_names")) {
            DataTable t;
            t.columns = {QStringLiteral("name")};
            t.types = {ColumnType::String};
            for (const QJsonValue& v : o.value("data").toArray())
                t.rows.append(QJsonArray{v.toString()});
            return cp_table_result(t, cp_tr("%1 metric(s)").arg(t.row_count()));
        }
        const QJsonObject data = o.value("data").toObject();
        const QString type = data.value("resultType").toString();
        QJsonArray rows;
        const int max = call.parse.max_rows > 0 ? call.parse.max_rows : 100000;
        auto add = [&](const QJsonObject& labels, const QJsonArray& sample) {
            QJsonObject row;
            for (auto it = labels.constBegin(); it != labels.constEnd(); ++it)
                row.insert(it.key(), it.value());
            row.insert(QStringLiteral("timestamp"),
                       QDateTime::fromMSecsSinceEpoch(qint64(sample.at(0).toDouble() * 1000), QTimeZone::UTC).toString(Qt::ISODate));
            row.insert(QStringLiteral("value"), typed_text_cell(sample.at(1).toString()));
            rows.append(row);
        };
        if (type == QLatin1String("vector")) {
            for (const QJsonValue& s : data.value("result").toArray())
                if (rows.size() < max)
                    add(s.toObject().value("metric").toObject(), s.toObject().value("value").toArray());
        } else if (type == QLatin1String("matrix")) {
            for (const QJsonValue& s : data.value("result").toArray())
                for (const QJsonValue& v : s.toObject().value("values").toArray())
                    if (rows.size() < max)
                        add(s.toObject().value("metric").toObject(), v.toArray());
        } else if (type == QLatin1String("scalar") || type == QLatin1String("string")) {
            add(QJsonObject(), data.value("result").toArray());
        }
        if (rows.isEmpty() && op == SourceOp::Test) {
            DataTable t;
            t.columns = {QStringLiteral("value")};
            return cp_table_result(t, cp_tr("query API answered"));
        }
        PostResult res = cp_table_result(table_from_json_array(rows), op == SourceOp::Test ? cp_tr("query API answered") : QString());
        return res;
    }
    if (post == QLatin1String("opentsdb")) {
        QJsonArray rows;
        const int max = call.parse.max_rows > 0 ? call.parse.max_rows : 100000;
        for (const QJsonValue& s : root.toArray()) {
            const QJsonObject so = s.toObject();
            const QJsonObject dps = so.value("dps").toObject();
            for (auto it = dps.constBegin(); it != dps.constEnd() && rows.size() < max; ++it) {
                QJsonObject row{{"metric", so.value("metric")},
                                {"timestamp", QDateTime::fromSecsSinceEpoch(it.key().toLongLong(), QTimeZone::UTC).toString(Qt::ISODate)},
                                {"value", it.value()}};
                const QJsonObject tags = so.value("tags").toObject();
                for (auto t = tags.constBegin(); t != tags.constEnd(); ++t)
                    row.insert(t.key(), t.value());
                rows.append(row);
            }
        }
        return cp_table_result(table_from_json_array(rows));
    }
    if (post == QLatin1String("questdb")) {
        PostResult r;
        if (o.contains(QStringLiteral("error"))) {
            r.error = SourceError::make(ErrorCode::BadArgs, cp_tr("QuestDB: %1").arg(o.value("error").toString()),
                                        QStringLiteral("query"));
            return r;
        }
        DataTable t;
        for (const QJsonValue& c : o.value("columns").toArray()) {
            t.columns << c.toObject().value("name").toString();
            const QString qt = c.toObject().value("type").toString().toUpper();
            t.types << (qt == QLatin1String("DOUBLE") || qt == QLatin1String("FLOAT")   ? ColumnType::Number
                        : qt.contains(QLatin1String("INT")) || qt == QLatin1String("LONG") ? ColumnType::Integer
                        : qt == QLatin1String("TIMESTAMP")                                ? ColumnType::DateTime
                        : qt == QLatin1String("DATE")                                     ? ColumnType::Date
                        : qt == QLatin1String("BOOLEAN")                                  ? ColumnType::Bool
                                                                                          : ColumnType::String);
        }
        for (const QJsonValue& row : o.value("dataset").toArray())
            t.rows.append(row.toArray());
        return cp_table_result(t, op == SourceOp::Test && !t.rows.isEmpty()
                                      ? QStringLiteral("QuestDB %1").arg(cell_text(t.rows.first().at(0)))
                                      : QString());
    }
    if (post == QLatin1String("clickhouse")) {
        DataTable t;
        QStringList cols;
        for (const QJsonValue& m : o.value("meta").toArray()) {
            t.columns << m.toObject().value("name").toString();
            const QString ct = m.toObject().value("type").toString();
            t.types << (ct.contains(QLatin1String("Float")) || ct.contains(QLatin1String("Decimal")) ? ColumnType::Number
                        : ct.contains(QLatin1String("Int"))                                           ? ColumnType::Integer
                        : ct.startsWith(QLatin1String("DateTime"))                                    ? ColumnType::DateTime
                        : ct.startsWith(QLatin1String("Date"))                                        ? ColumnType::Date
                        : ct == QLatin1String("Bool")                                                 ? ColumnType::Bool
                                                                                                      : ColumnType::String);
        }
        for (const QJsonValue& row : o.value("data").toArray()) {
            QJsonArray r;
            const QJsonObject ro = row.toObject();
            for (int i = 0; i < t.columns.size(); ++i) {
                QJsonValue v = ro.value(t.columns[i]);
                // ClickHouse quotes 64-bit integers; make them numbers for typed columns.
                if (v.isString() && (t.types[i] == ColumnType::Integer || t.types[i] == ColumnType::Number))
                    v = typed_text_cell(v.toString());
                r.append(v);
            }
            t.rows.append(r);
        }
        QString summary;
        if (op == SourceOp::Test && !t.rows.isEmpty())
            summary = QStringLiteral("ClickHouse %1 · database %2").arg(cell_text(t.rows.first().at(0)), cell_text(t.rows.first().at(1)));
        return cp_table_result(t, summary);
    }
    if (post == QLatin1String("databricks")) {
        PostResult r;
        const QString state = o.value("status").toObject().value("state").toString();
        if (state == QLatin1String("FAILED") || state == QLatin1String("CANCELED")) {
            r.error = SourceError::make(ErrorCode::BadArgs,
                                        cp_tr("Databricks: %1").arg(o.value("status").toObject().value("error").toObject().value("message").toString(state)),
                                        QStringLiteral("query"));
            return r;
        }
        if (state == QLatin1String("PENDING") || state == QLatin1String("RUNNING")) {
            r.error = SourceError::make(ErrorCode::UpstreamDown,
                                        cp_tr("The SQL warehouse did not finish within 30 s (it may be starting). Retry in a minute."));
            return r;
        }
        DataTable t;
        for (const QJsonValue& c : o.value("manifest").toObject().value("schema").toObject().value("columns").toArray()) {
            t.columns << c.toObject().value("name").toString();
            t.types << column_type_from_name(c.toObject().value("type_name").toString().toLower() == QLatin1String("double")
                                                 ? QStringLiteral("number")
                                                 : c.toObject().value("type_name").toString());
        }
        for (const QJsonValue& row : o.value("result").toObject().value("data_array").toArray()) {
            QJsonArray rr;
            for (const QJsonValue& v : row.toArray())
                rr.append(v.isString() ? typed_text_cell(v.toString()) : v);
            t.rows.append(rr);
        }
        if (t.columns.isEmpty())
            t.columns = {QStringLiteral("result")};
        t.types = QVector<ColumnType>(t.columns.size(), ColumnType::Unknown);
        infer_column_types(t);
        return cp_table_result(t);
    }
    if (post == QLatin1String("iceberg_list")) {
        QJsonArray rows;
        for (const QJsonValue& n : o.value("namespaces").toArray())
            rows.append(QJsonObject{{"name", cell_text(n).remove(QLatin1Char('[')).remove(QLatin1Char(']')).remove(QLatin1Char('"'))},
                                    {"kind", "namespace"}});
        for (const QJsonValue& id : o.value("identifiers").toArray())
            rows.append(QJsonObject{{"name", id.toObject().value("name")}, {"kind", "table"}});
        return cp_table_result(table_from_json_array(rows), cp_tr("%1 item(s)").arg(rows.size()));
    }
    if (post == QLatin1String("iceberg_schema")) {
        const QJsonObject meta = o.value("metadata").toObject();
        const int current = meta.value("current-schema-id").toInt();
        QJsonArray rows;
        for (const QJsonValue& s : meta.value("schemas").toArray()) {
            if (s.toObject().value("schema-id").toInt() != current)
                continue;
            for (const QJsonValue& f : s.toObject().value("fields").toArray())
                rows.append(QJsonObject{{"id", f.toObject().value("id")},
                                        {"name", f.toObject().value("name")},
                                        {"type", cell_text(f.toObject().value("type"))},
                                        {"required", f.toObject().value("required")}});
        }
        PostResult r = cp_table_result(table_from_json_array(rows));
        r.data.meta.insert(QStringLiteral("note"),
                           cp_tr("Table schema from the catalog. Reading the data files needs a Parquet reader, which this "
                                 "build does not have."));
        return r;
    }
    if (post == QLatin1String("binance_klines") || post == QLatin1String("coinbase_candles")) {
        const bool cb = post == QLatin1String("coinbase_candles");
        QJsonArray rows;
        for (const QJsonValue& k : root.toArray()) {
            const QJsonArray a = k.toArray();
            if (cb)
                rows.append(QJsonObject{{"time", QDateTime::fromSecsSinceEpoch(qint64(a.at(0).toDouble()), QTimeZone::UTC).toString(Qt::ISODate)},
                                        {"open", a.at(3)}, {"high", a.at(2)}, {"low", a.at(1)}, {"close", a.at(4)}, {"volume", a.at(5)}});
            else
                rows.append(QJsonObject{{"time", QDateTime::fromMSecsSinceEpoch(qint64(a.at(0).toDouble()), QTimeZone::UTC).toString(Qt::ISODate)},
                                        {"open", typed_text_cell(a.at(1).toString())}, {"high", typed_text_cell(a.at(2).toString())},
                                        {"low", typed_text_cell(a.at(3).toString())}, {"close", typed_text_cell(a.at(4).toString())},
                                        {"volume", typed_text_cell(a.at(5).toString())}, {"trades", a.at(8)}});
        }
        DataTable t = table_from_records(rows, {"time", "open", "high", "low", "close", "volume", "trades"}, QJsonObject());
        if (cb) {
            t.columns.removeLast();
            t.types.removeLast();
            for (QJsonArray& r : t.rows)
                r.removeLast();
        }
        return cp_table_result(t);
    }
    if (post == QLatin1String("book")) {
        QJsonArray rows;
        for (const char* side : {"bids", "asks"})
            for (const QJsonValue& lvl : o.value(QLatin1String(side)).toArray())
                rows.append(QJsonObject{{"side", QString::fromLatin1(side).chopped(1)},
                                        {"price", typed_text_cell(lvl.toArray().at(0).toString())},
                                        {"quantity", typed_text_cell(lvl.toArray().at(1).toString())}});
        return cp_table_result(table_from_json_array(rows));
    }
    if (post == QLatin1String("kraken")) {
        PostResult r;
        const QString kerr = cell_text(o.value("error").toArray().first());
        if (!kerr.isEmpty()) {
            r.error = SourceError::make(ErrorCode::BadArgs, cp_tr("Kraken: %1").arg(kerr), QStringLiteral("query"));
            return r;
        }
        const QJsonObject result = o.value("result").toObject();
        if (op == SourceOp::Test)
            return cp_table_result(table_from_key_values(result), cp_tr("Kraken answered"));
        // OHLC: {"XXBTZUSD": [[time, open, high, low, close, vwap, volume, count], …], "last": n}
        for (auto it = result.constBegin(); it != result.constEnd(); ++it) {
            if (!it->isArray())
                continue;
            QJsonArray rows;
            for (const QJsonValue& k : it->toArray()) {
                const QJsonArray a = k.toArray();
                rows.append(QJsonObject{{"pair", it.key()},
                                        {"time", QDateTime::fromSecsSinceEpoch(qint64(a.at(0).toDouble()), QTimeZone::UTC).toString(Qt::ISODate)},
                                        {"open", typed_text_cell(a.at(1).toString())}, {"high", typed_text_cell(a.at(2).toString())},
                                        {"low", typed_text_cell(a.at(3).toString())}, {"close", typed_text_cell(a.at(4).toString())},
                                        {"vwap", typed_text_cell(a.at(5).toString())}, {"volume", typed_text_cell(a.at(6).toString())},
                                        {"trades", a.at(7)}});
            }
            return cp_table_result(table_from_records(rows, {"pair", "time", "open", "high", "low", "close", "vwap", "volume", "trades"},
                                                      QJsonObject()));
        }
        QString where;
        const QJsonValue recs = find_records(result, &where);
        return cp_table_result(recs.isArray() ? table_from_json_array(recs.toArray()) : table_from_key_values(result));
    }
    return generic();
}

} // namespace fincept::services::datasources
