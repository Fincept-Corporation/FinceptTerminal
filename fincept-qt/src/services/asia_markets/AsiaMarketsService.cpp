#include "services/asia_markets/AsiaMarketsService.h"

#include "core/logging/Logger.h"
#include "python/PythonRunner.h"

#include <QJsonDocument>
#include <QJsonParseError>

namespace fincept::services::asia_markets {

AsiaMarketsService& AsiaMarketsService::instance() {
    static AsiaMarketsService s;
    return s;
}

void AsiaMarketsService::fetch_endpoints(const QString& script, EndpointsCallback cb) {
    fincept::python::PythonRunner::instance().run(
        script, {QStringLiteral("get_all_endpoints")},
        [cb = std::move(cb), script](const fincept::python::PythonResult& result) {
            EndpointsResult out;
            if (!result.success) {
                out.error = result.error.isEmpty()
                                ? QStringLiteral("Endpoint listing failed")
                                : result.error;
                LOG_ERROR("AsiaMarketsService",
                          QStringLiteral("%1 get_all_endpoints failed: %2")
                              .arg(script, out.error.left(300)));
                if (cb) cb(out);
                return;
            }

            const QString json_str = fincept::python::extract_json(result.output);
            if (json_str.isEmpty()) {
                out.error = QStringLiteral("Empty endpoint response");
                if (cb) cb(out);
                return;
            }

            QJsonParseError err;
            const auto doc = QJsonDocument::fromJson(json_str.toUtf8(), &err);
            if (doc.isNull() || !doc.isObject()) {
                out.error = QStringLiteral("Invalid endpoint JSON: %1").arg(err.errorString());
                if (cb) cb(out);
                return;
            }

            out.success = true;
            out.data = doc.object();
            if (cb) cb(out);
        });
}

void AsiaMarketsService::query(const QString& script, const QString& endpoint,
                               const QStringList& extra_args, QueryCallback cb) {
    QStringList args;
    args << endpoint << extra_args;

    fincept::python::PythonRunner::instance().run(
        script, args,
        [cb = std::move(cb), script, endpoint](const fincept::python::PythonResult& result) {
            QueryResult out;
            if (!result.success) {
                out.error = result.error.isEmpty() ? QStringLiteral("Query failed") : result.error;
                LOG_ERROR("AsiaMarketsService",
                          QStringLiteral("%1 %2 failed: %3")
                              .arg(script, endpoint, out.error.left(300)));
                if (cb) cb(out);
                return;
            }

            const QString json_str = fincept::python::extract_json(result.output);
            if (json_str.isEmpty()) {
                out.error = QStringLiteral("No data from %1").arg(endpoint);
                if (cb) cb(out);
                return;
            }

            QJsonParseError err;
            const auto doc = QJsonDocument::fromJson(json_str.toUtf8(), &err);
            if (doc.isNull()) {
                out.error = QStringLiteral("JSON parse error: %1").arg(err.errorString());
                if (cb) cb(out);
                return;
            }

            const QJsonObject obj = doc.isObject() ? doc.object() : QJsonObject();

            if (obj.contains("error")) {
                out.error = obj["error"].toString();
                if (cb) cb(out);
                return;
            }
            if (obj.contains("success") && !obj["success"].toBool()) {
                out.error = obj.value("error").toString(QStringLiteral("Query returned failure"));
                if (cb) cb(out);
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

            out.success = true;
            out.rows = rows;
            if (cb) cb(out);
        });
}

} // namespace fincept::services::asia_markets
