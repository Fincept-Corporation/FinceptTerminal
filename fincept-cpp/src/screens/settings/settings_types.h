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
    Voice,
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

// Notification provider config field definition
struct NotifFieldDef {
    const char* key;
    const char* label;
    const char* placeholder;
    bool is_password;
    bool required;
};

// Notification provider definition — mirrors Tauri NotificationProvider
struct NotificationProviderDef {
    const char* id;
    const char* label;
    std::vector<NotifFieldDef> fields;
};

inline const std::vector<NotificationProviderDef>& notification_providers() {
    static const std::vector<NotificationProviderDef> providers = {
        // Tier 1 — most popular
        {"telegram",   "Telegram",    {{"bot_token", "Bot Token", "123456:ABC-DEF...", true, true},
                                        {"chat_id",   "Chat ID",   "e.g. 123456789",  false, true}}},
        {"discord",    "Discord",     {{"webhook_url", "Webhook URL", "https://discord.com/api/webhooks/...", false, true}}},
        {"whatsapp",   "WhatsApp",    {{"phone_id", "Phone Number ID", "e.g. 123456", false, true},
                                        {"token",    "Access Token",    "Bearer token",  true,  true}}},
        {"slack",      "Slack",       {{"webhook_url", "Webhook URL", "https://hooks.slack.com/services/...", false, true}}},
        {"pushover",   "Pushover",    {{"user_key", "User Key", "u...", true, true},
                                        {"api_token","App Token","a...", true, true}}},
        {"ntfy",       "ntfy",        {{"server_url", "Server URL", "https://ntfy.sh", false, true},
                                        {"topic",     "Topic",      "fincept-alerts",  false, true}}},
        {"email",      "Email",       {{"smtp_host","SMTP Host","smtp.gmail.com",false,true},
                                        {"smtp_port","Port","587",false,true},
                                        {"username", "Username","user@example.com",false,true},
                                        {"password", "Password","app password",true,true},
                                        {"to",       "To","recipient@example.com",false,true}}},
        // Tier 2 — power user
        {"teams",      "Teams",       {{"webhook_url", "Webhook URL", "https://outlook.office.com/webhook/...", false, true}}},
        {"pushbullet", "Pushbullet",  {{"access_token", "Access Token", "o.xxx...", true, true}}},
        {"ifttt",      "IFTTT",       {{"webhook_key", "Webhook Key", "xxx...", true, true},
                                        {"event_name", "Event Name", "fincept_alert", false, true}}},
        {"gotify",     "Gotify",      {{"server_url", "Server URL", "https://gotify.example.com", false, true},
                                        {"app_token", "App Token",  "Axxx...", true, true}}},
        // Tier 3 — specialized
        {"pagerduty",  "PagerDuty",   {{"routing_key", "Routing Key", "xxx...", true, true}}},
        {"opsgenie",   "OpsGenie",    {{"api_key", "API Key", "xxx...", true, true}}},
        {"twilio",     "Twilio SMS",  {{"account_sid","Account SID","ACxxx",false,true},
                                        {"auth_token", "Auth Token","xxx",true,true},
                                        {"from_number","From Number","+1234567890",false,true},
                                        {"to_number",  "To Number",  "+0987654321",false,true}}},
        {"line",       "LINE Notify", {{"access_token", "Access Token", "xxx...", true, true}}},
    };
    return providers;
}

} // namespace fincept::settings
