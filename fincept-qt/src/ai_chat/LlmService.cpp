// LlmService.cpp — Multi-provider LLM API client (Qt port)
// Translated from fincept-cpp llm_service.cpp:
//   libcurl  → QNetworkAccessManager + QEventLoop (blocking on bg thread)
//   std::future → QtConcurrent::run + signal
//   nlohmann/json → QJsonObject/QJsonArray

#include "ai_chat/LlmService.h"
#include "mcp/McpService.h"
#include "storage/repositories/LlmConfigRepository.h"
#include "storage/repositories/SettingsRepository.h"
#include "core/logging/Logger.h"

#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonValue>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QThread>
#include <QTimer>
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

void LlmService::ensure_config() {
    // Called with mutex_ held
    if (config_loaded_)
        return;

    provider_ = model_ = api_key_ = base_url_ = system_prompt_ = {};
    temperature_ = 0.7;
    max_tokens_  = 4096;

    auto providers = LlmConfigRepository::instance().list_providers();
    if (providers.is_ok()) {
        for (const auto& c : providers.value()) {
            if (c.is_active) {
                provider_ = c.provider.toLower();
                api_key_  = c.api_key;
                base_url_ = c.base_url;
                model_    = c.model;
                break;
            }
        }
        // Fallback: first config if none active
        if (provider_.isEmpty() && !providers.value().isEmpty()) {
            const auto& c = providers.value().first();
            provider_ = c.provider.toLower();
            api_key_  = c.api_key;
            base_url_ = c.base_url;
            model_    = c.model;
        }
    }

    // Fallback: if no provider configured, use Fincept with session API key
    if (provider_.isEmpty()) {
        provider_ = "fincept";
        model_    = "fincept-llm";
        base_url_ = "https://api.fincept.in/research/llm";
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
        temperature_   = gs.value().temperature;
        max_tokens_    = gs.value().max_tokens;
        system_prompt_ = gs.value().system_prompt;
    }

    config_loaded_ = true;
}

// ============================================================================
// Accessors
// ============================================================================

QString LlmService::active_provider() const {
    QMutexLocker lock(&mutex_);
    const_cast<LlmService*>(this)->ensure_config();
    return provider_;
}

QString LlmService::active_model() const {
    QMutexLocker lock(&mutex_);
    const_cast<LlmService*>(this)->ensure_config();
    return model_;
}

bool LlmService::is_configured() const {
    QMutexLocker lock(&mutex_);
    const_cast<LlmService*>(this)->ensure_config();
    if (provider_.isEmpty())
        return false;
    if (provider_requires_api_key(provider_))
        return !api_key_.isEmpty();
    return true;
}

// ============================================================================
// Endpoint + headers
// ============================================================================

QString LlmService::get_endpoint_url() const {
    // Called with mutex_ held
    const QString& p = provider_;

    // Fincept: base_url IS the full endpoint (no path suffix needed)
    if (p == "fincept") {
        if (!base_url_.isEmpty())
            return base_url_;
        return "https://api.fincept.in/research/llm";
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

    if (p == "openai")                    return "https://api.openai.com/v1/chat/completions";
    if (p == "anthropic")                 return "https://api.anthropic.com/v1/messages";
    if (p == "gemini" || p == "google")   return "https://generativelanguage.googleapis.com/v1beta/models/"
                                                  + model_ + ":generateContent";
    if (p == "groq")                      return "https://api.groq.com/openai/v1/chat/completions";
    if (p == "deepseek")                  return "https://api.deepseek.com/v1/chat/completions";
    if (p == "openrouter")                return "https://openrouter.ai/api/v1/chat/completions";
    if (p == "ollama")                    return "http://localhost:11434/v1/chat/completions";
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

QJsonObject LlmService::build_openai_request(
    const QString& user_message,
    const std::vector<ConversationMessage>& history,
    bool stream, bool with_tools)
{
    QJsonArray messages;
    if (!system_prompt_.isEmpty())
        messages.append(QJsonObject{{"role", "system"}, {"content", system_prompt_}});
    for (const auto& m : history)
        messages.append(QJsonObject{{"role", m.role}, {"content", m.content}});
    messages.append(QJsonObject{{"role", "user"}, {"content", user_message}});

    QJsonObject req;
    req["model"]       = model_;
    req["messages"]    = messages;
    req["temperature"] = temperature_;
    req["max_tokens"]  = max_tokens_;
    if (stream)
        req["stream"] = true;

    if (!stream && with_tools) {
        QJsonArray tools = mcp::McpService::instance().format_tools_for_openai();
        if (!tools.isEmpty())
            req["tools"] = tools;
    }
    return req;
}

QJsonObject LlmService::build_anthropic_request(
    const QString& user_message,
    const std::vector<ConversationMessage>& history,
    bool stream)
{
    QJsonArray messages;
    for (const auto& m : history) {
        if (m.role != "system")
            messages.append(QJsonObject{{"role", m.role}, {"content", m.content}});
    }
    messages.append(QJsonObject{{"role", "user"}, {"content", user_message}});

    QJsonObject req;
    req["model"]      = model_;
    req["messages"]   = messages;
    req["max_tokens"] = max_tokens_;
    if (temperature_ != 0.7)
        req["temperature"] = temperature_;
    if (!system_prompt_.isEmpty())
        req["system"] = system_prompt_;
    if (stream)
        req["stream"] = true;
    return req;
}

QJsonObject LlmService::build_gemini_request(
    const QString& user_message,
    const std::vector<ConversationMessage>& history)
{
    QJsonArray contents;
    for (const auto& m : history) {
        if (m.role == "system") continue;
        QString role = (m.role == "assistant") ? "model" : "user";
        contents.append(QJsonObject{
            {"role", role},
            {"parts", QJsonArray{QJsonObject{{"text", m.content}}}}
        });
    }
    contents.append(QJsonObject{
        {"role", "user"},
        {"parts", QJsonArray{QJsonObject{{"text", user_message}}}}
    });

    QJsonObject gen_cfg;
    gen_cfg["temperature"]     = temperature_;
    gen_cfg["maxOutputTokens"] = max_tokens_;

    QJsonObject req;
    req["contents"]         = contents;
    req["generationConfig"] = gen_cfg;
    if (!system_prompt_.isEmpty()) {
        req["systemInstruction"] = QJsonObject{
            {"parts", QJsonArray{QJsonObject{{"text", system_prompt_}}}}
        };
    }
    return req;
}

QJsonObject LlmService::build_fincept_request(
    const QString& user_message,
    const std::vector<ConversationMessage>& history,
    bool with_tools)
{
    QString prompt;
    if (!system_prompt_.isEmpty())
        prompt += "System: " + system_prompt_ + "\n\n";
    for (const auto& m : history) {
        if (m.role == "user")
            prompt += "User: " + m.content + "\n\n";
        else if (m.role == "assistant")
            prompt += "Assistant: " + m.content + "\n\n";
    }
    prompt += "User: " + user_message;

    QJsonObject req;
    req["prompt"]      = prompt;
    req["temperature"] = temperature_;
    req["max_tokens"]  = max_tokens_;

    if (with_tools) {
        QJsonArray all_tools = mcp::McpService::instance().format_tools_for_openai();
        if (!all_tools.isEmpty()) {
            QJsonArray fns;
            for (const auto& t : all_tools) {
                QJsonObject fn = t.toObject()["function"].toObject();
                // Validate: name and parameters must be non-empty
                if (fn.isEmpty() || fn["name"].toString().isEmpty())
                    continue;
                if (!fn.contains("parameters"))
                    fn["parameters"] = QJsonObject{{"type", "object"}, {"properties", QJsonObject{}}};
                fns.append(fn);
            }
            if (!fns.isEmpty())
                req["tools"] = fns;
        }
    }
    return req;
}

// ============================================================================
// Blocking HTTP POST (runs on background thread, uses its own NAM + event loop)
// ============================================================================

LlmService::HttpResult LlmService::blocking_post(
    const QString& url,
    const QJsonObject& body,
    const QMap<QString, QString>& headers,
    int timeout_ms)
{
    HttpResult result;

    // Each background thread call needs its own QNetworkAccessManager
    QNetworkAccessManager nam;
    QNetworkRequest req{QUrl(url)};
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    for (auto it = headers.constBegin(); it != headers.constEnd(); ++it)
        req.setRawHeader(it.key().toUtf8(), it.value().toUtf8());

    QByteArray json_data = QJsonDocument(body).toJson(QJsonDocument::Compact);
    QNetworkReply* reply = nam.post(req, json_data);

    // Block with QEventLoop until reply finishes or times out
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(timeout_ms);
    loop.exec();

    if (timer.isActive()) {
        timer.stop();
    } else {
        // Timeout
        reply->abort();
        result.error = "Request timed out";
        reply->deleteLater();
        return result;
    }

    result.status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    result.body   = reply->readAll();

    if (result.status >= 200 && result.status < 300) {
        result.success = true;
    } else {
        // Prefer server's JSON error message over Qt's generic network string.
        // Parse body first; fall back to Qt errorString if body has no message.
        QString server_msg;
        if (!result.body.isEmpty()) {
            auto err_doc = QJsonDocument::fromJson(result.body);
            if (!err_doc.isNull() && err_doc.isObject()) {
                QJsonObject ej = err_doc.object();
                // Fincept: {"success":false,"message":"..."}
                if (ej.contains("message") && ej["message"].isString())
                    server_msg = ej["message"].toString();
                // OpenAI: {"error":{"message":"..."}} or {"error":"..."}
                if (server_msg.isEmpty() && ej.contains("error")) {
                    QJsonValue ev = ej["error"];
                    if (ev.isString()) server_msg = ev.toString();
                    else if (ev.isObject()) server_msg = ev.toObject()["message"].toString();
                }
                // Anthropic: {"error":{"type":"...","message":"..."}}
                if (server_msg.isEmpty() && ej.contains("detail"))
                    server_msg = ej["detail"].toString();
            }
        }
        result.error = server_msg.isEmpty()
            ? QString("HTTP %1: %2").arg(result.status).arg(reply->errorString())
            : QString("HTTP %1: %2").arg(result.status).arg(server_msg);
    }

    reply->deleteLater();
    return result;
}

// ============================================================================
// Non-streaming request
// ============================================================================

LlmResponse LlmService::do_request(
    const QString& user_message,
    const std::vector<ConversationMessage>& history)
{
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
        req_body = build_fincept_request(user_message, history, false);
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
        if (!content.isEmpty())
            resp.content = content[0].toObject()["text"].toString();

    } else if (provider_ == "gemini" || provider_ == "google") {
        QJsonArray cands = rj["candidates"].toArray();
        if (!cands.isEmpty()) {
            QJsonArray parts = cands[0].toObject()["content"].toObject()["parts"].toArray();
            if (!parts.isEmpty())
                resp.content = parts[0].toObject()["text"].toString();
        }

    } else if (provider_ == "fincept") {
        QJsonObject data = rj.contains("data") ? rj["data"].toObject() : rj;

        // Check for tool calls
        QJsonArray tcs = data["tool_calls"].toArray();
        if (!tcs.isEmpty()) {
            LOG_INFO(TAG, QString("Fincept returned %1 tool calls").arg(tcs.size()));
            QString tool_results;
            for (const auto& tc_val : tcs) {
                QJsonObject tc = tc_val.toObject();
                QString fn_name = tc.contains("function")
                    ? tc["function"].toObject()["name"].toString()
                    : tc["name"].toString();
                QString args_str = tc.contains("function")
                    ? tc["function"].toObject()["arguments"].toString()
                    : tc["arguments"].toString("{}");

                QJsonObject fn_args = QJsonDocument::fromJson(args_str.toUtf8()).object();
                auto tool_res = mcp::McpService::instance().execute_openai_function(fn_name, fn_args);

                QString result_content;
                if (!tool_res.message.isEmpty())
                    result_content = tool_res.message;
                else if (!tool_res.data.isNull() && !tool_res.data.isUndefined())
                    result_content = QJsonDocument(tool_res.data.toObject()).toJson().left(2000);
                else
                    result_content = QJsonDocument(tool_res.to_json()).toJson().left(2000);

                int sep = fn_name.indexOf("__");
                QString short_name = (sep >= 0) ? fn_name.mid(sep + 2) : fn_name;
                tool_results += "\n**Tool: " + short_name + "**\n" + result_content + "\n";
            }

            // Follow-up request with tool results
            if (!tool_results.isEmpty()) {
                QString follow_prompt = "User asked: \"" + user_message + "\"\n\n"
                    "I retrieved this data:\n" + tool_results +
                    "\n\nPlease provide a clear, concise summary of this data in natural language.";

                QJsonObject follow_body{{"prompt", follow_prompt},
                                        {"temperature", temperature_},
                                        {"max_tokens", max_tokens_}};
                auto fu = blocking_post(url, follow_body, hdr);
                if (fu.success) {
                    auto fu_doc = QJsonDocument::fromJson(fu.body);
                    if (!fu_doc.isNull()) {
                        QJsonObject fu_d = fu_doc.object().contains("data")
                            ? fu_doc.object()["data"].toObject()
                            : fu_doc.object();
                        for (const QString& k : {"response","content","answer","text"}) {
                            if (fu_d.contains(k)) { resp.content = fu_d[k].toString(); break; }
                        }
                    }
                }
            }
        } else {
            for (const QString& k : {"response","content","answer","text","result"}) {
                if (data.contains(k)) { resp.content = data[k].toString(); break; }
            }
            if (resp.content.isEmpty() && data.contains("ai_response")) {
                QJsonValue ai = data["ai_response"];
                if (ai.isObject() && ai.toObject().contains("content"))
                    resp.content = ai.toObject()["content"].toString();
                else if (ai.isString())
                    resp.content = ai.toString();
            }
        }

    } else {
        // OpenAI-compatible — check for tool calls
        QJsonArray choices = rj["choices"].toArray();
        if (!choices.isEmpty()) {
            QJsonObject msg = choices[0].toObject()["message"].toObject();
            QJsonArray tcs  = msg["tool_calls"].toArray();

            if (!tcs.isEmpty()) {
                LOG_INFO(TAG, QString("LLM requested %1 tool calls").arg(tcs.size()));

                // Build initial messages array for tool loop
                QJsonArray loop_msgs;
                if (!system_prompt_.isEmpty())
                    loop_msgs.append(QJsonObject{{"role","system"},{"content",system_prompt_}});
                for (const auto& h : history)
                    loop_msgs.append(QJsonObject{{"role",h.role},{"content",h.content}});
                loop_msgs.append(QJsonObject{{"role","user"},{"content",user_message}});
                loop_msgs.append(msg); // assistant message with tool_calls

                // Execute tool calls and append results
                for (const auto& tc_val : tcs) {
                    QJsonObject tc = tc_val.toObject();
                    QString call_id = tc["id"].toString();
                    QString fn_name = tc["function"].toObject()["name"].toString();
                    QJsonObject fn_args = QJsonDocument::fromJson(
                        tc["function"].toObject()["arguments"].toString("{}").toUtf8()).object();

                    LOG_INFO(TAG, "Executing tool: " + fn_name);
                    auto tr = mcp::McpService::instance().execute_openai_function(fn_name, fn_args);
                    loop_msgs.append(QJsonObject{
                        {"role",         "tool"},
                        {"tool_call_id", call_id},
                        {"content",      QString::fromUtf8(QJsonDocument(tr.to_json()).toJson(QJsonDocument::Compact))}
                    });
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

    parse_usage(resp, rj, provider_);
    resp.success = true;
    return resp;
}

// ============================================================================
// Tool-call follow-up loop (OpenAI-compatible)
// ============================================================================

LlmResponse LlmService::do_tool_loop(
    QJsonArray loop_messages,
    const QString& url,
    const QMap<QString, QString>& headers)
{
    LlmResponse resp;
    static constexpr int MAX_ROUNDS = 5;

    for (int round = 0; round < MAX_ROUNDS; ++round) {
        QJsonObject fu;
        fu["model"]       = model_;
        fu["messages"]    = loop_messages;
        fu["temperature"] = temperature_;
        fu["max_tokens"]  = max_tokens_;

        QJsonArray tools = mcp::McpService::instance().format_tools_for_openai();
        if (!tools.isEmpty())
            fu["tools"] = tools;

        auto http = blocking_post(url, fu, headers);
        if (!http.success) {
            resp.error = "Tool follow-up failed: " + http.error;
            return resp;
        }

        auto doc = QJsonDocument::fromJson(http.body);
        if (doc.isNull()) { resp.error = "Parse error in tool follow-up"; return resp; }
        QJsonObject rj = doc.object();

        QJsonArray choices = rj["choices"].toArray();
        if (choices.isEmpty()) { resp.error = "No choices in tool follow-up"; return resp; }

        QJsonObject msg = choices[0].toObject()["message"].toObject();
        QJsonArray tcs  = msg["tool_calls"].toArray();

        if (!tcs.isEmpty()) {
            // Another round
            loop_messages.append(msg);
            for (const auto& tc_val : tcs) {
                QJsonObject tc = tc_val.toObject();
                QString cid    = tc["id"].toString();
                QString fname  = tc["function"].toObject()["name"].toString();
                QJsonObject fa = QJsonDocument::fromJson(
                    tc["function"].toObject()["arguments"].toString("{}").toUtf8()).object();

                auto tr = mcp::McpService::instance().execute_openai_function(fname, fa);
                loop_messages.append(QJsonObject{
                    {"role",         "tool"},
                    {"tool_call_id", cid},
                    {"content",      QString::fromUtf8(QJsonDocument(tr.to_json()).toJson(QJsonDocument::Compact))}
                });
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
// Streaming request (SSE)
// ============================================================================

LlmResponse LlmService::do_streaming_request(
    const QString& user_message,
    const std::vector<ConversationMessage>& history,
    StreamCallback on_chunk)
{
    // Gemini and Fincept: fall back to non-streaming + replay
    if (provider_ == "gemini" || provider_ == "google" || provider_ == "fincept") {
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
    QJsonObject req_body = (provider_ == "anthropic")
        ? build_anthropic_request(user_message, history, true)
        : build_openai_request(user_message, history, true, false);

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
            if (nl < 0) break;
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
                on_chunk("", true);
                done = true;
                loop.quit();
                return;
            }

            QString chunk = parse_sse_chunk(data, provider_);
            if (!chunk.isEmpty()) {
                accumulated += chunk;
                on_chunk(chunk, false);
            }
        }
    });

    loop.exec();
    timeout.stop();

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
        resp.prompt_tokens     = u["input_tokens"].toInt();
        resp.completion_tokens = u["output_tokens"].toInt();
        resp.total_tokens      = resp.prompt_tokens + resp.completion_tokens;
    } else {
        resp.prompt_tokens     = u["prompt_tokens"].toInt();
        resp.completion_tokens = u["completion_tokens"].toInt();
        resp.total_tokens      = u["total_tokens"].toInt();
    }
}

// ============================================================================
// Dynamic model fetching
// ============================================================================

QString LlmService::get_models_url(const QString& provider, const QString& api_key,
                                    const QString& base_url) {
    const QString p = provider.toLower();

    // Custom base_url (except fincept)
    if (!base_url.isEmpty() && p != "fincept") {
        QString base = base_url;
        if (base.endsWith('/')) base.chop(1);
        if (p == "anthropic") return base + "/v1/models?limit=1000";
        return base + "/v1/models";
    }

    if (p == "openai")      return "https://api.openai.com/v1/models";
    if (p == "anthropic")   return "https://api.anthropic.com/v1/models?limit=1000";
    if (p == "gemini" || p == "google") {
        QString url = "https://generativelanguage.googleapis.com/v1beta/models?pageSize=100";
        if (!api_key.isEmpty()) url += "&key=" + api_key;
        return url;
    }
    if (p == "groq")        return "https://api.groq.com/openai/v1/models";
    if (p == "deepseek")    return "https://api.deepseek.com/models";
    if (p == "openrouter")  return "https://openrouter.ai/api/v1/models";
    if (p == "ollama") {
        QString base = base_url.isEmpty() ? "http://localhost:11434" : base_url;
        if (base.endsWith('/')) base.chop(1);
        return base + "/api/tags";
    }
    return {};
}

QMap<QString, QString> LlmService::get_models_headers(const QString& provider,
                                                       const QString& api_key) {
    QMap<QString, QString> h;
    const QString p = provider.toLower();

    if (p == "anthropic") {
        if (!api_key.isEmpty()) h["x-api-key"] = api_key;
        h["anthropic-version"] = "2023-06-01";
    } else if (p == "gemini" || p == "google") {
        // Key is in URL query param
        if (!api_key.isEmpty()) h["x-goog-api-key"] = api_key;
    } else if (p == "ollama" || p == "fincept") {
        // No auth needed
    } else {
        // OpenAI-compatible: OpenAI, Groq, DeepSeek, OpenRouter
        if (!api_key.isEmpty()) h["Authorization"] = "Bearer " + api_key;
    }
    return h;
}

QStringList LlmService::parse_models_response(const QString& provider,
                                                const QByteArray& body) {
    QStringList models;
    auto doc = QJsonDocument::fromJson(body);
    if (doc.isNull() || !doc.isObject()) return models;
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
                if (method.toString() == "generateContent") { can_generate = true; break; }
            }
            if (!can_generate) continue;

            QString name = m["name"].toString();
            // Strip "models/" prefix
            if (name.startsWith("models/")) name = name.mid(7);
            if (!name.isEmpty()) models.append(name);
        }
    } else if (p == "ollama") {
        // {"models": [{"name": "llama3:8b", ...}]}
        QJsonArray arr = root["models"].toArray();
        for (const auto& v : arr) {
            QString name = v.toObject()["name"].toString();
            if (!name.isEmpty()) models.append(name);
        }
    } else {
        // OpenAI-compatible: OpenAI, Anthropic, Groq, DeepSeek, OpenRouter
        // {"data": [{"id": "model-id", ...}]}
        QJsonArray arr = root["data"].toArray();
        for (const auto& v : arr) {
            QString id = v.toObject()["id"].toString();
            if (!id.isEmpty()) models.append(id);
        }
    }

    models.sort(Qt::CaseInsensitive);
    return models;
}

void LlmService::fetch_models(const QString& provider, const QString& api_key,
                                const QString& base_url) {
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
                if (error.isEmpty()) error = "HTTP " + QString::number(status);
            } else {
                models = parse_models_response(provider, reply->readAll());
                if (models.isEmpty())
                    error = "No models found";
            }
        }

        reply->deleteLater();

        if (self) {
            QMetaObject::invokeMethod(self, [self, provider, models, error]() {
                if (self)
                    emit self->models_fetched(provider, models, error);
            }, Qt::QueuedConnection);
        }
    });
}

// ============================================================================
// Public API
// ============================================================================

LlmResponse LlmService::chat(
    const QString& user_message,
    const std::vector<ConversationMessage>& history)
{
    QMutexLocker lock(&mutex_);
    ensure_config();

    if (provider_.isEmpty())
        return LlmResponse{.error = "No LLM provider configured"};

    // Take config snapshot — release lock before blocking network call
    QString p = provider_, k = api_key_, b = base_url_, m = model_, sp = system_prompt_;
    double  t = temperature_;
    int     mx = max_tokens_;
    lock.unlock();

    // Restore snapshot into members for use by helper methods
    // (helpers read member variables, so we need them set)
    // This is safe because chat() is always called from a background thread.
    provider_ = p; api_key_ = k; base_url_ = b; model_ = m;
    system_prompt_ = sp; temperature_ = t; max_tokens_ = mx;

    return do_request(user_message, history);
}

void LlmService::chat_streaming(
    const QString& user_message,
    const std::vector<ConversationMessage>& history,
    StreamCallback on_chunk)
{
    // Snapshot config under lock
    QString p, k, b, m, sp;
    double  t;
    int     mx;
    {
        QMutexLocker lock(&mutex_);
        ensure_config();
        p  = provider_;  k  = api_key_;
        b  = base_url_;  m  = model_;
        sp = system_prompt_; t = temperature_; mx = max_tokens_;
    }

    if (p.isEmpty()) {
        on_chunk("", true);
        emit finished_streaming(LlmResponse{.error = "No LLM provider configured"});
        return;
    }

    // Copy history for lambda capture
    std::vector<ConversationMessage> history_copy = history;

    QPointer<LlmService> self = this;
    QtConcurrent::run([self, p, k, b, m, sp, t, mx,
                       user_message, history_copy, on_chunk]() {
        if (!self) return;

        // Set config snapshot on this thread
        self->provider_      = p;  self->api_key_  = k;
        self->base_url_      = b;  self->model_    = m;
        self->system_prompt_ = sp; self->temperature_ = t;
        self->max_tokens_    = mx;

        auto resp = self->do_streaming_request(user_message, history_copy, on_chunk);

        if (self) {
            QMetaObject::invokeMethod(self, [self, resp]() {
                if (self)
                    emit self->finished_streaming(resp);
            }, Qt::QueuedConnection);
        }
    });
}

} // namespace fincept::ai_chat
