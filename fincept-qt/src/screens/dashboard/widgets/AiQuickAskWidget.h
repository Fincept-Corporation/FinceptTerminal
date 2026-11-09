#pragma once
// AiQuickAskWidget — one-line question box that asks the configured LLM
// (LlmService::chat_streaming, plain chat: ToolPolicy::None — no MCP tools, no
// UI control) and streams the answer into the tile.
//
// Each question runs under its own chat_session_id ("dashboard-quick-ask-<uuid>")
// so this tile can tell its reply apart on the process-wide finished_streaming()
// signal (the AI Chat tab and the floating Quick Chat bubble ignore it) and can
// cancel it: a new question or destroying the tile cancels the one in flight.

#include "screens/dashboard/widgets/BaseWidget.h"
#include "services/llm/LlmService.h"

#include <QString>

class QLineEdit;
class QTextBrowser;

namespace fincept::screens::widgets {

class AiQuickAskWidget : public BaseWidget {
    Q_OBJECT
  public:
    explicit AiQuickAskWidget(const QJsonObject& cfg = {}, QWidget* parent = nullptr);
    ~AiQuickAskWidget() override;

  protected:
    void showEvent(QShowEvent* e) override;
    void on_theme_changed() override;
    void retranslateUi() override;

  private:
    enum class AskState { Idle, NotConfigured, Thinking, Answered, Failed, Cancelled };

    void apply_styles();
    void on_ask();
    void on_chunk(quint64 gen, const QString& chunk);
    void on_finished(const fincept::ai_chat::LlmResponse& response);
    void cancel_in_flight();
    void refresh_config_state();
    void set_state(AskState state);
    void update_status_text();
    void render_not_configured();
    void render_failure();
    void render_stream();

    QWidget* root_ = nullptr;
    QLineEdit* input_ = nullptr;
    QPushButton* ask_btn_ = nullptr;
    QTextBrowser* answer_ = nullptr;
    QLabel* status_label_ = nullptr;
    QPushButton* settings_btn_ = nullptr;
    QPushButton* continue_btn_ = nullptr;

    AskState state_ = AskState::Idle;
    QString pending_session_;    ///< chat_session_id of the request in flight (empty = idle)
    quint64 request_gen_ = 0;    ///< bumps per question; stale chunks are dropped
    QString stream_buffer_;      ///< answer text streamed so far
    qsizetype rendered_len_ = 0; ///< how much of stream_buffer_ is already in answer_
    QString req_provider_;       ///< provider / model the current answer came from
    QString req_model_;
    int last_tokens_ = 0;
    QString last_error_; ///< raw LlmResponse::error of the last failure (shown, never logged)
};

} // namespace fincept::screens::widgets
