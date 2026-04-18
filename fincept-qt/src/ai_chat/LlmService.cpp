// LlmService.cpp — Multi-provider LLM API client (Qt port)
// Translated from fincept-cpp llm_service.cpp:
//   libcurl  → QNetworkAccessManager + QEventLoop (blocking on bg thread)
//   std::future → QtConcurrent::run + signal
//   nlohmann/json → QJsonObject/QJsonArray

#include "ai_chat/LlmService.h"

#include "core/logging/Logger.h"
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
#include <QThread>
#include <QTimer>
#include <QUuid>
#include <QVariant>
#include <QtConcurrent/QtConcurrent>

namespace fincept::ai_chat {

static constexpr const char* TAG = "LlmService";

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
    max_tokens_ = 4096;
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
        system_prompt_ = "You are Fincept AI, the intelligent assistant embedded inside the "
                         "Fincept Terminal — a professional desktop financial intelligence application. "
                         "You have access to a set of tools that let you interact with the terminal "
                         "directly: navigate to any screen, fetch live market data, manage watchlists, "
                         "query portfolios, execute trades on paper, run Python analytics, and more. "
                         "ALWAYS use the available tools when the user asks you to perform an action "
                         "that a tool can fulfil (e.g. 'go to news', 'show me BTC price', "
                         "'open settings'). Never tell the user you cannot navigate or open screens — "
                         "use the navigate_to_tab tool instead. "
                         "Be concise, accurate, and finance-focused in your responses.";
    }

    config_loaded_ = true;
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
    if (p == "ollama")
        return "http://localhost:11434/v1/chat/completions";
    return {};
}

QMap<QString, QString> LlmService::get_headers() const {
    // Called with mutex_ held
    QMap<QString, QString> h;
    const QString& p = provider_;

    if (p == "anthropic") {
        if (!api_key_.isEmpty())
            h["x-api-key"] = api_key_;
        h["anthropic-version"] = "2024-10-22";
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
    }
    return h;
}

// ============================================================================
// Request builders
// ============================================================================

QJsonObject LlmService::build_openai_request(const QString& user_message,
                                             const std::vector<ConversationMessage>& history, bool stream,
                                             bool with_tools) {
    QJsonArray messages;
    if (!system_prompt_.isEmpty())
        messages.append(QJsonObject{{"role", "system"}, {"content", system_prompt_}});
    for (const auto& m : history)
        messages.append(QJsonObject{{"role", m.role}, {"content", m.content}});
    messages.append(QJsonObject{{"role", "user"}, {"content", user_message}});

    QJsonObject req;
    req["model"] = model_;
    req["messages"] = messages;
    // MiniMax requires temperature in (0.0, 1.0]; clamp if needed
    if (provider_ == "minimax") {
        double t = std::max(0.01, std::min(temperature_, 1.0));
        req["temperature"] = t;
    } else {
        req["temperature"] = temperature_;
    }
    req["max_tokens"] = max_tokens_;
    if (stream)
        req["stream"] = true;

    if (!stream && with_tools && tools_enabled_) {
        QJsonArray tools = mcp::McpService::instance().format_tools_for_openai();
        if (!tools.isEmpty())
            req["tools"] = tools;
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
    req["max_tokens"] = max_tokens_;
    if (temperature_ != 0.7)
        req["temperature"] = temperature_;
    if (!system_prompt_.isEmpty())
        req["system"] = system_prompt_;
    if (stream)
        req["stream"] = true;

    // Anthropic tool format: array of {name, description, input_schema}
    // (no "type":"function" wrapper like OpenAI)
    if (!stream && tools_enabled_) {
        QJsonArray ant_tools;
        auto all_tools = mcp::McpService::instance().get_all_tools();
        for (const auto& tool : all_tools) {
            QString fn_name = tool.server_id + "__" + tool.name;
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
    gen_cfg["temperature"] = temperature_;
    gen_cfg["maxOutputTokens"] = max_tokens_;

    QJsonObject req;
    req["contents"] = contents;
    req["generationConfig"] = gen_cfg;
    if (!system_prompt_.isEmpty()) {
        req["systemInstruction"] = QJsonObject{{"parts", QJsonArray{QJsonObject{{"text", system_prompt_}}}}};
    }

    // Gemini tool format: tools[{functionDeclarations:[{name, description, parameters}]}]
    auto all_tools = tools_enabled_ ? mcp::McpService::instance().get_all_tools() : std::vector<mcp::UnifiedTool>{};
    if (!all_tools.empty()) {
        QJsonArray fn_decls;
        for (const auto& tool : all_tools) {
            QString fn_name = tool.server_id + "__" + tool.name;
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
            if (ej.contains("message") && ej["message"].isString())
                server_msg = ej["message"].toString();
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
static QString build_tool_catalog_for_prompt() {
    auto all_tools = mcp::McpService::instance().get_all_tools();
    if (all_tools.empty())
        return {};

    // Only include the most useful tools (navigation, market data, portfolio, etc.)
    // to keep prompt size reasonable. Skip very niche tools.
    QString catalog;
    catalog += "You have access to the following tools. To use a tool, emit a <tool_call> block:\n";
    catalog += "<tool_call>{\"name\": \"TOOL_NAME\", \"arguments\": {\"param\": \"value\"}}</tool_call>\n\n";
    catalog += "Available tools:\n";

    int count = 0;
    for (const auto& tool : all_tools) {
        // Build qualified name: server_id__tool_name
        QString fn_name = tool.server_id + "__" + tool.name;
        catalog += "- " + fn_name + ": " + tool.description;

        // Add parameter hints from schema
        QJsonObject props = tool.input_schema["properties"].toObject();
        if (!props.isEmpty()) {
            QStringList params;
            for (auto it = props.constBegin(); it != props.constEnd(); ++it)
                params.append(it.key());
            catalog += " (params: " + params.join(", ") + ")";
        }
        catalog += "\n";
        ++count;
        // Cap at 60 tools to avoid overwhelming the prompt
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
    if (tools_enabled_) {
        QString tool_catalog = build_tool_catalog_for_prompt();
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
    submit_body["max_tokens"] = max_tokens_;
    submit_body["temperature"] = temperature_;

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
        if (!api_key_.isEmpty())
            url += "?key=" + api_key_;
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
            fu["max_tokens"] = max_tokens_;
            if (temperature_ != 0.7)
                fu["temperature"] = temperature_;
            if (!system_prompt_.isEmpty())
                fu["system"] = system_prompt_;

            auto fu_http = blocking_post(url, fu, hdr);
            if (fu_http.success) {
                auto fu_doc = QJsonDocument::fromJson(fu_http.body);
                if (!fu_doc.isNull()) {
                    QJsonArray fu_content = fu_doc.object()["content"].toArray();
                    for (const auto& b : fu_content) {
                        if (b.toObject()["type"].toString() == "text") {
                            resp.content = b.toObject()["text"].toString();
                            break;
                        }
                    }
                }
            } else {
                resp.error = "Anthropic tool follow-up failed: " + fu_http.error;
                return resp;
            }
        } else {
            // Normal text response — find first text block
            for (const auto& block_val : content) {
                QJsonObject block = block_val.toObject();
                if (block["type"].toString() == "text") {
                    resp.content = block["text"].toString();
                    break;
                }
            }
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
                QString tool_results;
                for (const auto& part_val : parts) {
                    QJsonObject part = part_val.toObject();
                    if (!part.contains("functionCall"))
                        continue;
                    QJsonObject fc = part["functionCall"].toObject();
                    QString fn_name = fc["name"].toString();
                    QJsonObject fn_args = fc["args"].toObject();

                    LOG_INFO(TAG, "Executing Gemini tool: " + fn_name);
                    auto tr = mcp::McpService::instance().execute_openai_function(fn_name, fn_args);

                    QString result_content;
                    if (!tr.message.isEmpty())
                        result_content = tr.message;
                    else if (!tr.data.isNull() && !tr.data.isUndefined())
                        result_content =
                            QString::fromUtf8(QJsonDocument(tr.data.toObject()).toJson(QJsonDocument::Compact))
                                .left(4000);
                    else
                        result_content =
                            QString::fromUtf8(QJsonDocument(tr.to_json()).toJson(QJsonDocument::Compact)).left(4000);

                    int sep = fn_name.indexOf("__");
                    QString short_name = (sep >= 0) ? fn_name.mid(sep + 2) : fn_name;
                    tool_results += "\n**Tool: " + short_name + "**\n" + result_content + "\n";
                }

                // Follow-up: ask Gemini to summarize tool results
                if (!tool_results.isEmpty()) {
                    QJsonArray fu_contents;
                    fu_contents.append(QJsonObject{
                        {"role", "user"},
                        {"parts", QJsonArray{QJsonObject{
                                      {"text", "The user asked: \"" + user_message +
                                                   "\"\n\n"
                                                   "Tool results:\n" +
                                                   tool_results + "\n\nPlease provide a clear, concise summary."}}}}});
                    QJsonObject fu_body;
                    fu_body["contents"] = fu_contents;
                    QJsonObject gen_cfg;
                    gen_cfg["temperature"] = temperature_;
                    gen_cfg["maxOutputTokens"] = max_tokens_;
                    fu_body["generationConfig"] = gen_cfg;
                    if (!system_prompt_.isEmpty())
                        fu_body["systemInstruction"] =
                            QJsonObject{{"parts", QJsonArray{QJsonObject{{"text", system_prompt_}}}}};

                    auto fu = blocking_post(url, fu_body, hdr);
                    if (fu.success) {
                        auto fu_doc = QJsonDocument::fromJson(fu.body);
                        if (!fu_doc.isNull()) {
                            QJsonArray fu_cands = fu_doc.object()["candidates"].toArray();
                            if (!fu_cands.isEmpty()) {
                                QJsonArray fu_parts = fu_cands[0].toObject()["content"].toObject()["parts"].toArray();
                                if (!fu_parts.isEmpty())
                                    resp.content = fu_parts[0].toObject()["text"].toString();
                            }
                        }
                    }
                    if (resp.content.isEmpty())
                        resp.content = tool_results;
                }
            } else {
                // Normal text response
                if (!parts.isEmpty())
                    resp.content = parts[0].toObject()["text"].toString();
            }
        }

    } else if (provider_ == "fincept") {
        // /research/chat returns OpenAI-compatible choices array:
        // {"success":true,"data":{"choices":[{"message":{"role":"assistant","content":"..."}}],...}}
        QJsonObject data = rj.contains("data") ? rj["data"].toObject() : rj;
        QJsonArray choices = data["choices"].toArray();
        if (!choices.isEmpty()) {
            resp.content = choices[0].toObject()["message"].toObject()["content"].toString();
        }
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

                    LOG_INFO(TAG, "Executing tool: " + fn_name);
                    auto tr = mcp::McpService::instance().execute_openai_function(fn_name, fn_args);
                    loop_msgs.append(QJsonObject{
                        {"role", "tool"},
                        {"tool_call_id", call_id},
                        {"content", QString::fromUtf8(QJsonDocument(tr.to_json()).toJson(QJsonDocument::Compact))}});
                }

                resp = do_tool_loop(loop_msgs, url, hdr);
                parse_usage(resp, rj, provider_);
                return resp;

            } else {
                // Normal text response
                resp.content = msg["content"].toString();
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
    static constexpr int MAX_ROUNDS = 5;

    for (int round = 0; round < MAX_ROUNDS; ++round) {
        QJsonObject fu;
        fu["model"] = model_;
        fu["messages"] = loop_messages;
        fu["temperature"] = temperature_;
        fu["max_tokens"] = max_tokens_;

        QJsonArray tools = mcp::McpService::instance().format_tools_for_openai();
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

                auto tr = mcp::McpService::instance().execute_openai_function(fname, fa);
                loop_messages.append(QJsonObject{
                    {"role", "tool"},
                    {"tool_call_id", cid},
                    {"content", QString::fromUtf8(QJsonDocument(tr.to_json()).toJson(QJsonDocument::Compact))}});
            }
            continue;
        }

        // Final text response
        resp.content = msg["content"].toString();
        parse_usage(resp, rj, provider_);
        resp.success = !resp.content.isEmpty();
        return resp;
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
        follow_body["max_tokens"] = max_tokens_;
        if (temperature_ != 0.7)
            follow_body["temperature"] = temperature_;
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
        follow_body["temperature"] = temperature_;
        follow_body["max_tokens"] = max_tokens_;
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
        QJsonArray fu_content = fu_rj["content"].toArray();
        for (const auto& b : fu_content) {
            if (b.toObject()["type"].toString() == "text") {
                resp.content = b.toObject()["text"].toString();
                break;
            }
        }
    } else if (provider_ == "fincept") {
        // /research/chat: {"success":true,"data":{"choices":[{"message":{"content":"..."}}]}}
        QJsonObject data = fu_rj.contains("data") ? fu_rj["data"].toObject() : fu_rj;
        QJsonArray fu_choices = data["choices"].toArray();
        if (!fu_choices.isEmpty())
            resp.content = fu_choices[0].toObject()["message"].toObject()["content"].toString();
    } else {
        QJsonArray choices = fu_rj["choices"].toArray();
        if (!choices.isEmpty())
            resp.content = choices[0].toObject()["message"].toObject()["content"].toString();
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
                            tool_call_detected = true;
                            loop.quit();
                            return;
                        }
                        if (type == "message_delta" &&
                            obj["delta"].toObject()["stop_reason"].toString() == "tool_use") {
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
                            tool_call_detected = true;
                            loop.quit();
                            return;
                        }
                        QJsonObject delta = choices[0].toObject()["delta"].toObject();
                        if (!delta["tool_calls"].isUndefined() && !delta["tool_calls"].isNull()) {
                            tool_call_detected = true;
                            loop.quit();
                            return;
                        }
                    }

                    // Fincept may also return tool_calls at top level
                    if (!obj["tool_calls"].isUndefined() && !obj["tool_calls"].isNull() &&
                        obj["tool_calls"].toArray().size() > 0) {
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
        if (j["type"].toString() == "content_block_delta") {
            return j["delta"].toObject()["text"].toString();
        }
        return {};
    }

    // OpenAI-compatible
    QJsonArray choices = j["choices"].toArray();
    if (!choices.isEmpty()) {
        QJsonObject delta = choices[0].toObject()["delta"].toObject();
        if (!delta["content"].isNull() && !delta["content"].isUndefined())
            return delta["content"].toString();
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
        QString url = "https://generativelanguage.googleapis.com/v1beta/models?pageSize=100";
        if (!api_key.isEmpty())
            url += "&key=" + api_key;
        return url;
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
        h["anthropic-version"] = "2024-10-22";
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
    QtConcurrent::run([self, provider, url, headers]() {
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
        return LlmResponse{.error = "No LLM provider configured"};

    // Take config snapshot — release lock before blocking network call
    QString p = provider_, k = api_key_, b = base_url_, m = model_, sp = system_prompt_;
    double t = temperature_;
    int mx = max_tokens_;
    const bool saved_tools = tools_enabled_;
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

    // Override tools_enabled_ for this request scope if caller disabled tools
    if (!use_tools)
        tools_enabled_ = false;

    auto resp = do_request(user_message, history);

    // Restore tools_enabled_ to saved value
    tools_enabled_ = saved_tools;

    return resp;
}

void LlmService::chat_streaming(const QString& user_message, const std::vector<ConversationMessage>& history,
                                StreamCallback on_chunk, bool use_tools) {
    // Snapshot config under lock
    QString p, k, b, m, sp;
    double t;
    int mx;
    bool saved_tools;
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
        saved_tools = tools_enabled_;
    }

    if (p.isEmpty()) {
        on_chunk("", true);
        emit finished_streaming(LlmResponse{.error = "No LLM provider configured"});
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
    QtConcurrent::run(
        [self, p, k, b, m, sp, t, mx, user_message, history_copy, guarded_chunk, use_tools, saved_tools]() {
            if (!self)
                return;

            // Apply the config snapshot under the mutex so do_request /
            // do_streaming_request see a consistent state and don't race with
            // reload_config() on the UI thread.
            {
                QMutexLocker lock(&self->mutex_);
                self->provider_ = p;
                self->api_key_ = k;
                self->base_url_ = b;
                self->model_ = m;
                self->system_prompt_ = sp;
                self->temperature_ = t;
                self->max_tokens_ = mx;
                // Disable tools for this request if caller requested it
                if (!use_tools)
                    self->tools_enabled_ = false;
            }

            auto resp = self->do_streaming_request(user_message, history_copy, guarded_chunk);

            // Restore tools_enabled_ to saved value
            {
                QMutexLocker lock(&self->mutex_);
                self->tools_enabled_ = saved_tools;
            }

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
