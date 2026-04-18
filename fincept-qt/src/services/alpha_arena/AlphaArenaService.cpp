#include "services/alpha_arena/AlphaArenaService.h"

#include "core/logging/Logger.h"
#include "python/PythonRunner.h"

#include <QJsonDocument>
#include <QJsonParseError>

namespace fincept::services::alpha_arena {

AlphaArenaService& AlphaArenaService::instance() {
    static AlphaArenaService s;
    return s;
}

void AlphaArenaService::run_action(const QJsonObject& payload, ActionCallback cb) {
    const QString json_arg =
        QString::fromUtf8(QJsonDocument(payload).toJson(QJsonDocument::Compact));
    const QString action = payload.value("action").toString();

    fincept::python::PythonRunner::instance().run(
        QStringLiteral("alpha_arena/main.py"), {json_arg},
        [cb = std::move(cb), action](const fincept::python::PythonResult& result) {
            ActionResult out;
            if (!result.success) {
                out.error = result.error.isEmpty()
                                ? QStringLiteral("Engine call failed")
                                : result.error;
                LOG_ERROR("AlphaArenaService",
                          QStringLiteral("%1 failed: %2").arg(action, out.error.left(300)));
                if (cb) cb(out);
                return;
            }

            const QString json_str = fincept::python::extract_json(result.output);
            if (json_str.isEmpty()) {
                out.error = QStringLiteral("No response from engine");
                if (cb) cb(out);
                return;
            }

            QJsonParseError err;
            const auto doc = QJsonDocument::fromJson(json_str.toUtf8(), &err);
            if (doc.isNull() || !doc.isObject()) {
                out.error = QStringLiteral("Invalid response: %1").arg(err.errorString());
                if (cb) cb(out);
                return;
            }

            const QJsonObject obj = doc.object();
            if (!obj.value("success").toBool(false)) {
                out.error = obj.value("error").toString(QStringLiteral("Engine reported failure"));
                if (cb) cb(out);
                return;
            }

            out.success = true;
            out.data = obj;
            if (cb) cb(out);
        });
}

} // namespace fincept::services::alpha_arena
