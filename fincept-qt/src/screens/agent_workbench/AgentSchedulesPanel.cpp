// AgentSchedulesPanel.cpp — SCHEDULES tab: list + editor for recurring runs.

#include "screens/agent_workbench/AgentSchedulesPanel.h"

#include "screens/agent_workbench/AgentWorkbenchCommon.h"
#include "screens/agent_workbench/AgentWorkbenchTemplates.h"
#include "services/coding_agents/CodingAgentService.h"
#include "ui/theme/Theme.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QSplitter>
#include <QTextCursor>
#include <QTextDocument>
#include <QTime>
#include <QTimeEdit>
#include <QVBoxLayout>

namespace fincept::screens {

namespace ca = services::coding_agents;

namespace {

QLabel* sched_label(const char* object_name, QWidget* parent) {
    auto* l = new QLabel(parent);
    l->setObjectName(QString::fromLatin1(object_name));
    return l;
}

QPushButton* sched_button(const char* object_name, QWidget* parent) {
    auto* b = new QPushButton(parent);
    b->setObjectName(QString::fromLatin1(object_name));
    b->setCursor(Qt::PointingHandCursor);
    return b;
}

} // namespace

AgentSchedulesPanel::AgentSchedulesPanel(QWidget* parent) : QWidget(parent) {
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setChildrenCollapsible(false);
    root->addWidget(splitter);

    // ── List ────────────────────────────────────────────────────────────────
    auto* left = new QWidget(splitter);
    left->setObjectName(QStringLiteral("awbPage"));
    auto* lv = new QVBoxLayout(left);
    lv->setContentsMargins(12, 10, 8, 10);
    lv->setSpacing(6);
    list_title_ = sched_label("awbSection", left);
    lv->addWidget(list_title_);
    list_ = new QListWidget(left);
    list_->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(list_, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row < 0)
            return;
        load_into_form(list_->item(row)->data(Qt::UserRole).toString());
    });
    lv->addWidget(list_, 1);
    auto* list_btns = new QHBoxLayout;
    new_btn_ = sched_button("awbBtn", left);
    delete_btn_ = sched_button("awbDangerBtn", left);
    connect(new_btn_, &QPushButton::clicked, this, &AgentSchedulesPanel::on_new);
    connect(delete_btn_, &QPushButton::clicked, this, &AgentSchedulesPanel::on_delete);
    list_btns->addWidget(new_btn_);
    list_btns->addWidget(delete_btn_);
    list_btns->addStretch(1);
    lv->addLayout(list_btns);
    splitter->addWidget(left);

    // ── Editor ──────────────────────────────────────────────────────────────
    auto* scroll = new QScrollArea(splitter);
    scroll->setWidgetResizable(true);
    auto* form = new QWidget(scroll);
    form->setObjectName(QStringLiteral("awbPage"));
    auto* fv = new QVBoxLayout(form);
    fv->setContentsMargins(12, 10, 14, 10);
    fv->setSpacing(6);
    form_title_ = sched_label("awbSection", form);
    fv->addWidget(form_title_);

    auto* grid = new QGridLayout;
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(6);
    int row = 0;
    name_lbl_ = sched_label("awbLabel", form);
    name_edit_ = new QLineEdit(form);
    name_edit_->setMaxLength(120);
    grid->addWidget(name_lbl_, row, 0);
    grid->addWidget(name_edit_, row++, 1, 1, 3);

    agent_lbl_ = sched_label("awbLabel", form);
    agent_combo_ = new QComboBox(form);
    agent_combo_->addItem(QString(), ca::agent_id(ca::AgentKind::Claude));
    agent_combo_->addItem(QString(), ca::agent_id(ca::AgentKind::Codex));
    connect(agent_combo_, &QComboBox::currentIndexChanged, this, [this](int) { on_agent_changed(); });
    model_lbl_ = sched_label("awbLabel", form);
    model_edit_ = new QLineEdit(form);
    model_edit_->setMaxLength(100);
    grid->addWidget(agent_lbl_, row, 0);
    grid->addWidget(agent_combo_, row, 1);
    grid->addWidget(model_lbl_, row, 2);
    grid->addWidget(model_edit_, row++, 3);

    mode_lbl_ = sched_label("awbLabel", form);
    mode_combo_ = new QComboBox(form);
    mode_combo_->addItem(QString(), ca::mode_id(ca::RunMode::Analysis));
    mode_combo_->addItem(QString(), ca::mode_id(ca::RunMode::WorkspaceEdits));
    budget_lbl_ = sched_label("awbLabel", form);
    budget_spin_ = new QDoubleSpinBox(form);
    budget_spin_->setRange(0.0, 1000.0);
    budget_spin_->setDecimals(2);
    budget_spin_->setSingleStep(0.5);
    budget_spin_->setPrefix(QStringLiteral("$ "));
    grid->addWidget(mode_lbl_, row, 0);
    grid->addWidget(mode_combo_, row, 1);
    grid->addWidget(budget_lbl_, row, 2);
    grid->addWidget(budget_spin_, row++, 3);

    tools_check_ = new QCheckBox(form);
    tools_check_->setChecked(true);
    grid->addWidget(tools_check_, row++, 1, 1, 3);

    when_lbl_ = sched_label("awbLabel", form);
    interval_radio_ = new QRadioButton(form);
    daily_radio_ = new QRadioButton(form);
    auto* kind_group = new QButtonGroup(this);
    kind_group->addButton(interval_radio_);
    kind_group->addButton(daily_radio_);
    daily_radio_->setChecked(true);
    connect(interval_radio_, &QRadioButton::toggled, this, [this](bool) { on_kind_changed(); });
    interval_spin_ = new QSpinBox(form);
    interval_spin_->setRange(ca::kScheduleMinIntervalMinutes, 7 * 24 * 60);
    interval_spin_->setSingleStep(15);
    interval_spin_->setValue(60);
    time_edit_ = new QTimeEdit(form);
    time_edit_->setDisplayFormat(QStringLiteral("HH:mm"));
    time_edit_->setTime(QTime(8, 0));
    grid->addWidget(when_lbl_, row, 0);
    auto* when_row = new QHBoxLayout;
    when_row->setSpacing(8);
    when_row->addWidget(daily_radio_);
    when_row->addWidget(time_edit_);
    when_row->addSpacing(16);
    when_row->addWidget(interval_radio_);
    when_row->addWidget(interval_spin_);
    when_row->addStretch(1);
    grid->addLayout(when_row, row++, 1, 1, 3);

    auto* days_row = new QHBoxLayout;
    days_row->setSpacing(8);
    for (int d = 1; d <= 7; ++d) {
        auto* cb = new QCheckBox(QLocale().dayName(d, QLocale::ShortFormat), form);
        cb->setChecked(d <= 5);
        day_checks_.append(cb);
        days_row->addWidget(cb);
    }
    days_row->addStretch(1);
    grid->addLayout(days_row, row++, 1, 1, 3);

    enabled_check_ = new QCheckBox(form);
    enabled_check_->setChecked(true);
    grid->addWidget(enabled_check_, row++, 1, 1, 3);

    template_lbl_ = sched_label("awbLabel", form);
    template_combo_ = new QComboBox(form);
    connect(template_combo_, &QComboBox::activated, this, &AgentSchedulesPanel::on_template);
    grid->addWidget(template_lbl_, row, 0);
    grid->addWidget(template_combo_, row++, 1, 1, 3);
    grid->setColumnStretch(1, 1);
    grid->setColumnStretch(3, 1);
    fv->addLayout(grid);

    prompt_lbl_ = sched_label("awbLabel", form);
    fv->addWidget(prompt_lbl_);
    prompt_edit_ = new QPlainTextEdit(form);
    prompt_edit_->setMinimumHeight(160);
    prompt_edit_->setTabChangesFocus(true);
    highlighter_ = new AgentPlaceholderHighlighter(prompt_edit_->document());
    fv->addWidget(prompt_edit_, 1);

    info_lbl_ = sched_label("awbHint", form);
    info_lbl_->setWordWrap(true);
    fv->addWidget(info_lbl_);

    auto* actions = new QHBoxLayout;
    actions->addStretch(1);
    run_now_btn_ = sched_button("awbBtn", form);
    save_btn_ = sched_button("awbPrimaryBtn", form);
    connect(run_now_btn_, &QPushButton::clicked, this, &AgentSchedulesPanel::on_run_now);
    connect(save_btn_, &QPushButton::clicked, this, [this]() { on_save(); });
    actions->addWidget(run_now_btn_);
    actions->addWidget(save_btn_);
    fv->addLayout(actions);
    scroll->setWidget(form);
    splitter->addWidget(scroll);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({320, 900});

    retranslate();
    on_kind_changed();
    on_agent_changed();
    clear_form();
}

void AgentSchedulesPanel::retranslate() {
    list_title_->setText(tr("SCHEDULES"));
    new_btn_->setText(tr("NEW"));
    delete_btn_->setText(tr("DELETE"));
    form_title_->setText(editing_id_.isEmpty() ? tr("NEW SCHEDULE") : tr("EDIT SCHEDULE"));
    name_lbl_->setText(tr("NAME"));
    name_edit_->setPlaceholderText(tr("e.g. Morning brief"));
    agent_lbl_->setText(tr("AGENT"));
    agent_combo_->setItemText(0, tr("Claude Code"));
    agent_combo_->setItemText(1, tr("Codex"));
    model_lbl_->setText(tr("MODEL"));
    model_edit_->setPlaceholderText(tr("CLI default"));
    mode_lbl_->setText(tr("MODE"));
    mode_combo_->setItemText(0, tr("Analysis (read-only)"));
    mode_combo_->setItemText(1, tr("Workspace edits"));
    budget_lbl_->setText(tr("MAX BUDGET"));
    budget_spin_->setSpecialValueText(tr("No cap"));
    tools_check_->setText(tr("Use Fincept terminal tools (live data over MCP)"));
    when_lbl_->setText(tr("WHEN"));
    daily_radio_->setText(tr("Daily at"));
    interval_radio_->setText(tr("Every"));
    interval_spin_->setSuffix(tr(" min"));
    enabled_check_->setText(tr("Enabled"));
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
    prompt_edit_->setPlaceholderText(tr("What should the agent do on every run?"));
    run_now_btn_->setText(tr("RUN NOW"));
    save_btn_->setText(tr("SAVE"));
    reload_list();
    refresh_info();
}

void AgentSchedulesPanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslate();
    QWidget::changeEvent(event);
}

void AgentSchedulesPanel::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    subscribe();
    reload_list();
    refresh_info();
}

void AgentSchedulesPanel::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    unsubscribe();
}

void AgentSchedulesPanel::subscribe() {
    if (!conns_.isEmpty())
        return;
    auto& svc = ca::CodingAgentService::instance();
    conns_ << connect(&svc, &ca::CodingAgentService::schedules_changed, this, [this]() {
        reload_list();
        refresh_info();
    });
    conns_ << connect(&svc, &ca::CodingAgentService::run_finished, this, [this](const QString&) { refresh_info(); });
    conns_ << connect(&svc, &ca::CodingAgentService::run_added, this, [this](const QString&) { refresh_info(); });
}

void AgentSchedulesPanel::unsubscribe() {
    for (const auto& c : std::as_const(conns_))
        disconnect(c);
    conns_.clear();
}

QString AgentSchedulesPanel::describe(const ca::CodingAgentSchedule& s) const {
    if (s.kind == ca::ScheduleKind::Interval) {
        if (s.interval_minutes % 60 == 0)
            return tr("Every %1 h").arg(s.interval_minutes / 60);
        return tr("Every %1 min").arg(s.interval_minutes);
    }
    QString days;
    const int mask = s.weekdays_mask & 0x7F;
    if (mask == 0x7F)
        days = tr("every day");
    else if (mask == 0x1F)
        days = tr("Mon–Fri");
    else if (mask == 0x60)
        days = tr("weekends");
    else {
        QStringList names;
        for (int d = 1; d <= 7; ++d) {
            if (mask & (1 << (d - 1)))
                names << QLocale().dayName(d, QLocale::ShortFormat);
        }
        days = names.join(QStringLiteral(", "));
    }
    return tr("Daily %1 · %2").arg(s.daily_time, days);
}

void AgentSchedulesPanel::reload_list() {
    auto& svc = ca::CodingAgentService::instance();
    const QSignalBlocker block(list_);
    list_->clear();
    int select_row = -1;
    for (const auto& s : svc.schedules()) {
        QString when;
        if (!s.enabled)
            when = tr("paused");
        else if (s.next_run_at_ms > 0)
            when = tr("next %1").arg(QDateTime::fromMSecsSinceEpoch(s.next_run_at_ms).toString(QStringLiteral("ddd HH:mm")));
        else
            when = tr("not scheduled");
        auto* item = new QListWidgetItem(QStringLiteral("%1\n%2 · %3 · %4")
                                             .arg(s.name, awb_agent_text(s.agent), describe(s), when),
                                         list_);
        item->setData(Qt::UserRole, s.id);
        item->setForeground(QColor(s.enabled ? ui::colors::TEXT_PRIMARY() : ui::colors::TEXT_TERTIARY()));
        if (s.id == editing_id_)
            select_row = list_->count() - 1;
    }
    if (select_row >= 0)
        list_->setCurrentRow(select_row);
    delete_btn_->setEnabled(!editing_id_.isEmpty());
}

void AgentSchedulesPanel::clear_form() {
    editing_id_.clear();
    name_edit_->clear();
    agent_combo_->setCurrentIndex(0);
    model_edit_->clear();
    mode_combo_->setCurrentIndex(0);
    tools_check_->setChecked(true);
    budget_spin_->setValue(0.0);
    daily_radio_->setChecked(true);
    time_edit_->setTime(QTime(8, 0));
    interval_spin_->setValue(60);
    for (int i = 0; i < day_checks_.size(); ++i)
        day_checks_[i]->setChecked(i < 5);
    enabled_check_->setChecked(true);
    prompt_edit_->clear();
    form_title_->setText(tr("NEW SCHEDULE"));
    {
        const QSignalBlocker block(list_);
        list_->clearSelection();
        list_->setCurrentRow(-1);
    }
    delete_btn_->setEnabled(false);
    refresh_info();
}

void AgentSchedulesPanel::load_into_form(const QString& id) {
    const ca::CodingAgentSchedule* s = ca::CodingAgentService::instance().find_schedule(id);
    if (!s)
        return;
    editing_id_ = s->id;
    name_edit_->setText(s->name);
    agent_combo_->setCurrentIndex(s->agent == ca::AgentKind::Codex ? 1 : 0);
    model_edit_->setText(s->model);
    mode_combo_->setCurrentIndex(s->mode == ca::RunMode::WorkspaceEdits ? 1 : 0);
    tools_check_->setChecked(s->use_fincept_tools);
    budget_spin_->setValue(s->max_budget_usd);
    (s->kind == ca::ScheduleKind::Interval ? interval_radio_ : daily_radio_)->setChecked(true);
    interval_spin_->setValue(s->interval_minutes);
    const QTime t = QTime::fromString(s->daily_time, QStringLiteral("HH:mm"));
    time_edit_->setTime(t.isValid() ? t : QTime(8, 0));
    for (int i = 0; i < day_checks_.size(); ++i)
        day_checks_[i]->setChecked((s->weekdays_mask & (1 << i)) != 0);
    enabled_check_->setChecked(s->enabled);
    prompt_edit_->setPlainText(s->prompt);
    form_title_->setText(tr("EDIT SCHEDULE"));
    delete_btn_->setEnabled(true);
    on_kind_changed();
    on_agent_changed();
    refresh_info();
}

ca::CodingAgentSchedule AgentSchedulesPanel::form_schedule() const {
    ca::CodingAgentSchedule s;
    s.id = editing_id_;
    s.name = name_edit_->text().trimmed();
    s.agent = agent_combo_->currentIndex() == 1 ? ca::AgentKind::Codex : ca::AgentKind::Claude;
    s.model = model_edit_->text().trimmed();
    s.mode = ca::mode_from_id(mode_combo_->currentData().toString());
    s.use_fincept_tools = tools_check_->isChecked();
    s.max_budget_usd = budget_spin_->value();
    s.kind = interval_radio_->isChecked() ? ca::ScheduleKind::Interval : ca::ScheduleKind::Daily;
    s.interval_minutes = interval_spin_->value();
    s.daily_time = time_edit_->time().toString(QStringLiteral("HH:mm"));
    int mask = 0;
    for (int i = 0; i < day_checks_.size(); ++i) {
        if (day_checks_[i]->isChecked())
            mask |= (1 << i);
    }
    s.weekdays_mask = mask;
    s.enabled = enabled_check_->isChecked();
    s.prompt = prompt_edit_->toPlainText().trimmed();
    return s;
}

void AgentSchedulesPanel::refresh_info() {
    if (!info_lbl_)
        return;
    auto& svc = ca::CodingAgentService::instance();
    const ca::CodingAgentSchedule* s = editing_id_.isEmpty() ? nullptr : svc.find_schedule(editing_id_);
    run_now_btn_->setEnabled(true);
    if (!s) {
        info_lbl_->setText(tr("Scheduled runs start in the background while the terminal is open. A run missed while "
                              "the terminal was closed runs once at the next start if it is less than 12 hours late."));
        return;
    }
    int count = 0;
    bool active = false;
    for (const auto& id : svc.run_ids()) {
        const ca::CodingAgentRun* r = svc.find_run(id);
        if (r && r->schedule_id == s->id) {
            ++count;
            active = active || !ca::is_terminal_status(r->status);
        }
    }
    auto fmt = [](qint64 ms) {
        return ms > 0 ? QDateTime::fromMSecsSinceEpoch(ms).toString(QStringLiteral("ddd yyyy-MM-dd HH:mm"))
                      : QStringLiteral("—");
    };
    QString text = tr("Next run: %1  ·  Last run: %2  ·  Runs in history: %3")
                       .arg(s->enabled ? fmt(s->next_run_at_ms) : tr("paused"), fmt(s->last_run_at_ms))
                       .arg(count);
    if (active)
        text += QStringLiteral("  ·  ") + tr("a run of this schedule is in progress");
    info_lbl_->setText(text);
}

void AgentSchedulesPanel::on_kind_changed() {
    const bool interval = interval_radio_->isChecked();
    interval_spin_->setEnabled(interval);
    time_edit_->setEnabled(!interval);
    for (auto* cb : std::as_const(day_checks_))
        cb->setEnabled(!interval);
}

void AgentSchedulesPanel::on_agent_changed() {
    const bool claude = agent_combo_->currentIndex() == 0;
    budget_spin_->setEnabled(claude);
    budget_lbl_->setEnabled(claude);
}

void AgentSchedulesPanel::on_template(int index) {
    if (index <= 0)
        return;
    const auto templates = agent_prompt_templates();
    if (index - 1 >= templates.size())
        return;
    const AgentPromptTemplate& t = templates[index - 1];
    prompt_edit_->setPlainText(t.body);
    if (name_edit_->text().trimmed().isEmpty())
        name_edit_->setText(t.title);
    if (t.needs_workspace)
        mode_combo_->setCurrentIndex(1);
    template_combo_->setCurrentIndex(0);
    const QTextCursor c = prompt_edit_->document()->find(agent_placeholder_regex());
    if (!c.isNull())
        prompt_edit_->setTextCursor(c);
    prompt_edit_->setFocus();
}

void AgentSchedulesPanel::on_new() {
    clear_form();
    name_edit_->setFocus();
}

bool AgentSchedulesPanel::on_save() {
    const ca::CodingAgentSchedule s = form_schedule();
    if (s.prompt.contains(agent_placeholder_regex())) {
        const auto answer = QMessageBox::question(
            this, tr("Save schedule"), tr("The prompt still contains placeholders such as <TICKER>. Save it anyway?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes)
            return false;
    }
    QString err;
    const QString id = ca::CodingAgentService::instance().save_schedule(s, &err);
    if (id.isEmpty()) {
        QMessageBox::warning(this, tr("Save schedule"), err);
        return false;
    }
    editing_id_ = id;
    form_title_->setText(tr("EDIT SCHEDULE"));
    reload_list();
    refresh_info();
    return true;
}

void AgentSchedulesPanel::on_delete() {
    if (editing_id_.isEmpty())
        return;
    const auto answer = QMessageBox::question(this, tr("Delete schedule"),
                                              tr("Delete this schedule? Runs it already produced stay in the history."),
                                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes)
        return;
    ca::CodingAgentService::instance().delete_schedule(editing_id_);
    clear_form();
    reload_list();
}

void AgentSchedulesPanel::on_run_now() {
    if (editing_id_.isEmpty() && !on_save())
        return; // run the schedule as saved — save a new one first
    QString err;
    const QString run_id = ca::CodingAgentService::instance().run_schedule_now(editing_id_, &err);
    if (run_id.isEmpty()) {
        QMessageBox::warning(this, tr("Run now"), err);
        return;
    }
    emit open_run_requested(run_id);
}

} // namespace fincept::screens
