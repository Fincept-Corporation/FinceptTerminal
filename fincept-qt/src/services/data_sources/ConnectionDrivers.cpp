// ConnectionDrivers.cpp — see ConnectionDrivers.h.

#include "services/data_sources/ConnectionDrivers.h"

#include "services/connectors/ConnectorCatalog.h"
#include "services/connectors/ConnectorCredentialBridge.h"
#include "services/connectors/ConnectorService.h"
#include "services/data_sources/RequestSigning.h"
#include "storage/secure/SecureStorage.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMutex>
#include <QMutexLocker>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QRegularExpression>
#include <QSqlDatabase>
#include <QSqlDriver>
#include <QSqlError>
#include <QSqlField>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QTcpSocket>
#include <QThread>
#include <QTimer>
#include <QUrlQuery>
#include <QUuid>

#ifdef HAS_QT_WEBSOCKETS
#    include <QWebSocket>
#    include <QWebSocketHandshakeOptions>
#endif
#ifdef FINCEPT_HAS_QXLSX
#    include <xlsxcell.h>
#    include <xlsxdocument.h>
#    include <xlsxworksheet.h>
#endif

#include <algorithm>
#include <chrono>
#include <future>

namespace fincept::services::datasources {

namespace {

constexpr qint64 kCdMaxBodyBytes = 64LL * 1024 * 1024;   ///< largest HTTP answer read into memory
constexpr qint64 kCdMaxFileBytes = 256LL * 1024 * 1024;  ///< larger files are sampled from the start
constexpr const char* kCdLocalFileConnector = "data_local_file"; ///< scripts/local_file_data.py

QString cd_tr(const char* text) {
    return QCoreApplication::translate("ConnectionDrivers", text);
}

bool cd_cancelled(const CancelToken& c) {
    return c && c->load();
}

QString cd_str(const QJsonObject& cfg, const char* key) {
    const QJsonValue v = cfg.value(QLatin1String(key));
    if (v.isDouble())
        return QString::number(v.toDouble(), 'g', 15);
    return v.toString().trimmed();
}

int cd_int(const QJsonObject& cfg, const char* key, int def) {
    const QJsonValue v = cfg.value(QLatin1String(key));
    if (v.isDouble() && v.toDouble() > 0)
        return int(v.toDouble());
    bool ok = false;
    const int i = v.toString().trimmed().toInt(&ok);
    return ok && i > 0 ? i : def;
}

QStringList cd_secrets_of(const QJsonObject& cfg) {
    QStringList out;
    for (auto it = cfg.constBegin(); it != cfg.constEnd(); ++it) {
        const QString k = it.key().toLower();
        if ((k.contains(QLatin1String("password")) || k.contains(QLatin1String("secret")) || k.contains(QLatin1String("token")) ||
             k.contains(QLatin1String("key")) || k == QLatin1String("credentials")) &&
            it->isString() && it->toString().size() >= 4 && !k.endsWith(QLatin1String("keyname")) &&
            !k.endsWith(QLatin1String("keyin")))
            out << it->toString();
    }
    return out;
}

SourceReply cd_fail(const SourceError& e) {
    SourceReply r;
    r.error = e;
    return r;
}

SourceReply cd_fail(ErrorCode code, const QString& message, const QString& field = QString()) {
    return cd_fail(SourceError::make(code, message, field));
}

SourceReply cd_ok(const DataTable& t, const QString& summary = QString()) {
    SourceReply r;
    r.ok = true;
    r.data.tables << t;
    r.summary = summary;
    return r;
}

QJsonValue cd_variant_cell(const QVariant& v) {
    if (!v.isValid() || v.isNull())
        return QJsonValue(QJsonValue::Null);
    switch (v.metaType().id()) {
        case QMetaType::Bool:
            return v.toBool();
        case QMetaType::Int:
        case QMetaType::UInt:
        case QMetaType::LongLong:
        case QMetaType::ULongLong:
        case QMetaType::Double:
        case QMetaType::Float:
        case QMetaType::Short:
        case QMetaType::UShort:
            return v.toDouble();
        case QMetaType::QDate:
            return v.toDate().toString(Qt::ISODate);
        case QMetaType::QDateTime:
            return v.toDateTime().toUTC().toString(Qt::ISODate);
        case QMetaType::QTime:
            return v.toTime().toString(Qt::ISODate);
        case QMetaType::QByteArray: {
            const QByteArray b = v.toByteArray();
            return b.size() > 256 ? QJsonValue(QStringLiteral("<%1 bytes>").arg(b.size())) : QJsonValue(QString::fromUtf8(b));
        }
        default:
            return v.toString();
    }
}

ColumnType cd_meta_type(int id) {
    switch (id) {
        case QMetaType::Bool:
            return ColumnType::Bool;
        case QMetaType::Int:
        case QMetaType::UInt:
        case QMetaType::LongLong:
        case QMetaType::ULongLong:
        case QMetaType::Short:
        case QMetaType::UShort:
            return ColumnType::Integer;
        case QMetaType::Double:
        case QMetaType::Float:
            return ColumnType::Number;
        case QMetaType::QDate:
            return ColumnType::Date;
        case QMetaType::QDateTime:
            return ColumnType::DateTime;
        default:
            return ColumnType::Unknown;
    }
}

DataTable cd_names_table(const QStringList& names, const QString& kind) {
    DataTable t;
    t.columns = {QStringLiteral("name"), QStringLiteral("kind")};
    t.types = {ColumnType::String, ColumnType::String};
    for (const QString& n : names)
        t.rows.append(QJsonArray{n, kind});
    return t;
}

// ═════════════════════════════════════════════════════════════════════════
// HTTP family
// ═════════════════════════════════════════════════════════════════════════

struct CdHttpAnswer {
    bool transport_ok = false;
    int status = 0;
    QByteArray body;
    QString content_type;
    QString retry_after;
    SourceError error;
    qint64 ms = 0;
};

SourceError cd_network_error(QNetworkReply::NetworkError e, const QString& what, const QString& detail) {
    switch (e) {
        case QNetworkReply::ConnectionRefusedError:
            return SourceError::make(ErrorCode::UpstreamDown, cd_tr("%1 refused the connection (nothing is listening on that "
                                                                    "host and port)").arg(what));
        case QNetworkReply::HostNotFoundError:
            return SourceError::make(ErrorCode::UpstreamDown, cd_tr("The host name of %1 could not be resolved").arg(what));
        case QNetworkReply::TimeoutError:
        case QNetworkReply::OperationCanceledError:
            return SourceError::make(ErrorCode::UpstreamDown, cd_tr("%1 did not answer in time").arg(what));
        case QNetworkReply::SslHandshakeFailedError:
            return SourceError::make(ErrorCode::UpstreamDown,
                                     cd_tr("The TLS handshake with %1 failed: %2").arg(what, detail));
        case QNetworkReply::ProxyAuthenticationRequiredError:
            return SourceError::make(ErrorCode::AuthFailed, cd_tr("The proxy requires authentication"));
        case QNetworkReply::RemoteHostClosedError:
            return SourceError::make(ErrorCode::UpstreamDown, cd_tr("%1 closed the connection").arg(what));
        default:
            return SourceError::make(ErrorCode::UpstreamDown, cd_tr("Could not reach %1: %2").arg(what, detail));
    }
}

/// A short server-side reason from an error body (JSON message / plain text), secrets redacted.
QString cd_server_message(const QByteArray& body, const QStringList& secrets) {
    QString msg;
    const QJsonDocument doc = QJsonDocument::fromJson(body);
    if (doc.isObject()) {
        const QJsonObject o = doc.object();
        for (const char* k : {"message", "error", "error_description", "detail", "reason", "title"}) {
            const QJsonValue v = o.value(QLatin1String(k));
            if (v.isString() && !v.toString().isEmpty()) {
                msg = v.toString();
                break;
            }
            if (v.isObject() && v.toObject().value("message").isString()) {
                msg = v.toObject().value("message").toString();
                break;
            }
        }
    } else if (!body.trimmed().startsWith('<')) {
        msg = QString::fromUtf8(body.left(200)).simplified();
    } else {
        const QRegularExpression code(QStringLiteral("<(Code|Message)>([^<]{1,200})</"));
        auto it = code.globalMatch(QString::fromUtf8(body.left(4096)));
        QStringList parts;
        while (it.hasNext())
            parts << it.next().captured(2);
        msg = parts.join(QStringLiteral(": "));
    }
    return redact_secrets(msg, secrets).left(200);
}

/// One HTTP exchange on this (worker) thread. `stream_limit` > 0: stop reading after that many
/// SSE events or `stream_ms` (event-stream sampling); the reply is then aborted on purpose.
CdHttpAnswer cd_http(QNetworkAccessManager& nam, HttpCall call, int timeout_ms, const CancelToken& cancel,
                     int stream_events = 0, int stream_ms = 0) {
    CdHttpAnswer a;
    QElapsedTimer clock;
    clock.start();

    if (call.pre_auth == QLatin1String("ibm_iam")) {
        HttpCall iam;
        iam.method = "POST";
        iam.url = QUrl(QStringLiteral("https://iam.cloud.ibm.com/identity/token"));
        iam.headers.emplaceBack("Content-Type", "application/x-www-form-urlencoded");
        iam.headers.emplaceBack("Accept", "application/json");
        iam.body = "grant_type=urn%3Aibm%3Aparams%3Aoauth%3Agrant-type%3Aapikey&apikey=" +
                   QUrl::toPercentEncoding(call.sign_params.value("api_key").toString());
        iam.what = QStringLiteral("IBM Cloud IAM");
        iam.secrets = call.secrets;
        const CdHttpAnswer t = cd_http(nam, iam, timeout_ms, cancel);
        if (!t.transport_ok || t.status >= 300) {
            a.error = t.transport_ok ? error_from_http_status(t.status, iam.what, t.retry_after,
                                                              cd_server_message(t.body, iam.secrets))
                                     : t.error;
            return a;
        }
        const QString token = QJsonDocument::fromJson(t.body).object().value("access_token").toString();
        if (token.isEmpty()) {
            a.error = SourceError::make(ErrorCode::AuthFailed, cd_tr("IBM Cloud IAM returned no access token"));
            return a;
        }
        call.headers.emplaceBack("Authorization", "Bearer " + token.toUtf8());
        call.secrets << token;
    }
    if (call.sign == QLatin1String("aws_v4")) {
        sign_aws_v4(call.method, call.url, call.headers, call.body, call.sign_params.value("access_key").toString(),
                    call.sign_params.value("secret_key").toString(), call.sign_params.value("region").toString(),
                    call.sign_params.value("service").toString(), QDateTime::currentDateTimeUtc());
    } else if (call.sign == QLatin1String("azure_shared_key")) {
        sign_azure_shared_key(call.method, call.url, call.headers, call.body, call.sign_params.value("account").toString(),
                              call.sign_params.value("key").toString(), QDateTime::currentDateTimeUtc());
    }

    QNetworkRequest req(call.url);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("FinceptTerminal-DataSources/1.0"));
    if (stream_events <= 0)
        req.setTransferTimeout(timeout_ms);
    for (const auto& h : std::as_const(call.headers))
        req.setRawHeader(h.first, h.second);

    QNetworkReply* reply = call.method == "GET" && call.body.isEmpty() ? nam.get(req)
                                                                       : nam.sendCustomRequest(req, call.method, call.body);
    QEventLoop loop;
    QTimer poll;
    poll.setInterval(100);
    bool cancelled = false;
    bool too_large = false;
    bool stream_done = false;
    QByteArray body;
    int events = 0;
    QObject::connect(&poll, &QTimer::timeout, &loop, [&]() {
        if (cd_cancelled(cancel)) {
            cancelled = true;
            reply->abort();
        } else if (stream_ms > 0 && clock.elapsed() >= stream_ms && !reply->isFinished()) {
            stream_done = true;
            reply->abort();
        } else if (stream_events <= 0 && clock.elapsed() > timeout_ms + 2000 && !reply->isFinished()) {
            reply->abort(); // belt and braces next to setTransferTimeout
        }
    });
    QObject::connect(reply, &QNetworkReply::readyRead, &loop, [&]() {
        body += reply->readAll();
        if (body.size() > kCdMaxBodyBytes) {
            too_large = true;
            reply->abort();
            return;
        }
        if (stream_events > 0) {
            events = int(body.count("\n\n")) + int(body.count("\r\n\r\n"));
            if (events >= stream_events) {
                stream_done = true;
                reply->abort();
            }
        }
    });
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    poll.start();
    if (!reply->isFinished())
        loop.exec();
    poll.stop();
    body += reply->readAll();
    a.ms = clock.elapsed();
    a.status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    a.content_type = reply->header(QNetworkRequest::ContentTypeHeader).toString();
    a.retry_after = QString::fromLatin1(reply->rawHeader("Retry-After"));
    a.body = body;
    const QNetworkReply::NetworkError err = reply->error();
    const QString err_text = redact_secrets(reply->errorString(), call.secrets);
    reply->deleteLater();

    if (cancelled) {
        a.error = SourceError::cancel();
        return a;
    }
    if (too_large) {
        a.error = SourceError::make(ErrorCode::BadArgs, cd_tr("The answer is larger than %1 MB — narrow the request (a path, "
                                                              "a filter or a smaller limit)")
                                                            .arg(kCdMaxBodyBytes / (1024 * 1024)));
        return a;
    }
    if (stream_done && a.status >= 200 && a.status < 300) {
        a.transport_ok = true;
        return a;
    }
    if (err != QNetworkReply::NoError && a.status == 0) {
        a.error = cd_network_error(err, call.what.isEmpty() ? call.url.host() : call.what, err_text);
        return a;
    }
    a.transport_ok = true;
    return a;
}

#ifdef FINCEPT_HAS_QXLSX
/// Rows of one sheet (or the first) of an .xlsx workbook; `sheet_names` receives every sheet.
bool cd_read_xlsx(QIODevice* dev, const QString& sheet, bool has_header, int limit, DataTable* out, QStringList* sheet_names,
                  bool* truncated, QString* why) {
    QXlsx::Document xlsx(dev);
    if (!xlsx.load()) {
        *why = cd_tr("The file is not a readable .xlsx workbook");
        return false;
    }
    *sheet_names = xlsx.sheetNames();
    if (sheet_names->isEmpty()) {
        *why = cd_tr("The workbook has no sheets");
        return false;
    }
    QString target = sheet;
    bool idx_ok = false;
    const int idx = sheet.toInt(&idx_ok);
    if (idx_ok && idx >= 0 && idx < sheet_names->size())
        target = sheet_names->at(idx);
    if (target.isEmpty())
        target = sheet_names->first();
    if (!sheet_names->contains(target)) {
        *why = cd_tr("No sheet named \"%1\" (sheets: %2)").arg(target, sheet_names->join(QStringLiteral(", ")));
        return false;
    }
    xlsx.selectSheet(target);
    QXlsx::Worksheet* ws = xlsx.currentWorksheet();
    if (!ws) {
        *why = cd_tr("\"%1\" is a chart sheet, not a table").arg(target);
        return false;
    }
    const auto dim = ws->dimension();
    const int last_row = dim.lastRow();
    const int last_col = std::min(dim.lastColumn(), 512);
    const int first = dim.firstRow() > 0 ? dim.firstRow() : 1;
    auto cell_value = [&](int r, int c) -> QJsonValue {
        QXlsx::Cell* cell = ws->cellAt(r, c);
        if (!cell)
            return QJsonValue(QJsonValue::Null);
        if (cell->isDateTime()) {
            const QVariant dt = cell->dateTime();
            if (dt.metaType().id() == QMetaType::QDateTime) {
                const QDateTime d = dt.toDateTime();
                return d.time() == QTime(0, 0) ? QJsonValue(d.date().toString(Qt::ISODate)) : QJsonValue(d.toString(Qt::ISODate));
            }
            return cd_variant_cell(dt);
        }
        return cd_variant_cell(cell->value());
    };
    DataTable t;
    t.name = target;
    int data_start = first;
    if (has_header) {
        for (int c = 1; c <= last_col; ++c) {
            QString h = cell_text(cell_value(first, c)).trimmed();
            if (h.isEmpty())
                h = QStringLiteral("column_%1").arg(c);
            QString unique = h;
            for (int n = 2; t.columns.contains(unique); ++n)
                unique = h + QStringLiteral("_%1").arg(n);
            t.columns << unique;
        }
        data_start = first + 1;
    } else {
        for (int c = 1; c <= last_col; ++c)
            t.columns << QStringLiteral("column_%1").arg(c);
    }
    for (int r = data_start; r <= last_row; ++r) {
        if (limit > 0 && t.rows.size() >= limit) {
            *truncated = true;
            break;
        }
        QJsonArray row;
        bool any = false;
        for (int c = 1; c <= last_col; ++c) {
            const QJsonValue v = cell_value(r, c);
            any = any || !v.isNull();
            row.append(v);
        }
        if (any)
            t.rows.append(row);
    }
    t.types = QVector<ColumnType>(t.columns.size(), ColumnType::Unknown);
    infer_column_types(t);
    *out = t;
    return true;
}
#endif

/// Bytes of a file (CSV, JSON, XML, XLSX) — local or fetched from an object store — into a table.
SourceReply cd_table_from_bytes(const QByteArray& bytes, const QString& name, const ParseOptions& opts, const QString& sheet,
                                bool has_header, int limit) {
    const QString lower = name.toLower();
    if (lower.endsWith(QLatin1String(".xlsx")) || lower.endsWith(QLatin1String(".xlsm"))) {
#ifdef FINCEPT_HAS_QXLSX
        QByteArray copy = bytes;
        QBuffer buf(&copy);
        buf.open(QIODevice::ReadOnly);
        DataTable t;
        QStringList sheets;
        bool truncated = false;
        QString why;
        if (!cd_read_xlsx(&buf, sheet, has_header, limit, &t, &sheets, &truncated, &why))
            return cd_fail(ErrorCode::BadArgs, why, QStringLiteral("sheet"));
        SourceReply r = cd_ok(t, cd_tr("sheet %1 · %2 row(s)").arg(t.name, QString::number(t.row_count())));
        r.data.truncated = truncated;
        r.data.meta.insert(QStringLiteral("sheets"), QJsonArray::fromStringList(sheets));
        return r;
#else
        return cd_fail(ErrorCode::NotSupported, cd_tr("This build has no Excel reader; save the sheet as CSV."));
#endif
    }
    if (lower.endsWith(QLatin1String(".xls")))
        return cd_fail(ErrorCode::NotSupported, cd_tr("Legacy .xls workbooks are not supported; save it as .xlsx or CSV."));
    for (const char* ext : {".parquet", ".feather", ".arrow", ".orc", ".avro"})
        if (lower.endsWith(QLatin1String(ext)))
            return cd_fail(ErrorCode::NotSupported, cd_tr("This build has no Parquet / Arrow / ORC / Avro reader; convert the "
                                                          "file to CSV."));
    ParseOptions o = opts;
    if (o.file_name.isEmpty())
        o.file_name = name;
    o.max_rows = limit;
    const ParseOutcome po = parse_table(bytes, o);
    if (!po.ok)
        return cd_fail(po.error);
    SourceReply r = cd_ok(po.table, cd_tr("%1 row(s) · %2 column(s)").arg(po.table.row_count()).arg(po.table.column_count()));
    r.data.truncated = po.truncated;
    if (!po.note.isEmpty())
        r.notes << po.note;
    return r;
}

SourceReply cd_run_http(const TypeSupport& sup, const SourceRequest& req, const CancelToken& cancel) {
    const HttpPlan plan = http_plan(sup, req.op, req.config, req.entity, req.query, req.limit);
    if (!plan.ok)
        return cd_fail(plan.error);
    if (!plan.static_rows.isEmpty())
        return cd_ok(table_from_json_array(plan.static_rows), cd_tr("%1 endpoint(s)").arg(plan.static_rows.size()));

    QNetworkAccessManager nam;
    CdHttpAnswer last;
    HttpCall last_call;
    const bool stream = sup.driver == DriverKind::EventStream;
    for (const HttpCall& call : plan.calls) {
        int events = 0;
        int ms = 0;
        if (stream) {
            events = req.op == SourceOp::Test ? 1 : std::max(1, call.parse.max_rows);
            ms = (req.op == SourceOp::Test ? 4 : cd_int(req.config, "sampleSeconds", 5)) * 1000;
        }
        last = cd_http(nam, call, req.timeout_ms, cancel, events, ms);
        last_call = call;
        if (!last.transport_ok)
            return cd_fail(last.error);
        if (last.status < 200 || last.status >= 300) {
            return cd_fail(error_from_http_status(last.status, call.what.isEmpty() ? call.url.host() : call.what,
                                                  last.retry_after, cd_server_message(last.body, call.secrets)));
        }
    }
    SourceReply r;
    if (!last_call.object_name.isEmpty()) {
        ParseOptions o = last_call.parse;
        o.content_type = last.content_type;
        r = cd_table_from_bytes(last.body, last_call.object_name, o, cd_str(req.config, "sheet"), true, req.limit);
    } else if (stream && last.body.trimmed().isEmpty()) {
        if (req.op == SourceOp::Test) {
            DataTable t;
            t.columns = {QStringLiteral("data")};
            r = cd_ok(t, cd_tr("stream open (HTTP %1) — no event in the first seconds").arg(last.status));
        } else {
            r = cd_fail(ErrorCode::NoData, cd_tr("The stream sent no events during the sample window"));
        }
    } else {
        const PostResult pr = post_process(last_call, req.op, last.body, last.content_type);
        if (!pr.ok) {
            if (req.op == SourceOp::Test && pr.error.code == ErrorCode::NoData) {
                DataTable t;
                t.columns = {QStringLiteral("value")};
                r = cd_ok(t, cd_tr("HTTP %1 — answered with no rows").arg(last.status));
            } else {
                SourceError e = pr.error;
                e.message = redact_secrets(e.message, last_call.secrets);
                r = cd_fail(e);
            }
        } else {
            r.ok = true;
            r.data = pr.data;
            r.summary = pr.summary;
            if (pr.data.meta.contains(QStringLiteral("note")))
                r.notes << pr.data.meta.value(QStringLiteral("note")).toString();
        }
    }
    if (r.ok && req.op == SourceOp::Test) {
        const QString base = cd_tr("HTTP %1 in %2 ms").arg(last.status).arg(last.ms);
        r.summary = r.summary.isEmpty() ? base : r.summary + QStringLiteral(" · ") + base;
    }
    r.latency_ms = last.ms;
    return r;
}

// ═════════════════════════════════════════════════════════════════════════
// WebSocket sampling
// ═════════════════════════════════════════════════════════════════════════

SourceReply cd_run_websocket(const SourceRequest& req, const CancelToken& cancel) {
#ifdef HAS_QT_WEBSOCKETS
    const QString url = cd_str(req.config, "url");
    if (url.isEmpty())
        return cd_fail(ErrorCode::BadArgs, cd_tr("Enter the WebSocket URL"), QStringLiteral("url"));
    const QUrl u(url);
    if (u.scheme() != QLatin1String("ws") && u.scheme() != QLatin1String("wss"))
        return cd_fail(ErrorCode::BadArgs, cd_tr("A WebSocket URL starts with ws:// or wss://"), QStringLiteral("url"));
    const int max_msgs = req.op == SourceOp::Test ? 1 : std::min(req.limit, cd_int(req.config, "maxMessages", 50));
    const int window_ms = (req.op == SourceOp::Test ? 4 : cd_int(req.config, "sampleSeconds", 5)) * 1000;
    QString subscribe = req.query.trimmed().isEmpty() ? req.config.value("subscribe").toString().trimmed() : req.query.trimmed();
    QStringList secrets = cd_secrets_of(req.config);

    QWebSocket ws;
    QNetworkRequest nreq(u);
    const QString token = cd_str(req.config, "authToken");
    if (!token.isEmpty())
        nreq.setRawHeader("Authorization", "Bearer " + token.toUtf8());
    QWebSocketHandshakeOptions opts;
    QStringList protocols;
    for (const QString& p : cd_str(req.config, "protocols").split(QLatin1Char(','), Qt::SkipEmptyParts))
        protocols << p.trimmed();
    opts.setSubprotocols(protocols);

    QEventLoop loop;
    QTimer poll;
    poll.setInterval(100);
    QElapsedTimer clock;
    clock.start();
    qint64 connected_ms = -1;
    bool cancelled = false;
    QString socket_error;
    QJsonArray rows;
    QObject::connect(&ws, &QWebSocket::connected, &loop, [&]() {
        connected_ms = clock.elapsed();
        clock.restart();
        // Several messages may be needed (NATS: "CONNECT {}" then "SUB x 1"): one per line when not JSON.
        if (!subscribe.isEmpty()) {
            const QJsonDocument doc = QJsonDocument::fromJson(subscribe.toUtf8());
            if (!doc.isNull())
                ws.sendTextMessage(subscribe);
            else
                for (const QString& line : subscribe.split(QLatin1Char('\n'), Qt::SkipEmptyParts))
                    ws.sendTextMessage(line.trimmed() + QStringLiteral("\r\n"));
        }
    });
    auto take = [&](const QString& text) {
        const QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8());
        if (doc.isObject())
            rows.append(doc.object());
        else if (doc.isArray())
            rows.append(QJsonObject{{"message", doc.array()}});
        else
            rows.append(QJsonObject{{"message", text.left(2000)}});
        if (rows.size() >= max_msgs)
            loop.quit();
    };
    QObject::connect(&ws, &QWebSocket::textMessageReceived, &loop, take);
    QObject::connect(&ws, &QWebSocket::binaryMessageReceived, &loop,
                     [&](const QByteArray& b) { take(QString::fromUtf8(b)); });
    QObject::connect(&ws, &QWebSocket::errorOccurred, &loop, [&](QAbstractSocket::SocketError) {
        socket_error = ws.errorString();
        loop.quit();
    });
    QObject::connect(&ws, &QWebSocket::disconnected, &loop, &QEventLoop::quit);
    QObject::connect(&poll, &QTimer::timeout, &loop, [&]() {
        if (cd_cancelled(cancel)) {
            cancelled = true;
            loop.quit();
        } else if (connected_ms < 0 && clock.elapsed() > req.timeout_ms) {
            socket_error = cd_tr("no handshake within %1 s").arg(req.timeout_ms / 1000);
            loop.quit();
        } else if (connected_ms >= 0 && clock.elapsed() >= window_ms) {
            loop.quit();
        }
    });
    poll.start();
    ws.open(nreq, opts);
    loop.exec();
    poll.stop();
    const qint64 sampled = clock.elapsed();
    ws.abort();
    if (cancelled)
        return cd_fail(SourceError::cancel());
    if (connected_ms < 0) {
        const QString e = redact_secrets(socket_error, secrets);
        if (e.contains(QLatin1String("401")) || e.contains(QLatin1String("403")))
            return cd_fail(ErrorCode::AuthFailed, cd_tr("The server refused the WebSocket handshake: %1").arg(e));
        return cd_fail(ErrorCode::UpstreamDown, cd_tr("Could not open the WebSocket: %1").arg(e));
    }
    SourceReply r;
    r.latency_ms = connected_ms;
    if (rows.isEmpty()) {
        if (req.op == SourceOp::Test) {
            DataTable t;
            t.columns = {QStringLiteral("message")};
            r = cd_ok(t, cd_tr("connected in %1 ms · no message in %2 s (send a subscribe message?)")
                             .arg(connected_ms)
                             .arg(sampled / 1000));
            r.latency_ms = connected_ms;
            return r;
        }
        return cd_fail(ErrorCode::NoData,
                       cd_tr("Connected, but no message arrived in %1 s. Many feeds need a subscribe message first.")
                           .arg(window_ms / 1000));
    }
    r = cd_ok(table_from_json_array(rows),
              cd_tr("connected in %1 ms · %2 message(s) in %3 s").arg(connected_ms).arg(rows.size()).arg(sampled / 1000.0, 0, 'f', 1));
    r.latency_ms = connected_ms;
    return r;
#else
    Q_UNUSED(req);
    Q_UNUSED(cancel);
    return cd_fail(ErrorCode::NotSupported, cd_tr("This build was made without Qt WebSockets."));
#endif
}

// ═════════════════════════════════════════════════════════════════════════
// Local files
// ═════════════════════════════════════════════════════════════════════════

SourceReply cd_run_local_file(const SourceRequest& req, const CancelToken& cancel);

/// Excel files the native reader handles (.xlsx / .xlsm with QXlsx); the rest go to data_local_file.
bool cd_native_excel(const QString& file_name) {
#ifdef FINCEPT_HAS_QXLSX
    return !file_name.endsWith(QLatin1String(".xls"), Qt::CaseInsensitive);
#else
    Q_UNUSED(file_name);
    return false;
#endif
}

SourceReply cd_run_file(const TypeSupport& sup, const SourceRequest& req, const CancelToken& cancel) {
    const QString path = cd_str(req.config, "filepath");
    if (path.isEmpty())
        return cd_fail(ErrorCode::BadArgs, cd_tr("Choose the file"), QStringLiteral("filepath"));
    if (sup.profile == QLatin1String("connector") ||
        (sup.profile == QLatin1String("excel") && !cd_native_excel(QFileInfo(path).fileName())))
        return cd_run_local_file(req, cancel);
    const QFileInfo info(path);
    if (!info.exists())
        return cd_fail(ErrorCode::BadArgs, cd_tr("File not found: %1").arg(path), QStringLiteral("filepath"));
    if (!info.isFile())
        return cd_fail(ErrorCode::BadArgs, cd_tr("Not a file: %1").arg(path), QStringLiteral("filepath"));
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) // never anything but read-only
        return cd_fail(ErrorCode::AuthFailed, cd_tr("The file cannot be read: %1").arg(f.errorString()), QStringLiteral("filepath"));
    if (cd_cancelled(cancel))
        return cd_fail(SourceError::cancel());
    QElapsedTimer clock;
    clock.start();
    const bool partial = info.size() > kCdMaxFileBytes;
    const QByteArray bytes = f.read(partial ? kCdMaxFileBytes : info.size());
    f.close();

    ParseOptions o;
    QString name = info.fileName();
    const int limit = req.op == SourceOp::Test ? 50 : req.limit;
    if (sup.profile == QLatin1String("csv")) {
        o.format = name.endsWith(QLatin1String(".tsv"), Qt::CaseInsensitive) ? TextFormat::Tsv : TextFormat::Csv;
        QString d = cd_str(req.config, "delimiter");
        if (d == QLatin1String("\\t") || d.compare(QLatin1String("tab"), Qt::CaseInsensitive) == 0)
            o.delimiter = QLatin1Char('\t');
        else if (d.size() == 1)
            o.delimiter = d.at(0);
        o.has_header = req.config.contains(QStringLiteral("hasHeader")) ? req.config.value("hasHeader").toBool(true) : true;
    } else if (sup.profile == QLatin1String("json")) {
        o.format = name.endsWith(QLatin1String("l"), Qt::CaseInsensitive) || name.endsWith(QLatin1String(".ndjson"))
                       ? TextFormat::NdJson
                       : TextFormat::Auto;
        o.rows_path = cd_str(req.config, "jsonPath");
    } else if (sup.profile == QLatin1String("xml")) {
        o.format = TextFormat::Xml;
        o.rows_path = cd_str(req.config, "xpath");
    } else if (sup.profile == QLatin1String("excel")) {
        if (!name.endsWith(QLatin1String(".xlsx"), Qt::CaseInsensitive) && !name.endsWith(QLatin1String(".xlsm"), Qt::CaseInsensitive) &&
            !name.endsWith(QLatin1String(".xls"), Qt::CaseInsensitive))
            name += QStringLiteral(".xlsx");
    }
    const bool header = req.config.contains(QStringLiteral("hasHeader")) ? req.config.value("hasHeader").toBool(true) : true;
    const QString sheet = req.entity.isEmpty() ? cd_str(req.config, "sheet") : req.entity;
    SourceReply r = cd_table_from_bytes(bytes, name, o, sheet, header, limit);
    if (!r.ok)
        return r;
    if (req.op == SourceOp::List && sup.profile == QLatin1String("excel")) {
        QStringList sheets;
        for (const QJsonValue& s : r.data.meta.value(QStringLiteral("sheets")).toArray())
            sheets << s.toString();
        SourceReply l = cd_ok(cd_names_table(sheets, QStringLiteral("sheet")), cd_tr("%1 sheet(s)").arg(sheets.size()));
        return l;
    }
    r.data.truncated = r.data.truncated || partial;
    r.data.source = info.fileName();
    r.latency_ms = clock.elapsed();
    if (req.op == SourceOp::Test)
        r.summary = cd_tr("%1 · %2 · %3 column(s)")
                        .arg(info.fileName(), QLocale().formattedDataSize(info.size()))
                        .arg(r.data.tables.first().column_count());
    if (partial)
        r.notes << cd_tr("Only the first %1 MB of the file were read").arg(kCdMaxFileBytes / (1024 * 1024));
    return r;
}

// ═════════════════════════════════════════════════════════════════════════
// SQL (Qt drivers that load on this machine)
// ═════════════════════════════════════════════════════════════════════════

bool cd_driver_loads(const QString& driver) {
    static QMutex m;
    static QHash<QString, bool> cache;
    QMutexLocker lock(&m);
    const auto it = cache.constFind(driver);
    if (it != cache.constEnd())
        return it.value();
    bool ok = false;
    if (QSqlDatabase::isDriverAvailable(driver)) {
        const QString name = QStringLiteral("ds_probe_") + driver;
        {
            QSqlDatabase db = QSqlDatabase::addDatabase(driver, name);
            ok = db.isValid() && db.driver() && !db.lastError().text().contains(QLatin1String("not loaded"), Qt::CaseInsensitive);
        }
        QSqlDatabase::removeDatabase(name);
    }
    cache.insert(driver, ok);
    return ok;
}

QStringList cd_odbc_drivers(const QString& flavour) {
    if (flavour == QLatin1String("mssql"))
        return {QStringLiteral("ODBC Driver 18 for SQL Server"), QStringLiteral("ODBC Driver 17 for SQL Server"),
                QStringLiteral("SQL Server")};
    if (flavour == QLatin1String("postgres"))
        return {QStringLiteral("PostgreSQL Unicode(x64)"), QStringLiteral("PostgreSQL Unicode"), QStringLiteral("PostgreSQL ANSI")};
    if (flavour == QLatin1String("mysql"))
        return {QStringLiteral("MySQL ODBC 9.0 Unicode Driver"), QStringLiteral("MySQL ODBC 8.0 Unicode Driver"),
                QStringLiteral("MariaDB ODBC 3.2 Driver"), QStringLiteral("MariaDB ODBC 3.1 Driver")};
    if (flavour == QLatin1String("snowflake"))
        return {QStringLiteral("SnowflakeDSIIDriver")};
    if (flavour == QLatin1String("vertica"))
        return {QStringLiteral("Vertica")};
    if (flavour == QLatin1String("oracle"))
        return {QStringLiteral("Oracle in instantclient_23"), QStringLiteral("Oracle in instantclient_21"),
                QStringLiteral("Oracle in OraClient19Home1")};
    return {};
}

QString cd_install_step(const QString& flavour) {
    if (flavour == QLatin1String("postgres"))
        return cd_tr("Install the PostgreSQL client library (libpq: the PostgreSQL installer's \"Command Line Tools\", or "
                     "\"apt install libpq5\" / \"brew install libpq\") so the bundled Qt PostgreSQL driver can load — or, on "
                     "Windows, install the psqlODBC driver.");
    if (flavour == QLatin1String("mysql"))
        return cd_tr("On Windows install \"MySQL Connector/ODBC\" (or \"MariaDB Connector/ODBC\"); other platforms have no "
                     "MySQL driver in this build — export to CSV instead.");
    if (flavour == QLatin1String("mssql"))
        return cd_tr("On Windows install \"ODBC Driver 18 for SQL Server\" from Microsoft (the built-in \"SQL Server\" driver "
                     "also works); this build has no SQL Server driver on Linux / macOS.");
    if (flavour == QLatin1String("oracle"))
        return cd_tr("Install Oracle Instant Client with its ODBC package on Windows; other platforms have no Oracle driver in "
                     "this build.");
    if (flavour == QLatin1String("snowflake"))
        return cd_tr("Install the Snowflake ODBC driver (SnowflakeDSIIDriver) on Windows.");
    if (flavour == QLatin1String("vertica"))
        return cd_tr("Install the Vertica ODBC client driver on Windows.");
    return QString();
}

QString cd_native_driver(const QString& flavour) {
    if (flavour == QLatin1String("sqlite"))
        return QStringLiteral("QSQLITE");
    if (flavour == QLatin1String("postgres"))
        return QStringLiteral("QPSQL");
    if (flavour == QLatin1String("mysql"))
        return QStringLiteral("QMYSQL");
    if (flavour == QLatin1String("oracle"))
        return QStringLiteral("QOCI");
    return QString();
}

ErrorCode cd_sql_error_code(const QString& text) {
    const QString t = text.toLower();
    if (t.contains(QLatin1String("password")) || t.contains(QLatin1String("access denied")) ||
        t.contains(QLatin1String("login failed")) || t.contains(QLatin1String("authentication")) ||
        t.contains(QLatin1String("not authorized")))
        return ErrorCode::AuthFailed;
    if (t.contains(QLatin1String("could not connect")) || t.contains(QLatin1String("connection refused")) ||
        t.contains(QLatin1String("can't connect")) || t.contains(QLatin1String("timeout")) ||
        t.contains(QLatin1String("timed out")) || t.contains(QLatin1String("could not translate host")) ||
        t.contains(QLatin1String("unknown host")) || t.contains(QLatin1String("network")) ||
        t.contains(QLatin1String("server does not exist")))
        return ErrorCode::UpstreamDown;
    if (t.contains(QLatin1String("data source name not found")) || t.contains(QLatin1String("driver not loaded")) ||
        t.contains(QLatin1String("can't open lib")))
        return ErrorCode::NotSupported;
    return ErrorCode::Unknown;
}

bool cd_is_read_query(const QString& sql) {
    static const QRegularExpression kRead(
        QStringLiteral("^\\s*(select|with|show|describe|desc|explain|pragma|values|table)\\b"),
        QRegularExpression::CaseInsensitiveOption);
    QString s = sql;
    static const QRegularExpression kComments(QStringLiteral("^(\\s*(--[^\\n]*\\n|/\\*.*?\\*/))+"),
                                              QRegularExpression::DotMatchesEverythingOption);
    s.remove(kComments);
    return kRead.match(s).hasMatch();
}

SourceReply cd_run_sql(const TypeSupport& sup, const SourceRequest& req, const CancelToken& cancel) {
    const QString flavour = sup.profile;
    QString why, fix;
    const QString driver = sql_driver_for(flavour, &why, &fix);
    if (driver.isEmpty()) {
        SourceError e = SourceError::make(ErrorCode::NotSupported, why);
        e.fix = fix;
        return cd_fail(e);
    }
    const QJsonObject& cfg = req.config;
    const QString name = QStringLiteral("ds_conn_") + QUuid::createUuid().toString(QUuid::Id128);
    QStringList secrets = cd_secrets_of(cfg);
    SourceReply out;
    QElapsedTimer clock;
    clock.start();
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(driver, name);
        if (driver == QLatin1String("QSQLITE")) {
            const QString path = cd_str(cfg, "filepath");
            if (path.isEmpty() || !QFileInfo::exists(path)) {
                db = QSqlDatabase();
                QSqlDatabase::removeDatabase(name);
                return cd_fail(ErrorCode::BadArgs,
                               path.isEmpty() ? cd_tr("Choose the database file") : cd_tr("File not found: %1").arg(path),
                               QStringLiteral("filepath"));
            }
            db.setDatabaseName(path);
            db.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY;QSQLITE_BUSY_TIMEOUT=3000"));
        } else {
            const QString host = cd_str(cfg, "host").isEmpty() ? cd_str(cfg, "server").isEmpty() ? cd_str(cfg, "account")
                                                                                                    : cd_str(cfg, "server")
                                                               : cd_str(cfg, "host");
            const QString database = cd_str(cfg, "database").isEmpty() ? cd_str(cfg, "serviceName") : cd_str(cfg, "database");
            const int port = cd_int(cfg, "port", 0);
            const QString user = cd_str(cfg, "username");
            const QString pass = cfg.value(QStringLiteral("password")).toString();
            if (host.isEmpty()) {
                db = QSqlDatabase();
                QSqlDatabase::removeDatabase(name);
                return cd_fail(ErrorCode::BadArgs, cd_tr("Enter the host"), QStringLiteral("host"));
            }
            if (driver == QLatin1String("QODBC")) {
                // DSN-less connection string for the ODBC driver sql_driver_for() found.
                QString server = host;
                if (port > 0)
                    server += (flavour == QLatin1String("mssql") ? QStringLiteral(",") : QStringLiteral(":")) + QString::number(port);
                QString cs = QStringLiteral("DRIVER={%1};").arg(why); // `why` carries the ODBC driver name on success
                if (flavour == QLatin1String("mssql"))
                    cs += QStringLiteral("SERVER=%1;DATABASE=%2;Encrypt=yes;TrustServerCertificate=yes;").arg(server, database);
                else if (flavour == QLatin1String("snowflake"))
                    cs += QStringLiteral("SERVER=%1.snowflakecomputing.com;DATABASE=%2;WAREHOUSE=%3;SCHEMA=%4;ROLE=%5;")
                              .arg(host, database, cd_str(cfg, "warehouse"), cd_str(cfg, "schema"), cd_str(cfg, "role"));
                else
                    cs += QStringLiteral("SERVER=%1;PORT=%2;DATABASE=%3;").arg(host).arg(port).arg(database);
                db.setDatabaseName(cs);
                db.setConnectOptions(QStringLiteral("SQL_ATTR_LOGIN_TIMEOUT=10;SQL_ATTR_CONNECTION_TIMEOUT=10"));
            } else {
                db.setHostName(host);
                db.setDatabaseName(database);
                if (port > 0)
                    db.setPort(port);
                if (driver == QLatin1String("QPSQL"))
                    db.setConnectOptions(QStringLiteral("connect_timeout=10") +
                                         (req.config.value("ssl").toBool() ? QStringLiteral(";requiressl=1") : QString()));
                else if (driver == QLatin1String("QMYSQL"))
                    db.setConnectOptions(QStringLiteral("MYSQL_OPT_CONNECT_TIMEOUT=10;MYSQL_OPT_READ_TIMEOUT=60"));
            }
            db.setUserName(user);
            db.setPassword(pass);
        }
        if (cd_cancelled(cancel)) {
            db = QSqlDatabase();
            QSqlDatabase::removeDatabase(name);
            return cd_fail(SourceError::cancel());
        }
        if (!db.open()) {
            const QString text = redact_secrets(db.lastError().text(), secrets);
            const ErrorCode code = cd_sql_error_code(text);
            db = QSqlDatabase();
            QSqlDatabase::removeDatabase(name);
            SourceError e = SourceError::make(code == ErrorCode::Unknown ? ErrorCode::UpstreamDown : code,
                                              cd_tr("Could not open the database: %1").arg(text.left(300)));
            if (code == ErrorCode::NotSupported)
                e.fix = cd_install_step(flavour);
            return cd_fail(e);
        }
        out.latency_ms = clock.elapsed();

        if (req.op == SourceOp::Test || req.op == SourceOp::List) {
            const QStringList tables = db.tables(QSql::Tables);
            const QStringList views = db.tables(QSql::Views);
            if (req.op == SourceOp::List) {
                DataTable t = cd_names_table(tables, QStringLiteral("table"));
                for (const QString& v : views)
                    t.rows.append(QJsonArray{v, QStringLiteral("view")});
                out.ok = true;
                out.data.tables << t;
                out.summary = cd_tr("%1 table(s), %2 view(s)").arg(tables.size()).arg(views.size());
            } else {
                QString version_sql = QStringLiteral("SELECT version()");
                if (driver == QLatin1String("QSQLITE"))
                    version_sql = QStringLiteral("SELECT 'SQLite ' || sqlite_version()");
                else if (flavour == QLatin1String("mssql"))
                    version_sql = QStringLiteral("SELECT @@VERSION");
                else if (flavour == QLatin1String("oracle"))
                    version_sql = QStringLiteral("SELECT banner FROM v$version");
                else if (flavour == QLatin1String("snowflake"))
                    version_sql = QStringLiteral("SELECT 'Snowflake ' || CURRENT_VERSION()");
                QSqlQuery q(db);
                QString version;
                if (q.exec(version_sql) && q.next())
                    version = q.value(0).toString().section(QLatin1Char('\n'), 0, 0).left(80);
                DataTable t;
                t.columns = {QStringLiteral("server"), QStringLiteral("tables"), QStringLiteral("views")};
                t.types = {ColumnType::String, ColumnType::Integer, ColumnType::Integer};
                t.rows.append(QJsonArray{version, double(tables.size()), double(views.size())});
                out.ok = true;
                out.data.tables << t;
                out.summary = cd_tr("%1 · %2 table(s) · connected in %3 ms")
                                  .arg(version.isEmpty() ? driver : version)
                                  .arg(tables.size())
                                  .arg(out.latency_ms);
            }
        } else {
            QString sql = req.query.trimmed();
            if (sql.isEmpty()) {
                if (req.entity.isEmpty()) {
                    db.close();
                    db = QSqlDatabase();
                    QSqlDatabase::removeDatabase(name);
                    return cd_fail(ErrorCode::BadArgs, cd_tr("Pick a table or enter a query"), QStringLiteral("query"));
                }
                const QString quoted = db.driver()->escapeIdentifier(req.entity, QSqlDriver::TableName);
                // One .arg() call per template: a chained .arg(limit) re-scans the text and would also fill a
                // "%2" inside the table name ("sales%2" -> "sales100"), querying the wrong table.
                const QString limit = QString::number(req.limit);
                sql = flavour == QLatin1String("mssql") ? QStringLiteral("SELECT TOP %1 * FROM %2").arg(limit, quoted)
                      : flavour == QLatin1String("oracle")
                          ? QStringLiteral("SELECT * FROM %1 FETCH FIRST %2 ROWS ONLY").arg(quoted, limit)
                          : QStringLiteral("SELECT * FROM %1 LIMIT %2").arg(quoted, limit);
            } else if (!cd_is_read_query(sql)) {
                db.close();
                db = QSqlDatabase();
                QSqlDatabase::removeDatabase(name);
                return cd_fail(ErrorCode::BadArgs,
                               cd_tr("Only read queries (SELECT, WITH, SHOW, DESCRIBE, EXPLAIN) run from Data Sources"),
                               QStringLiteral("query"));
            }
            QSqlQuery q(db);
            q.setForwardOnly(true);
            if (!q.exec(sql)) {
                const QString text = redact_secrets(q.lastError().text(), secrets);
                db.close();
                q = QSqlQuery();
                db = QSqlDatabase();
                QSqlDatabase::removeDatabase(name);
                const ErrorCode code = cd_sql_error_code(text);
                return cd_fail(code == ErrorCode::Unknown ? ErrorCode::BadArgs : code,
                               cd_tr("The query failed: %1").arg(text.left(400)), QStringLiteral("query"));
            }
            const QSqlRecord rec = q.record();
            DataTable t;
            t.name = req.entity;
            for (int c = 0; c < rec.count(); ++c) {
                t.columns << rec.fieldName(c);
                t.types << cd_meta_type(rec.field(c).metaType().id());
            }
            while (q.next()) {
                if (t.rows.size() >= req.limit) {
                    out.data.truncated = true;
                    break;
                }
                if ((t.rows.size() & 1023) == 0 && cd_cancelled(cancel))
                    break;
                QJsonArray row;
                for (int c = 0; c < rec.count(); ++c)
                    row.append(cd_variant_cell(q.value(c)));
                t.rows.append(row);
            }
            infer_column_types(t);
            out.ok = true;
            out.data.tables << t;
            out.summary = cd_tr("%1 row(s)").arg(t.row_count());
        }
        db.close();
    }
    QSqlDatabase::removeDatabase(name);
    if (cd_cancelled(cancel))
        return cd_fail(SourceError::cancel());
    out.data.source = type_support(req.type_id).type_id;
    return out;
}

// ═════════════════════════════════════════════════════════════════════════
// Catalogue connectors (market-data vendors with a Python connector)
// ═════════════════════════════════════════════════════════════════════════

/// Run one catalogue command through ConnectorService and wait (worker thread) for its answer.
SourceReply cd_wait_connector(const QString& connector, const QString& command, const QStringList& args, bool force,
                              int timeout_ms, const CancelToken& cancel) {
    namespace conn = fincept::services::connectors;
    auto promise = std::make_shared<std::promise<conn::ConnectorResult>>();
    std::future<conn::ConnectorResult> future = promise->get_future();
    conn::ConnectorService::RunOptions opts;
    opts.force = force;
    opts.timeout_ms = std::max(30000, timeout_ms);
    const QString rid =
        conn::ConnectorService::instance().run_with(connector, command, args, opts, [promise](const conn::ConnectorResult& r) {
            try {
                promise->set_value(r);
            } catch (...) {
            }
        });
    QElapsedTimer clock;
    clock.start();
    while (future.wait_for(std::chrono::milliseconds(100)) != std::future_status::ready) {
        if (cd_cancelled(cancel)) {
            conn::ConnectorService::instance().cancel(rid);
            return cd_fail(SourceError::cancel());
        }
    }
    const conn::ConnectorResult res = future.get();
    SourceReply r;
    r.latency_ms = res.duration_ms > 0 ? res.duration_ms : clock.elapsed();
    if (!res.success) {
        r.error = res.failure;
        if (r.error.code == ErrorCode::None)
            r.error = SourceError::make(ErrorCode::Unknown, res.error);
        return r;
    }
    r.ok = true;
    r.data = res.tabular;
    const QJsonArray omitted =
        res.tabular.meta.value(QStringLiteral("extra")).toObject().value(QStringLiteral("omitted")).toArray();
    if (!omitted.isEmpty()) {
        QStringList names;
        for (const QJsonValue& v : omitted)
            names << v.toString();
        r.notes << cd_tr("Fields over 2 KB were left out of the details: %1").arg(names.join(QStringLiteral(", ")));
    }
    return r;
}

SourceReply cd_run_catalogue(const TypeSupport& sup, const SourceRequest& req, const CancelToken& cancel) {
    namespace conn = fincept::services::connectors;
    auto& catalog = conn::ConnectorCatalog::instance();
    if (!catalog.ensure_loaded())
        return cd_fail(ErrorCode::NotSupported, cd_tr("The connector catalogue is unavailable: %1").arg(catalog.load_error()));
    const conn::ConnectorInfo* info = catalog.find(sup.profile);
    if (!info)
        return cd_fail(ErrorCode::NotSupported, cd_tr("The %1 connector is missing from the catalogue").arg(sup.profile));

    if (req.op == SourceOp::List) {
        DataTable t;
        t.columns = {QStringLiteral("name"), QStringLiteral("kind"), QStringLiteral("detail")};
        t.types = {ColumnType::String, ColumnType::String, ColumnType::String};
        for (const auto& c : info->commands)
            t.rows.append(QJsonArray{c.name, QStringLiteral("command"), c.args_hint});
        return cd_ok(t, cd_tr("%1 command(s)").arg(info->commands.size()));
    }

    SourceReply note_holder;
    // The Python connector reads its key from the env slot; say so when this connection's key is not that one.
    for (const auto& m : conn::connection_key_mappings(req.type_id)) {
        const QString mine = req.config.value(QLatin1String(m.field)).toString().trimmed();
        if (mine.isEmpty())
            continue;
        const SecretRead stored = SecureStorage::instance().read(QLatin1String(m.env_key));
        if (stored.failed()) {
            // Never write over a key that exists but cannot be read right now.
            note_holder.notes << cd_tr("%1 in Settings > Credentials could not be read (%2), so the connector "
                                       "may run without a key.")
                                     .arg(QLatin1String(m.env_key), stored.reason());
            continue;
        }
        const QString slot = stored.found() ? stored.value.trimmed() : QString();
        if (slot.isEmpty())
            SecureStorage::instance().store(QLatin1String(m.env_key), mine); // same rule as the save bridge
        else if (slot != mine)
            note_holder.notes << cd_tr("Ran with %1 from Settings > Credentials, which differs from the key in this "
                                       "connection — update it there to use this one.")
                                     .arg(QLatin1String(m.env_key));
        catalog.set_key_present(QLatin1String(m.env_key), true);
    }

    QString command = req.op == SourceOp::Test || req.entity.isEmpty() ? info->default_command : req.entity;
    QStringList args;
    if (req.op == SourceOp::Test || (req.entity.isEmpty() && req.query.trimmed().isEmpty()))
        args = info->default_args;
    else
        args = QProcess::splitCommand(req.query.trimmed());

    SourceReply r = cd_wait_connector(info->name, command, args, req.op == SourceOp::Test, req.timeout_ms, cancel);
    r.notes = note_holder.notes + r.notes;
    if (r.ok)
        r.summary = cd_tr("%1 %2 · %3 row(s)").arg(info->title, command).arg(r.data.total_rows());
    return r;
}

// Parquet / Feather / Arrow IPC files — and Excel where the native reader is absent or the file is
// a legacy .xls — are read by the keyless catalogue connector data_local_file (local_file_data.py:
// pyarrow / openpyxl / xlrd, read-only, local paths only). TEST = schema, LIST = sheets,
// FETCH = read <path> <limit> [sheet]; the argv is built from the catalogue's params.
SourceReply cd_run_local_file(const SourceRequest& req, const CancelToken& cancel) {
    namespace conn = fincept::services::connectors;
    const QString path = cd_str(req.config, "filepath");
    if (path.isEmpty())
        return cd_fail(ErrorCode::BadArgs, cd_tr("Choose the file"), QStringLiteral("filepath"));
    auto& catalog = conn::ConnectorCatalog::instance();
    const conn::ConnectorInfo* info = catalog.ensure_loaded() ? catalog.find(QLatin1String(kCdLocalFileConnector)) : nullptr;
    if (!info)
        return cd_fail(ErrorCode::NotSupported,
                       cd_tr("The local file connector (local_file_data.py) is not in this build's catalogue."));
    const QString command = req.op == SourceOp::Test ? QStringLiteral("schema")
                            : req.op == SourceOp::List ? QStringLiteral("sheets")
                                                       : QStringLiteral("read");
    const QString sheet = req.entity.isEmpty() ? cd_str(req.config, "sheet") : req.entity;
    const int limit = std::clamp(req.limit, 1, 100000);
    QStringList args;
    const conn::ConnectorCommand* cmd = info->command(command);
    if (cmd && cmd->params_known) {
        QVector<conn::ParamInput> inputs;
        for (const conn::ConnectorParam& p : cmd->params) {
            if (p.name == QLatin1String("path"))
                inputs << conn::ParamInput::of(path);
            else if (p.name == QLatin1String("limit"))
                inputs << conn::ParamInput::of(QString::number(limit));
            else if (p.name == QLatin1String("sheet"))
                inputs << conn::ParamInput::of(sheet);
            else
                inputs << conn::ParamInput();
        }
        const conn::ArgvResult built = conn::build_argv(command, cmd->params, inputs);
        if (!built.ok)
            return cd_fail(ErrorCode::BadArgs, built.error, QStringLiteral("filepath"));
        args = built.args();
    } else {
        args << path;
        if (command == QLatin1String("read"))
            args << QString::number(limit);
        if (command != QLatin1String("sheets") && !sheet.isEmpty())
            args << sheet;
    }
    SourceReply r = cd_wait_connector(info->name, command, args, true, req.timeout_ms, cancel);
    if (!r.ok)
        return r;
    const QString file = QFileInfo(path).fileName();
    r.data.source = file;
    if (req.op == SourceOp::Test)
        r.summary = cd_tr("%1 · %2 column(s)").arg(file, QString::number(r.data.total_rows()));
    else if (req.op == SourceOp::List)
        r.summary = cd_tr("%1 sheet(s)").arg(r.data.total_rows());
    return r;
}

// ═════════════════════════════════════════════════════════════════════════
// Redis (RESP) and Memcached (text protocol)
// ═════════════════════════════════════════════════════════════════════════

class CdResp {
  public:
    CdResp(QTcpSocket& s, int timeout_ms, const CancelToken& cancel) : s_(s), timeout_(timeout_ms), cancel_(cancel) {}

    /// One command; the reply as JSON (errors as {"error": text}). `ok` false on transport failure.
    QJsonValue call(const QList<QByteArray>& argv, bool* ok) {
        QByteArray out = "*" + QByteArray::number(argv.size()) + "\r\n";
        for (const QByteArray& a : argv)
            out += "$" + QByteArray::number(a.size()) + "\r\n" + a + "\r\n";
        s_.write(out);
        if (!s_.waitForBytesWritten(timeout_)) {
            *ok = false;
            return QJsonValue();
        }
        return read_value(ok);
    }

  private:
    bool fill(int need) {
        while (buf_.size() < need) {
            if (cd_cancelled(cancel_) || !s_.waitForReadyRead(timeout_))
                return false;
            buf_ += s_.readAll();
        }
        return true;
    }
    bool read_line(QByteArray* line) {
        for (;;) {
            const int nl = int(buf_.indexOf("\r\n"));
            if (nl >= 0) {
                *line = buf_.left(nl);
                buf_.remove(0, nl + 2);
                return true;
            }
            if (cd_cancelled(cancel_) || !s_.waitForReadyRead(timeout_))
                return false;
            buf_ += s_.readAll();
        }
    }
    QJsonValue read_value(bool* ok) {
        QByteArray line;
        if (!read_line(&line) || line.isEmpty()) {
            *ok = false;
            return QJsonValue();
        }
        const char t = line.at(0);
        const QByteArray rest = line.mid(1);
        switch (t) {
            case '+':
                return QString::fromUtf8(rest);
            case '-':
                return QJsonObject{{"error", QString::fromUtf8(rest)}};
            case ':':
                return rest.toDouble();
            case '$': {
                const int n = rest.toInt();
                if (n < 0)
                    return QJsonValue(QJsonValue::Null);
                if (!fill(n + 2)) {
                    *ok = false;
                    return QJsonValue();
                }
                const QByteArray v = buf_.left(n);
                buf_.remove(0, n + 2);
                return QString::fromUtf8(v);
            }
            case '*': {
                const int n = rest.toInt();
                if (n < 0)
                    return QJsonValue(QJsonValue::Null);
                QJsonArray a;
                for (int i = 0; i < n; ++i) {
                    a.append(read_value(ok));
                    if (!*ok)
                        return QJsonValue();
                }
                return a;
            }
            default:
                *ok = false;
                return QJsonValue();
        }
    }
    QTcpSocket& s_;
    int timeout_;
    CancelToken cancel_;
    QByteArray buf_;
};

SourceReply cd_run_redis(const SourceRequest& req, const CancelToken& cancel) {
    const QString host = cd_str(req.config, "host");
    if (host.isEmpty())
        return cd_fail(ErrorCode::BadArgs, cd_tr("Enter the Redis host"), QStringLiteral("host"));
    const int port = cd_int(req.config, "port", 6379);
    const int timeout = std::min(req.timeout_ms, 15000);
    QTcpSocket sock;
    QElapsedTimer clock;
    clock.start();
    sock.connectToHost(host, quint16(port));
    if (!sock.waitForConnected(timeout))
        return cd_fail(ErrorCode::UpstreamDown, cd_tr("Could not connect to Redis at %1:%2 — %3").arg(host).arg(port).arg(sock.errorString()));
    CdResp resp(sock, timeout, cancel);
    bool ok = true;
    auto err_of = [](const QJsonValue& v) { return v.isObject() ? v.toObject().value("error").toString() : QString(); };
    const QString pass = req.config.value(QStringLiteral("password")).toString();
    if (!pass.isEmpty()) {
        const QJsonValue a = resp.call({"AUTH", pass.toUtf8()}, &ok);
        if (!ok)
            return cd_fail(ErrorCode::UpstreamDown, cd_tr("Redis closed the connection during AUTH"));
        if (!err_of(a).isEmpty())
            return cd_fail(ErrorCode::AuthFailed, cd_tr("Redis rejected the password: %1").arg(redact_secrets(err_of(a), {pass})));
    }
    const int dbi = cd_int(req.config, "database", 0);
    if (dbi > 0) {
        const QJsonValue s = resp.call({"SELECT", QByteArray::number(dbi)}, &ok);
        if (ok && !err_of(s).isEmpty())
            return cd_fail(ErrorCode::BadArgs, cd_tr("SELECT %1 failed: %2").arg(dbi).arg(err_of(s)), QStringLiteral("database"));
    }
    const qint64 latency = clock.elapsed();
    if (req.op == SourceOp::Test) {
        const QJsonValue info = resp.call({"INFO", "server"}, &ok);
        if (!ok)
            return cd_fail(ErrorCode::UpstreamDown, cd_tr("No answer to INFO"));
        if (err_of(info).contains(QLatin1String("NOAUTH")))
            return cd_fail(ErrorCode::AuthFailed, cd_tr("Redis requires a password (NOAUTH)"), QStringLiteral("password"));
        if (!err_of(info).isEmpty())
            return cd_fail(ErrorCode::Unknown, err_of(info));
        DataTable t;
        t.columns = {QStringLiteral("key"), QStringLiteral("value")};
        QString version;
        for (const QString& line : info.toString().split(QLatin1Char('\n'))) {
            const QString l = line.trimmed();
            if (l.isEmpty() || l.startsWith(QLatin1Char('#')))
                continue;
            const QString k = l.section(QLatin1Char(':'), 0, 0);
            const QString v = l.section(QLatin1Char(':'), 1);
            if (k == QLatin1String("redis_version"))
                version = v;
            t.rows.append(QJsonArray{k, typed_text_cell(v)});
        }
        infer_column_types(t);
        SourceReply r = cd_ok(t, cd_tr("Redis %1 · connected in %2 ms").arg(version).arg(latency));
        r.latency_ms = latency;
        return r;
    }
    QString pattern = req.query.trimmed().isEmpty() ? QStringLiteral("*") : req.query.trimmed();
    if (req.op == SourceOp::List || req.entity.isEmpty()) {
        QJsonArray rows;
        QByteArray cursor = "0";
        do {
            const QJsonValue v = resp.call({"SCAN", cursor, "MATCH", pattern.toUtf8(), "COUNT", "500"}, &ok);
            if (!ok)
                break;
            if (!err_of(v).isEmpty())
                return cd_fail(err_of(v).contains(QLatin1String("NOAUTH")) ? ErrorCode::AuthFailed : ErrorCode::Unknown, err_of(v));
            cursor = v.toArray().at(0).toString().toUtf8();
            for (const QJsonValue& k : v.toArray().at(1).toArray())
                if (rows.size() < req.limit)
                    rows.append(QJsonObject{{"name", k.toString()}});
        } while (ok && cursor != "0" && rows.size() < req.limit && !cd_cancelled(cancel));
        for (int i = 0; i < rows.size() && ok; ++i) {
            QJsonObject o = rows[i].toObject();
            o.insert(QStringLiteral("kind"), resp.call({"TYPE", o.value("name").toString().toUtf8()}, &ok).toString());
            rows[i] = o;
        }
        if (rows.isEmpty()) {
            DataTable t;
            t.columns = {QStringLiteral("name"), QStringLiteral("kind")};
            return cd_ok(t, cd_tr("no keys match %1").arg(pattern));
        }
        return cd_ok(table_from_records(rows, {"name", "kind"}, QJsonObject()), cd_tr("%1 key(s)").arg(rows.size()));
    }
    const QByteArray key = req.entity.toUtf8();
    const QString type = resp.call({"TYPE", key}, &ok).toString();
    if (!ok)
        return cd_fail(ErrorCode::UpstreamDown, cd_tr("Redis closed the connection"));
    const QByteArray n = QByteArray::number(std::max(1, req.limit) - 1);
    QJsonArray rows;
    if (type == QLatin1String("string")) {
        const QJsonValue v = resp.call({"GET", key}, &ok);
        const QJsonDocument doc = QJsonDocument::fromJson(v.toString().toUtf8());
        if (doc.isArray())
            return cd_ok(table_from_json_array(doc.array()));
        if (doc.isObject())
            return cd_ok(table_from_key_values(doc.object()));
        rows.append(QJsonObject{{"key", req.entity}, {"value", typed_text_cell(v.toString())}});
    } else if (type == QLatin1String("hash")) {
        const QJsonArray a = resp.call({"HGETALL", key}, &ok).toArray();
        for (int i = 0; i + 1 < a.size(); i += 2)
            rows.append(QJsonObject{{"field", a.at(i)}, {"value", typed_text_cell(a.at(i + 1).toString())}});
    } else if (type == QLatin1String("list")) {
        const QJsonArray a = resp.call({"LRANGE", key, "0", n}, &ok).toArray();
        for (int i = 0; i < a.size(); ++i) {
            const QJsonDocument doc = QJsonDocument::fromJson(a.at(i).toString().toUtf8());
            rows.append(doc.isObject() ? doc.object() : QJsonObject{{"index", i}, {"value", typed_text_cell(a.at(i).toString())}});
        }
    } else if (type == QLatin1String("set")) {
        for (const QJsonValue& v : resp.call({"SMEMBERS", key}, &ok).toArray())
            if (rows.size() < req.limit)
                rows.append(QJsonObject{{"member", v}});
    } else if (type == QLatin1String("zset")) {
        const QJsonArray a = resp.call({"ZRANGE", key, "0", n, "WITHSCORES"}, &ok).toArray();
        for (int i = 0; i + 1 < a.size(); i += 2)
            rows.append(QJsonObject{{"member", a.at(i)}, {"score", typed_text_cell(a.at(i + 1).toString())}});
    } else if (type == QLatin1String("stream")) {
        for (const QJsonValue& e : resp.call({"XRANGE", key, "-", "+", "COUNT", QByteArray::number(req.limit)}, &ok).toArray()) {
            QJsonObject row{{"id", e.toArray().at(0)}};
            const QJsonArray f = e.toArray().at(1).toArray();
            for (int i = 0; i + 1 < f.size(); i += 2)
                row.insert(f.at(i).toString(), typed_text_cell(f.at(i + 1).toString()));
            rows.append(row);
        }
    } else if (type == QLatin1String("none")) {
        return cd_fail(ErrorCode::NoData, cd_tr("No key named %1").arg(req.entity), QStringLiteral("entity"));
    } else {
        return cd_fail(ErrorCode::NotSupported, cd_tr("Keys of type %1 are not read here").arg(type));
    }
    if (rows.isEmpty())
        return cd_fail(ErrorCode::NoData, cd_tr("%1 is empty").arg(req.entity));
    SourceReply r = cd_ok(table_from_json_array(rows), cd_tr("%1 · %2 row(s)").arg(type).arg(rows.size()));
    r.latency_ms = latency;
    return r;
}

SourceReply cd_run_memcached(const SourceRequest& req, const CancelToken& cancel) {
    const QString first = cd_str(req.config, "servers").split(QLatin1Char(','), Qt::SkipEmptyParts).value(0).trimmed();
    if (first.isEmpty())
        return cd_fail(ErrorCode::BadArgs, cd_tr("Enter a server (host:port)"), QStringLiteral("servers"));
    const QString host = first.section(QLatin1Char(':'), 0, 0);
    const int port = first.contains(QLatin1Char(':')) ? first.section(QLatin1Char(':'), 1).toInt() : 11211;
    QTcpSocket sock;
    QElapsedTimer clock;
    clock.start();
    sock.connectToHost(host, quint16(port));
    if (!sock.waitForConnected(10000))
        return cd_fail(ErrorCode::UpstreamDown, cd_tr("Could not connect to %1:%2 — %3").arg(host).arg(port).arg(sock.errorString()));
    const qint64 latency = clock.elapsed();
    sock.write("stats\r\n");
    QByteArray buf;
    while (!buf.contains("END\r\n") && !buf.startsWith("ERROR")) {
        if (cd_cancelled(cancel) || !sock.waitForReadyRead(10000))
            return cd_fail(ErrorCode::UpstreamDown, cd_tr("The server did not answer \"stats\""));
        buf += sock.readAll();
    }
    sock.write("quit\r\n");
    sock.waitForBytesWritten(1000);
    DataTable t;
    t.columns = {QStringLiteral("stat"), QStringLiteral("value")};
    QString version;
    for (const QByteArray& line : buf.split('\n')) {
        const QList<QByteArray> parts = line.trimmed().split(' ');
        if (parts.size() >= 3 && parts[0] == "STAT") {
            if (parts[1] == "version")
                version = QString::fromLatin1(parts[2]);
            t.rows.append(QJsonArray{QString::fromLatin1(parts[1]), typed_text_cell(QString::fromLatin1(parts[2]))});
        }
    }
    infer_column_types(t);
    SourceReply r = cd_ok(t, cd_tr("memcached %1 · connected in %2 ms").arg(version).arg(latency));
    r.latency_ms = latency;
    return r;
}

} // namespace

QString sql_driver_for(const QString& flavour, QString* why, QString* fix) {
    const QString native = cd_native_driver(flavour);
    if (!native.isEmpty() && cd_driver_loads(native)) {
        if (why)
            why->clear();
        return native;
    }
    // ODBC: the Qt driver must load AND a matching ODBC driver must be installed. The installed
    // driver names are not enumerable through Qt, so try a connection-less handshake per name.
    const QStringList odbc = cd_odbc_drivers(flavour);
    if (!odbc.isEmpty() && cd_driver_loads(QStringLiteral("QODBC"))) {
        for (const QString& d : odbc) {
            const QString name = QStringLiteral("ds_odbc_probe_") + QUuid::createUuid().toString(QUuid::Id128);
            bool found = false;
            {
                QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QODBC"), name);
                db.setDatabaseName(QStringLiteral("DRIVER={%1};SERVER=127.0.0.1;").arg(d));
                db.setConnectOptions(QStringLiteral("SQL_ATTR_LOGIN_TIMEOUT=1"));
                db.open();
                const QString e = db.lastError().text().toLower();
                // "Data source name not found and no default driver specified" = driver not installed.
                found = !(e.contains(QLatin1String("data source name not found")) || e.contains(QLatin1String("can't open lib")) ||
                          e.contains(QLatin1String("file not found")));
                db.close();
            }
            QSqlDatabase::removeDatabase(name);
            if (found) {
                if (why)
                    *why = d; // the ODBC driver name, used by the connection string
                return QStringLiteral("QODBC");
            }
        }
    }
    if (flavour == QLatin1String("sqlite")) {
        if (why)
            *why = cd_tr("The Qt SQLite driver did not load");
        return QString();
    }
    if (why)
        *why = cd_tr("No database driver for this engine loads in this build on this machine.");
    if (fix)
        *fix = cd_install_step(flavour);
    return QString();
}

TypeSupport effective_support(const QString& type_id) {
    TypeSupport t = type_support(type_id);
    if (!t.supported)
        return t;
    switch (t.driver) {
        case DriverKind::Sql:
            if (t.profile != QLatin1String("sqlite")) {
                // Cheap check only (plugin present); the full probe runs on the worker with TEST.
                const QString native = cd_native_driver(t.profile);
                const bool maybe = (!native.isEmpty() && QSqlDatabase::isDriverAvailable(native)) ||
                                   (!cd_odbc_drivers(t.profile).isEmpty() && QSqlDatabase::isDriverAvailable(QStringLiteral("QODBC")));
                if (!maybe) {
                    t.supported = false;
                    t.reason = cd_tr("No database driver for this engine is part of this build on this platform.");
                    t.fix = cd_install_step(t.profile);
                }
            }
            break;
        case DriverKind::WebSocket:
#ifndef HAS_QT_WEBSOCKETS
            t.supported = false;
            t.reason = cd_tr("This build was made without Qt WebSockets.");
            t.fix = cd_tr("Use a Server-Sent Events or REST endpoint of the same feed.");
#endif
            break;
        case DriverKind::File: {
            // Parquet / Feather / Arrow (and Excel without the native reader) are read by the
            // keyless catalogue connector data_local_file.
#ifdef FINCEPT_HAS_QXLSX
            const bool via_connector = t.profile == QLatin1String("connector");
#else
            const bool via_connector = t.profile == QLatin1String("connector") || t.profile == QLatin1String("excel");
#endif
            if (via_connector) {
                auto& catalog = fincept::services::connectors::ConnectorCatalog::instance();
                if (!catalog.ensure_loaded() || !catalog.find(QLatin1String(kCdLocalFileConnector))) {
                    t.supported = false;
                    t.reason = cd_tr("The local file connector (local_file_data.py) is not in this build's catalogue.");
                    t.fix = cd_tr("Save the file as CSV and add it as a CSV file source.");
                }
            }
            break;
        }
        case DriverKind::Catalogue: {
            auto& catalog = fincept::services::connectors::ConnectorCatalog::instance();
            if (catalog.ensure_loaded() && !catalog.find(t.profile)) {
                t.supported = false;
                t.reason = cd_tr("The %1 connector is not in this build's catalogue.").arg(t.profile);
                t.fix = cd_tr("Use the REST API type with the vendor's HTTP endpoint.");
            }
            break;
        }
        default:
            break;
    }
    return t;
}

SourceReply execute_source_request(const SourceRequest& req, const CancelToken& cancel) {
    const TypeSupport sup = effective_support(req.type_id);
    if (!sup.supported) {
        SourceError e = SourceError::make(ErrorCode::NotSupported, sup.reason);
        e.fix = sup.fix;
        return cd_fail(e);
    }
    SourceReply r;
    switch (sup.driver) {
        case DriverKind::Http:
        case DriverKind::EventStream:
            r = cd_run_http(sup, req, cancel);
            break;
        case DriverKind::WebSocket:
            r = cd_run_websocket(req, cancel);
            break;
        case DriverKind::File:
            r = cd_run_file(sup, req, cancel);
            break;
        case DriverKind::Sql:
            r = cd_run_sql(sup, req, cancel);
            break;
        case DriverKind::Catalogue:
            r = cd_run_catalogue(sup, req, cancel);
            break;
        case DriverKind::Redis:
            r = cd_run_redis(req, cancel);
            break;
        case DriverKind::Memcached:
            r = cd_run_memcached(req, cancel);
            break;
        case DriverKind::None:
            r = cd_fail(ErrorCode::NotSupported, sup.reason);
            break;
    }
    if (r.ok && req.op == SourceOp::Fetch && r.data.is_empty())
        return cd_fail(ErrorCode::NoData, cd_tr("The source answered but returned no rows for this request"));
    if (!r.ok)
        r.error.message = redact_secrets(r.error.message, cd_secrets_of(req.config));
    if (r.data.source.isEmpty())
        r.data.source = req.type_id;
    if (!r.data.as_of.isValid())
        r.data.as_of = QDateTime::currentDateTimeUtc();
    return r;
}

} // namespace fincept::services::datasources
