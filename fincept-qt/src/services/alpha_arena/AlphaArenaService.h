#pragma once
// AlphaArenaService — thin wrapper around the Alpha Arena Python engine so
// the screen does not spawn Python directly (D1 / P6).
//
// Alpha Arena is a user-driven competition runner: the user creates a
// competition, runs cycles, refreshes leaderboard. All actions are one-shot
// JSON-in / JSON-out calls against alpha_arena/main.py — no streaming cadence
// to put on the hub, so a non-Producer service wrapper is the right shape.

#include <QJsonObject>
#include <QObject>
#include <QString>

#include <functional>

namespace fincept::services::alpha_arena {

struct ActionResult {
    bool success = false;
    QJsonObject data;  // engine response (top-level JSON)
    QString error;
};

using ActionCallback = std::function<void(const ActionResult&)>;

class AlphaArenaService : public QObject {
    Q_OBJECT
  public:
    static AlphaArenaService& instance();

    /// Runs `alpha_arena/main.py <payload>` where payload is `{action, params, api_keys?}`.
    /// The caller builds the payload; we dispatch and parse the JSON response.
    void run_action(const QJsonObject& payload, ActionCallback cb);

  private:
    AlphaArenaService() = default;
};

} // namespace fincept::services::alpha_arena
