// LlmProfileRepository.cpp

#include "storage/repositories/LlmProfileRepository.h"

#include "core/logging/Logger.h"

#include <QSqlQuery>
#include <QUuid>

namespace fincept {

// ── Singleton ────────────────────────────────────────────────────────────────

LlmProfileRepository& LlmProfileRepository::instance() {
    static LlmProfileRepository s;
    return s;
}

// ── Row mapper ────────────────────────────────────────────────────────────────

LlmProfile LlmProfileRepository::map_profile(QSqlQuery& q) {
    LlmProfile p;
    p.id            = q.value(0).toString();
    p.name          = q.value(1).toString();
    p.provider      = q.value(2).toString();
    p.model_id      = q.value(3).toString();
    p.api_key       = q.value(4).toString();
    p.base_url      = q.value(5).toString();
    p.temperature   = q.value(6).toDouble();
    p.max_tokens    = q.value(7).toInt();
    p.system_prompt = q.value(8).toString();
    p.is_default    = q.value(9).toBool();
    p.created_at    = q.value(10).toString();
    p.updated_at    = q.value(11).toString();
    return p;
}

static const char* kSelectCols =
    "SELECT id, name, provider, model_id, api_key, base_url, "
    "       temperature, max_tokens, system_prompt, is_default, "
    "       created_at, updated_at "
    "FROM llm_profiles ";

// ── Profile CRUD ──────────────────────────────────────────────────────────────

Result<QVector<LlmProfile>> LlmProfileRepository::list_profiles() const {
    return query_list(QString(kSelectCols) + "ORDER BY name", {}, map_profile);
}

Result<LlmProfile> LlmProfileRepository::get_profile(const QString& id) const {
    return query_one(QString(kSelectCols) + "WHERE id = ?", {id}, map_profile);
}

Result<void> LlmProfileRepository::save_profile(const LlmProfile& p) {
    // Generate id if empty (new profile)
    QString id = p.id.isEmpty()
                 ? QUuid::createUuid().toString(QUuid::WithoutBraces)
                 : p.id;

    return exec_write(
        "INSERT OR REPLACE INTO llm_profiles "
        "  (id, name, provider, model_id, api_key, base_url, "
        "   temperature, max_tokens, system_prompt, is_default, updated_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, datetime('now'))",
        {id, p.name, p.provider, p.model_id, p.api_key, p.base_url,
         p.temperature, p.max_tokens, p.system_prompt, p.is_default ? 1 : 0});
}

Result<void> LlmProfileRepository::delete_profile(const QString& id) {
    // Assignments cascade-delete via FK ON DELETE CASCADE
    return exec_write("DELETE FROM llm_profiles WHERE id = ?", {id});
}

Result<void> LlmProfileRepository::set_default(const QString& id) {
    // Clear all defaults then set the target — two writes, still atomic because
    // callers should wrap in a transaction if needed; for UI use this is fine.
    auto r = exec_write("UPDATE llm_profiles SET is_default = 0", {});
    if (r.is_err()) return r;
    return exec_write("UPDATE llm_profiles SET is_default = 1 WHERE id = ?", {id});
}

// ── Assignment CRUD ───────────────────────────────────────────────────────────

Result<void> LlmProfileRepository::assign_profile(const QString& context_type,
                                                   const QString& context_id,
                                                   const QString& profile_id) {
    QVariant ctx_id = context_id.isEmpty() ? QVariant(QMetaType(QMetaType::QString)) : QVariant(context_id);
    return exec_write(
        "INSERT OR REPLACE INTO llm_profile_assignments "
        "  (context_type, context_id, profile_id, updated_at) "
        "VALUES (?, ?, ?, datetime('now'))",
        {context_type, ctx_id, profile_id});
}

Result<void> LlmProfileRepository::remove_assignment(const QString& context_type,
                                                      const QString& context_id) {
    if (context_id.isEmpty()) {
        return exec_write(
            "DELETE FROM llm_profile_assignments "
            "WHERE context_type = ? AND context_id IS NULL",
            {context_type});
    }
    return exec_write(
        "DELETE FROM llm_profile_assignments "
        "WHERE context_type = ? AND context_id = ?",
        {context_type, context_id});
}

QString LlmProfileRepository::get_assignment(const QString& context_type,
                                              const QString& context_id) const {
    QVariant ctx_id = context_id.isEmpty() ? QVariant(QMetaType(QMetaType::QString)) : QVariant(context_id);
    QString sql = context_id.isEmpty()
        ? "SELECT profile_id FROM llm_profile_assignments "
          "WHERE context_type = ? AND context_id IS NULL LIMIT 1"
        : "SELECT profile_id FROM llm_profile_assignments "
          "WHERE context_type = ? AND context_id = ? LIMIT 1";

    QVariantList params = context_id.isEmpty()
        ? QVariantList{context_type}
        : QVariantList{context_type, context_id};

    auto r = db().execute(sql, params);
    if (r.is_err()) return {};
    auto& q = r.value();
    if (!q.next()) return {};
    return q.value(0).toString();
}

// ── Resolution ────────────────────────────────────────────────────────────────

// Maps a context_type to its type-level default key.
// e.g. "agent" → "agent_default", "team_coordinator" → "team_default"
QString LlmProfileRepository::type_default_key(const QString& context_type) {
    if (context_type == "agent" || context_type == "agent_default")
        return "agent_default";
    if (context_type == "team" || context_type == "team_coordinator" || context_type == "team_default")
        return "team_default";
    if (context_type == "ai_chat")
        return "ai_chat";
    return context_type + "_default";
}

ResolvedLlmProfile LlmProfileRepository::profile_to_resolved(const LlmProfile& p) const {
    ResolvedLlmProfile r;
    r.profile_id    = p.id;
    r.profile_name  = p.name;
    r.provider      = p.provider;
    r.model_id      = p.model_id;
    r.api_key       = p.api_key;
    r.base_url      = p.base_url;
    r.temperature   = p.temperature;
    r.max_tokens    = p.max_tokens;
    r.system_prompt = p.system_prompt;
    return r;
}

ResolvedLlmProfile LlmProfileRepository::legacy_fallback() const {
    // Read from llm_configs (old table) as last resort
    auto r = db().execute(
        "SELECT provider, api_key, base_url, model FROM llm_configs WHERE is_active = 1 LIMIT 1");
    if (r.is_err()) return {};
    auto& q = r.value();
    if (!q.next()) return {};

    ResolvedLlmProfile rp;
    rp.profile_id   = {};
    rp.profile_name = "Legacy Active";
    rp.provider     = q.value(0).toString();
    rp.api_key      = q.value(1).toString();
    rp.base_url     = q.value(2).toString();
    rp.model_id     = q.value(3).toString();

    // Pull global temperature/max_tokens
    auto gs = db().execute(
        "SELECT temperature, max_tokens FROM llm_global_settings WHERE id = 1");
    if (gs.is_ok()) {
        auto& gq = gs.value();
        if (gq.next()) {
            rp.temperature = gq.value(0).toDouble();
            rp.max_tokens  = gq.value(1).toInt();
        }
    }
    if (rp.max_tokens == 0)
        rp.max_tokens = 4096;
    if (rp.temperature == 0.0)
        rp.temperature = 0.7;
    return rp;
}

ResolvedLlmProfile LlmProfileRepository::resolve_for_context(
    const QString& context_type, const QString& context_id) const
{
    // ── Level 1: entity-specific assignment ──────────────────────────────────
    if (!context_id.isEmpty()) {
        QString pid = get_assignment(context_type, context_id);
        if (!pid.isEmpty()) {
            auto pr = get_profile(pid);
            if (pr.is_ok()) {
                LOG_DEBUG("LlmProfileRepo",
                    QString("Resolved [%1/%2] → profile '%3'")
                        .arg(context_type, context_id, pr.value().name));
                return profile_to_resolved(pr.value());
            }
        }
    }

    // ── Level 2: type-level default assignment ────────────────────────────────
    {
        QString type_key = type_default_key(context_type);
        QString pid = get_assignment(type_key, {});
        if (!pid.isEmpty()) {
            auto pr = get_profile(pid);
            if (pr.is_ok()) {
                LOG_DEBUG("LlmProfileRepo",
                    QString("Resolved [%1] via type-default → profile '%2'")
                        .arg(context_type, pr.value().name));
                return profile_to_resolved(pr.value());
            }
        }
    }

    // ── Level 3: global default profile ──────────────────────────────────────
    {
        auto r = db().execute(
            "SELECT id, name, provider, model_id, api_key, base_url, "
            "       temperature, max_tokens, system_prompt, is_default, "
            "       created_at, updated_at "
            "FROM llm_profiles WHERE is_default = 1 LIMIT 1");
        if (r.is_ok()) {
            auto& q = r.value();
            if (q.next()) {
                LlmProfile p = map_profile(q);
                LOG_DEBUG("LlmProfileRepo",
                    QString("Resolved [%1] via global default → profile '%2'")
                        .arg(context_type, p.name));
                return profile_to_resolved(p);
            }
        }
    }

    // ── Level 4: legacy fallback ──────────────────────────────────────────────
    LOG_DEBUG("LlmProfileRepo",
        QString("Resolved [%1] via legacy active provider fallback").arg(context_type));
    return legacy_fallback();
}

} // namespace fincept
