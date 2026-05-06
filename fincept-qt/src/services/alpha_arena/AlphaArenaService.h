#pragma once
// AlphaArenaService — TEMPORARY no-op stub.
//
// The original service was deleted in Phase 1 of the production refactor (see
// .grill-me/alpha-arena-production-refactor.md). The new implementation is
// AlphaArenaEngine (services/alpha_arena/AlphaArenaEngine.h). This stub exists
// only to keep the legacy AlphaArenaScreen compiling until Phase 7 replaces
// the screen entirely.
//
// run_action() always reports success=false with error="engine_unavailable".
// Do NOT add real behaviour here. Add it in AlphaArenaEngine.

#include <QJsonObject>
#include <QString>

#include <functional>

namespace fincept::services::alpha_arena {

struct ActionResult {
    bool success = false;
    QString error;
    QJsonObject data;
};

using ActionCallback = std::function<void(const ActionResult&)>;

class AlphaArenaService {
  public:
    static AlphaArenaService& instance();
    void run_action(const QJsonObject& payload, ActionCallback cb);

  private:
    AlphaArenaService() = default;
};

} // namespace fincept::services::alpha_arena
