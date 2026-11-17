#pragma once
// ProviderCatalog — static metadata for every known LLM provider: ids, fallback
// model lists, default base URLs, key requirements, brand colors, and chat
// endpoint composition. Single source of truth shared by Settings UI
// (LlmConfigSection) and the Alpha Arena model registry. Data moved verbatim
// from LlmConfigSection.cpp (2026-06-10); update HERE when adding a provider.

#include <QString>
#include <QStringList>

namespace fincept::ai_chat {

class ProviderCatalog {
  public:
    static const QStringList& known_providers();
    /// Hard block for retired/forbidden providers (currently AtlasCloud). Returns
    /// true for the blocked provider id AND for any base_url pointing at its host,
    /// so it can't be reached even by manually pointing an OpenAI-compatible
    /// provider at the endpoint. Enforced at every request chokepoint in LlmService.
    static bool is_blocked(const QString& provider, const QString& base_url = {});
    /// An agent harness answering terminal AI: provider "harness:<id>" (see
    /// ModelRouter). Never in known_providers() — that list feeds the API-key
    /// settings and the Python key map; a harness has no key here.
    static bool is_harness(const QString& provider);
    /// "claude" for "harness:claude"; empty otherwise.
    static QString harness_id(const QString& provider);
    static QString display_name(const QString& provider_id);
    static QStringList fallback_models(const QString& provider);
    static QString default_base_url(const QString& provider);
    static bool requires_api_key(const QString& provider);     // false: ollama, fincept, harness:*
    static bool is_openai_compatible(const QString& provider); // everything except anthropic/gemini/fincept/harness:*
    /// Brand color used for arena agent identity (hex, e.g. "#10A37F"). A fixed
    /// identity colour for the dark theme — draw it through ui::colors::legible().
    static QString brand_color(const QString& provider);
    /// Full chat-completions endpoint. Mirrors LlmService::get_endpoint_url rules:
    /// custom base_url wins (appends /v1 + suffix unless already versioned/full);
    /// fincept is handled by the CALLER (needs AppConfig) — returns empty for it.
    static QString chat_endpoint(const QString& provider, const QString& base_url, const QString& model);
};

} // namespace fincept::ai_chat
