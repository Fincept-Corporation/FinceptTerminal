#include "services/data_normalization/DataNormalizationService.h"

#include "core/logging/Logger.h"
#include "network/http/HttpClient.h"
#include "storage/sqlite/Database.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QPointer>
#include <QRegularExpression>
#include <QTimeZone>
#include <QUrl>
#include <QUuid>
#include <QVector>

#include <cmath>

namespace fincept::services {

static const QString TAG = "DataNormalization";

// ── Schema definitions ─────────────────────────────────────────────────────
// Required fields per schema — used for validation.
static const QHash<QString, QStringList> kRequiredFields = {
    {"OHLCV", {"symbol", "timestamp", "open", "high", "low", "close", "volume"}},
    {"QUOTE", {"symbol", "timestamp", "price"}},
    {"TICK", {"symbol", "timestamp", "price", "quantity"}},
    {"ORDER", {"orderId", "symbol", "side", "type", "quantity", "status"}},
    {"POSITION", {"symbol", "quantity", "averagePrice"}},
    {"PORTFOLIO", {"totalValue", "timestamp"}},
    {"INSTRUMENT", {"symbol", "name", "exchange"}},
};

// ── JSONPath-subset engine + transforms ────────────────────────────────────
// Free functions with a file-unique `dns_` prefix (unity builds concatenate
// sibling translation units) so the behaviour can be exercised without the
// service. QtCore only — keep it that way.
// BEGIN_DNS_ENGINE
namespace {

struct DnsPathToken {
    enum class Kind { Key, Index, Wildcard, Keys };
    Kind kind = Kind::Key;
    QString key;
    int index = 0;
};

// Tokenise a JSONPath-style expression. Returns false for syntax this engine
// does not implement (recursive descent "..", filters "[?()]", slices "[a:b]")
// so callers can say "unsupported" instead of silently selecting a wrong node.
bool dns_tokenize_path(const QString& path, QVector<DnsPathToken>& out) {
    const QString p = path.trimmed();
    const qsizetype n = p.size();
    qsizetype i = 0;
    if (i < n && p[i] == QLatin1Char('$'))
        ++i;

    auto push_key = [&out](const QString& k) {
        DnsPathToken t;
        if (k == QLatin1String("*")) {
            t.kind = DnsPathToken::Kind::Wildcard;
        } else {
            t.kind = DnsPathToken::Kind::Key;
            t.key = k;
        }
        out.push_back(t);
    };

    while (i < n) {
        const QChar c = p[i];
        if (c == QLatin1Char('.')) {
            ++i;
            if (i >= n || p[i] == QLatin1Char('.'))
                return false; // trailing dot, or ".." recursive descent
            if (p[i] == QLatin1Char('[') || p[i] == QLatin1Char('~'))
                continue; // ".['key']" / ".~" — handled by the next iteration
            qsizetype j = i;
            while (j < n && p[j] != QLatin1Char('.') && p[j] != QLatin1Char('[') && p[j] != QLatin1Char('~'))
                ++j;
            if (j == i)
                return false;
            push_key(p.mid(i, j - i).trimmed());
            i = j;
        } else if (c == QLatin1Char('[')) {
            ++i;
            if (i >= n)
                return false;
            if (p[i] == QLatin1Char('\'') || p[i] == QLatin1Char('"')) {
                const QChar quote = p[i];
                ++i;
                const qsizetype j = p.indexOf(quote, i);
                if (j < 0 || j + 1 >= n || p[j + 1] != QLatin1Char(']'))
                    return false;
                DnsPathToken t;
                t.kind = DnsPathToken::Kind::Key;
                t.key = p.mid(i, j - i);
                out.push_back(t);
                i = j + 2;
            } else {
                const qsizetype j = p.indexOf(QLatin1Char(']'), i);
                if (j < 0)
                    return false;
                const QString inner = p.mid(i, j - i).trimmed();
                i = j + 1;
                DnsPathToken t;
                if (inner == QLatin1String("*")) {
                    t.kind = DnsPathToken::Kind::Wildcard;
                } else {
                    bool ok = false;
                    const int idx = inner.toInt(&ok);
                    if (!ok)
                        return false;
                    t.kind = DnsPathToken::Kind::Index;
                    t.index = idx;
                }
                out.push_back(t);
            }
        } else if (c == QLatin1Char('~')) {
            DnsPathToken t;
            t.kind = DnsPathToken::Kind::Keys;
            out.push_back(t);
            ++i;
        } else {
            // Bare key with no "$." prefix ("price", "a.b[0]").
            qsizetype j = i;
            while (j < n && p[j] != QLatin1Char('.') && p[j] != QLatin1Char('[') && p[j] != QLatin1Char('~'))
                ++j;
            if (j == i)
                return false;
            push_key(p.mid(i, j - i).trimmed());
            i = j;
        }
    }
    return true;
}

QJsonArray dns_property_names(const QJsonValue& cur) {
    QJsonArray names;
    if (cur.isObject()) {
        const QStringList keys = cur.toObject().keys();
        for (const QString& k : keys)
            names.append(k);
    } else if (cur.isArray()) {
        const qsizetype count = cur.toArray().size();
        for (qsizetype i = 0; i < count; ++i)
            names.append(static_cast<int>(i));
    }
    return names;
}

QJsonValue dns_eval_path(const QJsonValue& cur, const QVector<DnsPathToken>& toks, qsizetype pos) {
    if (pos >= toks.size())
        return cur;
    const DnsPathToken& t = toks[pos];
    switch (t.kind) {
        case DnsPathToken::Kind::Key: {
            if (cur.isObject()) {
                const QJsonObject o = cur.toObject();
                if (!o.contains(t.key))
                    return QJsonValue(QJsonValue::Undefined);
                return dns_eval_path(o.value(t.key), toks, pos + 1);
            }
            if (cur.isArray()) { // lenient: "$.data.0" indexes an array
                bool ok = false;
                const int idx = t.key.toInt(&ok);
                const QJsonArray a = cur.toArray();
                if (ok && idx >= 0 && idx < a.size())
                    return dns_eval_path(a.at(idx), toks, pos + 1);
            }
            return QJsonValue(QJsonValue::Undefined);
        }
        case DnsPathToken::Kind::Index: {
            if (!cur.isArray())
                return QJsonValue(QJsonValue::Undefined);
            const QJsonArray a = cur.toArray();
            const qsizetype idx = t.index < 0 ? a.size() + t.index : t.index;
            if (idx < 0 || idx >= a.size())
                return QJsonValue(QJsonValue::Undefined);
            return dns_eval_path(a.at(idx), toks, pos + 1);
        }
        case DnsPathToken::Kind::Wildcard: {
            // ".*~" — the property names of the node itself.
            if (pos + 1 < toks.size() && toks[pos + 1].kind == DnsPathToken::Kind::Keys) {
                if (!cur.isObject() && !cur.isArray())
                    return QJsonValue(QJsonValue::Undefined);
                return QJsonValue(dns_property_names(cur));
            }
            QJsonArray projected;
            if (cur.isArray()) {
                const QJsonArray a = cur.toArray();
                for (const QJsonValue& v : a) {
                    const QJsonValue r = dns_eval_path(v, toks, pos + 1);
                    if (!r.isUndefined())
                        projected.append(r);
                }
            } else if (cur.isObject()) {
                const QJsonObject o = cur.toObject();
                for (auto it = o.begin(); it != o.end(); ++it) {
                    const QJsonValue r = dns_eval_path(it.value(), toks, pos + 1);
                    if (!r.isUndefined())
                        projected.append(r);
                }
            } else {
                return QJsonValue(QJsonValue::Undefined);
            }
            return QJsonValue(projected);
        }
        case DnsPathToken::Kind::Keys:
            if (!cur.isObject() && !cur.isArray())
                return QJsonValue(QJsonValue::Undefined);
            return QJsonValue(dns_property_names(cur));
    }
    return QJsonValue(QJsonValue::Undefined);
}

QString dns_json_to_text(const QJsonValue& v) {
    switch (v.type()) {
        case QJsonValue::Null:
        case QJsonValue::Undefined:
            return QString();
        case QJsonValue::Bool:
            return v.toBool() ? QStringLiteral("true") : QStringLiteral("false");
        case QJsonValue::Double:
            return QString::number(v.toDouble(), 'g', 15);
        case QJsonValue::String:
            return v.toString();
        case QJsonValue::Array:
            return QString::fromUtf8(QJsonDocument(v.toArray()).toJson(QJsonDocument::Compact));
        case QJsonValue::Object:
            return QString::fromUtf8(QJsonDocument(v.toObject()).toJson(QJsonDocument::Compact));
    }
    return QString();
}

double dns_json_to_number(const QJsonValue& v, bool* ok) {
    if (v.isDouble()) {
        *ok = true;
        return v.toDouble();
    }
    if (v.isBool()) {
        *ok = true;
        return v.toBool() ? 1.0 : 0.0;
    }
    return v.toString().trimmed().toDouble(ok);
}

bool dns_is_known_transform(const QString& t) {
    static const QStringList kKnown = {QStringLiteral("to_number"),      QStringLiteral("to_string"),
                                       QStringLiteral("unix_ms_to_iso"), QStringLiteral("unix_s_to_iso"),
                                       QStringLiteral("unix_to_iso"),    QStringLiteral("upper"),
                                       QStringLiteral("lower"),          QStringLiteral("abs_value")};
    return kKnown.contains(t);
}

QJsonValue dns_apply_transform(const QJsonValue& val, const QString& transform) {
    if (val.isArray()) { // projection results: transform every element
        QJsonArray out;
        const QJsonArray in = val.toArray();
        for (const QJsonValue& e : in)
            out.append(e.isNull() ? e : dns_apply_transform(e, transform));
        return QJsonValue(out);
    }

    if (transform == QLatin1String("to_number")) {
        bool ok = false;
        const double d = dns_json_to_number(val, &ok);
        return ok ? QJsonValue(d) : QJsonValue(0.0);
    }
    if (transform == QLatin1String("to_string"))
        return QJsonValue(dns_json_to_text(val));

    if (transform == QLatin1String("unix_ms_to_iso") || transform == QLatin1String("unix_s_to_iso") ||
        transform == QLatin1String("unix_to_iso")) {
        bool ok = false;
        const double raw = dns_json_to_number(val, &ok);
        if (!ok || !std::isfinite(raw) || std::abs(raw) > 9.0e15)
            return val; // not a timestamp — leave it for validation to flag
        bool millis = transform == QLatin1String("unix_ms_to_iso");
        // "unix_to_iso" accepts either unit: 1e11 seconds is year 5138, 1e11 ms is 1973.
        if (transform == QLatin1String("unix_to_iso"))
            millis = std::abs(raw) >= 1e11;
        const qint64 whole = static_cast<qint64>(raw);
        const QDateTime dt = millis ? QDateTime::fromMSecsSinceEpoch(whole, QTimeZone::UTC)
                                    : QDateTime::fromSecsSinceEpoch(whole, QTimeZone::UTC);
        if (!dt.isValid())
            return val;
        return QJsonValue(dt.toString(Qt::ISODate));
    }

    if (transform == QLatin1String("upper"))
        return QJsonValue(dns_json_to_text(val).toUpper());
    if (transform == QLatin1String("lower"))
        return QJsonValue(dns_json_to_text(val).toLower());
    if (transform == QLatin1String("abs_value")) {
        bool ok = false;
        const double d = dns_json_to_number(val, &ok);
        return ok ? QJsonValue(std::abs(d)) : val;
    }

    return val; // unknown transform — pass through unchanged
}

} // namespace
// END_DNS_ENGINE

// ── Singleton ─────────────────────────────────────────────────────────────

DataNormalizationService& DataNormalizationService::instance() {
    static DataNormalizationService s;
    return s;
}

DataNormalizationService::DataNormalizationService() = default;

// ── Public API ─────────────────────────────────────────────────────────────

void DataNormalizationService::fetch_and_normalize(const DataMapping& mapping, NormalizeCallback cb,
                                                   bool force_refresh) {
    // The mapping's CACHE settings were saved but never read. Serve the newest
    // stored record while it is younger than the TTL instead of re-spending an
    // API quota (free tiers are as small as 25 calls/day).
    if (!force_refresh && mapping.cache_enabled && mapping.cache_ttl > 0) {
        if (const auto cached = latest_for_mapping(mapping.id)) {
            // extracted_at is SQLite's datetime('now'): "yyyy-MM-dd HH:mm:ss", UTC.
            const QString kSqliteTime = QStringLiteral("yyyy-MM-dd HH:mm:ss");
            QDateTime stored = QDateTime::fromString(cached->extracted_at, kSqliteTime);
            stored.setTimeZone(QTimeZone::UTC);
            const qint64 age_s = stored.isValid() ? stored.secsTo(QDateTime::currentDateTimeUtc()) : -1;
            // A mapping edited (re-saved under the same id) after the record was stored
            // would otherwise keep serving data extracted with its OLD field mappings.
            QDateTime edited = QDateTime::fromString(mapping.updated_at, kSqliteTime);
            edited.setTimeZone(QTimeZone::UTC);
            const bool edited_since = edited.isValid() && stored.isValid() && stored <= edited;
            // Only a clean record is worth replaying; one that failed validation should be
            // re-fetched (the upstream data, or the user's fix, may differ now).
            if (age_s >= 0 && age_s < mapping.cache_ttl && !edited_since && cached->errors.isEmpty()) {
                NormalizedRecord rec = *cached;
                rec.from_cache = true;
                LOG_INFO(TAG, QString("Mapping '%1' served from cache (%2 s old, TTL %3 s)")
                                  .arg(mapping.name)
                                  .arg(age_s)
                                  .arg(mapping.cache_ttl));
                cb(rec.errors.isEmpty(), rec);
                return;
            }
        }
    }

    const QString url = build_url(mapping);
    // Log the URL WITHOUT its query string: the shipped mapping templates put
    // `&apikey=`/`&token=` there, and this is LOG_INFO — the default level — so
    // the raw URL would write a live third-party credential into fincept.log.
    LOG_INFO(TAG, QString("Fetching mapping '%1' from %2")
                      .arg(mapping.name, QUrl(url).adjusted(QUrl::RemoveQuery).toString()));

    // HttpClient resolves anything that is not http(s) against the Fincept API
    // base URL — and attaches the user's Fincept session headers to it. A
    // mapping with a bare host ("api.example.com") must never reach it.
    if (!is_http_url(url)) {
        NormalizedRecord failed;
        failed.mapping_id = mapping.id;
        failed.schema_name = mapping.schema_name;
        failed.errors << tr("Base URL must start with http:// or https:// (got \"%1\")")
                             .arg(QUrl(url).adjusted(QUrl::RemoveQuery).toString());
        LOG_WARN(TAG, QString("Mapping '%1' rejected: URL is not absolute http(s)").arg(mapping.name));
        emit normalization_failed(mapping.id, failed.errors.join(QStringLiteral("; ")));
        cb(false, failed);
        return;
    }

    QPointer<DataNormalizationService> self = this;

    auto on_reply = [self, mapping, cb](Result<QJsonDocument> result) {
        if (!self)
            return;

        if (result.is_err()) {
            const QString err = QString::fromStdString(result.error());
            LOG_ERROR(TAG, QString("Fetch failed for mapping '%1': %2").arg(mapping.name, err));
            emit self->normalization_failed(mapping.id, err);
            NormalizedRecord failed;
            failed.mapping_id = mapping.id;
            failed.schema_name = mapping.schema_name;
            failed.errors << err; // the screen shows these — never hand it an empty reason
            cb(false, failed);
            return;
        }

        NormalizedRecord record = self->normalize_raw(mapping, result.value());
        persist(mapping, record);

        emit self->normalization_complete(record);
        cb(record.errors.isEmpty(), record);
    };

    // Auth + the HEADERS box ride on this request only. (They used to be applied
    // by mutating the shared HttpClient singleton, which leaked the third-party
    // token into Fincept API calls — see the git history of apply_auth().)
    const auto headers = build_request_headers(mapping.headers, mapping.auth_type, mapping.auth_token);

    QJsonObject body;
    if (!mapping.body.trimmed().isEmpty())
        body = QJsonDocument::fromJson(mapping.body.toUtf8()).object();

    auto& http = HttpClient::instance();
    const QString method = mapping.method.trimmed().toUpper();
    if (method == QLatin1String("POST"))
        http.post(url, body, on_reply, this, headers);
    else if (method == QLatin1String("PUT") || method == QLatin1String("PATCH")) // PATCH rides PUT, as in TEST API
        http.put(url, body, on_reply, this, headers);
    else if (method == QLatin1String("DELETE"))
        http.del(url, body, on_reply, this, headers);
    else
        http.get(url, on_reply, this, headers);
}

// ── Request helpers ────────────────────────────────────────────────────────

QMap<QByteArray, QByteArray> DataNormalizationService::build_request_headers(const QString& headers_text,
                                                                              const QString& auth_type,
                                                                              const QString& auth_token) {
    QMap<QByteArray, QByteArray> headers;

    const QString token = auth_token.trimmed();
    if (!token.isEmpty()) {
        if (auth_type == QLatin1String("Bearer Token") || auth_type == QLatin1String("OAuth2"))
            headers.insert("Authorization", "Bearer " + token.toUtf8());
        else if (auth_type == QLatin1String("API Key"))
            headers.insert("X-API-Key", token.toUtf8());
        else if (auth_type == QLatin1String("Basic Auth")) // AUTH VALUE is "user:password"
            headers.insert("Authorization", "Basic " + token.toUtf8().toBase64());
    }

    // RFC 7230 token characters — anything else is not a header name.
    static const QRegularExpression kHeaderName(QStringLiteral("^[A-Za-z0-9!#$%&'*+.^_`|~-]+$"));
    const QStringList lines = headers_text.split(QLatin1Char('\n'));
    for (const QString& raw_line : lines) {
        const QString line = raw_line.trimmed();
        const qsizetype colon = line.indexOf(QLatin1Char(':'));
        if (colon <= 0)
            continue;
        const QString name = line.left(colon).trimmed();
        const QString value = line.mid(colon + 1).trimmed();
        if (!kHeaderName.match(name).hasMatch())
            continue;
        if (value.contains(QLatin1Char('\r')) || value.contains(QChar(0))) // header injection
            continue;
        headers.insert(name.toLatin1(), value.toUtf8()); // explicit lines win over the AUTH VALUE
    }
    return headers;
}

QString DataNormalizationService::join_url(const QString& base, const QString& endpoint) {
    const QString b = base.trimmed();
    const QString e = endpoint.trimmed();
    if (is_http_url(e))
        return e;
    if (b.isEmpty())
        return e;
    if (e.isEmpty())
        return b;
    const bool base_slash = b.endsWith(QLatin1Char('/'));
    const bool end_slash = e.startsWith(QLatin1Char('/'));
    if (base_slash && end_slash)
        return b + e.mid(1);
    if (!base_slash && !end_slash)
        return b + QLatin1Char('/') + e;
    return b + e;
}

bool DataNormalizationService::is_http_url(const QString& url) {
    const QUrl u(url.trimmed());
    const QString scheme = u.scheme().toLower();
    return u.isValid() && !u.host().isEmpty() && (scheme == QLatin1String("http") || scheme == QLatin1String("https"));
}

bool DataNormalizationService::expression_supported(const QString& expression) {
    const QString p = expression.trimmed();
    if (p.isEmpty() || p == QLatin1String("$"))
        return true;
    QVector<DnsPathToken> toks;
    return dns_tokenize_path(p, toks);
}

bool DataNormalizationService::transform_supported(const QString& transform) {
    const QString t = transform.trimmed();
    return t.isEmpty() || dns_is_known_transform(t);
}

NormalizedRecord DataNormalizationService::normalize_raw(const DataMapping& mapping, const QJsonDocument& raw) {
    NormalizedRecord record;
    record.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    record.mapping_id = mapping.id;
    record.source_id = mapping.source_id;
    record.schema_name = mapping.schema_name;
    // An array response used to be stored as an empty object (raw.object() of an
    // array is {}), which made the audit copy useless for candle/position endpoints.
    record.raw = raw.isArray() ? QJsonObject{{QStringLiteral("data"), raw.array()}} : raw.object();
    record.extracted_at = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

    // Parse field_mappings_json
    const QJsonArray field_mappings = QJsonDocument::fromJson(mapping.field_mappings_json.toUtf8()).array();

    const QJsonValue root = raw.isArray() ? QJsonValue(raw.array()) : QJsonValue(raw.object());

    QJsonObject normalized;
    for (const QJsonValue& fv : field_mappings) {
        const QJsonObject fm = fv.toObject();
        const QString target = fm["target"].toString();
        const QString expression = fm["expression"].toString();
        const QString transform = fm["transform"].toString();
        // "default_val" is the persisted key; accept "default" too (the TEST preview
        // used to emit that spelling).
        const QString default_v = fm.contains("default_val") ? fm["default_val"].toString() : fm["default"].toString();

        if (target.isEmpty())
            continue;

        QJsonValue extracted;
        if (!expression.isEmpty()) {
            extracted = extract_jsonpath(root, expression);
        }

        // Fall back to default if extraction yielded null/undefined
        if (extracted.isNull() || extracted.isUndefined()) {
            if (!default_v.isEmpty()) {
                extracted = QJsonValue(default_v);
            } else {
                extracted = QJsonValue::Null;
            }
        }

        // Apply transform
        if (!transform.isEmpty() && !extracted.isNull()) {
            extracted = apply_transform(extracted, transform);
        }

        normalized[target] = extracted;
    }

    record.normalized = normalized;
    record.errors = validate_schema(normalized, mapping.schema_name);

    if (!record.errors.isEmpty()) {
        LOG_WARN(TAG,
                 QString("Mapping '%1' produced %2 validation error(s)").arg(mapping.name).arg(record.errors.size()));
    }

    return record;
}

std::optional<NormalizedRecord> DataNormalizationService::latest_for_mapping(const QString& mapping_id) {
    auto r = Database::instance().execute("SELECT id, mapping_id, source_id, schema_name, normalized_json, raw_json, "
                                          "       validation_errors, extracted_at "
                                          "FROM normalized_data WHERE mapping_id = ? "
                                          "ORDER BY extracted_at DESC LIMIT 1",
                                          {mapping_id});

    if (r.is_err() || !r.value().next())
        return std::nullopt;

    auto& q = r.value();
    NormalizedRecord rec;
    rec.id = q.value(0).toString();
    rec.mapping_id = q.value(1).toString();
    rec.source_id = q.value(2).toString();
    rec.schema_name = q.value(3).toString();
    rec.normalized = QJsonDocument::fromJson(q.value(4).toString().toUtf8()).object();
    rec.raw = QJsonDocument::fromJson(q.value(5).toString().toUtf8()).object();
    const QJsonArray errs = QJsonDocument::fromJson(q.value(6).toString().toUtf8()).array();
    for (const auto& e : errs)
        rec.errors << e.toString();
    rec.extracted_at = q.value(7).toString();
    return rec;
}

QVector<NormalizedRecord> DataNormalizationService::records_for_schema(const QString& schema_name) {
    auto r = Database::instance().execute("SELECT id, mapping_id, source_id, schema_name, normalized_json, raw_json, "
                                          "       validation_errors, extracted_at "
                                          "FROM normalized_data WHERE schema_name = ? "
                                          "ORDER BY extracted_at DESC LIMIT 500",
                                          {schema_name});

    QVector<NormalizedRecord> results;
    if (r.is_err())
        return results;

    auto& q = r.value();
    while (q.next()) {
        NormalizedRecord rec;
        rec.id = q.value(0).toString();
        rec.mapping_id = q.value(1).toString();
        rec.source_id = q.value(2).toString();
        rec.schema_name = q.value(3).toString();
        rec.normalized = QJsonDocument::fromJson(q.value(4).toString().toUtf8()).object();
        rec.raw = QJsonDocument::fromJson(q.value(5).toString().toUtf8()).object();
        const QJsonArray errs = QJsonDocument::fromJson(q.value(6).toString().toUtf8()).array();
        for (const auto& e : errs)
            rec.errors << e.toString();
        rec.extracted_at = q.value(7).toString();
        results.append(rec);
    }
    return results;
}

// ── JSONPath extraction ────────────────────────────────────────────────────
// Engine lives in the dns_ helpers above. Supports $.key  $.a.b.c  $[0]  [-1]
// ['quoted key']  [*] / .* projection (array of the remaining path per element)
// and a trailing ~ (property names). Unsupported syntax yields Undefined.

QJsonValue DataNormalizationService::extract_jsonpath(const QJsonValue& root, const QString& path) {
    const QString p = path.trimmed();
    if (p.isEmpty() || p == QLatin1String("$"))
        return root;

    QVector<DnsPathToken> toks;
    if (!dns_tokenize_path(p, toks)) {
        LOG_WARN(TAG, QString("Unsupported path expression: %1").arg(p));
        return QJsonValue(QJsonValue::Undefined);
    }
    return dns_eval_path(root, toks, 0);
}

// ── Transform functions ────────────────────────────────────────────────────

QJsonValue DataNormalizationService::apply_transform(const QJsonValue& val, const QString& transform) {
    return dns_apply_transform(val, transform.trimmed());
}

// ── Schema validation ──────────────────────────────────────────────────────

QStringList DataNormalizationService::validate_schema(const QJsonObject& data, const QString& schema_name) {
    QStringList errors;
    const auto it = kRequiredFields.find(schema_name);
    if (it == kRequiredFields.end())
        return errors; // unknown schema, skip

    for (const QString& field : it.value()) {
        if (!data.contains(field) || data[field].isNull() || data[field].isUndefined()) {
            errors << QString("Missing required field: %1").arg(field);
        }
    }
    return errors;
}

// ── Persistence ────────────────────────────────────────────────────────────

void DataNormalizationService::persist(const DataMapping& mapping, const NormalizedRecord& record) {
    const QString norm_json = QJsonDocument(record.normalized).toJson(QJsonDocument::Compact);
    const QString raw_json = QJsonDocument(record.raw).toJson(QJsonDocument::Compact);

    QJsonArray err_array;
    for (const QString& e : record.errors)
        err_array.append(e);
    const QString err_json = QJsonDocument(err_array).toJson(QJsonDocument::Compact);

    const QString hash = QCryptographicHash::hash(norm_json.toUtf8(), QCryptographicHash::Sha256).toHex();

    auto r = Database::instance().execute(
        "INSERT INTO normalized_data "
        "(id, mapping_id, source_id, schema_name, normalized_json, raw_json, "
        " validation_errors, data_hash) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?)",
        {record.id, mapping.id, mapping.source_id, mapping.schema_name, norm_json, raw_json, err_json, hash});

    if (r.is_err()) {
        LOG_ERROR(TAG, QString("Failed to persist normalized record: %1").arg(QString::fromStdString(r.error())));
        return;
    }

    // Every RUN used to append a row holding the full raw API response and nothing
    // ever removed one. Keep the newest few per mapping (the cache + history only
    // ever read the latest; records_for_schema() caps its read at 500).
    constexpr int kDnsKeepPerMapping = 50;
    auto pruned = Database::instance().execute(
        "DELETE FROM normalized_data WHERE mapping_id = ? AND id NOT IN ("
        "  SELECT id FROM normalized_data WHERE mapping_id = ? ORDER BY extracted_at DESC, rowid DESC LIMIT ?)",
        {mapping.id, mapping.id, kDnsKeepPerMapping});
    if (pruned.is_err()) {
        LOG_WARN(TAG, QString("Could not prune old normalized records: %1").arg(QString::fromStdString(pruned.error())));
    }
}

// ── HTTP helpers ───────────────────────────────────────────────────────────

QString DataNormalizationService::build_url(const DataMapping& mapping) {
    return join_url(mapping.base_url, mapping.endpoint);
}

} // namespace fincept::services
