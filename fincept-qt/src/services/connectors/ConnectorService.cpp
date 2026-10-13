// ConnectorService.cpp — catalogue connector runner + cache + DataHub producer.
// See ConnectorService.h for the contract.

#include "services/connectors/ConnectorService.h"

#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "datahub/TopicPolicy.h"
#include "python/PythonRunner.h"
#include "services/connectors/ConnectorCatalog.h"
#include "storage/secure/SecureStorage.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QPointer>
#include <QRegularExpression>
#include <QSet>
#include <QtConcurrent/QtConcurrent>

#include <algorithm>

namespace fincept::services::connectors {

namespace {

constexpr const char* kConnSvcTag = "ConnectorService";
constexpr qint64 kConnSvcCacheBudgetBytes = 128LL * 1024 * 1024;
constexpr int kConnSvcCacheMaxEntries = 256;
constexpr int kConnSvcMaxArgs = 32;
constexpr int kConnSvcMaxArgChars = 4096;
constexpr int kConnSvcMaxColumns = 64;
constexpr int kConnSvcColumnScanRows = 500;
constexpr qint64 kConnSvcPrettyLimitBytes = 8LL * 1024 * 1024;
constexpr int kConnSvcErrorChars = 1500;

qint64 conn_svc_now_ms() {
    return QDateTime::currentMSecsSinceEpoch();
}

/// Mask credentials that a provider (or the HTTP library, which echoes the request URL in
/// its exception text) copied into a message: query-string keys and the literal values of
/// the connector's own stored keys.
QString conn_svc_redact(QString text, const QStringList& secrets) {
    for (const QString& s : secrets)
        text.replace(s, QStringLiteral("***"));
    static const QRegularExpression kQueryKey(
        QStringLiteral("\\b(api[_-]?key|apikey|access[_-]?key|access[_-]?token|token|app[_-]?id|appid|key|"
                       "subscription-key|password|secret|registrationkey|userid)=([^&\\s\"']+)"),
        QRegularExpression::CaseInsensitiveOption);
    text.replace(kQueryKey, QStringLiteral("\\1=***"));
    return text;
}

QString conn_svc_human_bytes(qint64 n) {
    if (n >= 1024 * 1024)
        return QString::number(double(n) / (1024.0 * 1024.0), 'f', 1) + QStringLiteral(" MB");
    if (n >= 1024)
        return QString::number(double(n) / 1024.0, 'f', 1) + QStringLiteral(" KB");
    return QString::number(n) + QStringLiteral(" B");
}

/// Cap a display string at ConnectorService::kDisplayCapBytes (approximated in characters),
/// cutting at a line boundary and saying so explicitly — never a silent truncation.
QString conn_svc_cap(const QString& text, qint64 total_bytes) {
    const qint64 cap = ConnectorService::kDisplayCapBytes;
    if (text.size() <= cap)
        return text;
    qsizetype cut = text.lastIndexOf(QLatin1Char('\n'), cap);
    if (cut < cap / 2)
        cut = cap;
    return text.left(cut) + QStringLiteral("\n\n[… TRUNCATED — showing the first %1 of %2. Use EXPORT CSV for the "
                                           "full table, or narrow the request with arguments.]")
                                .arg(conn_svc_human_bytes(cut), conn_svc_human_bytes(total_bytes));
}

QString conn_svc_trim_error(QString text) {
    text = text.trimmed();
    if (text.size() > kConnSvcErrorChars)
        text = QStringLiteral("…") + text.right(kConnSvcErrorChars); // a traceback's useful part is its tail
    return text;
}

// ── Table projection ─────────────────────────────────────────────────────────

bool conn_svc_mostly_objects(const QJsonArray& a) {
    if (a.isEmpty())
        return false;
    const int n = std::min<int>(int(a.size()), 50);
    int objs = 0;
    for (int i = 0; i < n; ++i)
        if (a.at(i).isObject())
            ++objs;
    return objs * 2 > n;
}

bool conn_svc_mostly_arrays(const QJsonArray& a) {
    if (a.isEmpty())
        return false;
    const int n = std::min<int>(int(a.size()), 50);
    int arrs = 0;
    for (int i = 0; i < n; ++i)
        if (a.at(i).isArray())
            ++arrs;
    return arrs * 2 > n;
}

/// QJsonObject iterates keys alphabetically (Qt does not keep the source order), so put the
/// columns a reader scans for first and leave the rest alphabetical.
QStringList conn_svc_order_columns(QStringList cols) {
    static const QStringList kFirst = {
        "key",    "date",   "datetime",  "time",      "timestamp", "period", "year",  "month",   "quarter",
        "symbol", "ticker", "code",      "id",        "name",      "title",  "label", "country", "country_code",
        "region", "series", "series_id", "indicator", "value",     "price",  "close",
    };
    QStringList head;
    for (const QString& f : kFirst) {
        const qsizetype i = cols.indexOf(f);
        if (i >= 0) {
            head << cols.takeAt(i);
        } else {
            // case-insensitive match (DATE, Date, ...)
            for (qsizetype j = 0; j < cols.size(); ++j) {
                if (cols[j].compare(f, Qt::CaseInsensitive) == 0) {
                    head << cols.takeAt(j);
                    break;
                }
            }
        }
    }
    return head + cols;
}

QStringList conn_svc_record_columns(const QJsonArray& rows) {
    QStringList cols;
    QSet<QString> seen;
    bool has_scalar = false;
    const int n = std::min<int>(int(rows.size()), kConnSvcColumnScanRows);
    for (int i = 0; i < n && cols.size() < kConnSvcMaxColumns; ++i) {
        const QJsonValue v = rows.at(i);
        if (!v.isObject()) {
            has_scalar = true;
            continue;
        }
        const QJsonObject o = v.toObject();
        for (auto it = o.constBegin(); it != o.constEnd() && cols.size() < kConnSvcMaxColumns; ++it) {
            if (!seen.contains(it.key())) {
                seen.insert(it.key());
                cols << it.key();
            }
        }
    }
    cols = conn_svc_order_columns(cols);
    if (has_scalar && !seen.contains(QStringLiteral("value")))
        cols << QStringLiteral("value");
    return cols;
}

struct ConnSvcCandidate {
    QString path;
    QJsonValue value;
    double score = -1;
};

void conn_svc_consider(const QString& path, const QJsonValue& v, int depth, ConnSvcCandidate& best) {
    double score = -1;
    if (v.isArray()) {
        const QJsonArray a = v.toArray();
        if (a.isEmpty())
            return;
        if (conn_svc_mostly_objects(a))
            score = 3e6 + a.size();
        else if (conn_svc_mostly_arrays(a))
            score = 1.5e6 + a.size();
        else if (a.size() >= 2)
            score = 1e6 + a.size();
    } else if (v.isObject()) {
        const QJsonObject o = v.toObject();
        if (o.size() >= 2) {
            int objs = 0;
            for (auto it = o.constBegin(); it != o.constEnd(); ++it)
                if (it->isObject())
                    ++objs;
            if (objs * 5 >= o.size() * 4) // >= 80 % of the values are records -> a dict of records
                score = 2e6 + o.size();
        }
    }
    if (score < 0)
        return;
    score -= depth; // prefer the shallower of two equal candidates
    if (score > best.score) {
        best.path = path;
        best.value = v;
        best.score = score;
    }
}

void conn_svc_walk(const QJsonObject& o, const QString& prefix, int depth, ConnSvcCandidate& best) {
    for (auto it = o.constBegin(); it != o.constEnd(); ++it) {
        const QString path = prefix.isEmpty() ? it.key() : prefix + QLatin1Char('.') + it.key();
        conn_svc_consider(path, it.value(), depth, best);
        if (it->isObject() && depth < 3)
            conn_svc_walk(it->toObject(), path, depth + 1, best);
    }
}

QJsonArray conn_svc_key_value_rows(const QJsonObject& o) {
    QJsonArray rows;
    for (auto it = o.constBegin(); it != o.constEnd(); ++it) {
        if (it->isObject() && it->toObject().size() <= 20) {
            const QJsonObject inner = it->toObject();
            for (auto jt = inner.constBegin(); jt != inner.constEnd(); ++jt)
                rows.append(QJsonObject{{"key", it.key() + QLatin1Char('.') + jt.key()}, {"value", jt.value()}});
        } else {
            rows.append(QJsonObject{{"key", it.key()}, {"value", it.value()}});
        }
    }
    return rows;
}

void conn_svc_project(const QJsonValue& root, ConnectorResult& r) {
    if (root.isArray()) {
        const QJsonArray a = root.toArray();
        // World Bank v2 shape: [ {page metadata}, [ records... ] ]
        if (a.size() == 2 && a.at(0).isObject() && a.at(1).isArray() && conn_svc_mostly_objects(a.at(1).toArray())) {
            r.table = a.at(1).toArray();
            r.table_path = QStringLiteral("[1]");
            r.table_kind = QStringLiteral("records");
            r.columns = conn_svc_record_columns(r.table);
            return;
        }
        r.table = a;
        r.table_path = QStringLiteral("(root)");
        if (conn_svc_mostly_objects(a)) {
            r.table_kind = QStringLiteral("records");
            r.columns = conn_svc_record_columns(a);
        } else if (conn_svc_mostly_arrays(a)) {
            r.table_kind = QStringLiteral("rows");
            int width = 0;
            for (int i = 0; i < std::min<int>(int(a.size()), kConnSvcColumnScanRows); ++i)
                width = std::max<int>(width, int(a.at(i).toArray().size()));
            for (int c = 0; c < std::min(width, kConnSvcMaxColumns); ++c)
                r.columns << QString::number(c);
        } else {
            r.table_kind = QStringLiteral("values");
            r.columns = {QStringLiteral("value")};
        }
        return;
    }
    if (!root.isObject()) {
        r.table = QJsonArray{root};
        r.table_path = QStringLiteral("(root)");
        r.table_kind = QStringLiteral("scalar");
        r.columns = {QStringLiteral("value")};
        return;
    }

    const QJsonObject o = root.toObject();
    ConnSvcCandidate best;
    conn_svc_consider(QStringLiteral("(root)"), o, 0, best); // the root itself may be a dict of records
    conn_svc_walk(o, QString(), 1, best);
    if (best.score >= 0) {
        r.table_path = best.path;
        if (best.value.isArray()) {
            const QJsonArray a = best.value.toArray();
            r.table = a;
            if (conn_svc_mostly_objects(a)) {
                r.table_kind = QStringLiteral("records");
                r.columns = conn_svc_record_columns(a);
            } else if (conn_svc_mostly_arrays(a)) {
                r.table_kind = QStringLiteral("rows");
                int width = 0;
                for (int i = 0; i < std::min<int>(int(a.size()), kConnSvcColumnScanRows); ++i)
                    width = std::max<int>(width, int(a.at(i).toArray().size()));
                for (int c = 0; c < std::min(width, kConnSvcMaxColumns); ++c)
                    r.columns << QString::number(c);
            } else {
                r.table_kind = QStringLiteral("values");
                r.columns = {QStringLiteral("value")};
            }
        } else {
            // dict of records: {"AAPL": {...}, "MSFT": {...}} -> one row per key
            const QJsonObject dict = best.value.toObject();
            QJsonArray rows;
            for (auto it = dict.constBegin(); it != dict.constEnd(); ++it) {
                QJsonObject row;
                if (it->isObject())
                    row = it->toObject();
                else
                    row.insert(QStringLiteral("value"), it.value());
                row.insert(QStringLiteral("key"), it.key());
                rows.append(row);
            }
            r.table = rows;
            r.table_kind = QStringLiteral("dict");
            r.columns = conn_svc_record_columns(rows);
        }
        return;
    }

    // No collection anywhere: show the most informative object as key/value pairs. A
    // `{"success":..., "data": {...}}` envelope is unwrapped first.
    QJsonObject target = o;
    QString path = QStringLiteral("(root)");
    for (const char* k : {"data", "result", "results", "rates", "values"}) {
        const QJsonValue v = o.value(QLatin1String(k));
        if (v.isObject() && v.toObject().size() >= 2) {
            target = v.toObject();
            path = QLatin1String(k);
            break;
        }
    }
    r.table = conn_svc_key_value_rows(target);
    r.table_path = path;
    r.table_kind = QStringLiteral("key/value");
    r.columns = {QStringLiteral("key"), QStringLiteral("value")};
}

/// An upstream error carried inside an otherwise-successful payload, e.g.
/// {"series": ..., "data": {"error": "HTTP 403 ..."}} or Alpha Vantage's {"Information": ...}.
/// (A top-level {"error": ...} never gets here — PythonRunner reports it as a failure.)
QString conn_svc_embedded_error(const QJsonValue& v, int depth) {
    if (depth > 2)
        return {};
    if (v.isObject()) {
        const QJsonObject o = v.toObject();
        for (const char* k : {"error", "Error Message", "Information", "Note"}) {
            const QJsonValue e = o.value(QLatin1String(k));
            if (e.isString() && !e.toString().trimmed().isEmpty())
                return e.toString().trimmed().left(300);
            if (e.isObject()) {
                const QString m = e.toObject().value(QStringLiteral("message")).toString();
                if (!m.isEmpty())
                    return m.left(300);
            }
        }
        for (auto it = o.constBegin(); it != o.constEnd(); ++it) {
            if (it->isObject()) {
                const QString w = conn_svc_embedded_error(it.value(), depth + 1);
                if (!w.isEmpty())
                    return w;
            }
        }
    } else if (v.isArray() && depth == 0) {
        const QJsonArray a = v.toArray();
        // World Bank API error envelope: [{"message": [{"id": "120", "key": "...", "value": "..."}]}]
        if (!a.isEmpty() && a.first().isObject()) {
            const QJsonValue msg = a.first().toObject().value(QStringLiteral("message"));
            if (msg.isArray() && !msg.toArray().isEmpty()) {
                const QJsonObject m0 = msg.toArray().first().toObject();
                const QString t = (m0.value(QStringLiteral("key")).toString() + QStringLiteral(": ") +
                                   m0.value(QStringLiteral("value")).toString())
                                      .trimmed();
                if (t.size() > 2)
                    return t.left(300);
            }
        }
    }
    return {};
}

/// Parse + normalise one script result. Runs on a QtConcurrent worker thread.
ConnectorResult conn_svc_build_result(const QString& name, const QString& command, const QStringList& args,
                                      const python::PythonResult& pr, const QStringList& secrets, qint64 duration_ms) {
    ConnectorResult r;
    r.connector = name;
    r.command = command;
    r.args = args;
    r.duration_ms = duration_ms;
    r.fetched_at_ms = conn_svc_now_ms();

    if (!pr.success) {
        QString err = pr.error.trimmed();
        if (err.isEmpty())
            err = pr.exit_code == 0 ? QStringLiteral("The connector returned no data")
                                    : QStringLiteral("The connector exited with code %1").arg(pr.exit_code);
        r.error = conn_svc_trim_error(conn_svc_redact(err, secrets));
        return r;
    }

    QString text = pr.output;
    for (const QString& s : secrets)
        text.replace(s, QStringLiteral("***"));
    const QByteArray utf8 = text.toUtf8();
    r.payload_bytes = utf8.size();

    QJsonParseError pe{};
    const QJsonDocument doc = QJsonDocument::fromJson(utf8, &pe);
    if (pe.error != QJsonParseError::NoError || doc.isNull()) {
        r.error = QStringLiteral("The connector printed malformed JSON (%1)").arg(pe.errorString());
        return r;
    }
    r.data = doc.isArray() ? QJsonValue(doc.array()) : QJsonValue(doc.object());
    conn_svc_project(r.data, r);
    r.warning = conn_svc_redact(conn_svc_embedded_error(r.data, 0), secrets);

    r.raw_text = conn_svc_cap(text, r.payload_bytes);
    if (r.payload_bytes <= kConnSvcPrettyLimitBytes) {
        r.pretty_text = conn_svc_cap(QString::fromUtf8(doc.toJson(QJsonDocument::Indented)), r.payload_bytes);
    } else {
        r.pretty_text =
            QStringLiteral("[Payload is %1 — too large to pretty-print. The RAW tab shows the first %2; "
                           "EXPORT CSV writes the full table.]")
                .arg(conn_svc_human_bytes(r.payload_bytes), conn_svc_human_bytes(ConnectorService::kDisplayCapBytes));
    }
    r.success = true;
    return r;
}

QJsonArray conn_svc_args_json(const QStringList& args) {
    QJsonArray a;
    for (const QString& s : args)
        a.append(s);
    return a;
}

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// Construction
// ─────────────────────────────────────────────────────────────────────────────

ConnectorService& ConnectorService::instance() {
    static ConnectorService s;
    return s;
}

ConnectorService::ConnectorService(QObject* parent) : QObject(parent) {
    qRegisterMetaType<fincept::services::connectors::ConnectorResult>("fincept::services::connectors::ConnectorResult");
}

// ─────────────────────────────────────────────────────────────────────────────
// Keys
// ─────────────────────────────────────────────────────────────────────────────

QString ConnectorService::request_key(const QString& name, const QString& command, const QStringList& args) {
    return name + QChar(0x1e) + command + QChar(0x1e) + args.join(QChar(0x1f));
}

QString ConnectorService::topic_for(const QString& name, const QString& command, const QStringList& args) {
    QString hash = QStringLiteral("_");
    if (!args.isEmpty()) {
        const QByteArray digest =
            QCryptographicHash::hash(args.join(QChar(0x1f)).toUtf8(), QCryptographicHash::Sha1).toHex();
        hash = QString::fromLatin1(digest.left(16));
    }
    return QStringLiteral("connector:") + name + QLatin1Char(':') + command + QLatin1Char(':') + hash;
}

QStringList ConnectorService::secret_values_for(const QString& name) const {
    QStringList out;
    const ConnectorInfo* info = ConnectorCatalog::instance().find(name);
    if (!info)
        return out;
    for (const QString& k : info->env_keys) {
        const auto r = SecureStorage::instance().retrieve(k);
        if (r.is_ok() && r.value().trimmed().size() >= 6)
            out << r.value().trimmed();
        const QString env = qEnvironmentVariable(k.toUtf8().constData()).trimmed();
        if (env.size() >= 6)
            out << env;
    }
    return out;
}

// ─────────────────────────────────────────────────────────────────────────────
// Run
// ─────────────────────────────────────────────────────────────────────────────

QString ConnectorService::run(const QString& name, const QString& command, const QStringList& args, bool force,
                              Callback cb) {
    const QString rid = QStringLiteral("conn-%1").arg(next_id_.fetch_add(1));
    // Always queued — even on our own thread — so a cache hit can never be delivered before the
    // caller has stored the request id it is about to filter result_ready by.
    QPointer<ConnectorService> self(this);
    QMetaObject::invokeMethod(
        this,
        [self, rid, name, command, args, force, cb = std::move(cb)]() {
            if (self)
                self->start_on_service_thread(rid, name, command, args, force, cb);
        },
        Qt::QueuedConnection);
    return rid;
}

void ConnectorService::deliver(const ConnectorResult& result, const Callback& cb) {
    if (result.success)
        emit result_ready(result.request_id, result);
    else
        emit error_occurred(result.request_id, result.error);
    if (cb)
        cb(result);
}

void ConnectorService::start_on_service_thread(const QString& request_id, const QString& name, const QString& command,
                                               const QStringList& args, bool force, Callback cb) {
    ConnectorResult base;
    base.request_id = request_id;
    base.connector = name;
    base.command = command;
    base.args = args;
    base.fetched_at_ms = conn_svc_now_ms();
    auto fail = [&](const QString& message) {
        base.success = false;
        base.error = message;
        deliver(base, cb);
    };

    auto& catalog = ConnectorCatalog::instance();
    if (!catalog.ensure_loaded())
        return fail(QStringLiteral("Connector catalogue unavailable: %1").arg(catalog.load_error()));
    const ConnectorInfo* info = catalog.find(name);
    if (!info)
        return fail(QStringLiteral("Unknown data connector '%1'").arg(name));
    if (!info->has_command(command))
        return fail(QStringLiteral("Unknown command '%1' for %2. Valid commands: %3")
                        .arg(command, name, info->command_names().join(QStringLiteral(", "))));
    if (args.size() > kConnSvcMaxArgs)
        return fail(QStringLiteral("Too many arguments (%1; at most %2)").arg(args.size()).arg(kConnSvcMaxArgs));
    for (const QString& a : args) {
        if (a.size() > kConnSvcMaxArgChars || a.contains(QChar(0)))
            return fail(QStringLiteral("Argument rejected: longer than %1 characters or contains a NUL byte")
                            .arg(kConnSvcMaxArgChars));
    }
    if (info->key_required) {
        bool any = false;
        for (const QString& k : info->env_keys)
            any = any || catalog.key_present(k);
        if (!any)
            return fail(QStringLiteral("%1 needs an API key (%2). Add it in Data Sources › Catalog or Settings › "
                                       "Credentials, then run again.")
                            .arg(info->title, info->env_keys.join(QStringLiteral(" / "))));
    }

    const QString key = request_key(name, command, args);
    const qint64 now = conn_svc_now_ms();
    if (!force) {
        auto it = cache_.find(key);
        if (it != cache_.end()) {
            if (it->expires_ms > now) {
                it->last_used_ms = now;
                ConnectorResult hit = it->result;
                hit.request_id = request_id;
                hit.from_cache = true;
                LOG_DEBUG(kConnSvcTag, QString("Cache hit %1 %2").arg(name, command));
                deliver(hit, cb);
                return;
            }
            cache_bytes_ -= it->bytes;
            cache_.erase(it);
        }
    }

    emit request_started(request_id, name, command);

    // De-dupe: an identical request already running answers this one too (fresh enough even
    // for a forced refresh — it started after the user asked).
    auto running = in_flight_.find(key);
    if (running != in_flight_.end()) {
        running->waiters.append(Waiter{request_id, std::move(cb)});
        return;
    }
    InFlight f;
    f.connector = name;
    f.command = command;
    f.args = args;
    f.started_ms = now;
    f.ttl_sec = info->ttl_sec > 0 ? info->ttl_sec : kDefaultTtlSec;
    f.waiters.append(Waiter{request_id, std::move(cb)});
    in_flight_.insert(key, f);
    topics_.insert(topic_for(name, command, args), TopicRecord{name, command, args});

    QStringList argv;
    argv << command << args;
    python::PythonRunner::RunOptions opts;
    opts.timeout_ms = kRunTimeoutMs;
    const QStringList secrets = secret_values_for(name);
    LOG_INFO(kConnSvcTag, QString("Run %1 %2 (%3 arg(s))").arg(info->script, command).arg(args.size()));

    QPointer<ConnectorService> self(this);
    python::PythonRunner::instance().run_with_options(
        info->script, argv, opts, [self, key, name, command, args, secrets, now](python::PythonResult pr) {
            if (!self)
                return;
            const qint64 duration = conn_svc_now_ms() - now;
            // Parsing and normalising a large payload is real work — keep it off the GUI thread.
            (void)QtConcurrent::run([self, key, name, command, args, secrets, duration, pr = std::move(pr)]() {
                ConnectorResult r = conn_svc_build_result(name, command, args, pr, secrets, duration);
                ConnectorService* svc = self.data();
                if (!svc)
                    return;
                QMetaObject::invokeMethod(
                    svc,
                    [self, key, r = std::move(r)]() mutable {
                        if (self)
                            self->finish(key, std::move(r));
                    },
                    Qt::QueuedConnection);
            });
        });
}

void ConnectorService::finish(const QString& key, ConnectorResult result) {
    auto it = in_flight_.find(key);
    if (it == in_flight_.end())
        return;
    const InFlight f = it.value();
    in_flight_.erase(it);

    const QString topic = topic_for(f.connector, f.command, f.args);
    if (result.success) {
        LOG_INFO(kConnSvcTag, QString("%1 %2: %3 row(s), %4, %5 ms")
                                  .arg(f.connector, f.command)
                                  .arg(result.table.size())
                                  .arg(conn_svc_human_bytes(result.payload_bytes))
                                  .arg(result.duration_ms));
        store_in_cache(key, result, f.ttl_sec);
        if (hub_registered_) {
            const QJsonObject envelope{
                {"connector", f.connector},
                {"command", f.command},
                {"args", conn_svc_args_json(f.args)},
                {"fetched_at", QDateTime::fromMSecsSinceEpoch(result.fetched_at_ms).toUTC().toString(Qt::ISODate)},
                {"duration_ms", double(result.duration_ms)},
                {"rows", int(result.table.size())},
                {"data", result.data},
            };
            fincept::datahub::DataHub::instance().publish(topic, QVariant(envelope));
        }
    } else {
        LOG_WARN(kConnSvcTag, QString("%1 %2 failed: %3").arg(f.connector, f.command, result.error.left(300)));
        if (hub_registered_)
            fincept::datahub::DataHub::instance().publish_error(topic, result.error);
    }

    for (const Waiter& w : f.waiters) {
        ConnectorResult copy = result;
        copy.request_id = w.request_id;
        deliver(copy, w.cb);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Cache
// ─────────────────────────────────────────────────────────────────────────────

void ConnectorService::store_in_cache(const QString& key, const ConnectorResult& result, int ttl_sec) {
    if (ttl_sec <= 0)
        return;
    CacheEntry e;
    e.result = result;
    e.result.from_cache = false;
    const qint64 now = conn_svc_now_ms();
    e.expires_ms = now + qint64(ttl_sec) * 1000;
    e.last_used_ms = now;
    // QJsonValue costs a few times its text size; the two display strings are UTF-16.
    e.bytes = result.payload_bytes * 3 + (result.pretty_text.size() + result.raw_text.size()) * 2;
    if (e.bytes > kConnSvcCacheBudgetBytes / 2)
        return; // one huge payload must not evict everything else
    auto old = cache_.find(key);
    if (old != cache_.end()) {
        cache_bytes_ -= old->bytes;
        cache_.erase(old);
    }
    cache_bytes_ += e.bytes;
    cache_.insert(key, std::move(e));
    evict_if_needed();
}

void ConnectorService::evict_if_needed() {
    const qint64 now = conn_svc_now_ms();
    for (auto it = cache_.begin(); it != cache_.end();) {
        if (it->expires_ms <= now) {
            cache_bytes_ -= it->bytes;
            it = cache_.erase(it);
        } else {
            ++it;
        }
    }
    while ((cache_bytes_ > kConnSvcCacheBudgetBytes || cache_.size() > kConnSvcCacheMaxEntries) && !cache_.isEmpty()) {
        auto lru = cache_.begin();
        for (auto it = cache_.begin(); it != cache_.end(); ++it)
            if (it->last_used_ms < lru->last_used_ms)
                lru = it;
        cache_bytes_ -= lru->bytes;
        cache_.erase(lru);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Credentials
// ─────────────────────────────────────────────────────────────────────────────

void ConnectorService::refresh_key_status() {
    if (key_refresh_running_.exchange(true))
        return;
    QPointer<ConnectorService> self(this);
    (void)QtConcurrent::run([self]() {
        auto& catalog = ConnectorCatalog::instance();
        if (catalog.ensure_loaded())
            catalog.refresh_key_presence(); // one SecureStorage read per env key — never on the GUI thread
        ConnectorService* svc = self.data();
        if (!svc)
            return;
        QMetaObject::invokeMethod(
            svc,
            [self]() {
                if (!self)
                    return;
                self->key_refresh_running_ = false;
                emit self->key_status_changed();
            },
            Qt::QueuedConnection);
    });
}

bool ConnectorService::store_key(const QString& env_key, const QString& value, QString* error) {
    auto& catalog = ConnectorCatalog::instance();
    const QString v = value.trimmed();
    if (v.isEmpty()) {
        if (error)
            *error = tr("The key is empty.");
        return false;
    }
    if (!catalog.ensure_loaded() || !catalog.all_env_keys().contains(env_key)) {
        if (error)
            *error = tr("%1 is not a key any catalogue connector uses.").arg(env_key);
        return false;
    }
    const auto r = SecureStorage::instance().store(env_key, v);
    if (r.is_err()) {
        if (error)
            *error = tr("Could not save the key: %1").arg(QString::fromStdString(r.error()));
        LOG_ERROR(kConnSvcTag, "Failed to store API key " + env_key);
        return false;
    }
    catalog.set_key_present(env_key, true);
    LOG_INFO(kConnSvcTag, "Stored API key " + env_key + " from the Data Sources catalog");
    emit key_status_changed();
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// DataHub producer
// ─────────────────────────────────────────────────────────────────────────────

QStringList ConnectorService::topic_patterns() const {
    return {QStringLiteral("connector:*")};
}

void ConnectorService::refresh(const QStringList& topics) {
    for (const QString& topic : topics) {
        TopicRecord rec;
        const auto it = topics_.constFind(topic);
        if (it != topics_.constEnd()) {
            rec = it.value();
        } else {
            // connector:<name>:<command>:_  — no-argument requests can be replayed without a
            // prior run; anything else needs the args, which only a previous run recorded.
            const QStringList parts = topic.split(QLatin1Char(':'));
            if (parts.size() != 4 || parts[3] != QLatin1String("_")) {
                fincept::datahub::DataHub::instance().publish_error(
                    topic, QStringLiteral("Unknown connector request — run it once (Data Sources › Catalog or the "
                                          "data_* tool) so its arguments are known"));
                continue;
            }
            rec = TopicRecord{parts[1], parts[2], {}};
        }
        run(rec.connector, rec.command, rec.args, /*force=*/true);
    }
}

int ConnectorService::max_requests_per_sec() const {
    return 2; // Python spawn pacing; PythonRunner caps concurrency at 3 anyway
}

void ConnectorService::ensure_registered_with_hub() {
    if (hub_registered_)
        return;
    auto& hub = fincept::datahub::DataHub::instance();
    hub.register_producer(this);

    auto policy_for = [](int ttl_sec) {
        fincept::datahub::TopicPolicy p;
        p.ttl_ms = ttl_sec * 1000;
        p.min_interval_ms = 5 * 1000;
        p.refresh_timeout_ms = kRunTimeoutMs + 15 * 1000; // > the PythonRunner watchdog
        p.push_only = false;
        return p;
    };
    // Pattern policies match in registration order, so per-connector TTL overrides go first.
    auto& catalog = ConnectorCatalog::instance();
    if (catalog.ensure_loaded()) {
        for (const auto& c : catalog.connectors()) {
            if (c.ttl_sec > 0)
                hub.set_policy_pattern(QStringLiteral("connector:") + c.name + QStringLiteral(":*"),
                                       policy_for(c.ttl_sec));
        }
    }
    hub.set_policy_pattern(QStringLiteral("connector:*"), policy_for(kDefaultTtlSec));
    hub_registered_ = true;
    LOG_INFO(kConnSvcTag, "Registered with DataHub (connector:*)");
}

} // namespace fincept::services::connectors
