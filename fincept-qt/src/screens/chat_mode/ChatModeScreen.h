#pragma once
#include "screens/chat_mode/ChatModeTypes.h"
#include "screens/common/ISectionLinked.h"
#include "screens/common/IStatefulScreen.h"
#include "services/agent_tasks/TaskRunTypes.h"

#include <QList>
#include <QPointer>
#include <QWidget>

#include <functional>

class QMenu;
class QPushButton;
class QSplitter;
class QTimer;

namespace fincept::ui {
class ModelBadge;
class ScreenCommandBar;
} // namespace fincept::ui

namespace fincept::chat_mode {

class ChatAgentPanel;
class ChatMessagePanel;
class ChatModeLeftRail;
class ChatSessionPanel;
class NewRunDialog;
class RunListPanel;
class RunSchedulesDialog;
class RunStatusBar;

/// Full-screen Chat Mode — replaces the entire terminal UI (2026-11-17 revamp).
///
///   [◂ WORKSPACE F9] ▌CHAT MODE [› conversation title  F2] [NEW] [ENGINE ▾] | [TEMPLATES] [SCHEDULE] [EXPORT ▾] … [⋯]
///   ┌ LEFT RAIL ─────────┬ CENTRE ───────────────────────────────┬ RIGHT RAIL ────────┐
///   │ RUNS·CHATS·MEMORY·MCP│ start page / transcript document     │ CONTEXT·PLAN·        │
///   │ (ChatModeLeftRail) │ composer (ChatMessagePanel)           │ ACTIVITY·FILES       │
///   └────────────────────┴───────────────────────────────────────┴────────────────────┘
///   [status: engine · state · steps · title · tokens · credits · time · last · tools · MCP · balance · PAPER]
///
/// Every message is a run of agent_tasks::TaskRunService: it belongs to the
/// service, not to this screen, so it keeps going when the user switches
/// conversations, leaves Chat Mode, locks or closes the window. The screen only
/// shows the run of the conversation it displays (re-attaching on return), and
/// reports its visibility to the service (notifications) and to the app-level
/// TerminalToolBridge. The rails sit in a splitter (sizes remembered), collapse at
/// narrow widths and toggle with Ctrl+B / Ctrl+Alt+B.
class ChatModeScreen : public QWidget, public fincept::screens::IStatefulScreen, public fincept::screens::ISectionLinked {
    Q_OBJECT
  public:
    explicit ChatModeScreen(QWidget* parent = nullptr);
    ~ChatModeScreen() override;

    void restore_state(const QVariantMap& state) override;
    QVariantMap save_state() const override;
    QString state_key() const override { return "chat_mode"; }
    // v2: draft composer text, scroll position (later keys are additive: rails, section).
    int state_version() const override { return 2; }

    /// Open a run named from outside (Notes' OPEN CHAT MODE through
    /// nav.switch_screen {"screen_id": "chat_mode", "run_id": …}): the RUNS rail with
    /// the run selected and its conversation open. `run_ref` is a run id or its
    /// 8-character prefix (run_view::resolve_run_ref). Waits for the persisted runs
    /// when they are not loaded yet. An unknown or deleted run: the RUNS list with
    /// the notice "That run no longer exists".
    void open_run_link(const QString& run_ref);

    /// ISectionLinked — "runs" | "conversations" | "memory" (= "memories", "agents") |
    /// "mcp" | "schedules" | "templates" (focus = a template id) | "new" (focus = text
    /// for the composer, never sent) | "context" | "plan" | "activity" | "files".
    /// For "runs" (and any section) a focus that names a run opens it (open_run_link).
    bool open_section(const QString& section, const QString& focus) override;

  signals:
    void exit_requested();

  protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;
    bool eventFilter(QObject* watched, QEvent* e) override;

  private:
    // ── Frame ──
    QWidget* top_ = nullptr;
    QPushButton* back_btn_ = nullptr;
    ui::ScreenCommandBar* bar_ = nullptr;
    ui::ModelBadge* harness_badge_ = nullptr;
    QMenu* export_menu_ = nullptr;
    QMenu* more_menu_ = nullptr;
    QSplitter* split_ = nullptr;
    ChatModeLeftRail* left_ = nullptr;
    ChatSessionPanel* session_panel_ = nullptr; ///< left_->conversations()
    RunListPanel* run_list_ = nullptr;          ///< left_->runs()
    ChatMessagePanel* message_panel_ = nullptr;
    ChatAgentPanel* agent_panel_ = nullptr;     ///< the inspector (right rail)
    RunStatusBar* status_bar_ = nullptr;
    QTimer* sizes_timer_ = nullptr;
    QPointer<NewRunDialog> new_run_dialog_; ///< non-modal, one per window
    QPointer<RunSchedulesDialog> schedules_dialog_;

    // ── Rails ──
    bool left_user_hidden_ = false;  ///< the user closed the rail (Ctrl+B)
    bool right_user_hidden_ = false; ///< the user closed the inspector (Ctrl+Alt+B)
    bool left_forced_ = false;       ///< the user opened it although the window is narrow
    bool right_forced_ = false;
    QList<int> saved_sizes_;         ///< last sizes with both rails shown
    bool sizes_restored_ = false;

    // ── State ──
    QString active_session_uuid_;
    QString current_run_id_;   ///< the run the inspector / status bar show
    QString pending_run_link_; ///< open_run_link() before the persisted runs loaded
    int load_seq_ = 0;         ///< only the newest conversation load may fill the transcript
    bool view_counted_ = false;
    /// The last "could not load / register" notices. Every visit to Chat Mode
    /// retries; the same failure is reported once, not once per visit.
    QString last_load_notice_;
    QString last_bridge_notice_;

    void build_ui();
    void build_command_bar();
    void wire_signals();
    void set_view_counted(bool visible);
    void on_bar_action(const QString& id);
    void refresh_engine_chip();
    void refresh_title_field();
    void commit_title(const QString& text);

    // rails
    void apply_rails();
    void toggle_left();
    void toggle_right();
    void focus_left(const QString& section);
    void focus_right(const QString& page);
    void save_rail_settings();
    void restore_rail_settings();

    // export
    void export_markdown();
    /// OPEN IN CODE TAB (Python blocks): a one-cell notebook under <files>/chat_mode,
    /// then Chat Mode is left and the Code tab opens it (nothing runs).
    void open_code_in_code_tab(const QString& code);

    // conversations
    void on_session_selected(const QString& uuid);
    void reload_active_session();
    /// Open the most recently used conversation (first visit, and the RETRY of the
    /// service banner when the list could not be read).
    void load_recent_session(bool retry);
    void on_new_session();
    /// Ctrl+N / NEW: a fresh start page; the conversation is created with the first message.
    void start_new_conversation();
    void on_delete_session(const QString& uuid);
    void on_rename_session(const QString& uuid, const QString& title);
    void on_send_requested(const QString& message, StreamMode mode);
    void on_answer_requested(const QString& answer, StreamMode mode);
    void ensure_active_session(std::function<void(const QString& uuid)> then);
    /// Re-reads /chat/stats into the CHATS footer ("N conversations · M messages").
    void refresh_stats();

    // ── Runs ─────────────────────────────────────────────────────────────────
    agent_tasks::RunRequest chat_request(const QString& text, StreamMode mode, bool compose) const;
    /// Start `request` in `session_uuid`; binds it to the composer's pending send.
    void start_chat_run(agent_tasks::RunRequest request, const QString& session_uuid);
    /// After a conversation loaded: show its open run, or a just-finished run whose
    /// answer the stored messages do not hold yet.
    void attach_session_run(const QVector<ChatMessage>& messages);
    void set_current_run(const QString& run_id);
    void open_run(const QString& run_id);
    void on_run_added(const QString& run_id);
    void on_run_finished(const QString& run_id);
    void on_run_event(const QString& run_id, const agent_tasks::RunEvent& ev);
    void open_new_run_dialog(const QString& template_id = {});
    void open_schedules_dialog();
};

} // namespace fincept::chat_mode
