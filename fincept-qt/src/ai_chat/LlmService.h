#pragma once
// LlmService.h — Multi-provider LLM API client (Qt port)
// Supports OpenAI-compatible APIs (OpenAI, Groq, DeepSeek, OpenRouter, Ollama),
// Anthropic, Google Gemini, and Fincept's own endpoint.
// Streaming via QNetworkReply::readyRead + SSE parsing.

#include "core/result/Result.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QMutex>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>
#include <QStringList>

#include <functional>
#include <optional>
#include <vector>

namespace fincept::ai_chat {

// ── Provider helpers ──────────────────────────────────────────────────────────

inline bool provider_supports_streaming(const QString& provider) {
    return provider == "openai" || provider == "anthropic" || provider == "gemini" || provider == "google" ||
           provider == "groq" || provider == "deepseek" || provider == "openrouter" || provider == "ollama" ||
           provider == "fincept";
}

inline bool provider_requires_api_key(const QString& provider) {
    return provider != "ollama" && provider != "fincept";
}

// ── Data types ────────────────────────────────────────────────────────────────

struct ConversationMessage {
    QString role; // "system", "user", "assistant"
    QString content;
};

struct LlmResponse {
    QString content;
    QString error;
    int prompt_tokens = 0;
    int completion_tokens = 0;
    int total_tokens = 0;
    bool success = false;
};

// chunk_text, is_done
using StreamCallback = std::function<void(const QString&, bool)>;

// ── LlmService ────────────────────────────────────────────────────────────────

class LlmService : public QObject {
    Q_OBJECT
  public:
    static LlmService& instance();

    // Non-streaming (blocking — call from background thread via QtConcurrent::run)
    LlmResponse chat(const QString& user_message, const std::vector<ConversationMessage>& history);

    // Streaming — launches background thread; on_chunk called on that thread.
    // Emit finished_streaming(response) when done to get result on UI thread.
    void chat_streaming(const QString& user_message, const std::vector<ConversationMessage>& history,
                        StreamCallback on_chunk);

    // Reload config from DB (call after user changes LLM settings)
    void reload_config();

    QString active_provider() const;
    QString active_model() const;
    bool is_configured() const;

    // Fetch available models for a provider (async — emits models_fetched)
    void fetch_models(const QString& provider, const QString& api_key, const QString& base_url = {});

    LlmService(const LlmService&) = delete;
    LlmService& operator=(const LlmService&) = delete;

  signals:
    void finished_streaming(LlmResponse response);
    void config_changed(); // emitted after reload_config() — UI can react
    void models_fetched(const QString& provider, const QStringList& models, const QString& error);

  private:
    LlmService();

    mutable QMutex mutex_;

    // Active config (reloaded lazily)
    QString provider_;
    QString api_key_;
    QString base_url_;
    QString model_;
    double temperature_ = 0.7;
    int max_tokens_ = 4096;
    QString system_prompt_;
    bool config_loaded_ = false;

    void ensure_config();

    // Request builders → QJsonObject
    QJsonObject build_openai_request(const QString& user_message, const std::vector<ConversationMessage>& history,
                                     bool stream, bool with_tools = true);
    QJsonObject build_anthropic_request(const QString& user_message, const std::vector<ConversationMessage>& history,
                                        bool stream);
    QJsonObject build_gemini_request(const QString& user_message, const std::vector<ConversationMessage>& history);
    QJsonObject build_fincept_request(const QString& user_message, const std::vector<ConversationMessage>& history,
                                      bool with_tools);

    QString get_endpoint_url() const;
    QMap<QString, QString> get_headers() const;

    // Synchronous HTTP helpers (use QNetworkAccessManager + QEventLoop)
    // Must be called from a background thread.
    LlmResponse do_request(const QString& user_message, const std::vector<ConversationMessage>& history);
    LlmResponse do_streaming_request(const QString& user_message, const std::vector<ConversationMessage>& history,
                                     StreamCallback on_chunk);

    // Tool-call follow-up loop (OpenAI-compatible)
    LlmResponse do_tool_loop(QJsonArray loop_messages, const QString& url, const QMap<QString, QString>& headers);

    // Detect and execute tool calls embedded as text/XML in the response content.
    // Returns std::nullopt if no text-based tool calls were found.
    std::optional<LlmResponse> try_extract_and_execute_text_tool_calls(const QString& content,
                                                                       const QString& user_message,
                                                                       const QString& url,
                                                                       const QMap<QString, QString>& headers);

    // Models-list helpers
    static QString get_models_url(const QString& provider, const QString& api_key, const QString& base_url);
    static QMap<QString, QString> get_models_headers(const QString& provider, const QString& api_key);
    static QStringList parse_models_response(const QString& provider, const QByteArray& body);

    // Parse SSE data line → extracted text chunk
    static QString parse_sse_chunk(const QString& data, const QString& provider);

    // Parse token usage from response JSON
    static void parse_usage(LlmResponse& resp, const QJsonObject& rj, const QString& provider);

    // Synchronous POST helper (blocks calling thread via QEventLoop)
    struct HttpResult {
        bool success = false;
        int status = 0;
        QByteArray body;
        QString error;
    };
    static HttpResult blocking_post(const QString& url, const QJsonObject& body, const QMap<QString, QString>& headers,
                                    int timeout_ms = 120000);
};

} // namespace fincept::ai_chat
