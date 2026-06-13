// ProviderCatalog.cpp — static metadata for every known LLM provider.
// Single source of truth shared by Settings UI (LlmConfigSection) and the
// Alpha Arena model registry. Data moved verbatim from LlmConfigSection.cpp
// (2026-06-10); update HERE when adding a provider.
#include "services/llm/ProviderCatalog.h"

#include <QHash>
#include <QRegularExpression>

namespace fincept::ai_chat {

const QStringList& ProviderCatalog::known_providers() {
    static const QStringList kProviders = {"openai",  "anthropic", "gemini",   "groq",  "deepseek",
                                           "openrouter", "minimax", "kimi", "ollama", "xai",   "fincept",
                                           "astraflow", "astraflow_cn", "aihubmix", "atlascloud"};
    return kProviders;
}

QString ProviderCatalog::display_name(const QString& provider_id) {
    static const QHash<QString, QString> kNames = {
        {"openai", "OpenAI"},        {"anthropic", "Anthropic"},      {"gemini", "Gemini"},
        {"groq", "Groq"},            {"deepseek", "DeepSeek"},        {"openrouter", "OpenRouter"},
        {"minimax", "MiniMax"},      {"kimi", "Kimi"},                {"ollama", "Ollama"},
        {"xai", "xAI"},              {"fincept", "Fincept LLM (recommended)"}, {"astraflow", "AstraFlow"},
        {"astraflow_cn", "AstraFlow CN"}, {"aihubmix", "AIHubMix"},   {"atlascloud", "AtlasCloud"},
    };
    const QString id = provider_id.toLower();
    const auto it = kNames.find(id);
    if (it != kNames.end())
        return it.value();
    // Unknown/custom provider: capitalize the first letter as a sensible default.
    if (provider_id.isEmpty())
        return provider_id;
    QString s = provider_id;
    s[0] = s[0].toUpper();
    return s;
}

QString ProviderCatalog::default_base_url(const QString& provider) {
    const QString p = provider.toLower();
    if (p == "openai")
        return {}; // uses default
    if (p == "anthropic")
        return {};
    if (p == "gemini")
        return {};
    if (p == "groq")
        return {};
    if (p == "deepseek")
        return {};
    if (p == "openrouter")
        return {};
    if (p == "minimax")
        return "https://api.minimax.io/v1";
    if (p == "kimi")
        return {}; // defaults to https://api.moonshot.ai
    if (p == "ollama")
        return "http://localhost:11434";
    if (p == "xai")
        return {};
    if (p == "fincept")
        return {}; // endpoints are hardcoded in LlmService, no base_url needed
    if (p == "astraflow")
        return "https://api-us-ca.umodelverse.ai/v1"; // Astraflow global endpoint (UCloud)
    if (p == "astraflow_cn")
        return "https://api.modelverse.cn/v1"; // Astraflow China endpoint (UCloud)
    if (p == "aihubmix")
        return "https://aihubmix.com/v1"; // AIHubMix — OpenAI-compatible aggregator (500+ models)
    if (p == "atlascloud")
        return "https://api.atlascloud.ai/v1"; // AtlasCloud — OpenAI-compatible aggregator (300+ open models)
    return {};
}

QStringList ProviderCatalog::fallback_models(const QString& provider) {
    const QString p = provider.toLower();
    if (p == "openai")
        return {"gpt-4o", "gpt-4o-mini", "gpt-4-turbo", "o3-mini"};
    if (p == "anthropic")
        return {"claude-sonnet-4-5-20250514", "claude-opus-4-5", "claude-3-5-sonnet-20241022",
                "claude-3-haiku-20240307"};
    if (p == "gemini")
        return {"gemini-2.5-flash", "gemini-2.5-pro", "gemini-2.0-flash"};
    if (p == "groq")
        return {"llama-3.3-70b-versatile", "mixtral-8x7b-32768", "gemma2-9b-it"};
    if (p == "deepseek")
        return {"deepseek-chat", "deepseek-reasoner"};
    if (p == "openrouter")
        return {"openai/gpt-4o", "anthropic/claude-sonnet-4-5", "google/gemini-2.5-flash"};
    if (p == "minimax")
        return {"MiniMax-M2.7", "MiniMax-M2.7-highspeed", "MiniMax-M2.5", "MiniMax-M2.5-highspeed"};
    if (p == "kimi")
        return {"moonshot-v1-auto",
                "moonshot-v1-8k",
                "moonshot-v1-32k",
                "moonshot-v1-128k",
                "kimi-k2.5",
                "kimi-k2.6",
                "kimi-k2-thinking",
                "kimi-k2-thinking-turbo",
                "kimi-k2-0905-preview",
                "kimi-k2-turbo-preview",
                "kimi-k2-0711-preview",
                "moonshot-v1-8k-vision-preview",
                "moonshot-v1-32k-vision-preview",
                "moonshot-v1-128k-vision-preview"};
    if (p == "ollama")
        return {}; // Local provider — models fetched live from /api/tags. No fallback so the
                   // combo only shows what the user actually has installed locally.
    if (p == "xai")
        return {"grok-4-latest", "grok-4", "grok-3", "grok-3-mini"};
    if (p == "fincept")
        return {"MiniMax-M2.7", "MiniMax-M2.7-highspeed", "MiniMax-M2.5", "MiniMax-M2.5-highspeed"};
    if (p == "astraflow" || p == "astraflow_cn")
        // Astraflow by UCloud — OpenAI-compatible aggregator supporting 200+ models.
        // A non-exhaustive starter list; full list fetchable via Fetch button.
        return {"gpt-4o", "gpt-4o-mini", "gpt-4-turbo", "o3-mini",
                "claude-sonnet-4-5-20250514", "claude-3-5-sonnet-20241022",
                "gemini-2.5-flash", "gemini-2.5-pro",
                "deepseek-chat", "deepseek-reasoner",
                "llama-3.3-70b-versatile", "qwen-turbo", "qwen-plus"};
    if (p == "aihubmix")
        // AIHubMix — OpenAI-compatible aggregator routing 500+ models (OpenAI, Claude,
        // Gemini, DeepSeek, Qwen, ...) through one /v1/chat/completions endpoint. Models
        // keep their upstream ids. Starter list only; full list via the Fetch button.
        return {"gpt-4o", "gpt-4o-mini", "gpt-4.1", "o3-mini",
                "claude-sonnet-4-5-20250514", "claude-3-5-sonnet-20241022",
                "gemini-2.5-flash", "gemini-2.5-pro",
                "deepseek-chat", "deepseek-reasoner",
                "qwen-max", "qwen-plus", "grok-4"};
    if (p == "atlascloud")
        // AtlasCloud — OpenAI-compatible aggregator (300+ models). Model ids are
        // HuggingFace-style "org/model" strings (NOT short aliases). Note: a few ids
        // listed by /v1/models are unprovisioned and return 400 (e.g. google/gemini-2.0-flash,
        // openai/gpt-4o*). The list below is a verified-working starter set; use the
        // Fetch button for the full live catalog (/v1/models).
        return {"deepseek-ai/DeepSeek-V3.1", "deepseek-ai/deepseek-r1-0528",
                "qwen/qwen3-32b", "Qwen/Qwen3-235B-A22B-Instruct-2507",
                "zai-org/GLM-4.6", "moonshotai/Kimi-K2-Instruct", "MiniMaxAI/MiniMax-M2",
                "google/gemini-2.5-flash", "anthropic/claude-haiku-4.5-20251001"};
    return {};
}

bool ProviderCatalog::requires_api_key(const QString& provider) {
    const QString p = provider.toLower();
    return p != "ollama" && p != "fincept";
}

bool ProviderCatalog::is_openai_compatible(const QString& provider) {
    const QString p = provider.toLower();
    return p != "anthropic" && p != "gemini" && p != "google" && p != "fincept";
}

QString ProviderCatalog::brand_color(const QString& provider) {
    static const QHash<QString, QString> kColors = {
        {"openai", "#10A37F"},   {"anthropic", "#D97757"}, {"gemini", "#4285F4"},
        {"groq", "#F55036"},     {"deepseek", "#4D6BFE"},  {"openrouter", "#8B5CF6"},
        {"minimax", "#FF4D6A"},  {"kimi", "#16D9C4"},      {"ollama", "#9CA3AF"},
        {"xai", "#E7E9EA"},      {"fincept", "#FF8800"},   {"astraflow", "#38BDF8"},
        {"astraflow_cn", "#38BDF8"}, {"aihubmix", "#F59E0B"}, {"atlascloud", "#34D399"},
    };
    const auto it = kColors.find(provider.toLower());
    if (it != kColors.end()) return it.value();
    static const QStringList kPalette = {"#E879F9", "#A3E635", "#FB7185", "#60A5FA", "#FBBF24"};
    return kPalette[static_cast<qsizetype>(qHash(provider.toLower()) % static_cast<size_t>(kPalette.size()))];
}

// Recursion is depth-1 by construction: it only fires for aggregators whose
// default_base_url is non-empty, and that call takes the base_url branch.
// NOLINTNEXTLINE(misc-no-recursion)
QString ProviderCatalog::chat_endpoint(const QString& provider, const QString& base_url, const QString& model) {
    const QString p = provider.toLower();
    if (p == "fincept") return {}; // caller composes AppConfig::api_base_url() + "/research/chat"

    if (!base_url.isEmpty()) {
        QString base = base_url;
        while (base.endsWith('/')) base.chop(1);
        const QString suffix = (p == "anthropic") ? QStringLiteral("/messages")
                                                  : QStringLiteral("/chat/completions");
        if (base.endsWith(suffix)) return base;
        static const QRegularExpression re(QStringLiteral("/v\\d+[a-zA-Z]*$"));
        if (re.match(base).hasMatch()) return base + suffix;
        return base + QStringLiteral("/v1") + suffix;
    }
    if (p == "openai")     return "https://api.openai.com/v1/chat/completions";
    if (p == "anthropic")  return "https://api.anthropic.com/v1/messages";
    if (p == "gemini" || p == "google")
        return "https://generativelanguage.googleapis.com/v1beta/models/" + model + ":generateContent";
    if (p == "groq")       return "https://api.groq.com/openai/v1/chat/completions";
    if (p == "deepseek")   return "https://api.deepseek.com/v1/chat/completions";
    if (p == "openrouter") return "https://openrouter.ai/api/v1/chat/completions";
    if (p == "minimax")    return "https://api.minimax.io/v1/chat/completions";
    if (p == "kimi")       return "https://api.moonshot.ai/v1/chat/completions";
    if (p == "ollama")     return "http://localhost:11434/v1/chat/completions";
    if (p == "xai")        return "https://api.x.ai/v1/chat/completions";
    const QString def = default_base_url(p);
    if (!def.isEmpty()) return chat_endpoint(p, def, model);
    return {};
}

} // namespace fincept::ai_chat
