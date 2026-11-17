// AgentSchedulesPanel.cpp — SCHEDULES tab: list + editor for recurring runs.

#include "screens/agent_config/AgentSchedulesPanel.h"

#include "screens/agent_config/AgentRunCommon.h"
#include "screens/agent_config/AgentPromptTemplates.h"
#include "screens/agent_config/HarnessRunOptionsWidget.h"
#include "services/coding_agents/CodingAgentService.h"
#include "services/harness/HarnessRegistry.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QDateTime>
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
#include <QStackedWidget>
#include <QTime>
#include <QTimeEdit>
#include <QVBoxLayout>

#include <initializer_list>

namespace fincept::screens {

namespace ca = services::coding_agents;

namespace agent_schedules_detail {

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

} // namespace agent_schedules_detail

using namespace agent_schedules_detail;

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
    list_stack_ = new QStackedWidget(left);
    list_ = new QListWidget(list_stack_);
    list_->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(list_, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row < 0)
            return;
        load_into_form(list_->item(row)->data(Qt::UserRole).toString());
    });
    list_stack_->addWidget(list_);
    list_empty_lbl_ = sched_label("awbPlaceholder", list_stack_);
    list_empty_lbl_->setAlignment(Qt::AlignCenter);
    list_empty_lbl_->setWordWrap(true);
    list_stack_->addWidget(list_empty_lbl_);
    lv->addWidget(list_stack_, 1);
    auto* list_btns = new QHBoxLayout;
    new_btn_ = sched_button("awbBtn", left);
    delete_btn_ = sched_button("awbDangerBtn", left);
    delete_btn_->setProperty("fincept_agent_risk", "delete"); // agents: deletes the schedule
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

    name_lbl_ = sched_label("awbLabel", form);
    fv->addWidget(name_lbl_);
    name_edit_ = new QLineEdit(form);
    name_edit_->setMaxLength(120);
    fv->addWidget(name_edit_);

    when_lbl_ = sched_label("awbLabel", form);
    fv->addWidget(when_lbl_);
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
    auto* when_grid = new QGridLayout;
    when_grid->setHorizontalSpacing(8);
    when_grid->setVerticalSpacing(4);
    when_grid->addWidget(daily_radio_, 0, 0);
    when_grid->addWidget(time_edit_, 0, 1);
    when_grid->addWidget(interval_radio_, 1, 0);
    when_grid->addWidget(interval_spin_, 1, 1);
    when_grid->setColumnStretch(2, 1);
    fv->addLayout(when_grid);
    auto* days_row = new QHBoxLayout;
    days_row->setSpacing(8);
    for (int d = 1; d <= 7; ++d) {
        auto* cb = new QCheckBox(QLocale().dayName(d, QLocale::ShortFormat), form);
        cb->setChecked(d <= 5);
        day_checks_.append(cb);
        days_row->addWidget(cb);
    }
    days_row->addStretch(1);
    fv->addLayout(days_row);
    enabled_check_ = new QCheckBox(form);
    enabled_check_->setChecked(true);
    fv->addWidget(enabled_check_);

    // The run options — identical to the RUN composer's.
    options_ = new HarnessRunOptionsWidget(HarnessRunOptionsWidget::Purpose::Schedule, form);
    connect(options_, &HarnessRunOptionsWidget::template_inserted, this, [this](const QString& title) {
        if (name_edit_->text().trimmed().isEmpty())
            name_edit_->setText(title);
    });
    connect(options_, &HarnessRunOptionsWidget::harness_changed, this, [this](const QString&) { refresh_editability(); });
    fv->addWidget(options_, 1);

    info_lbl_ = sched_label("awbHint", form);
    info_lbl_->setWordWrap(true);
    fv->addWidget(info_lbl_);

    auto* actions = new QHBoxLayout;
    actions->addStretch(1);
    run_now_btn_ = sched_button("awbBtn", form);
    run_now_btn_->setProperty("fincept_agent_risk", "spend"); // agents: runs the scheduled harness task now (model quota)
    save_btn_ = sched_button("awbPrimaryBtn", form);
    save_btn_->setProperty("fincept_agent_risk", "spend"); // agents: arms unattended runs of the harness (model quota)
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
    clear_form();

    // List items carry their text colour as an item brush — re-coloured once an
    // appearance change has settled.
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this, [this]() { reload_list(); });
    connect(&harness::HarnessRegistry::instance(), &harness::HarnessRegistry::status_changed, this,
            [this](const QString&) { refresh_editability(); });
}

void AgentSchedulesPanel::retranslate() {
    list_title_->setText(tr("SCHEDULES"));
    list_empty_lbl_->setText(tr("No schedules yet.\n\nA schedule runs an agent brief every N minutes or daily at a "
                                "time — e.g. a morning market brief before the open. Fill in the form on the right and "
                                "press SAVE."));
    new_btn_->setText(tr("NEW"));
    delete_btn_->setText(tr("DELETE"));
    form_title_->setText(editing_id_.isEmpty() ? tr("NEW SCHEDULE") : tr("EDIT SCHEDULE"));
    name_lbl_->setText(tr("NAME"));
    name_edit_->setPlaceholderText(tr("e.g. Morning brief"));
    when_lbl_->setText(tr("WHEN"));
    daily_radio_->setText(tr("Daily at"));
    interval_radio_->setText(tr("Every"));
    interval_spin_->setSuffix(tr(" min"));
    enabled_check_->setText(tr("Enabled"));
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
    auto& registry = harness::HarnessRegistry::instance();
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
                                             .arg(s.name, awb_harness_name(s.harness), describe(s), when),
                                         list_);
        item->setData(Qt::UserRole, s.id);
        const bool known = registry.is_known(s.harness);
        item->setForeground(QColor(!known ? ui::colors::WARNING()
                                          : (s.enabled ? ui::colors::TEXT_PRIMARY() : ui::colors::TEXT_TERTIARY())));
        if (s.id == editing_id_)
            select_row = list_->count() - 1;
    }
    list_stack_->setCurrentIndex(list_->count() > 0 ? 0 : 1);
    if (select_row >= 0)
        list_->setCurrentRow(select_row);
    delete_btn_->setEnabled(!editing_id_.isEmpty());
}

void AgentSchedulesPanel::clear_form() {
    editing_id_.clear();
    name_edit_->clear();
    options_->reset_to_defaults();
    daily_radio_->setChecked(true);
    time_edit_->setTime(QTime(8, 0));
    interval_spin_->setValue(60);
    for (int i = 0; i < day_checks_.size(); ++i)
        day_checks_[i]->setChecked(i < 5);
    enabled_check_->setChecked(true);
    form_title_->setText(tr("NEW SCHEDULE"));
    {
        const QSignalBlocker block(list_);
        list_->clearSelection();
        list_->setCurrentRow(-1);
    }
    delete_btn_->setEnabled(false);
    refresh_editability();
    refresh_info();
}

void AgentSchedulesPanel::load_into_form(const QString& id) {
    const ca::CodingAgentSchedule* s = ca::CodingAgentService::instance().find_schedule(id);
    if (!s)
        return;
    editing_id_ = s->id;
    name_edit_->setText(s->name);
    HarnessRunOptions o;
    o.harness = s->harness; // verbatim — an unknown id is shown as such, never relabelled
    o.model = s->model;
    o.effort = s->effort;
    o.permission = s->permission;
    o.tools = s->use_fincept_tools;
    o.budget_usd = s->max_budget_usd;
    o.time_limit_s = s->time_limit_s;
    o.max_turns = s->max_turns;
    o.workdir = s->workdir;
    o.prompt = s->prompt;
    options_->set_options(o);
    (s->kind == ca::ScheduleKind::Interval ? interval_radio_ : daily_radio_)->setChecked(true);
    interval_spin_->setValue(s->interval_minutes);
    const QTime t = QTime::fromString(s->daily_time, QStringLiteral("HH:mm"));
    time_edit_->setTime(t.isValid() ? t : QTime(8, 0));
    for (int i = 0; i < day_checks_.size(); ++i)
        day_checks_[i]->setChecked((s->weekdays_mask & (1 << i)) != 0);
    enabled_check_->setChecked(s->enabled);
    form_title_->setText(tr("EDIT SCHEDULE"));
    delete_btn_->setEnabled(true);
    on_kind_changed();
    refresh_editability();
    refresh_info();
}

ca::CodingAgentSchedule AgentSchedulesPanel::form_schedule() const {
    const HarnessRunOptions o = options_->options();
    ca::CodingAgentSchedule s;
    s.id = editing_id_;
    s.name = name_edit_->text().trimmed();
    s.harness = o.harness;
    s.model = o.model;
    s.effort = o.effort;
    s.permission = o.permission;
    s.use_fincept_tools = o.tools;
    s.max_budget_usd = o.budget_usd;
    s.time_limit_s = o.time_limit_s;
    s.max_turns = o.max_turns;
    s.workdir = o.workdir;
    s.prompt = o.prompt;
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
    return s;
}

void AgentSchedulesPanel::refresh_editability() {
    // A schedule of a harness this build does not have: read-only, deletable.
    const bool known = options_->harness_known();
    options_->set_read_only(!known);
    for (QWidget* w : std::initializer_list<QWidget*>{name_edit_, interval_radio_, daily_radio_, enabled_check_})
        w->setEnabled(known);
    save_btn_->setEnabled(known);
    QString why;
    run_now_btn_->setEnabled(known && options_->harness_runnable(&why));
    run_now_btn_->setToolTip(why);
    on_kind_changed();
}

void AgentSchedulesPanel::refresh_info() {
    if (!info_lbl_)
        return;
    auto& svc = ca::CodingAgentService::instance();
    const ca::CodingAgentSchedule* s = editing_id_.isEmpty() ? nullptr : svc.find_schedule(editing_id_);
    if (!s) {
        info_lbl_->setText(tr("Scheduled runs start in the background while the terminal is open. A run missed while "
                              "the terminal was closed runs once at the next start if it is less than 12 hours late."));
        return;
    }
    if (!harness::HarnessRegistry::instance().is_known(s->harness)) {
        info_lbl_->setText(tr("This schedule uses %1, which this build does not have — it is kept as it is and does not "
                              "run. Delete it, or create a new schedule with another harness.")
                               .arg(awb_harness_name(s->harness)));
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
    const bool editable = options_ ? options_->harness_known() : true;
    interval_spin_->setEnabled(interval && editable);
    time_edit_->setEnabled(!interval && editable);
    for (auto* cb : std::as_const(day_checks_))
        cb->setEnabled(!interval && editable);
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
