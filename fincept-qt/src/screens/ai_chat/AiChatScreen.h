#pragma once

#include "core/events/EventBus.h"
#include "core/symbol/IGroupLinked.h"
#include "core/symbol/SymbolGroup.h"
#include "core/symbol/SymbolRef.h"
#include "screens/common/IStatefulScreen.h"
#include "services/llm/LlmService.h"

#include <QHash>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QScrollArea>
#include <QStringList>
#include <QTimer>
#include <QVBoxLayout>
#include <QVector>
#include <QWidget>

#include <vector>

namespace fincept::screens {

class AiChatScreen : public QWidget, public IStatefulScreen, public fincept::IGroupLinked {
    Q_OBJECT
    Q_INTERFACES(fincept::IGroupLinked)
  public:
    explicit AiChatScreen(QWidget* parent = nullptr);

    void restore_state(const QVariantMap& state) override;
    QVariantMap save_state() const override;
    QString state_key() const override { return "ai_chat"; }
    // v2 adds: draft text, search text, scroll position, attached file path.
    // v3 adds: sidebar_collapsed.
    int state_version() const override { return 3; }

    // IGroupLinked — Phase 7: AI Chat is a consumer-only participant. When a
    // panel in the same group selects a symbol, the chat input gets a
    // gentle "Context: <SYMBOL>" prefix appended (visible to the user) so
    // their next prompt has the security in scope. Chat itself never
    // publishes — the symbol leaving here would not match any meaningful
    // user intent (the user typed a question, not a ticker selection).
    void set_group(fincept::SymbolGroup g) override;
    fincept::SymbolGroup group() const override { return link_group_; }
    void on_group_symbol_changed(const fincept::SymbolRef& ref) override;
    fincept::SymbolRef current_symbol() const override { return linked_symbol_; }

  protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    bool eventFilter(QObject* obj, QEvent* event) override;
    void changeEvent(QEvent* event) override;

  private slots:
    void on_send();
    void on_attach_file();
    void on_new_session();
    void on_session_selected(int row);
    void on_delete_session();
    void on_rename_session();
    void on_stream_chunk(const QString& chunk, bool done);
    void on_streaming_done(ai_chat::LlmResponse response);
    void on_provider_changed();
    void on_search_changed(const QString& text);
    void on_typing_indicator_tick();
    void on_toggle_sidebar();

  private:
    // ── Sidebar ──────────────────────────────────────────────────────────
    QWidget* sidebar_ = nullptr;
    QLineEdit* search_edit_ = nullptr;
    QListWidget* session_list_ = nullptr;
    QPushButton* new_btn_ = nullptr;
    QPushButton* delete_btn_ = nullptr;
    QPushButton* rename_btn_ = nullptr;
    QLabel* provider_lbl_ = nullptr;
    QLabel* model_lbl_ = nullptr;

    // ── Sidebar collapse state ───────────────────────────────────────────
    QPushButton* sidebar_toggle_btn_ = nullptr;
    QPropertyAnimation* sidebar_anim_ = nullptr;
    bool sidebar_collapsed_ = false;
    static constexpr int kSidebarExpandedWidth = 280;
    void apply_sidebar_collapsed(bool collapsed, bool animate);

    // ── Chat header ──────────────────────────────────────────────────────
    QWidget* chat_widget_ = nullptr;
    QLabel* hdr_status_dot_ = nullptr;
    QLabel* hdr_status_lbl_ = nullptr;
    QLabel* hdr_session_lbl_ = nullptr;
    QLabel* hdr_model_lbl_ = nullptr;
    QLabel* hdr_tokens_lbl_ = nullptr;
    QPushButton* export_btn_ = nullptr;

    // ── Chat area ────────────────────────────────────────────────────────
    QScrollArea* scroll_area_ = nullptr;
    QWidget* messages_container_ = nullptr;
    QVBoxLayout* messages_layout_ = nullptr;
    QWidget* welcome_panel_ = nullptr;
    QPushButton* load_earlier_btn_ = nullptr; // "N earlier messages hidden" (render window)
    QPlainTextEdit* input_box_ = nullptr;
    QPushButton* send_btn_ = nullptr;
    QPushButton* attach_btn_ = nullptr;
    QLabel* attach_badge_ = nullptr; // shows attached file name
    QString attached_file_path_;

    // "This conversation used X / Y" — the active LLM is global, so a
    // conversation reopened after a provider switch silently continues on a
    // different model. Surfaced above the composer with a one-click switch back.
    QWidget* model_notice_ = nullptr;
    QLabel* model_notice_lbl_ = nullptr;
    QPushButton* model_notice_btn_ = nullptr;
    QString conv_provider_; // provider/model of the conversation's latest answer
    QString conv_model_;
    bool model_notice_dismissed_ = false;

    // ── Typing indicator ─────────────────────────────────────────────────
    QWidget* typing_indicator_ = nullptr;
    QLabel* typing_dots_lbl_ = nullptr;
    QTimer* typing_timer_ = nullptr;
    int typing_step_ = 0;

    // ── State ────────────────────────────────────────────────────────────
    QString active_session_id_;
    // Snapshot of request‑time context – used by the async response handler
    QString pending_req_session_;
    QString pending_req_provider_;
    QString pending_req_model_;
    QString active_session_title_;
    std::vector<ai_chat::ConversationMessage> history_;
    bool streaming_ = false;
    // True while the in-flight request went through chat_streaming() (session
    // lane — cancel_active_request() reaches it); false for the blocking chat()
    // fallback, which can only be detached, not interrupted.
    bool stoppable_ = false;
    bool scroll_pending_ = false;
    // Auto-follow the newest content. Cleared when the user scrolls up to read,
    // re-armed when they come back to the bottom or send a new message.
    bool stick_to_bottom_ = true;
    // Set around our own setValue() calls so the valueChanged handler doesn't
    // mistake an auto-scroll for the user scrolling.
    bool programmatic_scroll_ = false;
    QPointer<QLabel> streaming_bubble_;
    QPointer<QWidget> streaming_row_;
    QPointer<QLabel> streaming_role_lbl_;
    int total_tokens_ = 0;
    int total_messages_ = 0;

    // ── Stop that really stops ───────────────────────────────────────────
    // Stop asks LlmService to cancel; most turns end within ~100 ms. A turn
    // blocked inside a tool cannot be interrupted, so after kStopGraceMs the UI
    // stops waiting: the turn is closed as "(stopped)" and the request is
    // DETACHED — its late chunks are dropped by generation, and its late
    // finished_streaming() (the next one for that session: the session lane is
    // FIFO) is swallowed via abandoned_responses_.
    static constexpr int kStopGraceMs = 2500;
    quint64 request_gen_ = 0;
    bool stop_requested_ = false;
    QTimer* stop_grace_timer_ = nullptr;
    QHash<QString, int> abandoned_responses_; // session id → late responses to ignore

    // Regenerate / Retry action on the newest answer (only one is live).
    QPointer<QPushButton> answer_action_btn_;
    // The newest answer's row + role label, dimmed when it is regenerated.
    QPointer<QWidget> last_answer_row_;
    QPointer<QLabel> last_answer_role_lbl_;

    // ── Render window (bounded memory) ──────────────────────────────────
    // Every bubble is a QLabel holding a rich-text document; a thousand-message
    // conversation kept a thousand of them alive. Only the newest
    // render_limit_ messages are built; older ones load on demand.
    static constexpr int kDefaultRenderLimit = 150;
    static constexpr int kMaxLiveRows = 450; // live session: trim oldest rows past this
    int render_limit_ = kDefaultRenderLimit;
    int hidden_message_count_ = 0;
    QList<QPointer<QWidget>> rendered_rows_;

    // ── Per-stream "Thinking" (chain-of-thought) section ─────────────────────
    // Reasoning models stream their thoughts on a separate channel (see
    // think_stream_prefix()). We collect them into a collapsible card inserted
    // ABOVE the answer bubble so the answer stays clean. Reset between messages.
    QPointer<QWidget> thinking_card_;
    QPointer<QPushButton> thinking_header_;
    QPointer<QLabel> thinking_body_;
    QString thinking_text_;
    bool thinking_flush_armed_ = false;

    // ── Per-stream "Tools" section ───────────────────────────────────────────
    // Tool activity arrives on its own channel (see tool_stream_prefix()): a
    // start line per call, then a completion record (LlmToolActivity.h) with
    // outcome + duration. One collapsible card per turn: a compact header
    // ("12 tool calls · 1 failed · 18.4 s") over one line per step / run.
    struct ToolStep {
        enum class State { Running, Ok, Failed, Job };
        QString label;  // "get_quote · AAPL"
        QString base;   // "get_quote" — runs of the same base collapse into "×N"
        QString detail; // failure reason / job id
        State state = State::Running;
        qint64 started_ms = 0;  // UI clock, for steps that never get a record
        qint64 elapsed_ms = -1; // from the completion record
        bool parallel = false;
    };
    QPointer<QWidget> tools_card_;
    QPointer<QPushButton> tools_header_;
    QPointer<QLabel> tools_body_;
    QVector<ToolStep> tool_steps_;
    QStringList tool_job_ids_;   // background jobs seen this turn (receipts + job_* calls)
    QStringList tracked_job_ids_; // jobs whose card line still refreshes (any turn)
    QHash<QString, QPointer<QLabel>> job_labels_; // job id → the card label showing it
    QTimer* job_timer_ = nullptr;

    // ── Build ────────────────────────────────────────────────────────────
    void build_ui();
    void retranslateUi();
    void build_sidebar();
    void build_chat_area();
    QWidget* build_header_bar();
    QWidget* build_welcome();
    QWidget* build_input_area();
    QWidget* build_model_notice();
    QWidget* build_typing_indicator();
    void refresh_theme();

    // ── Data ─────────────────────────────────────────────────────────────
    void load_sessions();
    void load_messages(const QString& session_id);
    void create_new_session();
    QLabel* add_message_bubble(const QString& role, const QString& content, const QString& timestamp = {},
                               const QString& footer_meta = {});
    QLabel* add_streaming_bubble();
    void insert_row(QWidget* row, Qt::Alignment align = {});
    void trim_rendered_rows();
    void on_load_earlier();

    // ── Request lifecycle ────────────────────────────────────────────────
    void start_request(const QString& wire_text, std::vector<ai_chat::ConversationMessage> snapshot);
    void finish_turn(const ai_chat::LlmResponse& response);
    void on_stop_requested();
    void detach_stopped_request();
    void on_regenerate();
    void add_answer_action(QLabel* body, bool failed);
    void retire_answer_action();
    void show_fincept_notice(const ai_chat::LlmResponse& response);
    QString answer_footer_meta(const QString& provider, const QString& model, int tokens) const;
    static std::vector<ai_chat::ConversationMessage> wire_history(const std::vector<ai_chat::ConversationMessage>& h);

    // ── Thinking / tools cards (AiChatScreen_Tools.cpp) ──────────────────
    void append_thinking_chunk(const QString& text);
    void create_thinking_card();
    void flush_thinking();
    void finalize_thinking_card();
    void reset_thinking_state();
    void append_tool_chunk(const QString& text);
    void create_tools_card();
    void render_tools_card();
    void finalize_tools_card();
    void reset_tools_state();
    void track_job(const QString& job_id);
    void refresh_job_lines();
    QString tool_trailer() const; // persisted with the answer (see strip_tool_trailer)
    static QString strip_tool_trailer(const QString& content, QJsonArray* steps);
    void add_static_tools_card(const QJsonArray& steps);
    static QString render_tool_lines(const QVector<ToolStep>& steps);
    static QString tools_summary(const QVector<ToolStep>& steps, bool finished);

    // ── Conversation tools ───────────────────────────────────────────────
    void on_export_markdown();
    void on_copy_conversation_markdown();
    QString conversation_markdown() const;
    void update_model_notice();
    void on_use_conversation_model();
    void show_session_menu(const QPoint& pos);

    void clear_messages();
    void scroll_to_bottom();
    void set_input_enabled(bool enabled);
    void update_stats();
    void show_welcome(bool show);
    void show_typing(bool show);

    // EventBus subscriptions — registered in showEvent, released in hideEvent.
    // MCP set_active_llm publishes llm.provider_changed when the LLM (or
    // Finagent) switches the active provider. We force-reload LlmService so
    // the next message uses the new provider; LlmService::config_changed
    // signal then triggers on_provider_changed for header refresh.
    QList<EventBus::HandlerId> mcp_event_subs_;
    void subscribe_mcp_events();
    void unsubscribe_mcp_events();

    // Phase 7 linking. SymbolGroup::None when unlinked.
    fincept::SymbolGroup link_group_ = fincept::SymbolGroup::None;
    // Last symbol received from the linked group. Saved as the screen's
    // "current_symbol" so the link badge reflects the most recent context.
    // Used to prefix the next user-sent message with "[Context: <SYMBOL>]"
    // so the LLM has the security in scope without the user having to type
    // it. Consumed (one-shot) on send; the linked symbol persists so the
    // badge keeps showing until the group changes.
    fincept::SymbolRef linked_symbol_;
    // True from the moment a linked symbol arrives until the next message is sent. The
    // doc above promises the "[Context: …]" prefix is one-shot, but nothing ever
    // consumed it: linked_symbol_ persists (for the badge), so EVERY later message kept
    // being tagged with a symbol the conversation had long moved on from.
    bool linked_context_pending_ = false;
};

} // namespace fincept::screens
