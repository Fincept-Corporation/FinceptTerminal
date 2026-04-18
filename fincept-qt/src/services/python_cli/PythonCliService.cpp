#include "services/python_cli/PythonCliService.h"

#include "core/logging/Logger.h"
#include "python/PythonRunner.h"

#include <QJsonDocument>
#include <QJsonParseError>

namespace fincept::services::python_cli {

PythonCliService& PythonCliService::instance() {
    static PythonCliService s;
    return s;
}

void PythonCliService::run(const QString& script, const QStringList& args, CliCallback cb) {
    fincept::python::PythonRunner::instance().run(
        script, args,
        [cb = std::move(cb), script](const fincept::python::PythonResult& result) {
            CliResult out;
            if (!result.success) {
                out.error = result.error.isEmpty()
                                ? QStringLiteral("Python process failed")
                                : result.error;
                LOG_ERROR("PythonCliService",
                          QStringLiteral("%1 failed: %2").arg(script, out.error.left(300)));
                if (cb) cb(out);
                return;
            }

            const QString json_str = fincept::python::extract_json(result.output);
            if (json_str.isEmpty()) {
                out.error = QStringLiteral("No JSON output from %1").arg(script);
                if (cb) cb(out);
                return;
            }

            QJsonParseError err;
            const auto doc = QJsonDocument::fromJson(json_str.toUtf8(), &err);
            if (doc.isNull() || !doc.isObject()) {
                out.error = QStringLiteral("Invalid JSON: %1").arg(err.errorString());
                if (cb) cb(out);
                return;
            }

            const QJsonObject obj = doc.object();
            if (obj.contains("error")) {
                out.error = obj["error"].toString(QStringLiteral("Script reported error"));
                if (cb) cb(out);
                return;
            }
            if (obj.contains("success") && !obj["success"].toBool(true)) {
                out.error = obj.value("error").toString(QStringLiteral("Script reported failure"));
                if (cb) cb(out);
                return;
            }

            out.success = true;
            out.data = obj;
            if (cb) cb(out);
        });
}

} // namespace fincept::services::python_cli
