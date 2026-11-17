#include "storage/cache/TabSessionStore.h"

#include "core/logging/Logger.h"
#include "storage/sqlite/CacheDatabase.h"

#include <QJsonDocument>

namespace fincept {

namespace {
/// A stored state blob as an object; damaged JSON is "nothing stored" (the screen
/// starts fresh) but is noted at DEBUG — never the blob itself.
QJsonObject tss_parse_object(const QString& json, const QString& key) {
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        if (!json.isEmpty())
            LOG_DEBUG("TabSessionStore", logf("Persisted screen state is not valid JSON",
                                              {{"key", key}, {"bytes", json.size()}, {"error", err.errorString()}}));
        return {};
    }
    return doc.object();
}
} // namespace

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
    s.filters = tss_parse_object(q.value(3).toString(), s.tab_id);
    s.selections = tss_parse_object(q.value(4).toString(), s.tab_id);
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

namespace {
// Internal markers stored inside state_json next to the screen's own keys.
// Stripped on every read, so restore_state() only ever sees domain keys.
constexpr const char* kScreenTypeMarker = "__screen_type__"; // UUID rows: the screen's type id
constexpr const char* kWriterMarker = "__writer_uuid__";     // screen-key rows: panel that wrote it
} // namespace

Result<void> TabSessionStore::save_screen_state(const QString& key, const QJsonObject& state, int state_version,
                                                const QString& session_id, const QString& writer_uuid) {
    auto& cdb = CacheDatabase::instance();
    if (!cdb.is_open())
        return Result<void>::err("Cache database not open");

    QJsonObject stored = state;
    if (!writer_uuid.isEmpty())
        stored.insert(QLatin1String(kWriterMarker), writer_uuid);
    const QString json = QString::fromUtf8(QJsonDocument(stored).toJson(QJsonDocument::Compact));

    // One statement = one atomic SQLite transaction: a crash mid-write leaves
    // either the previous row or the new one, never a torn row.
    auto r = cdb.execute("INSERT OR REPLACE INTO screen_state "
                         "(screen_key, state_version, state_json, updated_at, session_id) "
                         "VALUES (?, ?, ?, strftime('%Y-%m-%dT%H:%M:%fZ','now'), ?)",
                         {key, state_version, json, session_id});

    if (r.is_err())
        return Result<void>::err(r.error());
    return Result<void>::ok();
}

Result<ScreenStateRow> TabSessionStore::load_screen_state_row(const QString& key, int expected_version) {
    auto& cdb = CacheDatabase::instance();
    if (!cdb.is_open())
        return Result<ScreenStateRow>::err("Cache database not open");

    auto r = cdb.execute("SELECT state_version, state_json, updated_at FROM screen_state WHERE screen_key = ?", {key});
    if (r.is_err())
        return Result<ScreenStateRow>::err(r.error());

    ScreenStateRow row;
    auto& q = r.value();
    if (!q.next() || q.value(0).toInt() != expected_version)
        return Result<ScreenStateRow>::ok(row); // nothing stored, or version mismatch — start fresh

    row.state = tss_parse_object(q.value(1).toString(), key);
    row.updated_at = q.value(2).toString();
    row.writer_uuid = row.state.value(QLatin1String(kWriterMarker)).toString();
    row.state.remove(QLatin1String(kWriterMarker));
    row.found = true;
    return Result<ScreenStateRow>::ok(row);
}

Result<QJsonObject> TabSessionStore::load_screen_state(const QString& key, int expected_version) {
    auto r = load_screen_state_row(key, expected_version);
    if (r.is_err())
        return Result<QJsonObject>::err(r.error());
    return Result<QJsonObject>::ok(r.value().state); // empty when nothing stored / version mismatch
}

// ── Phase 4b: instance-UUID-keyed screen state ───────────────────────────────

Result<void> TabSessionStore::save_screen_state_by_uuid(const QString& instance_uuid, const QString& screen_key,
                                                        const QJsonObject& state, int state_version,
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
        augmented.insert(QLatin1String(kScreenTypeMarker), screen_key);
    const QString augmented_json = QString::fromUtf8(QJsonDocument(augmented).toJson(QJsonDocument::Compact));

    auto r = cdb.execute("INSERT OR REPLACE INTO screen_state "
                         "(screen_key, state_version, state_json, updated_at, session_id, instance_uuid) "
                         "VALUES (?, ?, ?, strftime('%Y-%m-%dT%H:%M:%fZ','now'), ?, ?)",
                         {synthesised_key, state_version, augmented_json, session_id, instance_uuid});

    if (r.is_err())
        return Result<void>::err(r.error());
    return Result<void>::ok();
}

Result<ScreenStateRow> TabSessionStore::load_screen_state_row_by_uuid(const QString& instance_uuid,
                                                                     int expected_version) {
    auto& cdb = CacheDatabase::instance();
    if (!cdb.is_open())
        return Result<ScreenStateRow>::err("Cache database not open");
    ScreenStateRow row;
    if (instance_uuid.isEmpty())
        return Result<ScreenStateRow>::ok(row); // not an error — just no state

    auto r = cdb.execute("SELECT state_version, state_json, updated_at FROM screen_state WHERE instance_uuid = ?",
                         {instance_uuid});
    if (r.is_err())
        return Result<ScreenStateRow>::err(r.error());

    auto& q = r.value();
    if (!q.next() || q.value(0).toInt() != expected_version)
        return Result<ScreenStateRow>::ok(row); // nothing stored, or version mismatch — start fresh

    row.state = tss_parse_object(q.value(1).toString(), instance_uuid);
    // Strip the diagnostic-only markers before handing back to the screen —
    // its restore_state() implementation expects only domain keys.
    row.state.remove(QLatin1String(kScreenTypeMarker));
    row.state.remove(QLatin1String(kWriterMarker));
    row.updated_at = q.value(2).toString();
    row.found = true;
    return Result<ScreenStateRow>::ok(row);
}

Result<QJsonObject> TabSessionStore::load_screen_state_by_uuid(const QString& instance_uuid, int expected_version) {
    auto r = load_screen_state_row_by_uuid(instance_uuid, expected_version);
    if (r.is_err())
        return Result<QJsonObject>::err(r.error());
    return Result<QJsonObject>::ok(r.value().state);
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
