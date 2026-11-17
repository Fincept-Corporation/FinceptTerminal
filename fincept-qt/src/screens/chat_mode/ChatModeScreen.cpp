// ChatModeScreen.cpp — Chat Mode's frame (2026-11-17 revamp): command bar, three
// re-flowing zones in a splitter (RUNS rail · document · inspector), status bar.
// The conversation / run logic is the WP2 screen's, unchanged in meaning.

#include "screens/chat_mode/ChatModeScreen.h"

#include "core/config/AppPaths.h"
#include "core/events/EventBus.h"
#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "screens/chat_mode/ChatAgentPanel.h"
#include "screens/chat_mode/ChatComposer.h"
#include "screens/chat_mode/ChatMessagePanel.h"
#include "screens/chat_mode/ChatModeLeftRail.h"
#include "screens/chat_mode/ChatModeService.h"
#include "screens/chat_mode/ChatSessionPanel.h"
#include "screens/chat_mode/ComposerContextBar.h"
#include "screens/chat_mode/NewRunDialog.h"
#include "screens/chat_mode/RunListPanel.h"
#include "screens/chat_mode/RunSchedulesDialog.h"
#include "screens/chat_mode/RunStatusBar.h"
#include "screens/chat_mode/RunViewLogic.h"
#include "screens/chat_mode/TerminalToolBridge.h"
#include "screens/chat_mode/TranscriptView.h"
#include "screens/ai_chat/AiChatTypes.h"
#include "services/agent_tasks/RunPlanPrompts.h"
#include "services/agent_tasks/TaskRunService.h"
#include "services/llm/ModelRouter.h"
#include "ui/navigation/KeyRouting.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"
#include "ui/widgets/ModelBadge.h"
#include "ui/widgets/ScreenCommandBar.h"
#include "ui/widgets/ScreenStatusBar.h"
#include "ui/widgets/ThemeHelpers.h"

#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QJsonArray>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>
#include <QPointer>
#include <QPushButton>
#include <QResizeEvent>
#include <QSaveFile>
#include <QSettings>
#include <QShortcut>
#include <QShowEvent>
#include <QSplitter>
#include <QThreadPool>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

namespace fincept::chat_mode {

namespace agt = fincept::agent_tasks;

namespace chat_mode_screen_detail {

/// A finished run is shown from local state when the stored conversation does not
/// hold its answer yet — but only for this long after it ended.
constexpr qint64 kRecentRunMs = 10 * 60 * 1000;

constexpr const char* kCmsSizesKey = "chat_mode/rail_sizes";
constexpr const char* kCmsHiddenKey = "chat_mode/rails_hidden"; ///< "left,right" of the rails the user closed

/// Index of the last stored user message equal to `text`, -1 when none. A planned
/// task is stored as its planning request: that one counts as the task's message.
int cms_last_user_message(const QVector<ChatMessage>& messages, const QString& text) {
    const QString needle = text.trimmed();
    for (int i = static_cast<int>(messages.size()) - 1; i >= 0; --i) {
        if (messages.at(i).role != QLatin1String("user"))
            continue;
        if (messages.at(i).content.trimmed() == needle)
            return i;
        const agt::run_plan_prompts::RequestInfo info = agt::run_plan_prompts::classify_request(messages.at(i).content);
        if (info.kind == agt::run_plan_prompts::RequestKind::Plan && info.goal == needle)
            return i;
    }
    return -1;
}

bool cms_has_answer_after(const QVector<ChatMessage>& messages, int idx) {
    for (int i = idx + 1; i < messages.size(); ++i)
        if (messages.at(i).role == QLatin1String("assistant") &&
            !messages.at(i).metadata.contains(QStringLiteral("proposal_decision")))
            return true;
    return false;
}

/// The frame's sheet (screen background, top strip, back button, splitter handles).
QString cms_style() {
    return QStringLiteral(
               "QWidget#chatModeScreen{background:%1;}"
               "QWidget#chatModeTop{background:%2;border:none;border-bottom:1px solid %3;}"
               "QPushButton#chatModeBack{background:transparent;color:%4;border:none;border-right:1px solid %3;"
               "padding:0 14px;font-family:%5;font-size:%6px;font-weight:700;letter-spacing:1px;}"
               "QPushButton#chatModeBack:hover{color:%7;background:%8;}"
               "QPushButton#chatModeBack:focus{color:%7;border:1px solid %7;}"
               "QSplitter#chatModeSplit{background:%1;}"
               "QSplitter#chatModeSplit::handle{background:%1;}"
               "QSplitter#chatModeSplit::handle:hover{background:%9;}")
        .arg(ui::colors::BG_BASE(), ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM(), ui::colors::TEXT_SECONDARY(),
             QString(ui::fonts::DATA_FAMILY), ui::fonts::px(-3), ui::colors::AMBER(), ui::colors::BG_HOVER(),
             ui::colors::AMBER_DIM());
}

/// A file name stem from a title (letters, digits, '-', '_').
QString cms_file_stem(const QString& title) {
    QString s;
    for (const QChar c : title.left(48))
        s += (c.isLetterOrNumber() || c == QLatin1Char('-') || c == QLatin1Char('_')) ? c : QLatin1Char('_');
    while (s.contains(QStringLiteral("__")))
        s.replace(QStringLiteral("__"), QStringLiteral("_"));
    return s.isEmpty() ? QStringLiteral("conversation") : s;
}

} // namespace chat_mode_screen_detail

using namespace chat_mode_screen_detail;

ChatModeScreen::ChatModeScreen(QWidget* parent) : QWidget(parent) {
    build_ui();
    wire_signals();
    // main.cpp initialises the run engine on its normal start path; the first-time
    // Python setup path builds the window elsewhere. Every window has this screen,
    // so it makes sure (idempotently) that interrupted runs are marked and listed.
    QTimer::singleShot(0, this, []() { agt::TaskRunService::instance().initialize_deferred(); });
    // Fonts changed: the rails' minimum widths follow (re-fitted once styles are final).
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this, [this]() { apply_rails(); });
}

ChatModeScreen::~ChatModeScreen() {
    set_view_counted(false);
}

// ── Frame ────────────────────────────────────────────────────────────────────

void ChatModeScreen::build_ui() {
    setObjectName(QStringLiteral("chatModeScreen"));
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(cms_style()); // before any child exists

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    // Top: the way back to the workspace (far left), then the command bar.
    top_ = new QWidget(this);
    top_->setObjectName(QStringLiteral("chatModeTop"));
    top_->setAttribute(Qt::WA_StyledBackground, true);
    auto* tl = new QHBoxLayout(top_);
    tl->setContentsMargins(0, 0, 0, 0);
    tl->setSpacing(0);
    back_btn_ = new QPushButton(top_);
    back_btn_->setObjectName(QStringLiteral("chatModeBack"));
    back_btn_->setCursor(Qt::PointingHandCursor);
    back_btn_->setText(tr("◂ WORKSPACE   F9"));
    back_btn_->setToolTip(tr("Back to the workspace (F9, or Esc). Runs keep going in the background."));
    back_btn_->setAccessibleName(tr("Back to the workspace"));
    back_btn_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    tl->addWidget(back_btn_);
    bar_ = new ui::ScreenCommandBar(top_);
    tl->addWidget(bar_, 1);
    outer->addWidget(top_);

    // The three zones.
    split_ = new QSplitter(Qt::Horizontal, this);
    split_->setObjectName(QStringLiteral("chatModeSplit"));
    split_->setChildrenCollapsible(false);
    // Wide enough to grab; the rails draw their own hairlines.
    split_->setHandleWidth(std::max(3, QFontMetrics(ui::fonts::font(0)).averageCharWidth() / 2));
    left_ = new ChatModeLeftRail(split_);
    session_panel_ = left_->conversations();
    run_list_ = left_->runs();
    message_panel_ = new ChatMessagePanel(split_);
    agent_panel_ = new ChatAgentPanel(split_, AgentPanelPages::Inspector);
    agent_panel_->set_context_bar(message_panel_->context_bar());
    split_->addWidget(left_);
    split_->addWidget(message_panel_);
    split_->addWidget(agent_panel_);
    split_->setStretchFactor(0, 0);
    split_->setStretchFactor(1, 1);
    split_->setStretchFactor(2, 0);
    outer->addWidget(split_, 1);

    status_bar_ = new RunStatusBar(this);
    outer->addWidget(status_bar_);
    build_command_bar(); // after the composer exists: the engine chip shares its menu

    sizes_timer_ = new QTimer(this);
    sizes_timer_->setSingleShot(true);
    sizes_timer_->setInterval(600);

    // Keys (Qt::WidgetWithChildrenShortcut: only while focus is inside Chat Mode). Ctrl+L
    // stays the shell's LOCK — the composer is Ctrl+Shift+L.
    const auto key = [this](const QKeySequence& seq, std::function<void()> fn) {
        auto* s = new QShortcut(seq, this);
        s->setContext(Qt::WidgetWithChildrenShortcut);
        connect(s, &QShortcut::activated, this, std::move(fn));
    };
    key(QKeySequence(Qt::CTRL | Qt::Key_N), [this]() { start_new_conversation(); });
    key(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_L), [this]() { message_panel_->focus_composer(); });
    key(QKeySequence(Qt::ALT | Qt::Key_1), [this]() { focus_left(QStringLiteral("runs")); });
    key(QKeySequence(Qt::ALT | Qt::Key_2), [this]() { message_panel_->focus_transcript(); });
    key(QKeySequence(Qt::ALT | Qt::Key_3), [this]() { focus_right(QString()); });
    key(QKeySequence(Qt::CTRL | Qt::Key_B), [this]() { toggle_left(); });
    key(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_B), [this]() { toggle_right(); });
    key(QKeySequence(Qt::Key_F2), [this]() { bar_->focus_command_field(true); });
    ui::claim_keys(this, {QKeySequence(Qt::CTRL | Qt::Key_N), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_L),
                          QKeySequence(Qt::ALT | Qt::Key_1), QKeySequence(Qt::ALT | Qt::Key_2),
                          QKeySequence(Qt::ALT | Qt::Key_3), QKeySequence(Qt::CTRL | Qt::Key_B),
                          QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_B), QKeySequence(Qt::Key_F2)});
    restore_rail_settings();
}

void ChatModeScreen::build_command_bar() {
    using Role = ui::ScreenCommandBar::Role;
    bar_->set_title(tr("CHAT MODE"));
    // The conversation's title, edited in place (Enter renames, Esc keeps it).
    bar_->set_command_field(tr("Conversation title — F2 to rename"), QStringLiteral("F2"));
    bar_->set_command_width_chars(44, 14);
    bar_->command_field()->installEventFilter(this);
    bar_->add_action(QStringLiteral("new"), tr("NEW"), QStringLiteral("Ctrl+N"),
                     tr("A new conversation: the start page, the message box ready (Ctrl+N)"), Role::Primary, 100);
    // The composer's engine menu (cloud one reply / multi-step / local / harness).
    bar_->add_menu_action(QStringLiteral("engine"), tr("ENGINE"), message_panel_->context_bar()->engine_menu(), {},
                          tr("What the next message runs on"), 90);
    bar_->add_separator(80);
    bar_->add_action(QStringLiteral("templates"), tr("TEMPLATES"), {},
                     tr("Start a task from a template: parameters, engine, budget (NEW RUN)"), Role::Normal, 70);
    bar_->add_action(QStringLiteral("schedule"), tr("SCHEDULE"), {},
                     tr("Runs that start on their own, once, daily or at an interval"), Role::Normal, 60);
    export_menu_ = new QMenu(bar_);
    export_menu_->setStyleSheet(ui::ScreenCommandBar::menu_css());
    bar_->add_menu_action(QStringLiteral("export"), tr("EXPORT"), export_menu_, {},
                          tr("This conversation as Markdown, or every conversation as JSON"), 50);
    bar_->add_separator(40);
    bar_->add_toggle(QStringLiteral("left"), tr("RUNS RAIL"), true, QStringLiteral("Ctrl+B"),
                     tr("Show or hide the runs / conversations rail (Ctrl+B)"), 30);
    bar_->add_toggle(QStringLiteral("right"), tr("INSPECTOR"), true, QStringLiteral("Ctrl+Alt+B"),
                     tr("Show or hide the inspector: context, plan, activity, files (Ctrl+Alt+B)"), 30);
    more_menu_ = new QMenu(bar_);
    more_menu_->setStyleSheet(ui::ScreenCommandBar::menu_css());
    bar_->add_menu_action(QStringLiteral("more"), QStringLiteral("⋯"), more_menu_, {}, tr("More"), 20);
    bar_->set_overflow_text(tr("More"));
}

void ChatModeScreen::refresh_engine_chip() {
    ComposerContextBar* cb = message_panel_->context_bar();
    const QString label = cb->engine_label();
    QString shown = tr("ENGINE · %1").arg(label.toUpper());
    shown.replace(QLatin1Char('&'), QStringLiteral("&&"));
    bar_->set_action_text(QStringLiteral("engine"), shown);
    bar_->set_action_tooltip(QStringLiteral("engine"),
                             tr("What the next message runs on: %1 — %2").arg(label, cb->budget_summary()));
    status_bar_->set_next_engine(label);
    const bool harness = cb->engine() == QLatin1String(run_view::kHarnessEngineId);
    if (harness_badge_ && harness_badge_->isVisibleTo(bar_->right_area()) != harness)
        harness_badge_->setVisible(harness);
}

void ChatModeScreen::refresh_title_field() {
    if (bar_->command_field() && bar_->command_field()->hasFocus())
        return; // being edited
    bar_->set_command_text(message_panel_->session_title());
}

void ChatModeScreen::commit_title(const QString& text) {
    const QString title = text.simplified();
    if (title.isEmpty() || title == message_panel_->session_title()) {
        refresh_title_field();
        return;
    }
    if (active_session_uuid_.isEmpty()) {
        status_bar_->bar()->show_notice(tr("The conversation is created with its first message — name it then."));
        refresh_title_field();
        return;
    }
    on_rename_session(active_session_uuid_, title.left(120));
    message_panel_->focus_composer();
}

void ChatModeScreen::on_bar_action(const QString& id) {
    if (id == QLatin1String("new"))
        start_new_conversation();
    else if (id == QLatin1String("templates"))
        open_new_run_dialog();
    else if (id == QLatin1String("schedule"))
        open_schedules_dialog();
}

void ChatModeScreen::wire_signals() {
    // Command bar.
    connect(back_btn_, &QPushButton::clicked, this, &ChatModeScreen::exit_requested);
    connect(bar_, &ui::ScreenCommandBar::action_triggered, this, &ChatModeScreen::on_bar_action);
    connect(bar_, &ui::ScreenCommandBar::action_toggled, this, [this](const QString& id, bool) {
        if (id == QLatin1String("left"))
            toggle_left();
        else if (id == QLatin1String("right"))
            toggle_right();
    });
    connect(bar_, &ui::ScreenCommandBar::command_submitted, this, &ChatModeScreen::commit_title);
    connect(bar_, &ui::ScreenCommandBar::command_cancelled, this, [this]() {
        refresh_title_field();
        message_panel_->focus_composer();
    });
    connect(bar_->command_field(), &QLineEdit::editingFinished, this, [this]() {
        if (!bar_->command_field()->hasFocus())
            refresh_title_field(); // left without Enter: the title stays as it was
    });
    // The engine chip (the composer's engine menu) follows the choice; a harness shows its model.
    ComposerContextBar* cb = message_panel_->context_bar();
    harness_badge_ = new ui::ModelBadge(QString::fromLatin1(ai_chat::kFeatureChatModeHarness), bar_->right_area());
    harness_badge_->set_compact(true);
    harness_badge_->setVisible(false);
    if (auto* rl = qobject_cast<QHBoxLayout*>(bar_->right_area()->layout()))
        rl->addWidget(harness_badge_);
    connect(cb, &ComposerContextBar::engine_changed, this, [this](const QString&) { refresh_engine_chip(); });
    connect(cb, &ComposerContextBar::run_as_task_toggled, this, [this](bool) { refresh_engine_chip(); });
    connect(cb, &ComposerContextBar::context_changed, this, [this]() { refresh_engine_chip(); });
    connect(&ai_chat::ModelRouter::instance(), &ai_chat::ModelRouter::routes_changed, this,
            [this]() { refresh_engine_chip(); });
    connect(export_menu_, &QMenu::aboutToShow, this, [this]() {
        export_menu_->clear();
        QAction* md = export_menu_->addAction(tr("This conversation as Markdown…"));
        md->setEnabled(!message_panel_->transcript()->is_empty());
        connect(md, &QAction::triggered, this, [this]() { export_markdown(); });
        QAction* all = export_menu_->addAction(tr("Every conversation as JSON…"));
        connect(all, &QAction::triggered, this, [this]() { session_panel_->export_conversations(); });
    });
    connect(more_menu_, &QMenu::aboutToShow, this, [this]() {
        more_menu_->clear();
        connect(more_menu_->addAction(tr("Rename the conversation\tF2")), &QAction::triggered, this,
                [this]() { bar_->focus_command_field(true); });
        connect(more_menu_->addAction(tr("New conversation\tCtrl+N")), &QAction::triggered, this,
                [this]() { start_new_conversation(); });
        connect(more_menu_->addAction(tr("Message box\tCtrl+Shift+L")), &QAction::triggered, this,
                [this]() { message_panel_->focus_composer(); });
        more_menu_->addSeparator();
        connect(more_menu_->addAction(tr("Runs\tAlt+1")), &QAction::triggered, this,
                [this]() { focus_left(QStringLiteral("runs")); });
        connect(more_menu_->addAction(tr("Conversations")), &QAction::triggered, this,
                [this]() { focus_left(QStringLiteral("conversations")); });
        connect(more_menu_->addAction(tr("Agent memory")), &QAction::triggered, this,
                [this]() { focus_left(QStringLiteral("memory")); });
        connect(more_menu_->addAction(tr("MCP servers")), &QAction::triggered, this,
                [this]() { focus_left(QStringLiteral("mcp")); });
        connect(more_menu_->addAction(tr("The conversation\tAlt+2")), &QAction::triggered, this,
                [this]() { message_panel_->focus_transcript(); });
        connect(more_menu_->addAction(tr("Inspector\tAlt+3")), &QAction::triggered, this,
                [this]() { focus_right(QString()); });
        more_menu_->addSeparator();
        connect(more_menu_->addAction(tr("Back to the workspace\tF9 / Esc")), &QAction::triggered, this,
                &ChatModeScreen::exit_requested);
    });

    // Draft + scroll persistence — debounced through ScreenStateManager.
    const auto notify = [this]() { fincept::ScreenStateManager::instance().notify_changed(this); };
    connect(message_panel_, &ChatMessagePanel::draft_changed, this, notify);
    connect(message_panel_, &ChatMessagePanel::scroll_changed, this, notify);
    connect(message_panel_, &ChatMessagePanel::mode_toggled, this, [notify](StreamMode) { notify(); });
    connect(message_panel_, &ChatMessagePanel::profile_changed, this, notify);
    connect(left_, &ChatModeLeftRail::section_changed, this, [notify](const QString&) { notify(); });
    connect(agent_panel_, &ChatAgentPanel::page_changed, this, [notify](const QString&) { notify(); });

    // Session panel -> screen
    connect(session_panel_, &ChatSessionPanel::session_selected, this, &ChatModeScreen::on_session_selected);
    connect(session_panel_, &ChatSessionPanel::new_session_requested, this, &ChatModeScreen::on_new_session);
    connect(session_panel_, &ChatSessionPanel::delete_session_requested, this, &ChatModeScreen::on_delete_session);
    connect(session_panel_, &ChatSessionPanel::rename_session_requested, this, &ChatModeScreen::on_rename_session);
    connect(session_panel_, &ChatSessionPanel::exit_chat_mode_requested, this, &ChatModeScreen::exit_requested);

    // Message panel -> screen
    connect(message_panel_, &ChatMessagePanel::send_requested, this, &ChatModeScreen::on_send_requested);
    connect(message_panel_, &ChatMessagePanel::answer_requested, this, &ChatModeScreen::on_answer_requested);
    connect(message_panel_, &ChatMessagePanel::stop_requested, this, [](const QString& run_id) {
        // A cloud reply detaches (the server finishes and charges it; the run recovers
        // its answer); other engines stop.
        agt::TaskRunService::instance().cancel_run(run_id);
    });
    connect(message_panel_, &ChatMessagePanel::retry_load_requested, this, &ChatModeScreen::reload_active_session);
    // A trade decided through the server: the server wrote the outcome into the
    // conversation (it moves up the list).
    connect(message_panel_, &ChatMessagePanel::proposal_decided, this,
            [this](const QString&, const QString&) { session_panel_->refresh_sessions(); });
    connect(message_panel_, &ChatMessagePanel::session_title_changed, this, [this](const QString&) { refresh_title_field(); });
    connect(message_panel_, &ChatMessagePanel::last_response_changed, status_bar_, &RunStatusBar::set_last_response_ms);
    connect(message_panel_, &ChatMessagePanel::run_open_requested, this, [this](const QString& id) {
        run_list_->reveal_run(id);
        open_run(id);
    });
    connect(message_panel_, &ChatMessagePanel::run_selection_requested, this, &ChatModeScreen::set_current_run);
    connect(message_panel_, &ChatMessagePanel::templates_requested, this, [this]() { open_new_run_dialog(); });
    connect(message_panel_, &ChatMessagePanel::template_requested, this,
            [this](const QString& id) { open_new_run_dialog(id); });
    connect(message_panel_, &ChatMessagePanel::symbol_open_requested, this, [this](const QString& symbol) {
        // Chat Mode covers the dock: leave it, then the window opens the symbol.
        emit exit_requested();
        EventBus::instance().publish(QStringLiteral("nav.open_symbol"),
                                     {{QStringLiteral("screen_id"), QStringLiteral("equity_research")},
                                      {QStringLiteral("symbol"), symbol}});
    });
    connect(message_panel_, &ChatMessagePanel::code_open_requested, this, &ChatModeScreen::open_code_in_code_tab);
    connect(message_panel_, &ChatMessagePanel::leave_requested, this, &ChatModeScreen::exit_requested);
    connect(message_panel_, &ChatMessagePanel::escape_pressed, this, &ChatModeScreen::exit_requested);

    // Runs -> transcript. Every signal carries its run id: the panel renders only
    // the run it is attached to, so windows never see each other's replies.
    auto& runs = agt::TaskRunService::instance();
    connect(&runs, &agt::TaskRunService::run_event, message_panel_, &ChatMessagePanel::on_run_event);
    connect(&runs, &agt::TaskRunService::run_event, this, &ChatModeScreen::on_run_event);
    connect(&runs, &agt::TaskRunService::run_text_delta, message_panel_, &ChatMessagePanel::on_run_text_delta);
    connect(&runs, &agt::TaskRunService::run_updated, message_panel_, &ChatMessagePanel::on_run_updated);
    connect(&runs, &agt::TaskRunService::run_added, this, &ChatModeScreen::on_run_added);
    connect(&runs, &agt::TaskRunService::run_finished, this, &ChatModeScreen::on_run_finished);
    connect(&runs, &agt::TaskRunService::runs_reset, this, [this]() {
        // A run link that arrived before the runs did (open_run_link) is opened now.
        if (!pending_run_link_.isEmpty()) {
            const QString link = pending_run_link_;
            open_run_link(link);
            if (!run_view::resolve_run_ref(agt::TaskRunService::instance().run_ids(), link).isEmpty())
                return;
        }
        // Persisted runs arrived after the conversation loaded (start-up): a run
        // that waits for an answer in this conversation is shown now.
        if (message_panel_->attached_run().isEmpty() && !active_session_uuid_.isEmpty()) {
            const QString open = agt::TaskRunService::instance().open_run_for_session(active_session_uuid_);
            if (!open.isEmpty()) {
                message_panel_->attach_run(open, /*user_bubble_shown=*/true);
                set_current_run(open);
            }
        }
    });

    // The account balance (from a turn's `finish`, or /user/profile).
    connect(&ChatModeService::instance(), &ChatModeService::credit_balance_changed, message_panel_,
            &ChatMessagePanel::set_credits);
    connect(&ChatModeService::instance(), &ChatModeService::insufficient_credits, message_panel_,
            &ChatMessagePanel::on_insufficient_credits);

    // The app-level tool bridge: its state is in the status bar / CONTEXT; refusals in the transcript.
    auto& bridge = TerminalToolBridge::instance();
    connect(&bridge, &TerminalToolBridge::bridge_error, this, [this](const QString& message) {
        if (!isVisible() || message == last_bridge_notice_)
            return; // the same refusal again (the bridge retries on every visit)
        last_bridge_notice_ = message;
        message_panel_->add_notice(message);
    });
    connect(&bridge, &TerminalToolBridge::tools_status_changed, this, [this](const ToolBridgeStatus& status) {
        if (status.state == ToolBridgeStatus::State::Usable || status.state == ToolBridgeStatus::State::Unconfirmed)
            last_bridge_notice_.clear(); // working again: a later refusal is news again
    });

    // RUNS rail and inspector.
    connect(run_list_, &RunListPanel::run_selected, this, &ChatModeScreen::set_current_run);
    connect(run_list_, &RunListPanel::run_activated, this, &ChatModeScreen::open_run);
    connect(run_list_, &RunListPanel::new_run_requested, this, [this]() { open_new_run_dialog(); });
    connect(run_list_, &RunListPanel::schedules_requested, this, &ChatModeScreen::open_schedules_dialog);
    connect(agent_panel_, &ChatAgentPanel::run_selection_requested, this, &ChatModeScreen::set_current_run);
    connect(agent_panel_, &ChatAgentPanel::run_open_requested, this, &ChatModeScreen::open_run);
    // An empty FILES page starts a task; a hand-off opens its screen.
    connect(agent_panel_, &ChatAgentPanel::new_run_requested, this, [this]() { open_new_run_dialog(); });
    connect(agent_panel_, &ChatAgentPanel::open_screen_requested, this, [this](const QString& screen_id) {
        // Leave Chat Mode (it covers the dock), then let the window show the screen.
        emit exit_requested();
        EventBus::instance().publish(QStringLiteral("nav.switch_screen"), {{QStringLiteral("screen_id"), screen_id}});
    });
    for (ChatAgentPanel* p : {agent_panel_, left_->agent()})
        connect(p, &ChatAgentPanel::mcp_state_changed, status_bar_, &RunStatusBar::set_mcp_state);
    // The run the inspector shows was deleted (here or in another window): show
    // the run RUNS selected in its place, or nothing — never a deleted run.
    connect(&runs, &agt::TaskRunService::run_updated, this, [this](const QString& id) {
        if (id == current_run_id_ && !agt::TaskRunService::instance().find_run(id))
            set_current_run(run_list_->selected_run() == id ? QString() : run_list_->selected_run());
    });
    // The chat service failed to list the conversations: one banner with RETRY.
    connect(message_panel_, &ChatMessagePanel::service_retry_requested, this, [this]() {
        session_panel_->refresh_sessions();
        load_recent_session(/*retry=*/true);
    });

    // Rails: remember the sizes the user drags to.
    connect(split_, &QSplitter::splitterMoved, this, [this](int, int) { sizes_timer_->start(); });
    connect(sizes_timer_, &QTimer::timeout, this, [this]() { save_rail_settings(); });

    refresh_engine_chip();
}

// ── Rails ────────────────────────────────────────────────────────────────────

void ChatModeScreen::apply_rails() {
    if (!split_)
        return;
    // Narrow windows: the inspector goes first, then the runs rail, so the document
    // keeps a readable width (about 64 characters of the user's font).
    const QFontMetrics fm(ui::fonts::font(0));
    const int centre_min = fm.averageCharWidth() * 64;
    const int left_min = std::max(left_->minimumSizeHint().width(), ui::metrics::width(220));
    const int right_min = std::max(agent_panel_->minimumSizeHint().width(), ui::metrics::width(260));
    const int w = width();
    bool right_fits = w >= centre_min + right_min + (left_user_hidden_ ? 0 : left_min);
    bool left_fits = w >= centre_min + left_min;
    if (right_fits)
        right_forced_ = false;
    if (left_fits)
        left_forced_ = false;
    const bool show_left = !left_user_hidden_ && (left_fits || left_forced_);
    const bool show_right = !right_user_hidden_ && (right_fits || right_forced_);
    if (left_->isVisibleTo(split_) != show_left)
        left_->setVisible(show_left);
    if (agent_panel_->isVisibleTo(split_) != show_right)
        agent_panel_->setVisible(show_right);
    bar_->set_action_checked(QStringLiteral("left"), show_left);
    bar_->set_action_checked(QStringLiteral("right"), show_right);
    if (!sizes_restored_ && w > 0) {
        sizes_restored_ = true;
        const int lw = saved_sizes_.size() == 3 && saved_sizes_.at(0) > 0 ? saved_sizes_.at(0) : ui::metrics::width(290);
        const int rw = saved_sizes_.size() == 3 && saved_sizes_.at(2) > 0 ? saved_sizes_.at(2) : ui::metrics::width(330);
        split_->setSizes({lw, std::max(centre_min, w - lw - rw), rw});
    }
}

void ChatModeScreen::toggle_left() {
    const bool shown = left_->isVisibleTo(split_);
    left_user_hidden_ = shown;
    left_forced_ = !shown;
    apply_rails();
    if (!shown)
        left_->focus_section();
    else
        message_panel_->focus_composer();
    save_rail_settings();
}

void ChatModeScreen::toggle_right() {
    const bool shown = agent_panel_->isVisibleTo(split_);
    right_user_hidden_ = shown;
    right_forced_ = !shown;
    apply_rails();
    if (!shown)
        agent_panel_->focus_page();
    else
        message_panel_->focus_composer();
    save_rail_settings();
}

void ChatModeScreen::focus_left(const QString& section) {
    if (!left_->isVisibleTo(split_)) {
        left_user_hidden_ = false;
        left_forced_ = true;
        apply_rails();
    }
    if (!section.isEmpty())
        left_->show_section(section);
    left_->focus_section();
}

void ChatModeScreen::focus_right(const QString& page) {
    if (!agent_panel_->isVisibleTo(split_)) {
        right_user_hidden_ = false;
        right_forced_ = true;
        apply_rails();
    }
    if (!page.isEmpty())
        agent_panel_->show_page(page);
    agent_panel_->focus_page();
}

void ChatModeScreen::save_rail_settings() {
    QSettings s;
    if (left_->isVisibleTo(split_) && agent_panel_->isVisibleTo(split_)) {
        saved_sizes_ = split_->sizes();
        QStringList v;
        for (int x : std::as_const(saved_sizes_))
            v << QString::number(x);
        s.setValue(QLatin1String(kCmsSizesKey), v.join(QLatin1Char(',')));
    }
    s.setValue(QLatin1String(kCmsHiddenKey), QStringLiteral("%1,%2").arg(left_user_hidden_ ? 1 : 0).arg(right_user_hidden_ ? 1 : 0));
}

void ChatModeScreen::restore_rail_settings() {
    QSettings s;
    saved_sizes_.clear();
    for (const QString& part : s.value(QLatin1String(kCmsSizesKey)).toString().split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        bool ok = false;
        const int v = part.toInt(&ok);
        if (ok)
            saved_sizes_ << v;
    }
    if (saved_sizes_.size() != 3)
        saved_sizes_.clear();
    const QStringList hidden = s.value(QLatin1String(kCmsHiddenKey)).toString().split(QLatin1Char(','));
    left_user_hidden_ = hidden.value(0) == QLatin1String("1");
    right_user_hidden_ = hidden.value(1) == QLatin1String("1");
}

// ── Export ───────────────────────────────────────────────────────────────────

void ChatModeScreen::export_markdown() {
    const QString text = message_panel_->transcript()->to_markdown();
    if (text.trimmed().isEmpty())
        return;
    const QString title = message_panel_->session_title().isEmpty() ? tr("Chat Mode conversation")
                                                                    : message_panel_->session_title();
    // Non-modal: open() returns at once; the choice arrives through fileSelected.
    auto* dlg = new QFileDialog(this, tr("Export the conversation"), cms_file_stem(title) + QStringLiteral(".md"),
                                tr("Markdown (*.md);;Text (*.txt)"));
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setAcceptMode(QFileDialog::AcceptSave);
    const QString body = QStringLiteral("# %1\n\n").arg(title) + text;
    QPointer<ChatModeScreen> self(this);
    connect(dlg, &QFileDialog::fileSelected, this, [self, body](const QString& path) {
        if (path.isEmpty())
            return;
        // Written on a worker, atomically.
        QThreadPool::globalInstance()->start([self, body, path]() {
            QSaveFile f(path);
            bool ok = f.open(QIODevice::WriteOnly | QIODevice::Text);
            if (ok)
                ok = f.write(body.toUtf8()) >= 0 && f.commit();
            const QString name = QFileInfo(path).fileName();
            QMetaObject::invokeMethod(
                QCoreApplication::instance(),
                [self, ok, name, path]() {
                    if (!ok)
                        LOG_WARN("ChatModeScreen", QStringLiteral("Conversation export failed path=%1").arg(log_path(path)));
                    if (self)
                        self->status_bar_->bar()->show_notice(ok ? tr("Exported to %1").arg(name)
                                                                 : tr("The conversation could not be written to %1")
                                                                       .arg(name),
                                                              {}, {}, 7000,
                                                              ok ? ui::ScreenStatusBar::Tone::Ok
                                                                 : ui::ScreenStatusBar::Tone::Danger);
                },
                Qt::QueuedConnection);
        });
    });
    dlg->open();
}

// ── OPEN IN CODE TAB (as AI Chat) ────────────────────────────────────────────

void ChatModeScreen::open_code_in_code_tab(const QString& code) {
    // A one-cell notebook in the terminal's own files folder, opened in the Code tab
    // (opened only — nothing runs).
    const QString dir = QDir(AppPaths::files()).filePath(QStringLiteral("chat_mode"));
    QDir().mkpath(dir);
    const QString path = QDir(dir).filePath(
        QStringLiteral("snippet_%1.ipynb").arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss_zzz"))));
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly) || f.write(screens::aichat::notebook_json(code)) < 0 || !f.commit()) {
        status_bar_->bar()->show_notice(tr("Could not create the notebook: %1").arg(f.errorString()), QString(), {},
                                        6000, ui::ScreenStatusBar::Tone::Danger);
        return;
    }
    LOG_INFO("ChatModeScreen", QString("Opened a code block in the Code tab (%1 characters)").arg(code.size()));
    // Chat Mode covers the window: leave it (the WORKSPACE path) so the Code tab is seen.
    emit exit_requested();
    EventBus::instance().publish(QStringLiteral("nav.switch_screen"),
                                 QVariantMap{{"screen_id", QStringLiteral("code_editor")},
                                             {"section", QStringLiteral("notebook:") + path}});
}

// ── Show / hide — lifecycle ──────────────────────────────────────────────────

void ChatModeScreen::set_view_counted(bool visible) {
    if (visible == view_counted_)
        return;
    view_counted_ = visible;
    // Runs notify only when no Chat Mode is in view; the tool bridge stays up while
    // one is (and, independently, while a run is going).
    agt::TaskRunService::instance().set_view_visible(visible);
    TerminalToolBridge::instance().set_view_visible(visible);
}

void ChatModeScreen::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    apply_rails();
}

void ChatModeScreen::keyPressEvent(QKeyEvent* e) {
    // Esc that nothing inside used (popups, inline forms, searches close first):
    // back to the workspace.
    if (e->key() == Qt::Key_Escape && e->modifiers() == Qt::NoModifier) {
        emit exit_requested();
        return;
    }
    QWidget::keyPressEvent(e);
}

bool ChatModeScreen::eventFilter(QObject* watched, QEvent* e) {
    // Esc in the title field: keep the title as it was, back to the message box.
    if (bar_ && watched == bar_->command_field() && e->type() == QEvent::KeyPress) {
        const auto* ke = static_cast<QKeyEvent*>(e);
        if (ke->key() == Qt::Key_Escape) {
            bar_->set_command_text(message_panel_->session_title());
            message_panel_->focus_composer();
            return true;
        }
    }
    return QWidget::eventFilter(watched, e);
}

void ChatModeScreen::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    apply_rails();
    LOG_DEBUG("ChatModeScreen", "Entered chat mode");
    set_view_counted(true);
    session_panel_->refresh_sessions();
    refresh_engine_chip();

    QPointer<ChatModeScreen> self = this;
    ChatModeService::instance().get_credits([self](bool ok, double credits, QString) {
        if (ok)
            ChatModeService::instance().report_credit_balance(credits);
        if (self && ok)
            self->message_panel_->set_credits(credits);
    });
    refresh_stats();
    // Investment profiles for the composer's picker (the account's default is preselected).
    ChatModeService::instance().list_profiles([self](bool ok, QVector<AgentProfile> profiles, QString err) {
        if (!self)
            return;
        if (!ok) {
            LOG_DEBUG("ChatModeScreen", "Profiles load failed: " + err); // ChatModeService logs the failure
            return;
        }
        self->message_panel_->set_profiles(profiles);
    });
    // Restore the most recent conversation (a brand-new profile keeps the start page).
    if (active_session_uuid_.isEmpty())
        load_recent_session(/*retry=*/false);
}

void ChatModeScreen::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    // Leaving Chat Mode (or locking the window) does not touch a reply in progress:
    // it belongs to TaskRunService and the tool bridge stays up while it runs.
    set_view_counted(false);
    LOG_DEBUG("ChatModeScreen", "Exited chat mode");
}

void ChatModeScreen::load_recent_session(bool retry) {
    LOG_DEBUG("ChatModeScreen", retry ? "Retrying the conversation list" : "No active session -- loading most recent");
    QPointer<ChatModeScreen> self = this;
    ChatModeService::instance().list_sessions(1, [self](bool ok, QVector<ChatSession> sessions, QString err) {
        if (!self)
            return;
        if (!ok) {
            LOG_DEBUG("ChatModeScreen", "List sessions failed: " + err); // ChatModeService logs the failure
            // One banner with RETRY, replaced in place: every visit retries.
            self->last_load_notice_ = tr("Could not load your conversations: %1").arg(err);
            self->message_panel_->show_service_problem(self->last_load_notice_);
            return;
        }
        self->last_load_notice_.clear(); // loaded: a later failure is news again
        self->message_panel_->clear_service_problem();
        if (sessions.isEmpty() || !self->active_session_uuid_.isEmpty())
            return;
        self->on_session_selected(sessions.first().uuid);
    });
}

void ChatModeScreen::refresh_stats() {
    QPointer<ChatModeScreen> self = this;
    ChatModeService::instance().get_stats([self](bool ok, ChatStats stats, QString) {
        if (self && ok)
            self->session_panel_->update_stats(stats);
    });
}

// ── Session actions ──────────────────────────────────────────────────────────

void ChatModeScreen::on_session_selected(const QString& uuid) {
    LOG_DEBUG("ChatModeScreen", QString("Session selected: %1").arg(uuid.left(8)));
    active_session_uuid_ = uuid;
    message_panel_->set_session_id(uuid);
    session_panel_->set_active_session(uuid);
    fincept::ScreenStateManager::instance().notify_changed(this);
    // Never leave the previous conversation's transcript under the new selection.
    message_panel_->clear_messages();
    message_panel_->set_session_title({});
    set_current_run(agt::TaskRunService::instance().latest_run_for_session(uuid));
    ChatModeService::instance().activate_session(uuid, [](bool, QString) {});
    reload_active_session();
}

void ChatModeScreen::reload_active_session() {
    const QString uuid = active_session_uuid_;
    if (uuid.isEmpty())
        return;
    const int seq = ++load_seq_;
    QPointer<ChatModeScreen> self = this;
    ChatModeService::instance().load_session(
        uuid, [self, uuid, seq](bool ok, ChatSession session, QVector<ChatMessage> messages, QString err, int) {
            if (!self)
                return;
            // Only the newest load for the conversation now selected may fill the transcript.
            if (self->active_session_uuid_ != uuid || seq != self->load_seq_)
                return;
            if (!ok) {
                LOG_WARN("ChatModeScreen", "Load session failed: " + err);
                self->message_panel_->show_load_error(err);
                // A reply in progress here is still shown (and still lands in the conversation).
                const QString open = agt::TaskRunService::instance().open_run_for_session(uuid);
                if (!open.isEmpty())
                    self->message_panel_->attach_run(open, /*user_bubble_shown=*/false);
                return;
            }
            self->message_panel_->set_session_title(session.title);
            self->message_panel_->load_messages(messages);
            self->attach_session_run(messages);
        });
}

void ChatModeScreen::attach_session_run(const QVector<ChatMessage>& messages) {
    auto& svc = agt::TaskRunService::instance();
    const QString open = svc.open_run_for_session(active_session_uuid_);
    if (!open.isEmpty()) {
        const agt::TaskRun* r = svc.find_run(open);
        // The server stores the user's message before the agent runs, so a reply in
        // progress usually has its message in the transcript already. A paused run's
        // turn is stored whole (with its question).
        const int idx = cms_last_user_message(messages, r->prompt);
        const bool planned = r->executor_id() == QLatin1String(agt::kExecutorCloudPlan);
        const bool shown = r->status == agt::RunStatus::WaitingInput || r->status == agt::RunStatus::Paused ||
                           (idx >= 0 && (planned || idx == static_cast<int>(messages.size()) - 1));
        message_panel_->attach_run(open, shown);
        set_current_run(open);
        return;
    }
    // A run that ended moments ago may not be stored yet (or failed before the server
    // stored anything): show its outcome from local state.
    const QString latest = svc.latest_run_for_session(active_session_uuid_);
    const agt::TaskRun* r = latest.isEmpty() ? nullptr : svc.find_run(latest);
    if (!r || r->finished_at_ms <= 0 || QDateTime::currentMSecsSinceEpoch() - r->finished_at_ms > kRecentRunMs)
        return;
    const int idx = cms_last_user_message(messages, r->prompt);
    if (idx >= 0 && cms_has_answer_after(messages, idx))
        return; // the stored conversation has it
    if (r->status == agt::RunStatus::Completed && r->final_text.trimmed().isEmpty())
        return;
    message_panel_->attach_run(latest, /*user_bubble_shown=*/idx >= 0);
}

void ChatModeScreen::on_new_session() {
    QPointer<ChatModeScreen> self = this;
    // Untranslated on purpose — see kChatDefaultSessionTitle.
    ChatModeService::instance().create_session(QString::fromLatin1(kChatDefaultSessionTitle),
                                               [self](bool ok, ChatSession session, QString err) {
                                                   if (!self)
                                                       return;
                                                   if (!ok) {
                                                       LOG_WARN("ChatModeScreen", "Create session failed: " + err);
                                                       self->message_panel_->add_notice(
                                                           tr("Could not create a conversation: %1").arg(err));
                                                       return;
                                                   }
                                                   self->session_panel_->refresh_sessions();
                                                   self->refresh_stats();
                                                   self->on_session_selected(session.uuid);
                                               });
}

void ChatModeScreen::start_new_conversation() {
    // Nothing is created yet: the first message creates the conversation
    // (ensure_active_session), so empty "New Conversation" rows do not pile up.
    active_session_uuid_.clear();
    ++load_seq_; // a load still in flight must not fill the fresh page
    message_panel_->set_session_id({});
    session_panel_->set_active_session({});
    message_panel_->show_start_page();
    set_current_run({});
    fincept::ScreenStateManager::instance().notify_changed(this);
    message_panel_->focus_composer();
}

void ChatModeScreen::on_delete_session(const QString& uuid) {
    QPointer<ChatModeScreen> self = this;
    ChatModeService::instance().delete_session(uuid, [self, uuid](bool ok, QString err) {
        if (!self)
            return;
        if (!ok) {
            LOG_WARN("ChatModeScreen", "Delete failed: " + err);
            self->message_panel_->add_notice(tr("Could not delete the conversation: %1").arg(err));
            return;
        }
        self->session_panel_->refresh_sessions();
        self->refresh_stats();
        if (self->active_session_uuid_ != uuid)
            return; // another conversation is shown: leave it
        self->active_session_uuid_.clear();
        self->message_panel_->set_session_id({});
        self->message_panel_->clear_messages();
        ChatModeService::instance().list_sessions(1, [self](bool list_ok, QVector<ChatSession> sessions, QString) {
            if (!self)
                return;
            if (!list_ok || sessions.isEmpty()) {
                self->start_new_conversation();
                return;
            }
            self->on_session_selected(sessions.first().uuid);
        });
    });
}

void ChatModeScreen::on_rename_session(const QString& uuid, const QString& title) {
    QPointer<ChatModeScreen> self = this;
    ChatModeService::instance().rename_session(uuid, title, [self, uuid, title](bool ok, QString err) {
        if (!self)
            return;
        if (!ok) {
            LOG_WARN("ChatModeScreen", "Rename failed: " + err);
            self->message_panel_->add_notice(tr("Could not rename the conversation: %1").arg(err));
            self->refresh_title_field();
        } else if (uuid == self->active_session_uuid_) {
            self->message_panel_->set_session_title(title); // the bar shows the open conversation's name
        }
        self->session_panel_->refresh_sessions();
    });
}

// ── Send message ─────────────────────────────────────────────────────────────

agt::RunRequest ChatModeScreen::chat_request(const QString& text, StreamMode mode, bool compose) const {
    agt::RunRequest req;
    ComposerContextBar* bar = message_panel_->context_bar();
    req.prompt = compose && bar ? bar->compose_prompt(text) : text;
    req.engine = bar ? bar->engine() : QString::fromLatin1(agt::kEngineCloud);
    req.plan_first = bar && bar->run_as_task();
    req.context = bar ? bar->context() : QJsonObject{};
    req.mode = mode == StreamMode::Deep ? QStringLiteral("deep") : QStringLiteral("lite");
    // The investment profile picked in the composer (0 = none) — read now, at send time.
    req.profile_id = message_panel_->selected_profile_id();
    req.origin = QString::fromLatin1(agt::kOriginChat);
    if (req.plan_first) {
        // The bar's budget: the local engine runs on the user's own model and has no credit limit.
        if (bar) {
            req.budget = bar->budget();
        } else {
            req.budget.max_credits = agt::kDefaultMaxCredits;
            req.budget.max_steps = agt::kDefaultMaxSteps;
        }
    }
    return req;
}

void ChatModeScreen::start_chat_run(agt::RunRequest request, const QString& session_uuid) {
    request.session_uuid = session_uuid;
    QString err;
    const QString run_id = agt::TaskRunService::instance().start_run(request, &err);
    // Bind only if the composer still waits for this send in this conversation (the
    // user may have switched meanwhile — the run goes on regardless).
    const bool ours = message_panel_->has_pending_send() && active_session_uuid_ == session_uuid;
    if (run_id.isEmpty()) {
        if (ours)
            message_panel_->fail_pending_send(err);
        return;
    }
    if (ours) {
        message_panel_->bind_pending_run(run_id);
        set_current_run(run_id);
    }
}

void ChatModeScreen::on_send_requested(const QString& message, StreamMode mode) {
    LOG_INFO("ChatModeScreen",
             QString("Chat message sent mode=%1 session=%2 query_chars=%3")
                 .arg(mode == StreamMode::Deep ? "deep" : "lite",
                      active_session_uuid_.isEmpty() ? QStringLiteral("-") : active_session_uuid_.left(8))
                 .arg(message.size()));
    const agt::RunRequest req = chat_request(message, mode, /*compose=*/true);
    QPointer<ChatModeScreen> self = this;
    ensure_active_session([self, req](const QString& uuid) {
        if (self)
            self->start_chat_run(req, uuid);
    });
}

void ChatModeScreen::on_answer_requested(const QString& answer, StreamMode mode) {
    LOG_INFO("ChatModeScreen", QString("Chat answer sent to paused run session=%1 answer_chars=%2")
                                   .arg(active_session_uuid_.isEmpty() ? QStringLiteral("-") : active_session_uuid_.left(8))
                                   .arg(answer.size()));
    auto& svc = agt::TaskRunService::instance();
    const QString open = svc.open_run_for_session(active_session_uuid_);
    if (const agt::TaskRun* r = open.isEmpty() ? nullptr : svc.find_run(open);
        r && r->status == agt::RunStatus::WaitingInput) {
        if (svc.answer(open, answer)) {
            message_panel_->bind_pending_run(open);
            set_current_run(open);
            return;
        }
    }
    // No local run waits here (the question came with the stored conversation, or
    // the terminal restarted): a new run that answers the server's paused run — or,
    // if nothing waits there either, goes out as an ordinary message.
    agt::RunRequest req = chat_request(answer, mode, /*compose=*/false);
    req.plan_first = false;
    req.budget = {};
    req.context.insert(QLatin1String(agt::kCtxAnswerToPause), true);
    QPointer<ChatModeScreen> self = this;
    ensure_active_session([self, req](const QString& uuid) {
        if (self)
            self->start_chat_run(req, uuid);
    });
}

void ChatModeScreen::ensure_active_session(std::function<void(const QString&)> then) {
    if (!active_session_uuid_.isEmpty()) {
        then(active_session_uuid_);
        return;
    }
    QPointer<ChatModeScreen> self = this;
    ChatModeService::instance().create_session(
        QString::fromLatin1(kChatDefaultSessionTitle),
        [self, then = std::move(then)](bool ok, ChatSession session, QString err) {
            if (!self)
                return;
            if (!ok) {
                LOG_WARN("ChatModeScreen", "Auto-create session failed: " + err);
                // The composer is locked on the pending send: release it with the reason.
                if (self->message_panel_->has_pending_send())
                    self->message_panel_->fail_pending_send(tr("Could not start a conversation: %1").arg(err));
                return;
            }
            if (self->active_session_uuid_.isEmpty()) {
                self->active_session_uuid_ = session.uuid;
                self->message_panel_->set_session_id(session.uuid);
                self->session_panel_->set_active_session(session.uuid);
            }
            self->session_panel_->refresh_sessions();
            self->refresh_stats();
            then(session.uuid);
        });
}

// ── Runs ─────────────────────────────────────────────────────────────────────

void ChatModeScreen::set_current_run(const QString& run_id) {
    current_run_id_ = run_id;
    agent_panel_->set_current_run(run_id);
    status_bar_->set_run(run_id);
    run_list_->select_run(run_id);
}

void ChatModeScreen::open_run(const QString& run_id) {
    const agt::TaskRun* r = agt::TaskRunService::instance().find_run(run_id);
    if (!r)
        return;
    if (!r->session_uuid.isEmpty() && r->session_uuid != active_session_uuid_)
        on_session_selected(r->session_uuid);
    set_current_run(run_id);
}

void ChatModeScreen::open_run_link(const QString& run_ref) {
    const QString ref = run_ref.trimmed();
    if (ref.isEmpty())
        return;
    focus_left(QStringLiteral("runs")); // RUNS: where the run is listed
    auto& svc = agt::TaskRunService::instance();
    if (!svc.is_loaded()) {
        // The persisted runs are still being read: the runs_reset handler opens it.
        pending_run_link_ = ref;
        svc.initialize_deferred();
        return;
    }
    pending_run_link_.clear();
    const QString id = run_view::resolve_run_ref(svc.run_ids(), ref);
    if (id.isEmpty()) {
        LOG_DEBUG("ChatModeScreen", QStringLiteral("Run link names no run ref=%1").arg(ref.left(8)));
        run_list_->show_notice(tr("That run no longer exists"));
        return;
    }
    run_list_->reveal_run(id); // a filter or search that hides it is cleared
    open_run(id);
}

bool ChatModeScreen::open_section(const QString& section_in, const QString& focus) {
    const QString section = section_in.trimmed().toLower();
    bool known = true;
    if (section.isEmpty() || section == QLatin1String("runs")) {
        focus_left(QStringLiteral("runs"));
    } else if (section == QLatin1String("conversations") || section == QLatin1String("chats")) {
        focus_left(QStringLiteral("conversations"));
    } else if (section == QLatin1String("memory") || section == QLatin1String("memories") ||
               section == QLatin1String("agents") || section == QLatin1String("agent")) {
        focus_left(QStringLiteral("memory"));
    } else if (section == QLatin1String("mcp")) {
        focus_left(QStringLiteral("mcp"));
    } else if (section == QLatin1String("schedules") || section == QLatin1String("schedule")) {
        open_schedules_dialog();
    } else if (section == QLatin1String("templates") || section == QLatin1String("template")) {
        open_new_run_dialog(focus);
        return true; // focus = the template, not a run
    } else if (section == QLatin1String("new")) {
        start_new_conversation();
        if (!focus.trimmed().isEmpty())
            message_panel_->set_draft_text(focus); // filled, never sent
        return true;
    } else if (section == QLatin1String("context") || section == QLatin1String("plan") ||
               section == QLatin1String("activity") || section == QLatin1String("files") ||
               section == QLatin1String("artefacts")) {
        focus_right(section == QLatin1String("artefacts") ? QStringLiteral("files") : section);
    } else {
        known = false;
    }
    if (!focus.trimmed().isEmpty())
        open_run_link(focus);
    return known;
}

void ChatModeScreen::on_run_added(const QString& run_id) {
    const agt::TaskRun* r = agt::TaskRunService::instance().find_run(run_id);
    if (!r || r->session_uuid.isEmpty() || r->session_uuid != active_session_uuid_)
        return;
    // Started in this conversation by another window (or the scheduler, or a RETRY):
    // show it here too. A send from this window is bound by start_chat_run() instead.
    if (message_panel_->has_pending_send() || !message_panel_->attached_run().isEmpty()) {
        const agt::TaskRun* attached = agt::TaskRunService::instance().find_run(message_panel_->attached_run());
        if (message_panel_->has_pending_send() || (attached && !agt::is_terminal(attached->status)))
            return;
    }
    message_panel_->attach_run(run_id, /*user_bubble_shown=*/false);
    set_current_run(run_id);
}

void ChatModeScreen::on_run_finished(const QString& run_id) {
    // A run of the open conversation the transcript never followed (resumed from the
    // RUNS / PLAN panel): show what it stored.
    const bool followed = message_panel_->attached_run() == run_id;
    message_panel_->on_run_finished(run_id);
    const agt::TaskRun* r = agt::TaskRunService::instance().find_run(run_id);
    if (r && !r->session_uuid.isEmpty() && r->session_uuid == active_session_uuid_ && isVisible()) {
        if (!followed && !message_panel_->has_pending_send()) {
            const agt::TaskRun* attached = agt::TaskRunService::instance().find_run(message_panel_->attached_run());
            if (!attached || agt::is_terminal(attached->status))
                reload_active_session();
        }
        session_panel_->refresh_sessions(); // message count / order
        refresh_stats();
    }
}

void ChatModeScreen::on_run_event(const QString& run_id, const agt::RunEvent& ev) {
    if (ev.kind != agt::RunEventKind::Status)
        return;
    const agt::TaskRun* r = agt::TaskRunService::instance().find_run(run_id);
    if (!r)
        return;
    // A run of the open conversation that was resumed (RESUME / RETRY in the RUNS or
    // PLAN panel) is not a new run, so nothing attached the transcript to it: reload
    // at the start of its next turn — that shows the stored steps and attaches.
    if (ev.data.contains(QStringLiteral("turn")) && !r->session_uuid.isEmpty() &&
        r->session_uuid == active_session_uuid_ && isVisible() && !message_panel_->has_pending_send() &&
        message_panel_->attached_run() != run_id) {
        const agt::TaskRun* attached = agt::TaskRunService::instance().find_run(message_panel_->attached_run());
        if (!attached || agt::is_terminal(attached->status))
            reload_active_session();
    }
    const QString title = ev.data.value(QStringLiteral("new_title")).toString();
    if (title.isEmpty())
        return;
    QString shown = title;
    // The server titles a new conversation after its first message. For a planned task
    // that is the planning request, and for a message sent with context it is the typed
    // text plus the appended context block. Name the conversation after the task instead.
    const bool planned = r->executor_id() == QLatin1String(agt::kExecutorCloudPlan);
    const bool composed = title.contains(QLatin1Char('\n')) || title.contains(QLatin1Char('\r'));
    if ((planned || composed) && !r->session_uuid.isEmpty()) {
        const QString task_title = r->title.simplified().left(80);
        if (!task_title.isEmpty() && task_title != title) {
            shown = task_title;
            QPointer<ChatModeScreen> self = this;
            ChatModeService::instance().rename_session(r->session_uuid, task_title, [self](bool, QString) {
                if (self && self->isVisible())
                    self->session_panel_->refresh_sessions();
            });
        }
    }
    if (r->session_uuid == active_session_uuid_) {
        message_panel_->set_session_title(shown); // the server auto-titled the conversation
        session_panel_->refresh_sessions();
    }
}

void ChatModeScreen::open_new_run_dialog(const QString& template_id) {
    if (new_run_dialog_) { // one per window: bring it back instead of stacking copies
        if (!template_id.isEmpty())
            new_run_dialog_->set_template(template_id);
        new_run_dialog_->raise();
        new_run_dialog_->activateWindow();
        return;
    }
    auto* dlg = new NewRunDialog(this);
    new_run_dialog_ = dlg;
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->set_session(QString()); // a NEW RUN gets its own conversation
    // The composer's context chips (symbol, portfolio, profile) pre-fill the form.
    if (ComposerContextBar* bar = message_panel_->context_bar())
        dlg->set_context(bar->context());
    if (!template_id.isEmpty())
        dlg->set_template(template_id);
    connect(dlg, &NewRunDialog::run_started, this, [this](const QString& run_id) {
        open_run(run_id);
        focus_left(QStringLiteral("runs")); // RUNS: where it is followed
    });
    connect(dlg, &NewRunDialog::schedule_requested, this, [this](const QJsonObject& draft) {
        open_schedules_dialog();
        if (schedules_dialog_)
            schedules_dialog_->start_new_schedule(draft);
    });
    dlg->show(); // non-modal: never exec() for run actions
}

void ChatModeScreen::open_schedules_dialog() {
    if (schedules_dialog_) {
        schedules_dialog_->raise();
        schedules_dialog_->activateWindow();
        return;
    }
    auto* dlg = new RunSchedulesDialog(this);
    schedules_dialog_ = dlg;
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    connect(dlg, &RunSchedulesDialog::run_started, this, [this](const QString& run_id) { open_run(run_id); });
    dlg->show();
}

// ── IStatefulScreen ──────────────────────────────────────────────────────────

QVariantMap ChatModeScreen::save_state() const {
    QVariantMap s{{"session_uuid", active_session_uuid_}};
    if (message_panel_) {
        s.insert("draft", message_panel_->draft_text());
        s.insert("scroll", message_panel_->scroll_position());
        s.insert("mode", message_panel_->current_mode() == StreamMode::Deep ? QStringLiteral("deep") : QStringLiteral("lite"));
        s.insert("profile_id", message_panel_->preferred_profile_id());
    }
    if (left_)
        s.insert("left_section", left_->section());
    if (agent_panel_)
        s.insert("right_page", agent_panel_->current_page());
    return s;
}

void ChatModeScreen::restore_state(const QVariantMap& state) {
    const QString uuid = state.value("session_uuid").toString();
    if (!uuid.isEmpty() && uuid != active_session_uuid_)
        on_session_selected(uuid);
    if (!message_panel_)
        return;
    if (state.contains("mode"))
        message_panel_->set_stream_mode(state.value("mode").toString() == QLatin1String("deep") ? StreamMode::Deep
                                                                                              : StreamMode::Lite);
    if (state.contains("profile_id"))
        message_panel_->set_preferred_profile_id(state.value("profile_id").toInt());
    const QString draft = state.value("draft").toString();
    if (!draft.isEmpty())
        message_panel_->set_draft_text(draft);
    if (state.contains("left_section"))
        left_->show_section(state.value("left_section").toString());
    if (state.contains("right_page"))
        agent_panel_->show_page(state.value("right_page").toString());
    // Scroll position is only meaningful after the conversation was built (on a
    // worker): deferred past the first layout passes.
    const int scroll = state.value("scroll", -1).toInt();
    if (scroll >= 0) {
        QPointer<ChatModeScreen> self = this;
        QTimer::singleShot(400, this, [self, scroll]() {
            if (self && self->message_panel_)
                self->message_panel_->set_scroll_position(scroll);
        });
    }
}

} // namespace fincept::chat_mode
