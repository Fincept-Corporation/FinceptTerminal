#include "services/python_cli/PythonCliService.h"

#include "core/logging/Logger.h"
#include "python/PythonRunner.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>

namespace fincept::services::python_cli {

namespace {

// One reply -> CliResult, shared by both run() overloads.
void pcs_deliver(const QString& script, const fincept::python::PythonResult& result, bool cancelled,
                 const CliCallback& cb) {
    CliResult out;
    if (!result.success) {
        out.cancelled = cancelled;
        out.error = result.error.isEmpty() ? QStringLiteral("Python process failed") : result.error;
        if (!cancelled)
            LOG_ERROR("PythonCliService",
                      logf("Python CLI script failed", {{"script", script}, {"error", log_scrub(out.error.left(300))}}));
        if (cb)
            cb(out);
        return;
    }

    const QString json_str = fincept::python::extract_json(result.output);
    if (json_str.isEmpty()) {
        out.error = QStringLiteral("No JSON output from %1").arg(script);
        if (cb)
            cb(out);
        return;
    }

    QJsonParseError err;
    const auto doc = QJsonDocument::fromJson(json_str.toUtf8(), &err);
    if (doc.isNull() || !doc.isObject()) {
        out.error = QStringLiteral("Invalid JSON: %1").arg(err.errorString());
        if (cb)
            cb(out);
        return;
    }

    const QJsonObject obj = doc.object();
    // `"error"` is a failure only when it carries one. A success payload that
    // also reports `"error": null` / `false` / `""` (several scripts do) must
    // not be turned into a failure — same rule PythonRunner applies.
    const QJsonValue err_val = obj.value("error");
    const bool has_error = err_val.isString()   ? !err_val.toString().trimmed().isEmpty()
                           : err_val.isBool()   ? err_val.toBool()
                           : err_val.isObject() ? !err_val.toObject().isEmpty()
                           : err_val.isArray()  ? !err_val.toArray().isEmpty()
                                                : !(err_val.isNull() || err_val.isUndefined());
    if (has_error) {
        out.error = err_val.isString() ? err_val.toString() : obj.value("message").toString();
        if (out.error.isEmpty())
            out.error = QStringLiteral("Script reported error");
        if (cb)
            cb(out);
        return;
    }
    if (obj.contains("success") && !obj["success"].toBool(true)) {
        out.error = obj.value("error").toString(QStringLiteral("Script reported failure"));
        if (cb)
            cb(out);
        return;
    }

    out.success = true;
    out.data = obj;
    if (cb)
        cb(out);
}

} // namespace

PythonCliService& PythonCliService::instance() {
    static PythonCliService s;
    return s;
}

void PythonCliService::run(const QString& script, const QStringList& args, CliCallback cb) {
    fincept::python::PythonRunner::instance().run(
        script, args, [cb = std::move(cb), script](const fincept::python::PythonResult& result) {
            pcs_deliver(script, result, /*cancelled=*/false, cb);
        });
}

void PythonCliService::run(const QString& script, const QStringList& args, const CliRunOptions& options,
                           CliCallback cb) {
    using fincept::python::PythonRunner;
    PythonRunner::RunOptions ro;
    ro.timeout_ms = options.timeout_ms > 0 ? options.timeout_ms : PythonRunner::kTimeoutFromScript;
    ro.cancel_token = options.cancel_token;
    ro.stdin_data = options.stdin_data;
    ro.max_output_bytes = options.max_output_bytes;
    const auto token = options.cancel_token;
    PythonRunner::instance().run_with_options(
        script, args, ro, [cb = std::move(cb), script, token](const fincept::python::PythonResult& result) {
            const bool cancelled = !result.success && token && token->load();
            pcs_deliver(script, result, cancelled, cb);
        });
}

} // namespace fincept::services::python_cli
