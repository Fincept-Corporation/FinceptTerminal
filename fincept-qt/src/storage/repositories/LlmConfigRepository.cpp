#include "storage/repositories/LlmConfigRepository.h"

namespace fincept {

LlmConfigRepository& LlmConfigRepository::instance() {
    static LlmConfigRepository s;
    return s;
}

LlmConfig LlmConfigRepository::map_config(QSqlQuery& q) {
    return {q.value(0).toString(), q.value(1).toString(), q.value(2).toString(), q.value(3).toString(),
            q.value(4).toBool(),   q.value(5).toBool(),   q.value(6).toString(), q.value(7).toString()};
}

LlmModelConfig LlmConfigRepository::map_model(QSqlQuery& q) {
    return {q.value(0).toString(), q.value(1).toString(), q.value(2).toString(), q.value(3).toString(),
            q.value(4).toString(), q.value(5).toString(), q.value(6).toBool(),   q.value(7).toBool(),
            q.value(8).toString(), q.value(9).toString()};
}

Result<QVector<LlmConfig>> LlmConfigRepository::list_providers() {
    return query_list("SELECT provider, api_key, base_url, model, is_active, tools_enabled, created_at, updated_at "
                      "FROM llm_configs ORDER BY provider",
                      {}, map_config);
}

Result<LlmConfig> LlmConfigRepository::get_active_provider() {
    return query_one("SELECT provider, api_key, base_url, model, is_active, tools_enabled, created_at, updated_at "
                     "FROM llm_configs WHERE is_active = 1 LIMIT 1",
                     {}, map_config);
}

Result<void> LlmConfigRepository::save_provider(const LlmConfig& c) {
    return exec_write(
        "INSERT OR REPLACE INTO llm_configs (provider, api_key, base_url, model, is_active, tools_enabled, updated_at) "
        "VALUES (?, ?, ?, ?, ?, ?, datetime('now'))",
        {c.provider, c.api_key, c.base_url, c.model, c.is_active ? 1 : 0, c.tools_enabled ? 1 : 0});
}

Result<void> LlmConfigRepository::set_active(const QString& provider) {
    exec_write("UPDATE llm_configs SET is_active = 0", {});
    return exec_write("UPDATE llm_configs SET is_active = 1 WHERE provider = ?", {provider});
}

Result<void> LlmConfigRepository::delete_provider(const QString& provider) {
    return exec_write("DELETE FROM llm_configs WHERE provider = ?", {provider});
}

Result<LlmGlobalSettings> LlmConfigRepository::get_global_settings() {
    auto r = db().execute("SELECT temperature, max_tokens, system_prompt FROM llm_global_settings WHERE id = 1");
    if (r.is_err())
        return Result<LlmGlobalSettings>::err(r.error());
    auto& q = r.value();
    if (!q.next())
        return Result<LlmGlobalSettings>::ok(LlmGlobalSettings{});
    return Result<LlmGlobalSettings>::ok({q.value(0).toDouble(), q.value(1).toInt(), q.value(2).toString()});
}

Result<void> LlmConfigRepository::save_global_settings(const LlmGlobalSettings& s) {
    return exec_write("INSERT OR REPLACE INTO llm_global_settings (id, temperature, max_tokens, system_prompt) "
                      "VALUES (1, ?, ?, ?)",
                      {s.temperature, s.max_tokens, s.system_prompt});
}

Result<QVector<LlmModelConfig>> LlmConfigRepository::list_models(const QString& provider) {
    if (provider.isEmpty()) {
        auto r = db().execute(
            "SELECT id, provider, model_id, display_name, api_key, base_url, "
            "is_enabled, is_default, created_at, updated_at FROM llm_model_configs ORDER BY provider, display_name");
        if (r.is_err())
            return Result<QVector<LlmModelConfig>>::err(r.error());
        QVector<LlmModelConfig> result;
        auto& q = r.value();
        while (q.next())
            result.append(map_model(q));
        return Result<QVector<LlmModelConfig>>::ok(std::move(result));
    }
    auto r = db().execute("SELECT id, provider, model_id, display_name, api_key, base_url, "
                          "is_enabled, is_default, created_at, updated_at FROM llm_model_configs "
                          "WHERE provider = ? ORDER BY display_name",
                          {provider});
    if (r.is_err())
        return Result<QVector<LlmModelConfig>>::err(r.error());
    QVector<LlmModelConfig> result;
    auto& q = r.value();
    while (q.next())
        result.append(map_model(q));
    return Result<QVector<LlmModelConfig>>::ok(std::move(result));
}

Result<void> LlmConfigRepository::save_model(const LlmModelConfig& m) {
    return exec_write("INSERT OR REPLACE INTO llm_model_configs "
                      "(id, provider, model_id, display_name, api_key, base_url, is_enabled, is_default, updated_at) "
                      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, datetime('now'))",
                      {m.id, m.provider, m.model_id, m.display_name, m.api_key, m.base_url, m.is_enabled ? 1 : 0,
                       m.is_default ? 1 : 0});
}

Result<void> LlmConfigRepository::delete_model(const QString& id) {
    return exec_write("DELETE FROM llm_model_configs WHERE id = ?", {id});
}

} // namespace fincept
