#include "storage/repositories/LlmConfigRepository.h"

#include "storage/secure/SecureStorage.h"

namespace fincept {

namespace {
// Encrypt LLM API keys at rest: keep them in SecureStorage (AES-256-GCM,
// machine-bound key) instead of the plaintext SQL column. Reads fall back to
// the column so pre-existing plaintext rows keep working and no key is ever
// lost; writes move the key into SecureStorage and blank the column (only when
// the encrypted store succeeds — otherwise the plaintext value is kept).
QString provider_secret_key(const QString& provider) {
    return QStringLiteral("llm:apikey:provider:") + provider;
}
QString model_secret_key(const QString& id) {
    return QStringLiteral("llm:apikey:model:") + id;
}

void hydrate_provider_key(LlmConfig& c) {
    if (!c.api_key.isEmpty())
        return; // legacy plaintext already present in the column
    auto sr = SecureStorage::instance().retrieve(provider_secret_key(c.provider));
    if (sr.is_ok())
        c.api_key = sr.value();
}

void hydrate_model_key(LlmModelConfig& m) {
    if (!m.api_key.isEmpty())
        return;
    auto sr = SecureStorage::instance().retrieve(model_secret_key(m.id));
    if (sr.is_ok())
        m.api_key = sr.value();
}
} // namespace

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
    auto r = query_list("SELECT provider, api_key, base_url, model, is_active, tools_enabled, created_at, updated_at "
                        "FROM llm_configs ORDER BY provider",
                        {}, map_config);
    if (r.is_err())
        return r;
    QVector<LlmConfig> out = r.value();
    for (auto& c : out)
        hydrate_provider_key(c);
    return Result<QVector<LlmConfig>>::ok(std::move(out));
}

Result<LlmConfig> LlmConfigRepository::get_active_provider() {
    auto r = query_one("SELECT provider, api_key, base_url, model, is_active, tools_enabled, created_at, updated_at "
                       "FROM llm_configs WHERE is_active = 1 LIMIT 1",
                       {}, map_config);
    if (r.is_ok()) {
        LlmConfig c = r.value();
        hydrate_provider_key(c);
        return Result<LlmConfig>::ok(std::move(c));
    }
    return r;
}

Result<void> LlmConfigRepository::save_provider(const LlmConfig& c) {
    // Move the API key into SecureStorage; blank the plaintext column only if the
    // encrypted store succeeded, so a SecureStorage failure never loses the key.
    QString column_key = c.api_key;
    if (!c.api_key.isEmpty() && SecureStorage::instance().store(provider_secret_key(c.provider), c.api_key).is_ok())
        column_key.clear();
    return exec_write(
        "INSERT OR REPLACE INTO llm_configs (provider, api_key, base_url, model, is_active, tools_enabled, updated_at) "
        "VALUES (?, ?, ?, ?, ?, ?, datetime('now'))",
        {c.provider, column_key, c.base_url, c.model, c.is_active ? 1 : 0, c.tools_enabled ? 1 : 0});
}

Result<void> LlmConfigRepository::set_active(const QString& provider) {
    exec_write("UPDATE llm_configs SET is_active = 0", {});
    return exec_write("UPDATE llm_configs SET is_active = 1 WHERE provider = ?", {provider});
}

Result<void> LlmConfigRepository::delete_provider(const QString& provider) {
    SecureStorage::instance().remove(provider_secret_key(provider)); // drop the encrypted key too
    return exec_write("DELETE FROM llm_configs WHERE provider = ?", {provider});
}

Result<LlmGlobalSettings> LlmConfigRepository::get_global_settings() {
    auto r = db().execute("SELECT temperature, max_tokens, system_prompt, max_tool_rounds "
                          "FROM llm_global_settings WHERE id = 1");
    if (r.is_err())
        return Result<LlmGlobalSettings>::err(r.error());
    auto& q = r.value();
    if (!q.next())
        return Result<LlmGlobalSettings>::ok(LlmGlobalSettings{});
    LlmGlobalSettings gs;
    gs.temperature = q.value(0).toDouble();
    gs.max_tokens = q.value(1).toInt();
    gs.system_prompt = q.value(2).toString();
    // NULL on rows written before v030 — keep struct default (40).
    const QVariant v = q.value(3);
    if (!v.isNull())
        gs.max_tool_rounds = v.toInt();
    return Result<LlmGlobalSettings>::ok(gs);
}

Result<void> LlmConfigRepository::save_global_settings(const LlmGlobalSettings& s) {
    return exec_write("INSERT OR REPLACE INTO llm_global_settings "
                      "(id, temperature, max_tokens, system_prompt, max_tool_rounds) "
                      "VALUES (1, ?, ?, ?, ?)",
                      {s.temperature, s.max_tokens, s.system_prompt, s.max_tool_rounds});
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
        while (q.next()) {
            LlmModelConfig m = map_model(q);
            hydrate_model_key(m);
            result.append(m);
        }
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
    // Same as save_provider: encrypt the per-model API key, blank the column
    // only if the encrypted store succeeded.
    QString column_key = m.api_key;
    if (!m.api_key.isEmpty() && SecureStorage::instance().store(model_secret_key(m.id), m.api_key).is_ok())
        column_key.clear();
    return exec_write("INSERT OR REPLACE INTO llm_model_configs "
                      "(id, provider, model_id, display_name, api_key, base_url, is_enabled, is_default, updated_at) "
                      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, datetime('now'))",
                      {m.id, m.provider, m.model_id, m.display_name, column_key, m.base_url, m.is_enabled ? 1 : 0,
                       m.is_default ? 1 : 0});
}

Result<void> LlmConfigRepository::delete_model(const QString& id) {
    SecureStorage::instance().remove(model_secret_key(id));
    return exec_write("DELETE FROM llm_model_configs WHERE id = ?", {id});
}

} // namespace fincept
