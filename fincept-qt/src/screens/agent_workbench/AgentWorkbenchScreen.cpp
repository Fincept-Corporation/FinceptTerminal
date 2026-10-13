// AgentWorkbenchScreen.cpp — root of the Agent Workbench: header, tabs, the one
// stylesheet, settings dialog and IStatefulScreen persistence.

#include "screens/agent_workbench/AgentWorkbenchScreen.h"

#include "core/logging/Logger.h"
#include "mcp/server/McpHttpServer.h"
#include "screens/agent_workbench/AgentConnectPanel.h"
#include "screens/agent_workbench/AgentRunPanel.h"
#include "screens/agent_workbench/AgentRunsPanel.h"
#include "screens/agent_workbench/AgentSchedulesPanel.h"
#include "screens/agent_workbench/AgentWorkbenchCommon.h"
#include "services/coding_agents/CodingAgentService.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QCheckBox>
#include <QDesktopServices>
#include <QDialog>
#include <QDir>
#include <QEvent>
#include <QFileDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>
#include <vector>

namespace fincept::screens {

namespace ca = services::coding_agents;

namespace {

enum WorkbenchTab { kTabRun = 0, kTabRuns, kTabSchedules, kTabConnect, kTabCount };

/// The workbench's only stylesheet. Tokens (@name) are replaced from the live
/// theme so a theme switch just re-applies it.
QString workbench_qss() {
    using namespace ui::colors;
    QString qss = QStringLiteral(R"QSS(
#awbScreen { background: @bg_base; }
#awbScreen QWidget#awbComposer, #awbScreen QWidget#awbTranscriptPane, #awbScreen QWidget#awbPage { background: @bg_base; }
#awbHeader { background: @bg_raised; border-bottom: 1px solid @border_dim; }
#awbTitle { color: @amber; font-weight: 700; letter-spacing: 2px; font-size: @title_size; background: transparent; }
#awbSummary { color: @text_secondary; letter-spacing: 1px; background: transparent; }
#awbScreen QPushButton#awbTabBtn { background: transparent; color: @text_secondary; border: 1px solid @border_dim; padding: 5px 16px; font-weight: 700; letter-spacing: 1px; }
#awbScreen QPushButton#awbTabBtn:hover { color: @text_primary; border-color: @border_bright; }
#awbScreen QPushButton#awbTabBtn[active="true"] { background: @amber; color: @text_on_accent; border-color: @amber; }
#awbSection { color: @amber; font-weight: 700; letter-spacing: 2px; background: transparent; }
#awbLabel { color: @text_secondary; font-weight: 700; letter-spacing: 1px; background: transparent; }
#awbHint { color: @text_tertiary; background: transparent; }
#awbRunTitle { color: @text_primary; font-weight: 700; font-size: @title_size; background: transparent; }
#awbPlaceholder { color: @text_tertiary; background: @bg_base; border: 1px solid @border_dim; padding: 24px; }
#awbErrorBanner { color: @negative; background: @negative_bg; border: 1px solid @negative; padding: 6px 8px; }
#awbCard { background: @bg_surface; border: 1px solid @border_dim; }
#awbCardTitle { color: @text_primary; font-weight: 700; letter-spacing: 1px; background: transparent; }
#awbScreen QLineEdit, #awbScreen QPlainTextEdit, #awbScreen QTextBrowser, #awbScreen QComboBox, #awbScreen QSpinBox, #awbScreen QDoubleSpinBox, #awbScreen QTimeEdit { background: @bg_surface; color: @text_primary; border: 1px solid @border_med; padding: 3px 6px; selection-background-color: @amber_dim; selection-color: @text_primary; }
#awbScreen QLineEdit:focus, #awbScreen QPlainTextEdit:focus, #awbScreen QComboBox:focus, #awbScreen QSpinBox:focus, #awbScreen QDoubleSpinBox:focus, #awbScreen QTimeEdit:focus { border-color: @amber; }
#awbScreen QLineEdit:disabled, #awbScreen QDoubleSpinBox:disabled, #awbScreen QSpinBox:disabled, #awbScreen QTimeEdit:disabled { color: @text_dim; border-color: @border_dim; }
#awbScreen QComboBox QAbstractItemView { background: @bg_surface; color: @text_primary; selection-background-color: @accent_bg; selection-color: @amber; border: 1px solid @border_med; }
#awbScreen QLineEdit#awbSnippet, #awbScreen QPlainTextEdit#awbSnippet { color: @cyan; background: @bg_base; }
#awbScreen QPushButton#awbBtn { background: @bg_raised; color: @text_primary; border: 1px solid @border_med; padding: 4px 12px; font-weight: 700; letter-spacing: 1px; }
#awbScreen QPushButton#awbBtn:hover { color: @amber; border-color: @amber; }
#awbScreen QPushButton#awbBtn:disabled { color: @text_dim; border-color: @border_dim; }
#awbScreen QPushButton#awbPrimaryBtn { background: @amber; color: @text_on_accent; border: 1px solid @amber; padding: 6px 18px; font-weight: 700; letter-spacing: 1px; }
#awbScreen QPushButton#awbPrimaryBtn:hover { background: @amber_dim; border-color: @amber_dim; }
#awbScreen QPushButton#awbPrimaryBtn:disabled { background: @bg_raised; color: @text_dim; border-color: @border_dim; }
#awbScreen QPushButton#awbDangerBtn { background: transparent; color: @negative; border: 1px solid @negative; padding: 4px 12px; font-weight: 700; letter-spacing: 1px; }
#awbScreen QPushButton#awbDangerBtn:hover { background: @negative_bg; }
#awbScreen QPushButton#awbDangerBtn:disabled { color: @text_dim; border-color: @border_dim; }
#awbScreen QPushButton#awbAgentBtn { background: @bg_surface; color: @text_secondary; border: 1px solid @border_med; padding: 7px 10px; font-weight: 700; letter-spacing: 1px; }
#awbScreen QPushButton#awbAgentBtn:hover { color: @text_primary; }
#awbScreen QPushButton#awbAgentBtn:checked { color: @amber; border-color: @amber; background: @accent_bg; }
QLabel#awbPill { color: @text_secondary; border: 1px solid @border_med; padding: 2px 10px; font-weight: 700; letter-spacing: 1px; background: transparent; }
QLabel#awbPill[status="queued"] { color: @info; border-color: @info; }
QLabel#awbPill[status="running"] { color: @amber; border-color: @amber; }
QLabel#awbPill[status="succeeded"] { color: @positive; border-color: @positive; }
QLabel#awbPill[status="failed"], QLabel#awbPill[status="interrupted"] { color: @negative; border-color: @negative; }
QLabel#awbPill[status="cancelled"] { color: @text_tertiary; border-color: @border_med; }
QLabel#awbServerState { color: @text_tertiary; font-weight: 700; letter-spacing: 1px; background: transparent; }
QLabel#awbServerState[state="running"] { color: @positive; }
QLabel#awbServerState[state="error"] { color: @negative; }
QLabel#awbResult { color: @text_secondary; background: transparent; }
QLabel#awbResult[state="ok"] { color: @positive; }
QLabel#awbResult[state="error"] { color: @negative; }
#awbScreen QCheckBox, #awbScreen QRadioButton { color: @text_primary; background: transparent; spacing: 6px; }
#awbScreen QCheckBox#awbDangerToggle { color: @negative; font-weight: 700; }
#awbScreen QTableView { background: @bg_base; alternate-background-color: @row_alt; color: @text_primary; border: 1px solid @border_dim; gridline-color: @border_dim; selection-background-color: @bg_hover; selection-color: @text_primary; }
#awbScreen QHeaderView::section { background: @bg_raised; color: @text_secondary; border: none; border-right: 1px solid @border_dim; border-bottom: 1px solid @border_dim; padding: 4px 6px; font-weight: 700; letter-spacing: 1px; }
#awbScreen QListView#awbTranscript { background: @bg_base; border: 1px solid @border_dim; outline: none; }
#awbScreen QListWidget { background: @bg_surface; color: @text_primary; border: 1px solid @border_dim; outline: none; }
#awbScreen QListWidget::item { padding: 6px 8px; border-bottom: 1px solid @border_dim; }
#awbScreen QListWidget::item:selected { background: @accent_bg; color: @amber; }
#awbScreen QSplitter::handle { background: @border_dim; }
#awbScreen QScrollArea { background: transparent; border: none; }
#awbScreen QScrollBar:vertical { background: @bg_base; width: 8px; margin: 0; }
#awbScreen QScrollBar::handle:vertical { background: @border_med; min-height: 24px; }
#awbScreen QScrollBar::add-line:vertical, #awbScreen QScrollBar::sub-line:vertical { height: 0; }
QDialog#awbDialog { background: @bg_surface; }
)QSS");
    std::vector<std::pair<QString, QString>> tokens = {
        {QStringLiteral("@bg_base"), BG_BASE()},
        {QStringLiteral("@bg_surface"), BG_SURFACE()},
        {QStringLiteral("@bg_raised"), BG_RAISED()},
        {QStringLiteral("@bg_hover"), BG_HOVER()},
        {QStringLiteral("@border_dim"), BORDER_DIM()},
        {QStringLiteral("@border_med"), BORDER_MED()},
        {QStringLiteral("@border_bright"), BORDER_BRIGHT()},
        {QStringLiteral("@text_primary"), TEXT_PRIMARY()},
        {QStringLiteral("@text_secondary"), TEXT_SECONDARY()},
        {QStringLiteral("@text_tertiary"), TEXT_TERTIARY()},
        {QStringLiteral("@text_dim"), TEXT_DIM()},
        {QStringLiteral("@text_on_accent"), TEXT_ON_ACCENT()},
        {QStringLiteral("@amber_dim"), AMBER_DIM()},
        {QStringLiteral("@amber"), AMBER()},
        {QStringLiteral("@accent_bg"), ACCENT_BG()},
        {QStringLiteral("@positive"), POSITIVE()},
        {QStringLiteral("@negative_bg"), NEGATIVE_BG()},
        {QStringLiteral("@negative"), NEGATIVE()},
        {QStringLiteral("@info"), INFO()},
        {QStringLiteral("@cyan"), CYAN()},
        {QStringLiteral("@row_alt"), ROW_ALT()},
        {QStringLiteral("@title_size"), QString::number(ui::fonts::font_px(1)) + QStringLiteral("px")},
    };
    // Longest token first so "@amber" never eats the prefix of "@amber_dim".
    std::sort(tokens.begin(), tokens.end(),
              [](const auto& a, const auto& b) { return a.first.size() > b.first.size(); });
    for (const auto& [key, value] : tokens)
        qss.replace(key, value);
    return qss;
}

} // namespace

AgentWorkbenchScreen::AgentWorkbenchScreen(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("awbScreen"));
    apply_theme();
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this,
            [this](const ui::ThemeTokens&) { apply_theme(); });

    ca::CodingAgentService::instance().initialize_deferred(); // cheap + idempotent

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* header = new QWidget(this);
    header->setObjectName(QStringLiteral("awbHeader"));
    auto* hl = new QHBoxLayout(header);
    hl->setContentsMargins(12, 8, 12, 8);
    hl->setSpacing(6);
    title_ = new QLabel(header);
    title_->setObjectName(QStringLiteral("awbTitle"));
    hl->addWidget(title_);
    hl->addSpacing(16);
    for (int i = 0; i < kTabCount; ++i) {
        auto* b = new QPushButton(header);
        b->setObjectName(QStringLiteral("awbTabBtn"));
        b->setCursor(Qt::PointingHandCursor);
        connect(b, &QPushButton::clicked, this, [this, i]() { set_tab(i); });
        tab_btns_.append(b);
        hl->addWidget(b);
    }
    hl->addStretch(1);
    summary_ = new QLabel(header);
    summary_->setObjectName(QStringLiteral("awbSummary"));
    hl->addWidget(summary_);
    settings_btn_ = new QPushButton(header);
    settings_btn_->setObjectName(QStringLiteral("awbBtn"));
    settings_btn_->setCursor(Qt::PointingHandCursor);
    connect(settings_btn_, &QPushButton::clicked, this, &AgentWorkbenchScreen::open_settings);
    hl->addWidget(settings_btn_);
    root->addWidget(header);

    stack_ = new QStackedWidget(this);
    run_panel_ = new AgentRunPanel(stack_);
    runs_panel_ = new AgentRunsPanel(stack_);
    schedules_panel_ = new AgentSchedulesPanel(stack_);
    connect_panel_ = new AgentConnectPanel(stack_);
    stack_->addWidget(run_panel_);
    stack_->addWidget(runs_panel_);
    stack_->addWidget(schedules_panel_);
    stack_->addWidget(connect_panel_);
    root->addWidget(stack_, 1);

    connect(runs_panel_, &AgentRunsPanel::open_run_requested, this, &AgentWorkbenchScreen::open_run);
    connect(schedules_panel_, &AgentSchedulesPanel::open_run_requested, this, &AgentWorkbenchScreen::open_run);

    retranslate();
    set_tab(kTabRun);
    LOG_INFO("AgentWorkbench", "Screen constructed");
}

AgentWorkbenchScreen::~AgentWorkbenchScreen() {
    if (visibility_reported_)
        ca::CodingAgentService::instance().set_view_visible(false);
}

void AgentWorkbenchScreen::apply_theme() {
    setStyleSheet(workbench_qss());
}

void AgentWorkbenchScreen::retranslate() {
    title_->setText(tr("AGENT WORKBENCH"));
    const QStringList labels = {tr("RUN"), tr("RUNS"), tr("SCHEDULES"), tr("CONNECT")};
    for (int i = 0; i < tab_btns_.size() && i < labels.size(); ++i)
        tab_btns_[i]->setText(labels[i]);
    settings_btn_->setText(tr("SETTINGS"));
    settings_btn_->setToolTip(tr("CLI paths, concurrency and notifications"));
    update_summary();
}

void AgentWorkbenchScreen::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslate();
    QWidget::changeEvent(event);
}

void AgentWorkbenchScreen::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    auto& svc = ca::CodingAgentService::instance();
    if (!visibility_reported_) {
        svc.set_view_visible(true);
        visibility_reported_ = true;
    }
    if (!cli_probed_) {
        cli_probed_ = true;
        svc.refresh_cli_detection(); // paths now, versions async
    }
    if (conns_.isEmpty()) {
        auto refresh = [this]() { update_summary(); };
        conns_ << connect(&svc, &ca::CodingAgentService::run_added, this, refresh);
        conns_ << connect(&svc, &ca::CodingAgentService::run_finished, this, refresh);
        conns_ << connect(&svc, &ca::CodingAgentService::runs_reset, this, refresh);
        conns_ << connect(&svc, &ca::CodingAgentService::run_updated, this, refresh);
        conns_ << connect(&mcp::McpHttpServer::instance(), &mcp::McpHttpServer::status_changed, this, refresh);
    }
    update_summary();
}

void AgentWorkbenchScreen::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    if (visibility_reported_) {
        ca::CodingAgentService::instance().set_view_visible(false);
        visibility_reported_ = false;
    }
    for (const auto& c : std::as_const(conns_))
        disconnect(c);
    conns_.clear();
}

void AgentWorkbenchScreen::set_tab(int index) {
    if (index < 0 || index >= kTabCount)
        index = kTabRun;
    tab_ = index;
    stack_->setCurrentIndex(index);
    for (int i = 0; i < tab_btns_.size(); ++i) {
        const bool active = i == index;
        if (tab_btns_[i]->property("active").toBool() != active || !tab_btns_[i]->property("active").isValid()) {
            tab_btns_[i]->setProperty("active", active);
            awb_repolish(tab_btns_[i]);
        }
    }
}

void AgentWorkbenchScreen::update_summary() {
    auto& svc = ca::CodingAgentService::instance();
    auto& srv = mcp::McpHttpServer::instance();
    QStringList parts;
    parts << tr("%1 RUNNING").arg(svc.running_count());
    parts << tr("%1 QUEUED").arg(svc.queued_count());
    parts << (srv.is_running() ? tr("MCP ● ON :%1").arg(srv.port()) : tr("MCP ○ OFF"));
    summary_->setText(parts.join(QStringLiteral("  ·  ")));
}

void AgentWorkbenchScreen::open_run(const QString& run_id) {
    set_tab(kTabRun);
    run_panel_->show_run(run_id);
}

void AgentWorkbenchScreen::open_settings() {
    auto& svc = ca::CodingAgentService::instance();
    QDialog dlg(this);
    dlg.setObjectName(QStringLiteral("awbDialog"));
    dlg.setWindowTitle(tr("Agent Workbench settings"));
    dlg.setMinimumWidth(640);
    auto* v = new QVBoxLayout(&dlg);
    auto* grid = new QGridLayout;
    grid->setHorizontalSpacing(8);
    grid->setVerticalSpacing(8);

    struct PathRow {
        ca::AgentKind kind;
        QLineEdit* edit;
    };
    QList<PathRow> rows;
    int r = 0;
    for (ca::AgentKind kind : {ca::AgentKind::Claude, ca::AgentKind::Codex}) {
        auto* lbl = new QLabel(kind == ca::AgentKind::Claude ? tr("CLAUDE CODE PATH") : tr("CODEX PATH"), &dlg);
        lbl->setObjectName(QStringLiteral("awbLabel"));
        auto* edit = new QLineEdit(&dlg);
        edit->setText(QDir::toNativeSeparators(svc.cli_path_override(kind)));
        const ca::CliInfo ci = svc.cli_info(kind);
        edit->setPlaceholderText(ci.installed() && !ci.path_overridden
                                     ? tr("Auto-detected: %1").arg(QDir::toNativeSeparators(ci.path))
                                     : tr("Auto-detect"));
        edit->setClearButtonEnabled(true);
        auto* browse = new QPushButton(tr("BROWSE"), &dlg);
        browse->setObjectName(QStringLiteral("awbBtn"));
        connect(browse, &QPushButton::clicked, &dlg, [&dlg, edit]() {
#ifdef Q_OS_WIN
            const QString filter = AgentWorkbenchScreen::tr("Executables (*.exe *.cmd *.bat);;All files (*)");
#else
            const QString filter = AgentWorkbenchScreen::tr("All files (*)");
#endif
            const QString path = QFileDialog::getOpenFileName(&dlg, AgentWorkbenchScreen::tr("Select the CLI executable"),
                                                              QDir::homePath(), filter);
            if (!path.isEmpty())
                edit->setText(QDir::toNativeSeparators(path));
        });
        grid->addWidget(lbl, r, 0);
        grid->addWidget(edit, r, 1);
        grid->addWidget(browse, r, 2);
        rows.append({kind, edit});
        ++r;
    }

    auto* conc_lbl = new QLabel(tr("MAX CONCURRENT RUNS"), &dlg);
    conc_lbl->setObjectName(QStringLiteral("awbLabel"));
    auto* conc_spin = new QSpinBox(&dlg);
    conc_spin->setRange(1, ca::CodingAgentService::kMaxConcurrentLimit);
    conc_spin->setValue(svc.max_concurrent());
    auto* conc_hint = new QLabel(tr("Extra runs wait in a queue."), &dlg);
    conc_hint->setObjectName(QStringLiteral("awbHint"));
    grid->addWidget(conc_lbl, r, 0);
    grid->addWidget(conc_spin, r, 1, Qt::AlignLeft);
    grid->addWidget(conc_hint, r, 2);
    ++r;

    auto* notify = new QCheckBox(tr("Desktop notification when a run finishes while the workbench is not in view"), &dlg);
    notify->setChecked(svc.notify_on_finish());
    grid->addWidget(notify, r, 1, 1, 2);
    ++r;

    auto* ws_lbl = new QLabel(tr("WORKSPACE"), &dlg);
    ws_lbl->setObjectName(QStringLiteral("awbLabel"));
    auto* ws_path = new QLabel(QDir::toNativeSeparators(ca::CodingAgentService::workspace_root()), &dlg);
    ws_path->setObjectName(QStringLiteral("awbHint"));
    ws_path->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto* ws_open = new QPushButton(tr("OPEN"), &dlg);
    ws_open->setObjectName(QStringLiteral("awbBtn"));
    connect(ws_open, &QPushButton::clicked, &dlg, []() {
        const QString root = ca::CodingAgentService::workspace_root();
        QDir().mkpath(root);
        QDesktopServices::openUrl(QUrl::fromLocalFile(root));
    });
    grid->addWidget(ws_lbl, r, 0);
    grid->addWidget(ws_path, r, 1);
    grid->addWidget(ws_open, r, 2);
    grid->setColumnStretch(1, 1);
    v->addLayout(grid);

    auto* buttons = new QHBoxLayout;
    auto* redetect = new QPushButton(tr("RE-DETECT"), &dlg);
    redetect->setObjectName(QStringLiteral("awbBtn"));
    connect(redetect, &QPushButton::clicked, &dlg, []() { ca::CodingAgentService::instance().refresh_cli_detection(); });
    buttons->addWidget(redetect);
    buttons->addStretch(1);
    auto* cancel = new QPushButton(tr("CANCEL"), &dlg);
    cancel->setObjectName(QStringLiteral("awbBtn"));
    connect(cancel, &QPushButton::clicked, &dlg, &QDialog::reject);
    auto* save = new QPushButton(tr("SAVE"), &dlg);
    save->setObjectName(QStringLiteral("awbPrimaryBtn"));
    connect(save, &QPushButton::clicked, &dlg, &QDialog::accept);
    buttons->addWidget(cancel);
    buttons->addWidget(save);
    v->addLayout(buttons);

    if (dlg.exec() != QDialog::Accepted)
        return;
    for (const auto& row : rows) {
        const QString path = QDir::fromNativeSeparators(row.edit->text().trimmed());
        if (path != svc.cli_path_override(row.kind))
            svc.set_cli_path_override(row.kind, path);
    }
    svc.set_max_concurrent(conc_spin->value());
    svc.set_notify_on_finish(notify->isChecked());
    update_summary();
}

QVariantMap AgentWorkbenchScreen::save_state() const {
    return {{"tab", tab_}, {"run", run_panel_->save_state()}};
}

void AgentWorkbenchScreen::restore_state(const QVariantMap& state) {
    run_panel_->restore_state(state.value("run").toMap());
    set_tab(state.value("tab", 0).toInt());
}

} // namespace fincept::screens
