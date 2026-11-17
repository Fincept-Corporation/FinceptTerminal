#pragma once
// ReportAiLlmBackend — the production ReportAiBackend: Report Builder drafting through the
// terminal's model router (ModelRouter feature kReportDraftFeature) and LlmService.
//
//   start()  LlmService::chat_streaming(…, ToolPolicy::None, "report_draft:<id>",
//            ChatRoute{"report_draft"}). The instruction travels at the top of the user turn:
//            LlmService passes "system" history entries on only to OpenAI-compatible
//            providers (the Anthropic, Gemini and Fincept request builders drop them, and
//            the global system prompt cannot be replaced per request), so the user turn is
//            the one place every provider and harness reads. History is empty.
//            Chunks arrive on an LlmService worker thread and are handed to the GUI thread
//            (queued on qApp) before the session sees them; the result comes from the
//            process-wide finished_streaming(), filtered by origin_session_id.
//   cancel() LlmService::cancel_active_request("report_draft:<id>") — only this request.
//   model_info()  ModelRouter's first choice for the feature, its billing note (Fincept
//            LLM: "uses your Fincept credits"), and needs-setup when Fincept LLM would answer
//            but the user is not signed in.
// Kept out of ReportAiSession.cpp so the session's tests do not link LlmService.
// GUI thread only.

#include "services/report_builder/ReportAiSession.h"

#include <QHash>
#include <QObject>
#include <QString>

#include <functional>

namespace fincept::ai_chat {
struct LlmResponse;
}

namespace fincept::services {

class ReportAiLlmBackend : public QObject, public ReportAiBackend {
  public:
    explicit ReportAiLlmBackend(QObject* parent = nullptr);
    ~ReportAiLlmBackend() override;

    void start(quint64 id, const QString& system, const QString& user,
               std::function<void(quint64, const QString& chunk)> on_chunk,
               std::function<void(quint64, const AiBackendResult&)> on_done) override;
    void cancel(quint64 id) override;
    AiModelInfo model_info() const override;

    /// "report_draft:<id>" — the LlmService chat session id of request `id`.
    static QString session_id_for(quint64 id);

  private:
    struct Pending {
        quint64 id = 0;
        std::function<void(quint64, const QString&)> on_chunk;
        std::function<void(quint64, const AiBackendResult&)> on_done;
    };
    void deliver_chunk(const QString& session_id, const QString& chunk);
    void on_finished(const ai_chat::LlmResponse& response);

    QHash<QString, Pending> pending_; // LlmService session id -> request
};

} // namespace fincept::services
