#pragma once
// LLM Service — Multi-provider LLM API client
// Supports OpenAI-compatible APIs (OpenAI, Groq, DeepSeek, OpenRouter, Ollama),
// Anthropic, Google Gemini, and Fincept's own endpoint.
// Streaming via chunked transfer + SSE parsing.

#include <string>
#include <vector>
#include <functional>
#include <future>
#include <mutex>
#include <optional>
#include <nlohmann/json.hpp>
#include "storage/database.h"

namespace fincept::ai_chat {

using json = nlohmann::json;

// Providers that support streaming
inline bool provider_supports_streaming(const std::string& provider) {
    return provider == "openai" || provider == "anthropic" ||
           provider == "gemini" || provider == "google" ||
           provider == "groq" || provider == "deepseek" ||
           provider == "openrouter" || provider == "ollama" ||
           provider == "fincept";
}

// Providers that don't require API key
inline bool provider_requires_api_key(const std::string& provider) {
    return provider != "ollama" && provider != "fincept";
}

// Conversation message (role + content)
struct ConversationMessage {
    std::string role;    // "system", "user", "assistant"
    std::string content;
};

// LLM response
struct LLMResponse {
    std::string content;
    std::string error;
    int prompt_tokens = 0;
    int completion_tokens = 0;
    int total_tokens = 0;
    bool success = false;
};

// Streaming callback: (chunk_text, is_done)
using StreamCallback = std::function<void(const std::string&, bool)>;

class LLMService {
public:
    static LLMService& instance();

    // Send a message with conversation history, returns full response
    LLMResponse chat(const std::string& user_message,
                     const std::vector<ConversationMessage>& history);

    // Send a message with streaming callback (called on background thread)
    // Returns future that resolves to final response
    std::future<LLMResponse> chat_streaming(
        const std::string& user_message,
        const std::vector<ConversationMessage>& history,
        StreamCallback on_chunk);

    // Reload config from database
    void reload_config();

    // Get active config info
    std::string active_provider() const;
    std::string active_model() const;
    bool is_configured() const;

    LLMService(const LLMService&) = delete;
    LLMService& operator=(const LLMService&) = delete;

private:
    LLMService() = default;

    mutable std::mutex mutex_;
    db::LLMConfig config_;
    db::LLMGlobalSettings global_settings_;
    bool config_loaded_ = false;

    void ensure_config();

    // Provider-specific request builders
    json build_openai_request(const std::string& user_message,
                              const std::vector<ConversationMessage>& history,
                              bool stream);
    json build_anthropic_request(const std::string& user_message,
                                 const std::vector<ConversationMessage>& history,
                                 bool stream);
    json build_gemini_request(const std::string& user_message,
                              const std::vector<ConversationMessage>& history);
    json build_fincept_request(const std::string& user_message,
                               const std::vector<ConversationMessage>& history,
                               bool with_tools);

    // Get the API endpoint URL for the active provider
    std::string get_endpoint_url() const;

    // Get auth headers for the active provider
    std::map<std::string, std::string> get_headers() const;

    // Non-streaming request
    LLMResponse do_request(const std::string& user_message,
                           const std::vector<ConversationMessage>& history);

    // Streaming request (blocks until done, calls callback)
    LLMResponse do_streaming_request(const std::string& user_message,
                                     const std::vector<ConversationMessage>& history,
                                     StreamCallback on_chunk);

public:
    // Parse SSE stream data (public for stream_write_callback access)
    static std::string parse_sse_chunk(const std::string& line, const std::string& provider);

private:
    // Parse final response for token usage
    static void parse_usage(LLMResponse& resp, const json& response_json, const std::string& provider);
};

} // namespace fincept::ai_chat
