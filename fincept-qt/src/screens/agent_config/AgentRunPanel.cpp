// AgentRunPanel.cpp — harness-run page of the inspector (see the header).

#include "screens/agent_config/AgentRunPanel.h"

#include "screens/agent_config/AgentRunCommon.h"
#include "screens/agent_config/AgentRunFilesView.h"
#include "screens/agent_config/AgentTranscriptView.h"
#include "services/coding_agents/CodingAgentService.h"
#include "services/harness/HarnessRegistry.h"
#include "ui/markdown/MarkdownRenderer.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QClipboard>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QDir>
#include <QEvent>
#include <QFileDialog>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QTextBrowser>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

namespace fincept::screens {

namespace ca = services::coding_agents;

namespace agent_run_panel_detail {

enum ViewTab { kTabTranscript = 0, kTabFiles = 1 };
enum TranscriptPage { kPageEmpty = 0, kPageLoading = 1, kPageTranscript = 2 };

QLabel* arp_label(const char* object_name, QWidget* parent) {
    auto* l = new QLabel(parent);
    l->setObjectName(QString::fromLatin1(object_name));
    return l;
}

QPushButton* arp_button(const char* object_name, QWidget* parent) {
    auto* b = new QPushButton(parent);
    b->setObjectName(QString::fromLatin1(object_name));
    b->setCursor(Qt::PointingHandCursor);
    return b;
}

} // namespace agent_run_panel_detail

using namespace agent_run_panel_detail;

AgentRunPanel::AgentRunPanel(QWidget* parent) : QWidget(parent) {
    model_ = new AgentTranscriptModel(this);

    flush_timer_ = new QTimer(this);
    flush_timer_->setSingleShot(true);
    flush_timer_->setInterval(100);
    connect(flush_timer_, &QTimer::timeout, this, &AgentRunPanel::flush_pending);

    tick_timer_ = new QTimer(this);
    tick_timer_->setInterval(1000);
    connect(tick_timer_, &QTimer::timeout, this, &AgentRunPanel::refresh_header);

    build();
    retranslate();
    refresh_header();

    // Harness state decides what the empty view lists.
    connect(&harness::HarnessRegistry::instance(), &harness::HarnessRegistry::status_changed, this,
            [this](const QString&) {
                if (run_id_.isEmpty())
                    refresh_empty_state();
            });
}

// ── Layout ───────────────────────────────────────────────────────────────────

void AgentRunPanel::build() {
    setObjectName(QStringLiteral("awbTranscriptPane"));
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(10, 8, 10, 8);
    v->setSpacing(6);

    auto* title_row = new QHBoxLayout;
    run_title_ = arp_label("awbRunTitle", this);
    run_title_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    run_title_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    title_row->addWidget(run_title_, 1);
    run_pill_ = arp_label("awbPill", this);
    title_row->addWidget(run_pill_);
    v->addLayout(title_row);

    run_meta_ = arp_label("awbHint", this);
    run_meta_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    run_meta_->setWordWrap(true);
    v->addWidget(run_meta_);

    auto* cmd_row = new QHBoxLayout;
    cmd_row->setSpacing(6);
    cmd_lbl_ = arp_label("awbLabel", this);
    cmd_row->addWidget(cmd_lbl_);
    cmd_edit_ = new QLineEdit(this);
    cmd_edit_->setObjectName(QStringLiteral("awbSnippet"));
    cmd_edit_->setReadOnly(true);
    cmd_row->addWidget(cmd_edit_, 1);
    cmd_copy_btn_ = arp_button("awbBtn", this);
    connect(cmd_copy_btn_, &QPushButton::clicked, this, [this]() {
        if (!cmd_edit_->text().isEmpty())
            QGuiApplication::clipboard()->setText(cmd_edit_->text());
    });
    cmd_row->addWidget(cmd_copy_btn_);
    v->addLayout(cmd_row);

    // Two rows: the inspector is a side pane, five buttons in one row did not fit.
    auto* actions = new QHBoxLayout;
    actions->setSpacing(6);
    stop_btn_ = arp_button("awbDangerBtn", this); // stopping commits nothing: no risk marker
    connect(stop_btn_, &QPushButton::clicked, this, &AgentRunPanel::on_stop);
    open_folder_btn_ = arp_button("awbBtn", this);
    connect(open_folder_btn_, &QPushButton::clicked, this, &AgentRunPanel::on_open_folder);
    export_btn_ = arp_button("awbBtn", this);
    export_btn_->setProperty("fincept_agent_risk", "file"); // agents: writes the transcript to a file the user picks
    connect(export_btn_, &QPushButton::clicked, this, &AgentRunPanel::on_export);
    actions->addWidget(stop_btn_);
    actions->addWidget(open_folder_btn_);
    actions->addWidget(export_btn_);
    actions->addStretch(1);
    v->addLayout(actions);
    auto* result_row = new QHBoxLayout;
    result_row->setSpacing(6);
    view_result_btn_ = arp_button("awbBtn", this);
    connect(view_result_btn_, &QPushButton::clicked, this, &AgentRunPanel::on_view_result);
    copy_result_btn_ = arp_button("awbBtn", this);
    connect(copy_result_btn_, &QPushButton::clicked, this, &AgentRunPanel::on_copy_result);
    result_row->addWidget(view_result_btn_);
    result_row->addWidget(copy_result_btn_);
    result_row->addStretch(1);
    v->addLayout(result_row);

    error_banner_ = arp_label("awbErrorBanner", this);
    error_banner_->setWordWrap(true);
    error_banner_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    error_banner_->hide();
    v->addWidget(error_banner_);

    note_lbl_ = arp_label("awbHint", this);
    note_lbl_->setWordWrap(true);
    note_lbl_->setTextFormat(Qt::RichText);
    connect(note_lbl_, &QLabel::linkActivated, this, [this](const QString& link) {
        if (link.startsWith(QLatin1String("run:")))
            show_run(link.mid(4));
    });
    note_lbl_->hide();
    v->addWidget(note_lbl_);

    // TRANSCRIPT | FILES
    auto* tabs = new QHBoxLayout;
    tabs->setSpacing(4);
    transcript_tab_btn_ = arp_button("awbTabBtn", this);
    files_tab_btn_ = arp_button("awbTabBtn", this);
    connect(transcript_tab_btn_, &QPushButton::clicked, this, [this]() { set_view_tab(kTabTranscript); });
    connect(files_tab_btn_, &QPushButton::clicked, this, [this]() { set_view_tab(kTabFiles); });
    tabs->addWidget(transcript_tab_btn_);
    tabs->addWidget(files_tab_btn_);
    tabs->addStretch(1);
    v->addLayout(tabs);

    view_stack_ = new QStackedWidget(this);
    transcript_stack_ = new QStackedWidget(view_stack_);

    // Empty state: never a blank pane.
    empty_page_ = new QWidget(transcript_stack_);
    empty_page_->setObjectName(QStringLiteral("awbPage"));
    auto* ev = new QVBoxLayout(empty_page_);
    ev->setContentsMargins(16, 16, 16, 16);
    ev->setSpacing(10);
    ev->addStretch(1);
    empty_title_ = arp_label("awbSection", empty_page_);
    empty_title_->setAlignment(Qt::AlignCenter);
    ev->addWidget(empty_title_);
    empty_text_ = arp_label("awbHint", empty_page_);
    empty_text_->setAlignment(Qt::AlignCenter);
    empty_text_->setWordWrap(true);
    empty_text_->setTextFormat(Qt::RichText);
    ev->addWidget(empty_text_);
    auto* empty_btns = new QHBoxLayout;
    empty_btns->addStretch(1);
    empty_primary_btn_ = arp_button("awbPrimaryBtn", empty_page_);
    empty_secondary_btn_ = arp_button("awbBtn", empty_page_);
    connect(empty_primary_btn_, &QPushButton::clicked, this, [this]() {
        auto& svc = ca::CodingAgentService::instance();
        if (!svc.run_ids().isEmpty()) {
            show_run(svc.run_ids().constFirst());
            return;
        }
        emit new_run_requested(); // opens the composer; nothing runs from here
    });
    connect(empty_secondary_btn_, &QPushButton::clicked, this, &AgentRunPanel::show_harnesses_requested);
    empty_btns->addWidget(empty_primary_btn_);
    empty_btns->addWidget(empty_secondary_btn_);
    empty_btns->addStretch(1);
    ev->addLayout(empty_btns);
    ev->addStretch(2);
    transcript_stack_->addWidget(empty_page_);

    loading_lbl_ = arp_label("awbPlaceholder", transcript_stack_);
    loading_lbl_->setAlignment(Qt::AlignCenter);
    transcript_stack_->addWidget(loading_lbl_);

    view_ = new AgentTranscriptView(transcript_stack_);
    view_->set_transcript_model(model_);
    transcript_stack_->addWidget(view_);
    view_stack_->addWidget(transcript_stack_);

    files_view_ = new AgentRunFilesView(view_stack_);
    connect(files_view_, &AgentRunFilesView::file_count_changed, this, [this](int n) {
        files_tab_btn_->setText(n > 0 ? tr("FILES (%1)").arg(n) : tr("FILES"));
    });
    view_stack_->addWidget(files_view_);
    v->addWidget(view_stack_, 1);

    auto* follow_row = new QHBoxLayout;
    follow_row->setSpacing(6);
    followup_edit_ = new QLineEdit(this);
    followup_edit_->setProperty("fincept_agent_risk", "spend"); // agents: Enter sends a follow-up to the running agent (model quota)
    followup_edit_->setMaxLength(ca::kAgentPromptMaxChars);
    connect(followup_edit_, &QLineEdit::returnPressed, this, &AgentRunPanel::on_follow_up);
    follow_row->addWidget(followup_edit_, 1);
    followup_btn_ = arp_button("awbBtn", this);
    followup_btn_->setProperty("fincept_agent_risk", "spend"); // agents: sends a follow-up prompt (model quota)
    connect(followup_btn_, &QPushButton::clicked, this, &AgentRunPanel::on_follow_up);
    follow_row->addWidget(followup_btn_);
    v->addLayout(follow_row);
    set_view_tab(kTabTranscript);
}

void AgentRunPanel::retranslate() {
    cmd_lbl_->setText(tr("COMMAND"));
    cmd_edit_->setPlaceholderText(tr("shown once the run starts"));
    cmd_edit_->setToolTip(tr("The exact command line, with the prompts shown as their lengths and secrets masked"));
    cmd_copy_btn_->setText(tr("COPY"));
    stop_btn_->setToolTip(tr("Stop the agent (press again to kill it immediately)"));
    open_folder_btn_->setText(tr("OPEN FOLDER"));
    view_result_btn_->setText(tr("VIEW RESULT"));
    copy_result_btn_->setText(tr("COPY RESULT"));
    export_btn_->setText(tr("EXPORT .MD"));
    transcript_tab_btn_->setText(tr("TRANSCRIPT"));
    files_tab_btn_->setText(files_view_->file_count() > 0 ? tr("FILES (%1)").arg(files_view_->file_count()) : tr("FILES"));
    loading_lbl_->setText(tr("Loading transcript…"));
    followup_edit_->setPlaceholderText(tr("Follow-up question — resumes this run's session…"));
    followup_btn_->setText(tr("FOLLOW UP"));
    refresh_header();
}

void AgentRunPanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslate();
    QWidget::changeEvent(event);
}

// ── Lifecycle ────────────────────────────────────────────────────────────────

void AgentRunPanel::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    subscribe();
    resync_transcript();
    // Never open on a blank pane: the latest harness run, when there is one.
    auto& svc = ca::CodingAgentService::instance();
    if (run_id_.isEmpty() && !auto_opened_ && pending_restore_run_.isEmpty() && !svc.run_ids().isEmpty()) {
        auto_opened_ = true;
        show_run(svc.run_ids().constFirst());
        return;
    }
    refresh_header();
}

void AgentRunPanel::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    unsubscribe();
    flush_timer_->stop();
    tick_timer_->stop();
    pending_.clear(); // resynced from the service cache on the next show
}

void AgentRunPanel::subscribe() {
    if (!conns_.isEmpty())
        return;
    auto& svc = ca::CodingAgentService::instance();
    conns_ << connect(&svc, &ca::CodingAgentService::run_event, this, &AgentRunPanel::on_run_event);
    conns_ << connect(&svc, &ca::CodingAgentService::run_updated, this, &AgentRunPanel::on_run_updated);
    conns_ << connect(&svc, &ca::CodingAgentService::run_finished, this, [this](const QString& id) {
        if (id == run_id_) {
            flush_pending();
            model_->drop_live();
        }
        on_run_updated(id);
    });
    conns_ << connect(&svc, &ca::CodingAgentService::runs_reset, this, &AgentRunPanel::on_runs_reset);
}

void AgentRunPanel::unsubscribe() {
    for (const auto& c : std::as_const(conns_))
        disconnect(c);
    conns_.clear();
}

// ── Run view ─────────────────────────────────────────────────────────────────

void AgentRunPanel::remember_command(const QVector<ca::AgentEvent>& events) {
    command_fallback_.clear();
    for (const ca::AgentEvent& ev : events) {
        if (ev.kind != ca::EventKind::Status || !ev.title.startsWith(QLatin1String("Starting ")))
            continue;
        for (const QString& line : ev.body.split(QLatin1Char('\n'))) {
            if (line.startsWith(QLatin1String("Command: "))) {
                command_fallback_ = line.mid(9);
                return; // the first start of this run
            }
        }
    }
}

void AgentRunPanel::show_run(const QString& run_id) {
    const bool changed = run_id != run_id_;
    run_id_ = run_id;
    stop_clicked_ = false;
    pending_.clear();
    flush_timer_->stop();
    model_->clear_events();
    command_fallback_.clear();
    followup_edit_->clear();
    auto& svc = ca::CodingAgentService::instance();
    const ca::CodingAgentRun* run = run_id.isEmpty() ? nullptr : svc.find_run(run_id);
    files_view_->set_run(run_id, run ? run->workdir : QString(),
                         run ? run->permission == harness::PermissionMode::ReadOnly : true);
    if (changed)
        emit run_shown(run_id);
    if (run_id.isEmpty()) {
        refresh_header();
        return;
    }
    if (svc.has_cached_events(run_id)) {
        loading_ = false;
        const QVector<ca::AgentEvent> events = svc.cached_events(run_id);
        model_->reset_events(events);
        files_view_->reset_events(events);
        remember_command(events);
        refresh_header();
        view_->scroll_to_end();
        return;
    }
    loading_ = true;
    refresh_header();
    svc.fetch_events(run_id, this, [this, run_id](const QVector<ca::AgentEvent>& events) {
        if (run_id != run_id_)
            return; // the user moved on
        loading_ = false;
        model_->reset_events(events);
        files_view_->reset_events(events);
        remember_command(events);
        refresh_header();
        view_->scroll_to_end();
    });
}

void AgentRunPanel::resync_transcript() {
    if (run_id_.isEmpty() || loading_)
        return;
    auto& svc = ca::CodingAgentService::instance();
    if (!svc.has_cached_events(run_id_))
        return;
    const bool at_bottom = view_->is_at_bottom();
    const QVector<ca::AgentEvent> events = svc.cached_events(run_id_);
    const int before = model_->last_seq();
    model_->append_events(events); // only events newer than the last shown
    QVector<ca::AgentEvent> fresh;
    for (const ca::AgentEvent& e : events) {
        if (e.seq > before)
            fresh.append(e);
    }
    files_view_->add_events(fresh);
    if (at_bottom)
        view_->scroll_to_end();
}

void AgentRunPanel::on_run_event(const QString& run_id, const ca::AgentEvent& event) {
    if (run_id != run_id_ || loading_)
        return;
    if (event.partial) {
        // Live text: the coalescing already happened in the service (≤ 10/s).
        const bool at_bottom = view_->is_at_bottom();
        model_->set_live(event);
        if (at_bottom)
            view_->scroll_to_end();
        return;
    }
    pending_.append(event);
    if (!flush_timer_->isActive())
        flush_timer_->start();
}

void AgentRunPanel::flush_pending() {
    if (pending_.isEmpty())
        return;
    const bool at_bottom = view_->is_at_bottom();
    model_->append_events(pending_);
    files_view_->add_events(pending_);
    pending_.clear();
    if (at_bottom)
        view_->scroll_to_end();
    if (model_->hidden_count() > 0)
        refresh_header(); // the "earlier events hidden" note changed
}

void AgentRunPanel::on_run_updated(const QString& run_id) {
    if (run_id == run_id_)
        refresh_header();
}

void AgentRunPanel::on_runs_reset() {
    auto& svc = ca::CodingAgentService::instance();
    if (!pending_restore_run_.isEmpty() && svc.is_loaded()) {
        const QString restore = pending_restore_run_;
        pending_restore_run_.clear();
        if (run_id_.isEmpty() && svc.find_run(restore)) {
            show_run(restore);
            return;
        }
    }
    if (!run_id_.isEmpty() && !svc.find_run(run_id_)) {
        show_run(QString()); // deleted
        return;
    }
    if (run_id_.isEmpty() && !auto_opened_ && isVisible() && !svc.run_ids().isEmpty()) {
        auto_opened_ = true; // the history arrived after the first show
        show_run(svc.run_ids().constFirst());
        return;
    }
    refresh_header();
}

void AgentRunPanel::set_view_tab(int tab) {
    view_tab_ = tab == kTabFiles ? kTabFiles : kTabTranscript;
    view_stack_->setCurrentIndex(view_tab_);
    for (QPushButton* b : {transcript_tab_btn_, files_tab_btn_}) {
        const bool active = (b == files_tab_btn_) == (view_tab_ == kTabFiles);
        if (b->property("active").toBool() != active || !b->property("active").isValid()) {
            b->setProperty("active", active);
            awb_repolish(b);
        }
    }
}

void AgentRunPanel::refresh_empty_state() {
    auto& svc = ca::CodingAgentService::instance();
    auto& registry = harness::HarnessRegistry::instance();
    const QStringList installed = registry.installed_ids();
    if (!svc.run_ids().isEmpty()) {
        empty_title_->setText(tr("NO RUN SELECTED"));
        empty_text_->setText(tr("Pick a run in the RUNS list, or open the latest one. Runs keep going in the "
                                "background when you leave this screen — a desktop notification tells you when one "
                                "finishes.")
                                 .toHtmlEscaped());
        empty_primary_btn_->setText(tr("OPEN LATEST RUN"));
        empty_primary_btn_->setEnabled(true);
        empty_secondary_btn_->setText(tr("HARNESSES"));
        return;
    }
    empty_title_->setText(tr("NO RUNS YET"));
    QStringList lines;
    if (installed.isEmpty()) {
        lines << tr("No coding agent was found on this computer yet. HARNESSES lists the ones the terminal supports, "
                    "with how to install and sign in to each; Settings › AI Agents checks the connections.")
                     .toHtmlEscaped();
    } else {
        QStringList names;
        for (const QString& id : installed) {
            const harness::HarnessStatus st = registry.status(id);
            const AwbHarnessState state = awb_harness_state(id);
            names << QStringLiteral("<span style='color:%1'>●</span> %2%3")
                         .arg(awb_tone_color(state.tone), registry.display_name(id).toHtmlEscaped(),
                              st.installation && !st.installation->version.isEmpty()
                                  ? QStringLiteral(" ") + st.installation->version.toHtmlEscaped()
                                  : QString());
        }
        lines << tr("Ready on this computer:").toHtmlEscaped() + QStringLiteral(" ") +
                     names.join(QStringLiteral(" &nbsp; "));
        lines << tr("NEW RUN opens the composer: pick a harness and a starter prompt, adjust it, and press RUN. The "
                    "agent works in its own folder and can use the terminal's live data.")
                     .toHtmlEscaped();
    }
    empty_text_->setText(lines.join(QStringLiteral("<br><br>")));
    empty_primary_btn_->setText(tr("START FROM A TEMPLATE"));
    empty_primary_btn_->setEnabled(!installed.isEmpty());
    empty_secondary_btn_->setText(tr("HARNESSES"));
}

void AgentRunPanel::refresh_header() {
    auto& svc = ca::CodingAgentService::instance();
    auto& registry = harness::HarnessRegistry::instance();
    const ca::CodingAgentRun* run = run_id_.isEmpty() ? nullptr : svc.find_run(run_id_);
    const bool have = run != nullptr;
    stop_btn_->setEnabled(have && !ca::is_terminal_status(run->status));
    stop_btn_->setText(stop_clicked_ ? tr("FORCE STOP") : tr("STOP"));
    open_folder_btn_->setEnabled(have && !run->workdir.isEmpty()); // existence is checked off-thread on click
    view_result_btn_->setEnabled(have && !run->final_text.isEmpty());
    copy_result_btn_->setEnabled(have && !run->final_text.isEmpty());
    export_btn_->setEnabled(have);
    QString why;
    const bool can_follow = have && svc.can_follow_up(run_id_, &why);
    followup_edit_->setEnabled(can_follow);
    followup_btn_->setEnabled(can_follow);
    followup_btn_->setToolTip(can_follow ? QString() : why);
    files_tab_btn_->setEnabled(have);

    if (!have) {
        run_title_->setText(tr("No run selected"));
        run_pill_->hide();
        run_meta_->clear();
        cmd_edit_->clear();
        error_banner_->hide();
        note_lbl_->hide();
        set_view_tab(kTabTranscript);
        refresh_empty_state();
        transcript_stack_->setCurrentIndex(kPageEmpty);
        tick_timer_->stop();
        return;
    }

    run_title_->setText(run->title);
    run_title_->setToolTip(run->prompt.left(1500));
    run_pill_->show();
    run_pill_->setText(awb_run_pill_text(*run));
    if (run_pill_->property("status").toString() != ca::status_id(run->status)) {
        run_pill_->setProperty("status", ca::status_id(run->status));
        awb_repolish(run_pill_);
    }

    QStringList meta;
    meta << (run->harness_version.isEmpty() ? registry.display_name(run->harness)
                                            : QStringLiteral("%1 %2").arg(registry.display_name(run->harness),
                                                                          run->harness_version));
    meta << (run->model.isEmpty() ? tr("default model") : run->model);
    if (!run->effort.isEmpty())
        meta << tr("effort %1").arg(run->effort);
    meta << awb_permission_text(run->permission);
    if (run->status == ca::RunStatus::Queued) {
        const int pos = svc.queue_position(run_id_);
        if (pos > 0)
            meta << tr("queue position %1").arg(pos);
    } else if (run->started_at_ms > 0) {
        QString d = awb_duration(awb_run_duration_ms(*run));
        if (run->time_limit_s > 0)
            d += QStringLiteral(" / ") + awb_duration(static_cast<qint64>(run->time_limit_s) * 1000);
        meta << d;
    }
    if (run->input_tokens > 0 || run->output_tokens > 0) {
        QString t = tr("%1 in / %2 out tok").arg(awb_tokens(run->input_tokens), awb_tokens(run->output_tokens));
        if (run->cached_tokens > 0)
            t += QStringLiteral(" (") + tr("%1 cached").arg(awb_tokens(run->cached_tokens)) + QLatin1Char(')');
        meta << t;
    }
    if (run->cost_usd.has_value())
        meta << QStringLiteral("$%1").arg(*run->cost_usd, 0, 'f', 4);
    if (run->max_budget_usd > 0.0)
        meta << tr("budget $%1").arg(run->max_budget_usd, 0, 'f', 2);
    if (run->max_turns > 0)
        meta << tr("max %1 turns").arg(run->max_turns);
    if (!run->session_id.isEmpty())
        meta << tr("session %1").arg(run->session_id.left(8));
    if (!run->use_fincept_tools)
        meta << tr("terminal tools off");
    run_meta_->setText(meta.join(QStringLiteral("  ·  ")));
    run_meta_->setToolTip(QDir::toNativeSeparators(run->workdir));

    QString cmd = svc.command_line(run_id_);
    if (cmd.isEmpty())
        cmd = command_fallback_;
    if (cmd_edit_->text() != cmd) {
        cmd_edit_->setText(cmd);
        cmd_edit_->setCursorPosition(0);
    }
    cmd_copy_btn_->setEnabled(!cmd.isEmpty());

    const bool failed = run->status == ca::RunStatus::Failed || run->status == ca::RunStatus::Interrupted;
    error_banner_->setVisible(failed && !run->error.isEmpty());
    if (failed && !run->error.isEmpty())
        error_banner_->setText(run->error.left(1200));

    QStringList notes;
    if (!registry.is_known(run->harness))
        notes << tr("Made with %1 — shown read-only.").arg(registry.display_name(run->harness).toHtmlEscaped());
    if (!run->parent_run_id.isEmpty()) {
        const ca::CodingAgentRun* parent = svc.find_run(run->parent_run_id);
        notes << (parent ? tr("Follow-up of <a href='run:%1'>%2</a>")
                               .arg(run->parent_run_id, parent->title.toHtmlEscaped())
                         : tr("Follow-up of a deleted run"));
    }
    if (model_->hidden_count() > 0)
        notes << tr("%1 earlier events are not shown — EXPORT writes the full transcript.").arg(model_->hidden_count());
    note_lbl_->setVisible(!notes.isEmpty());
    if (!notes.isEmpty())
        note_lbl_->setText(notes.join(QStringLiteral("<br>")));

    transcript_stack_->setCurrentIndex(loading_ ? kPageLoading : kPageTranscript);

    const bool running = run->status == ca::RunStatus::Running;
    if (running && isVisible()) {
        if (!tick_timer_->isActive())
            tick_timer_->start();
    } else {
        tick_timer_->stop();
    }
}

void AgentRunPanel::on_stop() {
    if (run_id_.isEmpty())
        return;
    if (ca::CodingAgentService::instance().stop_run(run_id_)) {
        stop_clicked_ = true;
        refresh_header();
    }
}

void AgentRunPanel::on_follow_up() {
    const QString text = followup_edit_->text().trimmed();
    if (text.isEmpty() || run_id_.isEmpty())
        return;
    QString err;
    const QString id = ca::CodingAgentService::instance().follow_up(run_id_, text, &err);
    if (id.isEmpty()) {
        QMessageBox::warning(this, tr("Follow up"), err);
        return;
    }
    followup_edit_->clear();
    show_run(id);
    emit run_started(id);
}

void AgentRunPanel::on_open_folder() {
    auto& svc = ca::CodingAgentService::instance();
    const ca::CodingAgentRun* run = svc.find_run(run_id_);
    if (!run || run->workdir.isEmpty())
        return;
    const QString folder = run->workdir;
    // The existence check runs on the service's file thread: a folder on a dead
    // network share must not freeze the screen.
    svc.check_folder(folder, this, [this, folder](bool exists) {
        if (exists)
            QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
        else
            QMessageBox::information(this, tr("Open folder"),
                                     tr("The working folder no longer exists:\n%1").arg(QDir::toNativeSeparators(folder)));
    });
}

void AgentRunPanel::on_view_result() {
    const ca::CodingAgentRun* run = ca::CodingAgentService::instance().find_run(run_id_);
    if (!run || run->final_text.isEmpty())
        return;
    QDialog dlg(this);
    dlg.setObjectName(QStringLiteral("awbDialog"));
    dlg.setWindowTitle(run->title.left(120));
    const QFontMetrics fm(ui::fonts::font());
    dlg.resize(fm.averageCharWidth() * 110, fm.height() * 36);
    auto* lay = new QVBoxLayout(&dlg);
    auto* browser = new QTextBrowser(&dlg);
    browser->setOpenExternalLinks(true);
    // The terminal's themed markdown view (tables, code blocks, headings).
    browser->setHtml(ui::MarkdownRenderer::render(run->final_text));
    lay->addWidget(browser, 1);
    auto* close = new QPushButton(tr("CLOSE"), &dlg);
    close->setObjectName(QStringLiteral("awbBtn"));
    connect(close, &QPushButton::clicked, &dlg, &QDialog::accept);
    lay->addWidget(close, 0, Qt::AlignRight);
    dlg.exec();
}

void AgentRunPanel::on_copy_result() {
    const ca::CodingAgentRun* run = ca::CodingAgentService::instance().find_run(run_id_);
    if (run && !run->final_text.isEmpty())
        QGuiApplication::clipboard()->setText(run->final_text);
}

void AgentRunPanel::on_export() {
    auto& svc = ca::CodingAgentService::instance();
    const ca::CodingAgentRun* run = svc.find_run(run_id_);
    if (!run)
        return;
    const QString suggested = run->workdir + QStringLiteral("/TRANSCRIPT-") + run->id.left(8) + QStringLiteral(".md");
    const QString path =
        QFileDialog::getSaveFileName(this, tr("Export transcript"), suggested, tr("Markdown (*.md);;All files (*)"));
    if (path.isEmpty())
        return;
    const QString id = run_id_;
    svc.fetch_events(id, this, [this, id, path](const QVector<ca::AgentEvent>& events) {
        write_transcript_file(id, path, events);
    });
}

void AgentRunPanel::write_transcript_file(const QString& run_id, const QString& path,
                                          const QVector<ca::AgentEvent>& events) {
    auto& svc = ca::CodingAgentService::instance();
    const ca::CodingAgentRun* run = svc.find_run(run_id);
    if (!run)
        return;
    auto when = [](qint64 ms) {
        return ms > 0 ? QDateTime::fromMSecsSinceEpoch(ms).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")) : QString();
    };
    QString md;
    md += QStringLiteral("# %1\n\n").arg(run->title);
    md += QStringLiteral("| | |\n|---|---|\n");
    md += QStringLiteral("| Run | `%1` |\n").arg(run->id);
    md += QStringLiteral("| Harness | %1%2 |\n")
              .arg(awb_harness_name(run->harness),
                   run->harness_version.isEmpty() ? QString() : QStringLiteral(" ") + run->harness_version);
    md += QStringLiteral("| Model | %1 |\n").arg(run->model.isEmpty() ? QStringLiteral("CLI default") : run->model);
    if (!run->effort.isEmpty())
        md += QStringLiteral("| Effort | %1 |\n").arg(run->effort);
    md += QStringLiteral("| Mode | %1 |\n").arg(awb_permission_text(run->permission));
    md += QStringLiteral("| Terminal tools | %1 |\n").arg(run->use_fincept_tools ? QStringLiteral("on") : QStringLiteral("off"));
    md += QStringLiteral("| Status | %1 |\n").arg(ca::status_id(run->status));
    md += QStringLiteral("| Started | %1 |\n").arg(when(run->started_at_ms));
    md += QStringLiteral("| Finished | %1 |\n").arg(when(run->finished_at_ms));
    if (run->cost_usd.has_value())
        md += QStringLiteral("| Cost | $%1 |\n").arg(*run->cost_usd, 0, 'f', 4);
    md += QStringLiteral("| Tokens | %1 in / %2 out (%3 cached) |\n")
              .arg(run->input_tokens)
              .arg(run->output_tokens)
              .arg(run->cached_tokens);
    if (!run->session_id.isEmpty())
        md += QStringLiteral("| Session | `%1` |\n").arg(run->session_id);
    md += QStringLiteral("| Working folder | `%1` |\n").arg(QDir::toNativeSeparators(run->workdir));
    md += QStringLiteral("| Origin | %1 |\n\n").arg(run->origin);
    md += QStringLiteral("## Prompt\n\n%1\n\n## Transcript\n\n").arg(run->prompt);
    for (const auto& ev : events) {
        if (ev.partial)
            continue;
        md += QStringLiteral("### %1 · %2%3 · %4\n\n")
                  .arg(when(ev.ts_ms), ca::event_kind_id(ev.kind), ev.is_error ? QStringLiteral(" (error)") : QString(),
                       ev.title);
        if (!ev.body.isEmpty())
            md += QStringLiteral("````text\n%1\n````\n\n").arg(ev.body);
    }
    if (!run->final_text.isEmpty())
        md += QStringLiteral("## Result\n\n%1\n").arg(run->final_text);
    if (!run->error.isEmpty())
        md += QStringLiteral("\n## Error\n\n````text\n%1\n````\n").arg(run->error);

    // Written on the service's file thread (the folder may be on a slow share).
    svc.write_file(path, md.toUtf8(), this, [this, path](bool ok, const QString& error) {
        if (!ok) {
            QMessageBox::warning(this, tr("Export transcript"), tr("Could not write %1: %2").arg(path, error));
            return;
        }
        emit notice(tr("Transcript saved to %1").arg(QDir::toNativeSeparators(path)));
    });
}

// ── State ────────────────────────────────────────────────────────────────────

QVariantMap AgentRunPanel::save_state() const {
    return {{QStringLiteral("run_id"), run_id_}};
}

void AgentRunPanel::restore_state(const QVariantMap& state) {
    const QString run_id = state.value("run_id").toString();
    auto& svc = ca::CodingAgentService::instance();
    if (!run_id.isEmpty() && svc.find_run(run_id))
        show_run(run_id);
    else if (!run_id.isEmpty() && !svc.is_loaded())
        pending_restore_run_ = run_id; // history still loading — selected in on_runs_reset()
}

} // namespace fincept::screens
