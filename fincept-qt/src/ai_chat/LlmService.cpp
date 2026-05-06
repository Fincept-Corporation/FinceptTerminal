// LlmService.cpp — Multi-provider LLM API client (Qt port)
// Translated from fincept-cpp llm_service.cpp:
//   libcurl  → QNetworkAccessManager + QEventLoop (blocking on bg thread)
//   std::future → QtConcurrent::run + signal
//   nlohmann/json → QJsonObject/QJsonArray

#include "ai_chat/LlmService.h"

#include "ai_chat/LlmContentExtractors.h"
#include "ai_chat/LlmRequestPolicy.h"
#include "ai_chat/ModelCatalog.h"
#include "auth/AuthManager.h"

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

static constexpr const char* kLlmSvcTag = "LlmService";



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
    // Called with mutex_ held.
    //
    // Fincept credentials are refreshed on every call regardless of cache
    // state — the cache is a one-shot for everything else (tool-RAG hint
    // generation walks the entire MCP registry), but credentials track the
    // auth lifecycle. The user can authenticate AFTER the first
    // ensure_config() ran (e.g. AI chat opened before login completes), and
    // a stale cached empty key would 401 every subsequent request.
    if (config_loaded_) {
        if (provider_ == "fincept") {
            const auto& sess = fincept::auth::AuthManager::instance().session();
            if (!sess.api_key.isEmpty()) {
                api_key_ = sess.api_key;
            } else {
                auto stored = SettingsRepository::instance().get("fincept_api_key");
                if (stored.is_ok() && !stored.value().isEmpty())
                    api_key_ = stored.value();
            }
        }
        return;
    }

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
        LOG_INFO(kLlmSvcTag, "No LLM provider configured — using Fincept default");
    }

    // Fincept always resolves API key from the authenticated session, never
    // from llm_configs. Prefer AuthManager's in-memory session (authoritative
    // and live); fall back to SettingsRepository("fincept_api_key") which
    // AuthManager writes during auto_configure_fincept_llm (legacy path).
    if (provider_ == "fincept") {
        const auto& sess = fincept::auth::AuthManager::instance().session();
        if (!sess.api_key.isEmpty()) {
            api_key_ = sess.api_key;
        } else {
            auto stored_key = SettingsRepository::instance().get("fincept_api_key");
            if (stored_key.is_ok() && !stored_key.value().isEmpty())
                api_key_ = stored_key.value();
        }
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
    LOG_INFO(kLlmSvcTag, QString("LLM config loaded: provider=%1 model=%2 tools_enabled=%3 "
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
        // api_key_ is refreshed from AuthManager on every ensure_config() call,
        // so it tracks the auth lifecycle even if it was empty when the cache
        // was first populated.
        if (!api_key_.isEmpty())
            h["X-API-Key"] = api_key_;
        // session_token lives only in AuthManager (never written to
        // SettingsRepository) — read it live so it's always current.
        const auto& sess = fincept::auth::AuthManager::instance().session();
        if (!sess.session_token.isEmpty())
            h["X-Session-Token"] = sess.session_token;
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
        LOG_INFO(kLlmSvcTag, "do_request: routing to fincept_async_request");
        return fincept_async_request(user_message, history);
    } else {
        req_body = build_openai_request(user_message, history, false, true);
    }

    LOG_DEBUG(kLlmSvcTag, QString("POST %1 provider=%2 model=%3").arg(url, provider_, model_));

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
            LOG_INFO(kLlmSvcTag, "Anthropic requested tool_use");

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

                LOG_INFO(kLlmSvcTag, "Executing Anthropic tool: " + tool_name);
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
                LOG_INFO(kLlmSvcTag, "Gemini requested function call(s)");
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

                    LOG_INFO(kLlmSvcTag, "Executing Gemini tool: " + fn_name);
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
            LOG_WARN(kLlmSvcTag, "Fincept /research/chat returned empty choices or content");
            return resp;
        }

    } else {
        // OpenAI-compatible — check for tool calls
        QJsonArray choices = rj["choices"].toArray();
        if (!choices.isEmpty()) {
            QJsonObject msg = choices[0].toObject()["message"].toObject();
            QJsonArray tcs = msg["tool_calls"].toArray();

            if (!tcs.isEmpty()) {
                LOG_INFO(kLlmSvcTag, QString("LLM requested %1 tool calls").arg(tcs.size()));

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

                    LOG_INFO(kLlmSvcTag, QString("Executing tool: %1 args=%2").arg(fn_name,
                                  QString::fromUtf8(QJsonDocument(fn_args).toJson(QJsonDocument::Compact)).left(200)));
                    auto tr = mcp::McpService::instance().execute_openai_function(fn_name, fn_args);
                    LOG_INFO(kLlmSvcTag, QString("Tool %1 -> %2 (msg=%3 err=%4)")
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
        LOG_INFO(kLlmSvcTag, "Checking response for text-based tool calls, content starts with: " + resp.content.left(120));
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
                            LOG_INFO(kLlmSvcTag, "STREAM: Anthropic tool_use content_block_start detected");
                            tool_call_detected = true;
                            loop.quit();
                            return;
                        }
                        if (type == "message_delta" &&
                            obj["delta"].toObject()["stop_reason"].toString() == "tool_use") {
                            LOG_INFO(kLlmSvcTag, "STREAM: Anthropic stop_reason=tool_use detected");
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
                            LOG_INFO(kLlmSvcTag,
                                     QString("STREAM: OpenAI-compat finish_reason=tool_calls detected (%1)")
                                         .arg(provider_));
                            tool_call_detected = true;
                            loop.quit();
                            return;
                        }
                        QJsonObject delta = choices[0].toObject()["delta"].toObject();
                        if (!delta["tool_calls"].isUndefined() && !delta["tool_calls"].isNull()) {
                            LOG_INFO(kLlmSvcTag, QString("STREAM: OpenAI-compat delta.tool_calls detected (%1)")
                                              .arg(provider_));
                            tool_call_detected = true;
                            loop.quit();
                            return;
                        }
                    }

                    // Fincept may also return tool_calls at top level
                    if (!obj["tool_calls"].isUndefined() && !obj["tool_calls"].isNull() &&
                        obj["tool_calls"].toArray().size() > 0) {
                        LOG_INFO(kLlmSvcTag, "STREAM: top-level tool_calls detected (fincept)");
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
                    LOG_INFO(kLlmSvcTag, "Tool call XML detected in streamed text — falling back to non-streaming");
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
        LOG_INFO(kLlmSvcTag, "Tool call detected in stream — falling back to tool loop");
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
        LOG_ERROR(kLlmSvcTag, "Stream request failed: " + resp.error);
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
    detail::ToolPolicyGuard guard(use_tools ? ToolPolicy::All : ToolPolicy::None);
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
            // thread_local detail::ToolPolicyGuard below.
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

            detail::ToolPolicyGuard guard(policy);
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
