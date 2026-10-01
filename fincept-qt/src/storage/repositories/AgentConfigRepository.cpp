#include "storage/repositories/AgentConfigRepository.h"

#include "storage/sync/SyncOutbox.h"

#include <QJsonDocument>
#include <QJsonObject>

namespace fincept {

namespace {
/// Agent configs once snapshotted the resolved LLM profile into
/// `config_json.model`, `api_key` included. That row lives in plain SQLite, is
/// pushed to cloud sync and is returned by MCP list tools — a provider key must
/// not travel with it (the live key is re-resolved from the agent's profile at
/// run time; the stored `model` snapshot is ignored). This is the central choke
/// point: save() strips it for every writer, and reads strip it for older rows
/// that have not been re-saved yet. A config without the key is returned
/// byte-for-byte unchanged.
QString agentcfg_strip_model_api_key(const QString& config_json) {
    if (!config_json.contains(QLatin1String("api_key")))
        return config_json; // cheap pre-check: nothing to do for the common case
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(config_json.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return config_json;
    QJsonObject obj = doc.object();
    QJsonValue model_val = obj.value(QLatin1String("model"));
    if (!model_val.isObject())
        return config_json;
    QJsonObject model = model_val.toObject();
    if (!model.contains(QLatin1String("api_key")))
        return config_json;
    model.remove(QLatin1String("api_key"));
    obj.insert(QLatin1String("model"), model);
    return QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}
} // namespace

AgentConfigRepository& AgentConfigRepository::instance() {
    static AgentConfigRepository s;
    return s;
}

AgentConfig AgentConfigRepository::map_row(QSqlQuery& q) {
    return {q.value(0).toString(),
            q.value(1).toString(),
            q.value(2).toString(),
            agentcfg_strip_model_api_key(q.value(3).toString()),
            q.value(4).toString(),
            q.value(5).toBool(),
            q.value(6).toBool(),
            q.value(7).toString(),
            q.value(8).toString()};
}

static const char* kCols =
    "id, name, description, config_json, category, is_default, is_active, created_at, updated_at";

Result<QVector<AgentConfig>> AgentConfigRepository::list_all() {
    return query_list(QString("SELECT %1 FROM agent_configs ORDER BY name").arg(kCols), {}, map_row);
}

Result<QVector<AgentConfig>> AgentConfigRepository::list_by_category(const QString& category) {
    return query_list(QString("SELECT %1 FROM agent_configs WHERE category = ? ORDER BY name").arg(kCols), {category},
                      map_row);
}

Result<AgentConfig> AgentConfigRepository::get(const QString& id) {
    return query_one(QString("SELECT %1 FROM agent_configs WHERE id = ?").arg(kCols), {id}, map_row);
}

Result<AgentConfig> AgentConfigRepository::get_active() {
    return query_one(QString("SELECT %1 FROM agent_configs WHERE is_active = 1 LIMIT 1").arg(kCols), {}, map_row);
}

Result<void> AgentConfigRepository::save(const AgentConfig& c) {
    auto r =
        exec_write("INSERT OR REPLACE INTO agent_configs "
                   "(id, name, description, config_json, category, is_default, is_active, updated_at) "
                   "VALUES (?, ?, ?, ?, ?, ?, ?, datetime('now'))",
                   {c.id, c.name, c.description, agentcfg_strip_model_api_key(c.config_json), c.category,
                    c.is_default ? 1 : 0, c.is_active ? 1 : 0});
    if (r.is_ok())
        SyncOutbox::record_unique("agent_config", c.id, "upsert");
    return r;
}

Result<void> AgentConfigRepository::remove(const QString& id) {
    auto r = exec_write("DELETE FROM agent_configs WHERE id = ?", {id});
    if (r.is_ok())
        SyncOutbox::record("agent_config", id, "delete");
    return r;
}

Result<void> AgentConfigRepository::set_active(const QString& id) {
    // Single atomic statement (see LlmConfigRepository::set_active): no window with
    // zero active agents, and an unknown id leaves the current active agent alone.
    auto r = exec_write("UPDATE agent_configs SET is_active = CASE WHEN id = ? THEN 1 ELSE 0 END, "
                        "updated_at = CASE WHEN id = ? THEN datetime('now') ELSE updated_at END "
                        "WHERE EXISTS (SELECT 1 FROM agent_configs WHERE id = ?)",
                        {id, id, id});
    if (r.is_ok())
        SyncOutbox::record_unique("agent_config", id, "activate");
    return r;
}

} // namespace fincept
