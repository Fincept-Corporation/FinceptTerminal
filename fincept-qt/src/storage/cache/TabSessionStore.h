#pragma once
#include "core/result/Result.h"

#include <QJsonObject>
#include <QString>

namespace fincept {

struct TabSession {
    QString tab_id;
    QString screen_name;
    double scroll_position = 0;
    QJsonObject filters;
    QJsonObject selections;
    QString last_accessed;
};

/// Persists per-tab UI state (scroll position, filters, selections) in cache.db.
class TabSessionStore {
  public:
    static TabSessionStore& instance();

    Result<void> save(const TabSession& session);
    Result<TabSession> load(const QString& tab_id);
    Result<void> remove(const QString& tab_id);
    Result<void> clear_all();

    /// Persist screen UI state keyed by screen_key.
    /// state_version is stored alongside — if the loaded version doesn't match
    /// expected_version, an empty QJsonObject is returned so the screen starts fresh.
    Result<void>        save_screen_state(const QString& key,
                                          const QJsonObject& state,
                                          int state_version,
                                          const QString& session_id = {});
    Result<QJsonObject> load_screen_state(const QString& key, int expected_version);

  private:
    TabSessionStore() = default;
};

} // namespace fincept
