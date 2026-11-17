#include "services/asia_markets/AsiaMarketsDecode.h"

#include "python/PythonRunner.h"
#include "ui/widgets/AutoLoadPolicy.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>

namespace {

using fincept::python::PythonResult;

/// A connector script reports failure as {"error": "msg"}, {"error": {"error": "msg", ...}} or
/// {"success": false, ...}; successful responses may carry "error": null. Returns the message, or
/// an empty string when the value does not describe an error.
QString asiadec_error_text(const QJsonValue& v) {
    if (v.isString())
        return v.toString();
    if (v.isObject()) {
        const QJsonObject o = v.toObject();
        const QString msg = o.value(QLatin1String("error")).toString();
        return msg.isEmpty() ? o.value(QLatin1String("message")).toString() : msg;
    }
    return {};
}

/// True when "error" carries an actual error (not null / "" / false).
bool asiadec_has_error(const QJsonValue& v) {
    if (v.isNull() || v.isUndefined())
        return false;
    if (v.isString())
        return !v.toString().isEmpty();
    if (v.isBool())
        return v.toBool();
    return true;
}

enum class AsiaDecParse { Ok, NoJson, Invalid };

/// The script's JSON payload. PythonRunner already hands over the extracted payload, so the text is
/// parsed as it is; only text that does not parse is scanned for its last complete JSON value
/// (python::extract_json), as the service always did.
AsiaDecParse asiadec_parse(const QString& output, QJsonDocument* doc, QString* parse_error) {
    QJsonParseError err{};
    *doc = QJsonDocument::fromJson(output.toUtf8(), &err);
    if (err.error == QJsonParseError::NoError && !doc->isNull())
        return AsiaDecParse::Ok;
    const QString json = fincept::python::extract_json(output);
    if (json.isEmpty())
        return AsiaDecParse::NoJson;
    *doc = QJsonDocument::fromJson(json.toUtf8(), &err);
    if (err.error == QJsonParseError::NoError && !doc->isNull())
        return AsiaDecParse::Ok;
    *parse_error = err.errorString();
    return AsiaDecParse::Invalid;
}

/// The JSON object a failed run printed before exiting, if any.
QJsonObject asiadec_failure_json(const PythonResult& py) {
    QJsonDocument doc;
    QString unused;
    if (asiadec_parse(py.output, &doc, &unused) != AsiaDecParse::Ok)
        return {};
    return doc.isObject() ? doc.object() : QJsonObject();
}

/// Best-effort message of a failed run: the JSON the script printed before exiting non-zero
/// ({"success": false, "error": "..."}) is far more useful than "exited with code 1".
QString asiadec_failure_message(const PythonResult& py, const QJsonObject& failure, const QString& fallback) {
    const QString msg = asiadec_error_text(failure.value(QLatin1String("error")));
    if (!msg.isEmpty())
        return msg;
    return py.error.isEmpty() ? fallback : py.error;
}

/// The script's failure class: "error_type" ("network" / "unavailable" / "usage" / "missing_param"),
/// else "error_code" / "code" when a script uses those.
QString asiadec_error_type(const QJsonObject& o) {
    for (const char* key : {"error_type", "error_code", "code"}) {
        const QJsonValue v = o.value(QLatin1String(key));
        if (v.isString() && !v.toString().isEmpty())
            return v.toString();
        if (v.isDouble())
            return QString::number(v.toInt());
    }
    return {};
}

/// The parameter names of the structured "needs an input" answer (akshare_screen_args.py
/// missing_param(): {"error_type": "missing_param", "missing": ["symbol", …], "code": "bad_args"}).
QStringList asiadec_missing(const QJsonObject& o) {
    QStringList out;
    const QJsonArray missing = o.value(QLatin1String("missing")).toArray();
    for (const QJsonValue& v : missing) {
        const QString name = v.toString().trimmed();
        if (!name.isEmpty())
            out << name;
    }
    return out;
}

} // namespace

namespace fincept::services::asia_markets {

QueryResult decode_query(const fincept::python::PythonResult& py, const QString& endpoint) {
    QueryResult out;
    if (!py.success) {
        const QJsonObject failure = asiadec_failure_json(py);
        out.error = asiadec_failure_message(py, failure, QStringLiteral("Query failed"));
        out.error_type = asiadec_error_type(failure);
        out.missing = asiadec_missing(failure);
        return out;
    }

    QJsonDocument doc;
    QString parse_error;
    switch (asiadec_parse(py.output, &doc, &parse_error)) {
        case AsiaDecParse::NoJson:
            out.error = QStringLiteral("No data from %1").arg(endpoint);
            return out;
        case AsiaDecParse::Invalid:
            out.error = QStringLiteral("JSON parse error: %1").arg(parse_error);
            return out;
        case AsiaDecParse::Ok:
            break;
    }

    const QJsonObject obj = doc.isObject() ? doc.object() : QJsonObject();
    if (asiadec_has_error(obj.value(QLatin1String("error"))) ||
        (obj.contains(QLatin1String("success")) && !obj.value(QLatin1String("success")).toBool())) {
        const QString msg = asiadec_error_text(obj.value(QLatin1String("error")));
        out.error = msg.isEmpty() ? QStringLiteral("Query returned failure") : msg;
        out.error_type = asiadec_error_type(obj);
        out.missing = asiadec_missing(obj);
        return out;
    }

    QJsonArray rows;
    if (obj.contains(QLatin1String("data"))) {
        const QJsonValue d = obj.value(QLatin1String("data"));
        if (d.isArray()) {
            rows = d.toArray();
        } else if (d.isObject()) {
            rows.append(d);
        } else {
            QJsonObject wrapper;
            wrapper.insert(QStringLiteral("value"), d);
            rows.append(wrapper);
        }
    } else if (doc.isArray()) {
        rows = doc.array();
    } else {
        rows.append(obj);
    }

    // Ordered column names, if the script supplied them. QJsonObject sorts its keys, so deriving
    // columns from a row scrambles the source DataFrame's column order.
    QStringList columns;
    const QJsonValue cols = obj.value(QLatin1String("columns"));
    if (cols.isArray()) {
        const QJsonArray list = cols.toArray();
        for (const QJsonValue& c : list)
            columns << (c.isString() ? c.toString() : c.toVariant().toString()); // a year column may be a number
    }

    out.success = true;
    out.rows = rows;
    out.columns = columns;
    // Named parameter values the endpoint does not take (akshare_screen_args.py).
    const QJsonArray ignored = obj.value(QLatin1String("ignored_params")).toArray();
    for (const QJsonValue& p : ignored)
        out.ignored_params << p.toString();
    out.approx_bytes = fincept::ui::autoload::approx_json_bytes(QJsonValue(out.rows));
    return out;
}

EndpointsResult decode_endpoints(const fincept::python::PythonResult& py) {
    EndpointsResult out;
    if (!py.success) {
        out.error = asiadec_failure_message(py, asiadec_failure_json(py), QStringLiteral("Endpoint listing failed"));
        return out;
    }

    QJsonDocument doc;
    QString parse_error;
    const AsiaDecParse parsed = asiadec_parse(py.output, &doc, &parse_error);
    if (parsed == AsiaDecParse::NoJson) {
        out.error = QStringLiteral("Empty endpoint response");
        return out;
    }
    if (parsed == AsiaDecParse::Invalid || !doc.isObject()) {
        out.error = QStringLiteral("Invalid endpoint JSON: %1")
                        .arg(parse_error.isEmpty() ? QStringLiteral("not a JSON object") : parse_error);
        return out;
    }

    // A script that cannot serve its endpoint list reports it as {"success": false, "error"} (e.g. a
    // deprecated connector) — surface that instead of an empty list.
    const QJsonObject eobj = doc.object();
    if (asiadec_has_error(eobj.value(QLatin1String("error"))) ||
        (eobj.contains(QLatin1String("success")) && !eobj.value(QLatin1String("success")).toBool())) {
        const QString msg = asiadec_error_text(eobj.value(QLatin1String("error")));
        out.error = msg.isEmpty() ? QStringLiteral("Endpoint listing failed") : msg;
        return out;
    }

    out.success = true;
    out.data = eobj;
    return out;
}

} // namespace fincept::services::asia_markets
