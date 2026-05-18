// Multi-provider LLM API client. Sync paths block on QEventLoop and must run on a worker thread.

#include "services/llm/LlmService.h"

#include "services/llm/LlmContentExtractors.h"
#include "services/llm/LlmRequestPolicy.h"
#include "services/llm/ModelCatalog.h"
#include "auth/AuthManager.h"

#include "core/logging/Logger.h"
#include "core/config/AppConfig.h"
#include "mcp/McpProvider.h"
#include "mcp/McpService.h"
#include "storage/repositories/LlmConfigRepository.h"
#include "storage/repositories/SettingsRepository.h"
#include "storage/secure/SecureStorage.h"

#    include "datahub/DataHub.h"
#    include "datahub/TopicPolicy.h"

#include <QCoreApplication>
#include <QEvent>
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
    // Called with mutex_ held. Cache is one-shot except for Fincept credentials —
    // those re-resolve every call so a login that happens after first ensure_config() doesn't 401 forever.
    if (config_loaded_) {
        if (provider_ == "fincept") {
            const auto& sess = fincept::auth::AuthManager::instance().session();
            if (!sess.api_key.isEmpty()) {
                api_key_ = sess.api_key;
            } else {
                auto stored = fincept::SecureStorage::instance().retrieve("api_key");
                if (stored.is_ok() && !stored.value().isEmpty())
                    api_key_ = stored.value();
            }
        }
        return;
    }

    provider_ = model_ = api_key_ = base_url_ = system_prompt_ = {};
    temperature_ = 0.7;
    // 0 means "no user override → ModelCatalog::output_cap()". The v15 migration reset legacy 2000 defaults.
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
        // No active provider — pick the first one.
        if (provider_.isEmpty() && !providers.value().isEmpty()) {
            const auto& c = providers.value().first();
            provider_ = c.provider.toLower();
            api_key_ = c.api_key;
            base_url_ = c.base_url;
            model_ = c.model;
            tools_enabled_ = c.tools_enabled;
        }
    }

    // Nothing configured — default to Fincept with the session key.
    if (provider_.isEmpty()) {
        provider_ = "fincept";
        model_ = "MiniMax-M2.7";
        base_url_ = {};
        LOG_INFO(kLlmSvcTag, "No LLM provider configured — using Fincept default");
    }

    // Fincept key always comes from the live AuthManager session; SettingsRepository fallback is the legacy path.
   if (provider_ == "fincept") {
        const auto& sess = fincept::auth::AuthManager::instance().session();
        if (!sess.api_key.isEmpty()) {
            api_key_ = sess.api_key;
        } else {
            auto stored_key = fincept::SecureStorage::instance().retrieve("api_key");
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

    // Default system prompt — primes the model to actually use tools instead of declining tool-feasible requests.
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
            "  the chat. STEP 0 (mandatory): call report_session_context to see whether this chat "
            "  is already authoring a report (linked_report set) or is starting fresh (linked_report "
            "  null with stale components on the canvas). If linked, CONTINUE editing it. If unlinked "
            "  and the canvas has components, call report_clear first UNLESS the user explicitly "
            "  said 'edit this report' / 'add to this report' / 'continue the report' — in which "
            "  case keep the canvas. The flow is: (1) DESIGN A UNIQUE STRUCTURE for the subject — every ticker, "
            "  sector, or topic deserves its own outline. A Tesla EV-and-FSD memo, a JPM credit note, "
            "  and a TLT duration play should NOT share the same skeleton. Decide which sections "
            "  actually matter for THIS subject (could be Catalysts, Bear Case, Capital Structure, "
            "  Unit Economics, Regulatory Overhang, Insider Activity, Optionality, etc.) — do NOT "
            "  default to the generic Company Overview / Financials / Valuation / Thesis / Risks / "
            "  Target template. Do NOT call report_apply_template unless the user explicitly asks "
            "  for a template OR the request is for a generic non-research scaffold (Meeting Notes, "
            "  Trade Journal, Pre-Market Checklist). (2) call report_set_metadata with a "
            "  subject-specific title (e.g. 'Tesla: Margin Compression vs FSD Optionality' — not "
            "  'Stock Research'), author, company, date. (3) build the structure by calling "
            "  report_add_component for each section you designed in step 1. (4) gather data with "
            "  tools like get_equity_info, get_equity_quote, edgar_get_financials, "
            "  edgar_10k_sections, edgar_calc_multiples, get_equity_news, search_news. "
            "  (5) populate each section by calling report_update_component with the gathered data. "
            "  Use stable component ids returned by report_add_component — never indices. "
            "  COVERAGE: before declaring the report done, verify every section you added has "
            "  real content (no placeholders like 'Describe the company…').\n"
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

    // Tool RAG discovery hint: lists categories dynamically so the model can form good tool.list() queries.
    // The `[Tool discovery]` sentinel makes append idempotent across reloads. Cached static for prompt-cache stability.
    if (tools_enabled_ && !system_prompt_.contains("[Tool discovery]")) {
        // Tool registry is immutable after McpInit; runtime additions won't appear until restart (acceptable here).
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
    // Only fields the Python model constructor accepts. profile_id / profile_name are internal metadata —
    // sending them lands in extra_model_kwargs and fails as unexpected kwargs.
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

int LlmService::resolved_max_tokens() const {
    // Called with mutex_ held.
    constexpr int kFallback = 8192;
    const int catalog_cap = ModelCatalog::output_cap(provider_, model_);

    // <=0 = unset; clamp user values to the model's published cap to avoid a 400.
    if (max_tokens_ > 0) {
        if (catalog_cap > 0 && max_tokens_ > catalog_cap)
            return catalog_cap;
        return max_tokens_;
    }

    if (catalog_cap > 0)
        return catalog_cap;
    return kFallback;
}

QString LlmService::get_endpoint_url() const {
    // Called with mutex_ held.
    const QString& p = provider_;

    // Fincept sync chat endpoint (async lives in fincept_async_request).
    if (p == "fincept") {
        return "https://api.fincept.in/research/chat";
    }

    // Custom base_url wins over hard-coded defaults.
    if (!base_url_.isEmpty()) {
        QString base = base_url_;
        if (base.endsWith('/'))
            base.chop(1);
        const QString suffix = (p == "anthropic") ? QStringLiteral("/messages")
                                                  : QStringLiteral("/chat/completions");
        // If the base already includes the full endpoint, use it as-is.
        if (base.endsWith(suffix))
            return base;
        // If the base already includes an API version segment (e.g. ".../v1"),
        // append only the suffix; otherwise add "/v1" first.
        static const QRegularExpression versioned(QStringLiteral("/v\\d+(beta)?$"));
        if (versioned.match(base).hasMatch())
            return base + suffix;
        return base + "/v1" + suffix;
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
    // Called with mutex_ held.
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
        if (!api_key_.isEmpty())
            h["X-API-Key"] = api_key_;
        // session_token only lives in AuthManager — read live, never cached.
        const auto& sess = fincept::auth::AuthManager::instance().session();
        if (!sess.session_token.isEmpty())
            h["X-Session-Token"] = sess.session_token;
        h["User-Agent"] = "FinceptTerminal/4.0"; // Cloudflare requires it.
    } else {
        if (!api_key_.isEmpty())
            h["Authorization"] = "Bearer " + api_key_;
        if (p == "openrouter") {
            // Attribution for the openrouter.ai/rankings leaderboard.
            h["HTTP-Referer"] = "https://fincept.in";
            h["X-Title"] = "Fincept Terminal";
        }
    }
    return h;
}



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
        // Auth via x-goog-api-key header — do NOT append ?key= (would leak into access logs).
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

    if (provider_ == "anthropic") {
        QJsonArray content = rj["content"].toArray();
        QString stop_reason = rj["stop_reason"].toString();

        // Check for tool_use blocks
        if (stop_reason == "tool_use") {
            LOG_INFO(kLlmSvcTag, "Anthropic requested tool_use");

            // Build follow-up messages: original + assistant turn + tool results
            QJsonArray loop_msgs;
            if (!system_prompt_.isEmpty()) {
            } // Anthropic puts system at top level, not in messages.
            for (const auto& h : history)
                if (h.role != "system")
                    loop_msgs.append(QJsonObject{{"role", h.role}, {"content", h.content}});
            loop_msgs.append(QJsonObject{{"role", "user"}, {"content", user_message}});
            loop_msgs.append(QJsonObject{{"role", "assistant"}, {"content", content}});

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

            loop_msgs.append(QJsonObject{{"role", "user"}, {"content", tool_results}});

            // No tools in follow-up — prevents infinite loop.
            QJsonObject fu;
            fu["model"] = model_;
            fu["messages"] = loop_msgs;
            fu["max_tokens"] = resolved_max_tokens();
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
            resp.content = extract_anthropic_content_text(content);
        }

    } else if (provider_ == "gemini" || provider_ == "google") {
        QJsonArray cands = rj["candidates"].toArray();
        if (!cands.isEmpty()) {
            QJsonArray parts = cands[0].toObject()["content"].toObject()["parts"].toArray();

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

                    // Gemini requires response to be an object — wrap strings under "result".
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
                    // history + original user + model(functionCall) + user(functionResponse).
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
                        {"parts", parts}}); // echo functionCall parts verbatim
                    fu_contents.append(QJsonObject{
                        {"role", "user"},
                        {"parts", fn_response_parts}});

                    QJsonObject fu_body;
                    fu_body["contents"] = fu_contents;
                    QJsonObject gen_cfg;
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
                        // Render function responses as readable text.
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
                resp.content = extract_gemini_parts_text(parts);
            }
        }

    } else if (provider_ == "fincept") {
        // {"success":..., "data":{"choices":[{"message":{"content":...}}]}} or top-level shape.
        QJsonObject data = rj.contains("data") ? rj["data"].toObject() : rj;
        QJsonArray choices = data["choices"].toArray();
        if (!choices.isEmpty())
            resp.content = extract_openai_message_text(choices[0].toObject()["message"].toObject());
        // success=true with empty content is a soft error.
        if (resp.content.isEmpty()) {
            resp.error = "Fincept LLM returned an empty response. Please try again.";
            LOG_WARN(kLlmSvcTag, "Fincept /research/chat returned empty choices or content");
            return resp;
        }

    } else {
        // OpenAI-compatible.
        QJsonArray choices = rj["choices"].toArray();
        if (!choices.isEmpty()) {
            QJsonObject msg = choices[0].toObject()["message"].toObject();
            QJsonArray tcs = msg["tool_calls"].toArray();

            if (!tcs.isEmpty()) {
                LOG_INFO(kLlmSvcTag, QString("LLM requested %1 tool calls").arg(tcs.size()));

                QJsonArray loop_msgs;
                if (!system_prompt_.isEmpty())
                    loop_msgs.append(QJsonObject{{"role", "system"}, {"content", system_prompt_}});
                for (const auto& h : history)
                    loop_msgs.append(QJsonObject{{"role", h.role}, {"content", h.content}});
                loop_msgs.append(QJsonObject{{"role", "user"}, {"content", user_message}});
                loop_msgs.append(msg); // assistant turn with tool_calls

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
                // Helper handles reasoning models, content-as-parts arrays, and refusal messages.
                resp.content = strip_think_blocks(extract_openai_message_text(msg));
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

LlmResponse LlmService::do_streaming_request(const QString& user_message,
                                             const std::vector<ConversationMessage>& history, StreamCallback on_chunk) {
    // Gemini uses :streamGenerateContent, Fincept uses async submit/poll — neither
    // fits this OpenAI-compat SSE path. Fall back to do_request and emit as one chunk.
    if (provider_ == "gemini" || provider_ == "google" || provider_ == "fincept") {
        detail::ProgressEmitterGuard pg([on_chunk](const QString& s) { on_chunk(s, false); });
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
    // Anthropic streaming handles tools out-of-band so it goes through a different request shape.
    QJsonObject req_body = (provider_ == "anthropic") ? build_anthropic_request(user_message, history, true)
                                                      : build_openai_request(user_message, history, true, true);

    // Heap-allocate the QNAM so we can deleteLater() it and drain
    // DeferredDelete before this function returns — a stack QNAM here used
    // to leak socket-notifier teardown work onto the main runloop and crash
    // hours later inside qt_mac_socket_callback. See eventloop_request().
    auto* nam = new QNetworkAccessManager;
    QNetworkRequest req{QUrl(url)};
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Accept", "text/event-stream");
    req.setRawHeader("Cache-Control", "no-cache");
    // Per Qt 6 network docs: HTTP/2 is enabled by default and batches frames,
    // which causes SSE chunks to arrive in bursts instead of as the server
    // flushes them. Force HTTP/1.1 so readyRead fires per TCP packet.
    req.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
    for (auto it = hdr.constBegin(); it != hdr.constEnd(); ++it)
        req.setRawHeader(it.key().toUtf8(), it.value().toUtf8());

    QByteArray json_data = QJsonDocument(req_body).toJson(QJsonDocument::Compact);
    QNetworkReply* reply = nam->post(req, json_data);

    QByteArray partial_line;
    QString accumulated;
    QJsonObject final_usage_obj;
    bool done = false;
    bool tool_call_detected = false;

    // <think>…</think> reasoning filter. Reasoning models (MiniMax M2.7,
    // DeepSeek-R1 derivatives, …) stream their chain-of-thought inline in
    // `delta.content` wrapped in <think> tags. The user shouldn't see that —
    // only the answer. State persists across SSE chunks because tags can
    // straddle the chunk boundary ("<thi" + "nk>...").
    bool in_think = false;
    QString think_pending;
    auto filter_think = [&](const QString& chunk) -> QString {
        QString out;
        QString work = think_pending + chunk;
        think_pending.clear();
        qsizetype pos = 0;
        const qsizetype n = work.length();
        while (pos < n) {
            if (in_think) {
                qsizetype end = work.indexOf(QStringLiteral("</think>"), pos, Qt::CaseInsensitive);
                if (end < 0) {
                    // Hold last 8 chars in case </think> straddles.
                    qsizetype safe = std::max<qsizetype>(pos, n - 8);
                    think_pending = work.mid(safe);
                    return out;
                }
                in_think = false;
                pos = end + 8;
            } else {
                qsizetype start = work.indexOf(QStringLiteral("<think>"), pos, Qt::CaseInsensitive);
                if (start < 0) {
                    // Emit everything except the last 7 chars (length of "<think>")
                    // which might be a partial open tag.
                    qsizetype safe = std::max<qsizetype>(pos, n - 7);
                    out += work.mid(pos, safe - pos);
                    think_pending = work.mid(safe);
                    return out;
                }
                out += work.mid(pos, start - pos);
                in_think = true;
                pos = start + 7;
            }
        }
        return out;
    };

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
        partial_line += reply->readAll();
        while (true) {
            int nl = partial_line.indexOf('\n');
            if (nl < 0)
                break;
            QByteArray raw_line = partial_line.left(nl);
            partial_line.remove(0, nl + 1);

            if (!raw_line.isEmpty() && raw_line.back() == '\r')
                raw_line.chop(1);

            QString line = QString::fromUtf8(raw_line).trimmed();
            if (line.isEmpty() || !line.startsWith("data: "))
                continue;

            QString data = line.mid(6); // strip "data: "
            if (data == "[DONE]") {
                if (!tool_call_detected)
                    on_chunk("", true);
                done = true;
                loop.quit();
                return;
            }

            // OpenAI sends a trailing usage chunk (empty choices[]) when stream_options.include_usage=true.
            {
                auto usage_doc = QJsonDocument::fromJson(data.toUtf8());
                if (!usage_doc.isNull() && usage_doc.isObject()) {
                    QJsonObject uobj = usage_doc.object();
                    if (uobj.contains("usage") && uobj["usage"].isObject())
                        final_usage_obj = uobj["usage"].toObject();
                }
            }

            if (!tool_call_detected) {
                auto doc = QJsonDocument::fromJson(data.toUtf8());
                if (!doc.isNull() && doc.isObject()) {
                    QJsonObject obj = doc.object();

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

                    QJsonArray choices = obj["choices"].toArray();
                    if (!choices.isEmpty()) {
                        const QString finish = choices[0].toObject()["finish_reason"].toString();
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

                    // Fincept can also surface tool_calls at top level.
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

                // Some providers stream tool calls as XML/text markup — detect, suppress output, fall back to do_request.
                // Patterns covered: <tool_call>, </tool_call>, <minimax:tool_call>, <invoke name=,
                // ```tool_call code fences, and bare `minimax:tool_call ... /minimax:tool_call`.
                if (!tool_call_detected) {
                    static const QRegularExpression rx_text_tool(
                        QStringLiteral("<\\s*/?\\s*(?:\\w+\\s*:\\s*)?tool_call\\b"
                                       "|<\\s*invoke\\s+name="
                                       "|```\\s*tool_call\\b"
                                       "|\\b\\w+\\s*:\\s*tool_call\\b"),
                        QRegularExpression::CaseInsensitiveOption);
                    if (rx_text_tool.match(accumulated).hasMatch()) {
                        tool_call_detected = true;
                        LOG_INFO(kLlmSvcTag, "Tool call markup detected in streamed text — falling back to non-streaming");
                        loop.quit();
                        return;
                    }
                }

                const QString visible = filter_think(chunk);
                if (!visible.isEmpty())
                    on_chunk(visible, false);
            }
        }
    });

    loop.exec();
    timeout.stop();

    auto drain_nam = [&]() {
        reply->deleteLater();
        nam->deleteLater();
        // Pump DeferredDelete on this thread so socket-notifier teardown
        // completes while a dispatcher still exists — prevents the
        // qt_mac_socket_callback use-after-free on the main thread later.
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    };

    if (tool_call_detected) {
        LOG_INFO(kLlmSvcTag, "Tool call detected in stream — falling back to tool loop");
        reply->abort();
        drain_nam();

        // Sentinel tells the chat screen to reset the bubble before the real reply arrives.
        on_chunk("\x01__TOOL_CALL_CLEAR__", false);

        // Wire per-tool progress into the chat bubble so the user sees something
        // during the multi-second tool execution phase instead of a blank wait.
        detail::ProgressEmitterGuard pg([on_chunk](const QString& s) { on_chunk(s, false); });
        auto tool_resp = do_request(user_message, history);
        if (tool_resp.success && !tool_resp.content.isEmpty())
            on_chunk(QStringLiteral("\n") + tool_resp.content, false);
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

    drain_nam();

    if (!done)
        on_chunk("", true);

    return resp;
}
// ============================================================================
// Public API
// ============================================================================

LlmResponse LlmService::chat(const QString& user_message, const std::vector<ConversationMessage>& history,
                             bool use_tools) {
    {
        QMutexLocker lock(&mutex_);
        ensure_config();
        if (provider_.isEmpty())
            return LlmResponse{.content = {}, .error = "No LLM provider configured"};
    }
    // Helpers read members directly. ensure_config() already wrote them under
    // mutex; we release before the blocking network call to avoid serialising
    // concurrent chats. Reassigning a local snapshot back to the members after
    // unlocking would clobber any reload_config() that landed in the gap.

    // thread_local guard avoids racing with concurrent chat_streaming calls from the floating bubble.
    detail::ToolPolicyGuard guard(use_tools ? ToolPolicy::All : ToolPolicy::None);
    return do_request(user_message, history);
}

void LlmService::chat_streaming(const QString& user_message, const std::vector<ConversationMessage>& history,
                                StreamCallback on_chunk, bool use_tools) {
    chat_streaming(user_message, history, std::move(on_chunk),
                   use_tools ? ToolPolicy::All : ToolPolicy::None);
}

void LlmService::chat_streaming(const QString& user_message, const std::vector<ConversationMessage>& history,
                                StreamCallback on_chunk, ToolPolicy policy,
                                const QString& chat_session_id) {
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

    std::vector<ConversationMessage> history_copy = history;

    QPointer<LlmService> self = this;
    // Shadow-publish every chunk to DataHub under a per-stream session id so observer panels
    // can tail `llm:session:*:stream` without being wired in. Push-only, coalesced.
    const QString stream_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString stream_topic = QStringLiteral("llm:session:") + stream_id + QStringLiteral(":stream");
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
    // Skip on_chunk if LlmService dies before the chunk arrives.
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
            // Subscribers stay attached but cached state is discarded.
            hub.retire_topic(stream_topic);
        }
    };
    (void)QtConcurrent::run(
        [self, p, k, b, m, sp, t, mx, user_message, history_copy, guarded_chunk, policy,
         chat_session_id]() {
            if (!self)
                return;

            // Snapshot under mutex so do_*_request see consistent state and don't race with reload_config().
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
            detail::ChatSessionGuard session_guard(chat_session_id);
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
