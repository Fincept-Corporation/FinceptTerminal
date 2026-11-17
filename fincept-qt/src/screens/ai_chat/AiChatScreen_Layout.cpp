// src/screens/ai_chat/AiChatScreen_Layout.cpp
//
// UI construction: the command bar, the status bar, the three zones and the wiring of the
// child widgets (transcript, sessions rail, context rail, composer, start page).
//
// Part of the partial-class split of AiChatScreen.cpp.

#include "core/agent/AgentRiskMarkers.h"
#include "core/events/EventBus.h"
#include "core/session/ScreenStateManager.h"
#include "screens/ai_chat/AiChatComposer.h"
#include "screens/ai_chat/AiChatContextRail.h"
#include "screens/ai_chat/AiChatScreen.h"
#include "screens/ai_chat/AiChatSessionRail.h"
#include "screens/ai_chat/AiChatStartPage.h"
#include "screens/ai_chat/AiChatStyle.h"
#include "screens/ai_chat/AiChatTranscript.h"
#include "services/llm/ModelRouter.h"
#include "ui/navigation/KeyRouting.h"
#include "ui/widgets/ModelBadge.h"
#include "ui/widgets/ScreenCommandBar.h"
#include "ui/widgets/ScreenStatusBar.h"
#include "ui/widgets/report/AddToReportButton.h"

#include <QDateTime>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QShortcut>
#include <QVBoxLayout>

namespace fincept::screens {

namespace st = aichat::style;
using SB = ui::ScreenStatusBar;
using CB = ui::ScreenCommandBar;

void AiChatScreen::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    build_command_bar();
    root->addWidget(bar_);

    body_ = new QWidget(this);
    auto* hl = new QHBoxLayout(body_);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(0);

    sessions_ = new aichat::AicSessionRail(body_);
    sessions_->setObjectName(QString::fromLatin1(st::kRailLeft));
    sessions_->setAttribute(Qt::WA_StyledBackground, true);
    hl->addWidget(sessions_);

    centre_ = new QWidget(body_);
    auto* cl = new QVBoxLayout(centre_);
    cl->setContentsMargins(0, 0, 0, 0);
    cl->setSpacing(0);
    transcript_ = new aichat::AicTranscriptView(aichat::AicTranscriptView::Density::Desk, centre_);
    transcript_->setObjectName(QString::fromLatin1(st::kTranscript));
    transcript_->set_report_source(tr("AI Chat"));
    cl->addWidget(transcript_, 1);
    // The start page and the transcript are never visible together; hidden widgets take
    // no room, so neither dictates the other's height.
    start_page_ = new aichat::AicStartPage(centre_);
    start_page_->hide();
    cl->addWidget(start_page_, 1);
    cl->addWidget(build_model_notice());
    composer_ = new aichat::AicComposer(centre_);
    cl->addWidget(composer_);
    hl->addWidget(centre_, 1);

    context_rail_ = new aichat::AicContextRail(body_);
    context_rail_->setObjectName(QString::fromLatin1(st::kRailRight));
    context_rail_->setAttribute(Qt::WA_StyledBackground, true);
    context_rail_->set_tool_policy(tools_on_, nav_on_);
    context_rail_->set_saved_prompts(saved_prompts_);
    hl->addWidget(context_rail_);

    root->addWidget(body_, 1);

    build_status_bar();
    root->addWidget(status_);

    composer_->set_saved_prompts(saved_prompts_);
    wire_children();

    // Screen keys (focus anywhere inside the tab). Ctrl+K stays the app's Search; no
    // reserved shell keys (F5, F9, F10, F11, Ctrl+1..9) are bound here.
    auto* new_sc = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_N), this);
    new_sc->setContext(Qt::WidgetWithChildrenShortcut);
    connect(new_sc, &QShortcut::activated, this, &AiChatScreen::on_new_session);
    auto* rail_sc = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_B), this);
    rail_sc->setContext(Qt::WidgetWithChildrenShortcut);
    connect(rail_sc, &QShortcut::activated, this, &AiChatScreen::toggle_sessions);
    // F2 = rename the session in place (the title field). A function key belongs to the
    // shell unless claimed for this tab.
    ui::claim_keys(this, {QKeySequence(Qt::Key_F2)});
    auto* rename_sc = new QShortcut(QKeySequence(Qt::Key_F2), this);
    rename_sc->setContext(Qt::WidgetWithChildrenShortcut);
    connect(rename_sc, &QShortcut::activated, this, &AiChatScreen::begin_rename);

    retranslateUi();
    apply_zones(/*force=*/true);
}

void AiChatScreen::build_command_bar() {
    bar_ = new CB(this);
    bar_->set_title(tr("AI CHAT"));
    // The session title, edited in place: Enter renames, Esc / leaving restores it.
    bar_->set_command_field(tr("Session title"), QStringLiteral("F2"));
    bar_->set_command_width_chars(34, 12);
    if (auto* f = bar_->command_field()) {
        f->setAccessibleName(tr("Session title"));
        f->setMaxLength(200);
        f->installEventFilter(this);
    }
    bar_->add_action(QStringLiteral("new"), tr("NEW CHAT"), QStringLiteral("Ctrl+N"), tr("Start a new chat (Ctrl+N)"),
                     CB::Role::Primary, 100);
    bar_->add_toggle(QStringLiteral("sessions"), tr("SESSIONS"), true, QStringLiteral("Ctrl+B"),
                     tr("Show or hide the sessions list (Ctrl+B)"), 45);
    bar_->add_separator(60);
    bar_->add_toggle(QStringLiteral("tools"), tr("TOOLS"), tools_on_, QString(),
                     tr("Let the model use the terminal's tools (quotes, news, portfolio, …) in this chat"), 70);
    bar_->add_toggle(QStringLiteral("nav"), tr("NAV"), nav_on_, QString(),
                     tr("Let the model switch tabs for you (navigation tools)"), 50);
    bar_->add_separator(60);

    auto* export_menu = new QMenu(this);
    export_menu->setStyleSheet(st::menu_sheet());
    connect(export_menu->addAction(tr("Save as Markdown…")), &QAction::triggered, this,
            [this]() { export_markdown(active_session_id_); });
    connect(export_menu->addAction(tr("Copy as Markdown")), &QAction::triggered, this,
            [this]() { copy_markdown(active_session_id_); });
    export_menu->addSeparator();
    ui::add_send_to_report_action(export_menu, tr("AI Chat"), [this]() {
        report::ReportPayload p;
        p.title = active_session_title_.isEmpty() ? tr("AI Chat") : active_session_title_;
        p.source = tr("AI Chat");
        p.as_of = QDateTime::currentDateTime();
        p.markdown = conversation_markdown(active_session_id_);
        return p;
    });
    bar_->add_menu_action(QStringLiteral("export"), tr("EXPORT"), export_menu, QString(),
                          tr("Save, copy or add this conversation to a report"), 65);

    auto* more = new QMenu(this);
    more->setStyleSheet(st::menu_sheet());
    connect(more->addAction(tr("Rename session  (F2)")), &QAction::triggered, this, &AiChatScreen::begin_rename);
    auto* pin = more->addAction(tr("Pin session"));
    pin->setObjectName(QStringLiteral("aicPinAction"));
    connect(pin, &QAction::triggered, this,
            [this]() { set_pinned(active_session_id_, !pinned_.contains(active_session_id_)); });
    more->addSeparator();
    connect(more->addAction(tr("Model settings…")), &QAction::triggered, this, []() { ui::open_llm_settings(); });
    more->addSeparator();
    auto* del = more->addAction(tr("Delete session…"));
    del->setProperty(core::agent::kAgentRiskProperty, QStringLiteral("delete"));
    connect(del, &QAction::triggered, this, [this]() { delete_session(active_session_id_); });
    connect(more, &QMenu::aboutToShow, this, [this, pin]() {
        pin->setText(pinned_.contains(active_session_id_) ? tr("Unpin session") : tr("Pin session"));
        pin->setEnabled(!active_session_id_.isEmpty());
    });
    bar_->add_menu_action(QStringLiteral("more"), QStringLiteral("⋯"), more, QString(),
                          tr("Rename, pin or delete this session"), 30);

    // Which model answers AI Chat (ModelRouter) — the picker — and what it costs / caps.
    auto* right = bar_->right_area();
    model_badge_ = new ui::ModelBadge(QString::fromLatin1(ai_chat::kFeatureAiChat), right);
    connect(model_badge_, &ui::ModelBadge::choice_changed, this, [this]() {
        model_notice_dismissed_ = false;
        update_stats();
    });
    model_hint_ = new QLabel(right);
    model_hint_->setObjectName(QString::fromLatin1(st::kDim));
    if (auto* rl = right->layout()) {
        rl->addWidget(model_badge_);
        rl->addWidget(model_hint_);
    }
    bar_->set_overflow_text(tr("More"));

    connect(bar_, &CB::action_triggered, this, [this](const QString& id) {
        if (id == QLatin1String("new"))
            on_new_session();
    });
    connect(bar_, &CB::action_toggled, this, [this](const QString& id, bool on) {
        if (id == QLatin1String("sessions")) {
            toggle_sessions();
            return;
        }
        if (id == QLatin1String("tools")) {
            tools_on_ = on;
            bar_->set_action_enabled(QStringLiteral("nav"), on);
        } else if (id == QLatin1String("nav")) {
            nav_on_ = on;
        } else {
            return;
        }
        context_rail_->set_tool_policy(tools_on_, nav_on_);
        ScreenStateManager::instance().notify_changed(this);
    });
    connect(bar_, &CB::command_submitted, this, [this](const QString& text) {
        rename_session(active_session_id_, text);
        composer_->focus_editor();
    });
    connect(bar_, &CB::command_cancelled, this, [this]() {
        bar_->set_command_text(active_session_title_);
        composer_->focus_editor();
    });
    connect(bar_, &CB::command_navigate, this, [this](int) { transcript_->setFocus(Qt::TabFocusReason); });
}

void AiChatScreen::build_status_bar() {
    status_ = new SB(this);
    status_->add_item(QStringLiteral("route"), SB::Segment::Left, QString(), SB::Tone::Normal, 100);
    status_->add_item(QStringLiteral("state"), SB::Segment::Left, QString(), SB::Tone::Ok, 95);
    status_->set_chip(QStringLiteral("state"), true);
    status_->add_item(QStringLiteral("tools"), SB::Segment::Left, QString(), SB::Tone::Dim, 40);
    status_->add_item(QStringLiteral("tokens"), SB::Segment::Right, QString(), SB::Tone::Normal, 80);
    status_->add_item(QStringLiteral("cost"), SB::Segment::Right, QString(), SB::Tone::Normal, 60);
    status_->add_item(QStringLiteral("last"), SB::Segment::Right, QString(), SB::Tone::Dim, 50);
    status_->add_item(QStringLiteral("asof"), SB::Segment::Right, QString(), SB::Tone::Dim, 30);
    status_->set_clock(true, /*seconds=*/false);
}

// "This conversation used X / Y" strip above the composer (hidden unless the
// conversation's last answer came from a different provider/model than the one now active).
QWidget* AiChatScreen::build_model_notice() {
    model_notice_ = new QFrame(centre_);
    model_notice_->setObjectName(QString::fromLatin1(st::kErrorBlock));
    model_notice_->setProperty("tone", QStringLiteral("accent"));
    model_notice_->setAttribute(Qt::WA_StyledBackground, true);
    auto* hl = new QHBoxLayout(model_notice_);
    hl->setContentsMargins(st::u(3), st::u(1), st::u(2), st::u(1));
    hl->setSpacing(st::u(2));
    model_notice_lbl_ = new QLabel(model_notice_);
    model_notice_lbl_->setObjectName(QString::fromLatin1(st::kBody));
    model_notice_lbl_->setWordWrap(true);
    hl->addWidget(model_notice_lbl_, 1);
    model_notice_btn_ = new QPushButton(model_notice_);
    model_notice_btn_->setObjectName(QString::fromLatin1(st::kBtn));
    model_notice_btn_->setCursor(Qt::PointingHandCursor);
    model_notice_btn_->setFixedHeight(st::button_h());
    connect(model_notice_btn_, &QPushButton::clicked, this, &AiChatScreen::on_use_conversation_model);
    hl->addWidget(model_notice_btn_);
    auto* close = new QPushButton(QStringLiteral("✕"), model_notice_);
    close->setObjectName(QString::fromLatin1(st::kIconBtn));
    close->setCursor(Qt::PointingHandCursor);
    close->setFixedSize(st::button_h(), st::button_h());
    close->setAccessibleName(tr("Dismiss the model notice"));
    close->setToolTip(tr("Keep using the current model for this conversation"));
    connect(close, &QPushButton::clicked, this, [this]() {
        model_notice_dismissed_ = true;
        update_model_notice();
    });
    hl->addWidget(close);
    model_notice_->hide();
    return model_notice_;
}

void AiChatScreen::wire_children() {
    using namespace aichat;
    // Transcript
    connect(transcript_, &AicTranscriptView::action_requested, this, &AiChatScreen::on_turn_action);
    connect(transcript_, &AicTranscriptView::code_open_requested, this, &AiChatScreen::open_code_in_code_tab);
    connect(transcript_, &AicTranscriptView::status_message, this,
            [this](const QString& text) { status_->show_notice(text, QString(), {}, 4000); });

    // Sessions rail
    connect(sessions_, &AicSessionRail::session_activated, this, [this](const QString& id) {
        if (overlay_ == Overlay::Sessions)
            close_overlay();
        if (id != active_session_id_)
            on_session_selected(id);
    });
    connect(sessions_, &AicSessionRail::new_requested, this, &AiChatScreen::on_new_session);
    connect(sessions_, &AicSessionRail::rename_requested, this, [this](const QString& id) {
        if (id != active_session_id_)
            on_session_selected(id);
        if (id == active_session_id_)
            begin_rename();
    });
    connect(sessions_, &AicSessionRail::pin_toggled, this, &AiChatScreen::set_pinned);
    connect(sessions_, &AicSessionRail::export_requested, this, &AiChatScreen::export_markdown);
    connect(sessions_, &AicSessionRail::copy_requested, this, &AiChatScreen::copy_markdown);
    connect(sessions_, &AicSessionRail::delete_requested, this, &AiChatScreen::delete_session);
    connect(sessions_, &AicSessionRail::search_changed, this,
            [this](const QString&) { ScreenStateManager::instance().notify_changed(this); });

    // Composer
    connect(composer_, &AicComposer::send_requested, this, &AiChatScreen::on_send);
    connect(composer_, &AicComposer::stop_requested, this, &AiChatScreen::on_stop_requested);
    connect(composer_, &AicComposer::regenerate_requested, this, &AiChatScreen::on_regenerate);
    connect(composer_, &AicComposer::attach_file_requested, this, &AiChatScreen::pick_files);
    connect(composer_, &AicComposer::files_dropped, this, &AiChatScreen::add_files);
    connect(composer_, &AicComposer::image_rejected, this, [this](const QString& name) {
        status_->show_notice(tr("%1 was not attached: images can't be sent — this chat sends text only.").arg(name),
                             QString(), {}, 6000, SB::Tone::Warn);
    });
    connect(composer_, &AicComposer::context_attach_requested, this, &AiChatScreen::add_context);
    connect(composer_, &AicComposer::chip_removed, this, &AiChatScreen::remove_context);
    connect(composer_, &AicComposer::text_changed, this,
            [this]() { ScreenStateManager::instance().notify_changed(this); });

    // Context rail
    connect(context_rail_, &AicContextRail::linked_attach_toggled, this, [this](bool attached) {
        linked_context_pending_ = attached && linked_symbol_.is_valid();
        refresh_context_ui();
    });
    connect(context_rail_, &AicContextRail::detach_requested, this, &AiChatScreen::remove_context);
    connect(context_rail_, &AicContextRail::add_symbol_requested, this, &AiChatScreen::pick_symbol);
    connect(context_rail_, &AicContextRail::add_file_requested, this, &AiChatScreen::pick_files);
    connect(context_rail_, &AicContextRail::add_reference_requested, this, &AiChatScreen::add_context);
    connect(context_rail_, &AicContextRail::tool_policy_changed, this, [this](bool tools_on, bool nav_on) {
        tools_on_ = tools_on;
        nav_on_ = nav_on;
        bar_->set_action_checked(QStringLiteral("tools"), tools_on_);
        bar_->set_action_checked(QStringLiteral("nav"), nav_on_);
        bar_->set_action_enabled(QStringLiteral("nav"), tools_on_);
        update_status();
        ScreenStateManager::instance().notify_changed(this);
    });
    connect(context_rail_, &AicContextRail::manage_tools_requested, this, []() {
        EventBus::instance().publish(QStringLiteral("nav.switch_screen"),
                                     QVariantMap{{"screen_id", QStringLiteral("mcp_servers")}});
    });
    connect(context_rail_, &AicContextRail::prompt_activated, this, [this](const PromptTemplate& p) {
        QString body = p.body;
        const QString sym = linked_symbol_.is_valid() ? linked_symbol_.symbol : QString();
        if (!sym.isEmpty())
            body.replace(QStringLiteral("{SYMBOL}"), sym);
        composer_->set_text(body);
        composer_->focus_editor();
        if (overlay_ == Overlay::Context)
            close_overlay();
    });
    connect(context_rail_, &AicContextRail::prompt_delete_requested, this, &AiChatScreen::delete_saved_prompt);
    connect(context_rail_, &AicContextRail::save_draft_requested, this, &AiChatScreen::save_draft_prompt);
    connect(context_rail_, &AicContextRail::peek_requested, this,
            [this](int section) { open_overlay(Overlay::Context, section); });

    // Start page
    connect(start_page_, &AicStartPage::prompt_chosen, this, [this](const QString& prompt) {
        composer_->set_text(prompt); // fills only — the user presses Send
        composer_->focus_editor();
    });
    connect(start_page_, &AicStartPage::session_chosen, this, &AiChatScreen::on_session_selected);
}

void AiChatScreen::retranslateUi() {
    if (bar_) {
        bar_->set_title(tr("AI CHAT"));
        bar_->set_action_text(QStringLiteral("new"), tr("NEW CHAT"));
        bar_->set_action_tooltip(QStringLiteral("new"), tr("Start a new chat (Ctrl+N)"));
        bar_->set_action_text(QStringLiteral("sessions"), tr("SESSIONS"));
        bar_->set_action_text(QStringLiteral("tools"), tr("TOOLS"));
        bar_->set_action_text(QStringLiteral("nav"), tr("NAV"));
        bar_->set_action_text(QStringLiteral("export"), tr("EXPORT"));
        if (auto* f = bar_->command_field())
            f->setPlaceholderText(tr("Session title"));
    }
    update_model_notice();
    update_stats();
}

} // namespace fincept::screens
