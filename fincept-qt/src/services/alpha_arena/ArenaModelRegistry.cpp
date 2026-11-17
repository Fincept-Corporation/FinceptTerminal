#include "services/alpha_arena/ArenaModelRegistry.h"

#include "auth/AuthManager.h"
#include "services/alpha_arena/ArenaModelReadiness.h"
#include "core/logging/Logger.h"
#include "services/harness/HarnessEligibility.h"
#include "services/harness/HarnessRegistry.h"
#include "services/harness/IHarnessAdapter.h"
#include "services/llm/LlmService.h"
#include "services/llm/ModelRouter.h"
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

namespace arena_harness_layer_detail {

/// One installed harness that can answer terminal AI, with the models to offer.
struct ArenaHarnessRow {
    ai_chat::RouterHarness state;
    QStringList models; // registry default first
};

/// The harnesses the wizard may list (GUI thread): chat-capable adapters that
/// are installed. Not-installed / still-detecting ones are left out.
QVector<ArenaHarnessRow> arena_harness_rows() {
    QVector<ArenaHarnessRow> out;
    auto& reg = harness::HarnessRegistry::instance();
    for (const ai_chat::RouterHarness& h : ai_chat::ModelRouter::instance().harness_states()) {
        const harness::IHarnessAdapter* a = reg.adapter(h.id);
        if (!a || !a->descriptor().caps.chat)
            continue;
        if (h.eligibility.state == harness::TerminalState::NotInstalled ||
            h.eligibility.state == harness::TerminalState::Detecting)
            continue;
        ArenaHarnessRow row;
        row.state = h;
        for (const harness::ModelInfo& m : reg.status(h.id).models) {
            if (!m.id.isEmpty() && !row.models.contains(m.id))
                row.models << m.id;
        }
        if (!h.default_model.isEmpty()) {
            row.models.removeAll(h.default_model);
            row.models.prepend(h.default_model);
        }
        if (!row.models.isEmpty())
            out.append(row);
    }
    return out;
}

} // namespace arena_harness_layer_detail

ArenaModelRegistry& ArenaModelRegistry::instance() {
    static ArenaModelRegistry r;
    return r;
}

ArenaModelRegistry::ArenaModelRegistry() {
    connect(&fincept::ai_chat::LlmService::instance(), &fincept::ai_chat::LlmService::config_changed, this,
            &ArenaModelRegistry::models_changed);
    // Harness sign-in / consent / model-list changes: re-list only when the
    // harness layer really changed (detection emits often; the wizard re-lists).
    harness_signature_ = harness_layer_signature();
    auto& reg = harness::HarnessRegistry::instance();
    connect(&reg, &harness::HarnessRegistry::status_changed, this, [this](const QString&) { on_harness_state_changed(); });
    connect(&reg, &harness::HarnessRegistry::defaults_changed, this, [this]() { on_harness_state_changed(); });
    connect(&ai_chat::ModelRouter::instance(), &ai_chat::ModelRouter::routes_changed, this,
            [this]() { on_harness_state_changed(); });
}

QString ArenaModelRegistry::harness_layer_signature() const {
    QStringList parts;
    for (const auto& row : arena_harness_layer_detail::arena_harness_rows()) {
        parts << row.state.id + QLatin1Char(':') + (row.state.eligibility.usable ? QLatin1Char('1') : QLatin1Char('0')) +
                     QLatin1Char(':') + row.state.eligibility.label + QLatin1Char(':') + row.models.join(QLatin1Char(','));
    }
    return parts.join(QLatin1Char('|'));
}

void ArenaModelRegistry::on_harness_state_changed() {
    const QString sig = harness_layer_signature();
    if (sig == harness_signature_)
        return;
    harness_signature_ = sig;
    emit models_changed();
}

QVector<ArenaModelOption> ArenaModelRegistry::available_models() const {
    QVector<ArenaModelOption> out;
    QSet<QString> seen;
    QSet<QString> providers_present; // providers that contributed in layers 1-3
    auto add = [&](ArenaModelOption o) {
        const QString key = o.provider.toLower() + "/" + o.model_id;
        if (o.model_id.isEmpty() || seen.contains(key))
            return;
        seen.insert(key);
        if (o.color_hex.isEmpty())
            o.color_hex = ProviderCatalog::brand_color(o.provider);
        // API layers (1-4): the only reason a row is not ready is a missing key.
        if (!o.ready && o.not_ready_reason.isEmpty() && o.source_kind != QLatin1String("harness")) {
            o.not_ready_reason = readiness::api_not_ready_text();
            o.detail = readiness::api_not_ready_detail(ProviderCatalog::display_name(o.provider));
        }
        out.append(std::move(o));
    };

    // 1. Profiles (highest priority — explicit user intent).
    if (auto r = LlmProfileRepository::instance().list_profiles(); r.is_err())
        warn_layer_err("profiles", r.error());
    else
        for (const auto& p : r.value()) {
            providers_present.insert(p.provider.toLower());
            ArenaModelOption o;
            o.provider = p.provider;
            o.model_id = p.model_id;
            o.display_name = p.name + " (" + p.model_id + ")";
            o.source_kind = "profile";
            o.source_ref = p.id;
            o.ready = !p.api_key.isEmpty() || !ProviderCatalog::requires_api_key(p.provider);
            add(o);
        }
    // 2. Custom model configs.
    if (auto r = LlmConfigRepository::instance().list_models(); r.is_err())
        warn_layer_err("model configs", r.error());
    else
        for (const auto& m : r.value()) {
            if (!m.is_enabled)
                continue;
            providers_present.insert(m.provider.toLower());
            ArenaModelOption o;
            o.provider = m.provider;
            o.model_id = m.model_id;
            o.display_name = m.display_name.isEmpty() ? m.model_id : m.display_name;
            o.source_kind = "model";
            o.source_ref = m.id;
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
            if (!c.model.isEmpty() && !models.contains(c.model))
                models.prepend(c.model);
            for (const QString& mid : models) {
                ArenaModelOption o;
                o.provider = c.provider;
                o.model_id = mid;
                o.display_name = ProviderCatalog::display_name(c.provider) + " / " + mid;
                o.source_kind = "provider";
                o.source_ref = c.provider;
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
        if (providers_present.contains(p.toLower()))
            continue;
        const bool ready = !ProviderCatalog::requires_api_key(p);
        for (const QString& mid : ProviderCatalog::fallback_models(p)) {
            ArenaModelOption o;
            o.provider = p;
            o.model_id = mid;
            o.display_name = ProviderCatalog::display_name(p) + " / " + mid;
            o.source_kind = "provider";
            o.source_ref = p;
            o.ready = ready;
            add(o);
        }
    }
    // 5. Agent harnesses — explicit choice only. Each decision is one harness
    //    process: at the default 3-minute cadence about 480 calls per agent per
    //    day, on the user's own API key and its rate limits; `warning` says so.
    //    display_name stays the plain name: it becomes the agent's name.
    for (const auto& row : arena_harness_layer_detail::arena_harness_rows()) {
        const ai_chat::RouterHarness& h = row.state;
        const QString vendor = h.vendor.isEmpty() ? h.display_name : h.vendor;
        for (const QString& mid : row.models) {
            ArenaModelOption o;
            o.provider = QStringLiteral("harness:") + h.id;
            o.model_id = mid;
            o.source_kind = QStringLiteral("harness");
            o.source_ref = h.id;
            o.ready = h.eligibility.usable;
            o.display_name = QStringLiteral("%1 · %2").arg(h.display_name, mid);
            if (o.ready) {
                o.warning = readiness::harness_call_volume_warning(vendor);
                o.detail = h.eligibility.reason;
            } else {
                o.not_ready_reason = readiness::harness_not_ready_text(h.eligibility);
                o.detail = h.eligibility.reason;
                o.needs_consent = readiness::harness_needs_consent(h.eligibility);
            }
            add(o);
        }
    }
    return out;
}

Result<ArenaCredentials> ArenaModelRegistry::resolve_credentials(const QString& source_kind, const QString& source_ref,
                                                                 const QString& provider) const {
    ArenaCredentials cred;
    // A harness authenticates with the user's own CLI setup: no key, no endpoint.
    // Whether it may answer right now is re-checked by the harness call itself.
    if (source_kind == QLatin1String("harness") || ProviderCatalog::is_harness(provider))
        return Result<ArenaCredentials>::ok(cred);
    if (source_kind == QLatin1String("profile")) {
        if (auto r = LlmProfileRepository::instance().list_profiles(); r.is_ok())
            for (const auto& p : r.value())
                if (p.id == source_ref) {
                    cred.api_key = p.api_key;
                    cred.base_url = p.base_url;
                    break;
                }
    } else if (source_kind == QLatin1String("model")) {
        if (auto r = LlmConfigRepository::instance().list_models(); r.is_ok())
            for (const auto& m : r.value())
                if (m.id == source_ref) {
                    cred.api_key = m.api_key;
                    cred.base_url = m.base_url;
                    break;
                }
    } else { // "provider"
        if (auto r = LlmConfigRepository::instance().list_providers(); r.is_ok())
            for (const auto& c : r.value())
                if (c.provider == provider) {
                    cred.api_key = c.api_key;
                    cred.base_url = c.base_url;
                    break;
                }
    }
    // The Fincept key lives in AuthManager (session → SecureStorage), not in the
    // llm_configs row — exactly how LlmService resolves it. Without this, Fincept
    // agents posted no X-API-Key and every decision came back 401.
    if (cred.api_key.isEmpty() && provider.compare(QLatin1String("fincept"), Qt::CaseInsensitive) == 0)
        cred.api_key = fincept::auth::AuthManager::instance().fincept_api_key();
    if (cred.base_url.isEmpty())
        cred.base_url = ProviderCatalog::default_base_url(provider);
    if (cred.api_key.isEmpty() && ProviderCatalog::requires_api_key(provider))
        return Result<ArenaCredentials>::err("no API key configured for " + provider.toStdString());
    return Result<ArenaCredentials>::ok(cred);
}

} // namespace fincept::arena
