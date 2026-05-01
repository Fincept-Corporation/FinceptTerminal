// LlmService.cpp — Multi-provider LLM API client (Qt port)
// Translated from fincept-cpp llm_service.cpp:
//   libcurl  → QNetworkAccessManager + QEventLoop (blocking on bg thread)
//   std::future → QtConcurrent::run + signal
//   nlohmann/json → QJsonObject/QJsonArray

#include "ai_chat/LlmService.h"

#include "ai_chat/ModelCatalog.h"

#include "core/logging/Logger.h"
#include "core/config/AppConfig.h"
#include "mcp/McpProvider.h"
#include "mcp/McpService.h"
#include "storage/repositories/LlmConfigRepository.h"
#include "storage/repositories/SettingsRepository.h"

#    include "datahub/DataHub.h"
#    include "datahub/TopicPolicy.h"

#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QRegularExpression>
#include <QSet>
#include <QThread>
#include <QTimer>
#include <QUuid>
#include <QVariant>
#include <QtConcurrent/QtConcurrent>

namespace fincept::ai_chat {

static constexpr const char* TAG = "LlmService";

// Per-thread tool policy override for the in-flight request. Set by chat()
// and chat_streaming() workers before invoking helpers; restored by the RAII
// guard. Replaces the old pattern of mutating `tools_enabled_` on the shared
// instance, which raced when the floating AiChatBubble and the AI Chat tab
// ran concurrently and the bubble's "restore" leaked tools=false back to the
// tab.
//
// `t_request_policy` controls what the format helpers do:
//   ToolPolicy::All           — attach the global tool catalog as-is.
//   ToolPolicy::NoNavigation  — attach tools but exclude the `navigation`
//                               category (floating bubble: model can call
//                               benign tools like add_to_watchlist without
//                               redirecting the user's active screen).
//   ToolPolicy::None          — attach no tools (kept for backward compat).
static thread_local LlmService::ToolPolicy t_request_policy = LlmService::ToolPolicy::All;

namespace {
struct ToolPolicyGuard {
    LlmService::ToolPolicy prev;
    explicit ToolPolicyGuard(LlmService::ToolPolicy p) : prev(t_request_policy) { t_request_policy = p; }
    ~ToolPolicyGuard() { t_request_policy = prev; }
    ToolPolicyGuard(const ToolPolicyGuard&) = delete;
    ToolPolicyGuard& operator=(const ToolPolicyGuard&) = delete;
};
} // namespace

// True if the current request should attach any tools at all.
static bool effective_tools_enabled(bool global_tools_enabled) {
    return global_tools_enabled && t_request_policy != LlmService::ToolPolicy::None;
}

// True if the current request should hide the `navigation` category.
static bool should_hide_navigation() {
    return t_request_policy == LlmService::ToolPolicy::NoNavigation;
}

// Apply per-request policy on top of the LlmService-level tool_filter_.
// Currently this only injects the `navigation` exclusion for the floating
// bubble — but it's the right place to grow other per-request constraints.
static mcp::ToolFilter apply_request_policy(const mcp::ToolFilter& base) {
    mcp::ToolFilter out = base;
    if (should_hide_navigation() && !out.exclude_categories.contains(QStringLiteral("navigation")))
        out.exclude_categories.append(QStringLiteral("navigation"));
    return out;
}

// ============================================================================
// Content extractors (shared across non-streaming paths and tool follow-ups)
// ============================================================================

// Extract user-visible text from an OpenAI-compatible `message` object.
// Handles: plain `content` string, reasoning-only replies (`reasoning_content`),
// content-as-parts arrays (some providers echo OpenAI Responses-style parts),
// and refusal messages. Never returns the empty string when the API actually
// replied with something — the caller can rely on `isEmpty()` meaning "no reply".
static QString extract_openai_message_text(const QJsonObject& msg) {
    // Plain string content — the standard shape
    QJsonValue cv = msg["content"];
    if (cv.isString()) {
        QString s = cv.toString();
        if (!s.isEmpty())
            return s;
    } else if (cv.isArray()) {
        // Some providers (GPT-4o vision, OpenRouter multimodal echoes) return
        // content as an array of {type, text} parts. Concatenate all text parts.
        QString joined;
        for (const auto& pv : cv.toArray()) {
            QJsonObject p = pv.toObject();
            QString type = p["type"].toString();
            if (type == "text" || type == "output_text")
                joined += p["text"].toString();
        }
        if (!joined.isEmpty())
            return joined;
    }

    // Reasoning models (kimi-k2.5/k2.6/k2-thinking, deepseek-reasoner, some
    // xAI grok-4 reasoning variants) put the final answer in `reasoning_content`
    // when max_tokens is exhausted mid-reasoning, or when `content` is deliberately
    // empty. Fall back so the user sees the chain-of-thought instead of nothing.
    QString rc = msg["reasoning_content"].toString();
    if (!rc.isEmpty())
        return rc;

    // Some OpenAI-compatible providers (newer OpenAI, Groq) emit a `refusal`
    // field instead of content when the model declines. Surfacing it is better
    // than returning blank.
    QString refusal = msg["refusal"].toString();
    if (!refusal.isEmpty())
        return refusal;

    return {};
}

// Extract user-visible text from an Anthropic /v1/messages content-blocks array.
// Handles: `text` blocks (the normal case), `thinking` blocks (extended-thinking
// whose text lives in block.thinking — falls back only when no text block exists),
// and concatenates multiple text blocks (Claude can emit several when a tool_use
// block sits between them).
static QString extract_anthropic_content_text(const QJsonArray& content) {
    QString text;
    QString thinking_fallback;
    for (const auto& bv : content) {
        QJsonObject b = bv.toObject();
        const QString type = b["type"].toString();
        if (type == "text") {
            text += b["text"].toString();
        } else if (type == "thinking" && thinking_fallback.isEmpty()) {
            thinking_fallback = b["thinking"].toString();
        }
    }
    if (!text.isEmpty())
        return text;
    return thinking_fallback; // may be empty — caller decides
}

// Extract user-visible text from a Gemini candidate's parts array.
// Handles: multiple `text` parts (Gemini often splits long replies),
// `thought:true` parts (extended thinking — only fallback if no normal text),
// and silently skips `functionCall` parts which the caller handles separately.
static QString extract_gemini_parts_text(const QJsonArray& parts) {
    QString text;
    QString thought_fallback;
    for (const auto& pv : parts) {
        QJsonObject p = pv.toObject();
        if (p.contains("functionCall"))
            continue;
        QString t = p["text"].toString();
        if (t.isEmpty())
            continue;
        if (p["thought"].toBool()) {
            if (thought_fallback.isEmpty())
                thought_fallback = t;
        } else {
            text += t;
        }
    }
    if (!text.isEmpty())
        return text;
    return thought_fallback;
}

// ============================================================================
// Singleton
// ============================================================================

LlmService::LlmService() = default;

LlmService& LlmService::instance() {
    static LlmService s;
    return s;
}

void LlmService::reload_config() {
    {
        QMutexLocker lock(&mutex_);
        config_loaded_ = false;
        ensure_config();
    }
    emit config_changed();
}

void LlmService::ensure_config() const {
    // Called with mutex_ held
    if (config_loaded_)
        return;

    provider_ = model_ = api_key_ = base_url_ = system_prompt_ = {};
    temperature_ = 0.7;
    // 0 = "no user override; use ModelCatalog::output_cap(provider, model)".
    // The DB still holds whatever the user picked (including the legacy
    // 2000-token default). A v15 migration resets historical 2000 values
    // to 0; users who deliberately picked 2000 will get the model default
    // — that's the right call given 2000 was never a meaningful user choice.
    max_tokens_ = 0;
    tools_enabled_ = true;

    auto providers = LlmConfigRepository::instance().list_providers();
    if (providers.is_ok()) {
        for (const auto& c : providers.value()) {
            if (c.is_active) {
                provider_ = c.provider.toLower();
                api_key_ = c.api_key;
                base_url_ = c.base_url;
                model_ = c.model;
                tools_enabled_ = c.tools_enabled;
                break;
            }
        }
        // Fallback: first config if none active
        if (provider_.isEmpty() && !providers.value().isEmpty()) {
            const auto& c = providers.value().first();
            provider_ = c.provider.toLower();
            api_key_ = c.api_key;
            base_url_ = c.base_url;
            model_ = c.model;
            tools_enabled_ = c.tools_enabled;
        }
    }

    // Fallback: if no provider configured, use Fincept with session API key
    if (provider_.isEmpty()) {
        provider_ = "fincept";
        model_ = "MiniMax-M2.7";
        base_url_ = {};
        LOG_INFO(TAG, "No LLM provider configured — using Fincept default");
    }

    // Fincept always resolves API key from session (never stored in llm_configs)
    if (provider_ == "fincept") {
        auto stored_key = SettingsRepository::instance().get("fincept_api_key");
        if (stored_key.is_ok() && !stored_key.value().isEmpty())
            api_key_ = stored_key.value();
    }

    auto gs = LlmConfigRepository::instance().get_global_settings();
    if (gs.is_ok()) {
        temperature_ = gs.value().temperature;
        max_tokens_ = gs.value().max_tokens;
        system_prompt_ = gs.value().system_prompt;
    }

    // Inject default system prompt when user hasn't configured one.
    // This tells the model it is running inside the Fincept Terminal and
    // should use the provided tools (navigation, market data, portfolio, etc.)
    // rather than declining requests it can actually fulfil via a tool call.
    if (system_prompt_.trimmed().isEmpty()) {
        system_prompt_ =
            "You are Fincept AI, the intelligent assistant embedded inside the Fincept Terminal — "
            "a professional desktop financial intelligence application. You have access to tools that "
            "let you interact with the terminal directly: navigate screens, fetch live market data, "
            "manage watchlists, query portfolios, paper-trade, run Python analytics, search SEC Edgar "
            "filings, fetch news, and BUILD REPORTS LIVE in the Report Builder.\n"
            "\n"
            "Behaviour rules:\n"
            "• ALWAYS use a tool when one can fulfil the request — never decline an action that a tool "
            "  exists for. Never tell the user you cannot navigate or open screens.\n"
            "• Building a report (e.g. 'create an equity research report on TSLA'): your job is to "
            "  WRITE THE REPORT INTO THE REPORT BUILDER USING TOOLS. Do not narrate the report into "
            "  the chat. The flow is: (1) optionally call report_apply_template with the closest match "
            "  for context, (2) call report_get_state to learn the current component ids, (3) gather "
            "  data with tools like get_quote, edgar_get_financials, edgar_10k_sections, "
            "  edgar_calc_multiples, get_news, search_news, (4) populate the report by calling "
            "  report_update_component or report_add_component for each section. Use stable component "
            "  ids returned by report_get_state / report_add_component — never indices.\n"
            "• Report formatting (CRITICAL for a polished result):\n"
            "  - text/list/quote/callout content SUPPORTS MARKDOWN. Use **bold** to highlight key "
            "    figures (e.g. 'Revenue grew **22% YoY** to **$96.8B**'). Use *italic* sparingly. "
            "    Do not paste raw asterisks expecting them to render — they do, but only inside the "
            "    content of those component types.\n"
            "  - For tables, ALWAYS pass real data via config={'csv':'Header1,Header2|Cell1,Cell2|...'}. "
            "    Pipe `|` separates rows, comma `,` separates cells. First row is auto-bolded. Never "
            "    leave a table empty — it renders as 'Header 1, Header 2, ...' placeholder text.\n"
            "  - For charts, pass config={'chart_type':'bar'|'line'|'pie','title':...,'data':'1,2,3',"
            "    'labels':'Q1,Q2,Q3'}.\n"
            "  - Set proper metadata FIRST via report_set_metadata: title (e.g. 'Tesla Equity Research "
            "    Report'), author (e.g. 'Fincept Research'), company, and date. Don't leave 'Analyst' "
            "    or 'Untitled Report' defaults.\n"
            "  - Avoid one-line ramblings. Each text component should be a tight paragraph.\n"
            "• Python scripts: ONLY pass script names returned by list_python_scripts. Never invent or "
            "  guess script names. If list_python_scripts returns nothing useful, fall back to other "
            "  tools (get_quote, edgar_*, get_candles, etc.) — those are the canonical data sources.\n"
            "• When you have completed the user's request, reply with a concise summary in chat. "
            "  Do not paste the full report content into chat — the report lives in the Report "
            "  Builder canvas and the user is watching it fill in.\n"
            "Be concise, accurate, and finance-focused.";
    }

    // ── Tier-3 / Tool RAG discovery hint ─────────────────────────────────
    //
    // Tool RAG (Tool Search) ships only ~6 tools to the LLM each turn (the
    // Tier-0 set: tool.list, tool.describe, navigate, list_tabs,
    // get_current_tab, get_auth_status). Everything else is discovered on
    // demand via tool.list(query) — this fixes the 30-50-tool accuracy
    // collapse documented by Anthropic.
    //
    // For the LLM to form good queries it needs to know WHAT categories
    // exist. We enumerate them dynamically from the registered tools so the
    // hint stays accurate when categories are added/removed. Built once per
    // ensure_config() pass, cached statically across requests so the prompt
    // prefix is byte-stable for prompt-cache hits.
    //
    // Idempotent append (`[Tool discovery]` sentinel guards against
    // double-stacking on reload).
    if (tools_enabled_ && !system_prompt_.contains("[Tool discovery]")) {
        // Static cache — the tool registry is immutable after McpInit. Any
        // future runtime registrations will see a stale category list until
        // the next process restart, which is acceptable for a hint string.
        static const QString kHint = []() -> QString {
            const auto all = mcp::McpProvider::instance().list_all_tools();
            QSet<QString> cats;
            for (const auto& t : all) {
                if (!t.category.isEmpty())
                    cats.insert(t.category);
            }
            QStringList sorted_cats = cats.values();
            std::sort(sorted_cats.begin(), sorted_cats.end());

            QString hint =
                "\n\n[Tool discovery] You see only a small subset of tools each turn. "
                "To find a tool for any action you don't already have, call "
                "tool.list(query=\"<natural-language description>\"). "
                "It returns the top 5 most relevant tools (BM25-ranked). "
                "Then call tool.describe(name) for the full input schema, then invoke it. "
                "For requests with multiple intents (\"get news AND add to watchlist\"), "
                "call tool.list MULTIPLE TIMES — once per intent. "
                "Never decline an action you can fulfil via a discoverable tool.";
            if (!sorted_cats.isEmpty()) {
                hint += "\nAvailable tool categories: " + sorted_cats.join(", ") + ".";
            }
            return hint;
        }();
        system_prompt_ += kHint;
    }

    config_loaded_ = true;
    const int resolved = resolved_max_tokens();
    const int catalog_cap = ModelCatalog::output_cap(provider_, model_);
    LOG_INFO(TAG, QString("LLM config loaded: provider=%1 model=%2 tools_enabled=%3 "
                          "max_tokens(user=%4 catalog=%5 resolved=%6)")
                      .arg(provider_, model_, tools_enabled_ ? "TRUE" : "FALSE")
                      .arg(max_tokens_).arg(catalog_cap).arg(resolved));
}

// ============================================================================
// Accessors
// ============================================================================

QString LlmService::active_provider() const {
    QMutexLocker lock(&mutex_);
    ensure_config();
    return provider_;
}

QString LlmService::active_model() const {
    QMutexLocker lock(&mutex_);
    ensure_config();
    return model_;
}

QString LlmService::active_api_key() const {
    QMutexLocker lock(&mutex_);
    ensure_config();
    return api_key_;
}

QString LlmService::active_base_url() const {
    QMutexLocker lock(&mutex_);
    ensure_config();
    return base_url_;
}

double LlmService::active_temperature() const {
    QMutexLocker lock(&mutex_);
    ensure_config();
    return temperature_;
}

int LlmService::active_max_tokens() const {
    QMutexLocker lock(&mutex_);
    ensure_config();
    return max_tokens_;
}

bool LlmService::tools_enabled() const {
    QMutexLocker lock(&mutex_);
    ensure_config();
    return tools_enabled_;
}

bool LlmService::is_configured() const {
    QMutexLocker lock(&mutex_);
    ensure_config();
    if (provider_.isEmpty())
        return false;
    if (provider_requires_api_key(provider_))
        return !api_key_.isEmpty();
    return true;
}

ResolvedLlmProfile LlmService::resolve_profile(const QString& context_type, const QString& context_id) const {
    return LlmProfileRepository::instance().resolve_for_context(context_type, context_id);
}

// static
QJsonObject LlmService::profile_to_json(const ResolvedLlmProfile& p) {
    // Only include fields the Python model constructor accepts.
    // profile_id / profile_name are internal metadata — never send them to Python
    // or they land in extra_model_kwargs and fail as unexpected kwargs (e.g. Claude(profile_id=...)).
    QJsonObject obj;
    obj["provider"] = p.provider;
    obj["model_id"] = p.model_id;
    obj["api_key"] = p.api_key;
    obj["base_url"] = p.base_url;
    obj["temperature"] = p.temperature;
    obj["max_tokens"] = p.max_tokens;
    if (!p.system_prompt.isEmpty())
        obj["system_prompt"] = p.system_prompt;
    return obj;
}

// ============================================================================
// Endpoint + headers
// ============================================================================

int LlmService::resolved_max_tokens() const {
    // Called with mutex_ held by the caller (build_*_request paths).
    constexpr int kFallback = 8192;
    const int catalog_cap = ModelCatalog::output_cap(provider_, model_);

    // User-set value (loaded from llm_global_settings or per-profile).
    // Treat <=0 as "unset — use model default".
    if (max_tokens_ > 0) {
        // User asked for a specific number — honour it but clamp to the
        // model's published cap so we don't get a 400 from the API.
        if (catalog_cap > 0 && max_tokens_ > catalog_cap)
            return catalog_cap;
        return max_tokens_;
    }

    if (catalog_cap > 0)
        return catalog_cap;
    return kFallback;
}

QString LlmService::get_endpoint_url() const {
    // Called with mutex_ held
    const QString& p = provider_;

    // Fincept: two endpoints — sync chat and async LLM
    // base_url_ stores the base domain; append path here.
    if (p == "fincept") {
        // sync endpoint for chat (short replies)
        return "https://api.fincept.in/research/chat";
    }

    // Custom base_url takes priority for other providers
    if (!base_url_.isEmpty()) {
        QString base = base_url_;
        if (base.endsWith('/'))
            base.chop(1);
        if (p == "anthropic")
            return base + "/v1/messages";
        return base + "/v1/chat/completions";
    }

    if (p == "openai")
        return "https://api.openai.com/v1/chat/completions";
    if (p == "anthropic")
        return "https://api.anthropic.com/v1/messages";
    if (p == "gemini" || p == "google")
        return "https://generativelanguage.googleapis.com/v1beta/models/" + model_ + ":generateContent";
    if (p == "groq")
        return "https://api.groq.com/openai/v1/chat/completions";
    if (p == "deepseek")
        return "https://api.deepseek.com/v1/chat/completions";
    if (p == "openrouter")
        return "https://openrouter.ai/api/v1/chat/completions";
    if (p == "minimax")
        return "https://api.minimax.io/v1/chat/completions";
    if (p == "kimi")
        return "https://api.moonshot.ai/v1/chat/completions";
    if (p == "ollama")
        return "http://localhost:11434/v1/chat/completions";
    if (p == "xai")
        return "https://api.x.ai/v1/chat/completions";
    return {};
}

QMap<QString, QString> LlmService::get_headers() const {
    // Called with mutex_ held
    QMap<QString, QString> h;
    const QString& p = provider_;

    if (p == "anthropic") {
        if (!api_key_.isEmpty())
            h["x-api-key"] = api_key_;
        h["anthropic-version"] = "2023-06-01";
    } else if (p == "gemini" || p == "google") {
        if (!api_key_.isEmpty())
            h["x-goog-api-key"] = api_key_;
    } else if (p == "fincept") {
        // api_key_ already resolved from session by ensure_config()
        if (!api_key_.isEmpty())
            h["X-API-Key"] = api_key_;
        auto tok = SettingsRepository::instance().get("session_token");
        if (tok.is_ok() && !tok.value().isEmpty())
            h["X-Session-Token"] = tok.value();
        // Cloudflare requires a User-Agent header
        h["User-Agent"] = "FinceptTerminal/4.0";
    } else {
        // OpenAI-compatible
        if (!api_key_.isEmpty())
            h["Authorization"] = "Bearer " + api_key_;
        if (p == "openrouter") {
            // Optional attribution — appears on openrouter.ai/rankings leaderboard
            h["HTTP-Referer"] = "https://fincept.in";
            h["X-Title"] = "Fincept Terminal";
        }
    }
    return h;
}

// ============================================================================
// Request builders
// ============================================================================

QJsonObject LlmService::build_openai_request(const QString& user_message,
                                             const std::vector<ConversationMessage>& history, bool stream,
                                             bool with_tools) {
    const QString model_lower = model_.toLower();

    // deepseek-reasoner doesn't accept `tools`.
    const bool is_ds_reasoner = (provider_ == "deepseek" && model_lower.contains("reasoner"));

    // Groq's whisper-* (audio) and llama-guard-* (safety classifier) do not accept tools.
    const bool groq_no_tools =
        (provider_ == "groq" && (model_lower.startsWith("whisper-") || model_lower.contains("llama-guard")));

    QJsonArray messages;
    if (!system_prompt_.isEmpty())
        messages.append(QJsonObject{{"role", "system"}, {"content", system_prompt_}});
    for (const auto& m : history)
        messages.append(QJsonObject{{"role", m.role}, {"content", m.content}});
    messages.append(QJsonObject{{"role", "user"}, {"content", user_message}});

    QJsonObject req;
    req["model"] = model_;
    req["messages"] = messages;
    // Temperature intentionally omitted — each provider uses its own default.
    // OpenAI deprecated max_tokens; gpt-5 / o-series require max_completion_tokens.
    // xAI also prefers max_completion_tokens. Other OpenAI-compatible providers
    // still expect max_tokens.
    const int mx = resolved_max_tokens();
    if (provider_ == "openai" || provider_ == "xai")
        req["max_completion_tokens"] = mx;
    else
        req["max_tokens"] = mx;
    if (stream) {
        req["stream"] = true;
        // Streamed OpenAI / xAI responses omit usage unless we opt in
        if (provider_ == "openai" || provider_ == "xai")
            req["stream_options"] = QJsonObject{{"include_usage", true}};
    }

    // Send tools on BOTH streaming and non-streaming requests. The streaming
    // path detects a tool-call response (finish_reason="tool_calls" or
    // delta.tool_calls) and falls back to non-streaming do_request, which
    // executes the tools and follows up. Without sending tools on stream,
    // the model has no idea they exist and answers from training data —
    // which silently breaks live tool calling for OpenAI/Kimi/Groq/etc.
    // deepseek-reasoner rejects tools entirely; some Groq models also.
    const bool tools_effectively_on = effective_tools_enabled(tools_enabled_);
    if (with_tools && tools_effectively_on && !is_ds_reasoner && !groq_no_tools) {
        QJsonArray tools = mcp::McpService::instance().format_tools_for_openai(apply_request_policy(tool_filter_));
        if (!tools.isEmpty())
            req["tools"] = tools;
        LOG_INFO(TAG, QString("OpenAI request: stream=%1 provider=%2 tools=%3 (count=%4)")
                          .arg(stream ? "true" : "false", provider_,
                               tools.isEmpty() ? "none" : "attached")
                          .arg(tools.size()));
    } else {
        LOG_WARN(TAG, QString("OpenAI request: stream=%1 provider=%2 NO TOOLS — "
                              "with_tools=%3 tools_effectively_on=%4 ds_reasoner=%5 groq_no_tools=%6")
                          .arg(stream ? "true" : "false", provider_)
                          .arg(with_tools ? "true" : "false")
                          .arg(tools_effectively_on ? "true" : "false")
                          .arg(is_ds_reasoner ? "true" : "false")
                          .arg(groq_no_tools ? "true" : "false"));
    }
    return req;
}

QJsonObject LlmService::build_anthropic_request(const QString& user_message,
                                                const std::vector<ConversationMessage>& history, bool stream) {
    QJsonArray messages;
    for (const auto& m : history) {
        if (m.role != "system")
            messages.append(QJsonObject{{"role", m.role}, {"content", m.content}});
    }
    messages.append(QJsonObject{{"role", "user"}, {"content", user_message}});

    QJsonObject req;
    req["model"] = model_;
    req["messages"] = messages;
    req["max_tokens"] = resolved_max_tokens();
    // Temperature intentionally omitted — Anthropic defaults to 1.0.
    if (!system_prompt_.isEmpty())
        req["system"] = system_prompt_;
    if (stream)
        req["stream"] = true;

    // Anthropic tool format: array of {name, description, input_schema}
    // (no "type":"function" wrapper like OpenAI). Tools are sent for both
    // streaming and non-streaming requests so the model can request a
    // tool_use even mid-stream — the streaming code path detects this and
    // falls back to do_request to execute and follow up.
    if (effective_tools_enabled(tools_enabled_)) {
        QJsonArray ant_tools;
        auto all_tools = mcp::McpService::instance().get_all_tools(apply_request_policy(tool_filter_));
        for (const auto& tool : all_tools) {
            QString fn_name = tool.server_id + "__" + mcp::McpProvider::encode_tool_name_for_wire(tool.name);
            QJsonObject schema = tool.input_schema;
            if (schema.isEmpty()) {
                schema["type"] = "object";
                schema["properties"] = QJsonObject();
            }
            ant_tools.append(
                QJsonObject{{"name", fn_name}, {"description", tool.description}, {"input_schema", schema}});
        }
        if (!ant_tools.isEmpty())
            req["tools"] = ant_tools;
    }
    return req;
}

QJsonObject LlmService::build_gemini_request(const QString& user_message,
                                             const std::vector<ConversationMessage>& history) {
    QJsonArray contents;
    for (const auto& m : history) {
        if (m.role == "system")
            continue;
        QString role = (m.role == "assistant") ? "model" : "user";
        contents.append(QJsonObject{{"role", role}, {"parts", QJsonArray{QJsonObject{{"text", m.content}}}}});
    }
    contents.append(QJsonObject{{"role", "user"}, {"parts", QJsonArray{QJsonObject{{"text", user_message}}}}});

    QJsonObject gen_cfg;
    // Temperature intentionally omitted — Gemini defaults to 1.0.
    gen_cfg["maxOutputTokens"] = resolved_max_tokens();

    QJsonObject req;
    req["contents"] = contents;
    req["generationConfig"] = gen_cfg;
    if (!system_prompt_.isEmpty()) {
        req["systemInstruction"] = QJsonObject{{"parts", QJsonArray{QJsonObject{{"text", system_prompt_}}}}};
    }

    // Gemini tool format: tools[{functionDeclarations:[{name, description, parameters}]}]
    auto all_tools = effective_tools_enabled(tools_enabled_)
                         ? mcp::McpService::instance().get_all_tools(apply_request_policy(tool_filter_))
                         : std::vector<mcp::UnifiedTool>{};
    if (!all_tools.empty()) {
        QJsonArray fn_decls;
        for (const auto& tool : all_tools) {
            QString fn_name = tool.server_id + "__" + mcp::McpProvider::encode_tool_name_for_wire(tool.name);
            QJsonObject schema = tool.input_schema;
            if (schema.isEmpty()) {
                schema["type"] = "object";
                schema["properties"] = QJsonObject();
            }
            fn_decls.append(QJsonObject{{"name", fn_name}, {"description", tool.description}, {"parameters", schema}});
        }
        if (!fn_decls.isEmpty())
            req["tools"] = QJsonArray{QJsonObject{{"functionDeclarations", fn_decls}}};
    }

    return req;
}

QJsonObject LlmService::build_fincept_request(const QString& user_message,
                                              const std::vector<ConversationMessage>& history, bool with_tools) {
    // Fincept /research/chat uses the OpenAI messages array format
    QJsonArray messages;
    if (!system_prompt_.isEmpty())
        messages.append(QJsonObject{{"role", "system"}, {"content", system_prompt_}});
    for (const auto& m : history)
        messages.append(QJsonObject{{"role", m.role}, {"content", m.content}});
    messages.append(QJsonObject{{"role", "user"}, {"content", user_message}});

    QJsonObject req;
    req["messages"] = messages;
    // Pass model if set to a real model name (not the legacy placeholder)
    if (!model_.isEmpty() && model_ != "fincept-llm")
        req["model"] = model_;

    Q_UNUSED(with_tools) // Fincept /research/chat does not support tools yet
    return req;
}

// ============================================================================
// Blocking HTTP POST (runs on background thread, uses its own NAM + event loop)
// ============================================================================

LlmService::HttpResult LlmService::blocking_post(const QString& url, const QJsonObject& body,
                                                 const QMap<QString, QString>& headers, int timeout_ms) {
    // Delegate to eventloop_request which works reliably on background threads.
    // The old waitForReadyRead() approach failed for Cloudflare-protected endpoints
    // because QNetworkAccessManager requires an event loop for TLS/SSL negotiation.
    QByteArray json_data = QJsonDocument(body).toJson(QJsonDocument::Compact);
    return eventloop_request("POST", url, json_data, headers, timeout_ms);
}

// ============================================================================
// Blocking GET helper (mirrors blocking_post but without a body)
// ============================================================================

LlmService::HttpResult LlmService::blocking_get(const QString& url, const QMap<QString, QString>& headers,
                                                int timeout_ms) {
    return eventloop_request("GET", url, {}, headers, timeout_ms);
}

// ============================================================================
// Fincept async path — POST /research/llm/async → poll /research/llm/status/{id}
// Used for the primary LLM response (sync /research/chat for follow-ups)
// ============================================================================

// Helper: synchronous POST/GET using QEventLoop on a background thread.
// This is required for endpoints behind Cloudflare (like api.fincept.in)
// because QNetworkAccessManager needs an event loop to process TLS/SSL
// negotiation and HTTP redirects. The waitForReadyRead() approach used by
// blocking_post() works for some servers but fails for Cloudflare-protected ones.
LlmService::HttpResult LlmService::eventloop_request(const QString& method, const QString& url, const QByteArray& body,
                                                     const QMap<QString, QString>& headers, int timeout_ms) {
    HttpResult result;
    QNetworkAccessManager nam;
    QNetworkRequest req{QUrl(url)};
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    for (auto it = headers.constBegin(); it != headers.constEnd(); ++it)
        req.setRawHeader(it.key().toUtf8(), it.value().toUtf8());

    QNetworkReply* reply = (method == "GET") ? nam.get(req) : nam.post(req, body);

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    timer.start(timeout_ms);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (!reply->isFinished()) {
        reply->abort();
        reply->deleteLater();
        result.error = "Request timed out";
        return result;
    }

    result.status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    result.body = reply->readAll();
    result.success = (result.status >= 200 && result.status < 300);
    if (!result.success) {
        QString server_msg;
        auto err_doc = QJsonDocument::fromJson(result.body);
        if (!err_doc.isNull() && err_doc.isObject()) {
            QJsonObject ej = err_doc.object();
            // Top-level {"message": ...} (OpenAI/Anthropic legacy shape)
            if (ej.contains("message") && ej["message"].isString())
                server_msg = ej["message"].toString();
            // Nested {"error": {"message": ..., "metadata": {"raw": ...}}}
            // (OpenAI current, OpenRouter, DeepSeek, Groq all use this shape)
            if (server_msg.isEmpty() && ej.contains("error") && ej["error"].isObject()) {
                QJsonObject eo = ej["error"].toObject();
                if (eo.contains("message") && eo["message"].isString())
                    server_msg = eo["message"].toString();
                // OpenRouter surfaces upstream errors in metadata.raw
                if (eo.contains("metadata") && eo["metadata"].isObject()) {
                    QJsonObject md = eo["metadata"].toObject();
                    QString raw = md["raw"].toString();
                    QString provider_name = md["provider_name"].toString();
                    if (!raw.isEmpty())
                        server_msg += " (upstream " + provider_name + ": " + raw + ")";
                }
            }
        }
        result.error = server_msg.isEmpty() ? QString("HTTP %1: %2").arg(result.status).arg(reply->errorString())
                                            : QString("HTTP %1: %2").arg(result.status).arg(server_msg);
    }
    reply->deleteLater();
    return result;
}

// Build a compact tool catalog string for injection into the system prompt.
// This allows models that don't support structured tool_calls to still
// emit text-based tool invocations that try_extract_and_execute_text_tool_calls
// can detect and execute.
//
// Two modes:
//   • Tool RAG ON + default filter: emit only the Tier-0 tools + explicit
//     instructions to use tool.list for everything else. Same disclosure
//     model as structured providers — keeps the catalog small and forces
//     deliberate discovery.
//   • Tool RAG OFF or explicit filter: legacy behaviour — list up to 60
//     filtered tools inline.
static QString build_tool_catalog_for_prompt(const mcp::ToolFilter& filter) {
    auto all_tools = mcp::McpService::instance().get_all_tools(filter);
    if (all_tools.empty())
        return {};

    const bool default_filter = filter.categories.isEmpty() &&
                                filter.exclude_categories.isEmpty() &&
                                filter.name_patterns.isEmpty() &&
                                filter.exclude_name_patterns.isEmpty() &&
                                filter.max_tools == 0;
    const bool use_rag = default_filter &&
                         fincept::AppConfig::instance()
                             .get("mcp/use_tool_rag", QVariant(true))
                             .toBool();

    QString catalog;
    catalog += "You have access to tools. To use one, emit a <tool_call> block:\n";
    catalog += "<tool_call>{\"name\": \"TOOL_NAME\", \"arguments\": {\"param\": \"value\"}}</tool_call>\n\n";

    if (use_rag) {
        // ── Tier-0 mode ──
        // Mirror McpService::tier_0_tool_names() (kept in sync manually — small
        // list, low churn). Could be exposed via an accessor if it grows.
        static const QSet<QString> kTier0 = {
            "tool.list", "tool.describe", "navigate_to_tab", "list_tabs",
            "get_current_tab", "get_auth_status",
        };
        catalog += "Always-available tools:\n";
        for (const auto& tool : all_tools) {
            if (!kTier0.contains(tool.name))
                continue;
            QString fn_name = tool.server_id + "__" + mcp::McpProvider::encode_tool_name_for_wire(tool.name);
            catalog += "- " + fn_name + ": " + tool.description + "\n";
        }
        const QString wire_list = mcp::McpProvider::encode_tool_name_for_wire("tool.list");
        const QString wire_describe = mcp::McpProvider::encode_tool_name_for_wire("tool.describe");
        catalog += QStringLiteral(
            "\nFor any other capability, call fincept-terminal__%1 with a natural-language "
            "query (e.g. {\"query\": \"draft a research report\"}). It returns the top 5 most "
            "relevant tools. Then call fincept-terminal__%2(name) for the full schema, "
            "then invoke the tool. For multi-intent requests, call %1 multiple times.\n")
            .arg(wire_list, wire_describe);
        return catalog;
    }

    // ── Legacy mode ──
    catalog += "Available tools:\n";
    int count = 0;
    for (const auto& tool : all_tools) {
        QString fn_name = tool.server_id + "__" + mcp::McpProvider::encode_tool_name_for_wire(tool.name);
        catalog += "- " + fn_name + ": " + tool.description;
        QJsonObject props = tool.input_schema["properties"].toObject();
        if (!props.isEmpty()) {
            QStringList params;
            for (auto it = props.constBegin(); it != props.constEnd(); ++it)
                params.append(it.key());
            catalog += " (params: " + params.join(", ") + ")";
        }
        catalog += "\n";
        ++count;
        if (count >= 60) {
            catalog += "... and " + QString::number(all_tools.size() - 60) + " more tools available.\n";
            break;
        }
    }
    return catalog;
}

LlmResponse LlmService::fincept_async_request(const QString& user_message,
                                              const std::vector<ConversationMessage>& history) {
    LlmResponse resp;

    // Build prompt string for the async endpoint (it takes a plain prompt, not messages)
    QString prompt;
    if (!system_prompt_.isEmpty())
        prompt += system_prompt_ + "\n\n";

    // Inject tool catalog so the model can emit text-based tool calls
    if (effective_tools_enabled(tools_enabled_)) {
        QString tool_catalog = build_tool_catalog_for_prompt(apply_request_policy(tool_filter_));
        if (!tool_catalog.isEmpty())
            prompt += tool_catalog + "\n";
    }

    for (const auto& m : history) {
        if (m.role == "user")
            prompt += "User: " + m.content + "\n\n";
        else if (m.role == "assistant")
            prompt += "Assistant: " + m.content + "\n\n";
    }
    prompt += "User: " + user_message;

    QJsonObject submit_body;
    submit_body["prompt"] = prompt;
    submit_body["max_tokens"] = resolved_max_tokens();
    // Temperature intentionally omitted — Fincept backend uses its own default.

    auto hdr = get_headers();
    const QString async_url = "https://api.fincept.in/research/llm/async";
    const QString status_base = "https://api.fincept.in/research/llm/status/";

    LOG_INFO(TAG, QString("Fincept async: submitting to %1 (api_key=%2, prompt_len=%3)")
                      .arg(async_url)
                      .arg(api_key_.isEmpty() ? "EMPTY" : api_key_.left(12) + "...")
                      .arg(prompt.length()));

    QByteArray json_data = QJsonDocument(submit_body).toJson(QJsonDocument::Compact);
    auto submit = eventloop_request("POST", async_url, json_data, hdr, 30000);
    if (!submit.success) {
        resp.error = "Fincept async submit failed: " + submit.error;
        LOG_ERROR(TAG, resp.error);
        return resp;
    }

    auto submit_doc = QJsonDocument::fromJson(submit.body);
    if (submit_doc.isNull()) {
        resp.error = "Fincept async: failed to parse submit response";
        return resp;
    }

    // Response can nest task_id at top level or inside data
    QJsonObject sj = submit_doc.object();
    QString task_id = sj["task_id"].toString();
    if (task_id.isEmpty())
        task_id = sj["data"].toObject()["task_id"].toString();
    if (task_id.isEmpty()) {
        resp.error = "Fincept async: no task_id in submit response";
        return resp;
    }
    LOG_INFO(TAG, "Fincept async task_id: " + task_id);

    // Poll every 3 seconds, up to 120 seconds total
    const QString poll_url = status_base + task_id;
    constexpr int MAX_POLLS = 40;
    for (int i = 0; i < MAX_POLLS; ++i) {
        QThread::msleep(3000);

        auto poll = eventloop_request("GET", poll_url, {}, hdr, 15000);
        if (!poll.success) {
            LOG_WARN(TAG, "Fincept async poll failed: " + poll.error);
            continue;
        }

        auto poll_doc = QJsonDocument::fromJson(poll.body);
        if (poll_doc.isNull())
            continue;

        QJsonObject pj = poll_doc.object();
        QString status = pj["status"].toString();
        QJsonObject data_obj = pj["data"].toObject();
        if (status.isEmpty())
            status = data_obj["status"].toString();

        LOG_INFO(TAG, QString("Fincept async poll %1 status=%2").arg(i + 1).arg(status));

        if (status == "completed") {
            // data.data.response
            QString response = data_obj["data"].toObject()["response"].toString();
            if (response.isEmpty())
                response = data_obj["response"].toString();
            if (response.isEmpty()) {
                resp.error = "Fincept async completed but response is empty";
                LOG_WARN(TAG, "Fincept async task completed with empty response field");
                return resp;
            }
            resp.content = response;
            resp.success = true;

            QJsonObject usage = data_obj["data"].toObject()["usage"].toObject();
            if (!usage.isEmpty()) {
                resp.prompt_tokens = usage["input_tokens"].toInt();
                resp.completion_tokens = usage["output_tokens"].toInt();
                resp.total_tokens = usage["total_tokens"].toInt();
            }

            // Check for text-based tool calls in the response.
            // The model may have emitted <tool_call>...</tool_call> blocks.
            if (!resp.content.isEmpty()) {
                LOG_INFO(TAG, "Fincept: checking response for text-based tool calls");
                // Use the sync /research/chat endpoint for follow-up after tool execution
                QString followup_url = get_endpoint_url();
                auto followup_hdr = get_headers();
                auto tool_result =
                    try_extract_and_execute_text_tool_calls(resp.content, user_message, followup_url, followup_hdr);
                if (tool_result.has_value()) {
                    LOG_INFO(TAG, "Fincept: text tool calls detected and executed");
                    return tool_result.value();
                }
            }
            return resp;
        }

        if (status == "failed") {
            QString err = pj["error"].toString();
            if (err.isEmpty())
                err = data_obj["error"].toString();
            resp.error = "Fincept async task failed: " + (err.isEmpty() ? "unknown error" : err);
            return resp;
        }
    }

    resp.error = "Fincept async timed out waiting for response";
    return resp;
}

// ============================================================================
// Non-streaming request
// ============================================================================

LlmResponse LlmService::do_request(const QString& user_message, const std::vector<ConversationMessage>& history) {
    LlmResponse resp;

    QString url = get_endpoint_url();
    if (url.isEmpty()) {
        resp.error = "No endpoint URL for provider: " + provider_;
        return resp;
    }

    auto hdr = get_headers();
    QJsonObject req_body;

    if (provider_ == "anthropic") {
        req_body = build_anthropic_request(user_message, history, false);
    } else if (provider_ == "gemini" || provider_ == "google") {
        req_body = build_gemini_request(user_message, history);
        // Auth goes via x-goog-api-key header (set by get_headers()).
        // Do not append ?key= to URL — it leaks the key into access logs.
    } else if (provider_ == "fincept") {
        // Fincept uses two separate endpoints:
        // Primary response → async (submit + poll, returns richer model output)
        // Follow-ups (tool results) → sync /research/chat
        LOG_INFO(TAG, "do_request: routing to fincept_async_request");
        return fincept_async_request(user_message, history);
    } else {
        req_body = build_openai_request(user_message, history, false, true);
    }

    LOG_DEBUG(TAG, QString("POST %1 provider=%2 model=%3").arg(url, provider_, model_));

    auto http = blocking_post(url, req_body, hdr);

    if (!http.success) {
        resp.error = http.error;
        return resp;
    }

    auto doc = QJsonDocument::fromJson(http.body);
    if (doc.isNull()) {
        resp.error = "Failed to parse response JSON";
        return resp;
    }
    QJsonObject rj = doc.object();

    // ── Extract content by provider ──────────────────────────────────────

    if (provider_ == "anthropic") {
        QJsonArray content = rj["content"].toArray();
        QString stop_reason = rj["stop_reason"].toString();

        // Check for tool_use blocks
        if (stop_reason == "tool_use") {
            LOG_INFO(TAG, "Anthropic requested tool_use");

            // Build follow-up messages: original + assistant turn + tool results
            QJsonArray loop_msgs;
            if (!system_prompt_.isEmpty()) {
            } // system is top-level in Anthropic, not in messages
            for (const auto& h : history)
                if (h.role != "system")
                    loop_msgs.append(QJsonObject{{"role", h.role}, {"content", h.content}});
            loop_msgs.append(QJsonObject{{"role", "user"}, {"content", user_message}});
            // Assistant message with the tool_use content blocks
            loop_msgs.append(QJsonObject{{"role", "assistant"}, {"content", content}});

            // Execute each tool_use block and collect results
            QJsonArray tool_results;
            for (const auto& block_val : content) {
                QJsonObject block = block_val.toObject();
                if (block["type"].toString() != "tool_use")
                    continue;
                QString tool_id = block["id"].toString();
                QString tool_name = block["name"].toString();
                QJsonObject input = block["input"].toObject();

                LOG_INFO(TAG, "Executing Anthropic tool: " + tool_name);
                auto tr = mcp::McpService::instance().execute_openai_function(tool_name, input);
                tool_results.append(QJsonObject{
                    {"type", "tool_result"},
                    {"tool_use_id", tool_id},
                    {"content", QString::fromUtf8(QJsonDocument(tr.to_json()).toJson(QJsonDocument::Compact))}});
            }

            // Follow-up: user turn with tool_result blocks
            loop_msgs.append(QJsonObject{{"role", "user"}, {"content", tool_results}});

            // Build follow-up request (no tools to avoid infinite loop)
            QJsonObject fu;
            fu["model"] = model_;
            fu["messages"] = loop_msgs;
            fu["max_tokens"] = resolved_max_tokens();
            // Temperature intentionally omitted — Anthropic default.
            if (!system_prompt_.isEmpty())
                fu["system"] = system_prompt_;

            auto fu_http = blocking_post(url, fu, hdr);
            if (fu_http.success) {
                auto fu_doc = QJsonDocument::fromJson(fu_http.body);
                if (!fu_doc.isNull())
                    resp.content = extract_anthropic_content_text(fu_doc.object()["content"].toArray());
            } else {
                resp.error = "Anthropic tool follow-up failed: " + fu_http.error;
                return resp;
            }
        } else {
            // Normal text response — concatenate all text blocks and fall back
            // to extended-thinking content if no text was emitted (e.g. when
            // max_tokens is exhausted mid-thinking).
            resp.content = extract_anthropic_content_text(content);
        }

    } else if (provider_ == "gemini" || provider_ == "google") {
        QJsonArray cands = rj["candidates"].toArray();
        if (!cands.isEmpty()) {
            QJsonArray parts = cands[0].toObject()["content"].toObject()["parts"].toArray();

            // Check for functionCall parts (Gemini tool use)
            bool has_function_calls = false;
            for (const auto& part_val : parts) {
                if (part_val.toObject().contains("functionCall")) {
                    has_function_calls = true;
                    break;
                }
            }

            if (has_function_calls) {
                LOG_INFO(TAG, "Gemini requested function call(s)");
                // Execute each tool call and build matching functionResponse parts.
                // Gemini's multi-turn contract: user turn → model turn with functionCall
                // parts → user turn with functionResponse parts (NOT plain text).
                QJsonArray fn_response_parts;
                for (const auto& part_val : parts) {
                    QJsonObject part = part_val.toObject();
                    if (!part.contains("functionCall"))
                        continue;
                    QJsonObject fc = part["functionCall"].toObject();
                    QString fn_name = fc["name"].toString();
                    QJsonObject fn_args = fc["args"].toObject();

                    LOG_INFO(TAG, "Executing Gemini tool: " + fn_name);
                    auto tr = mcp::McpService::instance().execute_openai_function(fn_name, fn_args);

                    // Build a JSON response object. Gemini requires response to be an
                    // object (string values are wrapped under a key).
                    QJsonObject response_obj;
                    if (!tr.data.isNull() && !tr.data.isUndefined() && tr.data.isObject())
                        response_obj = tr.data.toObject();
                    else if (!tr.message.isEmpty())
                        response_obj["result"] = tr.message;
                    else
                        response_obj = tr.to_json();

                    fn_response_parts.append(QJsonObject{
                        {"functionResponse",
                         QJsonObject{{"name", fn_name}, {"response", response_obj}}}});
                }

                if (!fn_response_parts.isEmpty()) {
                    // Rebuild contents: prior history + original user + model(functionCall) + user(functionResponse)
                    QJsonArray fu_contents;
                    for (const auto& m : history) {
                        if (m.role == "system")
                            continue;
                        QString role = (m.role == "assistant") ? "model" : "user";
                        fu_contents.append(QJsonObject{
                            {"role", role},
                            {"parts", QJsonArray{QJsonObject{{"text", m.content}}}}});
                    }
                    fu_contents.append(QJsonObject{
                        {"role", "user"},
                        {"parts", QJsonArray{QJsonObject{{"text", user_message}}}}});
                    fu_contents.append(QJsonObject{
                        {"role", "model"},
                        {"parts", parts}}); // echo the functionCall parts verbatim
                    fu_contents.append(QJsonObject{
                        {"role", "user"},
                        {"parts", fn_response_parts}});

                    QJsonObject fu_body;
                    fu_body["contents"] = fu_contents;
                    QJsonObject gen_cfg;
                    // Temperature intentionally omitted — Gemini default.
                    gen_cfg["maxOutputTokens"] = resolved_max_tokens();
                    fu_body["generationConfig"] = gen_cfg;
                    if (!system_prompt_.isEmpty())
                        fu_body["systemInstruction"] =
                            QJsonObject{{"parts", QJsonArray{QJsonObject{{"text", system_prompt_}}}}};

                    auto fu = blocking_post(url, fu_body, hdr);
                    if (fu.success) {
                        auto fu_doc = QJsonDocument::fromJson(fu.body);
                        if (!fu_doc.isNull()) {
                            QJsonArray fu_cands = fu_doc.object()["candidates"].toArray();
                            if (!fu_cands.isEmpty())
                                resp.content = extract_gemini_parts_text(
                                    fu_cands[0].toObject()["content"].toObject()["parts"].toArray());
                        }
                    }
                    if (resp.content.isEmpty()) {
                        // Fallback: render function responses as readable text
                        QString fallback;
                        for (const auto& pv : fn_response_parts) {
                            QJsonObject fr = pv.toObject()["functionResponse"].toObject();
                            QString fn_name = fr["name"].toString();
                            int sep = fn_name.indexOf("__");
                            QString short_name = (sep >= 0) ? fn_name.mid(sep + 2) : fn_name;
                            fallback += "\n**Tool: " + short_name + "**\n" +
                                        QString::fromUtf8(
                                            QJsonDocument(fr["response"].toObject()).toJson(QJsonDocument::Compact))
                                            .left(4000) +
                                        "\n";
                        }
                        resp.content = fallback;
                    }
                }
            } else {
                // Normal text response — concatenate all text parts; fall back
                // to `thought:true` parts if no normal text was emitted.
                resp.content = extract_gemini_parts_text(parts);
            }
        }

    } else if (provider_ == "fincept") {
        // /research/chat returns OpenAI-compatible choices array:
        // {"success":true,"data":{"choices":[{"message":{"role":"assistant","content":"..."}}],...}}
        QJsonObject data = rj.contains("data") ? rj["data"].toObject() : rj;
        QJsonArray choices = data["choices"].toArray();
        if (!choices.isEmpty())
            resp.content = extract_openai_message_text(choices[0].toObject()["message"].toObject());
        // API returned success=true but empty response text — treat as soft error
        if (resp.content.isEmpty()) {
            resp.error = "Fincept LLM returned an empty response. Please try again.";
            LOG_WARN(TAG, "Fincept /research/chat returned empty choices or content");
            return resp;
        }

    } else {
        // OpenAI-compatible — check for tool calls
        QJsonArray choices = rj["choices"].toArray();
        if (!choices.isEmpty()) {
            QJsonObject msg = choices[0].toObject()["message"].toObject();
            QJsonArray tcs = msg["tool_calls"].toArray();

            if (!tcs.isEmpty()) {
                LOG_INFO(TAG, QString("LLM requested %1 tool calls").arg(tcs.size()));

                // Build initial messages array for tool loop
                QJsonArray loop_msgs;
                if (!system_prompt_.isEmpty())
                    loop_msgs.append(QJsonObject{{"role", "system"}, {"content", system_prompt_}});
                for (const auto& h : history)
                    loop_msgs.append(QJsonObject{{"role", h.role}, {"content", h.content}});
                loop_msgs.append(QJsonObject{{"role", "user"}, {"content", user_message}});
                loop_msgs.append(msg); // assistant message with tool_calls

                // Execute tool calls and append results
                for (const auto& tc_val : tcs) {
                    QJsonObject tc = tc_val.toObject();
                    QString call_id = tc["id"].toString();
                    QString fn_name = tc["function"].toObject()["name"].toString();
                    QJsonObject fn_args =
                        QJsonDocument::fromJson(tc["function"].toObject()["arguments"].toString("{}").toUtf8())
                            .object();

                    LOG_INFO(TAG, QString("Executing tool: %1 args=%2").arg(fn_name,
                                  QString::fromUtf8(QJsonDocument(fn_args).toJson(QJsonDocument::Compact)).left(200)));
                    auto tr = mcp::McpService::instance().execute_openai_function(fn_name, fn_args);
                    LOG_INFO(TAG, QString("Tool %1 -> %2 (msg=%3 err=%4)")
                                      .arg(fn_name, tr.success ? "OK" : "FAIL",
                                           tr.message.left(120), tr.error.left(120)));
                    loop_msgs.append(QJsonObject{
                        {"role", "tool"},
                        {"tool_call_id", call_id},
                        {"content", QString::fromUtf8(QJsonDocument(tr.to_json()).toJson(QJsonDocument::Compact))}});
                }

                resp = do_tool_loop(loop_msgs, url, hdr);
                parse_usage(resp, rj, provider_);
                return resp;

            } else {
                // Normal text response — helper handles plain content, reasoning
                // models (kimi-k2.x / deepseek-reasoner), content-as-parts arrays,
                // and refusal messages in one place.
                resp.content = extract_openai_message_text(msg);
            }
        }
    }

    // ── Detect text-based tool calls ────────────────────────────────────
    // Some models (e.g. minimax, certain OpenRouter models) don't use the
    // structured tool_calls field. Instead they emit tool invocations as
    // XML or custom markup in the text response. Detect these patterns and
    // execute the tools, then ask the LLM to summarize the results.
    if (!resp.content.isEmpty()) {
        LOG_INFO(TAG, "Checking response for text-based tool calls, content starts with: " + resp.content.left(120));
        auto text_tool_result = try_extract_and_execute_text_tool_calls(resp.content, user_message, url, hdr);
        if (text_tool_result.has_value()) {
            resp = text_tool_result.value();
            if (resp.success)
                return resp;
        }
    }

    parse_usage(resp, rj, provider_);
    resp.success = true;
    return resp;
}

// ============================================================================
// Tool-call follow-up loop (OpenAI-compatible)
// ============================================================================

LlmResponse LlmService::do_tool_loop(QJsonArray loop_messages, const QString& url,
                                     const QMap<QString, QString>& headers) {
    LlmResponse resp;
    // 15 rounds covers complex agentic workflows (multi-step research →
    // template → fill → polish). Each round can contain many parallel
    // tool calls so this isn't 15 tool calls — it's 15 reasoning steps.
    static constexpr int MAX_ROUNDS = 15;
    LOG_INFO(TAG, QString("TOOL LOOP: starting (max %1 rounds, model=%2)").arg(MAX_ROUNDS).arg(model_));

    for (int round = 0; round < MAX_ROUNDS; ++round) {
        QJsonObject fu;
        fu["model"] = model_;
        fu["messages"] = loop_messages;
        // Temperature intentionally omitted — provider default.
        fu["max_tokens"] = resolved_max_tokens();

        QJsonArray tools = mcp::McpService::instance().format_tools_for_openai(apply_request_policy(tool_filter_));
        if (!tools.isEmpty())
            fu["tools"] = tools;

        auto http = blocking_post(url, fu, headers);
        if (!http.success) {
            resp.error = "Tool follow-up failed: " + http.error;
            return resp;
        }

        auto doc = QJsonDocument::fromJson(http.body);
        if (doc.isNull()) {
            resp.error = "Parse error in tool follow-up";
            return resp;
        }
        QJsonObject rj = doc.object();

        QJsonArray choices = rj["choices"].toArray();
        if (choices.isEmpty()) {
            resp.error = "No choices in tool follow-up";
            return resp;
        }

        QJsonObject msg = choices[0].toObject()["message"].toObject();
        QJsonArray tcs = msg["tool_calls"].toArray();

        if (!tcs.isEmpty()) {
            // Another round
            loop_messages.append(msg);
            for (const auto& tc_val : tcs) {
                QJsonObject tc = tc_val.toObject();
                QString cid = tc["id"].toString();
                QString fname = tc["function"].toObject()["name"].toString();
                QJsonObject fa =
                    QJsonDocument::fromJson(tc["function"].toObject()["arguments"].toString("{}").toUtf8()).object();

                LOG_INFO(TAG, QString("TOOL LOOP r%1: executing %2 args=%3").arg(round).arg(fname,
                              QString::fromUtf8(QJsonDocument(fa).toJson(QJsonDocument::Compact)).left(200)));
                auto tr = mcp::McpService::instance().execute_openai_function(fname, fa);
                LOG_INFO(TAG, QString("TOOL LOOP r%1: %2 -> %3 (msg=%4 err=%5)")
                                  .arg(round).arg(fname,
                                       tr.success ? "OK" : "FAIL", tr.message.left(120), tr.error.left(120)));
                loop_messages.append(QJsonObject{
                    {"role", "tool"},
                    {"tool_call_id", cid},
                    {"content", QString::fromUtf8(QJsonDocument(tr.to_json()).toJson(QJsonDocument::Compact))}});
            }
            continue;
        }

        // Final text response
        resp.content = extract_openai_message_text(msg);
        parse_usage(resp, rj, provider_);
        resp.success = !resp.content.isEmpty();
        LOG_INFO(TAG, QString("TOOL LOOP: finished after %1 round(s) — %2 chars of text")
                          .arg(round + 1).arg(resp.content.length()));
        return resp;
    }

    // Max rounds exhausted. The user would see an empty bubble. Force one
    // final non-tool turn so the model summarizes whatever it accomplished
    // so the chat doesn't go silent.
    LOG_WARN(TAG, "TOOL LOOP: exceeded max rounds — forcing summary turn (no tools)");
    {
        // Append a synthetic system nudge instructing the model to wrap up.
        loop_messages.append(QJsonObject{
            {"role", "system"},
            {"content", "You have used your tool-call budget. Reply now with a final answer to the user "
                        "summarizing what you accomplished and what (if anything) is incomplete. Do not "
                        "request any more tools."}});

        QJsonObject fu;
        fu["model"] = model_;
        fu["messages"] = loop_messages;
        fu["max_tokens"] = resolved_max_tokens();
        // Deliberately omit the tools field so the model is forced to produce text.

        auto http = blocking_post(url, fu, headers);
        if (http.success) {
            auto doc = QJsonDocument::fromJson(http.body);
            if (!doc.isNull()) {
                QJsonObject rj = doc.object();
                QJsonArray choices = rj["choices"].toArray();
                if (!choices.isEmpty()) {
                    QJsonObject msg = choices[0].toObject()["message"].toObject();
                    resp.content = extract_openai_message_text(msg);
                    parse_usage(resp, rj, provider_);
                    resp.success = !resp.content.isEmpty();
                    LOG_INFO(TAG, QString("TOOL LOOP: summary fallback produced %1 chars")
                                      .arg(resp.content.length()));
                    if (resp.success)
                        return resp;
                }
            }
        }
    }
    resp.error = "Tool call loop exceeded maximum rounds";
    return resp;
}

// ============================================================================
// Text-based tool call extraction
// ============================================================================
// Models that don't support structured tool_calls (e.g. minimax, some
// OpenRouter models) may emit tool invocations as XML/text markup in their
// response. This method detects common patterns and executes the tools.

std::optional<LlmResponse> LlmService::try_extract_and_execute_text_tool_calls(const QString& content,
                                                                               const QString& user_message,
                                                                               const QString& url,
                                                                               const QMap<QString, QString>& headers) {

    // ── Pattern 1: <tool_call>{"name":"...", "arguments":{...}}</tool_call>
    // ── Pattern 2: <invoke name="..." args='{"key":"val"}'>
    // ── Pattern 3: minimax:tool_call  CONTENT /minimax:tool_call
    // ── Pattern 4: ```tool_call\n{"name":"...", "arguments":{...}}\n```
    // ── Pattern 5: function_name(arg1, arg2) style calls embedded in text

    struct TextToolCall {
        QString name;
        QJsonObject args;
    };
    std::vector<TextToolCall> calls;

    LOG_INFO(TAG, "Checking for text-based tool calls in response (" + QString::number(content.length()) + " chars)");

    // --- Pattern 1: XML <tool_call> blocks (without namespace prefix) ---
    {
        static const QRegularExpression rx("<tool_call>\\s*(\\{[\\s\\S]*?\\})\\s*</tool_call>",
                                           QRegularExpression::MultilineOption |
                                               QRegularExpression::DotMatchesEverythingOption);
        auto it = rx.globalMatch(content);
        while (it.hasNext()) {
            auto m = it.next();
            auto doc = QJsonDocument::fromJson(m.captured(1).toUtf8());
            if (doc.isObject()) {
                QJsonObject obj = doc.object();
                QString name = obj["name"].toString();
                if (name.isEmpty())
                    name = obj["function"].toString();
                QJsonObject args = obj["arguments"].toObject();
                if (args.isEmpty() && obj["arguments"].isString())
                    args = QJsonDocument::fromJson(obj["arguments"].toString().toUtf8()).object();
                if (!name.isEmpty())
                    calls.push_back({name, args});
            }
        }
    }

    // --- Pattern 2: <invoke name="..."> with <parameter> children or JSON body ---
    // Handles formats like:
    //   <invoke name="tool">{"key":"val"}</invoke>
    //   <invoke name="tool"><parameter name="key">value</parameter></invoke>
    //   <minimax:tool_call><invoke name="tool"><parameter ...>...</invoke></minimax:tool_call>
    if (calls.empty()) {
        static const QRegularExpression rx_invoke("<invoke\\s+name=\"([^\"]+)\"[^>]*>([\\s\\S]*?)</invoke>",
                                                  QRegularExpression::MultilineOption |
                                                      QRegularExpression::DotMatchesEverythingOption);
        static const QRegularExpression rx_param("<parameter\\s+name=\"([^\"]+)\"[^>]*>([\\s\\S]*?)</parameter>",
                                                 QRegularExpression::MultilineOption |
                                                     QRegularExpression::DotMatchesEverythingOption);

        LOG_INFO(TAG, "Pattern 2: invoke regex valid=" + QString(rx_invoke.isValid() ? "yes" : "no"));
        auto dbg_match = rx_invoke.match(content);
        LOG_INFO(TAG, "Pattern 2: hasMatch=" + QString(dbg_match.hasMatch() ? "yes" : "no"));

        auto it = rx_invoke.globalMatch(content);
        while (it.hasNext()) {
            auto m = it.next();
            QString name = m.captured(1);
            QString body = m.captured(2).trimmed();
            QJsonObject args;

            // Try parsing <parameter> children first
            auto pit = rx_param.globalMatch(body);
            bool has_params = false;
            while (pit.hasNext()) {
                auto pm = pit.next();
                QString pname = pm.captured(1);
                QString pval = pm.captured(2).trimmed();
                has_params = true;

                // Try to parse value as JSON (for arrays/objects)
                auto pdoc = QJsonDocument::fromJson(pval.toUtf8());
                if (!pdoc.isNull()) {
                    if (pdoc.isArray())
                        args[pname] = pdoc.array();
                    else if (pdoc.isObject())
                        args[pname] = pdoc.object();
                    else
                        args[pname] = pval;
                } else {
                    args[pname] = pval;
                }
            }

            // Fallback: try body as JSON object (no <parameter> tags)
            if (!has_params && !body.isEmpty()) {
                auto jdoc = QJsonDocument::fromJson(body.toUtf8());
                if (jdoc.isObject())
                    args = jdoc.object();
            }

            if (!name.isEmpty())
                calls.push_back({name, args});
        }
    }

    // --- Pattern 3: minimax:tool_call ... /minimax:tool_call or similar ---
    if (calls.empty()) {
        // Match patterns like "minimax:tool_call CONTENT /minimax:tool_call"
        // or "tool_call: CONTENT /tool_call" — generic vendor-prefixed tool calls
        static const QRegularExpression rx("(?:\\w+:)?tool_call\\s+([\\s\\S]*?)\\s*/(?:\\w+:)?tool_call",
                                           QRegularExpression::MultilineOption);
        auto it = rx.globalMatch(content);
        while (it.hasNext()) {
            auto m = it.next();
            QString body = m.captured(1).trimmed();

            // Try as JSON first
            auto doc = QJsonDocument::fromJson(body.toUtf8());
            if (doc.isObject()) {
                QJsonObject obj = doc.object();
                QString name = obj["name"].toString();
                if (name.isEmpty())
                    name = obj["function"].toString();
                QJsonObject args = obj["arguments"].toObject();
                if (args.isEmpty() && obj["arguments"].isString())
                    args = QJsonDocument::fromJson(obj["arguments"].toString().toUtf8()).object();
                if (!name.isEmpty()) {
                    calls.push_back({name, args});
                    continue;
                }
            }

            // Not JSON — treat as raw SQL/command from the model.
            // Look for a function name prefix like "query  SELECT ..."
            // In the user's case: "minimax:tool_call   SELECT ... /minimax:tool_call"
            // This is the model trying to emit a raw SQL query for an external tool.
            // We need to find the right tool to route this to.
            // Check if there's a tool whose name contains "query" in external servers
            auto all_tools = mcp::McpService::instance().get_all_tools();
            for (const auto& tool : all_tools) {
                if (tool.is_internal)
                    continue;
                // Match tools that accept a "query" or "sql" parameter
                QJsonObject schema = tool.input_schema;
                QJsonObject props = schema["properties"].toObject();
                for (auto pit = props.constBegin(); pit != props.constEnd(); ++pit) {
                    QString key = pit.key().toLower();
                    if (key == "query" || key == "sql" || key == "statement") {
                        QString fn_name = tool.server_id + "__" + tool.name;
                        QJsonObject args;
                        args[pit.key()] = body;
                        calls.push_back({fn_name, args});
                        break;
                    }
                }
                if (!calls.empty())
                    break;
            }
        }
    }

    // --- Pattern 4: ```tool_call\n...\n``` code blocks ---
    if (calls.empty()) {
        static const QRegularExpression rx("```tool_call\\s*\\n([\\s\\S]*?)\\n\\s*```",
                                           QRegularExpression::MultilineOption);
        auto it = rx.globalMatch(content);
        while (it.hasNext()) {
            auto m = it.next();
            auto doc = QJsonDocument::fromJson(m.captured(1).trimmed().toUtf8());
            if (doc.isObject()) {
                QJsonObject obj = doc.object();
                QString name = obj["name"].toString();
                QJsonObject args = obj["arguments"].toObject();
                if (args.isEmpty() && obj["arguments"].isString())
                    args = QJsonDocument::fromJson(obj["arguments"].toString().toUtf8()).object();
                if (!name.isEmpty())
                    calls.push_back({name, args});
            }
        }
    }

    if (calls.empty())
        return std::nullopt;

    LOG_INFO(TAG, QString("Detected %1 text-based tool call(s) in response").arg(calls.size()));

    // Execute each detected tool call
    QString tool_results;
    for (const auto& tc : calls) {
        LOG_INFO(TAG, "Executing text-detected tool: " + tc.name);
        auto tr = mcp::McpService::instance().execute_openai_function(tc.name, tc.args);

        QString result_content;
        if (!tr.message.isEmpty())
            result_content = tr.message;
        else if (!tr.data.isNull() && !tr.data.isUndefined())
            result_content =
                QString::fromUtf8(QJsonDocument(tr.data.toObject()).toJson(QJsonDocument::Compact)).left(4000);
        else
            result_content = QString::fromUtf8(QJsonDocument(tr.to_json()).toJson(QJsonDocument::Compact)).left(4000);

        int sep = tc.name.indexOf("__");
        QString short_name = (sep >= 0) ? tc.name.mid(sep + 2) : tc.name;
        tool_results += "\n**Tool: " + short_name + "**\n" + result_content + "\n";
    }

    if (tool_results.isEmpty())
        return std::nullopt;

    // Build follow-up request asking the LLM to summarize the tool results
    LlmResponse resp;
    QString follow_prompt = "The user asked: \"" + user_message +
                            "\"\n\n"
                            "I executed the requested tool(s) and obtained these results:\n" +
                            tool_results +
                            "\n\nPlease provide a clear, concise summary of this data in natural language. "
                            "Do NOT emit any tool calls or XML markup in your response.";

    QJsonObject follow_body;
    if (provider_ == "anthropic") {
        QJsonArray msgs;
        msgs.append(QJsonObject{{"role", "user"}, {"content", follow_prompt}});
        follow_body["model"] = model_;
        follow_body["messages"] = msgs;
        follow_body["max_tokens"] = resolved_max_tokens();
        // Temperature intentionally omitted — Anthropic default.
        if (!system_prompt_.isEmpty())
            follow_body["system"] = system_prompt_;
    } else if (provider_ == "fincept") {
        // /research/chat uses messages array
        QJsonArray msgs;
        if (!system_prompt_.isEmpty())
            msgs.append(QJsonObject{{"role", "system"}, {"content", system_prompt_}});
        msgs.append(QJsonObject{{"role", "user"}, {"content", follow_prompt}});
        follow_body["messages"] = msgs;
        if (!model_.isEmpty() && model_ != "fincept-llm")
            follow_body["model"] = model_;
    } else {
        // OpenAI-compatible
        QJsonArray msgs;
        if (!system_prompt_.isEmpty())
            msgs.append(QJsonObject{{"role", "system"}, {"content", system_prompt_}});
        msgs.append(QJsonObject{{"role", "user"}, {"content", follow_prompt}});
        follow_body["model"] = model_;
        follow_body["messages"] = msgs;
        // Temperature intentionally omitted — provider default.
        follow_body["max_tokens"] = resolved_max_tokens();
    }

    // No tools in follow-up to prevent infinite loop
    auto fu = blocking_post(url, follow_body, headers);
    if (!fu.success) {
        // Even if follow-up fails, return the raw tool results
        resp.content = tool_results;
        resp.success = true;
        return resp;
    }

    auto fu_doc = QJsonDocument::fromJson(fu.body);
    if (fu_doc.isNull()) {
        resp.content = tool_results;
        resp.success = true;
        return resp;
    }

    QJsonObject fu_rj = fu_doc.object();

    // Extract text from follow-up response (provider-aware)
    if (provider_ == "anthropic") {
        resp.content = extract_anthropic_content_text(fu_rj["content"].toArray());
    } else if (provider_ == "fincept") {
        // /research/chat: {"success":true,"data":{"choices":[{"message":{"content":"..."}}]}}
        QJsonObject data = fu_rj.contains("data") ? fu_rj["data"].toObject() : fu_rj;
        QJsonArray fu_choices = data["choices"].toArray();
        if (!fu_choices.isEmpty())
            resp.content = extract_openai_message_text(fu_choices[0].toObject()["message"].toObject());
    } else {
        QJsonArray choices = fu_rj["choices"].toArray();
        if (!choices.isEmpty())
            resp.content = extract_openai_message_text(choices[0].toObject()["message"].toObject());
    }

    if (resp.content.isEmpty())
        resp.content = tool_results; // fallback to raw results

    resp.success = true;
    parse_usage(resp, fu_rj, provider_);
    return resp;
}

// ============================================================================
// Streaming request (SSE)
// ============================================================================

LlmResponse LlmService::do_streaming_request(const QString& user_message,
                                             const std::vector<ConversationMessage>& history, StreamCallback on_chunk) {
    // All providers fall back to non-streaming do_request so the full tool-call /
    // follow-up loop runs correctly regardless of provider.
    // Streaming is disabled globally until per-provider SSE tool-call handling is
    // implemented for each backend.
    {
        auto resp = do_request(user_message, history);
        if (resp.success && !resp.content.isEmpty())
            on_chunk(resp.content, false);
        on_chunk("", true);
        return resp;
    }

    LlmResponse resp;
    QString url = get_endpoint_url();
    if (url.isEmpty()) {
        resp.error = "No endpoint URL for provider: " + provider_;
        on_chunk("", true);
        return resp;
    }

    auto hdr = get_headers();
    // Send tools for OpenAI-compatible streaming; Anthropic streaming doesn't
    // support tool_choice in the same SSE flow so we handle that separately.
    QJsonObject req_body = (provider_ == "anthropic") ? build_anthropic_request(user_message, history, true)
                                                      : build_openai_request(user_message, history, true, true);

    // Use dedicated NAM on this background thread
    QNetworkAccessManager nam;
    QNetworkRequest req{QUrl(url)};
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Accept", "text/event-stream");
    for (auto it = hdr.constBegin(); it != hdr.constEnd(); ++it)
        req.setRawHeader(it.key().toUtf8(), it.value().toUtf8());

    QByteArray json_data = QJsonDocument(req_body).toJson(QJsonDocument::Compact);
    QNetworkReply* reply = nam.post(req, json_data);

    QByteArray partial_line;
    QString accumulated;
    QJsonObject final_usage_obj;
    bool done = false;
    bool tool_call_detected = false;

    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    timeout.start(120000);

    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(reply, &QNetworkReply::finished, &loop, [&]() {
        done = true;
        loop.quit();
    });
    QObject::connect(reply, &QNetworkReply::readyRead, &loop, [&]() {
        // Drain all available data
        partial_line += reply->readAll();
        while (true) {
            int nl = partial_line.indexOf('\n');
            if (nl < 0)
                break;
            QByteArray raw_line = partial_line.left(nl);
            partial_line.remove(0, nl + 1);

            // Remove \r
            if (!raw_line.isEmpty() && raw_line.back() == '\r')
                raw_line.chop(1);

            QString line = QString::fromUtf8(raw_line).trimmed();
            if (line.isEmpty() || !line.startsWith("data: "))
                continue;

            QString data = line.mid(6); // remove "data: "
            if (data == "[DONE]") {
                if (!tool_call_detected)
                    on_chunk("", true);
                done = true;
                loop.quit();
                return;
            }

            // Capture usage chunk (OpenAI sends usage in a trailing chunk with
            // empty choices[] when stream_options.include_usage=true)
            {
                auto usage_doc = QJsonDocument::fromJson(data.toUtf8());
                if (!usage_doc.isNull() && usage_doc.isObject()) {
                    QJsonObject uobj = usage_doc.object();
                    if (uobj.contains("usage") && uobj["usage"].isObject())
                        final_usage_obj = uobj["usage"].toObject();
                }
            }

            // Detect tool calls in streaming SSE — all providers
            if (!tool_call_detected) {
                auto doc = QJsonDocument::fromJson(data.toUtf8());
                if (!doc.isNull() && doc.isObject()) {
                    QJsonObject obj = doc.object();

                    // Anthropic native SSE format
                    if (provider_ == "anthropic") {
                        const QString type = obj["type"].toString();
                        if (type == "content_block_start" &&
                            obj["content_block"].toObject()["type"].toString() == "tool_use") {
                            LOG_INFO(TAG, "STREAM: Anthropic tool_use content_block_start detected");
                            tool_call_detected = true;
                            loop.quit();
                            return;
                        }
                        if (type == "message_delta" &&
                            obj["delta"].toObject()["stop_reason"].toString() == "tool_use") {
                            LOG_INFO(TAG, "STREAM: Anthropic stop_reason=tool_use detected");
                            tool_call_detected = true;
                            loop.quit();
                            return;
                        }
                    }

                    // OpenAI-compatible format (used by fincept, openai, and others)
                    QJsonArray choices = obj["choices"].toArray();
                    if (!choices.isEmpty()) {
                        const QString finish = choices[0].toObject()["finish_reason"].toString();
                        if (finish == "tool_calls" || finish == "stop") {
                            // "stop" with accumulated tool XML → also check
                        }
                        if (finish == "tool_calls") {
                            LOG_INFO(TAG,
                                     QString("STREAM: OpenAI-compat finish_reason=tool_calls detected (%1)")
                                         .arg(provider_));
                            tool_call_detected = true;
                            loop.quit();
                            return;
                        }
                        QJsonObject delta = choices[0].toObject()["delta"].toObject();
                        if (!delta["tool_calls"].isUndefined() && !delta["tool_calls"].isNull()) {
                            LOG_INFO(TAG, QString("STREAM: OpenAI-compat delta.tool_calls detected (%1)")
                                              .arg(provider_));
                            tool_call_detected = true;
                            loop.quit();
                            return;
                        }
                    }

                    // Fincept may also return tool_calls at top level
                    if (!obj["tool_calls"].isUndefined() && !obj["tool_calls"].isNull() &&
                        obj["tool_calls"].toArray().size() > 0) {
                        LOG_INFO(TAG, "STREAM: top-level tool_calls detected (fincept)");
                        tool_call_detected = true;
                        loop.quit();
                        return;
                    }
                }
            }

            QString chunk = parse_sse_chunk(data, provider_);
            if (!chunk.isEmpty()) {
                accumulated += chunk;

                // Detect tool calls embedded as XML in the streamed text.
                // Some APIs return tool calls as text XML rather than
                // structured JSON. Detect early, suppress output, fallback
                // to non-streaming path which handles tool execution.
                if (!tool_call_detected &&
                    (accumulated.contains("<tool_call>") || accumulated.contains("<invoke name=") ||
                     accumulated.contains("tool_call>"))) {
                    tool_call_detected = true;
                    LOG_INFO(TAG, "Tool call XML detected in streamed text — falling back to non-streaming");
                    loop.quit();
                    return;
                }

                on_chunk(chunk, false);
            }
        }
    });

    loop.exec();
    timeout.stop();

    // If the model requested tool calls, fall back to non-streaming do_request
    // which already handles the full tool-call/follow-up loop correctly.
    if (tool_call_detected) {
        LOG_INFO(TAG, "Tool call detected in stream — falling back to tool loop");
        reply->abort();
        reply->deleteLater();

        // Clear any partial XML that was already streamed to the UI.
        // Send a special "clear" sentinel so the chat screen can reset the bubble.
        on_chunk("\x01__TOOL_CALL_CLEAR__", false);

        auto tool_resp = do_request(user_message, history);
        if (tool_resp.success && !tool_resp.content.isEmpty())
            on_chunk(tool_resp.content, false);
        on_chunk("", true);
        return tool_resp;
    }

    int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (reply->error() != QNetworkReply::NoError) {
        resp.error = reply->errorString();
        LOG_ERROR(TAG, "Stream request failed: " + resp.error);
    } else if (status >= 200 && status < 300) {
        resp.content = accumulated;
        resp.success = true;
        if (!final_usage_obj.isEmpty()) {
            QJsonObject wrap{{"usage", final_usage_obj}};
            parse_usage(resp, wrap, provider_);
        }
    } else {
        resp.error = QString("HTTP %1").arg(status);
    }

    reply->deleteLater();

    if (!done)
        on_chunk("", true); // ensure done fires

    return resp;
}

// ============================================================================
// SSE parsing
// ============================================================================

QString LlmService::parse_sse_chunk(const QString& data, const QString& provider) {
    auto doc = QJsonDocument::fromJson(data.toUtf8());
    if (doc.isNull() || !doc.isObject())
        return {};
    QJsonObject j = doc.object();

    if (provider == "anthropic") {
        // delta is a tagged union. text_delta carries the final answer;
        // thinking_delta carries the chain-of-thought for extended-thinking
        // models (claude-3-7 / claude-opus-4+ with `thinking` param). Surfacing
        // both keeps the UI responsive during long reasoning phases. Other
        // delta types (input_json_delta, signature_delta) must not be rendered.
        if (j["type"].toString() == "content_block_delta") {
            QJsonObject delta = j["delta"].toObject();
            const QString dtype = delta["type"].toString();
            if (dtype == "text_delta")
                return delta["text"].toString();
            if (dtype == "thinking_delta")
                return delta["thinking"].toString();
        }
        return {};
    }

    // OpenAI-compatible
    QJsonArray choices = j["choices"].toArray();
    if (!choices.isEmpty()) {
        QJsonObject delta = choices[0].toObject()["delta"].toObject();
        if (!delta["content"].isNull() && !delta["content"].isUndefined()) {
            QString s = delta["content"].toString();
            if (!s.isEmpty())
                return s;
        }
        // Reasoning models (kimi-k2.5 / kimi-k2.6 / kimi-k2-thinking*, deepseek-reasoner,
        // grok-4 reasoning variants) stream their chain-of-thought as
        // `delta.reasoning_content` and only emit `delta.content` after reasoning
        // completes. Surface reasoning deltas so the user sees progress instead
        // of a blank bubble for 10+ seconds.
        if (!delta["reasoning_content"].isNull() && !delta["reasoning_content"].isUndefined()) {
            QString s = delta["reasoning_content"].toString();
            if (!s.isEmpty())
                return s;
        }
        // Refusal deltas — newer OpenAI and some Groq safety paths stream a
        // `refusal` field in place of `content` when the model declines.
        if (!delta["refusal"].isNull() && !delta["refusal"].isUndefined())
            return delta["refusal"].toString();
    }
    return {};
}

// ============================================================================
// Token usage
// ============================================================================

void LlmService::parse_usage(LlmResponse& resp, const QJsonObject& rj, const QString& provider) {
    if (!rj.contains("usage"))
        return;
    QJsonObject u = rj["usage"].toObject();
    if (provider == "anthropic") {
        resp.prompt_tokens = u["input_tokens"].toInt();
        resp.completion_tokens = u["output_tokens"].toInt();
        resp.total_tokens = resp.prompt_tokens + resp.completion_tokens;
    } else {
        resp.prompt_tokens = u["prompt_tokens"].toInt();
        resp.completion_tokens = u["completion_tokens"].toInt();
        resp.total_tokens = u["total_tokens"].toInt();
    }
}

// ============================================================================
// Dynamic model fetching
// ============================================================================

QString LlmService::get_models_url(const QString& provider, const QString& api_key, const QString& base_url) {
    Q_UNUSED(api_key) // Auth goes via headers (see get_models_headers()), URL is provider-based.
    const QString p = provider.toLower();

    // Custom base_url (except fincept)
    if (!base_url.isEmpty() && p != "fincept") {
        QString base = base_url;
        if (base.endsWith('/'))
            base.chop(1);
        if (p == "anthropic")
            return base + "/v1/models?limit=1000";
        return base + "/v1/models";
    }

    if (p == "openai")
        return "https://api.openai.com/v1/models";
    if (p == "anthropic")
        return "https://api.anthropic.com/v1/models?limit=1000";
    if (p == "gemini" || p == "google") {
        // Auth goes via x-goog-api-key header (set by get_models_headers()).
        return "https://generativelanguage.googleapis.com/v1beta/models?pageSize=100";
    }
    if (p == "groq")
        return "https://api.groq.com/openai/v1/models";
    if (p == "deepseek")
        return "https://api.deepseek.com/models";
    if (p == "openrouter")
        return "https://openrouter.ai/api/v1/models";
    if (p == "ollama") {
        QString base = base_url.isEmpty() ? "http://localhost:11434" : base_url;
        if (base.endsWith('/'))
            base.chop(1);
        return base + "/api/tags";
    }
    if (p == "xai")
        return "https://api.x.ai/v1/models";
    if (p == "kimi")
        return "https://api.moonshot.ai/v1/models";
    if (p == "fincept")
        return "https://api.fincept.in/research/llm/models";
    // minimax: no public /v1/models endpoint — fallback models used instead
    return {};
}

QMap<QString, QString> LlmService::get_models_headers(const QString& provider, const QString& api_key) {
    QMap<QString, QString> h;
    const QString p = provider.toLower();

    if (p == "anthropic") {
        if (!api_key.isEmpty())
            h["x-api-key"] = api_key;
        h["anthropic-version"] = "2023-06-01";
    } else if (p == "gemini" || p == "google") {
        // Key is in URL query param
        if (!api_key.isEmpty())
            h["x-goog-api-key"] = api_key;
    } else if (p == "ollama") {
        // No auth needed
    } else if (p == "fincept") {
        // Resolve API key from session (same logic as ensure_config)
        QString resolved_key = api_key;
        if (resolved_key.isEmpty()) {
            auto stored = SettingsRepository::instance().get("fincept_api_key");
            if (stored.is_ok() && !stored.value().isEmpty())
                resolved_key = stored.value();
        }
        if (!resolved_key.isEmpty())
            h["X-API-Key"] = resolved_key;
    } else {
        // OpenAI-compatible: OpenAI, Groq, DeepSeek, OpenRouter
        if (!api_key.isEmpty())
            h["Authorization"] = "Bearer " + api_key;
    }
    return h;
}

QStringList LlmService::parse_models_response(const QString& provider, const QByteArray& body) {
    QStringList models;
    auto doc = QJsonDocument::fromJson(body);
    if (doc.isNull() || !doc.isObject())
        return models;
    QJsonObject root = doc.object();
    const QString p = provider.toLower();

    if (p == "gemini" || p == "google") {
        // {"models": [{"name": "models/gemini-...", "supportedGenerationMethods": [...]}]}
        QJsonArray arr = root["models"].toArray();
        for (const auto& v : arr) {
            QJsonObject m = v.toObject();
            // Only include models that support generateContent
            QJsonArray methods = m["supportedGenerationMethods"].toArray();
            bool can_generate = false;
            for (const auto& method : methods) {
                if (method.toString() == "generateContent") {
                    can_generate = true;
                    break;
                }
            }
            if (!can_generate)
                continue;

            QString name = m["name"].toString();
            // Strip "models/" prefix
            if (name.startsWith("models/"))
                name = name.mid(7);
            if (!name.isEmpty())
                models.append(name);
        }
    } else if (p == "ollama") {
        // {"models": [{"name": "llama3:8b", ...}]}
        QJsonArray arr = root["models"].toArray();
        for (const auto& v : arr) {
            QString name = v.toObject()["name"].toString();
            if (!name.isEmpty())
                models.append(name);
        }
    } else if (p == "fincept") {
        // {"success":true,"data":{"models":["MiniMax-M2.7",...]}}
        // or {"success":true,"data":["model1","model2",...]}
        QJsonValue data_val = root["data"];
        QJsonArray arr;
        if (data_val.isObject())
            arr = data_val.toObject()["models"].toArray();
        else if (data_val.isArray())
            arr = data_val.toArray();
        for (const auto& v : arr) {
            QString id = v.isString() ? v.toString() : v.toObject()["id"].toString();
            if (!id.isEmpty())
                models.append(id);
        }
        // If API doesn't expose a models endpoint yet, fall back to known models
        if (models.isEmpty())
            models = {"MiniMax-M2.7", "MiniMax-M2.7-highspeed", "MiniMax-M2.5"};
    } else {
        // OpenAI-compatible: OpenAI, Anthropic, Groq, DeepSeek, OpenRouter
        // {"data": [{"id": "model-id", ...}]}
        QJsonArray arr = root["data"].toArray();
        for (const auto& v : arr) {
            QString id = v.toObject()["id"].toString();
            if (!id.isEmpty())
                models.append(id);
        }
    }

    models.sort(Qt::CaseInsensitive);
    return models;
}

void LlmService::fetch_models(const QString& provider, const QString& api_key, const QString& base_url) {
    // Fincept has no public /models listing endpoint — return known models immediately.
    if (provider.toLower() == "fincept") {
        emit models_fetched(provider,
                            {"MiniMax-M2.7", "MiniMax-M2.7-highspeed", "MiniMax-M2.5", "MiniMax-M2.5-highspeed"}, {});
        return;
    }

    QString url = get_models_url(provider, api_key, base_url);
    if (url.isEmpty()) {
        emit models_fetched(provider, {}, "Unknown provider: " + provider);
        return;
    }

    auto headers = get_models_headers(provider, api_key);

    QPointer<LlmService> self = this;
    (void)QtConcurrent::run([self, provider, url, headers]() {
        // Blocking GET (reuse blocking_post pattern but with GET)
        QNetworkAccessManager nam;
        QNetworkRequest req{QUrl(url)};
        for (auto it = headers.constBegin(); it != headers.constEnd(); ++it)
            req.setRawHeader(it.key().toUtf8(), it.value().toUtf8());

        QNetworkReply* reply = nam.get(req);

        QEventLoop loop;
        QTimer timer;
        timer.setSingleShot(true);
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
        timer.start(15000);
        loop.exec();

        QString error;
        QStringList models;

        if (!timer.isActive()) {
            reply->abort();
            error = "Request timed out";
        } else {
            timer.stop();
            int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (reply->error() != QNetworkReply::NoError || status < 200 || status >= 300) {
                error = reply->errorString();
                if (error.isEmpty())
                    error = "HTTP " + QString::number(status);
            } else {
                models = parse_models_response(provider, reply->readAll());
                if (models.isEmpty())
                    error = "No models found";
            }
        }

        reply->deleteLater();

        if (self) {
            QMetaObject::invokeMethod(
                self,
                [self, provider, models, error]() {
                    if (self)
                        emit self->models_fetched(provider, models, error);
                },
                Qt::QueuedConnection);
        }
    });
}

// ============================================================================
// Public API
// ============================================================================

LlmResponse LlmService::chat(const QString& user_message, const std::vector<ConversationMessage>& history,
                             bool use_tools) {
    QMutexLocker lock(&mutex_);
    ensure_config();

    if (provider_.isEmpty())
        return LlmResponse{.content = {}, .error = "No LLM provider configured"};

    // Take config snapshot — release lock before blocking network call
    QString p = provider_, k = api_key_, b = base_url_, m = model_, sp = system_prompt_;
    double t = temperature_;
    int mx = max_tokens_;
    lock.unlock();

    // Restore snapshot into members for use by helper methods
    // (helpers read member variables, so we need them set)
    // This is safe because chat() is always called from a background thread.
    provider_ = p;
    api_key_ = k;
    base_url_ = b;
    model_ = m;
    system_prompt_ = sp;
    temperature_ = t;
    max_tokens_ = mx;

    // Per-message intent classifier removed. Tool RAG (tool.list) replaces
    // it — instead of guessing categories from keywords server-side, the LLM
    // now searches the catalog itself with a natural-language query. Higher
    // accuracy, no English-only keyword list to maintain.

    // Per-request tools-off override (thread_local). Replaces the old
    // pattern of mutating tools_enabled_ on the singleton, which raced with
    // concurrent chat_streaming calls from the floating bubble.
    ToolPolicyGuard guard(use_tools ? ToolPolicy::All : ToolPolicy::None);
    return do_request(user_message, history);
}

// Back-compat boolean overload — delegates to the enum form.
void LlmService::chat_streaming(const QString& user_message, const std::vector<ConversationMessage>& history,
                                StreamCallback on_chunk, bool use_tools) {
    chat_streaming(user_message, history, std::move(on_chunk),
                   use_tools ? ToolPolicy::All : ToolPolicy::None);
}

void LlmService::chat_streaming(const QString& user_message, const std::vector<ConversationMessage>& history,
                                StreamCallback on_chunk, ToolPolicy policy) {
    // Snapshot config under lock
    QString p, k, b, m, sp;
    double t;
    int mx;
    {
        QMutexLocker lock(&mutex_);
        ensure_config();
        p = provider_;
        k = api_key_;
        b = base_url_;
        m = model_;
        sp = system_prompt_;
        t = temperature_;
        mx = max_tokens_;
    }

    if (p.isEmpty()) {
        on_chunk("", true);
        emit finished_streaming(LlmResponse{.content = {}, .error = "No LLM provider configured"});
        return;
    }

    // Copy history for lambda capture
    std::vector<ConversationMessage> history_copy = history;

    QPointer<LlmService> self = this;
    // Phase 9: shadow-publish every stream chunk to the DataHub under a
    // per-stream session id. Subscribers (future observer panels, LLM
    // diagnostics, agent MCP bridges) can tail `llm:session:*:stream`
    // without being wired into the original caller. Push-only, coalesced
    // at the policy layer so a token firehose doesn't spam subscribers.
    const QString stream_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString stream_topic = QStringLiteral("llm:session:") + stream_id + QStringLiteral(":stream");
    // One-shot pattern registration — LlmService is not a Producer (no
    // pull semantics), just a publisher. The policy caps cached state
    // and applies coalescing across the topic family.
    [[maybe_unused]] static bool policy_once = []() {
        auto& hub = fincept::datahub::DataHub::instance();
        fincept::datahub::TopicPolicy policy;
        policy.push_only = true;
        policy.coalesce_within_ms = 50;
        policy.ttl_ms = 5 * 60 * 1000;
        hub.set_policy_pattern(QStringLiteral("llm:session:*"), policy);
        LOG_INFO("LlmService", "Registered DataHub policy for llm:session:*");
        return true;
    }();
    // Guard on_chunk: if LlmService is destroyed before a chunk arrives,
    // skip the callback entirely rather than invoking into a dead context.
    StreamCallback guarded_chunk = [self, on_chunk
        , stream_id, stream_topic
    ](const QString& chunk, bool done) {
        if (!self)
            return;
        on_chunk(chunk, done);
        QJsonObject payload{
            {"session_id", stream_id},
            {"chunk", chunk},
            {"done", done},
        };
        auto& hub = fincept::datahub::DataHub::instance();
        hub.publish(stream_topic, QVariant(payload));
        if (done) {
            // Retire the per-session topic; subscribers remain attached
            // but cached state is discarded. Phase 9.
            hub.retire_topic(stream_topic);
        }
    };
    (void)QtConcurrent::run(
        [self, p, k, b, m, sp, t, mx, user_message, history_copy, guarded_chunk, policy]() {
            if (!self)
                return;

            // Apply the config snapshot under the mutex so do_request /
            // do_streaming_request see a consistent state and don't race with
            // reload_config() on the UI thread. We no longer mutate
            // tools_enabled_ — per-request opt-out is conveyed via the
            // thread_local ToolPolicyGuard below.
            {
                QMutexLocker lock(&self->mutex_);
                self->provider_ = p;
                self->api_key_ = k;
                self->base_url_ = b;
                self->model_ = m;
                self->system_prompt_ = sp;
                self->temperature_ = t;
                self->max_tokens_ = mx;
            }

            ToolPolicyGuard guard(policy);
            auto resp = self->do_streaming_request(user_message, history_copy, guarded_chunk);

            if (self) {
                QMetaObject::invokeMethod(
                    self,
                    [self, resp]() {
                        if (self)
                            emit self->finished_streaming(resp);
                    },
                    Qt::QueuedConnection);
            }
        });
}

} // namespace fincept::ai_chat
