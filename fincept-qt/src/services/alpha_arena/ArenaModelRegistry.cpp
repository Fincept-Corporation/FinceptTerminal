#include "services/alpha_arena/ArenaModelRegistry.h"
#include "core/logging/Logger.h"
#include "services/llm/LlmService.h"
#include "services/llm/ProviderCatalog.h"
#include "storage/repositories/LlmConfigRepository.h"
#include "storage/repositories/LlmProfileRepository.h"
#include <QSet>

namespace fincept::arena {
using fincept::ai_chat::ProviderCatalog;

namespace {
// A failed settings read must not look like "no models configured".
void warn_layer_err(const char* layer, const std::string& err) {
    LOG_WARN("ArenaModelRegistry",
             QStringLiteral("%1 read failed: %2").arg(QLatin1String(layer), QString::fromStdString(err)));
}
} // namespace

ArenaModelRegistry& ArenaModelRegistry::instance() { static ArenaModelRegistry r; return r; }

ArenaModelRegistry::ArenaModelRegistry() {
    connect(&fincept::ai_chat::LlmService::instance(), &fincept::ai_chat::LlmService::config_changed,
            this, &ArenaModelRegistry::models_changed);
}

QVector<ArenaModelOption> ArenaModelRegistry::available_models() const {
    QVector<ArenaModelOption> out;
    QSet<QString> seen;
    QSet<QString> providers_present;   // providers that contributed in layers 1-3
    auto add = [&](ArenaModelOption o) {
        const QString key = o.provider.toLower() + "/" + o.model_id;
        if (o.model_id.isEmpty() || seen.contains(key)) return;
        seen.insert(key);
        if (o.color_hex.isEmpty()) o.color_hex = ProviderCatalog::brand_color(o.provider);
        out.append(std::move(o));
    };

    // 1. Profiles (highest priority — explicit user intent).
    if (auto r = LlmProfileRepository::instance().list_profiles(); r.is_err())
        warn_layer_err("profiles", r.error());
    else
        for (const auto& p : r.value()) {
            providers_present.insert(p.provider.toLower());
            ArenaModelOption o;
            o.provider = p.provider; o.model_id = p.model_id;
            o.display_name = p.name + " (" + p.model_id + ")";
            o.source_kind = "profile"; o.source_ref = p.id;
            o.ready = !p.api_key.isEmpty() || !ProviderCatalog::requires_api_key(p.provider);
            add(o);
        }
    // 2. Custom model configs.
    if (auto r = LlmConfigRepository::instance().list_models(); r.is_err())
        warn_layer_err("model configs", r.error());
    else
        for (const auto& m : r.value()) {
            if (!m.is_enabled) continue;
            providers_present.insert(m.provider.toLower());
            ArenaModelOption o;
            o.provider = m.provider; o.model_id = m.model_id;
            o.display_name = m.display_name.isEmpty() ? m.model_id : m.display_name;
            o.source_kind = "model"; o.source_ref = m.id;
            o.ready = !m.api_key.isEmpty() || !ProviderCatalog::requires_api_key(m.provider);
            add(o);
        }
    // 3. Provider rows — configured model + catalog fallbacks. NOT filtered on
    //    is_active (that marks the chat-active provider, not validity).
    if (auto r = LlmConfigRepository::instance().list_providers(); r.is_err())
        warn_layer_err("providers", r.error());
    else
        for (const auto& c : r.value()) {
            providers_present.insert(c.provider.toLower());
            const bool ready = !c.api_key.isEmpty() || !ProviderCatalog::requires_api_key(c.provider);
            QStringList models = ProviderCatalog::fallback_models(c.provider);
            if (!c.model.isEmpty() && !models.contains(c.model)) models.prepend(c.model);
            for (const QString& mid : models) {
                ArenaModelOption o;
                o.provider = c.provider; o.model_id = mid;
                o.display_name = ProviderCatalog::display_name(c.provider) + " / " + mid;
                o.source_kind = "provider"; o.source_ref = c.provider;
                o.ready = ready;
                add(o);
            }
        }
    // 4. Catalog browse layer — every known provider is selectable in the
    //    wizard even without an llm_configs row yet. Skipped for providers that
    //    already contributed above (their rows carry real key/model state).
    //    ready only for keyless providers (ollama/fincept); ollama's fallback
    //    list is intentionally empty (live /api/tags), so it may add nothing.
    for (const QString& p : ProviderCatalog::known_providers()) {
        if (providers_present.contains(p.toLower())) continue;
        const bool ready = !ProviderCatalog::requires_api_key(p);
        for (const QString& mid : ProviderCatalog::fallback_models(p)) {
            ArenaModelOption o;
            o.provider = p; o.model_id = mid;
            o.display_name = ProviderCatalog::display_name(p) + " / " + mid;
            o.source_kind = "provider"; o.source_ref = p;
            o.ready = ready;
            add(o);
        }
    }
    return out;
}

Result<ArenaCredentials> ArenaModelRegistry::resolve_credentials(const QString& source_kind,
                                                                 const QString& source_ref,
                                                                 const QString& provider) const {
    ArenaCredentials cred;
    if (source_kind == QLatin1String("profile")) {
        if (auto r = LlmProfileRepository::instance().list_profiles(); r.is_ok())
            for (const auto& p : r.value())
                if (p.id == source_ref) { cred.api_key = p.api_key; cred.base_url = p.base_url; break; }
    } else if (source_kind == QLatin1String("model")) {
        if (auto r = LlmConfigRepository::instance().list_models(); r.is_ok())
            for (const auto& m : r.value())
                if (m.id == source_ref) { cred.api_key = m.api_key; cred.base_url = m.base_url; break; }
    } else {   // "provider"
        if (auto r = LlmConfigRepository::instance().list_providers(); r.is_ok())
            for (const auto& c : r.value())
                if (c.provider == provider) { cred.api_key = c.api_key; cred.base_url = c.base_url; break; }
    }
    if (cred.base_url.isEmpty()) cred.base_url = ProviderCatalog::default_base_url(provider);
    if (cred.api_key.isEmpty() && ProviderCatalog::requires_api_key(provider))
        return Result<ArenaCredentials>::err("no API key configured for " + provider.toStdString());
    return Result<ArenaCredentials>::ok(cred);
}

} // namespace fincept::arena
