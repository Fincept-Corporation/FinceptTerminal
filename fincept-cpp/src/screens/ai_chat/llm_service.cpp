#include "llm_service.h"
#include "http/http_client.h"
#include "core/logger.h"
#include "mcp/mcp_service.h"
#include <curl/curl.h>
#include <sstream>
#include <algorithm>
#include <cstring>

namespace fincept::ai_chat {

// ============================================================================
// Singleton
// ============================================================================
LLMService& LLMService::instance() {
    static LLMService svc;
    return svc;
}

void LLMService::reload_config() {
    std::lock_guard<std::mutex> lock(mutex_);
    config_loaded_ = false;
}

void LLMService::ensure_config() {
    if (config_loaded_) return;

    auto configs = db::ops::get_llm_configs();
    config_ = {};
    for (auto& c : configs) {
        if (c.is_active) {
            config_ = c;
            break;
        }
    }
    // Fallback: use first config if none active
    if (config_.provider.empty() && !configs.empty()) {
        config_ = configs[0];
    }

    // Normalize provider name to lowercase for consistent matching
    std::transform(config_.provider.begin(), config_.provider.end(),
                   config_.provider.begin(), ::tolower);

    global_settings_ = db::ops::get_llm_global_settings();
    config_loaded_ = true;
}

std::string LLMService::active_provider() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_.provider;
}

std::string LLMService::active_model() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_.model;
}

bool LLMService::is_configured() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (config_.provider.empty()) return false;
    if (provider_requires_api_key(config_.provider)) {
        return config_.api_key.has_value() && !config_.api_key->empty();
    }
    return true;
}

// ============================================================================
// Endpoint URLs
// ============================================================================
std::string LLMService::get_endpoint_url() const {
    const auto& p = config_.provider;

    // Custom base_url takes priority (except for providers with fixed endpoints)
    if (config_.base_url && !config_.base_url->empty() && p != "fincept") {
        std::string base = *config_.base_url;
        // Ensure no trailing slash
        if (!base.empty() && base.back() == '/') base.pop_back();

        if (p == "anthropic") {
            return base + "/v1/messages";
        }
        return base + "/v1/chat/completions";
    }

    if (p == "openai")      return "https://api.openai.com/v1/chat/completions";
    if (p == "anthropic")   return "https://api.anthropic.com/v1/messages";
    if (p == "gemini" || p == "google")
        return "https://generativelanguage.googleapis.com/v1beta/models/" +
               config_.model + ":generateContent";
    if (p == "groq")        return "https://api.groq.com/openai/v1/chat/completions";
    if (p == "deepseek")    return "https://api.deepseek.com/v1/chat/completions";
    if (p == "openrouter")  return "https://openrouter.ai/api/v1/chat/completions";
    if (p == "ollama")      return "http://localhost:11434/v1/chat/completions";
    if (p == "fincept")     return "https://api.fincept.in/research/llm";

    return "";
}

std::map<std::string, std::string> LLMService::get_headers() const {
    std::map<std::string, std::string> headers;
    const auto& p = config_.provider;
    std::string key = config_.api_key.value_or("");

    if (p == "anthropic") {
        if (!key.empty()) headers["x-api-key"] = key;
        headers["anthropic-version"] = "2023-06-01";
    } else if (p == "gemini" || p == "google") {
        // Gemini uses query param, but we also set header
        if (!key.empty()) headers["x-goog-api-key"] = key;
    } else if (p == "fincept") {
        // Fincept uses X-API-Key from fincept_api_key setting (auto-set on login)
        // Also supports X-Session-Token for session-based auth
        if (key.empty()) {
            // Try to get fincept_api_key from settings
            auto stored_key = db::ops::get_setting("fincept_api_key");
            if (stored_key.has_value() && !stored_key->empty()) {
                key = *stored_key;
            }
        }
        if (!key.empty()) headers["X-API-Key"] = key;
        // Add session token if available
        auto session_token = db::ops::get_setting("session_token");
        if (session_token.has_value() && !session_token->empty()) {
            headers["X-Session-Token"] = *session_token;
        }
    } else {
        // OpenAI-compatible (openai, groq, deepseek, openrouter, ollama)
        if (!key.empty()) headers["Authorization"] = "Bearer " + key;
    }

    return headers;
}

// ============================================================================
// Request builders
// ============================================================================
json LLMService::build_openai_request(const std::string& user_message,
                                       const std::vector<ConversationMessage>& history,
                                       bool stream) {
    json messages = json::array();

    // System prompt
    if (!global_settings_.system_prompt.empty()) {
        messages.push_back({{"role", "system"}, {"content", global_settings_.system_prompt}});
    }

    // History
    for (auto& msg : history) {
        messages.push_back({{"role", msg.role}, {"content", msg.content}});
    }

    // Current message
    messages.push_back({{"role", "user"}, {"content", user_message}});

    json req;
    req["model"] = config_.model;
    req["messages"] = messages;
    req["temperature"] = global_settings_.temperature;
    req["max_tokens"] = global_settings_.max_tokens;
    if (stream) req["stream"] = true;

    // Inject MCP tools for function calling (non-streaming only for tool loop)
    if (!stream) {
        auto tools = mcp::MCPService::instance().format_tools_for_openai();
        if (!tools.empty()) {
            req["tools"] = tools;
        }
    }

    return req;
}

json LLMService::build_anthropic_request(const std::string& user_message,
                                          const std::vector<ConversationMessage>& history,
                                          bool stream) {
    json messages = json::array();

    // Anthropic doesn't use system in messages array
    for (auto& msg : history) {
        if (msg.role != "system") {
            messages.push_back({{"role", msg.role}, {"content", msg.content}});
        }
    }
    messages.push_back({{"role", "user"}, {"content", user_message}});

    json req;
    req["model"] = config_.model;
    req["messages"] = messages;
    req["max_tokens"] = global_settings_.max_tokens;
    if (global_settings_.temperature != 0.7) {
        req["temperature"] = global_settings_.temperature;
    }
    if (!global_settings_.system_prompt.empty()) {
        req["system"] = global_settings_.system_prompt;
    }
    if (stream) req["stream"] = true;

    return req;
}

json LLMService::build_gemini_request(const std::string& user_message,
                                       const std::vector<ConversationMessage>& history) {
    json contents = json::array();

    for (auto& msg : history) {
        if (msg.role == "system") continue;
        std::string role = (msg.role == "assistant") ? "model" : "user";
        contents.push_back({
            {"role", role},
            {"parts", json::array({{{"text", msg.content}}})}
        });
    }
    contents.push_back({
        {"role", "user"},
        {"parts", json::array({{{"text", user_message}}})}
    });

    json req;
    req["contents"] = contents;

    json gen_config;
    gen_config["temperature"] = global_settings_.temperature;
    gen_config["maxOutputTokens"] = global_settings_.max_tokens;
    req["generationConfig"] = gen_config;

    if (!global_settings_.system_prompt.empty()) {
        req["systemInstruction"] = {
            {"parts", json::array({{{"text", global_settings_.system_prompt}}})}
        };
    }

    return req;
}

json LLMService::build_fincept_request(const std::string& user_message,
                                        const std::vector<ConversationMessage>& history,
                                        bool with_tools) {
    // Fincept uses a simple { prompt, temperature, max_tokens, tools? } format
    // Conversation history is concatenated into the prompt string
    std::string prompt;

    if (!global_settings_.system_prompt.empty()) {
        prompt += "System: " + global_settings_.system_prompt + "\n\n";
    }

    for (auto& msg : history) {
        if (msg.role == "user") {
            prompt += "User: " + msg.content + "\n\n";
        } else if (msg.role == "assistant") {
            prompt += "Assistant: " + msg.content + "\n\n";
        }
    }

    prompt += "User: " + user_message;

    json req;
    req["prompt"] = prompt;
    req["temperature"] = global_settings_.temperature;
    req["max_tokens"] = global_settings_.max_tokens;

    // Add MCP tools if requested
    if (with_tools) {
        auto tools = mcp::MCPService::instance().format_tools_for_openai();
        if (!tools.empty()) {
            // Fincept expects tools in OpenAI function format (just the function part)
            json tool_fns = json::array();
            for (auto& t : tools) {
                if (t.contains("function")) {
                    tool_fns.push_back(t["function"]);
                }
            }
            if (!tool_fns.empty()) {
                req["tools"] = tool_fns;
            }
        }
    }

    return req;
}

// ============================================================================
// Non-streaming request
// ============================================================================
LLMResponse LLMService::do_request(const std::string& user_message,
                                    const std::vector<ConversationMessage>& history) {
    LLMResponse resp;
    const auto& p = config_.provider;

    std::string url = get_endpoint_url();
    if (url.empty()) {
        resp.error = "No endpoint URL for provider: " + p;
        return resp;
    }

    auto headers = get_headers();
    json req_body;

    if (p == "anthropic") {
        req_body = build_anthropic_request(user_message, history, false);
    } else if (p == "gemini" || p == "google") {
        req_body = build_gemini_request(user_message, history);
        // Add API key as query param
        std::string key = config_.api_key.value_or("");
        if (!key.empty()) url += "?key=" + key;
    } else if (p == "fincept") {
        // Build with tools if any are available
        bool has_tools = mcp::MCPService::instance().tool_count() > 0;
        req_body = build_fincept_request(user_message, history, has_tools);
    } else {
        // OpenAI-compatible
        req_body = build_openai_request(user_message, history, false);
    }

    LOG_DEBUG("LLM", "POST %s provider=%s model=%s", url.c_str(), p.c_str(), config_.model.c_str());

    auto http_resp = http::HttpClient::instance().post_json(url, req_body, headers);

    if (!http_resp.success) {
        resp.error = http_resp.error.empty() ?
            ("HTTP " + std::to_string(http_resp.status_code)) : http_resp.error;
        // Try to extract error message from response body
        if (!http_resp.body.empty()) {
            try {
                auto err_json = json::parse(http_resp.body);
                if (err_json.contains("error")) {
                    auto& e = err_json["error"];
                    if (e.is_string()) resp.error = e.get<std::string>();
                    else if (e.contains("message")) resp.error = e["message"].get<std::string>();
                }
            } catch (...) {}
        }
        return resp;
    }

    try {
        auto rj = json::parse(http_resp.body);

        // Extract content based on provider
        if (p == "anthropic") {
            if (rj.contains("content") && rj["content"].is_array() && !rj["content"].empty()) {
                resp.content = rj["content"][0]["text"].get<std::string>();
            }
        } else if (p == "gemini" || p == "google") {
            if (rj.contains("candidates") && !rj["candidates"].empty()) {
                auto& parts = rj["candidates"][0]["content"]["parts"];
                if (!parts.empty()) {
                    resp.content = parts[0]["text"].get<std::string>();
                }
            }
        } else if (p == "fincept") {
            // Fincept response: { success, data: { response, tool_calls?, usage? } }
            auto response_data = rj.value("data", rj); // data may be nested or flat

            // Check for tool calls first
            if (response_data.contains("tool_calls") &&
                response_data["tool_calls"].is_array() &&
                !response_data["tool_calls"].empty()) {

                LOG_INFO("LLM", "Fincept returned %zu tool calls", response_data["tool_calls"].size());

                std::string tool_results_text;
                for (auto& tc : response_data["tool_calls"]) {
                    std::string fn_name = tc.contains("function")
                        ? tc["function"].value("name", "") : tc.value("name", "");
                    std::string args_str = tc.contains("function")
                        ? tc["function"].value("arguments", "{}") : tc.value("arguments", "{}");

                    json fn_args;
                    try { fn_args = json::parse(args_str); }
                    catch (...) { fn_args = json::object(); }

                    LOG_INFO("LLM", "Executing Fincept tool: %s", fn_name.c_str());
                    auto tool_result = mcp::MCPService::instance().execute_openai_function(fn_name, fn_args);

                    // Extract readable content from tool result
                    auto tr_json = tool_result.to_json();
                    std::string result_content;
                    if (!tool_result.message.empty()) result_content = tool_result.message;
                    else if (!tool_result.data.is_null()) result_content = tool_result.data.dump(2).substr(0, 2000);
                    else result_content = tr_json.dump(2).substr(0, 2000);

                    // Extract just the tool name part (after __)
                    auto sep = fn_name.find("__");
                    std::string short_name = (sep != std::string::npos) ? fn_name.substr(sep + 2) : fn_name;
                    tool_results_text += "\n**Tool: " + short_name + "**\n" + result_content + "\n";
                }

                // Send tool results back to Fincept for final formatting
                if (!tool_results_text.empty()) {
                    std::string follow_prompt = "User asked: \"" + user_message +
                        "\"\n\nI retrieved this data:\n" + tool_results_text +
                        "\n\nPlease provide a clear, concise summary of this data in natural language.";

                    json follow_body = {
                        {"prompt", follow_prompt},
                        {"temperature", global_settings_.temperature},
                        {"max_tokens", global_settings_.max_tokens}
                    };

                    auto fu_resp = http::HttpClient::instance().post_json(url, follow_body, headers);
                    if (fu_resp.success) {
                        try {
                            auto fu_json = json::parse(fu_resp.body);
                            auto fu_data = fu_json.value("data", fu_json);
                            resp.content = fu_data.value("response",
                                fu_data.value("content",
                                    fu_data.value("answer",
                                        fu_data.value("text", std::string("")))));
                        } catch (...) {}
                    }
                }
            } else {
                // No tool calls — extract text content
                resp.content = response_data.value("response",
                    response_data.value("content",
                        response_data.value("answer",
                            response_data.value("text",
                                response_data.value("result", std::string(""))))));

                // Also check nested ai_response (legacy format)
                if (resp.content.empty() && response_data.contains("ai_response")) {
                    auto& ai = response_data["ai_response"];
                    if (ai.contains("content")) resp.content = ai["content"].get<std::string>();
                    else if (ai.is_string()) resp.content = ai.get<std::string>();
                }
            }
        } else {
            // OpenAI-compatible — check for tool calls
            if (rj.contains("choices") && !rj["choices"].empty()) {
                auto& choice = rj["choices"][0];
                auto& msg = choice["message"];

                // Check if LLM wants to call tools
                if (msg.contains("tool_calls") && msg["tool_calls"].is_array() && !msg["tool_calls"].empty()) {
                    LOG_INFO("LLM", "LLM requested %zu tool calls", msg["tool_calls"].size());

                    // Build messages array for the tool call loop
                    json loop_messages = json::array();

                    // Add system prompt
                    if (!global_settings_.system_prompt.empty()) {
                        loop_messages.push_back({{"role", "system"}, {"content", global_settings_.system_prompt}});
                    }
                    // Add history
                    for (auto& h : history) {
                        loop_messages.push_back({{"role", h.role}, {"content", h.content}});
                    }
                    // Add current user message
                    loop_messages.push_back({{"role", "user"}, {"content", user_message}});
                    // Add assistant message with tool_calls
                    loop_messages.push_back(msg);

                    // Execute each tool call and add results
                    for (auto& tc : msg["tool_calls"]) {
                        std::string call_id = tc.value("id", "");
                        std::string fn_name = tc["function"].value("name", "");
                        json fn_args;
                        try {
                            fn_args = json::parse(tc["function"].value("arguments", "{}"));
                        } catch (...) {
                            fn_args = json::object();
                        }

                        LOG_INFO("LLM", "Executing tool: %s", fn_name.c_str());

                        auto tool_result = mcp::MCPService::instance().execute_openai_function(fn_name, fn_args);
                        std::string result_str = tool_result.to_json().dump();

                        loop_messages.push_back({
                            {"role", "tool"},
                            {"tool_call_id", call_id},
                            {"content", result_str}
                        });
                    }

                    // Send follow-up request with tool results (max 5 rounds)
                    static constexpr int MAX_TOOL_ROUNDS = 5;
                    for (int round = 0; round < MAX_TOOL_ROUNDS; ++round) {
                        json follow_up;
                        follow_up["model"] = config_.model;
                        follow_up["messages"] = loop_messages;
                        follow_up["temperature"] = global_settings_.temperature;
                        follow_up["max_tokens"] = global_settings_.max_tokens;

                        auto tools_json = mcp::MCPService::instance().format_tools_for_openai();
                        if (!tools_json.empty()) follow_up["tools"] = tools_json;

                        auto fu_resp = http::HttpClient::instance().post_json(url, follow_up, headers);
                        if (!fu_resp.success) {
                            resp.error = "Tool follow-up request failed";
                            return resp;
                        }

                        auto fu_json = json::parse(fu_resp.body);
                        if (fu_json.contains("choices") && !fu_json["choices"].empty()) {
                            auto& fu_choice = fu_json["choices"][0];
                            auto& fu_msg = fu_choice["message"];

                            if (fu_msg.contains("tool_calls") && fu_msg["tool_calls"].is_array() && !fu_msg["tool_calls"].empty()) {
                                // Another round of tool calls
                                loop_messages.push_back(fu_msg);
                                for (auto& tc2 : fu_msg["tool_calls"]) {
                                    std::string cid = tc2.value("id", "");
                                    std::string fname = tc2["function"].value("name", "");
                                    json fargs;
                                    try { fargs = json::parse(tc2["function"].value("arguments", "{}")); }
                                    catch (...) { fargs = json::object(); }

                                    auto tr = mcp::MCPService::instance().execute_openai_function(fname, fargs);
                                    loop_messages.push_back({
                                        {"role", "tool"},
                                        {"tool_call_id", cid},
                                        {"content", tr.to_json().dump()}
                                    });
                                }
                                continue; // Next round
                            }

                            // Final text response
                            if (fu_msg.contains("content") && fu_msg["content"].is_string()) {
                                resp.content = fu_msg["content"].get<std::string>();
                            }
                            parse_usage(resp, fu_json, p);
                        }
                        break; // Done — got text response
                    }
                } else {
                    // Normal text response (no tool calls)
                    if (msg.contains("content") && !msg["content"].is_null()) {
                        resp.content = msg["content"].get<std::string>();
                    }
                }
            }
        }

        parse_usage(resp, rj, p);
        resp.success = true;

    } catch (const std::exception& ex) {
        resp.error = std::string("Parse error: ") + ex.what();
    }

    return resp;
}

// ============================================================================
// Streaming via libcurl (SSE)
// ============================================================================
struct StreamContext {
    StreamCallback callback;
    std::string provider;
    std::string buffer;        // partial line buffer
    std::string accumulated;   // full accumulated content
    int tokens = 0;
};

static size_t stream_write_callback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* ctx = static_cast<StreamContext*>(userdata);
    size_t total = size * nmemb;
    ctx->buffer.append(ptr, total);

    // Process complete lines
    size_t pos;
    while ((pos = ctx->buffer.find('\n')) != std::string::npos) {
        std::string line = ctx->buffer.substr(0, pos);
        ctx->buffer.erase(0, pos + 1);

        // Remove \r
        if (!line.empty() && line.back() == '\r') line.pop_back();

        // Skip empty lines
        if (line.empty()) continue;

        // SSE format: "data: {...}"
        if (line.rfind("data: ", 0) != 0) continue;
        std::string data = line.substr(6);

        if (data == "[DONE]") {
            ctx->callback("", true);
            continue;
        }

        std::string chunk = LLMService::parse_sse_chunk(data, ctx->provider);
        if (!chunk.empty()) {
            ctx->accumulated += chunk;
            ctx->tokens++;
            ctx->callback(chunk, false);
        }
    }

    return total;
}

LLMResponse LLMService::do_streaming_request(const std::string& user_message,
                                               const std::vector<ConversationMessage>& history,
                                               StreamCallback on_chunk) {
    LLMResponse resp;
    const auto& p = config_.provider;

    // Gemini and Fincept don't support SSE streaming, fall back to non-streaming
    if (p == "gemini" || p == "google" || p == "fincept") {
        resp = do_request(user_message, history);
        if (resp.success && !resp.content.empty()) {
            on_chunk(resp.content, false);
        }
        on_chunk("", true);
        return resp;
    }

    std::string url = get_endpoint_url();
    if (url.empty()) {
        resp.error = "No endpoint URL for provider: " + p;
        on_chunk("", true);
        return resp;
    }

    auto headers = get_headers();
    json req_body;

    if (p == "anthropic") {
        req_body = build_anthropic_request(user_message, history, true);
    } else {
        req_body = build_openai_request(user_message, history, true);
    }

    std::string body_str = req_body.dump();

    // Use raw libcurl for streaming
    CURL* curl = curl_easy_init();
    if (!curl) {
        resp.error = "Failed to initialize CURL";
        on_chunk("", true);
        return resp;
    }

    StreamContext ctx;
    ctx.callback = on_chunk;
    ctx.provider = p;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body_str.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)body_str.size());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, stream_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ctx);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 120L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT,
        "Mozilla/5.0 FinceptTerminal/4.0.0");

    // Build header list
    struct curl_slist* header_list = nullptr;
    header_list = curl_slist_append(header_list, "Content-Type: application/json");
    header_list = curl_slist_append(header_list, "Accept: text/event-stream");
    for (const auto& [key, value] : headers) {
        std::string h = key + ": " + value;
        header_list = curl_slist_append(header_list, h.c_str());
    }
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);

    LOG_DEBUG("LLM", "STREAM POST %s provider=%s", url.c_str(), p.c_str());

    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        resp.error = curl_easy_strerror(res);
        LOG_ERROR("LLM", "Stream request failed: %s", resp.error.c_str());
    } else {
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        if (http_code >= 200 && http_code < 300) {
            resp.content = ctx.accumulated;
            resp.completion_tokens = ctx.tokens;
            resp.success = true;
        } else {
            resp.error = "HTTP " + std::to_string(http_code);
            // If we got error text instead of stream
            if (!ctx.accumulated.empty()) {
                try {
                    auto err = json::parse(ctx.buffer.empty() ? ctx.accumulated : ctx.buffer);
                    if (err.contains("error")) {
                        auto& e = err["error"];
                        if (e.is_string()) resp.error = e.get<std::string>();
                        else if (e.contains("message")) resp.error = e["message"].get<std::string>();
                    }
                } catch (...) {}
            }
        }
    }

    // Ensure done callback fires
    on_chunk("", true);

    curl_slist_free_all(header_list);
    curl_easy_cleanup(curl);

    return resp;
}

// ============================================================================
// SSE chunk parsing
// ============================================================================
std::string LLMService::parse_sse_chunk(const std::string& data, const std::string& provider) {
    try {
        auto j = json::parse(data);

        if (provider == "anthropic") {
            // Anthropic: {"type":"content_block_delta","delta":{"type":"text_delta","text":"..."}}
            if (j.value("type", "") == "content_block_delta") {
                if (j.contains("delta") && j["delta"].contains("text")) {
                    return j["delta"]["text"].get<std::string>();
                }
            }
            return "";
        }

        // OpenAI-compatible: {"choices":[{"delta":{"content":"..."}}]}
        if (j.contains("choices") && !j["choices"].empty()) {
            auto& delta = j["choices"][0]["delta"];
            if (delta.contains("content") && !delta["content"].is_null()) {
                return delta["content"].get<std::string>();
            }
        }
    } catch (...) {
        // Malformed chunk, skip
    }
    return "";
}

void LLMService::parse_usage(LLMResponse& resp, const json& rj, const std::string& provider) {
    try {
        if (provider == "anthropic") {
            if (rj.contains("usage")) {
                resp.prompt_tokens = rj["usage"].value("input_tokens", 0);
                resp.completion_tokens = rj["usage"].value("output_tokens", 0);
                resp.total_tokens = resp.prompt_tokens + resp.completion_tokens;
            }
        } else if (rj.contains("usage")) {
            resp.prompt_tokens = rj["usage"].value("prompt_tokens", 0);
            resp.completion_tokens = rj["usage"].value("completion_tokens", 0);
            resp.total_tokens = rj["usage"].value("total_tokens", 0);
        }
    } catch (...) {}
}

// ============================================================================
// Public API
// ============================================================================
LLMResponse LLMService::chat(const std::string& user_message,
                              const std::vector<ConversationMessage>& history) {
    std::lock_guard<std::mutex> lock(mutex_);
    ensure_config();

    if (config_.provider.empty()) {
        return LLMResponse{.error = "No LLM provider configured"};
    }

    return do_request(user_message, history);
}

std::future<LLMResponse> LLMService::chat_streaming(
    const std::string& user_message,
    const std::vector<ConversationMessage>& history,
    StreamCallback on_chunk) {

    // Copy config under lock
    db::LLMConfig cfg_copy;
    db::LLMGlobalSettings gs_copy;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ensure_config();
        cfg_copy = config_;
        gs_copy = global_settings_;
    }

    return std::async(std::launch::async, [this, user_message, history, on_chunk,
                                            cfg_copy, gs_copy]() -> LLMResponse {
        // Use copied config (don't hold lock during network call)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            config_ = cfg_copy;
            global_settings_ = gs_copy;
        }

        if (config_.provider.empty()) {
            on_chunk("", true);
            return LLMResponse{.error = "No LLM provider configured"};
        }

        return do_streaming_request(user_message, history, on_chunk);
    });
}

} // namespace fincept::ai_chat
