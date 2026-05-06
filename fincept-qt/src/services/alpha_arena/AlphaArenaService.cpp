#include "services/alpha_arena/AlphaArenaService.h"

#include "core/logging/Logger.h"

#include <QTimer>

namespace fincept::services::alpha_arena {

AlphaArenaService& AlphaArenaService::instance() {
    static AlphaArenaService s;
    return s;
}

void AlphaArenaService::run_action(const QJsonObject& payload, ActionCallback cb) {
    LOG_WARN("AlphaArena",
             QStringLiteral("AlphaArenaService stub invoked for action=%1; "
                            "engine_unavailable until Phase 7 screen rewrite lands.")
                 .arg(payload.value(QStringLiteral("action")).toString()));
    if (!cb)
        return;

    ActionResult r;
    r.success = false;
    r.error = QStringLiteral("engine_unavailable");

    QTimer::singleShot(0, [cb = std::move(cb), r = std::move(r)]() { cb(r); });
}

} // namespace fincept::services::alpha_arena
