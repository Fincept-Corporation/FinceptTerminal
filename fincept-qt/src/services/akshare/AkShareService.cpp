#include "services/akshare/AkShareService.h"

#include "core/logging/Logger.h"
#include "python/PythonRunner.h"

#include <QJsonDocument>
#include <QJsonParseError>

namespace {

/// A connector script reports failure as {"error": "msg"}, {"error": {"error": "msg", ...}} or
/// {"success": false, ...}; successful responses may carry "error": null. Returns the message, or
/// an empty string when the value does not describe an error.
QString akshare_svc_error_text(const QJsonValue& v) {
    if (v.isString())
        return v.toString();
    if (v.isObject()) {
        const QJsonObject o = v.toObject();
        const QString msg = o.value("error").toString();
        return msg.isEmpty() ? o.value("message").toString() : msg;
    }
    return {};
}

/// True when "error" carries an actual error (not null / "" / false).
bool akshare_svc_has_error(const QJsonValue& v) {
    if (v.isNull() || v.isUndefined())
        return false;
    if (v.isString())
        return !v.toString().isEmpty();
    if (v.isBool())
        return v.toBool();
    return true;
}

/// Best-effort message from a failed script run: the JSON the script printed before exiting
/// non-zero ({"success": false, "error": "..."}) is far more useful than "exited with code 1".
QString akshare_svc_failure_message(const fincept::python::PythonResult& result, const QString& fallback) {
    const QString json_str = fincept::python::extract_json(result.output);
    if (!json_str.isEmpty()) {
        const auto doc = QJsonDocument::fromJson(json_str.toUtf8());
        if (doc.isObject()) {
            const QString msg = akshare_svc_error_text(doc.object().value("error"));
            if (!msg.isEmpty())
                return msg;
        }
    }
    return result.error.isEmpty() ? fallback : result.error;
}

} // namespace

namespace fincept::services::akshare {

AkShareService& AkShareService::instance() {
    static AkShareService s;
    return s;
}

void AkShareService::fetch_endpoints(const QString& script, EndpointsCallback cb) {
    fincept::python::PythonRunner::instance().run(
        script, {QStringLiteral("get_all_endpoints")},
        [cb = std::move(cb), script](const fincept::python::PythonResult& result) {
            EndpointsResult out;
            if (!result.success) {
                out.error = akshare_svc_failure_message(result, QStringLiteral("Endpoint listing failed"));
                LOG_ERROR("AkShareService",
                          QStringLiteral("%1 get_all_endpoints failed: %2").arg(script, out.error.left(300)));
                if (cb)
                    cb(out);
                return;
            }

            const QString json_str = fincept::python::extract_json(result.output);
            if (json_str.isEmpty()) {
                out.error = QStringLiteral("Empty endpoint response");
                if (cb)
                    cb(out);
                return;
            }

            QJsonParseError err;
            const auto doc = QJsonDocument::fromJson(json_str.toUtf8(), &err);
            if (doc.isNull() || !doc.isObject()) {
                out.error = QStringLiteral("Invalid endpoint JSON: %1").arg(err.errorString());
                if (cb)
                    cb(out);
                return;
            }

            // A script that cannot serve its endpoint list reports it as {"success": false, "error"}
            // (e.g. a deprecated connector) — surface that instead of an empty list.
            const QJsonObject eobj = doc.object();
            if (akshare_svc_has_error(eobj.value("error")) || (eobj.contains("success") && !eobj.value("success").toBool())) {
                const QString msg = akshare_svc_error_text(eobj.value("error"));
                out.error = msg.isEmpty() ? QStringLiteral("Endpoint listing failed") : msg;
                if (cb)
                    cb(out);
                return;
            }

            out.success = true;
            out.data = eobj;
            if (cb)
                cb(out);
        });
}

void AkShareService::query(const QString& script, const QString& endpoint, const QStringList& extra_args,
                           QueryCallback cb) {
    QStringList args;
    args << endpoint << extra_args;

    fincept::python::PythonRunner::instance().run(
        script, args, [cb = std::move(cb), script, endpoint](const fincept::python::PythonResult& result) {
            QueryResult out;
            if (!result.success) {
                out.error = akshare_svc_failure_message(result, QStringLiteral("Query failed"));
                LOG_ERROR("AkShareService",
                          QStringLiteral("%1 %2 failed: %3").arg(script, endpoint, out.error.left(300)));
                if (cb)
                    cb(out);
                return;
            }

            const QString json_str = fincept::python::extract_json(result.output);
            if (json_str.isEmpty()) {
                out.error = QStringLiteral("No data from %1").arg(endpoint);
                if (cb)
                    cb(out);
                return;
            }

            QJsonParseError err;
            const auto doc = QJsonDocument::fromJson(json_str.toUtf8(), &err);
            if (doc.isNull()) {
                out.error = QStringLiteral("JSON parse error: %1").arg(err.errorString());
                if (cb)
                    cb(out);
                return;
            }

            const QJsonObject obj = doc.isObject() ? doc.object() : QJsonObject();

            if (akshare_svc_has_error(obj.value("error")) || (obj.contains("success") && !obj["success"].toBool())) {
                const QString msg = akshare_svc_error_text(obj.value("error"));
                out.error = msg.isEmpty() ? QStringLiteral("Query returned failure") : msg;
                if (cb)
                    cb(out);
                return;
            }

            QJsonArray rows;
            if (obj.contains("data")) {
                const auto d = obj["data"];
                if (d.isArray()) {
                    rows = d.toArray();
                } else if (d.isObject()) {
                    rows.append(d);
                } else {
                    QJsonObject wrapper;
                    wrapper["value"] = d;
                    rows.append(wrapper);
                }
            } else if (doc.isArray()) {
                rows = doc.array();
            } else {
                rows.append(obj);
            }

            // Ordered column names, if the script supplied them. QJsonObject sorts
            // its keys, so deriving columns from a row would scramble the source
            // DataFrame's column order — the script's explicit list preserves it.
            QStringList columns;
            if (obj.contains("columns") && obj["columns"].isArray()) {
                for (const auto& c : obj["columns"].toArray())
                    columns << c.toString();
            }

            out.success = true;
            out.rows = rows;
            out.columns = columns;
            if (cb)
                cb(out);
        });
}

} // namespace fincept::services::akshare
