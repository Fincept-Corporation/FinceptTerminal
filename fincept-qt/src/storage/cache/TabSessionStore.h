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
    Result<void> save_screen_state(const QString& key, const QJsonObject& state, int state_version,
                                   const QString& session_id = {});
    Result<QJsonObject> load_screen_state(const QString& key, int expected_version);

    /// Phase 4b: persist screen UI state keyed by per-instance UUID rather
    /// than by screen type id. Two panels of the same type (e.g. two
    /// watchlists) get distinct rows; closing one and re-opening a fresh
    /// instance starts with default state.
    ///
    /// Storage uses the same screen_state table — instance_uuid is the
    /// primary lookup; screen_key still gets written (the screen's type
    /// id) for diagnostics and for the legacy by-key path that some
    /// non-panel callers still use (workspace participants, etc.).
    Result<void> save_screen_state_by_uuid(const QString& instance_uuid,
                                           const QString& screen_key,
                                           const QJsonObject& state,
                                           int state_version,
                                           const QString& session_id = {});
    Result<QJsonObject> load_screen_state_by_uuid(const QString& instance_uuid,
                                                  int expected_version);

    /// Drop a UUID-keyed row. Called by DockScreenRouter when a panel is
    /// permanently closed (vs. dematerialised) so the cache doesn't
    /// accumulate orphan rows for closed instances.
    Result<void> remove_screen_state_by_uuid(const QString& instance_uuid);

  private:
    TabSessionStore() = default;
};

} // namespace fincept
