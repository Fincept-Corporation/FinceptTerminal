#pragma once
// Settings types — shared constants, enums, predefined API key definitions
// Used by all settings sections

#include <imgui.h>
#include <string>
#include <vector>

namespace fincept::settings {

// Sidebar navigation sections
enum class Section {
    Credentials,
    LLM,
    Appearance,
    Storage,
    Notifications,
    General
};

// Status message shown at bottom of settings
struct StatusMessage {
    enum Type { None, Success, Error, Info };
    Type type = None;
    std::string text;
    double expire_time = 0;  // ImGui time when message should disappear
};

// Predefined API key entry for credentials section
struct ApiKeyDef {
    const char* key;
    const char* label;
    const char* placeholder;
};

// All supported API key definitions — mirrors sqliteConstants.ts PREDEFINED_API_KEYS
inline const std::vector<ApiKeyDef>& predefined_api_keys() {
    static const std::vector<ApiKeyDef> keys = {
        {"openai_api_key",         "OpenAI API Key",         "sk-..."},
        {"anthropic_api_key",      "Anthropic API Key",      "sk-ant-..."},
        {"google_api_key",         "Google API Key",         "AIza..."},
        {"groq_api_key",           "Groq API Key",           "gsk_..."},
        {"perplexity_api_key",     "Perplexity API Key",     "pplx-..."},
        {"firecrawl_api_key",      "FireCrawl API Key",      "fc-..."},
        {"polygon_api_key",        "Polygon.io API Key",     "..."},
        {"alpha_vantage_api_key",  "Alpha Vantage API Key",  "..."},
        {"finnhub_api_key",        "Finnhub API Key",        "..."},
        {"fmp_api_key",            "Financial Modeling Prep", "..."},
        {"tiingo_api_key",         "Tiingo API Key",         "..."},
        {"quandl_api_key",         "Quandl/Nasdaq API Key",  "..."},
        {"newsapi_api_key",        "NewsAPI Key",            "..."},
        {"fred_api_key",           "FRED API Key",           "..."},
        {"eia_api_key",            "EIA API Key",            "..."},
        {"wto_api_key",            "WTO API Key",            "..."},
        {"bls_api_key",            "BLS API Key",            "..."},
        {"bea_api_key",            "BEA API Key",            "..."},
        {"elevenlabs_api_key",     "ElevenLabs API Key",     "..."},
    };
    return keys;
}

// LLM provider definitions
inline const std::vector<const char*>& llm_providers() {
    static const std::vector<const char*> providers = {
        "Fincept", "OpenAI", "Anthropic", "Google", "Ollama", "Groq",
        "Mistral", "Perplexity", "DeepSeek", "Cohere", "Fireworks",
        "Together", "OpenRouter", "LMStudio", "Cerebras", "SambaNova",
        "xAI", "Azure OpenAI", "Amazon Bedrock"
    };
    return providers;
}

// Color theme preset
struct ColorPreset {
    const char* name;
    ImVec4 primary, secondary, success, alert, warning, info;
    ImVec4 accent, text, text_muted, background, panel;
};

// Font family options
inline const std::vector<const char*>& font_families() {
    static const std::vector<const char*> fonts = {
        "Consolas", "Courier New", "Monaco",
        "Fira Code", "JetBrains Mono", "Source Code Pro"
    };
    return fonts;
}

// Timezone entries
struct TimezoneEntry {
    const char* label;
    const char* value;
};

inline const std::vector<TimezoneEntry>& timezone_list() {
    static const std::vector<TimezoneEntry> zones = {
        {"UTC",                        "UTC"},
        {"US/Eastern (New York)",      "America/New_York"},
        {"US/Central (Chicago)",       "America/Chicago"},
        {"US/Mountain (Denver)",       "America/Denver"},
        {"US/Pacific (Los Angeles)",   "America/Los_Angeles"},
        {"Canada/Toronto",             "America/Toronto"},
        {"Brazil/Sao Paulo",           "America/Sao_Paulo"},
        {"UK/London",                  "Europe/London"},
        {"Europe/Paris",               "Europe/Paris"},
        {"Europe/Berlin",              "Europe/Berlin"},
        {"Europe/Zurich",              "Europe/Zurich"},
        {"Europe/Moscow",              "Europe/Moscow"},
        {"Asia/Dubai",                 "Asia/Dubai"},
        {"Asia/Mumbai",                "Asia/Kolkata"},
        {"Asia/Singapore",             "Asia/Singapore"},
        {"Asia/Hong Kong",             "Asia/Hong_Kong"},
        {"Asia/Shanghai",              "Asia/Shanghai"},
        {"Asia/Tokyo",                 "Asia/Tokyo"},
        {"Asia/Seoul",                 "Asia/Seoul"},
        {"Australia/Sydney",           "Australia/Sydney"},
        {"Pacific/Auckland",           "Pacific/Auckland"},
    };
    return zones;
}

// Notification provider definition
struct NotificationProviderDef {
    const char* id;
    const char* label;
    const char* placeholder;
    bool uses_webhook;  // true = webhook URL, false = API key
};

inline const std::vector<NotificationProviderDef>& notification_providers() {
    static const std::vector<NotificationProviderDef> providers = {
        {"pushbullet", "Pushbullet",  "Access Token",  false},
        {"ifttt",      "IFTTT",       "Webhook Key",   true},
        {"discord",    "Discord",     "Webhook URL",   true},
        {"slack",      "Slack",       "Webhook URL",   true},
        {"email",      "Email",       "SMTP Config",   false},
        {"webhook",    "Custom Webhook", "URL",         true},
    };
    return providers;
}

} // namespace fincept::settings
