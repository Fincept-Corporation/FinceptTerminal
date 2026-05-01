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

// ── Phase 4b: instance-UUID-keyed screen state ───────────────────────────────

Result<void> TabSessionStore::save_screen_state_by_uuid(const QString& instance_uuid,
                                                        const QString& screen_key,
                                                        const QJsonObject& state,
                                                        int state_version,
                                                        const QString& session_id) {
    auto& cdb = CacheDatabase::instance();
    if (!cdb.is_open())
        return Result<void>::err("Cache database not open");
    if (instance_uuid.isEmpty())
        return Result<void>::err("instance_uuid is required for UUID-keyed save");

    const QString json = QString::fromUtf8(QJsonDocument(state).toJson(QJsonDocument::Compact));

    // The legacy table has screen_key as PRIMARY KEY, which we keep for
    // backward compat. UUID-keyed rows use a synthesised screen_key of the
    // form "uuid:<instance_uuid>" so they don't collide with type-id keys
    // in the legacy code path. The real type id is preserved separately
    // by writing it back via screen_key column for diagnostics — wait,
    // primary key collision. Use upsert on the synthesised key but store
    // the real screen type in the json blob instead. Simplest invariant.
    const QString synthesised_key = "uuid:" + instance_uuid;

    // Embed the screen_type into the saved object so consumers reading by
    // UUID still know what type id this state belongs to. The screen
    // restore_state() callback ignores unknown keys, so no incompatibility.
    QJsonObject augmented = state;
    if (!screen_key.isEmpty())
        augmented.insert("__screen_type__", screen_key);
    const QString augmented_json =
        QString::fromUtf8(QJsonDocument(augmented).toJson(QJsonDocument::Compact));

    auto r = cdb.execute(
        "INSERT OR REPLACE INTO screen_state "
        "(screen_key, state_version, state_json, updated_at, session_id, instance_uuid) "
        "VALUES (?, ?, ?, strftime('%Y-%m-%dT%H:%M:%fZ','now'), ?, ?)",
        {synthesised_key, state_version, augmented_json, session_id, instance_uuid});

    if (r.is_err())
        return Result<void>::err(r.error());
    return Result<void>::ok();
}

Result<QJsonObject> TabSessionStore::load_screen_state_by_uuid(const QString& instance_uuid,
                                                                int expected_version) {
    auto& cdb = CacheDatabase::instance();
    if (!cdb.is_open())
        return Result<QJsonObject>::err("Cache database not open");
    if (instance_uuid.isEmpty())
        return Result<QJsonObject>::ok(QJsonObject{}); // not an error — just no state

    auto r = cdb.execute(
        "SELECT state_version, state_json FROM screen_state WHERE instance_uuid = ?",
        {instance_uuid});

    if (r.is_err())
        return Result<QJsonObject>::err(r.error());

    auto& q = r.value();
    if (!q.next())
        return Result<QJsonObject>::ok(QJsonObject{}); // no saved state yet

    const int saved_version = q.value(0).toInt();
    if (saved_version != expected_version)
        return Result<QJsonObject>::ok(QJsonObject{}); // version mismatch — start fresh

    QJsonObject obj = QJsonDocument::fromJson(q.value(1).toString().toUtf8()).object();
    // Strip the diagnostic-only __screen_type__ marker before handing back
    // to the screen — its restore_state() implementation expects only
    // domain keys.
    obj.remove("__screen_type__");
    return Result<QJsonObject>::ok(obj);
}

Result<void> TabSessionStore::remove_screen_state_by_uuid(const QString& instance_uuid) {
    auto& cdb = CacheDatabase::instance();
    if (!cdb.is_open())
        return Result<void>::err("Cache database not open");
    if (instance_uuid.isEmpty())
        return Result<void>::ok(); // nothing to remove

    auto r = cdb.execute("DELETE FROM screen_state WHERE instance_uuid = ?", {instance_uuid});
    if (r.is_err())
        return Result<void>::err(r.error());
    return Result<void>::ok();
}

} // namespace fincept
