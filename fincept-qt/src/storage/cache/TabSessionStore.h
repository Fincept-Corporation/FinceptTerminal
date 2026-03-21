#pragma once
#include <QString>
#include <QJsonObject>
#include "core/result/Result.h"

namespace fincept {

struct TabSession {
    QString     tab_id;
    QString     screen_name;
    double      scroll_position = 0;
    QJsonObject filters;
    QJsonObject selections;
    QString     last_accessed;
};

/// Persists per-tab UI state (scroll position, filters, selections) in cache.db.
class TabSessionStore {
public:
    static TabSessionStore& instance();

    Result<void>       save(const TabSession& session);
    Result<TabSession> load(const QString& tab_id);
    Result<void>       remove(const QString& tab_id);
    Result<void>       clear_all();

private:
    TabSessionStore() = default;
};

} // namespace fincept
