// Notification System — cross-platform local OS notifications + ImGui fallback
// + external provider routing (Telegram, Discord, Slack, ntfy, etc.)
//
// Windows:  Shell_NotifyIcon balloon
// macOS:    NSUserNotificationCenter via Obj-C++ (notification_mac.mm)
// Linux:    notify-send
// Fallback: ImGui overlay toast (always available)
// External: HTTP POST to enabled providers (loaded from DB config)

#include "notification.h"
#include "logger.h"
#include "http/http_client.h"
#include "storage/database.h"
#include <imgui.h>
#include <algorithm>
#include <chrono>
#include <deque>
#include <thread>
#include <nlohmann/json.hpp>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#include <strsafe.h>
#elif defined(__APPLE__)
namespace fincept::core::notify::platform {
    bool send_mac(const std::string& title, const std::string& body, int level);
}
#elif defined(__linux__)
#include <cstdlib>
#include <cstdio>
#endif

namespace fincept::core {

using json = nlohmann::json;
static const char* TAG = "Notify";

// ============================================================================
// In-app ImGui toast system (unchanged)
// ============================================================================

struct Toast {
    std::string title;
    std::string body;
    NotifyLevel level;
    float       lifetime;
    float       max_life;
    int64_t     id;
};

static constexpr float TOAST_DURATION_INFO    = 4.0f;
static constexpr float TOAST_DURATION_SUCCESS = 3.0f;
static constexpr float TOAST_DURATION_WARNING = 5.0f;
static constexpr float TOAST_DURATION_ERROR   = 6.0f;
static constexpr int   MAX_VISIBLE_TOASTS     = 5;
static constexpr float TOAST_WIDTH            = 320.0f;
static constexpr float TOAST_PADDING          = 8.0f;
static constexpr float TOAST_FADE_TIME        = 0.5f;

static std::deque<Toast> s_toasts;
static std::mutex        s_toast_mutex;
static int64_t           s_next_id = 1;
static bool              s_os_enabled = true;

static float toast_duration(NotifyLevel level) {
    switch (level) {
        case NotifyLevel::Info:    return TOAST_DURATION_INFO;
        case NotifyLevel::Success: return TOAST_DURATION_SUCCESS;
        case NotifyLevel::Warning: return TOAST_DURATION_WARNING;
        case NotifyLevel::Error:   return TOAST_DURATION_ERROR;
    }
    return TOAST_DURATION_INFO;
}

static ImVec4 toast_color(NotifyLevel level) {
    switch (level) {
        case NotifyLevel::Info:    return {0.2f, 0.6f, 0.9f, 1.0f};
        case NotifyLevel::Success: return {0.1f, 0.75f, 0.3f, 1.0f};
        case NotifyLevel::Warning: return {0.95f, 0.7f, 0.1f, 1.0f};
        case NotifyLevel::Error:   return {0.9f, 0.15f, 0.15f, 1.0f};
    }
    return {0.5f, 0.5f, 0.5f, 1.0f};
}

static const char* toast_icon(NotifyLevel level) {
    switch (level) {
        case NotifyLevel::Info:    return "[i]";
        case NotifyLevel::Success: return "[+]";
        case NotifyLevel::Warning: return "[!]";
        case NotifyLevel::Error:   return "[x]";
    }
    return "[?]";
}

static const char* level_str(NotifyLevel level) {
    switch (level) {
        case NotifyLevel::Info:    return "info";
        case NotifyLevel::Success: return "success";
        case NotifyLevel::Warning: return "warning";
        case NotifyLevel::Error:   return "error";
    }
    return "info";
}

static const char* level_emoji(NotifyLevel level) {
    switch (level) {
        case NotifyLevel::Info:    return "\xe2\x84\xb9\xef\xb8\x8f";   // ℹ️
        case NotifyLevel::Success: return "\xe2\x9c\x85";               // ✅
        case NotifyLevel::Warning: return "\xe2\x9a\xa0\xef\xb8\x8f";   // ⚠️
        case NotifyLevel::Error:   return "\xe2\x9d\x8c";               // ❌
    }
    return "";
}

// ============================================================================
// Platform-specific OS notification (unchanged)
// ============================================================================

#ifdef _WIN32
static bool send_os_notification_win(const std::string& title, const std::string& body) {
    static bool initialized = false;
    static NOTIFYICONDATAA nid = {};
    static HWND hwnd = nullptr;
    if (!initialized) {
        hwnd = GetActiveWindow();
        if (!hwnd) hwnd = GetForegroundWindow();
        if (!hwnd) return false;
        ZeroMemory(&nid, sizeof(nid));
        nid.cbSize = sizeof(nid);
        nid.hWnd = hwnd;
        nid.uID = 1;
        nid.uFlags = NIF_INFO | NIF_ICON | NIF_TIP;
        nid.hIcon = LoadIconA(nullptr, IDI_APPLICATION);
        StringCchCopyA(nid.szTip, ARRAYSIZE(nid.szTip), "Fincept Terminal");
        nid.dwInfoFlags = NIIF_INFO;
        Shell_NotifyIconA(NIM_ADD, &nid);
        initialized = true;
    }
    nid.uFlags = NIF_INFO;
    StringCchCopyA(nid.szInfoTitle, ARRAYSIZE(nid.szInfoTitle), title.c_str());
    StringCchCopyA(nid.szInfo, ARRAYSIZE(nid.szInfo), body.c_str());
    nid.dwInfoFlags = NIIF_INFO;
    return Shell_NotifyIconA(NIM_MODIFY, &nid) == TRUE;
}
#elif defined(__linux__)
static bool send_os_notification_linux(const std::string& title, const std::string& body) {
    std::string cmd = "notify-send";
    cmd += " \"" + title + "\" \"" + body + "\"";
    cmd += " --app-name=\"Fincept Terminal\" -t 5000 2>/dev/null &";
    return system(cmd.c_str()) == 0;
}
#endif

static bool send_os_notification(const std::string& title, const std::string& body,
                                   NotifyLevel level) {
    if (!s_os_enabled) return false;
#ifdef _WIN32
    return send_os_notification_win(title, body);
#elif defined(__APPLE__)
    return notify::platform::send_mac(title, body, static_cast<int>(level));
#elif defined(__linux__)
    return send_os_notification_linux(title, body);
#else
    (void)title; (void)body; (void)level;
    return false;
#endif
}

// ============================================================================
// External provider routing — loads config from DB, sends via HTTP
// ============================================================================

struct ProviderState {
    std::string id;
    bool enabled = false;
    std::map<std::string, std::string> config;
};

static std::vector<ProviderState> s_providers;
static std::mutex s_providers_mutex;
static bool s_providers_loaded = false;

// Event filters
static bool s_filter_success = true;
static bool s_filter_error   = true;
static bool s_filter_warning = true;
static bool s_filter_info    = true;

// Format message for text-based providers
static std::string format_message(const std::string& title, const std::string& body,
                                    NotifyLevel level) {
    return std::string(level_emoji(level)) + " *" + title + "*\n" + body;
}

// --- Individual provider send functions ---

static bool send_telegram(const std::map<std::string, std::string>& cfg,
                           const std::string& title, const std::string& body, NotifyLevel level) {
    auto it_token = cfg.find("bot_token");
    auto it_chat = cfg.find("chat_id");
    if (it_token == cfg.end() || it_chat == cfg.end()) return false;
    if (it_token->second.empty() || it_chat->second.empty()) return false;

    std::string url = "https://api.telegram.org/bot" + it_token->second + "/sendMessage";
    json payload = {
        {"chat_id", it_chat->second},
        {"text", format_message(title, body, level)},
        {"parse_mode", "Markdown"}
    };
    auto resp = http::HttpClient::instance().post_json(url, payload);
    return resp.success && resp.status_code == 200;
}

static bool send_discord(const std::map<std::string, std::string>& cfg,
                          const std::string& title, const std::string& body, NotifyLevel level) {
    auto it = cfg.find("webhook_url");
    if (it == cfg.end() || it->second.empty()) return false;

    json payload = {
        {"content", format_message(title, body, level)},
        {"username", "Fincept Terminal"}
    };
    auto resp = http::HttpClient::instance().post_json(it->second, payload);
    return resp.success;
}

static bool send_slack(const std::map<std::string, std::string>& cfg,
                        const std::string& title, const std::string& body, NotifyLevel level) {
    auto it = cfg.find("webhook_url");
    if (it == cfg.end() || it->second.empty()) return false;

    json payload = {{"text", format_message(title, body, level)}};
    auto resp = http::HttpClient::instance().post_json(it->second, payload);
    return resp.success;
}

static bool send_teams(const std::map<std::string, std::string>& cfg,
                        const std::string& title, const std::string& body, NotifyLevel /*level*/) {
    auto it = cfg.find("webhook_url");
    if (it == cfg.end() || it->second.empty()) return false;

    json payload = {
        {"@type", "MessageCard"},
        {"summary", title},
        {"text", "**" + title + "**\n\n" + body}
    };
    auto resp = http::HttpClient::instance().post_json(it->second, payload);
    return resp.success;
}

static bool send_ntfy(const std::map<std::string, std::string>& cfg,
                       const std::string& title, const std::string& body, NotifyLevel level) {
    auto it_url = cfg.find("server_url");
    auto it_topic = cfg.find("topic");
    if (it_url == cfg.end() || it_topic == cfg.end()) return false;
    if (it_topic->second.empty()) return false;

    std::string server = it_url->second.empty() ? "https://ntfy.sh" : it_url->second;
    std::string url = server + "/" + it_topic->second;

    int priority = 3; // default
    if (level == NotifyLevel::Error) priority = 5;
    else if (level == NotifyLevel::Warning) priority = 4;

    json payload = {
        {"topic", it_topic->second},
        {"title", title},
        {"message", body},
        {"priority", priority}
    };
    auto resp = http::HttpClient::instance().post_json(url, payload);
    return resp.success;
}

static bool send_pushover(const std::map<std::string, std::string>& cfg,
                            const std::string& title, const std::string& body, NotifyLevel level) {
    auto it_user = cfg.find("user_key");
    auto it_token = cfg.find("api_token");
    if (it_user == cfg.end() || it_token == cfg.end()) return false;
    if (it_user->second.empty() || it_token->second.empty()) return false;

    int priority = 0;
    if (level == NotifyLevel::Error) priority = 1;
    else if (level == NotifyLevel::Warning) priority = 0;
    else if (level == NotifyLevel::Info) priority = -1;

    json payload = {
        {"token", it_token->second},
        {"user", it_user->second},
        {"title", title},
        {"message", body},
        {"priority", priority}
    };
    auto resp = http::HttpClient::instance().post_json("https://api.pushover.net/1/messages.json", payload);
    return resp.success && resp.status_code == 200;
}

static bool send_pushbullet(const std::map<std::string, std::string>& cfg,
                              const std::string& title, const std::string& body, NotifyLevel /*level*/) {
    auto it = cfg.find("access_token");
    if (it == cfg.end() || it->second.empty()) return false;

    json payload = {
        {"type", "note"},
        {"title", title},
        {"body", body}
    };
    http::Headers headers = {
        {"Access-Token", it->second},
        {"Content-Type", "application/json"}
    };
    auto resp = http::HttpClient::instance().post("https://api.pushbullet.com/v2/pushes",
                                                    payload.dump(), headers);
    return resp.success && resp.status_code == 200;
}

static bool send_gotify(const std::map<std::string, std::string>& cfg,
                          const std::string& title, const std::string& body, NotifyLevel level) {
    auto it_url = cfg.find("server_url");
    auto it_token = cfg.find("app_token");
    if (it_url == cfg.end() || it_token == cfg.end()) return false;
    if (it_url->second.empty() || it_token->second.empty()) return false;

    int priority = 5;
    if (level == NotifyLevel::Error) priority = 8;
    else if (level == NotifyLevel::Warning) priority = 6;

    std::string url = it_url->second + "/message?token=" + it_token->second;
    json payload = {{"title", title}, {"message", body}, {"priority", priority}};
    auto resp = http::HttpClient::instance().post_json(url, payload);
    return resp.success;
}

static bool send_ifttt(const std::map<std::string, std::string>& cfg,
                        const std::string& title, const std::string& body, NotifyLevel /*level*/) {
    auto it_key = cfg.find("webhook_key");
    auto it_event = cfg.find("event_name");
    if (it_key == cfg.end() || it_key->second.empty()) return false;
    std::string event = (it_event != cfg.end() && !it_event->second.empty())
                        ? it_event->second : "fincept_alert";

    std::string url = "https://maker.ifttt.com/trigger/" + event + "/with/key/" + it_key->second;
    json payload = {{"value1", title}, {"value2", body}};
    auto resp = http::HttpClient::instance().post_json(url, payload);
    return resp.success;
}

static bool send_pagerduty(const std::map<std::string, std::string>& cfg,
                             const std::string& title, const std::string& body, NotifyLevel level) {
    auto it = cfg.find("routing_key");
    if (it == cfg.end() || it->second.empty()) return false;

    std::string severity = "info";
    if (level == NotifyLevel::Error) severity = "critical";
    else if (level == NotifyLevel::Warning) severity = "warning";

    json payload = {
        {"routing_key", it->second},
        {"event_action", "trigger"},
        {"payload", {
            {"summary", title + ": " + body},
            {"severity", severity},
            {"source", "Fincept Terminal"}
        }}
    };
    auto resp = http::HttpClient::instance().post_json(
        "https://events.pagerduty.com/v2/enqueue", payload);
    return resp.success;
}

static bool send_opsgenie(const std::map<std::string, std::string>& cfg,
                            const std::string& title, const std::string& body, NotifyLevel level) {
    auto it = cfg.find("api_key");
    if (it == cfg.end() || it->second.empty()) return false;

    std::string priority = "P3";
    if (level == NotifyLevel::Error) priority = "P1";
    else if (level == NotifyLevel::Warning) priority = "P2";

    json payload = {
        {"message", title},
        {"description", body},
        {"priority", priority},
        {"source", "Fincept Terminal"}
    };
    http::Headers headers = {
        {"Authorization", "GenieKey " + it->second},
        {"Content-Type", "application/json"}
    };
    auto resp = http::HttpClient::instance().post("https://api.opsgenie.com/v2/alerts",
                                                    payload.dump(), headers);
    return resp.success;
}

static bool send_whatsapp(const std::map<std::string, std::string>& cfg,
                            const std::string& title, const std::string& body, NotifyLevel level) {
    auto it_phone = cfg.find("phone_id");
    auto it_token = cfg.find("token");
    if (it_phone == cfg.end() || it_token == cfg.end()) return false;
    if (it_phone->second.empty() || it_token->second.empty()) return false;

    // WhatsApp Business API — requires pre-approved template or active conversation
    std::string url = "https://graph.facebook.com/v18.0/" + it_phone->second + "/messages";
    json payload = {
        {"messaging_product", "whatsapp"},
        {"type", "text"},
        {"text", {{"body", format_message(title, body, level)}}}
    };
    http::Headers headers = {
        {"Authorization", "Bearer " + it_token->second},
        {"Content-Type", "application/json"}
    };
    auto resp = http::HttpClient::instance().post(url, payload.dump(), headers);
    return resp.success;
}

static bool send_twilio(const std::map<std::string, std::string>& cfg,
                         const std::string& title, const std::string& body, NotifyLevel /*level*/) {
    auto it_sid = cfg.find("account_sid");
    auto it_auth = cfg.find("auth_token");
    auto it_from = cfg.find("from_number");
    auto it_to = cfg.find("to_number");
    if (it_sid == cfg.end() || it_auth == cfg.end() ||
        it_from == cfg.end() || it_to == cfg.end()) return false;
    if (it_sid->second.empty() || it_auth->second.empty()) return false;

    std::string url = "https://api.twilio.com/2010-04-01/Accounts/" +
                      it_sid->second + "/Messages.json";
    std::string form_body = "From=" + it_from->second +
                            "&To=" + it_to->second +
                            "&Body=" + title + ": " + body;
    http::Headers headers = {{"Content-Type", "application/x-www-form-urlencoded"}};
    // Twilio uses basic auth — encode sid:token
    // HttpClient doesn't have basic auth built in, use header
    std::string auth_str = it_sid->second + ":" + it_auth->second;
    // Base64 encode (simple implementation for auth)
    static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string encoded;
    int val = 0, bits = -6;
    for (unsigned char c : auth_str) {
        val = (val << 8) + c;
        bits += 8;
        while (bits >= 0) {
            encoded += b64[(val >> bits) & 0x3F];
            bits -= 6;
        }
    }
    if (bits > -6) encoded += b64[((val << 8) >> (bits + 8)) & 0x3F];
    while (encoded.size() % 4) encoded += '=';

    headers["Authorization"] = "Basic " + encoded;
    auto resp = http::HttpClient::instance().post(url, form_body, headers);
    return resp.success;
}

static bool send_line(const std::map<std::string, std::string>& cfg,
                       const std::string& title, const std::string& body, NotifyLevel /*level*/) {
    auto it = cfg.find("access_token");
    if (it == cfg.end() || it->second.empty()) return false;

    std::string form_body = "message=" + title + "\n" + body;
    http::Headers headers = {
        {"Authorization", "Bearer " + it->second},
        {"Content-Type", "application/x-www-form-urlencoded"}
    };
    auto resp = http::HttpClient::instance().post(
        "https://notify-api.line.me/api/notify", form_body, headers);
    return resp.success;
}

// Dispatch to the right provider
static bool send_to_provider(const std::string& id, const std::map<std::string, std::string>& cfg,
                               const std::string& title, const std::string& body, NotifyLevel level) {
    if (id == "telegram")   return send_telegram(cfg, title, body, level);
    if (id == "discord")    return send_discord(cfg, title, body, level);
    if (id == "slack")      return send_slack(cfg, title, body, level);
    if (id == "teams")      return send_teams(cfg, title, body, level);
    if (id == "ntfy")       return send_ntfy(cfg, title, body, level);
    if (id == "pushover")   return send_pushover(cfg, title, body, level);
    if (id == "pushbullet") return send_pushbullet(cfg, title, body, level);
    if (id == "gotify")     return send_gotify(cfg, title, body, level);
    if (id == "ifttt")      return send_ifttt(cfg, title, body, level);
    if (id == "pagerduty")  return send_pagerduty(cfg, title, body, level);
    if (id == "opsgenie")   return send_opsgenie(cfg, title, body, level);
    if (id == "whatsapp")   return send_whatsapp(cfg, title, body, level);
    if (id == "twilio")     return send_twilio(cfg, title, body, level);
    if (id == "line")       return send_line(cfg, title, body, level);
    // email: would need SMTP implementation — skip for now, log only
    if (id == "email") {
        LOG_INFO(TAG, "Email provider not yet implemented (SMTP required)");
        return false;
    }
    return false;
}

// ============================================================================
// Public API
// ============================================================================

namespace notify {

void load_provider_config() {
    auto config_str = db::ops::get_setting("notifications_config");
    if (!config_str) {
        s_providers_loaded = true;
        return;
    }

    try {
        auto j = json::parse(*config_str);

        if (j.contains("filters")) {
            auto& f = j["filters"];
            s_filter_success = f.value("success", true);
            s_filter_error   = f.value("error", true);
            s_filter_warning = f.value("warning", true);
            s_filter_info    = f.value("info", true);
        }

        if (j.contains("providers")) {
            std::lock_guard<std::mutex> lock(s_providers_mutex);
            s_providers.clear();
            for (auto& [key, val] : j["providers"].items()) {
                ProviderState ps;
                ps.id = key;
                ps.enabled = val.value("enabled", false);
                // Load all config fields
                if (val.contains("config") && val["config"].is_object()) {
                    for (auto& [fk, fv] : val["config"].items()) {
                        ps.config[fk] = fv.get<std::string>();
                    }
                }
                // Legacy: single "value" field → map to first config key
                if (val.contains("value") && val["value"].is_string() && ps.config.empty()) {
                    ps.config["value"] = val["value"].get<std::string>();
                }
                s_providers.push_back(std::move(ps));
            }
        }
    } catch (const std::exception& e) {
        LOG_ERROR(TAG, "Failed to load notification config: %s", e.what());
    }

    s_providers_loaded = true;
    LOG_INFO(TAG, "Loaded notification config: %d providers", (int)s_providers.size());
}

bool is_level_enabled(NotifyLevel level) {
    switch (level) {
        case NotifyLevel::Success: return s_filter_success;
        case NotifyLevel::Error:   return s_filter_error;
        case NotifyLevel::Warning: return s_filter_warning;
        case NotifyLevel::Info:    return s_filter_info;
    }
    return true;
}

void send_to_providers(const std::string& title, const std::string& body, NotifyLevel level) {
    if (!s_providers_loaded) load_provider_config();

    std::vector<ProviderState> enabled;
    {
        std::lock_guard<std::mutex> lock(s_providers_mutex);
        for (const auto& p : s_providers) {
            if (p.enabled) enabled.push_back(p);
        }
    }

    if (enabled.empty()) return;

    // Fire-and-forget on background thread
    std::thread([enabled, title, body, level]() {
        for (const auto& p : enabled) {
            try {
                bool ok = send_to_provider(p.id, p.config, title, body, level);
                LOG_DEBUG(TAG, "Provider %s: %s", p.id.c_str(), ok ? "sent" : "failed");
            } catch (const std::exception& e) {
                LOG_ERROR(TAG, "Provider %s error: %s", p.id.c_str(), e.what());
            }
        }
    }).detach();
}

bool test_provider(const std::string& provider_id) {
    if (!s_providers_loaded) load_provider_config();

    std::map<std::string, std::string> cfg;
    {
        std::lock_guard<std::mutex> lock(s_providers_mutex);
        for (const auto& p : s_providers) {
            if (p.id == provider_id) {
                cfg = p.config;
                break;
            }
        }
    }

    return send_to_provider(provider_id, cfg,
                             "Fincept Terminal",
                             "Test notification — your provider is configured correctly!",
                             NotifyLevel::Info);
}

void send(const std::string& title, const std::string& body, NotifyLevel level) {
    LOG_INFO(TAG, "Notification: [%s] %s — %s",
             toast_icon(level), title.c_str(), body.c_str());

    // Always queue an ImGui toast (regardless of filters)
    {
        std::lock_guard<std::mutex> lock(s_toast_mutex);
        float dur = toast_duration(level);
        s_toasts.push_back({title, body, level, dur, dur, s_next_id++});
        while (static_cast<int>(s_toasts.size()) > MAX_VISIBLE_TOASTS) {
            s_toasts.pop_front();
        }
    }

    // OS notification (fire-and-forget)
    send_os_notification(title, body, level);

    // External providers — only if this level is enabled
    if (is_level_enabled(level)) {
        send_to_providers(title, body, level);
    }
}

void render_toasts() {
    std::lock_guard<std::mutex> lock(s_toast_mutex);
    if (s_toasts.empty()) return;

    float dt = ImGui::GetIO().DeltaTime;
    ImVec2 display = ImGui::GetIO().DisplaySize;
    float y_offset = display.y - TOAST_PADDING;

    for (auto& t : s_toasts) t.lifetime -= dt;
    s_toasts.erase(
        std::remove_if(s_toasts.begin(), s_toasts.end(),
                       [](const Toast& t) { return t.lifetime <= 0.0f; }),
        s_toasts.end());

    for (int i = static_cast<int>(s_toasts.size()) - 1; i >= 0; --i) {
        auto& t = s_toasts[i];
        float alpha = 1.0f;
        float age = t.max_life - t.lifetime;
        if (age < TOAST_FADE_TIME) alpha = age / TOAST_FADE_TIME;
        else if (t.lifetime < TOAST_FADE_TIME) alpha = t.lifetime / TOAST_FADE_TIME;

        float text_h = ImGui::CalcTextSize(t.body.c_str(), nullptr, false, TOAST_WIDTH - 40).y;
        float toast_h = 20.0f + text_h + 12.0f;
        y_offset -= toast_h + 4.0f;
        float x_pos = display.x - TOAST_WIDTH - TOAST_PADDING;

        ImGui::SetNextWindowPos(ImVec2(x_pos, y_offset));
        ImGui::SetNextWindowSize(ImVec2(TOAST_WIDTH, toast_h));
        ImGui::SetNextWindowBgAlpha(0.92f * alpha);

        char win_id[32];
        std::snprintf(win_id, sizeof(win_id), "##toast_%lld", (long long)t.id);

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
                                 ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoFocusOnAppearing |
                                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize |
                                 ImGuiWindowFlags_NoDocking;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 6));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.08f, 0.10f, 1.0f));

        if (ImGui::Begin(win_id, nullptr, flags)) {
            ImVec4 accent = toast_color(t.level);
            accent.w = alpha;
            ImVec2 p = ImGui::GetWindowPos();
            ImGui::GetWindowDrawList()->AddRectFilled(
                p, ImVec2(p.x + 3.0f, p.y + toast_h),
                ImGui::ColorConvertFloat4ToU32(accent));

            ImGui::SetCursorPosX(12.0f);
            ImVec4 title_col = toast_color(t.level);
            title_col.w = alpha;
            ImGui::TextColored(title_col, "%s %s", toast_icon(t.level), t.title.c_str());

            ImGui::SetCursorPosX(12.0f);
            ImVec4 body_col = {0.8f, 0.8f, 0.8f, alpha};
            ImGui::PushTextWrapPos(TOAST_WIDTH - 16);
            ImGui::TextColored(body_col, "%s", t.body.c_str());
            ImGui::PopTextWrapPos();
        }
        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);
    }
}

void clear() {
    std::lock_guard<std::mutex> lock(s_toast_mutex);
    s_toasts.clear();
}

void set_os_notifications_enabled(bool enabled) {
    s_os_enabled = enabled;
    LOG_INFO(TAG, "OS notifications %s", enabled ? "enabled" : "disabled");
}

bool os_notifications_enabled() {
    return s_os_enabled;
}

} // namespace notify
} // namespace fincept::core
