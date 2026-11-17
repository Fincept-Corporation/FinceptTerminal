#pragma once
#include "screens/chat_mode/ChatModeTypes.h"
#include "screens/chat_mode/ChatMarkdown.h"
#include "screens/chat_mode/TranscriptModel.h"
#include "services/agent_tasks/TaskRunTypes.h"

#include <QHash>
#include <QJsonObject>
#include <QStringList>
#include <QVector>
#include <QWidget>

class QFrame;
class QLabel;
class QPushButton;
class QStackedWidget;
class QTimer;

namespace fincept::chat_mode {

class ChatComposer;
class ChatStartPage;
class ComposerContextBar;
class TranscriptView;

/// Chat Mode's centre: the start page or the conversation as a document
/// (TranscriptView: virtualised, Markdown rendered incrementally while it streams),
/// and the composer (ChatComposer) under it. 2026-11-17 revamp of the WP2 panel —
/// the public contract with ChatModeScreen is unchanged.
///
/// A reply in progress is a run of agent_tasks::TaskRunService, not state of this
/// widget: the panel only RENDERS the run of the conversation it shows
/// (attach_run()). Switching conversations, leaving Chat Mode or closing the window
/// never touches the run; coming back re-attaches and replays what happened
/// meanwhile. Stored conversations are turned into transcript items on a worker
/// thread (request ids: only the newest load is shown).
class ChatMessagePanel : public QWidget {
    Q_OBJECT
  public:
    explicit ChatMessagePanel(QWidget* parent = nullptr);
    ~ChatMessagePanel() override;

    /// The stored messages of the shown conversation (built on a worker; the whole
    /// conversation is reachable by scrolling).
    void load_messages(const QVector<ChatMessage>& messages);
    void clear_messages();
    /// Loading the conversation failed: the reason and a Retry (emits retry_load_requested()).
    void show_load_error(const QString& message);
    void set_session_title(const QString& title);
    QString session_title() const { return session_title_; }
    void set_stream_mode(StreamMode mode);
    StreamMode current_mode() const;

    // Draft + scroll accessors used by ChatModeScreen for session persistence.
    QString draft_text() const;
    void set_draft_text(const QString& text);
    int scroll_position() const;
    void set_scroll_position(int pos);

    // Investment profile sent with each message (GET /profile/profiles).
    void set_profiles(const QVector<AgentProfile>& profiles);
    int selected_profile_id() const;
    int preferred_profile_id() const;
    void set_preferred_profile_id(int id);

    /// The conversation shown (where trade decisions are recorded). Set by the screen.
    void set_session_id(const QString& session_id) { session_id_ = session_id; }
    QString session_id() const { return session_id_; }

    /// The composer's next message answers a question the agent paused on.
    bool is_awaiting_answer() const;

    ComposerContextBar* context_bar() const;
    ChatComposer* composer() const { return composer_; }
    TranscriptView* transcript() const { return view_; }
    /// Keyboard focus into the composer (Ctrl+Shift+L) / the transcript (Alt+2).
    void focus_composer();
    void focus_transcript();
    /// The start page is shown (no conversation content).
    bool start_page_shown() const;
    /// Show the start page for a brand-new conversation (Ctrl+N): the transcript is cleared.
    void show_start_page();

    // ── Runs ─────────────────────────────────────────────────────────────────
    void attach_run(const QString& run_id, bool user_bubble_shown);
    QString attached_run() const { return rv_.run_id; }
    bool has_pending_send() const { return pending_send_; }
    void bind_pending_run(const QString& run_id);
    void fail_pending_send(const QString& message);
    void show_error(const QString& message);
    void add_notice(const QString& text);
    void show_service_problem(const QString& text);
    void clear_service_problem();

  signals:
    void draft_changed();
    void scroll_changed();
    void profile_changed();
    void send_requested(const QString& message, StreamMode mode);
    /// The user answered the question the agent's run paused on.
    void answer_requested(const QString& answer, StreamMode mode);
    /// STOP: stop following the attached run (the server finishes a cloud reply anyway).
    void stop_requested(const QString& run_id);
    void mode_toggled(StreamMode mode);
    void proposal_decided(const QString& proposal_id, const QString& status);
    void retry_load_requested();
    void service_retry_requested();
    /// The shown conversation's display title changed (the command bar shows it).
    void session_title_changed(const QString& title);
    /// Response time of the last answer (stored or just finished), ms; < 0 unknown.
    void last_response_changed(qint64 ms);
    /// A recent run on the start page / a run a transcript action created.
    void run_open_requested(const QString& run_id);
    void run_selection_requested(const QString& run_id);
    /// NEW RUN's gallery (start page, "/" menu) — optionally preselected.
    void templates_requested();
    void template_requested(const QString& template_id);
    /// A ticker chip's "Open in Equity Research" (the screen leaves Chat Mode first).
    void symbol_open_requested(const QString& symbol);
    /// OPEN IN CODE TAB on a Python code block (the screen writes a notebook and leaves).
    void code_open_requested(const QString& code);
    /// A start-page SET UP link opens another screen: Chat Mode must leave first.
    void leave_requested();
    /// Esc in an idle composer with an empty draft.
    void escape_pressed();

  public slots:
    void on_run_event(const QString& run_id, const fincept::agent_tasks::RunEvent& ev);
    void on_run_text_delta(const QString& run_id, int step, const QString& chunk);
    void on_run_updated(const QString& run_id);
    void on_run_finished(const QString& run_id);
    void on_insufficient_credits(const QString& message);
    void set_credits(double credits);
    void set_turn_cost(double credits);

  private:
    // ── Widgets ──
    QFrame* service_banner_ = nullptr;
    QLabel* service_banner_text_ = nullptr;
    QPushButton* service_retry_btn_ = nullptr;
    QPushButton* service_dismiss_btn_ = nullptr;
    QStackedWidget* stack_ = nullptr;
    ChatStartPage* start_ = nullptr;
    TranscriptView* view_ = nullptr;
    ChatComposer* composer_ = nullptr;

    QString session_id_;
    QString session_title_;
    double last_credits_ = 0;
    bool send_confirmed_ = false;
    QString awaiting_question_;
    QString turn_answers_question_;
    QString turn_answer_text_;
    QString status_text_;          ///< the live activity line ("thinking…")
    QHash<QString, quint64> proposal_items_; ///< proposal id -> Approval item

    // ── History (worker) ──
    int load_generation_ = 0;
    bool history_pending_ = false;
    bool deferred_attach_ = false;
    bool deferred_attach_shown_ = false;
    QStringList deferred_notices_;
    quint64 loading_item_ = 0;

    // ── The attached run ──
    struct RunView {
        QString run_id;
        int last_seq = 0;
        int turn = -1;                    ///< the live agent turn
        qint64 turn_started_ms = 0;
        quint64 plan_item = 0;
        quint64 reasoning_item = 0;
        QString thinking_text;
        QHash<QString, quint64> activity; ///< ref id -> Activity item
        QVector<quint64> activity_order;  ///< this turn's activity rows (folded when it ends)
        int tools = 0;
        int delegations = 0;
        QString text;                     ///< everything streamed in this turn
        QString pending_text;             ///< chunks not rendered yet
        int seg_start = 0;                ///< start of the open text segment in `text`
        md::StreamCursor cursor;          ///< over text.mid(seg_start)
        int seg_parsed = 0;               ///< stable part of the segment already rendered
        QVector<quint64> tail;            ///< items of the open tail (replaced on each update)
        QVector<quint64> seg_items;       ///< Markdown items of the open segment (settled + tail)
        QVector<quint64> text_items;      ///< every Markdown item of this turn
        QVector<md::Link> links;
        QStringList follow_ups;
        QString error_code;
        bool turn_finalized = false;
        bool finished_rendered = false;
        bool steps_shown = false;
    };
    RunView rv_;
    bool busy_ = false;
    bool pending_send_ = false;
    bool last_item_is_credits_ = false; ///< no second OUT OF CREDITS card in a row
    QTimer* stream_flush_timer_ = nullptr;
    static constexpr int kStreamFlushMs = 40; ///< <= 25 transcript updates a second
    static constexpr int kMaxThinkingChars = 20000;

    void build_ui();
    void wire();
    void update_page();
    md::ParseOptions parse_options() const;

    // transcript helpers
    int add_user_turn(const QString& text, const QString& context_block, const QDateTime& when);
    void ensure_turn();
    quint64 add_live_item(const transcript::ItemPtr& item);
    quint64 add_note(const QString& text);
    quint64 add_error(const QString& text, const QString& action = {}, const QString& action_text = {},
                      const QString& ref = {});
    void add_credits_card();
    void add_question(const QString& question, const QString& hint);
    void add_proposal(const QJsonObject& proposal, const QString& status);
    void settle_proposal(const QString& proposal_id, const QString& text);
    void decide_proposal(quint64 item_id, bool approved, const QString& fallback_text, int attempt);
    void set_awaiting_answer(bool on, const QString& question = {});
    void set_status(const QString& text);

    // run rendering
    void do_attach(const QString& run_id, bool user_bubble_shown);
    void render_event(const agent_tasks::RunEvent& ev, bool replay);
    void upsert_activity(const agent_tasks::RunEvent& ev);
    void refresh_plan_item();
    void close_text_segment();
    void flush_stream();
    void finalize_turn(const QString& final_text);
    void start_new_turn_view();
    void update_busy_state();

    // composer / transcript actions
    void on_submit(const QString& text);
    void send_text(const QString& text, bool as_answer);
    void on_transcript_action(const transcript::Action& action);
    void send_turn_to_notes(int turn);
    void save_activity_artefact(quint64 item_id);
    void show_symbol_menu(const QString& symbol);
    void history_ready(int generation, const QVector<transcript::Turn>& turns, const QVector<transcript::ItemPtr>& items,
                       const QString& latest_question, qint64 last_response_ms);
};

} // namespace fincept::chat_mode
