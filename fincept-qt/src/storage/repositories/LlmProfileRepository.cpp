// LlmProfileRepository.cpp

#include "storage/repositories/LlmProfileRepository.h"

#include "core/logging/Logger.h"
#include "storage/repositories/LlmConfigRepository.h"
#include "storage/secure/SecureStorage.h"

#include <QSqlQuery>
#include <QUuid>

namespace fincept {

namespace {
// Encrypt the profile API key at rest in SecureStorage; reads fall back to the
// plaintext column so legacy rows keep working and no key is lost.
QString profile_secret_key(const QString& id) {
    return QStringLiteral("llm:apikey:profile:") + id;
}
} // namespace

// ── Singleton ────────────────────────────────────────────────────────────────

LlmProfileRepository& LlmProfileRepository::instance() {
    static LlmProfileRepository s;
    return s;
}

// ── Row mapper ────────────────────────────────────────────────────────────────

LlmProfile LlmProfileRepository::map_profile(QSqlQuery& q) {
    LlmProfile p;
    p.id = q.value(0).toString();
    p.name = q.value(1).toString();
    p.provider = q.value(2).toString();
    p.model_id = q.value(3).toString();
    p.api_key = q.value(4).toString();
    p.base_url = q.value(5).toString();
    p.temperature = q.value(6).toDouble();
    p.max_tokens = q.value(7).toInt();
    p.system_prompt = q.value(8).toString();
    p.is_default = q.value(9).toBool();
    p.created_at = q.value(10).toString();
    p.updated_at = q.value(11).toString();
    p.reasoning_effort = q.value(12).toString(); // v061
    if (p.is_harness())
        return p; // a harness profile never carries a key (nothing to decrypt)
    // Decrypt the api_key from SecureStorage when the column is blank (encrypted
    // row); a non-empty column is a legacy plaintext value, used as-is.
    if (p.api_key.isEmpty() && !p.id.isEmpty()) {
        // Error ≠ "nothing stored": the key stays empty for this load only; save()
        // stores a key only when one is given, so an unread key is never overwritten.
        const SecretRead sr = SecureStorage::instance().read(profile_secret_key(p.id));
        if (sr.found())
            p.api_key = sr.value;
    }
    return p;
}

static const char* kSelectCols = "SELECT id, name, provider, model_id, api_key, base_url, "
                                 "       temperature, max_tokens, system_prompt, is_default, "
                                 "       created_at, updated_at, reasoning_effort "
                                 "FROM llm_profiles ";

// ── Profile CRUD ──────────────────────────────────────────────────────────────

Result<QVector<LlmProfile>> LlmProfileRepository::list_profiles() const {
    // Harness profiles serve terminal AI only (ModelRouter); the agent runtime,
    // node editors and AI Quant Lab, which list profiles here, cannot use them.
    return query_list(QString(kSelectCols) + "WHERE lower(provider) NOT LIKE 'harness:%' ORDER BY name", {},
                      map_profile);
}

Result<QVector<LlmProfile>> LlmProfileRepository::list_all_profiles() const {
    return query_list(QString(kSelectCols) + "ORDER BY name", {}, map_profile);
}

Result<QVector<LlmProfileAssignment>> LlmProfileRepository::list_assignments() const {
    auto r = db().execute("SELECT context_type, context_id, profile_id FROM llm_profile_assignments", {});
    if (r.is_err())
        return Result<QVector<LlmProfileAssignment>>::err(r.error());
    QVector<LlmProfileAssignment> out;
    auto& q = r.value();
    while (q.next()) {
        LlmProfileAssignment a;
        a.context_type = q.value(0).toString();
        a.context_id = q.value(1).isNull() ? QString() : q.value(1).toString();
        a.profile_id = q.value(2).toString();
        out.append(a);
    }
    return Result<QVector<LlmProfileAssignment>>::ok(out);
}

Result<LlmProfile> LlmProfileRepository::get_profile(const QString& id) const {
    return query_one(QString(kSelectCols) + "WHERE id = ?", {id}, map_profile);
}

Result<void> LlmProfileRepository::save_profile(const LlmProfile& p) {
    // Generate id if empty (new profile)
    QString id = p.id.isEmpty() ? QUuid::createUuid().toString(QUuid::WithoutBraces) : p.id;

    // A harness profile never stores a key, never a base URL, and is never the
    // global default (that one also serves the agent runtime).
    const bool harness = p.is_harness();
    const QString api_key = harness ? QString() : p.api_key;
    // Encrypt the api_key; blank the column only once the encrypted copy was
    // written and read back equal — otherwise the plaintext stays (never lost).
    QString column_key = api_key;
    if (!api_key.isEmpty() && SecureStorage::instance().store_verified(profile_secret_key(id), api_key).is_ok())
        column_key.clear();
    if (harness)
        SecureStorage::instance().remove(profile_secret_key(id)); // a provider switched to a harness drops its key

    return exec_write("INSERT OR REPLACE INTO llm_profiles "
                      "  (id, name, provider, model_id, api_key, base_url, "
                      "   temperature, max_tokens, system_prompt, is_default, reasoning_effort, updated_at) "
                      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, datetime('now'))",
                      {id, p.name, p.provider, p.model_id, column_key, harness ? QString() : p.base_url,
                       p.temperature, p.max_tokens, p.system_prompt, (p.is_default && !harness) ? 1 : 0,
                       p.reasoning_effort.trimmed()});
}

Result<void> LlmProfileRepository::delete_profile(const QString& id) {
    SecureStorage::instance().remove(profile_secret_key(id)); // drop the encrypted key too
    // Assignments cascade-delete via FK ON DELETE CASCADE
    return exec_write("DELETE FROM llm_profiles WHERE id = ?", {id});
}

Result<void> LlmProfileRepository::set_default(const QString& id) {
    {
        const auto target = get_profile(id);
        if (target.is_ok() && target.value().is_harness())
            return Result<void>::err("A harness profile cannot be the default profile (the default also serves "
                                     "Agent Studio, which cannot use a harness yet)");
    }
    // Clear all defaults then set the target. idx_llm_profiles_default is a partial
    // UNIQUE index on is_default = 1, so this cannot be one CASE-style UPDATE (the
    // new default would collide with the old one mid-statement) — use a transaction
    // so a failure or an unknown id cannot leave the table with no default. If the
    // caller is already inside a transaction BEGIN fails and we run in theirs.
    const bool own_tx = db().begin_transaction().is_ok();
    auto finish = [&](const Result<void>& r) -> Result<void> {
        if (own_tx) {
            if (r.is_ok()) {
                auto c = db().commit();
                if (c.is_err()) {
                    db().rollback();
                    return c;
                }
            } else {
                db().rollback();
            }
        }
        return r;
    };

    auto r = exec_write("UPDATE llm_profiles SET is_default = 0", {});
    if (r.is_err())
        return finish(r);
    auto set = db().execute("UPDATE llm_profiles SET is_default = 1 WHERE id = ?", {id});
    if (set.is_err())
        return finish(Result<void>::err(set.error()));
    if (set.value().numRowsAffected() == 0)
        return finish(Result<void>::err("Profile not found"));
    return finish(Result<void>::ok());
}

// ── Assignment CRUD ───────────────────────────────────────────────────────────

Result<void> LlmProfileRepository::assign_profile(const QString& context_type, const QString& context_id,
                                                  const QString& profile_id) {
    QVariant ctx_id = context_id.isEmpty() ? QVariant(QMetaType(QMetaType::QString)) : QVariant(context_id);
    return exec_write("INSERT OR REPLACE INTO llm_profile_assignments "
                      "  (context_type, context_id, profile_id, updated_at) "
                      "VALUES (?, ?, ?, datetime('now'))",
                      {context_type, ctx_id, profile_id});
}

Result<void> LlmProfileRepository::remove_assignment(const QString& context_type, const QString& context_id) {
    if (context_id.isEmpty()) {
        return exec_write("DELETE FROM llm_profile_assignments "
                          "WHERE context_type = ? AND context_id IS NULL",
                          {context_type});
    }
    return exec_write("DELETE FROM llm_profile_assignments "
                      "WHERE context_type = ? AND context_id = ?",
                      {context_type, context_id});
}

QString LlmProfileRepository::get_assignment(const QString& context_type, const QString& context_id) const {
    QVariant ctx_id = context_id.isEmpty() ? QVariant(QMetaType(QMetaType::QString)) : QVariant(context_id);
    QString sql = context_id.isEmpty() ? "SELECT profile_id FROM llm_profile_assignments "
                                         "WHERE context_type = ? AND context_id IS NULL LIMIT 1"
                                       : "SELECT profile_id FROM llm_profile_assignments "
                                         "WHERE context_type = ? AND context_id = ? LIMIT 1";

    QVariantList params = context_id.isEmpty() ? QVariantList{context_type} : QVariantList{context_type, context_id};

    auto r = db().execute(sql, params);
    if (r.is_err())
        return {};
    auto& q = r.value();
    if (!q.next())
        return {};
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
    r.profile_id = p.id;
    r.profile_name = p.name;
    r.provider = p.provider;
    r.model_id = p.model_id;
    r.api_key = p.api_key;
    r.base_url = p.base_url;
    r.temperature = p.temperature;
    r.max_tokens = p.max_tokens;
    r.system_prompt = p.system_prompt;
    return r;
}

ResolvedLlmProfile LlmProfileRepository::legacy_fallback() const {
    // Read the active provider via LlmConfigRepository (which decrypts the
    // api_key from SecureStorage) rather than a raw SELECT on llm_configs —
    // that column is now blanked for encrypted rows.
    auto cr = LlmConfigRepository::instance().get_active_provider();
    if (cr.is_err())
        return {};
    const LlmConfig c = cr.value();
    if (c.provider.isEmpty())
        return {};

    ResolvedLlmProfile rp;
    rp.profile_id = {};
    rp.profile_name = "Legacy Active";
    rp.provider = c.provider;
    rp.api_key = c.api_key;
    rp.base_url = c.base_url;
    rp.model_id = c.model;

    // Pull global temperature/max_tokens
    auto gs = db().execute("SELECT temperature, max_tokens FROM llm_global_settings WHERE id = 1");
    if (gs.is_ok()) {
        auto& gq = gs.value();
        if (gq.next()) {
            rp.temperature = gq.value(0).toDouble();
            rp.max_tokens = gq.value(1).toInt();
        }
    }
    if (rp.max_tokens == 0)
        rp.max_tokens = 4096;
    if (rp.temperature == 0.0)
        rp.temperature = 0.7;
    return rp;
}

ResolvedLlmProfile LlmProfileRepository::resolve_for_context(const QString& context_type,
                                                             const QString& context_id) const {
    // A harness profile serves terminal AI only (ModelRouter): for the agent
    // runtime an assignment to one is passed over and the next level answers.
    // ── Level 1: entity-specific assignment ──────────────────────────────────
    if (!context_id.isEmpty()) {
        QString pid = get_assignment(context_type, context_id);
        if (!pid.isEmpty()) {
            auto pr = get_profile(pid);
            if (pr.is_ok() && !pr.value().is_harness()) {
                LOG_DEBUG("LlmProfileRepo",
                          QString("Resolved [%1/%2] → profile '%3'").arg(context_type, context_id, pr.value().name));
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
            if (pr.is_ok() && !pr.value().is_harness()) {
                LOG_DEBUG("LlmProfileRepo",
                          QString("Resolved [%1] via type-default → profile '%2'").arg(context_type, pr.value().name));
                return profile_to_resolved(pr.value());
            }
        }
    }

    // ── Level 3: global default profile ──────────────────────────────────────
    {
        auto r = db().execute(QString(kSelectCols) +
                              "WHERE is_default = 1 AND lower(provider) NOT LIKE 'harness:%' LIMIT 1");
        if (r.is_ok()) {
            auto& q = r.value();
            if (q.next()) {
                LlmProfile p = map_profile(q);
                LOG_DEBUG("LlmProfileRepo",
                          QString("Resolved [%1] via global default → profile '%2'").arg(context_type, p.name));
                return profile_to_resolved(p);
            }
        }
    }

    // ── Level 4: legacy fallback ──────────────────────────────────────────────
    LOG_DEBUG("LlmProfileRepo", QString("Resolved [%1] via legacy active provider fallback").arg(context_type));
    return legacy_fallback();
}

} // namespace fincept
