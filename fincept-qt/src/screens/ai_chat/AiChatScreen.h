#pragma once
// AiChatScreen — the AI Chat tab: a research desk's AI console.
//
//   ┌ ScreenCommandBar: ▌AI CHAT [session title …… F2] [NEW CHAT Ctrl+N] [SESSIONS Ctrl+B] [TOOLS] [NAV]
//   │                   [EXPORT ▾] [⋯ ▾] ………………………………………… [● model ▾] REPLY ≤ 8K · billing
//   ├ sessions rail │ transcript (or the start page)              │ context rail
//   │               │ model notice strip                           │
//   │               │ composer                                     │
//   └ ScreenStatusBar: provider · model | ● READY … notices … TOK 18.2k | $0.04 | LAST 4.1 s | AS OF 14:32
//
// Breakpoints (resizeEvent): >= 1500 px all three zones; 1100–1500 the context rail is an icon
// strip (a click shows it as an overlay); < 1100 the sessions rail is a drawer (Ctrl+B /
// SESSIONS) over the transcript. Overlays close on Esc or a click on the scrim.
//
// Split across:
//   AiChatScreen.cpp            lifecycle, state, keys, breakpoints, overlays, theme, linking
//   AiChatScreen_Layout.cpp     build_ui: bars, zones, wiring of the child widgets
//   AiChatScreen_Messaging.cpp  send / stream / finish / stop / regenerate / notices / status
//   AiChatScreen_Sessions.cpp   sessions, pins, rename, delete, export, the model notice
//   AiChatScreen_Tools.cpp      tool activity, background jobs, tool catalogue, context items
// Child widgets: AiChatTranscript (virtualised transcript), AiChatSessionRail,
// AiChatContextRail, AiChatComposer, AiChatStartPage; model + parser: AiChatTypes; look:
// AiChatStyle.
//
// Never sends anything by itself: example cards, prompt templates, EDIT & RESEND and QUOTE only
// fill the composer; a request starts only from SEND / Enter / REGENERATE / RETRY / ANSWER WITH
// (all marked fincept_agent_risk "spend"). Logs carry character counts, never prompt or answer text.

#include "core/events/EventBus.h"
#include "core/symbol/IGroupLinked.h"
#include "core/symbol/SymbolGroup.h"
#include "core/symbol/SymbolRef.h"
#include "screens/ai_chat/AiChatTypes.h"
#include "screens/common/ISectionLinked.h"
#include "screens/common/IStatefulScreen.h"
#include "services/llm/LlmService.h"

#include <QHash>
#include <QPointer>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVector>
#include <QWidget>

#include <vector>

class QLabel;
class QPushButton;

namespace fincept::ui {
class ModelBadge;
class ScreenCommandBar;
class ScreenStatusBar;
} // namespace fincept::ui

namespace fincept::screens {

namespace aichat {
class AicComposer;
class AicContextRail;
class AicSessionRail;
class AicStartPage;
class AicTranscriptView;
} // namespace aichat

class AiChatScreen : public QWidget,
                     public IStatefulScreen,
                     public fincept::IGroupLinked,
                     public fincept::screens::ISectionLinked {
    Q_OBJECT
    Q_INTERFACES(fincept::IGroupLinked)
  public:
    explicit AiChatScreen(QWidget* parent = nullptr);
    ~AiChatScreen() override;

    void restore_state(const QVariantMap& state) override;
    QVariantMap save_state() const override;
    QString state_key() const override { return "ai_chat"; }
    // v2 adds: draft text, search text, scroll position, attached file path.
    // v3 adds: sidebar_collapsed.
    // v4 adds: attached_files (list), tools_on, nav_on.
    int state_version() const override { return 4; }

    // ISectionLinked — no tabs: "new" (the New Chat button), "session:<id>" (a saved
    // conversation, selected in the list as a click does). `focus` = a session id (with an empty
    // section or "sessions"). Never sends anything to a model.
    bool open_section(const QString& section, const QString& focus) override;

    // IGroupLinked — consumer only. A symbol selected in the linked group becomes a context
    // item of the next message ("[Context: SYMBOL (class)]" on the wire), shown in the context
    // rail and as a composer chip with DETACH — nothing is attached silently. One-shot: consumed
    // by the next send. Chat itself never publishes.
    void set_group(fincept::SymbolGroup g) override;
    fincept::SymbolGroup group() const override { return link_group_; }
    void on_group_symbol_changed(const fincept::SymbolRef& ref) override;
    fincept::SymbolRef current_symbol() const override { return linked_symbol_; }

  protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;
    bool eventFilter(QObject* obj, QEvent* event) override;
    void changeEvent(QEvent* event) override;

  private slots:
    void on_send();
    void on_new_session();
    void on_session_selected(const QString& id);
    void on_stream_chunk(const QString& chunk, bool done);
    void on_streaming_done(ai_chat::LlmResponse response);
    /// LlmService::route_notice: our request's harness was unavailable and the
    /// next model answers — announced before the answer streams.
    void on_route_notice(const QString& origin_session_id, const QString& feature, const QString& notice);
    void on_provider_changed();

  private:
    enum class Zones { Wide, Medium, Narrow };
    enum class Overlay { None, Sessions, Context };

    // ── Frame ────────────────────────────────────────────────────────────
    ui::ScreenCommandBar* bar_ = nullptr;
    ui::ScreenStatusBar* status_ = nullptr;
    ui::ModelBadge* model_badge_ = nullptr; // which model answers AI Chat (ModelRouter); click = picker
    QLabel* model_hint_ = nullptr;          // reply cap · who pays
    QWidget* body_ = nullptr;
    QWidget* centre_ = nullptr;
    QWidget* scrim_ = nullptr;              // behind an overlay rail; a click closes it
    aichat::AicSessionRail* sessions_ = nullptr;
    aichat::AicTranscriptView* transcript_ = nullptr;
    aichat::AicStartPage* start_page_ = nullptr;
    aichat::AicComposer* composer_ = nullptr;
    aichat::AicContextRail* context_rail_ = nullptr;

    Zones zones_ = Zones::Wide;
    Overlay overlay_ = Overlay::None;
    bool sidebar_collapsed_ = false; // user's choice for the docked sessions rail (Ctrl+B)

    // "This conversation used X / Y" — the active LLM is global, so a conversation
    // reopened after a provider switch silently continues on a different model. Surfaced
    // above the composer with a one-click switch back.
    QWidget* model_notice_ = nullptr;
    QLabel* model_notice_lbl_ = nullptr;
    QPushButton* model_notice_btn_ = nullptr;
    QString conv_provider_; // provider/model of the conversation's latest answer
    QString conv_model_;
    bool model_notice_dismissed_ = false;

    // ── Session state ────────────────────────────────────────────────────
    QString active_session_id_;
    QString active_session_title_;
    QVector<aichat::SessionSummary> sessions_cache_;
    QSet<QString> pinned_;                 // SettingsRepository "ai_chat.pinned_sessions"
    QHash<QString, QString> snippets_;     // session id → last message, this run
    bool unread_ = false;                  // an answer arrived while the tab was hidden
    std::vector<ai_chat::ConversationMessage> history_;
    int total_tokens_ = 0;
    int total_messages_ = 0;
    // This run's answers only (the database keeps totals, not in / out / cost).
    qint64 run_prompt_tokens_ = -1;
    qint64 run_completion_tokens_ = -1;
    double run_cost_usd_ = -1.0;
    qint64 last_answer_ms_ = -1;
    QDateTime last_answer_at_;
    bool last_failed_ = false;

    // ── Request lifecycle ────────────────────────────────────────────────
    QString pending_req_session_;
    QString pending_req_provider_;
    QString pending_req_model_;
    QString pending_req_label_;
    bool route_notice_shown_ = false; // the pending request's fallback was announced already
    bool streaming_ = false;
    // True while the in-flight request went through chat_streaming() (session lane —
    // cancel_active_request() reaches it); false for the blocking chat() fallback,
    // which can only be detached, not interrupted.
    bool stoppable_ = false;
    quint64 streaming_turn_ = 0;     // the assistant turn being written
    quint64 last_answer_turn_ = 0;   // newest real answer (dimmed when regenerated)
    qint64 request_started_ms_ = 0;
    bool tools_on_ = true;           // ToolPolicy::All / None for this chat
    bool nav_on_ = true;             // ToolPolicy::All vs NoNavigation
    QSet<QString> known_symbols_;    // linked / mentioned / tool-argument symbols → ticker chips

    // Stop asks LlmService to cancel; most turns end within ~100 ms. A turn blocked inside
    // a tool cannot be interrupted, so after kStopGraceMs the UI stops waiting: the turn is
    // closed as stopped and the request is DETACHED — its late chunks are dropped by
    // generation, and its late finished_streaming() (the next one for that session: the
    // session lane is FIFO) is swallowed via abandoned_responses_.
    static constexpr int kStopGraceMs = 2500;
    quint64 request_gen_ = 0;
    bool stop_requested_ = false;
    QTimer* stop_grace_timer_ = nullptr;
    QHash<QString, int> abandoned_responses_; // session id → late responses to ignore

    // ── Tool activity (AiChatScreen_Tools.cpp) ───────────────────────────
    QVector<aichat::ToolStep> tool_steps_;
    QStringList tool_job_ids_;     // background jobs seen this turn
    QStringList tracked_job_ids_;  // jobs whose status line still refreshes (any turn)
    QTimer* job_timer_ = nullptr;
    QTimer* tool_render_timer_ = nullptr; // coalesces tool-step repaints
    bool tool_catalogue_loading_ = false;
    bool tool_catalogue_loaded_ = false;  // the first catalogue arrived (before: "loading…")

    // ── Context the next message carries ─────────────────────────────────
    QVector<aichat::Attachment> context_; // mentions, portfolio / watchlist references
    QVector<aichat::Attachment> files_;   // attached files (metadata only until sent)
    QVector<aichat::PromptTemplate> saved_prompts_;

    // ── Build (AiChatScreen_Layout.cpp) ──────────────────────────────────
    void build_ui();
    void build_command_bar();
    void build_status_bar();
    QWidget* build_model_notice();
    void wire_children();
    void retranslateUi();
    void refresh_theme();
    void apply_zones(bool force = false);
    void open_overlay(Overlay which, int section = 0);
    void close_overlay();
    void place_overlay();
    void toggle_sessions();
    void update_tab_order();

    // ── Sessions (AiChatScreen_Sessions.cpp) ─────────────────────────────
    void load_sessions();
    void load_messages(const QString& session_id);
    void create_new_session();
    void show_start_page(bool show);
    void begin_rename();
    void rename_session(const QString& id, const QString& title);
    void delete_session(const QString& id);
    void set_pinned(const QString& id, bool pinned);
    void load_pins();
    void store_pins() const;
    QString session_title(const QString& id) const;
    QString conversation_markdown(const QString& session_id) const;
    void export_markdown(const QString& session_id);
    void copy_markdown(const QString& session_id);
    void update_model_notice();
    void on_use_conversation_model();

    // ── Messaging (AiChatScreen_Messaging.cpp) ───────────────────────────
    /// `skip_harness`: answer without that harness (the user accepted the fallback offer
    /// after it was unavailable).
    void start_request(const QString& wire_text, std::vector<ai_chat::ConversationMessage> snapshot,
                       const QString& skip_harness = {});
    void finish_turn(const ai_chat::LlmResponse& response);
    void on_stop_requested();
    void detach_stopped_request();
    void on_regenerate();
    void on_answer_with(quint64 turn_id);
    void on_turn_action(quint64 turn_id, aichat::TurnAction action);
    quint64 add_notice(aichat::NoticeKind kind, const QString& markdown, const QString& error = {});
    void show_fincept_notice(const ai_chat::LlmResponse& response);
    void open_code_in_code_tab(const QString& code, const QString& lang);
    void set_busy(bool busy);
    void update_stats();
    void update_status();
    void update_model_hint();
    static std::vector<ai_chat::ConversationMessage> wire_history(const std::vector<ai_chat::ConversationMessage>& h);

    // ── Tools / context (AiChatScreen_Tools.cpp) ─────────────────────────
    void append_tool_chunk(const QString& text);
    void finalize_tool_steps();
    void track_job(const QString& job_id);
    void refresh_job_lines();
    void load_tool_catalogue();
    void note_symbols_from_tools();
    QVector<aichat::Attachment> outgoing_context() const; // linked (if attached) + context_
    void refresh_context_ui();
    void add_files(const QStringList& paths);
    void pick_files();
    void pick_symbol();
    void add_context(const aichat::Attachment& item);
    void remove_context(const aichat::Attachment& item);
    void save_draft_prompt();
    void delete_saved_prompt(int index);

    // EventBus subscriptions — registered in showEvent, released in hideEvent.
    // MCP set_active_llm publishes llm.provider_changed when the LLM (or Finagent) switches
    // the active provider: LlmService reloads, its config_changed refreshes the header.
    QList<EventBus::HandlerId> mcp_event_subs_;
    void subscribe_mcp_events();
    void unsubscribe_mcp_events();

    // Symbol-group linking. SymbolGroup::None when unlinked.
    fincept::SymbolGroup link_group_ = fincept::SymbolGroup::None;
    // Last symbol received from the linked group (current_symbol()).
    fincept::SymbolRef linked_symbol_;
    // The linked symbol is attached to the next message (one-shot; DETACH clears it).
    bool linked_context_pending_ = false;
};

} // namespace fincept::screens
