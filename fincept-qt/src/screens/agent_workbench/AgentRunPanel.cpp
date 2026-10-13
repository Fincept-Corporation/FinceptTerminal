// AgentRunPanel.cpp — RUN tab: composer + live transcript.

#include "screens/agent_workbench/AgentRunPanel.h"

#include "screens/agent_workbench/AgentTranscriptView.h"
#include "screens/agent_workbench/AgentWorkbenchCommon.h"
#include "screens/agent_workbench/AgentWorkbenchTemplates.h"
#include "services/coding_agents/CodingAgentService.h"
#include "ui/theme/Theme.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QDir>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSaveFile>
#include <QScrollArea>
#include <QShortcut>
#include <QSplitter>
#include <QStackedWidget>
#include <QTextBrowser>
#include <QTextCursor>
#include <QTextDocument>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

namespace fincept::screens {

namespace ca = services::coding_agents;

namespace {

constexpr int kPickerMaxRuns = 60;

QLabel* make_label(const char* object_name, QWidget* parent) {
    auto* l = new QLabel(parent);
    l->setObjectName(QString::fromLatin1(object_name));
    return l;
}

QPushButton* make_button(const char* object_name, QWidget* parent) {
    auto* b = new QPushButton(parent);
    b->setObjectName(QString::fromLatin1(object_name));
    b->setCursor(Qt::PointingHandCursor);
    return b;
}

} // namespace

AgentRunPanel::AgentRunPanel(QWidget* parent) : QWidget(parent) {
    model_ = new AgentTranscriptModel(this);

    flush_timer_ = new QTimer(this);
    flush_timer_->setSingleShot(true);
    flush_timer_->setInterval(100);
    connect(flush_timer_, &QTimer::timeout, this, &AgentRunPanel::flush_pending);

    tick_timer_ = new QTimer(this);
    tick_timer_->setInterval(1000);
    connect(tick_timer_, &QTimer::timeout, this, &AgentRunPanel::refresh_header);

    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setChildrenCollapsible(false);
    splitter->addWidget(build_composer());
    splitter->addWidget(build_transcript_pane());
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({400, 900});
    root->addWidget(splitter);

    retranslate();
    on_agent_changed();
    refresh_header();
}

// ── Layout ───────────────────────────────────────────────────────────────────

QWidget* AgentRunPanel::build_composer() {
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setMinimumWidth(340);

    auto* box = new QWidget(scroll);
    box->setObjectName(QStringLiteral("awbComposer"));
    auto* v = new QVBoxLayout(box);
    v->setContentsMargins(14, 12, 14, 12);
    v->setSpacing(6);

    composer_title_ = make_label("awbSection", box);
    v->addWidget(composer_title_);

    agent_lbl_ = make_label("awbLabel", box);
    v->addWidget(agent_lbl_);
    auto* agent_row = new QHBoxLayout;
    agent_row->setSpacing(6);
    claude_btn_ = make_button("awbAgentBtn", box);
    codex_btn_ = make_button("awbAgentBtn", box);
    claude_btn_->setCheckable(true);
    codex_btn_->setCheckable(true);
    claude_btn_->setChecked(true);
    auto* group = new QButtonGroup(this);
    group->setExclusive(true);
    group->addButton(claude_btn_);
    group->addButton(codex_btn_);
    connect(claude_btn_, &QPushButton::toggled, this, [this](bool) { on_agent_changed(); });
    agent_row->addWidget(claude_btn_, 1);
    agent_row->addWidget(codex_btn_, 1);
    v->addLayout(agent_row);

    agent_status_ = make_label("awbHint", box);
    agent_status_->setTextFormat(Qt::RichText);
    agent_status_->setOpenExternalLinks(true);
    agent_status_->setWordWrap(true);
    agent_status_->setTextInteractionFlags(Qt::TextBrowserInteraction);
    v->addWidget(agent_status_);

    model_lbl_ = make_label("awbLabel", box);
    v->addWidget(model_lbl_);
    model_edit_ = new QLineEdit(box);
    model_edit_->setMaxLength(100);
    v->addWidget(model_edit_);

    mode_lbl_ = make_label("awbLabel", box);
    v->addWidget(mode_lbl_);
    mode_combo_ = new QComboBox(box);
    mode_combo_->addItem(QString(), ca::mode_id(ca::RunMode::Analysis));
    mode_combo_->addItem(QString(), ca::mode_id(ca::RunMode::WorkspaceEdits));
    v->addWidget(mode_combo_);

    tools_check_ = new QCheckBox(box);
    tools_check_->setChecked(true);
    v->addWidget(tools_check_);

    budget_lbl_ = make_label("awbLabel", box);
    v->addWidget(budget_lbl_);
    auto* budget_row = new QHBoxLayout;
    budget_spin_ = new QDoubleSpinBox(box);
    budget_spin_->setRange(0.0, 1000.0);
    budget_spin_->setDecimals(2);
    budget_spin_->setSingleStep(0.5);
    budget_spin_->setPrefix(QStringLiteral("$ "));
    budget_row->addWidget(budget_spin_);
    budget_hint_ = make_label("awbHint", box);
    budget_row->addWidget(budget_hint_, 1);
    v->addLayout(budget_row);

    folder_lbl_ = make_label("awbLabel", box);
    v->addWidget(folder_lbl_);
    auto* folder_row = new QHBoxLayout;
    folder_row->setSpacing(4);
    folder_edit_ = new QLineEdit(box);
    folder_edit_->setClearButtonEnabled(true);
    folder_row->addWidget(folder_edit_, 1);
    folder_browse_btn_ = make_button("awbBtn", box);
    connect(folder_browse_btn_, &QPushButton::clicked, this, &AgentRunPanel::on_browse_folder);
    folder_row->addWidget(folder_browse_btn_);
    v->addLayout(folder_row);

    template_lbl_ = make_label("awbLabel", box);
    v->addWidget(template_lbl_);
    template_combo_ = new QComboBox(box);
    connect(template_combo_, &QComboBox::activated, this, &AgentRunPanel::on_template_activated);
    v->addWidget(template_combo_);

    prompt_lbl_ = make_label("awbLabel", box);
    v->addWidget(prompt_lbl_);
    prompt_edit_ = new QPlainTextEdit(box);
    prompt_edit_->setObjectName(QStringLiteral("awbPrompt"));
    prompt_edit_->setMinimumHeight(180);
    prompt_edit_->setTabChangesFocus(true);
    highlighter_ = new AgentPlaceholderHighlighter(prompt_edit_->document());
    v->addWidget(prompt_edit_, 1);
    for (const auto& seq : {QKeySequence(Qt::CTRL | Qt::Key_Return), QKeySequence(Qt::CTRL | Qt::Key_Enter)}) {
        auto* sc = new QShortcut(seq, prompt_edit_);
        sc->setContext(Qt::WidgetShortcut);
        connect(sc, &QShortcut::activated, this, &AgentRunPanel::on_run);
    }

    auto* run_row = new QHBoxLayout;
    run_hint_ = make_label("awbHint", box);
    run_hint_->setWordWrap(true);
    run_row->addWidget(run_hint_, 1);
    run_btn_ = make_button("awbPrimaryBtn", box);
    connect(run_btn_, &QPushButton::clicked, this, &AgentRunPanel::on_run);
    run_row->addWidget(run_btn_);
    v->addLayout(run_row);

    scroll->setWidget(box);
    return scroll;
}

QWidget* AgentRunPanel::build_transcript_pane() {
    auto* pane = new QWidget(this);
    pane->setObjectName(QStringLiteral("awbTranscriptPane"));
    auto* v = new QVBoxLayout(pane);
    v->setContentsMargins(12, 10, 12, 10);
    v->setSpacing(6);

    auto* picker_row = new QHBoxLayout;
    picker_lbl_ = make_label("awbLabel", pane);
    picker_row->addWidget(picker_lbl_);
    run_picker_ = new QComboBox(pane);
    run_picker_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    run_picker_->setMinimumContentsLength(30);
    connect(run_picker_, &QComboBox::activated, this, &AgentRunPanel::on_picker_activated);
    picker_row->addWidget(run_picker_, 1);
    v->addLayout(picker_row);

    auto* title_row = new QHBoxLayout;
    run_title_ = make_label("awbRunTitle", pane);
    run_title_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    title_row->addWidget(run_title_, 1);
    run_pill_ = make_label("awbPill", pane);
    title_row->addWidget(run_pill_);
    v->addLayout(title_row);

    run_meta_ = make_label("awbHint", pane);
    run_meta_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    run_meta_->setWordWrap(true);
    v->addWidget(run_meta_);

    auto* actions = new QHBoxLayout;
    actions->setSpacing(6);
    stop_btn_ = make_button("awbDangerBtn", pane);
    connect(stop_btn_, &QPushButton::clicked, this, &AgentRunPanel::on_stop);
    open_folder_btn_ = make_button("awbBtn", pane);
    connect(open_folder_btn_, &QPushButton::clicked, this, &AgentRunPanel::on_open_folder);
    view_result_btn_ = make_button("awbBtn", pane);
    connect(view_result_btn_, &QPushButton::clicked, this, &AgentRunPanel::on_view_result);
    copy_result_btn_ = make_button("awbBtn", pane);
    connect(copy_result_btn_, &QPushButton::clicked, this, &AgentRunPanel::on_copy_result);
    export_btn_ = make_button("awbBtn", pane);
    connect(export_btn_, &QPushButton::clicked, this, &AgentRunPanel::on_export);
    actions->addWidget(stop_btn_);
    actions->addWidget(open_folder_btn_);
    actions->addWidget(view_result_btn_);
    actions->addWidget(copy_result_btn_);
    actions->addWidget(export_btn_);
    actions->addStretch(1);
    v->addLayout(actions);

    error_banner_ = make_label("awbErrorBanner", pane);
    error_banner_->setWordWrap(true);
    error_banner_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    error_banner_->hide();
    v->addWidget(error_banner_);

    note_lbl_ = make_label("awbHint", pane);
    note_lbl_->setWordWrap(true);
    note_lbl_->setTextFormat(Qt::RichText);
    connect(note_lbl_, &QLabel::linkActivated, this, [this](const QString& link) {
        if (link.startsWith(QLatin1String("run:")))
            show_run(link.mid(4));
    });
    note_lbl_->hide();
    v->addWidget(note_lbl_);

    transcript_stack_ = new QStackedWidget(pane);
    placeholder_lbl_ = make_label("awbPlaceholder", transcript_stack_);
    placeholder_lbl_->setAlignment(Qt::AlignCenter);
    placeholder_lbl_->setWordWrap(true);
    transcript_stack_->addWidget(placeholder_lbl_);
    view_ = new AgentTranscriptView(transcript_stack_);
    view_->set_transcript_model(model_);
    transcript_stack_->addWidget(view_);
    v->addWidget(transcript_stack_, 1);

    auto* follow_row = new QHBoxLayout;
    follow_row->setSpacing(6);
    followup_edit_ = new QLineEdit(pane);
    followup_edit_->setMaxLength(ca::kAgentPromptMaxChars);
    connect(followup_edit_, &QLineEdit::returnPressed, this, &AgentRunPanel::on_follow_up);
    follow_row->addWidget(followup_edit_, 1);
    followup_btn_ = make_button("awbBtn", pane);
    connect(followup_btn_, &QPushButton::clicked, this, &AgentRunPanel::on_follow_up);
    follow_row->addWidget(followup_btn_);
    v->addLayout(follow_row);
    return pane;
}

void AgentRunPanel::retranslate() {
    composer_title_->setText(tr("NEW AGENT RUN"));
    agent_lbl_->setText(tr("AGENT"));
    claude_btn_->setText(tr("CLAUDE CODE"));
    codex_btn_->setText(tr("CODEX"));
    model_lbl_->setText(tr("MODEL"));
    mode_lbl_->setText(tr("MODE"));
    mode_combo_->setItemText(0, tr("Analysis (read-only)"));
    mode_combo_->setItemText(1, tr("Workspace edits (may write files)"));
    tools_check_->setText(tr("Use Fincept terminal tools (live data over MCP)"));
    tools_check_->setToolTip(tr("Connects the agent to this terminal's MCP server: quotes, history, fundamentals, "
                                "filings, news, macro and portfolio tools. Orders still need your approval."));
    budget_lbl_->setText(tr("MAX BUDGET"));
    budget_spin_->setSpecialValueText(tr("No cap"));
    budget_hint_->setText(tr("Claude Code only — the run stops at this spend"));
    folder_lbl_->setText(tr("WORKING FOLDER"));
    folder_edit_->setPlaceholderText(tr("Auto — a new folder per run"));
    folder_browse_btn_->setText(tr("BROWSE"));
    template_lbl_->setText(tr("TEMPLATE"));
    {
        const QSignalBlocker block(template_combo_);
        template_combo_->clear();
        template_combo_->addItem(tr("Insert a starter prompt…"));
        for (const auto& t : agent_prompt_templates())
            template_combo_->addItem(t.title);
        template_combo_->setCurrentIndex(0);
    }
    prompt_lbl_->setText(tr("PROMPT"));
    prompt_edit_->setPlaceholderText(tr("Describe the task — e.g. “Earnings preview for NVDA”. Templates insert a "
                                        "starting point; <PLACEHOLDERS> are highlighted."));
    run_btn_->setText(tr("RUN  ▶"));
    run_btn_->setToolTip(tr("Start the agent (Ctrl+Enter)"));

    picker_lbl_->setText(tr("RUN"));
    stop_btn_->setToolTip(tr("Stop the agent (press again to kill it immediately)"));
    open_folder_btn_->setText(tr("OPEN FOLDER"));
    view_result_btn_->setText(tr("VIEW RESULT"));
    copy_result_btn_->setText(tr("COPY RESULT"));
    export_btn_->setText(tr("EXPORT .MD"));
    followup_edit_->setPlaceholderText(tr("Follow-up question — resumes this run's session…"));
    followup_btn_->setText(tr("FOLLOW UP"));
    on_agent_changed();
    refresh_hint();
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
    refresh_cli_status();
    refresh_hint();
    refresh_run_picker();
    resync_transcript();
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
    conns_ << connect(&svc, &ca::CodingAgentService::run_added, this, [this](const QString&) {
        refresh_run_picker();
        refresh_hint();
    });
    conns_ << connect(&svc, &ca::CodingAgentService::run_finished, this, &AgentRunPanel::on_run_updated);
    conns_ << connect(&svc, &ca::CodingAgentService::runs_reset, this, &AgentRunPanel::on_runs_reset);
    conns_ << connect(&svc, &ca::CodingAgentService::cli_status_changed, this, &AgentRunPanel::refresh_cli_status);
}

void AgentRunPanel::unsubscribe() {
    for (const auto& c : std::as_const(conns_))
        disconnect(c);
    conns_.clear();
}

// ── Composer ─────────────────────────────────────────────────────────────────

ca::AgentKind AgentRunPanel::selected_agent() const {
    return codex_btn_ && codex_btn_->isChecked() ? ca::AgentKind::Codex : ca::AgentKind::Claude;
}

void AgentRunPanel::on_agent_changed() {
    const bool claude = selected_agent() == ca::AgentKind::Claude;
    budget_spin_->setEnabled(claude);
    budget_lbl_->setEnabled(claude);
    model_edit_->setPlaceholderText(claude ? tr("CLI default — or e.g. sonnet, opus")
                                           : tr("CLI default (from ~/.codex/config.toml) — or a model id"));
    refresh_cli_status();
}

void AgentRunPanel::refresh_cli_status() {
    auto& svc = ca::CodingAgentService::instance();
    const ca::AgentKind kind = selected_agent();
    const ca::CliInfo ci = svc.cli_info(kind);
    QString html;
    if (ci.installed()) {
        QString version;
        if (!ci.version.isEmpty())
            version = QStringLiteral("v") + ci.version;
        else if (ci.probing || !ci.probed)
            version = tr("detecting version…");
        else
            version = tr("version unknown");
        html = QStringLiteral("<span style='color:%1'>●</span> %2 · %3")
                   .arg(QString(ui::colors::POSITIVE()), version.toHtmlEscaped(),
                        QDir::toNativeSeparators(ci.path).toHtmlEscaped());
        if (ci.via_cmd_shim)
            html += QStringLiteral(" · ") + tr("npm launcher").toHtmlEscaped();
        if (ci.version.isEmpty() && !ci.probe_error.isEmpty())
            html += QStringLiteral("<br><span style='color:%1'>%2</span>")
                        .arg(QString(ui::colors::WARNING()), ci.probe_error.left(200).toHtmlEscaped());
    } else {
        html = QStringLiteral("<span style='color:%1'>●</span> %2 — <a style='color:%3' href='%4'>%5</a>")
                   .arg(QString(ui::colors::NEGATIVE()), tr("Not installed").toHtmlEscaped(),
                        QString(ui::colors::AMBER()), ca::CodingAgentService::install_guide_url(kind),
                        tr("Install guide").toHtmlEscaped());
    }
    agent_status_->setText(html);
    run_btn_->setEnabled(ci.installed());
    claude_btn_->setToolTip(svc.cli_info(ca::AgentKind::Claude).installed() ? QString() : tr("Claude Code CLI not found"));
    codex_btn_->setToolTip(svc.cli_info(ca::AgentKind::Codex).installed() ? QString() : tr("Codex CLI not found"));
}

void AgentRunPanel::refresh_hint() {
    auto& svc = ca::CodingAgentService::instance();
    run_hint_->setText(tr("Ctrl+Enter to run · %1 running · %2 queued · max %3 at once")
                           .arg(svc.running_count())
                           .arg(svc.queued_count())
                           .arg(svc.max_concurrent()));
}

void AgentRunPanel::on_template_activated(int index) {
    if (index <= 0)
        return;
    const auto templates = agent_prompt_templates();
    if (index - 1 >= templates.size())
        return;
    const AgentPromptTemplate& t = templates[index - 1];
    if (prompt_edit_->toPlainText().trimmed().isEmpty()) {
        prompt_edit_->setPlainText(t.body);
    } else {
        QTextCursor c = prompt_edit_->textCursor();
        c.movePosition(QTextCursor::End);
        c.insertText(QStringLiteral("\n\n") + t.body);
    }
    if (t.needs_workspace)
        mode_combo_->setCurrentIndex(1); // the template asks for files in the working folder
    template_combo_->setCurrentIndex(0);
    select_first_placeholder();
    prompt_edit_->setFocus();
}

void AgentRunPanel::select_first_placeholder() {
    const QTextCursor c = prompt_edit_->document()->find(agent_placeholder_regex());
    if (!c.isNull())
        prompt_edit_->setTextCursor(c);
}

void AgentRunPanel::set_composer_prompt(const QString& prompt) {
    prompt_edit_->setPlainText(prompt);
    select_first_placeholder();
}

void AgentRunPanel::on_browse_folder() {
    const QString start = folder_edit_->text().trimmed().isEmpty() ? QDir::homePath() : folder_edit_->text().trimmed();
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Choose the agent's working folder"), start);
    if (!dir.isEmpty())
        folder_edit_->setText(QDir::toNativeSeparators(dir));
}

void AgentRunPanel::on_run() {
    if (!run_btn_->isEnabled())
        return;
    const QString prompt = prompt_edit_->toPlainText().trimmed();
    if (prompt.isEmpty()) {
        prompt_edit_->setFocus();
        return;
    }
    if (prompt.contains(agent_placeholder_regex())) {
        const auto answer = QMessageBox::question(
            this, tr("Agent Workbench"),
            tr("The prompt still contains placeholders such as <TICKER>. Run it anyway?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes) {
            select_first_placeholder();
            prompt_edit_->setFocus();
            return;
        }
    }
    ca::RunRequest req;
    req.agent = selected_agent();
    req.prompt = prompt;
    req.model = model_edit_->text().trimmed();
    req.mode = ca::mode_from_id(mode_combo_->currentData().toString());
    req.use_fincept_tools = tools_check_->isChecked();
    req.max_budget_usd = req.agent == ca::AgentKind::Claude ? budget_spin_->value() : 0.0;
    req.workdir = folder_edit_->text().trimmed();
    req.origin = QStringLiteral("user");

    QString err;
    const QString id = ca::CodingAgentService::instance().start_run(req, &err);
    if (id.isEmpty()) {
        QMessageBox::warning(this, tr("Agent Workbench"), err);
        return;
    }
    show_run(id);
    emit run_started(id);
}

// ── Transcript ───────────────────────────────────────────────────────────────

void AgentRunPanel::refresh_run_picker() {
    auto& svc = ca::CodingAgentService::instance();
    const QSignalBlocker block(run_picker_);
    run_picker_->clear();
    run_picker_->addItem(tr("— select a run —"), QString());
    int n = 0;
    for (const auto& id : svc.run_ids()) {
        const ca::CodingAgentRun* r = svc.find_run(id);
        if (!r)
            continue;
        run_picker_->addItem(QStringLiteral("%1  ·  %2").arg(awb_status_text(r->status), r->title), id);
        if (++n >= kPickerMaxRuns)
            break;
    }
    if (!run_id_.isEmpty() && run_picker_->findData(run_id_) < 0) {
        if (const ca::CodingAgentRun* r = svc.find_run(run_id_))
            run_picker_->addItem(QStringLiteral("%1  ·  %2").arg(awb_status_text(r->status), r->title), run_id_);
    }
    const int idx = run_picker_->findData(run_id_);
    run_picker_->setCurrentIndex(idx < 0 ? 0 : idx);
}

void AgentRunPanel::update_picker_item(const QString& run_id) {
    const int idx = run_picker_->findData(run_id);
    const ca::CodingAgentRun* r = ca::CodingAgentService::instance().find_run(run_id);
    if (idx >= 0 && r)
        run_picker_->setItemText(idx, QStringLiteral("%1  ·  %2").arg(awb_status_text(r->status), r->title));
}

void AgentRunPanel::on_picker_activated(int index) {
    show_run(run_picker_->itemData(index).toString());
}

void AgentRunPanel::show_run(const QString& run_id) {
    run_id_ = run_id;
    stop_clicked_ = false;
    pending_.clear();
    flush_timer_->stop();
    model_->clear_events();
    followup_edit_->clear();
    {
        const QSignalBlocker block(run_picker_);
        int idx = run_picker_->findData(run_id);
        if (idx < 0 && !run_id.isEmpty()) {
            refresh_run_picker();
            idx = run_picker_->findData(run_id);
        }
        run_picker_->setCurrentIndex(idx < 0 ? 0 : idx);
    }
    if (run_id.isEmpty()) {
        refresh_header();
        return;
    }
    auto& svc = ca::CodingAgentService::instance();
    if (svc.has_cached_events(run_id)) {
        loading_ = false;
        model_->reset_events(svc.cached_events(run_id));
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
    model_->append_events(svc.cached_events(run_id_)); // only events newer than the last shown
    if (at_bottom)
        view_->scroll_to_end();
}

void AgentRunPanel::on_run_event(const QString& run_id, const ca::AgentEvent& event) {
    if (run_id != run_id_ || loading_)
        return;
    pending_.append(event);
    if (!flush_timer_->isActive())
        flush_timer_->start();
}

void AgentRunPanel::flush_pending() {
    if (pending_.isEmpty())
        return;
    const bool at_bottom = view_->is_at_bottom();
    model_->append_events(pending_);
    pending_.clear();
    if (at_bottom)
        view_->scroll_to_end();
    if (model_->hidden_count() > 0)
        refresh_header(); // the "earlier events hidden" note changed
}

void AgentRunPanel::on_run_updated(const QString& run_id) {
    update_picker_item(run_id);
    refresh_hint();
    if (run_id == run_id_)
        refresh_header();
}

void AgentRunPanel::on_runs_reset() {
    auto& svc = ca::CodingAgentService::instance();
    refresh_run_picker();
    refresh_hint();
    if (!pending_restore_run_.isEmpty() && svc.is_loaded()) {
        const QString restore = pending_restore_run_;
        pending_restore_run_.clear();
        if (run_id_.isEmpty() && svc.find_run(restore)) {
            show_run(restore);
            return;
        }
    }
    if (!run_id_.isEmpty() && !svc.find_run(run_id_))
        show_run(QString()); // deleted
    else
        refresh_header();
}

void AgentRunPanel::refresh_header() {
    auto& svc = ca::CodingAgentService::instance();
    const ca::CodingAgentRun* run = run_id_.isEmpty() ? nullptr : svc.find_run(run_id_);
    const bool have = run != nullptr;
    stop_btn_->setEnabled(have && !ca::is_terminal_status(run->status));
    stop_btn_->setText(stop_clicked_ ? tr("FORCE STOP") : tr("STOP"));
    open_folder_btn_->setEnabled(have && QFileInfo(run->workdir).isDir());
    view_result_btn_->setEnabled(have && !run->final_text.isEmpty());
    copy_result_btn_->setEnabled(have && !run->final_text.isEmpty());
    export_btn_->setEnabled(have);
    QString why;
    const bool can_follow = have && svc.can_follow_up(run_id_, &why);
    followup_edit_->setEnabled(can_follow);
    followup_btn_->setEnabled(can_follow);
    followup_btn_->setToolTip(can_follow ? QString() : why);

    if (!have) {
        run_title_->setText(tr("No run selected"));
        run_pill_->hide();
        run_meta_->clear();
        error_banner_->hide();
        note_lbl_->hide();
        placeholder_lbl_->setText(
            tr("Compose a prompt on the left and press RUN, or pick a run above.\n\nRuns keep going in the background "
               "when you leave this screen — you get a desktop notification when one finishes."));
        transcript_stack_->setCurrentIndex(0);
        tick_timer_->stop();
        return;
    }

    run_title_->setText(run->title);
    run_title_->setToolTip(run->prompt.left(1500));
    run_pill_->show();
    run_pill_->setText(awb_status_text(run->status));
    if (run_pill_->property("status").toString() != ca::status_id(run->status)) {
        run_pill_->setProperty("status", ca::status_id(run->status));
        awb_repolish(run_pill_);
    }

    QStringList meta;
    meta << awb_agent_text(run->agent);
    meta << (run->model.isEmpty() ? tr("default model") : run->model);
    meta << (run->mode == ca::RunMode::WorkspaceEdits ? tr("workspace edits") : tr("read-only"));
    if (run->status == ca::RunStatus::Queued) {
        const int pos = svc.queue_position(run_id_);
        if (pos > 0)
            meta << tr("queue position %1").arg(pos);
    } else if (run->started_at_ms > 0) {
        meta << awb_duration(awb_run_duration_ms(*run));
    }
    if (run->cost_usd.has_value())
        meta << QStringLiteral("$%1").arg(*run->cost_usd, 0, 'f', 4);
    if (run->input_tokens > 0 || run->output_tokens > 0)
        meta << tr("%1 in / %2 out tok").arg(awb_tokens(run->input_tokens), awb_tokens(run->output_tokens));
    if (!run->session_id.isEmpty())
        meta << tr("session %1").arg(run->session_id.left(8));
    if (!run->use_fincept_tools)
        meta << tr("terminal tools off");
    run_meta_->setText(meta.join(QStringLiteral("  ·  ")));
    run_meta_->setToolTip(QDir::toNativeSeparators(run->workdir));

    const bool failed = run->status == ca::RunStatus::Failed || run->status == ca::RunStatus::Interrupted;
    error_banner_->setVisible(failed && !run->error.isEmpty());
    if (failed && !run->error.isEmpty())
        error_banner_->setText(run->error.left(1200));

    QStringList notes;
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

    if (loading_) {
        placeholder_lbl_->setText(tr("Loading transcript…"));
        transcript_stack_->setCurrentIndex(0);
    } else {
        transcript_stack_->setCurrentIndex(1);
    }

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
        QMessageBox::warning(this, tr("Agent Workbench"), err);
        return;
    }
    followup_edit_->clear();
    show_run(id);
    emit run_started(id);
}

void AgentRunPanel::on_open_folder() {
    const ca::CodingAgentRun* run = ca::CodingAgentService::instance().find_run(run_id_);
    if (run)
        QDesktopServices::openUrl(QUrl::fromLocalFile(run->workdir));
}

void AgentRunPanel::on_view_result() {
    const ca::CodingAgentRun* run = ca::CodingAgentService::instance().find_run(run_id_);
    if (!run || run->final_text.isEmpty())
        return;
    QDialog dlg(this);
    dlg.setObjectName(QStringLiteral("awbDialog"));
    dlg.setWindowTitle(run->title.left(120));
    dlg.resize(900, 680);
    auto* lay = new QVBoxLayout(&dlg);
    auto* browser = new QTextBrowser(&dlg);
    browser->setOpenExternalLinks(true);
    browser->setMarkdown(run->final_text);
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
    const ca::CodingAgentRun* run = ca::CodingAgentService::instance().find_run(run_id);
    if (!run)
        return;
    auto when = [](qint64 ms) {
        return ms > 0 ? QDateTime::fromMSecsSinceEpoch(ms).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")) : QString();
    };
    QString md;
    md += QStringLiteral("# %1\n\n").arg(run->title);
    md += QStringLiteral("| | |\n|---|---|\n");
    md += QStringLiteral("| Run | `%1` |\n").arg(run->id);
    md += QStringLiteral("| Agent | %1 |\n").arg(ca::agent_display_name(run->agent));
    md += QStringLiteral("| Model | %1 |\n").arg(run->model.isEmpty() ? QStringLiteral("CLI default") : run->model);
    md += QStringLiteral("| Mode | %1 |\n").arg(ca::mode_id(run->mode));
    md += QStringLiteral("| Terminal tools | %1 |\n").arg(run->use_fincept_tools ? QStringLiteral("on") : QStringLiteral("off"));
    md += QStringLiteral("| Status | %1 |\n").arg(ca::status_id(run->status));
    md += QStringLiteral("| Started | %1 |\n").arg(when(run->started_at_ms));
    md += QStringLiteral("| Finished | %1 |\n").arg(when(run->finished_at_ms));
    if (run->cost_usd.has_value())
        md += QStringLiteral("| Cost | $%1 |\n").arg(*run->cost_usd, 0, 'f', 4);
    md += QStringLiteral("| Tokens | %1 in / %2 out |\n").arg(run->input_tokens).arg(run->output_tokens);
    if (!run->session_id.isEmpty())
        md += QStringLiteral("| Session | `%1` |\n").arg(run->session_id);
    md += QStringLiteral("| Working folder | `%1` |\n").arg(QDir::toNativeSeparators(run->workdir));
    md += QStringLiteral("| Origin | %1 |\n\n").arg(run->origin);
    md += QStringLiteral("## Prompt\n\n%1\n\n## Transcript\n\n").arg(run->prompt);
    for (const auto& ev : events) {
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

    QSaveFile f(path);
    const QByteArray bytes = md.toUtf8();
    if (!f.open(QIODevice::WriteOnly) || f.write(bytes) != bytes.size() || !f.commit()) {
        QMessageBox::warning(this, tr("Agent Workbench"), tr("Could not write %1: %2").arg(path, f.errorString()));
        return;
    }
}

// ── State ────────────────────────────────────────────────────────────────────

QVariantMap AgentRunPanel::save_state() const {
    return {{"agent", ca::agent_id(selected_agent())},
            {"mode", mode_combo_->currentData().toString()},
            {"tools", tools_check_->isChecked()},
            {"model", model_edit_->text()},
            {"budget", budget_spin_->value()},
            {"run_id", run_id_}};
}

void AgentRunPanel::restore_state(const QVariantMap& state) {
    if (ca::agent_from_id(state.value("agent").toString()) == ca::AgentKind::Codex)
        codex_btn_->setChecked(true);
    else
        claude_btn_->setChecked(true);
    const int mode_idx = mode_combo_->findData(state.value("mode").toString());
    if (mode_idx >= 0)
        mode_combo_->setCurrentIndex(mode_idx);
    tools_check_->setChecked(state.value("tools", true).toBool());
    model_edit_->setText(state.value("model").toString());
    budget_spin_->setValue(state.value("budget", 0.0).toDouble());
    on_agent_changed();
    const QString run_id = state.value("run_id").toString();
    auto& svc = ca::CodingAgentService::instance();
    if (!run_id.isEmpty() && svc.find_run(run_id))
        show_run(run_id);
    else if (!run_id.isEmpty() && !svc.is_loaded())
        pending_restore_run_ = run_id; // history still loading — selected in on_runs_reset()
}

} // namespace fincept::screens
