#pragma once
// Multi-provider LLM client (OpenAI-compatible, Anthropic, Gemini, Fincept). Streams via SSE.

#include "core/result/Result.h"
#include "mcp/McpTypes.h"
#include "storage/repositories/LlmProfileRepository.h"

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

inline bool provider_supports_streaming(const QString& provider) {
    return provider == "openai" || provider == "anthropic" || provider == "gemini" || provider == "google" ||
           provider == "groq" || provider == "deepseek" || provider == "openrouter" || provider == "minimax" ||
           provider == "kimi" || provider == "ollama" || provider == "xai" || provider == "fincept";
}

inline bool provider_requires_api_key(const QString& provider) {
    return provider != "ollama" && provider != "fincept";
}

struct ConversationMessage {
    QString role; // system | user | assistant
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

/// (chunk_text, is_done) — invoked on a background thread.
using StreamCallback = std::function<void(const QString&, bool)>;

class LlmService : public QObject {
    Q_OBJECT
  public:
    static LlmService& instance();

    /// Blocking — call from a background thread (QtConcurrent::run).
    LlmResponse chat(const QString& user_message, const std::vector<ConversationMessage>& history,
                     bool use_tools = true);

    /// Per-request tool scope. NoNavigation lets the floating bubble keep benign tools
    /// without yanking the user away. None drops tools entirely.
    enum class ToolPolicy { All, NoNavigation, None };

    /// Spawns a background thread. on_chunk runs on that thread; finished_streaming() lands on UI thread.
    void chat_streaming(const QString& user_message, const std::vector<ConversationMessage>& history,
                        StreamCallback on_chunk, ToolPolicy policy = ToolPolicy::All);

    /// Back-compat: false→None, true→All. Prefer the enum overload.
    void chat_streaming(const QString& user_message, const std::vector<ConversationMessage>& history,
                        StreamCallback on_chunk, bool use_tools);

    /// Call after the user changes LLM settings.
    void reload_config();

    QString active_provider() const;
    QString active_model() const;
    QString active_api_key() const;
    QString active_base_url() const;
    double active_temperature() const;
    int active_max_tokens() const;
    bool tools_enabled() const;
    bool is_configured() const;

    /// context_type ∈ {ai_chat, agent, agent_default, team, team_default, team_coordinator}.
    /// context_id is the agent/team id; empty for type-level queries.
    ResolvedLlmProfile resolve_profile(const QString& context_type, const QString& context_id = {}) const;

    /// JSON shape for embedding in AgentService payloads.
    static QJsonObject profile_to_json(const ResolvedLlmProfile& p);

    /// Async — emits models_fetched.
    void fetch_models(const QString& provider, const QString& api_key, const QString& base_url = {});

    LlmService(const LlmService&) = delete;
    LlmService& operator=(const LlmService&) = delete;

  signals:
    void finished_streaming(LlmResponse response);
    void config_changed();
    void models_fetched(const QString& provider, const QStringList& models, const QString& error);

  private:
    LlmService();

    mutable QMutex mutex_;

    // Lazily reloaded; mutable so const accessors can call ensure_config().
    mutable QString provider_;
    mutable QString api_key_;
    mutable QString base_url_;
    mutable QString model_;
    mutable double temperature_ = 0.7;
    mutable int max_tokens_ = 4096;
    mutable QString system_prompt_;
    mutable bool tools_enabled_ = true;
    mutable bool config_loaded_ = false;

    // Empty filter = full catalogue. Read in build_*_request under mutex_.
    mcp::ToolFilter tool_filter_;

    // Lives on main thread — fetch_models() runs the reply on the GUI event loop.
    // Don't move to a worker thread: QNAM's DNS/TLS resolvers expect a Qt-managed thread.
    QNetworkAccessManager* models_nam_ = nullptr;

    void ensure_config() const;

    QJsonObject build_openai_request(const QString& user_message, const std::vector<ConversationMessage>& history,
                                     bool stream, bool with_tools = true);
    QJsonObject build_anthropic_request(const QString& user_message, const std::vector<ConversationMessage>& history,
                                        bool stream);
    QJsonObject build_gemini_request(const QString& user_message, const std::vector<ConversationMessage>& history);
    QJsonObject build_fincept_request(const QString& user_message, const std::vector<ConversationMessage>& history,
                                      bool with_tools);

    QString get_endpoint_url() const;
    QMap<QString, QString> get_headers() const;

    /// Resolution order: user max_tokens (clamped to model cap) → model cap → 8192. Called with mutex_ held.
    int resolved_max_tokens() const;

    // Sync HTTP helpers (QNAM + QEventLoop) — call from background thread only.
    LlmResponse do_request(const QString& user_message, const std::vector<ConversationMessage>& history);
    LlmResponse do_streaming_request(const QString& user_message, const std::vector<ConversationMessage>& history,
                                     StreamCallback on_chunk);

    LlmResponse do_tool_loop(QJsonArray loop_messages, const QString& url, const QMap<QString, QString>& headers);

    /// Returns nullopt if the content had no text/XML tool calls.
    std::optional<LlmResponse> try_extract_and_execute_text_tool_calls(const QString& content,
                                                                       const QString& user_message, const QString& url,
                                                                       const QMap<QString, QString>& headers);

    static QString get_models_url(const QString& provider, const QString& api_key, const QString& base_url);
    static QMap<QString, QString> get_models_headers(const QString& provider, const QString& api_key);
    static QStringList parse_models_response(const QString& provider, const QByteArray& body);

    static QString parse_sse_chunk(const QString& data, const QString& provider);

    static void parse_usage(LlmResponse& resp, const QJsonObject& rj, const QString& provider);

    /// Blocks the calling thread via QEventLoop.
    struct HttpResult {
        bool success = false;
        int status = 0;
        QByteArray body;
        QString error;
    };
    static HttpResult blocking_post(const QString& url, const QJsonObject& body, const QMap<QString, QString>& headers,
                                    int timeout_ms = 120000);
    static HttpResult blocking_get(const QString& url, const QMap<QString, QString>& headers, int timeout_ms = 30000);

    /// QEventLoop-based path used for Cloudflare-protected Fincept endpoints.
    static HttpResult eventloop_request(const QString& method, const QString& url, const QByteArray& body,
                                        const QMap<QString, QString>& headers, int timeout_ms = 30000);

    /// POST /research/llm/async then poll /research/llm/status/{id}.
    LlmResponse fincept_async_request(const QString& user_message, const std::vector<ConversationMessage>& history);
};

} // namespace fincept::ai_chat
