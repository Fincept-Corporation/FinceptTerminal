#pragma once
// ArenaLlmClient — stateless native multi-provider completion client (spec §3.3).
// One complete() call = one decision request. Parallel-safe: each call owns its
// QNetworkReply. Response parsing is a pure static for selftest coverage.
#include <QNetworkAccessManager>
#include <QObject>
#include <functional>

namespace fincept::arena {

struct ArenaLlmRequest {
    QString provider, model_id, api_key, base_url, system_prompt, user_prompt;
    int max_tokens = 2048;
    int timeout_ms = 60000;
};
struct ArenaLlmResult {
    QString content, error;
    int prompt_tokens = 0, completion_tokens = 0;
    qint64 latency_ms = 0;
    bool success = false;
};

class IArenaLlmClient : public QObject {
    Q_OBJECT
  public:
    using QObject::QObject;
    virtual void complete(const ArenaLlmRequest& req, std::function<void(ArenaLlmResult)> cb) = 0;
};

class ArenaLlmClient : public IArenaLlmClient {
    Q_OBJECT
  public:
    explicit ArenaLlmClient(QObject* parent = nullptr);
    void complete(const ArenaLlmRequest& req, std::function<void(ArenaLlmResult)> cb) override;

    // Pure helpers (selftest-covered):
    static QByteArray build_body(const ArenaLlmRequest& req);
    static ArenaLlmResult parse_response(const QString& provider, const QByteArray& body);

  private:
    QNetworkAccessManager* nam_ = nullptr;   // GUI-thread QNAM (like LlmService::models_nam_)
};

} // namespace fincept::arena
