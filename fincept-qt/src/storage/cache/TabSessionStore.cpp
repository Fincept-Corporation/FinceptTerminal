#include "storage/cache/TabSessionStore.h"

#include "storage/sqlite/CacheDatabase.h"

#include <QJsonDocument>

namespace fincept {

TabSessionStore& TabSessionStore::instance() {
    static TabSessionStore s;
    return s;
}

Result<void> TabSessionStore::save(const TabSession& session) {
    auto& cdb = CacheDatabase::instance();
    if (!cdb.is_open())
        return Result<void>::err("Cache database not open");

    QString filters_str = QString::fromUtf8(QJsonDocument(session.filters).toJson(QJsonDocument::Compact));
    QString selections_str = QString::fromUtf8(QJsonDocument(session.selections).toJson(QJsonDocument::Compact));

    auto r = cdb.execute("INSERT OR REPLACE INTO tab_sessions "
                         "(tab_id, screen_name, scroll_position, filters_json, selections_json, "
                         " last_accessed, updated_at) "
                         "VALUES (?, ?, ?, ?, ?, datetime('now'), datetime('now'))",
                         {session.tab_id, session.screen_name, session.scroll_position, filters_str, selections_str});

    if (r.is_err())
        return Result<void>::err(r.error());
    return Result<void>::ok();
}

Result<TabSession> TabSessionStore::load(const QString& tab_id) {
    auto& cdb = CacheDatabase::instance();
    if (!cdb.is_open())
        return Result<TabSession>::err("Cache database not open");

    auto r = cdb.execute("SELECT tab_id, screen_name, scroll_position, filters_json, selections_json, last_accessed "
                         "FROM tab_sessions WHERE tab_id = ?",
                         {tab_id});

    if (r.is_err())
        return Result<TabSession>::err(r.error());
    auto& q = r.value();
    if (!q.next())
        return Result<TabSession>::err("Tab session not found");

    TabSession s;
    s.tab_id = q.value(0).toString();
    s.screen_name = q.value(1).toString();
    s.scroll_position = q.value(2).toDouble();
    s.filters = QJsonDocument::fromJson(q.value(3).toString().toUtf8()).object();
    s.selections = QJsonDocument::fromJson(q.value(4).toString().toUtf8()).object();
    s.last_accessed = q.value(5).toString();
    return Result<TabSession>::ok(s);
}

Result<void> TabSessionStore::remove(const QString& tab_id) {
    auto& cdb = CacheDatabase::instance();
    if (!cdb.is_open())
        return Result<void>::err("Cache database not open");

    auto r = cdb.execute("DELETE FROM tab_sessions WHERE tab_id = ?", {tab_id});
    if (r.is_err())
        return Result<void>::err(r.error());
    return Result<void>::ok();
}

Result<void> TabSessionStore::clear_all() {
    auto& cdb = CacheDatabase::instance();
    if (!cdb.is_open())
        return Result<void>::err("Cache database not open");

    auto r = cdb.execute("DELETE FROM tab_sessions", {});
    if (r.is_err())
        return Result<void>::err(r.error());
    return Result<void>::ok();
}

Result<void> TabSessionStore::save_screen_state(const QString& key, const QJsonObject& state, int state_version,
                                                const QString& session_id) {
    auto& cdb = CacheDatabase::instance();
    if (!cdb.is_open())
        return Result<void>::err("Cache database not open");

    const QString json = QString::fromUtf8(QJsonDocument(state).toJson(QJsonDocument::Compact));

    auto r = cdb.execute("INSERT OR REPLACE INTO screen_state "
                         "(screen_key, state_version, state_json, updated_at, session_id) "
                         "VALUES (?, ?, ?, strftime('%Y-%m-%dT%H:%M:%fZ','now'), ?)",
                         {key, state_version, json, session_id});

    if (r.is_err())
        return Result<void>::err(r.error());
    return Result<void>::ok();
}

Result<QJsonObject> TabSessionStore::load_screen_state(const QString& key, int expected_version) {
    auto& cdb = CacheDatabase::instance();
    if (!cdb.is_open())
        return Result<QJsonObject>::err("Cache database not open");

    auto r = cdb.execute("SELECT state_version, state_json FROM screen_state WHERE screen_key = ?", {key});

    if (r.is_err())
        return Result<QJsonObject>::err(r.error());

    auto& q = r.value();
    if (!q.next())
        return Result<QJsonObject>::ok(QJsonObject{}); // no saved state yet

    const int saved_version = q.value(0).toInt();
    if (saved_version != expected_version)
        return Result<QJsonObject>::ok(QJsonObject{}); // version mismatch — start fresh

    const QJsonObject obj = QJsonDocument::fromJson(q.value(1).toString().toUtf8()).object();
    return Result<QJsonObject>::ok(obj);
}

} // namespace fincept
